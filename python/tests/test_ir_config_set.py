"""PY-MEL-010: compiled ConfigSet sends and test-only hostile ABI injection."""

from __future__ import annotations

import ctypes
import dataclasses
import gc
import io
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
from ams_mel import _native as n, _types
import test_control_channel as control_fixtures


SUBMIT = "ams_mel_ir_c2_submit_config_set"
CLOSE = "ams_mel_ir_return_request_close"


def diagnostic(args, status=n.AMS_MEL_OK, text=b""):
    buffer, capacity, required = args[-3:]
    if required is not None:
        ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(text) + 1
    if buffer is not None and capacity:
        ctypes.memmove(buffer, text + b"\0", min(capacity, len(text) + 1))
    return status


class ConfigSetTests(unittest.TestCase):
    def open(self, scenario="config-empty", enabled=True, **options):
        provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        session = m.Session.open(provider, scenario, "", **options)
        self.addCleanup(session.close)
        control = session.open_control_channel(control_fixtures.ControlChannelTests.config())
        self.addCleanup(control.close)
        if enabled:
            control.enable()
        return session, control

    def error(self, kind, call):
        with self.assertRaises(m.MelError) as caught:
            call()
        self.assertEqual(caught.exception.kind, kind)
        return caught.exception

    def request(self, control, command=None):
        request = control.submit_config_set(command or m.ConfigSetCommand(0))
        self.addCleanup(request.close)
        self.assertIs(type(request), m.ReturnRequest)
        return request

    def test_public_model_identity_defaults_and_frozen(self):
        self.assertIn("ConfigSetCommand", m.__all__)
        self.assertIs(m.ConfigSetCommand, _types.ConfigSetCommand)
        self.assertEqual(m.ConfigSetCommand.__module__, "ams_mel")
        command = m.ConfigSetCommand(301)
        self.assertEqual(dataclasses.asdict(command),
                         dict(command_id=301, system_time_ns=0, config=""))
        self.assertEqual(pickle.loads(pickle.dumps(command)), command)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            command.config = "changed"

    def test_compiled_field_fidelity(self):
        for scenario, command in (
            ("config-full", m.ConfigSetCommand(0xFEDCBA98, -1234567890123, "configuration-€")),
            ("config-empty", m.ConfigSetCommand(0)),
            ("config-positive", m.ConfigSetCommand(0x80000000, 9876543210, "positive")),
        ):
            with self.subTest(scenario=scenario):
                _, control = self.open(scenario)
                with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
                    request = self.request(control, command)
                submit.assert_called_once()
                expected = m.ReturnCompleted(m.CommandReturn.SUCCESS)
                self.assertEqual(request.wait(5000), expected)
                self.assertEqual(request.wait(0), expected)
                request.close()
                control.close()

    def test_exact_boundaries_and_utf8_once_per_submission(self):
        _, control = self.open("success")
        native = getattr(n, SUBMIT)

        class CountingText(str):
            encodes = 0

            def encode(self, *args, **kwargs):
                self.encodes += 1
                return super().encode(*args, **kwargs)

        for command_id, timestamp in ((0, -(1 << 63)), (0xFFFFFFFF, (1 << 63) - 1)):
            text = CountingText("e\u0301-€-😀")
            command = m.ConfigSetCommand(command_id, timestamp, text)
            text.encodes = 0  # Construction validates; submission must encode once.

            def inspect(*args):
                gc.collect()  # Backing storage must still be live during the call.
                raw = ctypes.cast(args[1], ctypes.POINTER(n.IrConfigSetCommandV1))[0]
                self.assertEqual(raw.command_id, command_id)
                self.assertEqual(raw.system_time_ns, timestamp)
                expected = b"e\xcc\x81-\xe2\x82\xac-\xf0\x9f\x98\x80"
                self.assertEqual(raw.config.size, len(expected))
                self.assertEqual(ctypes.string_at(raw.config.data, raw.config.size), expected)
                return native(*args)

            with mock.patch.object(n, SUBMIT, side_effect=inspect) as submit:
                request = self.request(control, command)
            self.assertEqual(text.encodes, 1)
            submit.assert_called_once()
            self.assertEqual(request.wait(5000), m.ReturnCompleted(m.CommandReturn.SUCCESS))
            request.close()

    def test_fail_payload_and_explicit_rejection_are_distinct_from_metadata(self):
        for scenario, expected, state in (
            ("config-fail", m.ReturnCompleted(m.CommandReturn.FAIL), m.C2CommandState.REJECTED),
            ("config-reject", m.ReturnRejected(m.MelErrorCode.INVALID_PARAMETERS,
                                             "invalid configuration"), m.C2CommandState.ACCEPTED),
        ):
            with self.subTest(scenario=scenario):
                _, control = self.open(scenario)
                metadata = control.open_metadata()
                self.addCleanup(metadata.close)
                request = self.request(control, m.ConfigSetCommand(301))
                self.assertEqual(request.wait(5000), expected)
                self.assertEqual(request.wait(0), expected)
                event = metadata.receive(5000)
                self.assertEqual(event.kind, m.C2MetadataKind.COMMAND_STATUS)
                self.assertEqual(event.payload.command_id, 301)
                self.assertEqual(event.payload.state, state)
                # Accepted metadata can coexist with MEL rejection; neither is
                # a synthetic "physically applied" result from Python.
                metadata.close()
                request.close()
                control.close()

    def test_all_return_payloads_use_existing_request(self):
        _, control = self.open()
        request = self.request(control)
        for value in m.CommandReturn:
            def wait(*args):
                result = ctypes.cast(args[2], ctypes.POINTER(n.IrReturnResultV1))[0]
                result.value = int(value)
                result.error_code = n.AMS_MEL_ERROR_NONE
                return diagnostic(args)
            with self.subTest(value=value), mock.patch.object(
                    n, "ams_mel_ir_return_request_wait", side_effect=wait):
                self.assertEqual(request.wait(0), m.ReturnCompleted(value))

    def test_long_rejection_uses_cached_wait_without_resubmission(self):
        _, control = self.open()
        text = "x" * 510 + "€" + "y" * 100

        def wait(*args):
            result = ctypes.cast(args[2], ctypes.POINTER(n.IrReturnResultV1))[0]
            result.error_code = int(m.MelErrorCode.INVALID_PARAMETERS)
            return diagnostic(args, n.AMS_MEL_COMMAND_REJECTED, text.encode("utf-8"))

        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
            request = self.request(control)
            with mock.patch.object(n, "ams_mel_ir_return_request_wait", side_effect=wait) as waiting:
                self.assertEqual(request.wait(5000), m.ReturnRejected(
                    m.MelErrorCode.INVALID_PARAMETERS, text))
                self.assertEqual(waiting.call_count, 2)
                self.assertEqual(waiting.call_args_list[1].args[1], 0)
        submit.assert_called_once()

    def test_invalid_fields_at_construction_and_after_deliberate_mutation(self):
        _, control = self.open()
        invalid = {
            "command_id": (-1, 1 << 32, True, False, 1.0, "1", None),
            "system_time_ns": (-(1 << 63) - 1, 1 << 63, True, False, 1.0, "1", None),
            "config": (None, b"config", 1, [], "nul\0text", "\ud800", "\udfff", "\ud800\udfff"),
        }
        with mock.patch.object(n, SUBMIT) as submit:
            for field, values in invalid.items():
                for value in values:
                    with self.subTest(field=field, value=repr(value)):
                        fields = dict(command_id=0, system_time_ns=0, config="")
                        fields[field] = value
                        self.error(m.ErrorKind.INVALID_ARGUMENT,
                                   lambda: m.ConfigSetCommand(**fields))
                        command = m.ConfigSetCommand(0)
                        object.__setattr__(command, field, value)
                        self.error(m.ErrorKind.INVALID_ARGUMENT,
                                   lambda: control.submit_config_set(command))
            submit.assert_not_called()

    def test_invalid_objects_and_closed_channel_do_not_submit(self):
        _, control = self.open()
        with mock.patch.object(n, SUBMIT) as submit:
            for command in (None, 0, "", {}, object()):
                self.error(m.ErrorKind.INVALID_ARGUMENT,
                           lambda: control.submit_config_set(command))
            control.close()
            self.error(m.ErrorKind.INVALID_ARGUMENT,
                       lambda: control.submit_config_set(m.ConfigSetCommand(0)))
            submit.assert_not_called()

    def test_native_requires_enable_no_implicit_metadata(self):
        _, control = self.open(enabled=False)
        with mock.patch.object(n, "ams_mel_ir_c2_enable", wraps=n.ams_mel_ir_c2_enable) as enable, \
                mock.patch.object(n, "ams_mel_ir_c2_metadata_open") as metadata, \
                mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
            self.error(m.ErrorKind.PROVIDER_FAILED,
                       lambda: control.submit_config_set(m.ConfigSetCommand(0)))
            enable.assert_not_called()
            metadata.assert_not_called()
            submit.assert_called_once()
            control.enable()
            self.assertEqual(self.request(control).wait(5000),
                             m.ReturnCompleted(m.CommandReturn.SUCCESS))

    def test_success_without_owner(self):
        _, control = self.open()
        with mock.patch.object(n, SUBMIT, side_effect=lambda *a: diagnostic(a)) as submit, \
                mock.patch.object(n, CLOSE) as release:
            self.error(m.ErrorKind.PROTOCOL_INCONSISTENCY,
                       lambda: control.submit_config_set(m.ConfigSetCommand(0)))
            gc.collect()
            submit.assert_called_once()
            release.assert_not_called()

    def test_submission_status_errors_are_single_call(self):
        _, control = self.open()
        for status, kind in (
            (n.AMS_MEL_PROVIDER_EXCEPTION, m.ErrorKind.PROVIDER_EXCEPTION),
            (n.AMS_MEL_RESOURCE_EXHAUSTED, m.ErrorKind.RESOURCE_EXHAUSTED),
            (123456, m.ErrorKind.UNKNOWN),
        ):
            with self.subTest(status=status), mock.patch.object(
                    n, SUBMIT, side_effect=lambda *a: diagnostic(a, status, b"original")) as submit, \
                    mock.patch.object(n, CLOSE) as release:
                error = self.error(kind, lambda: control.submit_config_set(m.ConfigSetCommand(0)))
                self.assertEqual(error.native_status, status)
                self.assertEqual(error.diagnostic, "original")
                submit.assert_called_once()
                release.assert_not_called()

    def test_compiled_provider_send_exception(self):
        # The unchanged config-full provider throws on any conversion mismatch.
        # Intentionally supply valid but unexpected fields to exercise the real
        # C++ exception-to-C-status boundary, not a Python-raised substitute.
        _, control = self.open("config-full")
        with mock.patch.object(n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit, \
                mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as release:
            error = self.error(m.ErrorKind.PROVIDER_EXCEPTION,
                               lambda: control.submit_config_set(m.ConfigSetCommand(0)))
            self.assertEqual(error.diagnostic, "ConfigSet conversion mismatch")
            submit.assert_called_once()
            release.assert_not_called()

    def test_failure_with_published_owner_cleanup_once_and_original_preserved(self):
        _, control = self.open()
        for consumes in (False, True):
            for raises in (False, True):
                original = RuntimeError("original boundary exception")

                def submit(*args):
                    ctypes.cast(args[2], ctypes.POINTER(n.IrReturnRequestHandle))[0] = 0x1234
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
                            control.submit_config_set(m.ConfigSetCommand(0))
                        self.assertIs(caught.exception, original)
                    else:
                        error = self.error(m.ErrorKind.PROVIDER_EXCEPTION,
                                           lambda: control.submit_config_set(m.ConfigSetCommand(0)))
                        self.assertEqual(error.diagnostic, "original")
                    gc.collect()
                    submitted.assert_called_once()
                    released.assert_called_once()

    def test_malformed_diagnostics_release_real_owner_once(self):
        _, control = self.open()
        native = getattr(n, SUBMIT)
        for corruption, kind in (
            ("zero", m.ErrorKind.PROTOCOL_INCONSISTENCY),
            ("unterminated", m.ErrorKind.PROTOCOL_INCONSISTENCY),
            ("nul", m.ErrorKind.PROTOCOL_INCONSISTENCY),
            ("utf8", m.ErrorKind.INVALID_UTF8),
            ("success-text", m.ErrorKind.PROTOCOL_INCONSISTENCY),
            ("oversized", m.ErrorKind.PROTOCOL_INCONSISTENCY),
        ):
            def submit(*args):
                self.assertEqual(native(*args), n.AMS_MEL_OK)
                if corruption == "nul":
                    return diagnostic(args, text=b"a\0b")
                if corruption == "utf8":
                    return diagnostic(args, text=b"\xff")
                if corruption == "success-text":
                    return diagnostic(args, text=b"unexpected")
                required = ctypes.cast(args[-1], ctypes.POINTER(ctypes.c_size_t))
                required[0] = {"zero": 0, "unterminated": 1,
                               "oversized": args[-2] + 1}[corruption]
                if corruption == "unterminated":
                    ctypes.memmove(args[-3], b"x", 1)
                return n.AMS_MEL_OK

            with self.subTest(corruption=corruption), \
                    mock.patch.object(n, SUBMIT, side_effect=submit) as submitted, \
                    mock.patch.object(n, CLOSE, wraps=getattr(n, CLOSE)) as released:
                self.error(kind, lambda: control.submit_config_set(m.ConfigSetCommand(0)))
                gc.collect()
                submitted.assert_called_once()
                released.assert_called_once()

    def test_request_close_consumes_owner_even_if_cleanup_reports_failure(self):
        _, control = self.open()
        native = getattr(n, CLOSE)

        def close(*args):
            self.assertEqual(native(*args), n.AMS_MEL_OK)
            return diagnostic(args, n.AMS_MEL_INTERNAL_ERROR, b"cleanup failed")

        with mock.patch.object(n, CLOSE, side_effect=close) as release:
            request = self.request(control)
            self.error(m.ErrorKind.INTERNAL_ERROR, request.close)
            self.assertFalse(request.is_open)
            request.close()
            request.__del__()
            release.assert_called_once()

    def test_documented_usage_example(self):
        document = Path(__file__).resolve().parents[2] / "docs/py-mel-010-c2-config-set.md"
        example = document.read_text(encoding="utf-8").split("```python\n", 1)[1].split("```", 1)[0]
        provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        output = io.StringIO()
        with mock.patch("sys.stdout", output), mock.patch.object(
                n, SUBMIT, wraps=getattr(n, SUBMIT)) as submit:
            exec(compile(example, str(document), "exec"), dict(
                provider_path=provider, instance_name="success",
                control_config=control_fixtures.ControlChannelTests.config()))
        submit.assert_called_once()
        self.assertTrue(output.getvalue().strip())

    def test_deterministic_async_lifecycle_and_provider_unload(self):
        # Isolate the explicit test DSO handle, counters, and real unload log.
        if os.environ.get("AMS_MEL_CONFIG_SET_CHILD") != "1":
            result = subprocess.run(
                [sys.executable, "-W", "error", "-m", "unittest",
                 "test_ir_config_set.ConfigSetTests.test_deterministic_async_lifecycle_and_provider_unload"],
                env=dict(os.environ, AMS_MEL_CONFIG_SET_CHILD="1",
                         PYTHONPATH=os.pathsep.join(sys.path)),
                capture_output=True, text=True, timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return
        import _ctypes

        provider_path = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
        provider = ctypes.CDLL(str(provider_path))
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
            self.assertEqual(boundary(1, 3, 0, counts), 1)
            self.assertEqual(owner(1, counts[2], ctypes.byref(observed)), 1)

        with tempfile.TemporaryDirectory(prefix="py010-lifetime-") as temporary, \
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
                               lambda: control.submit_config_set(m.ConfigSetCommand(1)))
                    self.assertEqual(submit.call_count, 2)  # one admission, one refusal
                if close_early:
                    request.close()
                    request.close()
                    self.error(m.ErrorKind.RESOURCE_EXHAUSTED,
                               lambda: control.submit_config_set(m.ConfigSetCommand(2)))
                control.close()
                session.close()
                self.assertNotIn("c2_channel_destroyed", log.read_text())
                self.assertNotIn("library_unloaded", log.read_text())
                self.assertEqual(gate(2, 1, None, None), 1)
                if not close_early:
                    expected = m.ReturnCompleted(m.CommandReturn.SUCCESS)
                    self.assertEqual(request.wait(15000), expected)
                    self.assertEqual(request.wait(0), expected)
                reclaimed()
                request.close()
                request.close()
                # Remove registered test cleanup bound methods before checking
                # that ReturnRequest retains no Python parents.
                self.doCleanups()
                del session, control
                gc.collect()
                self.assertIsNone(session_ref())
                self.assertIsNone(control_ref())
                events = log.read_text()
                self.assertEqual(events.count("config_set_sent"), 1)
                self.assertEqual(events.count("c2_channel_destroyed"), 1)
                self.assertNotIn("library_unloaded", events)  # test CDLL still holds it
                log.write_text("")
            # An admitted provider future exception is an error on Wait, not
            # an admission failure; use the existing deterministic gate flag.
            self.assertEqual(gate(3, 0, None, None), 1)
            session, control = self.open("completion-scale")
            request = self.request(control)
            self.assertEqual(gate(2, 1, None, None), 1)
            error = self.error(m.ErrorKind.PROVIDER_EXCEPTION, lambda: request.wait(15000))
            self.assertIn("held future exception", error.diagnostic)
            reclaimed()
            request.close()
            self.doCleanups()
            # All admitted operations and graph cleanup are synchronized above,
            # not inferred from sleeps. Drop the sole remaining test DSO ref.
            _ctypes.dlclose(provider._handle)
            self.assertIn("library_unloaded", log.read_text())


if __name__ == "__main__":
    unittest.main()
