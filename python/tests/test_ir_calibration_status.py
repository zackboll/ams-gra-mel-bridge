"""PY-MEL-023: complete snapshots, hostile ABI metadata and provider lifetimes."""

from __future__ import annotations

import _ctypes
import copy
import ctypes as c
import gc
import os
import pickle
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from dataclasses import FrozenInstanceError
from pathlib import Path
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _calibration_status as impl, _native as n
import test_control_channel as control
from test_ir_health import provider
from test_ir_health_metadata import assert_owned
from test_ir_image_los_quaternion import diagnostic, error


PREFIX = "ams_mel_ir_calibration_status_"
OPEN = "ams_mel_ir_c2_calibration_status_open"
VIEW = c.POINTER(n.IrCalibrationStatusV1)
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
UTF8 = m.ErrorKind.INVALID_UTF8
INTERNAL = m.ErrorKind.INTERNAL_ERROR
R = m.SubsystemCalibrationResult
ITEM = m.CompletedCalibrationItem
ENTRY = m.CompletedCalibration
STATUS = m.CalibrationStatus
EMPTY = STATUS((), ())


def expected(seed):
    """Every field from status_values in the compiled mock, independent of ctypes."""
    def uid(offset, label):
        return m.UciId(bytes((seed + offset + i) % 256 for i in range(16)), label)

    delta = ITEM("delta", R.NOT_CALIBRATED, "not calibrated")
    return STATUS(
        (uid(0, ""), uid(40, "active-b"), uid(0, "")),
        (ENTRY(uid(100, ""), 0, R.NOT_SET, "", ()),
         ENTRY(uid(101, "completed-1"), -1, R.COMPLETED, "",
               (ITEM("alpha", R.COMPLETED, ""),)),
         ENTRY(uid(102, "completed-2"), -(1 << 63), R.FAILED, "f" * 700,
               (ITEM("", R.FAILED, "item failure"),
                ITEM("beta", R.INTERRUPTED, "q" * 600), ITEM("gamma", R.NOT_SET, ""))),
         ENTRY(uid(103, "completed-3"), (1 << 63) - 1, R.INTERRUPTED,
               "interrupted by operator", (delta, delta)),
         ENTRY(uid(104, "completed-4"), 1700000000123456789, R.NOT_CALIBRATED,
               "ünicode not calibrated", ())),
    )


RICH = (expected(10), EMPTY, expected(20))


@contextmanager
def channel_for(scenario="calstatus-callback-rich"):
    with m.Session.open(provider(), scenario, "") as session:
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            yield channel


@contextmanager
def receiver():
    with channel_for() as channel, channel.open_calibration_status() as stream:
        yield stream


def set_output(output, typ, value):
    c.cast(output, c.POINTER(typ))[0] = value


def view_of(pointer):
    def view(event, output, *diag):
        set_output(output, VIEW, pointer)
        diagnostic(*diag)
        return n.AMS_MEL_OK
    return mock.patch.object(n, PREFIX + "event_view", side_effect=view)


class CalibrationStatusTests(unittest.TestCase):
    def test_local_open_validation_and_pre_invocation_allocations(self):
        with channel_for() as channel, mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as native:
            for capacity in (0, -1, True, False, "1", 1.0, None, c.c_size_t(-1).value + 1):
                with self.subTest(capacity=capacity):
                    failure = error(self, m.ErrorKind.INVALID_ARGUMENT,
                                    lambda: channel.open_calibration_status(capacity))
                    self.assertIsInstance(failure, m.CalibrationStatusOpenError)
                    self.assertIsNone(failure.registration)
            for module, function in ((m.CalibrationStatusStream, "_from_owner"),
                                     (impl, "_call_with_diagnostic"), (c, "byref")):
                with mock.patch.object(module, function, side_effect=MemoryError):
                    with self.assertRaises(MemoryError):
                        channel.open_calibration_status()
            native.assert_not_called()
            with channel.open_calibration_status(c.c_size_t(-1).value) as stream:
                self.assertEqual(stream.receive(), RICH[0])
            error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_status)
            native.assert_called_once()

    def test_registration_failures_never_retry_and_keep_separate_returns(self):
        for scenario, kind, status, registration in (
            ("no-calstatus", m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED,
             m.CommandReturn.NOT_SUPPORTED),
            ("calstatus-register-fail", m.ErrorKind.PROVIDER_FAILED, n.AMS_MEL_PROVIDER_FAILED,
             m.CommandReturn.FAIL),
            ("calstatus-register-throw", m.ErrorKind.PROVIDER_EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION, None),
        ):
            with self.subTest(scenario=scenario), channel_for(scenario) as channel, \
                    mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as native:
                failure = error(self, kind, channel.open_calibration_status, status)
                self.assertIsInstance(failure, m.CalibrationStatusOpenError)
                self.assertIs(failure.registration, registration)
                again = error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_status)
                self.assertIsNone(again.registration)
                native.assert_called_once()

    def test_invalid_open_output_cleanup_and_uncertain_invocation(self):
        for case in ("failure", "unknown return", "invalid diagnostic", "missing owner", "exception"):
            with self.subTest(case=case), channel_for() as channel:
                open_native, close_native = getattr(n, OPEN), getattr(n, PREFIX + "close")
                original = RuntimeError("native invocation uncertainty")

                def opened(*args):
                    status = open_native(*args)
                    if case == "failure":
                        return n.AMS_MEL_PROVIDER_FAILED
                    if case == "unknown return":
                        set_output(args[3], c.c_uint32, 99)
                    if case == "invalid diagnostic":
                        set_output(args[-1], c.c_size_t, 0)
                    if case == "missing owner":
                        close_native(args[2], None, 0, None)
                    if case == "exception":
                        raise original
                    return status

                def cleanup(*args):
                    close_native(*args)
                    raise RuntimeError("secondary cleanup")

                with mock.patch.object(n, OPEN, side_effect=opened) as native, \
                        mock.patch.object(n, PREFIX + "close", side_effect=cleanup) as close:
                    if case == "exception":
                        with self.assertRaises(RuntimeError) as caught:
                            channel.open_calibration_status()
                        self.assertIs(caught.exception, original)
                    else:
                        failed = case == "failure"
                        failure = error(self, m.ErrorKind.PROVIDER_FAILED if failed else PROTOCOL,
                                        channel.open_calibration_status,
                                        n.AMS_MEL_PROVIDER_FAILED if failed else None)
                        self.assertIs(failure.registration, None if case == "unknown return"
                                      else m.CommandReturn.SUCCESS)
                    error(self, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_status)
                    native.assert_called_once()
                    gc.collect()
                    self.assertEqual(close.call_count, 0 if case == "missing owner" else 1)

    def test_receive_validation_and_exact_timeout_forwarding(self):
        with receiver() as stream, mock.patch.object(
                n, PREFIX + "receive", wraps=getattr(n, PREFIX + "receive")) as receive:
            for timeout in (-1, True, 1 << 32, 1.5, "1"):
                error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
            receive.assert_not_called()
            self.assertEqual(stream.receive((1 << 32) - 1), RICH[0])
            self.assertEqual(receive.call_args.args[1], (1 << 32) - 1)
            self.assertEqual(stream.receive(), EMPTY)
            self.assertEqual(receive.call_args.args[1], 0)

    def test_full_width_counters_and_terminal_status(self):
        with receiver() as stream:
            counts = ((1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1)

            def counters(owner, output, *diag):
                set_output(output, n.IrC2MetadataCountersV1, n.IrC2MetadataCountersV1(*counts))
                diagnostic(*diag)
                return n.AMS_MEL_OK

            with mock.patch.object(n, PREFIX + "get_counters", side_effect=counters):
                self.assertEqual(stream.counters(), m.C2MetadataCounters(*counts))
                self.assertEqual(stream.counters(), m.C2MetadataCounters(*counts))
            self.assertEqual(tuple(stream.receive() for _ in RICH), RICH)

            def failed(*args):
                diagnostic(*args[-3:], b"CalibrationStatus channel cleanup failed")
                return n.AMS_MEL_PROVIDER_FAILED

            # No compiled rich fixture fails detach. Exercise the terminal
            # error translation at the ABI boundary after the real queue drains.
            for suffix, operation in (("receive", stream.receive), ("get_counters", stream.counters)):
                with mock.patch.object(n, PREFIX + suffix, side_effect=failed) as call:
                    error(self, m.ErrorKind.PROVIDER_FAILED, operation, n.AMS_MEL_PROVIDER_FAILED)
                    call.assert_called_once()

    def test_owner_copy_context_and_finalization(self):
        with channel_for() as channel:
            stream = channel.open_calibration_status()
            for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                with self.assertRaises(TypeError):
                    operation(stream)
            close_native = stream._close_function
            cleanup_error = RuntimeError("cleanup")

            def throws(*args):
                close_native(*args)
                raise cleanup_error

            primary = KeyboardInterrupt("application")
            with mock.patch.object(stream, "_close_function", side_effect=throws) as close:
                with self.assertRaises(KeyboardInterrupt) as caught, stream as entered:
                    self.assertIs(entered, stream)
                    raise primary
                self.assertIs(caught.exception, primary)
                with self.assertRaises(RuntimeError) as cached:
                    stream.close()
                self.assertIs(cached.exception, cleanup_error)
                stream.__del__()
                close.assert_called_once()
                self.assertFalse(stream.is_open)

        with channel_for() as channel:
            stream = channel.open_calibration_status()

            def finalize(*args):
                close_native(*args)
                raise RuntimeError("finalizer cleanup")

            finalizer = mock.Mock(side_effect=finalize)
            stream._close_function = finalizer
            ref = weakref.ref(stream)
            del stream
            gc.collect()
            self.assertIsNone(ref())
            finalizer.assert_called_once()

    def test_stream_close_local_failure_is_retryable(self):
        with receiver() as stream, mock.patch.object(
                stream, "_close_function", wraps=stream._close_function) as close:
            with mock.patch.object(c, "byref", side_effect=MemoryError):
                with self.assertRaises(MemoryError):
                    stream.close()
            close.assert_not_called()
            self.assertTrue(stream.is_open)
            self.assertEqual(stream.receive(), RICH[0])
            stream.close()
            stream.close()
            close.assert_called_once()
            self.assertFalse(stream.is_open)
            for operation in (stream.receive, stream.counters, stream.__enter__):
                error(self, m.ErrorKind.INVALID_ARGUMENT, operation)

    def test_uncertain_stream_close_is_cached_and_disarmed(self):
        for case in ("failure", "exception", "diagnostic", "success retains owner"):
            for consume in (False, True) if case != "success retains owner" else (False,):
                with self.subTest(case=case, consume=consume), channel_for() as channel:
                    stream = channel.open_calibration_status()
                    real_close = stream._close_function
                    retained = c.c_void_p(stream._owner.value)

                    def close(output, *diag):
                        if consume:
                            real_close(output, *diag)
                        diagnostic(*diag, message=b"\xff" if case == "diagnostic" else b"")
                        if case == "exception":
                            raise RuntimeError("uncertain Close")
                        return n.AMS_MEL_PROVIDER_FAILED if case == "failure" else n.AMS_MEL_OK

                    try:
                        with mock.patch.object(stream, "_close_function", side_effect=close) as call:
                            if case == "exception":
                                with self.assertRaises(RuntimeError) as caught:
                                    stream.close()
                                failure = caught.exception
                            elif case == "failure":
                                failure = error(self, m.ErrorKind.PROVIDER_FAILED, stream.close,
                                                n.AMS_MEL_PROVIDER_FAILED)
                            else:
                                failure = error(self, UTF8 if case == "diagnostic" else PROTOCOL, stream.close)
                            self.assertFalse(stream.is_open)
                            for operation in (stream.close, stream.receive, stream.counters, stream.__enter__):
                                with self.assertRaises(type(failure)) as again:
                                    operation()
                                self.assertIs(again.exception, failure)
                            stream.__del__()
                            call.assert_called_once()
                    finally:
                        if not consume:
                            # Only the test knows its mock did not consume this handle.
                            self.assertEqual(real_close(c.byref(retained), None, 0, None), n.AMS_MEL_OK)

    def test_isolated_provider_contracts(self):
        for case in ("rich", "enabled", "shapes", "overflow", "malformed", "allocation", "capacity",
                     "parent-first", "consumer-first", "native-event", "gc-parents", "late", "in-flight",
                     "in-flight-parent", "configuration-first", "configuration-last", "sync-held",
                     "failed-registration-late"):
            with self.subTest(case=case):
                result = subprocess.run([sys.executable, "-W", "error", __file__, "--child", case],
                                        capture_output=True, text=True, timeout=45)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_documented_example(self):
        path = Path(__file__).resolve().parents[2] / "docs/py-mel-023-calibration-status-reception.md"
        example = path.read_text(encoding="utf-8").split("```python\n", 1)[1].split("```", 1)[0]
        result = subprocess.run([sys.executable, "-W", "error", "-c", example],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class NativeView:
    """Allocated backing for a status with two entries and a late nested item."""

    def __init__(self):
        self.buffers = []
        self.active = (n.UciIdV1 * 1)(self.uid(0, "active 校正"))
        self.items = (n.IrCompletedCalibrationItemV1 * 2)(
            n.IrCompletedCalibrationItemV1(self.text(b""), 0, self.text(b"")),
            n.IrCompletedCalibrationItemV1(self.text("gain µ".encode()), 4, self.text("原因".encode())),
        )
        self.completed = (n.IrCompletedCalibrationV1 * 2)(
            n.IrCompletedCalibrationV1(self.uid(16, ""), -(1 << 63), 1, self.text(b""),
                                       n.IrCompletedCalibrationItemSpanV1()),
            n.IrCompletedCalibrationV1(self.uid(32, "completed €"), (1 << 63) - 1, 2,
                                       self.text(b"reason"),
                                       n.IrCompletedCalibrationItemSpanV1(self.items, 2)),
        )
        self.raw = n.IrCalibrationStatusV1(n.UciIdSpanV1(self.active, 1),
                                           n.IrCompletedCalibrationSpanV1(self.completed, 2))

    def text(self, value):
        if not value:
            return n.StringViewV1()
        backing = c.create_string_buffer(value, len(value))  # No NUL terminator required.
        self.buffers.append(backing)
        return n.StringViewV1(c.cast(backing, n.CharPointer), len(value))

    def uid(self, seed, label):
        return n.UciIdV1((c.c_uint8 * 16)(*range(seed, seed + 16)), self.text(label.encode()))

    def erase(self):
        for backing in (*self.buffers, self.active, self.items, self.completed, self.raw):
            c.memset(c.addressof(backing), 0, c.sizeof(backing))


class HostileCalibrationStatusTests(unittest.TestCase):
    def reject_view(self, pointer, kind=PROTOCOL, message=None):
        with receiver() as stream, view_of(pointer), mock.patch.object(
                n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
            failure = error(self, kind, stream.receive)
            if message is not None:
                self.assertIn(message, failure.diagnostic)
            close.assert_called_once()
        return failure

    def test_fixed_view_metadata_and_no_event(self):
        self.reject_view(VIEW(), message="NULL")
        self.reject_view(c.cast(1, VIEW), message="misaligned")
        self.reject_view(c.cast(-c.alignment(n.IrCalibrationStatusV1), VIEW),
                         message="overflows the native address space")
        with receiver() as stream, mock.patch.object(n, PREFIX + "event_view") as view, \
                mock.patch.object(n, PREFIX + "event_close") as close:
            def no_owner(*args):
                diagnostic(*args[-3:])
                return n.AMS_MEL_OK

            with mock.patch.object(n, PREFIX + "receive", side_effect=no_owner):
                error(self, PROTOCOL, stream.receive)
            view.assert_not_called()
            close.assert_not_called()

    def test_all_spans_reject_invalid_structure_before_access(self):
        for scope in ("active", "completed", "items"):
            for case in ("null", "misalignment", "count limit", "max size_t", "address overflow",
                         "allocation size"):
                with self.subTest(scope=scope, case=case):
                    backing = NativeView()
                    span, element = {
                        "active": (backing.raw.active_calibration_ids, n.UciIdV1),
                        "completed": (backing.raw.completed_calibrations, n.IrCompletedCalibrationV1),
                        "items": (backing.completed[1].calibration_items, n.IrCompletedCalibrationItemV1),
                    }[scope]
                    pointer = c.POINTER(element)
                    if case == "null":
                        span.data = pointer()
                        message = "NULL"
                    elif case == "misalignment":
                        span.data = c.cast(1, pointer)
                        message = "misaligned"
                    elif case in ("count limit", "max size_t"):
                        span.size = ((1 << 31) if case == "count limit" else c.c_size_t(-1).value)
                        message = "INT32_MAX"
                    elif case == "address overflow":
                        span.data = c.cast(-c.alignment(element), pointer)
                        message = "overflows the native address space"
                    else:
                        # Model a smaller Py_ssize_t limit, still larger than
                        # the other allocated spans; check before indexing.
                        span.size = 1024
                        with mock.patch.object(sys, "maxsize", 4096):
                            self.reject_view(c.pointer(backing.raw), message="size is not representable")
                        continue
                    self.reject_view(c.pointer(backing.raw), message=message)

    def test_all_text_fields_reject_bad_size_or_encoding(self):
        for field in ("active label", "completed label", "reason", "item name", "item reason"):
            for case, kind in (("null", PROTOCOL), ("limit", PROTOCOL), ("address overflow", PROTOCOL),
                               (b"\xe2\x82", UTF8), (b"\xed\xa0\x80", UTF8), (b"x\0y", PROTOCOL)):
                with self.subTest(field=field, case=case):
                    backing = NativeView()
                    text = {
                        "active label": backing.active[0].descriptive_label,
                        "completed label": backing.completed[1].calibration_id.descriptive_label,
                        "reason": backing.completed[1].fail_reason,
                        "item name": backing.items[1].item_name,
                        "item reason": backing.items[1].fail_reason,
                    }[field]
                    if case == "null":
                        text.data, text.size = n.CharPointer(), 1
                    elif case == "limit":
                        text.size = 1 << 31
                    elif case == "address overflow":
                        text.data, text.size = c.cast(-1, n.CharPointer), 2
                    else:
                        replacement = backing.text(case)
                        text.data, text.size = replacement.data, replacement.size
                    self.reject_view(c.pointer(backing.raw), kind)

    def test_invalid_enum_at_either_level_no_partial_publication(self):
        for level in ("overall", "item"):
            for value in (5, 99, 0xFFFFFFFF):
                with self.subTest(level=level, value=value):
                    backing = NativeView()
                    if level == "overall":
                        backing.completed[1].result = value
                    else:
                        backing.items[1].result = value  # Last item of the last entry.
                    with mock.patch.object(impl, "_copy_completed", wraps=impl._copy_completed) as completed, \
                            mock.patch.object(impl, "CalibrationStatus", wraps=STATUS) as publish:
                        self.reject_view(c.pointer(backing.raw), message=f"{level} result: {value}")
                        self.assertEqual(completed.call_count, 2)
                        publish.assert_not_called()

    def test_signed_abi_and_owned_values_after_backing_erasure(self):
        backing = NativeView()
        want = STATUS(
            (m.UciId(bytes(range(16)), "active 校正"),),
            (ENTRY(m.UciId(bytes(range(16, 32)), ""), -(1 << 63), R.COMPLETED, "", ()),
             ENTRY(m.UciId(bytes(range(32, 48)), "completed €"), (1 << 63) - 1, R.FAILED, "reason",
                   (ITEM("", R.NOT_SET, ""), ITEM("gain µ", R.NOT_CALIBRATED, "原因")))),
        )
        # There are no invalid int64_t bit patterns. Inject the raw signed
        # boundaries without ever assigning an out-of-range ctypes integer.
        self.assertEqual(backing.completed[0].time_tag_ns, -(1 << 63))
        self.assertEqual(backing.completed[1].time_tag_ns, (1 << 63) - 1)
        with receiver() as stream, view_of(c.pointer(backing.raw)):
            native_close = getattr(n, PREFIX + "event_close")

            def erase_after_close(*args):
                code = native_close(*args)
                backing.erase()
                return code

            with mock.patch.object(n, PREFIX + "event_close", side_effect=erase_after_close) as close:
                received = stream.receive()
                close.assert_called_once()
        self.assertEqual(received, want)
        assert_owned(self, received)
        for value, field in ((received, "active_calibration_ids"),
                             (received.completed_calibrations[1], "time_tag_ns"),
                             (received.completed_calibrations[1].calibration_items[1], "item_name")):
            with self.assertRaises(FrozenInstanceError):
                setattr(value, field, None)
        self.assertEqual(pickle.loads(pickle.dumps(received)), want)

    def test_conversion_allocation_failures_at_all_levels(self):
        for symbol in ("CalibrationStatus", "CompletedCalibration", "CompletedCalibrationItem",
                       "_copy_uci_id", "_copy_string_view"):
            for exception in (MemoryError, OverflowError):
                with self.subTest(symbol=symbol, exception=exception), receiver() as stream, \
                        mock.patch.object(impl, symbol, side_effect=exception), mock.patch.object(
                            n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
                    error(self, INTERNAL, stream.receive)
                    close.assert_called_once()
        for call_index in (1, 2, 4):  # Active tuple, completed tuple, first nonempty item tuple.
            with self.subTest(span=call_index), receiver() as stream:
                real_span, calls = impl._checked_span, 0

                def span(*args):
                    nonlocal calls
                    calls += 1
                    if calls == call_index:
                        raise MemoryError
                    return real_span(*args)

                with mock.patch.object(impl, "_checked_span", side_effect=span), mock.patch.object(
                        n, PREFIX + "event_close", wraps=getattr(n, PREFIX + "event_close")) as close:
                    error(self, INTERNAL, stream.receive)
                    close.assert_called_once()
                self.assertEqual(stream.receive(), EMPTY)  # A later event still works.

    def test_receive_and_view_failures_after_output_preserve_primary_error(self):
        for stage in ("receive", "event_view"):
            for case in ("status", "exception", "zero diagnostic", "oversized success", "invalid UTF-8",
                         "unterminated diagnostic", "embedded NUL diagnostic"):
                with self.subTest(stage=stage, case=case), receiver() as stream:
                    real_call = getattr(n, PREFIX + stage)
                    real_close = getattr(n, PREFIX + "event_close")
                    original = RuntimeError("original native invocation failure")

                    def fail_after_output(*args):
                        self.assertEqual(real_call(*args), n.AMS_MEL_OK)
                        output, capacity, required = args[-3:]
                        if case == "status":
                            diagnostic(output, capacity, required, b"primary failure")
                            return n.AMS_MEL_PROVIDER_FAILED
                        if case == "exception":
                            raise original
                        if case == "zero diagnostic":
                            set_output(required, c.c_size_t, 0)
                        if case == "oversized success":
                            set_output(required, c.c_size_t, capacity + 1)
                        if case == "invalid UTF-8":
                            diagnostic(output, capacity, required, b"\xff")
                        if case == "unterminated diagnostic":
                            c.memmove(output, b"x", 1)
                        if case == "embedded NUL diagnostic":
                            diagnostic(output, capacity, required, b"x\0y")
                        return n.AMS_MEL_OK

                    def cleanup(*args):
                        self.assertEqual(real_close(*args), n.AMS_MEL_OK)
                        raise RuntimeError("secondary cleanup error")

                    with mock.patch.object(n, PREFIX + stage, side_effect=fail_after_output), \
                            mock.patch.object(n, PREFIX + "event_close", side_effect=cleanup) as close, \
                            mock.patch.object(impl, "_status_from_raw") as convert:
                        if case == "exception":
                            with self.assertRaises(RuntimeError) as caught:
                                stream.receive()
                            self.assertIs(caught.exception, original)
                        elif case == "status":
                            failure = error(self, m.ErrorKind.PROVIDER_FAILED, stream.receive,
                                            n.AMS_MEL_PROVIDER_FAILED)
                            self.assertEqual(failure.diagnostic, "primary failure")
                        else:
                            error(self, UTF8 if case == "invalid UTF-8" else PROTOCOL, stream.receive)
                        close.assert_called_once()
                        convert.assert_not_called()

    def test_event_close_failure_is_never_retried_or_published(self):
        for case in ("status", "exception", "diagnostic", "success retains owner"):
            for consume in (False, True) if case != "success retains owner" else (False,):
                with self.subTest(case=case, consume=consume), receiver() as stream:
                    real_close = getattr(n, PREFIX + "event_close")
                    retained = c.c_void_p()

                    def close(output, *diag):
                        if consume:
                            real_close(output, *diag)
                        else:
                            retained.value = c.cast(output, c.POINTER(c.c_void_p))[0]
                        diagnostic(*diag)
                        if case == "diagnostic":
                            set_output(diag[-1], c.c_size_t, 0)
                        elif case == "exception":
                            raise RuntimeError("event close raised")
                        return n.AMS_MEL_PROVIDER_FAILED if case == "status" else n.AMS_MEL_OK

                    try:
                        with mock.patch.object(n, PREFIX + "event_close", side_effect=close) as call:
                            if case == "exception":
                                with self.assertRaisesRegex(RuntimeError, "event close raised"):
                                    stream.receive()
                            elif case == "status":
                                error(self, m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
                            else:
                                error(self, PROTOCOL, stream.receive)
                            call.assert_called_once()
                    finally:
                        if retained.value:
                            self.assertEqual(real_close(c.byref(retained), None, 0, None), n.AMS_MEL_OK)

    def test_pre_invocation_event_close_allocation_failure_gets_one_cleanup(self):
        with receiver() as stream:
            real_call = impl._call_with_diagnostic
            real_close = getattr(n, PREFIX + "event_close")

            def fail_before_close(call):
                if call.__name__ == "close_event":
                    raise MemoryError
                return real_call(call)

            with mock.patch.object(impl, "_call_with_diagnostic", side_effect=fail_before_close), \
                    mock.patch.object(n, PREFIX + "event_close", wraps=real_close) as close:
                error(self, INTERNAL, stream.receive)
                close.assert_called_once()
                self.assertEqual(close.call_args.args[1:], (None, 0, None))


def probe(library, name, *argtypes):
    fn = getattr(library, name)
    fn.argtypes, fn.restype = list(argtypes), c.c_int
    return fn


def run_private_request_fixture(test, channel, stream, case):
    """Drive the metadata fixtures through the existing native C-test controls."""
    channel.enable()
    result_type = n.IrCalibrationStatusResultV1
    submit = n.ams_mel_ir_c2_submit_calibration_status
    wait = n.ams_mel_ir_calibration_status_request_wait
    close = n.ams_mel_ir_calibration_status_request_close

    @contextmanager
    def start(seed, mode):
        command = n.IrCalibrationStatusCommandV1(seed, mode)
        request = n.IrCalibrationStatusRequestHandle()
        test.assertEqual(submit(channel._owner, c.byref(command), c.byref(request), None, 0, None), n.AMS_MEL_OK)
        try:
            yield request
        finally:
            test.assertEqual(close(c.byref(request), None, 0, None), n.AMS_MEL_OK)

    def emit(seed, mode):
        with start(seed, mode) as request:
            result = result_type()
            test.assertEqual(wait(request, 15000, c.byref(result), None, 0, None), n.AMS_MEL_COMMAND_REJECTED)
            test.assertEqual(result.error_code, n.AMS_MEL_ERROR_UNSUPPORTED)

    if case == "sync-held":
        library = c.CDLL(str(provider()), mode=os.RTLD_NOW | os.RTLD_NOLOAD)
        try:
            release = probe(library, "mock_completion_release_family", c.c_uint, c.c_uint)
            with start(5, 6) as request:
                result = result_type()
                test.assertEqual(stream.receive(), expected(77))
                test.assertEqual(wait(request, 0, c.byref(result), None, 0, None), n.AMS_MEL_TIMEOUT)
                stream.close()
                test.assertEqual(wait(request, 0, c.byref(result), None, 0, None), n.AMS_MEL_TIMEOUT)
                test.assertEqual(release(10, 1), 1)  # Status family in the mock provider.
                test.assertEqual(wait(request, 15000, c.byref(result), None, 0, None), n.AMS_MEL_OK)
                test.assertEqual(impl._status_from_raw(result.status), expected(5))
        finally:
            _ctypes.dlclose(library._handle)
    else:
        failpoint = "AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE"
        # Reproduce the compiled C capacity contract through the private ABI.
        emit(41, 0)
        os.environ[failpoint] = "allocation"
        getenv = c.CDLL(None).getenv
        getenv.argtypes, getenv.restype = [c.c_char_p], c.c_char_p
        emit(42, 0)
        test.assertEqual(stream.counters(), m.C2MetadataCounters(2, 1, 0))
        for mode in range(1, 11):  # Each of nine native mutations, then null.
            emit(43, mode)
            test.assertEqual(getenv(failpoint.encode()), b"allocation")
            test.assertEqual(stream.counters(), m.C2MetadataCounters(2 + mode, 1, mode))
        test.assertEqual(stream.receive(), expected(41))
        emit(44, 0)
        test.assertIsNone(getenv(failpoint.encode()))
        error(test, m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        test.assertEqual(stream.counters(), m.C2MetadataCounters(13, 1, 11))
        emit(45, 0)
        test.assertEqual(stream.receive(), expected(45))
        test.assertEqual(stream.counters(), m.C2MetadataCounters(14, 1, 11))

    # Native terminal publication precedes worker reclamation. Await the
    # latter before expecting the final provider dlclose in this subprocess.
    boundary = probe(n._LIBRARY, "ams_mel_test_completion_boundary", c.c_uint, c.c_uint,
                     c.c_uint64, c.POINTER(c.c_uint64))
    owner = probe(n._LIBRARY, "ams_mel_test_completion_owner", c.c_uint, c.c_uint64, c.POINTER(c.c_uint64))
    values, observed = (c.c_uint64 * 4)(), c.c_uint64()
    test.assertEqual(boundary(9, 3, 0, values), 1)  # Status completion-probe family.
    test.assertEqual(owner(9, values[2], c.byref(observed)), 1)


def native_event_survives(test, stream, channel, session, log):
    event = n.IrCalibrationStatusEventHandle()
    test.assertEqual(n.ams_mel_ir_calibration_status_receive(
        stream._owner, 0, c.byref(event), None, 0, None), n.AMS_MEL_OK)
    try:
        # Close all other owners before touching the event's graph.
        stream.close()
        channel.close()
        session.close()
        test.assertIn("library_unloaded", log.read_text().splitlines())
        view = VIEW()
        test.assertEqual(n.ams_mel_ir_calibration_status_event_view(
            event, c.byref(view), None, 0, None), n.AMS_MEL_OK)
        result = impl._status_from_raw(view.contents)
    finally:
        test.assertEqual(n.ams_mel_ir_calibration_status_event_close(
            c.byref(event), None, 0, None), n.AMS_MEL_OK)
    test.assertEqual(result, RICH[0])
    assert_owned(test, result)


def child(case):
    test = unittest.TestCase()
    scenario = {
        "shapes": "calstatus-callback-shapes", "overflow": "calstatus-callback-overflow",
        "malformed": "calstatus-callback-malformed", "allocation": "calstatus-callback-allocation",
        "capacity": "calstatus-callback-capacity", "late": "calstatus-late",
        "in-flight": "calstatus-in-flight", "in-flight-parent": "calstatus-in-flight",
        "sync-held": "calstatus-sync-held",
        "configuration-first": "calibration-callback-rich",
        "failed-registration-late": "calstatus-register-fail",
    }.get(case, "calstatus-callback-rich")
    with tempfile.TemporaryDirectory(prefix="py023-") as folder:
        log = Path(folder) / "lifetime.log"
        log.touch()
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        session = m.Session.open(provider(), scenario, "")
        channel = session.open_control_channel(control.ControlChannelTests.config())
        if case == "enabled":
            channel.enable()
        elif case == "allocation":
            os.environ["AMS_MEL_TEST_CALIBRATION_STATUS_COPY_FAILURE"] = "allocation"
        elif case in ("in-flight", "in-flight-parent"):
            barrier = probe(n._LIBRARY, "ams_mel_test_calibration_status_callback", c.c_uint)
            test.assertEqual(barrier(0), 1)

        values = None
        if case in ("configuration-first", "failed-registration-late"):
            if case == "configuration-first":
                with channel.open_calibration_configuration() as configuration:
                    test.assertEqual(len(configuration.receive().entries), 3)
                want = m.CommandReturn.NOT_SUPPORTED
            else:
                want = m.CommandReturn.FAIL
            with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as register:
                failure = error(test, m.ErrorKind.PROVIDER_FAILED, channel.open_calibration_status,
                                n.AMS_MEL_PROVIDER_FAILED)
                test.assertIs(failure.registration, want)
                error(test, m.ErrorKind.INVALID_ARGUMENT, channel.open_calibration_status)
                register.assert_called_once()
        else:
            capacity = {"overflow": 2, "capacity": 1, "malformed": 1}.get(case, 16)
            # Opening a status consumer cannot call any other admission path.
            with mock.patch.object(n, "ams_mel_ir_c2_enable") as enable, \
                    mock.patch.object(n, "ams_mel_ir_c2_submit_calibration_status") as submit, \
                    mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as base, \
                    mock.patch.object(n, "ams_mel_ir_c2_calibration_configuration_open") as config:
                stream = channel.open_calibration_status(capacity)
                for operation in (enable, submit, base, config):
                    operation.assert_not_called()

            if case == "in-flight":
                test.assertEqual(barrier(1), 1)  # Callback counted in flight and parked.
                try:
                    test.assertEqual(stream.counters(), m.C2MetadataCounters(1, 0, 0))
                    stream.close()
                    with log.open("a") as output:
                        output.write("status_consumer_closed\n")
                finally:
                    test.assertEqual(barrier(2), 1)
            elif case == "in-flight-parent":
                test.assertEqual(barrier(1), 1)
                with ThreadPoolExecutor(max_workers=1) as worker:
                    closing = worker.submit(channel.close)
                    try:
                        # The receiver has its own handle. Deactivation wakes
                        # it while parent teardown joins the parked callback.
                        error(test, m.ErrorKind.STREAM_STOPPED, lambda: stream.receive(5000),
                              n.AMS_MEL_STREAM_STOPPED)
                        test.assertFalse(closing.done())
                        test.assertNotIn("c2_channel_destroyed", log.read_text().splitlines())
                    finally:
                        test.assertEqual(barrier(2), 1)
                    closing.result(timeout=10)
            elif case in ("capacity", "sync-held"):
                run_private_request_fixture(test, channel, stream, case)
            elif case == "native-event":
                native_event_survives(test, stream, channel, session, log)
            else:
                if case == "configuration-last":
                    failure = error(test, m.ErrorKind.PROVIDER_FAILED, channel.open_calibration_configuration,
                                    n.AMS_MEL_PROVIDER_FAILED)
                    test.assertIs(failure.registration, m.CommandReturn.NOT_SUPPORTED)
                if case == "parent-first":
                    channel.close()
                    session.close()
                if case == "gc-parents":
                    refs = weakref.ref(channel), weakref.ref(session)
                    del channel, session
                    gc.collect()
                    test.assertTrue(all(ref() is None for ref in refs))
                shape = expected(30)
                want = {
                    "shapes": (EMPTY, STATUS(shape.active_calibration_ids, ()),
                               STATUS((), shape.completed_calibrations),
                               STATUS((shape.active_calibration_ids[1],), (shape.completed_calibrations[2],))),
                    "overflow": (expected(0), expected(1)), "malformed": (RICH[0],),
                    "allocation": (expected(1),),
                }.get(case, RICH)
                counters = {"shapes": (4, 0, 0), "overflow": (5, 3, 0),
                            "malformed": (11, 0, 10), "allocation": (2, 0, 1)}.get(case, (3, 0, 0))
                stopped = case in ("parent-first", "gc-parents")
                if stopped:
                    test.assertIn("library_unloaded", log.read_text().splitlines())
                values = tuple(stream.receive() for _ in want)
                test.assertEqual(values, want)
                test.assertEqual(stream.counters(), m.C2MetadataCounters(*counters))
                error(test, m.ErrorKind.STREAM_STOPPED if stopped else m.ErrorKind.TIMEOUT,
                      lambda: stream.receive(1), n.AMS_MEL_STREAM_STOPPED if stopped else n.AMS_MEL_TIMEOUT)
            stream.close()

        if case != "gc-parents":
            channel.close()
            session.close()
        events = log.read_text().splitlines()
        test.assertIn("library_unloaded", events)
        if values is not None:
            test.assertEqual(values, want)
            assert_owned(test, values)
            test.assertEqual(pickle.loads(pickle.dumps(values)), want)
        if case == "in-flight":
            order = [events.index(event) for event in ("status_consumer_closed", "calibration_status_callback_returned",
                                                       "c2_channel_destroyed", "library_unloaded")]
            test.assertEqual(order, sorted(order))
        if case in ("late", "failed-registration-late"):
            test.assertLess(events.index("late_calibration_status_callback_returned"), events.index("library_unloaded"))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--child":
        child(sys.argv[2])
    else:
        unittest.main()
