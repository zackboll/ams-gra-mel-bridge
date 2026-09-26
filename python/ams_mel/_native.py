"""Private ctypes declarations for the complete current ``ams_mel_c`` façade."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path


AMS_MEL_OK = 0
AMS_MEL_INVALID_ARGUMENT = 1
AMS_MEL_LIBRARY_LOAD_FAILED = 2
AMS_MEL_SYMBOL_NOT_FOUND = 3
AMS_MEL_FACTORY_FAILED = 4
AMS_MEL_INITIALIZATION_FAILED = 5
AMS_MEL_PROVIDER_EXCEPTION = 6
AMS_MEL_BUFFER_TOO_SMALL = 7
AMS_MEL_INTERNAL_ERROR = 8
AMS_MEL_TIMEOUT = 9
AMS_MEL_STREAM_STOPPED = 10
AMS_MEL_PROVIDER_FAILED = 11
AMS_MEL_COMMAND_REJECTED = 12
AMS_MEL_RESOURCE_EXHAUSTED = 13


class SessionOptionsV1(ctypes.Structure):
    _fields_ = [("max_async_requests", ctypes.c_uint32)]


AMS_MEL_IR_CHANNEL_IRST_TRACK = 0
AMS_MEL_IR_CHANNEL_IRST_IMAGE = 1
AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL = 2
AMS_MEL_IR_CHANNEL_SCHEDULING, AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS, AMS_MEL_IR_CHANNEL_INSTRUMENTATION, AMS_MEL_IR_CHANNEL_STACKED_IMAGE, AMS_MEL_IR_CHANNEL_RESERVED_1, AMS_MEL_IR_CHANNEL_RESERVED_2 = range(3, 9)
# Upstream Priority defines exactly Normal and Debug; no MaxExclusive value.
AMS_MEL_IR_PRIORITY_NORMAL, AMS_MEL_IR_PRIORITY_DEBUG = 0, 1
AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT = 1
AMS_MEL_IR_MFA_MODE_UNUSED = 0
AMS_MEL_IR_MFA_MODE_TASK_SCHED = 1
AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED = 2
AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED = 3
AMS_MEL_IR_MFA_STATE_NOT_SET = 0
AMS_MEL_IR_MFA_STATE_UNKNOWN = 1
AMS_MEL_IR_MFA_STATE_NOT_INSTALLED = 2
AMS_MEL_IR_MFA_STATE_OFF = 3
AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION = 4
AMS_MEL_IR_MFA_STATE_INITIALIZATION = 5
AMS_MEL_IR_MFA_STATE_STANDBY = 6
AMS_MEL_IR_MFA_STATE_OPERATE = 7
AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY = 8
AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY = 9
AMS_MEL_IR_MFA_STATE_MAINTENANCE = 10
AMS_MEL_IR_MFA_STATE_CALIBRATION = 11
AMS_MEL_IR_MFA_STATE_INITIATED_BIT = 12
AMS_MEL_IR_MFA_STATE_SHUTDOWN = 13
AMS_MEL_IR_MFA_STATE_DEGRADED = 14
AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE = 15
AMS_MEL_STATE_TRANSITION_NOT_SET, AMS_MEL_STATE_TRANSITION_NOT_TRANSITIONING, AMS_MEL_STATE_TRANSITION_SHUTTING_DOWN, AMS_MEL_STATE_TRANSITION_TRANSITIONING = range(4)
AMS_MEL_COMPONENT_STATE_NOT_SET, AMS_MEL_COMPONENT_STATE_UNKNOWN, AMS_MEL_COMPONENT_STATE_NOT_INSTALLED, AMS_MEL_COMPONENT_STATE_OFF, AMS_MEL_COMPONENT_STATE_INITIALIZING, AMS_MEL_COMPONENT_STATE_OPERATIONAL, AMS_MEL_COMPONENT_STATE_DEGRADED, AMS_MEL_COMPONENT_STATE_DISABLED, AMS_MEL_COMPONENT_STATE_FAULTED = range(9)
AMS_MEL_TEMPERATURE_STATE_NOT_SET, AMS_MEL_TEMPERATURE_STATE_UNDER_TEMP, AMS_MEL_TEMPERATURE_STATE_NORMAL, AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_WARNING, AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_DEGRADED, AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_SHUTDOWN = range(6)
AMS_MEL_IR_FAILURE_NA, AMS_MEL_IR_FAILURE_CRITICAL, AMS_MEL_IR_FAILURE_MAJOR, AMS_MEL_IR_FAILURE_PARAMETRIC, AMS_MEL_IR_FAILURE_INFORMATIONAL, AMS_MEL_IR_FAILURE_AVAILABLE, AMS_MEL_IR_FAILURE_NOT_PRESENT = range(7)
AMS_MEL_IR_CSCI_MODE_UNKNOWN, AMS_MEL_IR_CSCI_MODE_UNUSED, AMS_MEL_IR_CSCI_MODE_INITIALIZATION, AMS_MEL_IR_CSCI_MODE_MAINTENANCE, AMS_MEL_IR_CSCI_MODE_IDLE, AMS_MEL_IR_CSCI_MODE_OPERATIONAL, AMS_MEL_IR_CSCI_MODE_VSA, AMS_MEL_IR_CSCI_MODE_QUICK_LOOK, AMS_MEL_IR_CSCI_MODE_TRACK, AMS_MEL_IR_CSCI_MODE_IMAGING, AMS_MEL_IR_CSCI_MODE_NOISE = range(11)
AMS_MEL_SECURITY_EVENT_NONE, AMS_MEL_SECURITY_EVENT_AUTHENTICATION, AMS_MEL_SECURITY_EVENT_INTEGRITY, AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT, AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT, AMS_MEL_SECURITY_EVENT_SYSTEM, AMS_MEL_SECURITY_EVENT_SANITIZATION = range(7)
AMS_MEL_SECURITY_OUTCOME_NOT_SET, AMS_MEL_SECURITY_OUTCOME_FAILURE, AMS_MEL_SECURITY_OUTCOME_SUCCESS = range(3)
AMS_MEL_SECURITY_SEVERITY_NOT_SET, AMS_MEL_SECURITY_SEVERITY_CRITICAL, AMS_MEL_SECURITY_SEVERITY_ERROR, AMS_MEL_SECURITY_SEVERITY_INFORMATIONAL, AMS_MEL_SECURITY_SEVERITY_WARNING = range(5)
AMS_MEL_IR_COORD_FRAME_INERTIAL = 0
AMS_MEL_IR_COORD_FRAME_AIRCRAFT = 1
AMS_MEL_IR_DEGRADATION_CAPACITY = 0
AMS_MEL_IR_DEGRADATION_VOLUME = 1
AMS_MEL_IR_DEGRADATION_RANGE = 2
AMS_MEL_IR_DEGRADATION_REVISIT = 3
AMS_MEL_IR_RETURN_SUCCESS = 0
AMS_MEL_IR_RETURN_BAD_POINTER = 1
AMS_MEL_IR_RETURN_FAIL = 2
AMS_MEL_IR_RETURN_NOT_SUPPORTED = 3
AMS_MEL_IR_RETURN_NOT_IMPLEMENTED = 4
AMS_MEL_ERROR_NONE = 0
AMS_MEL_ERROR_INVALID_ID = 1
AMS_MEL_ERROR_INVALID_STATE = 2
AMS_MEL_ERROR_INVALID_PARAMETERS = 3
AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS = 4
AMS_MEL_ERROR_INSUFFICIENT_RESOURCES = 5
AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES = 6
AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES = 7
AMS_MEL_ERROR_UNSUPPORTED = 8
AMS_MEL_IR_PIXEL_MONO = 0
AMS_MEL_IR_PIXEL_RGB = 1
AMS_MEL_IR_PIXEL_BAYER = 2
AMS_MEL_IR_SENSOR_UNSPECIFIED, AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL, AMS_MEL_IR_SENSOR_GIMBAL_VERTICAL, AMS_MEL_IR_SENSOR_GIMBAL_ROTATION, AMS_MEL_IR_SENSOR_STEP_STARE = range(5)
AMS_MEL_IR_BAND_INVALID, AMS_MEL_IR_BAND_MULTIBAND, AMS_MEL_IR_BAND_IR_FAR, AMS_MEL_IR_BAND_IR_NEAR, AMS_MEL_IR_BAND_IR_LONGWAVE, AMS_MEL_IR_BAND_IR_MIDWAVE, AMS_MEL_IR_BAND_IR_SHORTWAVE, AMS_MEL_IR_BAND_VISIBLE_WHITE, AMS_MEL_IR_BAND_VISIBLE_RED, AMS_MEL_IR_BAND_VISIBLE_GREEN, AMS_MEL_IR_BAND_VISIBLE_BLUE, AMS_MEL_IR_BAND_UVA, AMS_MEL_IR_BAND_UVB, AMS_MEL_IR_BAND_UVC, AMS_MEL_IR_BAND_UV_VACUUM = range(15)
AMS_MEL_IR_COORDINATE_LLA, AMS_MEL_IR_COORDINATE_ECEF, AMS_MEL_IR_COORDINATE_NED_PLATFORM, AMS_MEL_IR_COORDINATE_NED_SENSOR = range(4)
AMS_MEL_IR_METADATA_BAD_PIXEL_LIST = 0
AMS_MEL_IR_METADATA_OPTICAL_DISTORTION_MAP, AMS_MEL_IR_METADATA_LF_STATUS, AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT, AMS_MEL_IR_METADATA_LINE_OF_SIGHT_QUATERNION, AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER, AMS_MEL_IR_METADATA_MFA_STATUS, AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED, AMS_MEL_IR_METADATA_BIT_CONFIGURATION, AMS_MEL_IR_METADATA_COMMAND_STATUS, AMS_MEL_IR_METADATA_BIT_STATUS, AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_MESSAGE, AMS_MEL_IR_METADATA_TASK_EXECUTING_REP, AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP, AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK, AMS_MEL_IR_METADATA_SCHED_CREATED_REP, AMS_MEL_IR_METADATA_IRST_TRACK_REPORT, AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP, AMS_MEL_IR_METADATA_CAMERA_COMMAND_RESP, AMS_MEL_IR_METADATA_CAMERA_PROTECT_CMD_RESP, AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT, AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP, AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA, AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE, AMS_MEL_IR_METADATA_LOS_3D_KINEMATICS_TYPE, AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE, AMS_MEL_IR_METADATA_TASK_EVENTS, AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT, AMS_MEL_IR_METADATA_RESERVED_3, AMS_MEL_IR_METADATA_RESERVED_5, AMS_MEL_IR_METADATA_RESERVED_9 = range(1, 31)
AMS_MEL_IR_METADATA_RESERVED_10 = 31
AMS_MEL_IR_IMAGE_STARING = 0
AMS_MEL_IR_IMAGE_SCANNING = 1
AMS_MEL_IR_FLIP_NONE = 0
AMS_MEL_IR_FLIP_VERTICAL = 1
AMS_MEL_IR_FLIP_HORIZONTAL = 2
AMS_MEL_IR_FLIP_BOTH = 3
AMS_MEL_IR_C2_METADATA_COMMAND_STATUS = 1
AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION = 2
AMS_MEL_IR_C2_METADATA_BIT_STATUS = 3
AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST = 4
AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST = 1
AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT = 2
AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER = 3
AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE = 4
AMS_MEL_POSITION_SOLUTION_NOT_SET = 0
AMS_MEL_POSITION_SOLUTION_ALIGNING = 1
AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL = 2
AMS_MEL_POSITION_SOLUTION_GPS = 3
AMS_MEL_POSITION_SOLUTION_BLENDED = 4
AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE = 5
AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN = 0
AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS, AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS, AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS, AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS, AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT, AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED = range(1, 7)
AMS_MEL_IR_COMMAND_NOT_SET, AMS_MEL_IR_COMMAND_RECEIVED, AMS_MEL_IR_COMMAND_ACCEPTED, AMS_MEL_IR_COMMAND_REJECTED, AMS_MEL_IR_COMMAND_CANCELLED = range(5)
(
    AMS_MEL_IR_CANNOT_COMPLY_NOT_SET, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION, AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE,
    AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE, AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME,
    AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM, AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE,
    AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT, AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS,
    AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE, AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE,
    AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT, AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT,
    AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE, AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT,
    AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT, AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE,
    AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE, AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES,
    AMS_MEL_IR_CANNOT_COMPLY_RANKING, AMS_MEL_IR_CANNOT_COMPLY_WEATHER,
    AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE, AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR,
    AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING, AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR,
    AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET, AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID,
    AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER, AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER,
    AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR, AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE,
    AMS_MEL_IR_CANNOT_COMPLY_CANCELLED, AMS_MEL_IR_CANNOT_COMPLY_OTHER,
    AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN, AMS_MEL_IR_CANNOT_COMPLY_ABORTED,
    AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER,
) = range(47)
AMS_MEL_BIT_CONTROL_NOT_SET, AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND, AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND, AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED = range(4)
AMS_MEL_BIT_RESULT_NOT_SET, AMS_MEL_BIT_RESULT_PASS, AMS_MEL_BIT_RESULT_FAIL, AMS_MEL_BIT_RESULT_INTERRUPTED, AMS_MEL_BIT_RESULT_NOT_TESTED = range(5)
AMS_MEL_FAULT_SEVERITY_NOT_SET, AMS_MEL_FAULT_SEVERITY_NOMINAL, AMS_MEL_FAULT_SEVERITY_CAUTION, AMS_MEL_FAULT_SEVERITY_WARNING, AMS_MEL_FAULT_SEVERITY_FAILED = range(5)
AMS_MEL_FAULT_STATE_NOT_SET, AMS_MEL_FAULT_STATE_SET, AMS_MEL_FAULT_STATE_CLEARED, AMS_MEL_FAULT_STATE_UNKNOWN = range(4)

AMS_MEL_ABI_VERSION_MAJOR = 0
AMS_MEL_ABI_VERSION_MINOR = 1


class AbiVersionV1(ctypes.Structure):
    _fields_ = [
        ("major", ctypes.c_uint32),
        ("minor", ctypes.c_uint32),
    ]


class ProviderVersionV1(ctypes.Structure):
    _fields_ = [
        ("api_version", ctypes.c_uint32),
        ("library_version", ctypes.c_uint32),
        ("vendor", ctypes.POINTER(ctypes.c_char)),
        ("vendor_capacity", ctypes.c_size_t),
        ("vendor_required", ctypes.c_size_t),
        ("description", ctypes.POINTER(ctypes.c_char)),
        ("description_capacity", ctypes.c_size_t),
        ("description_required", ctypes.c_size_t),
    ]


class StringViewV1(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.POINTER(ctypes.c_char)),
        ("size", ctypes.c_size_t),
    ]

class U32SpanV1(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint32)), ("size", ctypes.c_size_t)]
class StringViewSpanV1(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(StringViewV1)), ("size", ctypes.c_size_t)]
class IrScanTypeV1(ctypes.Structure):
    _fields_ = [("continuous_scan", ctypes.c_uint32), ("returning", ctypes.c_uint32),
                ("agile_scan", ctypes.c_uint32)]
class IrScanParamV1(ctypes.Structure):
    _fields_ = [("elevation_defined_with_range_and_altitude", ctypes.c_uint32),
        ("center_az_rad", ctypes.c_double), ("center_el_rad", ctypes.c_double),
        ("center_frame_ref_el", ctypes.c_uint32), ("center_frame_ref_az", ctypes.c_uint32),
        ("scan_width_rad", ctypes.c_double), ("scan_height_rad", ctypes.c_double),
        ("scan_type", IrScanTypeV1), ("scan_id", ctypes.c_uint32),
        ("scan_rate_rad_per_second", ctypes.c_double),
        ("preferred_revisit_interval_seconds", ctypes.c_double),
        ("required_revisit_interval_seconds", ctypes.c_double),
        ("max_range_of_interest_m", ctypes.c_uint32),
        ("min_range_of_interest_m", ctypes.c_uint32),
        ("elevation_scan_center_altitude_m", ctypes.c_uint32),
        ("elevation_scan_center_range_m", ctypes.c_uint32),
        ("degradation_method", ctypes.c_uint32)]
class IrModeCommandV1(ctypes.Structure):
    _fields_ = [("command_id", ctypes.c_uint32), ("state", ctypes.c_uint32),
                ("mode", ctypes.c_uint32), ("scan_parameters", IrScanParamV1)]
class IrBitCommandV1(ctypes.Structure):
    _fields_ = [("command_id", ctypes.c_uint32), ("initiate_bit_ids", U32SpanV1),
                ("cancel_bit_ids", U32SpanV1), ("clear_fault_codes", StringViewSpanV1)]
class IrConfigSetCommandV1(ctypes.Structure):
    _fields_ = [("command_id", ctypes.c_uint32), ("system_time_ns", ctypes.c_int64),
                ("config", StringViewV1)]


class UciIdV1(ctypes.Structure):
    _fields_ = [
        ("uuid", ctypes.c_uint8 * 16),
        ("descriptive_label", StringViewV1),
    ]

class UciIdSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(UciIdV1)), ("size", ctypes.c_size_t)]
class IrCommandStatusV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("state",ctypes.c_uint32),("reason_id",ctypes.c_uint32),("reason_description",StringViewV1)]
class BitTypeV1(ctypes.Structure): _fields_ = [("bit_id",UciIdV1),("accepted_interface",ctypes.c_uint32),("bit_item_names",StringViewSpanV1),("subsystem_component_ids",UciIdSpanV1),("expected_duration_ns",ctypes.c_int64)]
class BitTypeSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(BitTypeV1)),("size",ctypes.c_size_t)]
class BitConfigurationV1(ctypes.Structure): _fields_ = [("bit_types",BitTypeSpanV1)]
class ActiveBitV1(ctypes.Structure): _fields_ = [("bit_id",UciIdV1),("estimated_completion_time_ns",ctypes.c_int64),("estimated_percent_complete",ctypes.c_double)]
class ActiveBitSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(ActiveBitV1)),("size",ctypes.c_size_t)]
class CompletedBitItemV1(ctypes.Structure): _fields_ = [("bit_item_name",StringViewV1),("result",ctypes.c_uint32),("fail_reason",StringViewV1)]
class CompletedBitItemSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(CompletedBitItemV1)),("size",ctypes.c_size_t)]
class CompletedBitV1(ctypes.Structure): _fields_ = [("bit_id",UciIdV1),("time_tag_ns",ctypes.c_int64),("result",ctypes.c_uint32),("fail_reason",StringViewV1),("bit_items",CompletedBitItemSpanV1)]
class CompletedBitSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(CompletedBitV1)),("size",ctypes.c_size_t)]
class FaultDataV1(ctypes.Structure): _fields_ = [("key",StringViewV1),("value",StringViewV1),("format",StringViewV1),("units",StringViewV1)]
class FaultDataSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(FaultDataV1)),("size",ctypes.c_size_t)]
class FaultAmbiguityGroupV1(ctypes.Structure): _fields_ = [("diagnostic_test_ids",UciIdSpanV1),("component_ids",UciIdSpanV1)]
class FaultAmbiguityGroupSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(FaultAmbiguityGroupV1)),("size",ctypes.c_size_t)]
class FaultV1(ctypes.Structure): _fields_ = [("fault_id",UciIdV1),("severity",ctypes.c_uint32),("state",ctypes.c_uint32),("fault_data",FaultDataSpanV1),("detection_time_ns",ctypes.c_int64),("fault_code",StringViewV1),("fault_description",StringViewV1),("component_ids",UciIdSpanV1),("ambiguity_groups",FaultAmbiguityGroupSpanV1)]
class FaultSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(FaultV1)),("size",ctypes.c_size_t)]
class BitStatusV1(ctypes.Structure): _fields_ = [("active_bits",ActiveBitSpanV1),("completed_bits",CompletedBitSpanV1),("faults",FaultSpanV1)]
class IrChannelCommsTestReportV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("request_id",ctypes.c_uint32)]
class IrC2MetadataEventV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("command_status",IrCommandStatusV1),("bit_configuration",BitConfigurationV1),("bit_status",BitStatusV1),("channel_comms_test",IrChannelCommsTestReportV1)]
class IrC2MetadataCountersV1(ctypes.Structure): _fields_ = [("events_received",ctypes.c_uint64),("events_dropped_queue_full",ctypes.c_uint64),("malformed_or_unsupported",ctypes.c_uint64)]
class IrBadPixelV1(ctypes.Structure): _fields_ = [("row",ctypes.c_uint32),("column",ctypes.c_uint32),("reason",ctypes.c_uint32)]
class IrBadPixelSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrBadPixelV1)),("size",ctypes.c_size_t)]
class IrBadPixelListV1(ctypes.Structure): _fields_ = [("reported_size",ctypes.c_uint32),("reported_count",ctypes.c_uint32),("pixels",IrBadPixelSpanV1)]
class IrImageMetadataEventV1(ctypes.Structure): pass


class ComponentLocationV1(ctypes.Structure):
    _fields_ = [
        ("offset_x_m", ctypes.c_double),
        ("offset_y_m", ctypes.c_double),
        ("offset_z_m", ctypes.c_double),
        ("key", StringViewV1),
        ("system_name", StringViewV1),
    ]
class IrChannelCommsTestRequestV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("channel_id",ctypes.c_uint32),("request_id",ctypes.c_uint32)]
class IrChannelCommsTestResultV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("request_id",ctypes.c_uint32),("error_code",ctypes.c_uint32)]
class IrBandInfoV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("min_wavelength_m",ctypes.c_double),("max_wavelength_m",ctypes.c_double)]
class IrBandInfoSpanV1(ctypes.Structure): pass
IrBandInfoSpanV1._fields_ = [("data",ctypes.POINTER(IrBandInfoV1)),("size",ctypes.c_size_t)]
class IrImageBandV1(ctypes.Structure): _fields_ = [("band_index",ctypes.c_uint32),("bands",IrBandInfoSpanV1)]
class IrImageBandSpanV1(ctypes.Structure): pass
IrImageBandSpanV1._fields_ = [("data",ctypes.POINTER(IrImageBandV1)),("size",ctypes.c_size_t)]
class IrChannelCapabilityV1(ctypes.Structure): _fields_ = [("channel_id",UciIdV1),("height",ctypes.c_uint32),("width",ctypes.c_uint32),("bit_depth",ctypes.c_uint32),("row_pitch",ctypes.c_uint32),("buffer_size",ctypes.c_uint32),("image_size",ctypes.c_uint32),("number_of_bands",ctypes.c_uint32),("pixel_format",ctypes.c_uint32),("sensor_types",U32SpanV1),("platform_id",UciIdV1),("sensor_location",ComponentLocationV1),("channel_types",U32SpanV1),("task_schedule_depth",ctypes.c_uint32),("odc_available",ctypes.c_uint32),("nuc_available",ctypes.c_uint32),("metadata_capabilities",U32SpanV1),("image_bands",IrImageBandSpanV1),("nav_frames",U32SpanV1)]


class IrStreamConfigV1(ctypes.Structure):
    _fields_ = [
        ("channel_type", ctypes.c_uint32),
        ("channel_id", UciIdV1),
        ("platform_id", UciIdV1),
        ("sensor_location", ComponentLocationV1),
        ("buffer_count", ctypes.c_size_t),
        ("buffer_size", ctypes.c_size_t),
        ("queue_capacity", ctypes.c_size_t),
    ]


class IrC2ConfigV1(ctypes.Structure):
    _fields_ = [
        ("channel_type", ctypes.c_uint32),
        ("channel_id", UciIdV1),
        ("platform_id", UciIdV1),
        ("sensor_location", ComponentLocationV1),
    ]


class IrHealthConfigV1(ctypes.Structure):
    _fields_ = [("channel_id", UciIdV1), ("channel_type", ctypes.c_uint32), ("platform_id", UciIdV1), ("sensor_location", ComponentLocationV1)]
class EulerV1(ctypes.Structure): _fields_ = [("roll", ctypes.c_double), ("pitch", ctypes.c_double), ("yaw", ctypes.c_double)]
class IrAzElV1(ctypes.Structure): _fields_ = [("azimuth_rad",ctypes.c_double),("elevation_rad",ctypes.c_double)]
class IrLineOfSightReportV1(ctypes.Structure): _fields_ = [("system_time_ns",ctypes.c_int64),("pointing_angle",IrAzElV1),("pointing_angle_rates",IrAzElV1),("at_speed",ctypes.c_uint8),("in_tolerance",ctypes.c_uint8),("platform_attitude",EulerV1),("validity_flag_bitfield",ctypes.c_uint32),("image_rotation_rad",ctypes.c_double)]
class IrLineOfSightEulerV1(ctypes.Structure): _fields_ = [("system_time_ns",ctypes.c_int64),("attitude",EulerV1),("attitude_rates",EulerV1)]
class IrNavigationResponseV1(ctypes.Structure): _fields_ = [("system_time_ns",ctypes.c_int64),("command_id",ctypes.c_uint32),("request_id",ctypes.c_uint32)]
IrImageMetadataEventV1._fields_ = [("kind",ctypes.c_uint32),("bad_pixel_list",IrBadPixelListV1),("line_of_sight_report",IrLineOfSightReportV1),("line_of_sight_euler",IrLineOfSightEulerV1),("navigation_response",IrNavigationResponseV1)]
class NorthEastDownV1(ctypes.Structure): _fields_ = [("north",ctypes.c_double),("east",ctypes.c_double),("down",ctypes.c_double)]
class AttitudeRateV1(ctypes.Structure): _fields_ = [("attitude_rate",EulerV1),("attitude_rate_time_ns",ctypes.c_int64)]
class PositionVelocityCovarianceV1(ctypes.Structure): _fields_ = [
    ("position_position_pn_pn",ctypes.c_double),("position_position_pn_pe",ctypes.c_double),
    ("position_position_pn_pd",ctypes.c_double),("position_position_pe_pe",ctypes.c_double),
    ("position_position_pe_pd",ctypes.c_double),("position_position_pd_pd",ctypes.c_double),
    ("position_velocity_pn_vn",ctypes.c_double),("position_velocity_pn_ve",ctypes.c_double),
    ("position_velocity_pn_vd",ctypes.c_double),("position_velocity_pe_ve",ctypes.c_double),
    ("position_velocity_pe_vd",ctypes.c_double),("position_velocity_pd_vd",ctypes.c_double),
    ("velocity_velocity_vn_vn",ctypes.c_double),("velocity_velocity_vn_ve",ctypes.c_double),
    ("velocity_velocity_vn_vd",ctypes.c_double),("velocity_velocity_ve_ve",ctypes.c_double),
    ("velocity_velocity_ve_vd",ctypes.c_double),("velocity_velocity_vd_vd",ctypes.c_double)]
class NavigationReportV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),("state",ctypes.c_uint32),
    ("latitude_rad",ctypes.c_double),("longitude_rad",ctypes.c_double),("altitude_m",ctypes.c_double),
    ("attitude",EulerV1),("attitude_rate",AttitudeRateV1),
    ("speed",NorthEastDownV1),("acceleration",NorthEastDownV1),
    ("wander_angle_rad",ctypes.c_double),("magnetic_heading",ctypes.c_double),("altitude_msl",ctypes.c_double),
    ("position_velocity_covariance_uncertainty",PositionVelocityCovarianceV1)]
class IrNavigationResultV1(ctypes.Structure): _fields_ = [("response",IrNavigationResponseV1),("error_code",ctypes.c_uint32)]
class IrInstrumentationConfigV1(ctypes.Structure):
    _fields_ = [("channel_id", UciIdV1), ("channel_type", ctypes.c_uint32), ("platform_id", UciIdV1), ("sensor_location", ComponentLocationV1)]
class IrTrackConfigV1(ctypes.Structure):
    _fields_ = [("channel_id", UciIdV1), ("channel_type", ctypes.c_uint32), ("platform_id", UciIdV1), ("sensor_location", ComponentLocationV1)]
# Complete IRSTTrackReport. Reuses the one canonical NorthEastDownV1 for both
# NED vectors. Upstream IrstTrackState/IrstTrackMode declare no MaxExclusive
# value, so anything above Dropped/Stare is malformed.
AMS_MEL_IR_TRACK_STATE_IDLE = 0
AMS_MEL_IR_TRACK_STATE_DETECTED = 1
AMS_MEL_IR_TRACK_STATE_COAST = 2
AMS_MEL_IR_TRACK_STATE_DROPPED = 3
AMS_MEL_IR_TRACK_MODE_IDLE = 0
AMS_MEL_IR_TRACK_MODE_SCAN = 1
AMS_MEL_IR_TRACK_MODE_STARE = 2
AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT = 1
AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA = 2
AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE = 3
AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE = 4
# Upstream MAX_CANDIDATE_OBJECTS: the fixed storage length of the published
# std::array<CandidateObject, 900>. numberOfCOs selects the meaningful prefix;
# a larger count is malformed.
AMS_MEL_IR_MAX_CANDIDATE_OBJECTS = 900
# Upstream HotRegionTypeEnum; no MaxExclusive value exists upstream.
AMS_MEL_IR_HOT_REGION_INVALID = 0
AMS_MEL_IR_HOT_REGION_FLARE = 1
AMS_MEL_IR_HOT_REGION_SOLAR = 2
AMS_MEL_IR_HOT_REGION_MASK = 3
# The canonical IR XYZ, quaternion, uncertainty, and SensorInertialState
# records, declared here so the Track metadata event can reuse them exactly as
# the C header does. No layout changed and no duplicate exists.
class IrDirectionalV1(ctypes.Structure): _fields_ = [("x",ctypes.c_double),("y",ctypes.c_double),("z",ctypes.c_double)]
class IrQuaternionV1(ctypes.Structure): _fields_ = [("x",ctypes.c_double),("y",ctypes.c_double),("z",ctypes.c_double),("w",ctypes.c_double)]
class IrUncertaintyV1(ctypes.Structure): _fields_ = [("sensor_uncertainties",ctypes.c_uint32),("platform_uncertainties",ctypes.c_uint32)]
class IrSensorInertialStateV1(ctypes.Structure): _fields_ = [("system_time_ns",ctypes.c_int64),("q_xyzw",IrQuaternionV1),("q_ecef_xyzw",IrQuaternionV1),("sensor_position",IrDirectionalV1),("sensor_velocity",IrDirectionalV1),("uncertainties",IrUncertaintyV1)]
# The one canonical row/column pair, matching upstream RowCol; deliberately not
# an XYZ triple with a meaningless third component.
class IrRowColV1(ctypes.Structure): _fields_ = [("row",ctypes.c_double),("column",ctypes.c_double)]
# Complete HotRegion; the geometry keeps its upstream uint16_t width.
class IrHotRegionV1(ctypes.Structure): _fields_ = [
    ("kind",ctypes.c_uint32),("size",ctypes.c_uint16),("top",ctypes.c_uint16),
    ("left",ctypes.c_uint16),("right",ctypes.c_uint16),("bottom",ctypes.c_uint16)]
class IrHotRegionSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrHotRegionV1)),("size",ctypes.c_size_t)]
# Complete CandidateObjectHeader. cfar is upstream float and stays binary32; it
# is deliberately not widened to double, and the validity bitfield is carried
# verbatim rather than decoded.
class IrCandidateObjectHeaderV1(ctypes.Structure): _fields_ = [
    ("number_of_cos",ctypes.c_uint16),("stack_frame_index",ctypes.c_uint16),
    ("cfar",ctypes.c_float),("validity_flag_bitfield",ctypes.c_uint16),
    ("tov_utc_ns",ctypes.c_int64)]
# Complete CandidateObject; reuses the canonical row/column and XYZ records.
class IrCandidateObjectV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),("detection_category",ctypes.c_uint32),
    ("sensor_index",ctypes.c_uint32),("subpixel",IrRowColV1),
    ("intensity",ctypes.c_double),("sensor_relative_unit",IrDirectionalV1),
    ("signal_to_interference_ratio",ctypes.c_double),
    ("signal_to_noise_ratio",ctypes.c_double)]
class IrCandidateObjectSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrCandidateObjectV1)),("size",ctypes.c_size_t)]
# Complete CandidateObjectMessage. Upstream declares no send and no
# RequestFor, so this is inbound callback metadata. Both spans borrow storage
# owned by the native event owner and stay valid until event close.
class IrCandidateObjectMessageV1(ctypes.Structure): _fields_ = [
    ("header",IrCandidateObjectHeaderV1),("inertial_state",IrSensorInertialStateV1),
    ("hot_regions",IrHotRegionSpanV1),("candidate_objects",IrCandidateObjectSpanV1)]
# Upstream RequestSystemTrackData is an inbound @Optional request delivered
# through the Track metadata callback; TrackChannel declares no matching send.
class IrRequestSystemTrackDataV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),("command_id",ctypes.c_uint32),
    ("request_id",ctypes.c_uint32),("track_id",ctypes.c_uint32)]
class IrTrackReportV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),("activity_id",ctypes.c_uint32),
    ("measured_ned",NorthEastDownV1),("measured_intensity",ctypes.c_double),("measured_snr",ctypes.c_double),
    ("filtered_ned",NorthEastDownV1),("filtered_intensity",ctypes.c_double),("filtered_snr",ctypes.c_double),
    ("range_m",ctypes.c_double),("range_error_m",ctypes.c_double),("spatial_extent_rad",ctypes.c_double),
    ("track_quality",ctypes.c_double),("clutter",ctypes.c_double),("age_ns",ctypes.c_int64),
    ("state",ctypes.c_uint32),("mode",ctypes.c_uint32)]
# FROZEN Track metadata event v1: exactly these three members. Nothing may be
# appended again; later Track metadata payloads use a new version record.
class IrTrackMetadataEventV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("track_report",IrTrackReportV1),("request_system_track_data",IrRequestSystemTrackDataV1)]
# Track metadata event v2: the complete frozen v1 record first, then the
# additive CandidateObjectMessage payload. base.kind stays the discriminator.
class IrTrackMetadataEventV2(ctypes.Structure): _fields_ = [("base",IrTrackMetadataEventV1),("candidate_object_message",IrCandidateObjectMessageV1)]
# Upstream candidateObjectWithBackground is std::array<std::array<int16_t,3>,3>;
# the one fixed representation is row-major samples[row * 3 + column].
AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE = 3
AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES = 9
class IrCandidateBackgroundV1(ctypes.Structure): _fields_ = [
    ("samples",ctypes.c_int16 * 9)]
# Complete CandidateObjectPreProc. Each entry carries its OWN nested inertial
# state. No value is clamped or normalized; edge is normalized to exactly 0/1.
class IrCandidateObjectPreProcV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),("detection_category",ctypes.c_uint32),
    ("sensor_index",ctypes.c_uint32),("subpixel",IrRowColV1),
    ("intensity",ctypes.c_double),("sensor_relative_unit",IrDirectionalV1),
    ("signal_to_interference_ratio",ctypes.c_double),
    ("signal_to_noise_ratio",ctypes.c_double),
    ("candidate_object_with_background",IrCandidateBackgroundV1),
    ("clutter",ctypes.c_double),("candidate_object_quality",ctypes.c_double),
    ("sir_delta",ctypes.c_double),("inertial_state",IrSensorInertialStateV1),
    ("edge",ctypes.c_uint8),("az_sigma",ctypes.c_double),
    ("el_sigma",ctypes.c_double),("background_normalizer",ctypes.c_double)]
class IrCandidateObjectPreProcSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrCandidateObjectPreProcV1)),("size",ctypes.c_size_t)]
# Complete CandidateObjectPreProcMessage. The delivering callback is @Optional;
# upstream declares no send and no RequestFor. The PreProc container is a
# std::vector, so the span size is the vector's own size and number_of_cos is
# deliberately not used to truncate it: no such invariant is published.
class IrCandidateObjectPreProcMessageV1(ctypes.Structure): _fields_ = [
    ("header",IrCandidateObjectHeaderV1),("inertial_state",IrSensorInertialStateV1),
    ("hot_regions",IrHotRegionSpanV1),
    ("candidate_object_preprocs",IrCandidateObjectPreProcSpanV1)]
# Track metadata event v3: the complete frozen v2 record first, then the
# additive CandidateObjectPreProcMessage payload. base.base.kind stays the one
# discriminator.
class IrTrackMetadataEventV3(ctypes.Structure): _fields_ = [("base",IrTrackMetadataEventV2),("candidate_object_preproc_message",IrCandidateObjectPreProcMessageV1)]
class IrInstrumentationLevelCommandV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("priority",ctypes.c_uint32)]
class IrInstrumentationReportV1(ctypes.Structure): _fields_ = [("command_id",ctypes.c_uint32),("size",ctypes.c_uint32),("timestamp_ns",ctypes.c_int64),("priority",ctypes.c_uint32)]
class IrInstrumentationResultV1(ctypes.Structure): _fields_ = [("report",IrInstrumentationReportV1),("error_code",ctypes.c_uint32)]
class IrInstrumentationMetadataEventV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("report",IrInstrumentationReportV1)]
class U8SpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(ctypes.c_uint8)),("size",ctypes.c_size_t)]
class IrContributingSensorV1(ctypes.Structure): _fields_ = [("location",ComponentLocationV1),("sensor_id",ctypes.c_uint32)]
class IrNavErrorV1(ctypes.Structure): _fields_ = [("x",ctypes.c_double),("y",ctypes.c_double),("z",ctypes.c_double),("w",ctypes.c_double)]
class IrOrientationV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("euler",EulerV1),("quaternion",IrQuaternionV1)]
class IrSensorNavStateV1(ctypes.Structure): _fields_ = [("position",IrDirectionalV1),("position_error",IrNavErrorV1),("velocity",IrDirectionalV1),("velocity_error",IrNavErrorV1),("acceleration",IrDirectionalV1),("acceleration_error",IrNavErrorV1),("orientation",IrOrientationV1),("orientation_error",IrNavErrorV1),("orientation_velocity",IrOrientationV1),("orientation_velocity_error",IrNavErrorV1),("orientation_acceleration",IrOrientationV1),("orientation_acceleration_error",IrNavErrorV1),("coordinate_system",ctypes.c_uint32)]
# Conditionally required TrackDataUpdate (@RequiredIfTrackUpdate). Upstream
# TrackStatus declares no MaxExclusive value, so any input above Delete is
# INVALID_ARGUMENT. The two times stay in upstream epoch seconds and the one
# canonical IrDirectionalV1 is reused for both ECEF vectors.
AMS_MEL_IR_TRACK_STATUS_CREATE = 0
AMS_MEL_IR_TRACK_STATUS_UPDATE = 1
AMS_MEL_IR_TRACK_STATUS_PREDICT = 2
AMS_MEL_IR_TRACK_STATUS_DELETE = 3
class IrTrackCovarianceV1(ctypes.Structure): _fields_ = [
    ("xx",ctypes.c_double),("xy",ctypes.c_double),("xz",ctypes.c_double),
    ("x_vx",ctypes.c_double),("x_vy",ctypes.c_double),("x_vz",ctypes.c_double),
    ("yy",ctypes.c_double),("yz",ctypes.c_double),
    ("y_vx",ctypes.c_double),("y_vy",ctypes.c_double),("y_vz",ctypes.c_double),
    ("zz",ctypes.c_double),
    ("z_vx",ctypes.c_double),("z_vy",ctypes.c_double),("z_vz",ctypes.c_double),
    ("vx_vx",ctypes.c_double),("vx_vy",ctypes.c_double),("vx_vz",ctypes.c_double),
    ("vy_vy",ctypes.c_double),("vy_vz",ctypes.c_double),
    ("vz_vz",ctypes.c_double)]
class IrTrackDataUpdateV1(ctypes.Structure): _fields_ = [
    ("platform_id",ctypes.c_uint32),("capability_uuid",UciIdV1),("activity_uuid",UciIdV1),
    ("track_id",ctypes.c_uint32),("entity_uuid",UciIdV1),("track_status",ctypes.c_uint32),
    ("time_of_validity_seconds",ctypes.c_double),("time_of_last_update_seconds",ctypes.c_double),
    ("track_position_ecef",IrDirectionalV1),("track_velocity_ecef",IrDirectionalV1),
    ("covariance",IrTrackCovarianceV1),
    ("maneuver_probability",ctypes.c_double),("track_quality",ctypes.c_double)]
# Reuses the one generic IrCommandStatusV1 layout. A successful CommandStatus
# whose own state is Rejected is still AMS_MEL_OK.
class IrTrackUpdateResultV1(ctypes.Structure): _fields_ = [("status",IrCommandStatusV1),("error_code",ctypes.c_uint32)]
# Complete @Optional SystemTrackDataResponse input. system_time_ns stays signed
# nanoseconds, the ranges/rates stay in upstream meters and meters/second, and
# the angles stay in radians; nothing is clamped or normalized. az_el_valid and
# range_valid use the established uint8 bool representation and accept only 0
# or 1. The canonical IrAzElV1 is reused for both angle pairs.
class IrSystemTrackDataResponseV1(ctypes.Structure): _fields_ = [
    ("system_time_ns",ctypes.c_int64),
    ("command_id",ctypes.c_uint32),("request_id",ctypes.c_uint32),("track_id",ctypes.c_uint32),
    ("range_m",ctypes.c_double),("range_rate_mps",ctypes.c_double),
    ("range_error_m",ctypes.c_double),("range_rate_error_mps",ctypes.c_double),
    ("az_el_valid",ctypes.c_uint8),("range_valid",ctypes.c_uint8),
    ("inertial_az_el",IrAzElV1),("az_el_error",IrAzElV1)]
# Semantically distinct from IrTrackUpdateResultV1 while reusing the one
# generic IrCommandStatusV1 layout.
class IrTrackSystemResponseResultV1(ctypes.Structure): _fields_ = [("status",IrCommandStatusV1),("error_code",ctypes.c_uint32)]
class IrSensorInertialStateSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrSensorInertialStateV1)),("size",ctypes.c_size_t)]
class IrSensorNavStateSpanV1(ctypes.Structure): _fields_ = [("data",ctypes.POINTER(IrSensorNavStateV1)),("size",ctypes.c_size_t)]
class IrFrameSnapshotV1(ctypes.Structure): _fields_ = [("system_time_ns",ctypes.c_int64),("integration_time_ns",ctypes.c_int64),("width",ctypes.c_uint32),("height",ctypes.c_uint32),("bits_per_pixel",ctypes.c_uint32),("number_of_bands",ctypes.c_uint32),("horizontal_fov_rad",ctypes.c_double),("vertical_fov_rad",ctypes.c_double),("contributing_sensor",IrContributingSensorV1),("pixel_format",ctypes.c_uint32),("frame_id",ctypes.c_uint32),("subframe_id",ctypes.c_uint32),("subframe_total",ctypes.c_uint32),("image_type",ctypes.c_uint32),("image_flip",ctypes.c_uint32),("image_flags",U32SpanV1),("dither_row",ctypes.c_double),("dither_column",ctypes.c_double),("row_offset",ctypes.c_uint32),("column_offset",ctypes.c_uint32),("sensor_inertial_states",IrSensorInertialStateSpanV1),("sensor_nav_states",IrSensorNavStateSpanV1),("band_index",ctypes.c_uint8),("pixels",U8SpanV1)]
class ForeignKeyV1(ctypes.Structure): _fields_ = [("key", StringViewV1), ("system_name", StringViewV1)]
class InstallationDetailsV1(ctypes.Structure): _fields_ = [("location", ComponentLocationV1), ("orientation", EulerV1), ("boresight", EulerV1)]
class TemperatureStatusV1(ctypes.Structure): _fields_ = [("temperature_c", ctypes.c_double), ("state", ctypes.c_uint32)]
class MfaComponentV1(ctypes.Structure): _fields_ = [("component_id", UciIdV1), ("state", ctypes.c_uint32), ("temperature", TemperatureStatusV1), ("installation_location_id", ForeignKeyV1), ("installation_details", InstallationDetailsV1)]
class MfaComponentSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(MfaComponentV1)), ("size", ctypes.c_size_t)]
class AboutV1(ctypes.Structure): _fields_ = [("model", StringViewV1), ("serial_number", StringViewV1), ("software_version", StringViewV1), ("bootloader_software_version", StringViewV1), ("hardware_version", StringViewV1)]
class MfaStatusV1(ctypes.Structure): _fields_ = [("state", ctypes.c_uint32), ("state_description", StringViewV1), ("mode_description", StringViewV1), ("transition_status", ctypes.c_uint32), ("about", AboutV1), ("components", MfaComponentSpanV1)]
class IrSubsystemDepInfoV1(ctypes.Structure): _fields_ = [("subsystem_id", ctypes.c_uint32), ("criticality", ctypes.c_uint32), ("failure", ctypes.c_uint32)]
class IrSubsystemDepInfoSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(IrSubsystemDepInfoV1)), ("size", ctypes.c_size_t)]
class IrVersionV1(ctypes.Structure): _fields_ = [("source", ctypes.c_uint32), ("major_revision", ctypes.c_uint32), ("minor_revision", ctypes.c_uint32), ("engineering_revision", ctypes.c_uint32)]
class IrSubsystemCsciInfoV1(ctypes.Structure): _fields_ = [("csci", StringViewV1), ("mode", ctypes.c_uint32), ("version", IrVersionV1), ("criticality", ctypes.c_uint32), ("failure", ctypes.c_uint32), ("bit_report", ctypes.c_uint32), ("connection_established", ctypes.c_uint32)]
class IrSubsystemCsciInfoSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(IrSubsystemCsciInfoV1)), ("size", ctypes.c_size_t)]
class IrSubsystemStatusV1(ctypes.Structure): _fields_ = [("subsystem_id", ctypes.c_uint32), ("criticality", ctypes.c_uint32), ("status_sequence_number", ctypes.c_uint32), ("failure", ctypes.c_uint32), ("subsystem_count", ctypes.c_uint32), ("subsystems", IrSubsystemDepInfoSpanV1), ("csci_count", ctypes.c_uint32), ("csci", IrSubsystemCsciInfoSpanV1)]
class NameValuePairV1(ctypes.Structure): _fields_ = [("name", StringViewV1), ("value", StringViewV1)]
class NameValuePairSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(NameValuePairV1)), ("size", ctypes.c_size_t)]
class SecurityArtifactV1(ctypes.Structure): _fields_ = [("component_id", UciIdV1), ("associated_id", UciIdV1)]
class SecurityArtifactSpanV1(ctypes.Structure): _fields_ = [("data", ctypes.POINTER(SecurityArtifactV1)), ("size", ctypes.c_size_t)]
class SecurityEventV1(ctypes.Structure): _fields_ = [("kind", ctypes.c_uint32), ("category", ctypes.c_uint32), ("details", StringViewV1), ("subsystem_id", UciIdV1), ("service_id", UciIdV1), ("mdf_id", UciIdV1)]
class SecurityAuditRecordV1(ctypes.Structure): _fields_ = [("security_event_id", UciIdV1), ("event_timestamp_ns", ctypes.c_int64), ("subsystem_id", UciIdV1), ("artifacts", SecurityArtifactSpanV1), ("event", SecurityEventV1), ("outcome", ctypes.c_uint32), ("severity", ctypes.c_uint32)]
class IrHealthMetadataEventV1(ctypes.Structure): _fields_ = [("kind", ctypes.c_uint32), ("mfa_status", MfaStatusV1), ("bit_status", BitStatusV1), ("subsystem_status", IrSubsystemStatusV1), ("discrete_status", NameValuePairSpanV1), ("security_audit", SecurityAuditRecordV1), ("mfa_status_detailed", NameValuePairSpanV1)]


class IrModeResultV1(ctypes.Structure):
    _fields_ = [
        ("mode", ctypes.c_uint32),
        ("error_code", ctypes.c_uint32),
    ]


class IrReturnResultV1(ctypes.Structure):
    _fields_ = [
        ("value", ctypes.c_uint32),
        ("error_code", ctypes.c_uint32),
    ]


class IrFrameV1(ctypes.Structure):
    _fields_ = [
        ("system_time_ns", ctypes.c_int64),
        ("integration_time_ns", ctypes.c_int64),
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
        ("bits_per_pixel", ctypes.c_uint32),
        ("number_of_bands", ctypes.c_uint32),
        ("horizontal_fov_rad", ctypes.c_double),
        ("vertical_fov_rad", ctypes.c_double),
        ("pixel_format", ctypes.c_uint32),
        ("frame_id", ctypes.c_uint32),
        ("subframe_id", ctypes.c_uint32),
        ("subframe_total", ctypes.c_uint32),
        ("image_type", ctypes.c_uint32),
        ("image_flip", ctypes.c_uint32),
        ("image_flags", ctypes.c_uint32),
        ("dither_row", ctypes.c_double),
        ("dither_column", ctypes.c_double),
        ("row_offset", ctypes.c_uint32),
        ("column_offset", ctypes.c_uint32),
        ("band_index", ctypes.c_uint8),
        ("reserved", ctypes.c_uint8 * 7),
        ("pixels", ctypes.POINTER(ctypes.c_uint8)),
        ("pixel_capacity", ctypes.c_size_t),
        ("pixel_required", ctypes.c_size_t),
    ]


class IrStreamCountersV1(ctypes.Structure):
    _fields_ = [
        ("frames_received", ctypes.c_uint64),
        ("frames_dropped_queue_full", ctypes.c_uint64),
        ("malformed_or_unsupported_frames", ctypes.c_uint64),
    ]


SessionHandle = ctypes.c_void_p
IrStreamHandle = ctypes.c_void_p
IrFrameSnapshotHandle = ctypes.c_void_p
IrImageMetadataHandle = ctypes.c_void_p
IrImageMetadataEventHandle = ctypes.c_void_p
IrC2Handle = ctypes.c_void_p
IrModeRequestHandle = ctypes.c_void_p
IrReturnRequestHandle = ctypes.c_void_p
IrChannelCommsRequestHandle = ctypes.c_void_p
IrChannelCapabilityHandle = ctypes.c_void_p
IrC2MetadataHandle = ctypes.c_void_p
IrC2MetadataEventHandle = ctypes.c_void_p
IrHealthHandle = ctypes.c_void_p
IrHealthMetadataHandle = ctypes.c_void_p
IrHealthMetadataEventHandle = ctypes.c_void_p
IrNavigationRequestHandle = ctypes.c_void_p
IrInstrumentationHandle = ctypes.c_void_p
IrInstrumentationRequestHandle = ctypes.c_void_p
IrInstrumentationMetadataHandle = ctypes.c_void_p
IrInstrumentationMetadataEventHandle = ctypes.c_void_p
# Track channel ownership/lifecycle foundation, the @RequiredIfTrack
# IRSTTrackReport callback, and the @RequiredIfTrackUpdate TrackDataUpdate send.
IrTrackHandle = ctypes.c_void_p
IrTrackMetadataHandle = ctypes.c_void_p
IrTrackMetadataEventHandle = ctypes.c_void_p
IrTrackUpdateRequestHandle = ctypes.c_void_p
# A deliberately distinct handle for the @Optional SystemTrackDataResponse
# request family.
IrTrackSystemResponseRequestHandle = ctypes.c_void_p
CharPointer = ctypes.POINTER(ctypes.c_char)
SizePointer = ctypes.POINTER(ctypes.c_size_t)


def _library_path() -> Path:
    value = os.environ.get("AMS_MEL_NATIVE_LIB")
    if not value:
        raise ImportError(
            "AMS_MEL_NATIVE_LIB must name the full path to libams_mel_c.so.0 "
            "or libams_mel_c.so"
        )
    path = Path(value)
    if not path.is_absolute():
        raise ImportError("AMS_MEL_NATIVE_LIB must be an absolute path")
    return path


_LIBRARY = ctypes.CDLL(str(_library_path()))

ams_mel_get_abi_version = _LIBRARY.ams_mel_get_abi_version
ams_mel_get_abi_version.argtypes = [ctypes.POINTER(AbiVersionV1)]
ams_mel_get_abi_version.restype = ctypes.c_int32

ams_mel_session_open = _LIBRARY.ams_mel_session_open
ams_mel_session_open.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(SessionHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_session_open.restype = ctypes.c_int32

ams_mel_session_open_with_options = _LIBRARY.ams_mel_session_open_with_options
ams_mel_session_open_with_options.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(SessionOptionsV1),
    ctypes.POINTER(SessionHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_session_open_with_options.restype = ctypes.c_int32

ams_mel_session_get_provider_version = (
    _LIBRARY.ams_mel_session_get_provider_version
)
ams_mel_session_get_provider_version.argtypes = [
    SessionHandle,
    ctypes.POINTER(ProviderVersionV1),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_session_get_provider_version.restype = ctypes.c_int32

ams_mel_session_close = _LIBRARY.ams_mel_session_close
ams_mel_session_close.argtypes = [
    ctypes.POINTER(SessionHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_session_close.restype = ctypes.c_int32

ams_mel_ir_stream_open = _LIBRARY.ams_mel_ir_stream_open
ams_mel_ir_stream_open.argtypes = [
    SessionHandle,
    ctypes.POINTER(IrStreamConfigV1),
    ctypes.POINTER(IrStreamHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_open.restype = ctypes.c_int32

ams_mel_ir_stream_start = _LIBRARY.ams_mel_ir_stream_start
ams_mel_ir_stream_start.argtypes = [
    IrStreamHandle,
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_start.restype = ctypes.c_int32

ams_mel_ir_stream_receive = _LIBRARY.ams_mel_ir_stream_receive
ams_mel_ir_stream_receive.argtypes = [
    IrStreamHandle,
    ctypes.c_uint32,
    ctypes.POINTER(IrFrameV1),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_receive.restype = ctypes.c_int32
ams_mel_ir_stream_get_capabilities = _LIBRARY.ams_mel_ir_stream_get_capabilities
ams_mel_ir_stream_get_capabilities.argtypes = [IrStreamHandle,ctypes.POINTER(IrChannelCapabilityHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_stream_get_capabilities.restype = ctypes.c_int32
ams_mel_ir_image_metadata_open = _LIBRARY.ams_mel_ir_image_metadata_open
ams_mel_ir_image_metadata_open.argtypes = [IrStreamHandle,ctypes.c_size_t,ctypes.POINTER(IrImageMetadataHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_open.restype = ctypes.c_int32
ams_mel_ir_image_metadata_receive = _LIBRARY.ams_mel_ir_image_metadata_receive
ams_mel_ir_image_metadata_receive.argtypes = [IrImageMetadataHandle,ctypes.c_uint32,ctypes.POINTER(IrImageMetadataEventHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_receive.restype = ctypes.c_int32
ams_mel_ir_image_metadata_get_counters = _LIBRARY.ams_mel_ir_image_metadata_get_counters
ams_mel_ir_image_metadata_get_counters.argtypes = [IrImageMetadataHandle,ctypes.POINTER(IrC2MetadataCountersV1),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_get_counters.restype = ctypes.c_int32
ams_mel_ir_image_metadata_close = _LIBRARY.ams_mel_ir_image_metadata_close
ams_mel_ir_image_metadata_close.argtypes = [ctypes.POINTER(IrImageMetadataHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_close.restype = ctypes.c_int32
ams_mel_ir_image_metadata_event_view = _LIBRARY.ams_mel_ir_image_metadata_event_view
ams_mel_ir_image_metadata_event_view.argtypes = [IrImageMetadataEventHandle,ctypes.POINTER(ctypes.POINTER(IrImageMetadataEventV1)),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_event_view.restype = ctypes.c_int32
ams_mel_ir_image_metadata_event_close = _LIBRARY.ams_mel_ir_image_metadata_event_close
ams_mel_ir_image_metadata_event_close.argtypes = [ctypes.POINTER(IrImageMetadataEventHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_image_metadata_event_close.restype = ctypes.c_int32

ams_mel_ir_stream_submit_navigation_report = _LIBRARY.ams_mel_ir_stream_submit_navigation_report
ams_mel_ir_stream_submit_navigation_report.argtypes = [IrStreamHandle,ctypes.POINTER(NavigationReportV1),ctypes.POINTER(IrNavigationRequestHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_stream_submit_navigation_report.restype = ctypes.c_int32

ams_mel_ir_navigation_request_wait = _LIBRARY.ams_mel_ir_navigation_request_wait
ams_mel_ir_navigation_request_wait.argtypes = [IrNavigationRequestHandle,ctypes.c_uint32,ctypes.POINTER(IrNavigationResultV1),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_navigation_request_wait.restype = ctypes.c_int32

ams_mel_ir_navigation_request_close = _LIBRARY.ams_mel_ir_navigation_request_close
ams_mel_ir_navigation_request_close.argtypes = [ctypes.POINTER(IrNavigationRequestHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_navigation_request_close.restype = ctypes.c_int32
ams_mel_ir_stream_receive_snapshot = _LIBRARY.ams_mel_ir_stream_receive_snapshot
ams_mel_ir_stream_receive_snapshot.argtypes = [IrStreamHandle,ctypes.c_uint32,ctypes.POINTER(IrFrameSnapshotHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_stream_receive_snapshot.restype = ctypes.c_int32
ams_mel_ir_frame_snapshot_view = _LIBRARY.ams_mel_ir_frame_snapshot_view
ams_mel_ir_frame_snapshot_view.argtypes = [IrFrameSnapshotHandle,ctypes.POINTER(ctypes.POINTER(IrFrameSnapshotV1)),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_frame_snapshot_view.restype = ctypes.c_int32
ams_mel_ir_frame_snapshot_close = _LIBRARY.ams_mel_ir_frame_snapshot_close
ams_mel_ir_frame_snapshot_close.argtypes = [ctypes.POINTER(IrFrameSnapshotHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_frame_snapshot_close.restype = ctypes.c_int32

ams_mel_ir_stream_get_counters = _LIBRARY.ams_mel_ir_stream_get_counters
ams_mel_ir_stream_get_counters.argtypes = [
    IrStreamHandle,
    ctypes.POINTER(IrStreamCountersV1),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_get_counters.restype = ctypes.c_int32

ams_mel_ir_stream_stop = _LIBRARY.ams_mel_ir_stream_stop
ams_mel_ir_stream_stop.argtypes = [
    IrStreamHandle,
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_stop.restype = ctypes.c_int32

ams_mel_ir_stream_close = _LIBRARY.ams_mel_ir_stream_close
ams_mel_ir_stream_close.argtypes = [
    ctypes.POINTER(IrStreamHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_stream_close.restype = ctypes.c_int32

ams_mel_ir_c2_open = _LIBRARY.ams_mel_ir_c2_open
ams_mel_ir_c2_open.argtypes = [
    SessionHandle,
    ctypes.POINTER(IrC2ConfigV1),
    ctypes.POINTER(IrC2Handle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_c2_open.restype = ctypes.c_int32

ams_mel_ir_c2_enable = _LIBRARY.ams_mel_ir_c2_enable
ams_mel_ir_c2_enable.argtypes = [
    IrC2Handle,
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_c2_enable.restype = ctypes.c_int32

ams_mel_ir_c2_submit_operate = _LIBRARY.ams_mel_ir_c2_submit_operate
ams_mel_ir_c2_submit_operate.argtypes = [
    IrC2Handle,
    ctypes.c_uint32,
    ctypes.POINTER(IrModeRequestHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_c2_submit_operate.restype = ctypes.c_int32

ams_mel_ir_c2_submit_mode = _LIBRARY.ams_mel_ir_c2_submit_mode
ams_mel_ir_c2_submit_mode.argtypes = [IrC2Handle, ctypes.POINTER(IrModeCommandV1),
    ctypes.POINTER(IrModeRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_c2_submit_mode.restype = ctypes.c_int32

ams_mel_ir_mode_request_wait = _LIBRARY.ams_mel_ir_mode_request_wait
ams_mel_ir_mode_request_wait.argtypes = [
    IrModeRequestHandle,
    ctypes.c_uint32,
    ctypes.POINTER(IrModeResultV1),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_mode_request_wait.restype = ctypes.c_int32

ams_mel_ir_mode_request_close = _LIBRARY.ams_mel_ir_mode_request_close
ams_mel_ir_mode_request_close.argtypes = [
    ctypes.POINTER(IrModeRequestHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_mode_request_close.restype = ctypes.c_int32

ams_mel_ir_c2_submit_bit_noop = _LIBRARY.ams_mel_ir_c2_submit_bit_noop
ams_mel_ir_c2_submit_bit_noop.argtypes = [
    IrC2Handle,
    ctypes.c_uint32,
    ctypes.POINTER(IrReturnRequestHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_c2_submit_bit_noop.restype = ctypes.c_int32

ams_mel_ir_c2_submit_bit = _LIBRARY.ams_mel_ir_c2_submit_bit
ams_mel_ir_c2_submit_bit.argtypes = [IrC2Handle, ctypes.POINTER(IrBitCommandV1),
    ctypes.POINTER(IrReturnRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_c2_submit_bit.restype = ctypes.c_int32
ams_mel_ir_c2_submit_config_set = _LIBRARY.ams_mel_ir_c2_submit_config_set
ams_mel_ir_c2_submit_config_set.argtypes = [IrC2Handle,
    ctypes.POINTER(IrConfigSetCommandV1), ctypes.POINTER(IrReturnRequestHandle),
    CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_c2_submit_config_set.restype = ctypes.c_int32
ams_mel_ir_c2_send_keepalive = _LIBRARY.ams_mel_ir_c2_send_keepalive
ams_mel_ir_c2_send_keepalive.argtypes = [IrC2Handle,ctypes.POINTER(IrReturnRequestHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_send_keepalive.restype = ctypes.c_int32
ams_mel_ir_c2_submit_comms_test = _LIBRARY.ams_mel_ir_c2_submit_comms_test
ams_mel_ir_c2_submit_comms_test.argtypes = [IrC2Handle,ctypes.POINTER(IrChannelCommsTestRequestV1),ctypes.POINTER(IrChannelCommsRequestHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_submit_comms_test.restype = ctypes.c_int32
ams_mel_ir_channel_comms_request_wait = _LIBRARY.ams_mel_ir_channel_comms_request_wait
ams_mel_ir_channel_comms_request_wait.argtypes = [IrChannelCommsRequestHandle,ctypes.c_uint32,ctypes.POINTER(IrChannelCommsTestResultV1),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_channel_comms_request_wait.restype = ctypes.c_int32
ams_mel_ir_channel_comms_request_close = _LIBRARY.ams_mel_ir_channel_comms_request_close
ams_mel_ir_channel_comms_request_close.argtypes = [ctypes.POINTER(IrChannelCommsRequestHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_channel_comms_request_close.restype = ctypes.c_int32
ams_mel_ir_c2_get_capabilities = _LIBRARY.ams_mel_ir_c2_get_capabilities
ams_mel_ir_c2_get_capabilities.argtypes = [IrC2Handle,ctypes.POINTER(IrChannelCapabilityHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_get_capabilities.restype = ctypes.c_int32
ams_mel_ir_channel_capability_view = _LIBRARY.ams_mel_ir_channel_capability_view
ams_mel_ir_channel_capability_view.argtypes = [IrChannelCapabilityHandle,ctypes.POINTER(ctypes.POINTER(IrChannelCapabilityV1)),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_channel_capability_view.restype = ctypes.c_int32
ams_mel_ir_channel_capability_close = _LIBRARY.ams_mel_ir_channel_capability_close
ams_mel_ir_channel_capability_close.argtypes = [ctypes.POINTER(IrChannelCapabilityHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_channel_capability_close.restype = ctypes.c_int32

ams_mel_ir_return_request_wait = _LIBRARY.ams_mel_ir_return_request_wait
ams_mel_ir_return_request_wait.argtypes = [
    IrReturnRequestHandle,
    ctypes.c_uint32,
    ctypes.POINTER(IrReturnResultV1),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_return_request_wait.restype = ctypes.c_int32

ams_mel_ir_return_request_close = _LIBRARY.ams_mel_ir_return_request_close
ams_mel_ir_return_request_close.argtypes = [
    ctypes.POINTER(IrReturnRequestHandle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_return_request_close.restype = ctypes.c_int32

ams_mel_ir_c2_close = _LIBRARY.ams_mel_ir_c2_close
ams_mel_ir_c2_close.argtypes = [
    ctypes.POINTER(IrC2Handle),
    CharPointer,
    ctypes.c_size_t,
    SizePointer,
]
ams_mel_ir_c2_close.restype = ctypes.c_int32

ams_mel_ir_c2_metadata_open = _LIBRARY.ams_mel_ir_c2_metadata_open
ams_mel_ir_c2_metadata_open.argtypes = [IrC2Handle,ctypes.c_size_t,ctypes.POINTER(IrC2MetadataHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_open.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_register_comms_test = _LIBRARY.ams_mel_ir_c2_metadata_register_comms_test
ams_mel_ir_c2_metadata_register_comms_test.argtypes = [IrC2MetadataHandle,CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_register_comms_test.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_receive = _LIBRARY.ams_mel_ir_c2_metadata_receive
ams_mel_ir_c2_metadata_receive.argtypes = [IrC2MetadataHandle,ctypes.c_uint32,ctypes.POINTER(IrC2MetadataEventHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_receive.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_get_counters = _LIBRARY.ams_mel_ir_c2_metadata_get_counters
ams_mel_ir_c2_metadata_get_counters.argtypes = [IrC2MetadataHandle,ctypes.POINTER(IrC2MetadataCountersV1),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_get_counters.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_close = _LIBRARY.ams_mel_ir_c2_metadata_close
ams_mel_ir_c2_metadata_close.argtypes = [ctypes.POINTER(IrC2MetadataHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_close.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_event_view = _LIBRARY.ams_mel_ir_c2_metadata_event_view
ams_mel_ir_c2_metadata_event_view.argtypes = [IrC2MetadataEventHandle,ctypes.POINTER(ctypes.POINTER(IrC2MetadataEventV1)),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_event_view.restype = ctypes.c_int32
ams_mel_ir_c2_metadata_event_close = _LIBRARY.ams_mel_ir_c2_metadata_event_close
ams_mel_ir_c2_metadata_event_close.argtypes = [ctypes.POINTER(IrC2MetadataEventHandle),CharPointer,ctypes.c_size_t,SizePointer]
ams_mel_ir_c2_metadata_event_close.restype = ctypes.c_int32

ams_mel_ir_health_open = _LIBRARY.ams_mel_ir_health_open
ams_mel_ir_health_open.argtypes = [SessionHandle, ctypes.POINTER(IrHealthConfigV1), ctypes.POINTER(IrHealthHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_open.restype = ctypes.c_int32
ams_mel_ir_health_enable = _LIBRARY.ams_mel_ir_health_enable
ams_mel_ir_health_enable.argtypes = [IrHealthHandle, CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_enable.restype = ctypes.c_int32
ams_mel_ir_health_get_capabilities = _LIBRARY.ams_mel_ir_health_get_capabilities
ams_mel_ir_health_get_capabilities.argtypes = [IrHealthHandle, ctypes.POINTER(IrChannelCapabilityHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_get_capabilities.restype = ctypes.c_int32
ams_mel_ir_health_close = _LIBRARY.ams_mel_ir_health_close
ams_mel_ir_health_close.argtypes = [ctypes.POINTER(IrHealthHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_close.restype = ctypes.c_int32
ams_mel_ir_health_metadata_open = _LIBRARY.ams_mel_ir_health_metadata_open
ams_mel_ir_health_metadata_open.argtypes = [IrHealthHandle, ctypes.c_size_t, ctypes.POINTER(IrHealthMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_open.restype = ctypes.c_int32
ams_mel_ir_health_metadata_receive = _LIBRARY.ams_mel_ir_health_metadata_receive
ams_mel_ir_health_metadata_receive.argtypes = [IrHealthMetadataHandle, ctypes.c_uint32, ctypes.POINTER(IrHealthMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_receive.restype = ctypes.c_int32
ams_mel_ir_health_metadata_get_counters = _LIBRARY.ams_mel_ir_health_metadata_get_counters
ams_mel_ir_health_metadata_get_counters.argtypes = [IrHealthMetadataHandle, ctypes.POINTER(IrC2MetadataCountersV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_get_counters.restype = ctypes.c_int32
ams_mel_ir_health_metadata_close = _LIBRARY.ams_mel_ir_health_metadata_close
ams_mel_ir_health_metadata_close.argtypes = [ctypes.POINTER(IrHealthMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_close.restype = ctypes.c_int32
ams_mel_ir_health_metadata_event_view = _LIBRARY.ams_mel_ir_health_metadata_event_view
ams_mel_ir_health_metadata_event_view.argtypes = [IrHealthMetadataEventHandle, ctypes.POINTER(ctypes.POINTER(IrHealthMetadataEventV1)), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_event_view.restype = ctypes.c_int32
ams_mel_ir_health_metadata_event_close = _LIBRARY.ams_mel_ir_health_metadata_event_close
ams_mel_ir_health_metadata_event_close.argtypes = [ctypes.POINTER(IrHealthMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_health_metadata_event_close.restype = ctypes.c_int32

ams_mel_ir_instrumentation_open = _LIBRARY.ams_mel_ir_instrumentation_open
ams_mel_ir_instrumentation_open.argtypes = [SessionHandle, ctypes.POINTER(IrInstrumentationConfigV1), ctypes.POINTER(IrInstrumentationHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_open.restype = ctypes.c_int32
ams_mel_ir_instrumentation_enable = _LIBRARY.ams_mel_ir_instrumentation_enable
ams_mel_ir_instrumentation_enable.argtypes = [IrInstrumentationHandle, CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_enable.restype = ctypes.c_int32
ams_mel_ir_instrumentation_get_capabilities = _LIBRARY.ams_mel_ir_instrumentation_get_capabilities
ams_mel_ir_instrumentation_get_capabilities.argtypes = [IrInstrumentationHandle, ctypes.POINTER(IrChannelCapabilityHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_get_capabilities.restype = ctypes.c_int32
ams_mel_ir_instrumentation_submit_level = _LIBRARY.ams_mel_ir_instrumentation_submit_level
ams_mel_ir_instrumentation_submit_level.argtypes = [IrInstrumentationHandle, ctypes.POINTER(IrInstrumentationLevelCommandV1), ctypes.POINTER(IrInstrumentationRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_submit_level.restype = ctypes.c_int32
ams_mel_ir_instrumentation_request_wait = _LIBRARY.ams_mel_ir_instrumentation_request_wait
ams_mel_ir_instrumentation_request_wait.argtypes = [IrInstrumentationRequestHandle, ctypes.c_uint32, ctypes.POINTER(IrInstrumentationResultV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_request_wait.restype = ctypes.c_int32
ams_mel_ir_instrumentation_request_close = _LIBRARY.ams_mel_ir_instrumentation_request_close
ams_mel_ir_instrumentation_request_close.argtypes = [ctypes.POINTER(IrInstrumentationRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_request_close.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_open = _LIBRARY.ams_mel_ir_instrumentation_metadata_open
ams_mel_ir_instrumentation_metadata_open.argtypes = [IrInstrumentationHandle, ctypes.c_size_t, ctypes.POINTER(IrInstrumentationMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_open.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_receive = _LIBRARY.ams_mel_ir_instrumentation_metadata_receive
ams_mel_ir_instrumentation_metadata_receive.argtypes = [IrInstrumentationMetadataHandle, ctypes.c_uint32, ctypes.POINTER(IrInstrumentationMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_receive.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_get_counters = _LIBRARY.ams_mel_ir_instrumentation_metadata_get_counters
ams_mel_ir_instrumentation_metadata_get_counters.argtypes = [IrInstrumentationMetadataHandle, ctypes.POINTER(IrC2MetadataCountersV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_get_counters.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_close = _LIBRARY.ams_mel_ir_instrumentation_metadata_close
ams_mel_ir_instrumentation_metadata_close.argtypes = [ctypes.POINTER(IrInstrumentationMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_close.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_event_view = _LIBRARY.ams_mel_ir_instrumentation_metadata_event_view
ams_mel_ir_instrumentation_metadata_event_view.argtypes = [IrInstrumentationMetadataEventHandle, ctypes.POINTER(ctypes.POINTER(IrInstrumentationMetadataEventV1)), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_event_view.restype = ctypes.c_int32
ams_mel_ir_instrumentation_metadata_event_close = _LIBRARY.ams_mel_ir_instrumentation_metadata_event_close
ams_mel_ir_instrumentation_metadata_event_close.argtypes = [ctypes.POINTER(IrInstrumentationMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_metadata_event_close.restype = ctypes.c_int32
ams_mel_ir_instrumentation_close = _LIBRARY.ams_mel_ir_instrumentation_close
ams_mel_ir_instrumentation_close.argtypes = [ctypes.POINTER(IrInstrumentationHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_instrumentation_close.restype = ctypes.c_int32
ams_mel_ir_track_open = _LIBRARY.ams_mel_ir_track_open
ams_mel_ir_track_open.argtypes = [SessionHandle, ctypes.POINTER(IrTrackConfigV1), ctypes.POINTER(IrTrackHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_open.restype = ctypes.c_int32
ams_mel_ir_track_enable = _LIBRARY.ams_mel_ir_track_enable
ams_mel_ir_track_enable.argtypes = [IrTrackHandle, CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_enable.restype = ctypes.c_int32
ams_mel_ir_track_get_capabilities = _LIBRARY.ams_mel_ir_track_get_capabilities
ams_mel_ir_track_get_capabilities.argtypes = [IrTrackHandle, ctypes.POINTER(IrChannelCapabilityHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_get_capabilities.restype = ctypes.c_int32
ams_mel_ir_track_close = _LIBRARY.ams_mel_ir_track_close
ams_mel_ir_track_close.argtypes = [ctypes.POINTER(IrTrackHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_close.restype = ctypes.c_int32
ams_mel_ir_track_metadata_open = _LIBRARY.ams_mel_ir_track_metadata_open
ams_mel_ir_track_metadata_open.argtypes = [IrTrackHandle, ctypes.c_size_t, ctypes.POINTER(IrTrackMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_open.restype = ctypes.c_int32
ams_mel_ir_track_metadata_receive = _LIBRARY.ams_mel_ir_track_metadata_receive
ams_mel_ir_track_metadata_receive.argtypes = [IrTrackMetadataHandle, ctypes.c_uint32, ctypes.POINTER(IrTrackMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_receive.restype = ctypes.c_int32
ams_mel_ir_track_metadata_get_counters = _LIBRARY.ams_mel_ir_track_metadata_get_counters
ams_mel_ir_track_metadata_get_counters.argtypes = [IrTrackMetadataHandle, ctypes.POINTER(IrC2MetadataCountersV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_get_counters.restype = ctypes.c_int32
ams_mel_ir_track_metadata_close = _LIBRARY.ams_mel_ir_track_metadata_close
ams_mel_ir_track_metadata_close.argtypes = [ctypes.POINTER(IrTrackMetadataHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_close.restype = ctypes.c_int32
ams_mel_ir_track_metadata_event_view = _LIBRARY.ams_mel_ir_track_metadata_event_view
ams_mel_ir_track_metadata_event_view.argtypes = [IrTrackMetadataEventHandle, ctypes.POINTER(ctypes.POINTER(IrTrackMetadataEventV1)), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_event_view.restype = ctypes.c_int32
ams_mel_ir_track_metadata_event_view_v2 = _LIBRARY.ams_mel_ir_track_metadata_event_view_v2
ams_mel_ir_track_metadata_event_view_v2.argtypes = [IrTrackMetadataEventHandle, ctypes.POINTER(ctypes.POINTER(IrTrackMetadataEventV2)), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_event_view_v2.restype = ctypes.c_int32
ams_mel_ir_track_metadata_event_view_v3 = _LIBRARY.ams_mel_ir_track_metadata_event_view_v3
ams_mel_ir_track_metadata_event_view_v3.argtypes = [IrTrackMetadataEventHandle, ctypes.POINTER(ctypes.POINTER(IrTrackMetadataEventV3)), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_event_view_v3.restype = ctypes.c_int32
ams_mel_ir_track_metadata_event_close = _LIBRARY.ams_mel_ir_track_metadata_event_close
ams_mel_ir_track_metadata_event_close.argtypes = [ctypes.POINTER(IrTrackMetadataEventHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_metadata_event_close.restype = ctypes.c_int32
ams_mel_ir_track_submit_update = _LIBRARY.ams_mel_ir_track_submit_update
ams_mel_ir_track_submit_update.argtypes = [IrTrackHandle, ctypes.POINTER(IrTrackDataUpdateV1), ctypes.POINTER(IrTrackUpdateRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_submit_update.restype = ctypes.c_int32
ams_mel_ir_track_update_request_wait = _LIBRARY.ams_mel_ir_track_update_request_wait
ams_mel_ir_track_update_request_wait.argtypes = [IrTrackUpdateRequestHandle, ctypes.c_uint32, ctypes.POINTER(IrTrackUpdateResultV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_update_request_wait.restype = ctypes.c_int32
ams_mel_ir_track_update_request_close = _LIBRARY.ams_mel_ir_track_update_request_close
ams_mel_ir_track_update_request_close.argtypes = [ctypes.POINTER(IrTrackUpdateRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_update_request_close.restype = ctypes.c_int32
ams_mel_ir_track_submit_system_track_data_response = _LIBRARY.ams_mel_ir_track_submit_system_track_data_response
ams_mel_ir_track_submit_system_track_data_response.argtypes = [IrTrackHandle, ctypes.POINTER(IrSystemTrackDataResponseV1), ctypes.POINTER(IrTrackSystemResponseRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_submit_system_track_data_response.restype = ctypes.c_int32
ams_mel_ir_track_system_response_request_wait = _LIBRARY.ams_mel_ir_track_system_response_request_wait
ams_mel_ir_track_system_response_request_wait.argtypes = [IrTrackSystemResponseRequestHandle, ctypes.c_uint32, ctypes.POINTER(IrTrackSystemResponseResultV1), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_system_response_request_wait.restype = ctypes.c_int32
ams_mel_ir_track_system_response_request_close = _LIBRARY.ams_mel_ir_track_system_response_request_close
ams_mel_ir_track_system_response_request_close.argtypes = [ctypes.POINTER(IrTrackSystemResponseRequestHandle), CharPointer, ctypes.c_size_t, SizePointer]
ams_mel_ir_track_system_response_request_close.restype = ctypes.c_int32

BOUND_FUNCTION_NAMES = (
    "ams_mel_get_abi_version",
    "ams_mel_session_open",
    "ams_mel_session_open_with_options",
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
    "ams_mel_ir_track_metadata_open",
    "ams_mel_ir_track_metadata_receive",
    "ams_mel_ir_track_metadata_get_counters",
    "ams_mel_ir_track_metadata_close",
    "ams_mel_ir_track_metadata_event_view",
    "ams_mel_ir_track_metadata_event_view_v2",
    "ams_mel_ir_track_metadata_event_view_v3",
    "ams_mel_ir_track_metadata_event_close",
    "ams_mel_ir_track_submit_update",
    "ams_mel_ir_track_update_request_wait",
    "ams_mel_ir_track_update_request_close",
    "ams_mel_ir_track_submit_system_track_data_response",
    "ams_mel_ir_track_system_response_request_wait",
    "ams_mel_ir_track_system_response_request_close",
)
