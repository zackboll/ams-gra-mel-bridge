"""Opt-in C2 CameraProtectCmdResp reception over the existing C facade."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._c2_types import C2MetadataCounters
from ._types import CommandReturn
from ._validation import _validate_size_t, _validate_timeout


class CameraProtectStatus(IntEnum):
    """Provider response data, distinct from native status and registration Return."""

    OK = 0
    UNAVAILABLE = 1
    UNSUPPORTED = 2


@dataclass(frozen=True)
class CameraProtectResponse:
    """Owned uint32 ID, signed int64 nanoseconds (no epoch), and response status.

    UNAVAILABLE and UNSUPPORTED are valid data, not MEL command rejections.
    No deadline, command correlation or asynchronous completion is inferred.
    """

    command_id: int
    response_time_ns: int
    command_response: CameraProtectStatus


class CameraProtectResponseOpenError(MelError):
    """Open failure with a separately published, known provider Return, if any."""

    registration: CommandReturn | None

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _response_from_raw(raw: _native.IrCameraProtectResponseV1) -> CameraProtectResponse:
    # The caller validates the complete fixed-record address before any read.
    try:
        response = CameraProtectStatus(raw.command_response)
    except ValueError as error:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "unknown CameraProtectCmdResp command_response") from error
    return CameraProtectResponse(int(raw.command_id), int(raw.response_time_ns), response)


def _open_camera_protect_response(parent, queue_capacity: int) -> CameraProtectResponseStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)  # Not a published MEL Return.
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_camera_protect_response_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CameraProtectResponse registration is one-shot")
        # Own the output slot before registration can synchronously deliver.
        stream = CameraProtectResponseStream._from_owner(_native.IrCameraProtectMetadataHandle())

        def register(diagnostic, required):
            parent._camera_protect_response_attempted = True
            return _native.ams_mel_ir_c2_camera_protect_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                ctypes.byref(registration), diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CameraProtectCmdResp open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()  # One attempt even for unexpected failed-call output.
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise CameraProtectResponseOpenError(
                error.kind, error.diagnostic, error.diagnostic_required,
                error.native_status, result) from error
        raise
    return stream


class CameraProtectResponseStream(_NativeOwner):
    """Unique consumer of a native bounded FIFO with DROP-INCOMING overflow.

    Externally serialize receive/counters/close, and registration versus parent
    C2 operations; the GIL is not sufficient. No Python parent is retained.
    Close only ends consumption: no unregister or immediate callback quiescence
    is promised. C2 teardown and provider retention are native responsibilities.
    """

    _close_function = staticmethod(_native.ams_mel_ir_camera_protect_close)

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.open_camera_protect_response")

    def __copy__(self):
        raise TypeError("CameraProtectResponseStream is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CameraProtectResponseStream is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CameraProtectResponseStream cannot be pickled")

    def _require_open(self) -> None:
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CameraProtectCmdResp owner has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> CameraProtectResponse:
        """Receive once, copy completely, release the event, then publish.

        Zero is a poll; positive timeouts are finite uint32 milliseconds.
        A timeout never cancels registration or provider work.
        Queued events drain before native stopped/failed state is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = None
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_camera_protect_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            event = _native.IrCameraProtectEventHandle()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_camera_protect_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraProtectCmdResp receive succeeded without an event")
            view = ctypes.POINTER(_native.IrCameraProtectResponseV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_camera_protect_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrCameraProtectResponseV1, "CameraProtectCmdResp event view")
            result = _response_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraProtectCmdResp event close did not consume its owner")
        except BaseException as error:
            # Preserve the primary error. Close may consume ownership even when
            # it fails or its diagnostic is malformed: never repeat an attempt.
            if event is not None and event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_camera_protect_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            if isinstance(error, (MemoryError, OverflowError)):
                raise _local_error(ErrorKind.INTERNAL_ERROR,
                                   "unable to allocate owned CameraProtectCmdResp reception storage") from error
            raise
        return result

    def counters(self) -> C2MetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_camera_protect_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return C2MetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                  int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Close only this consumer; never retry uncertain ownership consumption."""
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CameraProtectCmdResp owner has uncertain closure")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraProtectCmdResp close did not consume its owner")
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
