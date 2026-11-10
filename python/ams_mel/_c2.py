"""Private c2 implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._c2_metadata import C2MetadataStream
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _local_error,
)
from ._validation import (
    _validate_size_t,
    _validate_uint32,
)
from ._requests import (
    ModeRequest,
    ReturnRequest,
)
from ._common import (
    ChannelView,
    _open_channel_view,
)


class ControlChannel:
    """Unique C2 owner; enable, submit, and close require serialization."""

    def __init__(self) -> None:
        raise TypeError(
            "ControlChannel objects must be created by Session.open_control_channel"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrC2Handle) -> ControlChannel:
        channel = cls.__new__(cls)
        channel._owner = owner
        channel._owner_pointer = _ctypes.pointer(owner)
        channel._close_function = _native.ams_mel_ir_c2_close
        return channel

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ControlChannel is closed")

    def enable(self) -> None:
        """Explicitly enable this control channel."""

        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_enable(
                self._owner,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def open_metadata(self, queue_capacity: int = 16) -> C2MetadataStream:
        """Register BIT configuration, CommandStatus, BIT status once in order.

        Attached or Enabled; never implicitly enables C2 or registers CommsTest.
        Synchronous callbacks are allowed. Failed/partial attempts consume the
        native one-shot; closing metadata does not permit reopening.
        Externally serialize registration with other parent C2 operations.
        """
        self._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        metadata = C2MetadataStream._from_owner(_native.IrC2MetadataHandle())
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_metadata_open(
                    self._owner, queue_capacity, metadata._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required)))
            if not metadata.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "C2 metadata open succeeded without an owner")
        except BaseException:
            metadata.__del__()
            raise
        return metadata

    def submit_operate(self, command_id: int) -> ModeRequest:
        """Submit exactly Operate/TaskSched and return its asynchronous request."""

        self._require_open()
        _validate_uint32(command_id, "command_id")
        owner = _native.IrModeRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_submit_operate(
                    self._owner,
                    command_id,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_mode_request_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "control submission succeeded without returning a request owner",
            )
        return ModeRequest._from_owner(owner)

    def submit_bit_noop(self, command_id: int) -> ReturnRequest:
        """Submit BIT with empty payload lists and return its asynchronous request."""

        self._require_open()
        _validate_uint32(command_id, "command_id")
        owner = _native.IrReturnRequestHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_submit_bit_noop(
                    self._owner,
                    command_id,
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_return_request_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "BIT submission succeeded without returning a request owner",
            )
        return ReturnRequest._from_owner(owner)

    def channel_view(self) -> ChannelView:
        """Create a weak common :class:`ChannelView` of this control channel.

        Valid while Attached (before :meth:`enable`) or Enabled. The View
        retains neither this object nor the Session; it owns only a native
        weak-view wrapper. Multiple independent Views are allowed.
        """

        if not self.is_open:
            raise _local_error(ErrorKind.PROVIDER_FAILED, "C2 channel is closed")
        return _open_channel_view(_native.ams_mel_ir_channel_from_c2, self._owner)

    def close(self) -> None:
        """Close this owner; retryable detach failure can leave it open."""

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

    def __enter__(self) -> ControlChannel:
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
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass
