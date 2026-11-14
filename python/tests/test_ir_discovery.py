"""PY-MEL-013: complete Control snapshots and one-attempt native cleanup."""

from __future__ import annotations

from contextlib import ExitStack, contextmanager
import ctypes
import dataclasses
import gc
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

import ams_mel as m
from ams_mel import _discovery, _ffi, _native as n
from test_common_channel import assert_owned_graph


CREATE = "ams_mel_ir_capability_list_create"
COUNT = "ams_mel_ir_capability_list_count"
GET = "ams_mel_ir_capability_list_get"
VIEW = "ams_mel_ir_channel_capability_view"
ENTRY_CLOSE = "ams_mel_ir_channel_capability_close"
LIST_CLOSE = "ams_mel_ir_capability_list_close"
NAMES = (CREATE, COUNT, GET, VIEW, ENTRY_CLOSE, LIST_CLOSE)
TYPES = (m.ChannelType.COMMAND_AND_CONTROL, m.ChannelType.HEALTH_AND_STATUS,
         m.ChannelType.IRST_IMAGE, m.ChannelType.STACKED_IMAGE)


def uuid(seed):
    return bytes((seed + 7 * i) & 255 for i in range(16))


def expected_rich(index):
    """Independent literals from the C test and rich_channel_capability mock."""
    return m.ChannelCapability(
        m.UciId(uuid(0x10 + index), f"discovery-{index}-α"),
        1080, 1920, 12, 4096, 8388608, 4147200, 3, m.PixelFormat.RGB,
        (m.SensorType.GIMBAL_HORIZONTAL, m.SensorType.STEP_STARE),
        m.UciId(uuid(0x31), "platform-€"),
        m.ComponentLocation(1.25, -2.5, 3.75, "sensor-key", "system-β"),
        (TYPES[index], m.ChannelType.INSTRUMENTATION, m.ChannelType.RESERVED_2),
        17, True, True,
        (m.MetadataCapability.BAD_PIXEL_LIST, m.MetadataCapability.COMMAND_STATUS,
         m.MetadataCapability.CHANNEL_COMMS_TEST_REP, m.MetadataCapability.RESERVED_10),
        (m.ImageBand(2, (m.BandInfo(m.BandType.IR_LONGWAVE, 8e-6, 12e-6),
                         m.BandInfo(m.BandType.IR_MIDWAVE, 3e-6, 5e-6))),
         m.ImageBand(9, (m.BandInfo(m.BandType.VISIBLE_RED, 620e-9, 750e-9),))),
        (m.CoordinateSystem.NED_SENSOR, m.CoordinateSystem.ECEF),
    )


def diagnostic(args, status=n.AMS_MEL_OK, text=b""):
    buffer, capacity, required = args[-3:]
    if required is not None:
        ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        ctypes.memmove(buffer, text + b"\0", min(capacity, len(text) + 1))
    return status


def owner_slot(argument):
    return ctypes.cast(argument, ctypes.POINTER(ctypes.c_void_p)).contents


class NativeCalls:
    """Intercept the actual mock-backed ABI; no fabricated owner addresses.

    An ownership-uncertain injected Close is rescued explicitly by the test,
    outside the production operation. Consumed-but-uncleared owners are never
    rescued or dereferenced a second time.
    """

    def __init__(self):
        self.real = {name: getattr(n, name) for name in NAMES}
        self.calls = {name: 0 for name in NAMES}
        self.events = []
        self.hooks = {}
        self.rescue = []

    def invoke(self, name, args):
        self.calls[name] += 1
        self.events.append(name)
        if name in self.hooks:
            return self.hooks[name](args)
        return self.real[name](*args)

    @contextmanager
    def installed(self):
        try:
            with ExitStack() as stack:
                for name in NAMES:
                    stack.enter_context(mock.patch.object(
                        n, name, side_effect=lambda *args, name=name: self.invoke(name, args)
                    ))
                yield self
        finally:
            for close, address in self.rescue:
                owner = ctypes.c_void_p(address)
                close(ctypes.byref(owner), None, 0, None)


class DiscoveryTests(unittest.TestCase):
    def open(self, scenario="discovery-rich"):
        provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        return m.Session.open(provider, scenario, "")

    def test_zero_layout_and_order(self):
        with self.open("discovery-zero") as session:
            values = session.capabilities()
        self.assertIs(type(values), tuple)
        self.assertEqual(len(values), 4)
        self.assertEqual(tuple(int(t) for t in TYPES), (2, 4, 1, 6))
        for i, value in enumerate(values):
            self.assertEqual(value.channel_id, m.UciId(uuid(0x10 + i), f"discovery-{i}-α"))
            self.assertEqual(value.channel_types, (TYPES[i],))
            self.assertEqual((value.height, value.width, value.bit_depth, value.row_pitch,
                              value.buffer_size, value.image_size, value.number_of_bands),
                             (0,) * 7)
            assert_owned_graph(self, value)

    def test_rich_complete_graph_and_public_identity(self):
        from ams_mel._types import ChannelCapability
        with self.open() as session:
            values = session.capabilities()
        self.assertEqual(values, tuple(expected_rich(i) for i in range(4)))
        self.assertIs(m.ChannelCapability, ChannelCapability)
        for value in values:
            self.assertIs(type(value), ChannelCapability)
            self.assertEqual(pickle.loads(pickle.dumps(value)), value)
            assert_owned_graph(self, value)
            with self.assertRaises(dataclasses.FrozenInstanceError):
                value.width = 7
        self.assertEqual([name for name in m.__all__ if "ChannelCapability" in name],
                         ["ChannelCapability"])

    def test_empty(self):
        with self.open("discovery-empty") as session, NativeCalls().installed() as calls:
            self.assertEqual(session.capabilities(), ())
        self.assertEqual(calls.calls, dict(zip(NAMES, (1, 1, 0, 0, 0, 1))))

    def test_repeated_discovery_closes_before_return(self):
        with self.open() as session, NativeCalls().installed() as calls:
            first = session.capabilities()
            self.assertEqual(calls.calls, dict(zip(NAMES, (1, 1, 4, 4, 4, 1))))
            second = session.capabilities()
            self.assertEqual(first, second)
            self.assertEqual(first, tuple(expected_rich(i) for i in range(4)))
            self.assertIsNot(first[0], second[0])
            self.assertEqual(calls.calls, dict(zip(NAMES, (2, 2, 8, 8, 8, 2))))
            self.assertEqual(calls.events[:15], [CREATE, COUNT] +
                             [GET, VIEW, ENTRY_CLOSE] * 4 + [LIST_CLOSE])

    def test_closed_session_never_creates_list(self):
        session = self.open()
        session.close()
        with mock.patch.object(n, CREATE) as create:
            with self.assertRaises(m.MelError) as caught:
                session.capabilities()
            self.assertEqual(caught.exception.kind, m.ErrorKind.INVALID_ARGUMENT)
            create.assert_not_called()

    def test_provider_failures_never_return_partial_results(self):
        cases = (
            ("malformed", m.ErrorKind.PROVIDER_FAILED, "provider returned malformed ChannelCapability", 11),
            ("bad-label", m.ErrorKind.PROVIDER_FAILED, "provider returned malformed ChannelCapability", 11),
            ("throw", m.ErrorKind.PROVIDER_EXCEPTION, "mock discovery exception", 6),
            ("unknown-throw", m.ErrorKind.PROVIDER_EXCEPTION, "unknown provider capability exception", 6),
            ("bad-alloc", m.ErrorKind.INTERNAL_ERROR, "capability list allocation failed", 8),
        )
        for scenario, kind, text, status in cases:
            with self.subTest(scenario=scenario), self.open("discovery-" + scenario) as session:
                with NativeCalls().installed() as calls:
                    with self.assertRaises(m.MelError) as caught:
                        session.capabilities()
                error = caught.exception
                self.assertEqual((error.kind, error.diagnostic, error.native_status,
                                  error.diagnostic_required),
                                 (kind, text, status, len(text.encode()) + 1))
                self.assertEqual(calls.calls, dict(zip(NAMES, (1, 0, 0, 0, 0, 0))))

    def test_provider_unload_in_isolated_process(self):
        # No observer dlopen may pin the DSO. Probe only after Session Close.
        script = '''
import ctypes, os, pickle
from pathlib import Path
from ams_mel import Session
from test_ir_discovery import expected_rich, assert_owned_graph
import unittest
provider = str(Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so")
with Session.open(provider, "discovery-rich", "") as session:
    values = session.capabilities()
assert "library_unloaded" in Path(os.environ["AMS_MEL_TEST_LIFETIME_LOG"]).read_text()
dl = ctypes.CDLL(None)
dl.dlopen.argtypes = [ctypes.c_char_p, ctypes.c_int]
dl.dlopen.restype = ctypes.c_void_p
dl.dlclose.argtypes = [ctypes.c_void_p]
dl.dlclose.restype = ctypes.c_int
loaded = dl.dlopen(os.fsencode(provider), os.RTLD_NOW | os.RTLD_NOLOAD)
if loaded:
    dl.dlclose(loaded)
assert not loaded, "provider still loaded"
assert values == tuple(expected_rich(i) for i in range(4))
assert_owned_graph(unittest.TestCase(), values)
assert pickle.loads(pickle.dumps(values)) == values
events = Path(os.environ["AMS_MEL_TEST_LIFETIME_LOG"]).read_text().splitlines()
assert events == ["manager_factory_called", "control_factory_called", "init_called",
                  "discovery_called", "control_destroyed", "manager_destroyed", "library_unloaded"], events
print("PROVIDER_UNLOAD_CONFIRMED; complete owned graph inspected")
'''
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, "-W", "error", "-c", script],
                env={**os.environ, "AMS_MEL_TEST_LIFETIME_LOG": str(Path(directory) / "life"),
                     "PYTHONPATH": os.pathsep.join((str(Path(__file__).resolve().parents[1]),
                                                  str(Path(__file__).resolve().parent)))},
                capture_output=True, text=True, timeout=30,
            )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PROVIDER_UNLOAD_CONFIRMED", result.stdout)


class HostileDiscoveryTests(unittest.TestCase):
    open = DiscoveryTests.open

    def check_failure(self, calls, kind=m.ErrorKind.PROVIDER_FAILED, exception=m.MelError):
        with self.open() as session, calls.installed():
            with self.assertRaises(exception) as caught:
                session.capabilities()
            gc.collect()
        if exception is m.MelError:
            self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def test_null_successful_owners_and_view(self):
        for name, expected in ((CREATE, (1, 0, 0, 0, 0, 0)),
                               (GET, (1, 1, 1, 0, 0, 1)),
                               (VIEW, (1, 1, 1, 1, 1, 1))):
            with self.subTest(name=name):
                calls = NativeCalls()
                calls.hooks[name] = lambda args: diagnostic(args)
                self.check_failure(calls, m.ErrorKind.PROTOCOL_INCONSISTENCY)
                self.assertEqual(calls.calls, dict(zip(NAMES, expected)))

    def test_failures_with_and_without_published_owners(self):
        for name in (CREATE, COUNT, GET, VIEW):
            for publish in (False, True):
                with self.subTest(name=name, publish=publish):
                    calls = NativeCalls()

                    def fail(args):
                        if publish:
                            self.assertEqual(calls.real[name](*args), n.AMS_MEL_OK)
                        return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"original failure")

                    calls.hooks[name] = fail
                    error = self.check_failure(calls)
                    self.assertEqual(error.diagnostic, "original failure")
                    self.assertEqual(calls.calls[ENTRY_CLOSE], int(name == VIEW or (name == GET and publish)))
                    self.assertEqual(calls.calls[LIST_CLOSE], int(name != CREATE or publish))

    def test_impossible_counts_rejected_before_get(self):
        for count in (sys.maxsize, sys.maxsize // ctypes.sizeof(ctypes.py_object) + 1,
                      ctypes.c_size_t(-1).value):
            with self.subTest(count=count):
                calls = NativeCalls()

                def oversized(args):
                    ctypes.cast(args[1], ctypes.POINTER(ctypes.c_size_t))[0] = count
                    return diagnostic(args)

                calls.hooks[COUNT] = oversized
                self.check_failure(calls, m.ErrorKind.PROTOCOL_INCONSISTENCY)
                self.assertEqual(calls.calls[GET], 0)
                self.assertEqual(calls.calls[LIST_CLOSE], 1)

    def test_later_copy_failures_release_every_acquired_entry(self):
        for index in (1, 3):
            for failure in (MemoryError("copy allocation"), ValueError("copy conversion")):
                with self.subTest(index=index, failure=type(failure)):
                    calls = NativeCalls()
                    real = _ffi._capability_from_raw
                    copied = 0

                    def convert(raw):
                        nonlocal copied
                        copied += 1
                        if copied == index + 1:
                            raise failure
                        return real(raw)

                    with mock.patch.object(_ffi, "_capability_from_raw", side_effect=convert):
                        error = self.check_failure(calls, exception=type(failure))
                    self.assertIs(error, failure)
                    self.assertEqual(calls.calls[ENTRY_CLOSE], index + 1)
                    self.assertEqual(calls.calls[LIST_CLOSE], 1)

    def test_final_tuple_allocation_failure_closes_list_after_all_entries(self):
        calls = NativeCalls()
        failure = MemoryError("final tuple allocation")
        with mock.patch.object(_discovery, "tuple", create=True, side_effect=failure):
            self.assertIs(self.check_failure(calls, exception=MemoryError), failure)
        self.assertEqual(calls.calls, dict(zip(NAMES, (1, 1, 4, 4, 4, 1))))

    def test_malformed_nested_graph_uses_live_native_storage(self):
        calls = NativeCalls()

        def malformed(args):
            status = calls.real[VIEW](*args)
            raw = ctypes.cast(args[1], ctypes.POINTER(ctypes.POINTER(n.IrChannelCapabilityV1)))[0].contents
            raw.image_bands.data[1].bands.size = 1
            raw.image_bands.data[1].bands.data = ctypes.POINTER(n.IrBandInfoV1)()
            return status

        calls.hooks[VIEW] = malformed
        self.check_failure(calls, m.ErrorKind.PROTOCOL_INCONSISTENCY)
        self.assertEqual(calls.calls[ENTRY_CLOSE], 1)
        self.assertEqual(calls.calls[LIST_CLOSE], 1)

    def test_unknown_enums_duplicates_and_ieee_values(self):
        calls = NativeCalls()

        def altered(args):
            status = calls.real[VIEW](*args)
            raw = ctypes.cast(args[1], ctypes.POINTER(ctypes.POINTER(n.IrChannelCapabilityV1)))[0].contents
            raw.pixel_format = 0xFFFFFFFF
            raw.channel_types.data[0] = 0xF0000001
            raw.channel_types.data[1] = 0xF0000001
            raw.sensor_types.data[1] = 0xF0000002
            raw.metadata_capabilities.data[3] = 0xF0000003
            raw.nav_frames.data[1] = 0xF0000004
            raw.image_bands.data[0].bands.data[0].kind = 0xF0000005
            raw.sensor_location.offset_x_m = -0.0
            raw.sensor_location.offset_y_m = float("inf")
            raw.sensor_location.offset_z_m = float("nan")
            raw.image_bands.data[0].bands.data[0].min_wavelength_m = -float("inf")
            return status

        calls.hooks[VIEW] = altered
        with self.open() as session, calls.installed():
            values = session.capabilities()
        for value in values:
            self.assertEqual(int(value.pixel_format), 0xFFFFFFFF)
            self.assertEqual(value.channel_types[:2], (0xF0000001,) * 2)
            self.assertEqual(value.channel_types[0].name, "UNKNOWN_0xF0000001")
            self.assertEqual(int(value.sensor_types[1]), 0xF0000002)
            self.assertEqual(int(value.metadata_capabilities[3]), 0xF0000003)
            self.assertEqual(int(value.nav_frames[1]), 0xF0000004)
            self.assertEqual(int(value.image_bands[0].bands[0].kind), 0xF0000005)
            self.assertEqual(struct.pack("d", value.sensor_location.offset_x_m), struct.pack("d", -0.0))
            self.assertEqual(value.sensor_location.offset_y_m, float("inf"))
            self.assertTrue(math.isnan(value.sensor_location.offset_z_m))
            self.assertEqual(value.image_bands[0].bands[0].min_wavelength_m, -float("inf"))
            self.assertEqual((value.number_of_bands, len(value.image_bands),
                              tuple(len(b.bands) for b in value.image_bands)), (3, 2, (2, 1)))
            assert_owned_graph(self, value)

    def test_close_failures_never_retry_consumed_or_uncertain_owners(self):
        for name in (ENTRY_CLOSE, LIST_CLOSE):
            for consumed in (False, True):
                for clear in (False, True):
                    with self.subTest(name=name, consumed=consumed, clear=clear):
                        calls = NativeCalls()

                        def fail_close(args):
                            slot = owner_slot(args[0])
                            address = slot.value
                            if consumed:
                                self.assertEqual(calls.real[name](*args), n.AMS_MEL_OK)
                            else:
                                calls.rescue.append((calls.real[name], address))
                            slot.value = None if clear else address
                            return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"cleanup failed")

                        calls.hooks[name] = fail_close
                        error = self.check_failure(calls, m.ErrorKind.INTERNAL_ERROR)
                        self.assertEqual(error.diagnostic, "cleanup failed")
                        self.assertEqual(calls.calls[name], 1)
                        self.assertEqual(calls.calls[LIST_CLOSE], 1)
                        self.assertEqual(calls.calls[ENTRY_CLOSE], 1 if name == ENTRY_CLOSE else 4)

    def test_successful_close_without_clear_is_not_retried(self):
        for name in (ENTRY_CLOSE, LIST_CLOSE):
            with self.subTest(name=name):
                calls = NativeCalls()

                def consumed_uncleared(args):
                    address = owner_slot(args[0]).value
                    status = calls.real[name](*args)
                    owner_slot(args[0]).value = address
                    return status

                calls.hooks[name] = consumed_uncleared
                self.check_failure(calls, m.ErrorKind.PROTOCOL_INCONSISTENCY)
                self.assertEqual(calls.calls[name], 1)

    def test_original_exception_survives_both_cleanup_failures(self):
        calls = NativeCalls()
        failure = MemoryError("original copy failure")

        def fail_entry(args):
            calls.real[ENTRY_CLOSE](*args)
            return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"entry cleanup")

        def fail_list(args):
            calls.real[LIST_CLOSE](*args)
            return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"list cleanup")

        calls.hooks.update({ENTRY_CLOSE: fail_entry, LIST_CLOSE: fail_list})
        with mock.patch.object(_ffi, "_capability_from_raw", side_effect=failure):
            self.assertIs(self.check_failure(calls, exception=MemoryError), failure)
        self.assertEqual((calls.calls[ENTRY_CLOSE], calls.calls[LIST_CLOSE]), (1, 1))

    def test_malformed_diagnostics_and_unknown_status(self):
        for payload, required, status, kind in (
            (b"\xff\0", 2, 8, m.ErrorKind.INVALID_UTF8),
            (b"xx", 2, 8, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"\0x\0", 3, 8, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"\0", 0, 8, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"\0", 5000, 0, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"\0", 5000, -123, m.ErrorKind.UNKNOWN),
            (b"future\0", 7, -123, m.ErrorKind.UNKNOWN),
        ):
            with self.subTest(payload=payload, required=required, status=status):
                calls = NativeCalls()

                def malformed(args):
                    calls.real[GET](*args)  # Real owner published before bad status/diagnostic.
                    ctypes.memmove(args[-3], payload, len(payload))
                    ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = required
                    return status

                calls.hooks[GET] = malformed
                error = self.check_failure(calls, kind)
                if kind is m.ErrorKind.UNKNOWN:
                    self.assertEqual(error.native_status, -123)
                    self.assertEqual(error.diagnostic_required, required)
                    self.assertEqual(error.diagnostic, None if required == 5000 else "future")
                self.assertEqual((calls.calls[ENTRY_CLOSE], calls.calls[LIST_CLOSE]), (1, 1))

    def test_diagnostic_allocation_failure_still_attempts_cleanup_once(self):
        calls = NativeCalls()
        real = _discovery._call_with_diagnostic

        def allocation_failure(call):
            if calls.calls[VIEW] == 1:
                raise MemoryError("diagnostic allocation")
            return real(call)

        with mock.patch.object(_discovery, "_call_with_diagnostic", side_effect=allocation_failure):
            self.check_failure(calls, exception=MemoryError)
        self.assertEqual((calls.calls[ENTRY_CLOSE], calls.calls[LIST_CLOSE]), (1, 1))

    def test_close_python_exception_is_not_retried(self):
        for name in (ENTRY_CLOSE, LIST_CLOSE):
            with self.subTest(name=name):
                calls = NativeCalls()
                failure = RuntimeError("Close boundary failed after consumption")

                def close_raises(args):
                    address = owner_slot(args[0]).value
                    calls.real[name](*args)
                    owner_slot(args[0]).value = address
                    raise failure

                calls.hooks[name] = close_raises
                self.assertIs(self.check_failure(calls, exception=RuntimeError), failure)
                self.assertEqual(calls.calls[name], 1)

    def test_second_entry_get_failure_releases_prefix_and_published_entry(self):
        calls = NativeCalls()

        def fail_second(args):
            status = calls.real[GET](*args)
            if calls.calls[GET] == 2:
                return diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"second get failed")
            return status

        calls.hooks[GET] = fail_second
        error = self.check_failure(calls)
        self.assertEqual(error.diagnostic, "second get failed")
        self.assertEqual(calls.calls, dict(zip(NAMES, (1, 1, 2, 1, 2, 1))))

    def test_entry_close_error_survives_list_close_error(self):
        calls = NativeCalls()
        for name in (ENTRY_CLOSE, LIST_CLOSE):
            def fail(args, name=name):
                calls.real[name](*args)
                return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, name.encode())
            calls.hooks[name] = fail
        error = self.check_failure(calls, m.ErrorKind.INTERNAL_ERROR)
        self.assertEqual(error.diagnostic, ENTRY_CLOSE)
        self.assertEqual((calls.calls[ENTRY_CLOSE], calls.calls[LIST_CLOSE]), (1, 1))

    def test_misaligned_view_rejected_without_dereference(self):
        calls = NativeCalls()
        storage = (ctypes.c_char * (ctypes.sizeof(n.IrChannelCapabilityV1) + 8))()

        def misaligned(args):
            ctypes.cast(args[1], ctypes.POINTER(ctypes.c_void_p))[0] = ctypes.addressof(storage) + 1
            return diagnostic(args)

        calls.hooks[VIEW] = misaligned
        self.check_failure(calls, m.ErrorKind.PROTOCOL_INCONSISTENCY)
        self.assertEqual((calls.calls[ENTRY_CLOSE], calls.calls[LIST_CLOSE]), (1, 1))


if __name__ == "__main__":
    unittest.main()
