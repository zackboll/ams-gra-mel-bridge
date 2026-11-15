"""Independent consumption owner for provider ChannelCommsTestRep callbacks."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._types import CommsTestReport
from ._validation import _validate_timeout


@dataclass(frozen=True)
class CommsMetadataCounters:
    """Complete saturating uint64 callback counters, not queue-length estimates."""

    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int


class CommsMetadataStream:
    """Bounded native FIFO, independent of Python parents and request futures.

    Externally serialize operations and Close on this owner, and registration
    with typed-channel operations. The GIL is insufficient synchronization.
    Close stops consumption; it neither unregisters callbacks nor disables or
    detaches a channel, cancels requests, or promises callback quiescence/unload.
    Queued copies can drain after typed closure before STOPPED or FAILED.
    """

    def __init__(self) -> None:
        raise TypeError("use ChannelView.open_comms_metadata")

    @classmethod
    def _from_owner(cls, owner: _native.IrChannelCommsMetadataHandle) -> CommsMetadataStream:
        # Finish potentially raising preparation before transferring ownership.
        pointer = ctypes.pointer(owner)
        result = cls.__new__(cls)
        result._owner_pointer = pointer
        result._close_attempted = False
        result._owner = owner
        return result

    @property
    def is_open(self) -> bool:
        """Local handle presence, not channel health or callback admission."""
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open or self._close_attempted:
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "Comms metadata owner is closed or has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> CommsTestReport:
        """Poll at zero; finite wait otherwise. Timeout never cancels work."""
        self._require_open()
        _validate_timeout(timeout_ms)
        raw = _native.IrChannelCommsTestReportV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_channel_comms_metadata_receive(
                self._owner, timeout_ms, ctypes.byref(raw), diagnostic,
                len(diagnostic), ctypes.byref(required)))
        return CommsTestReport(int(raw.command_id), int(raw.request_id))

    def counters(self) -> CommsMetadataCounters:
        """Copy all three uint64 counters without consuming or resetting events."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_channel_comms_metadata_get_counters(
                self._owner, ctypes.byref(raw), diagnostic,
                len(diagnostic), ctypes.byref(required)))
        return CommsMetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                     int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Idempotent on success; never retry an ownership-uncertain native Close."""
        if not self.is_open:
            return
        if self._close_attempted:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Comms metadata owner unresolved after an earlier close attempt")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return _native.ams_mel_ir_channel_comms_metadata_close(
                self._owner_pointer, diagnostic, len(diagnostic), ctypes.byref(required))

        _call_with_diagnostic(close_owner)
        if self.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native Comms metadata close did not consume its owner")

    def __enter__(self) -> CommsMetadataStream:
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
            if self.is_open and not self._close_attempted:
                self._close_attempted = True
                _native.ams_mel_ir_channel_comms_metadata_close(
                    self._owner_pointer, None, 0, None)
        except BaseException:
            pass
