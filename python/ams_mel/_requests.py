"""Private requests implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _decode_complete_buffer,
    _error_from_status,
    _local_error,
)
from ._validation import (
    _validate_timeout,
)
from ._types import (
    CommandReturn,
    CommsCompleted,
    CommsRejected,
    CommsTestReport,
    MelErrorCode,
    MfaMode,
    ModeRejected,
    ModeSuccess,
    ReturnCompleted,
    ReturnRejected,
)


_WAIT_DIAGNOSTIC_CAPACITY = 512


class ModeRequest:
    """Unique asynchronous mode request; close does not cancel provider work."""

    def __init__(self) -> None:
        raise TypeError("ModeRequest objects must be created by ControlChannel.submit_operate")

    @classmethod
    def _from_owner(cls, owner: _native.IrModeRequestHandle) -> ModeRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_mode_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ModeRequest is closed")

    def wait(self, timeout_ms: int) -> ModeSuccess | ModeRejected:
        """Wait finitely for a cached result. Zero polls; timeout does not cancel."""

        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_mode(self._owner, timeout_ms)

    def close(self) -> None:
        """Release this public owner without cancelling pending provider work."""

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

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


class ReturnRequest:
    """Unique asynchronous Return request; close does not cancel provider work.

    Created by :meth:`ControlChannel.submit_bit_noop` (C2 BIT) and
    :meth:`ChannelView.send_keepalive` (common KeepAlive). Behavior is
    identical for both: cached Wait, timeout is not cancellation, and the
    request owns the admitted native family graph independently of its parents.
    """

    def __init__(self) -> None:
        raise TypeError(
            "ReturnRequest objects must be created by ControlChannel.submit_bit_noop "
            "or ChannelView.send_keepalive"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrReturnRequestHandle) -> ReturnRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_return_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "ReturnRequest is closed")

    def wait(self, timeout_ms: int) -> ReturnCompleted | ReturnRejected:
        """Wait finitely for a Return; zero polls and timeout does not cancel."""

        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_return(self._owner, timeout_ms)

    def close(self) -> None:
        """Release this public owner without cancelling pending provider work."""

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

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


class CommsRequest:
    """Unique asynchronous CommsTest request; close does not cancel provider work.

    Created by :meth:`ChannelView.submit_comms_test`. Owns the admitted native
    family graph independently of the View, typed owner, and Session. Wait and
    close on one request require external serialization.
    """

    def __init__(self) -> None:
        raise TypeError(
            "CommsRequest objects must be created by ChannelView.submit_comms_test"
        )

    @classmethod
    def _from_owner(cls, owner: _native.IrChannelCommsRequestHandle) -> CommsRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_channel_comms_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CommsRequest is closed")

    def wait(self, timeout_ms: int) -> CommsCompleted | CommsRejected:
        """Wait finitely for CommsTest; zero polls and timeout does not cancel."""

        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_comms(self._owner, timeout_ms)

    def close(self) -> None:
        """Release this public owner without cancelling pending provider work."""

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

    def __del__(self) -> None:
        try:
            owner = getattr(self, "_owner", None)
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass


def _wait_for_mode(
    owner: _native.IrModeRequestHandle, timeout_ms: int
) -> ModeSuccess | ModeRejected:
    result = _native.IrModeResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _WAIT_DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(
        _native.ams_mel_ir_mode_request_wait(
            owner,
            timeout_ms,
            _ctypes.byref(result),
            diagnostic,
            len(diagnostic),
            _ctypes.byref(required),
        )
    )
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native wait returned an invalid diagnostic size",
        )
    if status == _native.AMS_MEL_TIMEOUT:
        message = None
        if required_value <= len(diagnostic):
            message = _decode_complete_buffer(
                bytes(diagnostic.raw[:required_value]), "diagnostic"
            ) or None
        raise _error_from_status(status, message, required_value)

    first_mode = int(result.mode)
    first_error_code = int(result.error_code)
    if required_value <= len(diagnostic):
        message = _decode_complete_buffer(
            bytes(diagnostic.raw[:required_value]), "diagnostic"
        )
    else:
        try:
            complete = _ctypes.create_string_buffer(required_value)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate complete wait diagnostic storage",
            ) from None
        retry_result = _native.IrModeResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(
            _native.ams_mel_ir_mode_request_wait(
                owner,
                0,
                _ctypes.byref(retry_result),
                complete,
                len(complete),
                _ctypes.byref(retry_required),
            )
        )
        if (
            retry_status != status
            or int(retry_result.mode) != first_mode
            or int(retry_result.error_code) != first_error_code
            or int(retry_required.value) != required_value
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native terminal wait changed during diagnostic retry",
            )
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")

    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native wait returned a diagnostic",
            )
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native wait returned an error code",
            )
        try:
            return ModeSuccess(MfaMode(int(result.mode)))
        except ValueError as error:
            raise _local_error(
                ErrorKind.PROVIDER_FAILED,
                "native provider returned an unknown MFA mode",
            ) from error
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return ModeRejected(MelErrorCode(int(result.error_code)), message)
    raise _error_from_status(status, message or None, required_value)


def _wait_for_return(
    owner: _native.IrReturnRequestHandle, timeout_ms: int
) -> ReturnCompleted | ReturnRejected:
    result = _native.IrReturnResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _WAIT_DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(
        _native.ams_mel_ir_return_request_wait(
            owner,
            timeout_ms,
            _ctypes.byref(result),
            diagnostic,
            len(diagnostic),
            _ctypes.byref(required),
        )
    )
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native Return wait returned an invalid diagnostic size",
        )
    if status == _native.AMS_MEL_TIMEOUT:
        message = None
        if required_value <= len(diagnostic):
            message = _decode_complete_buffer(
                bytes(diagnostic.raw[:required_value]), "diagnostic"
            ) or None
        raise _error_from_status(status, message, required_value)

    first_value = int(result.value)
    first_error_code = int(result.error_code)
    if required_value <= len(diagnostic):
        message = _decode_complete_buffer(
            bytes(diagnostic.raw[:required_value]), "diagnostic"
        )
    else:
        try:
            complete = _ctypes.create_string_buffer(required_value)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate complete Return wait diagnostic storage",
            ) from None
        retry_result = _native.IrReturnResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(
            _native.ams_mel_ir_return_request_wait(
                owner,
                0,
                _ctypes.byref(retry_result),
                complete,
                len(complete),
                _ctypes.byref(retry_required),
            )
        )
        if (
            retry_status != status
            or int(retry_result.value) != first_value
            or int(retry_result.error_code) != first_error_code
            or int(retry_required.value) != required_value
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native terminal Return wait changed during diagnostic retry",
            )
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")

    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native Return wait returned a diagnostic",
            )
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native Return wait returned an error code",
            )
        try:
            return ReturnCompleted(CommandReturn(int(result.value)))
        except ValueError as error:
            raise _local_error(
                ErrorKind.PROVIDER_FAILED,
                f"native provider returned unknown command Return value {int(result.value)}",
            ) from error
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return ReturnRejected(MelErrorCode(int(result.error_code)), message)
    raise _error_from_status(status, message or None, required_value)


def _wait_for_comms(
    owner: _native.IrChannelCommsRequestHandle, timeout_ms: int
) -> CommsCompleted | CommsRejected:
    result = _native.IrChannelCommsTestResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, _WAIT_DIAGNOSTIC_CAPACITY)
    required = _ctypes.c_size_t()
    status = int(
        _native.ams_mel_ir_channel_comms_request_wait(
            owner,
            timeout_ms,
            _ctypes.byref(result),
            diagnostic,
            len(diagnostic),
            _ctypes.byref(required),
        )
    )
    required_value = int(required.value)
    if required_value == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY,
            "native CommsTest wait returned an invalid diagnostic size",
        )
    if status == _native.AMS_MEL_TIMEOUT:
        message = None
        if required_value <= len(diagnostic):
            message = _decode_complete_buffer(
                bytes(diagnostic.raw[:required_value]), "diagnostic"
            ) or None
        raise _error_from_status(status, message, required_value)

    first_command_id = int(result.command_id)
    first_request_id = int(result.request_id)
    first_error_code = int(result.error_code)
    if required_value <= len(diagnostic):
        message = _decode_complete_buffer(
            bytes(diagnostic.raw[:required_value]), "diagnostic"
        )
    else:
        try:
            complete = _ctypes.create_string_buffer(required_value)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(
                ErrorKind.INTERNAL_ERROR,
                "unable to allocate complete CommsTest wait diagnostic storage",
            ) from None
        retry_result = _native.IrChannelCommsTestResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(
            _native.ams_mel_ir_channel_comms_request_wait(
                owner,
                0,
                _ctypes.byref(retry_result),
                complete,
                len(complete),
                _ctypes.byref(retry_required),
            )
        )
        if (
            retry_status != status
            or int(retry_result.command_id) != first_command_id
            or int(retry_result.request_id) != first_request_id
            or int(retry_result.error_code) != first_error_code
            or int(retry_required.value) != required_value
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native terminal CommsTest wait changed during diagnostic retry",
            )
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")

    if status == _native.AMS_MEL_OK:
        if required_value != 1 or message:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native CommsTest wait returned a diagnostic",
            )
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful native CommsTest wait returned an error code",
            )
        return CommsCompleted(
            CommsTestReport(int(result.command_id), int(result.request_id))
        )
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return CommsRejected(MelErrorCode(int(result.error_code)), message)
    raise _error_from_status(status, message or None, required_value)
