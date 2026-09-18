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

    def test_private_layer_binds_complete_current_facade(self) -> None:
        self.assertEqual(
            _native.BOUND_FUNCTION_NAMES,
            (
                "ams_mel_get_abi_version",
                "ams_mel_session_open",
                "ams_mel_session_get_provider_version",
                "ams_mel_session_close",
                "ams_mel_ir_stream_open",
                "ams_mel_ir_stream_start",
                "ams_mel_ir_stream_receive",
                "ams_mel_ir_stream_get_counters",
                "ams_mel_ir_stream_stop",
                "ams_mel_ir_stream_close",
                "ams_mel_ir_c2_open",
                "ams_mel_ir_c2_enable",
                "ams_mel_ir_c2_submit_operate",
                "ams_mel_ir_c2_submit_mode",
                "ams_mel_ir_mode_request_wait",
                "ams_mel_ir_mode_request_close",
                "ams_mel_ir_c2_submit_bit_noop",
                "ams_mel_ir_c2_submit_bit",
                "ams_mel_ir_c2_submit_config_set",
                "ams_mel_ir_return_request_wait",
                "ams_mel_ir_return_request_close",
                "ams_mel_ir_c2_close",
            ),
        )
        self.assertEqual(len(_native.BOUND_FUNCTION_NAMES), 22)
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
            _native.AMS_MEL_TIMEOUT,
            _native.AMS_MEL_STREAM_STOPPED,
            _native.AMS_MEL_PROVIDER_FAILED,
            _native.AMS_MEL_COMMAND_REJECTED,
            _native.AMS_MEL_IR_CHANNEL_IRST_IMAGE,
            _native.AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
            _native.AMS_MEL_IR_MFA_MODE_UNUSED,
            _native.AMS_MEL_IR_MFA_MODE_TASK_SCHED,
            _native.AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED,
            _native.AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED,
            *range(_native.AMS_MEL_IR_MFA_STATE_NOT_SET,
                   _native.AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE + 1),
            _native.AMS_MEL_IR_COORD_FRAME_INERTIAL,
            _native.AMS_MEL_IR_COORD_FRAME_AIRCRAFT,
            _native.AMS_MEL_IR_DEGRADATION_CAPACITY,
            _native.AMS_MEL_IR_DEGRADATION_VOLUME,
            _native.AMS_MEL_IR_DEGRADATION_RANGE,
            _native.AMS_MEL_IR_DEGRADATION_REVISIT,
            _native.AMS_MEL_IR_RETURN_SUCCESS,
            _native.AMS_MEL_IR_RETURN_BAD_POINTER,
            _native.AMS_MEL_IR_RETURN_FAIL,
            _native.AMS_MEL_IR_RETURN_NOT_SUPPORTED,
            _native.AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
            _native.AMS_MEL_ERROR_NONE,
            _native.AMS_MEL_ERROR_INVALID_ID,
            _native.AMS_MEL_ERROR_INVALID_STATE,
            _native.AMS_MEL_ERROR_INVALID_PARAMETERS,
            _native.AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS,
            _native.AMS_MEL_ERROR_INSUFFICIENT_RESOURCES,
            _native.AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES,
            _native.AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES,
            _native.AMS_MEL_ERROR_UNSUPPORTED,
            _native.AMS_MEL_IR_PIXEL_MONO,
            _native.AMS_MEL_IR_IMAGE_STARING,
            _native.AMS_MEL_IR_IMAGE_SCANNING,
            _native.AMS_MEL_IR_FLIP_NONE,
            _native.AMS_MEL_IR_FLIP_VERTICAL,
            _native.AMS_MEL_IR_FLIP_HORIZONTAL,
            _native.AMS_MEL_IR_FLIP_BOTH,
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
            self._layout(_native.StringViewV1, "data", "size")
        )
        expected.extend(self._layout(_native.U32SpanV1, "data", "size"))
        expected.extend(self._layout(_native.StringViewSpanV1, "data", "size"))
        expected.extend(self._layout(_native.IrScanTypeV1,
            "continuous_scan", "returning", "agile_scan"))
        expected.extend(self._layout(_native.IrScanParamV1,
            "elevation_defined_with_range_and_altitude", "center_az_rad",
            "center_el_rad", "center_frame_ref_el", "center_frame_ref_az",
            "scan_width_rad", "scan_height_rad", "scan_type", "scan_id",
            "scan_rate_rad_per_second", "preferred_revisit_interval_seconds",
            "required_revisit_interval_seconds", "max_range_of_interest_m",
            "min_range_of_interest_m", "elevation_scan_center_altitude_m",
            "elevation_scan_center_range_m", "degradation_method"))
        expected.extend(self._layout(_native.IrModeCommandV1,
            "command_id", "state", "mode", "scan_parameters"))
        expected.extend(self._layout(_native.IrBitCommandV1,
            "command_id", "initiate_bit_ids", "cancel_bit_ids", "clear_fault_codes"))
        expected.extend(self._layout(_native.IrConfigSetCommandV1,
            "command_id", "system_time_ns", "config"))
        expected.extend(self._layout(_native.UciIdV1, "uuid", "descriptive_label"))
        expected.extend(
            self._layout(
                _native.ComponentLocationV1,
                "offset_x_m", "offset_y_m", "offset_z_m", "key", "system_name",
            )
        )
        expected.extend(
            self._layout(
                _native.IrStreamConfigV1,
                "channel_type", "channel_id", "platform_id", "sensor_location",
                "buffer_count", "buffer_size", "queue_capacity",
            )
        )
        expected.extend(
            self._layout(
                _native.IrC2ConfigV1,
                "channel_type", "channel_id", "platform_id", "sensor_location",
            )
        )
        expected.extend(
            self._layout(_native.IrModeResultV1, "mode", "error_code")
        )
        expected.extend(
            self._layout(_native.IrReturnResultV1, "value", "error_code")
        )
        expected.extend(
            self._layout(
                _native.IrFrameV1,
                "system_time_ns", "integration_time_ns", "width", "height",
                "bits_per_pixel", "number_of_bands", "horizontal_fov_rad",
                "vertical_fov_rad", "pixel_format", "frame_id", "subframe_id",
                "subframe_total", "image_type", "image_flip", "image_flags",
                "dither_row", "dither_column", "row_offset", "column_offset",
                "band_index", "reserved", "pixels", "pixel_capacity",
                "pixel_required",
            )
        )
        expected.extend(
            self._layout(
                _native.IrStreamCountersV1,
                "frames_received", "frames_dropped_queue_full",
                "malformed_or_unsupported_frames",
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
