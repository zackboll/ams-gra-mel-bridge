"""Safe Python session foundation over the ``ams_mel_c`` façade.

Operations using the same :class:`Session` must be externally serialized. The
GIL does not strengthen the native MEL ownership or concurrency contract.
"""

from __future__ import annotations

import ctypes as _ctypes
from dataclasses import dataclass
from enum import Enum
import os
from typing import Callable

from . import _native


_DIAGNOSTIC_CAPACITY = 4096


@dataclass(frozen=True)
class AbiVersion:
    major: int
    minor: int


@dataclass(frozen=True)
class ProviderVersion:
    api_version: int
    library_version: int
    vendor: str
    description: str


class ErrorKind(Enum):
    INVALID_ARGUMENT = "invalid_argument"
    LIBRARY_LOAD_FAILED = "library_load_failed"
    SYMBOL_NOT_FOUND = "symbol_not_found"
    FACTORY_FAILED = "factory_failed"
    INITIALIZATION_FAILED = "initialization_failed"
    PROVIDER_EXCEPTION = "provider_exception"
    BUFFER_TOO_SMALL = "buffer_too_small"
    PROVIDER_FAILED = "provider_failed"
    INTERNAL_ERROR = "internal_error"
    INVALID_UTF8 = "invalid_utf8"
    PROTOCOL_INCONSISTENCY = "protocol_inconsistency"
    UNKNOWN = "unknown"


class MelError(Exception):
    """Structured failure from local validation or the native façade."""

    def __init__(
        self,
        kind: ErrorKind,
        diagnostic: str | None,
        diagnostic_required: int | None = None,
        native_status: int | None = None,
    ) -> None:
        self.kind = kind
        self.diagnostic = diagnostic
        self.diagnostic_required = diagnostic_required
        self.native_status = native_status
        detail = diagnostic if diagnostic is not None else "no complete diagnostic"
        super().__init__(f"{kind.value}: {detail}")


_STATUS_KINDS = {
    _native.AMS_MEL_INVALID_ARGUMENT: ErrorKind.INVALID_ARGUMENT,
    _native.AMS_MEL_LIBRARY_LOAD_FAILED: ErrorKind.LIBRARY_LOAD_FAILED,
    _native.AMS_MEL_SYMBOL_NOT_FOUND: ErrorKind.SYMBOL_NOT_FOUND,
    _native.AMS_MEL_FACTORY_FAILED: ErrorKind.FACTORY_FAILED,
    _native.AMS_MEL_INITIALIZATION_FAILED: ErrorKind.INITIALIZATION_FAILED,
    _native.AMS_MEL_PROVIDER_EXCEPTION: ErrorKind.PROVIDER_EXCEPTION,
    _native.AMS_MEL_BUFFER_TOO_SMALL: ErrorKind.BUFFER_TOO_SMALL,
    _native.AMS_MEL_PROVIDER_FAILED: ErrorKind.PROVIDER_FAILED,
    _native.AMS_MEL_INTERNAL_ERROR: ErrorKind.INTERNAL_ERROR,
}


def _error_from_status(
    status: int, diagnostic: str | None, required: int | None
) -> MelError:
    return MelError(
        _STATUS_KINDS.get(status, ErrorKind.UNKNOWN),
        diagnostic,
        required,
        status,
    )


def _local_error(kind: ErrorKind, diagnostic: str) -> MelError:
    return MelError(kind, diagnostic)


def _decode_complete_buffer(buffer: bytes, field: str) -> str:
    if not buffer or buffer[-1] != 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native provider returned unterminated {field}",
        )
    payload = buffer[:-1]
    if b"\0" in payload:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native provider returned embedded NUL in {field}",
        )
    try:
        return payload.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise _local_error(
            ErrorKind.INVALID_UTF8,
            f"native provider returned invalid UTF-8 in {field}",
        ) from error


NativeCall = Callable[[_ctypes.Array[_ctypes.c_char], _ctypes.c_size_t], int]


def _call_with_diagnostic(call: NativeCall) -> None:
    diagnostic = _ctypes.create_string_buffer(_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(call(diagnostic, required))
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native diagnostic requires invalid capacity 0",
        )
    if required_value > _DIAGNOSTIC_CAPACITY:
        if status == _native.AMS_MEL_OK:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native call returned an oversized diagnostic",
            )
        raise _error_from_status(status, None, required_value)
    message = _decode_complete_buffer(
        bytes(diagnostic.raw[:required_value]), "diagnostic"
    )
    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native call returned a diagnostic",
            )
        return
    raise _error_from_status(status, message or None, required_value)


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


def abi_version() -> AbiVersion:
    """Return the version of the C façade without loading a provider."""

    raw = _native.AbiVersionV1()
    status = int(_native.ams_mel_get_abi_version(_ctypes.byref(raw)))
    if status != _native.AMS_MEL_OK:
        raise _error_from_status(status, None, None)
    return AbiVersion(int(raw.major), int(raw.minor))


class Session:
    """Unique owner of one native provider session."""

    def __init__(self) -> None:
        raise TypeError("Session objects must be created with Session.open")

    @classmethod
    def _from_owner(cls, owner: _native.SessionHandle) -> Session:
        session = cls.__new__(cls)
        session._owner = owner
        session._owner_pointer = _ctypes.pointer(owner)
        session._close_function = _native.ams_mel_session_close
        return session

    @classmethod
    def open(
        cls,
        provider_library: str | os.PathLike[str],
        instance: str,
        aperture: str,
    ) -> Session:
        """Load, create, and initialize a provider session."""

        try:
            path = os.fspath(provider_library)
        except TypeError as error:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "provider_library must be str or return str from os.fspath",
            ) from error
        if not isinstance(path, str):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "provider_library must be str; bytes paths are not accepted",
            )
        library_bytes = _utf8_input(path, "provider_library")
        instance_bytes = _utf8_input(instance, "instance")
        aperture_bytes = _utf8_input(aperture, "aperture")
        owner = _native.SessionHandle()

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_session_open(
                    library_bytes,
                    instance_bytes,
                    aperture_bytes,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_session_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "session open succeeded without returning an owner",
            )
        return cls._from_owner(owner)

    @property
    def is_open(self) -> bool:
        """Whether this object still owns a native session."""

        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "Session is closed")

    def provider_version(self) -> ProviderVersion:
        """Return the provider's complete version information."""

        self._require_open()
        raw = _native.ProviderVersionV1()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: (
                    _native.ams_mel_session_get_provider_version(
                        self._owner,
                        _ctypes.byref(raw),
                        diagnostic,
                        len(diagnostic),
                        _ctypes.byref(required),
                    )
                )
            )
        except MelError as error:
            if error.kind is not ErrorKind.BUFFER_TOO_SMALL:
                raise
        else:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "provider version size query unexpectedly succeeded",
            )

        vendor_required = int(raw.vendor_required)
        description_required = int(raw.description_required)
        if vendor_required == 0 or description_required == 0:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native provider returned a zero version string size",
            )
        vendor = _ctypes.create_string_buffer(vendor_required)
        description = _ctypes.create_string_buffer(description_required)
        _ctypes.memset(vendor, 0xFF, vendor_required)
        _ctypes.memset(description, 0xFF, description_required)
        raw.vendor = _ctypes.cast(vendor, _native.CharPointer)
        raw.vendor_capacity = vendor_required
        raw.description = _ctypes.cast(description, _native.CharPointer)
        raw.description_capacity = description_required

        _call_with_diagnostic(
            lambda diagnostic, required: (
                _native.ams_mel_session_get_provider_version(
                    self._owner,
                    _ctypes.byref(raw),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        )
        if (
            int(raw.vendor_required) != vendor_required
            or int(raw.description_required) != description_required
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "provider version sizes changed between query and copy",
            )
        return ProviderVersion(
            int(raw.api_version),
            int(raw.library_version),
            _decode_complete_buffer(bytes(vendor.raw), "vendor"),
            _decode_complete_buffer(bytes(description.raw), "description"),
        )

    def close(self) -> None:
        """Release this owner. Repeated successful close is harmless."""

        if not self.is_open:
            return
        _call_with_diagnostic(
            lambda diagnostic, required: self._close_function(
                self._owner_pointer,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def __enter__(self) -> Session:
        self._require_open()
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        if exception_type is None:
            self.close()
        else:
            try:
                self.close()
            except BaseException:
                pass
        return False

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


__all__ = [
    "AbiVersion",
    "ErrorKind",
    "MelError",
    "ProviderVersion",
    "Session",
    "abi_version",
]
