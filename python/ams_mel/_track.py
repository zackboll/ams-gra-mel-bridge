"""Private Track ownership; no Track-specific callbacks or commands."""

from __future__ import annotations

import ctypes as _ctypes
from typing import Any

from . import _native
from ._common import ChannelView, _open_channel_view
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _channel_capabilities
from ._types import ChannelCapability


class TrackChannel:
    """Unique Attached/Enabled Track owner, independent of the public Session.

    Unlike the owner, common views are weak. Callers must serialize operations
    and close on the same Track owner; the GIL does not provide that guarantee.
    """

    def __init__(self) -> None:
        raise TypeError("use Session.open_track_channel")

    @classmethod
    def _prepare_open(cls, owner: _native.IrTrackHandle) -> TrackChannel:
        if type(owner) is not _native.IrTrackHandle or owner.value:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Track open requires a valid empty owner slot")
        pointer = _ctypes.pointer(owner)
        result = cls.__new__(cls)
        result._owner_pointer = pointer
        result._close_function = _native.ams_mel_ir_track_close
        result._owner = owner
        result._close_state = "opening"
        return result

    def _publish_open(self) -> None:
        if not self._owner.value:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Track open succeeded without returning an owner")
        self._close_state = "live"

    def _abandon_open(self) -> None:
        # Native Open owns rollback. An unexpected owner gets at most one
        # non-masking cleanup attempt, even if publication was interrupted.
        self._close_state = "uncertain"
        try:
            if self._owner.value:
                self._close_function(self._owner_pointer, None, 0, None)
        except BaseException:
            pass

    def __reduce_ex__(self, protocol: int) -> Any:
        raise TypeError("TrackChannel has unique ownership and cannot be copied or pickled")

    @property
    def is_open(self) -> bool:
        """Whether the owner is locally open, not whether Enable succeeded."""
        return self._close_state == "live" and bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "TrackChannel is logically closed")

    def enable(self) -> None:
        """Explicitly Enable; already Enabled is a no-op, and failure is terminal."""
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_track_enable(
                self._owner, diagnostic, len(diagnostic), _ctypes.byref(required)))

    def capabilities(self) -> ChannelCapability:
        """Copy complete capabilities while Attached or Enabled, without enabling."""
        self._require_open()
        return _channel_capabilities(self._owner, _native.ams_mel_ir_track_get_capabilities)

    def channel_view(self) -> ChannelView:
        """Return a weak common view that keeps neither this owner nor Session alive."""
        self._require_open()
        return _open_channel_view(_native.ams_mel_ir_channel_from_track, self._owner)

    def close(self) -> None:
        """Close the owner, or explicitly retry a documented retained detach failure.

        A consumed owner stays closed even when disable failed. Uncertain native
        ownership is disarmed and cannot be retried. Failures preparing the
        call, before invocation, leave ownership unchanged.
        """
        if self._close_state == "uncertain":
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Track ownership unresolved after Close")
        if self._close_state == "consumed":
            return
        original = self._owner.value
        status = None

        def close_owner(diagnostic, required):
            nonlocal status
            pointer = self._owner_pointer
            close = self._close_function
            capacity = len(diagnostic)
            required_pointer = _ctypes.byref(required)
            self._close_state = "uncertain"
            status = int(close(pointer, diagnostic, capacity, required_pointer))
            return status

        try:
            _call_with_diagnostic(close_owner)
        except MelError as error:
            if self._close_state == "uncertain":
                if not self._owner.value:
                    self._close_state = "consumed"
                elif (self._owner.value == original
                      and status == _native.AMS_MEL_PROVIDER_FAILED
                      and error.native_status == status
                      and error.kind is ErrorKind.PROVIDER_FAILED
                      and error.diagnostic == "Track detach failed; provider state retained"):
                    self._close_state = "retry"
            raise
        if self._owner.value:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Track Close succeeded without consuming the owner")
        self._close_state = "consumed"

    def __enter__(self) -> TrackChannel:
        """Return the open owner without enabling it."""
        self._require_open()
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close once on exit, preserving an exception from the context body."""
        if self.is_open:
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
            if self.is_open:
                self._close_state = "uncertain"
                self._close_function(self._owner_pointer, None, 0, None)
        except BaseException:
            pass
