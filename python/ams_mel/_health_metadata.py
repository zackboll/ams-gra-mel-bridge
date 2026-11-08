"""Independent native metadata owner; no Python callbacks or background work."""

from __future__ import annotations

import ctypes

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._health_convert import _event_from_raw
from ._health_types import HealthMetadataCounters, HealthMetadataEvent
from ._validation import _validate_timeout


class HealthMetadataStream(_NativeOwner):
    """Independent public owner of the native six-family DROP-INCOMING queue.

    Externally serialize receive, counters and close on this owner. Closing
    ends consumption, not the Health channel or callback registration, and
    promises no immediate callback quiescence. No Python parent is retained.
    """

    _close_function = staticmethod(_native.ams_mel_ir_health_metadata_close)

    def __init__(self) -> None:
        raise TypeError("use HealthStatusChannel.open_metadata")

    def receive(self, timeout_ms: int = 0) -> HealthMetadataEvent:
        """Poll at zero; otherwise wait in native code. Deep-copy before release.

        Queued events drain before STREAM_STOPPED or PROVIDER_FAILED; an active
        empty queue reports TIMEOUT. No partially converted event is returned.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrHealthMetadataEventHandle()
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_health_metadata_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_health_metadata_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Health metadata receive succeeded without an event")
            view = ctypes.POINTER(_native.IrHealthMetadataEventV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_health_metadata_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrHealthMetadataEventV1, "Health event view")
            result = _event_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "native Health event close did not consume its owner")
        except BaseException:
            # Exactly one attempt, even if cleanup raises or retains a handle.
            # If diagnostic allocation failed before entering close_event,
            # cleanup is still needed; once native Close begins, never retry.
            if event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_health_metadata_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            raise
        return result

    def counters(self) -> HealthMetadataCounters:
        """Full uint64 callback counters; receiving does not reset them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_health_metadata_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return HealthMetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                      int(raw.malformed_or_unsupported))

    def __del__(self) -> None:
        # A finalizer is one best-effort close, never a retry of uncertain cleanup.
        try:
            if getattr(self, "_finalized", False):
                return
            self._finalized = True
            super().__del__()
        except BaseException:
            pass
