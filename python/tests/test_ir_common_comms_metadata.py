"""PY-MEL-015: real compiled callbacks, independent futures, hostile ABI outputs.

Retained callbacks, unload and concurrent teardown run in fresh interpreters.
No fabricated pointer is passed to any real native operation.
"""

from __future__ import annotations

from contextlib import closing
import ctypes as c
import dataclasses
import gc
import os
from pathlib import Path
import pickle
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _comms_metadata, _errors, _native as n, _types
import test_control_channel as control
import test_image_stream as images
import test_instrumentation as instrumentation
import test_ir_health as health
import test_ir_image_control as image_control
from test_ir_c2_metadata import diagnostic


P = "ams_mel_ir_channel_comms_metadata_"
BASE = "e00369ff95d8adbccd8d2e32012addf2f966a2ec"
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
SYNC = m.CommsTestReport(0xABCDEF01, 0xFEDCBA98)
MAX64 = (1 << 64) - 1


def attach(session, family):
    if family == "health":
        return session.open_health_channel(health.config())
    if family == "image":
        return session.open_image_stream(images.ImageStreamTests.config())
    return session.open_image_control(image_control.config())


def error(test, kind, operation):
    with test.assertRaises(m.MelError) as caught:
        operation()
    test.assertIs(caught.exception.kind, kind)
    return caught.exception


class CommsMetadataTests(health._HealthTestCase):
    def receiver(self, scenario="parity", family="health", capacity=16, enabled=False):
        session = m.Session.open(health.provider(), "common-meta-" + scenario, "")
        self.addCleanup(session.close)
        channel = attach(session, family)
        self.addCleanup(channel.close)
        if enabled:
            channel.enable() if family == "health" else channel.start()
        view = channel.channel_view()
        self.addCleanup(view.close)
        stream = view.open_comms_metadata(capacity)
        self.addCleanup(stream.close)
        return session, channel, view, stream

    def test_public_identity_frozen_pickle_and_existing_report(self):
        self.assertIs(m.CommsMetadataStream, _comms_metadata.CommsMetadataStream)
        self.assertIs(m.CommsMetadataCounters, _comms_metadata.CommsMetadataCounters)
        self.assertIs(m.CommsTestReport, _types.CommsTestReport)
        for cls in (m.CommsMetadataCounters, m.CommsMetadataStream):
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        value = m.CommsMetadataCounters(MAX64, MAX64 - 1, 1 << 63)
        self.assertEqual(pickle.loads(pickle.dumps(value)), value)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            value.events_received = 0
        with self.assertRaises(TypeError):
            m.CommsMetadataStream()
        self.assertIsNot(m.CommsMetadataCounters, m.C2MetadataCounters)
        self.assertIsNot(m.CommsMetadataCounters, m.HealthMetadataCounters)

    def test_capacity_validation_before_native_and_registration_not_consumed(self):
        _, channel = self.open("common-meta-parity")
        with channel.channel_view() as view:
            with mock.patch.object(n, P + "open", wraps=getattr(n, P + "open")) as opened:
                for value in (0, -1, True, False, 1.5, "2", 1 << (8 * c.sizeof(c.c_size_t))):
                    self.error(m.ErrorKind.INVALID_ARGUMENT,
                               lambda: view.open_comms_metadata(value))
                opened.assert_not_called()
                with view.open_comms_metadata() as stream:
                    self.assertTrue(stream.is_open)
                opened.assert_called_once()
            self.assertEqual(self.events().count("common_comms_registration"), 1)
        with mock.patch.object(n, P + "open") as opened:
            self.error(m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
            opened.assert_not_called()

    def test_health_attached_and_enabled_callback_separate_from_future(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled):
                session, channel, view, stream = self.receiver(enabled=enabled)
                self.assertEqual(self.events().count("health_enabled"), int(enabled))
                for command, request in ((0x80000001, 0xF0000003), (0xFFFFFFFF, 0xFFFFFFFF)):
                    with closing(view.submit_comms_test(m.CommsTestRequest(command, 4, request))) as future:
                        # Callback is already available without waiting on this future.
                        report = stream.receive(1000)
                        self.assertIs(type(report), m.CommsTestReport)
                        self.assertEqual(report, m.CommsTestReport(command, request))
                        self.assertEqual(future.wait(1000), m.CommsCompleted(
                            m.CommsTestReport(command, request)))
                stream.close()
                channel.close()
                session.close()

    def test_image_acquisition_and_zero_layout_control(self):
        for family in ("image", "control"):
            with self.subTest(family=family):
                before = len(self.events())
                session, channel, view, stream = self.receiver("zero" if family == "control" else "parity", family)
                if family == "control":
                    image_control.assert_zero(self, view.capabilities())
                with closing(view.submit_comms_test(m.CommsTestRequest(0xFFFFFFFF, 1, 0x80000000))) as future:
                    self.assertEqual(stream.receive(), m.CommsTestReport(0xFFFFFFFF, 0x80000000))
                    self.assertEqual(future.wait(1000), m.CommsCompleted(
                        m.CommsTestReport(0xFFFFFFFF, 0x80000000)))
                stream.close()
                channel.close()
                session.close()
                if family == "control":
                    # Check only this attachment's events, after acquisition case.
                    lines = self.events()[before:]
                    image_control.assert_no_storage(self, lines)
                    self.assertNotIn("image_enabled", lines)

    def test_image_normal_metadata_is_a_separate_subscription(self):
        _, channel, view, stream = self.receiver("parity", "control")
        with channel.open_metadata() as ordinary:
            self.error(m.ErrorKind.TIMEOUT, ordinary.receive)
            with closing(view.submit_comms_test(m.CommsTestRequest(3, 1, 7))) as future:
                self.assertEqual(stream.receive(), m.CommsTestReport(3, 7))
                self.error(m.ErrorKind.TIMEOUT, ordinary.receive)
                self.assertEqual(future.wait(1000), m.CommsCompleted(m.CommsTestReport(3, 7)))

    def test_synchronous_fifo_drop_incoming_and_counters_do_not_consume(self):
        _, _, _, stream = self.receiver("sync")
        self.assertEqual(stream.counters(), m.CommsMetadataCounters(1, 0, 0))
        self.assertEqual(stream.receive(), SYNC)
        for family in ("health", "image", "control"):
            _, _, _, queue = self.receiver("overflow", family, 2)
            expected = m.CommsMetadataCounters(6, 4, 0)
            self.assertEqual(queue.counters(), expected)
            for i in range(2):
                self.assertEqual(queue.receive(), m.CommsTestReport(0x80000000 + i, 0xF0000000 + i))
                self.assertEqual(queue.counters(), expected)
            self.error(m.ErrorKind.TIMEOUT, queue.receive)

    def test_null_callback_and_active_timeout_does_not_cancel_registration(self):
        _, _, view, stream = self.receiver("null")
        self.assertEqual(stream.counters(), m.CommsMetadataCounters(1, 0, 1))
        self.error(m.ErrorKind.TIMEOUT, stream.receive)
        self.error(m.ErrorKind.TIMEOUT, lambda: stream.receive(1))
        with closing(view.submit_comms_test(m.CommsTestRequest(1, 4, 2))) as future:
            self.assertEqual(stream.receive(), m.CommsTestReport(1, 2))
            future.wait(1000)
        self.assertEqual(stream.counters(), m.CommsMetadataCounters(2, 0, 1))

    def test_timeout_validation_before_receive(self):
        _, _, _, stream = self.receiver()
        with mock.patch.object(n, P + "receive") as receive:
            for timeout in (-1, True, 1.5, "0", 1 << 32):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: stream.receive(timeout))
            receive.assert_not_called()

    def test_future_only_does_not_manufacture_metadata(self):
        _, _, view, stream = self.receiver("future-only")
        with closing(view.submit_comms_test(m.CommsTestRequest(11, 4, 13))) as future:
            self.assertEqual(future.wait(1000), m.CommsCompleted(m.CommsTestReport(11, 13)))
        self.error(m.ErrorKind.TIMEOUT, stream.receive)
        self.assertEqual(stream.counters(), m.CommsMetadataCounters(0, 0, 0))

    def test_receiver_survives_view_and_queued_reports_survive_typed_close(self):
        _, channel, view, stream = self.receiver("sync")
        view.close()
        self.assertTrue(stream.is_open)
        channel.close()
        self.assertEqual(stream.receive(), SYNC)
        self.error(m.ErrorKind.STREAM_STOPPED, stream.receive)
        stream.close()
        stream.close()
        self.assertFalse(stream.is_open)
        for operation in (stream.receive, stream.counters, stream.__enter__):
            self.error(m.ErrorKind.INVALID_ARGUMENT, operation)

    def test_receiver_close_does_not_unregister_or_cancel_future(self):
        _, _, view, stream = self.receiver()
        stream.close()
        with closing(view.submit_comms_test(m.CommsTestRequest(23, 4, 29))) as future:
            self.assertEqual(future.wait(1000), m.CommsCompleted(m.CommsTestReport(23, 29)))
        self.error(m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
        self.assertEqual(self.events().count("common_comms_registration"), 1)

    def test_c2_rejected_and_dedicated_receiver_works_without_duplicate_registration(self):
        session = m.Session.open(health.provider(), "common-meta-parity", "")
        self.addCleanup(session.close)
        with session.open_control_channel(control.ControlChannelTests.config()) as channel:
            with channel.channel_view() as view:
                self.error(m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
            with channel.open_metadata() as stream:
                stream.enable_comms_test_events()
                with channel.channel_view() as view:
                    with closing(view.submit_comms_test(m.CommsTestRequest(31, 2, 37))) as future:
                        event = stream.receive(1000)
                        self.assertEqual(event.kind, m.C2MetadataKind.CHANNEL_COMMS_TEST)
                        self.assertEqual(event.payload, m.CommsTestReport(31, 37))
                        self.assertEqual(future.wait(1000), m.CommsCompleted(m.CommsTestReport(31, 37)))
        self.assertNotIn("common_comms_registration", self.events())
        self.assertFalse(hasattr(m.Session, "open_stacked_image"))
        self.assertFalse(hasattr(m, "StackedImage"))

    def test_instrumentation_is_not_a_successful_empty_receiver(self):
        with m.Session.open(health.provider(), "common-meta-parity", "") as session:
            with session.open_instrumentation_channel(instrumentation.config()) as channel:
                with channel.channel_view() as view:
                    self.error(m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)

    def test_baseline_negative_control(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            archive = subprocess.check_output(["git", "-C", str(root), "archive", BASE, "python"])
            subprocess.run(["tar", "-x", "-C", directory], input=archive, check=True)
            environment = dict(os.environ, PYTHONPATH=str(Path(directory) / "python"))
            subprocess.run([sys.executable, "-W", "error", "-c", "\n".join((
                "import ams_mel as m", "from ams_mel import _native as n",
                "assert not hasattr(m.ChannelView, 'open_comms_metadata')",
                "assert not hasattr(m, 'CommsMetadataStream')",
                "assert not hasattr(m, 'CommsMetadataCounters')",
                "assert len(n.ams_mel_ir_channel_comms_metadata_open.argtypes) == 7",
            ))], cwd=directory, env=environment, check=True, timeout=30)

    def test_documented_examples_against_compiled_mock(self):
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-015-channel-comms-metadata.md"
        source = document.read_text().split("```python\n", 1)[1].split("```", 1)[0]
        namespace = {}
        exec(compile(source, str(document), "exec"), namespace)
        for name, configuration, route in (("receive_health", health.config(), 4),
                                            ("receive_image", image_control.config(), 1)):
            before = len(self.events())
            callback, future, counters = namespace[name](
                health.provider(), "common-meta-parity", configuration, route,
                0xFFFFFFFF, 0x80000000)
            self.assertEqual(callback, m.CommsTestReport(0xFFFFFFFF, 0x80000000))
            self.assertEqual(future, m.CommsCompleted(m.CommsTestReport(0xFFFFFFFF, 0x80000000)))
            self.assertEqual(counters, m.CommsMetadataCounters(1, 0, 0))
            if route == 1:
                image_control.assert_no_storage(self, self.events()[before:])


class HostileOutputs(health._HealthTestCase):
    receiver = CommsMetadataTests.receiver

    def test_open_success_null_owner(self):
        _, channel = self.open("common-meta-parity")
        with channel.channel_view() as view:
            def opened(*args):
                args[3][0] = n.AMS_MEL_IR_RETURN_SUCCESS
                return diagnostic(args)
            with mock.patch.object(n, P + "open", side_effect=opened) as call, \
                    mock.patch.object(n, P + "close") as close:
                self.error(PROTOCOL, view.open_comms_metadata)
                call.assert_called_once()
                close.assert_not_called()

    def test_open_hostile_outputs_release_real_published_owner_once(self):
        native_open = getattr(n, P + "open")
        native_close = getattr(n, P + "close")
        cases = ("registration", "failure", "length-zero", "length-large", "utf8",
                 "unknown", "exception", "construction")
        for case in cases:
            with self.subTest(case=case):
                _, channel = self.open("common-meta-parity")
                with channel.channel_view() as view:
                    def opened(*args):
                        self.assertEqual(native_open(*args), n.AMS_MEL_OK)
                        if case == "registration":
                            args[3][0] = n.AMS_MEL_IR_RETURN_FAIL
                        elif case == "failure":
                            return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"original failure")
                        elif case in ("length-zero", "length-large"):
                            c.cast(args[-1], c.POINTER(c.c_size_t))[0] = 0 if case == "length-zero" else 4097
                        elif case == "utf8":
                            return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"\xff")
                        elif case == "unknown":
                            return diagnostic(args, 987654, b"unknown failure")
                        elif case == "exception":
                            raise KeyboardInterrupt("original Open")
                        return n.AMS_MEL_OK

                    def cleanup(*args):
                        native_close(*args)
                        raise SystemExit("secondary cleanup")

                    with mock.patch.object(n, P + "open", side_effect=opened) as call, \
                            mock.patch.object(n, P + "close", side_effect=cleanup) as close, \
                            mock.patch.object(m.CommsMetadataStream, "_from_owner",
                                              side_effect=RuntimeError("original construction")):
                        if case in ("exception", "construction"):
                            cls = KeyboardInterrupt if case == "exception" else RuntimeError
                            with self.assertRaisesRegex(cls, "original"):
                                view.open_comms_metadata()
                        else:
                            kind = {"failure": m.ErrorKind.PROVIDER_FAILED,
                                    "utf8": m.ErrorKind.INVALID_UTF8,
                                    "unknown": m.ErrorKind.UNKNOWN}.get(case, PROTOCOL)
                            failure = self.error(kind, view.open_comms_metadata)
                            if case == "unknown":
                                self.assertEqual(failure.native_status, 987654)
                        call.assert_called_once()
                        close.assert_called_once()

    def test_close_at_most_once_for_every_uncertain_outcome(self):
        native_close = getattr(n, P + "close")
        for case in ("retained-failure", "consumed-failure", "retained-success", "exception"):
            with self.subTest(case=case):
                _, _, _, stream = self.receiver()
                def close(*args):
                    if case == "consumed-failure":
                        native_close(*args)
                    if case == "exception":
                        raise KeyboardInterrupt("uncertain close")
                    return diagnostic(args, n.AMS_MEL_OK if case == "retained-success"
                                      else n.AMS_MEL_PROVIDER_FAILED)
                try:
                    with mock.patch.object(n, P + "close", side_effect=close) as call:
                        if case == "exception":
                            with self.assertRaisesRegex(KeyboardInterrupt, "uncertain"):
                                stream.close()
                        else:
                            self.error(PROTOCOL if case == "retained-success" else
                                       m.ErrorKind.PROVIDER_FAILED, stream.close)
                        if stream.is_open:
                            self.error(PROTOCOL, stream.close)
                            self.error(m.ErrorKind.INVALID_ARGUMENT, stream.receive)
                        else:
                            stream.close()
                        stream.__del__()
                        self.assertFalse(stream.__exit__(ValueError, ValueError("application"), None))
                        call.assert_called_once()
                finally:
                    # Test controller releases the real mock allocation. The
                    # public owner deliberately never retries uncertain Close.
                    if stream.is_open:
                        native_close(stream._owner_pointer, None, 0, None)

    def test_pre_invocation_close_failure_is_retryable(self):
        _, _, _, stream = self.receiver()
        with mock.patch.object(_errors._ctypes, "create_string_buffer",
                               side_effect=MemoryError("before invocation")), \
                mock.patch.object(n, P + "close") as call:
            with self.assertRaises(MemoryError):
                stream.close()
            call.assert_not_called()
        self.assertFalse(stream._close_attempted)
        stream.close()
        self.assertFalse(stream.is_open)

    def test_application_exception_precedes_close_exception(self):
        _, _, _, stream = self.receiver()
        native_close = getattr(n, P + "close")
        def close(*args):
            native_close(*args)
            raise KeyboardInterrupt("secondary")
        with mock.patch.object(n, P + "close", side_effect=close) as call:
            with self.assertRaisesRegex(ValueError, "application"):
                with stream:
                    raise ValueError("application")
            stream.__del__()
            call.assert_called_once()

    def test_receive_and_counter_failures_do_not_publish_temporary_values(self):
        _, _, _, stream = self.receiver()
        for suffix, record, operation in (
                ("receive", n.IrChannelCommsTestReportV1, stream.receive),
                ("get_counters", n.IrC2MetadataCountersV1, stream.counters)):
            def fail(*args):
                output = args[2] if suffix == "receive" else args[1]
                c.memset(output, 0xFF, c.sizeof(record))
                return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"not a result")
            with mock.patch.object(n, P + suffix, side_effect=fail):
                self.error(m.ErrorKind.PROVIDER_FAILED, operation)

    def test_finalizer_nonraising_and_successful_close_idempotent(self):
        _, _, _, stream = self.receiver()
        native_close = getattr(n, P + "close")
        with mock.patch.object(n, P + "close", wraps=native_close) as call:
            stream.close()
            stream.close()
            stream.__del__()
            call.assert_called_once()

    def test_finalizer_exception_is_nonraising_and_not_retried(self):
        _, _, _, stream = self.receiver()
        native_close = getattr(n, P + "close")
        with mock.patch.object(n, P + "close", side_effect=SystemExit("uncertain")) as call:
            stream.__del__()
            stream.__del__()
            call.assert_called_once()
        native_close(stream._owner_pointer, None, 0, None)


def child(case, family):
    """Process isolation for provider global state, DSO pins and teardown races."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py015-") as directory:
        log = Path(directory) / "lifetime"
        barrier = Path(directory) / "callback"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        os.environ["AMS_MEL_TEST_COMMS_BARRIER"] = str(barrier)
        scenario = {"weak": "sync", "blocked": "parity", "owner-first": "async",
                    "typed-callback": "async-typed"}.get(case, case)
        session = m.Session.open(health.provider(), "common-meta-" + scenario, "")
        channel = attach(session, family)
        view = channel.channel_view()
        events = lambda: log.read_text().splitlines()
        if case in ("refuse", "fail", "partial", "throw"):
            error(test, m.ErrorKind.PROVIDER_EXCEPTION if case == "throw" else
                  m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
            error(test, m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
            test.assertEqual(events().count("common_comms_registration"), 1)
            channel.close()
            session.close()
            if case in ("partial", "throw"):
                test.assertIn("common_comms_partial_callback_at_destruction", events())
            test.assertIn("library_unloaded", events())
            return
        stream = view.open_comms_metadata(2)
        if case == "weak":
            references = [weakref.ref(value) for value in (view, channel, session)]
            view.close()
            del view, channel, session
            gc.collect()
            test.assertTrue(all(reference() is None for reference in references))
            test.assertIn("library_unloaded", events())
            test.assertEqual(stream.receive(), SYNC)
            error(test, m.ErrorKind.STREAM_STOPPED, stream.receive)
        elif case == "held":
            pin = c.CDLL(str(health.provider()))
            release = pin.mock_completion_release_family
            release.argtypes = [c.c_uint, c.c_uint]
            release.restype = c.c_int
            future = view.submit_comms_test(m.CommsTestRequest(0xFFFFFFFF, 4, 0x80000000))
            test.assertEqual(stream.receive(), m.CommsTestReport(0xFFFFFFFF, 0x80000000))
            error(test, m.ErrorKind.TIMEOUT, lambda: future.wait(0))
            channel.close()
            session.close()
            test.assertNotIn("health_channel_destroyed", events())
            test.assertEqual(release(2, 1), 1)
            test.assertEqual(future.wait(1000), m.CommsCompleted(m.CommsTestReport(0xFFFFFFFF, 0x80000000)))
            future.close()
            health.quiesce()
            test.assertIn("health_channel_destroyed", events())
            error(test, m.ErrorKind.STREAM_STOPPED, stream.receive)
            import _ctypes
            _ctypes.dlclose(pin._handle)
            test.assertIn("library_unloaded", events())
        elif case == "saturation":
            probe = n._LIBRARY.ams_mel_test_comms_metadata_probe
            probe.argtypes = [n.IrChannelCommsMetadataHandle, c.c_uint, c.c_void_p]
            probe.restype = c.c_int
            test.assertEqual(probe(stream._owner, 0, None), 1)
            for i in range(4):
                with closing(view.submit_comms_test(m.CommsTestRequest(0xFFFFFFFF - i, 4, 0x80000000 + i))) as future:
                    future.wait(1000)
            expected = m.CommsMetadataCounters(MAX64, MAX64, MAX64)
            test.assertEqual(stream.counters(), expected)
            for i in range(2):
                test.assertEqual(stream.receive(), m.CommsTestReport(0xFFFFFFFF - i, 0x80000000 + i))
            test.assertEqual(stream.counters(), expected)
        elif case in ("detach-fail", "disable-fail"):
            if case == "disable-fail":
                channel.enable() if family == "health" else channel.start()
            error(test, m.ErrorKind.PROVIDER_FAILED, channel.close)
            error(test, m.ErrorKind.PROVIDER_FAILED, stream.receive)
            test.assertNotIn("library_unloaded", events())
            if family != "health" and channel.is_open:
                error(test, m.ErrorKind.PROVIDER_FAILED, channel.close)
            else:
                channel.close()
        elif case == "blocked":
            entered = threading.Event()
            finished = threading.Event()
            failures = []
            receive = getattr(n, P + "receive")
            def observed(*args):
                entered.set()
                return receive(*args)
            def wait():
                try:
                    error(test, m.ErrorKind.STREAM_STOPPED, lambda: stream.receive(10000))
                except BaseException as failure:
                    failures.append(failure)
                finally:
                    finished.set()
            with mock.patch.object(n, P + "receive", side_effect=observed):
                worker = threading.Thread(target=wait)
                worker.start()
                test.assertTrue(entered.wait(5))
                channel.close()
                test.assertTrue(finished.wait(5))
                worker.join()
            test.assertEqual(failures, [])
        elif case in ("owner-first", "typed-callback"):
            # A provider thread enters a test-only callback hook and remains
            # there while the public consumption owner is closed.
            entered, release = threading.Event(), threading.Event()
            hook_type = c.CFUNCTYPE(None)
            def hold():
                entered.set()
                release.wait(10)
            hook = hook_type(hold)
            probe = n._LIBRARY.ams_mel_test_comms_metadata_probe
            probe.argtypes = [n.IrChannelCommsMetadataHandle, c.c_uint, hook_type]
            probe.restype = c.c_int
            test.assertEqual(probe(stream._owner, 1, hook), 1)
            Path(str(barrier) + ".start").touch()
            test.assertTrue(entered.wait(5))
            if case == "owner-first":
                stream.close()
                test.assertNotIn("library_unloaded", events())
                release.set()
                channel.close()
            else:
                failures = []
                def close_typed():
                    try:
                        channel.close()
                    except BaseException as failure:
                        failures.append(failure)
                closer = threading.Thread(target=close_typed)
                closer.start()
                try:
                    # Typed closure deactivates the queue before waiting for
                    # the provider callback held in the native callback hook.
                    error(test, m.ErrorKind.STREAM_STOPPED, lambda: stream.receive(5000))
                    test.assertNotIn("library_unloaded", events())
                finally:
                    release.set()
                    closer.join(5)
                test.assertFalse(closer.is_alive())
                test.assertEqual(failures, [])
            test.assertIn("common_comms_callback_returned", events())
        stream.close()
        if case != "weak":
            view.close()
            channel.close()
            session.close()
            health.quiesce()
            test.assertEqual(events().count("library_unloaded"), 1)
            if family == "control":
                image_control.assert_no_storage(test, events())


class IsolatedLifetimes(unittest.TestCase):
    def test_isolated_callback_lifetimes(self):
        for family in ("health", "image", "control"):
            for case in ("weak", "blocked", "owner-first", "typed-callback", "refuse", "fail", "partial", "throw",
                         "detach-fail", "disable-fail", "saturation"):
                with self.subTest(family=family, case=case):
                    if family == "control" and case == "disable-fail":
                        continue  # A control-only attachment cannot be started.
                    result = subprocess.run(
                        [sys.executable, "-W", "error", __file__, case, family],
                        capture_output=True, text=True, timeout=35)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_pending_future_retains_its_own_graph(self):
        result = subprocess.run([sys.executable, "-W", "error", __file__, "held", "health"],
                                capture_output=True, text=True, timeout=35)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    child(sys.argv[1], sys.argv[2])
