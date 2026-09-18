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

AMS_MEL_IR_CHANNEL_IRST_IMAGE = 1
AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL = 2
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
AMS_MEL_IR_IMAGE_STARING = 0
AMS_MEL_IR_IMAGE_SCANNING = 1
AMS_MEL_IR_FLIP_NONE = 0
AMS_MEL_IR_FLIP_VERTICAL = 1
AMS_MEL_IR_FLIP_HORIZONTAL = 2
AMS_MEL_IR_FLIP_BOTH = 3
AMS_MEL_IR_C2_METADATA_COMMAND_STATUS = 1
AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION = 2
AMS_MEL_IR_C2_METADATA_BIT_STATUS = 3
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
class IrC2MetadataEventV1(ctypes.Structure): _fields_ = [("kind",ctypes.c_uint32),("command_status",IrCommandStatusV1),("bit_configuration",BitConfigurationV1),("bit_status",BitStatusV1)]
class IrC2MetadataCountersV1(ctypes.Structure): _fields_ = [("events_received",ctypes.c_uint64),("events_dropped_queue_full",ctypes.c_uint64),("malformed_or_unsupported",ctypes.c_uint64)]


class ComponentLocationV1(ctypes.Structure):
    _fields_ = [
        ("offset_x_m", ctypes.c_double),
        ("offset_y_m", ctypes.c_double),
        ("offset_z_m", ctypes.c_double),
        ("key", StringViewV1),
        ("system_name", StringViewV1),
    ]


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
IrC2Handle = ctypes.c_void_p
IrModeRequestHandle = ctypes.c_void_p
IrReturnRequestHandle = ctypes.c_void_p
IrC2MetadataHandle = ctypes.c_void_p
IrC2MetadataEventHandle = ctypes.c_void_p
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

BOUND_FUNCTION_NAMES = (
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
    "ams_mel_ir_c2_metadata_open",
    "ams_mel_ir_c2_metadata_receive",
    "ams_mel_ir_c2_metadata_get_counters",
    "ams_mel_ir_c2_metadata_close",
    "ams_mel_ir_c2_metadata_event_view",
    "ams_mel_ir_c2_metadata_event_close",
)
