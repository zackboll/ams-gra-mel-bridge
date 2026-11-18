"""PY-MEL-019: owned responses, hostile ABI outputs and isolated DSO lifetimes."""

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
from ams_mel import _native as n, _camera_protect_response as impl
import test_control_channel as control
from test_ir_health import provider
from test_ir_health_metadata import assert_owned
from test_ir_image_los_quaternion import diagnostic, error


P = "ams_mel_ir_camera_protect_"
OPEN = "ams_mel_ir_c2_camera_protect_open"
RAW = n.IrCameraProtectResponseV1
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
S = m.CameraProtectStatus
R = m.CameraProtectResponse
RICH = (R(1, -(1 << 63), S.OK), R(0xFFFFFFFF, (1 << 63) - 1, S.UNAVAILABLE),
        R(0, -1, S.UNSUPPORTED))


@contextmanager
def owners(scenario="protect-callback-rich", capacity=16):
    with m.Session.open(provider(), scenario, "") as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            if capacity is None:
                yield session, channel
            else:
                with channel.open_camera_protect_response(capacity) as stream:
                    yield session, channel, stream


@contextmanager
def inject_view(pointer):
    def view(event, output, diag, capacity, required):
        c.cast(output, c.POINTER(c.POINTER(RAW)))[0] = pointer
        diagnostic(diag, capacity, required)
        return n.AMS_MEL_OK
    with mock.patch.object(n, P + "event_view", side_effect=view):
        yield


class CameraProtectResponseTests(unittest.TestCase):
    def test_public_api(self):
        from ams_mel import ir, _c2_types
        self.assertEqual([(s.name, s.value) for s in S],
                         [("OK", 0), ("UNAVAILABLE", 1), ("UNSUPPORTED", 2)])
        for cls in (S, R, m.CameraProtectResponseStream, m.CameraProtectResponseOpenError):
            self.assertIs(getattr(ir, cls.__name__), cls)
            self.assertIs(getattr(impl, cls.__name__), cls)
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertEqual(m.__all__.count(cls.__name__), 1)
            self.assertEqual(ir.__all__.count(cls.__name__), 1)
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        self.assertIs(impl.C2MetadataCounters, _c2_types.C2MetadataCounters)
        signatures = {
            m.ControlChannel.open_camera_protect_response:
                "(self, queue_capacity: 'int' = 16) -> 'CameraProtectResponseStream'",
            m.CameraProtectResponseStream.receive:
                "(self, timeout_ms: 'int' = 0) -> 'CameraProtectResponse'",
            m.CameraProtectResponseStream.counters: "(self) -> 'C2MetadataCounters'",
            m.CameraProtectResponseStream.close: "(self) -> 'None'",
            R: "(command_id: 'int', response_time_ns: 'int', command_response: 'CameraProtectStatus') -> None",
        }
        for function, signature in signatures.items():
            self.assertEqual(str(inspect.signature(function)), signature)
        self.assertIsInstance(inspect.getattr_static(m.CameraProtectResponseStream, "is_open"), property)
        with self.assertRaises(TypeError):
            m.CameraProtectResponseStream()
        for value in RICH:
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            assert_owned(self, value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                value.command_id = 4

    def test_sync_fifo_owned_and_no_implicit_work(self):
        with owners(capacity=None) as (_, channel):
            with mock.patch.object(n, "ams_mel_ir_c2_enable") as enable, \
                    mock.patch.object(n, "ams_mel_ir_c2_submit_camera_protect") as submit, \
                    mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as metadata:
                stream = channel.open_camera_protect_response()
                for call in (enable, submit, metadata):
                    call.assert_not_called()
            with stream, mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_camera_protect_event_close) as close:
                values = tuple(stream.receive() for _ in RICH)
                self.assertEqual(close.call_count, 3)
                self.assertTrue(all(not call.args[0]._obj.value for call in close.call_args_list))
                self.assertEqual(values, RICH)
                self.assertIs(type(stream.counters()), m.C2MetadataCounters)
                self.assertEqual(stream.counters(), m.C2MetadataCounters(3, 0, 0))
            self.assertEqual(values, RICH)
            with mock.patch.object(n, OPEN) as opened:
                error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_camera_protect_response)
                opened.assert_not_called()

    def test_capacity_validation_and_pre_native_allocations_do_not_consume(self):
        with owners(capacity=None) as (_, channel):
            with mock.patch.object(n, OPEN) as opened:
                for capacity in (0, -1, c.c_size_t(-1).value + 1, True, 1.5, None, "1"):
                    error(self, m.ErrorKind.INVALID_ARGUMENT,
                          lambda: channel.open_camera_protect_response(capacity))
                opened.assert_not_called()
            original = MemoryError("pre-native allocation")
            with mock.patch.object(impl, "_call_with_diagnostic", side_effect=original), \
                    mock.patch.object(n, OPEN) as opened:
                with self.assertRaises(MemoryError) as caught:
                    channel.open_camera_protect_response()
                self.assertIs(caught.exception, original)
                opened.assert_not_called()
            with channel.open_camera_protect_response(c.c_size_t(-1).value) as stream:
                self.assertEqual(stream.receive(), RICH[0])

    def test_registration_returns_and_native_status_are_separate(self):
        for scenario, kind, registration in (
                ("metadata-rich", m.ErrorKind.PROVIDER_FAILED, m.CommandReturn.NOT_SUPPORTED),
                ("protect-register-fail", m.ErrorKind.PROVIDER_FAILED, m.CommandReturn.FAIL),
                ("protect-register-throw", m.ErrorKind.PROVIDER_EXCEPTION, None)):
            with self.subTest(scenario=scenario), owners(scenario, None) as (_, channel):
                with self.assertRaises(m.CameraProtectResponseOpenError) as caught:
                    channel.open_camera_protect_response()
                self.assertIs(caught.exception.kind, kind)
                self.assertIs(caught.exception.registration, registration)
                with mock.patch.object(n, OPEN) as opened:
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_camera_protect_response)
                    opened.assert_not_called()
        for returned in (None, 99, int(m.CommandReturn.BAD_POINTER)):
            with self.subTest(returned=returned), owners(capacity=None) as (_, channel):
                def opened(parent, capacity, output, registration, diag, length, required):
                    if returned is not None:
                        c.cast(registration, c.POINTER(c.c_uint32))[0] = returned
                    diagnostic(diag, length, required, b"NotSupported is diagnostic prose only")
                    return n.AMS_MEL_PROVIDER_FAILED
                with mock.patch.object(n, OPEN, side_effect=opened) as call:
                    with self.assertRaises(m.CameraProtectResponseOpenError) as caught:
                        channel.open_camera_protect_response()
                    self.assertIs(caught.exception.registration,
                                  m.CommandReturn.BAD_POINTER if returned == int(m.CommandReturn.BAD_POINTER) else None)
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_camera_protect_response)
                    self.assertEqual(call.call_count, 1)

    def test_timeout_validation_and_uint32_forwarding(self):
        with owners() as (_, _, stream):
            with mock.patch.object(n, P + "receive") as receive:
                for timeout in (-1, 1 << 32, True, 1.5, None, "0"):
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
                receive.assert_not_called()
            with mock.patch.object(n, P + "receive", wraps=n.ams_mel_ir_camera_protect_receive) as receive:
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
                    mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_camera_protect_event_close) as close:
                error(self, PROTOCOL, stream.receive)
                self.assertEqual(close.call_count, 1)
        for status in (3, 0xFFFFFFFF):
            raw = RAW(0xFFFFFFFF, -(1 << 63), status)
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
                                n.ams_mel_ir_camera_protect_event_close(args[2], None, 0, None)
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
                            mock.patch.object(n, P + "event_close", wraps=n.ams_mel_ir_camera_protect_event_close) as close:
                        if case == "throw":
                            with self.assertRaises(RuntimeError):
                                stream.receive()
                        else:
                            kind = {"failure": m.ErrorKind.PROVIDER_FAILED, "utf8": m.ErrorKind.INVALID_UTF8}.get(case, PROTOCOL)
                            error(self, kind, stream.receive,
                                  n.AMS_MEL_PROVIDER_FAILED if case == "failure" else None)
                        self.assertEqual(close.call_count, 1)

    def test_conversion_and_allocation_preserve_primary_error(self):
        for function, original in (("CameraProtectResponse", MemoryError("snapshot")),
                                   ("_response_from_raw", OverflowError("conversion")),
                                   ("_response_from_raw", RuntimeError("application")),
                                   ("_response_from_raw", KeyboardInterrupt("application"))):
            with self.subTest(function=function, original=type(original)), owners() as (_, _, stream):
                native = n.ams_mel_ir_camera_protect_event_close
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
                mock.patch.object(n, "IrCameraProtectEventHandle", side_effect=MemoryError), \
                mock.patch.object(n, P + "receive") as receive:
            error(self, m.ErrorKind.INTERNAL_ERROR, stream.receive)
            receive.assert_not_called()

    def test_event_close_uncertainty_is_never_retried(self):
        for consume in (False, True):
            for case in ("failure", "throw", "diagnostic", "unconsumed"):
                with self.subTest(consume=consume, case=case), owners() as (_, _, stream):
                    native = n.ams_mel_ir_camera_protect_event_close
                    retained = []
                    def close(output, diag, capacity, required):
                        if consume:
                            native(output, diag, capacity, required)
                        else:
                            retained.append(n.IrCameraProtectEventHandle(output._obj.value))
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
                    stream = channel.open_camera_protect_response()
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
            stream = channel.open_camera_protect_response()
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
            stream = channel.open_camera_protect_response()
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
            native = n.ams_mel_ir_camera_protect_get_counters
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
        # The compiled protection fixtures do not combine callbacks with a
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
                close = n.ams_mel_ir_camera_protect_close
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
                        mock.patch.object(m.CameraProtectResponseStream, "_close_function", staticmethod(cleanup)):
                    with self.assertRaises(RuntimeError if case == "throw" else m.CameraProtectResponseOpenError):
                        channel.open_camera_protect_response()
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_camera_protect_response)
                    self.assertEqual(call.call_count, 1)
                    gc.collect()
                    self.assertEqual(cleanup_calls, [] if case == "missing" else [1])

    def test_isolated_compiled_provider_scenarios(self):
        for case in ("rich", "enabled", "malformed", "overflow", "allocation", "parent-first",
                     "consumer-first", "late", "late-closed", "in-flight", "gc-parents",
                     "register-fail", "register-throw", "independence-first", "independence-last",
                     "independence-protect-first", "independence-protect-last"):
            with self.subTest(case=case):
                environment = dict(os.environ)
                environment["PYTHONPATH"] = os.pathsep.join((str(Path(__file__).resolve().parent),
                                                            environment.get("PYTHONPATH", "")))
                result = subprocess.run([sys.executable, "-W", "error", __file__, "--child", case],
                                        env=environment, capture_output=True, text=True, timeout=45)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


def child(case):
    test = unittest.TestCase()
    scenario = {"malformed": "protect-callback-malformed", "overflow": "protect-callback-overflow",
                "allocation": "protect-callback-allocation", "late": "protect-late",
                "late-closed": "protect-late", "in-flight": "protect-in-flight",
                "register-fail": "protect-register-fail", "register-throw": "protect-register-throw",
                "independence-first": "metadata-rich", "independence-last": "metadata-rich"}.get(case, "protect-callback-rich")
    with tempfile.TemporaryDirectory(prefix="py019-") as folder:
        log = Path(folder) / "lifetime.log"
        log.touch()
        barrier = Path(folder) / "barrier"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        if case == "in-flight":
            os.environ["AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_BARRIER"] = str(barrier)
        session = m.Session.open(provider(), scenario, "")
        channel = session.open_control_channel(control.ControlChannelTests.config())
        if case == "enabled":
            channel.enable()
        if case.startswith("independence-protect"):
            if case.endswith("last"):
                base = channel.open_metadata()
            with channel.open_camera_protect_response() as responses:
                if case.endswith("first"):
                    base = channel.open_metadata()
                test.assertEqual(tuple(responses.receive() for _ in RICH), RICH)
                with mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as opened:
                    error(test, m.ErrorKind.INVALID_ARGUMENT, channel.open_camera_protect_response)
                    opened.assert_not_called()
            base.close()
            channel.close()
            session.close()
            test.assertIn("library_unloaded", log.read_text().splitlines())
            return
        if case.startswith("independence"):
            if case.endswith("first"):
                base = channel.open_metadata()
            with test.assertRaises(m.CameraProtectResponseOpenError) as caught:
                channel.open_camera_protect_response()
            test.assertIs(caught.exception.registration, m.CommandReturn.NOT_SUPPORTED)
            if case.endswith("last"):
                base = channel.open_metadata()
            base.receive(1000)
            base.close()
            channel.close()
            session.close()
            return
        if case.startswith("register"):
            with test.assertRaises(m.CameraProtectResponseOpenError):
                channel.open_camera_protect_response()
            channel.close()
            session.close()
            events = log.read_text().splitlines()
            if case == "register-fail":
                test.assertLess(events.index("late_protect_callback_returned"), events.index("library_unloaded"))
            test.assertIn("library_unloaded", events)
            return
        if case == "allocation":
            os.environ["AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_FAILURE"] = "allocation"
        stream = channel.open_camera_protect_response(2 if case == "overflow" else 16)
        if case in ("consumer-first", "late-closed", "in-flight"):
            if case == "in-flight":
                # Existing provider file barrier; no sleep-based proof. inotify
                # waits for actual file creation with a bounded select timeout.
                import select
                libc = c.CDLL(None, use_errno=True)
                fd = libc.inotify_init1(os.O_CLOEXEC | os.O_NONBLOCK)
                test.assertGreaterEqual(fd, 0)
                try:
                    test.assertGreaterEqual(libc.inotify_add_watch(fd, os.fsencode(folder), 0x100), 0)
                    barrier.with_suffix(".start").touch()
                    while not barrier.with_suffix(".entered").exists():
                        test.assertTrue(select.select([fd], [], [], 10)[0], "callback entry barrier timed out")
                        os.read(fd, 65536)
                finally:
                    os.close(fd)
            stream.close()
            with log.open("a") as output:
                output.write("protect_owner_closed\n")
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
            expected = {"malformed": (R(9, 9, S.UNSUPPORTED),),
                        "overflow": (R(0, 0, S.OK), R(1, 1, S.UNAVAILABLE)),
                        "allocation": (R(2, 2, S.UNAVAILABLE),)}.get(case, RICH)
            test.assertEqual(tuple(stream.receive() for _ in expected), expected)
            counts = {"malformed": (4, 0, 3), "overflow": (5, 3, 0), "allocation": (2, 0, 1),
                      "late": (4, 0, 0)}.get(case, (3, 0, 0))
            test.assertEqual(stream.counters(), m.C2MetadataCounters(*counts))
            kind, status = ((m.ErrorKind.STREAM_STOPPED, n.AMS_MEL_STREAM_STOPPED) if case in
                            ("parent-first", "late", "gc-parents") else (m.ErrorKind.TIMEOUT, n.AMS_MEL_TIMEOUT))
            error(test, kind, lambda: stream.receive(1), status)
            stream.close()
            if case != "gc-parents":
                channel.close()
                session.close()
        events = log.read_text().splitlines()
        test.assertIn("library_unloaded", events)
        order = (("protect_callback_entered", "protect_owner_closed", "protect_callback_returned",
                  "c2_channel_destroyed", "library_unloaded") if case == "in-flight" else
                 ("late_protect_callback_invoked", "late_protect_callback_returned", "library_unloaded")
                 if case in ("late", "late-closed") else ())
        positions = [events.index(event) for event in order]
        test.assertEqual(positions, sorted(positions))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--child":
        child(sys.argv[2])
    else:
        unittest.main()
