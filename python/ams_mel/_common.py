"""Private common implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
from typing import Any

from . import _native
from ._comms_metadata import CommsMetadataStream
from ._validation import _validate_size_t
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _local_error,
)
from ._types import (
    ChannelCapability,
    CommsTestRequest,
)
from ._ffi import (
    _best_effort_close,
    _channel_capabilities,
)
from ._requests import (
    CommsRequest,
    ReturnRequest,
)


class _NativeOwner:
    """Private owner mechanics; native close alone decides handle consumption."""

    @classmethod
    def _from_owner(cls, owner: Any) -> Any:
        result = cls.__new__(cls)
        result._owner = owner
        result._owner_pointer = _ctypes.pointer(owner)
        return result

    @property
    def is_open(self) -> bool:
        """Local handle ownership, not enablement or provider health."""
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{type(self).__name__} is closed")

    def close(self) -> None:
        """Release ownership; retain a retryable handle exactly as native returns it."""
        if not self.is_open:
            return
        _call_with_diagnostic(
            lambda diagnostic, required: self._close_function(
                self._owner_pointer, diagnostic, len(diagnostic), _ctypes.byref(required),
            )
        )
        if self.is_open:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native close succeeded without consuming the owner",
            )

    def __enter__(self) -> Any:
        """Return this existing open owner without starting or enabling provider work."""

        self._require_open()
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close on context exit, preserving an exception raised by the with body."""

        if exception_type is None:
            self.close()
        else:
            self.__del__()
        return False

    def __del__(self) -> None:
        try:
            if self.is_open:
                self._close_function(self._owner_pointer, None, 0, None)
        except BaseException:
            # Never free an uncertain native graph or replace an application error.
            pass


def _open_channel_view(convert: Any, source: _ctypes.c_void_p) -> ChannelView:
    """Run one native typed -> weak View conversion.

    ``source`` is the typed owner's private handle, borrowed only for the
    duration of the call; nothing here retains it or its Python owner.
    """

    owner = _native.IrChannelHandle()
    try:
        _call_with_diagnostic(
            lambda diagnostic, required: convert(
                source,
                _ctypes.byref(owner),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
    except BaseException:
        _best_effort_close(_native.ams_mel_ir_channel_close, owner)
        raise
    if not owner.value:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "channel view conversion succeeded without returning a view",
        )
    return ChannelView._from_owner(owner)


class ChannelView:
    """Weak common Channel view of one typed owner.

    Created by :meth:`ControlChannel.channel_view`, :meth:`ImageStream.channel_view`,
    :meth:`InstrumentationChannel.channel_view`, :meth:`HealthStatusChannel.channel_view`,
    or :meth:`StackedChannel.channel_view`.
    This object owns only the native
    ``ams_mel_ir_channel`` weak-view wrapper; that native view owns only weak
    family state. It never retains its typed Python source, the Session, the
    provider channel, or the provider library, so it can outlive all of them
    without delaying teardown. :attr:`is_open` reports only that the wrapper is
    still owned: after source teardown it stays ``True`` while channel operations
    fail with ``PROVIDER_FAILED``. Closing the weak wrapper remains independent.

    Operations and :meth:`close` on one View require external serialization;
    the GIL is not a thread-safety contract.
    """

    def __init__(self) -> None:
        raise TypeError(
            "ChannelView objects must be created by ControlChannel.channel_view "
            "or ImageStream.channel_view"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrChannelHandle) -> ChannelView:
        view = cls.__new__(cls)
        view._owner = owner
        view._owner_pointer = _ctypes.pointer(owner)
        view._close_function = _native.ams_mel_ir_channel_close
        return view

    @property
    def is_open(self) -> bool:
        """Whether this object still owns its native weak-view wrapper.

        This does not report whether the underlying family still exists.
        """

        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.PROVIDER_FAILED, "ChannelView is closed")

    def send_keepalive(self) -> ReturnRequest:
        """Submit the inherited KeepAlive; returns the existing ReturnRequest.

        Uses the Session's async admission. Once admitted, the request (not
        this View) owns the family graph; closing the View does not cancel it.
        """

        self._require_open()
        owner = _native.IrReturnRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_channel_send_keepalive(
                    self._owner,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            _best_effort_close(_native.ams_mel_ir_return_request_close, owner)
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "KeepAlive submission succeeded without returning a request owner",
            )
        return ReturnRequest._from_owner(owner)

    def submit_comms_test(self, request: CommsTestRequest) -> CommsRequest:
        """Submit the inherited CommsTest; all three IDs are sent exactly."""

        self._require_open()
        if not isinstance(request, CommsTestRequest):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "request must be CommsTestRequest"
            )
        # Re-validate: a frozen dataclass can still be mutated deliberately.
        request.__post_init__()
        raw = _native.IrChannelCommsTestRequestV1(
            command_id=request.command_id,
            channel_id=request.channel_id,
            request_id=request.request_id,
        )
        owner = _native.IrChannelCommsRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_channel_submit_comms_test(
                    self._owner,
                    _ctypes.byref(raw),
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            _best_effort_close(_native.ams_mel_ir_channel_comms_request_close, owner)
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "CommsTest submission succeeded without returning a request owner",
            )
        return CommsRequest._from_owner(owner)

    def open_comms_metadata(self, queue_capacity: int = 16) -> CommsMetadataStream:
        """Register inherited provider-to-consumer CommsTest metadata once.

        Health, Image and StackedImage views support this operation. C2 must
        use its dedicated metadata API. Registration does
        not enable the channel and failure can consume the native attempt.
        The receiver retains no Python parent and is not a request future.
        """
        self._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if not queue_capacity:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        owner = _native.IrChannelCommsMetadataHandle()
        registration = _ctypes.c_uint32(0xFFFFFFFF)
        owner_pointer = _ctypes.pointer(owner)
        registration_pointer = _ctypes.pointer(registration)
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_channel_comms_metadata_open(
                    self._owner, queue_capacity, owner_pointer, registration_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required)))
            if registration.value != _native.AMS_MEL_IR_RETURN_SUCCESS:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Comms metadata Open succeeded with inconsistent registration")
            if not owner.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Comms metadata Open succeeded without an owner")
            return CommsMetadataStream._from_owner(owner)
        except BaseException:
            # A single cleanup invocation; preserve the original failure even
            # if Close raises or leaves ownership uncertain. No finalizer owns
            # this output slot when construction has not completed.
            try:
                if owner.value:
                    _native.ams_mel_ir_channel_comms_metadata_close(owner_pointer, None, 0, None)
            except BaseException:
                pass
            raise

    def capabilities(self) -> ChannelCapability:
        """Return a complete Python-owned ChannelCapability snapshot.

        The native snapshot is copied in full and then closed before this
        returns; the result holds no native memory.
        """

        self._require_open()
        return _channel_capabilities(self._owner, _native.ams_mel_ir_channel_get_capabilities)

    def close(self) -> None:
        """Close only this weak wrapper; idempotent.

        Never closes, disables, or detaches the typed owner and never cancels
        pending requests.
        """

        if not self.is_open:
            return
        _call_with_diagnostic(
            lambda diagnostic, required: self._close_function(
                self._owner_pointer,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        if self._owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "ChannelView close succeeded without clearing the view",
            )

    def __enter__(self) -> ChannelView:
        """Return this existing open owner without starting or enabling provider work."""

        self._require_open()
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close on context exit, preserving an exception raised by the with body."""

        if exception_type is None:
            self.close()
        else:
            try:
                self.close()
            except BaseException:
                pass
        return False

    def __del__(self) -> None:
        # Best-effort wrapper close only; never touches a typed owner.
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass
