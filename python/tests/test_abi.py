from __future__ import annotations

import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from ams_mel import AbiVersion, abi_version
from ams_mel import _native


class AbiTests(unittest.TestCase):
    def test_reports_exact_facade_version(self) -> None:
        self.assertEqual(abi_version(), AbiVersion(major=0, minor=1))

    def test_private_layer_binds_only_task_010_functions(self) -> None:
        self.assertEqual(
            _native.BOUND_FUNCTION_NAMES,
            (
                "ams_mel_get_abi_version",
                "ams_mel_session_open",
                "ams_mel_session_get_provider_version",
                "ams_mel_session_close",
            ),
        )
        for name in _native.BOUND_FUNCTION_NAMES:
            function = getattr(_native, name)
            self.assertIsNotNone(function.argtypes)
            self.assertIs(function.restype, ctypes.c_int32)

    def test_ctypes_declarations_match_authoritative_c_header(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        native_library = Path(os.environ["AMS_MEL_NATIVE_LIB"]).resolve()
        compiler = os.environ.get("CC", "cc")
        with tempfile.TemporaryDirectory(prefix="ams-mel-python-abi-") as directory:
            probe = Path(directory) / "abi_probe"
            compile_result = subprocess.run(
                [
                    compiler,
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Wpedantic",
                    "-Werror",
                    f"-I{repository / 'native/include'}",
                    str(Path(__file__).with_name("abi_probe.c")),
                    str(native_library),
                    f"-Wl,-rpath,{native_library.parent}",
                    "-o",
                    str(probe),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            probe_result = subprocess.run(
                [str(probe)], check=False, capture_output=True, text=True
            )
            self.assertEqual(probe_result.returncode, 0, probe_result.stderr)

        actual = [int(value) for value in probe_result.stdout.split()]
        expected = [
            _native.AMS_MEL_OK,
            _native.AMS_MEL_INVALID_ARGUMENT,
            _native.AMS_MEL_LIBRARY_LOAD_FAILED,
            _native.AMS_MEL_SYMBOL_NOT_FOUND,
            _native.AMS_MEL_FACTORY_FAILED,
            _native.AMS_MEL_INITIALIZATION_FAILED,
            _native.AMS_MEL_PROVIDER_EXCEPTION,
            _native.AMS_MEL_BUFFER_TOO_SMALL,
            _native.AMS_MEL_INTERNAL_ERROR,
            _native.AMS_MEL_PROVIDER_FAILED,
        ]
        expected.extend(self._layout(_native.AbiVersionV1, "major", "minor"))
        expected.extend(
            self._layout(
                _native.ProviderVersionV1,
                "api_version",
                "library_version",
                "vendor",
                "vendor_capacity",
                "vendor_required",
                "description",
                "description_capacity",
                "description_required",
            )
        )
        expected.extend(
            [
                _native.AMS_MEL_OK,
                _native.AMS_MEL_ABI_VERSION_MAJOR,
                _native.AMS_MEL_ABI_VERSION_MINOR,
                _native.AMS_MEL_ABI_VERSION_MAJOR,
                _native.AMS_MEL_ABI_VERSION_MINOR,
            ]
        )
        self.assertEqual(actual, expected)

    @staticmethod
    def _layout(structure: type[ctypes.Structure], *fields: str) -> list[int]:
        values = [ctypes.sizeof(structure), ctypes.alignment(structure)]
        values.extend(getattr(structure, field).offset for field in fields)
        return values


if __name__ == "__main__":
    unittest.main()
