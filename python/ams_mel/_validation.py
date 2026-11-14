"""Private validation implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
from ._errors import (
    ErrorKind,
    _local_error,
)


_INT64_MAX = (1 << 63) - 1


_UINT32_MAX = (1 << 32) - 1


_SIZE_T_MAX = (1 << (_ctypes.sizeof(_ctypes.c_size_t) * 8)) - 1


def _utf8_input(value: str, name: str) -> bytes:
    if not isinstance(value, str):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be str")
    if "\0" in value:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} contains an embedded NUL"
        )
    try:
        return value.encode("utf-8", errors="strict")
    except UnicodeEncodeError as error:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} must be valid UTF-8"
        ) from error


def _validate_size_t(value: int, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an integer")
    if value < 0 or value > _SIZE_T_MAX:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} must be representable in size_t"
        )


def _validate_timeout(timeout_ms: int) -> None:
    if isinstance(timeout_ms, bool) or not isinstance(timeout_ms, int):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "timeout_ms must be an integer")
    if timeout_ms < 0 or timeout_ms > _UINT32_MAX:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, "timeout_ms must be representable in uint32_t"
        )


def _validate_uint32(value: int, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an integer")
    if value < 0 or value > _UINT32_MAX:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} must be representable in uint32_t"
        )


def _validate_int64(value: int, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an integer")
    if value < -(_INT64_MAX + 1) or value > _INT64_MAX:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} must be representable in int64_t"
        )
