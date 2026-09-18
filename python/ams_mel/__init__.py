"""Safe Python sessions, IR Mono8 streams, and IR C2 over ``ams_mel_c``.

IR C2 provides asynchronous :class:`ModeRequest` and :class:`ReturnRequest`
owners. BIT support is limited to the empty/no-op profile; payload-bearing BIT
is not exposed. Request timeout and close do not cancel provider work.

Operations using one owner must follow the native external-serialization
contract. At most one receive operation may consume an :class:`ImageStream` at
a time. The GIL is not a substitute for either contract; this API does not
claim that owners are thread-safe. Wait must not race the corresponding request
close, and control enable, either submission operation, and close require
external serialization.
"""

from __future__ import annotations

import ctypes as _ctypes
from dataclasses import dataclass
from enum import Enum, IntEnum
import os
from typing import Callable

from . import _native


_DIAGNOSTIC_CAPACITY = 4096
_WAIT_DIAGNOSTIC_CAPACITY = 512
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


@dataclass(frozen=True)
class ControlConfig:
    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation

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

    @classmethod
    def new(
        cls,
        channel_id: UciId,
        platform_id: UciId,
        sensor_location: ComponentLocation,
    ) -> ControlConfig:
        return cls(channel_id, platform_id, sensor_location)


class MfaMode(IntEnum):
    UNUSED = _native.AMS_MEL_IR_MFA_MODE_UNUSED
    TASK_SCHED = _native.AMS_MEL_IR_MFA_MODE_TASK_SCHED
    SCAN_VOLUME_SCHED = _native.AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED
    SCAN_BAR_SCHED = _native.AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED


class CommandReturn(IntEnum):
    SUCCESS = _native.AMS_MEL_IR_RETURN_SUCCESS
    BAD_POINTER = _native.AMS_MEL_IR_RETURN_BAD_POINTER
    FAIL = _native.AMS_MEL_IR_RETURN_FAIL
    NOT_SUPPORTED = _native.AMS_MEL_IR_RETURN_NOT_SUPPORTED
    NOT_IMPLEMENTED = _native.AMS_MEL_IR_RETURN_NOT_IMPLEMENTED


class MelErrorCode(IntEnum):
    NONE = _native.AMS_MEL_ERROR_NONE
    INVALID_ID = _native.AMS_MEL_ERROR_INVALID_ID
    INVALID_STATE = _native.AMS_MEL_ERROR_INVALID_STATE
    INVALID_PARAMETERS = _native.AMS_MEL_ERROR_INVALID_PARAMETERS
    INSUFFICIENT_PERMISSIONS = _native.AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS
    INSUFFICIENT_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_RESOURCES
    INSUFFICIENT_LOCAL_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES
    INSUFFICIENT_REMOTE_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES
    UNSUPPORTED = _native.AMS_MEL_ERROR_UNSUPPORTED

    @classmethod
    def _missing_(cls, value: object) -> MelErrorCode | None:
        if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= _UINT32_MAX:
            unknown = int.__new__(cls, value)
            unknown._name_ = f"UNKNOWN_0x{value:08X}"
            unknown._value_ = value
            return unknown
        return None


@dataclass(frozen=True)
class ModeSuccess:
    mode: MfaMode


@dataclass(frozen=True)
class ModeRejected:
    code: MelErrorCode
    description: str


@dataclass(frozen=True)
class ReturnCompleted:
    value: CommandReturn


@dataclass(frozen=True)
class ReturnRejected:
    code: MelErrorCode
    description: str


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


def _validate_uint32(value: int, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an integer")
    if value < 0 or value > _UINT32_MAX:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, f"{name} must be representable in uint32_t"
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


class _RawControlConfig(_RawImageConfig):
    def __init__(self, config: ControlConfig) -> None:
        self.buffers = []
        self.raw = _native.IrC2ConfigV1(
            channel_type=_native.AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
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

    def open_control_channel(self, config: ControlConfig) -> ControlChannel:
        """Open an IR CommandAndControl channel without retaining this object."""

        self._require_open()
        if not isinstance(config, ControlConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be ControlConfig")
        bundle = _RawControlConfig(config)
        owner = _native.IrC2Handle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_open(
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
                _native.ams_mel_ir_c2_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "control channel open succeeded without returning an owner",
            )
        return ControlChannel._from_owner(owner)

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


class ControlChannel:
    """Unique C2 owner; enable, submit, and close require serialization."""

    def __init__(self) -> None:
        raise TypeError(
            "ControlChannel objects must be created by Session.open_control_channel"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrC2Handle) -> ControlChannel:
        channel = cls.__new__(cls)
        channel._owner = owner
        channel._owner_pointer = _ctypes.pointer(owner)
        channel._close_function = _native.ams_mel_ir_c2_close
        return channel

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ControlChannel is closed")

    def enable(self) -> None:
        """Explicitly enable this control channel."""

        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_enable(
                self._owner,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def submit_operate(self, command_id: int) -> ModeRequest:
        """Submit exactly Operate/TaskSched and return its asynchronous request."""

        self._require_open()
        _validate_uint32(command_id, "command_id")
        owner = _native.IrModeRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_submit_operate(
                    self._owner,
                    command_id,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_mode_request_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "control submission succeeded without returning a request owner",
            )
        return ModeRequest._from_owner(owner)

    def submit_bit_noop(self, command_id: int) -> ReturnRequest:
        """Submit BIT with empty payload lists and return its asynchronous request."""

        self._require_open()
        _validate_uint32(command_id, "command_id")
        owner = _native.IrReturnRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_submit_bit_noop(
                    self._owner,
                    command_id,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_return_request_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "BIT submission succeeded without returning a request owner",
            )
        return ReturnRequest._from_owner(owner)

    def close(self) -> None:
        """Close this owner; retryable detach failure can leave it open."""

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

    def __enter__(self) -> ControlChannel:
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


class ModeRequest:
    """Unique asynchronous mode request; close does not cancel provider work."""

    def __init__(self) -> None:
        raise TypeError("ModeRequest objects must be created by ControlChannel.submit_operate")

    @classmethod
    def _from_owner(cls, owner: _native.IrModeRequestHandle) -> ModeRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_mode_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ModeRequest is closed")

    def wait(self, timeout_ms: int) -> ModeSuccess | ModeRejected:
        """Wait finitely for a cached result. Zero polls; timeout does not cancel."""

        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_mode(self._owner, timeout_ms)

    def close(self) -> None:
        """Release this public owner without cancelling pending provider work."""

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

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


class ReturnRequest:
    """Unique asynchronous Return request; close does not cancel provider work."""

    def __init__(self) -> None:
        raise TypeError(
            "ReturnRequest objects must be created by ControlChannel.submit_bit_noop"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrReturnRequestHandle) -> ReturnRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_return_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ReturnRequest is closed")

    def wait(self, timeout_ms: int) -> ReturnCompleted | ReturnRejected:
        """Wait finitely for a Return; zero polls and timeout does not cancel."""

        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_return(self._owner, timeout_ms)

    def close(self) -> None:
        """Release this public owner without cancelling pending provider work."""

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

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


def _wait_for_mode(
    owner: _native.IrModeRequestHandle, timeout_ms: int
) -> ModeSuccess | ModeRejected:
    result = _native.IrModeResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _WAIT_DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(
        _native.ams_mel_ir_mode_request_wait(
            owner,
            timeout_ms,
            _ctypes.byref(result),
            diagnostic,
            len(diagnostic),
            _ctypes.byref(required),
        )
    )
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native wait returned an invalid diagnostic size",
        )
    if status == _native.AMS_MEL_TIMEOUT:
        message = None
        if required_value <= len(diagnostic):
            message = _decode_complete_buffer(
                bytes(diagnostic.raw[:required_value]), "diagnostic"
            ) or None
        raise _error_from_status(status, message, required_value)

    first_mode = int(result.mode)
    first_error_code = int(result.error_code)
    if required_value <= len(diagnostic):
        message = _decode_complete_buffer(
            bytes(diagnostic.raw[:required_value]), "diagnostic"
        )
    else:
        try:
            complete = _ctypes.create_string_buffer(required_value)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate complete wait diagnostic storage",
            ) from None
        retry_result = _native.IrModeResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(
            _native.ams_mel_ir_mode_request_wait(
                owner,
                0,
                _ctypes.byref(retry_result),
                complete,
                len(complete),
                _ctypes.byref(retry_required),
            )
        )
        if (
            retry_status != status
            or int(retry_result.mode) != first_mode
            or int(retry_result.error_code) != first_error_code
            or int(retry_required.value) != required_value
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native terminal wait changed during diagnostic retry",
            )
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")

    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native wait returned a diagnostic",
            )
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native wait returned an error code",
            )
        try:
            return ModeSuccess(MfaMode(int(result.mode)))
        except ValueError as error:
            raise _local_error(
                ErrorKind.PROVIDER_FAILED,
                "native provider returned an unknown MFA mode",
            ) from error
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return ModeRejected(MelErrorCode(int(result.error_code)), message)
    raise _error_from_status(status, message or None, required_value)


def _wait_for_return(
    owner: _native.IrReturnRequestHandle, timeout_ms: int
) -> ReturnCompleted | ReturnRejected:
    result = _native.IrReturnResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _WAIT_DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(
        _native.ams_mel_ir_return_request_wait(
            owner,
            timeout_ms,
            _ctypes.byref(result),
            diagnostic,
            len(diagnostic),
            _ctypes.byref(required),
        )
    )
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native Return wait returned an invalid diagnostic size",
        )
    if status == _native.AMS_MEL_TIMEOUT:
        message = None
        if required_value <= len(diagnostic):
            message = _decode_complete_buffer(
                bytes(diagnostic.raw[:required_value]), "diagnostic"
            ) or None
        raise _error_from_status(status, message, required_value)

    first_value = int(result.value)
    first_error_code = int(result.error_code)
    if required_value <= len(diagnostic):
        message = _decode_complete_buffer(
            bytes(diagnostic.raw[:required_value]), "diagnostic"
        )
    else:
        try:
            complete = _ctypes.create_string_buffer(required_value)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate complete Return wait diagnostic storage",
            ) from None
        retry_result = _native.IrReturnResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(
            _native.ams_mel_ir_return_request_wait(
                owner,
                0,
                _ctypes.byref(retry_result),
                complete,
                len(complete),
                _ctypes.byref(retry_required),
            )
        )
        if (
            retry_status != status
            or int(retry_result.value) != first_value
            or int(retry_result.error_code) != first_error_code
            or int(retry_required.value) != required_value
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native terminal Return wait changed during diagnostic retry",
            )
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")

    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native Return wait returned a diagnostic",
            )
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native Return wait returned an error code",
            )
        try:
            return ReturnCompleted(CommandReturn(int(result.value)))
        except ValueError as error:
            raise _local_error(
                ErrorKind.PROVIDER_FAILED,
                f"native provider returned unknown command Return value {int(result.value)}",
            ) from error
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return ReturnRejected(MelErrorCode(int(result.error_code)), message)
    raise _error_from_status(status, message or None, required_value)


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
    "CommandReturn",
    "ControlChannel",
    "ControlConfig",
    "ErrorKind",
    "Frame",
    "ImageConfig",
    "ImageFlip",
    "ImageStream",
    "ImageType",
    "MelError",
    "MelErrorCode",
    "MfaMode",
    "ModeRejected",
    "ModeRequest",
    "ModeSuccess",
    "ProviderVersion",
    "ReturnCompleted",
    "ReturnRejected",
    "ReturnRequest",
    "Session",
    "StreamCounters",
    "UciId",
    "abi_version",
]
