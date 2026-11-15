"""Optional Image OpticalDistortionMap reception over the existing C facade."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span, _checked_span_address
from ._image_types import ImageMetadataCounters
from ._types import CommandReturn
from ._validation import _validate_size_t, _validate_timeout


@dataclass(frozen=True)
class ForwardRightDown:
    """Owned components, without normalization or inferred units."""

    forward: float
    right: float
    down: float


@dataclass(frozen=True)
class RowCol:
    """Owned row/column components, copied without transformation."""

    row: float
    column: float


@dataclass(frozen=True)
class OpticalDistortionMap:
    """Complete owned snapshot; dimensions do not constrain collection lengths.

    IEEE values and signed zero pass unchanged. NaN payload bits are not promised.
    Both collections preserve provider order and duplicates independently.
    """

    width: int
    height: int
    right_size: int
    down_size: int
    pixels_per_odm: int
    right_min: float
    right_max: float
    down_min: float
    down_max: float
    frd: tuple[ForwardRightDown, ...]
    row_col: tuple[RowCol, ...]


class OpticalDistortionMapOpenError(MelError):
    """Open failure with a separately published, known provider Return, if any."""

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _copy_frd(raw: _native.IrForwardRightDownV1) -> ForwardRightDown:
    return ForwardRightDown(float(raw.forward), float(raw.right), float(raw.down))


def _copy_row_col(raw: _native.IrRowColV1) -> RowCol:
    return RowCol(float(raw.row), float(raw.column))


def _map_from_raw(raw: _native.IrOpticalDistortionMapV1) -> OpticalDistortionMap:
    # Validate BOTH spans before reading even the first element. No relationship
    # between their lengths, dimensions or pixels_per_odm is part of the contract.
    if raw.frd.size > 0x7FFFFFFF or raw.row_col.size > 0x7FFFFFFF:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "ODM collection exceeds native count limit")
    _checked_span_address(raw.frd.data, raw.frd.size, _native.IrForwardRightDownV1, "ODM frd")
    _checked_span_address(raw.row_col.data, raw.row_col.size, _native.IrRowColV1, "ODM row_col")
    try:
        return OpticalDistortionMap(
            int(raw.width), int(raw.height), int(raw.right_size), int(raw.down_size),
            int(raw.pixels_per_odm), float(raw.right_min), float(raw.right_max),
            float(raw.down_min), float(raw.down_max),
            _checked_span(raw.frd.data, raw.frd.size, _native.IrForwardRightDownV1,
                          _copy_frd, "ODM frd"),
            _checked_span(raw.row_col.data, raw.row_col.size, _native.IrRowColV1,
                          _copy_row_col, "ODM row_col"))
    except (MemoryError, OverflowError) as error:
        raise _local_error(ErrorKind.INTERNAL_ERROR, "unable to allocate owned ODM snapshot") from error


def _open_optical_distortion_map(parent, queue_capacity: int) -> OpticalDistortionMapStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)  # Not a published MEL Return.
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_odm_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "OpticalDistortionMap registration is one-shot")
        # Own the output slot before registration can synchronously deliver.
        stream = OpticalDistortionMapStream._from_owner(_native.IrImageOdmMetadataHandle())

        def register(diagnostic, required):
            parent._odm_attempted = True
            return _native.ams_mel_ir_image_odm_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                ctypes.byref(registration), diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "ODM open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()  # One attempt even for unexpected failed-call output.
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise OpticalDistortionMapOpenError(
                error.kind, error.diagnostic, error.diagnostic_required,
                error.native_status, result) from error
        raise
    return stream


class OpticalDistortionMapStream(_NativeOwner):
    """Unique consumer of a native bounded FIFO with DROP-INCOMING overflow.

    Externally serialize receive/counters/close, and registration versus parent
    Start/Stop/Close; the GIL is not sufficient. No Python parent is retained.
    Close only ends consumption: no unregister, acquisition stop or immediate
    callback quiescence is promised. The C facade retains provider resources.
    """

    _close_function = staticmethod(_native.ams_mel_ir_image_odm_close)

    def __init__(self) -> None:
        raise TypeError("use ImageStream.open_optical_distortion_map")

    def _require_open(self) -> None:
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ODM owner has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> OpticalDistortionMap:
        """Receive once, copy completely, release the event, then publish.

        Zero is a poll. A timeout never cancels registration or provider work.
        Queued maps drain before native stopped/failed state is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = None
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_image_odm_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            event = _native.IrImageOdmEventHandle()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_odm_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "ODM receive succeeded without an event")
            view = ctypes.POINTER(_native.IrOpticalDistortionMapV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_odm_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrOpticalDistortionMapV1, "ODM event view")
            result = _map_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "ODM event close did not consume its owner")
        except BaseException as error:
            # Preserve the primary error. Close may consume ownership even when
            # it fails or its diagnostic is malformed: never repeat an attempt.
            if event is not None and event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_image_odm_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            if isinstance(error, (MemoryError, OverflowError)):
                raise _local_error(ErrorKind.INTERNAL_ERROR,
                                   "unable to allocate owned ODM reception storage") from error
            raise
        return result

    def counters(self) -> ImageMetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_image_odm_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return ImageMetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                     int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Close only this consumer; never retry uncertain ownership consumption."""
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ODM owner has uncertain closure")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "ODM close did not consume its owner")
        except BaseException as error:
            if getattr(self, "_close_attempted", False):
                self._close_error = error
            raise

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass