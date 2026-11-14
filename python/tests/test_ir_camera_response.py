"""PY-MEL-009 compiled mock qualification and hostile reception-boundary tests.

Expected fixture values/order follow native/tests/test_ir_camera_command.c.
Global failpoints and actual provider unload run only in fresh interpreters.
"""

from __future__ import annotations

import ctypes as c
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
from ams_mel import _camera_response as impl, _native as n
import test_ir_health as health
import test_image_stream as image_tests
from test_ir_health_metadata import assert_owned


P = "ams_mel_ir_camera_response_"
OPEN = "ams_mel_ir_image_camera_response_open"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
RAW = n.IrCameraCommandResponseV1


def rich():
    return (m.CameraCommandResponse(1, 0, -(1 << 63), True),
            m.CameraCommandResponse(0xFFFFFFFF, 0xFFFFFFFF, (1 << 63) - 1, False),
            m.CameraCommandResponse(0, 7, -1, True))


def diagnostic(output, capacity, required, message=b""):
    if output is not None and capacity:
        c.memmove(output, message + b"\0", min(capacity, len(message) + 1))
    if required is not None:
        c.cast(required, c.POINTER(c.c_size_t))[0] = len(message) + 1


class CameraResponseTests(unittest.TestCase):
    def owners(self, scenario="camera-callback-rich", capacity=16):
        session = m.Session.open(health.provider(), scenario, "")
        self.addCleanup(session.close)
        parent = session.open_image_stream(image_tests.ImageStreamTests.config())
        self.addCleanup(parent.close)
        if capacity is None:
            return session, parent
        stream = parent.open_camera_response(capacity)
        self.addCleanup(stream.close)
        return session, parent, stream

    def error(self, kind, operation, status=None):
        with self.assertRaises(m.MelError) as caught:
            operation()
        self.assertIs(caught.exception.kind, kind)
        self.assertEqual(caught.exception.native_status, status)
        return caught.exception

    def test_exact_public_signatures_identity_pickle_and_frozen_models(self):
        signatures = {
            m.ImageStream.open_camera_response:
                "(self, queue_capacity: 'int' = 16) -> 'CameraResponseStream'",
            m.CameraResponseStream.receive:
                "(self, timeout_ms: 'int' = 0) -> 'CameraCommandResponse'",
            m.CameraResponseStream.counters: "(self) -> 'CameraResponseCounters'",
            m.CameraResponseStream.close: "(self) -> 'None'",
            m.CameraCommandResponse:
                "(command_id: 'int', size: 'int', timestamp_ns: 'int', command_success: 'bool') -> None",
            m.CameraResponseCounters:
                "(events_received: 'int', events_dropped_queue_full: 'int', malformed_or_unsupported: 'int') -> None",
        }
        for function, expected in signatures.items():
            self.assertEqual(str(inspect.signature(function)), expected)
        self.assertIsInstance(inspect.getattr_static(m.CameraResponseStream, "is_open"), property)
        for cls in (m.CameraCommandResponse, m.CameraResponseCounters,
                    m.CameraResponseOpenError, m.CameraResponseStream):
            self.assertIs(getattr(impl, cls.__name__), cls)
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertIn(cls.__name__, m.__all__)
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        for value in (*rich(), m.CameraResponseCounters(1, 2, 3)):
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            assert_owned(self, value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                setattr(value, dataclasses.fields(value)[0].name, 0)
        with self.assertRaises(TypeError):
            m.CameraResponseStream()

    def test_sync_open_rich_fifo_full_width_and_active_empty_timeout(self):
        _, _, stream = self.owners()
        self.assertEqual(stream.counters(), m.CameraResponseCounters(3, 0, 0))
        self.assertEqual(tuple(stream.receive() for _ in range(3)), rich())
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        self.assertEqual(stream.counters(), m.CameraResponseCounters(3, 0, 0))

    def test_invalid_capacity_closed_parent_and_one_shot_success(self):
        _, parent = self.owners(capacity=None)
        with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
            for capacity in (0, -1, True, False, 1.5, "2", None, 1 << (8 * c.sizeof(c.c_size_t))):
                error = self.error(m.ErrorKind.INVALID_ARGUMENT,
                                   lambda: parent.open_camera_response(capacity))
                self.assertIsInstance(error, m.CameraResponseOpenError)
                self.assertIsNone(error.registration)
            self.assertEqual(opened.call_count, 0)
            with parent.open_camera_response() as stream:
                self.assertTrue(stream.is_open)
            self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_camera_response)
            self.assertEqual(opened.call_count, 1)
            parent.close()
            error = self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_camera_response)
            self.assertIsNone(error.registration)
            self.assertEqual(opened.call_count, 1)

    def test_optional_registration_return_distinctions_and_one_shot_failure(self):
        for scenario, kind, status, registration in (
                ("image-metadata-sync", m.ErrorKind.PROVIDER_FAILED,
                 n.AMS_MEL_PROVIDER_FAILED, m.CommandReturn.NOT_SUPPORTED),
                ("camera-register-fail", m.ErrorKind.PROVIDER_FAILED,
                 n.AMS_MEL_PROVIDER_FAILED, m.CommandReturn.FAIL),
                ("camera-register-throw", m.ErrorKind.PROVIDER_EXCEPTION,
                 n.AMS_MEL_PROVIDER_EXCEPTION, None)):
            with self.subTest(scenario=scenario):
                _, parent = self.owners(scenario, None)
                with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as opened:
                    error = self.error(kind, parent.open_camera_response, status)
                    self.assertIsInstance(error, m.CameraResponseOpenError)
                    self.assertIs(error.registration, registration)
                    self.assertTrue(error.diagnostic)
                    self.assertEqual(error.diagnostic_required,
                                     len(error.diagnostic.encode()) + 1)
                    repeated = self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_camera_response)
                    self.assertIsNone(repeated.registration)
                    self.assertEqual(opened.call_count, 1)

    def test_registration_failure_preserves_all_returns_and_cleans_unexpected_owner_once(self):
        native_open = getattr(n, OPEN)
        native_close = m.CameraResponseStream._close_function
        for registration in (*m.CommandReturn, None, 12345):
            for publish_owner in (False, True):
                with self.subTest(registration=registration, publish_owner=publish_owner):
                    _, parent = self.owners(capacity=None)
                    outputs = []
                    statuses = []
                    message = b"injected CameraCommandResp registration failure"

                    def register(owner, capacity, output, result, diag, size, required):
                        outputs.append(output)
                        self.assertFalse(output[0])
                        self.assertEqual(c.cast(result, c.POINTER(c.c_uint32))[0], 0xFFFFFFFF)
                        if publish_owner:
                            native_result = c.c_uint32()
                            self.assertEqual(native_open(
                                owner, capacity, output, c.byref(native_result), diag, size,
                                required), n.AMS_MEL_OK)
                            self.assertTrue(output[0])
                        if registration is not None:
                            c.cast(result, c.POINTER(c.c_uint32))[0] = int(registration)
                        diagnostic(diag, size, required, message)
                        statuses.append(n.AMS_MEL_PROVIDER_FAILED)
                        return n.AMS_MEL_PROVIDER_FAILED

                    closed = mock.Mock(wraps=native_close)
                    try:
                        with mock.patch.object(n, OPEN, side_effect=register) as opened, \
                                mock.patch.object(m.CameraResponseStream, "_close_function",
                                                  staticmethod(closed)):
                            error = self.error(m.ErrorKind.PROVIDER_FAILED,
                                               parent.open_camera_response,
                                               n.AMS_MEL_PROVIDER_FAILED)
                            self.assertIsInstance(error, m.CameraResponseOpenError)
                            self.assertEqual(statuses, [n.AMS_MEL_PROVIDER_FAILED])
                            self.assertEqual(error.diagnostic, message.decode())
                            self.assertEqual(error.diagnostic_required, len(message) + 1)
                            self.assertIsInstance(error.__cause__, m.MelError)
                            for field in ("kind", "native_status", "diagnostic", "diagnostic_required"):
                                self.assertEqual(getattr(error, field), getattr(error.__cause__, field))
                            repeated = self.error(m.ErrorKind.INVALID_ARGUMENT,
                                                  parent.open_camera_response)
                            self.assertIsInstance(repeated, m.CameraResponseOpenError)
                            self.assertIsNone(repeated.registration)
                            opened.assert_called_once()
                            self.assertFalse(outputs[0][0])
                            gc.collect()
                            self.assertEqual(closed.call_count, int(publish_owner))
                            expected = registration if isinstance(registration, m.CommandReturn) else None
                            self.assertIs(error.registration, expected)
                    finally:
                        # Reclaim a real fixture owner if a wrapper regression leaves it behind.
                        for output in outputs:
                            if output[0]:
                                native_close(output, None, 0, None)

    def test_unsupported_registration_leaves_ordinary_metadata_and_image_usable(self):
        _, parent = self.owners("image-metadata-sync", None)
        with parent.open_metadata() as ordinary:
            error = self.error(m.ErrorKind.PROVIDER_FAILED, parent.open_camera_response,
                               n.AMS_MEL_PROVIDER_FAILED)
            self.assertIs(error.registration, m.CommandReturn.NOT_SUPPORTED)
            ordinary.receive()
            parent.start()
            parent.stop()
        self.assertTrue(parent.is_open)

    def test_registration_metadata_nuc_start_receive_and_submission_are_independent(self):
        _, parent = self.owners(capacity=None)
        names = ("ams_mel_ir_image_metadata_open", "ams_mel_ir_image_nuc_open",
                 "ams_mel_ir_stream_start", "ams_mel_ir_stream_receive",
                 "ams_mel_ir_stream_submit_camera_command")
        from contextlib import ExitStack
        with ExitStack() as stack:
            calls = [stack.enter_context(mock.patch.object(n, name, wraps=getattr(n, name)))
                     for name in names]
            with parent.open_camera_response() as stream:
                self.assertEqual(stream.receive(), rich()[0])
            for call in calls:
                call.assert_not_called()
        with parent.open_metadata() as ordinary:
            self.assertEqual(ordinary.counters().events_received, 0)
            with self.assertRaises(m.NucOpenError) as caught:
                parent.open_nuc()
            self.assertIs(caught.exception.registration, m.CommandReturn.NOT_SUPPORTED)
        _, parent = self.owners("nuc-rich", None)
        with parent.open_nuc() as nuc, parent.open_metadata() as ordinary:
            self.error(m.ErrorKind.PROVIDER_FAILED, parent.open_camera_response,
                       n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(nuc.counters().events_received, 3)
            nuc.receive()
            self.assertTrue(ordinary.is_open)

    def test_overflow_drop_incoming_and_malformed_callback(self):
        _, _, stream = self.owners("camera-callback-overflow", 2)
        self.assertEqual(stream.counters(), m.CameraResponseCounters(5, 3, 0))
        self.assertEqual(tuple(stream.receive() for _ in range(2)),
                         (m.CameraCommandResponse(0, 0, 0, True),
                          m.CameraCommandResponse(1, 1, 1, False)))
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
        _, _, stream = self.owners("camera-callback-malformed", 2)
        self.assertEqual(stream.counters(), m.CameraResponseCounters(2, 0, 1))
        self.assertEqual(stream.receive(), m.CameraCommandResponse(9, 9, 9, True))
        self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)

    def test_stop_drains_queue_then_stream_stopped(self):
        _, parent, stream = self.owners()
        parent.stop()
        self.assertEqual(tuple(stream.receive() for _ in range(3)), rich())
        self.error(m.ErrorKind.STREAM_STOPPED, lambda: stream.receive(1000), n.AMS_MEL_STREAM_STOPPED)

    def test_parent_first_and_session_first_no_python_retention(self):
        for session_first in (False, True):
            session = m.Session.open(health.provider(), "camera-callback-rich", "")
            parent = session.open_image_stream(image_tests.ImageStreamTests.config())
            with parent.open_camera_response() as stream:
                session_ref, parent_ref = weakref.ref(session), weakref.ref(parent)
                if session_first:
                    session.close()
                    self.assertEqual(stream.receive(), rich()[0])
                    parent.close()
                    expected = rich()[1:]
                else:
                    parent.close()
                    session.close()
                    expected = rich()
                del session, parent
                gc.collect()
                self.assertIsNone(session_ref())
                self.assertIsNone(parent_ref())
                self.assertEqual(tuple(stream.receive() for _ in expected), expected)
                self.error(m.ErrorKind.STREAM_STOPPED, stream.receive, n.AMS_MEL_STREAM_STOPPED)

    def test_close_idempotent_does_not_close_image_and_context_management(self):
        _, parent, stream = self.owners()
        with stream as entered:
            self.assertIs(entered, stream)
        stream.close()
        self.assertFalse(stream.is_open)
        self.assertTrue(parent.is_open)
        parent.start()
        parent.stop()
        for operation in (stream.receive, stream.counters, stream.__enter__):
            self.error(m.ErrorKind.INVALID_ARGUMENT, operation)

    def test_finalizer_once_nonraising_and_context_preserves_body(self):
        _, parent = self.owners(capacity=None)
        stream = parent.open_camera_response()
        real_close = stream._close_function
        def close(*args):
            real_close(*args)
            raise KeyboardInterrupt("cleanup")
        close = mock.Mock(side_effect=close)
        stream._close_function = close
        reference = weakref.ref(stream)
        del stream
        gc.collect()
        self.assertIsNone(reference())
        self.assertEqual(close.call_count, 1)
        _, _, stream = self.owners()
        native = stream._close_function
        def throwing(*args):
            native(*args)
            raise RuntimeError("cleanup")
        stream._close_function = mock.Mock(side_effect=throwing)
        with self.assertRaisesRegex(ValueError, "body"):
            with stream:
                raise ValueError("body")
        stream.__del__()
        self.assertEqual(stream._close_function.call_count, 1)
        self.assertFalse(stream.is_open)

    def test_request_completion_and_callback_delivery_are_independent(self):
        for scenario in ("camera-sync", "camera-callback-rich"):
            _, parent, stream = self.owners(scenario)
            command = n.IrCameraCommandV1(command_id=0x5151, width=123, system_time_ns=456)
            request = n.IrCameraCommandRequestHandle()
            result = n.IrCameraCommandResultV1()
            self.assertEqual(stream.counters().events_received, 0 if scenario == "camera-sync" else 3)
            try:
                self.assertEqual(n.ams_mel_ir_stream_submit_camera_command(
                    parent._owner, c.byref(command), c.byref(request), None, 0, None), n.AMS_MEL_OK)
                if scenario == "camera-sync":
                    # Available before any Wait; callback fields differ from request result.
                    self.assertEqual(stream.receive(), m.CameraCommandResponse(0x5151, 77, -77, True))
                self.assertEqual(n.ams_mel_ir_camera_command_request_wait(
                    request, 5000, c.byref(result), None, 0, None), n.AMS_MEL_OK)
                self.assertEqual((result.response.size, result.response.timestamp_ns), (123, 456))
                if scenario == "camera-callback-rich":
                    self.assertEqual(tuple(stream.receive() for _ in range(3)), rich())
                self.error(m.ErrorKind.TIMEOUT, stream.receive, n.AMS_MEL_TIMEOUT)
            finally:
                n.ams_mel_ir_camera_command_request_close(c.byref(request), None, 0, None)

    def inject_view(self, pointer):
        native = n.ams_mel_ir_camera_response_event_view
        def view(owner, output, *args):
            status = native(owner, output, *args)
            c.cast(output, c.POINTER(c.POINTER(RAW)))[0] = pointer
            return status
        return mock.patch.object(n, P + "event_view", side_effect=view)

    def test_noncanonical_boolean_and_invalid_outer_views(self):
        raw = RAW(1, 2, -3, 1)
        backing = c.create_string_buffer(c.sizeof(RAW) + c.alignment(RAW))
        maximum = (1 << (8 * c.sizeof(c.c_void_p))) - c.alignment(RAW)
        for case in ("bool2", "boolmax", "null", "misaligned", "overflow"):
            with self.subTest(case=case):
                _, _, stream = self.owners()
                raw.command_success = 2 if case == "bool2" else 0xFFFFFFFF
                pointer = c.pointer(raw)
                if case == "null":
                    pointer = c.POINTER(RAW)()
                elif case == "misaligned":
                    pointer = c.cast(c.addressof(backing) + 1, c.POINTER(RAW))
                elif case == "overflow":
                    pointer = c.cast(maximum, c.POINTER(RAW))
                with self.inject_view(pointer), mock.patch.object(
                        n, P + "event_close", wraps=getattr(n, P + "event_close")) as close:
                    self.error(PROTOCOL, stream.receive)
                    self.assertEqual(close.call_count, 1)

    def test_receive_once_missing_owner_and_failed_receive_unexpected_owner(self):
        for case in ("null", "failure", "diagnostic-zero", "diagnostic-utf8", "diagnostic-unclosed"):
            _, _, stream = self.owners()
            native = n.ams_mel_ir_camera_response_receive
            def receive(owner, timeout, output, diag, capacity, required):
                status = native(owner, timeout, output, diag, capacity, required)
                if case == "null":
                    n.ams_mel_ir_camera_response_event_close(output, None, 0, None)
                elif case == "failure":
                    return n.AMS_MEL_PROVIDER_EXCEPTION
                elif case == "diagnostic-zero":
                    c.cast(required, c.POINTER(c.c_size_t))[0] = 0
                elif case == "diagnostic-utf8":
                    diagnostic(diag, capacity, required, b"\xff")
                else:
                    c.memset(diag, ord("x"), capacity)
                return status
            with mock.patch.object(n, P + "receive", side_effect=receive) as received, mock.patch.object(
                    n, P + "event_close", wraps=getattr(n, P + "event_close")) as close:
                kind = (m.ErrorKind.PROVIDER_EXCEPTION if case == "failure" else
                        m.ErrorKind.INVALID_UTF8 if case == "diagnostic-utf8" else PROTOCOL)
                self.error(kind, stream.receive,
                           n.AMS_MEL_PROVIDER_EXCEPTION if case == "failure" else None)
                self.assertEqual(received.call_count, 1)
                # null case's explicit native disposal also passes through this spy.
                self.assertEqual(close.call_count, 1)

    def test_event_view_failure_preserves_status_and_closes_once(self):
        _, _, stream = self.owners()
        native = n.ams_mel_ir_camera_response_event_view
        def failed(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, P + "event_view", side_effect=failed), mock.patch.object(
                n, P + "event_close", wraps=getattr(n, P + "event_close")) as close:
            self.error(m.ErrorKind.PROVIDER_FAILED, stream.receive, n.AMS_MEL_PROVIDER_FAILED)
            self.assertEqual(close.call_count, 1)

    def test_conversion_baseexceptions_cleanup_failure_preserves_first(self):
        for original in (MemoryError("copy"), KeyboardInterrupt("copy")):
            _, _, stream = self.owners()
            native = n.ams_mel_ir_camera_response_event_close
            def close(*args):
                native(*args)
                raise RuntimeError("cleanup")
            with mock.patch.object(impl, "_response_from_raw", side_effect=original), mock.patch.object(
                    n, P + "event_close", side_effect=close) as closed:
                with self.assertRaises(type(original)) as caught:
                    stream.receive()
                self.assertIs(caught.exception, original)
                self.assertEqual(closed.call_count, 1)

    def test_retained_cleanup_failure_during_conversion_is_not_retried(self):
        _, _, stream = self.owners()
        native = n.ams_mel_ir_camera_response_event_close
        retained = n.IrCameraResponseEventHandle()
        original = KeyboardInterrupt("first")
        def close(output, *args):
            retained.value = c.cast(output, c.POINTER(n.IrCameraResponseEventHandle))[0]
            return n.AMS_MEL_PROVIDER_FAILED
        try:
            with mock.patch.object(impl, "_response_from_raw", side_effect=original), mock.patch.object(
                    n, P + "event_close", side_effect=close) as closed:
                with self.assertRaises(KeyboardInterrupt) as caught:
                    stream.receive()
                self.assertIs(caught.exception, original)
                self.assertEqual(closed.call_count, 1)
        finally:
            if retained.value:
                native(c.byref(retained), None, 0, None)

    def test_receive_preserves_all_native_error_categories_without_retry(self):
        _, _, stream = self.owners()
        from ams_mel._errors import _STATUS_KINDS
        for status, kind in (*_STATUS_KINDS.items(), (12345, m.ErrorKind.UNKNOWN)):
            def receive(owner, timeout, event, diag, capacity, required):
                diagnostic(diag, capacity, required, b"native failure")
                return status
            with mock.patch.object(n, P + "receive", side_effect=receive) as received, mock.patch.object(
                    n, P + "event_close") as closed:
                error = self.error(kind, stream.receive, status)
                self.assertEqual(error.diagnostic, "native failure")
                self.assertEqual(error.diagnostic_required, 15)
                self.assertEqual(received.call_count, 1)
                closed.assert_not_called()

    def test_uint32_max_timeout_is_passed_verbatim_once(self):
        _, _, stream = self.owners()
        with mock.patch.object(n, P + "receive", wraps=getattr(n, P + "receive")) as received:
            self.assertEqual(stream.receive(0xFFFFFFFF), rich()[0])
            self.assertEqual(received.call_count, 1)
            self.assertEqual(received.call_args.args[1], 0xFFFFFFFF)

    def test_event_close_consumed_retained_and_claimed_success_never_retried(self):
        for case in ("consumed-failure", "retained-failure", "retained-success"):
            _, _, stream = self.owners()
            native = n.ams_mel_ir_camera_response_event_close
            retained = n.IrCameraResponseEventHandle()
            def close(output, diag, capacity, required):
                if case == "consumed-failure":
                    native(output, diag, capacity, required)
                else:
                    retained.value = c.cast(output, c.POINTER(n.IrCameraResponseEventHandle))[0]
                    diagnostic(diag, capacity, required)
                return n.AMS_MEL_OK if case == "retained-success" else n.AMS_MEL_PROVIDER_FAILED
            try:
                with mock.patch.object(n, P + "event_close", side_effect=close) as closed:
                    self.error(PROTOCOL if case == "retained-success" else m.ErrorKind.PROVIDER_FAILED,
                               stream.receive, None if case == "retained-success" else n.AMS_MEL_PROVIDER_FAILED)
                    self.assertEqual(closed.call_count, 1)
            finally:
                if retained.value:
                    native(c.byref(retained), None, 0, None)

    def test_owner_close_consumed_retained_claimed_success_and_no_silent_retry(self):
        for case in ("consumed-failure", "retained-failure", "retained-success"):
            _, parent = self.owners(capacity=None)
            stream = parent.open_camera_response()
            native = stream._close_function
            retained = n.IrCameraResponseMetadataHandle(stream._owner.value)
            def close(output, diag, capacity, required):
                if case == "consumed-failure":
                    native(output, diag, capacity, required)
                    retained.value = None
                else:
                    diagnostic(diag, capacity, required)
                return n.AMS_MEL_OK if case == "retained-success" else n.AMS_MEL_PROVIDER_FAILED
            stream._close_function = mock.Mock(side_effect=close)
            try:
                kind = PROTOCOL if case == "retained-success" else m.ErrorKind.PROVIDER_FAILED
                status = None if case == "retained-success" else n.AMS_MEL_PROVIDER_FAILED
                first = self.error(kind, stream.close, status)
                self.assertIs(self.error(kind, stream.close, status), first)
                stream.__del__()
                self.assertEqual(stream._close_function.call_count, 1)
                self.assertEqual(stream.is_open, case != "consumed-failure")
                self.error(m.ErrorKind.INVALID_ARGUMENT, stream.receive)
            finally:
                if retained.value:
                    native(c.byref(retained), None, 0, None)
                    stream._owner.value = None

    def test_open_malformed_success_and_failure_unexpected_owner_cleanup_once(self):
        for case in ("null", "unwritten", "invalid", "fail-return", "unsupported-return", "failure-owner"):
            _, parent = self.owners(capacity=None)
            native = getattr(n, OPEN)
            def opened(owner, capacity, output, registration, *args):
                value = c.cast(registration, c.POINTER(c.c_uint32))
                self.assertEqual(value[0], 0xFFFFFFFF)
                status = native(owner, capacity, output, registration, *args)
                if case == "null":
                    n.ams_mel_ir_camera_response_close(output, None, 0, None)
                elif case == "unwritten":
                    value[0] = 0xFFFFFFFF
                elif case == "invalid":
                    value[0] = 1234
                elif case == "fail-return":
                    value[0] = int(m.CommandReturn.FAIL)
                elif case == "unsupported-return":
                    value[0] = int(m.CommandReturn.NOT_SUPPORTED)
                elif case == "failure-owner":
                    return n.AMS_MEL_PROVIDER_FAILED
                return status
            closed = mock.Mock(wraps=n.ams_mel_ir_camera_response_close)
            with mock.patch.object(n, OPEN, side_effect=opened), mock.patch.object(
                    m.CameraResponseStream, "_close_function", staticmethod(closed)):
                error = self.error(m.ErrorKind.PROVIDER_FAILED if case == "failure-owner" else PROTOCOL,
                                   parent.open_camera_response,
                                   n.AMS_MEL_PROVIDER_FAILED if case == "failure-owner" else None)
                self.assertIsInstance(error, m.CameraResponseOpenError)
                self.assertEqual(closed.call_count, 0 if case == "null" else 1)
                gc.collect()
                self.assertEqual(closed.call_count, 0 if case == "null" else 1)
                if case in ("unwritten", "invalid"):
                    self.assertIsNone(error.registration)
                self.error(m.ErrorKind.INVALID_ARGUMENT, parent.open_camera_response)

    def test_unpublished_registration_not_inferred_from_diagnostic_and_cleanup_error(self):
        _, parent = self.owners(capacity=None)
        def opened(owner, capacity, output, registration, diag, diag_capacity, required):
            diagnostic(diag, diag_capacity, required, b"not supported: FAIL SUCCESS")
            return n.AMS_MEL_PROVIDER_FAILED
        with mock.patch.object(n, OPEN, side_effect=opened):
            error = self.error(m.ErrorKind.PROVIDER_FAILED, parent.open_camera_response, n.AMS_MEL_PROVIDER_FAILED)
            self.assertIsNone(error.registration)
        _, parent = self.owners(capacity=None)
        native = getattr(n, OPEN)
        native_close = n.ams_mel_ir_camera_response_close
        def opened(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        def close(*args):
            native_close(*args)
            raise KeyboardInterrupt("cleanup")
        with mock.patch.object(n, OPEN, side_effect=opened), mock.patch.object(
                m.CameraResponseStream, "_close_function", staticmethod(mock.Mock(side_effect=close))):
            error = self.error(m.ErrorKind.PROVIDER_FAILED, parent.open_camera_response, n.AMS_MEL_PROVIDER_FAILED)
            self.assertIs(error.registration, m.CommandReturn.SUCCESS)
            self.assertEqual(m.CameraResponseStream._close_function.call_count, 1)

    def test_open_retained_cleanup_failure_preserves_original_and_finalizes_once(self):
        _, parent = self.owners(capacity=None)
        native = getattr(n, OPEN)
        native_close = n.ams_mel_ir_camera_response_close
        retained = n.IrCameraResponseMetadataHandle()
        def opened(*args):
            native(*args)
            return n.AMS_MEL_PROVIDER_FAILED
        def close(output, *args):
            retained.value = c.cast(output, c.POINTER(n.IrCameraResponseMetadataHandle))[0]
            raise MemoryError("cleanup")
        try:
            closed = mock.Mock(side_effect=close)
            with mock.patch.object(n, OPEN, side_effect=opened), mock.patch.object(
                    m.CameraResponseStream, "_close_function", staticmethod(closed)):
                error = self.error(m.ErrorKind.PROVIDER_FAILED, parent.open_camera_response,
                                   n.AMS_MEL_PROVIDER_FAILED)
                del error
                gc.collect()
                self.assertEqual(closed.call_count, 1)
        finally:
            if retained.value:
                native_close(c.byref(retained), None, 0, None)

    def test_full_uint64_counters_native_failure_and_invalid_timeout_before_receive(self):
        _, _, stream = self.owners()
        native = n.ams_mel_ir_camera_response_get_counters
        def counters(owner, output, *args):
            native(owner, output, *args)
            raw = c.cast(output, c.POINTER(n.IrC2MetadataCountersV1)).contents
            raw.events_received = (1 << 64) - 1
            raw.events_dropped_queue_full = (1 << 63) + 1
            raw.malformed_or_unsupported = (1 << 63) - 1
            return n.AMS_MEL_OK
        with mock.patch.object(n, P + "get_counters", side_effect=counters):
            self.assertEqual(stream.counters(), m.CameraResponseCounters(
                (1 << 64) - 1, (1 << 63) + 1, (1 << 63) - 1))
        def failed(*args):
            native(*args)
            return n.AMS_MEL_INTERNAL_ERROR
        with mock.patch.object(n, P + "get_counters", side_effect=failed):
            self.error(m.ErrorKind.INTERNAL_ERROR, stream.counters, n.AMS_MEL_INTERNAL_ERROR)
        with mock.patch.object(n, P + "receive") as receive:
            for timeout in (-1, 1 << 32, True, 1.5, None, "0"):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
            receive.assert_not_called()

    def test_isolated_allocation_late_callbacks_unload_and_owned_values(self):
        for case in ("allocation", "late-owner-first", "late-parent-first", "owned",
                     "register-fail", "register-throw"):
            with self.subTest(case=case):
                env = dict(os.environ)
                env["PYTHONPATH"] = os.pathsep.join(
                    (str(Path(__file__).resolve().parent), env.get("PYTHONPATH", "")))
                result = subprocess.run(
                    [sys.executable, "-W", "error", "-c",
                     "from test_ir_camera_response import isolated; isolated(" + repr(case) + ")"],
                    env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


def isolated(case):
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-camera-response-") as directory:
        log = Path(directory) / "lifetime.log"
        with mock.patch.dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(log)):
            scenario = ("camera-callback-allocation" if case == "allocation" else
                        "camera-late" if case.startswith("late-") else
                        "camera-" + case if case.startswith("register-") else "camera-callback-rich")
            session = m.Session.open(health.provider(), scenario, "")
            parent = session.open_image_stream(image_tests.ImageStreamTests.config())
            if case.startswith("register-"):
                with test.assertRaises(m.CameraResponseOpenError):
                    parent.open_camera_response()
                parent.close()
                session.close()
            else:
                if case == "allocation":
                    os.environ["AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE"] = "allocation"
                stream = parent.open_camera_response()
                if case == "allocation":
                    libc = c.CDLL(None)
                    libc.getenv.argtypes = [c.c_char_p]
                    libc.getenv.restype = c.c_char_p
                    test.assertIsNone(libc.getenv(b"AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE"))
                    test.assertEqual(stream.counters(), m.CameraResponseCounters(2, 0, 1))
                    test.assertEqual(stream.receive(), m.CameraCommandResponse(2, 2, 2, True))
                    with test.assertRaises(m.MelError) as caught:
                        stream.receive()
                    test.assertIs(caught.exception.kind, m.ErrorKind.TIMEOUT)
                else:
                    values = tuple(stream.receive() for _ in range(3))
                if case == "late-owner-first":
                    stream.close()
                    test.assertTrue(parent.is_open)
                session.close()
                parent.close()
                if case != "late-owner-first":
                    with test.assertRaises(m.MelError) as caught:
                        stream.receive()
                    test.assertIs(caught.exception.kind, m.ErrorKind.STREAM_STOPPED)
                stream.close()
            lines = log.read_text().splitlines()
            test.assertIn("library_unloaded", lines)
            # The actual DSO mapping must be gone, not merely the public Session.
            if Path("/proc/self/maps").exists():
                test.assertNotIn(str(health.provider().resolve()), Path("/proc/self/maps").read_text())
            if case.startswith("late-") or case == "register-fail":
                names = (("camera_registration_attempted",) if case == "register-fail" else ()) + (
                    "late_camera_callback_invoked", "late_camera_callback_returned", "library_unloaded")
                indices = [lines.index(name) for name in names]
                test.assertEqual(indices, sorted(indices))
            if case not in ("allocation", "register-fail", "register-throw"):
                test.assertEqual(values, rich())
                assert_owned(test, values)
                test.assertEqual(pickle.loads(pickle.dumps(values)), rich())


if __name__ == "__main__":
    unittest.main()
