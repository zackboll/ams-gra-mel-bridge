"""Private StackedImage ownership, not stacked-image acquisition."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._common import ChannelView, _open_channel_view
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _channel_capabilities
from ._types import ChannelCapability


class StackedChannel:
    """Unique Attached/Enabled owner; externally serialize all operations.

    Views are weak. Session closure does not invalidate this channel and Close
    never cancels admitted requests. No buffers or image listeners are created.
    After a native Close attempt only a documented retained detach failure may
    be explicitly retried. Automatic cleanup never retries, even that failure.
    Uncertain ownership deliberately retains resources rather than risking a
    double release or unloading code still used by provider objects.
    """

    def __init__(self) -> None:
        raise TypeError("use Session.open_stacked_channel")

    @classmethod
    def _from_owner(cls, owner: _native.IrStackedHandle) -> StackedChannel:
        # Complete potentially raising preparation before transferring ownership.
        pointer = _ctypes.pointer(owner)
        result = cls.__new__(cls)
        result._owner_pointer = pointer
        result._close_function = _native.ams_mel_ir_stacked_close
        result._close_state = "live"
        result._owner = owner
        return result

    @property
    def is_open(self) -> bool:
        """Local handle presence only, not enablement, health or usability."""
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open or self._close_state != "live":
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "StackedChannel is logically closed")

    def enable(self) -> None:
        """Explicit idempotent Enable; native refusal/exception fails the lifecycle."""
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stacked_enable(
                self._owner, diagnostic, len(diagnostic), _ctypes.byref(required)))

    def capabilities(self) -> ChannelCapability:
        """Complete immutable Python-owned snapshot, including zero stack layouts."""
        self._require_open()
        return _channel_capabilities(self._owner, _native.ams_mel_ir_stacked_get_capabilities)

    def channel_view(self) -> ChannelView:
        """Existing common services; retains neither this Python owner nor Session."""
        self._require_open()
        return _open_channel_view(_native.ams_mel_ir_channel_from_stacked, self._owner)

    def close(self) -> None:
        """Close once, or explicitly retry only a proven retained detach failure.

        Disable failure may consume the owner despite reporting an error. An
        exception, malformed diagnostic or unexpected output disarms cleanup.
        Preparation failures before native invocation leave ownership unchanged.
        """
        if self._close_state == "uncertain":
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Stacked ownership unresolved after Close")
        if not self.is_open:
            return
        original = self._owner.value
        status = None

        def close_owner(diagnostic, required):
            nonlocal status
            # Prepare arguments before changing state: no native call means no
            # ownership uncertainty. Once entered, only the exact contract below
            # can restore explicit retry permission.
            pointer = self._owner_pointer
            capacity = len(diagnostic)
            required_pointer = _ctypes.byref(required)
            close_function = self._close_function
            self._close_state = "uncertain"
            status = int(close_function(pointer, diagnostic, capacity, required_pointer))
            return status

        try:
            _call_with_diagnostic(close_owner)
        except MelError as error:
            if self._close_state == "uncertain":
                if not self._owner.value:
                    self._close_state = "consumed"
                elif (self._owner.value == original
                      and status == _native.AMS_MEL_PROVIDER_FAILED
                      and error.native_status == _native.AMS_MEL_PROVIDER_FAILED
                      and error.kind is ErrorKind.PROVIDER_FAILED
                      and error.diagnostic == "Stacked detach failed; provider state retained"):
                    self._close_state = "retry"
            raise
        if self._owner.value:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Stacked Close succeeded without consuming the owner")
        self._close_state = "consumed"

    def __enter__(self) -> StackedChannel:
        """Return this existing open owner without starting or enabling provider work."""

        self._require_open()
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        # Context cleanup is automatic, not an explicit detach retry request.
        """Close on context exit, preserving an exception raised by the with body."""

        if self._close_state == "live":
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
            if self._close_state == "live" and self.is_open:
                self._close_state = "uncertain"
                self._close_function(self._owner_pointer, None, 0, None)
        except BaseException:
            pass
