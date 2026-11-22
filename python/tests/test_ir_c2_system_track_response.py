"""PY-MEL-026: compiled mock, hostile FFI, and isolated C2 request lifetime."""

from contextlib import ExitStack, contextmanager
import copy
import ctypes as c
from dataclasses import FrozenInstanceError, fields, replace
from decimal import Decimal
import gc
import itertools
import math
import os
from pathlib import Path
import pickle
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _c2_system_track_response as impl, _native as n
from ams_mel._image_types import AzimuthElevation
import test_control_channel as control
from test_ir_erase_command import IntLike, IntSubclass, failure, mock_library, probe
from test_ir_health import provider
from test_ir_image_los_quaternion import diagnostic


SUBMIT = 'ams_mel_ir_c2_submit_system_track_data_response'
WAIT = 'ams_mel_ir_c2_system_response_request_wait'
CLOSE = 'ams_mel_ir_c2_system_response_request_close'
TRACK = ('ams_mel_ir_track_submit_system_track_data_response',
         'ams_mel_ir_track_system_response_request_wait',
         'ams_mel_ir_track_system_response_request_close')
SEND_FAMILY, RECLAMATION_FAMILY = 12, 11
MAX_U32 = (1 << 32) - 1
MIN_I64, MAX_I64 = -(1 << 63), (1 << 63) - 1
RAW = n.IrC2SystemResponseResultV1
VALUE = m.SystemTrackDataResponse(
    -8765432109876, 0xF1234567, 0xE2345678, 0xD3456789,
    123456.75, -456.125, 12.5, -0.875, True, True,
    AzimuthElevation(-1.25, 0.625), AzimuthElevation(0.03125, -0.015625),
)
ECHO = 'C2 system response accepted µ'
LONG = 'x' * 510 + '€' + 'y' * 100
INVALID, INTERNAL = m.ErrorKind.INVALID_ARGUMENT, m.ErrorKind.INTERNAL_ERROR
PROTOCOL, UTF8 = m.ErrorKind.PROTOCOL_INCONSISTENCY, m.ErrorKind.INVALID_UTF8
REJECTED = n.AMS_MEL_COMMAND_REJECTED


@contextmanager
def channel_for(scenario='c2-sys-echo', *, enabled=True):
    with m.Session.open(provider(), scenario, '') as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            if enabled:
                channel.enable()
            yield channel


def observe():
    with mock_library() as library:
        last = probe(library, 'mock_c2_system_response_last', c.POINTER(c.c_uint64), c.POINTER(c.c_uint))
        slots, sends = (c.c_uint64 * 14)(), c.c_uint()
        seen = last(slots, c.byref(sends))
        return (tuple(slots) if seen else None), sends.value


def bits(value):
    return struct.unpack('=Q', struct.pack('=d', value))[0]


def unbits(value):
    return struct.unpack('=d', struct.pack('=Q', value))[0]


def expected(command_id=VALUE.command_id, state=m.C2CommandState.ACCEPTED,
             reason=m.C2CannotComply.NOT_SET, text=ECHO):
    return m.C2SystemTrackDataCompleted(m.C2CommandStatus(command_id, state, reason, text))


class CompiledMockTests(unittest.TestCase):
    def test_full_record_and_only_c2_dispatch(self):
        with channel_for(enabled=False) as channel, ExitStack() as stack:
            spies = {name: stack.enter_context(mock.patch.object(n, name, wraps=getattr(n, name)))
                     for name in (SUBMIT, WAIT, CLOSE)}
            forbidden = [stack.enter_context(mock.patch.object(n, name, side_effect=AssertionError(name)))
                         for name in (*TRACK, 'ams_mel_ir_track_open', 'ams_mel_ir_c2_metadata_open')]
            before = observe()[1]
            failure(self, m.ErrorKind.PROVIDER_FAILED,
                    lambda: channel.submit_system_track_data_response(VALUE), n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(observe()[1], before)
            channel.enable()
            with channel.submit_system_track_data_response(VALUE) as request:
                self.assertEqual(request.wait(5000), expected())
                self.assertEqual(request.wait(), expected())
            self.assertEqual(observe(), ((
                (-8765432109876) % (1 << 64), 0xF1234567, 0xE2345678, 0xD3456789,
                bits(123456.75), bits(-456.125), bits(12.5), bits(-0.875), 1, 1,
                bits(-1.25), bits(0.625), bits(0.03125), bits(-0.015625),
            ), before + 1))
            self.assertEqual([spies[name].call_count for name in (SUBMIT, WAIT, CLOSE)], [2, 2, 1])
            for spy in forbidden:
                spy.assert_not_called()

    def test_boundaries_flags_and_raw_ieee_values(self):
        ieee = (-0.0, 0.0, 5e-324, -5e-324, math.inf, -math.inf, math.nan, -2.5)
        float_slots = (4, 5, 6, 7, 10, 11, 12, 13)
        with channel_for() as channel:
            for shift in range(8):
                numbers = ieee[shift:] + ieee[:shift]
                flags = tuple(itertools.product((False, True), repeat=2))[shift % 4]
                ids = tuple(MAX_U32 if shift & (1 << i) else 0 for i in range(3))
                stamp = MIN_I64 if shift % 2 else MAX_I64
                response = m.SystemTrackDataResponse(
                    stamp, *ids, *numbers[:4], *flags,
                    AzimuthElevation(*numbers[4:6]), AzimuthElevation(*numbers[6:]),
                )
                with self.subTest(shift=shift), channel.submit_system_track_data_response(response) as req:
                    self.assertEqual(req.wait(5000), expected(ids[0]))
                    raw, _ = observe()
                    self.assertEqual(raw[:4], (stamp % (1 << 64), *ids))
                    self.assertEqual(raw[8:10], tuple(map(int, flags)))
                    for slot, value in zip(float_slots, numbers):
                        if math.isnan(value):
                            self.assertTrue(math.isnan(unbits(raw[slot])))
                        else:
                            self.assertEqual(raw[slot], bits(value))

    def test_all_status_alternatives_and_full_text(self):
        self.assertEqual([int(state) for state in m.C2CommandState], list(range(5)))
        self.assertEqual([int(reason) for reason in m.C2CannotComply], list(range(47)))
        with channel_for('c2-sys-derived') as channel:
            for reason in m.C2CannotComply:
                state = m.C2CommandState(int(reason) % 5)
                response = replace(VALUE, request_id=int(state), track_id=int(reason), range_m=0.0)
                with self.subTest(state=state, reason=reason), channel.submit_system_track_data_response(response) as req:
                    result = req.wait(5000)
                    self.assertIs(type(result), m.C2SystemTrackDataCompleted)
                    self.assertIs(type(result.status), m.C2CommandStatus)
                    self.assertIs(result.status.state, state)
                    self.assertIs(result.status.reason, reason)
                    self.assertEqual(result, expected(state=state, reason=reason, text='C2 derived µ'))
            for variant, text in ((1.0, LONG), (4.0, '')):
                value = replace(VALUE, request_id=int(m.C2CommandState.REJECTED), track_id=0, range_m=variant)
                with channel.submit_system_track_data_response(value) as req:
                    result = req.wait(5000)
                    self.assertEqual(result, expected(state=m.C2CommandState.REJECTED, text=text))
                    self.assertEqual(req.wait(), result)
                self.assertEqual(result.status.reason_description, text)

    def test_error_or_rejections_including_614_byte_diagnostic(self):
        self.assertEqual(len(LONG.encode('utf-8')) + 1, 614)
        cases = (
            ('c2-sys-reject', m.MelErrorCode.INVALID_PARAMETERS, 'C2 system response rejected µ'),
            ('c2-sys-reject-long', m.MelErrorCode.INSUFFICIENT_RESOURCES, LONG),
            ('no-c2-system-response', m.MelErrorCode.UNSUPPORTED, ''),
        )
        for scenario, code, text in cases:
            with self.subTest(scenario=scenario), channel_for(scenario) as channel:
                with channel.submit_system_track_data_response(VALUE) as req:
                    result = req.wait(5000)
                    self.assertEqual(req.wait(), result)
                self.assertEqual(result, m.C2SystemTrackDataRejected(code, text))

    def test_provider_wait_failures(self):
        cases = [(f'c2-sys-{name}', VALUE) for name in
                 ('reject-utf8', 'reject-nul', 'unknown-error', 'null', 'future-throw')]
        base = replace(VALUE, request_id=0, track_id=0, range_m=0.0)
        cases.extend(('c2-sys-derived', replace(base, **change)) for change in (
            {'request_id': 5}, {'track_id': 47}, {'range_m': 2.0}, {'range_m': 3.0},
        ))
        for scenario, value in cases:
            with self.subTest(scenario=scenario, value=value), channel_for(scenario) as channel:
                with channel.submit_system_track_data_response(value) as req:
                    throws = scenario.endswith('throw')
                    kind = m.ErrorKind.PROVIDER_EXCEPTION if throws else m.ErrorKind.PROVIDER_FAILED
                    status = n.AMS_MEL_PROVIDER_EXCEPTION if throws else n.AMS_MEL_PROVIDER_FAILED
                    error = failure(self, kind, lambda: req.wait(5000), status)
                    failure(self, kind, req.wait, status, error.diagnostic)

    def test_provider_send_failures_do_not_retry(self):
        for scenario, kind, status in (
            ('c2-sys-send-throw', m.ErrorKind.PROVIDER_EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION),
            ('c2-sys-invalid-future', m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED),
        ):
            with self.subTest(scenario=scenario), channel_for(scenario) as channel:
                before = observe()[1]
                failure(self, kind, lambda: channel.submit_system_track_data_response(VALUE), status)
                self.assertEqual(observe()[1], before + 1)

    def test_request_unique_and_values_frozen(self):
        with channel_for() as channel, mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            req = channel.submit_system_track_data_response(VALUE)
            for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                with self.assertRaises(TypeError):
                    operation(req)
            result = req.wait(5000)
            self.assertEqual(req.wait(), result)
            req.close()
            req.close()
            self.assertFalse(req.is_open)
            failure(self, INVALID, req.wait)
            close.assert_called_once()
        for value in (VALUE, VALUE.inertial_az_el, result, result.status,
                      m.C2SystemTrackDataRejected(m.MelErrorCode.UNSUPPORTED, '')):
            with self.assertRaises(FrozenInstanceError):
                setattr(value, fields(value)[0].name, None)
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)

    def test_documented_mock_example(self):
        path = Path(__file__).resolve().parents[2] / 'docs/py-mel-026-c2-system-track-response.md'
        example, = re.findall(r'^```python\n(.*?)^```', path.read_text(encoding='utf-8'), re.M | re.S)
        subprocess.run([sys.executable, '-W', 'error', '-', str(provider())],
                       input=example, text=True, check=True, timeout=20)

    def test_isolated_lifecycle_and_failpoints(self):
        for case in ('admission', 'session-first', 'c2-first', 'gc-parents', 'finalize-observer',
                     'finish-failure', 'post-send-allocation', 'post-send-worker-launch'):
            with self.subTest(case=case):
                proc = subprocess.run([sys.executable, '-W', 'error', __file__, '--child', case],
                                      capture_output=True, text=True, timeout=30)
                self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)


def wait_reply(args, *, status=n.AMS_MEL_OK, text=b'\0', required=None,
               code=n.AMS_MEL_ERROR_NONE, state=2, reason=0, view=None):
    _, _, output, buffer, capacity, size = args
    raw = c.cast(output, c.POINTER(RAW)).contents
    raw.error_code = code
    raw.status.command_id = 888  # Deliberately different from the submitted ID.
    raw.status.state, raw.status.reason_id = state, reason
    if view is not None:
        raw.status.reason_description = view
    c.memmove(buffer, text, min(len(text), capacity))
    c.cast(size, c.POINTER(c.c_size_t)).contents.value = len(text) if required is None else required
    return status


class HostileFFITests(unittest.TestCase):
    def test_malformed_input_never_dispatches(self):
        numbers = ('range_m', 'range_rate_mps', 'range_error_m', 'range_rate_error_mps')
        ids = ('command_id', 'request_id', 'track_id')
        false_numbers = (True, '1.5', complex(1), 10**400, Decimal('1e400'))
        invalid = [(name, value) for name in numbers for value in false_numbers]
        invalid.extend((name, value) for name in ids for value in
                       (True, -1, MAX_U32 + 1, 1.0, IntSubclass(1), IntLike()))
        invalid.extend(('system_time_ns', value) for value in
                       (False, MIN_I64 - 1, MAX_I64 + 1, 1.0, IntSubclass(0), IntLike()))
        invalid.extend((name, value) for name in ('az_el_valid', 'range_valid') for value in (0, 1, 2, ''))
        incomplete = object.__new__(AzimuthElevation)
        invalid.extend((name, value) for name in ('inertial_az_el', 'az_el_error') for value in
                       ((1.0, 2.0), None, incomplete))
        with channel_for() as channel, mock.patch.object(n, SUBMIT) as submit:
            for name, value in invalid:
                with self.subTest(field=name, value=value):
                    failure(self, INVALID, lambda: replace(VALUE, **{name: value}))
                    forced = replace(VALUE)
                    object.__setattr__(forced, name, value)
                    failure(self, INVALID, lambda: channel.submit_system_track_data_response(forced))
            for field in ('azimuth_rad', 'elevation_rad'):
                for bad in false_numbers:
                    failure(self, INVALID, lambda: replace(AzimuthElevation(0.0, 0.0), **{field: bad}))
                for pair in ('inertial_az_el', 'az_el_error'):
                    forced = replace(VALUE, **{pair: AzimuthElevation(0.0, 0.0)})
                    object.__setattr__(getattr(forced, pair), field, Decimal('1e400'))
                    failure(self, INVALID, lambda: channel.submit_system_track_data_response(forced))
            for bad in (None, object.__new__(m.SystemTrackDataResponse)):
                failure(self, INVALID, lambda: channel.submit_system_track_data_response(bad))
            channel.close()
            failure(self, INVALID, lambda: channel.submit_system_track_data_response(VALUE))
            submit.assert_not_called()

    def test_encode_uses_validated_snapshot(self):
        with channel_for() as channel:
            value = replace(VALUE, az_el_error=replace(VALUE.az_el_error))
            real_record = n.IrSystemTrackDataResponseV1

            def allocate(*args):
                object.__setattr__(value, 'command_id', -1)
                object.__setattr__(value, 'az_el_valid', 2)
                object.__setattr__(value.az_el_error, 'elevation_rad', None)
                return real_record(*args)

            with mock.patch.object(n, 'IrSystemTrackDataResponseV1', side_effect=allocate):
                with channel.submit_system_track_data_response(value) as request:
                    self.assertEqual(request.wait(5000), expected())
                    raw, _ = observe()
                    self.assertEqual((raw[1], raw[8], raw[13]), (VALUE.command_id, 1, bits(-0.015625)))

    def test_missing_owner_and_pre_send_allocation(self):
        with channel_for() as channel, mock.patch.object(n, SUBMIT) as submit:
            send = lambda: channel.submit_system_track_data_response(VALUE)
            for name in ('IrAzElV1', 'IrSystemTrackDataResponseV1', 'IrC2SystemResponseRequestHandle'):
                with mock.patch.object(n, name, side_effect=MemoryError):
                    failure(self, INTERNAL, send)
            with mock.patch.object(c, 'create_string_buffer', side_effect=MemoryError):
                failure(self, INTERNAL, send)
            submit.assert_not_called()

            def missing(*args):
                diagnostic(args[3], args[4], args[5], b'')
                return n.AMS_MEL_OK

            submit.side_effect = missing
            failure(self, PROTOCOL, send)
            submit.assert_called_once()

    def test_failed_submit_one_best_effort_close_and_no_retry(self):
        for kind, status in ((INVALID, n.AMS_MEL_INVALID_ARGUMENT),
                             (m.ErrorKind.RESOURCE_EXHAUSTED, n.AMS_MEL_RESOURCE_EXHAUSTED),
                             (m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED),
                             (m.ErrorKind.PROVIDER_EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION),
                             (INTERNAL, n.AMS_MEL_INTERNAL_ERROR)):
            def failed(*args):
                c.cast(args[2], c.POINTER(c.c_void_p)).contents.value = 1  # Not dereferenced.
                diagnostic(args[3], args[4], args[5], b'primary')
                return status

            with (self.subTest(kind=kind), channel_for() as channel,
                  mock.patch.object(n, SUBMIT, side_effect=failed) as submit,
                  mock.patch.object(n, CLOSE, side_effect=KeyboardInterrupt) as close):
                failure(self, kind, lambda: channel.submit_system_track_data_response(VALUE), status, 'primary')
                gc.collect()
                submit.assert_called_once()
                close.assert_called_once()

    def test_timeout_never_retried_and_argument_validation(self):
        with channel_for() as channel, channel.submit_system_track_data_response(VALUE) as req:
            with mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(
                    a, status=n.AMS_MEL_TIMEOUT, required=2000)) as wait:
                for timeout in (-1, MAX_U32 + 1, 2.0, True, IntSubclass(1), IntLike()):
                    failure(self, INVALID, lambda: req.wait(timeout))
                wait.assert_not_called()
                error = failure(self, m.ErrorKind.TIMEOUT, lambda: req.wait(MAX_U32), n.AMS_MEL_TIMEOUT)
                self.assertEqual(error.diagnostic_required, 2000)
                self.assertIsNone(error.diagnostic)
                self.assertEqual(wait.call_args.args[1], MAX_U32)
                wait.assert_called_once()
            self.assertEqual(req.wait(5000), expected())

    def test_success_validates_only_complete_status(self):
        null = n.StringViewV1(None, 1)
        large = n.StringViewV1(c.cast(c.c_void_p(1), n.CharPointer), sys.maxsize + 1)
        last = -1 % (1 << (8 * c.sizeof(c.c_void_p)))
        wraps = n.StringViewV1(c.cast(c.c_void_p(last), n.CharPointer), 2)
        with channel_for() as channel, channel.submit_system_track_data_response(VALUE) as req:
            for kind, kwargs in (
                (PROTOCOL, {'state': 5}), (PROTOCOL, {'reason': 47}),
                (PROTOCOL, {'code': n.AMS_MEL_ERROR_INVALID_ID}),
                (PROTOCOL, {'text': b''}), (PROTOCOL, {'text': b'x'}),
                (PROTOCOL, {'text': b'x\0'}), (PROTOCOL, {'required': 0}),
                (PROTOCOL, {'required': 700}), (PROTOCOL, {'required': sys.maxsize + 1}),
                (PROTOCOL, {'view': null}), (PROTOCOL, {'view': large}), (PROTOCOL, {'view': wraps}),
            ):
                with self.subTest(kwargs=kwargs), mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(a, **kwargs)) as wait:
                    failure(self, kind, req.wait)
                    wait.assert_called_once()
            for payload, kind in ((b'\xff', UTF8), (b'a\0b', PROTOCOL), ('reason €'.encode(), None)):
                backing = c.create_string_buffer(b'_' + payload)
                # A char span has alignment 1: an odd byte address is legal, not misaligned.
                view = n.StringViewV1(c.cast(c.byref(backing, 1), n.CharPointer), len(payload))
                with mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(a, view=view)):
                    if kind is not None:
                        failure(self, kind, req.wait)
                    else:
                        result = req.wait()
                        c.memset(backing, 0xFF, len(backing))
                        self.assertEqual(result, expected(888, text='reason €'))

    def test_rejection_and_failure_do_not_read_inactive_fields(self):
        class RejectedRaw(RAW):
            @property
            def status(self):
                raise AssertionError('inactive CommandStatus accessed')

        class FailedRaw(RejectedRaw):
            @property
            def error_code(self):
                raise AssertionError('inactive error code accessed')

        encoded = LONG.encode() + b'\0'
        unknown = 0xF1234567
        with channel_for() as channel, channel.submit_system_track_data_response(VALUE) as req:
            for status, kind, raw_type in (
                (REJECTED, None, RejectedRaw),
                (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED, FailedRaw),
                (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION, FailedRaw),
                (n.AMS_MEL_INTERNAL_ERROR, INTERNAL, FailedRaw),
            ):
                with (self.subTest(status=status),
                      mock.patch.object(n, 'IrC2SystemResponseResultV1', raw_type),
                      mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(
                          a, status=status, code=unknown, text=encoded)) as wait):
                    if kind is None:
                        result = req.wait(41)
                        self.assertEqual(result.code.name, 'UNKNOWN_0xF1234567')
                        self.assertEqual(result.description, LONG)
                    else:
                        failure(self, kind, lambda: req.wait(41), status, LONG)
                    self.assertEqual([(call.args[1], call.args[4]) for call in wait.call_args_list],
                                     [(41, 512), (0, 614)])

    def test_bad_rejection_diagnostic_and_cache_mismatch(self):
        with channel_for() as channel, channel.submit_system_track_data_response(VALUE) as req:
            for text, kind in ((b'a\0b\0', PROTOCOL), (b'\xff\0', UTF8), (b'bad', PROTOCOL), (b'', PROTOCOL)):
                with mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(a, status=REJECTED, text=text)):
                    failure(self, kind, req.wait)
            for second in ({'status': n.AMS_MEL_OK}, {'required': 0}, {'required': 600}, {'code': 2}):
                original = dict(status=REJECTED, code=1, text=LONG.encode() + b'\0')
                with self.subTest(second=second), mock.patch.object(n, WAIT, side_effect=lambda *a: wait_reply(
                        a, **(original if a[1] else original | second))) as wait:
                    failure(self, PROTOCOL, lambda: req.wait(1))
                    self.assertEqual(wait.call_count, 2)

    def test_copy_allocation_failure_exposes_no_partial_result(self):
        with channel_for() as channel, channel.submit_system_track_data_response(VALUE) as req:
            with (mock.patch.object(c, 'string_at', side_effect=MemoryError),
                  mock.patch.object(impl, 'C2SystemTrackDataCompleted') as completed):
                failure(self, INTERNAL, lambda: req.wait(5000))
                completed.assert_not_called()
            self.assertEqual(req.wait(), expected())

    def test_close_allocation_failure_is_retryable(self):
        with channel_for() as channel, mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            req = channel.submit_system_track_data_response(VALUE)
            self.assertEqual(req.wait(5000), expected())
            with mock.patch.object(c, 'create_string_buffer', side_effect=MemoryError):
                with self.assertRaises(MemoryError):
                    req.close()
            close.assert_not_called()
            self.assertTrue(req.is_open)
            req.close()
            close.assert_called_once()

    def test_uncertain_close_disarms_and_preserves_exception(self):
        real_close = getattr(n, CLOSE)
        for mode in ('failed', 'unconsumed', 'raised'):
            original = RuntimeError('uncertain consumption')

            def uncertain(*args):
                owner = c.cast(args[0], c.POINTER(c.c_void_p)).contents
                stale = owner.value
                real_close(args[0], None, 0, None)  # Release the real owner exactly once.
                owner.value = stale
                if mode == 'raised':
                    raise original
                diagnostic(args[1], args[2], args[3], b'')
                return n.AMS_MEL_OK if mode == 'unconsumed' else n.AMS_MEL_INTERNAL_ERROR

            with (self.subTest(mode=mode), channel_for() as channel,
                  mock.patch.object(n, CLOSE, side_effect=uncertain) as close,
                  mock.patch.object(n, WAIT) as wait):
                req = channel.submit_system_track_data_response(VALUE)
                primary = ValueError('primary')
                with self.assertRaises(ValueError) as caught:
                    with req:
                        raise primary
                self.assertIs(caught.exception, primary)
                self.assertFalse(req.is_open)
                if mode == 'raised':
                    with self.assertRaises(RuntimeError) as error:
                        req.close()
                    self.assertIs(error.exception, original)
                    with self.assertRaises(RuntimeError) as repeated:
                        req.wait()
                    self.assertIs(repeated.exception, original)
                else:
                    kind = PROTOCOL if mode == 'unconsumed' else INTERNAL
                    status = None if mode == 'unconsumed' else n.AMS_MEL_INTERNAL_ERROR
                    error = failure(self, kind, req.close, status)
                    self.assertIs(failure(self, kind, req.wait, status), error)
                req.__del__()
                del req
                gc.collect()
                close.assert_called_once()
                wait.assert_not_called()

    def test_finalization_is_nonraising_and_nonrepeating(self):
        real_close = getattr(n, CLOSE)

        def raising(*args):
            real_close(*args)
            raise KeyboardInterrupt

        with channel_for() as channel, mock.patch.object(n, CLOSE, side_effect=raising) as close:
            req = channel.submit_system_track_data_response(VALUE)
            self.assertEqual(req.wait(5000), expected())
            req.__del__()
            self.assertFalse(req.is_open)
            del req
            gc.collect()
            close.assert_called_once()


def release(count, family=SEND_FAMILY):
    with mock_library() as library:
        return probe(library, 'mock_completion_release_family', c.c_uint, c.c_uint)(family, count)


def boundary(test, op=3, target=0):
    fn = probe(n._LIBRARY, 'ams_mel_test_completion_boundary',
               c.c_uint, c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    values = (c.c_uint64 * 4)()
    test.assertEqual(fn(RECLAMATION_FAMILY, op, target, values), 1)
    return tuple(values)


def await_reclamation(test, consumed):
    # Another request's terminal Wait is not evidence that a closed sibling's
    # worker consumed its future or released its admission and provider graph.
    actual, _, created, _ = boundary(test, 1, consumed)
    test.assertEqual(actual, consumed)
    fn = probe(n._LIBRARY, 'ams_mel_test_completion_owner', c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    observed = c.c_uint64()
    test.assertEqual(fn(RECLAMATION_FAMILY, created, c.byref(observed)), 1)
    test.assertEqual(observed.value, created)


def child(case):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix='py026-lifetime-') as folder:
        log = Path(folder) / 'events.log'
        log.touch()
        os.environ['AMS_MEL_TEST_LIFETIME_LOG'] = str(log)
        if case == 'finish-failure':
            os.environ['AMS_MEL_TEST_C2_FINISH_FAILURE'] = 'exception'
        scenario = 'c2-sys-mixed' if case == 'admission' else 'c2-sys-held'
        session = m.Session.open(provider(), scenario, '', max_async_requests=2)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        channel.enable()
        teardown = ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded')

        def retained():
            with mock_library():  # RTLD_NOLOAD and no lasting test-owned provider reference.
                pass
            test.assertTrue(set(teardown).isdisjoint(log.read_text().splitlines()))

        def timeout(req, milliseconds=0):
            failure(test, m.ErrorKind.TIMEOUT, lambda: req.wait(milliseconds), n.AMS_MEL_TIMEOUT)
            test.assertTrue(req.is_open)

        def exhausted():
            failure(test, m.ErrorKind.RESOURCE_EXHAUSTED,
                    lambda: channel.submit_system_track_data_response(VALUE),
                    n.AMS_MEL_RESOURCE_EXHAUSTED, 'async request limit reached')

        if case.startswith('post-send-'):
            failpoint = case.removeprefix('post-send-')
            os.environ['AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE'] = failpoint
            text = ('facade allocation failed after provider send' if failpoint == 'allocation'
                    else 'facade worker launch failed')
            with mock.patch.object(n, CLOSE) as close:
                failure(test, INTERNAL, lambda: channel.submit_system_track_data_response(VALUE),
                        n.AMS_MEL_INTERNAL_ERROR, text)
                close.assert_not_called()
            test.assertEqual(observe()[1], 1)
            channel.close()
            session.close()
            retained()
            test.assertEqual(release(1), 1)
            test.assertEqual(boundary(test)[0], 0)
            retained()  # Unpublished future/claim intentionally retained by native.
            return

        first = channel.submit_system_track_data_response(VALUE)
        if case == 'finish-failure':
            channel.close()
            session.close()
            retained()
            test.assertEqual(release(1), 1)
            for milliseconds in (5000, 0):
                failure(test, m.ErrorKind.PROVIDER_FAILED, lambda: first.wait(milliseconds),
                        n.AMS_MEL_PROVIDER_FAILED, 'deferred C2 cleanup failed')
            first.close()
            await_reclamation(test, 1)
            retained()
            return

        alternate = replace(VALUE, command_id=MAX_U32)
        second = channel.submit_system_track_data_response(alternate)
        test.assertIsNot(first, second)
        timeout(first, 1)
        timeout(second)
        exhausted()
        if case in ('admission', 'c2-first'):
            first.close()
        if case == 'finalize-observer':
            ref = weakref.ref(first)
            del first
            gc.collect()
            test.assertIsNone(ref())
        exhausted()
        test.assertEqual(observe()[1], 2)

        if case == 'admission':
            # A different Session has its own permit even when this one is full.
            with m.Session.open(provider(), 'c2-sys-echo', '', max_async_requests=1) as other:
                with other.open_control_channel(control.ControlChannelTests.config()) as other_channel:
                    other_channel.enable()
                    with other_channel.submit_system_track_data_response(VALUE) as separate:
                        test.assertEqual(separate.wait(5000), expected())
                    exhausted()
            test.assertEqual(observe()[1], 3)
            test.assertEqual(release(1, family=5), 0)  # No Track completions to release.
            timeout(second)
            test.assertEqual(release(2), 1)
            test.assertEqual(second.wait(5000), expected(MAX_U32))
            await_reclamation(test, 3)
            with channel.submit_system_track_data_response(VALUE) as third:
                timeout(third)
                test.assertEqual(observe()[1], 4)
                test.assertEqual(release(1), 1)
                test.assertEqual(third.wait(5000), expected())
            second.close()
            channel.close()
            session.close()
            await_reclamation(test, 4)
            return

        if case == 'session-first':
            session.close()
            timeout(second)
            channel.close()
        elif case == 'gc-parents':
            parents = weakref.ref(channel), weakref.ref(session)
            del channel, session
            gc.collect()
            test.assertTrue(all(ref() is None for ref in parents))
        else:
            channel.close()
            timeout(second)
            session.close()
        retained()
        boundary(test, 0)
        try:
            test.assertEqual(release(2), 1)
            boundary(test, 1, 2)  # All released futures are parked before native finish.
            retained()
            timeout(second)
        finally:
            boundary(test, 2)
        completed = second.wait(5000)
        test.assertEqual(completed, expected(MAX_U32))
        if case not in ('c2-first', 'finalize-observer'):
            test.assertEqual(first.wait(5000), expected())
        await_reclamation(test, 2)
        events = log.read_text().splitlines()
        test.assertEqual(events.count(f'future_consumed_{RECLAMATION_FAMILY}'), 2)
        test.assertEqual([event for event in events if event in teardown], list(teardown))
        with test.assertRaises(OSError), mock_library():
            test.fail('mock still loaded after all native workers reclaimed their graph')
        test.assertEqual(second.wait(), completed)
        second.close()
        if case != 'finalize-observer':
            first.close()
        test.assertEqual(pickle.loads(pickle.dumps(completed)), expected(MAX_U32))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        child(sys.argv[2])
    else:
        unittest.main()