"""Private ffi implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
import sys as _sys
from typing import Any, Callable, TypeVar

from . import _native
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _local_error,
)
from ._validation import (
    _utf8_input,
)
from ._types import (
    BandInfo,
    BandType,
    ChannelCapability,
    ChannelType,
    ComponentLocation,
    ControlConfig,
    CoordinateSystem,
    HealthConfig,
    ImageBand,
    ImageConfig,
    InstrumentationConfig,
    MetadataCapability,
    PixelFormat,
    SensorType,
    UciId,
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


class _RawHealthConfig(_RawImageConfig):
    """C field order and borrowed storage are kept through the native Open call."""

    def __init__(self, config: HealthConfig) -> None:
        self.buffers = []
        self.raw = _native.IrHealthConfigV1(
            channel_id=self._id(config.channel_id),
            channel_type=_native.AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS,
            platform_id=self._id(config.platform_id),
            sensor_location=_native.ComponentLocationV1(
                config.sensor_location.offset_x_m,
                config.sensor_location.offset_y_m,
                config.sensor_location.offset_z_m,
                self._view(config.sensor_location.key, "key"),
                self._view(config.sensor_location.system_name, "system_name"),
            ),
        )


class _RawInstrumentationConfig(_RawImageConfig):
    def __init__(self, config: InstrumentationConfig) -> None:
        self.buffers = []
        self.raw = _native.IrInstrumentationConfigV1(
            channel_id=self._id(config.channel_id),
            channel_type=_native.AMS_MEL_IR_CHANNEL_INSTRUMENTATION,
            platform_id=self._id(config.platform_id),
            sensor_location=_native.ComponentLocationV1(
                config.sensor_location.offset_x_m,
                config.sensor_location.offset_y_m,
                config.sensor_location.offset_z_m,
                self._view(config.sensor_location.key, "key"),
                self._view(config.sensor_location.system_name, "system_name"),
            ),
        )


def _best_effort_close(close_function: Any, owner: _ctypes.c_void_p) -> None:
    """Close a just-published native owner on a failure path; never raises."""

    try:
        if owner.value:
            close_function(_ctypes.byref(owner), None, 0, None)
    except Exception:
        pass


def _channel_capabilities(owner: Any, query: Any) -> ChannelCapability:
    snapshot = _CapabilitySnapshot()
    try:
        _call_with_diagnostic(
            lambda diagnostic, required: query(
                owner,
                _ctypes.byref(snapshot.owner),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        if not snapshot.owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "capability query succeeded without returning a snapshot",
            )
        record = _ctypes.POINTER(_native.IrChannelCapabilityV1)()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_channel_capability_view(
                snapshot.owner,
                _ctypes.byref(record),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        if not record:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "capability view succeeded without returning a record",
            )
        value = _capability_from_raw(record.contents)
        del record
        # Normal path: explicit Close only after the complete owned copy.
        snapshot.close()
    except BaseException:
        snapshot.abandon()
        raise
    return value


class _CapabilitySnapshot:
    """Private owner of one native ``ams_mel_ir_channel_capability`` snapshot.

    Every record borrowed from the snapshot is valid only while this owner is
    open, so the complete Python copy is made before :meth:`close`. Failure
    paths call :meth:`abandon`; ``__del__`` is a final non-raising fallback.
    """

    def __init__(self) -> None:
        self.owner = _native.IrChannelCapabilityHandle()

    def close(self) -> None:
        if not self.owner.value:
            return
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_channel_capability_close(
                _ctypes.byref(self.owner),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        if self.owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "capability close succeeded without clearing the snapshot",
            )

    def abandon(self) -> None:
        _best_effort_close(_native.ams_mel_ir_channel_capability_close, self.owner)

    def __del__(self) -> None:
        try:
            self.abandon()
        except BaseException:
            pass


_SpanElement = TypeVar("_SpanElement")


def _checked_span_address(
    data: Any, size: int, element_type: Any, field: str
) -> int | None:
    """Validate one native ``{data, size}`` span before any dereference.

    Returns ``None`` for a zero-length span (the pointer is not inspected).
    A non-empty span requires a non-NULL pointer aligned for ``element_type``
    and a total byte size representable in ``Py_ssize_t`` without overflowing
    the native address space; malformed metadata is ``PROTOCOL_INCONSISTENCY``.

    Limit: only pointer/size/alignment metadata is validated. This cannot prove
    that an arbitrary non-NULL, aligned address from a violated C ABI points to
    mapped memory; the native ABI contract remains responsible for that.
    """

    size = int(size)
    if size == 0:
        return None
    address = _ctypes.cast(data, _ctypes.c_void_p).value
    if not address:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {field} span has a NULL pointer with non-zero size",
        )
    if address % _ctypes.alignment(element_type) != 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {field} span pointer is misaligned",
        )
    if size > _sys.maxsize // _ctypes.sizeof(element_type):
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {field} span size is not representable",
        )
    if size * _ctypes.sizeof(element_type) > (
            (1 << (8 * _ctypes.sizeof(_ctypes.c_void_p))) - address):
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {field} span address overflows the native address space",
        )
    return address


def _checked_span(
    data: Any,
    size: int,
    element_type: Any,
    convert: Callable[[Any], _SpanElement],
    field: str,
) -> tuple[_SpanElement, ...]:
    """Copy a validated native span into an immutable tuple of owned values."""

    address = _checked_span_address(data, size, element_type, field)
    if address is None:
        return ()
    if int(size) > _sys.maxsize // _ctypes.sizeof(_ctypes.py_object):
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            f"native {field} tuple size is not representable",
        )
    try:
        elements = _ctypes.cast(address, _ctypes.POINTER(element_type))
        return tuple(convert(elements[index]) for index in range(int(size)))
    except (MemoryError, OverflowError, ValueError):
        raise _local_error(
            ErrorKind.INTERNAL_ERROR,
            f"unable to allocate owned storage for native {field} span",
        ) from None


def _copy_string_view(view: _native.StringViewV1, field: str) -> str:
    """Copy the exact bytes of a native string view and strictly decode them."""

    address = _checked_span_address(view.data, view.size, _ctypes.c_char, field)
    if address is None:
        return ""
    try:
        payload = _ctypes.string_at(address, int(view.size))
    except (MemoryError, OverflowError, ValueError):
        raise _local_error(
            ErrorKind.INTERNAL_ERROR, f"unable to allocate native {field} copy"
        ) from None
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


def _copy_uci_id(raw: _native.UciIdV1, field: str) -> UciId:
    # bytes(bytearray(...)) copies all 16 bytes; no ctypes array identity kept.
    return UciId(
        bytes(bytearray(raw.uuid)),
        _copy_string_view(raw.descriptive_label, f"{field} label"),
    )


def _copy_bool(value: int, field: str) -> bool:
    raw = int(value)
    if raw == 0:
        return False
    if raw == 1:
        return True
    raise _local_error(
        ErrorKind.PROTOCOL_INCONSISTENCY,
        f"native {field} is not a 0/1 boolean",
    )


def _copy_u32_enum_span(span: Any, enum_type: Any, field: str) -> tuple[Any, ...]:
    return _checked_span(
        span.data, span.size, _ctypes.c_uint32,
        lambda raw: enum_type(int(raw)), field,
    )


def _copy_band_info(raw: _native.IrBandInfoV1) -> BandInfo:
    return BandInfo(
        BandType(int(raw.kind)),
        float(raw.min_wavelength_m),
        float(raw.max_wavelength_m),
    )


def _copy_image_band(raw: _native.IrImageBandV1) -> ImageBand:
    return ImageBand(
        int(raw.band_index),
        _checked_span(
            raw.bands.data, raw.bands.size, _native.IrBandInfoV1,
            _copy_band_info, "image band info",
        ),
    )


def _capability_from_raw(raw: _native.IrChannelCapabilityV1) -> ChannelCapability:
    """Copy one borrowed native record into a complete Python-owned model.

    Must run while the native snapshot is open. Every value produced is an
    ``int``/``float``/``bool``/``str``/``bytes``, an IntEnum, a frozen
    dataclass, or a tuple of those; no ctypes object is retained.
    """

    location = raw.sensor_location
    return ChannelCapability(
        channel_id=_copy_uci_id(raw.channel_id, "channel_id"),
        height=int(raw.height),
        width=int(raw.width),
        bit_depth=int(raw.bit_depth),
        row_pitch=int(raw.row_pitch),
        buffer_size=int(raw.buffer_size),
        image_size=int(raw.image_size),
        number_of_bands=int(raw.number_of_bands),
        pixel_format=PixelFormat(int(raw.pixel_format)),
        sensor_types=_copy_u32_enum_span(raw.sensor_types, SensorType, "sensor_types"),
        platform_id=_copy_uci_id(raw.platform_id, "platform_id"),
        sensor_location=ComponentLocation(
            float(location.offset_x_m),
            float(location.offset_y_m),
            float(location.offset_z_m),
            _copy_string_view(location.key, "sensor_location key"),
            _copy_string_view(location.system_name, "sensor_location system_name"),
        ),
        channel_types=_copy_u32_enum_span(raw.channel_types, ChannelType, "channel_types"),
        task_schedule_depth=int(raw.task_schedule_depth),
        odc_available=_copy_bool(raw.odc_available, "odc_available"),
        nuc_available=_copy_bool(raw.nuc_available, "nuc_available"),
        metadata_capabilities=_copy_u32_enum_span(
            raw.metadata_capabilities, MetadataCapability, "metadata_capabilities"
        ),
        image_bands=_checked_span(
            raw.image_bands.data, raw.image_bands.size, _native.IrImageBandV1,
            _copy_image_band, "image_bands",
        ),
        nav_frames=_copy_u32_enum_span(raw.nav_frames, CoordinateSystem, "nav_frames"),
    )
