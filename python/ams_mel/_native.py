"""Private ctypes declarations for the Task-010 ``ams_mel_c`` subset."""

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
AMS_MEL_PROVIDER_FAILED = 11

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


SessionHandle = ctypes.c_void_p
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

BOUND_FUNCTION_NAMES = (
    "ams_mel_get_abi_version",
    "ams_mel_session_open",
    "ams_mel_session_get_provider_version",
    "ams_mel_session_close",
)
