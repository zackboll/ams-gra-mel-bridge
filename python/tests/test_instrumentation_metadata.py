"""PY-MEL-003: owned Instrumentation metadata over the existing compiled mock."""

from __future__ import annotations

import ctypes
from dataclasses import FrozenInstanceError, replace
import gc
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as mel
from ams_mel import _native
import test_common_channel as common
import test_instrumentation as commands


PREFIX = "ams_mel_ir_instrumentation_metadata_"
COUNTERS = mel.InstrumentationMetadataCounters
PROTOCOL = mel.ErrorKind.PROTOCOL_INCONSISTENCY
INVALID = mel.ErrorKind.INVALID_ARGUMENT


def held_parents() -> None:
    """The provider future cannot finish until the controller releases it."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-metadata-held-") as directory:
        base = Path(directory)
        log = base / "log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        os.environ["AMS_MEL_TEST_INSTRUMENTATION_HOLD"] = directory
        session = mel.Session.open(commands.provider(), "instr-lifetime-held", "")
        channel = session.open_instrumentation_channel(commands.config())
        metadata = channel.open_metadata()
        channel.enable()
        request = channel.submit_level(commands.COMMAND)
        commands.wait_for_marker(base / "entered")
        session_ref, channel_ref = weakref.ref(session), weakref.ref(channel)
        session.close()
        channel.close()
        del session, channel
        gc.collect()
        test.assertIsNone(session_ref())
        test.assertIsNone(channel_ref())
        test.assertEqual(metadata.receive(), commands.REPORT)
        test.assertEqual(metadata.receive(), commands.REPORT)
        with test.assertRaises(mel.MelError) as stopped:
            metadata.receive()
        test.assertEqual(stopped.exception.kind, mel.ErrorKind.STREAM_STOPPED)
        metadata.close()
        with test.assertRaises(mel.MelError) as pending:
            request.wait(0)
        test.assertEqual(pending.exception.kind, mel.ErrorKind.TIMEOUT)
        events = log.read_text()
        test.assertNotIn("instrumentation_completed", events)
        test.assertNotIn("instrumentation_channel_destroyed", events)
        test.assertNotIn("library_unloaded", events)
        (base / "closed").touch()
        (base / "release").touch()
        test.assertEqual(request.wait(5000), commands.COMPLETED)
        request.close()
        commands.drain_completion_owners()
        events = log.read_text().splitlines()
        test.assertLess(events.index("instrumentation_completed"),
                        events.index("instrumentation_channel_destroyed"))
        test.assertLess(events.index("instrumentation_channel_destroyed"),
                        events.index("library_unloaded"))


def callback_close_barrier() -> None:
    """Close consumption while a native callback is deterministically in flight."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-metadata-callback-") as directory:
        base = Path(directory) / "callback"
        log = Path(directory) / "log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        os.environ["AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_BARRIER"] = str(base)
        session = mel.Session.open(commands.provider(), "instr-common-metadata", "")
        channel = session.open_instrumentation_channel(commands.config())
        metadata = channel.open_metadata()
        Path(str(base) + ".start").touch()
        commands.wait_for_marker(Path(str(base) + ".entered"))
        observer = _native._LIBRARY.ams_mel_test_instrumentation_callbacks
        observer.argtypes = [_native.IrInstrumentationMetadataHandle,
                             ctypes.POINTER(ctypes.c_size_t)]
        observer.restype = ctypes.c_int
        count = ctypes.c_size_t()
        test.assertEqual(observer(metadata._owner, ctypes.byref(count)), 1)
        test.assertEqual(count.value, 1)
        test.assertEqual(metadata.counters(), COUNTERS(1, 0, 0))
        # Public metadata closure does not wait for this held callback.
        metadata.close()
        test.assertTrue(channel.is_open)
        test.assertNotIn("instrumentation_callback_returned", log.read_text())
        test.assertNotIn("instrumentation_channel_destroyed", log.read_text())
        session.close()
        Path(str(base) + ".release").touch()
        channel.close()  # Joins the existing provider callback thread.
        events = log.read_text().splitlines()
        test.assertLess(events.index("instrumentation_callback_returned"),
                        events.index("instrumentation_channel_destroyed"))
        test.assertLess(events.index("instrumentation_channel_destroyed"),
                        events.index("library_unloaded"))


class _MetadataTestCase(commands._InstrumentationTestCase):
    def metadata(self, scenario="instr-rich", capacity=16, enabled=False):
        session, channel = self.open(scenario)
        if enabled:
            channel.enable()
        metadata = channel.open_metadata(capacity)
        self.addCleanup(metadata.close)
        return session, channel, metadata

    def subprocess(self, code):
        result = subprocess.run(
            [sys.executable, "-W", "error", "-c", code],
            env={**os.environ, "PYTHONPATH": os.pathsep.join((
                str(Path(mel.__file__).parent.parent), str(Path(__file__).parent)))},
            capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class InstrumentationMetadataTests(_MetadataTestCase):
    def test_public_api_and_frozen_python_owned_counters(self):
        for name in ("InstrumentationMetadata", "InstrumentationMetadataCounters"):
            self.assertIn(name, mel.__all__)
        with self.assertRaisesRegex(TypeError, "InstrumentationChannel.open_metadata"):
            mel.InstrumentationMetadata()
        _, _, metadata = self.metadata()
        self.assertIsInstance(metadata, mel.InstrumentationMetadata)
        self.assertTrue(metadata.is_open)
        counters = metadata.counters()
        self.assertEqual(counters, COUNTERS(1, 0, 0))
        common.assert_owned_graph(self, counters)
        for field in ("events_received", "events_dropped_queue_full", "malformed_or_unsupported"):
            with self.assertRaises(FrozenInstanceError):
                setattr(counters, field, 0)
        maximal = COUNTERS(2**64 - 1, 2**64 - 2, 2**63 + 1)
        self.assertEqual(maximal.events_received, 2**64 - 1)
        self.assertEqual(maximal.events_dropped_queue_full, 2**64 - 2)
        self.assertEqual(maximal.malformed_or_unsupported, 2**63 + 1)
        common.assert_owned_graph(self, maximal)

    def test_open_attached_and_enabled_synchronous_registration(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled):
                before = self.events().count("instrumentation_enabled")
                _, channel, metadata = self.metadata(enabled=enabled)
                self.assertEqual(self.events().count("instrumentation_enabled"), before + enabled)
                self.assertEqual(metadata.receive(), commands.REPORT)
                if not enabled:
                    self.assert_error(mel.ErrorKind.PROVIDER_FAILED,
                                      lambda: channel.submit_level(commands.COMMAND))
                    channel.enable()
                with channel.submit_level(commands.COMMAND) as request:
                    self.assertEqual(request.wait(5000), commands.COMPLETED)
                self.assertEqual(metadata.receive(), commands.REPORT)
                metadata.close()
                channel.close()

    def test_default_and_explicit_capacity_and_size_t_max(self):
        native = getattr(_native, PREFIX + "open")
        for capacity in (None, 1, 2, 16, 32,
                         (1 << (ctypes.sizeof(ctypes.c_size_t) * 8)) - 1):
            with self.subTest(capacity=capacity):
                _, channel = self.open("instr-config")
                with mock.patch.object(_native, PREFIX + "open", wraps=native) as opened:
                    metadata = (channel.open_metadata() if capacity is None
                                else channel.open_metadata(capacity))
                    self.addCleanup(metadata.close)
                    self.assertEqual(opened.call_args.args[1], 16 if capacity is None else capacity)
                metadata.close()
                channel.close()

    def test_invalid_capacity_never_reaches_native_or_consumes_registration(self):
        _, channel = self.open()
        overflow = 1 << (ctypes.sizeof(ctypes.c_size_t) * 8)
        for capacity in (True, False, 0, -1, 1.0, "2", None, overflow, 10**100):
            with self.subTest(capacity=capacity), mock.patch.object(_native, PREFIX + "open") as opened:
                self.assert_error(INVALID, lambda: channel.open_metadata(capacity))
                opened.assert_not_called()
        with channel.open_metadata() as metadata:
            self.assertTrue(metadata.is_open)

    def test_timeout_validation_before_native_and_active_empty_poll(self):
        _, _, metadata = self.metadata("instr-config")
        for timeout in (True, False, -1, 2**32, 1.0, "0", None):
            with self.subTest(timeout=timeout), mock.patch.object(_native, PREFIX + "receive") as receive:
                self.assert_error(INVALID, lambda: metadata.receive(timeout))
                receive.assert_not_called()
        for call in (metadata.receive, lambda: metadata.receive(0), lambda: metadata.receive(1)):
            error = self.assert_error(mel.ErrorKind.TIMEOUT, call)
            self.assertEqual(error.native_status, _native.AMS_MEL_TIMEOUT)
            self.assertTrue(metadata.is_open)

    def test_send_callback_and_command_completion_are_independent(self):
        _, channel, metadata = self.metadata()
        channel.enable()
        # A registration callback is not a command completion.
        self.assertEqual(metadata.receive(), commands.REPORT)
        with channel.submit_level(commands.COMMAND) as request:
            self.assertEqual(metadata.receive(), commands.REPORT)
            self.assertEqual(request.wait(5000), commands.COMPLETED)
            self.assertEqual(request.wait(0), commands.COMPLETED)
        with channel.submit_level(commands.COMMAND) as request:
            # Conversely waiting never consumes a metadata report.
            self.assertEqual(request.wait(5000), commands.COMPLETED)
            self.assertEqual(metadata.receive(), commands.REPORT)
        self.assertEqual(metadata.counters(), COUNTERS(3, 0, 0))
        self.assert_error(mel.ErrorKind.TIMEOUT, metadata.receive)
        self.assertEqual(self.events().count("instrumentation_send_callback_returned"), 2)

    def test_bounded_fifo_drop_incoming_and_nonresetting_counters(self):
        _, _, metadata = self.metadata("instr-overflow", 2)
        before = metadata.counters()
        self.assertEqual(before, COUNTERS(6, 4, 0))
        self.assertEqual(metadata.receive(), replace(commands.REPORT, command_id=0))
        self.assertEqual(metadata.receive(), replace(commands.REPORT, command_id=1))
        self.assert_error(mel.ErrorKind.TIMEOUT, metadata.receive)
        self.assertEqual(metadata.counters(), before)

    def test_null_and_invalid_callback_priority_are_counted_not_queued(self):
        for scenario in ("instr-callback-null", "instr-callback-invalid-priority"):
            with self.subTest(scenario=scenario):
                _, _, metadata = self.metadata(scenario)
                self.assertEqual(metadata.counters(), COUNTERS(1, 0, 1))
                self.assert_error(mel.ErrorKind.TIMEOUT, metadata.receive)

    def test_callback_allocation_failure_fails_stream(self):
        _, channel, metadata = self.metadata("instr-callback-allocation")
        self.assertEqual(metadata.counters(), COUNTERS(1, 0, 0))
        error = self.assert_error(mel.ErrorKind.PROVIDER_FAILED, metadata.receive)
        self.assertEqual(error.native_status, _native.AMS_MEL_PROVIDER_FAILED)
        channel.close()
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, metadata.receive)

    def test_failed_queue_drains_existing_reports_before_provider_failed(self):
        _, channel, metadata = self.metadata()
        channel.enable()
        with mock.patch.dict(os.environ, {"AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_FAILURE": "allocation"}):
            with channel.submit_level(commands.COMMAND) as request:
                self.assertEqual(request.wait(5000), commands.COMPLETED)
        self.assertEqual(metadata.counters(), COUNTERS(2, 0, 0))
        channel.close()
        self.assertEqual(metadata.receive(), commands.REPORT)
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, metadata.receive)

    def test_failed_registration_publishes_no_owner_and_is_one_shot(self):
        _, channel = self.open("instr-register-fail")
        native = getattr(_native, PREFIX + "open")
        native_close = mel.InstrumentationMetadata._close_function
        owners = []

        def observe(*args):
            status = native(*args)
            owners.append(ctypes.cast(args[2], ctypes.POINTER(ctypes.c_void_p))[0])
            return status

        with mock.patch.object(_native, PREFIX + "open", side_effect=observe), \
                mock.patch.object(mel.InstrumentationMetadata, "_close_function",
                                  staticmethod(mock.Mock(wraps=native_close))) as close:
            self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.open_metadata)
            self.assert_error(INVALID, channel.open_metadata)
            self.assertEqual(owners, [None, None])
            close.__func__.assert_not_called()

    def test_duplicate_registration_rejected_even_after_metadata_close(self):
        _, channel, metadata = self.metadata()
        self.assert_error(INVALID, channel.open_metadata)
        metadata.close()
        self.assert_error(INVALID, channel.open_metadata)
        self.assertEqual(self.events().count("instrumentation_callback_registered"), 1)

    def test_report_retained_after_exactly_one_native_event_close(self):
        session, channel, metadata = self.metadata()
        native_close = getattr(_native, PREFIX + "event_close")
        with mock.patch.object(_native, PREFIX + "event_close", wraps=native_close) as close:
            report = metadata.receive(2**32 - 1)
            self.assertEqual(close.call_count, 1)
            self.assertFalse(ctypes.cast(close.call_args.args[0], ctypes.POINTER(ctypes.c_void_p))[0])
        metadata.close()
        channel.close()
        session.close()
        gc.collect()
        self.assertEqual(report, commands.REPORT)
        common.assert_owned_graph(self, report)
        with self.assertRaises(FrozenInstanceError):
            report.size = 0

    def test_metadata_close_idempotent_invalidates_only_consumption(self):
        _, channel, metadata = self.metadata()
        native_close = metadata._close_function
        with mock.patch.object(metadata, "_close_function", wraps=native_close) as close:
            metadata.close()
            metadata.close()
            self.assertEqual(close.call_count, 1)
        self.assertFalse(metadata.is_open)
        for call in (metadata.receive, metadata.counters, metadata.__enter__):
            self.assert_error(INVALID, call)
        self.assertTrue(channel.is_open)
        self.assertNotIn("instrumentation_disabled", self.events())
        self.assertNotIn("instrumentation_channel_destroyed", self.events())
        channel.enable()
        with channel.submit_level(commands.COMMAND) as request:
            self.assertEqual(request.wait(5000), commands.COMPLETED)
        self.assertIn("instrumentation_send_callback_returned", self.events())
        self.assert_error(INVALID, channel.open_metadata)

    def test_parent_first_close_no_python_parents_and_queued_drain(self):
        # Avoid the test base's cleanup bound methods retaining these parents.
        session = mel.Session.open(commands.provider(), "instr-rich", "")
        channel = session.open_instrumentation_channel(commands.config())
        self.addCleanup(lambda ref=weakref.ref(channel): ref() and ref().close())
        self.addCleanup(lambda ref=weakref.ref(session): ref() and ref().close())
        metadata = channel.open_metadata()
        self.addCleanup(metadata.close)
        session_ref, channel_ref = weakref.ref(session), weakref.ref(channel)
        session.close()
        # Session closure does not deactivate a live channel's queue.
        self.assertEqual(metadata.receive(), commands.REPORT)
        self.assert_error(mel.ErrorKind.TIMEOUT, metadata.receive)
        channel.enable()
        with channel.submit_level(commands.COMMAND) as request:
            self.assertEqual(request.wait(5000), commands.COMPLETED)
        channel.close()
        del session, channel
        gc.collect()
        self.assertIsNone(session_ref())
        self.assertIsNone(channel_ref())
        self.assertTrue(metadata.is_open)
        self.assertEqual(metadata.receive(), commands.REPORT)
        self.assertEqual(metadata.counters(), COUNTERS(2, 0, 0))
        error = self.assert_error(mel.ErrorKind.STREAM_STOPPED, metadata.receive)
        self.assertEqual(error.native_status, _native.AMS_MEL_STREAM_STOPPED)

    def test_metadata_close_does_not_cancel_held_request_after_parent_close(self):
        self.subprocess("import test_instrumentation_metadata as t; t.held_parents()")

    def test_metadata_close_does_not_promise_callback_quiescence(self):
        self.subprocess("import test_instrumentation_metadata as t; t.callback_close_barrier()")

    def test_retryable_channel_detach_keeps_failed_metadata_safe(self):
        self.subprocess('''
import test_instrumentation_metadata as t
session = t.mel.Session.open(t.commands.provider(), "instr-detach-fail", "")
channel = session.open_instrumentation_channel(t.commands.config())
metadata = channel.open_metadata()
session.close()
try:
    channel.close()
except t.mel.MelError as error:
    assert error.kind == t.mel.ErrorKind.PROVIDER_FAILED
else:
    raise AssertionError("expected retryable detach failure")
assert channel.is_open
try:
    metadata.receive()
except t.mel.MelError as error:
    assert error.kind == t.mel.ErrorKind.PROVIDER_FAILED
else:
    raise AssertionError("failed metadata must not time out")
metadata.close()
channel.close()
assert not channel.is_open
''')

    def test_closed_channel_rejects_open_without_native_registration(self):
        _, channel = self.open()
        channel.close()
        with mock.patch.object(_native, PREFIX + "open") as opened:
            self.assert_error(INVALID, channel.open_metadata)
            opened.assert_not_called()

    def test_finalizer_and_context_cleanup_preserve_body_exception(self):
        _, channel = self.open()
        metadata = channel.open_metadata()
        ref = weakref.ref(metadata)
        native_close = mel.InstrumentationMetadata._close_function
        close = mock.Mock(wraps=native_close)
        with mock.patch.object(mel.InstrumentationMetadata, "_close_function", staticmethod(close)):
            del metadata
            gc.collect()
            self.assertIsNone(ref())
            self.assertEqual(close.call_count, 1)
        _, channel = self.open()
        metadata = channel.open_metadata()
        self.addCleanup(metadata.close)
        with mock.patch.object(metadata, "_close_function", side_effect=RuntimeError("cleanup")):
            with self.assertRaisesRegex(ValueError, "body"):
                with metadata:
                    raise ValueError("body")
            metadata.__del__()  # Non-raising even if the close callable raises.
            with self.assertRaisesRegex(RuntimeError, "cleanup"):
                with metadata:
                    pass
        metadata.close()
        mel.InstrumentationMetadata.__new__(mel.InstrumentationMetadata).__del__()

    def test_context_normal_and_exception_exit_close(self):
        for exceptional in (False, True):
            _, channel = self.open()
            metadata = channel.open_metadata()
            if exceptional:
                with self.assertRaisesRegex(ValueError, "body"):
                    with metadata:
                        raise ValueError("body")
            else:
                with metadata as entered:
                    self.assertIs(entered, metadata)
            self.assertFalse(metadata.is_open)


class InstrumentationMetadataBoundaryTests(_MetadataTestCase):
    """Malformed C responses absent from the mock; real owners are still released."""

    def test_success_without_event_owner_is_rejected(self):
        _, _, metadata = self.metadata("instr-config")

        def no_owner(owner, timeout, output, diagnostic, capacity, required):
            diagnostic[0] = 0
            ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 1
            return _native.AMS_MEL_OK

        with mock.patch.object(_native, PREFIX + "receive", side_effect=no_owner), \
                mock.patch.object(_native, PREFIX + "event_close") as close:
            self.assert_error(PROTOCOL, metadata.receive)
            close.assert_not_called()

    def test_malformed_event_kind_priority_null_view_and_failed_view_cleanup(self):
        native_view = getattr(_native, PREFIX + "event_view")
        native_close = getattr(_native, PREFIX + "event_close")
        for mutation in ("kind", "priority", "null", "failure"):
            _, _, metadata = self.metadata()

            def malformed(owner, output, *args):
                status = native_view(owner, output, *args)
                pointer = ctypes.cast(output, ctypes.POINTER(
                    ctypes.POINTER(_native.IrInstrumentationMetadataEventV1)))
                if mutation == "null":
                    pointer[0] = ctypes.POINTER(_native.IrInstrumentationMetadataEventV1)()
                elif mutation == "kind":
                    pointer[0].contents.kind = 2
                elif mutation == "priority":
                    pointer[0].contents.report.priority = 2
                else:
                    return _native.AMS_MEL_PROVIDER_FAILED
                return status

            with self.subTest(mutation=mutation), \
                    mock.patch.object(_native, PREFIX + "event_view", side_effect=malformed), \
                    mock.patch.object(_native, PREFIX + "event_close", wraps=native_close) as close:
                self.assert_error(mel.ErrorKind.PROVIDER_FAILED if mutation == "failure" else PROTOCOL,
                                  metadata.receive)
                self.assertEqual(close.call_count, 1)

    def test_conversion_exception_releases_event_once_without_replacing_error(self):
        native_close = getattr(_native, PREFIX + "event_close")
        for failed_cleanup in (False, True):
            _, _, metadata = self.metadata()

            def close_event(*args):
                result = native_close(*args)
                if failed_cleanup:
                    raise RuntimeError("cleanup")
                return result

            with self.subTest(failed_cleanup=failed_cleanup), \
                    mock.patch.object(mel._instrumentation, "InstrumentationReport", side_effect=MemoryError("copy")), \
                    mock.patch.object(_native, PREFIX + "event_close", side_effect=close_event) as close:
                with self.assertRaisesRegex(MemoryError, "copy"):
                    metadata.receive()
                self.assertEqual(close.call_count, 1)

    def test_failed_receive_unexpected_owner_is_closed_not_viewed(self):
        native_receive = getattr(_native, PREFIX + "receive")
        native_close = getattr(_native, PREFIX + "event_close")
        for status, kind in ((_native.AMS_MEL_TIMEOUT, mel.ErrorKind.TIMEOUT),
                             (_native.AMS_MEL_STREAM_STOPPED, mel.ErrorKind.STREAM_STOPPED),
                             (_native.AMS_MEL_PROVIDER_FAILED, mel.ErrorKind.PROVIDER_FAILED),
                             (_native.AMS_MEL_PROVIDER_EXCEPTION, mel.ErrorKind.PROVIDER_EXCEPTION),
                             (12345, mel.ErrorKind.UNKNOWN)):
            _, _, metadata = self.metadata()

            def failed(*args):
                self.assertEqual(native_receive(*args), _native.AMS_MEL_OK)
                return status

            with self.subTest(status=status), \
                    mock.patch.object(_native, PREFIX + "receive", side_effect=failed), \
                    mock.patch.object(_native, PREFIX + "event_view") as view, \
                    mock.patch.object(_native, PREFIX + "event_close", wraps=native_close) as close:
                error = self.assert_error(kind, metadata.receive)
                self.assertEqual(error.native_status, status)
                view.assert_not_called()
                self.assertEqual(close.call_count, 1)

    def test_counter_native_errors_preserved(self):
        _, _, metadata = self.metadata()
        native = getattr(_native, PREFIX + "get_counters")

        def failed(*args):
            native(*args)
            return _native.AMS_MEL_INTERNAL_ERROR

        with mock.patch.object(_native, PREFIX + "get_counters", side_effect=failed):
            self.assert_error(mel.ErrorKind.INTERNAL_ERROR, metadata.counters)

    def test_event_close_failure_is_reported_once_after_copy(self):
        _, _, metadata = self.metadata()
        native_close = getattr(_native, PREFIX + "event_close")

        def consumed(*args):
            native_close(*args)
            return _native.AMS_MEL_INTERNAL_ERROR

        with mock.patch.object(_native, PREFIX + "event_close", side_effect=consumed) as close:
            self.assert_error(mel.ErrorKind.INTERNAL_ERROR, metadata.receive)
            self.assertEqual(close.call_count, 1)

    def test_open_failure_with_published_owner_preserves_original_exception(self):
        native_open = getattr(_native, PREFIX + "open")
        native_close = mel.InstrumentationMetadata._close_function
        _, channel = self.open()

        def failed_open(*args):
            native_open(*args)
            return _native.AMS_MEL_PROVIDER_FAILED

        def failed_cleanup(*args):
            native_close(*args)
            raise RuntimeError("cleanup")

        with mock.patch.object(_native, PREFIX + "open", side_effect=failed_open), \
                mock.patch.object(mel.InstrumentationMetadata, "_close_function",
                                  staticmethod(mock.Mock(side_effect=failed_cleanup))) as close:
            self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.open_metadata)
            self.assertEqual(close.__func__.call_count, 1)

    def test_successful_open_without_owner_is_rejected(self):
        _, channel = self.open()
        native_open = getattr(_native, PREFIX + "open")
        native_close = mel.InstrumentationMetadata._close_function

        def no_owner(*args):
            status = native_open(*args)
            native_close(args[2], None, 0, None)
            return status

        with mock.patch.object(_native, PREFIX + "open", side_effect=no_owner):
            self.assert_error(PROTOCOL, channel.open_metadata)

    def test_metadata_close_respects_retryable_consumed_and_nonconsumed_success(self):
        _, _, metadata = self.metadata()
        native = metadata._close_function
        for status, kind in ((_native.AMS_MEL_PROVIDER_FAILED, mel.ErrorKind.PROVIDER_FAILED),
                             (_native.AMS_MEL_OK, PROTOCOL)):
            def retained(owner, diagnostic, capacity, required):
                diagnostic[0] = 0
                ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 1
                return status

            with mock.patch.object(metadata, "_close_function", side_effect=retained):
                self.assert_error(kind, metadata.close)
                self.assertTrue(metadata.is_open)

        def consumed(*args):
            native(*args)
            return _native.AMS_MEL_PROVIDER_FAILED

        with mock.patch.object(metadata, "_close_function", side_effect=consumed) as close:
            self.assert_error(mel.ErrorKind.PROVIDER_FAILED, metadata.close)
            self.assertFalse(metadata.is_open)
            metadata.close()
            self.assertEqual(close.call_count, 1)


if __name__ == "__main__":
    unittest.main()
