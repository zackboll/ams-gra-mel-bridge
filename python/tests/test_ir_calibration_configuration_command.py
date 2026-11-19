"""PY-MEL-022: exact input, cached completion, hostile ABI and DSO lifetimes."""

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
from ams_mel import _calibration_configuration_command as impl, _native as n
from test_ir_calibration_configuration import expected
from test_ir_health_metadata import assert_owned
from ams_mel import _calibration_configuration as conversion
import test_control_channel as control
from test_ir_health import provider


SUBMIT = 'ams_mel_ir_c2_submit_calibration_configuration'
WAIT = 'ams_mel_ir_calibration_configuration_request_wait'
CLOSE = 'ams_mel_ir_calibration_configuration_request_close'
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
COMMAND = m.CalibrationConfigurationCommand(0xFFFFFFFF, 0)


def diagnostic(args, status=n.AMS_MEL_OK, text=b''):
    buffer, capacity, required = args[-3:]
    if required is not None:
        c.cast(required, c.POINTER(c.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        c.memmove(buffer, text + b'\0', min(capacity, len(text) + 1))
    return status


def result(args):
    return c.cast(args[2], c.POINTER(n.IrCalibrationConfigurationResultV1)).contents


def completion_probe():
    boundary = n._LIBRARY.ams_mel_test_completion_boundary
    boundary.argtypes = [c.c_uint, c.c_uint, c.c_uint64, c.POINTER(c.c_uint64)]
    boundary.restype = c.c_int
    owner = n._LIBRARY.ams_mel_test_completion_owner
    owner.argtypes = [c.c_uint, c.c_uint64, c.POINTER(c.c_uint64)]
    owner.restype = c.c_int
    values, observed = (c.c_uint64 * 4)(), c.c_uint64()
    if boundary(8, 3, 0, values) != 1 or owner(8, values[2], c.byref(observed)) != 1:
        raise AssertionError('CalibrationConfiguration completion reclamation barrier failed')
    return values[2]


def provider_controls():
    library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
    release = library.mock_completion_release_family
    release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
    last = library.mock_calibration_last_command
    last.argtypes = [c.POINTER(c.c_uint32), c.POINTER(c.c_uint)]
    last.restype = c.c_int
    # Drop this test's dlopen reference while native owners retain the DSO.
    import _ctypes
    _ctypes.dlclose(library._handle)
    return release, last


class CalibrationConfigurationCommandTests(unittest.TestCase):
    def open(self, scenario='calibration-rich', enabled=True, **options):
        session = m.Session.open(provider(), scenario, '', **options)
        self.addCleanup(session.close)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        self.addCleanup(channel.close)
        if enabled:
            channel.enable()
        return session, channel

    def request(self, channel, command=COMMAND):
        request = channel.submit_calibration_configuration(command)
        self.addCleanup(request.close)
        return request

    def error(self, kind, call, status=None):
        with self.assertRaises(m.MelError) as caught:
            call()
        self.assertIs(caught.exception.kind, kind)
        if status is not None:
            self.assertEqual(caught.exception.native_status, status)
        return caught.exception

    def test_validation_and_exact_encoding(self):
        class IntegerLike:
            def __index__(self):
                return 1
        _, channel = self.open()
        for field in ('command_id', 'request_id'):
            for value in (True, False, -1, 1 << 32, '1', 1.0, None, IntegerLike()):
                with self.subTest(field=field, value=value), mock.patch.object(n, SUBMIT) as submit:
                    fields = dict(command_id=0, request_id=0)
                    fields[field] = value
                    self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: m.CalibrationConfigurationCommand(**fields))
                    command = m.CalibrationConfigurationCommand(0, 0)
                    object.__setattr__(command, field, value)
                    self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_calibration_configuration(command))
                    submit.assert_not_called()
        for value in (None, object(), dc.make_dataclass('Unrelated', ['command_id', 'request_id'])(1, 2)):
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_calibration_configuration(value))
        _, last = provider_controls()
        for command_id in (0, 0xFFFFFFFF, 42):
            for request_id in (0, 0xFFFFFFFF, 7):
                with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
                    request = self.request(channel, m.CalibrationConfigurationCommand(command_id, request_id))
                    values, count = (c.c_uint32 * 2)(), c.c_uint()
                    self.assertEqual(last(values, c.byref(count)), 1)
                    self.assertEqual(list(values), [command_id, request_id])
                    submit.assert_called_once()
                    self.assertEqual(request.wait(5000).configuration, expected(command_id))
                    request.close()

    def test_success_fidelity_multiple_requests_and_public_identity(self):
        from ams_mel import ir
        for cls in (m.CalibrationConfigurationCommand, m.CalibrationConfigurationCompleted,
                    m.CalibrationConfigurationRejected, m.CalibrationConfigurationRequest):
            self.assertIs(getattr(ir, cls.__name__), cls)
            self.assertIs(getattr(impl, cls.__name__), cls)
            self.assertEqual(cls.__module__, 'ams_mel')
            self.assertEqual(m.__all__.count(cls.__name__), 1)
            self.assertEqual(ir.__all__.count(cls.__name__), 1)
        session, channel = self.open()
        requests = [self.request(channel, m.CalibrationConfigurationCommand(i, 99)) for i in (42, 7)]
        outcomes = [r.wait(5000) for r in requests]
        for i, request, outcome in zip((42, 7), requests, outcomes):
            self.assertIs(type(outcome.configuration), m.CalibrationConfiguration)
            self.assertEqual(outcome.configuration, expected(i))
            self.assertEqual(request.wait(), outcome)
            self.assertTrue(request.is_open)
            for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                with self.assertRaises(TypeError):
                    operation(request)
            with self.assertRaises(dc.FrozenInstanceError):
                outcome.configuration = None
            request.close()
        channel.close()
        session.close()
        for i, outcome in zip((42, 7), outcomes):
            assert_owned(self, outcome)
            self.assertEqual(pickle.loads(pickle.dumps(outcome)), m.CalibrationConfigurationCompleted(expected(i)))
        for scenario, configuration in (('calibration-empty', m.CalibrationConfiguration(())),
                                        ('calibration-one', m.CalibrationConfiguration(expected(1).entries[:1]))):
            _, channel = self.open(scenario)
            with channel.submit_calibration_configuration(m.CalibrationConfigurationCommand(1, 2)) as request:
                self.assertEqual(request.wait(5000).configuration, configuration)
        with self.assertRaises(dc.FrozenInstanceError):
            COMMAND.request_id = 2

    def test_native_rejections_and_failures(self):
        for scenario, code, description in (
            ('no-calibration', m.MelErrorCode.UNSUPPORTED, None),
            ('calibration-reject', m.MelErrorCode.INVALID_PARAMETERS, 'calibration rejected'),
            ('calibration-reject-long', m.MelErrorCode.INVALID_PARAMETERS, 'r' * 700),
        ):
            _, channel = self.open(scenario)
            request = self.request(channel)
            outcome = request.wait(5000)
            self.assertIs(type(outcome), m.CalibrationConfigurationRejected)
            self.assertEqual(outcome.code, code)
            if description is not None:
                self.assertEqual(outcome.description, description)
            self.assertEqual(request.wait(), outcome)
        for scenario in ('null', 'future-throw', 'sentinel', 'unknown', 'nul', 'utf8', 'unknown-error'):
            _, channel = self.open('calibration-' + scenario)
            request = self.request(channel)
            kind = m.ErrorKind.PROVIDER_EXCEPTION if scenario == 'future-throw' else m.ErrorKind.PROVIDER_FAILED
            first = self.error(kind, lambda: request.wait(5000))
            second = self.error(kind, request.wait)
            self.assertEqual((first.native_status, first.diagnostic), (second.native_status, second.diagnostic))
        for scenario, kind in (('send-throw', m.ErrorKind.PROVIDER_EXCEPTION),
                               ('invalid-future', m.ErrorKind.PROVIDER_FAILED)):
            _, channel = self.open('calibration-' + scenario)
            with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
                self.error(kind, lambda: channel.submit_calibration_configuration(COMMAND))
                submit.assert_called_once()

    def test_metadata_independence(self):
        _, channel = self.open('calibration-rich')
        with mock.patch.object(n, 'ams_mel_ir_c2_calibration_configuration_open') as register:
            request = self.request(channel)
            self.assertEqual(request.wait(5000).configuration, expected(COMMAND.command_id))
            register.assert_not_called()
        for scenario in ('calibration-sync', 'calibration-sync-held'):
            _, channel = self.open(scenario)
            with channel.open_calibration_configuration() as stream:
                release, _ = provider_controls()
                request = self.request(channel, m.CalibrationConfigurationCommand(42, 7))
                metadata = stream.receive(5000)
                self.assertEqual(metadata, expected(77))
                if scenario.endswith('held'):
                    self.error(m.ErrorKind.TIMEOUT, request.wait)
                    self.assertEqual(release(9, 1), 1)
                completion = request.wait(5000)
                self.assertEqual(completion.configuration, expected(42))
                self.assertIsNot(completion.configuration, metadata)
                self.error(m.ErrorKind.TIMEOUT, stream.receive)

    def test_native_snapshot_allocation_failure_is_not_success(self):
        _, channel = self.open()
        with mock.patch.dict(os.environ, {'AMS_MEL_TEST_CALIBRATION_COPY_FAILURE': 'allocation'}):
            request = self.request(channel)
            self.error(m.ErrorKind.INTERNAL_ERROR, lambda: request.wait(5000), n.AMS_MEL_INTERNAL_ERROR)
            self.error(m.ErrorKind.INTERNAL_ERROR, request.wait, n.AMS_MEL_INTERNAL_ERROR)

    def test_hostile_diagnostics_and_no_inactive_configuration_access(self):
        _, channel = self.open()
        request = self.request(channel)
        cases = (
            (n.AMS_MEL_OK, b'', 0, PROTOCOL),
            (n.AMS_MEL_OK, b'', 900, PROTOCOL),
            (n.AMS_MEL_OK, b'bad', None, PROTOCOL),
            (n.AMS_MEL_TIMEOUT, b'\xff', None, m.ErrorKind.INVALID_UTF8),
            (n.AMS_MEL_TIMEOUT, b'', 900, m.ErrorKind.TIMEOUT),
            (n.AMS_MEL_COMMAND_REJECTED, b'\xff', None, m.ErrorKind.INVALID_UTF8),
            (n.AMS_MEL_COMMAND_REJECTED, b'a\0b', None, PROTOCOL),
            (n.AMS_MEL_COMMAND_REJECTED, b'', c.c_size_t(-1).value, m.ErrorKind.INTERNAL_ERROR),
        )
        for status, text, size, kind in cases:
            def wait(*args):
                c.memset(args[2], 0xFF, c.sizeof(n.IrCalibrationConfigurationResultV1))
                result(args).error_code = 0
                diagnostic(args, status, text)
                if size is not None:
                    c.cast(args[-1], c.POINTER(c.c_size_t))[0] = size
                return status
            with self.subTest(status=status, size=size), mock.patch.object(n, WAIT, side_effect=wait) as native_wait:
                self.error(kind, request.wait)
                native_wait.assert_called_once()
        def wrong_code(*args):
            result(args).error_code = 99
            return diagnostic(args)
        with mock.patch.object(n, WAIT, side_effect=wrong_code):
            self.error(PROTOCOL, request.wait)
        for byte in (0xFF, ord('x')):
            def invalid_empty(*args):
                diagnostic(args)
                c.memset(args[-3], byte, args[-2])
                return n.AMS_MEL_OK
            with mock.patch.object(n, WAIT, side_effect=invalid_empty):
                self.error(PROTOCOL, request.wait)
        for status, kind in ((n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
                             (n.AMS_MEL_PROVIDER_FAILED, m.ErrorKind.PROVIDER_FAILED),
                             (n.AMS_MEL_INTERNAL_ERROR, m.ErrorKind.INTERNAL_ERROR),
                             (n.AMS_MEL_COMMAND_REJECTED, None)):
            def inactive(*args):
                c.memset(args[2], 0xFF, c.sizeof(n.IrCalibrationConfigurationResultV1))
                result(args).error_code = 0xDEADBEEF
                return diagnostic(args, status, b'provider diagnostic')
            with mock.patch.object(n, WAIT, side_effect=inactive), \
                    mock.patch.object(impl, '_configuration_from_raw') as convert:
                if kind is None:
                    outcome = request.wait()
                    self.assertEqual(outcome.code.name, 'UNKNOWN_0xDEADBEEF')
                    self.assertEqual(outcome.description, 'provider diagnostic')
                else:
                    self.error(kind, request.wait, status)
                convert.assert_not_called()

    def test_cached_diagnostic_consistency_only_active_fields(self):
        _, channel = self.open()
        request = self.request(channel)
        text = ('校正' * 150).encode()
        for status in (n.AMS_MEL_COMMAND_REJECTED, n.AMS_MEL_PROVIDER_FAILED,
                       n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR):
            for change in ('none', 'status', 'size', 'error', 'configuration', 'utf8', 'unterminated'):
                calls = []
                def wait(*args):
                    calls.append(args[1])
                    c.memset(args[2], 0xFF, c.sizeof(n.IrCalibrationConfigurationResultV1))
                    result(args).error_code = 99
                    second = len(calls) == 2
                    if second:
                        if change == 'configuration':
                            result(args).configuration.size = 0
                        if change == 'error':
                            result(args).error_code = 100
                    diagnostic(args, status, b'\xff' * len(text) if second and change == 'utf8' else text)
                    if second and change == 'size':
                        c.cast(args[-1], c.POINTER(c.c_size_t))[0] = len(text)
                    if second and change == 'unterminated':
                        c.memset(args[-3], ord('x'), args[-2])
                    return n.AMS_MEL_TIMEOUT if second and change == 'status' else status
                with self.subTest(status=status, change=change), mock.patch.object(n, WAIT, side_effect=wait):
                    if change == 'utf8':
                        self.error(m.ErrorKind.INVALID_UTF8, lambda: request.wait(71))
                    elif change in ('status', 'size', 'unterminated') or (
                            change == 'error' and status == n.AMS_MEL_COMMAND_REJECTED):
                        self.error(PROTOCOL, lambda: request.wait(71))
                    elif status == n.AMS_MEL_COMMAND_REJECTED:
                        self.assertEqual(request.wait(71), m.CalibrationConfigurationRejected(m.MelErrorCode(99), text.decode()))
                    else:
                        self.error({n.AMS_MEL_PROVIDER_FAILED: m.ErrorKind.PROVIDER_FAILED,
                                    n.AMS_MEL_PROVIDER_EXCEPTION: m.ErrorKind.PROVIDER_EXCEPTION,
                                    n.AMS_MEL_INTERNAL_ERROR: m.ErrorKind.INTERNAL_ERROR}[status],
                                   lambda: request.wait(71), status)
                    self.assertEqual(calls, [71, 0])

    def test_hostile_nested_spans_and_conversion_failure_leave_cached_result(self):
        _, channel = self.open()
        request = self.request(channel)
        expected_result = request.wait(5000)
        entry = n.IrSubsystemCalibrationV1()
        entries = (n.IrSubsystemCalibrationV1 * 1)(entry)
        raw = n.IrCalibrationConfigurationV1(entries, 1)
        def wait(*args):
            result(args).configuration = raw
            return diagnostic(args)
        with mock.patch.object(n, WAIT, side_effect=wait):
            for size, address in ((1, 0), (1, c.addressof(entries) + 1),
                                  (0x80000000, c.addressof(entries)),
                                  (1, c.c_void_p(-8).value)):
                raw.size = size
                raw.data = c.cast(address, c.POINTER(n.IrSubsystemCalibrationV1))
                self.error(PROTOCOL, request.wait)
            raw.data, raw.size = entries, 1
            for interface in (3, 0xFFFFFFFF):
                entries[0].accepted_interface = interface
                self.error(PROTOCOL, request.wait)
            entries[0].accepted_interface = 0
            for field, element in (('calibration_item_names', n.StringViewV1),
                                   ('subsystem_component_ids', n.UciIdV1),
                                   ('capability_ids', n.UciIdV1)):
                span = getattr(entries[0], field)
                for size, address in ((1, 0), (1, c.addressof(entries) + 1),
                                      (0x80000000, c.addressof(entries)),
                                      (1, c.c_void_p(-8).value)):
                    span.size = size
                    span.data = c.cast(address, c.POINTER(element))
                    self.error(PROTOCOL, request.wait)
                span.size, span.data = 0, c.POINTER(element)()
            names = (n.StringViewV1 * 1)()
            entries[0].calibration_item_names.data = names
            entries[0].calibration_item_names.size = 1
            for text, kind in ((b'\xff', m.ErrorKind.INVALID_UTF8), (b'a\0b', PROTOCOL)):
                backing = c.create_string_buffer(text)
                names[0] = n.StringViewV1(c.cast(backing, n.CharPointer), len(text))
                self.error(kind, request.wait)
            names[0] = n.StringViewV1(None, 1)
            self.error(PROTOCOL, request.wait)
            names[0] = n.StringViewV1(None, 0x80000000)
            self.error(PROTOCOL, request.wait)
        for target in ('_copy_id', '_copy_name', '_copy_entry', 'SubsystemCalibration', 'CalibrationConfiguration'):
            with mock.patch.object(conversion, target, side_effect=MemoryError('nested allocation')):
                self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            self.assertEqual(request.wait(), expected_result)

    def test_owned_unicode_order_and_duplicates(self):
        _, channel = self.open()
        request = self.request(channel)
        label = c.create_string_buffer('校正 µ'.encode())
        name = c.create_string_buffer(('µ' * 700).encode())
        identifier = n.UciIdV1()
        identifier.uuid[:] = bytes(range(16))
        identifier.descriptive_label = n.StringViewV1(c.cast(label, n.CharPointer), len(label.raw) - 1)
        names = (n.StringViewV1 * 3)(n.StringViewV1(c.cast(name, n.CharPointer), len(name.raw) - 1),
                                    n.StringViewV1(None, 0),
                                    n.StringViewV1(c.cast(name, n.CharPointer), len(name.raw) - 1))
        ids = (n.UciIdV1 * 2)(identifier, identifier)
        entries = (n.IrSubsystemCalibrationV1 * 2)()
        for entry in entries:
            entry.calibration_id = identifier
            entry.accepted_interface = 2
            entry.calibration_item_names.data, entry.calibration_item_names.size = names, 3
            entry.subsystem_component_ids.data, entry.subsystem_component_ids.size = ids, 2
        def wait(*args):
            result(args).configuration = n.IrCalibrationConfigurationV1(entries, 2)
            return diagnostic(args)
        with mock.patch.object(n, WAIT, side_effect=wait):
            outcome = request.wait()
        request.close()
        c.memset(label, 0, len(label))
        c.memset(name, 0, len(name))
        c.memset(entries, 0xFF, c.sizeof(entries))
        identifier_value = m.UciId(bytes(range(16)), '校正 µ')
        entry_value = m.SubsystemCalibration(identifier_value, m.CalibrationControlInterface.CALIBRATION_STATE_COMMAND,
                                            ('µ' * 700, '', 'µ' * 700), (identifier_value, identifier_value), ())
        self.assertEqual(outcome.configuration, m.CalibrationConfiguration((entry_value, entry_value)))
        assert_owned(self, outcome)
        self.assertEqual(pickle.loads(pickle.dumps(outcome)), outcome)

    def test_open_and_enabled_required_without_implicit_enable(self):
        _, channel = self.open(enabled=False)
        with mock.patch.object(n, 'ams_mel_ir_c2_enable') as enable:
            self.error(m.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_calibration_configuration(COMMAND),
                       n.AMS_MEL_PROVIDER_FAILED)
            enable.assert_not_called()
        channel.close()
        with mock.patch.object(n, SUBMIT) as submit:
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: channel.submit_calibration_configuration(COMMAND))
            submit.assert_not_called()


    def test_hostile_submission_missing_and_unexpected_owners(self):
        _, channel = self.open()
        with mock.patch.object(n, SUBMIT, side_effect=lambda *args: diagnostic(args)) as submit:
            self.error(PROTOCOL, lambda: channel.submit_calibration_configuration(COMMAND))
            self.assertEqual(submit.call_count, 1)
        for status in (n.AMS_MEL_INVALID_ARGUMENT, n.AMS_MEL_RESOURCE_EXHAUSTED,
                       n.AMS_MEL_PROVIDER_FAILED, n.AMS_MEL_PROVIDER_EXCEPTION, n.AMS_MEL_INTERNAL_ERROR):
            def fail(*args):
                c.cast(args[2], c.POINTER(c.c_void_p))[0] = 123
                return diagnostic(args, status, b'original send failure')
            with mock.patch.object(n, SUBMIT, side_effect=fail) as submit, \
                    mock.patch.object(n, CLOSE, side_effect=RuntimeError('secondary')) as close:
                with self.assertRaises(m.MelError) as caught:
                    channel.submit_calibration_configuration(COMMAND)
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
                channel.submit_calibration_configuration(COMMAND)
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
                           lambda: channel.submit_calibration_configuration(COMMAND))
                self.assertEqual(send.call_count, 1)
                self.assertEqual(close.call_count, 1)


    def test_allocation_failures_do_not_consume_request_or_send_twice(self):
        _, channel = self.open()
        with mock.patch.object(impl.CalibrationConfigurationRequest, '_from_owner', side_effect=MemoryError), \
                mock.patch.object(n, SUBMIT) as submit:
            self.error(m.ErrorKind.INTERNAL_ERROR, lambda: channel.submit_calibration_configuration(COMMAND))
            submit.assert_not_called()
        request = self.request(channel)
        expected = request.wait(5000)
        with mock.patch.object(n, 'IrCalibrationConfigurationResultV1', side_effect=MemoryError), \
                mock.patch.object(n, WAIT) as native_wait:
            self.error(m.ErrorKind.INTERNAL_ERROR, request.wait)
            native_wait.assert_not_called()
        for target in ('CalibrationConfigurationCompleted', '_configuration_from_raw'):
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
            request = impl.CalibrationConfigurationRequest._from_owner(c.c_void_p(123))
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
                request = impl.CalibrationConfigurationRequest._from_owner(c.c_void_p(123))
                with self.assertRaises(BaseException) as caught:
                    request.close()
                self.assertFalse(request.is_open)
                for call in (request.close, request.wait, request.__enter__):
                    with self.assertRaises(BaseException) as again:
                        call()
                    self.assertIs(again.exception, caught.exception)
                request.__del__()
                self.assertEqual(calls, [1])
                request = impl.CalibrationConfigurationRequest._from_owner(c.c_void_p(123))
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
        document = Path(__file__).resolve().parents[2] / 'docs/py-mel-022-calibration-configuration-command.md'
        example = document.read_text().split('```python\n', 1)[1].split('```', 1)[0]
        output = io.StringIO()
        with mock.patch('sys.stdout', output):
            exec(compile(example, str(document), 'exec'), {})
        self.assertIn('Mock completion:', output.getvalue())


    def test_pending_parent_and_provider_lifetimes_in_isolated_processes(self):
        for case in ('c2-first', 'session-first', 'gc-parents', 'close-observer', 'finalize-observer', 'snapshot-first'):
            with self.subTest(case=case):
                run = subprocess.run([sys.executable, '-W', 'error', str(Path(__file__).resolve()),
                                      '--child', case], capture_output=True, text=True, timeout=60,
                                     env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)))
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


def child(case):
    test = unittest.TestCase()
    completion_probe()
    with tempfile.TemporaryDirectory(prefix='py022-lifetime-') as folder:
        log = Path(folder) / 'lifetime.log'
        log.touch()
        os.environ['AMS_MEL_TEST_LIFETIME_LOG'] = str(log)
        session = m.Session.open(provider(), 'calibration-held', '', max_async_requests=2)
        channel = session.open_control_channel(control.ControlChannelTests.config())
        channel.enable()
        release, _ = provider_controls()
        first = channel.submit_calibration_configuration(COMMAND)
        second = channel.submit_calibration_configuration(m.CalibrationConfigurationCommand(7, 0xFFFFFFFF))
        for request in (first, second):
            for timeout in (0, 1):
                with test.assertRaises(m.MelError) as caught:
                    request.wait(timeout)
                test.assertIs(caught.exception.kind, m.ErrorKind.TIMEOUT)
        if case == 'snapshot-first':
            test.assertEqual(release(9, 2), 1)
            copied = second.wait(5000)
            test.assertEqual(copied.configuration, expected(7))
            test.assertEqual(first.wait(5000).configuration, expected(COMMAND.command_id))
            first.close()
            second.close()
            completion_probe()
            channel.close()
            session.close()
            test.assertEqual(log.read_text().splitlines().count('library_unloaded'), 1)
            assert_owned(test, copied)
            test.assertEqual(pickle.loads(pickle.dumps(copied.configuration)), expected(7))
            return
        if case == 'close-observer':
            first.close()
        if case == 'finalize-observer':
            ref = weakref.ref(first)
            del first
            gc.collect()
            test.assertIsNone(ref())
        if case in ('close-observer', 'finalize-observer'):
            with test.assertRaises(m.MelError) as caught:
                channel.submit_calibration_configuration(COMMAND)
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
        test.assertEqual(release(9, 2), 1)
        outcome = second.wait(5000)
        test.assertEqual(outcome, m.CalibrationConfigurationCompleted(expected(7)))
        test.assertEqual(second.wait(), outcome)
        if case not in ('close-observer', 'finalize-observer'):
            test.assertEqual(first.wait(5000).configuration, expected(COMMAND.command_id))
            first.close()
        completion_probe()  # Worker release is distinct from result publication.
        second.close()
        events = log.read_text().splitlines()
        for event in ('c2_channel_destroyed', 'control_destroyed', 'library_unloaded'):
            test.assertEqual(events.count(event), 1)
        test.assertLess(events.index('c2_channel_destroyed'), events.index('control_destroyed'))
        test.assertLess(events.index('control_destroyed'), events.index('library_unloaded'))
        test.assertEqual(pickle.loads(pickle.dumps(outcome.configuration)), expected(7))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        child(sys.argv[2])
    else:
        unittest.main()
