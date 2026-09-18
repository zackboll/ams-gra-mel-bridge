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


class UciIdV1(ctypes.Structure):
    _fields_ = [
        ("uuid", ctypes.c_uint8 * 16),
        ("descriptive_label", StringViewV1),
    ]


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
    "ams_mel_ir_mode_request_wait",
    "ams_mel_ir_mode_request_close",
    "ams_mel_ir_c2_submit_bit_noop",
    "ams_mel_ir_return_request_wait",
    "ams_mel_ir_return_request_close",
    "ams_mel_ir_c2_close",
)
