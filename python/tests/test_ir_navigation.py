"""PY-MEL-012: compiled-provider fidelity, hostile ABI and deterministic lifetimes."""

from __future__ import annotations

import copy
import ctypes
import dataclasses as dc
from decimal import Decimal
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
import time
import unittest
from unittest import mock
import weakref

import ams_mel as m
from ams_mel import _native as n, _navigation, _navigation_types as values
from ams_mel._health_types import Euler
from ams_mel._image_types import NavigationResponse
import test_image_stream as image_fixtures


SUBMIT = "ams_mel_ir_stream_submit_navigation_report"
WAIT = "ams_mel_ir_navigation_request_wait"
CLOSE = "ams_mel_ir_navigation_request_close"
EXPECTED = m.NavigationCompleted(m.NavigationResponse(-8765432109, 0xF1234567, 0x89ABCDEF))
COVARIANCE = (
    "position_position_pn_pn", "position_position_pn_pe", "position_position_pn_pd",
    "position_position_pe_pe", "position_position_pe_pd", "position_position_pd_pd",
    "position_velocity_pn_vn", "position_velocity_pn_ve", "position_velocity_pn_vd",
    "position_velocity_pe_ve", "position_velocity_pe_vd", "position_velocity_pd_vd",
    "velocity_velocity_vn_vn", "velocity_velocity_vn_ve", "velocity_velocity_vn_vd",
    "velocity_velocity_ve_ve", "velocity_velocity_ve_vd", "velocity_velocity_vd_vd",
)
SCALARS = ("latitude_rad", "longitude_rad", "altitude_m", "wander_angle_rad",
           "magnetic_heading", "altitude_msl")


def report(**changes):
    return m.NavigationReport(**(dict(
        system_time_ns=-123456789012, state=m.PositionSolutionState.BLENDED,
        latitude_rad=0.523598, longitude_rad=-1.308997, altitude_m=987.5,
        attitude=m.Euler(0.1, 0.2, 0.3),
        attitude_rate=m.AttitudeRate(m.Euler(0.11, -0.22, 0.33), -424242),
        speed=m.NorthEastDown(10.0, -20.0, 30.0),
        acceleration=m.NorthEastDown(-1.0, 2.0, -3.0),
        wander_angle_rad=0.05, magnetic_heading=12.5, altitude_msl=1000.25,
        position_velocity_covariance_uncertainty=m.PositionVelocityCovariance(
            **dict(zip(COVARIANCE, (i + 1.5 for i in range(18))))),
    ) | changes))


def diagnostic(args, status=n.AMS_MEL_OK, text=b""):
    buffer, capacity, required = args[-3:]
    if required is not None:
        ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        ctypes.memmove(buffer, text + b"\0", min(capacity, len(text) + 1))
    return status


def raw_result(args):
    return ctypes.cast(args[2], ctypes.POINTER(n.IrNavigationResultV1)).contents


def reclaimed():
    boundary = n._LIBRARY.ams_mel_test_completion_boundary
    boundary.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint64,
                         ctypes.POINTER(ctypes.c_uint64)]
    boundary.restype = ctypes.c_int
    owner = n._LIBRARY.ams_mel_test_completion_owner
    owner.argtypes = [ctypes.c_uint, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
    owner.restype = ctypes.c_int
    counts, observed = (ctypes.c_uint64 * 4)(), ctypes.c_uint64()
    if boundary(3, 3, 0, counts) != 1 or owner(3, counts[2], ctypes.byref(observed)) != 1:
        raise AssertionError("Navigation completion-owner reclamation probe failed")


class NavigationTests(unittest.TestCase):
    def open(self, scenario="success", **options):
        provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        session = m.Session.open(provider, scenario, "", **options)
        self.addCleanup(session.close)
        image = session.open_image_stream(image_fixtures.ImageStreamTests.config())
        self.addCleanup(image.close)
        return session, image

    def request(self, image, value=None):
        request = image.submit_navigation_report(value if value is not None else report())
        self.addCleanup(request.close)
        return request

    def error(self, kind, call):
        with self.assertRaises(m.MelError) as caught:
            call()
        self.assertIs(caught.exception.kind, kind)
        return caught.exception

    def test_compiled_full_fidelity_attached_running_and_repeated_wait(self):
        _, image = self.open("navigation-fidelity")
        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit, \
                mock.patch.object(n, "ams_mel_ir_stream_start", wraps=n.ams_mel_ir_stream_start) as start, \
                mock.patch.object(n, "ams_mel_ir_image_metadata_open") as metadata:
            first = self.request(image)
            self.assertEqual(first.wait(5000), EXPECTED)
            self.assertEqual(first.wait(0), EXPECTED)
            start.assert_not_called()
            metadata.assert_not_called()
            image.start()
            second = self.request(image)
            self.assertIsNot(first, second)
            self.assertEqual(second.wait(5000), EXPECTED)
            self.assertEqual(first.wait(0), EXPECTED)
            self.assertEqual(submit.call_count, 2)
        image.stop()
        self.error(m.ErrorKind.PROVIDER_FAILED, lambda: self.request(image))
        image.close()
        with mock.patch.object(n, SUBMIT) as submit:
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: self.request(image))
            submit.assert_not_called()

    def test_failed_image_rejects_submission(self):
        _, image = self.open("enable-fail")
        self.error(m.ErrorKind.PROVIDER_FAILED, image.start)
        self.error(m.ErrorKind.PROVIDER_FAILED, lambda: self.request(image))
        self.error(m.ErrorKind.PROVIDER_FAILED, image.close)

    def test_all_states_and_full_signed_timestamps(self):
        _, image = self.open()
        native = getattr(n, SUBMIT)
        for state in m.PositionSolutionState:
            for timestamp in (-(1 << 63), -1, 0, 1, (1 << 63) - 1):
                value = report(state=state, system_time_ns=timestamp,
                               attitude_rate=m.AttitudeRate(m.Euler(0, 0, 0), -timestamp - 1))

                def inspect(*args):
                    raw = ctypes.cast(args[1], ctypes.POINTER(n.NavigationReportV1)).contents
                    self.assertEqual(raw.state, int(state))
                    self.assertEqual(raw.system_time_ns, timestamp)
                    self.assertEqual(raw.attitude_rate.attitude_rate_time_ns, -timestamp - 1)
                    return native(*args)

                with self.subTest(state=state, timestamp=timestamp), \
                        mock.patch.object(n, SUBMIT, side_effect=inspect) as submit:
                    with image.submit_navigation_report(value) as request:
                        self.assertEqual(request.wait(5000), EXPECTED)
                    submit.assert_called_once()

    def test_ieee_boundaries_every_double_and_independent_covariance_order(self):
        _, image = self.open()
        native = getattr(n, SUBMIT)
        self.assertEqual(tuple(f.name for f in dc.fields(m.PositionVelocityCovariance)), COVARIANCE)
        for floating in (-0.0, 0.0, 5e-324, -5e-324, sys.float_info.min,
                         sys.float_info.max, -sys.float_info.max, float("inf"),
                         float("-inf"), float("nan"), Fraction(1, 8), Decimal("0.25")):
            value = report(**dict.fromkeys(SCALARS, floating),
                           attitude=m.Euler(floating, floating, floating),
                           attitude_rate=m.AttitudeRate(m.Euler(floating, floating, floating), 0),
                           speed=m.NorthEastDown(floating, floating, floating),
                           acceleration=m.NorthEastDown(floating, floating, floating),
                           position_velocity_covariance_uncertainty=m.PositionVelocityCovariance(
                               **dict.fromkeys(COVARIANCE, floating)))

            def inspect(*args):
                raw = ctypes.cast(args[1], ctypes.POINTER(n.NavigationReportV1)).contents
                actual = [getattr(raw, f) for f in SCALARS]
                actual += [getattr(triple, f) for triple in (raw.attitude, raw.attitude_rate.attitude_rate)
                           for f in ("roll", "pitch", "yaw")]
                actual += [getattr(triple, f) for triple in (raw.speed, raw.acceleration)
                           for f in ("north", "east", "down")]
                actual += [getattr(raw.position_velocity_covariance_uncertainty, f) for f in COVARIANCE]
                for number in actual:
                    if math.isnan(float(floating)):
                        self.assertTrue(math.isnan(number))
                    else:
                        self.assertEqual(struct.pack("d", number), struct.pack("d", float(floating)))
                return native(*args)

            with self.subTest(floating=floating), mock.patch.object(n, SUBMIT, side_effect=inspect):
                with image.submit_navigation_report(value) as request:
                    self.assertEqual(request.wait(5000), EXPECTED)

    def test_invalid_graph_construction_and_mutation_never_calls_native(self):
        _, image = self.open()
        invalid = dict(
            system_time_ns=(True, False, -(1 << 63) - 1, 1 << 63, 1.0, None),
            state=(5, 6, 0xFFFFFFFF, 0, True, None),
            attitude=(None, (0, 0, 0), m.NorthEastDown(0, 0, 0)),
            attitude_rate=(None, m.Euler(0, 0, 0)), speed=(None, m.Euler(0, 0, 0)),
            acceleration=(None, (0, 0, 0)),
            position_velocity_covariance_uncertainty=(None, [0] * 18),
        )
        bad_numbers = (True, False, "1", None, 1j, 10 ** 1000, Decimal("1e10000"))
        invalid.update(dict.fromkeys(SCALARS, bad_numbers))
        with mock.patch.object(n, SUBMIT) as submit:
            for field, candidates in invalid.items():
                for candidate in candidates:
                    with self.subTest(field=field, candidate=repr(candidate)):
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: report(**{field: candidate}))
                        value = report()
                        object.__setattr__(value, field, candidate)
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            for field in dc.fields(m.NavigationReport):
                value = report()
                object.__delattr__(value, field.name)
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            for parent_name, factory, names in (
                ("speed", lambda: m.NorthEastDown(1, 2, 3), ("north", "east", "down")),
                ("acceleration", lambda: m.NorthEastDown(1, 2, 3), ("north", "east", "down")),
                ("attitude", lambda: m.Euler(1, 2, 3), ("roll", "pitch", "yaw")),
                ("position_velocity_covariance_uncertainty",
                 lambda: m.PositionVelocityCovariance(*range(18)), COVARIANCE),
            ):
                for name in names:
                    for candidate in bad_numbers:
                        nested = factory()
                        object.__setattr__(nested, name, candidate)
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: report(**{parent_name: nested}))
                        value = report()
                        object.__setattr__(value, parent_name, nested)
                        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            for candidate in (True, 1 << 63, -(1 << 63) - 1, 1.0):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: m.AttitudeRate(m.Euler(0, 0, 0), candidate))
                value = report()
                object.__setattr__(value.attitude_rate, "attitude_rate_time_ns", candidate)
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            for candidate in (None, m.NorthEastDown(0, 0, 0)):
                value = report()
                object.__setattr__(value.attitude_rate, "attitude_rate", candidate)
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            value = report()
            object.__setattr__(value.attitude_rate.attitude_rate, "yaw", True)
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            for candidate in (None, {}, object()):
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(candidate))
            submit.assert_not_called()
        for factory in (lambda: m.NorthEastDown(True, 0, 0),
                        lambda: m.PositionVelocityCovariance(True, *range(17)),
                        lambda: m.AttitudeRate(None, 0)):
            self.error(m.ErrorKind.INVALID_ARGUMENT, factory)
        for state in (5, 6, 0xFFFFFFFF):
            with self.assertRaises(ValueError):
                m.PositionSolutionState(state)

    def test_nested_missing_fields_and_forged_enum_rejected_locally(self):
        _, image = self.open()
        with mock.patch.object(n, SUBMIT) as submit:
            for parent, names in (
                ("attitude", ("roll", "pitch", "yaw")),
                ("attitude_rate", ("attitude_rate", "attitude_rate_time_ns")),
                ("speed", ("north", "east", "down")),
                ("acceleration", ("north", "east", "down")),
                ("position_velocity_covariance_uncertainty", COVARIANCE),
            ):
                for name in names:
                    value = report()
                    object.__delattr__(getattr(value, parent), name)
                    self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: image.submit_navigation_report(value))
            forged = int.__new__(m.PositionSolutionState, 5)
            forged._name_, forged._value_ = "MAX_EXCLUSIVE", 5
            self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: report(state=forged))
            submit.assert_not_called()

    def test_compiled_rejections_and_failures_are_distinct(self):
        for scenario, kind, text in (
            ("navigation-reject", None, "invalid navigation report"),
            ("navigation-reject-long", None, None),
            ("navigation-null-result", m.ErrorKind.PROVIDER_FAILED,
             "provider returned null successful NavigationReportResp"),
            ("navigation-future-throw", m.ErrorKind.PROVIDER_EXCEPTION, "mock navigation future exception"),
            ("navigation-send-throw", m.ErrorKind.PROVIDER_EXCEPTION, "mock navigation send exception"),
        ):
            with self.subTest(scenario=scenario):
                _, image = self.open(scenario)
                if scenario == "navigation-send-throw":
                    self.assertEqual(self.error(kind, lambda: self.request(image)).diagnostic, text)
                    continue
                request = self.request(image)
                with mock.patch.object(n, WAIT, wraps=getattr(n, WAIT)) as wait, \
                        mock.patch.object(n, SUBMIT) as submit:
                    if kind is None:
                        result = request.wait(5000)
                        self.assertIs(type(result), m.NavigationRejected)
                        self.assertEqual(result.code, m.MelErrorCode.INVALID_PARAMETERS)
                        if text is None:
                            self.assertEqual(result.description, "x" * 510 + "€" + "y" * 100)
                            self.assertEqual(wait.call_count, 2)
                            self.assertEqual(wait.call_args_list[1].args[1], 0)
                        else:
                            self.assertEqual(result.description, text)
                        self.assertEqual(request.wait(0), result)
                    else:
                        error = self.error(kind, lambda: request.wait(5000))
                        self.assertEqual(error.diagnostic, text)
                        self.assertEqual(self.error(kind, lambda: request.wait(0)).diagnostic, text)
                    submit.assert_not_called()

    def test_hostile_diagnostics_status_and_success_invariants(self):
        _, image = self.open()
        request = self.request(image)
        cases = (
            (n.AMS_MEL_OK, b"unexpected", 0, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (n.AMS_MEL_OK, b"", 1, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (n.AMS_MEL_COMMAND_REJECTED, b"bad\0text", 3, m.ErrorKind.PROTOCOL_INCONSISTENCY),
            (n.AMS_MEL_COMMAND_REJECTED, b"\xff", 3, m.ErrorKind.INVALID_UTF8),
            (123456, b"unknown status", 0, m.ErrorKind.UNKNOWN),
        )
        for status, text, code, kind in cases:
            def wait(*args):
                raw_result(args).error_code = code
                return diagnostic(args, status, text)
            with self.subTest(status=status, text=text), mock.patch.object(n, WAIT, side_effect=wait):
                error = self.error(kind, lambda: request.wait(0))
                if kind is m.ErrorKind.UNKNOWN:
                    self.assertEqual(error.native_status, 123456)
        for length in (0, 2):
            def malformed(*args):
                ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] = length
                return n.AMS_MEL_COMMAND_REJECTED
            with mock.patch.object(n, WAIT, side_effect=malformed):
                self.error(m.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: request.wait(0))
        for timeout in (-1, 1 << 32, True, 1.0, None):
            with mock.patch.object(n, WAIT) as wait:
                self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: request.wait(timeout))
                wait.assert_not_called()

    def test_terminal_retry_checks_only_active_fields_and_detects_changes(self):
        _, image = self.open()
        request = self.request(image)
        text = ("x" * 510 + "€" + "y" * 100).encode()
        for status in (n.AMS_MEL_OK, n.AMS_MEL_COMMAND_REJECTED, n.AMS_MEL_PROVIDER_EXCEPTION):
            for change in (None, "status", "length", "code", "response"):
                calls = []

                def wait(*args):
                    calls.append(args[1])
                    second = len(calls) == 2
                    result = raw_result(args)
                    result.error_code = 0xFEDCBA98 if status == n.AMS_MEL_COMMAND_REJECTED else 0
                    result.response.system_time_ns = -1
                    result.response.command_id = 0xFFFFFFFF
                    result.response.request_id = 0x80000000
                    if second and change == "code":
                        result.error_code += 1
                    if second and change == "response":
                        result.response.request_id += 1
                    result_status = n.AMS_MEL_TIMEOUT if second and change == "status" else status
                    returned = diagnostic(args, result_status, text)
                    if second and change == "length":
                        ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))[0] -= 1
                    return returned

                with self.subTest(status=status, change=change), mock.patch.object(n, WAIT, side_effect=wait):
                    inconsistent = (change in ("status", "length")
                                    or change == "code" and status in (n.AMS_MEL_OK, n.AMS_MEL_COMMAND_REJECTED)
                                    or change == "response" and status == n.AMS_MEL_OK)
                    if inconsistent or status == n.AMS_MEL_OK:
                        self.error(m.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: request.wait(99))
                    elif status == n.AMS_MEL_COMMAND_REJECTED:
                        result = request.wait(99)
                        self.assertEqual(int(result.code), 0xFEDCBA98)
                        self.assertEqual(result.code.name, "UNKNOWN_0xFEDCBA98")
                        self.assertEqual(result.description, text.decode())
                    else:
                        self.assertEqual(self.error(m.ErrorKind.PROVIDER_EXCEPTION,
                                                    lambda: request.wait(99)).diagnostic, text.decode())
                    self.assertEqual(calls, [99, 0])
        with mock.patch.object(n, WAIT, side_effect=lambda *args: diagnostic(
                args, n.AMS_MEL_TIMEOUT, text)) as wait:
            self.error(m.ErrorKind.TIMEOUT, lambda: request.wait(0))
            wait.assert_called_once()

    def test_success_full_width_response_and_unknown_rejection_codes(self):
        _, image = self.open()
        request = self.request(image)
        for timestamp in (-(1 << 63), (1 << 63) - 1):
            def wait(*args):
                raw_result(args).response = n.IrNavigationResponseV1(timestamp, 0xFFFFFFFF, 0xFFFFFFFF)
                return diagnostic(args)
            with mock.patch.object(n, WAIT, side_effect=wait):
                self.assertEqual(request.wait(0), m.NavigationCompleted(
                    m.NavigationResponse(timestamp, 0xFFFFFFFF, 0xFFFFFFFF)))

    def test_long_failure_ignores_inactive_fields_and_validates_retrieved_text(self):
        _, image = self.open()
        request = self.request(image)
        for failure in ("unterminated", "utf8", "allocation"):
            calls = []

            def wait(*args):
                calls.append(args[1])
                ctypes.memset(args[2], len(calls), ctypes.sizeof(n.IrNavigationResultV1))
                status = diagnostic(args, n.AMS_MEL_PROVIDER_FAILED, b"x" * 600)
                if len(calls) == 2:
                    if failure == "unterminated":
                        ctypes.memset(args[-3], 0xFF, args[-2])
                    else:
                        ctypes.memset(args[-3], 0xFF, 1)
                return status

            with mock.patch.object(n, WAIT, side_effect=wait):
                if failure == "allocation":
                    original = ctypes.create_string_buffer

                    def allocate(size):
                        if size > 512:
                            raise MemoryError
                        return original(size)

                    with mock.patch.object(_navigation._ctypes, "create_string_buffer", side_effect=allocate):
                        self.error(m.ErrorKind.INTERNAL_ERROR, lambda: request.wait(0))
                    self.assertEqual(calls, [0])
                else:
                    self.error(m.ErrorKind.INVALID_UTF8 if failure == "utf8"
                               else m.ErrorKind.PROTOCOL_INCONSISTENCY, lambda: request.wait(10))
                    self.assertEqual(calls, [10, 0])

    def test_admission_and_release_failures_preserve_first_error_once(self):
        _, image = self.open()
        for status in (n.AMS_MEL_OK, n.AMS_MEL_PROVIDER_EXCEPTION):
            for published in (False, True):
                def submit(*args):
                    if published:
                        ctypes.cast(args[2], ctypes.POINTER(n.IrNavigationRequestHandle))[0] = 1234
                    return diagnostic(args, status)
                with self.subTest(status=status, published=published), \
                        mock.patch.object(n, SUBMIT, side_effect=submit) as submitted, \
                        mock.patch.object(n, CLOSE, side_effect=RuntimeError("cleanup failure")) as close:
                    if status == n.AMS_MEL_OK and published:
                        request = image.submit_navigation_report(report())
                        with self.assertRaisesRegex(RuntimeError, "cleanup failure"):
                            request.close()
                        self.assertFalse(request.is_open)
                        request.close()
                        request.__del__()
                    else:
                        self.error(m.ErrorKind.PROTOCOL_INCONSISTENCY if status == n.AMS_MEL_OK
                                   else m.ErrorKind.PROVIDER_EXCEPTION,
                                   lambda: image.submit_navigation_report(report()))
                    gc.collect()
                    submitted.assert_called_once()
                    self.assertEqual(close.call_count, int(published))
        request = m.NavigationRequest._from_owner(n.IrNavigationRequestHandle(1234))
        with mock.patch.object(n, CLOSE, side_effect=RuntimeError("cleanup")):
            request._close_function = getattr(n, CLOSE)
            with self.assertRaisesRegex(ValueError, "body"):
                with request:
                    raise ValueError("body")
            self.assertFalse(request.is_open)

    def test_hostile_submission_and_finalization_cleanup_attempts_once(self):
        _, image = self.open()
        for failure in ("raise", "bad-diagnostic"):
            first_error = ValueError("original submission failure")

            def submit(*args):
                ctypes.cast(args[2], ctypes.POINTER(n.IrNavigationRequestHandle))[0] = 1234
                if failure == "raise":
                    raise first_error
                return n.AMS_MEL_OK  # leaves required=0: malformed boundary

            def close(*args):
                return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"cleanup failure")

            with mock.patch.object(n, SUBMIT, side_effect=submit) as submitted, \
                    mock.patch.object(n, CLOSE, side_effect=close) as released:
                if failure == "raise":
                    with self.assertRaises(ValueError) as caught:
                        image.submit_navigation_report(report())
                    self.assertIs(caught.exception, first_error)
                else:
                    self.error(m.ErrorKind.PROTOCOL_INCONSISTENCY,
                               lambda: image.submit_navigation_report(report()))
                gc.collect()
                submitted.assert_called_once()
                released.assert_called_once()
        for explicit in (False, True):
            with mock.patch.object(n, CLOSE, side_effect=close) as released:
                request = m.NavigationRequest._from_owner(n.IrNavigationRequestHandle(1234))
                if explicit:
                    self.error(m.ErrorKind.INTERNAL_ERROR, request.close)
                request.__del__()
                request.__del__()
                request.close()
                self.assertFalse(request.is_open)
                released.assert_called_once()

    def test_public_identity_pickle_and_unique_ownership(self):
        self.assertIs(m.Euler, Euler)
        self.assertIs(m.NavigationResponse, NavigationResponse)
        for name in ("AttitudeRate", "NavigationCompleted", "NavigationRejected", "NavigationReport",
                     "NorthEastDown", "PositionSolutionState", "PositionVelocityCovariance"):
            cls = getattr(m, name)
            self.assertIs(cls, getattr(values, name))
            self.assertIn(name, m.__all__)
            self.assertEqual(cls.__module__, "ams_mel")
            self.assertIs(pickle.loads(pickle.dumps(cls)), cls)
        self.assertIs(m.NavigationRequest, _navigation.NavigationRequest)
        value = report()
        for item in (value, value.attitude_rate, value.speed, value.position_velocity_covariance_uncertainty,
                     EXPECTED, m.NavigationRejected(m.MelErrorCode.INVALID_PARAMETERS, "€"),
                     *m.PositionSolutionState):
            restored = pickle.loads(pickle.dumps(item))
            self.assertIs(type(restored), type(item))
            self.assertEqual(restored, item)
            if dc.is_dataclass(item):
                with self.assertRaises(dc.FrozenInstanceError):
                    item.unexpected = 1
        with self.assertRaises(TypeError):
            m.NavigationRequest()
        _, image = self.open()
        request = self.request(image)
        for call in (lambda: copy.copy(request), lambda: copy.deepcopy(request),
                     lambda: pickle.dumps(request)):
            with self.assertRaises(TypeError):
                call()
        request.close()
        request.close()
        self.error(m.ErrorKind.INVALID_ARGUMENT, lambda: request.wait(0))

    def test_sync_metadata_is_independent(self):
        _, image = self.open("navigation-sync")
        metadata = image.open_metadata(16)
        self.addCleanup(metadata.close)
        # Registration can itself emit unrelated events: drain separately.
        while True:
            try:
                metadata.receive(0)
            except m.MelError as error:
                self.assertIs(error.kind, m.ErrorKind.TIMEOUT)
                break
        request = self.request(image)
        event = metadata.receive(1000)
        self.assertIs(event.kind, m.ImageMetadataKind.NAVIGATION_RESPONSE)
        self.assertEqual(event.payload, EXPECTED.response)
        self.assertEqual(request.wait(5000), EXPECTED)
        self.error(m.ErrorKind.TIMEOUT, lambda: metadata.receive(0))
        self.assertEqual(request.wait(0), EXPECTED)

    def test_future_completion_does_not_synthesize_metadata(self):
        _, image = self.open("success")
        metadata = image.open_metadata(16)
        self.addCleanup(metadata.close)
        while True:
            try:
                metadata.receive(0)
            except m.MelError as error:
                self.assertIs(error.kind, m.ErrorKind.TIMEOUT)
                break
        request = self.request(image)
        self.assertEqual(request.wait(5000), EXPECTED)
        self.error(m.ErrorKind.TIMEOUT, lambda: metadata.receive(0))

    def test_deferred_cleanup_failure_is_not_success(self):
        reclaimed()
        with tempfile.TemporaryDirectory(prefix="py012-deferred-") as temp, \
                mock.patch.dict(os.environ, AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER=str(Path(temp) / "hold")):
            base = Path(temp) / "hold"
            _, image = self.open("navigation-hold-detach-fail")
            request = self.request(image)
            try:
                image.stop()
                self.error(m.ErrorKind.PROVIDER_FAILED, lambda: self.request(image))
            finally:
                base.with_suffix(".release").touch()
                base.with_suffix(".complete").touch()
            error = self.error(m.ErrorKind.PROVIDER_FAILED, lambda: request.wait(5000))
            self.assertEqual(error.diagnostic, "deferred Image cleanup failed")
            self.assertEqual(self.error(m.ErrorKind.PROVIDER_FAILED,
                                        lambda: request.wait(0)).diagnostic, error.diagnostic)
            reclaimed()
            self.error(m.ErrorKind.PROVIDER_FAILED, image.close)

    def test_deterministic_pending_lifetimes_and_metadata_before_completion(self):
        if os.environ.get("AMS_MEL_NAVIGATION_CHILD") != "1":
            result = subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest",
                 "test_ir_navigation.NavigationTests.test_deterministic_pending_lifetimes_and_metadata_before_completion"],
                env=dict(os.environ, AMS_MEL_NAVIGATION_CHILD="1",
                         PYTHONPATH=os.pathsep.join(sys.path)),
                capture_output=True, text=True, timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return
        reclaimed()
        for mode in ("image-first", "session-first", "request-close", "request-finalize", "metadata"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="py012-hold-") as temp:
                base, log = Path(temp) / "hold", Path(temp) / "log"
                with mock.patch.dict(os.environ, AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER=str(base),
                                     AMS_MEL_TEST_LIFETIME_LOG=str(log)):
                    session, image = self.open("navigation-hold", max_async_requests=2)
                    session_ref, image_ref = weakref.ref(session), weakref.ref(image)
                    metadata = image.open_metadata(16) if mode == "metadata" else None
                    if metadata:
                        while True:
                            try:
                                metadata.receive(0)
                            except m.MelError:
                                break
                    request = image.submit_navigation_report(report())
                    self.error(m.ErrorKind.TIMEOUT, lambda: request.wait(0))
                    second = image.submit_navigation_report(report())
                    self.assertEqual(second.wait(5000), EXPECTED)
                    # One held request retains admission even after its public Close.
                    if mode == "request-close":
                        request.close()
                        request.close()
                    if mode == "request-finalize":
                        reference = weakref.ref(request)
                        del request
                        gc.collect()
                        self.assertIsNone(reference())
                    if mode == "session-first":
                        session.close()
                        image.close()
                    elif mode != "metadata":
                        image.close()
                        session.close()
                    if mode != "metadata":
                        self.doCleanups()
                        del image, session
                        gc.collect()
                        self.assertIsNone(image_ref())
                        self.assertIsNone(session_ref())
                        self.assertNotIn("channel_destroyed", log.read_text())
                        self.assertNotIn("library_unloaded", log.read_text())
                    try:
                        base.with_suffix(".release").touch()
                        deadline = time.monotonic() + 5
                        while not base.with_suffix(".callback-done").exists():
                            self.assertLess(time.monotonic(), deadline, "provider callback barrier deadline")
                            time.sleep(0.001)
                        if mode == "metadata":
                            event = metadata.receive(0)
                            self.assertEqual(event.payload, EXPECTED.response)
                            self.error(m.ErrorKind.TIMEOUT, lambda: request.wait(0))
                        base.with_suffix(".complete").touch()
                        if mode not in ("request-close", "request-finalize"):
                            self.assertEqual(request.wait(5000), EXPECTED)
                            self.assertEqual(request.wait(0), EXPECTED)
                            request.close()
                        reclaimed()
                        second.close()
                        if metadata:
                            metadata.close()
                            image.close()
                            session.close()
                            self.doCleanups()
                        events = log.read_text().splitlines()
                        for name in ("channel_destroyed", "library_unloaded"):
                            self.assertEqual(events.count(name), 1, events)
                        self.assertLess(events.index("navigation_completed"), events.index("channel_destroyed"))
                        self.assertLess(events.index("channel_destroyed"), events.index("library_unloaded"))
                        self.assertLess(events.index("callbacks_quiesced_by_channel_destruction"),
                                        events.index("library_unloaded"))
                    finally:
                        base.with_suffix(".release").touch()
                        base.with_suffix(".complete").touch()

    def test_pending_request_resource_limit_close_does_not_free_admission(self):
        reclaimed()
        with tempfile.TemporaryDirectory(prefix="py012-limit-") as temp, \
                mock.patch.dict(os.environ, AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER=str(Path(temp) / "hold")):
            base = Path(temp) / "hold"
            _, image = self.open("navigation-hold", max_async_requests=1)
            request = self.request(image)
            try:
                self.error(m.ErrorKind.RESOURCE_EXHAUSTED, lambda: self.request(image))
                self.error(m.ErrorKind.TIMEOUT, lambda: request.wait(1))
                request.close()
                self.error(m.ErrorKind.RESOURCE_EXHAUSTED, lambda: self.request(image))
            finally:
                base.with_suffix(".release").touch()
                base.with_suffix(".complete").touch()
                reclaimed()

    def test_navigation_c11_layouts_match_unchanged_ctypes(self):
        root = Path(__file__).resolve().parents[2]
        structures = (
            (n.NavigationReportV1, "ams_mel_navigation_report_v1"),
            (n.EulerV1, "ams_mel_euler_v1"),
            (n.AttitudeRateV1, "ams_mel_attitude_rate_v1"),
            (n.NorthEastDownV1, "ams_mel_north_east_down_v1"),
            (n.PositionVelocityCovarianceV1, "ams_mel_position_velocity_covariance_v1"),
            (n.IrNavigationResultV1, "ams_mel_ir_navigation_result_v1"),
            (n.IrNavigationResponseV1, "ams_mel_ir_navigation_response_v1"),
        )
        expressions, expected = [], []
        for cls, cname in structures:
            expressions += [f"sizeof({cname})", f"_Alignof({cname})"]
            expected += [ctypes.sizeof(cls), ctypes.alignment(cls)]
            for name, _ in cls._fields_:
                expressions += [f"offsetof({cname}, {name})"]
                expected += [getattr(cls, name).offset]
        source = '#include <ams_mel/abi.h>\n#include <stdio.h>\nint main(void) {\n'
        source += "\n".join(f'printf("%zu\\n", {expr});' for expr in expressions)
        source += "\nreturn 0;\n}\n"
        with tempfile.TemporaryDirectory() as temp:
            cfile, exe = Path(temp) / "layout.c", Path(temp) / "layout"
            cfile.write_text(source)
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(root / "native/include"), str(cfile), "-o", str(exe)], check=True)
            actual = [int(v) for v in subprocess.check_output([str(exe)], text=True).split()]
        self.assertEqual(actual, expected)

    def test_documented_example(self):
        _, image = self.open("navigation-fidelity")
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-012-navigation-report.md"
        example = document.read_text().split("```python\n", 1)[1].split("```", 1)[0]
        output = io.StringIO()
        with mock.patch("sys.stdout", output):
            exec(compile(example, str(document), "exec"), {"image": image, "mock_test_report": report()})
        self.assertIn("Mock completion:", output.getvalue())


if __name__ == "__main__":
    unittest.main()
