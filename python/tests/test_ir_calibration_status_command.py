"""PY-MEL-024: compiled completion, hostile ABI, and native lifetime contracts."""

import _ctypes
import copy
import ctypes as c
from contextlib import contextmanager
import gc
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _calibration_status as conversion, _calibration_status_command as impl, _ffi, _native as n
import test_control_channel as control
from test_ir_calibration_status import EMPTY, NativeView, expected, probe
from test_ir_health import provider
from test_ir_health_metadata import assert_owned
from test_ir_image_los_quaternion import diagnostic


SUBMIT = 'ams_mel_ir_c2_submit_calibration_status'
WAIT = 'ams_mel_ir_calibration_status_request_wait'
CLOSE = 'ams_mel_ir_calibration_status_request_close'
MAX_UINT32 = (1 << 32) - 1
COMMAND = m.CalibrationStatusCommand(42, 7)
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
INVALID = m.ErrorKind.INVALID_ARGUMENT
INTERNAL = m.ErrorKind.INTERNAL_ERROR
UTF8 = m.ErrorKind.INVALID_UTF8
MOCK_RELEASE_FAMILY = 10
RECLAMATION_FAMILY = 9


def outcome(code, message):
    return m.CalibrationStatusRejected(m.MelErrorCode(code), message)


def response(*args, status=n.AMS_MEL_OK, raw=None, code=n.AMS_MEL_ERROR_NONE, text=b'',
             data=None, length=None):
    """Write to allocated Wait output; data optionally overrides diagnostic bytes."""
    output = c.cast(args[2], c.POINTER(n.IrCalibrationStatusResultV1))
    # Populate the wire layout independently of Python attribute access on read.
    record = n.IrCalibrationStatusResultV1(raw if raw is not None else n.IrCalibrationStatusV1(), code)
    c.memmove(output, c.byref(record), c.sizeof(record))
    buffer, capacity, required = args[-3:]
    diagnostic(buffer, capacity, required, text)
    if data is not None:
        c.memset(buffer, 0xFF, capacity)
        c.memmove(buffer, data, min(capacity, len(data)))
    if length is not None:
        c.cast(required, n.SizePointer)[0] = length
    return status


def provider_controls():
    library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
    try:
        observe = probe(library, 'mock_calibration_status_observe', c.POINTER(c.c_uint32),
                        c.POINTER(c.c_uint), c.POINTER(c.c_uint))
    finally:
        # Only query counters while a parent still retains the DSO.
        _ctypes.dlclose(library._handle)

    def counters():
        ids, sends, registrations = (c.c_uint32 * 2)(), c.c_uint(), c.c_uint()
        observe(ids, c.byref(sends), c.byref(registrations))
        return tuple(ids), sends.value, registrations.value

    def release(family, count):
        # Native workers can finish teardown as soon as the last promise is
        # set. Pin the test control's own provider stack until it returns.
        pin = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
        try:
            return probe(pin, 'mock_completion_release_family', c.c_uint, c.c_uint)(family, count)
        finally:
            _ctypes.dlclose(pin._handle)

    return release, counters


def await_reclamation(test):
    boundary = probe(n._LIBRARY, 'ams_mel_test_completion_boundary',
                     c.c_uint, c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    owner = probe(n._LIBRARY, 'ams_mel_test_completion_owner',
                  c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    values, observed = (c.c_uint64 * 4)(), c.c_uint64()
    test.assertEqual(boundary(RECLAMATION_FAMILY, 3, 0, values), 1)
    test.assertEqual(owner(RECLAMATION_FAMILY, values[2], c.byref(observed)), 1)


@contextmanager
def channel_for(scenario='calstatus-rich', *, enabled=True):
    with m.Session.open(provider(), scenario, '') as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            if enabled:
                channel.enable()
            yield channel


def failure(test, kind, call, status=None, message=None):
    with test.assertRaises(m.MelError) as raised:
        call()
    error = raised.exception
    test.assertIs(error.kind, kind)
    test.assertEqual(error.native_status, status)
    if message is not None:
        test.assertEqual(error.diagnostic, message)
        test.assertEqual(error.diagnostic_required, len(message.encode()) + 1)
    return error


class CalibrationStatusCommandTests(unittest.TestCase):
    def test_validation_encoding_admission_and_no_implicit_enable(self):
        class Index:
            def __index__(self):
                return 1

        class IntSubclass(int):
            pass

        with channel_for(enabled=False) as channel:
            _, observe = provider_controls()
            _, before_sends, before_registrations = observe()
            with mock.patch.object(n, SUBMIT) as submit:
                for value in (-1, MAX_UINT32 + 1, False, '7', 7.0, Index(), IntSubclass(7)):
                    for name in ('command_id', 'request_id'):
                        with self.subTest(value=value, field=name):
                            fields = dict(command_id=42, request_id=7)
                            failure(self, INVALID, lambda: m.CalibrationStatusCommand(**(fields | {name: value})))
                            cmd = m.CalibrationStatusCommand(**fields)
                            object.__setattr__(cmd, name, value)
                            failure(self, INVALID, lambda: channel.submit_calibration_status(cmd))
                failure(self, INVALID, lambda: channel.submit_calibration_status(
                    SimpleNamespace(command_id=42, request_id=7)))
            submit.assert_not_called()
            with mock.patch.object(n, 'ams_mel_ir_c2_enable') as enable:
                failure(self, m.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_calibration_status(COMMAND),
                        n.AMS_MEL_PROVIDER_FAILED, 'C2 channel is not enabled')
            enable.assert_not_called()
            self.assertEqual(observe()[1:], (before_sends, before_registrations))
            channel.enable()
            for sent, expected_sends in zip(((0, MAX_UINT32), (MAX_UINT32, 0), (42, 7)),
                                            range(before_sends + 1, before_sends + 4)):
                with self.subTest(sent=sent), channel.submit_calibration_status(m.CalibrationStatusCommand(*sent)) as req:
                    self.assertEqual(observe(), (sent, expected_sends, before_registrations))
                    self.assertEqual(req.wait(5000).status, expected(sent[0]))
            channel.close()
            with mock.patch.object(n, SUBMIT) as submit:
                failure(self, INVALID, lambda: channel.submit_calibration_status(COMMAND))
            submit.assert_not_called()

    def test_full_success_shapes_cached_wait_and_owned_copy(self):
        rich = expected(COMMAND.command_id)  # Exact fixture, independent of ctypes (also used by PY-MEL-023).
        active, completed = rich.active_calibration_ids, rich.completed_calibrations
        for shape, status in (
            ('rich', rich), ('empty', EMPTY), ('active-only', m.CalibrationStatus(active, ())),
            ('completed-only', m.CalibrationStatus((), completed)),
            ('one', m.CalibrationStatus((active[1],), (completed[2],))),
        ):
            with self.subTest(shape=shape), channel_for('calstatus-' + shape) as channel:
                with channel.submit_calibration_status(COMMAND) as req:
                    first, again = req.wait(5000), req.wait()
                    self.assertTrue(req.is_open)
                    self.assertEqual(first, m.CalibrationStatusCompleted(status))
                    self.assertEqual(again, first)
                    self.assertIsNot(first.status, again.status)
                    for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                        with self.assertRaises(TypeError):
                            operation(req)
                self.assertFalse(req.is_open)
                failure(self, INVALID, req.wait)
            assert_owned(self, first)
            self.assertEqual(pickle.loads(pickle.dumps(first)), m.CalibrationStatusCompleted(status))

    def test_native_rejections_failures_and_long_diagnostic(self):
        for scenario, code, text in (
            ('no-calstatus', m.MelErrorCode.UNSUPPORTED, ''),
            ('calstatus-reject', m.MelErrorCode.INVALID_STATE, 'calibration status rejected'),
            ('calstatus-reject-long', m.MelErrorCode.INVALID_STATE, 'r' * 700),
        ):
            with self.subTest(scenario=scenario), channel_for(scenario) as channel:
                with channel.submit_calibration_status(COMMAND) as req, mock.patch.object(
                        n, WAIT, wraps=getattr(n, WAIT)) as wait:
                    result = req.wait(5000)
                    self.assertIs(type(result), m.CalibrationStatusRejected)
                    self.assertIs(result.code, code)
                    self.assertEqual(result.description, text)
                    self.assertEqual([call.args[1] for call in wait.call_args_list],
                                     [5000, 0] if scenario.endswith('-long') else [5000])
                    self.assertEqual(req.wait(), result)

        malformed = ('sentinel', 'unknown', 'item-sentinel', 'item-unknown', 'nul', 'item-nul',
                     'utf8', 'item-utf8', 'completed-utf8')
        for suffix, status, kind, text in (
            ('null', n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED,
             'provider returned null successful CalibrationStatus'),
            ('future-throw', n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION,
             'mock calibration status future exception'),
            ('unknown-error', n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED,
             'provider returned unknown MEL error code'),
            *((name, n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED,
               'provider returned malformed CalibrationStatus data') for name in malformed),
        ):
            with self.subTest(suffix=suffix), channel_for('calstatus-' + suffix) as channel:
                with channel.submit_calibration_status(COMMAND) as req:
                    for timeout in (5000, 0):
                        failure(self, kind, lambda: req.wait(timeout), status, text)

    def test_native_send_exception_and_invalid_future_are_not_retried(self):
        for suffix, status, kind, text in (
            ('send-throw', n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION,
             'mock calibration status send exception'),
            ('invalid-future', n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED,
             'provider returned an invalid CalibrationStatusCmd future'),
        ):
            with self.subTest(suffix=suffix), channel_for('calstatus-' + suffix) as channel:
                _, observe = provider_controls()
                _, sends, registrations = observe()
                failure(self, kind, lambda: channel.submit_calibration_status(COMMAND), status, text)
                self.assertEqual(observe(), ((42, 7), sends + 1, registrations))

    def test_documented_example(self):
        document = Path(__file__).resolve().parents[2] / 'docs/py-mel-024-calibration-status-command.md'
        example = document.read_text().split('```python\n', 1)[1].split('```', 1)[0]
        scope = {}
        exec(compile(example, str(document), 'exec'), scope)
        self.assertEqual(scope['status'], expected(42))
        self.assertFalse(scope['request'].is_open)

    def test_pending_lifetime_and_native_failpoints_in_isolated_processes(self):
        for case in ('c2-first', 'session-first', 'gc-parents', 'close-observer', 'finalize-observer',
                     'sync-held', 'native-copy-failure', 'post-send-allocation', 'post-send-worker-launch'):
            with self.subTest(case=case):
                run = subprocess.run([sys.executable, '-W', 'error', str(Path(__file__).resolve()), '--child', case],
                                     env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)),
                                     capture_output=True, text=True, timeout=40)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class HostileCalibrationStatusCommandTests(unittest.TestCase):
    def test_invalid_timeout_does_not_wait(self):
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            with mock.patch.object(n, WAIT, side_effect=response) as wait:
                for value in (-1, MAX_UINT32 + 1, True, 1.0, '1', object()):
                    with self.subTest(value=value):
                        failure(self, INVALID, lambda: req.wait(value))
                wait.assert_not_called()
                self.assertEqual(req.wait(MAX_UINT32).status, EMPTY)
                self.assertEqual(wait.call_args.args[1], MAX_UINT32)

    def test_success_requires_an_owner_and_valid_empty_diagnostic(self):
        with channel_for() as channel, mock.patch.object(n, CLOSE) as close:
            with mock.patch.object(n, SUBMIT, side_effect=lambda *args: (
                    diagnostic(*args[-3:]) or n.AMS_MEL_OK)) as submit:
                failure(self, PROTOCOL, lambda: channel.submit_calibration_status(COMMAND))
                submit.assert_called_once()
            close.assert_not_called()

        # A provider accepted this request. Invalid success framing must not
        # retry the send or leak the published owner.
        submit_native = getattr(n, SUBMIT)

        def corrupted(*args):
            status = submit_native(*args)
            self.assertEqual(status, n.AMS_MEL_OK)
            diagnostic(*args[-3:], b'unexpected success text')
            return status

        with channel_for() as channel, mock.patch.object(n, SUBMIT, side_effect=corrupted) as submit, \
                mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            failure(self, PROTOCOL, lambda: channel.submit_calibration_status(COMMAND))
            submit.assert_called_once()
            close.assert_called_once()

    def test_failed_submit_with_unexpected_owner_preserves_primary_failure(self):
        for status, kind in (
            (n.AMS_MEL_INVALID_ARGUMENT, INVALID),
            (n.AMS_MEL_RESOURCE_EXHAUSTED, m.ErrorKind.RESOURCE_EXHAUSTED),
            (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED),
            (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
            (n.AMS_MEL_INTERNAL_ERROR, INTERNAL),
        ):
            with self.subTest(kind=kind), channel_for() as channel:
                opaque = c.c_byte()

                def refused(*args):
                    c.cast(args[2], c.POINTER(n.IrCalibrationStatusRequestHandle))[0] = c.addressof(opaque)
                    diagnostic(*args[-3:], b'original submission failure')
                    return status

                with mock.patch.object(n, SUBMIT, side_effect=refused) as submit, mock.patch.object(
                        n, CLOSE, side_effect=RuntimeError('cleanup also failed')) as close:
                    failure(self, kind, lambda: channel.submit_calibration_status(COMMAND), status,
                            'original submission failure')
                    gc.collect()
                    submit.assert_called_once()
                    close.assert_called_once()
                    self.assertEqual(close.call_args.args[1:], (None, 0, None))

    def test_allocation_failures_before_submit_or_close_leave_no_uncertain_send(self):
        with channel_for() as channel:
            for symbol in ('IrCalibrationStatusCommandV1', 'IrCalibrationStatusRequestHandle'):
                with self.subTest(symbol=symbol), mock.patch.object(n, symbol, side_effect=MemoryError), \
                        mock.patch.object(n, SUBMIT) as submit, mock.patch.object(n, CLOSE) as close:
                    failure(self, INTERNAL, lambda: channel.submit_calibration_status(COMMAND))
                    submit.assert_not_called()
                    close.assert_not_called()
            with mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
                req = channel.submit_calibration_status(COMMAND)
                try:
                    with mock.patch.object(c, 'create_string_buffer', side_effect=MemoryError):
                        with self.assertRaises(MemoryError):
                            req.close()
                    close.assert_not_called()
                    self.assertTrue(req.is_open)
                    self.assertEqual(req.wait(5000).status, expected(42))
                finally:
                    req.close()
                req.close()
                close.assert_called_once()

    def test_bad_success_and_failure_diagnostics(self):
        max_size_t = c.c_size_t(-1).value
        for status, options, kind in (
            (n.AMS_MEL_OK, dict(length=0), PROTOCOL),
            (n.AMS_MEL_OK, dict(data=b'X', length=1), PROTOCOL),
            (n.AMS_MEL_OK, dict(text=b'not empty'), PROTOCOL),
            (n.AMS_MEL_OK, dict(text=b'x' * 700), PROTOCOL),
            (n.AMS_MEL_OK, dict(code=n.AMS_MEL_ERROR_INVALID_STATE), PROTOCOL),
            (n.AMS_MEL_COMMAND_REJECTED, dict(length=max_size_t), PROTOCOL),
            (n.AMS_MEL_PROVIDER_FAILED, dict(length=2, data=b'ab'), PROTOCOL),
            (n.AMS_MEL_INTERNAL_ERROR, dict(text=b'embedded\0nul'), PROTOCOL),
            (n.AMS_MEL_PROVIDER_EXCEPTION, dict(text=b'\xff'), UTF8),
            (n.AMS_MEL_COMMAND_REJECTED, dict(text=b'x' * 700 + b'\xc0\x80'), UTF8),
        ):
            with self.subTest(status=status, options=options), channel_for() as channel:
                with channel.submit_calibration_status(COMMAND) as req, mock.patch.object(
                        n, WAIT, side_effect=lambda *args: response(*args, status=status, **options)) as wait:
                    with mock.patch.object(impl, '_status_from_raw') as convert:
                        failure(self, kind, req.wait)
                    convert.assert_not_called()
                    self.assertEqual(wait.call_count, 2 if options.get('text', b'').endswith(b'\xc0\x80') else 1)
                    self.assertTrue(req.is_open)

    def test_cached_reread_uses_only_active_fields_and_validates_terminal_identity(self):
        original_type = n.IrCalibrationStatusResultV1
        text = '跨界' * 200
        payload = text.encode()

        for status, kind in (
            (n.AMS_MEL_COMMAND_REJECTED, None),
            (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED),
            (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
            (n.AMS_MEL_INTERNAL_ERROR, INTERNAL),
            (n.AMS_MEL_TIMEOUT, m.ErrorKind.TIMEOUT),
        ):
            class GuardedResult(original_type):
                def __getattribute__(self, name):
                    if name == 'status' or (name == 'error_code' and status != n.AMS_MEL_COMMAND_REJECTED):
                        raise AssertionError(f'inactive field accessed: {name}')
                    return super().__getattribute__(name)

            # Callable patch only replaces allocation. ABI writes below always
            # target the true layout and can vary inactive bytes on reread.
            def injected(*args):
                storage = original_type()
                c.memset(c.byref(storage), 0xA5 if args[1] else 0x5A, c.sizeof(storage))
                c.memmove(args[2], c.byref(storage), c.sizeof(storage))
                if status == n.AMS_MEL_COMMAND_REJECTED:
                    c.cast(args[2], c.POINTER(original_type)).contents.error_code = 0xF1234567
                diagnostic(*args[-3:], payload)
                return status

            with self.subTest(kind=kind), channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
                with mock.patch.object(n, WAIT, side_effect=injected) as wait, mock.patch.object(
                        n, 'IrCalibrationStatusResultV1', GuardedResult):
                    if kind is None:
                        rejected = req.wait(75)
                        self.assertEqual(rejected, outcome(0xF1234567, text))
                        self.assertEqual(rejected.code.name, 'UNKNOWN_0xF1234567')
                    elif kind is m.ErrorKind.TIMEOUT:
                        error = failure(self, kind, lambda: req.wait(75), status)
                        self.assertIsNone(error.diagnostic)
                        self.assertEqual(error.diagnostic_required, len(payload) + 1)
                    else:
                        failure(self, kind, lambda: req.wait(75), status, text)
                    self.assertEqual([call.args[1] for call in wait.call_args_list],
                                     [75] if kind is m.ErrorKind.TIMEOUT else [75, 0])

        rejected = n.AMS_MEL_COMMAND_REJECTED
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            for change in (dict(status=n.AMS_MEL_OK), dict(length=len(payload)), dict(code=2)):
                def inconsistent(*args):
                    fields = dict(status=rejected, text=payload, code=1)
                    return response(*args, **(fields if args[1] else fields | change))

                with self.subTest(change=change), mock.patch.object(n, WAIT, side_effect=inconsistent) as wait:
                    error = failure(self, PROTOCOL, lambda: req.wait(75))
                    self.assertIn('changed during diagnostic reread', error.diagnostic)
                    self.assertEqual([call.args[1] for call in wait.call_args_list], [75, 0])

    def test_invalid_nested_spans(self):
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            for name in ('active', 'completed', 'items'):
                for case in ('null', 'misaligned', 'count limit', 'address overflow'):
                    with self.subTest(span=name, case=case):
                        backing = NativeView()
                        span, element = {
                            'active': (backing.raw.active_calibration_ids, n.UciIdV1),
                            'completed': (backing.raw.completed_calibrations, n.IrCompletedCalibrationV1),
                            'items': (backing.completed[1].calibration_items, n.IrCompletedCalibrationItemV1),
                        }[name]
                        pointer = c.POINTER(element)
                        if case == 'null':
                            span.data = pointer()
                            message = 'NULL'
                        elif case == 'misaligned':
                            span.data = c.cast(c.cast(span.data, c.c_void_p).value + 1, pointer)
                            message = 'misaligned'
                        elif case == 'count limit':
                            span.size = 1 << 31
                            message = 'INT32_MAX'
                        else:
                            # An aligned high address that must be rejected before any dereference.
                            span.data = c.cast(-c.alignment(element), pointer)
                            message = 'overflows the native address space'
                        with mock.patch.object(n, WAIT, side_effect=lambda *args: response(*args, raw=backing.raw)):
                            error = failure(self, PROTOCOL, req.wait)
                            self.assertIn(message, error.diagnostic)
                            self.assertTrue(req.is_open)

    def test_late_nested_corruption_never_publishes_a_partial_result(self):
        for field, value, kind in (
            ('overall', 5, PROTOCOL), ('overall', MAX_UINT32, PROTOCOL),
            ('item', 5, PROTOCOL), ('item', MAX_UINT32, PROTOCOL),
            ('label', b'bad\xff', UTF8), ('label', b'emb\0ed', PROTOCOL),
            ('item_name', b'bad\0name', PROTOCOL), ('fail_reason', b'\xed\xa0\x80', UTF8),
        ):
            with self.subTest(field=field, value=value), channel_for() as channel:
                backing = NativeView()
                entry, item = backing.completed[1], backing.items[1]
                if field == 'overall':
                    entry.result = value
                elif field == 'item':
                    item.result = value
                elif field == 'label':
                    entry.calibration_id.descriptive_label = backing.text(value)
                else:
                    setattr(item, field, backing.text(value))
                with channel.submit_calibration_status(COMMAND) as req, mock.patch.object(
                        n, WAIT, side_effect=lambda *args: response(*args, raw=backing.raw)):
                    with mock.patch.object(impl, 'CalibrationStatusCompleted') as publish:
                        failure(self, kind, req.wait)
                        publish.assert_not_called()
                    self.assertTrue(req.is_open)

    def test_allocation_failures_at_multiple_depths_can_retry_native_cache(self):
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            for module, name, fail_at in (
                (_ffi, 'UciId', 1), (conversion, 'CompletedCalibration', 4),
                (conversion, 'CompletedCalibrationItem', 6), (impl, 'CalibrationStatusCompleted', 1),
            ):
                original = getattr(module, name)
                calls = 0

                def allocate(*args, **kwargs):
                    nonlocal calls
                    calls += 1
                    if calls == fail_at:
                        raise MemoryError('simulated Python allocation failure')
                    return original(*args, **kwargs)

                with self.subTest(type=name), mock.patch.object(module, name, side_effect=allocate):
                    failure(self, INTERNAL, lambda: req.wait(5000))
                    self.assertEqual(calls, fail_at)
                    self.assertTrue(req.is_open)
                self.assertEqual(req.wait().status, expected(42))

    def test_diagnostic_allocation_failure_leaves_terminal_rejection_available(self):
        with channel_for('calstatus-reject-long') as channel, channel.submit_calibration_status(COMMAND) as req:
            create = c.create_string_buffer

            def allocate(size):
                if size == 701:
                    raise MemoryError('complete diagnostic')
                return create(size)

            with mock.patch.object(c, 'create_string_buffer', side_effect=allocate):
                failure(self, INTERNAL, lambda: req.wait(5000))
            self.assertEqual(req.wait(), outcome(m.MelErrorCode.INVALID_STATE, 'r' * 700))

    def test_successful_hostile_copy_owns_unicode_nested_bytes_after_close(self):
        backing = NativeView()
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            with mock.patch.object(n, WAIT, side_effect=lambda *args: response(*args, raw=backing.raw)):
                copied = req.wait()
        backing.erase()
        r = m.SubsystemCalibrationResult
        status = m.CalibrationStatus(
            (m.UciId(bytes(range(16)), 'active 校正'),),
            (m.CompletedCalibration(m.UciId(bytes(range(16, 32)), ''), -(1 << 63), r.COMPLETED, '', ()),
             m.CompletedCalibration(m.UciId(bytes(range(32, 48)), 'completed €'), (1 << 63) - 1,
                                    r.FAILED, 'reason',
                                    (m.CompletedCalibrationItem('', r.NOT_SET, ''),
                                     m.CompletedCalibrationItem('gain µ', r.NOT_CALIBRATED, '原因')))),
        )
        self.assertEqual(copied, m.CalibrationStatusCompleted(status))
        assert_owned(self, copied)

    def test_uncertain_close_is_disarmed_and_preserves_the_original_error(self):
        for case, kind, status in (
            ('failed-retained', INTERNAL, n.AMS_MEL_INTERNAL_ERROR),
            ('failed-consumed', INTERNAL, n.AMS_MEL_INTERNAL_ERROR),
            ('success-retained', PROTOCOL, None),
            ('success-invalid-diagnostic', PROTOCOL, None),
        ):
            with self.subTest(case=case), channel_for() as channel:
                native_close = getattr(n, CLOSE)

                def uncertain(*args):
                    # Consume the actual owner, then simulate an inconsistent
                    # output without passing the stale address back to native.
                    slot = c.cast(args[0], c.POINTER(c.c_void_p))
                    stale = slot[0]
                    self.assertEqual(native_close(*args), n.AMS_MEL_OK)
                    if case.endswith('-retained'):
                        slot[0] = stale
                    if case.startswith('failed-'):
                        diagnostic(*args[-3:], b'close failure')
                        return n.AMS_MEL_INTERNAL_ERROR
                    if case == 'success-invalid-diagnostic':
                        diagnostic(*args[-3:], b'invalid success')
                    return n.AMS_MEL_OK

                with mock.patch.object(n, CLOSE, side_effect=uncertain) as close, mock.patch.object(n, WAIT) as wait:
                    req = channel.submit_calibration_status(COMMAND)
                    # The with-body exception must win over uncertain Close.
                    exc = BaseException('primary exception')
                    with self.assertRaises(BaseException) as primary:
                        with req:
                            raise exc
                    self.assertIs(primary.exception, exc)
                    self.assertFalse(req.is_open)
                    original = failure(self, kind, req.close, status)
                    self.assertIs(failure(self, kind, req.wait, status), original)
                    self.assertIs(failure(self, kind, req.__enter__, status), original)
                    ref = weakref.ref(req)
                    del req, original
                    gc.collect()
                    self.assertIsNone(ref())
                    close.assert_called_once()
                    wait.assert_not_called()

    def test_context_manager_propagates_close_failure_when_body_succeeds(self):
        with channel_for() as channel:
            actual_close = getattr(n, CLOSE)

            def failed(*args):
                actual_close(*args)
                diagnostic(*args[-3:], b'close failed')
                return n.AMS_MEL_INTERNAL_ERROR

            with mock.patch.object(n, CLOSE, side_effect=failed) as close:
                with self.assertRaises(m.MelError) as caught:
                    with channel.submit_calibration_status(COMMAND) as req:
                        self.assertTrue(req.is_open)
                self.assertEqual(caught.exception.diagnostic, 'close failed')
                self.assertFalse(req.is_open)
                close.assert_called_once()

    def test_finalization_never_raises_or_repeats_close(self):
        with channel_for() as channel:
            native_close = getattr(n, CLOSE)

            def raising(*args):
                native_close(*args)
                raise KeyboardInterrupt('simulated failure after consumption')

            with mock.patch.object(n, CLOSE, side_effect=raising) as close, \
                    mock.patch.object(sys, 'unraisablehook') as unraisable:
                req = channel.submit_calibration_status(COMMAND)
                ref = weakref.ref(req)
                del req
                gc.collect()
                self.assertIsNone(ref())
                close.assert_called_once()
                self.assertEqual(close.call_args.args[1:], (None, 0, None))
                unraisable.assert_not_called()


def child(case):
    test = unittest.TestCase()
    if case == 'native-copy-failure':
        os.environ['AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE'] = 'allocation'
        with channel_for() as channel, channel.submit_calibration_status(COMMAND) as req:
            for timeout in (5000, 0):
                failure(test, INTERNAL, lambda: req.wait(timeout), n.AMS_MEL_INTERNAL_ERROR)
        await_reclamation(test)
        return

    with tempfile.TemporaryDirectory(prefix='py024-lifetime-') as folder:
        log = Path(folder) / 'provider.log'
        log.touch()
        os.environ['AMS_MEL_TEST_LIFETIME_LOG'] = str(log)
        scenario = 'calstatus-sync-held' if case == 'sync-held' else 'calstatus-held'
        session = m.Session.open(provider(), scenario, '', max_async_requests=2)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        channel.enable()
        release, observe = provider_controls()

        def timeout(req, milliseconds=0):
            failure(test, m.ErrorKind.TIMEOUT, lambda: req.wait(milliseconds), n.AMS_MEL_TIMEOUT)
            test.assertTrue(req.is_open)

        if case.startswith('post-send-'):
            suffix = case.removeprefix('post-send-')
            os.environ['AMS_MEL_TEST_CALIBRATION_STATUS_POST_SEND_FAILURE'] = suffix
            text = ('facade allocation failed after provider send' if suffix == 'allocation'
                    else 'facade worker launch failed')
            failure(test, INTERNAL, lambda: channel.submit_calibration_status(COMMAND),
                    n.AMS_MEL_INTERNAL_ERROR, text)
            test.assertEqual(observe(), ((42, 7), 1, 0))
            channel.close()
            session.close()
            test.assertNotIn('library_unloaded', log.read_text().splitlines())
            return  # Native fail-safe intentionally retains the unpublished future.

        if case == 'sync-held':
            with channel.open_calibration_status() as stream:
                with channel.submit_calibration_status(COMMAND) as req:
                    test.assertEqual(observe(), ((42, 7), 1, 1))
                    callback = stream.receive()
                    test.assertEqual(callback, expected(77))
                    timeout(req)
                    stream.close()
                    timeout(req, 1)
                    test.assertEqual(release(MOCK_RELEASE_FAMILY, 1), 1)
                    completed = req.wait(5000)
                    test.assertEqual(completed.status, expected(42))
            await_reclamation(test)
            channel.close()
            session.close()
            test.assertEqual(callback, expected(77))
            test.assertEqual(completed.status, expected(42))
            assert_owned(test, callback)
            assert_owned(test, completed)
            # Metadata registration may retain native callback code beyond Close.
            return

        first = channel.submit_calibration_status(COMMAND)
        second = channel.submit_calibration_status(m.CalibrationStatusCommand(7, MAX_UINT32))
        for req in (first, second):
            timeout(req)
            timeout(req, 1)
        del req
        exhausted = lambda: channel.submit_calibration_status(COMMAND)
        failure(test, m.ErrorKind.RESOURCE_EXHAUSTED, exhausted, n.AMS_MEL_RESOURCE_EXHAUSTED)
        if case == 'close-observer':
            first.close()
        if case == 'finalize-observer':
            ref = weakref.ref(first)
            del first
            gc.collect()
            test.assertIsNone(ref())
        # A closed or dropped observer did not release admission or cancel work.
        failure(test, m.ErrorKind.RESOURCE_EXHAUSTED, exhausted, n.AMS_MEL_RESOURCE_EXHAUSTED)
        test.assertEqual(observe(), ((7, MAX_UINT32), 2, 0))
        if case == 'session-first':
            session.close()
            timeout(second)
            channel.close()
        elif case == 'gc-parents':
            refs = weakref.ref(channel), weakref.ref(session)
            del channel, session
            gc.collect()
            test.assertTrue(all(ref() is None for ref in refs))
        else:
            channel.close()
            timeout(second)
            session.close()
        teardown = ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded')
        for event in teardown:
            test.assertNotIn(event, log.read_text().splitlines())
        test.assertEqual(release(MOCK_RELEASE_FAMILY, 2), 1)
        copied = second.wait(5000)
        test.assertEqual(copied.status, expected(7))
        test.assertEqual(second.wait(), copied)
        if case not in ('close-observer', 'finalize-observer'):
            test.assertEqual(first.wait(5000).status, expected(42))
        await_reclamation(test)
        events = log.read_text().splitlines()
        test.assertEqual(events.count(f'future_consumed_{RECLAMATION_FAMILY}'), 2)
        for event in teardown:
            test.assertEqual(events.count(event), 1)
        test.assertEqual(sorted(teardown, key=events.index), list(teardown))
        # Observers and their native snapshots still work after provider unload.
        test.assertEqual(second.wait(), copied)
        second.close()
        if case != 'finalize-observer':
            first.close()
        assert_owned(test, copied)
        test.assertEqual(pickle.loads(pickle.dumps(copied.status)), expected(7))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        child(sys.argv[2])
    else:
        unittest.main()
