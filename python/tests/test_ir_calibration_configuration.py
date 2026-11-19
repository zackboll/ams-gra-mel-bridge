"""PY-MEL-021: nested owned snapshots, hostile views and isolated DSO lifetimes."""

from __future__ import annotations

from contextlib import contextmanager
import ctypes as c
import copy
import dataclasses
import gc
import inspect
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
from ams_mel import _native as n, _calibration_configuration as impl
import test_control_channel as control
from test_ir_health import provider
from test_ir_health_metadata import assert_owned
from test_ir_image_los_quaternion import diagnostic, error


P = "ams_mel_ir_calibration_configuration_"
OPEN = "ams_mel_ir_c2_calibration_configuration_open"
RAW = n.IrCalibrationConfigurationV1
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
S = m.CalibrationControlInterface
R = m.CalibrationConfiguration
E = m.SubsystemCalibration


def uid(seed, label):
    return m.UciId(bytes((seed + i) % 256 for i in range(16)), label)


def expected(seed):
    # Exact independent expectations from calibration_values in mock_provider.cpp.
    return R((E(uid(seed, ""), S.NOT_SET, (), (), ()),
              E(uid(seed + 1, "calibration-1"), S.CALIBRATION_COMMAND,
                ("", "one", "z" * 700), (uid(32, "component"),),
                (uid(64, "capability-a"), uid(96, ""))),
              E(uid(seed + 2, "calibration-2"), S.CALIBRATION_STATE_COMMAND,
                ("last",), (uid(128, ""), uid(160, "component-b")), ())))


RICH = (expected(10), R(()), expected(20))


@contextmanager
def owners(scenario="calibration-callback-rich", capacity=16):
    with m.Session.open(provider(), scenario, "") as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            if capacity is None:
                yield session, channel
            else:
                with channel.open_calibration_configuration(capacity) as stream:
                    yield session, channel, stream


@contextmanager
def inject_view(pointer):
    def view(event, output, diag, capacity, required):
        c.cast(output, c.POINTER(c.POINTER(RAW)))[0] = pointer
        diagnostic(diag, capacity, required)
        return n.AMS_MEL_OK
    with mock.patch.object(n, P + "event_view", side_effect=view):
        yield


class CalibrationConfigurationTests(unittest.TestCase):
    def test_public_api(self):
        from ams_mel import ir, _c2_types
        self.assertEqual([(v.name, v.value) for v in S],
                         [("NOT_SET", 0), ("CALIBRATION_COMMAND", 1), ("CALIBRATION_STATE_COMMAND", 2)])
        for cls in (S, E, R, m.CalibrationConfigurationStream, m.CalibrationConfigurationOpenError):
            self.assertIs(getattr(ir, cls.__name__), cls)
            self.assertIs(getattr(impl, cls.__name__), cls)
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertEqual(m.__all__.count(cls.__name__), 1)
            self.assertEqual(ir.__all__.count(cls.__name__), 1)
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        self.assertIs(impl.C2MetadataCounters, _c2_types.C2MetadataCounters)
        signatures = {
            m.ControlChannel.open_calibration_configuration:
                "(self, queue_capacity: 'int' = 16) -> 'CalibrationConfigurationStream'",
            m.CalibrationConfigurationStream.receive:
                "(self, timeout_ms: 'int' = 0) -> 'CalibrationConfiguration'",
            m.CalibrationConfigurationStream.counters: "(self) -> 'C2MetadataCounters'",
            m.CalibrationConfigurationStream.close: "(self) -> 'None'",
            R: "(entries: 'tuple[SubsystemCalibration, ...]') -> None",
        }
        for function, signature in signatures.items():
            self.assertEqual(str(inspect.signature(function)), signature)
        self.assertIsInstance(inspect.getattr_static(m.CalibrationConfigurationStream, "is_open"), property)
        with self.assertRaises(TypeError):
            m.CalibrationConfigurationStream()
        for value in RICH:
            assert_owned(self, value)
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                value.entries = ()
        entry = RICH[0].entries[1]
        with self.assertRaises(dataclasses.FrozenInstanceError):
            entry.calibration_item_names = ()


    def test_sync_fifo_owned_and_no_implicit_work(self):
        with owners(capacity=None) as (_, channel):
            with mock.patch.object(n, "ams_mel_ir_c2_enable") as enable, \
                    mock.patch.object(n, "ams_mel_ir_c2_submit_calibration_configuration") as submit, \
                    mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as metadata:
                stream = channel.open_calibration_configuration()
                for call in (enable, submit, metadata):
                    call.assert_not_called()
            with stream, mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_calibration_configuration_event_close) as close:
                values = tuple(stream.receive() for _ in RICH)
                self.assertEqual(close.call_count, 3)
                self.assertTrue(all(not call.args[0]._obj.value for call in close.call_args_list))
                self.assertEqual(values, RICH)
                self.assertIs(type(stream.counters()), m.C2MetadataCounters)
                self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 0, 0))
            self.assertEqual(values, RICH)
            with mock.patch.object(n, OPEN) as opened:
                error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_configuration)
                opened.assert_not_called()

    def test_capacity_validation_and_pre_native_allocations_do_not_consume(self):
        with owners(capacity=None) as (_, channel):
            with mock.patch.object(n, OPEN) as opened:
                for capacity in (0, -1, c.c_size_t(-1).value + 1, True, 1.5, None, "1"):
                    error(self, m.ErrorKind.INVALID_ARGUMENT,
                          lambda: channel.open_calibration_configuration(capacity))
                opened.assert_not_called()
            original = MemoryError("pre-native allocation")
            with mock.patch.object(impl, "_call_with_diagnostic", side_effect=original), \
                    mock.patch.object(n, OPEN) as opened:
                with self.assertRaises(MemoryError) as caught:
                    channel.open_calibration_configuration()
                self.assertIs(caught.exception, original)
                opened.assert_not_called()
            with channel.open_calibration_configuration(c.c_size_t(-1).value) as stream:
                self.assertEqual(stream.receive(), RICH[0])

    def test_registration_returns_and_native_status_are_separate(self):
        for scenario, kind, registration in (
                ("metadata-rich", m.ErrorKind.PROVIDER_FAILED, m.CommandReturn.NOT_SUPPORTED),
                ("calibration-register-fail", m.ErrorKind.PROVIDER_FAILED, m.CommandReturn.FAIL),
                ("calibration-register-throw", m.ErrorKind.PROVIDER_EXCEPTION, None)):
            with self.subTest(scenario=scenario), owners(scenario, None) as (_, channel):
                with self.assertRaises(m.CalibrationConfigurationOpenError) as caught:
                    channel.open_calibration_configuration()
                self.assertIs(caught.exception.kind, kind)
                self.assertIs(caught.exception.registration, registration)
                with mock.patch.object(n, OPEN) as opened:
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_configuration)
                    opened.assert_not_called()
        for returned in (None, 99, int(m.CommandReturn.BAD_POINTER)):
            with self.subTest(returned=returned), owners(capacity=None) as (_, channel):
                def opened(parent, capacity, output, registration, diag, length, required):
                    if returned is not None:
                        c.cast(registration, c.POINTER(c.c_uint32))[0] = returned
                    diagnostic(diag, length, required, b"NotSupported is diagnostic prose only")
                    return n.AMS_MEL_PROVIDER_FAILED
                with mock.patch.object(n, OPEN, side_effect=opened) as call:
                    with self.assertRaises(m.CalibrationConfigurationOpenError) as caught:
                        channel.open_calibration_configuration()
                    self.assertIs(caught.exception.registration,
                                  m.CommandReturn.BAD_POINTER if returned == int(m.CommandReturn.BAD_POINTER) else None)
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_configuration)
                    self.assertEqual(call.call_count, 1)

    def test_timeout_validation_and_uint32_forwarding(self):
        with owners() as (_, _, stream):
            with mock.patch.object(n, P + "receive") as receive:
                for timeout in (-1, 1 << 32, True, 1.5, None, "0"):
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
                receive.assert_not_called()
            with mock.patch.object(n, P + "receive", wraps=n.ams_mel_ir_calibration_configuration_receive) as receive:
                for timeout, expected in zip((0, 1, 0xFFFFFFFF), RICH):
                    self.assertEqual(stream.receive(timeout), expected)
                    self.assertEqual(receive.call_args.args[1], timeout)
            error(self, m.ErrorKind.TIMEOUT, lambda: stream.receive(1), n.AMS_MEL_TIMEOUT)
            self.assertTrue(stream.is_open)

    def test_structural_views_and_unknown_status_fail_closed(self):
        backing = c.create_string_buffer(c.sizeof(RAW) + c.alignment(RAW))
        end = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(RAW)
        for address in (None, c.addressof(backing) + 1, end):
            with self.subTest(address=address), owners() as (_, _, stream), \
                    inject_view(c.cast(address, c.POINTER(RAW))), \
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_calibration_configuration_event_close) as close:
                error(self, PROTOCOL, stream.receive)
                self.assertEqual(close.call_count, 1)
        for status in (3, 99, 0xFFFFFFFF):
            entry = n.IrSubsystemCalibrationV1(accepted_interface=status)
            raw = RAW(c.pointer(entry), 1)
            with owners() as (_, _, stream), inject_view(c.pointer(raw)):
                error(self, PROTOCOL, stream.receive)

    def test_failed_receive_view_and_corrupt_diagnostics_publish_nothing(self):
        for operation in ("receive", "event_view"):
            for case in ("null", "failure", "throw", "zero", "oversized", "utf8", "unterminated", "embedded", "message"):
                with self.subTest(operation=operation, case=case), owners() as (_, _, stream):
                    native = getattr(n, P + operation)
                    def corrupted(*args):
                        status = native(*args)
                        diag, length, required = args[-3:]
                        if case == "null":
                            if operation == "receive":
                                n.ams_mel_ir_calibration_configuration_event_close(args[2], None, 0, None)
                            else:
                                c.cast(args[1], c.POINTER(c.POINTER(RAW)))[0] = c.POINTER(RAW)()
                        elif case == "failure":
                            return n.AMS_MEL_PROVIDER_FAILED
                        elif case == "throw":
                            raise RuntimeError("native invocation uncertainty")
                        elif case in ("zero", "oversized"):
                            c.cast(required, c.POINTER(c.c_size_t))[0] = 0 if case == "zero" else length + 1
                        elif case == "unterminated":
                            c.memset(diag, ord("x"), length)
                        else:
                            diagnostic(diag, length, required,
                                       {"utf8": b"\xff", "embedded": b"a\0b", "message": b"unexpected"}[case])
                        return status
                    with mock.patch.object(n, P + operation, side_effect=corrupted), \
                            mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_calibration_configuration_event_close) as close:
                        if case == "throw":
                            with self.assertRaises(RuntimeError):
                                stream.receive()
                        else:
                            kind = {"failure": m.ErrorKind.PROVIDER_FAILED, "utf8": m.ErrorKind.INVALID_UTF8}.get(case, PROTOCOL)
                            error(self, kind, stream.receive,
                                  n.AMS_MEL_PROVIDER_FAILED if case == "failure" else None)
                        self.assertEqual(close.call_count, 1)

    def test_conversion_and_allocation_preserve_primary_error(self):
        for function, original in (("CalibrationConfiguration", MemoryError("snapshot")),
                                   ("_configuration_from_raw", OverflowError("conversion")),
                                   ("_configuration_from_raw", RuntimeError("application")),
                                   ("_configuration_from_raw", KeyboardInterrupt("application"))):
            with self.subTest(function=function, original=type(original)), owners() as (_, _, stream):
                native = n.ams_mel_ir_calibration_configuration_event_close
                def close(*args):
                    native(*args)
                    raise RuntimeError("secondary cleanup")
                with mock.patch.object(impl, function, side_effect=original), \
                        mock.patch.object(n, P + "event_close", side_effect=close) as closed:
                    if isinstance(original, (MemoryError, OverflowError)):
                        caught = error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
                        self.assertIs(caught.__cause__, original)
                    else:
                        with self.assertRaises(type(original)) as caught:
                            stream.receive()
                        self.assertIs(caught.exception, original)
                    self.assertEqual(closed.call_count, 1)
        with owners() as (_, _, stream), \
                mock.patch.object(n, "IrCalibrationConfigurationEventHandle", side_effect=MemoryError), \
                mock.patch.object(n, P + "receive") as receive:
            error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
            receive.assert_not_called()

    def test_event_close_uncertainty_is_never_retried(self):
        for consume in (False, True):
            for case in ("failure", "throw", "diagnostic", "unconsumed"):
                with self.subTest(consume=consume, case=case), owners() as (_, _, stream):
                    native = n.ams_mel_ir_calibration_configuration_event_close
                    retained = []
                    def close(output, diag, capacity, required):
                        if consume:
                            native(output, diag, capacity, required)
                        else:
                            retained.append(n.IrCalibrationConfigurationEventHandle(output._obj.value))
                            diagnostic(diag, capacity, required)
                        if case == "throw":
                            raise RuntimeError("uncertain Close")
                        if case == "diagnostic":
                            c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                        return n.AMS_MEL_PROVIDER_FAILED if case == "failure" else n.AMS_MEL_OK
                    try:
                        with mock.patch.object(n, P + "event_close", side_effect=close) as closed:
                            if case == "unconsumed" and consume:
                                self.assertEqual(stream.receive(), RICH[0])
                            elif case == "throw":
                                with self.assertRaises(RuntimeError):
                                    stream.receive()
                            else:
                                error(self, m.ErrorKind.PROVIDER_FAILED if case == "failure" else PROTOCOL,
                                      stream.receive, n.AMS_MEL_PROVIDER_FAILED if case == "failure" else None)
                            self.assertEqual(closed.call_count, 1)
                    finally:
                        # Test knows this injection did not consume; wrapper must not guess.
                        for handle in retained:
                            native(c.byref(handle), None, 0, None)

    def test_stream_uncertainty_and_pre_call_failure(self):
        for consume in (False, True):
            for case in ("failure", "throw", "diagnostic", "unconsumed"):
                with self.subTest(consume=consume, case=case), owners(capacity=None) as (_, channel):
                    stream = channel.open_calibration_configuration()
                    native = stream._close_function
                    def close(output, diag, capacity, required):
                        if consume:
                            native(output, diag, capacity, required)
                        else:
                            diagnostic(diag, capacity, required)
                        if case == "throw":
                            raise RuntimeError("uncertain Close")
                        if case == "diagnostic":
                            c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                        return n.AMS_MEL_PROVIDER_FAILED if case == "failure" else n.AMS_MEL_OK
                    try:
                        with mock.patch.object(stream, "_close_function", side_effect=close) as closed:
                            if consume and case == "unconsumed":
                                stream.close()
                                stream.close()
                            else:
                                with self.assertRaises(BaseException) as caught:
                                    stream.close()
                                for operation in (stream.close, stream.receive, stream.counters, stream.__enter__):
                                    with self.assertRaises(type(caught.exception)) as again:
                                        operation()
                                    self.assertIs(again.exception, caught.exception)
                            stream.__del__()
                            self.assertEqual(closed.call_count, 1)
                    finally:
                        if not consume:
                            native(stream._owner_pointer, None, 0, None)
        with owners() as (_, _, stream):
            with mock.patch.object(impl, "_call_with_diagnostic", side_effect=MemoryError):
                with self.assertRaises(MemoryError):
                    stream.close()
            self.assertEqual(stream.receive(), RICH[0])

    def test_lifecycle_unique_owner_and_context_error_precedence(self):
        with owners(capacity=None) as (session, channel):
            stream = channel.open_calibration_configuration()
            self.assertFalse(any(value is channel or value is session for value in vars(stream).values()))
            for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                with self.assertRaises(TypeError):
                    operation(stream)
            native = stream._close_function
            def close(*args):
                native(*args)
                raise RuntimeError("cleanup")
            original = KeyboardInterrupt("application")
            with mock.patch.object(stream, "_close_function", side_effect=close) as closed:
                with self.assertRaises(KeyboardInterrupt) as caught:
                    with stream:
                        raise original
                self.assertIs(caught.exception, original)
                stream.__del__()
                self.assertEqual(closed.call_count, 1)
        with owners() as (_, _, stream):
            stream.close()
            stream.close()
            error(self, m.ErrorKind.INVALID_ARGUMENT, stream.receive)
        with owners(capacity=None) as (_, channel):
            stream = channel.open_calibration_configuration()
            reference = weakref.ref(stream)
            calls = []
            def finalize(*args):
                calls.append(1)
                native(*args)
                raise RuntimeError("non-raising finalizer")
            stream._close_function = finalize
            del stream
            gc.collect()
            self.assertIsNone(reference())
            self.assertEqual(calls, [1])

    def test_full_width_counters(self):
        with owners() as (_, _, stream):
            native = n.ams_mel_ir_calibration_configuration_get_counters
            def counters(owner, output, *args):
                native(owner, output, *args)
                raw = c.cast(output, c.POINTER(n.IrC2MetadataCountersV1)).contents
                raw.events_received, raw.events_dropped_queue_full, raw.malformed_or_unsupported = (
                    (1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1)
                return n.AMS_MEL_OK
            with mock.patch.object(n, P + "get_counters", side_effect=counters):
                self.assertEqual(stream.counters(), m.C2MetadataCounters(
                    (1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1))

    def test_failed_terminal_status_and_counter_error_conversion(self):
        # The compiled calibration fixtures do not combine callbacks with a
        # provider cleanup failure. Exercise the existing ABI status boundary
        # directly, without adding a native mock scenario or command request.
        with owners() as (_, _, stream):
            self.assertEqual(tuple(stream.receive() for _ in RICH), RICH)
            def failed(*args):
                diagnostic(*args[-3:], b"provider release could not be proven")
                return n.AMS_MEL_PROVIDER_FAILED
            with mock.patch.object(n, P + "receive", side_effect=failed):
                error(self, m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
            with mock.patch.object(n, P + "get_counters", side_effect=failed):
                error(self, m.ErrorKind.PROVIDER_FAILED, stream.counters, n.AMS_MEL_PROVIDER_FAILED)

    def test_failed_open_output_cleanup_and_one_shot_after_invocation_exception(self):
        for case in ("failure", "throw", "diagnostic", "return", "missing"):
            with self.subTest(case=case), owners(capacity=None) as (_, channel):
                native = getattr(n, OPEN)
                close = n.ams_mel_ir_calibration_configuration_close
                cleanup_calls = []
                def opened(*args):
                    status = native(*args)
                    if case == "throw":
                        raise RuntimeError("invocation uncertainty")
                    if case == "diagnostic":
                        c.cast(args[-1], c.POINTER(c.c_size_t))[0] = 0
                    elif case == "return":
                        c.cast(args[3], c.POINTER(c.c_uint32))[0] = 99
                    elif case == "missing":
                        close(args[2], None, 0, None)
                    return n.AMS_MEL_PROVIDER_FAILED if case == "failure" else status
                def cleanup(*args):
                    cleanup_calls.append(1)
                    close(*args)
                    raise RuntimeError("secondary cleanup")
                with mock.patch.object(n, OPEN, side_effect=opened) as call, \
                        mock.patch.object(m.CalibrationConfigurationStream, "_close_function", staticmethod(cleanup)):
                    with self.assertRaises(RuntimeError if case == "throw" else m.CalibrationConfigurationOpenError):
                        channel.open_calibration_configuration()
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_configuration)
                    self.assertEqual(call.call_count, 1)
                    gc.collect()
                    self.assertEqual(cleanup_calls, [] if case == "missing" else [1])

    def test_isolated_compiled_provider_scenarios(self):
        for case in ("rich", "enabled", "malformed", "overflow", "allocation", "parent-first",
                     "consumer-first", "late", "late-closed", "in-flight", "gc-parents",
                     "register-fail", "register-throw", "independence-first", "independence-last",
                     "independence-calibration-first", "independence-calibration-last"):
            with self.subTest(case=case):
                environment = dict(os.environ)
                environment["PYTHONPATH"] = os.pathsep.join((str(Path(__file__).resolve().parent),
                                                            environment.get("PYTHONPATH", "")))
                result = subprocess.run([sys.executable, "-W", "error", __file__, "--child", case],
                                        env=environment, capture_output=True, text=True, timeout=45)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class NestedViewTests(unittest.TestCase):
    """Only allocated backing is read; invalid addresses must reject pre-read."""

    def check_invalid(self, raw, kind=PROTOCOL):
        with owners() as (_, _, stream), inject_view(c.pointer(raw)), \
                mock.patch.object(n, P + "event_close", wraps=getattr(n, P + "event_close")) as close:
            error(self, kind, stream.receive)
            self.assertEqual(close.call_count, 1)

    def test_outer_and_all_nested_span_structures(self):
        entry = n.IrSubsystemCalibrationV1()
        end = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(type(entry))
        for address, count in ((None, 1), (c.addressof(entry), 0x80000000),
                               (c.addressof(entry), c.c_size_t(-1).value),
                               (c.addressof(entry) + 1, 1), (end, 2)):
            with self.subTest(outer=(address, count)):
                self.check_invalid(RAW(c.cast(address, c.POINTER(type(entry))), count))
        for field, element in (("calibration_item_names", n.StringViewV1),
                               ("subsystem_component_ids", n.UciIdV1),
                               ("capability_ids", n.UciIdV1)):
            backing = element()
            end = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(element)
            for address, count in ((None, 1), (c.addressof(backing), 0x80000000),
                                   (c.addressof(backing), c.c_size_t(-1).value),
                                   (c.addressof(backing) + 1, 1), (end, 2)):
                with self.subTest(field=field, count=count, address=address):
                    entry = n.IrSubsystemCalibrationV1()
                    span = getattr(entry, field)
                    span.data, span.size = c.cast(address, c.POINTER(element)), count
                    self.check_invalid(RAW(c.pointer(entry), 1))

    def test_string_limits_utf8_and_nul_at_every_level(self):
        for field in ("calibration_id", "calibration_item_names", "subsystem_component_ids", "capability_ids"):
            for payload, count, kind in ((b"x", 0x80000000, PROTOCOL),
                                          (b"x", c.c_size_t(-1).value, PROTOCOL),
                                          (None, 1, PROTOCOL), (b"\xff", 1, m.ErrorKind.INVALID_UTF8),
                                          (b"a\0b", 3, PROTOCOL)):
                with self.subTest(field=field, count=count, payload=payload):
                    backing = c.create_string_buffer(payload) if payload is not None else None
                    view = n.StringViewV1(c.cast(backing, n.CharPointer), count)
                    entry = n.IrSubsystemCalibrationV1()
                    if field == "calibration_id":
                        entry.calibration_id.descriptive_label = view
                    else:
                        item = view if field == "calibration_item_names" else n.UciIdV1(descriptive_label=view)
                        getattr(entry, field).data = c.pointer(item)
                        getattr(entry, field).size = 1
                    self.check_invalid(RAW(c.pointer(entry), 1), kind)

    def test_late_nested_corruption_never_publishes_a_partial_value(self):
        entries = (n.IrSubsystemCalibrationV1 * 3)()
        names = (n.StringViewV1 * 2)()
        names[1].size = 1  # invalid only after two entries and one empty name
        entries[2].calibration_item_names = n.StringViewSpanV1(names, 2)
        self.check_invalid(RAW(entries, 3))

    def test_string_pointer_arithmetic_and_empty_pointer_ignored(self):
        end = (1 << (8 * c.sizeof(c.c_void_p))) - 1
        entry = n.IrSubsystemCalibrationV1()
        entry.calibration_id.descriptive_label = n.StringViewV1(c.cast(end, n.CharPointer), 2)
        self.check_invalid(RAW(c.pointer(entry), 1))
        # Zero counts never inspect the pointer, even a deliberately unusable one.
        entry.calibration_id.descriptive_label.size = 0
        entry.calibration_item_names.data = c.cast(1, c.POINTER(n.StringViewV1))
        entry.subsystem_component_ids.data = c.cast(1, c.POINTER(n.UciIdV1))
        entry.capability_ids.data = c.cast(1, c.POINTER(n.UciIdV1))
        with owners() as (_, _, stream), inject_view(c.pointer(RAW(c.pointer(entry), 1))):
            self.assertEqual(stream.receive(), R((E(m.UciId(bytes(16), ""), S.NOT_SET, (), (), ()),)))
        with owners() as (_, _, stream), inject_view(c.pointer(RAW(c.cast(1, c.POINTER(type(entry))), 0))):
            self.assertEqual(stream.receive(), R(()))

    def test_owned_utf8_duplicates_and_independent_collections(self):
        buffers = [c.create_string_buffer(text.encode()) for text in ("校正 µ", "name €", "", "capability 🛰")]
        def view(index):
            return n.StringViewV1(c.cast(buffers[index], n.CharPointer), len(buffers[index].value))
        identifier = n.UciIdV1((c.c_uint8 * 16)(*range(16)), view(0))
        names = (n.StringViewV1 * 3)(view(1), view(2), view(1))
        ids = (n.UciIdV1 * 2)(identifier, identifier)
        capability = n.UciIdV1((c.c_uint8 * 16)(*range(255, 239, -1)), view(3))
        entries = (n.IrSubsystemCalibrationV1 * 2)()
        entry = n.IrSubsystemCalibrationV1(identifier, 2, n.StringViewSpanV1(names, 3),
                                          n.UciIdSpanV1(ids, 2), n.UciIdSpanV1(c.pointer(capability), 1))
        entries[0] = entries[1] = entry
        raw = RAW(entries, 2)
        with owners() as (_, _, stream), inject_view(c.pointer(raw)):
            result = stream.receive()
        wanted_entry = E(m.UciId(bytes(range(16)), "校正 µ"), S.CALIBRATION_STATE_COMMAND,
                         ("name €", "", "name €"), (m.UciId(bytes(range(16)), "校正 µ"),) * 2,
                         (m.UciId(bytes(range(255, 239, -1)), "capability 🛰"),))
        for backing in buffers:
            c.memset(backing, ord("!"), c.sizeof(backing))
        c.memset(entries, 0, c.sizeof(entries))
        self.assertEqual(result, R((wanted_entry, wanted_entry)))
        assert_owned(self, result)
        self.assertEqual(pickle.loads(pickle.dumps(result)), result)

    def test_nested_allocation_failures_close_without_partial_publication(self):
        for function in ("CalibrationConfiguration", "SubsystemCalibration", "_copy_entry",
                         "_copy_id", "_copy_name", "_copy_uci_id", "_copy_string_view"):
            for failure in (MemoryError, OverflowError):
                with self.subTest(function=function, failure=failure), owners() as (_, _, stream), \
                        mock.patch.object(impl, function, side_effect=failure), \
                        mock.patch.object(n, P + "event_close", wraps=getattr(n, P + "event_close")) as close:
                    error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_pre_native_owner_allocation_does_not_consume_one_shot(self):
        with owners(capacity=None) as (_, channel):
            with mock.patch.object(impl.CalibrationConfigurationStream, "_from_owner", side_effect=MemoryError), \
                    mock.patch.object(n, OPEN) as opened:
                with self.assertRaises(MemoryError):
                    channel.open_calibration_configuration()
                opened.assert_not_called()
            with channel.open_calibration_configuration() as stream:
                self.assertEqual(stream.receive(), RICH[0])


def child(case):
    test = unittest.TestCase()
    scenario = {"malformed": "calibration-callback-malformed", "overflow": "calibration-callback-overflow",
                "allocation": "calibration-callback-allocation", "late": "calibration-late",
                "late-closed": "calibration-late", "in-flight": "calibration-in-flight",
                "register-fail": "calibration-register-fail", "register-throw": "calibration-register-throw",
                "independence-first": "metadata-rich", "independence-last": "metadata-rich"}.get(
                    case, "calibration-callback-rich")
    with tempfile.TemporaryDirectory(prefix="py021-") as folder:
        log = Path(folder) / "lifetime.log"
        log.touch()
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        session = m.Session.open(provider(), scenario, "")
        channel = session.open_control_channel(control.ControlChannelTests.config())
        if case == "enabled":
            channel.enable()
        if case.startswith("independence"):
            if case.endswith("last"):
                base = channel.open_metadata()
            if case.startswith("independence-calibration"):
                stream = channel.open_calibration_configuration()
                test.assertEqual(tuple(stream.receive() for _ in RICH), RICH)
                stream.close()
            else:
                with test.assertRaises(m.CalibrationConfigurationOpenError) as caught:
                    channel.open_calibration_configuration()
                test.assertIs(caught.exception.registration, m.CommandReturn.NOT_SUPPORTED)
            if case.endswith("first"):
                base = channel.open_metadata()
            base.close()
            channel.close()
            session.close()
            test.assertIn("library_unloaded", log.read_text().splitlines())
            return
        if case.startswith("register"):
            with test.assertRaises(m.CalibrationConfigurationOpenError):
                channel.open_calibration_configuration()
            error(test, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_configuration)
            channel.close()
            session.close()
            events = log.read_text().splitlines()
            if case == "register-fail":
                test.assertLess(events.index("late_calibration_callback_returned"), events.index("library_unloaded"))
            test.assertIn("library_unloaded", events)
            return
        if case == "allocation":
            os.environ["AMS_MEL_TEST_CALIBRATION_CONFIGURATION_CALLBACK_FAILURE"] = "allocation"
        if case == "in-flight":
            probe = n._LIBRARY.ams_mel_test_calibration_callback
            probe.argtypes, probe.restype = [c.c_uint], c.c_int
            test.assertEqual(probe(0), 1)
        stream = channel.open_calibration_configuration(2 if case == "overflow" else 16)
        retained = None
        if case == "in-flight":
            test.assertEqual(probe(1), 1)  # callback is parked inside the bridge
            stream.close()
            with log.open("a") as output:
                output.write("calibration_owner_closed\n")
            test.assertEqual(probe(2), 1)
            channel.close()  # joins the producer before destroying the channel
            session.close()
        elif case in ("consumer-first", "late-closed"):
            retained = stream.receive()
            stream.close()
            channel.close()
            session.close()
        else:
            if case in ("parent-first", "late"):
                channel.close()
                session.close()
            if case == "gc-parents":
                refs = weakref.ref(channel), weakref.ref(session)
                del channel, session
                gc.collect()
                test.assertTrue(all(ref() is None for ref in refs))
            wanted = {"malformed": (expected(10),), "overflow": (expected(0), expected(1)),
                      "allocation": (expected(1),)}.get(case, RICH)
            retained = tuple(stream.receive() for _ in wanted)
            test.assertEqual(retained, wanted)
            counts = {"malformed": (6, 0, 5), "overflow": (5, 3, 0), "allocation": (2, 0, 1),
                      "late": (4, 0, 0)}.get(case, (3, 0, 0))
            test.assertEqual(stream.counters(), m.C2MetadataCounters(*counts))
            stopped = case in ("parent-first", "late", "gc-parents")
            error(test, m.ErrorKind.STREAM_STOPPED if stopped else m.ErrorKind.TIMEOUT,
                  lambda: stream.receive(1), n.AMS_MEL_STREAM_STOPPED if stopped else n.AMS_MEL_TIMEOUT)
            stream.close()
            if case != "gc-parents":
                channel.close()
                session.close()
        events = log.read_text().splitlines()
        test.assertIn("library_unloaded", events)
        if retained is not None:
            assert_owned(test, retained)
            test.assertEqual(pickle.loads(pickle.dumps(retained)), retained)
        if case == "in-flight":
            order = [events.index(name) for name in ("calibration_owner_closed", "calibration_callback_returned",
                                                     "c2_channel_destroyed", "library_unloaded")]
            test.assertEqual(order, sorted(order))
        if case in ("late", "late-closed"):
            test.assertLess(events.index("late_calibration_callback_returned"), events.index("library_unloaded"))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--child":
        child(sys.argv[2])
    else:
        unittest.main()
