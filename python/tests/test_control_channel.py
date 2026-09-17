from __future__ import annotations

import gc
import ctypes
import os
from pathlib import Path
import tempfile
import time
import unittest
import weakref
from unittest import mock

from ams_mel import (
    ComponentLocation,
    ControlChannel,
    ControlConfig,
    ErrorKind,
    ImageConfig,
    MelError,
    MelErrorCode,
    MfaMode,
    ModeRejected,
    ModeRequest,
    ModeSuccess,
    Session,
    UciId,
    _native,
)


class ControlChannelTests(unittest.TestCase):
    def setUp(self) -> None:
        self.mock_provider = (
            Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        )
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="ams-mel-python-c2-"
        )
        self.lifetime_log = Path(self.temporary_directory.name) / "lifetime.log"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(self.lifetime_log)

    def tearDown(self) -> None:
        os.environ.pop("AMS_MEL_TEST_LIFETIME_LOG", None)
        self.temporary_directory.cleanup()

    def lifecycle(self) -> str:
        try:
            return self.lifetime_log.read_text(encoding="utf-8")
        except FileNotFoundError:
            return ""

    def wait_for_event(self, event: str) -> str:
        for _ in range(200):
            events = self.lifecycle()
            if event in events:
                return events
            time.sleep(0.001)
        self.fail(f"timed out waiting for {event}:\n{self.lifecycle()}")

    def open(self, scenario: str) -> Session:
        return Session.open(self.mock_provider, scenario, "")

    @staticmethod
    def config() -> ControlConfig:
        return ControlConfig.new(
            UciId(bytes(range(16)), "IR C2 channel"),
            UciId(bytes(range(0xF0, 0x100)), "test platform"),
            ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
        )

    @staticmethod
    def image_config() -> ImageConfig:
        return ImageConfig.new(
            UciId(bytes(16), "IR image channel"),
            UciId(bytes(16), "test platform"),
            ComponentLocation(0.0, 0.0, 0.0, "station-1", "mock-aircraft"),
            buffer_count=2,
            buffer_size=64,
            queue_capacity=2,
        )

    def assert_error(self, kind: ErrorKind, call: object) -> MelError:
        with self.assertRaises(MelError) as caught:
            call()  # type: ignore[operator]
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def test_success_requires_enable_and_repeats_cached_result(self) -> None:
        session = self.open("c2-command-id")
        control = session.open_control_channel(self.config())
        self.assert_error(
            ErrorKind.PROVIDER_FAILED,
            lambda: control.submit_operate(0x89ABCDEF),
        )
        control.enable()
        control.enable()
        request = control.submit_operate(0x89ABCDEF)
        expected = ModeSuccess(MfaMode.TASK_SCHED)
        self.assertEqual(request.wait(1000), expected)
        self.assertEqual(request.wait(0), expected)
        request.close()
        request.close()
        control.close()
        control.close()
        session.close()

    def test_timeout_parent_close_and_independent_python_lifetimes(self) -> None:
        session = self.open("c2-delayed")
        session_reference = weakref.ref(session)
        control = session.open_control_channel(self.config())
        control_reference = weakref.ref(control)
        control.enable()
        request = control.submit_operate(7)
        self.assert_error(ErrorKind.TIMEOUT, lambda: request.wait(0))
        session.close()
        del session
        gc.collect()
        self.assertIsNone(session_reference())
        control.close()
        del control
        gc.collect()
        self.assertIsNone(control_reference())
        self.assertEqual(request.wait(1000), ModeSuccess(MfaMode.TASK_SCHED))
        request.close()

    def test_pending_request_close_does_not_cancel_or_unload_early(self) -> None:
        session = self.open("c2-lifetime")
        control = session.open_control_channel(self.config())
        control.enable()
        request = control.submit_operate(8)
        request.close()
        self.assertFalse(request.is_open)
        control.close()
        session.close()
        events = self.wait_for_event("library_unloaded")
        self.assertLess(events.index("mode_completed"), events.index("c2_channel_destroyed"))
        self.assertLess(events.index("c2_channel_destroyed"), events.index("control_destroyed"))
        self.assertLess(events.index("control_destroyed"), events.index("manager_destroyed"))
        self.assertLess(events.index("manager_destroyed"), events.index("library_unloaded"))

    def test_rejection_empty_invalid_utf8_and_long_diagnostics(self) -> None:
        cases = [
            ("c2-reject", "invalid task schedule"),
            ("c2-reject-empty", ""),
            (
                "c2-reject-invalid-utf8",
                "provider rejection description was invalid UTF-8 or contained NUL",
            ),
            ("c2-reject-long", "x" * 510 + "€" + "y" * 100),
        ]
        for scenario, description in cases:
            with self.subTest(scenario=scenario):
                session = self.open(scenario)
                control = session.open_control_channel(self.config())
                control.enable()
                request = control.submit_operate(1)
                expected = ModeRejected(
                    MelErrorCode.INVALID_PARAMETERS, description
                )
                self.assertEqual(request.wait(1000), expected)
                self.assertEqual(request.wait(0), expected)
                request.close()
                control.close()
                session.close()

    def test_unknown_rejection_code_is_preserved_as_terminal_result(self) -> None:
        unknown_code = 0xFEDCBA98
        description = "future provider rejection"

        def wait(
            owner: object,
            timeout_ms: int,
            result_pointer: object,
            diagnostic: object,
            diagnostic_capacity: int,
            required_pointer: object,
        ) -> int:
            del owner, timeout_ms
            result = ctypes.cast(
                result_pointer, ctypes.POINTER(_native.IrModeResultV1)
            ).contents
            result.mode = _native.AMS_MEL_IR_MFA_MODE_UNUSED
            result.error_code = unknown_code
            encoded = description.encode("utf-8") + b"\0"
            self.assertGreaterEqual(diagnostic_capacity, len(encoded))
            ctypes.memmove(diagnostic, encoded, len(encoded))
            ctypes.cast(
                required_pointer, ctypes.POINTER(ctypes.c_size_t)
            ).contents.value = len(encoded)
            return _native.AMS_MEL_COMMAND_REJECTED

        request = ModeRequest._from_owner(_native.IrModeRequestHandle(1))
        try:
            with mock.patch.object(_native, "ams_mel_ir_mode_request_wait", wait):
                result = request.wait(0)
        finally:
            request._owner.value = None
        self.assertIsInstance(result, ModeRejected)
        self.assertIsInstance(result.code, MelErrorCode)
        self.assertEqual(int(result.code), unknown_code)
        self.assertEqual(result.code.name, "UNKNOWN_0xFEDCBA98")
        self.assertEqual(result.description, description)

    def test_provider_and_future_failures_remain_structured_errors(self) -> None:
        for scenario, expected in (
            ("c2-null-result", ErrorKind.PROVIDER_FAILED),
            ("c2-future-throw", ErrorKind.PROVIDER_EXCEPTION),
        ):
            with self.subTest(scenario=scenario):
                session = self.open(scenario)
                control = session.open_control_channel(self.config())
                control.enable()
                request = control.submit_operate(1)
                self.assert_error(expected, lambda: request.wait(1000))
                request.close()
                control.close()
                session.close()

    def test_enable_and_submit_failures_publish_no_request(self) -> None:
        session = self.open("c2-enable-fail")
        control = session.open_control_channel(self.config())
        self.assert_error(ErrorKind.PROVIDER_FAILED, control.enable)
        self.assert_error(ErrorKind.PROVIDER_FAILED, lambda: control.submit_operate(1))
        control.close()
        session.close()

        session = self.open("c2-send-throw")
        control = session.open_control_channel(self.config())
        control.enable()
        self.assert_error(ErrorKind.PROVIDER_EXCEPTION, lambda: control.submit_operate(1))
        control.close()
        session.close()

    def test_close_cleared_cleanup_error_and_retryable_detach(self) -> None:
        session = self.open("c2-disable-fail")
        control = session.open_control_channel(self.config())
        control.enable()
        self.assert_error(ErrorKind.PROVIDER_FAILED, control.close)
        self.assertFalse(control.is_open)
        control.close()
        session.close()

        session = self.open("c2-detach-fail")
        control = session.open_control_channel(self.config())
        self.assert_error(ErrorKind.PROVIDER_FAILED, control.close)
        self.assertTrue(control.is_open)
        control.close()
        self.assertFalse(control.is_open)
        session.close()

    def test_configuration_and_public_input_validation(self) -> None:
        session = self.open("c2-config")
        control = session.open_control_channel(self.config())
        control.close()
        session.close()
        self.assert_error(
            ErrorKind.INVALID_ARGUMENT,
            lambda: ControlConfig("bad", self.config().platform_id, self.config().sensor_location),
        )
        self.assert_error(
            ErrorKind.INVALID_ARGUMENT,
            lambda: UciId(bytes(16), "bad\0label"),
        )
        self.assert_error(
            ErrorKind.INVALID_ARGUMENT,
            lambda: ComponentLocation(0.0, 0.0, 0.0, "key", "bad\0system"),
        )
        session = self.open("arguments")
        self.assert_error(
            ErrorKind.INVALID_ARGUMENT,
            lambda: session.open_control_channel("bad"),
        )
        session.close()

    def test_timeout_and_command_id_uint32_validation(self) -> None:
        session = self.open("c2-command-id")
        control = session.open_control_channel(self.config())
        control.enable()
        for command_id in (-1, 1 << 32, True, 1.0, "1", None):
            self.assert_error(
                ErrorKind.INVALID_ARGUMENT,
                lambda value=command_id: control.submit_operate(value),
            )
        request = control.submit_operate(0x89ABCDEF)
        for timeout in (-1, 1 << 32, True):
            self.assert_error(
                ErrorKind.INVALID_ARGUMENT,
                lambda value=timeout: request.wait(value),
            )
        self.assertEqual(
            request.wait((1 << 32) - 1), ModeSuccess(MfaMode.TASK_SCHED)
        )
        request.close()
        control.close()
        session.close()

        session = self.open("success")
        control = session.open_control_channel(self.config())
        control.enable()
        zero_request = control.submit_operate(0)
        self.assertEqual(zero_request.wait(1000), ModeSuccess(MfaMode.TASK_SCHED))
        zero_request.close()
        maximum_request = control.submit_operate(0xFFFFFFFF)
        self.assertEqual(
            maximum_request.wait(1000), ModeSuccess(MfaMode.TASK_SCHED)
        )
        maximum_request.close()
        control.close()
        session.close()

    def test_control_context_does_not_implicitly_enable(self) -> None:
        session = self.open("c2-command-id")
        control = session.open_control_channel(self.config())
        with control as entered:
            self.assertIs(entered, control)
            self.assert_error(
                ErrorKind.PROVIDER_FAILED,
                lambda: control.submit_operate(0x89ABCDEF),
            )
        self.assertFalse(control.is_open)
        session.close()

    def test_image_and_c2_coexist_after_parent_close(self) -> None:
        session = self.open("c2-coexist")
        stream = session.open_image_stream(self.image_config())
        control = session.open_control_channel(self.config())
        control.enable()
        request = control.submit_operate(42)
        stream.start()
        session.close()
        self.assertEqual(len(stream.receive(1000).pixels), 12)
        control.close()
        self.assertEqual(request.wait(1000), ModeSuccess(MfaMode.TASK_SCHED))
        request.close()
        stream.close()
        events = self.wait_for_event("library_unloaded")
        self.assertLess(events.index("mode_completed"), events.index("c2_channel_destroyed"))
        self.assertLess(events.index("c2_channel_destroyed"), events.index("library_unloaded"))
        self.assertLess(
            events.index("callbacks_quiesced_by_channel_destruction"),
            events.index("library_unloaded"),
        )

    def test_direct_owner_construction_is_rejected(self) -> None:
        with self.assertRaisesRegex(TypeError, "Session.open_control_channel"):
            ControlChannel()
        with self.assertRaisesRegex(TypeError, "ControlChannel.submit_operate"):
            ModeRequest()


if __name__ == "__main__":
    unittest.main()
