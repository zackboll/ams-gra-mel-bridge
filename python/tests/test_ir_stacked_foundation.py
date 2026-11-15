"""PY-MEL-016: real Stacked owners, exact ABI, deterministic lifetime gates."""

from __future__ import annotations

from contextlib import closing
import ctypes as c
import dataclasses as dc
from decimal import Decimal
from fractions import Fraction
import gc
import inspect
import json
import math
import os
from pathlib import Path
import pickle
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _ffi, _native as n, _stacked, _types
from test_common_channel import HIGH, HIGH_REPORT, assert_owned_graph
from test_ir_health import _HealthTestCase, provider, quiesce
from test_ir_navigation import diagnostic
from test_package_layout import public_snapshot


BASE = "cb33c5e5434fa27b9d9660f6a9d361ed0993e37b"
OPEN = "ams_mel_ir_stacked_open"
CLOSE = "ams_mel_ir_stacked_close"
QUERY = "ams_mel_ir_stacked_get_capabilities"
RETRY = b"Stacked detach failed; provider state retained"


def config(**changes):
    return m.StackedConfig(**(dict(
        channel_id=m.UciId(bytes(range(0xA0, 0xB0)), "IR stacked μ channel"),
        platform_id=m.UciId(bytes(range(0xF0, 0x100)), "test platform"),
        sensor_location=m.ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
    ) | changes))


def error(test, kind, operation):
    with test.assertRaises(m.MelError) as caught:
        operation()
    test.assertIs(caught.exception.kind, kind)
    return caught.exception


def no_acquisition(test, events):
    for event in ("stacked_forbidden_registration", "buffer_factory_called",
                  "buffer_registered", "buffer_initialized", "buffer_unregistered"):
        test.assertNotIn(event, events)


class StackedTests(_HealthTestCase):
    def opened(self, scenario="stacked-basic"):
        session = m.Session.open(provider(), scenario, "")
        self.addCleanup(session.close)
        channel = session.open_stacked_channel(config())
        self.addCleanup(channel.close)
        return session, channel

    def test_public_identity_frozen_pickle_and_exact_surface(self):
        for public, private in ((m.StackedConfig, _types.StackedConfig),
                                (m.StackedChannel, _stacked.StackedChannel)):
            self.assertIs(public, private)
            self.assertEqual(public.__module__, "ams_mel")
            self.assertIs(pickle.loads(pickle.dumps(public)), public)
        value = config()
        self.assertEqual(pickle.loads(pickle.dumps(value)), value)
        self.assertEqual([f.name for f in dc.fields(value)],
                         ["channel_id", "platform_id", "sensor_location"])
        with self.assertRaises(dc.FrozenInstanceError):
            value.channel_id = None
        with self.assertRaises(TypeError):
            m.StackedChannel()
        self.assertEqual({name for name in dir(m.StackedChannel) if not name.startswith("_")},
                         {"is_open", "enable", "capabilities", "channel_view", "close"})
        self.assertEqual(str(inspect.signature(m.Session.open_stacked_channel)),
                         "(self, config: 'StackedConfig') -> 'StackedChannel'")
        with mock.patch.object(n, OPEN) as opened:
            config()
            opened.assert_not_called()

    def qualify_fetched_base(self):
        """Explicit history qualification; ordinary shallow builds need no history."""
        # Compare against the fetched source, not a hand-updated expected list.
        with tempfile.TemporaryDirectory() as directory:
            archive = subprocess.check_output(["git", "-C", str(Path(__file__).resolve().parents[2]),
                                               "archive", BASE, "python"])
            subprocess.run(["tar", "-x", "-C", directory], input=archive, check=True)
            script = ("import json; from test_package_layout import public_snapshot; "
                      "print(json.dumps(public_snapshot()))")
            result = subprocess.run([sys.executable, "-W", "error", "-c", script],
                                    cwd=directory, env=dict(os.environ, PYTHONPATH=os.pathsep.join(
                                        (str(Path(directory) / "python"),
                                         str(Path(directory) / "python/tests")))),
                                    capture_output=True, text=True, timeout=30, check=True)
            baseline = json.loads(result.stdout)
        current = public_snapshot()
        self.assertEqual(set(current["all"]) - set(baseline["all"]),
                         {"StackedConfig", "StackedChannel"})
        self.assertEqual([v for v in current["all"] if v in baseline["all"]], baseline["all"])
        self.assertEqual(current["aliases"], baseline["aliases"])

        def compare(candidate):
            for name, contract in baseline["symbols"].items():
                actual = dict(candidate["symbols"][name])
                if name == "Session":
                    actual["methods"] = dict(actual["methods"])
                    del actual["methods"]["open_stacked_channel"]
                self.assertEqual(actual, contract, name)
        compare(current)
        import copy
        broken = copy.deepcopy(current)
        broken["symbols"]["Session"]["methods"]["capabilities"] = "broken"
        with self.assertRaises(AssertionError):
            compare(broken)

    def test_exact_record_storage_survives_gc_and_open_called_once(self):
        session, unused = self.opened("stacked-config")
        unused.close()
        value = config(sensor_location=m.ComponentLocation(-0.0, float("inf"),
                         float("nan"), "位置 β", ""),
                       platform_id=m.UciId(bytes(range(240, 256)), ""))
        original = getattr(n, OPEN)

        def inspect_open(*args):
            gc.collect()
            raw = c.cast(args[1], c.POINTER(n.IrStackedConfigV1)).contents
            self.assertEqual([name for name, _ in raw._fields_],
                             ["channel_id", "channel_type", "platform_id", "sensor_location"])
            self.assertEqual(raw.channel_type, 6)
            self.assertEqual(bytes(raw.channel_id.uuid), value.channel_id.uuid)
            self.assertEqual(bytes(raw.platform_id.uuid), value.platform_id.uuid)
            for view, text in ((raw.channel_id.descriptive_label, "IR stacked μ channel"),
                               (raw.platform_id.descriptive_label, ""),
                               (raw.sensor_location.key, "位置 β"),
                               (raw.sensor_location.system_name, "")):
                self.assertEqual(view.size, len(text.encode()))
                self.assertEqual(c.string_at(view.data, view.size), text.encode())
            self.assertEqual(struct.pack("d", raw.sensor_location.offset_x_m), struct.pack("d", -0.0))
            self.assertEqual(raw.sensor_location.offset_y_m, float("inf"))
            self.assertTrue(math.isnan(raw.sensor_location.offset_z_m))
            # The strict config fixture checks its own fixed values; submit the
            # inspected record to an ordinary session instead, never fabricate owners.
            return original(other._owner, *args[1:])

        with m.Session.open(provider(), "stacked-basic", "") as other:
            with mock.patch.object(n, OPEN, side_effect=inspect_open) as opened:
                with session.open_stacked_channel(value):
                    pass
                opened.assert_called_once()
        for number in (-0.0, 0, Fraction(1, 8), Decimal("1.25"), float("-inf")):
            bundle = _ffi._RawStackedConfig(config(sensor_location=m.ComponentLocation(
                number, 0, 0, "", "")))
            self.assertEqual(struct.pack("d", bundle.raw.sensor_location.offset_x_m),
                             struct.pack("d", float(number)))

    def test_invalid_nested_and_mutated_records_rejected_before_native(self):
        session, _ = self.opened()
        mutations = [("config", "channel_id", None), ("config", "platform_id", object()),
                     ("config", "sensor_location", {}), ("channel_id", "uuid", b"short"),
                     ("platform_id", "uuid", bytearray(16)),
                     ("channel_id", "uuid", "x" * 16)]
        for label in (None, b"bytes", "bad\0label", "bad\ud800"):
            mutations.extend((target, field, label) for target, field in (
                ("channel_id", "descriptive_label"), ("platform_id", "descriptive_label"),
                ("sensor_location", "key"), ("sensor_location", "system_name")))
        for number in (True, "1.0", None, object(), complex(1, 2), 10 ** 400, Decimal("1e9999")):
            mutations.extend(("sensor_location", field, number) for field in
                             ("offset_x_m", "offset_y_m", "offset_z_m"))
        with mock.patch.object(n, OPEN) as opened:
            for target, field, value in mutations:
                with self.subTest(target=target, field=field, value=repr(value)):
                    record = config()
                    object.__setattr__(record if target == "config" else getattr(record, target), field, value)
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_stacked_channel(record))
                    error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: m.StackedConfig(
                        record.channel_id, record.platform_id, record.sensor_location))
            for target, field in (("config", "channel_id"), ("channel_id", "uuid"),
                                  ("sensor_location", "offset_x_m")):
                record = config()
                object.__delattr__(record if target == "config" else getattr(record, target), field)
                error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_stacked_channel(record))
            for record in (None, object(), {}, m.ImageControlConfig(
                    config().channel_id, config().platform_id, config().sensor_location)):
                error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_stacked_channel(record))
            session.close()
            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: session.open_stacked_channel(config()))
            opened.assert_not_called()

    def test_noncanonical_nested_models_and_invalid_construction(self):
        class DerivedId(m.UciId):
            pass
        class DerivedLocation(m.ComponentLocation):
            pass
        for changes in (
                dict(channel_id=DerivedId(bytes(16), "")),
                dict(sensor_location=DerivedLocation(0, 0, 0, "", "")),
                dict(channel_id=None), dict(platform_id=object()),
                dict(sensor_location=m.ComponentLocation(10 ** 400, 0, 0, "", ""))):
            error(self, m.ErrorKind.INVALID_ARGUMENT, lambda: config(**changes))

    def test_capability_fidelity_typed_weak_and_owned(self):
        for scenario, depth in (("stacked-config", 23), ("stacked-zero", 23),
                                ("stacked-depth-zero", 0), ("stacked-depth-max", 0xFFFFFFFF)):
            with self.subTest(scenario=scenario):
                session, stacked = self.opened(scenario)
                discovered = session.capabilities()[0]
                a, b = stacked.channel_view(), stacked.channel_view()
                self.addCleanup(a.close)
                self.addCleanup(b.close)
                capability = stacked.capabilities()
                self.assertEqual(capability, discovered)
                self.assertEqual(a.capabilities(), capability)
                self.assertEqual(stacked.capabilities(), capability)
                self.assertIsNot(stacked.capabilities(), capability)
                self.assertEqual(capability.task_schedule_depth, depth)
                self.assertEqual(capability.channel_types,
                                 (m.ChannelType.STACKED_IMAGE, m.ChannelType.RESERVED_1))
                self.assertEqual(capability.channel_id.descriptive_label, "channel-α")
                self.assertEqual(capability.platform_id.descriptive_label, "platform-€")
                self.assertEqual(capability.sensor_location.system_name, "system-β")
                if scenario == "stacked-zero":
                    for field in ("height", "width", "bit_depth", "row_pitch", "buffer_size",
                                  "image_size", "number_of_bands"):
                        self.assertEqual(getattr(capability, field), 0)
                    self.assertEqual(capability.image_bands, ())
                    self.assertEqual(capability.nav_frames, ())
                else:
                    self.assertEqual((capability.height, capability.width, capability.bit_depth,
                                      capability.row_pitch, capability.buffer_size, capability.image_size,
                                      capability.number_of_bands, capability.pixel_format),
                                     (1080, 1920, 12, 4096, 8388608, 4147200, 3, m.PixelFormat.RGB))
                    self.assertEqual(tuple(v.band_index for v in capability.image_bands), (2, 9))
                    self.assertEqual(len(capability.image_bands[0].bands), 2)
                    self.assertEqual(len(capability.nav_frames), 2)
                a.close()
                stacked.enable()
                stacked.enable()
                session.close()
                self.assertEqual(b.capabilities(), capability)
                stacked.close()
                error(self, m.ErrorKind.PROVIDER_FAILED, b.capabilities)
                assert_owned_graph(self, capability)
                self.assertEqual(pickle.loads(pickle.dumps(capability)), capability)
        no_acquisition(self, self.events())

    def test_hostile_open_outputs_and_construction_failure_cleanup_once(self):
        session = m.Session.open(provider(), "stacked-basic", "")
        self.addCleanup(session.close)
        original, native_close = getattr(n, OPEN), getattr(n, CLOSE)
        for case in ("null", "failure", "malformed", "unknown", "construction", "cleanup-throws"):
            with self.subTest(case=case):
                owners = []
                def opened(*args):
                    if case == "null":
                        return diagnostic(args)
                    status = original(*args)
                    self.assertEqual(status, n.AMS_MEL_OK)
                    owners.append(n.IrStackedHandle(c.cast(args[2], c.POINTER(n.IrStackedHandle))[0]))
                    if case == "malformed":
                        return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"\xff")
                    if case in ("failure", "cleanup-throws"):
                        return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"primary")
                    if case == "unknown":
                        return diagnostic(args, -9876, b"unknown")
                    return status
                cleanup = mock.Mock(wraps=native_close)
                if case == "cleanup-throws":
                    cleanup.side_effect = RuntimeError("cleanup")
                with mock.patch.object(n, OPEN, side_effect=opened) as call, \
                     mock.patch.object(n, CLOSE, cleanup), \
                     mock.patch.object(m.StackedChannel, "_from_owner",
                                       side_effect=MemoryError("construction")):
                    with self.assertRaises(BaseException) as caught:
                        session.open_stacked_channel(config())
                    call.assert_called_once()
                    self.assertEqual(cleanup.call_count, int(case != "null"))
                    if case == "construction":
                        self.assertIsInstance(caught.exception, MemoryError)
                    if case == "cleanup-throws":
                        self.assertEqual(caught.exception.diagnostic, "primary")
                if case == "cleanup-throws":
                    native_close(c.byref(owners[0]), None, 0, None)

    def test_capability_failure_published_snapshot_and_weak_conversion_failure(self):
        _, stacked = self.opened()
        original = getattr(n, QUERY)
        def query(*args):
            self.assertEqual(original(*args), n.AMS_MEL_OK)
            return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"snapshot failure")
        with mock.patch.object(n, QUERY, side_effect=query), mock.patch.object(
                n, "ams_mel_ir_channel_capability_close", wraps=n.ams_mel_ir_channel_capability_close) as close:
            error(self, m.ErrorKind.PROVIDER_FAILED, stacked.capabilities)
            close.assert_called_once()
        convert = n.ams_mel_ir_channel_from_stacked
        def conversion(*args):
            self.assertEqual(convert(*args), n.AMS_MEL_OK)
            return diagnostic(args, n.AMS_MEL_PROVIDER_EXCEPTION, b"view failure")
        with mock.patch.object(n, "ams_mel_ir_channel_from_stacked", side_effect=conversion), \
             mock.patch.object(n, "ams_mel_ir_channel_close", wraps=n.ams_mel_ir_channel_close) as close:
            error(self, m.ErrorKind.PROVIDER_EXCEPTION, stacked.channel_view)
            close.assert_called_once()

    def test_capability_unknown_enums_duplicates_and_empty_collections(self):
        _, stacked = self.opened()
        original = n.ams_mel_ir_channel_capability_view
        types = (c.c_uint32 * 4)(6, 0xFFFFFFFF, 6, 8)
        sensors = (c.c_uint32 * 3)(0x80000000, 0x80000000, 0)
        def changed(*args):
            status = original(*args)
            raw = c.cast(args[1], c.POINTER(c.POINTER(n.IrChannelCapabilityV1)))[0].contents
            raw.channel_types.data, raw.channel_types.size = types, len(types)
            raw.sensor_types.data, raw.sensor_types.size = sensors, len(sensors)
            raw.metadata_capabilities.size = 0
            raw.image_bands.size = 0
            raw.nav_frames.size = 0
            raw.pixel_format = 0xFFFFFFFF
            return status
        with mock.patch.object(n, "ams_mel_ir_channel_capability_view", side_effect=changed):
            value = stacked.capabilities()
        self.assertEqual(tuple(map(int, value.channel_types)), (6, 0xFFFFFFFF, 6, 8))
        self.assertEqual(tuple(map(int, value.sensor_types)), (0x80000000, 0x80000000, 0))
        self.assertEqual(int(value.pixel_format), 0xFFFFFFFF)
        self.assertEqual((value.metadata_capabilities, value.image_bands, value.nav_frames), ((), (), ()))
        assert_owned_graph(self, value)

    def test_common_ready_attached_enabled_and_multiple_weak_views(self):
        session, stacked = self.opened("stacked-ada-common")
        with stacked.channel_view() as a, stacked.channel_view() as b:
            for enabled in (False, True):
                if enabled:
                    stacked.enable()
                with closing(a.send_keepalive()) as request:
                    self.assertEqual(request.wait(1000), m.ReturnCompleted(m.CommandReturn.SUCCESS))
                with closing(b.submit_comms_test(HIGH)) as request:
                    self.assertEqual(request.wait(1000), HIGH_REPORT)
            a.close()
            stacked.capabilities()
        quiesce()
        session.close()
        stacked.close()
        self.assertEqual(self.events().count("stacked_enabled"), 1)
        no_acquisition(self, self.events())

    def test_documented_consumer_against_compiled_mock(self):
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-016-stacked-foundation.md"
        source = document.read_text().split("```python\n", 1)[1].split("```", 1)[0]
        namespace = {}
        exec(compile(source, str(document), "exec"), namespace)
        pin = c.CDLL(str(provider()))
        release = pin.mock_completion_release_family
        release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
        wait = n.ams_mel_ir_return_request_wait
        def released_wait(*args):
            self.assertEqual(release(13, 1), 1)
            return wait(*args)
        with mock.patch.object(n, "ams_mel_ir_return_request_wait", side_effect=released_wait):
            result = namespace["fourth_channel"](provider(), "common-meta-parity", 0xF0000002)
        self.assertEqual(result[1], m.ReturnCompleted(m.CommandReturn.SUCCESS))
        self.assertEqual(result[2], HIGH_REPORT.report)
        self.assertEqual(result[3], HIGH_REPORT)
        quiesce()
        import _ctypes
        _ctypes.dlclose(pin._handle)
        no_acquisition(self, self.events())

    def test_close_preparation_failure_leaves_owner_live(self):
        _, stacked = self.opened()
        with mock.patch.object(_stacked, "_call_with_diagnostic", side_effect=MemoryError("before call")), \
             mock.patch.object(stacked, "_close_function", wraps=stacked._close_function) as close:
            with self.assertRaises(MemoryError):
                stacked.close()
            close.assert_not_called()
        self.assertTrue(stacked.is_open)
        stacked.capabilities()
        stacked.close()
        stacked.close()

    def test_common_metadata_real_callbacks_attached_enabled_fifo_and_one_shot(self):
        for enabled in (False, True):
            session, stacked = self.opened("common-meta-held")
            if enabled:
                stacked.enable()
            view = stacked.channel_view()
            self.addCleanup(view.close)
            with view.open_comms_metadata() as stream:
                # Existing Stacked common fixture uses its family-14 gate.
                pin = c.CDLL(str(provider()))
                release = pin.mock_completion_release_family
                release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
                with closing(view.submit_comms_test(HIGH)) as request:
                    self.assertEqual(stream.receive(1000), HIGH_REPORT.report)
                    error(self, m.ErrorKind.TIMEOUT, lambda: request.wait(0))
                    view.close()  # metadata does not borrow its Python weak view
                    self.assertEqual(release(14, 1), 1)
                    self.assertEqual(request.wait(1000), HIGH_REPORT)
                quiesce()
                import _ctypes
                _ctypes.dlclose(pin._handle)
            with stacked.channel_view() as other:
                error(self, m.ErrorKind.PROVIDER_FAILED, other.open_comms_metadata)
            stacked.close()
            session.close()
        for scenario, received, dropped in (("sync", 1, 0), ("overflow", 6, 4)):
            session, stacked = self.opened("common-meta-" + scenario)
            with stacked.channel_view() as view, view.open_comms_metadata(2) as stream:
                expected = m.CommsMetadataCounters(received, dropped, 0)
                self.assertEqual(stream.counters(), expected)
                reports = ([m.CommsTestReport(0xABCDEF01, 0xFEDCBA98)] if scenario == "sync" else
                           [m.CommsTestReport(0x80000000 + i, 0xF0000000 + i) for i in range(2)])
                for report in reports:
                    self.assertEqual(stream.receive(), report)
                    self.assertEqual(stream.counters(), expected)
                error(self, m.ErrorKind.TIMEOUT, stream.receive)
            stacked.close()
            session.close()


def child(scenario):
    """Bounded subprocess scenarios for retention, callbacks and actual DSO unload."""
    test = unittest.TestCase()
    with tempfile.TemporaryDirectory(prefix="py016-") as directory:
        log = Path(directory) / "lifetime"
        os.environ["AMS_MEL_TEST_LIFETIME_LOG"] = str(log)
        barrier = Path(directory) / "callback"
        os.environ["AMS_MEL_TEST_COMMS_BARRIER"] = str(barrier)
        events = lambda: log.read_text().splitlines()
        failures = {
            "stacked-not-advertised": m.ErrorKind.INITIALIZATION_FAILED,
            "stacked-empty": m.ErrorKind.INITIALIZATION_FAILED,
            "stacked-discovery-throw": m.ErrorKind.PROVIDER_EXCEPTION,
            "stacked-attach-null": m.ErrorKind.FACTORY_FAILED,
            "stacked-attach-throw": m.ErrorKind.PROVIDER_EXCEPTION,
            "stacked-wrong-concrete": m.ErrorKind.INITIALIZATION_FAILED,
            "stacked-capability-wrong": m.ErrorKind.INITIALIZATION_FAILED,
            "stacked-capability-throw": m.ErrorKind.INITIALIZATION_FAILED,
            "stacked-open-detach-fail": m.ErrorKind.PROVIDER_FAILED,
            "stacked-capability-throw-detach-fail": m.ErrorKind.PROVIDER_FAILED,
        }
        if scenario in failures:
            with m.Session.open(provider(), scenario, "") as session:
                with mock.patch.object(n, OPEN, wraps=getattr(n, OPEN)) as call:
                    failure = error(test, failures[scenario], lambda: session.open_stacked_channel(config()))
                    test.assertIsNotNone(failure.native_status)
                    call.assert_called_once()
                test.assertTrue(session.provider_version().vendor)
                if scenario not in ("stacked-empty", "stacked-discovery-throw"):
                    import test_control_channel as control
                    with session.open_control_channel(control.ControlChannelTests.config()):
                        pass
            retained = "detach-fail" in scenario
            test.assertEqual("library_unloaded" in events(), not retained)
            no_acquisition(test, events())
            return

        native_scenario = scenario if scenario.startswith(("stacked-", "common-meta-")) else "stacked-basic"
        if scenario == "context-error":
            native_scenario = "stacked-disable-throw"
        if scenario.startswith("hostile-"):
            native_scenario = "stacked-basic"
        session = m.Session.open(provider(), native_scenario, "", max_async_requests=2)
        stacked = session.open_stacked_channel(config())
        view = stacked.channel_view()

        if scenario.startswith("hostile-"):
            case = scenario.removeprefix("hostile-")
            native_close = stacked._close_function
            def close(*args):
                if case == "throws":
                    raise RuntimeError("uncertain")
                if case == "malformed":
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"\xff")
                if case == "unknown":
                    return diagnostic(args, -9876, b"unknown")
                if case == "undocumented":
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"not the detach contract")
                if case == "consumed":
                    test.assertEqual(native_close(*args), n.AMS_MEL_OK)
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"consumed failure")
                if case == "changed":
                    # Another REAL owner, never a fabricated address.
                    c.cast(args[0], c.POINTER(n.IrStackedHandle))[0] = second._owner
                    return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, RETRY)
                return diagnostic(args)  # success retaining owner is invalid
            second = session.open_stacked_channel(config()) if case == "changed" else None
            with mock.patch.object(stacked, "_close_function", side_effect=close) as call:
                if case == "finalizer":
                    call.side_effect = RuntimeError("finalizer")
                    stacked.__del__()
                    stacked.__del__()
                    test.assertEqual(call.call_count, 1)
                    session.close()
                    test.assertNotIn("library_unloaded", events())
                    return
                with test.assertRaisesRegex(ValueError, "application"):
                    with stacked:
                        with test.assertRaises(BaseException):
                            stacked.close()
                        raise ValueError("application")
                stacked.__del__()
                stacked.__del__()
                test.assertEqual(call.call_count, 1)
                error(test, m.ErrorKind.INVALID_ARGUMENT, stacked.enable)
                if stacked.is_open:
                    error(test, m.ErrorKind.PROTOCOL_INCONSISTENCY, stacked.close)
            session.close()
            if case != "consumed":
                test.assertNotIn("library_unloaded", events())
                # Intentionally retain uncertain real owners until subprocess exit.
                if second:
                    second._close_state = "uncertain"
            return

        if scenario in ("stacked-detach-fail", "stacked-detach-throw", "stacked-enabled-detach-fail"):
            stacked.enable()
            original = stacked._owner.value
            failure = error(test, m.ErrorKind.PROVIDER_FAILED, stacked.close)
            test.assertEqual(failure.diagnostic, RETRY.decode())
            test.assertEqual(stacked._owner.value, original)
            for operation in (stacked.enable, stacked.capabilities, stacked.channel_view):
                error(test, m.ErrorKind.INVALID_ARGUMENT, operation)
            error(test, m.ErrorKind.PROVIDER_FAILED, view.capabilities)
            stacked.__del__()  # must not retry a documented retained owner either
            stacked.__exit__(None, None, None)
            test.assertEqual(events().count("channel_detach_failed"), 1)
            test.assertNotIn("library_unloaded", events())
            stacked.close()  # only an explicit retry
            test.assertFalse(stacked.is_open)
            test.assertEqual(events().count("stacked_disabled"), 1)
        elif scenario in ("stacked-enable-fail", "stacked-enable-throw"):
            kind = m.ErrorKind.PROVIDER_EXCEPTION if scenario.endswith("throw") else m.ErrorKind.PROVIDER_FAILED
            error(test, kind, stacked.enable)
            for operation in (stacked.enable, stacked.capabilities, view.capabilities, view.send_keepalive):
                error(test, m.ErrorKind.PROVIDER_FAILED, operation)
            test.assertEqual(events().count("stacked_enabled"), 1)
            stacked.close()
        elif scenario in ("stacked-disable-fail", "stacked-disable-throw", "context-error"):
            stacked.enable()
            if scenario == "context-error":
                with test.assertRaisesRegex(ValueError, "application"):
                    with stacked:
                        raise ValueError("application")
            else:
                error(test, m.ErrorKind.PROVIDER_FAILED, stacked.close)
            test.assertFalse(stacked.is_open)
            stacked.close()
            stacked.__del__()
            test.assertEqual(events().count("stacked_disabled"), 1)
        elif scenario in ("pending", "abandoned", "stacked-deferred-detach-fail", "stacked-return-fail", "accounting"):
            pin = c.CDLL(str(provider()))
            release = pin.mock_completion_release_family
            release.argtypes, release.restype = [c.c_uint, c.c_uint], c.c_int
            ret, comms = view.send_keepalive(), view.submit_comms_test(HIGH)
            for request in (ret, comms):
                error(test, m.ErrorKind.TIMEOUT, lambda: request.wait(0))
            error(test, m.ErrorKind.RESOURCE_EXHAUSTED, view.send_keepalive)
            if scenario == "accounting":
                other = session.open_stacked_channel(config())
                observe = n._LIBRARY.ams_mel_test_stacked_requests
                observe.argtypes, observe.restype = [n.IrStackedHandle, c.POINTER(c.c_size_t)], c.c_int
                for channel, expected in ((stacked, 2), (other, 0)):
                    count = c.c_size_t()
                    test.assertEqual(observe(channel._owner, c.byref(count)), 1)
                    test.assertEqual(count.value, expected)
                with other.channel_view() as v:
                    error(test, m.ErrorKind.RESOURCE_EXHAUSTED, v.send_keepalive)
                other.close()
            channel_ref, session_ref = weakref.ref(stacked), weakref.ref(session)
            stacked.close()
            session.close()
            del stacked, session
            gc.collect()
            test.assertIsNone(channel_ref())
            test.assertIsNone(session_ref())
            test.assertEqual(events().count("channel_detached"), int(scenario == "accounting"))
            if scenario == "abandoned":
                ret.close()
                comms.close()
            test.assertEqual(release(14, 1), 1)
            if scenario != "abandoned":
                test.assertEqual(comms.wait(5000), HIGH_REPORT)
                # Retire only Comms; waiting for Return here would deadlock its
                # deliberately held promise. Make final cleanup ownership exact.
                owner = n._LIBRARY.ams_mel_test_completion_owner
                owner.argtypes, owner.restype = [c.c_uint, c.c_uint64, c.POINTER(c.c_uint64)], c.c_int
                observed = c.c_uint64()
                test.assertEqual(owner(2, 1, c.byref(observed)), 1)
            test.assertEqual(release(13, 1), 1)
            if scenario != "abandoned":
                expected = m.CommandReturn.FAIL if scenario == "stacked-return-fail" else m.CommandReturn.SUCCESS
                if scenario == "stacked-deferred-detach-fail":
                    error(test, m.ErrorKind.PROVIDER_FAILED, lambda: ret.wait(5000))
                else:
                    test.assertEqual(ret.wait(5000), m.ReturnCompleted(expected))
                ret.close()
                comms.close()
            quiesce()
            import _ctypes
            _ctypes.dlclose(pin._handle)
            retained = scenario == "stacked-deferred-detach-fail"
            test.assertEqual("library_unloaded" in events(), not retained)
            no_acquisition(test, events())
            return
        elif scenario.startswith("common-meta-"):
            case = scenario.removeprefix("common-meta-")
            if case in ("refuse", "fail", "partial", "throw"):
                error(test, m.ErrorKind.PROVIDER_EXCEPTION if case == "throw" else m.ErrorKind.PROVIDER_FAILED,
                      view.open_comms_metadata)
                error(test, m.ErrorKind.PROVIDER_FAILED, view.open_comms_metadata)
                test.assertEqual(events().count("common_comms_registration"), 1)
            else:
                stream = view.open_comms_metadata()
                refs = [weakref.ref(v) for v in (session, stacked, view)]
                view.close()
                del view
                if case == "async":
                    # Existing provider gate, not a timing assumption. Its
                    # producer must be released before destructor joins it.
                    stream.close()
                    Path(str(barrier) + ".start").touch()
                stacked.close()
                session.close()
                del stacked, session
                gc.collect()
                test.assertTrue(all(ref() is None for ref in refs))
                test.assertIn("library_unloaded", events())
                if case == "sync":
                    test.assertEqual(stream.receive(), m.CommsTestReport(0xABCDEF01, 0xFEDCBA98))
                    error(test, m.ErrorKind.STREAM_STOPPED, stream.receive)
                if case == "async":
                    test.assertIn("common_comms_callback_returned", events())
                stream.close()
                return
            stacked.close()
        elif scenario == "weak-gc":
            snapshot = stacked.capabilities()
            refs = [weakref.ref(v) for v in (stacked, session)]
            del stacked, session
            gc.collect()
            test.assertTrue(all(ref() is None for ref in refs))
            test.assertIn("library_unloaded", events())
            assert_owned_graph(test, snapshot)
            error(test, m.ErrorKind.PROVIDER_FAILED, view.capabilities)
            return
        elif scenario == "session-gc":
            ref = weakref.ref(session)
            del session
            gc.collect()
            test.assertIsNone(ref())
            stacked.capabilities()
            stacked.enable()
            test.assertNotIn("library_unloaded", events())
            stacked.close()
            test.assertIn("library_unloaded", events())
            return
        elif scenario == "stacked-keepalive-throw":
            error(test, m.ErrorKind.PROVIDER_EXCEPTION, view.send_keepalive)
            stacked.close()
        else:
            session.close()
            stacked.capabilities()
            stacked.close()
            test.assertNotIn("stacked_disabled", events())
        session.close()
        view.close()
        test.assertIn("library_unloaded", events())
        ordered = ["channel_detached", "stacked_channel_destroyed", "control_destroyed",
                   "manager_destroyed", "library_unloaded"]
        test.assertEqual([events().index(v) for v in ordered], sorted(events().index(v) for v in ordered))
        no_acquisition(test, events())


class StackedIsolatedTests(unittest.TestCase):
    def isolated(self, scenario):
        result = subprocess.run([sys.executable, "-W", "error", "-c",
                                 "from test_ir_stacked_foundation import child; child(" + repr(scenario) + ")"],
                                env=dict(os.environ, PYTHONPATH=os.pathsep.join(sys.path)),
                                capture_output=True, text=True, timeout=45)
        self.assertEqual(result.returncode, 0, scenario + "\n" + result.stdout + result.stderr)


def _scenario_test(scenario):
    def test(self):
        self.isolated(scenario)
    return test


for _scenario in (
    "stacked-not-advertised", "stacked-empty", "stacked-discovery-throw", "stacked-attach-null",
    "stacked-attach-throw", "stacked-wrong-concrete", "stacked-capability-wrong",
    "stacked-capability-throw", "stacked-open-detach-fail", "stacked-capability-throw-detach-fail",
    "stacked-detach-fail", "stacked-detach-throw", "stacked-enabled-detach-fail",
    "stacked-enable-fail", "stacked-enable-throw", "stacked-disable-fail", "stacked-disable-throw",
    "stacked-close-before-enable", "stacked-keepalive-throw", "stacked-return-fail",
    "pending", "abandoned", "accounting", "stacked-deferred-detach-fail", "weak-gc", "session-gc", "context-error",
    "hostile-throws", "hostile-malformed", "hostile-unknown", "hostile-undocumented",
    "hostile-consumed", "hostile-success-retained", "hostile-changed", "hostile-finalizer",
    "common-meta-refuse", "common-meta-fail", "common-meta-partial", "common-meta-throw",
    "common-meta-sync", "common-meta-async",
):
    setattr(StackedIsolatedTests, "test_" + _scenario.replace("-", "_"), _scenario_test(_scenario))


if __name__ == "__main__":
    if sys.argv[1:] == ["--baseline"]:
        StackedTests().qualify_fetched_base()
        print("PASS: fetched-base API delta and failing negative control")
    elif sys.argv[1:] == ["--reference"]:
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-016-stacked-foundation.md"
        source = document.read_text().split("```python\n")[2].split("```", 1)[0]
        namespace = {}
        exec(compile(source, str(document), "exec"), namespace)
        print(namespace["reference_fourth_channel"](os.environ["AMS_MEL_REFERENCE_PROVIDER"]))
        print("PASS: unchanged reference provider, including actual callback before future Wait")
    else:
        unittest.main()
