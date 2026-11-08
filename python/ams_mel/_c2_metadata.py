"""Independent native C2 queue owner; no Python callbacks or background work."""

from __future__ import annotations

import ctypes

from . import _native
from ._c2_convert import _event_from_raw
from ._c2_types import C2MetadataCounters, C2MetadataEvent
from ._common import _NativeOwner
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._validation import _validate_timeout


class C2MetadataStream(_NativeOwner):
    """Independent bounded FIFO for three base families and opt-in CommsTest.

    Externally serialize operations and Close, and registration with parent C2
    operations. The GIL is not this contract. No Python parent is retained.
    Close neither cancels requests nor unregisters callbacks, and promises no
    immediate callback quiescence. Queued events can drain after C2 teardown.
    """

    _close_function = staticmethod(_native.ams_mel_ir_c2_metadata_close)

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.open_metadata")

    def _require_open(self) -> None:
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "C2 metadata owner has uncertain closure")

    def enable_comms_test_events(self) -> None:
        """Attempt separate one-shot registration, including failed attempts.

        Allowed while C2 is Attached or Enabled, without a capability gate.
        Failure does not undo the three base registrations.
        """
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_metadata_register_comms_test(
                self._owner, diagnostic, len(diagnostic), ctypes.byref(required)))

    def receive(self, timeout_ms: int = 0) -> C2MetadataEvent:
        """Receive once, deep-copy, then release the event before returning.

        Zero polls. Queued events drain before STREAM_STOPPED/PROVIDER_FAILED;
        active-empty reports TIMEOUT. Metadata is not a request Wait result.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrC2MetadataEventHandle()
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_c2_metadata_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_metadata_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "C2 metadata receive succeeded without an event")
            view = ctypes.POINTER(_native.IrC2MetadataEventV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_metadata_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrC2MetadataEventV1, "C2 event view")
            result = _event_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "native C2 event close did not consume its owner")
        except BaseException:
            # Cleanup must not mask conversion/diagnostic/BaseException failures.
            # Once native Close begins, never retry an uncertain retained owner.
            if event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_c2_metadata_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            raise
        return result

    def counters(self) -> C2MetadataCounters:
        """Full uint64 callback counters, unchanged by consumption."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_metadata_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return C2MetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                  int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Idempotent on success; an uncertain retained Close is never retried."""
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "C2 metadata owner unresolved after an earlier close attempt")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(
                self._owner_pointer, diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(close_owner)
        if self.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native C2 metadata close did not consume its owner")

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass
