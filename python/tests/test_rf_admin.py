from __future__ import annotations

import _ctypes
from contextlib import contextmanager
import ctypes
import gc
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest
from unittest import mock
import weakref

import ams_mel
from ams_mel import ErrorKind, MFAState, MelError, RFAdmin, _native


SUCCESS_LIFECYCLE = [
    "rf_admin_factory_called",
    "rf_admin_shutdown",
    "rf_admin_destroyed",
    "library_unloaded",
]


class RFAdminTests(unittest.TestCase):
    def setUp(self) -> None:
        provider_directory = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"])
        self.provider = provider_directory / "libmock_rf_provider.so"
        self.missing_symbol = provider_directory / "libmissing_symbol_rf_provider.so"
        temporary = tempfile.TemporaryDirectory(prefix="ams-mel-python-rf-admin-")
        self.addCleanup(temporary.cleanup)
        self.log = Path(temporary.name) / "lifetime.log"
        # The RF mock deliberately opens without O_CREAT.
        self.log.touch()
        environment = mock.patch.dict(
            os.environ, {"AMS_MEL_TEST_LIFETIME_LOG": str(self.log)}
        )
        environment.start()
        self.addCleanup(environment.stop)

    def lifecycle(self) -> list[str]:
        return self.log.read_text(encoding="utf-8").splitlines()

    @contextmanager
    def observations(self):
        # These are existing TEST-only C exports, not provider C++ interfaces.
        # Drop this extra loader reference explicitly before checking DSO unload.
        library = ctypes.CDLL(str(self.provider))
        for name in ("command_calls", "shutdown_calls", "last_state"):
            function = getattr(library, f"mock_rf_admin_{name}")
            function.argtypes = []
            function.restype = ctypes.c_uint32
        try:
            yield library
        finally:
            _ctypes.dlclose(library._handle)

    def isolated(self, body: str) -> None:
        # Uncertain shutdown intentionally retains the native graph for process
        # lifetime. Never let such retention mask unload checks in other tests.
        preamble = """
import gc
import os
from pathlib import Path
import sys
import unittest
import weakref
from ams_mel import ErrorKind, MFAState, MelError, RFAdmin
self = unittest.TestCase()
provider = Path(os.environ['AMS_MEL_TEST_PROVIDER_DIR']) / 'libmock_rf_provider.so'
log = Path(os.environ['AMS_MEL_TEST_LIFETIME_LOG'])
def events():
    return log.read_text(encoding='utf-8').splitlines()
"""
        result = subprocess.run(
            [sys.executable, "-W", "error", "-c", preamble + textwrap.dedent(body)],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")

    def test_public_api_and_direct_construction(self) -> None:
        self.assertIn("RFAdmin", ams_mel.__all__)
        self.assertIn("MFAState", ams_mel.__all__)
        with self.assertRaisesRegex(TypeError, "RFAdmin.open"):
            RFAdmin()
        # A partly initialized finalizer is harmless too.
        RFAdmin.__new__(RFAdmin).__del__()

    def test_open_close_and_repeated_close(self) -> None:
        admin = RFAdmin.open(str(self.provider), "admin:ok")
        self.assertIs(admin.is_open, True)
        self.assertEqual(self.lifecycle(), ["rf_admin_factory_called"])
        admin.close()
        self.assertIs(admin.is_open, False)
        admin.close()
        admin.__del__()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_all_fifteen_exact_representations_reach_provider(self) -> None:
        names = [
            "NOT_SET", "UNKNOWN", "NOT_INSTALLED", "OFF", "PRE_INITIALIZATION",
            "INITIALIZATION", "STANDBY", "OPERATE", "OPERATE_RX_ONLY",
            "OPERATE_TX_ONLY", "MAINTENANCE", "CALIBRATION", "INITIATED_BIT",
            "SHUTDOWN", "DEGRADED",
        ]
        self.assertEqual(
            [(s.name, int(s)) for s in MFAState], list(zip(names, range(15)))
        )
        with self.observations() as observation:
            with RFAdmin.open(self.provider, "admin:ok") as admin:
                for state in MFAState:
                    with self.subTest(state=state):
                        self.assertIs(admin.command_state(state), True)
                        self.assertEqual(
                            observation.mock_rf_admin_last_state(), int(state)
                        )
                        self.assertIs(admin.command_state(int(state)), True)
                        self.assertEqual(
                            observation.mock_rf_admin_last_state(), int(state)
                        )
                self.assertEqual(observation.mock_rf_admin_command_calls(), 30)
            self.assertEqual(observation.mock_rf_admin_shutdown_calls(), 1)
        self.assertEqual(self.lifecycle()[-3:], SUCCESS_LIFECYCLE[-3:])

    def test_provider_rejection_is_false_not_failure(self) -> None:
        with RFAdmin.open(self.provider, "admin:reject") as admin:
            for state in MFAState:
                self.assertIs(admin.command_state(state), False)
            self.assertTrue(admin.is_open)

    def test_invalid_states_never_call_native(self) -> None:
        invalid = [
            -1, 15, 16, (1 << 32) - 1, 1 << 32, 1 << 100, -(1 << 100),
            True, False, 6.0, "6", b"6", None, object(), ctypes.c_uint32(6),
        ]
        with RFAdmin.open(self.provider, "admin:ok") as admin:
            with mock.patch.object(_native, "ams_mel_rf_admin_command_state") as call:
                for state in invalid:
                    with self.subTest(state=state):
                        with self.assertRaises(MelError) as caught:
                            admin.command_state(state)
                        self.assertIs(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
                call.assert_not_called()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_missing_provider_library(self) -> None:
        with self.assertRaises(MelError) as caught:
            RFAdmin.open(self.log.parent / "missing.so", "admin:ok")
        error = caught.exception
        self.assertIs(error.kind, ErrorKind.LIBRARY_LOAD_FAILED)
        self.assertEqual(error.native_status, _native.AMS_MEL_LIBRARY_LOAD_FAILED)
        self.assertIn("missing.so", error.diagnostic)
        self.assertEqual(
            error.diagnostic_required, len(error.diagnostic.encode("utf-8")) + 1
        )
        self.assertEqual(self.lifecycle(), [])

    def test_missing_factory_symbol_unloads_without_owner(self) -> None:
        with self.assertRaises(MelError) as caught:
            RFAdmin.open(self.missing_symbol, "admin:ok")
        self.assertIs(caught.exception.kind, ErrorKind.SYMBOL_NOT_FOUND)
        self.assertIn("createAdminMEL", caught.exception.diagnostic)
        self.assertEqual(self.lifecycle(), [])
        # This fixture has no unload log hook; NOLOAD must fail after rollback.
        with self.assertRaises(OSError):
            ctypes.CDLL(str(self.missing_symbol), mode=os.RTLD_NOLOAD)

    def test_factory_failures_release_library_and_publish_no_owner(self) -> None:
        cases = [
            ("admin:factory-null", ErrorKind.FACTORY_FAILED,
             _native.AMS_MEL_FACTORY_FAILED, "createAdminMEL returned null"),
            ("admin:factory-throw", ErrorKind.PROVIDER_EXCEPTION,
             _native.AMS_MEL_PROVIDER_EXCEPTION, "mock RF factory exception"),
            ("admin:factory-throw-unknown", ErrorKind.PROVIDER_EXCEPTION,
             _native.AMS_MEL_PROVIDER_EXCEPTION, "unknown provider factory exception"),
        ]
        for scenario, kind, status, diagnostic in cases:
            with self.subTest(scenario=scenario):
                self.log.write_text("", encoding="utf-8")
                with self.assertRaises(MelError) as caught:
                    RFAdmin.open(self.provider, scenario)
                self.assert_error(caught.exception, kind, status, diagnostic)
                gc.collect()
                self.assertEqual(
                    self.lifecycle(), ["rf_admin_factory_called", "library_unloaded"]
                )

    def assert_error(self, error, kind, status, diagnostic) -> None:
        self.assertIs(error.kind, kind)
        self.assertEqual(error.native_status, status)
        self.assertEqual(error.diagnostic, diagnostic)
        self.assertEqual(error.diagnostic_required, len(diagnostic.encode("utf-8")) + 1)

    def test_missing_interfaces_and_command_exceptions(self) -> None:
        cases = [
            ("admin:no-uci", ErrorKind.PROVIDER_FAILED, _native.AMS_MEL_PROVIDER_FAILED,
             "AdminMEL::getUCIControl returned null"),
            ("admin:no-status", ErrorKind.PROVIDER_FAILED,
             _native.AMS_MEL_PROVIDER_FAILED,
             "UCI_Control::getStatusControl returned null"),
            ("admin:command-throw", ErrorKind.PROVIDER_EXCEPTION,
             _native.AMS_MEL_PROVIDER_EXCEPTION, "mock Admin command exception"),
        ]
        for scenario, kind, status, diagnostic in cases:
            with self.subTest(scenario=scenario):
                self.log.write_text("", encoding="utf-8")
                with RFAdmin.open(self.provider, scenario) as admin:
                    with self.assertRaises(MelError) as caught:
                        admin.command_state(MFAState.STANDBY)
                    self.assert_error(caught.exception, kind, status, diagnostic)
                    self.assertTrue(admin.is_open)
                events = self.lifecycle()
                self.assertEqual(events[0], "rf_admin_factory_called")
                self.assertEqual(events[-3:], SUCCESS_LIFECYCLE[-3:])

    def test_shutdown_failure_consumes_handle_without_retry_or_unload(self) -> None:
        self.isolated("""
            admin = RFAdmin.open(provider, 'admin:shutdown-throw')
            with self.assertRaises(MelError) as caught:
                admin.close()
            error = caught.exception
            self.assertIs(error.kind, ErrorKind.PROVIDER_EXCEPTION)
            self.assertEqual(error.native_status, 6)
            self.assertEqual(error.diagnostic, 'mock Admin shutdown exception')
            self.assertEqual(error.diagnostic_required, 30)
            self.assertFalse(admin.is_open)
            admin.close()
            admin.__del__()
            ref = weakref.ref(admin)
            del admin
            gc.collect()
            self.assertIsNone(ref())
            self.assertEqual(events(), ['rf_admin_factory_called', 'rf_admin_shutdown'])
        """)

    def test_context_manager_normal_exit(self) -> None:
        with RFAdmin.open(self.provider, "admin:ok") as admin:
            self.assertTrue(admin.is_open)
            self.assertIs(admin.command_state(MFAState.STANDBY), True)
        self.assertFalse(admin.is_open)
        self.assertEqual(self.lifecycle()[-3:], SUCCESS_LIFECYCLE[-3:])

    def test_context_body_exception_survives_successful_cleanup(self) -> None:
        body_error = ValueError("body failed")
        with self.assertRaises(ValueError) as caught:
            with RFAdmin.open(self.provider, "admin:ok") as admin:
                raise body_error
        self.assertIs(caught.exception, body_error)
        self.assertFalse(admin.is_open)
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_context_base_exception_survives_shutdown_failure(self) -> None:
        self.isolated("""
            body_error = KeyboardInterrupt('body failed')
            with self.assertRaises(KeyboardInterrupt) as caught:
                with RFAdmin.open(provider, 'admin:shutdown-throw') as admin:
                    raise body_error
            self.assertIs(caught.exception, body_error)
            self.assertFalse(admin.is_open)
            admin.close()
            self.assertEqual(events(), ['rf_admin_factory_called', 'rf_admin_shutdown'])
        """)

    def test_context_manager_reports_shutdown_failure_on_normal_exit(self) -> None:
        self.isolated("""
            with self.assertRaises(MelError) as caught:
                with RFAdmin.open(provider, 'admin:shutdown-throw') as admin:
                    pass
            self.assertIs(caught.exception.kind, ErrorKind.PROVIDER_EXCEPTION)
            self.assertFalse(admin.is_open)
            admin.close()
            self.assertEqual(events(), ['rf_admin_factory_called', 'rf_admin_shutdown'])
        """)

    def test_automatic_finalization(self) -> None:
        admin = RFAdmin.open(self.provider, "admin:ok")
        reference = weakref.ref(admin)
        del admin
        gc.collect()
        self.assertIsNone(reference())
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_automatic_finalization_suppresses_shutdown_failure(self) -> None:
        self.isolated("""
            unraisable = []
            sys.unraisablehook = unraisable.append
            admin = RFAdmin.open(provider, 'admin:shutdown-throw')
            reference = weakref.ref(admin)
            del admin
            gc.collect()
            self.assertIsNone(reference())
            self.assertEqual(unraisable, [])
            self.assertEqual(events(), ['rf_admin_factory_called', 'rf_admin_shutdown'])
        """)

    def test_finalizer_suppresses_python_base_exception(self) -> None:
        admin = RFAdmin.open(self.provider, "admin:ok")
        native_close = admin._close_function
        with mock.patch.object(admin, "_close_function", side_effect=KeyboardInterrupt):
            admin.__del__()
        admin._close_function = native_close
        admin.close()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_closed_owner_operations_do_not_call_native(self) -> None:
        admin = RFAdmin.open(self.provider, "admin:ok")
        admin.close()
        with mock.patch.object(_native, "ams_mel_rf_admin_command_state") as call:
            for operation in (
                lambda: admin.command_state(MFAState.OFF), admin.__enter__
            ):
                with self.assertRaises(MelError) as caught:
                    operation()
                self.assertIs(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
            call.assert_not_called()

    def test_invalid_paths_and_text_never_call_native(self) -> None:
        class BytesPath:
            def __fspath__(self):
                return b"provider.so"

        class InvalidPath:
            def __fspath__(self):
                return 42

        class NulPath:
            def __fspath__(self):
                return "provider\0.so"

        cases = [(path, "admin:ok") for path in (
            None, 3, object(), b"provider.so", BytesPath(), InvalidPath(), NulPath(),
            str(self.provider) + "\0tail", str(self.provider) + "\ud800",
        )]
        cases += [(self.provider, text) for text in (
            None, 1, b"admin:ok", object(), "admin:ok\0tail", "\ud800", "\udfff",
        )]
        with mock.patch.object(_native, "ams_mel_rf_admin_open") as call:
            for path, configuration in cases:
                with self.subTest(path=path, configuration=configuration):
                    with self.assertRaises(MelError) as caught:
                        RFAdmin.open(path, configuration)
                    self.assertIs(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
            call.assert_not_called()
        self.assertEqual(self.lifecycle(), [])

    def test_pathlike_and_utf8_boundary_inputs(self) -> None:
        # Includes UTF-8 encoding width boundaries and the largest Unicode scalar.
        text = "\x7f\u0080\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff"
        path = self.log.parent / (text + ".so")
        path.symlink_to(self.provider)

        class TextPath:
            def __fspath__(self):
                return str(path)

        native_open = _native.ams_mel_rf_admin_open
        with mock.patch.object(
            _native, "ams_mel_rf_admin_open", wraps=native_open
        ) as call:
            with RFAdmin.open(TextPath(), text) as admin:
                self.assertIs(admin.command_state(MFAState.UNKNOWN), True)
            self.assertEqual(call.call_args.args[0], str(path).encode("utf-8"))
            self.assertEqual(call.call_args.args[1], text.encode("utf-8"))
            self.assertEqual(call.call_count, 1)
        self.assertEqual(self.lifecycle()[-3:], SUCCESS_LIFECYCLE[-3:])

    def test_empty_configuration_is_forwarded(self) -> None:
        with RFAdmin.open(self.provider, "") as admin:
            self.assertIs(admin.command_state(MFAState.NOT_SET), True)

    def test_independent_owners_no_session_or_parent_dependency(self) -> None:
        first = RFAdmin.open(self.provider, "admin:ok")
        second = RFAdmin.open(self.provider, "admin:reject")
        first.close()
        self.assertIs(second.command_state(MFAState.OPERATE), False)
        self.assertNotIn("library_unloaded", self.lifecycle())
        second.close()
        self.assertEqual(self.lifecycle().count("rf_admin_shutdown"), 2)
        self.assertEqual(self.lifecycle().count("rf_admin_destroyed"), 2)
        self.assertEqual(self.lifecycle()[-1], "library_unloaded")

    def test_malformed_or_unwritten_native_boolean(self) -> None:
        native_command = _native.ams_mel_rf_admin_command_state
        with RFAdmin.open(self.provider, "admin:ok") as admin:
            for value in (2, 255, (1 << 32) - 1, None):
                def corrupt(owner, state, accepted, diagnostic, capacity, required):
                    discarded = ctypes.c_uint32()
                    output = ctypes.byref(discarded) if value is None else accepted
                    result = native_command(
                        owner, state, output, diagnostic, capacity, required
                    )
                    if value is not None:
                        result_pointer = ctypes.cast(
                            accepted, ctypes.POINTER(ctypes.c_uint32)
                        )
                        result_pointer[0] = value
                    return result

                with self.subTest(value=value):
                    with mock.patch.object(
                        _native, "ams_mel_rf_admin_command_state", corrupt
                    ):
                        with self.assertRaises(MelError) as caught:
                            admin.command_state(MFAState.STANDBY)
                    self.assertIs(
                        caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY
                    )
            self.assertIs(admin.command_state(MFAState.STANDBY), True)

    def test_success_without_native_owner_is_protocol_error(self) -> None:
        def empty_open(path, configuration, owner, diagnostic, capacity, required):
            diagnostic[0] = b"\0"
            ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 1
            return _native.AMS_MEL_OK

        with mock.patch.object(_native, "ams_mel_rf_admin_open", empty_open):
            with self.assertRaises(MelError) as caught:
                RFAdmin.open(self.provider, "admin:ok")
        self.assertIs(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
        self.assertEqual(self.lifecycle(), [])

    def test_partial_open_diagnostic_failure_closes_native_owner(self) -> None:
        native_open = _native.ams_mel_rf_admin_open
        for payload, kind in (
            (b"\xff\0", ErrorKind.INVALID_UTF8),
            (b"unterminated", ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"a\0b\0", ErrorKind.PROTOCOL_INCONSISTENCY),
            (b"unexpected\0", ErrorKind.PROTOCOL_INCONSISTENCY),
        ):
            self.log.write_text("", encoding="utf-8")

            def corrupt(path, configuration, owner, diagnostic, capacity, required):
                result = native_open(
                    path, configuration, owner, diagnostic, capacity, required
                )
                ctypes.memmove(diagnostic, payload, len(payload))
                ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(payload)
                return result

            with self.subTest(payload=payload):
                with mock.patch.object(_native, "ams_mel_rf_admin_open", corrupt):
                    with self.assertRaises(MelError) as caught:
                        RFAdmin.open(self.provider, "admin:ok")
                self.assertIs(caught.exception.kind, kind)
                gc.collect()
                self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_complete_utf8_diagnostic_at_capacity_boundary(self) -> None:
        native_command = _native.ams_mel_rf_admin_command_state
        diagnostic_text = "x" * 4091 + "\U00010000"
        payload = diagnostic_text.encode("utf-8") + b"\0"
        self.assertEqual(len(payload), 4096)

        def diagnostic_command(owner, state, accepted, diagnostic, capacity, required):
            native_command(owner, state, accepted, diagnostic, capacity, required)
            ctypes.memmove(diagnostic, payload, len(payload))
            ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = len(payload)
            return _native.AMS_MEL_PROVIDER_EXCEPTION

        with RFAdmin.open(self.provider, "admin:ok") as admin:
            with mock.patch.object(
                _native, "ams_mel_rf_admin_command_state", diagnostic_command
            ):
                with self.assertRaises(MelError) as caught:
                    admin.command_state(MFAState.STANDBY)
            self.assert_error(
                caught.exception, ErrorKind.PROVIDER_EXCEPTION,
                _native.AMS_MEL_PROVIDER_EXCEPTION, diagnostic_text
            )
        self.assertEqual(self.lifecycle().count("rf_admin_command"), 1)

    def test_oversized_diagnostic_preserves_size_and_status_without_retry(self) -> None:
        # Existing ABI has no post-call diagnostic retrieval operation. Do not
        # repeat a side-effecting command or present a truncated message as whole.
        native_command = _native.ams_mel_rf_admin_command_state

        def oversized(owner, state, accepted, diagnostic, capacity, required):
            native_command(owner, state, accepted, diagnostic, capacity, required)
            ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = 5003
            return _native.AMS_MEL_PROVIDER_EXCEPTION

        with RFAdmin.open(self.provider, "admin:ok") as admin:
            with mock.patch.object(
                _native, "ams_mel_rf_admin_command_state", oversized
            ):
                with self.assertRaises(MelError) as caught:
                    admin.command_state(MFAState.STANDBY)
            self.assertIs(caught.exception.kind, ErrorKind.PROVIDER_EXCEPTION)
            self.assertEqual(
                caught.exception.native_status, _native.AMS_MEL_PROVIDER_EXCEPTION
            )
            self.assertEqual(caught.exception.diagnostic_required, 5003)
            self.assertIsNone(caught.exception.diagnostic)
        self.assertEqual(self.lifecycle().count("rf_admin_command"), 1)

    def test_partial_open_failure_preserved_when_cleanup_shutdown_throws(self) -> None:
        self.isolated("""
            import ctypes
            from unittest import mock
            from ams_mel import _native
            native_open = _native.ams_mel_rf_admin_open
            def corrupt(path, configuration, owner, diagnostic, capacity, required):
                result = native_open(
                    path, configuration, owner, diagnostic, capacity, required
                )
                diagnostic[0] = b'X'
                return result
            with mock.patch.object(_native, 'ams_mel_rf_admin_open', corrupt):
                with self.assertRaises(MelError) as caught:
                    RFAdmin.open(provider, 'admin:shutdown-throw')
            self.assertIs(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
            gc.collect()
            self.assertEqual(events(), ['rf_admin_factory_called', 'rf_admin_shutdown'])
        """)

    def test_unknown_status_and_invalid_diagnostic_size_boundaries(self) -> None:
        native_command = _native.ams_mel_rf_admin_command_state
        cases = [
            (-1234, 1, ErrorKind.UNKNOWN),
            (_native.AMS_MEL_OK, 0, ErrorKind.PROTOCOL_INCONSISTENCY),
            (_native.AMS_MEL_OK, 4097, ErrorKind.PROTOCOL_INCONSISTENCY),
        ]
        with RFAdmin.open(self.provider, "admin:ok") as admin:
            for status, size, kind in cases:
                def corrupt(owner, state, accepted, diagnostic, capacity, required):
                    native_command(
                        owner, state, accepted, diagnostic, capacity, required
                    )
                    ctypes.cast(required, ctypes.POINTER(ctypes.c_size_t))[0] = size
                    return status

                with self.subTest(status=status, size=size):
                    with mock.patch.object(
                        _native, "ams_mel_rf_admin_command_state", corrupt
                    ):
                        with self.assertRaises(MelError) as caught:
                            admin.command_state(MFAState.STANDBY)
                    self.assertIs(caught.exception.kind, kind)
                    if kind is ErrorKind.UNKNOWN:
                        self.assertEqual(caught.exception.native_status, status)

    def test_consuming_close_diagnostic_error_never_retries_shutdown(self) -> None:
        admin = RFAdmin.open(self.provider, "admin:ok")
        native_close = admin._close_function

        def corrupt(owner, diagnostic, capacity, required):
            result = native_close(owner, diagnostic, capacity, required)
            diagnostic[0] = b"X"
            return result

        admin._close_function = corrupt
        with self.assertRaises(MelError) as caught:
            admin.close()
        self.assertIs(caught.exception.kind, ErrorKind.PROTOCOL_INCONSISTENCY)
        self.assertFalse(admin.is_open)
        admin.close()
        admin.__del__()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)


if __name__ == "__main__":
    unittest.main()
