"""PY-MEL-004: Health lifecycle over the compiled provider and published C ABI.

Native-boundary injection is restricted to malformed outputs absent from the
mock. Completion probes/gates are test-only and never enter the public API.
Uncertain retention and explicit test DSO loading are subprocess-isolated.
"""

from __future__ import annotations

import ctypes
from dataclasses import FrozenInstanceError, replace
import gc
import math
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


def config() -> mel.HealthConfig:
    return mel.HealthConfig.new(
        mel.UciId(bytes(range(16)), "IR Health channel"),
        mel.UciId(bytes(range(0xF0, 0x100)), "test platform"),
        mel.ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
    )


def provider() -> Path:
    return Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"


def quiesce() -> None:
    """Block until common completion owners retire, never infer it from timing."""
    boundary = _native._LIBRARY.ams_mel_test_completion_boundary
    boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64)]
    boundary.restype = ctypes.c_int
    owner = _native._LIBRARY.ams_mel_test_completion_owner
    owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
    owner.restype = ctypes.c_int
    for family in (1, 2):
        counts = (ctypes.c_uint64 * 4)()
        observed = ctypes.c_uint64()
        if boundary(family, 3, 0, counts) != 1 or owner(family, counts[2], ctypes.byref(observed)) != 1:
            raise AssertionError("common completion owners did not retire")


class _HealthTestCase(unittest.TestCase):
    def setUp(self):
        quiesce()
        directory = tempfile.TemporaryDirectory(prefix="py-health-")
        self.addCleanup(directory.cleanup)
        self.log = Path(directory.name) / "lifetime.log"
        environment = mock.patch.dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(self.log))
        environment.start()
        self.addCleanup(environment.stop)

    def events(self):
        # Test-facade completion observations are not provider lifecycle events.
        lines = self.log.read_text(encoding="utf-8").splitlines() if self.log.exists() else []
        return [line for line in lines if not line.startswith(("completion_", "probe_"))]

    def open(self, scenario="health-capability", configuration=None):
        session = mel.Session.open(provider(), scenario, "")
        self.addCleanup(session.close)
        channel = session.open_health_channel(config() if configuration is None else configuration)
        self.addCleanup(channel.close)
        return session, channel

    def error(self, kind, operation, status=None, diagnostic=None):
        with self.assertRaises(mel.MelError) as caught:
            operation()
        error = caught.exception
        self.assertIs(error.kind, kind)
        if status is not None:
            self.assertEqual(error.native_status, status)
        if diagnostic is not None:
            self.assertEqual(error.diagnostic, diagnostic)
        return error

    def profile(self, value):
        """Every field of the existing rich Health profile, including nested bands."""
        common.assert_owned_graph(self, value)
        self.assertEqual(value, mel.ChannelCapability(
            channel_id=mel.UciId(common.mock_uuid(0x11), "channel-α"),
            height=1080, width=1920, bit_depth=12, row_pitch=4096,
            buffer_size=8_388_608, image_size=4_147_200, number_of_bands=3,
            pixel_format=mel.PixelFormat.RGB,
            sensor_types=(mel.SensorType.GIMBAL_HORIZONTAL, mel.SensorType.STEP_STARE),
            platform_id=mel.UciId(common.mock_uuid(0x31), "platform-€"),
            sensor_location=mel.ComponentLocation(1.25, -2.5, 3.75, "sensor-key", "system-β"),
            channel_types=(mel.ChannelType.HEALTH_AND_STATUS,),
            task_schedule_depth=17, odc_available=True, nuc_available=True,
            metadata_capabilities=(mel.MetadataCapability.MFA_STATUS,
                                   mel.MetadataCapability.MFA_STATUS_DETAILED,
                                   mel.MetadataCapability.BIT_STATUS,
                                   mel.MetadataCapability.SUBSYSTEM_STATUS_RESP),
            image_bands=(mel.ImageBand(2, (
                mel.BandInfo(mel.BandType.IR_LONGWAVE, 8.0e-6, 12.0e-6),
                mel.BandInfo(mel.BandType.IR_MIDWAVE, 3.0e-6, 5.0e-6))),
                mel.ImageBand(9, (mel.BandInfo(mel.BandType.VISIBLE_RED, 620.0e-9, 750.0e-9),))),
            nav_frames=(mel.CoordinateSystem.NED_SENSOR, mel.CoordinateSystem.ECEF),
        ))

    def expired(self, view):
        self.assertTrue(view.is_open)
        for operation in (view.send_keepalive, lambda: view.submit_comms_test(common.HIGH),
                          view.capabilities):
            self.error(mel.ErrorKind.PROVIDER_FAILED, operation, _native.AMS_MEL_PROVIDER_FAILED)

    def subprocess(self, source):
        environment = dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path))
        result = subprocess.run([sys.executable, "-W", "error", "-c", source],
                                env=environment, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("HEALTH_CHILD_OK", result.stdout)

class HealthTests(_HealthTestCase):
    def test_config_is_frozen_and_new_matches_constructor(self):
        value = config()
        self.assertEqual(value, mel.HealthConfig(value.channel_id, value.platform_id, value.sensor_location))
        for target, field in ((value, "channel_id"), (value.channel_id, "uuid"),
                              (value.sensor_location, "key")):
            with self.assertRaises(FrozenInstanceError):
                setattr(target, field, None)
        self.assertIn("HealthConfig", mel.__all__)
        self.assertIn("HealthStatusChannel", mel.__all__)
        common.assert_owned_graph(self, value)

    def test_config_rejects_invalid_types_uuids_and_coordinates(self):
        value = config()
        for field in ("channel_id", "platform_id", "sensor_location"):
            for bad in (None, "id", 1, bytes(16)):
                self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: replace(value, **{field: bad}))
        for bad in (b"", bytes(15), bytes(17), bytearray(16), "0" * 16, None):
            self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: mel.UciId(bad, ""))
        for field in ("offset_x_m", "offset_y_m", "offset_z_m"):
            for bad in (True, None, "1", 1 + 2j, 10**1000):
                location = replace(value.sensor_location, **{field: bad})
                self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: replace(value, sensor_location=location))

    def test_invalid_text_is_rejected_without_native_open(self):
        session, _ = self.open()
        with mock.patch.object(_native, "ams_mel_ir_health_open") as opened:
            for bad in ("a\0b", "\ud800", b"\xff", None):
                for target, field in ((config().channel_id, "descriptive_label"),
                                      (config().platform_id, "descriptive_label"),
                                      (config().sensor_location, "key"),
                                      (config().sensor_location, "system_name")):
                    self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: replace(target, **{field: bad}))
            for bad in (None, config().channel_id, mel.ControlConfig(
                    config().channel_id, config().platform_id, config().sensor_location)):
                self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: session.open_health_channel(bad))
            # Revalidation protects Open even against deliberate frozen-dataclass bypass.
            for path, field, bad in (("channel_id", "uuid", bytes(15)),
                                     ("platform_id", "descriptive_label", "\udfff"),
                                     ("sensor_location", "key", "x\0y"),
                                     ("sensor_location", "system_name", b"bad"),
                                     ("sensor_location", "offset_z_m", 10**1000)):
                value = config()
                object.__setattr__(getattr(value, path), field, bad)
                self.error(mel.ErrorKind.INVALID_ARGUMENT, lambda: session.open_health_channel(value))
            opened.assert_not_called()
        self.assertEqual(self.events().count("channel_attached"), 1)

    def test_exact_c_config_fidelity_and_field_order(self):
        text = " e\u0301-μ-€ " + "x" * 5000
        value = mel.HealthConfig.new(
            mel.UciId(bytes(range(16)), text), mel.UciId(bytes(range(255, 239, -1)), text[::-1]),
            mel.ComponentLocation(-0.0, -2.5, sys.float_info.max, text, text[::-1]),
        )
        native = _native.ams_mel_ir_health_open
        observations = []

        def inspect(session, pointer, *args):
            gc.collect()  # Borrowed buffers must remain alive at the actual C call.
            raw = ctypes.cast(pointer, ctypes.POINTER(_native.IrHealthConfigV1)).contents
            self.assertEqual([name for name, _ in type(raw)._fields_],
                             ["channel_id", "channel_type", "platform_id", "sensor_location"])
            self.assertEqual(raw.channel_type, 4)
            for actual, expected in ((raw.channel_id, value.channel_id), (raw.platform_id, value.platform_id)):
                self.assertEqual(bytes(actual.uuid), expected.uuid)
                self.assertEqual(ctypes.string_at(actual.descriptive_label.data,
                                                 actual.descriptive_label.size), expected.descriptive_label.encode())
            location = raw.sensor_location
            self.assertEqual(math.copysign(1, location.offset_x_m), -1)
            self.assertEqual((location.offset_y_m, location.offset_z_m), (-2.5, sys.float_info.max))
            for field in ("key", "system_name"):
                view = getattr(location, field)
                self.assertEqual(ctypes.string_at(view.data, view.size), getattr(value.sensor_location, field).encode())
            observations.append(True)
            return native(session, pointer, *args)

        with mock.patch.object(_native, "ams_mel_ir_health_open", side_effect=inspect):
            self.open(configuration=value)
        self.assertEqual(observations, [True])

    def test_empty_text_and_c_double_ieee_values_are_not_normalized(self):
        value = mel.HealthConfig.new(mel.UciId(bytes(16), ""), mel.UciId(bytes(16), ""),
                                    mel.ComponentLocation(float("inf"), float("-inf"), float("nan"), "", ""))
        native = _native.ams_mel_ir_health_open

        def inspect(session, pointer, *args):
            raw = ctypes.cast(pointer, ctypes.POINTER(_native.IrHealthConfigV1)).contents
            self.assertEqual((raw.channel_id.descriptive_label.size, raw.platform_id.descriptive_label.size,
                              raw.sensor_location.key.size, raw.sensor_location.system_name.size), (0, 0, 0, 0))
            self.assertEqual(raw.sensor_location.offset_x_m, float("inf"))
            self.assertEqual(raw.sensor_location.offset_y_m, float("-inf"))
            self.assertTrue(math.isnan(raw.sensor_location.offset_z_m))
            return native(session, pointer, *args)

        with mock.patch.object(_native, "ams_mel_ir_health_open", side_effect=inspect):
            self.open(configuration=value)

    def test_attached_enable_idempotence_and_complete_capabilities(self):
        session, channel = self.open()
        self.assertIsInstance(channel, mel.HealthStatusChannel)
        self.assertEqual(self.events().count("channel_attached"), 1)
        self.assertNotIn("health_enabled", self.events())
        attached = channel.capabilities()
        self.profile(attached)
        channel.enable()
        channel.enable()
        self.assertEqual(self.events().count("health_enabled"), 1)
        self.assertEqual(channel.capabilities(), attached)
        channel.close()
        channel.close()
        session.close()
        self.assertFalse(channel.is_open)
        events = self.events()
        for first, second in zip(("health_disabled", "channel_detached", "health_channel_destroyed",
                                  "control_destroyed", "manager_destroyed"),
                                 ("channel_detached", "health_channel_destroyed", "control_destroyed",
                                  "manager_destroyed", "library_unloaded")):
            self.assertLess(events.index(first), events.index(second))
        self.profile(attached)  # Actual DSO unload has been observed.

    def test_close_without_enable_does_not_disable(self):
        session, channel = self.open()
        channel.close()
        session.close()
        self.assertNotIn("health_disabled", self.events())
        self.assertEqual(self.events().count("channel_detached"), 1)
        self.assertIn("library_unloaded", self.events())

    def test_enable_failure_is_terminal_but_closable(self):
        session, channel = self.open("health-enable-fail")
        view = channel.channel_view()
        self.addCleanup(view.close)
        self.error(mel.ErrorKind.PROVIDER_FAILED, channel.enable, _native.AMS_MEL_PROVIDER_FAILED,
                   "Health enable failed")
        self.assertTrue(channel.is_open)
        for operation in (channel.enable, channel.capabilities):
            self.error(mel.ErrorKind.PROVIDER_FAILED, operation, _native.AMS_MEL_PROVIDER_FAILED)
        # Native view conversion is weak-wrapper creation, not a provider operation;
        # even if a wrapper is created in Failed state, its operations must fail.
        with channel.channel_view() as failed_view:
            self.expired(failed_view)
        self.expired(view)
        self.assertEqual(self.events().count("health_enabled"), 1)
        channel.close()
        session.close()
        self.assertEqual(self.events().count("health_disabled"), 1)
        self.assertIn("health_channel_destroyed", self.events())
        self.assertIn("library_unloaded", self.events())

    def test_session_close_first_and_no_python_parent_reference(self):
        session = mel.Session.open(provider(), "health-capability", "")
        channel = session.open_health_channel(config())
        self.addCleanup(channel.close)
        reference = weakref.ref(session)
        session.close()
        del session
        gc.collect()
        self.assertIsNone(reference())
        self.assertNotIn("control_destroyed", self.events())
        self.profile(channel.capabilities())
        channel.enable()
        channel.close()
        self.assertIn("library_unloaded", self.events())

    def test_weak_views_do_not_retain_owner_or_provider(self):
        for explicit in (False, True):
            with self.subTest(explicit=explicit):
                session = mel.Session.open(provider(), "health-capability", "")
                channel = session.open_health_channel(config())
                session_ref, channel_ref = weakref.ref(session), weakref.ref(channel)
                view = channel.channel_view()
                self.addCleanup(view.close)
                self.assertFalse(any(item is channel for item in gc.get_referents(view)))
                session.close()
                if explicit:
                    channel.close()
                del channel, session
                gc.collect()
                self.assertIsNone(session_ref())
                self.assertIsNone(channel_ref())
                self.assertIn("library_unloaded", self.events())
                before = self.events()
                self.expired(view)
                view.close()
                self.assertEqual(self.events(), before)

    def test_view_close_leaves_typed_owner_operational(self):
        _, channel = self.open()
        first, second = channel.channel_view(), channel.channel_view()
        self.addCleanup(second.close)
        first.close()
        first.close()
        self.assertFalse(first.is_open)
        self.assertTrue(channel.is_open)
        channel.enable()
        self.profile(channel.capabilities())
        self.assertEqual(second.capabilities(), channel.capabilities())

    def test_inherited_services_attached_and_enabled(self):
        _, channel = self.open("health-ada-common")
        with channel.channel_view() as view:
            for enabled in (False, True):
                if enabled:
                    channel.enable()
                self.assertEqual(view.capabilities(), channel.capabilities())
                keepalive = view.send_keepalive()
                self.assertIsInstance(keepalive, mel.ReturnRequest)
                self.assertEqual(keepalive.wait(5000), common.KEEPALIVE_SUCCESS)
                self.assertEqual(keepalive.wait(0), common.KEEPALIVE_SUCCESS)
                common.assert_owned_graph(self, keepalive.wait(0))
                keepalive.close()
                request = view.submit_comms_test(common.HIGH)
                try:
                    self.assertIsInstance(request, mel.CommsRequest)
                    self.assertEqual(request.wait(5000), common.HIGH_REPORT)
                    self.assertEqual(request.wait(0), common.HIGH_REPORT)
                finally:
                    request.close()
        quiesce()
        self.assertEqual(self.events().count("keepalive_sent"), 2)
        self.assertEqual(self.events().count("comms_sent"), 2)
        self.assertFalse(hasattr(channel, "send_keepalive"))
        self.assertFalse(hasattr(channel, "submit_comms_test"))

    def test_common_send_exceptions_preserve_category_and_owner(self):
        for scenario, method, message in (
                ("health-keepalive-throw", lambda v: v.send_keepalive(), "Health KeepAlive send failed"),
                ("health-comms-throw", lambda v: v.submit_comms_test(common.HIGH), "Health Comms send failed")):
            session, channel = self.open(scenario)
            with channel.channel_view() as view:
                self.error(mel.ErrorKind.PROVIDER_EXCEPTION, lambda: method(view),
                           _native.AMS_MEL_PROVIDER_EXCEPTION, message)
            channel.enable()
            self.profile(channel.capabilities())
            quiesce()
            channel.close()
            session.close()

    def test_pending_common_requests_survive_parent_closure(self):
        self.subprocess('''
import ctypes, _ctypes, gc, weakref
import test_ir_health as t
from test_common_channel import HIGH, HIGH_REPORT, KEEPALIVE_SUCCESS
case = t.HealthTests()
case.setUp()
try:
    library = ctypes.CDLL(str(t.provider()))
    gate = library.mock_completion_gate
    gate.argtypes = [ctypes.c_uint, ctypes.c_uint64,
                     ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)]
    gate.restype = ctypes.c_int
    session = t.mel.Session.open(t.provider(), "health-common", "")
    health = session.open_health_channel(t.config())
    health.enable()
    view = health.channel_view()
    keepalive, comms = view.send_keepalive(), view.submit_comms_test(HIGH)
    case.assertEqual(gate(1, 2, None, None), 1)
    for request in (keepalive, comms):
        case.error(t.mel.ErrorKind.TIMEOUT, lambda: request.wait(0))
    refs = [weakref.ref(value) for value in (session, health, view)]
    view.close()
    health.close()
    session.close()
    del view, health, session
    gc.collect()
    case.assertTrue(all(ref() is None for ref in refs))
    case.assertNotIn("health_disabled", case.events())
    case.assertNotIn("health_channel_destroyed", case.events())
    case.assertEqual(gate(2, 2, None, None), 1)
    # Release only this test's dlopen reference. The native graph must still
    # own the DSO until its outstanding common requests safely complete.
    _ctypes.dlclose(library._handle)
    case.assertEqual(keepalive.wait(5000), KEEPALIVE_SUCCESS)
    case.assertEqual(comms.wait(5000), HIGH_REPORT)
    case.assertEqual(keepalive.wait(0), KEEPALIVE_SUCCESS)
    case.assertEqual(comms.wait(0), HIGH_REPORT)
    keepalive.close()
    comms.close()
    t.quiesce()
    events = case.events()
    for first, second in (("health_disabled", "channel_detached"),
                          ("channel_detached", "health_channel_destroyed"),
                          ("health_channel_destroyed", "library_unloaded")):
        case.assertLess(events.index(first), events.index(second))
    print("HEALTH_CHILD_OK")
finally:
    case.doCleanups()
''')

    def test_failed_detach_retains_handle_and_explicit_retry_succeeds(self):
        session, channel = self.open("health-detach-fail")
        channel.enable()
        with channel.channel_view() as view:
            self.error(mel.ErrorKind.PROVIDER_FAILED, channel.close, _native.AMS_MEL_PROVIDER_FAILED,
                       "Health detach failed; provider state retained")
            self.assertTrue(channel.is_open)
            session.close()
            self.assertNotIn("health_channel_destroyed", self.events())
            self.assertNotIn("library_unloaded", self.events())
            self.expired(view)
            channel.close()
            self.assertFalse(channel.is_open)
            channel.close()
        self.assertEqual(self.events().count("channel_detach_failed"), 1)
        self.assertEqual(self.events().count("channel_detached"), 1)
        self.assertEqual(self.events().count("health_disabled"), 2)
        self.assertIn("library_unloaded", self.events())

    def test_uncertain_finalization_is_one_attempt_and_preserves_graph(self):
        self.subprocess('''
import gc, sys, weakref
import test_ir_health as t
case = t.HealthTests()
case.setUp()
try:
    errors = []
    sys.unraisablehook = errors.append
    session = t.mel.Session.open(t.provider(), "health-detach-fail", "")
    health = session.open_health_channel(t.config())
    health.enable()
    view = health.channel_view()
    ref = weakref.ref(health)
    del health
    gc.collect()
    session.close()
    case.assertIsNone(ref())
    case.assertEqual(errors, [])
    events = case.events()
    case.assertEqual(events.count("channel_detach_failed"), 1)
    for absent in ("channel_detached", "health_channel_destroyed", "control_destroyed", "library_unloaded"):
        case.assertNotIn(absent, events)
    case.expired(view)
    view.close()
    case.assertEqual(case.events(), events)
    print("HEALTH_CHILD_OK")
finally:
    case.doCleanups()
''')

    def test_native_open_failures_publish_no_owner(self):
        for scenario, kind, status, message in (
                ("attach-null", mel.ErrorKind.FACTORY_FAILED, _native.AMS_MEL_FACTORY_FAILED,
                 "attachChannel returned null"),
                ("attach-throw", mel.ErrorKind.PROVIDER_EXCEPTION, _native.AMS_MEL_PROVIDER_EXCEPTION,
                 "provider exception during Health open")):
            with self.subTest(scenario=scenario):
                session = mel.Session.open(provider(), scenario, "")
                self.addCleanup(session.close)
                self.error(kind, lambda: session.open_health_channel(config()), status, message)
                session.close()
        self.assertEqual(self.events().count("channel_attached"), 2)
        self.assertEqual(self.events().count("library_unloaded"), 2)
        self.assertNotIn("health_enabled", self.events())

    def test_closed_owners_reject_before_native_calls_and_no_direct_construction(self):
        session, channel = self.open()
        channel.close()
        session.close()
        for name, operation in (("ams_mel_ir_health_enable", channel.enable),
                                ("ams_mel_ir_health_get_capabilities", channel.capabilities),
                                ("ams_mel_ir_channel_from_health", channel.channel_view),
                                ("ams_mel_ir_health_open", lambda: session.open_health_channel(config()))):
            with mock.patch.object(_native, name) as native:
                self.error(mel.ErrorKind.INVALID_ARGUMENT, operation)
                native.assert_not_called()
        with self.assertRaisesRegex(TypeError, "Session.open_health_channel"):
            mel.HealthStatusChannel()

    def test_context_manager_and_finalizer_preserve_body_exception(self):
        _, channel = self.open()
        with channel as same:
            self.assertIs(same, channel)
            self.assertNotIn("health_enabled", self.events())
        self.assertFalse(channel.is_open)
        _, channel = self.open()
        with mock.patch.object(channel, "_close_function", side_effect=KeyboardInterrupt) as close:
            with self.assertRaisesRegex(ValueError, "body"):
                with channel:
                    raise ValueError("body")
            channel.__del__()
            self.assertEqual(close.call_count, 1)
        channel.close()  # Only an explicit close can retry after best-effort failure.

    def test_health_c2_image_and_instrumentation_owners_are_independent(self):
        session, health = self.open("health-common")
        value = config()
        c2 = session.open_control_channel(mel.ControlConfig(value.channel_id, value.platform_id, value.sensor_location))
        self.addCleanup(c2.close)
        instrumentation = session.open_instrumentation_channel(
            mel.InstrumentationConfig(value.channel_id, value.platform_id, value.sensor_location))
        self.addCleanup(instrumentation.close)
        stream = session.open_image_stream(mel.ImageConfig.new(
            value.channel_id, value.platform_id, value.sensor_location, 2, 64, 2))
        self.addCleanup(stream.close)
        health.close()
        c2.enable()
        instrumentation.enable()
        self.assertEqual(instrumentation.capabilities().channel_types, (mel.ChannelType.INSTRUMENTATION,))
        with c2.channel_view() as view:
            self.assertEqual(view.capabilities().channel_types, (mel.ChannelType.COMMAND_AND_CONTROL,))
        with stream.channel_view() as view:
            self.assertIs(view.capabilities().pixel_format, mel.PixelFormat.MONO)
        self.assertNotIn("c2_channel_destroyed", self.events())
        self.assertNotIn("instrumentation_channel_destroyed", self.events())


class HealthBoundaryTests(_HealthTestCase):
    """Malformed outputs not producible by the compiled provider.

    Reuse the same lifecycle assertions and compiled mock provider.
    Real owners/snapshots are always cleaned natively.
    """

    def test_success_without_health_owner_is_protocol_error(self):
        session = mel.Session.open(provider(), "health-capability", "")
        self.addCleanup(session.close)

        def empty(session, raw, output, diagnostic, capacity, required):
            common.write_diagnostic(diagnostic, capacity, b"")
            common.set_required(required, 1)
            return _native.AMS_MEL_OK

        with mock.patch.object(_native, "ams_mel_ir_health_open", side_effect=empty):
            self.error(mel.ErrorKind.PROTOCOL_INCONSISTENCY,
                       lambda: session.open_health_channel(config()))
        self.assertNotIn("channel_attached", self.events())

    def test_open_publication_is_cleaned_on_bad_diagnostic_or_exception(self):
        native_open = _native.ams_mel_ir_health_open
        native_close = mel.HealthStatusChannel._close_function
        for mutation, kind in (("size", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("utf8", mel.ErrorKind.INVALID_UTF8),
                               ("diagnostic", mel.ErrorKind.PROTOCOL_INCONSISTENCY),
                               ("failure", mel.ErrorKind.PROVIDER_FAILED),
                               ("exception", None)):
            session = mel.Session.open(provider(), "health-capability", "")
            self.addCleanup(session.close)
            before = self.events().count("health_channel_destroyed")

            def malformed(*args):
                status = native_open(*args)
                self.assertEqual(status, _native.AMS_MEL_OK)
                if mutation == "exception":
                    raise KeyboardInterrupt("after publication")
                if mutation == "size":
                    common.set_required(args[-1], 0)
                elif mutation == "utf8":
                    common.write_diagnostic(args[-3], args[-2], b"\xff")
                    common.set_required(args[-1], 2)
                elif mutation == "diagnostic":
                    common.write_diagnostic(args[-3], args[-2], b"unexpected")
                    common.set_required(args[-1], 11)
                elif mutation == "failure":
                    return _native.AMS_MEL_PROVIDER_FAILED
                return status

            close = mock.Mock(wraps=native_close)
            with self.subTest(mutation=mutation), \
                    mock.patch.object(_native, "ams_mel_ir_health_open", side_effect=malformed), \
                    mock.patch.object(mel.HealthStatusChannel, "_close_function", staticmethod(close)):
                if kind is None:
                    with self.assertRaisesRegex(KeyboardInterrupt, "after publication"):
                        session.open_health_channel(config())
                else:
                    self.error(kind, lambda: session.open_health_channel(config()))
                gc.collect()
                self.assertEqual(close.call_count, 1)
                self.assertEqual(close.call_args.args[1:], (None, 0, None))
            self.assertEqual(self.events().count("health_channel_destroyed"), before + 1)
            session.close()

    def test_open_error_cleanup_does_not_retry_uncertain_owner(self):
        self.subprocess('''
import ctypes, gc
from unittest import mock
import test_ir_health as t
case = t.HealthTests()
case.setUp()
try:
    session = t.mel.Session.open(t.provider(), "health-detach-fail", "")
    native = t._native.ams_mel_ir_health_open
    def malformed(*args):
        status = native(*args)
        ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = 0
        return status
    with mock.patch.object(t._native, "ams_mel_ir_health_open", side_effect=malformed):
        case.error(t.mel.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: session.open_health_channel(t.config()))
    gc.collect()
    session.close()
    case.assertEqual(case.events().count("channel_detach_failed"), 1)
    case.assertNotIn("channel_detached", case.events())
    case.assertNotIn("library_unloaded", case.events())
    print("HEALTH_CHILD_OK")
finally:
    case.doCleanups()
''')

    def test_consumed_close_failure_is_closed_and_never_retried(self):
        _, channel = self.open()
        native = channel._close_function

        def consumed(*args):
            self.assertEqual(native(*args), _native.AMS_MEL_OK)
            return _native.AMS_MEL_PROVIDER_FAILED

        close = mock.Mock(side_effect=consumed)
        channel._close_function = close
        self.error(mel.ErrorKind.PROVIDER_FAILED, channel.close, _native.AMS_MEL_PROVIDER_FAILED)
        self.assertFalse(channel.is_open)
        channel.close()
        channel.__del__()
        self.assertEqual(close.call_count, 1)

    def test_successful_close_must_consume_owner_and_normal_context_exit_raises(self):
        _, channel = self.open()

        def retains(pointer, diagnostic, capacity, required):
            common.write_diagnostic(diagnostic, capacity, b"")
            common.set_required(required, 1)
            return _native.AMS_MEL_OK

        with mock.patch.object(channel, "_close_function", side_effect=retains):
            self.error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, channel.close)
            self.assertTrue(channel.is_open)
            with self.assertRaises(mel.MelError):
                with channel:
                    pass
        channel.close()

    def test_capability_query_publication_cleanup(self):
        _, channel = self.open()
        query = _native.ams_mel_ir_health_get_capabilities
        close = _native.ams_mel_ir_channel_capability_close
        for mutation in ("null", "diagnostic"):
            def malformed(owner, output, diagnostic, capacity, required):
                if mutation == "null":
                    common.write_diagnostic(diagnostic, capacity, b"")
                    common.set_required(required, 1)
                    return _native.AMS_MEL_OK
                status = query(owner, output, diagnostic, capacity, required)
                common.set_required(required, 0)
                return status

            with self.subTest(mutation=mutation), \
                    mock.patch.object(_native, "ams_mel_ir_health_get_capabilities", side_effect=malformed), \
                    mock.patch.object(_native, "ams_mel_ir_channel_capability_close", wraps=close) as release:
                self.error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, channel.capabilities)
                self.assertEqual(release.call_count, 0 if mutation == "null" else 1)

    def test_malformed_capability_view_cleanup(self):
        _, channel = self.open()
        native_view = _native.ams_mel_ir_channel_capability_view
        native_close = _native.ams_mel_ir_channel_capability_close
        for mutation in ("null-record", "bool-odc", "bool-nuc", "null-span", "huge-span",
                         "unaligned-span", "null-image-bands", "null-inner-bands", "null-string",
                         "invalid-utf8", "exception"):
            keep = []

            def malformed(owner, output, *args):
                status = native_view(owner, output, *args)
                pointer = ctypes.cast(output, ctypes.POINTER(ctypes.POINTER(_native.IrChannelCapabilityV1)))
                raw = pointer[0].contents
                if mutation == "null-record":
                    pointer[0] = ctypes.POINTER(_native.IrChannelCapabilityV1)()
                elif mutation == "bool-odc": raw.odc_available = 2
                elif mutation == "bool-nuc": raw.nuc_available = 2
                elif mutation == "null-span":
                    raw.sensor_types.data = ctypes.POINTER(ctypes.c_uint32)()
                    raw.sensor_types.size = 1
                elif mutation == "huge-span": raw.nav_frames.size = sys.maxsize
                elif mutation == "unaligned-span":
                    raw.channel_types.data = ctypes.cast(1, ctypes.POINTER(ctypes.c_uint32))
                    raw.channel_types.size = 1
                elif mutation == "null-image-bands":
                    raw.image_bands.data = ctypes.POINTER(_native.IrImageBandV1)()
                    raw.image_bands.size = 1
                elif mutation == "null-inner-bands":
                    raw.image_bands.data[0].bands.data = ctypes.POINTER(_native.IrBandInfoV1)()
                    raw.image_bands.data[0].bands.size = 1
                elif mutation == "null-string":
                    raw.channel_id.descriptive_label.data = _native.CharPointer()
                    raw.channel_id.descriptive_label.size = 1
                elif mutation == "invalid-utf8":
                    text = ctypes.create_string_buffer(b"\xff")
                    keep.append(text)
                    raw.sensor_location.system_name = _native.StringViewV1(
                        ctypes.cast(text, _native.CharPointer), 1)
                elif mutation == "exception": raise MemoryError("conversion")
                return status

            with self.subTest(mutation=mutation), \
                    mock.patch.object(_native, "ams_mel_ir_channel_capability_view", side_effect=malformed), \
                    mock.patch.object(_native, "ams_mel_ir_channel_capability_close", wraps=native_close) as close:
                if mutation == "exception":
                    with self.assertRaisesRegex(MemoryError, "conversion"):
                        channel.capabilities()
                else:
                    self.error(mel.ErrorKind.INVALID_UTF8 if mutation == "invalid-utf8" else
                               mel.ErrorKind.PROTOCOL_INCONSISTENCY, channel.capabilities)
                self.assertEqual(close.call_count, 1)
            self.profile(channel.capabilities())

    def test_view_publication_cleanup_and_null_success(self):
        _, channel = self.open()
        convert = _native.ams_mel_ir_channel_from_health
        close = _native.ams_mel_ir_channel_close
        for mutation in ("null", "diagnostic"):
            def malformed(owner, output, diagnostic, capacity, required):
                if mutation == "null":
                    common.write_diagnostic(diagnostic, capacity, b"")
                    common.set_required(required, 1)
                    return _native.AMS_MEL_OK
                status = convert(owner, output, diagnostic, capacity, required)
                common.set_required(required, 0)
                return status

            with self.subTest(mutation=mutation), \
                    mock.patch.object(_native, "ams_mel_ir_channel_from_health", side_effect=malformed), \
                    mock.patch.object(_native, "ams_mel_ir_channel_close", wraps=close) as release:
                self.error(mel.ErrorKind.PROTOCOL_INCONSISTENCY, channel.channel_view)
                self.assertEqual(release.call_count, 0 if mutation == "null" else 1)
        self.profile(channel.capabilities())

    def test_capability_conversion_exception_closes_snapshot_without_masking(self):
        _, channel = self.open()
        native_close = _native.ams_mel_ir_channel_capability_close
        closes = []

        def cleanup(*args):
            closes.append(args[1:])
            native_close(*args)
            # Cleanup failure must not replace the original conversion error.
            raise RuntimeError("cleanup")

        with mock.patch.object(mel, "_capability_from_raw", side_effect=MemoryError("copy")), \
                mock.patch.object(_native, "ams_mel_ir_channel_capability_close", side_effect=cleanup):
            with self.assertRaisesRegex(MemoryError, "copy"):
                channel.capabilities()
        self.assertEqual(closes, [(None, 0, None)])
        self.profile(channel.capabilities())


if __name__ == "__main__":
    unittest.main()
