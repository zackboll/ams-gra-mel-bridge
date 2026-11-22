"""PY-MEL-027: Track lifecycle with the compiled mock and hostile FFI results."""

from contextlib import ExitStack
import copy
import ctypes as c
from dataclasses import FrozenInstanceError, fields, replace
from decimal import Decimal
import gc
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
from ams_mel import _ffi, _native as n, _session, _track
from test_common_channel import assert_owned_graph
import test_control_channel as c2_tests
from test_ir_health import provider
from test_ir_navigation import diagnostic


OPEN = "ams_mel_ir_track_open"
CLOSE = "ams_mel_ir_track_close"
ENABLE = "ams_mel_ir_track_enable"
QUERY = "ams_mel_ir_track_get_capabilities"
VIEW = "ams_mel_ir_channel_from_track"
CAP_VIEW = "ams_mel_ir_channel_capability_view"
CAP_CLOSE = "ams_mel_ir_channel_capability_close"
RETAINED = b"Track detach failed; provider state retained"
INVALID = m.ErrorKind.INVALID_ARGUMENT
PROTOCOL = m.ErrorKind.PROTOCOL_INCONSISTENCY
FAILED = m.ErrorKind.PROVIDER_FAILED
EXCEPTION = m.ErrorKind.PROVIDER_EXCEPTION
DESTROY = ["channel_detached", "track_channel_destroyed", "control_destroyed",
           "manager_destroyed", "library_unloaded"]
FORBIDDEN_EVENTS = {
    "track_report_registered", "track_request_system_track_data_registered",
    "track_candidate_object_registered", "track_candidate_object_preproc_registered",
    "track_data_update_sent", "track_system_track_data_response_sent",
}


def config():
    return m.TrackConfig.new(
        m.UciId(bytes(range(16)), "IR track channel"),
        m.UciId(bytes(range(0xF0, 0x100)), "test platform"),
        m.ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
    )


def error(test, kind, call, status=None, text=None):
    with test.assertRaises(m.MelError) as caught:
        call()
    value = caught.exception
    test.assertIs(value.kind, kind)
    test.assertEqual(value.native_status, status)
    if text is not None:
        test.assertEqual(value.diagnostic, text)


class TrackConfigTests(unittest.TestCase):
    def test_constructor_is_frozen_and_provider_free(self):
        with mock.patch.object(n, OPEN) as attach, mock.patch.object(n, ENABLE) as enable:
            value = config()
        attach.assert_not_called()
        enable.assert_not_called()
        self.assertEqual([field.name for field in fields(value)],
                         ["channel_id", "platform_id", "sensor_location"])
        self.assertEqual(value, pickle.loads(pickle.dumps(value)))
        with self.assertRaises(FrozenInstanceError):
            value.platform_id = None
        with self.assertRaises(TypeError):
            m.TrackChannel()
        self.assertEqual({name for name in dir(m.TrackChannel) if not name.startswith("_")},
                         {"is_open", "enable", "capabilities", "channel_view", "close"})

    def test_invalid_construction_and_mutation_never_reach_open(self):
        cases = [(None, name, object()) for name in ("channel_id", "platform_id", "sensor_location")]
        for name in ("channel_id", "platform_id"):
            cases.extend((name, "uuid", uuid) for uuid in (b"short", bytes(17), bytearray(16)))
            cases.extend((name, "descriptive_label", label) for label in (None, "a\0b", "\ud800"))
        for name in ("key", "system_name"):
            cases.extend(("sensor_location", name, label) for label in (None, "\0", "\udfff"))
        for name in ("offset_x_m", "offset_y_m", "offset_z_m"):
            cases.extend(("sensor_location", name, offset) for offset in (
                True, "1.0", None, complex(1, 2), 10**400, Decimal("1e9999")))
        with m.Session.open(provider(), "track-config", "") as session, mock.patch.object(n, OPEN) as opened:
            for field in (None, {}, object(), m.StackedConfig(**vars(config()))):
                error(self, INVALID, lambda: session.open_track_channel(field))
            for target, name, bad in cases:
                with self.subTest(target=target, field=name, bad=bad):
                    value = config()
                    object.__setattr__(getattr(value, target) if target else value, name, bad)
                    error(self, INVALID, lambda: replace(value))
                    error(self, INVALID, lambda: session.open_track_channel(value))
            for target, name in ((None, "platform_id"), ("channel_id", "uuid"),
                                 ("platform_id", "descriptive_label"), ("sensor_location", "offset_z_m")):
                value = config()
                object.__delattr__(getattr(value, target) if target else value, name)
                error(self, INVALID, lambda: session.open_track_channel(value))
            session.close()
            error(self, INVALID, lambda: session.open_track_channel(config()))
            opened.assert_not_called()

    def test_validation_and_encoding_use_the_same_captured_values(self):
        value = config()
        encode = _ffi._RawTrackConfig._id

        def mutate_then_encode(bundle, identity):
            object.__setattr__(value.platform_id, "uuid", b"wrong length")
            object.__setattr__(value.sensor_location, "offset_x_m", object())
            return encode(bundle, identity)

        with m.Session.open(provider(), "track-config", "") as session:
            with mock.patch.object(_ffi._RawTrackConfig, "_id", mutate_then_encode):
                session.open_track_channel(value).close()  # Mock verifies every original field.
            error(self, INVALID, lambda: session.open_track_channel(value))

    def test_primitive_subclasses_cannot_forge_validation_or_encoding(self):
        class ForgedBytes(bytes):
            def __len__(self):
                return 16

        class ForgedText(str):
            def __contains__(self, value):
                return False

            def encode(self, *args, **kwargs):
                return b"not the actual string"

        with m.Session.open(provider(), "track-config", "") as session:
            value = config()
            object.__setattr__(value.channel_id, "descriptive_label", ForgedText("IR track channel"))
            session.open_track_channel(value).close()
            with mock.patch.object(n, OPEN) as opened:
                object.__setattr__(value.channel_id, "descriptive_label", ForgedText("hidden\0NUL"))
                error(self, INVALID, lambda: session.open_track_channel(value))
                value = config()
                object.__setattr__(value.channel_id, "uuid", ForgedBytes(b"short"))
                error(self, INVALID, lambda: session.open_track_channel(value))
                opened.assert_not_called()


class TrackCapabilityTests(unittest.TestCase):
    def test_complete_config_and_owned_capability_without_track_callbacks_or_sends(self):
        forbidden = ("ams_mel_ir_track_metadata_open", "ams_mel_ir_track_submit_update",
                     "ams_mel_ir_track_submit_system_track_data_response")
        expected = m.ChannelCapability(
            channel_id=m.UciId(bytes(0x11 + 7 * i for i in range(16)), "channel-α"),
            height=1080, width=1920, bit_depth=12, row_pitch=4096, buffer_size=8388608,
            image_size=4147200, number_of_bands=3, pixel_format=m.PixelFormat.RGB,
            sensor_types=(m.SensorType.GIMBAL_HORIZONTAL, m.SensorType.STEP_STARE),
            platform_id=m.UciId(bytes(0x31 + 7 * i for i in range(16)), "platform-€"),
            sensor_location=m.ComponentLocation(1.25, -2.5, 3.75, "sensor-key", "system-β"),
            channel_types=(m.ChannelType.IRST_TRACK,), task_schedule_depth=17,
            odc_available=True, nuc_available=True,
            metadata_capabilities=(m.MetadataCapability.IRST_TRACK_REPORT,
                                   m.MetadataCapability.CHANNEL_COMMS_TEST_REP),
            image_bands=(m.ImageBand(2, (m.BandInfo(m.BandType.IR_LONGWAVE, 8e-6, 12e-6),
                                         m.BandInfo(m.BandType.IR_MIDWAVE, 3e-6, 5e-6))),
                         m.ImageBand(9, (m.BandInfo(m.BandType.VISIBLE_RED, 620e-9, 750e-9),))),
            nav_frames=(m.CoordinateSystem.NED_SENSOR, m.CoordinateSystem.ECEF),
        )
        with ExitStack() as stack:
            traps = [stack.enter_context(mock.patch.object(n, name, side_effect=AssertionError(name)))
                     for name in forbidden]
            enabled = stack.enter_context(mock.patch.object(n, ENABLE, wraps=getattr(n, ENABLE)))
            released = stack.enter_context(mock.patch.object(n, CAP_CLOSE, wraps=getattr(n, CAP_CLOSE)))
            with m.Session.open(provider(), "track-config", "") as session:
                with session.open_track_channel(config()) as track:
                    before = track.capabilities()
                    enabled.assert_not_called()
                    track.enable()
                    track.enable()
                    after = track.capabilities()
                self.assertFalse(track.is_open)
            self.assertEqual(released.call_count, 2)
            for trap in traps:
                trap.assert_not_called()
        self.assertIsNot(before, after)
        self.assertEqual(before, expected)
        self.assertEqual(after, expected)
        for value in (before, after):
            self.assertIs(type(value), m.ChannelCapability)
            assert_owned_graph(self, value)

    def test_track_and_c2_owners_are_independent(self):
        with m.Session.open(provider(), "completion-scale", "") as session:
            with session.open_control_channel(c2_tests.ControlChannelTests.config()) as c2:
                c2.enable()
                with session.open_track_channel(config()) as track:
                    track.enable()
                    self.assertEqual(track.capabilities().channel_types, (m.ChannelType.IRST_TRACK,))
                self.assertTrue(c2.is_open)
                with c2.channel_view() as view:
                    self.assertIn(m.ChannelType.COMMAND_AND_CONTROL, view.capabilities().channel_types)

    def test_unique_owner_cannot_be_copied_or_pickled(self):
        with m.Session.open(provider(), "track-lifetime", "") as session:
            with session.open_track_channel(config()) as track:
                for operation in (copy.copy, copy.deepcopy, pickle.dumps):
                    with self.assertRaises(TypeError):
                        operation(track)


class TrackLifetimeTests(unittest.TestCase):
    def isolated(self, case):
        # An emergency root or orphan is permanent in-process. Fresh interpreters
        # also make the DSO unload assertions independent of other Python tests.
        with tempfile.TemporaryDirectory(prefix="py-track-") as directory:
            log = Path(directory) / "lifetime.log"
            result = subprocess.run([sys.executable, "-W", "error", __file__, case],
                                    env=dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(log)),
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            events = log.read_text(encoding="utf-8").splitlines()
            self.assertTrue(FORBIDDEN_EVENTS.isdisjoint(events), events)
            return events

    def test_lifetime_orders_and_weak_views(self):
        for scenario in ("track-config", "track-lifetime", "track-parent-first", "weak-gc",
                         "track-no-enable", "finalize-live"):
            with self.subTest(scenario=scenario):
                events = self.isolated(scenario)
                expected = ([] if scenario == "track-no-enable" else ["track_enabled", "track_disabled"]) + DESTROY
                self.assertEqual(events.count("channel_attached"), 1)
                self.assertEqual([event for event in events if event in set(expected)], expected)
                if scenario == "track-no-enable":
                    self.assertNotIn("track_enabled", events)
                    self.assertNotIn("track_disabled", events)

    def test_open_failures_keep_native_taxonomy_and_do_not_retry(self):
        for scenario in ("attach-null", "attach-throw", "track-attach-null", "track-wrong-concrete",
                         "track-capability-wrong", "track-capability-throw", "track-open-detach-fail"):
            with self.subTest(scenario=scenario):
                events = self.isolated(scenario)
                self.assertEqual(events.count("channel_attached"), 1)
                self.assertNotIn("track_enabled", events)
                if scenario == "track-open-detach-fail":
                    self.assertEqual(events.count("channel_detach_failed"), 1)
                    self.assertNotIn("channel_detached", events)
                    self.assertNotIn("control_destroyed", events)
                else:
                    rolled_back = scenario in ("track-wrong-concrete", "track-capability-wrong",
                                               "track-capability-throw")
                    self.assertEqual(events.count("channel_detached"), int(rolled_back))

    def test_failed_enable_is_terminal(self):
        for scenario in ("track-enable-fail", "track-enable-throw"):
            with self.subTest(scenario=scenario):
                events = self.isolated(scenario)
                order = ["track_enabled", "track_disabled"] + DESTROY
                self.assertEqual([event for event in events if event in set(order)], order)

    def test_consumed_disable_error_and_context_primary(self):
        for scenario in ("track-disable-fail", "context-primary"):
            with self.subTest(scenario=scenario):
                events = self.isolated(scenario)
                order = ["track_enabled", "track_disabled"] + DESTROY
                self.assertEqual([event for event in events if event in set(order)], order)

    def test_retained_detach_requires_explicit_retry(self):
        events = self.isolated("track-detach-fail")
        order = ["track_enabled", "track_disabled", "channel_detach_failed", "track_disabled"] + DESTROY
        self.assertEqual([event for event in events if event in set(order)], order)
        events = self.isolated("finalize-retained")
        finalizer_events = self.isolated("finalize-detach-failure")
        for observed in (events, finalizer_events):
            self.assertEqual(observed.count("channel_detach_failed"), 1)
            self.assertEqual(observed.count("track_disabled"), 1)
            self.assertTrue(set(DESTROY[:-1]).isdisjoint(observed))


def _lifetime_case(test, case):
    open_errors = {
        "attach-null": (m.ErrorKind.FACTORY_FAILED, n.AMS_MEL_FACTORY_FAILED, "attachChannel returned null"),
        "attach-throw": (EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION, "provider exception during Track open"),
        "track-attach-null": (m.ErrorKind.FACTORY_FAILED, n.AMS_MEL_FACTORY_FAILED, "attachChannel returned null"),
        "track-wrong-concrete": (m.ErrorKind.INITIALIZATION_FAILED, n.AMS_MEL_INITIALIZATION_FAILED,
                                 "attached channel is not a TrackChannel"),
        "track-capability-wrong": (m.ErrorKind.INITIALIZATION_FAILED, n.AMS_MEL_INITIALIZATION_FAILED,
                                   "Track capability omits IRSTTrack"),
        "track-capability-throw": (m.ErrorKind.INITIALIZATION_FAILED, n.AMS_MEL_INITIALIZATION_FAILED,
                                   "provider exception during Track open"),
        "track-open-detach-fail": (FAILED, n.AMS_MEL_PROVIDER_FAILED, "incompatible Track channel and detach failed"),
    }
    scenario = {"weak-gc": "track-parent-first", "finalize-live": "track-lifetime",
                "finalize-retained": "track-detach-fail", "finalize-detach-failure": "track-detach-fail",
                "context-primary": "track-disable-fail"}.get(case, case)
    log = Path(os.environ["AMS_MEL_TEST_LIFETIME_LOG"])
    session = m.Session.open(provider(), scenario, "")
    if case in open_errors:
        kind, status, text = open_errors[case]
        with mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
            error(test, kind, lambda: session.open_track_channel(config()), status, text)
        close.assert_not_called()
        test.assertTrue(session.is_open)
        session.close()
        return

    with mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as close:
        track = session.open_track_channel(config())
        if case.startswith("track-enable-"):
            kind, status, text = ((FAILED, n.AMS_MEL_PROVIDER_FAILED, "Track enable failed")
                                  if case.endswith("fail") else
                                  (EXCEPTION, n.AMS_MEL_PROVIDER_EXCEPTION, "provider exception during Track enable"))
            error(test, kind, track.enable, status, text)
            error(test, FAILED, track.enable, n.AMS_MEL_PROVIDER_FAILED, "Track channel is not attached")
            error(test, FAILED, track.capabilities, n.AMS_MEL_PROVIDER_FAILED)
            track.close()
            session.close()
            return

        before = track.capabilities()
        test.assertNotIn("track_enabled", log.read_text(encoding="utf-8").splitlines())
        if case != "track-no-enable":
            track.enable()
            track.enable()
        if case == "track-config":
            test.assertEqual(before, track.capabilities())
        if case == "finalize-detach-failure":
            ref = weakref.ref(track)
            del track
            gc.collect()
            test.assertIsNone(ref())
            close.assert_called_once()
            session.close()
            test.assertNotIn("control_destroyed", log.read_text(encoding="utf-8").splitlines())
            return
        if case == "context-primary":
            with test.assertRaisesRegex(RuntimeError, "primary"):
                with track:
                    raise RuntimeError("primary")
            test.assertFalse(track.is_open)
            track.close()
        elif case == "track-disable-fail":
            error(test, FAILED, track.close, n.AMS_MEL_PROVIDER_FAILED, "Track disable failed")
            test.assertFalse(track.is_open)
            track.close()
        elif case in ("track-detach-fail", "finalize-retained"):
            with track:
                error(test, FAILED, track.close, n.AMS_MEL_PROVIDER_FAILED, RETAINED.decode())
                test.assertFalse(track.is_open)
                with ExitStack() as stack:
                    for method, name in (("enable", ENABLE), ("capabilities", QUERY), ("channel_view", VIEW)):
                        native = stack.enter_context(mock.patch.object(n, name))
                        error(test, INVALID, getattr(track, method))
                        native.assert_not_called()
            close.assert_called_once()  # Context exit must not retry.
            session.close()
            test.assertNotIn("control_destroyed", log.read_text(encoding="utf-8").splitlines())
            if case == "track-detach-fail":
                with mock.patch.object(c, "create_string_buffer", side_effect=MemoryError):
                    with test.assertRaises(MemoryError):
                        track.close()
                test.assertFalse(track.is_open)
                close.assert_called_once()
                track.close()
                track.close()
                test.assertEqual(close.call_count, 2)
            else:
                ref = weakref.ref(track)
                del track
                gc.collect()
                test.assertIsNone(ref())
                close.assert_called_once()
            return
        else:
            view = track.channel_view()
            test.assertIs(type(view), m.ChannelView)
            test.assertEqual(view.capabilities(), before)
            view.close()
            test.assertTrue(track.is_open)
            test.assertEqual(track.capabilities(), before)
            view = track.channel_view()
            if case == "weak-gc":
                ref = weakref.ref(session)
                del session
                gc.collect()
                test.assertIsNone(ref())
            elif case == "track-parent-first":
                session.close()
            test.assertEqual(track.capabilities(), before)
            test.assertNotIn("control_destroyed", log.read_text(encoding="utf-8").splitlines())
            track_ref = weakref.ref(track)
            if case not in ("weak-gc", "finalize-live"):
                track.close()
                track.close()
            del track
            gc.collect()
            test.assertIsNone(track_ref())
            if case != "weak-gc":
                session.close()
            test.assertIn("library_unloaded", log.read_text(encoding="utf-8").splitlines())
            test.assertTrue(view.is_open)
            error(test, FAILED, view.capabilities, n.AMS_MEL_PROVIDER_FAILED)
            error(test, FAILED, view.send_keepalive, n.AMS_MEL_PROVIDER_FAILED)
            error(test, FAILED, lambda: view.submit_comms_test(m.CommsTestRequest(1, 2, 3)),
                  n.AMS_MEL_PROVIDER_FAILED)
            view.close()
            test.assertEqual(before.channel_id.descriptive_label, "channel-α")
            assert_owned_graph(test, before)
            test.assertFalse(view.is_open)
            close.assert_called_once()
            return
        session.close()
        close.assert_called_once()


class TrackHostileFfiTests(unittest.TestCase):
    def session(self):
        session = m.Session.open(provider(), "track-lifetime", "")
        self.addCleanup(session.close)
        return session

    def test_open_requires_a_valid_non_null_owner(self):
        session = self.session()
        with mock.patch.object(n, OPEN, side_effect=lambda *args: diagnostic(args)), \
                mock.patch.object(n, CLOSE) as close:
            error(self, PROTOCOL, lambda: session.open_track_channel(config()))
            for invalid in (object(), n.IrTrackHandle(1)):
                error(self, PROTOCOL, lambda: _track.TrackChannel._prepare_open(invalid))
        close.assert_not_called()

    def test_unexpected_owner_cleanup_preserves_primary_and_never_retries(self):
        session = self.session()
        native_open, native_close = getattr(n, OPEN), getattr(n, CLOSE)
        for mode in ("failure", "exception", "malformed", "interrupted-publication"):
            with self.subTest(mode=mode), ExitStack() as stack:
                def open_track(*args):
                    self.assertEqual(native_open(*args), n.AMS_MEL_OK)
                    if mode == "exception":
                        raise RuntimeError("raised after publication")
                    if mode == "failure":
                        return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"unexpected publication")
                    if mode == "malformed":
                        return diagnostic(args, n.AMS_MEL_OK, b"bad success diagnostic")
                    return n.AMS_MEL_OK

                def close_track(*args):
                    self.assertEqual(native_close(*args), n.AMS_MEL_OK)
                    # Return an uncertain stale pointer after actual consumption.
                    c.cast(args[0], c.POINTER(n.IrTrackHandle))[0] = 1
                    raise RuntimeError("cleanup error must not mask primary")

                opened = stack.enter_context(mock.patch.object(n, OPEN, side_effect=open_track))
                closed = stack.enter_context(mock.patch.object(n, CLOSE, side_effect=close_track))
                if mode == "interrupted-publication":
                    publish = _track.TrackChannel._publish_open

                    def interrupted(track):
                        publish(track)
                        raise MemoryError

                    stack.enter_context(mock.patch.object(_track.TrackChannel, "_publish_open", interrupted))
                    with self.assertRaises(MemoryError):
                        session.open_track_channel(config())
                elif mode == "exception":
                    with self.assertRaisesRegex(RuntimeError, "raised after publication"):
                        session.open_track_channel(config())
                elif mode == "failure":
                    error(self, FAILED, lambda: session.open_track_channel(config()),
                          n.AMS_MEL_PROVIDER_FAILED, "unexpected publication")
                else:
                    error(self, PROTOCOL, lambda: session.open_track_channel(config()))
                gc.collect()
                opened.assert_called_once()
                closed.assert_called_once()

    def test_pre_invocation_allocation_failures_do_not_change_ownership(self):
        session = self.session()
        for target, attribute in ((_session, "_RawTrackConfig"), (n, "IrTrackHandle"),
                                  (c, "pointer"), (c, "create_string_buffer")):
            with self.subTest(attribute=attribute), mock.patch.object(n, OPEN) as opened, \
                    mock.patch.object(target, attribute, side_effect=MemoryError):
                with self.assertRaises(MemoryError):
                    session.open_track_channel(config())
                opened.assert_not_called()
        with mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as closed:
            track = session.open_track_channel(config())
            for attribute in ("create_string_buffer", "byref"):
                with mock.patch.object(c, attribute, side_effect=MemoryError):
                    with self.assertRaises(MemoryError):
                        track.close()
                self.assertTrue(track.is_open)
                closed.assert_not_called()
            self.assertEqual(track.capabilities().channel_types, (m.ChannelType.IRST_TRACK,))
            track.close()
            closed.assert_called_once()

    def test_malformed_capability_is_rejected_and_released(self):
        with self.session().open_track_channel(config()) as track:
            native_view = getattr(n, CAP_VIEW)

            def malformed(owner, output, *args):
                result = native_view(owner, output, *args)
                raw = c.cast(output, c.POINTER(c.POINTER(n.IrChannelCapabilityV1)))[0].contents
                raw.image_bands.data[0].bands.data = c.POINTER(n.IrBandInfoV1)()
                return result

            with mock.patch.object(n, CAP_VIEW, side_effect=malformed), \
                    mock.patch.object(n, CAP_CLOSE, wraps=getattr(n, CAP_CLOSE)) as release:
                error(self, PROTOCOL, track.capabilities)
                release.assert_called_once()
            self.assertTrue(track.capabilities().image_bands)

    def test_failed_capability_query_with_published_snapshot_releases_it(self):
        with self.session().open_track_channel(config()) as track:
            query = getattr(n, QUERY)

            def failed(*args):
                self.assertEqual(query(*args), n.AMS_MEL_OK)
                return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"failed after snapshot allocation")

            with mock.patch.object(n, QUERY, side_effect=failed), \
                    mock.patch.object(n, CAP_CLOSE, wraps=getattr(n, CAP_CLOSE)) as release:
                error(self, FAILED, track.capabilities, n.AMS_MEL_PROVIDER_FAILED,
                      "failed after snapshot allocation")
                release.assert_called_once()

    def test_capability_close_failure_is_not_reported_as_a_snapshot_success(self):
        with self.session().open_track_channel(config()) as track:
            native_close = getattr(n, CAP_CLOSE)

            def failed(*args):
                self.assertEqual(native_close(*args), n.AMS_MEL_OK)
                return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"snapshot close failed after consumption")

            with mock.patch.object(n, CAP_CLOSE, side_effect=failed) as close:
                error(self, m.ErrorKind.INTERNAL_ERROR, track.capabilities, n.AMS_MEL_INTERNAL_ERROR,
                      "snapshot close failed after consumption")
                close.assert_called_once()

    def test_uncertain_close_outputs_never_authorize_another_call(self):
        session = self.session()
        native_close = getattr(n, CLOSE)
        for mode in ("success-stale", "wrong-status", "wrong-diagnostic", "malformed-diagnostic",
                     "mismatched-pointer", "null-pointer", "arbitrary-exception", "spoofed-mel-error"):
            with self.subTest(mode=mode):
                def unexpected(*args):
                    output = c.cast(args[0], c.POINTER(n.IrTrackHandle))
                    address = output[0]
                    self.assertEqual(native_close(*args), n.AMS_MEL_OK)
                    output[0] = (1 if mode == "mismatched-pointer" else
                                 None if mode == "null-pointer" else address)
                    if mode == "arbitrary-exception":
                        raise RuntimeError("arbitrary exception after consumption")
                    if mode == "spoofed-mel-error":
                        raise m.MelError(FAILED, RETAINED.decode(), len(RETAINED) + 1, n.AMS_MEL_PROVIDER_FAILED)
                    if mode == "success-stale":
                        return diagnostic(args)
                    if mode == "wrong-status":
                        return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, RETAINED)
                    text = {"wrong-diagnostic": RETAINED + b" ",
                            "malformed-diagnostic": RETAINED + b"\0suffix"}.get(mode, RETAINED)
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, text)

                with mock.patch.object(n, CLOSE, side_effect=unexpected) as close:
                    track = session.open_track_channel(config())
                    ref = weakref.ref(track)
                    with track:
                        if mode == "arbitrary-exception":
                            with self.assertRaisesRegex(RuntimeError, "arbitrary exception"):
                                track.close()
                        elif mode in ("success-stale", "malformed-diagnostic"):
                            error(self, PROTOCOL, track.close)
                        elif mode == "wrong-status":
                            error(self, m.ErrorKind.INTERNAL_ERROR, track.close, n.AMS_MEL_INTERNAL_ERROR)
                        else:
                            error(self, FAILED, track.close, n.AMS_MEL_PROVIDER_FAILED)
                        self.assertFalse(track.is_open)
                    if mode == "null-pointer":
                        track.close()
                    else:
                        error(self, PROTOCOL, track.close)
                    del track
                    gc.collect()
                    self.assertIsNone(ref())
                    close.assert_called_once()


if __name__ == "__main__":
    _lifetime_case(unittest.TestCase(), sys.argv[1])
