from __future__ import annotations

import gc
import ctypes
import os
from pathlib import Path
import tempfile
import unittest

from ams_mel import ErrorKind, MelError, ProviderVersion, Session


SUCCESS_LIFECYCLE = (
    "manager_factory_called\n"
    "control_factory_called\n"
    "init_called\n"
    "control_destroyed\n"
    "manager_destroyed\n"
    "library_unloaded\n"
)


class SessionTests(unittest.TestCase):
    def setUp(self) -> None:
        provider_directory = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"])
        self.mock_provider = provider_directory / "libmock_ir_provider.so"
        self.missing_symbol_provider = (
            provider_directory / "libmissing_symbol_ir_provider.so"
        )
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="ams-mel-python-session-"
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

    def test_direct_construction_is_rejected(self) -> None:
        with self.assertRaisesRegex(TypeError, "Session.open"):
            Session()

    def test_open_version_explicit_close_and_repeated_close(self) -> None:
        session = Session.open(self.mock_provider, "success", "aperture-A")
        self.assertTrue(session.is_open)
        self.assertEqual(
            session.provider_version(),
            ProviderVersion(
                api_version=0x12345678,
                library_version=0x90ABCDEF,
                vendor="Mock IR Provider µ",
                description="Deterministic task 001 provider",
            ),
        )
        session.close()
        self.assertFalse(session.is_open)
        session.close()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)
        with self.assertRaises(MelError) as caught:
            session.provider_version()
        self.assertEqual(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)

    def test_context_manager_closes_and_unloads(self) -> None:
        with Session.open(self.mock_provider, "success", "aperture-A") as session:
            self.assertTrue(session.is_open)
            self.assertEqual(self.lifecycle().splitlines()[-1], "init_called")
        self.assertFalse(session.is_open)
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_context_manager_preserves_body_exception(self) -> None:
        session = Session.open(self.mock_provider, "success", "aperture-A")
        native_close = session.close

        def failed_close() -> None:
            raise MelError(ErrorKind.PROVIDER_EXCEPTION, "cleanup failed", 15, 6)

        session.close = failed_close  # type: ignore[method-assign]
        with self.assertRaisesRegex(ValueError, "body failed"):
            with session:
                raise ValueError("body failed")
        session.close = native_close  # type: ignore[method-assign]
        session.close()

    def test_context_manager_propagates_close_error_without_body_error(self) -> None:
        session = Session.open(self.mock_provider, "success", "aperture-A")
        native_close = session.close

        def failed_close() -> None:
            raise MelError(ErrorKind.PROVIDER_EXCEPTION, "cleanup failed", 15, 6)

        session.close = failed_close  # type: ignore[method-assign]
        with self.assertRaises(MelError) as caught:
            with session:
                pass
        self.assertEqual(caught.exception.diagnostic, "cleanup failed")
        session.close = native_close  # type: ignore[method-assign]
        session.close()

    def test_close_error_reflects_cleared_native_owner(self) -> None:
        session = Session.open(self.mock_provider, "success", "aperture-A")
        native_close_function = session._close_function

        def failing_native_close(
            owner: object, diagnostic: object, capacity: int, required: object
        ) -> int:
            native_status = native_close_function(owner, None, 0, None)
            self.assertEqual(native_status, 0)
            message = b"cleanup failed\0"
            self.assertGreaterEqual(capacity, len(message))
            ctypes.memmove(diagnostic, message, len(message))
            ctypes.cast(
                required, ctypes.POINTER(ctypes.c_size_t)
            ).contents.value = len(message)
            return 6

        session._close_function = failing_native_close
        with self.assertRaises(MelError) as caught:
            session.close()
        self.assertEqual(caught.exception.kind, ErrorKind.PROVIDER_EXCEPTION)
        self.assertFalse(session.is_open)
        session.close()
        session._close_function = native_close_function
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_finalizer_releases_provider_deterministically(self) -> None:
        session = Session.open(self.mock_provider, "success", "aperture-A")
        self.assertTrue(session.is_open)
        del session
        gc.collect()
        self.assertEqual(self.lifecycle(), SUCCESS_LIFECYCLE)

    def test_open_failure_kinds_are_preserved(self) -> None:
        cases = [
            (
                "/definitely/missing/libirmel.so",
                "x",
                ErrorKind.LIBRARY_LOAD_FAILED,
            ),
            (self.missing_symbol_provider, "x", ErrorKind.SYMBOL_NOT_FOUND),
            (self.mock_provider, "init-fail", ErrorKind.INITIALIZATION_FAILED),
        ]
        for provider, instance, expected in cases:
            with self.subTest(expected=expected):
                with self.assertRaises(MelError) as caught:
                    Session.open(provider, instance, "aperture-A")
                self.assertEqual(caught.exception.kind, expected)
                self.assertIsNotNone(caught.exception.diagnostic)
                self.assertIsNotNone(caught.exception.diagnostic_required)

    def test_embedded_nul_inputs_are_rejected_before_provider_activity(self) -> None:
        cases = [
            (f"{self.mock_provider}\0suffix", "success", "aperture-A"),
            (self.mock_provider, "success\0suffix", "aperture-A"),
            (self.mock_provider, "success", "aperture-A\0suffix"),
        ]
        for provider, instance, aperture in cases:
            with self.subTest(provider=provider, instance=instance, aperture=aperture):
                with self.assertRaises(MelError) as caught:
                    Session.open(provider, instance, aperture)
                self.assertEqual(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
                self.assertEqual(self.lifecycle(), "")

    def test_surrogate_inputs_are_rejected_before_provider_activity(self) -> None:
        for provider, instance, aperture in [
            (f"{self.mock_provider}\ud800", "success", "aperture-A"),
            (self.mock_provider, "success\ud800", "aperture-A"),
            (self.mock_provider, "success", "aperture-A\ud800"),
        ]:
            with self.subTest(instance=repr(instance), aperture=repr(aperture)):
                with self.assertRaises(MelError) as caught:
                    Session.open(provider, instance, aperture)
                self.assertEqual(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
                self.assertEqual(self.lifecycle(), "")

    def test_bytes_provider_path_is_rejected(self) -> None:
        with self.assertRaises(MelError) as caught:
            Session.open(os.fsencode(self.mock_provider), "success", "aperture-A")
        self.assertEqual(caught.exception.kind, ErrorKind.INVALID_ARGUMENT)
        self.assertEqual(self.lifecycle(), "")

    def test_invalid_provider_version_utf8_preserves_native_failure(self) -> None:
        with Session.open(
            self.mock_provider, "invalid-utf8-version", "aperture-A"
        ) as session:
            with self.assertRaises(MelError) as caught:
                session.provider_version()
        error = caught.exception
        self.assertEqual(error.kind, ErrorKind.PROVIDER_EXCEPTION)
        self.assertEqual(
            error.diagnostic, "provider version contains invalid UTF-8 or NUL"
        )
        self.assertEqual(error.native_status, 6)

    def test_oversized_diagnostic_has_no_truncated_text(self) -> None:
        with Session.open(
            self.mock_provider, "throw-version-oversized", "aperture-A"
        ) as session:
            with self.assertRaises(MelError) as caught:
                session.provider_version()
        error = caught.exception
        self.assertEqual(error.kind, ErrorKind.PROVIDER_EXCEPTION)
        self.assertEqual(error.native_status, 6)
        self.assertIsNone(error.diagnostic)
        self.assertEqual(error.diagnostic_required, 5003)


if __name__ == "__main__":
    unittest.main()
