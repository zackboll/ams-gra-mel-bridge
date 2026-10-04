"""Task 032B4: public Python common Channel for the C2 and Image owners.

Every integration test uses the public API; pure conversion tests reach only
the private helpers they are named after. No new mock behavior is used.
"""

from __future__ import annotations

import ctypes
import dataclasses
import gc
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
import weakref
from unittest import mock

import ams_mel
from ams_mel import (
    BandInfo,
    BandType,
    ChannelCapability,
    ChannelType,
    ChannelView,
    CommandReturn,
    CommsCompleted,
    CommsRejected,
    CommsRequest,
    CommsTestReport,
    CommsTestRequest,
    ComponentLocation,
    ControlConfig,
    CoordinateSystem,
    ErrorKind,
    ImageBand,
    ImageConfig,
    MelError,
    MelErrorCode,
    MetadataCapability,
    MfaMode,
    ModeSuccess,
    PixelFormat,
    ReturnCompleted,
    ReturnRejected,
    ReturnRequest,
    SensorType,
    Session,
    UciId,
    _native,
)

HIGH = CommsTestRequest(0x80000001, 0xF0000002, 0xE0000003)
HIGH_REPORT = CommsCompleted(CommsTestReport(0x80000001, 0xE0000003))
KEEPALIVE_SUCCESS = ReturnCompleted(CommandReturn.SUCCESS)
LONG_DESCRIPTION = "x" * 510 + "\u20ac" + "y" * 100
CAPABILITY_ENUMS = (
    PixelFormat, SensorType, ChannelType, MetadataCapability, BandType, CoordinateSystem,
)
_CTYPES_TYPES = (ctypes._SimpleCData, ctypes.Structure, ctypes.Union, ctypes.Array,
                 ctypes._Pointer)


def mock_uuid(seed: int) -> bytes:
    """The mock's ``metadata_id(seed, ...)`` UUID: byte i = seed + 7 * i."""

    return bytes((seed + 7 * index) & 0xFF for index in range(16))


def assert_owned_graph(test: unittest.TestCase, value: object) -> None:
    """Every node is a plain Python value, enum, frozen dataclass, or tuple."""

    test.assertNotIsInstance(value, _CTYPES_TYPES)
    test.assertNotIsInstance(value, (memoryview, bytearray, list, dict))
    if dataclasses.is_dataclass(value):
        test.assertTrue(type(value).__dataclass_params__.frozen)  # type: ignore[attr-defined]
        for field in dataclasses.fields(value):
            assert_owned_graph(test, getattr(value, field.name))
    elif isinstance(value, tuple):
        for item in value:
            assert_owned_graph(test, item)
    else:
        test.assertIsInstance(value, (int, float, str, bytes))
        test.assertIn(type(value).__module__, ("builtins", "ams_mel"))


class CommonChannelTests(unittest.TestCase):
    def setUp(self) -> None:
        self.mock_provider = (
            Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        )
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="ams-mel-python-common-"
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
        """Observe genuinely asynchronous worker teardown; not an ownership proof."""

        for _ in range(5000):
            events = self.lifecycle()
            if event in events:
                return events
            time.sleep(0.001)
        self.fail(f"timed out waiting for {event}:\n{self.lifecycle()}")

    def open(self, scenario: str) -> Session:
        return Session.open(self.mock_provider, scenario, "")

    @staticmethod
    def c2_config() -> ControlConfig:
        return ControlConfig.new(
            UciId(bytes(range(16)), "IR C2 channel"),
            UciId(bytes(range(0xF0, 0x100)), "test platform"),
            ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
        )

    @staticmethod
    def image_config() -> ImageConfig:
        return ImageConfig.new(
            UciId(bytes(range(16)), "IR image channel"),
            UciId(bytes(range(0xF0, 0x100)), "test platform"),
            ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
            buffer_count=2,
            buffer_size=64,
            queue_capacity=2,
        )

    def assert_error(self, kind: ErrorKind, call: object) -> MelError:
        with self.assertRaises(MelError) as caught:
            call()  # type: ignore[operator]
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def exercise(self, view: ChannelView) -> ChannelCapability:
        """KeepAlive (existing ReturnRequest), high-ID CommsTest, Capabilities."""

        keepalive = view.send_keepalive()
        self.assertIs(type(keepalive), ReturnRequest)
        self.assertEqual(keepalive.wait(5000), KEEPALIVE_SUCCESS)
        self.assertEqual(keepalive.wait(0), KEEPALIVE_SUCCESS)
        keepalive.close()
        keepalive.close()

        comms = view.submit_comms_test(HIGH)
        self.assertIs(type(comms), CommsRequest)
        self.assertEqual(comms.wait(5000), HIGH_REPORT)
        self.assertEqual(comms.wait(0), HIGH_REPORT)
        comms.close()
        comms.close()

        capability = view.capabilities()
        assert_owned_graph(self, capability)
        return capability

    def assert_expired(self, view: ChannelView) -> None:
        self.assertTrue(view.is_open, "an expired weak view is still owned")
        self.assert_error(ErrorKind.PROVIDER_FAILED, view.send_keepalive)
        self.assert_error(ErrorKind.PROVIDER_FAILED, lambda: view.submit_comms_test(HIGH))
        self.assert_error(ErrorKind.PROVIDER_FAILED, view.capabilities)

    def weak_view_after_teardown(
        self, view: ChannelView, destroyed: str, no_prior_requests: bool
    ) -> None:
        # With no earlier requests, teardown must be complete when Session
        # close returns: read the log ONCE with no waiting. Otherwise earlier
        # completion workers may release their final owner on their own thread.
        if no_prior_requests:
            events = self.lifecycle()
        else:
            events = self.wait_for_event("library_unloaded\n")
        self.assertTrue(view.is_open)
        self.assertEqual(events.splitlines().count(destroyed), 1, events)
        self.assertTrue(
            events.endswith("control_destroyed\nmanager_destroyed\nlibrary_unloaded\n"),
            f"provider teardown was delayed by the live View:\n{events}",
        )
        self.assert_expired(view)
        # Only the test-build facade's destruction of the completion owner it
        # preallocated for each refused KeepAlive (family 1) and CommsTest
        # (family 2) may appear; no provider event may.
        after = self.lifecycle()
        self.assertTrue(after.startswith(events))
        self.assertEqual(
            sorted(after[len(events):].splitlines()),
            ["completion_owner_destroyed_1", "completion_owner_destroyed_2"],
            f"expired View operations reached the provider:\n{after}",
        )
        view.close()
        self.assertFalse(view.is_open)
        view.close()
        self.assertEqual(self.lifecycle(), after, "View close produced lifetime activity")

    # ------------------------------------------------------------ lifecycle

    def test_c2_view_attached_and_enabled_typed_api_unchanged(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        self.assertIsInstance(view, ChannelView)
        self.assertTrue(view.is_open)

        # Attached: typed Enabled-only operations fail, common ones work.
        self.assert_error(ErrorKind.PROVIDER_FAILED, lambda: control.submit_operate(1))
        self.assert_error(ErrorKind.PROVIDER_FAILED, lambda: control.submit_bit_noop(2))
        self.assertEqual(
            self.exercise(view).channel_types, (ChannelType.COMMAND_AND_CONTROL,)
        )

        control.enable()
        self.assertEqual(
            self.exercise(view).channel_types, (ChannelType.COMMAND_AND_CONTROL,)
        )

        operate = control.submit_operate(3)
        self.assertEqual(operate.wait(5000), ModeSuccess(MfaMode.TASK_SCHED))
        bit = control.submit_bit_noop(4)
        self.assertEqual(bit.wait(5000), KEEPALIVE_SUCCESS)
        operate.close()
        bit.close()

        view.close()
        self.assertFalse(view.is_open)
        view.close()
        self.assertTrue(control.is_open, "view close closed the typed owner")
        control.close()
        session.close()

    def test_image_view_attached_and_running_existing_api_unchanged(self) -> None:
        session = self.open("comms-high")
        stream = session.open_image_stream(self.image_config())
        view = stream.channel_view()  # Created before Start.
        self.assertEqual(self.exercise(view).pixel_format, PixelFormat.MONO)

        stream.start()
        # comms-high returns the upstream-default Image capability record.
        self.assertEqual(self.exercise(view).pixel_format, PixelFormat.MONO)
        self.assertEqual(stream.receive(1000).frame_id, 1)
        self.assertGreaterEqual(stream.counters().frames_received, 1)

        view.close()
        view.close()
        self.assertTrue(stream.is_open)
        stream.close()
        session.close()

    # ------------------------------------------------------ request results

    def test_keepalive_return_fail_is_normal_completion(self) -> None:
        session = self.open("keepalive-fail")
        control = session.open_control_channel(self.c2_config())
        with control.channel_view() as view:
            request = view.send_keepalive()
            expected = ReturnCompleted(CommandReturn.FAIL)
            self.assertEqual(request.wait(5000), expected)
            self.assertEqual(request.wait(0), expected)
            request.close()
        control.close()
        session.close()

    def test_keepalive_rejection_preserves_long_diagnostic(self) -> None:
        self.assertEqual(len(LONG_DESCRIPTION.encode("utf-8")), 613)
        session = self.open("keepalive-reject")
        control = session.open_control_channel(self.c2_config())
        with control.channel_view() as view:
            request = view.send_keepalive()
            expected = ReturnRejected(MelErrorCode.INVALID_STATE, LONG_DESCRIPTION)
            self.assertEqual(request.wait(5000), expected)
            self.assertEqual(request.wait(0), expected)
            request.close()
        control.close()
        session.close()

    def test_comms_rejection_preserves_long_diagnostic(self) -> None:
        session = self.open("comms-reject")
        control = session.open_control_channel(self.c2_config())
        with control.channel_view() as view:
            request = view.submit_comms_test(HIGH)
            expected = CommsRejected(MelErrorCode.INVALID_PARAMETERS, LONG_DESCRIPTION)
            result = request.wait(5000)
            self.assertEqual(result, expected)
            self.assertEqual(len(result.description.encode("utf-8")), 613)
            self.assertEqual(request.wait(0), expected)
            request.close()
        control.close()
        session.close()

    def test_timeout_does_not_consume_and_results_are_cached(self) -> None:
        for scenario, submit, expected in (
            ("keepalive-delayed", lambda view: view.send_keepalive(), KEEPALIVE_SUCCESS),
            ("comms-delayed", lambda view: view.submit_comms_test(HIGH), HIGH_REPORT),
        ):
            with self.subTest(scenario=scenario):
                session = self.open(scenario)
                control = session.open_control_channel(self.c2_config())
                view = control.channel_view()
                request = submit(view)
                self.assert_error(ErrorKind.TIMEOUT, lambda: request.wait(0))
                self.assertTrue(request.is_open)
                self.assertEqual(request.wait(5000), expected)
                self.assertEqual(request.wait(0), expected)
                request.close()
                view.close()
                control.close()
                session.close()

    # ----------------------------------------------------------- close first

    def test_c2_view_close_first_leaves_typed_owner_usable(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        view.close()
        self.assertFalse(view.is_open)
        self.assertTrue(control.is_open)
        control.enable()
        operate = control.submit_operate(5)
        self.assertEqual(operate.wait(5000), ModeSuccess(MfaMode.TASK_SCHED))
        bit = control.submit_bit_noop(6)
        self.assertEqual(bit.wait(5000), KEEPALIVE_SUCCESS)
        operate.close()
        bit.close()
        view.close()
        control.close()
        self.assertFalse(control.is_open)
        session.close()

    def test_image_view_close_first_leaves_stream_usable(self) -> None:
        session = self.open("success")
        stream = session.open_image_stream(self.image_config())
        view = stream.channel_view()
        view.close()
        view.close()
        stream.start()
        self.assertEqual(stream.receive(1000).frame_id, 1)
        self.assertGreaterEqual(stream.counters().frames_received, 1)
        stream.close()
        self.assertFalse(stream.is_open)
        session.close()

    # ------------------------------------------------------------ weak views

    def test_c2_view_is_weak_and_does_not_delay_teardown(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        control.close()
        session.close()
        # Single read immediately after Session close; the defining assertion.
        events = self.lifecycle().splitlines()
        for event in ("c2_channel_destroyed", "control_destroyed",
                      "manager_destroyed", "library_unloaded"):
            self.assertIn(event, events)
        self.weak_view_after_teardown(view, "c2_channel_destroyed", True)

    def test_image_view_is_weak_and_does_not_delay_teardown(self) -> None:
        session = self.open("success")
        stream = session.open_image_stream(self.image_config())
        view = stream.channel_view()
        stream.close()
        session.close()
        self.weak_view_after_teardown(view, "channel_destroyed", True)

    def test_view_retains_no_python_source(self) -> None:
        for family in ("c2", "image"):
            with self.subTest(family=family):
                session = self.open("comms-high")
                session_ref = weakref.ref(session)
                source: object
                if family == "c2":
                    source = session.open_control_channel(self.c2_config())
                else:
                    source = session.open_image_stream(self.image_config())
                source_ref = weakref.ref(source)
                view = source.channel_view()  # type: ignore[attr-defined]
                self.assertEqual(
                    set(vars(view)), {"_owner", "_owner_pointer", "_close_function"}
                )
                self.assertFalse(any(item is source for item in gc.get_referents(view)))
                source.close()  # type: ignore[attr-defined]
                del source
                session.close()
                del session
                gc.collect()
                self.assertIsNone(source_ref())
                self.assertIsNone(session_ref())
                self.assertTrue(view.is_open)
                self.assert_expired(view)
                view.close()

    def test_multiple_views_are_independent_and_weak(self) -> None:
        for family, destroyed in (("c2", "c2_channel_destroyed"),
                                  ("image", "channel_destroyed")):
            with self.subTest(family=family):
                self.lifetime_log.unlink(missing_ok=True)
                session = self.open("comms-high")
                if family == "c2":
                    source = session.open_control_channel(self.c2_config())
                else:
                    source = session.open_image_stream(self.image_config())
                first = source.channel_view()
                second = source.channel_view()
                first.close()
                self.assertFalse(first.is_open)
                self.assertTrue(second.is_open)
                self.exercise(second)
                source.close()
                session.close()
                self.weak_view_after_teardown(second, destroyed, False)

    def test_pending_keepalive_outlives_view_c2_and_session(self) -> None:
        session = self.open("keepalive-lifetime")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        request = view.send_keepalive()
        self.assertIsInstance(request, ReturnRequest)
        self.assert_error(ErrorKind.TIMEOUT, lambda: request.wait(0))
        view.close()
        control.close()
        session.close()
        self.assertEqual(request.wait(5000), KEEPALIVE_SUCCESS)
        self.assertEqual(request.wait(0), KEEPALIVE_SUCCESS)
        request.close()
        events = self.wait_for_event("library_unloaded\n").splitlines()
        self.assertLess(events.index("keepalive_completed"),
                        events.index("c2_channel_destroyed"))
        self.assertLess(events.index("c2_channel_destroyed"),
                        events.index("library_unloaded"))

    # ---------------------------------------------------------- capabilities

    def assert_rich_c2(self, value: ChannelCapability) -> None:
        """The capability-rich profile established by the C/Ada/Rust tests."""

        assert_owned_graph(self, value)
        self.assertEqual(value.channel_id, UciId(mock_uuid(0x11), "channel-\u03b1"))
        self.assertEqual(value.platform_id, UciId(mock_uuid(0x31), "platform-\u20ac"))
        self.assertEqual((value.height, value.width), (1080, 1920))
        self.assertEqual((value.bit_depth, value.row_pitch), (12, 4096))
        self.assertEqual((value.buffer_size, value.image_size), (8_388_608, 4_147_200))
        self.assertEqual(value.number_of_bands, 3)
        self.assertIs(value.pixel_format, PixelFormat.RGB)
        self.assertEqual(
            value.sensor_types, (SensorType.GIMBAL_HORIZONTAL, SensorType.STEP_STARE)
        )
        self.assertEqual(
            value.sensor_location,
            ComponentLocation(1.25, -2.5, 3.75, "sensor-key", "system-\u03b2"),
        )
        self.assertEqual(value.channel_types, (
            ChannelType.COMMAND_AND_CONTROL, ChannelType.INSTRUMENTATION,
            ChannelType.RESERVED_2,
        ))
        self.assertEqual(value.task_schedule_depth, 17)
        self.assertIs(value.odc_available, True)
        self.assertIs(value.nuc_available, True)
        self.assertEqual(value.metadata_capabilities, (
            MetadataCapability.BAD_PIXEL_LIST, MetadataCapability.COMMAND_STATUS,
            MetadataCapability.CHANNEL_COMMS_TEST_REP, MetadataCapability.RESERVED_10,
        ))
        self.assertEqual(value.image_bands, (
            ImageBand(2, (BandInfo(BandType.IR_LONGWAVE, 8.0e-6, 12.0e-6),
                          BandInfo(BandType.IR_MIDWAVE, 3.0e-6, 5.0e-6))),
            ImageBand(9, (BandInfo(BandType.VISIBLE_RED, 620.0e-9, 750.0e-9),)),
        ))
        self.assertEqual(value.nav_frames,
                         (CoordinateSystem.NED_SENSOR, CoordinateSystem.ECEF))

    def assert_image_profile(self, value: ChannelCapability) -> None:
        """Only the fields the mock sets; the rest keep upstream defaults."""

        assert_owned_graph(self, value)
        self.assertEqual(value.channel_id, UciId(bytes(16), ""))
        self.assertEqual(value.platform_id, UciId(bytes(16), ""))
        self.assertEqual((value.height, value.width, value.bit_depth), (200, 320, 8))
        self.assertEqual((value.row_pitch, value.buffer_size, value.image_size), (0, 0, 0))
        self.assertEqual(value.number_of_bands, 1)
        self.assertIs(value.pixel_format, PixelFormat.MONO)
        self.assertEqual(value.sensor_types, (SensorType.UNSPECIFIED,))
        self.assertEqual(value.sensor_location, ComponentLocation(0.0, 0.0, 0.0, "", ""))
        self.assertEqual(value.channel_types, (ChannelType.IRST_IMAGE,))
        self.assertEqual(value.task_schedule_depth, 0)
        self.assertIs(value.odc_available, False)
        self.assertIs(value.nuc_available, False)
        self.assertEqual(value.metadata_capabilities, (
            MetadataCapability.BAD_PIXEL_LIST, MetadataCapability.LINE_OF_SIGHT_REPORT,
            MetadataCapability.LINE_OF_SIGHT_EULER, MetadataCapability.NAVIGATION_REPORT_RESP,
        ))
        self.assertEqual(value.image_bands, ())
        self.assertEqual(value.nav_frames, ())

    def test_c2_rich_capability_snapshot_is_fully_owned(self) -> None:
        session = self.open("capability-rich")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        capability = view.capabilities()
        self.assert_rich_c2(capability)
        view.close()
        control.close()
        session.close()
        self.assertTrue(self.lifecycle().endswith("library_unloaded\n"))
        gc.collect()
        self.assert_rich_c2(capability)
        self.assertEqual(dataclasses.replace(capability), capability)

    def test_image_capability_snapshot_is_fully_owned(self) -> None:
        session = self.open("image-capability-rich")
        stream = session.open_image_stream(self.image_config())
        view = stream.channel_view()
        attached = view.capabilities()
        self.assert_image_profile(attached)
        stream.start()
        running = view.capabilities()
        self.assertEqual(running, attached)
        view.close()
        stream.close()
        session.close()
        self.assertTrue(self.lifecycle().endswith("library_unloaded\n"))
        gc.collect()
        self.assert_image_profile(attached)
        self.assert_image_profile(running)

    # ------------------------------------------------------- closed / local

    def test_closed_source_conversion_is_provider_failed_without_native_call(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        stream = session.open_image_stream(self.image_config())
        control.close()
        stream.close()
        forbidden = mock.Mock(side_effect=AssertionError("native conversion called"))
        with (
            mock.patch.object(_native, "ams_mel_ir_channel_from_c2", forbidden),
            mock.patch.object(_native, "ams_mel_ir_channel_from_stream", forbidden),
        ):
            error = self.assert_error(ErrorKind.PROVIDER_FAILED, control.channel_view)
            self.assertEqual(error.diagnostic, "C2 channel is closed")
            error = self.assert_error(ErrorKind.PROVIDER_FAILED, stream.channel_view)
            self.assertEqual(error.diagnostic, "image stream is closed")
        forbidden.assert_not_called()
        session.close()

    def test_closed_view_operations_fail_locally(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        view.close()
        events = self.lifecycle()
        forbidden = mock.Mock(side_effect=AssertionError("native operation called"))
        with (
            mock.patch.object(_native, "ams_mel_ir_channel_send_keepalive", forbidden),
            mock.patch.object(_native, "ams_mel_ir_channel_submit_comms_test", forbidden),
            mock.patch.object(_native, "ams_mel_ir_channel_get_capabilities", forbidden),
            mock.patch.object(_native, "ams_mel_ir_channel_close", forbidden),
        ):
            for call in (view.send_keepalive, lambda: view.submit_comms_test(HIGH),
                         view.capabilities):
                error = self.assert_error(ErrorKind.PROVIDER_FAILED, call)
                self.assertEqual(error.diagnostic, "ChannelView is closed")
            view.close()
            with self.assertRaises(MelError) as caught:
                with view:
                    pass
            self.assertEqual(caught.exception.kind, ErrorKind.PROVIDER_FAILED)
        forbidden.assert_not_called()
        self.assertEqual(self.lifecycle(), events)
        control.close()
        session.close()

    def test_context_manager_closes_only_view_and_preserves_body_error(self) -> None:
        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        with control.channel_view() as view:
            self.assertTrue(view.is_open)
        self.assertFalse(view.is_open)
        self.assertTrue(control.is_open)

        def failing_close(*arguments: object) -> int:
            del arguments
            raise RuntimeError("close failure")

        view = control.channel_view()
        with self.assertRaisesRegex(ValueError, "body"):
            with mock.patch.object(view, "_close_function", failing_close):
                with view:
                    raise ValueError("body")
        view.close()
        control.close()
        session.close()

    def test_direct_construction_is_rejected(self) -> None:
        with self.assertRaisesRegex(TypeError, "ControlChannel.channel_view"):
            ChannelView()
        with self.assertRaisesRegex(TypeError, "ChannelView.submit_comms_test"):
            CommsRequest()
        with self.assertRaisesRegex(TypeError, "ChannelView.send_keepalive"):
            ReturnRequest()

    def test_finalization_releases_view_and_comms_request(self) -> None:
        session = self.open("comms-delayed")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        pending = view.submit_comms_test(HIGH)
        pending_ref = weakref.ref(pending)
        view_ref = weakref.ref(view)
        closes: list[str] = []
        real_view_close = _native.ams_mel_ir_channel_close
        real_comms_close = _native.ams_mel_ir_channel_comms_request_close

        def view_close(*arguments: object) -> int:
            closes.append(f"view:{arguments[1:]}")
            return int(real_view_close(*arguments))

        def comms_close(*arguments: object) -> int:
            closes.append(f"comms:{arguments[1:]}")
            return int(real_comms_close(*arguments))

        view._close_function = view_close
        pending._close_function = comms_close
        del view, pending
        gc.collect()
        self.assertIsNone(view_ref())
        self.assertIsNone(pending_ref())
        self.assertEqual(
            sorted(closes), ["comms:(None, 0, None)", "view:(None, 0, None)"]
        )
        self.assertTrue(control.is_open)
        control.close()
        session.close()
        self.wait_for_event("library_unloaded\n")

    def test_finalizers_suppress_every_exception(self) -> None:
        def failing_close(*arguments: object) -> int:
            del arguments
            raise RuntimeError("finalizer failure")

        hook_calls: list[object] = []
        original_hook = sys.unraisablehook
        sys.unraisablehook = hook_calls.append
        try:
            for cls, handle in (
                (ChannelView, _native.IrChannelHandle(1)),
                (CommsRequest, _native.IrChannelCommsRequestHandle(1)),
                (ReturnRequest, _native.IrReturnRequestHandle(1)),
            ):
                owner = cls._from_owner(handle)  # type: ignore[attr-defined]
                owner._close_function = failing_close
                owner.__del__()
                owner._owner.value = None
                del owner
                gc.collect()
        finally:
            sys.unraisablehook = original_hook
        self.assertEqual(hook_calls, [])

    def test_comms_request_input_validation(self) -> None:
        for field in ("command_id", "channel_id", "request_id"):
            for value in (-1, 2**32, True, 1.5, "1"):
                with self.subTest(field=field, value=value):
                    values: dict[str, object] = {
                        "command_id": 1, "channel_id": 2, "request_id": 3,
                    }
                    values[field] = value
                    error = self.assert_error(
                        ErrorKind.INVALID_ARGUMENT,
                        lambda: CommsTestRequest(**values),  # type: ignore[arg-type]
                    )
                    self.assertIn(field, error.diagnostic or "")
        self.assertEqual(CommsTestRequest(0, 0xFFFFFFFF, 0).channel_id, 0xFFFFFFFF)

        session = self.open("comms-high")
        control = session.open_control_channel(self.c2_config())
        view = control.channel_view()
        forbidden = mock.Mock(side_effect=AssertionError("native submit called"))
        with mock.patch.object(_native, "ams_mel_ir_channel_submit_comms_test", forbidden):
            for bad in ((1, 2, 3), {"command_id": 1, "channel_id": 2, "request_id": 3}):
                self.assert_error(
                    ErrorKind.INVALID_ARGUMENT,
                    lambda: view.submit_comms_test(bad),  # type: ignore[arg-type]
                )
            mutated = CommsTestRequest(1, 2, 3)
            object.__setattr__(mutated, "channel_id", -1)
            self.assert_error(
                ErrorKind.INVALID_ARGUMENT, lambda: view.submit_comms_test(mutated)
            )
        forbidden.assert_not_called()
        request = view.submit_comms_test(HIGH)
        for timeout in (-1, 2**32, True, 1.5):
            self.assert_error(ErrorKind.INVALID_ARGUMENT, lambda: request.wait(timeout))
        self.assertEqual(request.wait(5000), HIGH_REPORT)
        request.close()
        self.assert_error(ErrorKind.INVALID_ARGUMENT, lambda: request.wait(0))
        view.close()
        control.close()
        session.close()


def set_required(pointer: object, value: int) -> None:
    ctypes.cast(pointer, ctypes.POINTER(ctypes.c_size_t)).contents.value = value


def write_diagnostic(diagnostic: object, capacity: int, text: bytes) -> None:
    if diagnostic and capacity >= len(text) + 1:
        ctypes.memmove(diagnostic, text + b"\0", len(text) + 1)


def comms_result(pointer: object) -> _native.IrChannelCommsTestResultV1:
    return ctypes.cast(
        pointer, ctypes.POINTER(_native.IrChannelCommsTestResultV1)
    ).contents


class CommsWaitTests(unittest.TestCase):
    """Pure mock-patched ``_wait_for_comms`` tests (no provider)."""

    def wait_with(self, fake: object, timeout_ms: int = 0) -> object:
        request = CommsRequest._from_owner(_native.IrChannelCommsRequestHandle(1))
        try:
            with mock.patch.object(_native, "ams_mel_ir_channel_comms_request_wait", fake):
                return request.wait(timeout_ms)
        finally:
            request._owner.value = None

    def test_unknown_rejection_code_is_preserved(self) -> None:
        description = "future CommsTest rejection"

        def wait(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            comms_result(result).error_code = 0xFEDCBA98
            write_diagnostic(diagnostic, capacity, description.encode())
            set_required(required, len(description) + 1)
            return _native.AMS_MEL_COMMAND_REJECTED

        result = self.wait_with(wait)
        assert isinstance(result, CommsRejected)
        self.assertIsInstance(result.code, MelErrorCode)
        self.assertEqual(int(result.code), 0xFEDCBA98)
        self.assertEqual(result.code.name, "UNKNOWN_0xFEDCBA98")
        self.assertEqual(result.description, description)

    def test_success_preserves_all_32_bits_and_requires_clean_result(self) -> None:
        def ok(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            value = comms_result(result)
            value.command_id, value.request_id = 0xFFFFFFFF, 0x80000000
            write_diagnostic(diagnostic, capacity, b"")
            set_required(required, 1)
            return _native.AMS_MEL_OK

        self.assertEqual(
            self.wait_with(ok), CommsCompleted(CommsTestReport(0xFFFFFFFF, 0x80000000))
        )

        def ok_with_error(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            comms_result(result).error_code = _native.AMS_MEL_ERROR_INVALID_STATE
            write_diagnostic(diagnostic, capacity, b"")
            set_required(required, 1)
            return _native.AMS_MEL_OK

        def ok_with_text(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            write_diagnostic(diagnostic, capacity, b"x")
            set_required(required, 2)
            return _native.AMS_MEL_OK

        for fake in (ok_with_error, ok_with_text):
            with self.assertRaises(MelError) as caught:
                self.wait_with(fake)
            self.assertEqual(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_timeout_does_not_retry_for_oversized_diagnostic(self) -> None:
        calls = 0

        def wait(*arguments: object) -> int:
            nonlocal calls
            calls += 1
            set_required(arguments[-1], 2048)
            return _native.AMS_MEL_TIMEOUT

        with self.assertRaises(MelError) as caught:
            self.wait_with(wait)
        self.assertEqual(caught.exception.kind, ErrorKind.TIMEOUT)
        self.assertEqual(calls, 1)

    def test_long_diagnostic_retry_uses_exact_length(self) -> None:
        text = ("z" * 700).encode()
        capacities: list[tuple[int, int]] = []

        def wait(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            capacities.append((int(timeout), int(capacity)))
            value = comms_result(result)
            value.command_id, value.request_id = 7, 9
            value.error_code = _native.AMS_MEL_ERROR_INVALID_PARAMETERS
            write_diagnostic(diagnostic, capacity, text)
            set_required(required, len(text) + 1)
            return _native.AMS_MEL_COMMAND_REJECTED

        self.assertEqual(
            self.wait_with(wait, 1000),
            CommsRejected(MelErrorCode.INVALID_PARAMETERS, text.decode()),
        )
        self.assertEqual(capacities, [(1000, 512), (0, len(text) + 1)])

    def test_long_diagnostic_retry_requires_identical_result(self) -> None:
        for changed in ("status", "command_id", "request_id", "error_code", "required"):
            with self.subTest(changed=changed):
                calls = 0

                def wait(owner, timeout, result, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
                    nonlocal calls
                    calls += 1
                    second = calls == 2
                    value = comms_result(result)
                    value.command_id = 2 if second and changed == "command_id" else 1
                    value.request_id = 2 if second and changed == "request_id" else 1
                    value.error_code = (
                        _native.AMS_MEL_ERROR_INVALID_STATE
                        if second and changed == "error_code"
                        else _native.AMS_MEL_ERROR_INVALID_PARAMETERS
                    )
                    set_required(required, 601 if second and changed == "required" else 600)
                    if second and changed == "status":
                        return _native.AMS_MEL_PROVIDER_FAILED
                    return _native.AMS_MEL_COMMAND_REJECTED

                with self.assertRaises(MelError) as caught:
                    self.wait_with(wait, 1000)
                self.assertEqual(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
                self.assertEqual(calls, 2)


class OwnershipHardeningTests(unittest.TestCase):
    """Mock-patched native results: NULL-on-success and failure-with-owner."""

    @staticmethod
    def view() -> ChannelView:
        return ChannelView._from_owner(_native.IrChannelHandle(0x1000))

    @staticmethod
    def publisher(index: int, status: int, value: int | None) -> object:
        def call(*arguments: object) -> int:
            if value is not None:
                ctypes.cast(arguments[index], ctypes.POINTER(ctypes.c_void_p)).contents.value = value
            write_diagnostic(arguments[-3], int(arguments[-2]), b"")  # type: ignore[arg-type]
            set_required(arguments[-1], 1)
            return status
        return call

    @staticmethod
    def recording_close(closed: list[int]) -> object:
        def close(owner_pointer: object, *arguments: object) -> int:
            owner = ctypes.cast(owner_pointer, ctypes.POINTER(ctypes.c_void_p)).contents
            closed.append(int(owner.value or 0))
            owner.value = None
            return _native.AMS_MEL_OK
        return close

    def check(self, submit_name: str, index: int, close_name: str,
              operation: Callable) -> None:  # type: ignore[type-arg]
        view = self.view()
        try:
            with mock.patch.object(_native, submit_name,
                                   self.publisher(index, _native.AMS_MEL_OK, None)):
                with self.assertRaises(MelError) as caught:
                    operation(view)
                self.assertEqual(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
            closed: list[int] = []
            with (
                mock.patch.object(_native, submit_name,
                                  self.publisher(index, _native.AMS_MEL_PROVIDER_FAILED, 0x77)),
                mock.patch.object(_native, close_name, self.recording_close(closed)),
            ):
                with self.assertRaises(MelError) as caught:
                    operation(view)
                self.assertEqual(caught.exception.kind, ErrorKind.PROVIDER_FAILED)
            self.assertEqual(closed, [0x77])
        finally:
            view._owner.value = None

    def test_keepalive_submission(self) -> None:
        self.check("ams_mel_ir_channel_send_keepalive", 1,
                   "ams_mel_ir_return_request_close", lambda view: view.send_keepalive())

    def test_comms_submission(self) -> None:
        self.check("ams_mel_ir_channel_submit_comms_test", 2,
                   "ams_mel_ir_channel_comms_request_close",
                   lambda view: view.submit_comms_test(HIGH))

    def test_capability_query(self) -> None:
        self.check("ams_mel_ir_channel_get_capabilities", 1,
                   "ams_mel_ir_channel_capability_close", lambda view: view.capabilities())

    def test_conversions(self) -> None:
        for convert_name, method in (("ams_mel_ir_channel_from_c2", "ControlChannel"),
                                     ("ams_mel_ir_channel_from_stream", "ImageStream")):
            with self.subTest(convert=convert_name):
                source = getattr(ams_mel, method)._from_owner(ctypes.c_void_p(0x2000))
                try:
                    with mock.patch.object(_native, convert_name,
                                           self.publisher(1, _native.AMS_MEL_OK, None)):
                        with self.assertRaises(MelError) as caught:
                            source.channel_view()
                        self.assertEqual(caught.exception.kind,
                                         ErrorKind.PROTOCOL_INCONSISTENCY)
                    closed: list[int] = []
                    with (
                        mock.patch.object(_native, convert_name, self.publisher(
                            1, _native.AMS_MEL_INTERNAL_ERROR, 0x88)),
                        mock.patch.object(_native, "ams_mel_ir_channel_close",
                                          self.recording_close(closed)),
                    ):
                        with self.assertRaises(MelError) as caught:
                            source.channel_view()
                        self.assertEqual(caught.exception.kind, ErrorKind.INTERNAL_ERROR)
                    self.assertEqual(closed, [0x88])
                finally:
                    source._owner.value = None

    def test_view_close_must_clear_owner(self) -> None:
        view = self.view()
        try:
            with mock.patch.object(view, "_close_function",
                                   self.publisher(0, _native.AMS_MEL_OK, None)):
                with self.assertRaises(MelError) as caught:
                    view.close()
                self.assertEqual(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
        finally:
            view._owner.value = None


class CapabilityConversionTests(unittest.TestCase):
    """The capability guard and checked copies over a crafted in-process record.

    Every non-NULL pointer here addresses live ctypes storage; malformed cases
    are NULL+size, oversize, and misalignment, which must be rejected before
    any dereference. No unmapped address is ever handed to ctypes.
    """

    def setUp(self) -> None:
        self.keep: list[object] = []
        self.record = _native.IrChannelCapabilityV1()
        self.record.odc_available = 1
        self.record.nuc_available = 0

    def string(self, value: bytes) -> _native.StringViewV1:
        buffer = ctypes.create_string_buffer(value, len(value) + 1)
        self.keep.append(buffer)
        return _native.StringViewV1(ctypes.cast(buffer, _native.CharPointer), len(value))

    def u32_span(self, values: list[int]) -> _native.U32SpanV1:
        array = (ctypes.c_uint32 * len(values))(*values)
        self.keep.append(array)
        return _native.U32SpanV1(ctypes.cast(array, ctypes.POINTER(ctypes.c_uint32)),
                                 len(values))

    def run_capabilities(self) -> tuple[object, list[str]]:
        """Drive ChannelView.capabilities through the private snapshot guard."""

        events: list[str] = []
        record = self.record

        def get(view, out, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p)).contents.value = 0x55
            events.append("get")
            write_diagnostic(diagnostic, capacity, b"")
            set_required(required, 1)
            return _native.AMS_MEL_OK

        def view_record(snapshot, out, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            events.append("view")
            if not getattr(self, "null_record", False):
                ctypes.cast(out, ctypes.POINTER(ctypes.POINTER(
                    _native.IrChannelCapabilityV1))).contents.contents = record
            write_diagnostic(diagnostic, capacity, b"")
            set_required(required, 1)
            return _native.AMS_MEL_OK

        def close(owner, diagnostic, capacity, required):  # type: ignore[no-untyped-def]
            handle = ctypes.cast(owner, ctypes.POINTER(ctypes.c_void_p)).contents
            events.append(f"close:{'diag' if diagnostic else 'best-effort'}")
            if required and getattr(self, "close_fails", False):
                write_diagnostic(diagnostic, capacity, b"close failed")
                set_required(required, 13)
                return _native.AMS_MEL_INTERNAL_ERROR
            handle.value = None
            if required:
                write_diagnostic(diagnostic, capacity, b"")
                set_required(required, 1)
            return _native.AMS_MEL_OK

        view = ChannelView._from_owner(_native.IrChannelHandle(0x1000))
        try:
            with (
                mock.patch.object(_native, "ams_mel_ir_channel_get_capabilities", get),
                mock.patch.object(_native, "ams_mel_ir_channel_capability_view", view_record),
                mock.patch.object(_native, "ams_mel_ir_channel_capability_close", close),
            ):
                try:
                    return view.capabilities(), events
                except MelError as error:
                    return error, events
        finally:
            view._owner.value = None

    def assert_rejected(self, kind: ErrorKind) -> None:
        result, events = self.run_capabilities()
        self.assertIsInstance(result, MelError)
        assert isinstance(result, MelError)
        self.assertEqual(result.kind, kind, result)
        # Every failure path best-effort closes the native snapshot exactly once.
        self.assertEqual(events, ["get", "view", "close:best-effort"])

    def test_well_formed_record_is_copied_then_snapshot_closed(self) -> None:
        self.record.channel_id.uuid[:] = list(range(16))
        self.record.channel_id.descriptive_label = self.string("chan-\u03b1".encode())
        self.record.sensor_types = self.u32_span([1, 0xFEDCBA98])
        self.record.sensor_location.key = self.string(b"key")
        bands = (_native.IrBandInfoV1 * 1)(_native.IrBandInfoV1(8, 1.0, 2.0))
        image = (_native.IrImageBandV1 * 1)(_native.IrImageBandV1(
            5, _native.IrBandInfoSpanV1(ctypes.cast(bands, ctypes.POINTER(
                _native.IrBandInfoV1)), 1)))
        self.keep.extend((bands, image))
        self.record.image_bands = _native.IrImageBandSpanV1(
            ctypes.cast(image, ctypes.POINTER(_native.IrImageBandV1)), 1)
        value, events = self.run_capabilities()
        self.assertEqual(events, ["get", "view", "close:diag"])
        assert isinstance(value, ChannelCapability)
        self.assertEqual(value.channel_id, UciId(bytes(range(16)), "chan-\u03b1"))
        self.assertEqual(value.sensor_types[0], SensorType.GIMBAL_HORIZONTAL)
        self.assertEqual(value.sensor_types[1].name, "UNKNOWN_0xFEDCBA98")
        self.assertEqual(value.image_bands,
                         (ImageBand(5, (BandInfo(BandType.VISIBLE_RED, 1.0, 2.0),)),))
        self.assertIs(value.odc_available, True)
        self.assertIs(value.nuc_available, False)
        # Mutating/freeing the native storage cannot affect the owned copy.
        self.record.channel_id.uuid[0] = 0xFF
        ctypes.memset(self.keep[0], 0, 4)  # type: ignore[arg-type]
        self.keep.clear()
        gc.collect()
        self.assertEqual(value.channel_id, UciId(bytes(range(16)), "chan-\u03b1"))
        assert_owned_graph(self, value)

    def test_null_record_is_protocol_inconsistency(self) -> None:
        self.null_record = True
        self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_non_empty_spans_with_null_pointer(self) -> None:
        for field in ("sensor_types", "channel_types", "metadata_capabilities",
                      "nav_frames"):
            with self.subTest(field=field):
                self.setUp()
                setattr(self.record, field, _native.U32SpanV1(None, 3))
                self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)
        self.setUp()
        self.record.image_bands = _native.IrImageBandSpanV1(None, 1)
        self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)
        self.setUp()
        image = (_native.IrImageBandV1 * 1)(_native.IrImageBandV1(
            2, _native.IrBandInfoSpanV1(None, 2)))
        self.keep.append(image)
        self.record.image_bands = _native.IrImageBandSpanV1(
            ctypes.cast(image, ctypes.POINTER(_native.IrImageBandV1)), 1)
        self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)
        self.setUp()
        self.record.platform_id.descriptive_label = _native.StringViewV1(None, 4)
        self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_oversized_and_misaligned_spans_rejected_before_dereference(self) -> None:
        storage = (ctypes.c_uint64 * 4)()
        self.keep.append(storage)
        base = ctypes.addressof(storage)
        oversize = sys.maxsize // ctypes.sizeof(ctypes.c_uint32) + 1
        cases = (
            ("u32 oversize", "sensor_types", _native.U32SpanV1(
                ctypes.cast(base, ctypes.POINTER(ctypes.c_uint32)), oversize)),
            ("u32 misaligned", "nav_frames", _native.U32SpanV1(
                ctypes.cast(base + 1, ctypes.POINTER(ctypes.c_uint32)), 1)),
            ("band misaligned", "image_bands", _native.IrImageBandSpanV1(
                ctypes.cast(base + 4, ctypes.POINTER(_native.IrImageBandV1)), 1)),
        )
        for name, field, span in cases:
            with self.subTest(case=name):
                self.setUp()
                setattr(self.record, field, span)
                with mock.patch.object(
                    ctypes, "string_at", side_effect=AssertionError("dereferenced")
                ):
                    self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)
        self.setUp()
        self.record.channel_id.descriptive_label = _native.StringViewV1(
            ctypes.cast(base, _native.CharPointer), sys.maxsize + 1)
        with mock.patch.object(ctypes, "string_at",
                               side_effect=AssertionError("dereferenced")):
            self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_zero_length_spans_are_empty_without_inspecting_pointer(self) -> None:
        storage = (ctypes.c_uint64 * 2)()
        self.keep.append(storage)
        odd = ctypes.addressof(storage) + 1  # Misaligned but never inspected.
        self.record.sensor_types = _native.U32SpanV1(
            ctypes.cast(odd, ctypes.POINTER(ctypes.c_uint32)), 0)
        self.record.image_bands = _native.IrImageBandSpanV1(None, 0)
        self.record.platform_id.descriptive_label = _native.StringViewV1(
            ctypes.cast(odd, _native.CharPointer), 0)
        value, _ = self.run_capabilities()
        assert isinstance(value, ChannelCapability)
        self.assertEqual((value.sensor_types, value.image_bands), ((), ()))
        self.assertEqual(value.platform_id.descriptive_label, "")
        self.assertEqual(value.nav_frames, ())

    def test_invalid_utf8_and_embedded_nul_strings(self) -> None:
        self.record.sensor_location.system_name = self.string(b"bad\xc3\x28")
        self.assert_rejected(ErrorKind.INVALID_UTF8)
        self.setUp()
        self.record.channel_id.descriptive_label = self.string(b"a\0b")
        self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_booleans_accept_only_zero_and_one(self) -> None:
        for field, value in (("odc_available", 2), ("nuc_available", 0xFFFFFFFF)):
            with self.subTest(field=field, value=value):
                self.setUp()
                setattr(self.record, field, value)
                self.assert_rejected(ErrorKind.PROTOCOL_INCONSISTENCY)

    def test_owned_storage_failure_is_internal_error(self) -> None:
        self.record.sensor_types = self.u32_span([1, 2])
        with mock.patch.object(ams_mel, "SensorType", side_effect=MemoryError):
            self.assert_rejected(ErrorKind.INTERNAL_ERROR)
        self.setUp()
        self.record.channel_id.descriptive_label = self.string(b"label")
        with mock.patch.object(ctypes, "string_at", side_effect=MemoryError):
            self.assert_rejected(ErrorKind.INTERNAL_ERROR)

    def test_capability_close_failure_is_reported_then_best_effort_closed(self) -> None:
        self.close_fails = True
        result, events = self.run_capabilities()
        assert isinstance(result, MelError)
        self.assertEqual(result.kind, ErrorKind.INTERNAL_ERROR)
        self.assertEqual(result.diagnostic, "close failed")
        # The explicit Close failed and left the snapshot open, so the failure
        # path then made one best-effort close; no partial value escaped.
        self.assertEqual(events, ["get", "view", "close:diag", "close:best-effort"])


class PublicSurfaceTests(unittest.TestCase):
    def test_unknown_capability_enum_values_are_preserved(self) -> None:
        for enum_type in CAPABILITY_ENUMS:
            with self.subTest(enum=enum_type.__name__):
                value = enum_type(0xFEDCBA98)
                self.assertIsInstance(value, enum_type)
                self.assertEqual(int(value), 0xFEDCBA98)
                self.assertEqual(value.name, "UNKNOWN_0xFEDCBA98")
                self.assertEqual(enum_type(0xFEDCBA98), 0xFEDCBA98)
                self.assertEqual(enum_type(0xFFFFFFFF).name, "UNKNOWN_0xFFFFFFFF")
                # Out-of-range and non-integer values are not fabricated
                # (same rule as MelErrorCode._missing_).
                for bad in (-1, 2**32, 1.5, "1"):
                    with self.assertRaises(ValueError):
                        enum_type(bad)

    def test_capability_enum_inventory(self) -> None:
        self.assertEqual(
            [len(enum_type) for enum_type in CAPABILITY_ENUMS], [3, 5, 9, 32, 15, 4]
        )
        self.assertEqual(int(MetadataCapability.RESERVED_10), 31)
        self.assertEqual(int(BandType.UV_VACUUM), 14)
        self.assertNotIn("_Uint32PreservingEnum", ams_mel.__all__)

    def test_public_exports(self) -> None:
        new = {
            "ChannelView", "CommsTestRequest", "CommsTestReport", "CommsCompleted",
            "CommsRejected", "CommsRequest", "ChannelCapability", "PixelFormat",
            "SensorType", "ChannelType", "MetadataCapability", "BandType",
            "CoordinateSystem", "BandInfo", "ImageBand",
        }
        self.assertTrue(new <= set(ams_mel.__all__))
        self.assertEqual(len(ams_mel.__all__), len(set(ams_mel.__all__)))
        for name in ams_mel.__all__:
            self.assertFalse(name.startswith("_"))
            self.assertTrue(hasattr(ams_mel, name))
        for leaked in ("_native", "IrChannelHandle", "IrChannelCapabilityHandle",
                       "IrChannelCapabilityV1"):
            self.assertNotIn(leaked, ams_mel.__all__)
        # Task 034B1B1 adds two private raw C2 lifecycle bindings; no public RF API.
        self.assertEqual(len(_native.BOUND_FUNCTION_NAMES), 180)
        for leaked in ("RfAdminHandle", "RfC2Handle", "RfVaRequestHandle", "RfVaHandle",
                       "RfVaConfigV1", "RfVaResultV1", "RfVaInfoV1",
                       "RfDataHandle", "RfMfaInfoHandle", "RfMfaInfoV1",
                       "RfProductRxRequestHandle", "RfProductRxHandle",
                       "RfProductRxEventHandle", "RfProductRxEventV1",
                       "RfProductRxConfigV1", "RfComplexI16V1"):
            self.assertNotIn(leaked, ams_mel.__all__)
            self.assertFalse(hasattr(ams_mel, leaked))
        public_view_members = {name for name in dir(ChannelView) if not name.startswith("_")}
        self.assertEqual(
            public_view_members,
            {"is_open", "send_keepalive", "submit_comms_test", "capabilities", "close"},
        )
        self.assertEqual(
            {name for name in dir(CommsRequest) if not name.startswith("_")},
            {"is_open", "wait", "close"},
        )

    def test_frozen_result_types(self) -> None:
        report = CommsTestReport(1, 2)
        self.assertEqual([field.name for field in dataclasses.fields(report)],
                         ["command_id", "request_id"])
        self.assertEqual(len(dataclasses.fields(ChannelCapability)), 19)
        for value in (report, CommsCompleted(report), CommsRejected(MelErrorCode.NONE, ""),
                      HIGH, BandInfo(BandType.UVA, 0.0, 1.0), ImageBand(1, ())):
            with self.assertRaises(dataclasses.FrozenInstanceError):
                setattr(value, dataclasses.fields(value)[0].name, 0)


if __name__ == "__main__":
    unittest.main()
