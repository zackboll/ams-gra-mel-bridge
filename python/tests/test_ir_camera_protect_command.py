"""PY-MEL-020: exact input, cached completion, hostile ABI and DSO lifetimes."""

from __future__ import annotations

import copy
import ctypes as c
import dataclasses as dc
import gc
import io
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _camera_protect_command as impl, _native as n
import test_control_channel as control
from test_ir_health import provider


SUBMIT = 'ams_mel_ir_c2_submit_camera_protect'
WAIT = 'ams_mel_ir_camera_protect_request_wait'
CLOSE = 'ams_mel_ir_camera_protect_request_close'
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
COMMAND = m.CameraProtectCommand(0xFFFFFFFF, -(1 << 63), True)


def diagnostic(args, status=n.AMS_MEL_OK, text=b''):
    buffer, capacity, required = args[-3:]
    if required is not None:
        c.cast(required, c.POINTER(c.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        c.memmove(buffer, text + b'\0', min(capacity, len(text) + 1))
    return status


def result(args):
    return c.cast(args[2], c.POINTER(n.IrCameraProtectResultV1)).contents


def completion_probe():
    boundary = n._LIBRARY.ams_mel_test_completion_boundary
    boundary.argtypes = [c.c_uint, c.c_uint, c.c_uint64, c.POINTER(c.c_uint64)]
    boundary.restype = c.c_int
    owner = n._LIBRARY.ams_mel_test_completion_owner
    owner.argtypes = [c.c_uint, c.c_uint64, c.POINTER(c.c_uint64)]
    owner.restype = c.c_int
    values, observed = (c.c_uint64 * 4)(), c.c_uint64()
    if boundary(7, 3, 0, values) != 1 or owner(7, values[2], c.byref(observed)) != 1:
        raise AssertionError('CameraProtect completion reclamation barrier failed')
    return values[2]


def provider_controls():
    library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
    release = library.mock_completion_release_family
    release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
    last = library.mock_camera_protect_last_command
    last.argtypes = [c.POINTER(c.c_uint64), c.POINTER(c.c_uint)]
    last.restype = c.c_int
    # Drop this test's dlopen reference while native owners retain the DSO.
    import _ctypes
    _ctypes.dlclose(library._handle)
    return release, last


class CameraProtectCommandTests(unittest.TestCase):
    def open(self, scenario='protect-echo', enabled=True, **options):
        session = m.Session.open(provider(), scenario, '', **options)
        self.addCleanup(session.close)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        self.addCleanup(channel.close)
        if enabled:
            channel.enable()
        return session, channel

    def request(self, channel, command=COMMAND):
        request = channel.submit_camera_protect(command)
        self.addCleanup(request.close)
        return request

    def error(self, kind, call, status=None):
        with self.assertRaises(m.MelError) as caught:
            call()
        self.assertIs(caught.exception.kind, kind)
        if status is not None:
            self.assertEqual(caught.exception.native_status, status)
        return caught.exception

    def test_exact_fields_and_all_success_alternatives(self):
        for scenario, response in (('protect-echo', m.CameraProtectStatus.OK),
                                   ('protect-unavailable', m.CameraProtectStatus.UNAVAILABLE),
                                   ('protect-unsupported', m.CameraProtectStatus.UNSUPPORTED)):
            with self.subTest(scenario=scenario):
                session, channel = self.open(scenario)
                _, last = provider_controls()
                for ns in (-(1 << 63), (1 << 63) - 1, -1, 0):
                    for flag in (False, True):
                        command = m.CameraProtectCommand(0xFFFFFFFF, ns, flag)
                        request = self.request(channel, command)
                        values, sends = (c.c_uint64 * 3)(), c.c_uint()
                        self.assertEqual(last(values, c.byref(sends)), 1)
                        self.assertEqual(tuple(values), (0xFFFFFFFF, ns & ((1 << 64) - 1), int(flag)))
                        expected = m.CameraProtectCompleted(m.CameraProtectResponse(0xFFFFFFFF, ns, response))
                        self.assertEqual(request.wait(5000), expected)
                        self.assertEqual(request.wait(), expected)
                        request.close()
                channel.close()
                session.close()
                self.assertEqual(expected.response.response_time_ns, 0)

    def test_multiple_independent_requests_and_detached_results(self):
        session, channel = self.open()
        requests = [self.request(channel, m.CameraProtectCommand(i, -i, bool(i % 2))) for i in range(5)]
        channel.close()
        session.close()
        outcomes = [request.wait(5000) for request in requests]
        for i, (request, outcome) in enumerate(zip(requests, outcomes)):
            self.assertEqual(outcome.response, m.CameraProtectResponse(i, -i, m.CameraProtectStatus.OK))
            request.close()
            self.assertEqual(pickle.loads(pickle.dumps(outcome)), outcome)

    def test_strict_validation_and_revalidation_no_native_call(self):
        _, channel = self.open()
        class IntegerLike:
            def __index__(self):
                return 1
        bad = {
            'command_id': (True, False, -1, 1 << 32, '1', 1.0, None, IntegerLike()),
            'response_time_ns': (True, False, -(1 << 63) - 1, 1 << 63, '1', 1.0, None, IntegerLike()),
            'protect_flag': (0, 1, -1, 2, 'true', 1.0, None, IntegerLike()),
        }
        with mock.patch.object(n, SUBMIT) as submit:
            for field, values in bad.items():
                for value in values:
                    with self.subTest(field=field, value=value):
                        fields = dict(command_id=1, response_time_ns=0, protect_flag=False)
                        fields[field] = value
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: m.CameraProtectCommand(**fields))
                        command = m.CameraProtectCommand(1, 0, False)
                        object.__setattr__(command, field, value)
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_camera_protect(command))
            for value in (None, object(), dc.make_dataclass('Unrelated', COMMAND.__dataclass_fields__)(1, 0, True)):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_camera_protect(value))
            submit.assert_not_called()

    def test_open_and_enabled_required_without_implicit_enable(self):
        _, channel = self.open(enabled=False)
        with mock.patch.object(n, 'ams_mel_ir_c2_enable') as enable:
            self.error(m.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_camera_protect(COMMAND),
                       n.AMS_MEL_PROVIDER_FAILED)
            enable.assert_not_called()
        channel.close()
        with mock.patch.object(n, SUBMIT) as submit:
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_camera_protect(COMMAND))
            submit.assert_not_called()

    def test_mock_rejections_and_failures(self):
        for scenario, code in (('no-camera-protect', m.MelErrorCode.UNSUPPORTED),
                               ('protect-reject', m.MelErrorCode.INVALID_PARAMETERS),
                               ('protect-reject-long', m.MelErrorCode.INSUFFICIENT_RESOURCES)):
            with self.subTest(scenario=scenario):
                _, channel = self.open(scenario)
                request = self.request(channel)
                outcome = request.wait(5000)
                self.assertIs(type(outcome), m.CameraProtectRejected)
                self.assertEqual(outcome.code, code)
                self.assertEqual(request.wait(), outcome)
                if scenario == 'protect-reject-long':
                    self.assertEqual(len(outcome.description.encode('utf-8')) + 1, 614)
                    self.assertIn('€', outcome.description)
                if scenario == 'protect-reject':
                    self.assertEqual(outcome.description, 'camera protect rejected')
        for scenario, kind in (('protect-unknown-response', m.ErrorKind.PROVIDER_FAILED),
                                ('protect-unknown-error', m.ErrorKind.PROVIDER_FAILED),
                                ('protect-null', m.ErrorKind.PROVIDER_FAILED),
                                ('protect-future-throw', m.ErrorKind.PROVIDER_EXCEPTION)):
            with self.subTest(scenario=scenario):
                _, channel = self.open(scenario)
                request = self.request(channel)
                first = self.error(kind, lambda: request.wait(5000))
                second = self.error(kind, request.wait)
                self.assertEqual(first.diagnostic, second.diagnostic)
                self.assertEqual(first.native_status, second.native_status)
        for scenario, kind in (('protect-send-throw', m.ErrorKind.PROVIDER_EXCEPTION),
                                ('protect-invalid-future', m.ErrorKind.PROVIDER_FAILED)):
            _, channel = self.open(scenario)
            self.error(kind, lambda: channel.submit_camera_protect(COMMAND))

    def test_public_values_and_unique_owner(self):
        from ams_mel import ir
        from ams_mel._camera_protect_response import CameraProtectResponse
        self.assertIs(impl.CameraProtectResponse, CameraProtectResponse)
        self.assertIs(impl.MelErrorCode, m.MelErrorCode)
        for cls in (m.CameraProtectCommand, m.CameraProtectCompleted,
                    m.CameraProtectRejected, m.CameraProtectRequest):
            self.assertIs(getattr(ir, cls.__name__), cls)
            self.assertEqual(cls.__module__, 'ams_mel')
            self.assertEqual(m.__all__.count(cls.__name__), 1)
            self.assertEqual(ir.__all__.count(cls.__name__), 1)
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        for value in (COMMAND, m.CameraProtectCompleted(m.CameraProtectResponse(1, 0, m.CameraProtectStatus.OK)),
                      m.CameraProtectRejected(m.MelErrorCode(99), '€')):
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            with self.assertRaises(dc.FrozenInstanceError):
                value.unexpected = 1
        with self.assertRaises(TypeError):
            m.CameraProtectRequest()
        _, channel = self.open()
        request = self.request(channel)
        for call in (lambda: copy.copy(request), lambda: copy.deepcopy(request), lambda: pickle.dumps(request)):
            with self.assertRaises(TypeError):
                call()
        with request as same:
            self.assertIs(same, request)
        self.assertFalse(request.is_open)
        request.close()
        self.error(m.ErrorKind.INVALID_ARGUMENT, request.wait)

    def test_metadata_is_independent_and_synchronous_send_does_not_deadlock(self):
        for scenario in ('protect-sync', 'protect-echo'):
            _, channel = self.open(scenario)
            with channel.open_camera_protect_response() as stream:
                request = self.request(channel, m.CameraProtectCommand(0x5151, 31, True))
                if scenario == 'protect-sync':
                    self.assertEqual(stream.receive(), m.CameraProtectResponse(
                        0x5151, -77, m.CameraProtectStatus.UNAVAILABLE))
                self.assertEqual(request.wait(5000), m.CameraProtectCompleted(
                    m.CameraProtectResponse(0x5151, 31, m.CameraProtectStatus.OK)))
                self.error(m.ErrorKind.TIMEOUT, stream.receive)
                self.assertEqual(stream.counters().events_received, int(scenario == 'protect-sync'))
        _, channel = self.open()
        with mock.patch.object(n, 'ams_mel_ir_c2_camera_protect_open') as register:
            self.request(channel).wait(5000)
            register.assert_not_called()

    def test_callback_cannot_substitute_for_pending_wait(self):
        _, channel = self.open('protect-sync')
        with channel.open_camera_protect_response() as stream:
            request = self.request(channel)
            stream.receive()
            with mock.patch.object(n, WAIT, side_effect=lambda *args: diagnostic(args, n.AMS_MEL_TIMEOUT)) as wait:
                self.error(m.ErrorKind.TIMEOUT, request.wait)
                self.assertEqual(wait.call_count, 1)
            self.assertEqual(request.wait(5000).response.response_time_ns, COMMAND.response_time_ns)

    def test_hostile_submission_missing_and_unexpected_owners(self):
        _, channel = self.open()
        with mock.patch.object(n, SUBMIT, side_effect=lambda *args: diagnostic(args)) as submit:
            self.error(PROTOCOL, lambda: channel.submit_camera_protect(COMMAND))
            self.assertEqual(submit.call_count, 1)
        for status in (n.AMS_MEL_INVALID_ARGUMENT, n.AMS_MEL_RESOURCE_EXHAUSTED,
                       n.AMS_MEL_PROVIDER_FAILED, n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR):
            def fail(*args):
                c.cast(args[2], c.POINTER(c.c_void_p))[0] = 123
                return diagnostic(args, status, b'original send failure')
            with mock.patch.object(n, SUBMIT, side_effect=fail) as submit, \
                    mock.patch.object(n, CLOSE, side_effect=RuntimeError('secondary')) as close:
                with self.assertRaises(m.MelError) as caught:
                    channel.submit_camera_protect(COMMAND)
                self.assertEqual(caught.exception.native_status, status)
                self.assertEqual(caught.exception.diagnostic, 'original send failure')
                gc.collect()
                self.assertEqual(submit.call_count, 1)
                self.assertEqual(close.call_count, 1)
        primary = RuntimeError('FFI submission exception')
        def throws(*args):
            c.cast(args[2], c.POINTER(c.c_void_p))[0] = 123
            raise primary
        with mock.patch.object(n, SUBMIT, side_effect=throws), \
                mock.patch.object(n, CLOSE, side_effect=RuntimeError('secondary')) as close:
            with self.assertRaises(RuntimeError) as caught:
                channel.submit_camera_protect(COMMAND)
            self.assertIs(caught.exception, primary)
            self.assertEqual(close.call_count, 1)

    def test_corrupt_successful_submit_diagnostic_releases_once(self):
        _, channel = self.open()
        for text in (b'not empty', b'\xff', b'a\0b'):
            def submit(*args):
                c.cast(args[2], c.POINTER(c.c_void_p))[0] = 123
                return diagnostic(args, text=text)
            with mock.patch.object(n, SUBMIT, side_effect=submit) as send, \
                    mock.patch.object(n, CLOSE, side_effect=RuntimeError('secondary')) as close:
                self.error(m.ErrorKind.INVALID_UTF8 if text == b'\xff' else PROTOCOL,
                           lambda: channel.submit_camera_protect(COMMAND))
                self.assertEqual(send.call_count, 1)
                self.assertEqual(close.call_count, 1)

    def test_hostile_wait_outputs(self):
        _, channel = self.open()
        request = self.request(channel)
        cases = ((n.AMS_MEL_OK, b'bad', None, PROTOCOL),
                 (n.AMS_MEL_OK, b'\xff', None, m.ErrorKind.INVALID_UTF8),
                 (n.AMS_MEL_OK, b'a\0b', None, PROTOCOL),
                 (n.AMS_MEL_OK, b'', 0, PROTOCOL),
                 (n.AMS_MEL_TIMEOUT, b'\xff', None, m.ErrorKind.INVALID_UTF8),
                 (n.AMS_MEL_TIMEOUT, b'', 0, PROTOCOL),
                 (n.AMS_MEL_TIMEOUT, b'', 900, m.ErrorKind.TIMEOUT),
                 (n.AMS_MEL_COMMAND_REJECTED, b'', c.c_size_t(-1).value, m.ErrorKind.INTERNAL_ERROR))
        for status, text, size, kind in cases:
            def wait(*args):
                # Poison every inactive field. Timeout must never publish them.
                c.memset(args[2], 0xFF, c.sizeof(n.IrCameraProtectResultV1))
                diagnostic(args, status, text)
                if size is not None:
                    c.cast(args[-1], c.POINTER(c.c_size_t))[0] = size
                return status
            with self.subTest(status=status, text=text, size=size), \
                    mock.patch.object(n, WAIT, side_effect=wait) as native_wait:
                self.error(kind, lambda: request.wait(17))
                self.assertEqual(native_wait.call_count, 1)
        for field in ('error_code', 'command_response'):
            def wait(*args):
                raw = result(args)
                if field == 'error_code':
                    raw.error_code = 99
                else:
                    raw.response.command_response = 99
                return diagnostic(args)
            with mock.patch.object(n, WAIT, side_effect=wait):
                self.error(PROTOCOL, request.wait)

    def test_cached_diagnostic_retry_consistency_and_inactive_fields(self):
        _, channel = self.open()
        request = self.request(channel)
        text = ('€' * 250).encode()
        for status in (n.AMS_MEL_COMMAND_REJECTED, n.AMS_MEL_PROVIDER_FAILED,
                       n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR, n.AMS_MEL_OK):
            for change in ('none', 'status', 'size', 'error', 'response', 'unterminated', 'utf8'):
                calls = []
                def wait(*args):
                    calls.append(args[1])
                    raw = result(args)
                    raw.error_code = 99 if status == n.AMS_MEL_COMMAND_REJECTED else 0
                    raw.response.command_id = 42
                    reply = diagnostic(args, status, text)
                    if len(calls) == 2:
                        if change == 'status':
                            reply = n.AMS_MEL_TIMEOUT
                        if change == 'size':
                            c.cast(args[-1], c.POINTER(c.c_size_t))[0] -= 1
                        if change == 'error':
                            raw.error_code += 1
                        if change == 'response':
                            raw.response.response_time_ns = -1
                        if change == 'unterminated':
                            c.memset(args[-3], 0xFF, args[-2])
                        if change == 'utf8':
                            c.memset(args[-3], 0xFF, args[-2] - 1)
                    return reply
                with self.subTest(status=status, change=change), mock.patch.object(n, WAIT, side_effect=wait):
                    relevant_change = change in ('status', 'size', 'unterminated') or (
                        change == 'error' and status in (n.AMS_MEL_OK, n.AMS_MEL_COMMAND_REJECTED)) or (
                        change == 'response' and status == n.AMS_MEL_OK)
                    if change == 'utf8':
                        self.error(m.ErrorKind.INVALID_UTF8, lambda: request.wait(71))
                    elif relevant_change or status == n.AMS_MEL_OK:
                        self.error(PROTOCOL, lambda: request.wait(71))
                    elif status == n.AMS_MEL_COMMAND_REJECTED:
                        self.assertEqual(request.wait(71), m.CameraProtectRejected(m.MelErrorCode(99), text.decode()))
                    else:
                        failure = self.error({n.AMS_MEL_PROVIDER_FAILED: m.ErrorKind.PROVIDER_FAILED,
                                              n.AMS_MEL_PROVIDER_EXCEPTION: m.ErrorKind.PROVIDER_EXCEPTION,
                                              n.AMS_MEL_INTERNAL_ERROR: m.ErrorKind.INTERNAL_ERROR}[status],
                                             lambda: request.wait(71), status)
                        self.assertEqual(failure.diagnostic, text.decode())
                    self.assertEqual(calls, [71, 0])

    def test_allocation_failures_do_not_consume_request_or_send_twice(self):
        _, channel = self.open()
        with mock.patch.object(impl.CameraProtectRequest, '_from_owner', side_effect=MemoryError), \
                mock.patch.object(n, SUBMIT) as submit:
            self.error(m.ErrorKind.INTERNAL_ERROR, lambda: channel.submit_camera_protect(COMMAND))
            submit.assert_not_called()
        request = self.request(channel)
        expected = request.wait(5000)
        with mock.patch.object(n, 'IrCameraProtectResultV1', side_effect=MemoryError), \
                mock.patch.object(n, WAIT) as native_wait:
            self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            native_wait.assert_not_called()
        for target in ('CameraProtectCompleted', '_response_from_raw'):
            with mock.patch.object(impl, target, side_effect=MemoryError):
                self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            self.assertEqual(request.wait(), expected)
        with mock.patch.object(impl.ctypes, 'create_string_buffer', side_effect=MemoryError):
            self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            with self.assertRaises(MemoryError):
                request.close()
        self.assertTrue(request.is_open)
        self.assertEqual(request.wait(), expected)
        with mock.patch.object(impl.ctypes, 'byref', side_effect=MemoryError), \
                mock.patch.object(request, '_close_function') as close:
            with self.assertRaises(MemoryError):
                request.close()
            close.assert_not_called()
        self.assertTrue(request.is_open)
        self.assertEqual(request.wait(), expected)

        original = c.create_string_buffer
        def allocation(size):
            if size != impl._WAIT_DIAGNOSTIC_CAPACITY:
                raise MemoryError('complete diagnostic')
            return original(size)
        def wait(*args):
            return diagnostic(args, n.AMS_MEL_COMMAND_REJECTED, b'x' * 700)
        with mock.patch.object(n, WAIT, side_effect=wait) as native_wait, \
                mock.patch.object(impl.ctypes, 'create_string_buffer', side_effect=allocation):
            self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            self.assertEqual(native_wait.call_count, 1)
        self.assertEqual(request.wait(), expected)

    def test_successful_close_is_idempotent_and_finalization_nonraising(self):
        _, channel = self.open()
        request = self.request(channel)
        original = request._close_function
        request._close_function = mock.Mock(wraps=original)
        request.close()
        request.close()
        request.__del__()
        self.assertEqual(request._close_function.call_count, 1)
        with mock.patch.object(n, CLOSE, side_effect=RuntimeError('ignored')) as close:
            request = impl.CameraProtectRequest._from_owner(c.c_void_p(123))
            request.__del__()
            request.__del__()
            self.assertFalse(request.is_open)
            self.assertEqual(close.call_count, 1)

    def test_uncertain_close_is_never_repeated_and_preserves_primary(self):
        for mode in ('throw', 'status', 'not-consumed', 'bad-diagnostic'):
            calls = []
            primary = RuntimeError('uncertain close')
            def close(*args):
                calls.append(1)
                if mode == 'throw':
                    raise primary
                return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED if mode == 'status' else n.AMS_MEL_OK,
                                  b'\xff' if mode == 'bad-diagnostic' else b'')
            with mock.patch.object(n, CLOSE, side_effect=close):
                request = impl.CameraProtectRequest._from_owner(c.c_void_p(123))
                with self.assertRaises(BaseException) as caught:
                    request.close()
                self.assertFalse(request.is_open)
                for call in (request.close, request.wait, request.__enter__):
                    with self.assertRaises(BaseException) as again:
                        call()
                    self.assertIs(again.exception, caught.exception)
                request.__del__()
                self.assertEqual(calls, [1])
                request = impl.CameraProtectRequest._from_owner(c.c_void_p(123))
                with self.assertRaisesRegex(ValueError, 'body'):
                    with request:
                        raise ValueError('body')
                request.__del__()
                self.assertEqual(calls, [1, 1])
                self.assertIsNotNone(request._close_error)
                with self.assertRaises(BaseException) as again:
                    request.wait()
                self.assertIs(again.exception, request._close_error)

    def test_invalid_timeouts_never_wait(self):
        _, channel = self.open()
        request = self.request(channel)
        with mock.patch.object(n, WAIT) as wait:
            for timeout in (True, -1, 1 << 32, 1.5, '0', None):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: request.wait(timeout))
            wait.assert_not_called()

    def test_documented_example(self):
        document = Path(__file__).resolve().parents[2] / 'docs/py-mel-020-camera-protect-command.md'
        example = document.read_text().split('```python\n', 1)[1].split('```', 1)[0]
        output = io.StringIO()
        with mock.patch('sys.stdout', output):
            exec(compile(example, str(document), 'exec'), {})
        self.assertIn('Mock completion:', output.getvalue())

    def test_pending_parent_and_provider_lifetimes_in_isolated_processes(self):
        for case in ('c2-first', 'session-first', 'gc-parents', 'close-observer', 'finalize-observer'):
            with self.subTest(case=case):
                run = subprocess.run([sys.executable, '-W', 'error', str(Path(__file__).resolve()),
                                      '--child', case], capture_output=True, text=True, timeout=60,
                                     env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)))
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


def child(case):
    test = unittest.TestCase()
    completion_probe()
    with tempfile.TemporaryDirectory(prefix='py020-lifetime-') as folder:
        log = Path(folder) / 'lifetime.log'
        log.touch()
        os.environ['AMS_MEL_TEST_LIFETIME_LOG'] = str(log)
        session = m.Session.open(provider(), 'protect-held', '', max_async_requests=2)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        channel.enable()
        release, _ = provider_controls()
        first = channel.submit_camera_protect(COMMAND)
        second = channel.submit_camera_protect(m.CameraProtectCommand(7, -9, False))
        for request in (first, second):
            for timeout in (0, 1):
                with test.assertRaises(m.MelError) as caught:
                    request.wait(timeout)
                test.assertIs(caught.exception.kind, m.ErrorKind.TIMEOUT)
        if case == 'close-observer':
            first.close()
        if case == 'finalize-observer':
            ref = weakref.ref(first)
            del first
            gc.collect()
            test.assertIsNone(ref())
        if case in ('close-observer', 'finalize-observer'):
            with test.assertRaises(m.MelError) as caught:
                channel.submit_camera_protect(COMMAND)
            test.assertIs(caught.exception.kind, m.ErrorKind.RESOURCE_EXHAUSTED)
        if case == 'session-first':
            session.close()
            channel.close()
        elif case == 'gc-parents':
            refs = weakref.ref(channel), weakref.ref(session)
            del channel, session
            gc.collect()
            test.assertTrue(all(ref() is None for ref in refs))
        else:
            channel.close()
            session.close()
        for event in ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded'):
            test.assertNotIn(event, log.read_text().splitlines())
        test.assertEqual(release(8, 2), 1)
        expected = m.CameraProtectCompleted(m.CameraProtectResponse(7, -9, m.CameraProtectStatus.OK))
        test.assertEqual(second.wait(5000), expected)
        test.assertEqual(second.wait(), expected)
        if case not in ('close-observer', 'finalize-observer'):
            test.assertEqual(first.wait(5000).response.command_id, COMMAND.command_id)
            first.close()
        completion_probe()  # Worker release is distinct from result publication.
        second.close()
        events = log.read_text().splitlines()
        for event in ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded'):
            test.assertEqual(events.count(event), 1)
        test.assertLess(events.index('c2_channel_destroyed'), events.index('control_destroyed'))
        test.assertLess(events.index('control_destroyed'), events.index('library_unloaded'))
        test.assertEqual(expected.response.response_time_ns, -9)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        child(sys.argv[2])
    else:
        unittest.main()
