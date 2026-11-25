"""Unique consumer for the native four-family Track metadata queue."""

from __future__ import annotations

import ctypes

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._track_convert import _event_from_raw
from ._track_types import TrackMetadataCounters, TrackMetadataEvent
from ._validation import _validate_size_t, _validate_timeout

def _open_metadata(parent, queue_capacity: int) -> TrackMetadataStream:
    parent._require_open()
    _validate_size_t(queue_capacity, "queue_capacity")
    if queue_capacity == 0:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
    if parent._metadata_attempted:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "Track metadata registration is one-shot")

    stream = TrackMetadataStream._from_owner(_native.IrTrackMetadataHandle())
    stream._close_function = _native.ams_mel_ir_track_metadata_close
    stream._close_attempted = False
    owner, output = parent._owner, stream._owner_pointer
    open_native = _native.ams_mel_ir_track_metadata_open

    def register(diagnostic, required):
        capacity, required_pointer = len(diagnostic), ctypes.byref(required)
        parent._metadata_attempted = True
        return open_native(owner, queue_capacity, output, diagnostic, capacity, required_pointer)

    try:
        _call_with_diagnostic(register)
        if not stream.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Track metadata open succeeded without an owner")
    except BaseException:
        stream._close_quietly()  # Clean up an unexpected output without masking the error.
        raise
    return stream


class TrackMetadataStream(_NativeOwner):
    """Unique consumer of the native bounded FIFO; retains no Python parent.

    Serialize receive, counters and close on this owner, and registration
    versus operations on the parent. The GIL is not sufficient. Close ends
    consumption, not Track or callback registration, and promises no callback
    quiescence. Queued events can drain after Track or Session closes.
    """

    def __init__(self) -> None:
        raise TypeError("use TrackChannel.open_metadata")

    def __reduce_ex__(self, protocol: int):
        raise TypeError("TrackMetadataStream has unique ownership and cannot be copied or pickled")

    @property
    def is_open(self) -> bool:
        """Whether this consumer owns a local handle, independent of provider health."""
        return not self._close_attempted and bool(self._owner.value)

    def receive(self, timeout_ms: int = 0) -> TrackMetadataEvent:
        """Receive once, copy the complete active payload, then release its native event.

        Zero polls; positive timeouts are finite uint32 milliseconds. Queued
        events drain before a native stopped or failed status is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrTrackMetadataEventHandle()
        output = ctypes.byref(event)
        close_native = _native.ams_mel_ir_track_metadata_event_close
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            capacity, required_pointer = len(diagnostic), ctypes.byref(required)
            close_attempted = True
            return close_native(output, diagnostic, capacity, required_pointer)

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_track_metadata_receive(
                    self._owner, timeout_ms, output, diagnostic, len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Track metadata receive succeeded without an event")
            view = ctypes.POINTER(_native.IrTrackMetadataEventV3)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_track_metadata_event_view_v3(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrTrackMetadataEventV3, "Track metadata v3 event view")
            result = _event_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Track metadata event close did not consume its owner")
        except BaseException:
            # Native may have consumed the event even if Close failed, raised,
            # or left stale output. Only an unattempted Close can be run here.
            if event.value and not close_attempted:
                try:
                    close_native(output, None, 0, None)
                except BaseException:
                    pass
            raise
        finally:
            event.value = None
        return result

    def counters(self) -> TrackMetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()  # Shared native counter layout.
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_track_metadata_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return TrackMetadataCounters(raw.events_received, raw.events_dropped_queue_full,
                                     raw.malformed_or_unsupported)

    def close(self) -> None:
        """Close only this consumer; never retry a possibly consumed native owner."""
        if not self.is_open:
            return
        close_native, output = self._close_function, self._owner_pointer

        def close_owner(diagnostic, required):
            capacity, required_pointer = len(diagnostic), ctypes.byref(required)
            self._close_attempted = True
            return close_native(output, diagnostic, capacity, required_pointer)

        try:
            _call_with_diagnostic(close_owner)
            if self._owner.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Track metadata close did not consume its owner")
        finally:
            if self._close_attempted:
                self._owner.value = None

    def _close_quietly(self) -> None:
        try:
            if self.is_open:
                close_native, output = self._close_function, self._owner_pointer
                self._close_attempted = True
                try:
                    close_native(output, None, 0, None)
                finally:
                    self._owner.value = None
        except BaseException:
            pass

    def __del__(self) -> None:
        self._close_quietly()
