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
                "ams_mel_ir_stream_get_capabilities",
                "ams_mel_ir_image_metadata_open",
                "ams_mel_ir_image_metadata_receive",
                "ams_mel_ir_image_metadata_get_counters",
                "ams_mel_ir_image_metadata_close",
                "ams_mel_ir_image_metadata_event_view",
                "ams_mel_ir_image_metadata_event_close",
                "ams_mel_ir_stream_submit_navigation_report",
                "ams_mel_ir_navigation_request_wait",
                "ams_mel_ir_navigation_request_close",
                "ams_mel_ir_stream_receive_snapshot",
                "ams_mel_ir_frame_snapshot_view",
                "ams_mel_ir_frame_snapshot_close",
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
                "ams_mel_ir_c2_send_keepalive",
                "ams_mel_ir_c2_submit_comms_test",
                "ams_mel_ir_channel_comms_request_wait",
                "ams_mel_ir_channel_comms_request_close",
                "ams_mel_ir_c2_get_capabilities",
                "ams_mel_ir_channel_capability_view",
                "ams_mel_ir_channel_capability_close",
                "ams_mel_ir_return_request_wait",
                "ams_mel_ir_return_request_close",
                "ams_mel_ir_c2_close",
                "ams_mel_ir_c2_metadata_open",
                "ams_mel_ir_c2_metadata_register_comms_test",
                "ams_mel_ir_c2_metadata_receive",
                "ams_mel_ir_c2_metadata_get_counters",
                "ams_mel_ir_c2_metadata_close",
                "ams_mel_ir_c2_metadata_event_view",
                "ams_mel_ir_c2_metadata_event_close",
                "ams_mel_ir_health_open",
                "ams_mel_ir_health_enable",
                "ams_mel_ir_health_get_capabilities",
                "ams_mel_ir_health_close",
                "ams_mel_ir_health_metadata_open",
                "ams_mel_ir_health_metadata_receive",
                "ams_mel_ir_health_metadata_get_counters",
                "ams_mel_ir_health_metadata_close",
                "ams_mel_ir_health_metadata_event_view",
                "ams_mel_ir_health_metadata_event_close",
                "ams_mel_ir_instrumentation_open",
                "ams_mel_ir_instrumentation_enable",
                "ams_mel_ir_instrumentation_get_capabilities",
                "ams_mel_ir_instrumentation_submit_level",
                "ams_mel_ir_instrumentation_request_wait",
                "ams_mel_ir_instrumentation_request_close",
                "ams_mel_ir_instrumentation_metadata_open",
                "ams_mel_ir_instrumentation_metadata_receive",
                "ams_mel_ir_instrumentation_metadata_get_counters",
                "ams_mel_ir_instrumentation_metadata_close",
                "ams_mel_ir_instrumentation_metadata_event_view",
                "ams_mel_ir_instrumentation_metadata_event_close",
                "ams_mel_ir_instrumentation_close",
                "ams_mel_ir_track_open",
                "ams_mel_ir_track_enable",
                "ams_mel_ir_track_get_capabilities",
                "ams_mel_ir_track_close",
            ),
        )
        self.assertEqual(len(_native.BOUND_FUNCTION_NAMES), 76)
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
        expected.extend(self._layout(_native.U8SpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrContributingSensorV1, 'location', 'sensor_id'))
        expected.extend(self._layout(_native.IrDirectionalV1, 'x', 'y', 'z'))
        expected.extend(self._layout(_native.IrQuaternionV1, 'x', 'y', 'z', 'w'))
        expected.extend(self._layout(_native.IrNavErrorV1, 'x', 'y', 'z', 'w'))
        expected.extend(self._layout(_native.IrUncertaintyV1, 'sensor_uncertainties', 'platform_uncertainties'))
        expected.extend(self._layout(_native.IrOrientationV1, 'kind', 'euler', 'quaternion'))
        expected.extend(self._layout(_native.IrSensorInertialStateV1, 'system_time_ns', 'q_xyzw', 'q_ecef_xyzw', 'sensor_position', 'sensor_velocity', 'uncertainties'))
        expected.extend(self._layout(_native.IrSensorInertialStateSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrSensorNavStateV1, 'position', 'position_error', 'velocity', 'velocity_error', 'acceleration', 'acceleration_error', 'orientation', 'orientation_error', 'orientation_velocity', 'orientation_velocity_error', 'orientation_acceleration', 'orientation_acceleration_error', 'coordinate_system'))
        expected.extend(self._layout(_native.IrSensorNavStateSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrFrameSnapshotV1, 'system_time_ns', 'integration_time_ns', 'width', 'height', 'bits_per_pixel', 'number_of_bands', 'horizontal_fov_rad', 'vertical_fov_rad', 'contributing_sensor', 'pixel_format', 'frame_id', 'subframe_id', 'subframe_total', 'image_type', 'image_flip', 'image_flags', 'dither_row', 'dither_column', 'row_offset', 'column_offset', 'sensor_inertial_states', 'sensor_nav_states', 'band_index', 'pixels'))
        expected.extend([ctypes.sizeof(_native.IrFrameSnapshotHandle), ctypes.alignment(_native.IrFrameSnapshotHandle)])
        expected.extend([
            _native.AMS_MEL_IR_C2_METADATA_COMMAND_STATUS,
            _native.AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION,
            _native.AMS_MEL_IR_C2_METADATA_BIT_STATUS,
            _native.AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST,
            _native.AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT,
            _native.AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER,
            _native.AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE,
            _native.AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN,
            _native.AMS_MEL_IR_COMMAND_NOT_SET,
            _native.AMS_MEL_IR_COMMAND_RECEIVED,
            _native.AMS_MEL_IR_COMMAND_ACCEPTED,
            _native.AMS_MEL_IR_COMMAND_REJECTED,
            _native.AMS_MEL_IR_COMMAND_CANCELLED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_NOT_SET,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS,
            _native.AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES,
            _native.AMS_MEL_IR_CANNOT_COMPLY_RANKING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_WEATHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING,
            _native.AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET,
            _native.AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR,
            _native.AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE,
            _native.AMS_MEL_IR_CANNOT_COMPLY_CANCELLED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_OTHER,
            _native.AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN,
            _native.AMS_MEL_IR_CANNOT_COMPLY_ABORTED,
            _native.AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER,
            _native.AMS_MEL_BIT_CONTROL_NOT_SET,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND,
            _native.AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED,
            _native.AMS_MEL_BIT_RESULT_NOT_SET,
            _native.AMS_MEL_BIT_RESULT_PASS,
            _native.AMS_MEL_BIT_RESULT_FAIL,
            _native.AMS_MEL_BIT_RESULT_INTERRUPTED,
            _native.AMS_MEL_BIT_RESULT_NOT_TESTED,
            _native.AMS_MEL_FAULT_SEVERITY_NOT_SET,
            _native.AMS_MEL_FAULT_SEVERITY_NOMINAL,
            _native.AMS_MEL_FAULT_SEVERITY_CAUTION,
            _native.AMS_MEL_FAULT_SEVERITY_WARNING,
            _native.AMS_MEL_FAULT_SEVERITY_FAILED,
            _native.AMS_MEL_FAULT_STATE_NOT_SET,
            _native.AMS_MEL_FAULT_STATE_SET,
            _native.AMS_MEL_FAULT_STATE_CLEARED,
            _native.AMS_MEL_FAULT_STATE_UNKNOWN,
        ])
        expected.extend(self._layout(_native.UciIdSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrCommandStatusV1, 'command_id', 'state', 'reason_id', 'reason_description'))
        expected.extend(self._layout(_native.BitTypeV1, 'bit_id', 'accepted_interface', 'bit_item_names', 'subsystem_component_ids', 'expected_duration_ns'))
        expected.extend(self._layout(_native.BitTypeSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.BitConfigurationV1, 'bit_types'))
        expected.extend(self._layout(_native.ActiveBitV1, 'bit_id', 'estimated_completion_time_ns', 'estimated_percent_complete'))
        expected.extend(self._layout(_native.ActiveBitSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.CompletedBitItemV1, 'bit_item_name', 'result', 'fail_reason'))
        expected.extend(self._layout(_native.CompletedBitItemSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.CompletedBitV1, 'bit_id', 'time_tag_ns', 'result', 'fail_reason', 'bit_items'))
        expected.extend(self._layout(_native.CompletedBitSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultDataV1, 'key', 'value', 'format', 'units'))
        expected.extend(self._layout(_native.FaultDataSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultAmbiguityGroupV1, 'diagnostic_test_ids', 'component_ids'))
        expected.extend(self._layout(_native.FaultAmbiguityGroupSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.FaultV1, 'fault_id', 'severity', 'state', 'fault_data', 'detection_time_ns', 'fault_code', 'fault_description', 'component_ids', 'ambiguity_groups'))
        expected.extend(self._layout(_native.FaultSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.BitStatusV1, 'active_bits', 'completed_bits', 'faults'))
        expected.extend(self._layout(_native.IrC2MetadataEventV1, 'kind', 'command_status', 'bit_configuration', 'bit_status', 'channel_comms_test'))
        expected.extend(self._layout(_native.IrC2MetadataCountersV1, 'events_received', 'events_dropped_queue_full', 'malformed_or_unsupported'))
        expected.extend(self._layout(_native.IrBadPixelV1, 'row', 'column', 'reason'))
        expected.extend(self._layout(_native.IrBadPixelSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrBadPixelListV1, 'reported_size', 'reported_count', 'pixels'))
        expected.extend(self._layout(_native.IrAzElV1, 'azimuth_rad', 'elevation_rad'))
        expected.extend(self._layout(_native.IrLineOfSightReportV1, 'system_time_ns', 'pointing_angle', 'pointing_angle_rates', 'at_speed', 'in_tolerance', 'platform_attitude', 'validity_flag_bitfield', 'image_rotation_rad'))
        expected.extend(self._layout(_native.IrLineOfSightEulerV1, 'system_time_ns', 'attitude', 'attitude_rates'))
        expected.extend(self._layout(_native.IrNavigationResponseV1, 'system_time_ns', 'command_id', 'request_id'))
        expected.extend(self._layout(_native.IrImageMetadataEventV1, 'kind', 'bad_pixel_list', 'line_of_sight_report', 'line_of_sight_euler', 'navigation_response'))
        expected.extend(self._layout(_native.IrChannelCommsTestReportV1, 'command_id', 'request_id'))
        expected.extend(self._layout(_native.IrChannelCommsTestRequestV1, 'command_id', 'channel_id', 'request_id'))
        expected.extend(self._layout(_native.IrChannelCommsTestResultV1, 'command_id', 'request_id', 'error_code'))
        expected.extend(self._layout(_native.IrBandInfoV1, 'kind', 'min_wavelength_m', 'max_wavelength_m'))
        expected.extend(self._layout(_native.IrBandInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrImageBandV1, 'band_index', 'bands'))
        expected.extend(self._layout(_native.IrImageBandSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrChannelCapabilityV1, 'channel_id', 'height', 'width', 'bit_depth', 'row_pitch', 'buffer_size', 'image_size', 'number_of_bands', 'pixel_format', 'sensor_types', 'platform_id', 'sensor_location', 'channel_types', 'task_schedule_depth', 'odc_available', 'nuc_available', 'metadata_capabilities', 'image_bands', 'nav_frames'))
        expected.extend(self._layout(_native.IrHealthConfigV1, 'channel_id', 'channel_type', 'platform_id', 'sensor_location'))
        expected.extend(self._layout(_native.EulerV1, 'roll', 'pitch', 'yaw'))
        expected.extend(self._layout(_native.ForeignKeyV1, 'key', 'system_name'))
        expected.extend(self._layout(_native.InstallationDetailsV1, 'location', 'orientation', 'boresight'))
        expected.extend(self._layout(_native.TemperatureStatusV1, 'temperature_c', 'state'))
        expected.extend(self._layout(_native.MfaComponentV1, 'component_id', 'state', 'temperature', 'installation_location_id', 'installation_details'))
        expected.extend(self._layout(_native.MfaComponentSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.AboutV1, 'model', 'serial_number', 'software_version', 'bootloader_software_version', 'hardware_version'))
        expected.extend(self._layout(_native.MfaStatusV1, 'state', 'state_description', 'mode_description', 'transition_status', 'about', 'components'))
        expected.extend(self._layout(_native.IrSubsystemDepInfoV1, 'subsystem_id', 'criticality', 'failure'))
        expected.extend(self._layout(_native.IrSubsystemDepInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrVersionV1, 'source', 'major_revision', 'minor_revision', 'engineering_revision'))
        expected.extend(self._layout(_native.IrSubsystemCsciInfoV1, 'csci', 'mode', 'version', 'criticality', 'failure', 'bit_report', 'connection_established'))
        expected.extend(self._layout(_native.IrSubsystemCsciInfoSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.IrSubsystemStatusV1, 'subsystem_id', 'criticality', 'status_sequence_number', 'failure', 'subsystem_count', 'subsystems', 'csci_count', 'csci'))
        expected.extend(self._layout(_native.NameValuePairV1, 'name', 'value'))
        expected.extend(self._layout(_native.NameValuePairSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.SecurityArtifactV1, 'component_id', 'associated_id'))
        expected.extend(self._layout(_native.SecurityArtifactSpanV1, 'data', 'size'))
        expected.extend(self._layout(_native.SecurityEventV1, 'kind', 'category', 'details', 'subsystem_id', 'service_id', 'mdf_id'))
        expected.extend(self._layout(_native.SecurityAuditRecordV1, 'security_event_id', 'event_timestamp_ns', 'subsystem_id', 'artifacts', 'event', 'outcome', 'severity'))
        expected.extend(self._layout(_native.IrHealthMetadataEventV1, 'kind', 'mfa_status', 'bit_status', 'subsystem_status', 'discrete_status', 'security_audit', 'mfa_status_detailed'))
        expected.extend([ctypes.sizeof(_native.IrC2MetadataHandle), ctypes.alignment(_native.IrC2MetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrC2MetadataEventHandle), ctypes.alignment(_native.IrC2MetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrImageMetadataHandle), ctypes.alignment(_native.IrImageMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrImageMetadataEventHandle), ctypes.alignment(_native.IrImageMetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrChannelCommsRequestHandle), ctypes.alignment(_native.IrChannelCommsRequestHandle)])
        expected.extend([ctypes.sizeof(_native.IrChannelCapabilityHandle), ctypes.alignment(_native.IrChannelCapabilityHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthHandle), ctypes.alignment(_native.IrHealthHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthMetadataHandle), ctypes.alignment(_native.IrHealthMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrHealthMetadataEventHandle), ctypes.alignment(_native.IrHealthMetadataEventHandle)])
        expected.extend([ctypes.sizeof(_native.IrNavigationRequestHandle), ctypes.alignment(_native.IrNavigationRequestHandle)])
        expected.extend(
            [
                _native.AMS_MEL_POSITION_SOLUTION_NOT_SET,
                _native.AMS_MEL_POSITION_SOLUTION_ALIGNING,
                _native.AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL,
                _native.AMS_MEL_POSITION_SOLUTION_GPS,
                _native.AMS_MEL_POSITION_SOLUTION_BLENDED,
                _native.AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE,
            ]
        )
        expected.extend(self._layout(_native.NorthEastDownV1, 'north', 'east', 'down'))
        expected.extend(self._layout(_native.AttitudeRateV1, 'attitude_rate', 'attitude_rate_time_ns'))
        expected.extend(
            self._layout(
                _native.PositionVelocityCovarianceV1,
                'position_position_pn_pn',
                'position_position_pn_pe',
                'position_position_pn_pd',
                'position_position_pe_pe',
                'position_position_pe_pd',
                'position_position_pd_pd',
                'position_velocity_pn_vn',
                'position_velocity_pn_ve',
                'position_velocity_pn_vd',
                'position_velocity_pe_ve',
                'position_velocity_pe_vd',
                'position_velocity_pd_vd',
                'velocity_velocity_vn_vn',
                'velocity_velocity_vn_ve',
                'velocity_velocity_vn_vd',
                'velocity_velocity_ve_ve',
                'velocity_velocity_ve_vd',
                'velocity_velocity_vd_vd',
            )
        )
        expected.extend(
            self._layout(
                _native.NavigationReportV1,
                'system_time_ns',
                'state',
                'latitude_rad',
                'longitude_rad',
                'altitude_m',
                'attitude',
                'attitude_rate',
                'speed',
                'acceleration',
                'wander_angle_rad',
                'magnetic_heading',
                'altitude_msl',
                'position_velocity_covariance_uncertainty',
            )
        )
        expected.extend(self._layout(_native.IrNavigationResultV1, 'response', 'error_code'))
        expected.extend([ctypes.sizeof(_native.IrInstrumentationHandle), ctypes.alignment(_native.IrInstrumentationHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationRequestHandle), ctypes.alignment(_native.IrInstrumentationRequestHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationMetadataHandle), ctypes.alignment(_native.IrInstrumentationMetadataHandle)])
        expected.extend([ctypes.sizeof(_native.IrInstrumentationMetadataEventHandle), ctypes.alignment(_native.IrInstrumentationMetadataEventHandle)])
        expected.extend(
            [
                _native.AMS_MEL_IR_PRIORITY_NORMAL,
                _native.AMS_MEL_IR_PRIORITY_DEBUG,
                _native.AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT,
                _native.AMS_MEL_IR_CHANNEL_INSTRUMENTATION,
            ]
        )
        expected.extend(
            self._layout(
                _native.IrInstrumentationConfigV1,
                'channel_id',
                'channel_type',
                'platform_id',
                'sensor_location',
            )
        )
        expected.extend(self._layout(_native.IrInstrumentationLevelCommandV1, 'command_id', 'priority'))
        expected.extend(
            self._layout(
                _native.IrInstrumentationReportV1,
                'command_id',
                'size',
                'timestamp_ns',
                'priority',
            )
        )
        expected.extend(self._layout(_native.IrInstrumentationResultV1, 'report', 'error_code'))
        expected.extend(self._layout(_native.IrInstrumentationMetadataEventV1, 'kind', 'report'))
        expected.extend([ctypes.sizeof(_native.IrTrackHandle), ctypes.alignment(_native.IrTrackHandle)])
        expected.extend([_native.AMS_MEL_IR_CHANNEL_IRST_TRACK])
        expected.extend(
            self._layout(
                _native.IrTrackConfigV1,
                'channel_id',
                'channel_type',
                'platform_id',
                'sensor_location',
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
