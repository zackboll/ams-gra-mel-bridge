"""Track v3 metadata: compiled fixtures, ownership faults, and isolated lifetime."""

from contextlib import contextmanager
import copy
import ctypes as c
from dataclasses import replace
import gc
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
from ams_mel import _native as n, _track_convert, _track_metadata
from test_common_channel import assert_owned_graph
from test_ir_health import provider
from test_ir_navigation import diagnostic
from test_ir_track_foundation import config, DESTROY, FAILED, INVALID, PROTOCOL, error


OPEN = 'ams_mel_ir_track_metadata_open'
RECEIVE = 'ams_mel_ir_track_metadata_receive'
VIEW = 'ams_mel_ir_track_metadata_event_view_v3'
RELEASE = 'ams_mel_ir_track_metadata_event_close'
CLOSE = 'ams_mel_ir_track_metadata_close'
COUNTERS = 'ams_mel_ir_track_metadata_get_counters'
TIMEOUT = m.ErrorKind.TIMEOUT
STOPPED = m.ErrorKind.STREAM_STOPPED
K = m.TrackMetadataKind

REPORT = m.TrackMetadataEvent(K.IRST_TRACK_REPORT, m.IRSTTrackReport(
    -1234567890123, 0xF1234567, m.NorthEastDown(-1.25, 2.5, -3.75), 4.125, -5.25,
    m.NorthEastDown(6.5, -7.75, 8.875), -9.125, 10.25, 123456.75, 654.5,
    0.0125, 0.875, -0.5, 9876543210, m.IRSTTrackState.COAST, m.IRSTTrackMode.STARE,
))
REQUEST = m.TrackMetadataEvent(K.REQUEST_SYSTEM_TRACK_DATA, m.RequestSystemTrackData(
    -8765432109876, 0xC1234567, 0xD2345678, 0xE3456789,
))
CANDIDATE = m.TrackMetadataEvent(K.CANDIDATE_OBJECT_MESSAGE, m.CandidateObjectMessage(
    m.CandidateObjectHeader(3, 0xBEEF, c.c_float(1.5309e-7).value, 0xA5C3, -4433221100998877),
    m.SensorInertialState(
        -1122334455667788, m.Quaternion(0.125, -0.25, 0.375, -0.5),
        m.Quaternion(-0.625, 0.75, -0.875, 1.125),
        m.Directional(1234567.25, -2345678.5, 3456789.75),
        m.Directional(-11.125, 22.25, -33.375), m.Uncertainty(0xC0FFEE01, 0xDEADBE02),
    ),
    (m.HotRegion(m.HotRegionType.FLARE, 1111, 2222, 3333, 4444, 5555),
     m.HotRegion(m.HotRegionType.SOLAR, 6666, 7777, 8888, 9999, 10111),
     m.HotRegion(m.HotRegionType.MASK, 12222, 13333, 14444, 15555, 16666)),
    tuple(m.CandidateObject(
        -1000000000000 - i * 7, 0x11110000 + i, 0x22220000 + i,
        m.RowCol(100.5 + i, 200.25 + i), 3000.125 + i,
        m.Directional(0.1 + i, -0.2 - i, 0.3 + i), 40.5 + i, -50.75 - i,
    ) for i in range(3)),
))


def preproc_entry(i):
    j = i * 100
    return m.CandidateObjectPreProc(
        -2000000000000 - i * 13, 0x33330000 + i, 0x44440000 + i,
        m.RowCol(400.5 + i, 900.25 + i), 5000.125 + i,
        m.Directional(0.4 + i, -0.5 - i, 0.6 + i), 60.5 + i, -70.75 - i,
        ((1000 + j, -1001 - j, 1002 + j), (-1003 - j, 1004 + j, -1005 - j),
         (1006 + j, -1007 - j, 1008 + j)),
        -80.375 - i, 1.5 + i, -90.625 - i,
        m.SensorInertialState(
            -3344556677889900 - i,
            m.Quaternion(0.75 + i, -0.8125 - i, 0.875 + i, -0.9375 - i),
            m.Quaternion(-1.0625 - i, 1.125 + i, -1.1875 - i, 1.25 + i),
            m.Directional(111111.5 + i, -222222.25 - i, 333333.125 + i),
            m.Directional(-77.125 - i, 88.25 + i, -99.375 - i),
            m.Uncertainty(0xA1B2C300 + i, 0xD4E5F600 + i),
        ),
        i == 1, 0.03125 + i, -0.015625 - i, 123.4375 + i,
    )


PREPROC = m.TrackMetadataEvent(K.CANDIDATE_OBJECT_PREPROC_MESSAGE, m.CandidateObjectPreProcMessage(
    m.CandidateObjectHeader(3, 0xC0DE, c.c_float(2.7183e-6).value, 0x5A3C, -9988776655443322),
    m.SensorInertialState(
        -5544332211009988, m.Quaternion(0.0625, -0.125, 0.1875, -0.25),
        m.Quaternion(-0.3125, 0.375, -0.4375, 0.5),
        m.Directional(7654321.5, -8765432.25, 9876543.125),
        m.Directional(-44.625, 55.75, -66.875), m.Uncertainty(0xBADF00D1, 0xFEEDBEE2),
    ),
    (m.HotRegion(m.HotRegionType.SOLAR, 2001, 2002, 2003, 2004, 2005),
     m.HotRegion(m.HotRegionType.MASK, 3001, 3002, 3003, 3004, 3005)),
    (preproc_entry(0), preproc_entry(1)),
))
MIXED = (REPORT, CANDIDATE, REQUEST, PREPROC)


@contextmanager
def track_for(scenario):
    with m.Session.open(provider(), scenario, '') as session:
        with session.open_track_channel(config()) as track:
            yield track


class TrackMetadataTests(unittest.TestCase):
    def check_snapshots(self, stream, expected, counters):
        self.assertEqual(stream.counters(), counters)
        copied = tuple(stream.receive() for _ in expected)
        self.assertEqual(copied, expected)
        for value in copied:
            assert_owned_graph(self, value)
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
        error(self, TIMEOUT, lambda: stream.receive(1), n.AMS_MEL_TIMEOUT)
        self.assertEqual(stream.counters(), counters)

    def test_complete_rich_payloads_and_zero_request(self):
        for scenario, expected in (
            ('track-report', REPORT), ('track-request-rich', REQUEST),
            ('track-request-zero', replace(REQUEST, payload=m.RequestSystemTrackData(0, 0, 0, 0))),
            ('track-candidate-rich', CANDIDATE), ('track-preproc-rich', PREPROC),
            ('track-request-register-not-supported', REPORT),
        ):
            with self.subTest(scenario=scenario), track_for(scenario) as track, track.open_metadata() as stream:
                self.check_snapshots(stream, (expected,), m.TrackMetadataCounters(1, 0, 0))

    def test_shared_fifo_and_drop_incoming(self):
        for scenario, expected, capacities in (
            ('track-metadata-mixed', (REQUEST, REPORT, REQUEST), (8,)),
            ('track-candidate-mixed', (REPORT, CANDIDATE, REQUEST), (8,)),
            ('track-preproc-mixed', MIXED, (8, 1, 2, 3)),
        ):
            for capacity in capacities:
                with self.subTest(scenario=scenario, capacity=capacity), track_for(scenario) as track:
                    track.enable()
                    with track.open_metadata(capacity) as stream:
                        self.check_snapshots(stream, expected[:capacity], m.TrackMetadataCounters(
                            len(expected), max(0, len(expected) - capacity), 0))

    def test_each_family_overflows(self):
        for family, event in (('report', REPORT), ('request', REQUEST),
                              ('candidate', CANDIDATE), ('preproc', PREPROC)):
            with self.subTest(family=family), track_for(f'track-{family}-overflow') as track:
                with track.open_metadata(2) as stream:
                    expected = []
                    for i in range(2):
                        payload = event.payload
                        if family == 'report':
                            payload = replace(payload, activity_id=i)
                        elif family == 'request':
                            payload = replace(payload, request_id=i)
                        else:
                            payload = replace(payload, header=replace(payload.header, stack_frame_index=i))
                        expected.append(replace(event, payload=payload))
                    self.check_snapshots(stream, tuple(expected), m.TrackMetadataCounters(6, 4, 0))

    def test_empty_and_malformed_callbacks(self):
        for suffix in ('report-none', 'report-null', 'report-bad-state', 'report-bad-mode',
                       'request-null', 'candidate-null', 'candidate-too-many',
                       'candidate-bad-region', 'preproc-null', 'preproc-bad-region'):
            with self.subTest(suffix=suffix), track_for(f'track-{suffix}') as track, track.open_metadata() as stream:
                bad = int(suffix != 'report-none')
                self.check_snapshots(stream, (), m.TrackMetadataCounters(bad, 0, bad))

    def test_native_failed_registration_is_one_shot(self):
        for family, refusals in (('report', ('fail', 'throw')), ('request', ('fail', 'throw')),
                                 ('candidate', ('not-supported', 'fail', 'unknown', 'throw')),
                                 ('preproc', ('fail', 'unknown', 'throw'))):
            for refusal in refusals:
                with self.subTest(family=family, refusal=refusal), \
                        track_for(f'track-{family}-register-{refusal}') as track, \
                        mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
                    status = n.AMS_MEL_PROVIDER_EXCEPTION if refusal == 'throw' else n.AMS_MEL_PROVIDER_FAILED
                    kind = m.ErrorKind.PROVIDER_EXCEPTION if refusal == 'throw' else FAILED
                    error(self, kind, track.open_metadata, status)
                    error(self, INVALID, track.open_metadata)
                    opened.assert_called_once()

    def test_local_validation_before_native_and_unique_owner(self):
        with track_for('track-report') as track, mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
            for value in (-1, 0, c.c_size_t(-1).value + 1, True, None, '1', 1.5):
                error(self, INVALID, lambda: track.open_metadata(value))
            for target, attribute in ((n, 'IrTrackMetadataHandle'), (c, 'pointer'),
                                      (c, 'create_string_buffer'), (c, 'byref')):
                with mock.patch.object(target, attribute, side_effect=MemoryError):
                    with self.assertRaises(MemoryError):
                        track.open_metadata()
            opened.assert_not_called()
            with track.open_metadata(c.c_size_t(-1).value) as stream:
                with mock.patch.object(n, RECEIVE, wraps=getattr(n, RECEIVE)) as received:
                    for value in (-1, 1 << 32, True, None, '1', 1.5):
                        error(self, INVALID, lambda: stream.receive(value))
                    received.assert_not_called()
                    self.assertEqual(stream.receive(0xFFFFFFFF), REPORT)
                for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                    with self.assertRaises(TypeError):
                        operation(stream)
            self.assertFalse(stream.is_open)
            stream.close()
            error(self, INVALID, stream.receive)
            error(self, INVALID, stream.counters)
            error(self, INVALID, track.open_metadata)
            opened.assert_called_once()

    def test_event_constructor_requires_exact_kind_and_payload(self):
        class DerivedReport(m.IRSTTrackReport):
            pass

        for kind, payload in ((1, REPORT.payload), (K.IRST_TRACK_REPORT, REQUEST.payload),
                              (K.IRST_TRACK_REPORT, DerivedReport(**vars(REPORT.payload)))):
            with self.assertRaises(TypeError):
                m.TrackMetadataEvent(kind, payload)
        with self.assertRaises(TypeError):
            m.TrackMetadataStream()


def slot(output, typ):
    return c.cast(output, c.POINTER(typ)).contents


class TrackMetadataFFITests(unittest.TestCase):
    @contextmanager
    def native_view(self, stream, view):
        """Use a real received event owner, replacing only its borrowed view."""
        def publish(*args):
            c.cast(args[1], c.POINTER(type(view)))[0] = view
            return diagnostic(args)

        def forbidden(*args):
            self.fail('Track metadata must only use the v3 view')

        with mock.patch.object(n, VIEW, side_effect=publish) as viewed, \
                mock.patch.object(n, 'ams_mel_ir_track_metadata_event_view', side_effect=forbidden), \
                mock.patch.object(n, 'ams_mel_ir_track_metadata_event_view_v2', side_effect=forbidden), \
                mock.patch.object(n, RELEASE, wraps=getattr(n, RELEASE)) as released:
            yield stream.receive
            viewed.assert_called_once()
            released.assert_called_once()

    def test_open_ownership_faults_consume_the_attempt(self):
        primary = KeyboardInterrupt('registration interrupted')
        for mode in ('null', 'failed-owner', 'interrupted-owner'):
            with self.subTest(mode=mode), track_for('track-report') as track:
                real_open = getattr(n, OPEN)

                def opened(*args):
                    if mode == 'null':
                        return diagnostic(args)
                    self.assertEqual(real_open(*args), n.AMS_MEL_OK)
                    if mode == 'interrupted-owner':
                        raise primary
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED)

                real_close = getattr(n, CLOSE)

                def close_then_raise(*args):
                    self.assertEqual(real_close(*args), n.AMS_MEL_OK)
                    raise RuntimeError('cleanup must not mask Open')

                with mock.patch.object(n, OPEN, side_effect=opened) as attempted, \
                        mock.patch.object(n, CLOSE, side_effect=close_then_raise) as closed:
                    if mode == 'null':
                        error(self, PROTOCOL, track.open_metadata)
                    elif mode == 'failed-owner':
                        error(self, FAILED, track.open_metadata, n.AMS_MEL_PROVIDER_FAILED)
                    else:
                        with self.assertRaises(KeyboardInterrupt) as caught:
                            track.open_metadata()
                        self.assertIs(caught.exception, primary)
                    error(self, INVALID, track.open_metadata)
                    gc.collect()
                    attempted.assert_called_once()
                    self.assertEqual(closed.call_count, int(mode != 'null'))

    def test_receive_ownership_faults(self):
        for mode in ('null', 'failed-owner', 'interrupted-owner'):
            with self.subTest(mode=mode), track_for('track-report') as track, track.open_metadata() as stream:
                real_receive = getattr(n, RECEIVE)

                def receive(*args):
                    self.assertIsNone(slot(args[2], n.IrTrackMetadataEventHandle).value)
                    if mode == 'null':
                        return diagnostic(args)
                    self.assertEqual(real_receive(*args), n.AMS_MEL_OK)
                    if mode == 'interrupted-owner':
                        raise KeyboardInterrupt('receive interrupted')
                    return diagnostic(args, n.AMS_MEL_TIMEOUT)

                with mock.patch.object(n, RECEIVE, side_effect=receive) as received, \
                        mock.patch.object(n, VIEW) as viewed, \
                        mock.patch.object(n, RELEASE, wraps=getattr(n, RELEASE)) as released:
                    if mode == 'null':
                        error(self, PROTOCOL, stream.receive)
                    elif mode == 'failed-owner':
                        error(self, TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
                    else:
                        with self.assertRaisesRegex(KeyboardInterrupt, 'receive interrupted'):
                            stream.receive()
                    received.assert_called_once()
                    viewed.assert_not_called()
                    self.assertEqual(released.call_count, int(mode != 'null'))

    def test_complete_fixed_extent_checked_before_dereference(self):
        typ = n.IrTrackMetadataEventV3
        backing = typ()
        address_limit = c.c_size_t(-1).value + 1
        for address in (0, c.addressof(backing) + 1, address_limit - c.alignment(typ)):
            with self.subTest(address=address), track_for('track-report') as track, track.open_metadata() as stream:
                with self.native_view(stream, c.cast(address, c.POINTER(typ))) as receive:
                    error(self, PROTOCOL, receive)

    def test_only_active_payload_is_interpreted(self):
        raw = n.IrTrackMetadataEventV3()
        raw.base.base.kind = K.REQUEST_SYSTEM_TRACK_DATA
        raw.base.base.request_system_track_data = n.IrRequestSystemTrackDataV1(
            -(1 << 63), 0xFFFFFFFF, 0, 0x80000000)
        raw.base.base.track_report.state = 0xFFFFFFFF
        raw.base.base.track_report.mode = 0xFFFFFFFF
        raw.base.candidate_object_message.header.number_of_cos = 0xFFFF
        raw.candidate_object_preproc_message.candidate_object_preprocs.size = c.c_size_t(-1).value
        with track_for('track-report') as track, track.open_metadata() as stream:
            with self.native_view(stream, c.pointer(raw)) as receive:
                self.assertEqual(receive(), m.TrackMetadataEvent(
                    K.REQUEST_SYSTEM_TRACK_DATA, m.RequestSystemTrackData(-(1 << 63), 0xFFFFFFFF, 0, 0x80000000)))

    def test_invalid_discriminator_enums_bool_and_candidate_count(self):
        for field in ('kind', 'state', 'mode', 'region', 'edge', 'fixed-limit', 'fixed-mismatch'):
            with self.subTest(field=field), track_for('track-report') as track, track.open_metadata() as stream:
                raw = n.IrTrackMetadataEventV3()
                raw.base.base.kind = K.IRST_TRACK_REPORT
                if field == 'kind':
                    raw.base.base.kind = 0xFFFFFFFF
                elif field in ('state', 'mode'):
                    setattr(raw.base.base.track_report, field, 4 if field == 'state' else 3)
                elif field == 'edge':
                    raw.base.base.kind = K.CANDIDATE_OBJECT_PREPROC_MESSAGE
                    preproc = n.IrCandidateObjectPreProcV1(edge=2)
                    raw.candidate_object_preproc_message.candidate_object_preprocs = (
                        n.IrCandidateObjectPreProcSpanV1(c.pointer(preproc), 1))
                else:
                    raw.base.base.kind = K.CANDIDATE_OBJECT_MESSAGE
                    message = raw.base.candidate_object_message
                    if field == 'region':
                        region = n.IrHotRegionV1(kind=4)
                        message.hot_regions = n.IrHotRegionSpanV1(c.pointer(region), 1)
                    elif field == 'fixed-limit':
                        candidates = (n.IrCandidateObjectV1 * 901)()
                        message.header.number_of_cos = 901
                        message.candidate_objects = n.IrCandidateObjectSpanV1(candidates, 901)
                    else:
                        message.header.number_of_cos = 1
                with self.native_view(stream, c.pointer(raw)) as receive:
                    error(self, PROTOCOL, receive)

    def test_invalid_nested_spans(self):
        typ = n.IrHotRegionV1
        storage = typ()
        limit = c.c_size_t(-1).value + 1
        for address, size in ((0, 1), (c.addressof(storage) + 1, 1),
                              (limit - c.alignment(typ), 1), (c.addressof(storage), limit - 1)):
            with self.subTest(address=address, size=size), track_for('track-report') as track, track.open_metadata() as stream:
                raw = n.IrTrackMetadataEventV3()
                raw.base.base.kind = K.CANDIDATE_OBJECT_PREPROC_MESSAGE
                raw.candidate_object_preproc_message.hot_regions = n.IrHotRegionSpanV1(
                    c.cast(address, c.POINTER(typ)), size)
                with self.native_view(stream, c.pointer(raw)) as receive:
                    error(self, PROTOCOL, receive)

    def test_preproc_large_vector_repeats_empty_span_and_numeric_extremes(self):
        raw = n.IrTrackMetadataEventV3()
        raw.base.base.kind = K.CANDIDATE_OBJECT_PREPROC_MESSAGE
        message = raw.candidate_object_preproc_message
        message.header = n.IrCandidateObjectHeaderV1(0xFFFF, 0xFFFF, -0.0, 0xFFFF, (1 << 63) - 1)
        message.hot_regions = n.IrHotRegionSpanV1(c.cast(1, c.POINTER(n.IrHotRegionV1)), 0)
        entries = (n.IrCandidateObjectPreProcV1 * 901)()
        samples = (-32768, 32767, -1, 0, 12345, -23456, 127, -129, 256)
        for i in (0, 900):
            entries[i].candidate_object_with_background.samples[:] = samples
            entries[i].intensity = float('-inf')
            entries[i].az_sigma = float('inf')
            entries[i].background_normalizer = float('nan')
            entries[i].inertial_state.q_xyzw.y = -0.0
        message.candidate_object_preprocs = n.IrCandidateObjectPreProcSpanV1(entries, len(entries))
        with track_for('track-report') as track, track.open_metadata() as stream:
            with self.native_view(stream, c.pointer(raw)) as receive:
                payload = receive().payload
        assert_owned_graph(self, payload)
        self.assertEqual(payload.header, m.CandidateObjectHeader(0xFFFF, 0xFFFF, -0.0, 0xFFFF, (1 << 63) - 1))
        self.assertEqual(math.copysign(1, payload.header.cfar), -1)
        self.assertEqual(payload.hot_regions, ())
        self.assertEqual(len(payload.candidate_object_preprocs), 901)
        self.assertEqual(payload.candidate_object_preprocs[1], payload.candidate_object_preprocs[899])
        for item in (payload.candidate_object_preprocs[0], payload.candidate_object_preprocs[-1]):
            self.assertEqual(item.candidate_object_with_background, (samples[:3], samples[3:6], samples[6:]))
            self.assertEqual(item.intensity, float('-inf'))
            self.assertEqual(item.az_sigma, float('inf'))
            self.assertTrue(math.isnan(item.background_normalizer))
            self.assertEqual(math.copysign(1, item.inertial_state.q_xyzw.y), -1)

    def test_full_uint64_counters(self):
        expected = m.TrackMetadataCounters((1 << 64) - 1, (1 << 63) + 1, (1 << 64) - 2)

        def write(*args):
            raw = slot(args[1], n.IrC2MetadataCountersV1)
            raw.events_received = expected.events_received
            raw.events_dropped_queue_full = expected.events_dropped_queue_full
            raw.malformed_or_unsupported = expected.malformed_or_unsupported
            return diagnostic(args)

        with track_for('track-report') as track, track.open_metadata() as stream, \
                mock.patch.object(n, COUNTERS, side_effect=write) as queried:
            self.assertEqual(stream.counters(), expected)
            queried.assert_called_once()

    def test_empty_candidate_prefix_and_unbounded_independent_hot_regions(self):
        raw = n.IrTrackMetadataEventV3()
        raw.base.base.kind = K.CANDIDATE_OBJECT_MESSAGE
        message = raw.base.candidate_object_message
        message.candidate_objects = n.IrCandidateObjectSpanV1(
            c.cast(1, c.POINTER(n.IrCandidateObjectV1)), 0)
        region = n.IrHotRegionV1(0, 0xFFFF, 1, 2, 3, 4)
        regions = (n.IrHotRegionV1 * 901)(*([region] * 901))
        message.hot_regions = n.IrHotRegionSpanV1(regions, len(regions))
        with track_for('track-report') as track, track.open_metadata() as stream:
            with self.native_view(stream, c.pointer(raw)) as receive:
                payload = receive().payload
        self.assertEqual(payload.header.number_of_cos, 0)
        self.assertEqual(payload.candidate_objects, ())
        self.assertEqual(payload.hot_regions, (m.HotRegion(m.HotRegionType.INVALID, 0xFFFF, 1, 2, 3, 4),) * 901)

    def test_late_copy_failure_preserves_primary_and_closes_event(self):
        real_copy = _track_convert._preproc
        primary = MemoryError('second entry')

        def convert(raw):
            if raw.edge:
                raise primary
            return real_copy(raw)

        real_release = getattr(n, RELEASE)

        def fail_cleanup(*args):
            self.assertEqual(real_release(*args), n.AMS_MEL_OK)
            raise KeyboardInterrupt('must not replace allocation failure')

        with track_for('track-preproc-rich') as track, track.open_metadata() as stream, \
                mock.patch.object(_track_convert, '_preproc', side_effect=convert) as copied, \
                mock.patch.object(n, RELEASE, side_effect=fail_cleanup) as released:
            error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
            self.assertEqual(copied.call_count, 2)
            released.assert_called_once()
            error(self, TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)

    def test_event_release_failures_never_publish_or_repeat(self):
        real_release = getattr(n, RELEASE)
        for mode in ('consumed-failure', 'stale-success', 'stale-exception'):
            def close(*args):
                output = slot(args[0], n.IrTrackMetadataEventHandle)
                original = output.value
                self.assertEqual(real_release(*args), n.AMS_MEL_OK)
                self.assertIsNone(output.value)
                if mode == 'consumed-failure':
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED)
                output.value = original  # Never dereference or close this stale handle.
                if mode == 'stale-exception':
                    raise KeyboardInterrupt('event close interrupted')
                return diagnostic(args)

            with self.subTest(mode=mode), track_for('track-report') as track, track.open_metadata() as stream, \
                    mock.patch.object(n, RELEASE, side_effect=close) as released:
                if mode == 'stale-exception':
                    with self.assertRaisesRegex(KeyboardInterrupt, 'event close interrupted'):
                        stream.receive()
                elif mode == 'stale-success':
                    error(self, PROTOCOL, stream.receive)
                else:
                    error(self, FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
                released.assert_called_once()

    def test_stream_close_preparation_is_retryable_but_invocation_is_not(self):
        real_close = getattr(n, CLOSE)

        def fail_after_consuming(*args):
            output = slot(args[0], n.IrTrackMetadataHandle)
            original = output.value
            self.assertEqual(real_close(*args), n.AMS_MEL_OK)
            output.value = original
            raise KeyboardInterrupt('stream close interrupted')

        for context_error in (False, True):
            with self.subTest(context_error=context_error), track_for('track-report') as track, \
                    mock.patch.object(n, CLOSE, side_effect=fail_after_consuming) as closed:
                stream = track.open_metadata()
                with mock.patch.object(_track_metadata.ctypes, 'byref', side_effect=MemoryError):
                    with self.assertRaises(MemoryError):
                        stream.close()
                self.assertTrue(stream.is_open)
                closed.assert_not_called()
                if context_error:
                    with self.assertRaisesRegex(RuntimeError, 'primary context failure'):
                        with stream:
                            raise RuntimeError('primary context failure')
                else:
                    with self.assertRaisesRegex(KeyboardInterrupt, 'stream close interrupted'):
                        stream.close()
                self.assertFalse(stream.is_open)
                stream.close()
                error(self, INVALID, stream.receive)
                del stream
                gc.collect()
                closed.assert_called_once()
                self.assertTrue(track.is_open)


class TrackMetadataLifetimeTests(unittest.TestCase):
    def test_isolated_compiled_mock_lifetimes(self):
        for case in ('track-first', 'session-first', 'parent-gc', 'detach-retry',
                     'candidate-event', 'preproc-event', 'track-report-late',
                     'track-candidate-late', 'track-preproc-late', 'allocation',
                     'track-request-async', 'track-candidate-async', 'track-preproc-async',
                     'registration-callback', 'in-flight-stream-close'):
            with self.subTest(case=case), tempfile.TemporaryDirectory(prefix='track-meta-') as directory:
                root = Path(directory)
                log = root / 'lifetime.log'
                result = subprocess.run([sys.executable, '-W', 'error', __file__, case, directory],
                                        env=dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(log)),
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                events = log.read_text(encoding='utf-8').splitlines()
                self.assertEqual([event for event in events if event in DESTROY], DESTROY)
                self.assertFalse({'track_data_update_sent', 'track_system_track_data_response_sent',
                                  'c2_system_track_data_response_sent'} & set(events), events)


def isolated_case(case, root):
    test = unittest.TestCase()
    log = root / 'lifetime.log'
    barrier = root / 'callback'

    def events():
        return log.read_text(encoding='utf-8').splitlines()

    def order(*names):
        recorded = events()
        test.assertEqual([event for event in recorded if event in names], list(names), recorded)

    def wait_for_entry():
        # Poll only for the provider's deterministic signal, not for elapsed time.
        deadline = time.monotonic() + 5
        while not barrier.with_suffix('.entered').exists():
            if time.monotonic() >= deadline:
                test.fail('native callback did not reach its barrier')
            time.sleep(0.001)

    scenarios = {'track-first': 'track-preproc-mixed', 'session-first': 'track-preproc-mixed',
                 'parent-gc': 'track-preproc-mixed', 'detach-retry': 'track-detach-fail',
                 'allocation': 'track-report', 'registration-callback': 'track-report',
                 'in-flight-stream-close': 'track-common-metadata',
                 'candidate-event': 'track-candidate-lifetime', 'preproc-event': 'track-preproc-lifetime'}
    scenario = scenarios.get(case, case)
    if case == 'allocation':
        os.environ['AMS_MEL_TEST_TRACK_CALLBACK_FAILURE'] = 'allocation'
    if case in ('registration-callback', 'in-flight-stream-close'):
        os.environ['AMS_MEL_TEST_TRACK_CALLBACK_BARRIER'] = str(barrier)
    elif case.endswith('-async'):
        family = case.split('-')[1]
        os.environ[f'AMS_MEL_TEST_TRACK_{family.upper()}_BARRIER'] = str(barrier)

    session = m.Session.open(provider(), scenario, '')
    track = session.open_track_channel(config())
    test.assertFalse(any(name.endswith('_registered') for name in events()), events())

    if case == 'registration-callback':
        published = []
        worker = threading.Thread(target=lambda: published.append(track.open_metadata()))
        worker.start()
        try:
            wait_for_entry()
            test.assertEqual(published, [])
        finally:
            barrier.with_suffix('.release').touch()
            worker.join(timeout=5)
        test.assertFalse(worker.is_alive())
        stream, = published
        test.assertEqual(stream.receive(), REPORT)
        stream.close()
        track.close()
        session.close()
        order('track_report_registered', 'track_callback_entered', *DESTROY)
        return

    stream = track.open_metadata(8)
    track.enable()
    if case in ('track-first', 'session-first', 'parent-gc'):
        if case == 'track-first':
            track.close()
            session.close()
        elif case == 'session-first':
            session.close()
            test.assertNotIn('library_unloaded', events())
            track.close()
        else:
            session_ref, track_ref = weakref.ref(session), weakref.ref(track)
            del track, session
            gc.collect()
            test.assertIsNone(track_ref())
            test.assertIsNone(session_ref())
        test.assertTrue(stream.is_open)
        order('track_report_registered', 'track_candidate_object_registered',
              'track_request_system_track_data_registered', 'track_candidate_object_preproc_registered',
              *DESTROY)
        copied = tuple(stream.receive() for _ in MIXED)
        error(test, STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)
        test.assertEqual(stream.counters(), m.TrackMetadataCounters(4, 0, 0))
        stream.close()
        test.assertEqual(copied, MIXED)
        for value in copied:
            assert_owned_graph(test, value)
        return

    if case in ('candidate-event', 'preproc-event'):
        # The event owner is already acquired when Python asks for its view.
        # Close the entire provider graph before even requesting the view.
        view = getattr(n, VIEW)

        def teardown_before_view(*args):
            track.close()
            session.close()
            order(*DESTROY)
            test.assertTrue(stream.is_open)
            return view(*args)

        with mock.patch.object(n, VIEW, side_effect=teardown_before_view):
            copied = stream.receive()
        stream.close()
        test.assertEqual(copied, CANDIDATE if case == 'candidate-event' else PREPROC)
        assert_owned_graph(test, copied)
        return

    if case == 'detach-retry':
        error(test, FAILED, track.close, n.AMS_MEL_PROVIDER_FAILED,
              'Track detach failed; provider state retained')
        test.assertFalse(track.is_open)
        test.assertNotIn('library_unloaded', events())
        test.assertEqual(stream.receive(), REPORT)
        error(test, FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
        session.close()
        track.close()
        order('channel_detach_failed', *DESTROY)
        stream.close()
        return

    if case.endswith('-late'):
        prefix = {'track-report-late': 'track', 'track-candidate-late': 'track_candidate',
                  'track-preproc-late': 'track_preproc'}[case]
        stream.close()
        test.assertTrue(track.is_open)
        test.assertNotIn('track_disabled', events())
        track.close()
        session.close()
        order(f'{prefix}_late_callback_entered', f'{prefix}_late_callback_returned', *DESTROY)
        return

    if case == 'allocation':
        error(test, FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
        test.assertEqual(stream.counters().events_received, 1)
    elif case.endswith('-async'):
        try:
            error(test, TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
            test.assertEqual(stream.counters().events_received, 0)
        finally:
            barrier.touch()
        copied = stream.receive(5000)
        test.assertEqual(stream.counters(), m.TrackMetadataCounters(1, 0, 0))
    elif case == 'in-flight-stream-close':
        barrier.with_suffix('.start').touch()
        try:
            wait_for_entry()
            test.assertEqual(stream.counters(), m.TrackMetadataCounters(1, 0, 0))
            stream.close()  # Must not wait for the callback currently at the barrier.
            test.assertNotIn('track_callback_returned', events())
        finally:
            barrier.with_suffix('.release').touch()
    else:
        test.fail(f'unknown isolated lifetime case: {case}')
    stream.close()
    track.close()
    session.close()
    if case.endswith('-async'):
        order(f'track_{family}_emitted_async', f'track_{family}_async_returned', *DESTROY[1:])
        test.assertEqual(copied, {'request': REQUEST, 'candidate': CANDIDATE, 'preproc': PREPROC}[family])
    elif case == 'in-flight-stream-close':
        order('track_callback_entered', 'track_callback_returned', *DESTROY[1:])


if __name__ == '__main__':
    isolated_case(sys.argv[1], Path(sys.argv[2]))