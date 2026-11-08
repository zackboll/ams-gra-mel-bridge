"""Private image implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._image_metadata import ImageMetadataStream, _open_metadata
from ._nuc import NucStream, _open_nuc
from ._errors import (
    ErrorKind,
    MelError,
    _call_with_diagnostic,
    _local_error,
)
from ._validation import (
    _validate_timeout,
)
from ._types import (
    Frame,
    ImageFlip,
    ImageType,
    StreamCounters,
)
from ._common import (
    ChannelView,
    _open_channel_view,
)


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

    def open_nuc(self, queue_capacity: int = 16) -> NucStream:
        """Opt into optional one-shot NUC registration, without starting Image."""
        return _open_nuc(self, queue_capacity, _native.ams_mel_ir_image_nuc_open)

    def open_metadata(self, queue_capacity: int = 16) -> ImageMetadataStream:
        """One-shot ordinary metadata registration; prefer before Image Start."""
        return _open_metadata(self, queue_capacity)

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

    def channel_view(self) -> ChannelView:
        """Create a weak common :class:`ChannelView` of this stream.

        Valid before Start. The View retains neither this object nor the
        Session; it owns only a native weak-view wrapper. Multiple independent
        Views are allowed.
        """

        if not self.is_open:
            raise _local_error(ErrorKind.PROVIDER_FAILED, "image stream is closed")
        return _open_channel_view(_native.ams_mel_ir_channel_from_stream, self._owner)

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
