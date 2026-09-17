"""Safe Python sessions and IR Mono8 streams over the ``ams_mel_c`` façade.

Operations using one owner must follow the native external-serialization
contract. At most one receive operation may consume an :class:`ImageStream` at
a time. The GIL is not a substitute for either contract; this API does not
claim that ImageStream is thread-safe or support concurrent stop/receive.
"""

from __future__ import annotations

import ctypes as _ctypes
from dataclasses import dataclass
from enum import Enum
import os
from typing import Callable

from . import _native


_DIAGNOSTIC_CAPACITY = 4096
_INT64_MAX = (1 << 63) - 1
_UINT32_MAX = (1 << 32) - 1
_SIZE_T_MAX = (1 << (_ctypes.sizeof(_ctypes.c_size_t) * 8)) - 1


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


@dataclass(frozen=True)
class UciId:
    uuid: bytes
    descriptive_label: str

    def __post_init__(self) -> None:
        if not isinstance(self.uuid, bytes) or len(self.uuid) != 16:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "uuid must be exactly 16 bytes"
            )
        _utf8_input(self.descriptive_label, "descriptive_label")


@dataclass(frozen=True)
class ComponentLocation:
    offset_x_m: float
    offset_y_m: float
    offset_z_m: float
    key: str
    system_name: str

    def __post_init__(self) -> None:
        _utf8_input(self.key, "key")
        _utf8_input(self.system_name, "system_name")


@dataclass(frozen=True)
class ImageConfig:
    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation
    buffer_count: int = 3
    buffer_size: int = 1_048_576
    queue_capacity: int = 4

    def __post_init__(self) -> None:
        if not isinstance(self.channel_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "channel_id must be UciId")
        if not isinstance(self.platform_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "platform_id must be UciId")
        if not isinstance(self.sensor_location, ComponentLocation):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "sensor_location must be ComponentLocation",
            )
        for value, name in (
            (self.buffer_count, "buffer_count"),
            (self.buffer_size, "buffer_size"),
            (self.queue_capacity, "queue_capacity"),
        ):
            _validate_size_t(value, name)
            if value == 0:
                raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be positive")
        if self.buffer_size > _INT64_MAX:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "buffer_size must not exceed INT64_MAX"
            )
        if self.buffer_count - 1 > _INT64_MAX:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "buffer_count - 1 must not exceed INT64_MAX",
            )

    @classmethod
    def new(
        cls,
        channel_id: UciId,
        platform_id: UciId,
        sensor_location: ComponentLocation,
        buffer_count: int = 3,
        buffer_size: int = 1_048_576,
        queue_capacity: int = 4,
    ) -> ImageConfig:
        return cls(
            channel_id,
            platform_id,
            sensor_location,
            buffer_count,
            buffer_size,
            queue_capacity,
        )


class ImageType(Enum):
    STARING = "staring"
    SCANNING = "scanning"


class ImageFlip(Enum):
    NONE = "none"
    VERTICAL = "vertical"
    HORIZONTAL = "horizontal"
    BOTH = "both"


@dataclass(frozen=True)
class Frame:
    system_time_ns: int
    integration_time_ns: int
    width: int
    height: int
    bits_per_pixel: int
    number_of_bands: int
    horizontal_fov_rad: float
    vertical_fov_rad: float
    frame_id: int
    subframe_id: int
    subframe_total: int
    image_type: ImageType
    image_flip: ImageFlip
    image_flags: int
    dither_row: float
    dither_column: float
    row_offset: int
    column_offset: int
    band_index: int
    pixels: bytes


@dataclass(frozen=True)
class StreamCounters:
    frames_received: int
    frames_dropped_queue_full: int
    malformed_or_unsupported: int


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
    TIMEOUT = "timeout"
    STREAM_STOPPED = "stream_stopped"
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
    _native.AMS_MEL_TIMEOUT: ErrorKind.TIMEOUT,
    _native.AMS_MEL_STREAM_STOPPED: ErrorKind.STREAM_STOPPED,
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


class _RawImageConfig:
    """Exact C config plus strong references to all borrowed string storage."""

    def __init__(self, config: ImageConfig) -> None:
        self.buffers: list[_ctypes.Array[_ctypes.c_char]] = []
        self.raw = _native.IrStreamConfigV1(
            channel_type=_native.AMS_MEL_IR_CHANNEL_IRST_IMAGE,
            channel_id=self._id(config.channel_id),
            platform_id=self._id(config.platform_id),
            sensor_location=_native.ComponentLocationV1(
                offset_x_m=config.sensor_location.offset_x_m,
                offset_y_m=config.sensor_location.offset_y_m,
                offset_z_m=config.sensor_location.offset_z_m,
                key=self._view(config.sensor_location.key, "key"),
                system_name=self._view(
                    config.sensor_location.system_name, "system_name"
                ),
            ),
            buffer_count=config.buffer_count,
            buffer_size=config.buffer_size,
            queue_capacity=config.queue_capacity,
        )

    def _view(self, value: str, name: str) -> _native.StringViewV1:
        encoded = _utf8_input(value, name)
        buffer = _ctypes.create_string_buffer(encoded, len(encoded) + 1)
        self.buffers.append(buffer)
        return _native.StringViewV1(
            _ctypes.cast(buffer, _native.CharPointer), len(encoded)
        )

    def _id(self, value: UciId) -> _native.UciIdV1:
        return _native.UciIdV1(
            (_ctypes.c_uint8 * 16).from_buffer_copy(value.uuid),
            self._view(value.descriptive_label, "descriptive_label"),
        )


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

    def open_image_stream(self, config: ImageConfig) -> ImageStream:
        """Open a host-memory IRSTImage Mono8 stream without retaining this object."""

        self._require_open()
        if not isinstance(config, ImageConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be ImageConfig")
        bundle = _RawImageConfig(config)
        owner = _native.IrStreamHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_stream_open(
                    self._owner,
                    _ctypes.byref(bundle.raw),
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_stream_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "stream open succeeded without returning an owner",
            )
        return ImageStream._from_owner(owner)

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


class ImageStream:
    """Unique native stream owner; operations require external serialization."""

    def __init__(self) -> None:
        raise TypeError("ImageStream objects must be created by Session.open_image_stream")

    @classmethod
    def _from_owner(cls, owner: _native.IrStreamHandle) -> ImageStream:
        stream = cls.__new__(cls)
        stream._owner = owner
        stream._owner_pointer = _ctypes.pointer(owner)
        stream._close_function = _native.ams_mel_ir_stream_close
        return stream

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ImageStream is closed")

    def start(self) -> None:
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stream_start(
                self._owner,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def _receive_raw(self, timeout_ms: int, raw: _native.IrFrameV1) -> None:
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stream_receive(
                self._owner,
                timeout_ms,
                _ctypes.byref(raw),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def receive(self, timeout_ms: int) -> Frame:
        """Receive one Python-owned Mono8 frame. A zero timeout polls."""

        self._require_open()
        _validate_timeout(timeout_ms)
        raw = _native.IrFrameV1()
        try:
            self._receive_raw(timeout_ms, raw)
        except MelError as error:
            if error.kind is not ErrorKind.BUFFER_TOO_SMALL:
                raise
        else:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "empty receive unexpectedly succeeded",
            )

        pixel_required = int(raw.pixel_required)
        if pixel_required <= 0:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native frame requires zero pixel bytes",
            )
        try:
            pixel_storage = (_ctypes.c_uint8 * pixel_required)()
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate frame pixel storage",
            ) from None
        raw.pixels = _ctypes.cast(pixel_storage, _ctypes.POINTER(_ctypes.c_uint8))
        raw.pixel_capacity = pixel_required
        try:
            self._receive_raw(0, raw)
        except MelError as error:
            if error.kind is ErrorKind.BUFFER_TOO_SMALL:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "queued frame pixel requirement changed",
                ) from error
            if error.kind in (ErrorKind.TIMEOUT, ErrorKind.STREAM_STOPPED):
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "queued frame disappeared",
                ) from error
            raise
        if int(raw.pixel_required) != pixel_required:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "delivered frame pixel requirement changed",
            )
        pixels = bytes(pixel_storage)
        return _frame_from_raw(raw, pixels, pixel_required)

    def counters(self) -> StreamCounters:
        self._require_open()
        raw = _native.IrStreamCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stream_get_counters(
                self._owner,
                _ctypes.byref(raw),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        return StreamCounters(
            int(raw.frames_received),
            int(raw.frames_dropped_queue_full),
            int(raw.malformed_or_unsupported_frames),
        )

    def stop(self) -> None:
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stream_stop(
                self._owner,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def close(self) -> None:
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

    def __enter__(self) -> ImageStream:
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


def _frame_from_raw(
    raw: _native.IrFrameV1, pixels: bytes, pixel_required: int
) -> Frame:
    if (
        int(raw.pixel_format) != _native.AMS_MEL_IR_PIXEL_MONO
        or int(raw.bits_per_pixel) != 8
        or int(raw.number_of_bands) != 1
    ):
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native frame violates the Mono8 profile",
        )
    width = int(raw.width)
    height = int(raw.height)
    expected_bytes = width * height
    if width <= 0 or height <= 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native frame dimensions must be positive",
        )
    if expected_bytes != pixel_required or expected_bytes != len(pixels):
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native frame dimensions do not match pixel count",
        )
    image_types = {
        _native.AMS_MEL_IR_IMAGE_STARING: ImageType.STARING,
        _native.AMS_MEL_IR_IMAGE_SCANNING: ImageType.SCANNING,
    }
    image_flips = {
        _native.AMS_MEL_IR_FLIP_NONE: ImageFlip.NONE,
        _native.AMS_MEL_IR_FLIP_VERTICAL: ImageFlip.VERTICAL,
        _native.AMS_MEL_IR_FLIP_HORIZONTAL: ImageFlip.HORIZONTAL,
        _native.AMS_MEL_IR_FLIP_BOTH: ImageFlip.BOTH,
    }
    try:
        image_type = image_types[int(raw.image_type)]
        image_flip = image_flips[int(raw.image_flip)]
    except KeyError as error:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native frame contains an unknown image type or flip",
        ) from error
    return Frame(
        int(raw.system_time_ns),
        int(raw.integration_time_ns),
        width,
        height,
        int(raw.bits_per_pixel),
        int(raw.number_of_bands),
        float(raw.horizontal_fov_rad),
        float(raw.vertical_fov_rad),
        int(raw.frame_id),
        int(raw.subframe_id),
        int(raw.subframe_total),
        image_type,
        image_flip,
        int(raw.image_flags),
        float(raw.dither_row),
        float(raw.dither_column),
        int(raw.row_offset),
        int(raw.column_offset),
        int(raw.band_index),
        pixels,
    )


__all__ = [
    "AbiVersion",
    "ComponentLocation",
    "ErrorKind",
    "Frame",
    "ImageConfig",
    "ImageFlip",
    "ImageStream",
    "ImageType",
    "MelError",
    "ProviderVersion",
    "Session",
    "StreamCounters",
    "UciId",
    "abi_version",
]
