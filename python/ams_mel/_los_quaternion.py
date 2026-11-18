"""Optional Image LineOfSightQuaternion reception over the existing C facade."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._image_types import ImageMetadataCounters
from ._optical_distortion_map import ForwardRightDown
from ._types import CommandReturn
from ._validation import _validate_size_t, _validate_timeout


@dataclass(frozen=True)
class Quaternion:
    """Owned navigation-frame quaternion in XYZW order, without normalization."""

    x: float
    y: float
    z: float
    w: float


@dataclass(frozen=True)
class LineOfSightQuaternion:
    """Owned snapshot, without physical validation or inferred timestamp epoch.

    Rates are image-frame forward/right/down radians per second. IEEE values,
    including signed zero and NaN classification, pass through unchanged;
    NaN payload-bit fidelity is not promised beyond the native contract.
    """

    system_time_ns: int
    q_xyzw: Quaternion
    pqr_rps: ForwardRightDown


class LineOfSightQuaternionOpenError(MelError):
    """Open failure with a separately published, known provider Return, if any."""

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _quaternion_from_raw(raw: _native.IrLineOfSightQuaternionV1) -> LineOfSightQuaternion:
    # The caller validates the complete fixed-record address before any read.
    return LineOfSightQuaternion(
        int(raw.system_time_ns),
        Quaternion(float(raw.q_xyzw.x), float(raw.q_xyzw.y),
                   float(raw.q_xyzw.z), float(raw.q_xyzw.w)),
        ForwardRightDown(float(raw.pqr_rps.forward), float(raw.pqr_rps.right),
                         float(raw.pqr_rps.down)))


def _open_line_of_sight_quaternion(parent, queue_capacity: int) -> LineOfSightQuaternionStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)  # Not a published MEL Return.
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_los_quaternion_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "LineOfSightQuaternion registration is one-shot")
        # Own the output slot before registration can synchronously deliver.
        stream = LineOfSightQuaternionStream._from_owner(_native.IrImageLosQuaternionMetadataHandle())

        def register(diagnostic, required):
            parent._los_quaternion_attempted = True
            return _native.ams_mel_ir_image_los_quaternion_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                ctypes.byref(registration), diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "LOS quaternion open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()  # One attempt even for unexpected failed-call output.
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise LineOfSightQuaternionOpenError(
                error.kind, error.diagnostic, error.diagnostic_required,
                error.native_status, result) from error
        raise
    return stream


class LineOfSightQuaternionStream(_NativeOwner):
    """Unique consumer of a native bounded FIFO with DROP-INCOMING overflow.

    Externally serialize receive/counters/close, and registration versus parent
    Start/Stop/Close; the GIL is not sufficient. No Python parent is retained.
    Close only ends consumption: no unregister, acquisition stop or immediate
    callback quiescence is promised. The C facade retains provider resources.
    """

    _close_function = staticmethod(_native.ams_mel_ir_image_los_quaternion_close)

    def __init__(self) -> None:
        raise TypeError("use ImageStream.open_line_of_sight_quaternion")

    def __copy__(self):
        raise TypeError("LineOfSightQuaternionStream is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("LineOfSightQuaternionStream is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("LineOfSightQuaternionStream cannot be pickled")

    def _require_open(self) -> None:
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "LOS quaternion owner has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> LineOfSightQuaternion:
        """Receive once, copy completely, release the event, then publish.

        Zero is a poll. A timeout never cancels registration or provider work.
        Queued events drain before native stopped/failed state is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = None
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_image_los_quaternion_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            event = _native.IrImageLosQuaternionEventHandle()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_los_quaternion_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "LOS quaternion receive succeeded without an event")
            view = ctypes.POINTER(_native.IrLineOfSightQuaternionV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_los_quaternion_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrLineOfSightQuaternionV1, "LOS quaternion event view")
            result = _quaternion_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "LOS quaternion event close did not consume its owner")
        except BaseException as error:
            # Preserve the primary error. Close may consume ownership even when
            # it fails or its diagnostic is malformed: never repeat an attempt.
            if event is not None and event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_image_los_quaternion_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            if isinstance(error, (MemoryError, OverflowError)):
                raise _local_error(ErrorKind.INTERNAL_ERROR,
                                   "unable to allocate owned LOS quaternion reception storage") from error
            raise
        return result

    def counters(self) -> ImageMetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_image_los_quaternion_get_counters(
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
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "LOS quaternion owner has uncertain closure")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "LOS quaternion close did not consume its owner")
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
