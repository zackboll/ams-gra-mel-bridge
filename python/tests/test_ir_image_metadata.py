"""PY-MEL-007 compiled-provider regressions and hostile ABI output checks.

Global mock scenarios, callback barriers and actual DSO unload run in children.
Injection is limited to native outputs the compiled provider cannot construct.
"""

from __future__ import annotations

from contextlib import contextmanager
import ctypes as c
import dataclasses
import gc
import math
import os
from pathlib import Path
import pickle
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _errors, _image_convert, _image_metadata, _native as n
import test_image_stream as frames
from test_ir_health import provider, quiesce
from test_ir_health_metadata import assert_owned


P = "ams_mel_ir_image_metadata_"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
K = m.ImageMetadataKind


def bad(base=0):
    return m.ImageMetadataEvent(K.BAD_PIXEL_LIST, m.BadPixelList(0xA5A50001, 0x5A5A0003, tuple(
        m.BadPixel(row, column, m.BadPixelReason.UNKNOWN) for row, column in (
            (1 + base, 0x80000001), (0xF0000002, 17 + base), (123 + base, 456)))))


def report():
    return m.ImageMetadataEvent(K.LINE_OF_SIGHT_REPORT, m.LineOfSightReport(
        -123456789, m.AzimuthElevation(1.25, -2.5), m.AzimuthElevation(.125, -.25),
        True, False, m.Euler(.5, -.75, 1.0), 0xA5A50003, -1.5))


def euler():
    return m.ImageMetadataEvent(K.LINE_OF_SIGHT_EULER, m.LineOfSightEuler(
        987654321, m.Euler(-.5, 1.25, -2.0), m.Euler(.25, -.125, .0625)))


def navigation():
    return m.ImageMetadataEvent(K.NAVIGATION_RESPONSE,
                              m.NavigationResponse(-8765432109, 0xF1234567, 0x89ABCDEF))


RICH = (bad(), report(), euler(), navigation())


def error(test, kind, operation, status=None):
    with test.assertRaises(m.MelError) as caught:
        operation()
    test.assertIs(caught.exception.kind, kind)
    if status is not None:
        test.assertEqual(caught.exception.native_status, status)
    return caught.exception


def ordered(test, lines, expected):
    positions = [lines.index(item) for item in expected]
    test.assertEqual(positions, sorted(positions), lines)
    test.assertEqual(len(set(positions)), len(positions))


def run_child(case):
    """One isolated compiled-provider scenario with deterministic cleanup."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-image-metadata-") as directory:
        log = Path(directory) / "lifetime"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        barrier = Path(directory) / "callback"
        if case.startswith("barrier-"):
            os.environ["AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER"] = str(barrier)
            scenario = "image-metadata-nonquiescing"
        elif case in LIFETIMES:
            scenario = "nuc-rich" if case.startswith("nuc-") else "image-metadata-four-mixed"
        else:
            scenario = "image-metadata-" + case
        with m.Session.open(provider(), scenario, "") as session:
            with session.open_image_stream(frames.ImageStreamTests.config()) as image:
                if "register-" in case:
                    status = n.AMS_MEL_PROVIDER_EXCEPTION if case.endswith("throw") else n.AMS_MEL_PROVIDER_FAILED
                    failure = error(test, _errors._STATUS_KINDS[status], image.open_metadata, status)
                    test.assertTrue(failure.diagnostic)
                    test.assertEqual(failure.diagnostic_required, len(failure.diagnostic.encode()) + 1)
                    with log.open("a") as output:
                        output.write("open_failed\n")
                    error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_metadata, n.AMS_MEL_INVALID_ARGUMENT)
                    test.assertTrue(image.is_open)
                    image.start()
                    image.stop()
                else:
                    for capacity in (0, -1, True, 1.5, "2", 1 << (8 * c.sizeof(c.c_size_t))):
                        with mock.patch.object(n, P + "open") as op:
                            error(test, m.ErrorKind.INVALID_ARGUMENT, lambda: image.open_metadata(capacity))
                            op.assert_not_called()
                    early_nuc = image.open_nuc() if case == "nuc-first" else None
                    if case == "running":
                        image.start()
                    allocation = "allocation" in case
                    if allocation:
                        os.environ["AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE"] = "allocation"
                    capacity = 4 if case in LIFETIMES or case == "four-mixed" else 2
                    if case == "los-mixed":
                        capacity = 2
                    metadata = image.open_metadata(capacity)
                    os.environ.pop("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", None)
                    with metadata:
                        error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_metadata, n.AMS_MEL_INVALID_ARGUMENT)
                        if case.startswith("barrier-"):
                            barrier.with_suffix(".start").touch()
                            deadline = time.monotonic() + 10
                            while not barrier.with_suffix(".entered").exists():
                                test.assertLess(time.monotonic(), deadline, "native callback barrier not entered")
                                os.sched_yield()
                            test.assertNotIn("image_metadata_callback_returned", log.read_text())
                            if case == "barrier-close":
                                metadata.close()
                            else:
                                metadata.__del__()
                                metadata.__del__()
                            with log.open("a") as output:
                                output.write("public_metadata_closed\n")
                            test.assertNotIn("library_unloaded", log.read_text())
                            image.close()  # Native destructor releases and joins callback.
                        elif case.startswith("nuc-"):
                            with early_nuc or image.open_nuc() as nuc:
                                test.assertEqual([len(nuc.receive().points) for _ in range(3)], [0, 1, 3])
                            test.assertEqual(metadata.counters(), m.ImageMetadataCounters(0, 0, 0))
                            error(test, m.ErrorKind.TIMEOUT, metadata.receive, n.AMS_MEL_TIMEOUT)
                        elif case in LIFETIMES:
                            if case != "running":
                                image.start()
                            test.assertEqual(image.receive(1000).pixels, bytes(range(16, 28)))
                            if case == "metadata-first":
                                metadata.close()
                                test.assertTrue(image.is_open)
                                test.assertEqual(image.receive(1000).frame_id, 2)
                            elif case == "finalize":
                                metadata.__del__()
                                metadata.__del__()
                                test.assertTrue(image.is_open)
                                test.assertEqual(image.receive(1000).frame_id, 2)
                            else:
                                if case == "stop-drain":
                                    image.stop()
                                elif case == "image-first":
                                    image.close()
                                elif case == "session-first":
                                    session.close()
                                values = tuple(metadata.receive() for _ in range(4))
                                test.assertEqual(values, RICH)
                                if case in ("stop-drain", "image-first"):
                                    error(test, m.ErrorKind.STREAM_STOPPED, metadata.receive, n.AMS_MEL_STREAM_STOPPED)
                        else:
                            expected, counters = SCENARIOS[case]
                            test.assertEqual(metadata.counters(), m.ImageMetadataCounters(*counters))
                            values = tuple(metadata.receive() for _ in expected)
                            test.assertEqual(values, expected)
                            error(test, m.ErrorKind.TIMEOUT, metadata.receive, n.AMS_MEL_TIMEOUT)
                            test.assertEqual(metadata.counters(), m.ImageMetadataCounters(*counters))
                    test.assertFalse(metadata.is_open)
                    error(test, m.ErrorKind.INVALID_ARGUMENT, image.open_metadata,
                          n.AMS_MEL_INVALID_ARGUMENT if image.is_open else None)
        lines = log.read_text().splitlines()
        test.assertIn("library_unloaded", lines)
        if case.startswith("barrier-"):
            ordered(test, lines, ("image_metadata_callback_entered", "public_metadata_closed",
                                  "image_metadata_callback_returned", "channel_destroyed", "library_unloaded"))
        if case.startswith("navigation-register-"):
            ordered(test, lines, ("navigation_registration_failed", "open_failed", "retained_callback_invoked",
                                  "retained_callback_returned", "channel_destroyed", "library_unloaded"))
        if case == "four-mixed":
            ordered(test, lines, ("bad_pixel_registration_attempted", "line_of_sight_report_registration_attempted",
                                  "line_of_sight_euler_registration_attempted", "navigation_response_registration_attempted"))
        # Owned snapshots survive the actual provider DSO unload, not just Close.
        if "values" in locals():
            for value in values:
                assert_owned(test, value)
                test.assertEqual(pickle.loads(pickle.dumps(value)), value)
        print("IMAGE_METADATA_CHILD_COMPLETE")


SCENARIOS = {
    "sync": ((bad(),), (1, 0, 0)),
    "los-rich": ((report(), euler()), (2, 0, 0)),
    "four-mixed": (RICH, (6, 2, 0)),
    "overflow": ((bad(), bad(1)), (5, 3, 0)),
    "malformed": ((bad(),), (2, 0, 1)),
    "allocation": ((bad(9),), (2, 0, 1)),
    "los-mixed": ((bad(), report()), (5, 3, 0)),
    "los-report-null": ((report(),), (2, 0, 1)),
    "los-euler-null": ((euler(),), (2, 0, 1)),
    "los-report-allocation": ((report(),), (2, 0, 1)),
    "navigation-null": ((navigation(),), (2, 0, 1)),
    "navigation-allocation": ((navigation(),), (2, 0, 1)),
}
REGISTRATIONS = ("register-fail", "register-not-supported", "register-throw", "report-register-fail",
                 "report-register-throw", "euler-register-fail", "euler-register-throw",
                 "navigation-register-fail", "navigation-register-throw")
LIFETIMES = ("metadata-first", "image-first", "session-first", "stop-drain", "finalize", "running",
             "nuc-first", "nuc-after")


class ImageMetadataProviderTests(unittest.TestCase):
    """Each generated test is independently counted and process-isolated."""


def _provider_test(case):
    def test(self):
        environment = dict(os.environ)
        environment["PYTHONPATH"] = str(Path(__file__).resolve().parent) + os.pathsep + environment.get("PYTHONPATH", "")
        for key in ("AMS_MEL_TEST_LIFETIME_LOG", "AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE",
                    "AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER"):
            environment.pop(key, None)
        result = subprocess.run([sys.executable, "-W", "error", "-c",
                                 "from test_ir_image_metadata import run_child; run_child(" + repr(case) + ")"],
                                env=environment, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("IMAGE_METADATA_CHILD_COMPLETE", result.stdout)
    return test


for _case in (*SCENARIOS, *REGISTRATIONS, *LIFETIMES, "barrier-close", "barrier-finalize"):
    setattr(ImageMetadataProviderTests, "test_" + _case.replace("-", "_"), _provider_test(_case))


class ImageMetadataBoundaryTests(unittest.TestCase):
    def setUp(self):
        quiesce()

    def owners(self, capacity=16):
        session = m.Session.open(provider(), "image-metadata-four-mixed", "")
        self.addCleanup(session.close)
        image = session.open_image_stream(frames.ImageStreamTests.config())
        self.addCleanup(image.close)
        metadata = image.open_metadata(capacity)
        self.addCleanup(metadata.close)
        return session, image, metadata

    @contextmanager
    def view(self, raw=None, pointer=None):
        native = n.ams_mel_ir_image_metadata_event_view
        def inject(owner, output, *args):
            status = native(owner, output, *args)
            c.cast(output, c.POINTER(c.POINTER(n.IrImageMetadataEventV1)))[0] = (
                c.pointer(raw) if raw is not None else pointer)
            return status
        with mock.patch.object(n, P + "event_view", side_effect=inject):
            yield

    def rejected(self, raw=None, pointer=None):
        _, _, metadata = self.owners()
        with self.view(raw, pointer), mock.patch.object(n, P + "event_close",
                wraps=n.ams_mel_ir_image_metadata_event_close) as close:
            error(self, PROTOCOL, metadata.receive)
            self.assertEqual(close.call_count, 1)

    def test_unknown_kind_and_bad_pixel_reason(self):
        for kind in (0, 5, 0xFFFFFFFF):
            self.rejected(n.IrImageMetadataEventV1(kind=kind))
        pixels = (n.IrBadPixelV1 * 1)(n.IrBadPixelV1(1, 2, 99))
        raw = n.IrImageMetadataEventV1(kind=1)
        raw.bad_pixel_list.pixels = n.IrBadPixelSpanV1(pixels, 1)
        self.rejected(raw)

    def test_strict_booleans(self):
        for field in ("at_speed", "in_tolerance"):
            for invalid in (2, 255):
                with self.subTest(field=field, invalid=invalid):
                    raw = n.IrImageMetadataEventV1(kind=2)
                    setattr(raw.line_of_sight_report, field, invalid)
                    self.rejected(raw)

    def test_null_misaligned_and_huge_spans_and_views(self):
        pixels = (n.IrBadPixelV1 * 1)()
        for mutation in ("null", "misaligned", "huge", "overflow"):
            raw = n.IrImageMetadataEventV1(kind=1)
            raw.bad_pixel_list.pixels = n.IrBadPixelSpanV1(pixels, 1)
            span = raw.bad_pixel_list.pixels
            if mutation == "null":
                span.data = c.POINTER(n.IrBadPixelV1)()
            elif mutation == "misaligned":
                span.data = c.cast(c.addressof(pixels) + 1, c.POINTER(n.IrBadPixelV1))
            elif mutation == "huge":
                span.size = c.c_size_t(-1).value
            else:
                span.data = c.cast(c.c_void_p(-4), c.POINTER(n.IrBadPixelV1))
            with self.subTest(mutation=mutation):
                self.rejected(raw)
        raw = n.IrImageMetadataEventV1()
        for address in (0, c.addressof(raw) + 1, c.c_void_p(-8).value):
            self.rejected(pointer=c.cast(address, c.POINTER(n.IrImageMetadataEventV1)))

    def test_only_active_payload_is_read(self):
        for kind in K:
            raw = n.IrImageMetadataEventV1()
            c.memset(c.byref(raw), 255, c.sizeof(raw))
            raw.kind = kind
            field = {1: "bad_pixel_list", 2: "line_of_sight_report",
                     3: "line_of_sight_euler", 4: "navigation_response"}[kind]
            active = getattr(raw, field)
            c.memset(c.byref(active), 0, c.sizeof(active))
            _, _, metadata = self.owners()
            with self.view(raw):
                value = metadata.receive()
            self.assertIs(value.kind, kind)
            assert_owned(self, value)

    def test_owned_empty_and_duplicate_pixels_with_independent_reported_counts(self):
        _, _, metadata = self.owners()
        pixels = (n.IrBadPixelV1 * 2)(n.IrBadPixelV1(0xFFFFFFFF, 0x80000000, 0),
                                     n.IrBadPixelV1(0xFFFFFFFF, 0x80000000, 0))
        raw = n.IrImageMetadataEventV1(kind=1)
        raw.bad_pixel_list = n.IrBadPixelListV1(99, 0, n.IrBadPixelSpanV1(pixels, 2))
        with self.view(raw):
            event = metadata.receive()
        self.assertEqual(event.payload, m.BadPixelList(99, 0, (
            m.BadPixel(0xFFFFFFFF, 0x80000000, m.BadPixelReason.UNKNOWN),) * 2))
        pixels[0].row = 0
        self.assertEqual(event.payload.pixels[0].row, 0xFFFFFFFF)
        raw.bad_pixel_list.pixels = n.IrBadPixelSpanV1(c.cast(1, c.POINTER(n.IrBadPixelV1)), 0)
        with self.view(raw):
            self.assertEqual(metadata.receive().payload, m.BadPixelList(99, 0, ()))

    def test_all_floating_fields_and_signed_timestamp_extrema(self):
        # The compiled provider has no special-value/extrema fixture. Exercise
        # each active double through the real Receive/View/Close owner path.
        paths = {2: ("pointing_angle.azimuth_rad", "pointing_angle.elevation_rad",
                     "pointing_angle_rates.azimuth_rad", "pointing_angle_rates.elevation_rad",
                     "platform_attitude.roll", "platform_attitude.pitch", "platform_attitude.yaw",
                     "image_rotation_rad"),
                 3: ("attitude.roll", "attitude.pitch", "attitude.yaw", "attitude_rates.roll",
                     "attitude_rates.pitch", "attitude_rates.yaw")}
        for kind in (2, 3, 4):
            for stamp in (-(1 << 63), (1 << 63) - 1):
                for special in (-0.0, float("nan"), float("inf"), -float("inf"), 5e-324):
                    _, _, metadata = self.owners()
                    raw = n.IrImageMetadataEventV1(kind=kind)
                    active = getattr(raw, {2: "line_of_sight_report", 3: "line_of_sight_euler",
                                           4: "navigation_response"}[kind])
                    active.system_time_ns = stamp
                    for path in paths.get(kind, ()):
                        parent, _, leaf = path.rpartition(".")
                        setattr(getattr(active, parent) if parent else active, leaf, special)
                    if kind == 2:
                        active.validity_flag_bitfield = 0xFFFFFFFF
                        active.at_speed, active.in_tolerance = 0, 1
                    if kind == 4:
                        active.command_id, active.request_id = 0xFFFFFFFF, 0xFFFFFFFF
                    with self.view(raw):
                        value = metadata.receive().payload
                    self.assertEqual(value.system_time_ns, stamp)
                    for path in paths.get(kind, ()):
                        parent, _, leaf = path.rpartition(".")
                        actual = getattr(getattr(value, parent) if parent else value, leaf)
                        if math.isnan(special):
                            self.assertTrue(math.isnan(actual))
                        else:
                            self.assertEqual(struct.pack("d", actual), struct.pack("d", special))
                    if kind == 2:
                        self.assertEqual(value.validity_flag_bitfield, 0xFFFFFFFF)
                        self.assertIs(value.at_speed, False)
                        self.assertIs(value.in_tolerance, True)
                    if kind == 4:
                        self.assertEqual((value.command_id, value.request_id), (0xFFFFFFFF, 0xFFFFFFFF))

    def test_models_immutable_exact_discriminants_pickle_and_reused_euler(self):
        self.assertEqual([(k.name, k.value) for k in K], [
            ("BAD_PIXEL_LIST", 1), ("LINE_OF_SIGHT_REPORT", 2),
            ("LINE_OF_SIGHT_EULER", 3), ("NAVIGATION_RESPONSE", 4)])
        self.assertEqual(list(m.BadPixelReason), [m.BadPixelReason.UNKNOWN])
        for event in RICH:
            assert_owned(self, event)
            self.assertEqual(pickle.loads(pickle.dumps(event)), event)
            for value in (event, event.payload):
                for field in dataclasses.fields(value):
                    with self.assertRaises(dataclasses.FrozenInstanceError):
                        setattr(value, field.name, None)
            for other in RICH:
                if other.kind != event.kind:
                    with self.assertRaises(TypeError):
                        m.ImageMetadataEvent(event.kind, other.payload)
        with self.assertRaises(TypeError):
            m.ImageMetadataEvent(1, bad().payload)
        with self.assertRaises(TypeError):
            m.BadPixelList(0, 0, [])
        self.assertIs(type(report().payload.platform_attitude), m.Euler)
        self.assertIs(type(euler().payload.attitude), m.Euler)
        with self.assertRaises(TypeError):
            m.ImageMetadataStream()

    def test_native_status_preserved_with_unexpected_event_and_one_receive(self):
        for status in (n.AMS_MEL_TIMEOUT, n.AMS_MEL_STREAM_STOPPED, n.AMS_MEL_PROVIDER_FAILED,
                       n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR, n.AMS_MEL_RESOURCE_EXHAUSTED):
            _, _, metadata = self.owners()
            native = n.ams_mel_ir_image_metadata_receive
            def failed(*args):
                self.assertEqual(native(*args), n.AMS_MEL_OK)
                return status
            with mock.patch.object(n, P + "receive", side_effect=failed) as receive, \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_metadata_event_close) as close:
                failure = error(self, _errors._STATUS_KINDS[status], metadata.receive, status)
                self.assertEqual(failure.diagnostic_required, 1)
                self.assertEqual(receive.call_count, 1)
                self.assertEqual(close.call_count, 1)

    def test_success_without_event_and_failed_view_cleanup(self):
        _, _, metadata = self.owners()
        receive = n.ams_mel_ir_image_metadata_receive
        def empty(*args):
            status = receive(*args)
            n.ams_mel_ir_image_metadata_event_close(args[2], None, 0, None)
            return status
        with mock.patch.object(n, P + "receive", side_effect=empty):
            error(self, PROTOCOL, metadata.receive)
        native = n.ams_mel_ir_image_metadata_event_view
        def failed(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, P + "event_view", side_effect=failed), \
                mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_metadata_event_close) as close:
            error(self, m.ErrorKind.PROVIDER_FAILED, metadata.receive, n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(close.call_count, 1)

    def test_malformed_diagnostics_release_events(self):
        for stage in ("receive", "event_view", "event_close"):
            for mutation in ("zero", "utf8", "nul", "unterminated", "success-message", "oversized"):
                _, _, metadata = self.owners()
                native = getattr(n, P + stage)
                def malformed(*args):
                    status = native(*args)
                    diagnostic, required = args[-3], args[-1]
                    text = {"zero": b"\0", "utf8": b"\xff\0", "nul": b"a\0b\0",
                            "unterminated": b"x", "success-message": b"x\0", "oversized": b"\0"}[mutation]
                    c.memmove(diagnostic, text, len(text))
                    c.cast(required, c.POINTER(c.c_size_t))[0] = (
                        0 if mutation == "zero" else 99999 if mutation == "oversized" else len(text))
                    return status
                with mock.patch.object(n, P + stage, side_effect=malformed), \
                        mock.patch.object(_image_metadata, "_event_from_raw", wraps=_image_metadata._event_from_raw):
                    error(self, m.ErrorKind.INVALID_UTF8 if mutation == "utf8" else PROTOCOL, metadata.receive)

    def test_conversion_exceptions_survive_cleanup_failure(self):
        for exception in (MemoryError("primary"), RuntimeError("primary"), KeyboardInterrupt()):
            _, _, metadata = self.owners()
            native = n.ams_mel_ir_image_metadata_event_close
            def close(*args):
                native(*args)
                raise RuntimeError("secondary cleanup")
            with mock.patch.object(_image_metadata, "_event_from_raw", side_effect=exception), \
                    mock.patch.object(n, P + "event_close", side_effect=close) as cleanup:
                with self.assertRaises(type(exception)) as caught:
                    metadata.receive()
                self.assertIs(caught.exception, exception)
                self.assertEqual(cleanup.call_count, 1)

    def test_pixel_conversion_memory_error_is_cleaned_up(self):
        _, _, metadata = self.owners()
        with mock.patch.object(_image_convert, "_pixel_from_raw", side_effect=MemoryError), \
                mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_image_metadata_event_close) as close:
            error(self, m.ErrorKind.INTERNAL_ERROR, metadata.receive)
            self.assertEqual(close.call_count, 1)

    def test_event_close_consumed_retained_failure_and_success_inconsistency(self):
        for consumed, status in ((True, n.AMS_MEL_PROVIDER_FAILED), (False, n.AMS_MEL_PROVIDER_FAILED),
                                 (False, n.AMS_MEL_OK)):
            _, _, metadata = self.owners()
            native = n.ams_mel_ir_image_metadata_event_close
            retained = n.IrImageMetadataEventHandle()
            def close(owner, *args):
                if consumed:
                    native(owner, *args)
                else:
                    retained.value = c.cast(owner, c.POINTER(c.c_void_p))[0]
                    # A real call on a NULL owner supplies an honest diagnostic.
                    native(c.byref(c.c_void_p()), *args)
                return status
            try:
                with mock.patch.object(n, P + "event_close", side_effect=close) as cleanup:
                    error(self, m.ErrorKind.PROVIDER_FAILED if status != 0 else PROTOCOL, metadata.receive)
                    self.assertEqual(cleanup.call_count, 1)
            finally:
                if retained.value:
                    native(c.byref(retained), None, 0, None)  # Test-only reclaim, never production retry.

    def test_metadata_close_consumed_retained_failure_and_success_inconsistency(self):
        for consumed, status in ((True, n.AMS_MEL_PROVIDER_FAILED), (False, n.AMS_MEL_PROVIDER_FAILED),
                                 (False, n.AMS_MEL_OK)):
            _, image, metadata = self.owners()
            native = metadata._close_function
            def close(owner, *args):
                native(owner if consumed else c.byref(c.c_void_p()), *args)
                return status
            try:
                with mock.patch.object(metadata, "_close_function", side_effect=close) as cleanup:
                    error(self, m.ErrorKind.PROVIDER_FAILED if status != 0 else PROTOCOL, metadata.close)
                    if consumed:
                        metadata.close()
                        self.assertFalse(metadata.is_open)
                    else:
                        self.assertTrue(metadata.is_open)
                        error(self, PROTOCOL, metadata.close)
                        error(self, m.ErrorKind.INVALID_ARGUMENT, metadata.receive)
                        error(self, m.ErrorKind.INVALID_ARGUMENT, metadata.counters)
                    metadata.__del__()
                    metadata.__del__()
                    self.assertEqual(cleanup.call_count, 1)
                self.assertTrue(image.is_open)
            finally:
                native(metadata._owner_pointer, None, 0, None)

    def test_open_failure_after_publication_preserves_original_and_closes_once(self):
        session = m.Session.open(provider(), "image-metadata-sync", "")
        self.addCleanup(session.close)
        image = session.open_image_stream(frames.ImageStreamTests.config())
        self.addCleanup(image.close)
        native_open = n.ams_mel_ir_image_metadata_open
        native_close = m.ImageMetadataStream._close_function
        def failed(*args):
            self.assertEqual(native_open(*args), n.AMS_MEL_OK)
            return n.AMS_MEL_PROVIDER_FAILED
        def cleanup(*args):
            native_close(*args)
            raise MemoryError("secondary")
        with mock.patch.object(n, P + "open", side_effect=failed), \
                mock.patch.object(m.ImageMetadataStream, "_close_function", staticmethod(mock.Mock(side_effect=cleanup))):
            error(self, m.ErrorKind.PROVIDER_FAILED, image.open_metadata, n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(m.ImageMetadataStream._close_function.call_count, 1)
        self.assertTrue(image.is_open)
        error(self, m.ErrorKind.INVALID_ARGUMENT, image.open_metadata)

    def test_null_open_success_closed_image_and_full_uint64_counters(self):
        _, image, metadata = self.owners()
        native = n.ams_mel_ir_image_metadata_get_counters
        def high(owner, output, *args):
            status = native(owner, output, *args)
            c.cast(output, c.POINTER(n.IrC2MetadataCountersV1))[0] = n.IrC2MetadataCountersV1(
                (1 << 64) - 1, 1 << 63, (1 << 64) - 2)
            return status
        with mock.patch.object(n, P + "get_counters", side_effect=high):
            self.assertEqual(metadata.counters(), m.ImageMetadataCounters((1 << 64) - 1, 1 << 63, (1 << 64) - 2))
        with mock.patch.object(n, P + "receive") as receive:
            for timeout in (-1, 1 << 32, True, False, "0", .5, None):
                error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: metadata.receive(timeout))
            receive.assert_not_called()
        image.close()
        with mock.patch.object(n, P + "open") as op:
            error(self, m.ErrorKind.INVALID_ARGUMENT, image.open_metadata)
            op.assert_not_called()
        session = m.Session.open(provider(), "image-metadata-sync", "")
        self.addCleanup(session.close)
        image = session.open_image_stream(frames.ImageStreamTests.config())
        self.addCleanup(image.close)
        native_open = n.ams_mel_ir_image_metadata_open
        def empty(*args):
            status = native_open(*args)
            m.ImageMetadataStream._close_function(args[2], None, 0, None)
            return status
        with mock.patch.object(n, P + "open", side_effect=empty):
            error(self, PROTOCOL, image.open_metadata)

    def test_finalization_one_attempt_nonraising_and_no_python_parents(self):
        session, image, metadata = self.owners()
        session_ref, image_ref = weakref.ref(session), weakref.ref(image)
        # Cleanup callbacks retain parents; remove those references for this check.
        self._cleanups.clear()
        session.close()
        image.close()
        del session, image
        gc.collect()
        self.assertIsNone(session_ref())
        self.assertIsNone(image_ref())
        self.assertEqual(metadata.receive(), bad())
        native = metadata._close_function
        def close(*args):
            native(*args)
            raise RuntimeError("finalizer must not raise")
        with mock.patch.object(metadata, "_close_function", side_effect=close) as cleanup:
            metadata.__del__()
            metadata.__del__()
            self.assertEqual(cleanup.call_count, 1)
        self.assertFalse(metadata.is_open)

    def test_context_manager_preserves_application_exception(self):
        _, _, metadata = self.owners()
        original = RuntimeError("application")
        native = metadata._close_function
        def close(*args):
            native(*args)
            raise MemoryError("cleanup")
        with mock.patch.object(metadata, "_close_function", side_effect=close):
            with self.assertRaises(RuntimeError) as caught:
                with metadata:
                    raise original
            self.assertIs(caught.exception, original)

    def test_registration_never_starts_image_or_opens_nuc(self):
        session = m.Session.open(provider(), "image-metadata-sync", "")
        self.addCleanup(session.close)
        image = session.open_image_stream(frames.ImageStreamTests.config())
        self.addCleanup(image.close)
        with mock.patch.object(n, "ams_mel_ir_stream_start") as start, \
                mock.patch.object(n, "ams_mel_ir_image_nuc_open") as nuc:
            with image.open_metadata() as metadata:
                self.assertEqual(metadata.receive(), bad())
            start.assert_not_called()
            nuc.assert_not_called()

    def test_automatic_collection_closes_once_without_retaining_parent(self):
        session = m.Session.open(provider(), "image-metadata-sync", "")
        self.addCleanup(session.close)
        image = session.open_image_stream(frames.ImageStreamTests.config())
        self.addCleanup(image.close)
        metadata = image.open_metadata()
        reference = weakref.ref(metadata)
        native = metadata._close_function
        calls = []
        def close(*args):
            calls.append(1)
            native(*args)
            raise RuntimeError("non-raising finalization")
        metadata._close_function = close
        del metadata
        gc.collect()
        self.assertIsNone(reference())
        self.assertEqual(calls, [1])
        self.assertTrue(image.is_open)
        image.start()
        self.assertEqual(image.receive(1000).pixels, bytes(range(16, 28)))

    def test_valid_uint32_timeouts_forward_once_and_closed_owner_rejects_locally(self):
        _, _, metadata = self.owners()
        with mock.patch.object(n, P + "receive", wraps=n.ams_mel_ir_image_metadata_receive) as receive:
            for timeout in (0, 1, (1 << 32) - 1):
                metadata.receive(timeout)
                self.assertEqual(receive.call_args.args[1], timeout)
            self.assertEqual(receive.call_count, 3)
        metadata.close()
        metadata.close()
        with mock.patch.object(n, P + "receive") as receive, \
                mock.patch.object(n, P + "get_counters") as counters:
            error(self, m.ErrorKind.INVALID_ARGUMENT, metadata.receive)
            error(self, m.ErrorKind.INVALID_ARGUMENT, metadata.counters)
            error(self, m.ErrorKind.INVALID_ARGUMENT, metadata.__enter__)
            receive.assert_not_called()
            counters.assert_not_called()


if __name__ == "__main__":
    unittest.main()
