"""PY-MEL-025: mock-only EraseCommand qualification and hostile ABI checks."""

import _ctypes
from contextlib import contextmanager
import copy
import ctypes as c
import gc
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
from ams_mel import _erase_command as impl, _native as n
import test_control_channel as control
from test_ir_health import provider
from test_ir_image_los_quaternion import diagnostic


SUBMIT = 'ams_mel_ir_c2_submit_erase'
WAIT = 'ams_mel_ir_erase_request_wait'
CLOSE = 'ams_mel_ir_erase_request_close'
MAX_UINT32 = (1 << 32) - 1
COMMAND = m.EraseCommand(42, m.EraseCommandType.DISABLE)
LONG = 'x' * 510 + '€' + 'y' * 100
INVALID = m.ErrorKind.INVALID_ARGUMENT
INTERNAL = m.ErrorKind.INTERNAL_ERROR
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
REJECTED = n.AMS_MEL_COMMAND_REJECTED
SEND_FAMILY, RECLAMATION_FAMILY = 11, 10
RawResult = n.IrEraseResultV1


class IntSubclass(int):
    pass


class IntLike:
    def __index__(self):
        raise AssertionError('command validation must not coerce inputs')


def probe(library, name, *argtypes):
    fn = getattr(library, name)
    fn.argtypes, fn.restype = list(argtypes), c.c_int
    return fn


@contextmanager
def mock_library():
    # Only pin the already-loaded mock. Never load a provider in order to test
    # its presence. Keep the pin until after the control call returns.
    library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
    try:
        yield library
    finally:
        _ctypes.dlclose(library._handle)


def observe():
    with mock_library() as library:
        last = probe(library, 'mock_erase_last_command', c.POINTER(c.c_uint64), c.POINTER(c.c_uint))
        slots, sends = (c.c_uint64 * 2)(), c.c_uint()
        seen = last(slots, c.byref(sends))
        return (tuple(slots) if seen else None), sends.value


def release(count):
    with mock_library() as library:
        return probe(library, 'mock_completion_release_family', c.c_uint, c.c_uint)(SEND_FAMILY, count)


def completion_boundary(test, operation=3, target=0):
    boundary = probe(n._LIBRARY, 'ams_mel_test_completion_boundary',
                     c.c_uint, c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    values = (c.c_uint64 * 4)()
    test.assertEqual(boundary(RECLAMATION_FAMILY, operation, target, values), 1)
    return values


def await_reclamation(test, consumed):
    # A terminal Wait on one observer does not synchronize any sibling whose
    # observer was closed. Wait for every released future to reach the native
    # post-get boundary before checking the exact consumption count.
    values = completion_boundary(test, operation=1, target=consumed)
    # This includes temporary owners allocated for refused admissions; they
    # never enter the worker or consume a provider future.
    created = values[2]
    test.assertEqual(values[0], consumed)
    owner = probe(n._LIBRARY, 'ams_mel_test_completion_owner',
                  c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    observed = c.c_uint64()
    test.assertEqual(owner(RECLAMATION_FAMILY, created, c.byref(observed)), 1)
    test.assertEqual(observed.value, created)


@contextmanager
def channel_for(scenario='erase-echo', *, enabled=True):
    with m.Session.open(provider(), scenario, '') as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            if enabled:
                channel.enable()
            yield channel


def failure(test, kind, call, status=None, text=None):
    with test.assertRaises(m.MelError) as caught:
        call()
    error = caught.exception
    test.assertIs(error.kind, kind)
    test.assertEqual(error.native_status, status)
    if text is not None:
        test.assertEqual(error.diagnostic, text)
        test.assertEqual(error.diagnostic_required, len(text.encode('utf-8')) + 1)
    return error


def response(*args, status=n.AMS_MEL_OK, erase_type=2, code=n.AMS_MEL_ERROR_NONE,
             text=b'', data=None, length=None):
    record = RawResult(erase_type, code)
    c.memmove(args[2], c.byref(record), c.sizeof(record))
    buffer, capacity, required = args[-3:]
    diagnostic(buffer, capacity, required, text)
    if data is not None:
        c.memset(buffer, 0xFF, capacity)
        c.memmove(buffer, data, min(capacity, len(data)))
    if length is not None:
        c.cast(required, n.SizePointer)[0] = length
    return status


class EraseCommandTests(unittest.TestCase):
    def test_enabled_admission_and_exact_encoding(self):
        with channel_for(enabled=False) as channel:
            _, sends = observe()
            failure(self, m.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_erase(COMMAND),
                    n.AMS_MEL_PROVIDER_FAILED, 'C2 channel is not enabled')
            self.assertEqual(observe()[1], sends)
            channel.enable()
            for command_id in (0, 0x80000001, MAX_UINT32):
                for erase_type in m.EraseCommandType:
                    with self.subTest(command_id=command_id, erase_type=erase_type):
                        with channel.submit_erase(m.EraseCommand(command_id, erase_type)) as req:
                            for timeout in (5000, 0):
                                completed = req.wait(timeout)
                                self.assertEqual(completed, m.EraseCompleted(erase_type))
                                self.assertIs(completed.erase_type, erase_type)
                        self.assertFalse(req.is_open)
                        sends += 1
                        self.assertEqual(observe(), ((command_id, int(erase_type)), sends))
        with mock.patch.object(n, SUBMIT) as submit:
            failure(self, INVALID, lambda: channel.submit_erase(COMMAND))
            submit.assert_not_called()

    def test_each_returned_alternative_is_independent(self):
        for output in m.EraseCommandType:
            other = m.EraseCommandType((int(output) + 1) % 4)
            with self.subTest(output=output), channel_for(f'erase-response-{int(output)}') as channel:
                with channel.submit_erase(m.EraseCommand(MAX_UINT32, other)) as req:
                    self.assertIs(req.wait(5000).erase_type, output)
                self.assertEqual(observe()[0], (MAX_UINT32, int(other)))

    def test_native_rejections(self):
        for scenario, code, text in (
            ('no-erase', m.MelErrorCode.UNSUPPORTED, ''),
            ('erase-reject', m.MelErrorCode.INVALID_PARAMETERS, 'erase rejected'),
            ('erase-reject-long', m.MelErrorCode.INSUFFICIENT_RESOURCES, LONG),
        ):
            with self.subTest(scenario=scenario), channel_for(scenario) as channel:
                _, sends = observe()
                with channel.submit_erase(COMMAND) as req, mock.patch.object(n, WAIT, wraps=getattr(n, WAIT)) as wait:
                    for timeout in (5000, 0):
                        self.assertEqual(req.wait(timeout), m.EraseRejected(code, text))
                    if text == LONG:
                        self.assertEqual(len(text), 611)
                        self.assertEqual(len(text.encode('utf-8')), 613)
                        self.assertEqual([(call.args[1], call.args[-2]) for call in wait.call_args_list],
                                         [(5000, 512), (0, 614), (0, 512), (0, 614)])
                self.assertEqual(observe()[1], sends + 1)

    def test_native_wait_failures(self):
        bad_description = 'provider rejection description was invalid UTF-8 or contained NUL'
        for suffix, text in (
            ('reject-nul', bad_description), ('reject-utf8', bad_description),
            ('response-4', 'provider returned unknown EraseCommandType value'),
            ('response-99', 'provider returned unknown EraseCommandType value'),
            ('response-4294967295', 'provider returned unknown EraseCommandType value'),
            ('null', 'provider returned null successful EraseCommandType'),
            ('unknown-error', 'provider returned unknown MEL error code'),
            ('future-throw', 'mock erase future exception'),
        ):
            with self.subTest(suffix=suffix), channel_for('erase-' + suffix) as channel:
                status, kind = n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED
                if suffix == 'future-throw':
                    status, kind = n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION
                with channel.submit_erase(COMMAND) as req:
                    for timeout in (5000, 0):
                        failure(self, kind, lambda: req.wait(timeout), status, text)

    def test_native_send_failures_are_not_retried(self):
        for suffix, status, kind, text in (
            ('send-throw', n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION, 'mock erase send exception'),
            ('invalid-future', n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED,
             'provider returned an invalid EraseCommand future'),
        ):
            with self.subTest(suffix=suffix), channel_for('erase-' + suffix) as channel:
                _, sends = observe()
                failure(self, kind, lambda: channel.submit_erase(COMMAND), status, text)
                self.assertEqual(observe(), ((42, 2), sends + 1))

    def test_request_is_unique_and_close_is_idempotent(self):
        with channel_for() as channel, mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            with channel.submit_erase(COMMAND) as req:
                for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                    with self.subTest(operation=operation), self.assertRaises(TypeError):
                        operation(req)
                self.assertTrue(req.is_open)
            self.assertFalse(req.is_open)
            req.close()
            failure(self, INVALID, req.wait)
            failure(self, INVALID, req.__enter__)
            del req
            gc.collect()
            close.assert_called_once()

    def test_documented_mock_example(self):
        document = Path(__file__).resolve().parents[2] / 'docs/py-mel-025-erase-command.md'
        example = document.read_text().split('```python\n', 1)[1].split('```', 1)[0]
        scope = {}
        exec(compile(example, str(document), 'exec'), scope)
        self.assertEqual(scope['outcome'], m.EraseCompleted(m.EraseCommandType.DISABLE))
        self.assertFalse(scope['request'].is_open)

    def test_lifetime_and_native_failpoints_in_isolated_processes(self):
        for case in ('c2-first', 'session-first', 'gc-parents', 'close-observer', 'finalize-observer',
                     'admission', 'post-send-allocation', 'post-send-worker-launch', 'finish-failure'):
            with self.subTest(case=case):
                run = subprocess.run([sys.executable, '-W', 'error', str(Path(__file__).resolve()), '--child', case],
                                     env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)),
                                     capture_output=True, text=True, timeout=40)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class HostileEraseTests(unittest.TestCase):
    def test_invalid_values_at_construction_and_before_dispatch(self):
        forged = int.__new__(m.EraseCommandType, 99)
        with channel_for() as channel, mock.patch.object(n, SUBMIT) as submit:
            for field, values in (
                ('command_id', (True, -1, 1 << 32, 1.0, '1', IntSubclass(1), IntLike(), m.EraseCommandType.DISABLE)),
                ('erase_type', (True, 0, 1, 2, 3, 4, 99, MAX_UINT32, 1.0, 'DISABLE', IntLike(), forged)),
            ):
                for value in values:
                    with self.subTest(field=field, value=value):
                        args = dict(command_id=42, erase_type=m.EraseCommandType.DISABLE)
                        args[field] = value
                        failure(self, INVALID, lambda: m.EraseCommand(**args))
                        mutated = m.EraseCommand(42, m.EraseCommandType.DISABLE)
                        object.__setattr__(mutated, field, value)
                        failure(self, INVALID, lambda: channel.submit_erase(mutated))
            for command in (None, (42, m.EraseCommandType.DISABLE), object.__new__(m.EraseCommand)):
                failure(self, INVALID, lambda: channel.submit_erase(command))
            submit.assert_not_called()

    def test_success_without_owner_and_local_allocation_failure_never_send(self):
        def missing(*args):
            diagnostic(*args[-3:])
            return n.AMS_MEL_OK

        with channel_for() as channel, mock.patch.object(n, SUBMIT, side_effect=missing) as submit:
            with mock.patch.object(n, CLOSE) as close:
                failure(self, PROTOCOL, lambda: channel.submit_erase(COMMAND))
                submit.assert_called_once()
                close.assert_not_called()
            submit.reset_mock()
            for module, name in ((n, 'IrEraseCommandV1'), (n, 'IrEraseRequestHandle'), (c, 'create_string_buffer')):
                with self.subTest(name=name), mock.patch.object(module, name, side_effect=MemoryError):
                    failure(self, INTERNAL, lambda: channel.submit_erase(COMMAND))
            submit.assert_not_called()

    def test_unexpected_owner_on_failure_is_closed_once_without_masking(self):
        with channel_for() as channel:
            native_submit, native_close = getattr(n, SUBMIT), getattr(n, CLOSE)
            _, sends = observe()

            def failed(*args):
                self.assertEqual(native_submit(*args), n.AMS_MEL_OK)
                diagnostic(*args[-3:], b'submit failed after publishing')
                return n.AMS_MEL_INTERNAL_ERROR

            def close_failed(*args):
                self.assertEqual(native_close(*args), n.AMS_MEL_OK)
                raise KeyboardInterrupt('failure after real owner consumption')

            with mock.patch.object(n, SUBMIT, side_effect=failed) as submit, \
                    mock.patch.object(n, CLOSE, side_effect=close_failed) as close:
                failure(self, INTERNAL, lambda: channel.submit_erase(COMMAND), n.AMS_MEL_INTERNAL_ERROR,
                        'submit failed after publishing')
                gc.collect()
                submit.assert_called_once()
                close.assert_called_once()
                self.assertEqual(observe()[1], sends + 1)

    def test_invalid_timeout_and_nonterminal_timeout_do_not_retry(self):
        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            with mock.patch.object(n, WAIT, side_effect=lambda *args: response(
                *args, status=n.AMS_MEL_TIMEOUT, length=614,
            )) as wait:
                for value in (True, -1, 1 << 32, 0.0, '0', IntSubclass(1), IntLike()):
                    failure(self, INVALID, lambda: req.wait(value))
                wait.assert_not_called()
                error = failure(self, m.ErrorKind.TIMEOUT, lambda: req.wait(MAX_UINT32), n.AMS_MEL_TIMEOUT)
                self.assertIsNone(error.diagnostic)
                self.assertEqual(error.diagnostic_required, 614)
                wait.assert_called_once()
                self.assertEqual(wait.call_args.args[1], MAX_UINT32)
                self.assertTrue(req.is_open)

    def test_only_valid_success_results_are_published(self):
        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            for output in (dict(erase_type=4), dict(erase_type=99), dict(erase_type=MAX_UINT32),
                           dict(code=n.AMS_MEL_ERROR_INVALID_PARAMETERS), dict(length=0),
                           dict(length=sys.maxsize + 1), dict(length=614), dict(text=b'malformed'),
                           dict(data=b'x', length=1), dict(text=b'\0')):
                with self.subTest(output=output), mock.patch.object(n, WAIT, side_effect=lambda *args: response(
                    *args, **output,
                )) as wait:
                    failure(self, PROTOCOL, req.wait)
                    wait.assert_called_once()

    def test_malformed_rejection_diagnostics_fail_closed(self):
        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            for value, kind in ((b'', PROTOCOL), (b'no terminator', PROTOCOL),
                                (b'a\0b\0', PROTOCOL), (b'\xff\0', m.ErrorKind.INVALID_UTF8)):
                with self.subTest(value=value), mock.patch.object(n, WAIT, side_effect=lambda *args: response(
                    *args, status=REJECTED, length=len(value), data=value,
                )) as wait:
                    failure(self, kind, req.wait)
                    wait.assert_called_once()

    def test_cached_wait_does_not_read_inactive_fields(self):
        class PoisonEraseType(RawResult):
            @property
            def erase_type(self):
                raise AssertionError('inactive erase_type was read')

        class PoisonAll(PoisonEraseType):
            @property
            def error_code(self):
                raise AssertionError('inactive error_code was read')

        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            for status, kind in ((REJECTED, None), (n.AMS_MEL_INTERNAL_ERROR, INTERNAL),
                                 (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED),
                                 (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
                                 (n.AMS_MEL_TIMEOUT, m.ErrorKind.TIMEOUT)):
                def simulate(*args):
                    return response(*args, status=status, erase_type=args[-2], code=0xFACE1234,
                                    text=(LONG if status != n.AMS_MEL_TIMEOUT else '').encode('utf-8'))

                with self.subTest(status=status), mock.patch.object(n, WAIT, side_effect=simulate) as wait, \
                        mock.patch.object(n, 'IrEraseResultV1', PoisonEraseType if kind is None else PoisonAll):
                    if kind is None:
                        rejected = req.wait(51)
                        self.assertEqual(rejected, m.EraseRejected(m.MelErrorCode(0xFACE1234), LONG))
                        self.assertEqual(rejected.code.name, 'UNKNOWN_0xFACE1234')
                    else:
                        failure(self, kind, lambda: req.wait(51), status,
                                LONG if status != n.AMS_MEL_TIMEOUT else None)
                    expected = [(51, 512)]
                    if status != n.AMS_MEL_TIMEOUT:
                        expected.append((0, 614))
                    self.assertEqual([(call.args[1], call.args[-2]) for call in wait.call_args_list], expected)

    def test_cached_reread_mismatch_fails_closed(self):
        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            for changed in (dict(status=n.AMS_MEL_OK), dict(status=n.AMS_MEL_TIMEOUT),
                            dict(length=613), dict(code=123), dict(data=b'x' * 614), dict(data=b'\xff' * 613 + b'\0')):
                def simulate(*args):
                    fields = dict(status=REJECTED, code=42, text=LONG.encode('utf-8'))
                    if args[-2] == 614:
                        fields.update(changed)
                    return response(*args, **fields)

                with self.subTest(changed=changed), mock.patch.object(n, WAIT, side_effect=simulate) as wait:
                    kind = m.ErrorKind.INVALID_UTF8 if changed.get('data', b'').startswith(b'\xff') else PROTOCOL
                    failure(self, kind, lambda: req.wait(100))
                    self.assertEqual([call.args[1] for call in wait.call_args_list], [100, 0])

    def test_allocation_failure_leaves_terminal_outcome_available(self):
        create = c.create_string_buffer

        def fail_large(size):
            if size == 614:
                raise MemoryError('full diagnostic allocation')
            return create(size)

        with channel_for('erase-reject-long') as channel, channel.submit_erase(COMMAND) as req:
            with mock.patch.object(c, 'create_string_buffer', side_effect=fail_large):
                failure(self, INTERNAL, lambda: req.wait(5000))
            self.assertTrue(req.is_open)
            self.assertEqual(req.wait(), m.EraseRejected(m.MelErrorCode.INSUFFICIENT_RESOURCES, LONG))
        with channel_for() as channel, channel.submit_erase(COMMAND) as req:
            with mock.patch.object(impl, 'EraseCompleted', side_effect=MemoryError):
                failure(self, INTERNAL, lambda: req.wait(5000))
            self.assertEqual(req.wait(), m.EraseCompleted(COMMAND.erase_type))

    def test_preinvocation_close_allocation_failure_is_retryable(self):
        with channel_for() as channel, mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            req = channel.submit_erase(COMMAND)
            with mock.patch.object(c, 'create_string_buffer', side_effect=MemoryError), self.assertRaises(MemoryError):
                req.close()
            self.assertTrue(req.is_open)
            close.assert_not_called()
            req.close()
            req.close()
            close.assert_called_once()

    def test_close_disarms_uncertain_consumption_and_preserves_primary_exception(self):
        with channel_for() as channel:
            actual = getattr(n, CLOSE)
            for case in ('failed-with-null', 'failed-with-stale', 'ok-with-stale', 'call-raises'):
                def uncertain(*args):
                    slot = c.cast(args[0], c.POINTER(c.c_void_p))
                    stale = slot[0]
                    self.assertEqual(actual(*args), n.AMS_MEL_OK)
                    if case.endswith('stale'):
                        slot[0] = stale  # Never actually pass this back to native.
                    if case.startswith('failed'):
                        diagnostic(*args[-3:], b'close failed')
                        return n.AMS_MEL_INTERNAL_ERROR
                    if case == 'call-raises':
                        raise RuntimeError('failure during invocation')
                    return n.AMS_MEL_OK

                with self.subTest(case=case), mock.patch.object(n, CLOSE, side_effect=uncertain) as close, \
                        mock.patch.object(n, WAIT) as wait:
                    req = channel.submit_erase(COMMAND)
                    primary = KeyboardInterrupt('primary exception')
                    with self.assertRaises(KeyboardInterrupt) as caught:
                        with req:
                            raise primary
                    self.assertIs(caught.exception, primary)
                    self.assertFalse(req.is_open)
                    if case == 'call-raises':
                        with self.assertRaisesRegex(RuntimeError, 'during invocation') as cached:
                            req.close()
                        error = cached.exception
                    elif case.startswith('failed'):
                        error = failure(self, INTERNAL, req.close, n.AMS_MEL_INTERNAL_ERROR, 'close failed')
                    else:
                        error = failure(self, PROTOCOL, req.close)
                    for operation in (req.wait, req.__enter__):
                        with self.assertRaises(type(error)) as later:
                            operation()
                        self.assertIs(later.exception, error)
                    req.__del__()
                    close.assert_called_once()
                    wait.assert_not_called()

    def test_context_exit_propagates_close_failure_if_no_primary(self):
        with channel_for() as channel:
            actual = getattr(n, CLOSE)

            def failed(*args):
                self.assertEqual(actual(*args), n.AMS_MEL_OK)
                diagnostic(*args[-3:], b'close failed')
                return n.AMS_MEL_INTERNAL_ERROR

            with mock.patch.object(n, CLOSE, side_effect=failed) as close:
                def context():
                    with channel.submit_erase(COMMAND) as req:
                        self.assertTrue(req.is_open)

                failure(self, INTERNAL, context, n.AMS_MEL_INTERNAL_ERROR, 'close failed')
                gc.collect()
                close.assert_called_once()

    def test_finalization_is_nonraising_and_never_repeats_close(self):
        with channel_for() as channel:
            actual = getattr(n, CLOSE)

            def raising(*args):
                self.assertEqual(actual(*args), n.AMS_MEL_OK)
                raise KeyboardInterrupt('failure after consumption')

            with mock.patch.object(n, CLOSE, side_effect=raising) as close, \
                    mock.patch.object(sys, 'unraisablehook') as unraisable:
                req = channel.submit_erase(COMMAND)
                ref = weakref.ref(req)
                del req
                gc.collect()
                self.assertIsNone(ref())
                close.assert_called_once()
                self.assertEqual(close.call_args.args[1:], (None, 0, None))
                unraisable.assert_not_called()


def child(case):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix='py025-lifetime-') as folder:
        log = Path(folder) / 'mock-events.log'
        log.touch()
        os.environ['AMS_MEL_TEST_LIFETIME_LOG'] = str(log)
        if case == 'finish-failure':
            os.environ['AMS_MEL_TEST_C2_FINISH_FAILURE'] = 'exception'
        session = m.Session.open(provider(), 'erase-held', '', max_async_requests=2)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        channel.enable()
        teardown = ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded')

        def retained():
            with mock_library():
                pass  # Verify presence without leaving a test-owned dlopen reference.
            for event in teardown:
                test.assertNotIn(event, log.read_text().splitlines())

        def exhausted():
            failure(test, m.ErrorKind.RESOURCE_EXHAUSTED, lambda: channel.submit_erase(COMMAND),
                    n.AMS_MEL_RESOURCE_EXHAUSTED, 'async request limit reached')

        if case.startswith('post-send-'):
            failpoint = case.removeprefix('post-send-')
            os.environ['AMS_MEL_TEST_ERASE_POST_SEND_FAILURE'] = failpoint
            text = ('facade allocation failed after provider send' if failpoint == 'allocation'
                    else 'facade worker launch failed')
            with mock.patch.object(n, CLOSE) as close:
                failure(test, INTERNAL, lambda: channel.submit_erase(COMMAND), n.AMS_MEL_INTERNAL_ERROR, text)
                close.assert_not_called()
            test.assertEqual(observe(), ((42, 2), 1))
            channel.close()
            session.close()
            retained()
            test.assertEqual(release(1), 1)
            retained()  # The unpublished future/claim is intentionally retained.
            return

        def timeout(req, milliseconds=0):
            failure(test, m.ErrorKind.TIMEOUT, lambda: req.wait(milliseconds), n.AMS_MEL_TIMEOUT)
            test.assertTrue(req.is_open)

        first = channel.submit_erase(COMMAND)
        if case == 'finish-failure':
            channel.close()
            session.close()
            retained()
            test.assertEqual(release(1), 1)
            for milliseconds in (5000, 0):
                failure(test, m.ErrorKind.PROVIDER_FAILED, lambda: first.wait(milliseconds), n.AMS_MEL_PROVIDER_FAILED,
                        'deferred C2 cleanup failed')
            first.close()
            await_reclamation(test, 1)
            retained()
            return

        alternate = m.EraseCommand(MAX_UINT32, m.EraseCommandType.CLASSIFIED_DATA_ERASE)
        second = channel.submit_erase(alternate)
        test.assertIsNot(first, second)
        timeout(first, 1)
        timeout(second)
        exhausted()
        if case in ('close-observer', 'admission'):
            first.close()
        elif case == 'finalize-observer':
            ref = weakref.ref(first)
            del first
            gc.collect()
            test.assertIsNone(ref())
        exhausted()
        test.assertEqual(observe(), ((MAX_UINT32, 1), 2))

        if case == 'admission':
            test.assertEqual(release(2), 1)
            test.assertEqual(second.wait(5000), m.EraseCompleted(alternate.erase_type))
            await_reclamation(test, 2)
            with channel.submit_erase(COMMAND) as third:
                timeout(third)
                test.assertEqual(observe(), ((42, 2), 3))
                test.assertEqual(release(1), 1)
                test.assertEqual(third.wait(5000), m.EraseCompleted(COMMAND.erase_type))
            second.close()
            channel.close()
            session.close()
            await_reclamation(test, 3)
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
        # Park after future consumption but before native finish, so provider
        # retention is also checked with no in-flight mock release call.
        completion_boundary(test, operation=0)
        try:
            test.assertEqual(release(2), 1)
            completion_boundary(test, operation=1, target=2)
            retained()
            timeout(second)
        finally:
            completion_boundary(test, operation=2)
        completed = second.wait(5000)
        test.assertEqual(completed, m.EraseCompleted(alternate.erase_type))
        if case not in ('close-observer', 'finalize-observer'):
            test.assertEqual(first.wait(5000), m.EraseCompleted(COMMAND.erase_type))
        await_reclamation(test, 2)
        events = log.read_text().splitlines()
        test.assertEqual(events.count(f'future_consumed_{RECLAMATION_FAMILY}'), 2)
        test.assertEqual([event for event in events if event in teardown], list(teardown))
        with test.assertRaises(OSError), mock_library():
            test.fail('mock still loaded after reclamation')
        test.assertEqual(second.wait(), completed)
        second.close()
        if case != 'finalize-observer':
            first.close()
        test.assertEqual(pickle.loads(pickle.dumps(completed)), m.EraseCompleted(alternate.erase_type))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        child(sys.argv[2])
    else:
        unittest.main()
