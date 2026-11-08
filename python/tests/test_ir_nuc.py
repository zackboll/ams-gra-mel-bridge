"""PY-MEL-006: both compiled-provider families and hostile C-boundary outputs.

Process-global failpoints and provider/callback lifetime checks run in fresh
interpreters. Boundary injection never introduces native test exports.
"""

from __future__ import annotations

import ctypes as c
import dataclasses
import gc
import inspect
import math
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _errors, _native as n, _nuc
import test_ir_health as health
import test_image_stream as image_tests
from test_ir_health_metadata import assert_owned


P = "ams_mel_ir_nuc_"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY


def open_parent(session, image):
    return (session.open_image_stream(image_tests.ImageStreamTests.config()) if image
            else session.open_health_channel(health.config()))


def rich():
    def point(ns, band, low, high, pixel, value, kelvin, classification):
        return m.NucTempPoint(ns, m.BandInfo(band, low, high),
                             m.NucPixelValue(pixel, value), kelvin, classification)
    return (
        m.NucTempData(()),
        m.NucTempData((point(-(1 << 63), m.BandType.IR_MIDWAVE, 3e-6, 5e-6,
                            m.NucPixelKind.UINT8, 255, 300.25, m.NucPointType.NUC_POINT),)),
        m.NucTempData((
            point(-1, m.BandType.IR_LONGWAVE, 8e-6, 1.2e-5, m.NucPixelKind.UINT16,
                  65535, 0.0, m.NucPointType.MFA_ESTIMATE),
            point((1 << 63) - 1, m.BandType.UV_VACUUM, 1e-8, 2e-7,
                  m.NucPixelKind.UINT32, (1 << 32) - 1, 1e6, m.NucPointType.OTHER),
            point(42, m.BandType.INVALID, -1.0, 0.0, m.NucPixelKind.UINT8,
                  0, -5.5, m.NucPointType.NUC_POINT))))


def raw_graph():
    points = (n.IrTempPointV1 * 3)()
    for index, point in enumerate(points):
        point.timestamp_ns = -7
        point.frame_band = n.IrBandInfoV1(int(m.BandType.IR_MIDWAVE), -0.0, 5e-6)
        point.pixel_value = n.IrPixelValueV1(index + 1, 1)
        point.temperature_k = -0.0
        point.point_type = index
    return n.IrNucTempDataEventV1(n.IrTempPointSpanV1(points, 3)), points


class _NucCases:
    """Each case is inherited by both independently reported provider families."""

    def setUp(self):
        health.quiesce()
        self.directory = tempfile.TemporaryDirectory(prefix="py-nuc-")
        self.addCleanup(self.directory.cleanup)
        self.log = Path(self.directory.name) / "log"
        patch = mock.patch.dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(self.log))
        patch.start()
        self.addCleanup(patch.stop)

    @property
    def native_open_name(self):
        return "ams_mel_ir_image_nuc_open" if self.image else "ams_mel_ir_health_nuc_open"

    def owners(self, scenario="nuc-rich", capacity=16):
        session = m.Session.open(health.provider(), scenario, "")
        self.addCleanup(session.close)
        parent = open_parent(session, self.image)
        self.addCleanup(parent.close)
        if capacity is None:
            return session, parent
        stream = parent.open_nuc(capacity)
        self.addCleanup(stream.close)
        return session, parent, stream

    def error(self, kind, operation, status=None):
        with self.assertRaises(m.MelError) as caught:
            operation()
        self.assertIs(caught.exception.kind, kind)
        if status is not None:
            self.assertEqual(caught.exception.native_status, status)
        return caught.exception

    def test_public_api_owned_frozen_models_and_exact_enums(self):
        names = ("NucPointType", "NucPixelKind", "NucPixelValue", "NucTempPoint",
                 "NucTempData", "NucCounters", "NucOpenError", "NucStream")
        for name in names:
            self.assertIn(name, m.__all__)
            self.assertEqual(getattr(m, name).__module__, "ams_mel")
            self.assertIs(getattr(m, name), getattr(_nuc, name))
        self.assertEqual([(p.name, p.value) for p in m.NucPointType],
                         [("NUC_POINT", 0), ("MFA_ESTIMATE", 1), ("OTHER", 2)])
        self.assertEqual([(p.name, p.value) for p in m.NucPixelKind],
                         [("UINT8", 1), ("UINT16", 2), ("UINT32", 3)])
        self.assertTrue(issubclass(m.NucOpenError, m.MelError))
        self.assertEqual(str(inspect.signature(m.NucStream.receive)),
                         "(self, timeout_ms: 'int' = 0) -> 'NucTempData'")
        for owner in (m.ImageStream, m.HealthStatusChannel):
            self.assertEqual(str(inspect.signature(owner.open_nuc)),
                             "(self, queue_capacity: 'int' = 16) -> 'NucStream'")
        for value in (*rich(), rich()[1].points[0], rich()[1].points[0].pixel,
                      m.NucCounters(1, 2, 3)):
            assert_owned(self, value)
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                setattr(value, dataclasses.fields(value)[0].name, None)
        with self.assertRaises(TypeError):
            m.NucStream()
        for points in ([], (None,), (rich()[0],)):
            with self.assertRaises(TypeError):
                m.NucTempData(points)

    def test_pixel_value_tagged_width_validation(self):
        for kind, maximum in zip(m.NucPixelKind, (255, 65535, 4294967295)):
            for value in (0, 1, maximum):
                self.assertEqual(m.NucPixelValue(kind, value).value, value)
            for value in (-1, maximum + 1, True, 1.0, "1"):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: m.NucPixelValue(kind, value))
        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: m.NucPixelValue(1, 0))

    def test_synchronous_rich_and_timeout_with_single_native_receive(self):
        _, _, stream = self.owners()
        self.assertIs(type(stream), m.NucStream)
        lines = self.log.read_text().splitlines()
        self.assertNotIn("channel_enabled", lines)
        self.assertEqual(stream.counters(), m.NucCounters(3, 0, 0))
        with mock.patch.object(n, P + "receive", wraps=n.ams_mel_ir_nuc_receive) as receive:
            values = tuple(stream.receive() for _ in range(3))
            self.assertEqual(receive.call_count, 3)
        self.assertEqual(values, rich())
        assert_owned(self, values)
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        self.assertEqual(stream.counters(), m.NucCounters(3, 0, 0))

    def test_invalid_capacity_and_timeout_never_call_native(self):
        _, parent = self.owners(capacity=None)
        with mock.patch.object(n, self.native_open_name) as native:
            for value in (0, -1, 1 << (8 * c.sizeof(c.c_size_t)), True, 1.0, "1", None):
                error = self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: parent.open_nuc(value))
                self.assertIsInstance(error, m.NucOpenError)
                self.assertIsNone(error.registration)
                self.assertIsNone(error.native_status)
            native.assert_not_called()
        stream = parent.open_nuc()
        self.addCleanup(stream.close)
        with mock.patch.object(n, P + "receive") as native:
            for value in (-1, 1 << 32, True, 0.5, "0", None):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(value))
            native.assert_not_called()
        self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_nuc, n.AMS_MEL_INVALID_ARGUMENT)
        stream.close()
        self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_nuc, n.AMS_MEL_INVALID_ARGUMENT)
        parent.close()
        with mock.patch.object(n, self.native_open_name) as native:
            error = self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_nuc)
            self.assertIsNone(error.registration)
            native.assert_not_called()

    def test_exact_registration_failure_results_and_parent_still_usable(self):
        cases = (("nuc-register-fail", m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED,
                  m.CommandReturn.FAIL),
                 ("nuc-register-throw", m.ErrorKind.PROVIDER_EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION, None),
                 ("image-metadata-four-mixed" if self.image else "health-rich",
                  m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED, m.CommandReturn.NOT_SUPPORTED))
        for scenario, kind, status, registration in cases:
            with self.subTest(scenario=scenario):
                session, parent = self.owners(scenario, None)
                error = self.error(kind, parent.open_nuc, status)
                self.assertIsInstance(error, m.NucOpenError)
                self.assertIs(error.registration, registration)
                self.assertGreaterEqual(error.diagnostic_required, 1)
                repeated = self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_nuc,
                                      n.AMS_MEL_INVALID_ARGUMENT)
                self.assertIsNone(repeated.registration)
                if self.image:
                    parent.start()
                    parent.stop()
                else:
                    parent.enable()
                    self.assertTrue(parent.capabilities().channel_types)
                    with parent.open_metadata() as metadata:
                        if scenario == "health-rich":
                            self.assertEqual(metadata.counters().events_received, 12)
                            metadata.receive()
                parent.close()
                session.close()

    def test_ordinary_metadata_independent_and_does_not_register_nuc(self):
        _, parent = self.owners(capacity=None)
        with mock.patch.object(n, self.native_open_name) as register:
            with parent.open_metadata() as ordinary:
                self.assertEqual(ordinary.counters().events_received, 0)
            register.assert_not_called()
        with parent.open_nuc() as stream:
            self.assertEqual(stream.counters(), m.NucCounters(3, 0, 0))

    def test_overflow_fifo_and_malformed_callbacks_do_not_fail_stream(self):
        for scenario, counters, stamps in (
                ("nuc-overflow", m.NucCounters(5, 3, 0), (0, 1)),
                ("nuc-malformed", m.NucCounters(4, 0, 3), (5,))):
            with self.subTest(scenario=scenario):
                _, _, stream = self.owners(scenario, 2)
                self.assertEqual(stream.counters(), counters)
                events = tuple(stream.receive() for _ in stamps)
                self.assertEqual(tuple(x.points[0].timestamp_ns for x in events), stamps)
                if scenario == "nuc-malformed":
                    self.assertEqual(events[0].points[0].pixel,
                                     m.NucPixelValue(m.NucPixelKind.UINT16, 1234))
                self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
                self.assertEqual(stream.counters(), counters)

    def test_nuc_close_does_not_close_parent_or_ordinary_metadata(self):
        _, parent = self.owners(capacity=None)
        if self.image:
            with parent.open_metadata() as ordinary:
                with parent.open_nuc() as stream:
                    stream.receive()
                self.assertTrue(ordinary.is_open)
                self.assertEqual(ordinary.counters().events_received, 0)
                parent.start()
                parent.stop()
        else:
            with parent.open_metadata() as ordinary:
                with parent.open_nuc() as stream:
                    stream.receive()
                self.assertTrue(ordinary.is_open)
                self.assertEqual(ordinary.counters().events_received, 0)
                parent.enable()
                self.assertTrue(parent.capabilities().channel_types)
        self.assertTrue(parent.is_open)
        self.assertFalse(stream.is_open)

    def test_parent_and_session_not_retained_and_queued_drain_after_teardown(self):
        session, parent, stream = self.owners()
        session_ref, parent_ref = weakref.ref(session), weakref.ref(parent)
        session.close()
        parent.close()
        # Remove cleanup-bound-method references before checking Python retention.
        self._cleanups = [entry for entry in self._cleanups
                          if getattr(entry[0], "__self__", None) not in (session, parent)]
        del session, parent
        gc.collect()
        self.assertIsNone(session_ref())
        self.assertIsNone(parent_ref())
        self.assertEqual(tuple(stream.receive() for _ in range(3)), rich())
        self.error(m.ErrorKind.STREAM_STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)
        stream.close()
        stream.close()
        self.assertFalse(stream.is_open)
        self.error(m.ErrorKind.INVALID_ARGUMENT, stream.receive)
        self.error(m.ErrorKind.INVALID_ARGUMENT, stream.counters)

    def injected_view(self, stream, raw=None, pointer=None):
        native = n.ams_mel_ir_nuc_event_view
        def view(owner, output, *args):
            status = native(owner, output, *args)
            c.cast(output, c.POINTER(c.POINTER(n.IrNucTempDataEventV1)))[0] = (
                c.pointer(raw) if raw is not None else pointer)
            return status
        return mock.patch.object(n, P + "event_view", side_effect=view)

    def test_unknown_band_pixel_point_and_width_mismatches(self):
        for field, value in (("band", 0xFFFFFFFF), ("pixel", 0), ("pixel", 4),
                             ("point", 3), ("u8", 256), ("u16", 65536)):
            with self.subTest(field=field):
                _, _, stream = self.owners()
                raw, points = raw_graph()
                if field == "band":
                    m.BandType(value)  # Dynamic capability enum caching must not relax NUC.
                    points[0].frame_band.kind = value
                elif field == "pixel":
                    points[0].pixel_value.kind = value
                elif field == "point":
                    points[0].point_type = value
                else:
                    points[0].pixel_value = n.IrPixelValueV1(1 if field == "u8" else 2, value)
                with self.injected_view(stream, raw), mock.patch.object(
                        n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
                    self.error(PROTOCOL, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_null_misaligned_impossible_spans_and_outer_views(self):
        for case in ("null", "misaligned", "huge", "outer-null", "outer-misaligned"):
            with self.subTest(case=case):
                _, _, stream = self.owners()
                raw, points = raw_graph()
                if case == "null":
                    raw.temp_points.data = c.POINTER(n.IrTempPointV1)()
                elif case == "misaligned":
                    raw.temp_points.data = c.cast(c.addressof(points) + 1,
                                                  c.POINTER(n.IrTempPointV1))
                elif case == "huge":
                    raw.temp_points.size = c.c_size_t(-1).value
                pointer = None
                if case.startswith("outer"):
                    pointer = c.cast(0 if case == "outer-null" else c.addressof(raw) + 1,
                                     c.POINTER(n.IrNucTempDataEventV1))
                with self.injected_view(stream, raw if pointer is None else None, pointer), \
                        mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
                    self.error(PROTOCOL, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_empty_duplicates_all_bands_width_not_magnitude_and_double_fidelity(self):
        _, _, stream = self.owners()
        raw, points = raw_graph()
        with self.injected_view(stream, raw):
            value = stream.receive()
        self.assertEqual([p.pixel.kind for p in value.points], list(m.NucPixelKind))
        self.assertEqual([p.pixel.value for p in value.points], [1, 1, 1])
        for point in value.points:
            self.assertEqual(math.copysign(1, point.temperature_k), -1)
            self.assertEqual(math.copysign(1, point.frame_band.min_wavelength_m), -1)
        for band in m.BandType:
            # Replenish native events using a fresh independently attached parent.
            _, _, stream = self.owners()
            points[0].frame_band.kind = int(band)
            points[1] = points[0]
            points[2] = points[0]
            points[0].temperature_k = float("nan")
            points[1].temperature_k = float("inf")
            points[2].temperature_k = -float("inf")
            with self.injected_view(stream, raw):
                value = stream.receive()
            self.assertTrue(math.isnan(value.points[0].temperature_k))
            self.assertEqual(value.points[1].temperature_k, float("inf"))
            self.assertEqual(value.points[2].temperature_k, -float("inf"))
            self.assertIs(value.points[0].frame_band.kind, band)
        points[0].temperature_k = -0.0
        points[1] = points[0]
        raw.temp_points.size = 2
        with self.injected_view(stream, raw):
            value = stream.receive()
        self.assertEqual(value.points[0], value.points[1])
        raw.temp_points.size = 0
        raw.temp_points.data = c.cast(1, c.POINTER(n.IrTempPointV1))
        with self.injected_view(stream, raw):
            self.assertEqual(stream.receive(), m.NucTempData(()))

    def test_native_error_categories_and_unexpected_event_owner_cleanup(self):
        for status in (n.AMS_MEL_TIMEOUT, n.AMS_MEL_STREAM_STOPPED, n.AMS_MEL_PROVIDER_FAILED,
                       n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR,
                       n.AMS_MEL_RESOURCE_EXHAUSTED, n.AMS_MEL_INVALID_ARGUMENT):
            _, _, stream = self.owners()
            native = n.ams_mel_ir_nuc_receive
            def receive(*args):
                self.assertEqual(native(*args), n.AMS_MEL_OK)
                return status
            with mock.patch.object(n, P + "receive", side_effect=receive) as call, \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
                error = self.error(_errors._STATUS_KINDS[status], stream.receive, status)
                self.assertEqual(error.diagnostic_required, 1)
                self.assertEqual(call.call_count, 1)
                self.assertEqual(close.call_count, 1)

    def test_success_without_event_view_failure_conversion_and_allocation_cleanup(self):
        _, _, stream = self.owners()
        native_receive = n.ams_mel_ir_nuc_receive
        def empty(*args):
            status = native_receive(*args)
            n.ams_mel_ir_nuc_event_close(args[2], None, 0, None)
            return status
        with mock.patch.object(n, P + "receive", side_effect=empty):
            self.error(PROTOCOL, stream.receive)
        native_view = n.ams_mel_ir_nuc_event_view
        def failed(*args):
            native_view(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, P + "event_view", side_effect=failed), \
                mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
            self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(close.call_count, 1)
        # The last rich event has points; conversion failures must still release it.
        with mock.patch.object(_nuc, "_point_from_raw", side_effect=MemoryError), \
                mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
            self.error(m.ErrorKind.INTERNAL_ERROR, stream.receive)
            self.assertEqual(close.call_count, 1)
        for exception in (RuntimeError("conversion"), KeyboardInterrupt(), MemoryError()):
            _, _, stream = self.owners()
            with mock.patch.object(_nuc, "NucTempData", side_effect=exception), \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_nuc_event_close) as close:
                with self.assertRaises(type(exception)):
                    stream.receive()
                self.assertEqual(close.call_count, 1)

    def test_event_close_consumed_retained_and_cleanup_does_not_mask_original(self):
        for consumed in (False, True):
            _, _, stream = self.owners()
            real_close = n.ams_mel_ir_nuc_event_close
            retained = n.IrNucEventHandle()
            def close(output, diagnostic, capacity, required):
                retained.value = c.cast(output, c.POINTER(n.IrNucEventHandle))[0]
                if consumed:
                    real_close(output, diagnostic, capacity, required)
                    retained.value = None
                else:
                    c.memmove(diagnostic, b"\0", 1)
                    c.cast(required, c.POINTER(c.c_size_t))[0] = 1
                return n.AMS_MEL_PROVIDER_FAILED
            try:
                with mock.patch.object(n, P + "event_close", side_effect=close) as call:
                    self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
                    self.assertEqual(call.call_count, 1)
            finally:
                # Release the test's deliberately retained real allocation, not
                # a retry by the production owner under test.
                if retained.value:
                    real_close(c.byref(retained), None, 0, None)
        _, _, stream = self.owners()
        real_close = n.ams_mel_ir_nuc_event_close
        def cleanup(*args):
            real_close(*args)
            raise RuntimeError("cleanup")
        with mock.patch.object(_nuc, "NucTempData", side_effect=ValueError("original")), \
                mock.patch.object(n, P + "event_close", side_effect=cleanup) as close:
            with self.assertRaisesRegex(ValueError, "original"):
                stream.receive()
            self.assertEqual(close.call_count, 1)

    def test_open_anomalies_sentinel_and_unexpected_owner_cleanup_once(self):
        for outcome in ("failed-owner", "invalid", "non-success", "null", "unwritten"):
            _, parent = self.owners(capacity=None)
            native = getattr(n, self.native_open_name)
            def register(*args):
                self.assertEqual(c.cast(args[3], c.POINTER(c.c_uint32))[0], 0xFFFFFFFF)
                status = native(*args)
                self.assertEqual(status, n.AMS_MEL_OK)
                if outcome == "null":
                    n.ams_mel_ir_nuc_close(args[2], None, 0, None)
                elif outcome == "invalid":
                    c.cast(args[3], c.POINTER(c.c_uint32))[0] = 123
                elif outcome == "unwritten":
                    c.cast(args[3], c.POINTER(c.c_uint32))[0] = 0xFFFFFFFF
                elif outcome == "non-success":
                    c.cast(args[3], c.POINTER(c.c_uint32))[0] = int(m.CommandReturn.FAIL)
                return n.AMS_MEL_PROVIDER_FAILED if outcome == "failed-owner" else status
            real_close = n.ams_mel_ir_nuc_close
            def cleanup(*args):
                real_close(*args)
                raise RuntimeError("do not mask open error")
            with mock.patch.object(n, self.native_open_name, side_effect=register), \
                    mock.patch.object(m.NucStream, "_close_function",
                                      staticmethod(mock.Mock(side_effect=cleanup))):
                error = self.error(m.ErrorKind.PROVIDER_FAILED if outcome == "failed-owner" else PROTOCOL,
                                   parent.open_nuc)
                self.assertIsInstance(error, m.NucOpenError)
                self.assertIs(error.registration, m.CommandReturn.SUCCESS if outcome in (
                    "failed-owner", "null") else m.CommandReturn.FAIL if outcome == "non-success" else None)
                self.assertEqual(m.NucStream._close_function.call_count,
                                 0 if outcome == "null" else 1)

    def test_uint64_counters_and_failure_propagation(self):
        _, _, stream = self.owners()
        native = n.ams_mel_ir_nuc_get_counters
        def counters(owner, output, *args):
            status = native(owner, output, *args)
            raw = c.cast(output, c.POINTER(n.IrC2MetadataCountersV1)).contents
            raw.events_received = (1 << 64) - 1
            raw.events_dropped_queue_full = (1 << 63) + 1
            raw.malformed_or_unsupported = (1 << 63) - 1
            return status
        with mock.patch.object(n, P + "get_counters", side_effect=counters):
            self.assertEqual(stream.counters(), m.NucCounters((1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1))
        def failed(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, P + "get_counters", side_effect=failed):
            self.error(m.ErrorKind.PROVIDER_FAILED, stream.counters, n.AMS_MEL_PROVIDER_FAILED)

    def test_close_failures_never_retry_uncertain_handle_and_context_preserves_body(self):
        for consumed in (False, True):
            _, _, stream = self.owners()
            real_close = stream._close_function
            retained = n.IrNucMetadataHandle(stream._owner.value)
            def close(output, diagnostic, capacity, required):
                if consumed:
                    real_close(output, diagnostic, capacity, required)
                    retained.value = None
                else:
                    c.memmove(diagnostic, b"\0", 1)
                    c.cast(required, c.POINTER(c.c_size_t))[0] = 1
                return n.AMS_MEL_PROVIDER_FAILED
            try:
                stream._close_function = mock.Mock(side_effect=close)
                self.error(m.ErrorKind.PROVIDER_FAILED, stream.close, n.AMS_MEL_PROVIDER_FAILED)
                stream.close()
                stream.__del__()
                self.assertEqual(stream._close_function.call_count, 1)
                self.assertEqual(stream.is_open, not consumed)
            finally:
                if retained.value:
                    real_close(c.byref(retained), None, 0, None)
                    stream._owner.value = None
        _, _, stream = self.owners()
        real_close = stream._close_function
        def throwing(*args):
            real_close(*args)
            raise RuntimeError("cleanup")
        stream._close_function = mock.Mock(side_effect=throwing)
        with self.assertRaisesRegex(ValueError, "body"):
            with stream:
                raise ValueError("body")
        stream.__del__()
        self.assertEqual(stream._close_function.call_count, 1)

    def test_finalizer_releases_public_owner_once(self):
        _, parent = self.owners(capacity=None)
        stream = parent.open_nuc()
        reference = weakref.ref(stream)
        with mock.patch.object(m.NucStream, "_close_function",
                               staticmethod(mock.Mock(wraps=n.ams_mel_ir_nuc_close))):
            close = m.NucStream._close_function
            del stream
            gc.collect()
            self.assertIsNone(reference())
            self.assertEqual(close.call_count, 1)
        with parent.channel_view() as view:
            self.assertTrue(view.is_open)

    def test_isolated_allocation_late_callbacks_barrier_and_dso_owned_values(self):
        for case in ("allocation", "late", "barrier", "owned", "register-fail", "register-throw"):
            with self.subTest(case=case):
                environment = dict(os.environ)
                environment["PYTHONPATH"] = os.pathsep.join(
                    (str(Path(__file__).resolve().parent), environment.get("PYTHONPATH", "")))
                code = f"import test_ir_nuc as t; t.isolated_case({case!r}, {self.image!r})"
                result = subprocess.run([sys.executable, "-W", "error", "-c", code],
                                        env=environment, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class ImageNucTests(_NucCases, unittest.TestCase):
    image = True


class HealthNucTests(_NucCases, unittest.TestCase):
    image = False


class SharedNucTests(unittest.TestCase):
    def test_independent_families_image_stop_and_health_teardown(self):
        with m.Session.open(health.provider(), "nuc-rich", "") as session:
            with open_parent(session, True) as image, open_parent(session, False) as health_channel:
                with image.open_nuc() as image_nuc, health_channel.open_nuc() as health_nuc:
                    session.close()
                    image.start()
                    image.stop()
                    self.assertEqual(tuple(image_nuc.receive() for _ in range(3)), rich())
                    with self.assertRaises(m.MelError) as caught:
                        image_nuc.receive()
                    self.assertIs(caught.exception.kind, m.ErrorKind.STREAM_STOPPED)
                    self.assertEqual(tuple(health_nuc.receive() for _ in range(3)), rich())
                    with self.assertRaises(m.MelError) as caught:
                        health_nuc.receive()
                    self.assertIs(caught.exception.kind, m.ErrorKind.TIMEOUT)
                    health_channel.close()
                    with self.assertRaises(m.MelError) as caught:
                        health_nuc.receive()
                    self.assertIs(caught.exception.kind, m.ErrorKind.STREAM_STOPPED)


def isolated_case(case, image):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-nuc-child-") as directory:
        log = Path(directory) / "log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        scenario = {"allocation": "nuc-allocation", "late": "nuc-late",
                    "barrier": "nuc-nonquiescing", "owned": "nuc-rich",
                    "register-fail": "nuc-register-fail",
                    "register-throw": "nuc-register-throw"}[case]
        barrier = Path(directory) / "barrier"
        if case == "barrier":
            os.environ["AMS_MEL_TEST_NUC_CALLBACK_BARRIER"] = str(barrier)
        with m.Session.open(health.provider(), scenario, "") as session:
            with open_parent(session, image) as parent:
                if case.startswith("register"):
                    with test.assertRaises(m.NucOpenError) as caught:
                        parent.open_nuc()
                    test.assertIs(caught.exception.registration,
                                  m.CommandReturn.FAIL if case == "register-fail" else None)
                    test.assertEqual(caught.exception.native_status,
                                     n.AMS_MEL_PROVIDER_FAILED if case == "register-fail"
                                     else n.AMS_MEL_PROVIDER_EXCEPTION)
                    parent.close()
                    session.close()
                    lines = log.read_text().splitlines()
                    test.assertIn("library_unloaded", lines)
                    if case == "register-fail":
                        ordered = ("nuc_registration_attempted", "late_nuc_callback_invoked",
                                   "late_nuc_callback_returned", "library_unloaded")
                        indices = [lines.index(item) for item in ordered]
                        test.assertEqual(indices, sorted(indices))
                    return
                if case == "allocation":
                    os.environ["AMS_MEL_TEST_NUC_CALLBACK_FAILURE"] = "allocation"
                with parent.open_nuc() as stream:
                    session.close()
                    if case == "allocation":
                        libc = c.CDLL(None)
                        libc.getenv.argtypes = [c.c_char_p]
                        libc.getenv.restype = c.c_char_p
                        test.assertIsNone(libc.getenv(b"AMS_MEL_TEST_NUC_CALLBACK_FAILURE"))
                        test.assertEqual(stream.counters(), m.NucCounters(2, 0, 1))
                        test.assertEqual(stream.receive().points[0].timestamp_ns, 2)
                        with test.assertRaises(m.MelError) as error:
                            stream.receive()
                        test.assertIs(error.exception.kind, m.ErrorKind.TIMEOUT)
                    elif case == "late":
                        first = stream.receive()
                        parent.close()
                        test.assertEqual(tuple(stream.receive() for _ in range(2)), rich()[1:])
                        with test.assertRaises(m.MelError) as error:
                            stream.receive()
                        test.assertIs(error.exception.kind, m.ErrorKind.STREAM_STOPPED)
                        test.assertEqual(stream.counters(), m.NucCounters(4, 0, 0))
                        test.assertEqual(first, rich()[0])
                    elif case == "barrier":
                        barrier.with_suffix(".start").touch()
                        deadline = time.monotonic() + 10
                        while not barrier.with_suffix(".entered").exists():
                            if time.monotonic() >= deadline:
                                raise AssertionError("native callback did not reach barrier")
                            threading.Event().wait(0.001)
                        try:
                            stream.close()
                            with log.open("a") as output:
                                output.write("nuc_owner_closed\n")
                        finally:
                            barrier.with_suffix(".release").touch()
                        parent.close()
                    else:
                        values = tuple(stream.receive() for _ in range(3))
                        parent.close()
        lines = log.read_text().splitlines()
        test.assertIn("library_unloaded", lines)
        if case == "owned":
            test.assertEqual(values, rich())
            assert_owned(test, values)
        elif case in ("late", "barrier"):
            ordered = (("late_nuc_callback_invoked", "late_nuc_callback_returned", "library_unloaded")
                       if case == "late" else ("nuc_callback_entered", "nuc_owner_closed",
                                               "nuc_callback_returned", "library_unloaded"))
            indices = [lines.index(item) for item in ordered]
            test.assertEqual(indices, sorted(indices))


if __name__ == "__main__":
    unittest.main()
