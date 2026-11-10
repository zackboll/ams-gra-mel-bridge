"""Opt-in CameraCommandResp reception over the existing native FIFO."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address, _copy_bool
from ._types import CommandReturn
from ._validation import _validate_size_t, _validate_timeout


@dataclass(frozen=True)
class CameraCommandResponse:
    """Owned response; size is verbatim, timestamp is signed ns without an epoch.

    False command_success is a normal response, not a native error or MEL
    rejection. A callback says nothing about request-future completion.
    """

    command_id: int
    size: int
    timestamp_ns: int
    command_success: bool


@dataclass(frozen=True)
class CameraResponseCounters:
    """Full uint64 callback counts, independent of Python Receive calls."""

    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int


class CameraResponseOpenError(MelError):
    """Registration failure with the independently published provider Return."""

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _response_from_raw(raw: _native.IrCameraCommandResponseV1) -> CameraCommandResponse:
    return CameraCommandResponse(
        int(raw.command_id), int(raw.size), int(raw.timestamp_ns),
        _copy_bool(raw.command_success, "CameraCommandResp command_success"))


def _open_camera_response(parent, queue_capacity: int) -> CameraResponseStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_camera_response_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "CameraCommandResp registration is one-shot")
        # Allocate before registration can synchronously deliver callbacks.
        stream = CameraResponseStream._from_owner(_native.IrCameraResponseMetadataHandle())

        def register(diagnostic, required):
            parent._camera_response_attempted = True
            return _native.ams_mel_ir_image_camera_response_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                ctypes.byref(registration), diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CameraCommandResp open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()
        if isinstance(error, MelError):
            # Never infer Return from a status or diagnostic, including exceptions.
            result = (CommandReturn(registration.value)
                      if registration.value in (int(CommandReturn.SUCCESS),
                                                int(CommandReturn.NOT_SUPPORTED),
                                                int(CommandReturn.FAIL)) else None)
            raise CameraResponseOpenError(error.kind, error.diagnostic,
                                          error.diagnostic_required, error.native_status,
                                          result) from error
        raise
    return stream


class CameraResponseStream(_NativeOwner):
    """Unique public handle; no Python Image/Session or provider pointer retained.

    Native bounded FIFO drops incoming events when full. Malformed callbacks
    count but do not fail the stream. Image Stop drains retained events before
    STREAM_STOPPED. Close deactivates public consumption, not registration,
    requests, Image or Session; it promises no immediate callback quiescence.
    Native callback state controls provider-library retention.

    Externally serialize operations and Close on this owner, and registration
    versus Image Start/Stop/Close. The GIL is not a native ownership lock.
    """

    _close_function = staticmethod(_native.ams_mel_ir_camera_response_close)

    def __init__(self) -> None:
        raise TypeError("use ImageStream.open_camera_response")

    def receive(self, timeout_ms: int = 0) -> CameraCommandResponse:
        """Receive exactly once, validate and copy, then release the native event."""
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrCameraResponseEventHandle()
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_camera_response_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_camera_response_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraCommandResp receive succeeded without an event")
            view = ctypes.POINTER(_native.IrCameraCommandResponseV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_camera_response_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrCameraCommandResponseV1,
                                  "CameraCommandResp event view")
            result = _response_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraCommandResp event close did not consume its owner")
        except BaseException:
            if event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_camera_response_event_close(
                        ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            raise
        return result

    def counters(self) -> CameraResponseCounters:
        """Snapshot native callback counts without consuming or resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_camera_response_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return CameraResponseCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                      int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Idempotent after release; failed/uncertain Close is never retried."""
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "CameraCommandResp owner has uncertain closure")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CameraCommandResp close did not consume its owner")
        except BaseException as error:
            if getattr(self, "_close_attempted", False):
                self._close_error = error
            raise

    def _require_open(self) -> None:
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "CameraCommandResp owner has uncertain closure")

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass
