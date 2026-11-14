"""PY-MEL-011: complete ModeCmd fidelity and hostile admission boundaries."""

from __future__ import annotations

import ctypes
import dataclasses
from decimal import Decimal
from enum import IntEnum
from fractions import Fraction
import gc
import io
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
from ams_mel import _native as n, _types
import test_control_channel as fixtures


SUBMIT = "ams_mel_ir_c2_submit_mode"
CLOSE = "ams_mel_ir_mode_request_close"
FLOATS = ("center_az_rad", "center_el_rad", "scan_width_rad", "scan_height_rad",
          "scan_rate_rad_per_second", "preferred_revisit_interval_seconds",
          "required_revisit_interval_seconds")
UINTS = ("scan_id", "max_range_of_interest_m", "min_range_of_interest_m",
         "elevation_scan_center_altitude_m", "elevation_scan_center_range_m")
CONTROLS = ("continuous_scan", "returning", "agile_scan")


def diagnostic(args, status=n.AMS_MEL_OK, text=b""):
    buffer, capacity, required = args[-3:]
    if required is not None:
        ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        ctypes.memmove(buffer, text + b"\0", min(capacity, len(text) + 1))
    return status


def command(**changes):
    return m.ModeCommand(**(dict(command_id=302, state=m.MFAState.OPERATE,
                                mode=m.MfaMode.TASK_SCHED) | changes))


class GeneralModeTests(unittest.TestCase):
    def open(self, scenario="mode-general", enabled=True, **options):
        provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        session = m.Session.open(provider, scenario, "", **options)
        self.addCleanup(session.close)
        control = session.open_control_channel(fixtures.ControlChannelTests.config())
        self.addCleanup(control.close)
        if enabled:
            control.enable()
        return session, control

    def error(self, kind, call):
        with self.assertRaises(m.MelError) as caught:
            call()
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def request(self, control, value=None):
        request = control.submit_mode(value if value is not None else command())
        self.addCleanup(request.close)
        self.assertIs(type(request), m.ModeRequest)
        return request

    def test_public_identity_pickle_frozen_and_defaults(self):
        for name in ("ModeCommand", "ScanParameters", "ScanType",
                     "CoordinateFrameReference", "DegradationMethod", "MFAState", "MfaMode"):
            value = getattr(m, name)
            self.assertIn(name, m.__all__)
            self.assertIs(value, getattr(_types, name))
            self.assertEqual(value.__module__, "ams_mel")
        for value in (command(), m.ScanParameters(), m.ScanType(1, 2, 3),
                      *m.CoordinateFrameReference, *m.DegradationMethod):
            restored = pickle.loads(pickle.dumps(value))
            self.assertEqual(restored, value)
            self.assertIs(type(restored), type(value))
            if dataclasses.is_dataclass(value):
                with self.assertRaises(dataclasses.FrozenInstanceError):
                    value.unexpected = 1
        self.assertEqual([int(x) for x in m.CoordinateFrameReference], [0, 1])
        self.assertEqual([int(x) for x in m.DegradationMethod], [0, 1, 2, 3])
        scan = m.ScanParameters()
        self.assertIs(scan.elevation_defined_with_range_and_altitude, False)
        self.assertIs(scan.center_frame_ref_el, m.CoordinateFrameReference.INERTIAL)
        self.assertIs(scan.center_frame_ref_az, m.CoordinateFrameReference.AIRCRAFT)
        self.assertIs(scan.degradation_method, m.DegradationMethod.CAPACITY)
        self.assertEqual(scan.scan_type, m.ScanType(0, 0, 0))
        for name in FLOATS + UINTS:
            self.assertEqual(getattr(scan, name), 0)
        self.assertIsNot(command().scan_parameters, command().scan_parameters)
        self.assertIsNot(m.ScanParameters().scan_type, m.ScanParameters().scan_type)

    def test_compiled_full_field_fixture(self):
        scan = m.ScanParameters(
            True, 0.25, -0.5, m.CoordinateFrameReference.AIRCRAFT,
            m.CoordinateFrameReference.INERTIAL, 1.25, 0.75, m.ScanType(1, 2, 3),
            0x89ABCDEF, -0.125, 2.5, 3.5, 123456, 42, 7000, 9000,
            m.DegradationMethod.REVISIT)
        _, control = self.open("mode-full")
        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
            request = self.request(control, command(command_id=0x89ABCDEF,
                                   mode=m.MfaMode.SCAN_VOLUME_SCHED, scan_parameters=scan))
        submit.assert_called_once()
        expected = m.ModeSuccess(m.MfaMode.SCAN_VOLUME_SCHED)
        self.assertEqual(request.wait(5000), expected)
        self.assertEqual(request.wait(0), expected)

    def test_default_profiles_and_unchanged_operate_path(self):
        _, control = self.open()
        native = getattr(n, SUBMIT)

        def inspect(*args):
            raw = ctypes.cast(args[1], ctypes.POINTER(n.IrModeCommandV1)).contents
            scan = raw.scan_parameters
            for name, _ in n.IrScanParamV1._fields_:
                if name == "scan_type":
                    self.assertEqual([getattr(scan.scan_type, key) for key in CONTROLS], [0, 0, 0])
                else:
                    self.assertEqual(getattr(scan, name), 1 if name == "center_frame_ref_az" else 0)
            return native(*args)

        with mock.patch.object(n, SUBMIT, side_effect=inspect) as submit, \
                mock.patch.object(n, "ams_mel_ir_c2_submit_operate",
                                  wraps=n.ams_mel_ir_c2_submit_operate) as operate:
            for state, mode in ((m.MFAState.STANDBY, m.MfaMode.UNUSED),
                                (m.MFAState.OPERATE, m.MfaMode.TASK_SCHED)):
                request = self.request(control, command(state=state, mode=mode))
                self.assertEqual(request.wait(5000), m.ModeSuccess(mode))
                request.close()
            self.assertEqual(submit.call_count, 2)
            request = control.submit_operate(302)
            self.addCleanup(request.close)
            self.assertEqual(request.wait(5000), m.ModeSuccess(m.MfaMode.TASK_SCHED))
            self.assertEqual(submit.call_count, 2)
            operate.assert_called_once()

    def test_all_published_state_mode_pairs_without_inferred_restrictions(self):
        _, control = self.open()
        for state in m.MFAState:
            for mode in m.MfaMode:
                with self.subTest(state=state, mode=mode):
                    request = self.request(control, command(state=state, mode=mode))
                    self.assertEqual(request.wait(5000), m.ModeSuccess(mode))
                    request.close()

    def test_boundary_uint32_and_ieee_fidelity_without_normalization(self):
        _, control = self.open()
        native = getattr(n, SUBMIT)
        for number in (0, 0x80000000, 0xFFFFFFFF):
            for floating in (-0.0, 0.0, -1234567.25, sys.float_info.max,
                             -sys.float_info.max, 5e-324, float("inf"),
                             float("-inf"), float("nan"), 7, Fraction(1, 8), Decimal("0.25")):
                scan = m.ScanParameters(**dict.fromkeys(FLOATS, floating),
                                        **dict.fromkeys(UINTS, number),
                                        scan_type=m.ScanType(number, number, number))

                def inspect(*args):
                    raw = ctypes.cast(args[1], ctypes.POINTER(n.IrModeCommandV1)).contents
                    self.assertEqual(raw.command_id, number)
                    for name in FLOATS:
                        actual = getattr(raw.scan_parameters, name)
                        if math.isnan(float(floating)):
                            self.assertTrue(math.isnan(actual))
                        else:
                            self.assertEqual(struct.pack("d", actual), struct.pack("d", float(floating)))
                    for name in UINTS:
                        self.assertEqual(getattr(raw.scan_parameters, name), number)
                    for name in CONTROLS:
                        self.assertEqual(getattr(raw.scan_parameters.scan_type, name), number)
                    return native(*args)

                with self.subTest(number=number, floating=floating), \
                        mock.patch.object(n, SUBMIT, side_effect=inspect) as submit:
                    request = self.request(control, command(command_id=number, scan_parameters=scan))
                    self.assertEqual(request.wait(5000), m.ModeSuccess(m.MfaMode.TASK_SCHED))
                    request.close()
                    submit.assert_called_once()

    def test_invalid_construction_and_mutated_graphs_never_submit(self):
        _, control = self.open()

        class OtherEnum(IntEnum):
            ZERO = 0

        invalid_unsigned = (-1, 1 << 32, True, False, 1.0, "1", None)
        cases = [(m.ModeCommand, "command_id", value) for value in invalid_unsigned]
        for name, enum in (("state", m.MFAState), ("mode", m.MfaMode)):
            cases += [(m.ModeCommand, name, value) for value in
                      (-1, 15, 4, True, 0, 1.0, "0", None, OtherEnum.ZERO,
                       m.MfaMode.UNUSED if enum is m.MFAState else m.MFAState.NOT_SET)]
        cases += [(m.ModeCommand, "scan_parameters", value) for value in (None, {}, (), m.ScanType())]
        for name in UINTS:
            cases += [(m.ScanParameters, name, value) for value in invalid_unsigned]
        for name in CONTROLS:
            cases += [(m.ScanType, name, value) for value in invalid_unsigned]
        for name in FLOATS:
            cases += [(m.ScanParameters, name, value) for value in
                      (True, False, "1", None, object(), complex(1, 0), 10**1000,
                       Decimal("1e10000"), Fraction(10**1000, 1))]
        for name in ("center_frame_ref_el", "center_frame_ref_az", "degradation_method"):
            cases += [(m.ScanParameters, name, value) for value in
                      (-1, 4, 0, 0.0, True, None, OtherEnum.ZERO, m.MfaMode.UNUSED)]
        cases += [(m.ScanParameters, "elevation_defined_with_range_and_altitude", value)
                  for value in (0, 1, None, "true")]
        cases += [(m.ScanParameters, "scan_type", value) for value in (None, {}, (1, 2, 3))]
        with mock.patch.object(n, SUBMIT) as submit:
            for model, name, value in cases:
                with self.subTest(model=model.__name__, name=name, value=repr(value)[:60]):
                    kwargs = dict(command_id=0, state=m.MFAState.OPERATE, mode=m.MfaMode.TASK_SCHED)
                    self.error(m.ErrorKind.INVALID_ARGUMENT,
                               lambda: model(**((kwargs if model is m.ModeCommand else {}) | {name: value})))
                    root = command()
                    target = (root if model is m.ModeCommand else root.scan_parameters
                              if model is m.ScanParameters else root.scan_parameters.scan_type)
                    object.__setattr__(target, name, value)
                    self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: control.submit_mode(root))
            for malformed in (None, {}, 302, m.ConfigSetCommand(302)):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: control.submit_mode(malformed))
            control.close()
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: control.submit_mode(command()))
            submit.assert_not_called()

    def test_enabled_rule_no_implicit_enable_or_metadata(self):
        _, control = self.open(enabled=False)
        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit, \
                mock.patch.object(n, "ams_mel_ir_c2_enable") as enable, \
                mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as metadata:
            self.error(m.ErrorKind.PROVIDER_FAILED, lambda: control.submit_mode(command()))
            submit.assert_called_once()
            enable.assert_not_called()
            metadata.assert_not_called()

    def test_rejections_and_provider_failures(self):
        for scenario, expected in (
            ("c2-reject", m.ModeRejected(m.MelErrorCode.INVALID_PARAMETERS, "invalid task schedule")),
            ("c2-reject-empty", m.ModeRejected(m.MelErrorCode.INVALID_PARAMETERS, "")),
            ("c2-reject-long", m.ModeRejected(m.MelErrorCode.INVALID_PARAMETERS,
                                             "x" * 510 + "€" + "y" * 100)),
        ):
            with self.subTest(scenario=scenario):
                _, control = self.open(scenario)
                request = self.request(control)
                self.assertEqual(request.wait(5000), expected)
                self.assertEqual(request.wait(0), expected)
                request.close()
                control.close()
        _, control = self.open("c2-send-throw")
        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
            self.error(m.ErrorKind.PROVIDER_EXCEPTION, lambda: control.submit_mode(command()))
            submit.assert_called_once()

    def test_unknown_return_mode_is_not_remapped(self):
        _, control = self.open()
        request = self.request(control)

        def wait(*args):
            result = ctypes.cast(args[2], ctypes.POINTER(n.IrModeResultV1)).contents
            result.mode = 99
            result.error_code = n.AMS_MEL_ERROR_NONE
            return diagnostic(args)

        with mock.patch.object(n, "ams_mel_ir_mode_request_wait", side_effect=wait):
            self.error(m.ErrorKind.PROVIDER_FAILED, lambda: request.wait(0))

    def test_success_without_owner_and_unknown_status(self):
        _, control = self.open()
        for status, kind in ((n.AMS_MEL_OK, m.ErrorKind.PROTOCOL_INCONSISTENCY),
                             (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
                             (123456, m.ErrorKind.UNKNOWN)):
            with self.subTest(status=status), mock.patch.object(
                    n, SUBMIT, side_effect=lambda *a: diagnostic(a, status)) as submit, \
                    mock.patch.object(n, CLOSE) as release:
                error = self.error(kind, lambda: control.submit_mode(command()))
                if status != n.AMS_MEL_OK:
                    self.assertEqual(error.native_status, status)
                gc.collect()
                submit.assert_called_once()
                release.assert_not_called()

    def test_failure_owner_cleanup_once_preserves_first_exception(self):
        _, control = self.open()
        for consumes in (False, True):
            for raises in (False, True):
                original = RuntimeError("original boundary exception")

                def submit(*args):
                    args[2][0] = 0x1234
                    if raises:
                        raise original
                    return diagnostic(args, n.AMS_MEL_PROVIDER_EXCEPTION, b"original")

                def release(owner, *args):
                    self.assertEqual(owner[0], 0x1234)
                    if consumes:
                        owner[0] = None
                    if raises:
                        raise RuntimeError("cleanup exception")
                    return n.AMS_MEL_INTERNAL_ERROR

                with self.subTest(consumes=consumes, raises=raises), \
                        mock.patch.object(n, SUBMIT, side_effect=submit) as submitted, \
                        mock.patch.object(n, CLOSE, side_effect=release) as released:
                    if raises:
                        with self.assertRaises(RuntimeError) as caught:
                            control.submit_mode(command())
                        self.assertIs(caught.exception, original)
                    else:
                        error = self.error(m.ErrorKind.PROVIDER_EXCEPTION,
                                           lambda: control.submit_mode(command()))
                        self.assertEqual(error.diagnostic, "original")
                    gc.collect()
                    submitted.assert_called_once()
                    released.assert_called_once()

    def test_malformed_diagnostics_release_real_owner_once(self):
        _, control = self.open()
        native = getattr(n, SUBMIT)
        for corruption, kind in (("zero", m.ErrorKind.PROTOCOL_INCONSISTENCY),
                                 ("unterminated", m.ErrorKind.PROTOCOL_INCONSISTENCY),
                                 ("nul", m.ErrorKind.PROTOCOL_INCONSISTENCY),
                                 ("utf8", m.ErrorKind.INVALID_UTF8),
                                 ("success-text", m.ErrorKind.PROTOCOL_INCONSISTENCY),
                                 ("oversized", m.ErrorKind.PROTOCOL_INCONSISTENCY)):
            def submit(*args):
                self.assertEqual(native(*args), n.AMS_MEL_OK)
                if corruption in ("nul", "utf8", "success-text"):
                    return diagnostic(args, text={"nul": b"a\0b", "utf8": b"\xff",
                                                  "success-text": b"unexpected"}[corruption])
                ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = {
                    "zero": 0, "unterminated": 1, "oversized": args[-2] + 1}[corruption]
                if corruption == "unterminated":
                    ctypes.memmove(args[-3], b"x", 1)
                return n.AMS_MEL_OK

            with self.subTest(corruption=corruption), \
                    mock.patch.object(n, SUBMIT, side_effect=submit) as submitted, \
                    mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as released:
                self.error(kind, lambda: control.submit_mode(command()))
                gc.collect()
                submitted.assert_called_once()
                released.assert_called_once()

    def test_deterministic_async_lifecycle_and_provider_unload(self):
        if os.environ.get("AMS_MEL_GENERAL_MODE_CHILD") != "1":
            result = subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest",
                 "test_ir_general_mode.GeneralModeTests.test_deterministic_async_lifecycle_and_provider_unload"],
                env=dict(os.environ, AMS_MEL_GENERAL_MODE_CHILD="1",
                         PYTHONPATH=os.pathsep.join(sys.path)),
                capture_output=True, text=True, timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return
        import _ctypes

        provider = ctypes.CDLL(str(Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) /
                                   "libmock_ir_provider.so"))
        gate = provider.mock_completion_gate
        gate.argtypes = [ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_uint64)]
        gate.restype = ctypes.c_int
        boundary = n._LIBRARY.ams_mel_test_completion_boundary
        boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                             ctypes.POINTER(ctypes.c_uint64)]
        boundary.restype = ctypes.c_int
        owner = n._LIBRARY.ams_mel_test_completion_owner
        owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
        owner.restype = ctypes.c_int

        def reclaimed():
            counts = (ctypes.c_uint64 * 4)()
            observed = ctypes.c_uint64()
            self.assertEqual(boundary(0, 3, 0, counts), 1)
            self.assertEqual(owner(0, counts[2], ctypes.byref(observed)), 1)

        with tempfile.TemporaryDirectory(prefix="py011-lifetime-") as temporary, \
                mock.patch.dict(os.environ, AMS_MEL_TEST_LIFETIME_LOG=str(Path(temporary) / "log")):
            log = Path(temporary) / "log"
            for close_early in (False, True):
                session, control = self.open("completion-scale", max_async_requests=1)
                session_ref, control_ref = weakref.ref(session), weakref.ref(control)
                with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
                    request = self.request(control)
                    for timeout in (0, 1):
                        self.error(m.ErrorKind.TIMEOUT, lambda: request.wait(timeout))
                    self.error(m.ErrorKind.RESOURCE_EXHAUSTED,
                               lambda: control.submit_mode(command()))
                    self.assertEqual(submit.call_count, 2)
                if close_early:
                    request.close()
                    request.close()
                    self.error(m.ErrorKind.RESOURCE_EXHAUSTED,
                               lambda: control.submit_mode(command()))
                control.close()
                session.close()
                self.assertNotIn("c2_channel_destroyed", log.read_text())
                self.assertNotIn("library_unloaded", log.read_text())
                self.assertEqual(gate(2, 1, None, None), 1)
                if not close_early:
                    expected = m.ModeSuccess(m.MfaMode.TASK_SCHED)
                    self.assertEqual(request.wait(15000), expected)
                    self.assertEqual(request.wait(0), expected)
                reclaimed()
                request.close()
                request.close()
                self.doCleanups()
                del session, control
                gc.collect()
                self.assertIsNone(session_ref())
                self.assertIsNone(control_ref())
                events = log.read_text()
                self.assertEqual(events.count("mode_sent"), 1)
                self.assertEqual(events.count("c2_channel_destroyed"), 1)
                self.assertNotIn("library_unloaded", events)
                log.write_text("")
            self.assertEqual(gate(3, 0, None, None), 1)
            _, control = self.open("completion-scale")
            request = self.request(control)
            self.assertEqual(gate(2, 1, None, None), 1)
            error = self.error(m.ErrorKind.PROVIDER_EXCEPTION, lambda: request.wait(15000))
            self.assertIn("held future exception", error.diagnostic)
            reclaimed()
            request.close()
            self.doCleanups()
            _ctypes.dlclose(provider._handle)
            self.assertIn("library_unloaded", log.read_text())

    def test_documented_example(self):
        _, control = self.open()
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-011-general-mode.md"
        example = document.read_text().split("```python\n", 1)[1].split("```", 1)[0]
        output = io.StringIO()
        with mock.patch("sys.stdout", output):
            exec(compile(example, str(document), "exec"), {"control": control})
        self.assertIn("Provider returned mode:", output.getvalue())


if __name__ == "__main__":
    unittest.main()
