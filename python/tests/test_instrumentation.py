"""PY-MEL-002: compiled-provider commands/lifetimes and malformed ABI boundaries."""

from __future__ import annotations

import ctypes
from dataclasses import FrozenInstanceError, replace
import gc
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import weakref

import ams_mel as mel
from ams_mel import _native
import test_common_channel as common


P = mel.InstrumentationPriority
COMMAND = mel.InstrumentationLevelCommand(0xE1234567, P.DEBUG)
REPORT = mel.InstrumentationReport(0xF1234567, 0x89ABCDEF, -8765432109, P.DEBUG)
COMPLETED = mel.InstrumentationCompleted(REPORT)
LONG_TEXT = "x" * 510 + "€" + "y" * 100


def config() -> mel.InstrumentationConfig:
    return mel.InstrumentationConfig.new(
        mel.UciId(bytes(range(16)), "IR instrumentation channel"),
        mel.UciId(bytes(range(0xF0, 0x100)), "test platform"),
        mel.ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
    )


def provider() -> Path:
    return Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"


def completion_observers():
    boundary = _native._LIBRARY.ams_mel_test_completion_boundary
    boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64)]
    boundary.restype = ctypes.c_int
    owner = _native._LIBRARY.ams_mel_test_completion_owner
    owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
    owner.restype = ctypes.c_int
    return boundary, owner


def drain_completion_owners(family: int = 4) -> None:
    """Acquire the native retirement observation, not a guessed worker delay."""
    boundary, owner = completion_observers()
    counts = (ctypes.c_uint64 * 4)()
    observed = ctypes.c_uint64()
    if boundary(family, 3, 0, counts) != 1 or owner(family, counts[2], ctypes.byref(observed)) != 1:
        raise AssertionError("Instrumentation completion owners did not retire")


def wait_for_marker(path: Path) -> None:
    """Bounded event polling of the mock controller's explicit entered marker."""
    deadline = time.monotonic() + 5
    while not path.exists():
        if time.monotonic() >= deadline:
            raise AssertionError(f"mock did not reach {path}")
        time.sleep(0.001)


def held_lifecycle(mode: str) -> None:
    """Subprocess controller: the provider cannot complete until explicit release."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py-instr-held-") as directory:
        base = Path(directory)
        log = base / "log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        os.environ["AMS_MEL_TEST_INSTRUMENTATION_HOLD"] = directory
        session = mel.Session.open(provider(), "instr-lifetime-held", "")
        channel = session.open_instrumentation_channel(config())
        channel.enable()
        request = channel.submit_level(COMMAND)
        wait_for_marker(base / "entered")
        with test.assertRaises(mel.MelError) as caught:
            request.wait(0)
        test.assertEqual(caught.exception.kind, mel.ErrorKind.TIMEOUT)
        test.assertEqual(caught.exception.native_status, _native.AMS_MEL_TIMEOUT)
        test.assertTrue(request.is_open)
        session_ref, channel_ref = weakref.ref(session), weakref.ref(channel)
        session.close()
        channel.close()
        del session, channel
        gc.collect()
        test.assertIsNone(session_ref())
        test.assertIsNone(channel_ref())
        if mode == "close":
            request.close()
            request.close()
        elif mode == "finalize":
            request_ref = weakref.ref(request)
            del request
            gc.collect()
            test.assertIsNone(request_ref())
        events = log.read_text()
        test.assertNotIn("instrumentation_completed", events)
        test.assertNotIn("instrumentation_channel_destroyed", events)
        test.assertNotIn("library_unloaded", events)
        (base / "closed").touch()
        (base / "release").touch()
        if mode == "wait":
            test.assertEqual(request.wait(5000), COMPLETED)
            test.assertEqual(request.wait(0), COMPLETED)
            request.close()
        drain_completion_owners()
        events = log.read_text().splitlines()
        test.assertLess(events.index("instrumentation_completed"),
                        events.index("instrumentation_channel_destroyed"))
        test.assertLess(events.index("instrumentation_channel_destroyed"),
                        events.index("library_unloaded"))
        test.assertEqual(events.count("instrumentation_level_sent"), 1)


class _InstrumentationTestCase(unittest.TestCase):
    def setUp(self) -> None:
        drain_completion_owners()
        self.temp = tempfile.TemporaryDirectory(prefix="py-instr-")
        self.addCleanup(self.temp.cleanup)
        self.log = Path(self.temp.name) / "log"
        environment = mock.patch.dict(os.environ, {"AMS_MEL_TEST_LIFETIME_LOG": str(self.log)})
        environment.start()
        self.addCleanup(environment.stop)
        # Run before removing the environment or directory.
        self.addCleanup(drain_completion_owners)

    def events(self) -> list[str]:
        return self.log.read_text().splitlines() if self.log.exists() else []

    def open(self, scenario: str = "instr-config"):
        session = mel.Session.open(provider(), scenario, "")
        self.addCleanup(session.close)
        channel = session.open_instrumentation_channel(config())
        self.addCleanup(channel.close)
        return session, channel

    def request(self, scenario: str = "instr-rich"):
        session, channel = self.open(scenario)
        channel.enable()
        request = channel.submit_level(COMMAND)
        self.addCleanup(request.close)
        return session, channel, request

    def assert_error(self, kind, call):
        with self.assertRaises(mel.MelError) as caught:
            call()
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception


class InstrumentationTests(_InstrumentationTestCase):
    def test_public_exports_immutable_values_and_priorities(self):
        for name in ("Config", "Priority", "LevelCommand", "Report", "Completed",
                     "Rejected", "Channel", "Request"):
            self.assertIn("Instrumentation" + name, mel.__all__)
        self.assertEqual([(p.name, p.value) for p in P], [("NORMAL", 0), ("DEBUG", 1)])
        self.assertEqual(P.NORMAL, _native.AMS_MEL_IR_PRIORITY_NORMAL)
        self.assertEqual(P.DEBUG, _native.AMS_MEL_IR_PRIORITY_DEBUG)
        for value in (config(), COMMAND, REPORT, COMPLETED,
                      mel.InstrumentationRejected(mel.MelErrorCode.INVALID_STATE, "no")):
            common.assert_owned_graph(self, value)
            with self.assertRaises(FrozenInstanceError):
                value.extra = 1
        self.assertIs(mel.InstrumentationLevelCommand(1, 0).priority, P.NORMAL)
        with self.assertRaisesRegex(TypeError, "Session.open_instrumentation_channel"):
            mel.InstrumentationChannel()
        with self.assertRaisesRegex(TypeError, "InstrumentationChannel.submit_level"):
            mel.InstrumentationRequest()

    def test_input_validation_never_calls_native(self):
        bad = (-1, 1 << 32, True, 1.0, "1", None, ctypes.c_uint32(1))
        with mock.patch.object(_native, "ams_mel_ir_instrumentation_open") as native_open:
            for field in ("channel_id", "platform_id", "sensor_location"):
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                                  lambda f=field: replace(config(), **{f: None}))
            for value in bad:
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                                  lambda v=value: mel.InstrumentationLevelCommand(v, P.NORMAL))
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                                  lambda v=value: mel.InstrumentationLevelCommand(1, v))
            for value in (2, 99):
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                                  lambda v=value: mel.InstrumentationLevelCommand(1, v))
            for text in ("bad\0text", "\ud800", None, b"bytes"):
                for field in ("key", "system_name"):
                    self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda f=field, t=text:
                                      replace(config(), sensor_location=replace(
                                          config().sensor_location, **{f: t})))
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                                  lambda t=text: mel.UciId(bytes(16), t))
            for value in (True, "1", None, 10 ** 400):
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda v=value:
                                  replace(config(), sensor_location=replace(
                                      config().sensor_location, offset_x_m=v)))
            native_open.assert_not_called()
        for field, value in (("size", -1), ("timestamp_ns", True),
                             ("timestamp_ns", 1 << 63), ("timestamp_ns", -(1 << 63) - 1)):
            self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                              lambda f=field, v=value: replace(REPORT, **{f: v}))
        for code in (-1, 9, 99, True, 1.0):
            self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                              lambda c=code: mel.InstrumentationRejected(c, "no"))
        for code in range(9):
            self.assertIs(mel.InstrumentationRejected(code, "no").code, mel.MelErrorCode(code))
        self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda: mel.InstrumentationCompleted(None))

    def test_config_attach_without_enable_and_failed_pre_enable_submission(self):
        session, channel = self.open("instr-config")  # Provider checks every input field.
        self.assertTrue(channel.is_open)
        self.assertNotIn("instrumentation_enabled", self.events())
        with channel as entered:
            self.assertIs(entered, channel)
            self.assert_error(mel.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_level(COMMAND))
            self.assertNotIn("instrumentation_level_sent", self.events())
        self.assertFalse(channel.is_open)
        channel.close()
        session.close()

    def test_enable_is_explicit_and_idempotent(self):
        _, channel = self.open()
        channel.enable()
        channel.enable()
        self.assertEqual(self.events().count("instrumentation_enabled"), 1)

    def test_complete_capability_before_after_enable_and_after_unload(self):
        session, channel = self.open()
        before = channel.capabilities()
        common.assert_owned_graph(self, before)
        self.assertEqual(before.channel_types, (mel.ChannelType.INSTRUMENTATION,))
        self.assertEqual(before.metadata_capabilities, (
            mel.MetadataCapability.CHANNEL_COMMS_TEST_REP,
            mel.MetadataCapability.INSTRUMENTATION_REPORT,
        ))
        # Same rich upstream fixture: assert every remaining field with the
        # established full-field common Channel oracle, not only dimensions.
        normalized = replace(before,
            channel_types=(mel.ChannelType.COMMAND_AND_CONTROL,
                           mel.ChannelType.INSTRUMENTATION, mel.ChannelType.RESERVED_2),
            metadata_capabilities=(mel.MetadataCapability.BAD_PIXEL_LIST,
                mel.MetadataCapability.COMMAND_STATUS, mel.MetadataCapability.CHANNEL_COMMS_TEST_REP,
                mel.MetadataCapability.RESERVED_10))
        common.CommonChannelTests.assert_rich_c2(self, normalized)
        channel.enable()
        self.assertEqual(channel.capabilities(), before)
        channel.close()
        session.close()
        self.assertIn("library_unloaded", self.events())
        self.assertEqual(replace(before), before)
        common.assert_owned_graph(self, before)

    def test_weak_common_view_does_not_close_or_retain_typed_owner(self):
        # No cleanup bound methods here: they would themselves retain parents.
        session = mel.Session.open(provider(), "instr-config", "")
        channel = session.open_instrumentation_channel(config())
        view = channel.channel_view()
        self.addCleanup(view.close)
        self.assertEqual(view.capabilities(), channel.capabilities())
        view.close()
        self.assertTrue(channel.is_open)
        channel.enable()
        view = channel.channel_view()
        self.addCleanup(view.close)
        channel_ref, session_ref = weakref.ref(channel), weakref.ref(session)
        session.close()
        del session, channel
        gc.collect()
        self.assertIsNone(session_ref())
        self.assertIsNone(channel_ref())
        self.assertIn("library_unloaded", self.events())
        self.assertTrue(view.is_open)
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, view.capabilities)

    def test_common_services_on_instrumentation(self):
        _, channel = self.open("instr-ada-common")
        with channel.channel_view() as view:
            request = view.send_keepalive()
            self.assertEqual(request.wait(5000), mel.ReturnCompleted(mel.CommandReturn.SUCCESS))
            request.close()
            drain_completion_owners(1)
            request = view.submit_comms_test(mel.CommsTestRequest(0x80000001, 0xF0000002, 0xE0000003))
            self.assertEqual(request.wait(5000), mel.CommsCompleted(
                mel.CommsTestReport(0x80000001, 0xE0000003)))
            request.close()
            drain_completion_owners(2)
        self.assertTrue(channel.is_open)

    def test_debug_rich_report_and_native_terminal_cache(self):
        _, _, request = self.request()
        self.assertEqual(request.wait(5000), COMPLETED)
        self.assertEqual(request.wait(0), COMPLETED)
        self.assertEqual(self.events().count("instrumentation_level_sent"), 1)
        common.assert_owned_graph(self, request.wait(0))
        request.close()
        request.close()
        self.assertFalse(request.is_open)
        self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda: request.wait(0))

    def test_normal_priority_and_full_uint32_command_id(self):
        for scenario, command in (("instr-normal-priority", mel.InstrumentationLevelCommand(7, P.NORMAL)),
                                  ("instr-rich", COMMAND),
                                  ("instr-config", mel.InstrumentationLevelCommand(0, P.NORMAL)),
                                  ("instr-config", mel.InstrumentationLevelCommand(0xFFFFFFFF, P.DEBUG))):
            with self.subTest(command=command):
                _, channel = self.open(scenario)
                channel.enable()
                native = _native.ams_mel_ir_instrumentation_submit_level

                def inspect(owner, raw, *args):
                    record = ctypes.cast(raw, ctypes.POINTER(_native.IrInstrumentationLevelCommandV1)).contents
                    self.assertEqual(record.command_id, command.command_id)
                    self.assertEqual(record.priority, int(command.priority))
                    return native(owner, raw, *args)

                with mock.patch.object(_native, "ams_mel_ir_instrumentation_submit_level", side_effect=inspect):
                    with channel.submit_level(command) as request:
                        self.assertEqual(request.wait(0xFFFFFFFF), COMPLETED)
                channel.close()

    def test_wait_and_submit_argument_validation(self):
        _, channel, request = self.request()
        with mock.patch.object(_native, "ams_mel_ir_instrumentation_request_wait") as wait:
            for value in (-1, 1 << 32, True, 1.0, "1", None):
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda v=value: request.wait(v))
            wait.assert_not_called()
        with mock.patch.object(_native, "ams_mel_ir_instrumentation_submit_level") as submit:
            for value in (None, 1, COMMAND.command_id):
                self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, lambda v=value: channel.submit_level(v))
            submit.assert_not_called()
        self.assertEqual(request.wait(5000), COMPLETED)

    def test_complete_typed_rejection_and_checked_cached_diagnostic_retry(self):
        _, _, request = self.request("instr-reject")
        native = _native.ams_mel_ir_instrumentation_request_wait
        calls = []

        def observe(owner, timeout, result, diagnostic, capacity, required):
            calls.append((timeout, capacity))
            return native(owner, timeout, result, diagnostic, capacity, required)

        with mock.patch.object(_native, "ams_mel_ir_instrumentation_request_wait", side_effect=observe):
            result = request.wait(5000)
        self.assertEqual(calls, [(5000, 512), (0, len(LONG_TEXT.encode()) + 1)])
        self.assertEqual(result, mel.InstrumentationRejected(mel.MelErrorCode.INVALID_STATE, LONG_TEXT))
        self.assertIs(result.code, mel.MelErrorCode.INVALID_STATE)
        self.assertEqual(request.wait(0), result)
        self.assertEqual(self.events().count("instrumentation_level_sent"), 1)

    def test_provider_failures_are_not_rejections(self):
        for scenario, kind in (("instr-unknown-error", mel.ErrorKind.PROVIDER_FAILED),
                               ("instr-invalid-priority", mel.ErrorKind.PROVIDER_FAILED),
                               ("instr-null", mel.ErrorKind.PROVIDER_FAILED),
                               ("instr-future-throw", mel.ErrorKind.PROVIDER_EXCEPTION)):
            with self.subTest(scenario=scenario):
                _, _, request = self.request(scenario)
                error = self.assert_error(kind, lambda: request.wait(5000))
                self.assertEqual(self.assert_error(kind, lambda: request.wait(0)).diagnostic,
                                 error.diagnostic)
                request.close()
        _, channel = self.open("instr-send-throw")
        channel.enable()
        self.assert_error(mel.ErrorKind.PROVIDER_EXCEPTION, lambda: channel.submit_level(COMMAND))

    def test_rejection_that_fits_initial_buffer_needs_no_retry(self):
        _, _, request = self.request("instr-reject")
        native = _native.ams_mel_ir_instrumentation_request_wait
        with mock.patch.object(mel, "_WAIT_DIAGNOSTIC_CAPACITY", 1024), mock.patch.object(
                _native, "ams_mel_ir_instrumentation_request_wait", wraps=native) as wait:
            self.assertEqual(request.wait(5000), mel.InstrumentationRejected(
                mel.MelErrorCode.INVALID_STATE, LONG_TEXT))
            self.assertEqual(wait.call_count, 1)

    def test_attach_type_capability_failures(self):
        for scenario, kind in (("instr-attach-null", mel.ErrorKind.FACTORY_FAILED),
                               ("instr-wrong-type", mel.ErrorKind.INITIALIZATION_FAILED),
                               ("instr-capability-wrong", mel.ErrorKind.INITIALIZATION_FAILED)):
            with self.subTest(scenario=scenario), mel.Session.open(provider(), scenario, "") as session:
                self.assert_error(kind, lambda: session.open_instrumentation_channel(config()))
                self.assertTrue(session.is_open)

    def test_enable_failure_blocks_retry_capabilities_and_submission(self):
        for scenario, kind in (("instr-enable-fail", mel.ErrorKind.PROVIDER_FAILED),
                               ("instr-enable-throw", mel.ErrorKind.PROVIDER_EXCEPTION)):
            with self.subTest(scenario=scenario):
                _, channel = self.open(scenario)
                self.assert_error(kind, channel.enable)
                self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.enable)
                self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.capabilities)
                self.assert_error(mel.ErrorKind.PROVIDER_FAILED, lambda: channel.submit_level(COMMAND))
                channel.close()

    def test_closed_session_does_not_invalidate_independent_channel(self):
        session, channel = self.open()
        session.close()
        self.assertTrue(channel.is_open)
        channel.capabilities()
        channel.enable()
        with channel.submit_level(COMMAND) as request:
            self.assertEqual(request.wait(5000), COMPLETED)
        channel.close()
        self.assert_error(mel.ErrorKind.INVALID_ARGUMENT,
                          lambda: session.open_instrumentation_channel(config()))
        for call in (channel.enable, channel.capabilities, channel.channel_view,
                     lambda: channel.submit_level(COMMAND)):
            self.assert_error(mel.ErrorKind.INVALID_ARGUMENT, call)

    def test_retryable_detach_close_and_finalizers(self):
        _, channel = self.open("instr-detach-fail")
        channel.enable()
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.close)
        self.assertTrue(channel.is_open)
        self.assertNotIn("instrumentation_channel_destroyed", self.events())
        channel.close()
        closed_events = self.events()
        channel.close()
        self.assertFalse(channel.is_open)
        self.assertEqual(self.events(), closed_events)
        self.assertEqual(self.events().count("channel_detach_failed"), 1)
        self.assertEqual(self.events().count("channel_detached"), 1)
        self.assertEqual(self.events().count("instrumentation_channel_destroyed"), 1)
        channel.__del__()
        mel.InstrumentationRequest.__new__(mel.InstrumentationRequest).__del__()

    def subprocess(self, code: str):
        result = subprocess.run([sys.executable, "-W", "error", "-c", code],
            env={**os.environ, "PYTHONPATH": os.pathsep.join((str(Path(mel.__file__).parent.parent),
                str(Path(__file__).parent)))}, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_timeout_then_completion_after_both_parents_close(self):
        self.subprocess('import test_instrumentation as t; t.held_lifecycle("wait")')

    def test_request_close_does_not_cancel_pending_work(self):
        self.subprocess('import test_instrumentation as t; t.held_lifecycle("close")')

    def test_pending_request_finalizer_does_not_cancel_work(self):
        self.subprocess('import test_instrumentation as t; t.held_lifecycle("finalize")')

    def test_uncertain_finalizer_retains_native_graph_in_subprocess(self):
        self.subprocess('''
import gc, os, tempfile, weakref
from pathlib import Path
import test_instrumentation as t
with tempfile.TemporaryDirectory() as d:
    log = Path(d) / "log"
    os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
    session = t.mel.Session.open(t.provider(), "instr-detach-fail", "")
    channel = session.open_instrumentation_channel(t.config())
    channel.enable()
    session.close()
    ref = weakref.ref(channel)
    del channel
    gc.collect()
    assert ref() is None
    events = log.read_text()
    assert "channel_detach_failed" in events
    assert "instrumentation_channel_destroyed" not in events
    assert "library_unloaded" not in events
''')


class NativeBoundaryTests(_InstrumentationTestCase):
    """Injection is limited to malformed ABI responses the mock cannot publish."""

    def test_malformed_wait_outputs(self):
        _, _, request = self.request()
        self.assertEqual(request.wait(5000), COMPLETED)
        native = _native.ams_mel_ir_instrumentation_request_wait
        for mutation, kind in (("priority", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("success-error", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("unknown-code", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("zero-size", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("unterminated", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("embedded-nul", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("utf8", mel.ErrorKind.INVALID_UTF8),
                               ("unknown-status", mel.ErrorKind.UNKNOWN),
                               ("success-diagnostic", mel.ErrorKind.PROTOCOL_INCONSISTENCY)):
            def malformed(owner, timeout, raw, diagnostic, capacity, required):
                status = native(owner, timeout, raw, diagnostic, capacity, required)
                record = ctypes.cast(raw, ctypes.POINTER(_native.IrInstrumentationResultV1)).contents
                size = ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))
                if mutation == "priority": record.report.priority = 2
                elif mutation == "success-error": record.error_code = 1
                elif mutation == "unknown-code":
                    record.error_code = 99
                    status = _native.AMS_MEL_COMMAND_REJECTED
                elif mutation == "zero-size": size[0] = 0
                elif mutation == "unterminated": diagnostic[0] = b"x"
                elif mutation in ("embedded-nul", "utf8", "success-diagnostic"):
                    text = {"embedded-nul": b"\0x\0", "utf8": b"\xff\0",
                            "success-diagnostic": b"bad\0"}[mutation]
                    ctypes.memmove(diagnostic, text, len(text))
                    size[0] = len(text)
                elif mutation == "unknown-status": status = 123456
                return status

            with self.subTest(mutation=mutation), mock.patch.object(
                    _native, "ams_mel_ir_instrumentation_request_wait", side_effect=malformed):
                self.assert_error(kind, lambda: request.wait(0))
                self.assertTrue(request.is_open)
        self.assertEqual(request.wait(0), COMPLETED)

    def test_rejection_cache_identity_and_diagnostic_failures(self):
        _, _, request = self.request("instr-reject")
        native = _native.ams_mel_ir_instrumentation_request_wait
        for mutation in ("status", "code", "size", "command_id", "report_size", "timestamp", "priority",
                         "unterminated", "embedded-nul", "utf8"):
            calls = []

            def malformed(owner, timeout, raw, diagnostic, capacity, required):
                calls.append(timeout)
                status = native(owner, timeout, raw, diagnostic, capacity, required)
                if len(calls) == 2:
                    record = ctypes.cast(raw, ctypes.POINTER(_native.IrInstrumentationResultV1)).contents
                    size = ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))
                    if mutation == "status": status = _native.AMS_MEL_OK
                    elif mutation == "code": record.error_code = 8
                    elif mutation == "size": size[0] -= 1
                    elif mutation == "command_id": record.report.command_id += 1
                    elif mutation == "report_size": record.report.size += 1
                    elif mutation == "timestamp": record.report.timestamp_ns += 1
                    elif mutation == "priority": record.report.priority += 1
                    elif mutation == "unterminated": diagnostic[capacity - 1] = b"x"
                    elif mutation == "embedded-nul": diagnostic[0] = b"\0"
                    elif mutation == "utf8": diagnostic[0] = b"\xff"
                return status

            with self.subTest(mutation=mutation), mock.patch.object(
                    _native, "ams_mel_ir_instrumentation_request_wait", side_effect=malformed):
                self.assert_error(mel.ErrorKind.INVALID_UTF8 if mutation == "utf8"
                                  else mel.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: request.wait(5000))
            self.assertEqual(calls, [5000, 0])
        allocate = ctypes.create_string_buffer

        def fail_allocation(size):
            if size > 512:
                raise MemoryError("injected")
            return allocate(size)

        with mock.patch.object(ctypes, "create_string_buffer", side_effect=fail_allocation):
            self.assert_error(mel.ErrorKind.INTERNAL_ERROR, lambda: request.wait(0))
        self.assertEqual(request.wait(0).description, LONG_TEXT)

    def test_null_success_owners_and_publication_cleanup(self):
        session, channel = self.open()
        channel.enable()
        for name, call in (("ams_mel_ir_instrumentation_open",
                            lambda: session.open_instrumentation_channel(config())),
                           ("ams_mel_ir_instrumentation_submit_level", lambda: channel.submit_level(COMMAND)),
                           ("ams_mel_ir_instrumentation_get_capabilities", channel.capabilities)):
            def empty(*args):
                ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = 1
                args[-3][0] = b"\0"
                return _native.AMS_MEL_OK

            with self.subTest(name=name), mock.patch.object(_native, name, side_effect=empty):
                self.assert_error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, call)
        native = _native.ams_mel_ir_instrumentation_open

        def published_then_malformed(*args):
            status = native(*args)
            ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = 0
            return status

        before = self.events().count("instrumentation_channel_destroyed")
        with mock.patch.object(_native, "ams_mel_ir_instrumentation_open", side_effect=published_then_malformed):
            self.assert_error(mel.ErrorKind.PROTOCOL_INCONSISTENCY,
                              lambda: session.open_instrumentation_channel(config()))
        self.assertEqual(self.events().count("instrumentation_channel_destroyed"), before + 1)

    def test_consumed_cleanup_failure_and_context_exception_preservation(self):
        _, channel = self.open()
        native = channel._close_function

        def consumed(*args):
            native(*args)
            return _native.AMS_MEL_PROVIDER_FAILED

        channel._close_function = consumed
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, channel.close)
        self.assertFalse(channel.is_open)
        channel.close()
        _, channel = self.open()
        with mock.patch.object(channel, "_close_function", side_effect=RuntimeError("cleanup")):
            with self.assertRaisesRegex(ValueError, "body"):
                with channel:
                    raise ValueError("body")
            channel.__del__()
        channel.close()

    def test_snapshot_boundary_failure_releases_native_owner(self):
        _, channel = self.open()
        native_view = _native.ams_mel_ir_channel_capability_view
        native_close = _native.ams_mel_ir_channel_capability_close
        for mutation in ("null-record", "bad-bool", "bad-span"):
            def malformed(owner, output, *args):
                status = native_view(owner, output, *args)
                pointer = ctypes.cast(output, ctypes.POINTER(
                    ctypes.POINTER(_native.IrChannelCapabilityV1)))
                if mutation == "null-record":
                    pointer[0] = ctypes.POINTER(_native.IrChannelCapabilityV1)()
                elif mutation == "bad-bool":
                    pointer[0].contents.odc_available = 2
                else:
                    pointer[0].contents.sensor_types.data = ctypes.POINTER(ctypes.c_uint32)()
                    pointer[0].contents.sensor_types.size = 1
                return status

            with self.subTest(mutation=mutation), mock.patch.object(
                    _native, "ams_mel_ir_channel_capability_view", side_effect=malformed), \
                    mock.patch.object(_native, "ams_mel_ir_channel_capability_close",
                                      wraps=native_close) as close:
                self.assert_error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, channel.capabilities)
                self.assertEqual(close.call_count, 1)
        self.assertEqual(channel.capabilities().width, 1920)

    def test_request_publication_cleanup_and_close_contract(self):
        _, channel = self.open()
        channel.enable()
        native_submit = _native.ams_mel_ir_instrumentation_submit_level
        native_close = mel.InstrumentationRequest._close_function

        def malformed(*args):
            status = native_submit(*args)
            ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = 0
            return status

        with mock.patch.object(_native, "ams_mel_ir_instrumentation_submit_level", side_effect=malformed), \
                mock.patch.object(mel.InstrumentationRequest, "_close_function",
                                  staticmethod(mock.Mock(wraps=native_close))) as close:
            self.assert_error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: channel.submit_level(COMMAND))
            self.assertEqual(close.__func__.call_count, 1)
        with channel.submit_level(COMMAND) as request:
            self.assertTrue(request.is_open)
            self.assertEqual(request.wait(5000), COMPLETED)
            with mock.patch.object(request, "_close_function", return_value=_native.AMS_MEL_OK):
                self.assert_error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, request.close)
                self.assertTrue(request.is_open)
        self.assertFalse(request.is_open)
        request.close()
        request = channel.submit_level(COMMAND)
        self.assertEqual(request.wait(5000), COMPLETED)

        def consumed(*args):
            native_close(*args)
            return _native.AMS_MEL_PROVIDER_FAILED

        request._close_function = consumed
        self.assert_error(mel.ErrorKind.PROVIDER_FAILED, request.close)
        self.assertFalse(request.is_open)
        request.close()


if __name__ == "__main__":
    unittest.main()
