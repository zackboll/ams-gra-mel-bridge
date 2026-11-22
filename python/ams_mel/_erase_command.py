"""Optional IR C2 EraseCommand over the existing native completion observer."""

from __future__ import annotations

import ctypes
import sys
from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._common import _NativeOwner
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._types import MelErrorCode
from ._validation import _validate_timeout, _validate_uint32


class EraseCommandType(IntEnum):
    """The four published erase alternatives, with no sentinel or default."""

    ZEROIZE = _native.AMS_MEL_IR_ERASE_ZEROIZE
    CLASSIFIED_DATA_ERASE = _native.AMS_MEL_IR_ERASE_CLASSIFIED_DATA_ERASE
    DISABLE = _native.AMS_MEL_IR_ERASE_DISABLE
    CDE_AND_ZEROIZE = _native.AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE


@dataclass(frozen=True)
class EraseCommand:
    """Explicit erase alternative and exact uint32 command ID; construction does not submit."""

    command_id: int
    erase_type: EraseCommandType

    def __post_init__(self) -> None:
        _command_fields(self)


@dataclass(frozen=True)
class EraseCompleted:
    """Provider-returned alternative, not evidence of completed physical erasure or disablement."""

    erase_type: EraseCommandType


@dataclass(frozen=True)
class EraseRejected:
    """A MEL rejection with its exact code and complete UTF-8 description."""

    code: MelErrorCode
    description: str


def _command_fields(command: EraseCommand) -> tuple[int, EraseCommandType]:
    if type(command) is not EraseCommand:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "command must be EraseCommand")
    command_id = getattr(command, "command_id", None)
    erase_type = getattr(command, "erase_type", None)
    if type(command_id) is not int:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "command_id must be an exact int")
    _validate_uint32(command_id, "command_id")
    if type(erase_type) is not EraseCommandType or not any(
        erase_type is member for member in EraseCommandType
    ):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "erase_type must be an EraseCommandType member")
    return command_id, erase_type


class EraseRequest(_NativeOwner):
    """Unique native observer that does not retain the Python C2 or Session parent.

    Externally serialize Wait and Close on each request. Neither timeout nor
    Close cancels admitted provider work.
    """

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.submit_erase")

    @classmethod
    def _from_owner(cls, owner):
        request = super()._from_owner(owner)
        request._close_function = _native.ams_mel_ir_erase_request_close
        request._close_attempted = False
        request._close_error = None
        return request

    def __copy__(self):
        raise TypeError("EraseRequest is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("EraseRequest is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("EraseRequest cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def wait(self, timeout_ms: int = 0) -> EraseCompleted | EraseRejected:
        """Wait for finite uint32 milliseconds (zero polls) without consuming the observer.

        Repeated terminal Waits read the native cache, not another submission.
        The returned alternative need not match the one submitted.
        """
        self._require_open()
        if type(timeout_ms) is not int:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "timeout_ms must be an exact int")
        _validate_timeout(timeout_ms)
        try:
            return _wait(self._owner, timeout_ms)
        except (MemoryError, OverflowError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate EraseCommand result storage") from error

    def close(self) -> None:
        """Release the observer without cancelling admitted work.

        Allocation failure before invocation leaves Close retryable. After
        invocation, any failure is retained and ownership is disarmed.
        """
        if self._close_error is not None:
            raise self._close_error
        if not self.is_open:
            return
        close_native, output = self._close_function, self._owner_pointer

        def close_owner(diagnostic, required):
            capacity, required_pointer = len(diagnostic), ctypes.byref(required)
            self._close_attempted = True
            return close_native(output, diagnostic, capacity, required_pointer)

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "EraseRequest close did not consume its owner")
        except BaseException as error:
            if self._close_attempted:
                self._close_error = error
            raise
        finally:
            if self._close_attempted:
                self._owner.value = None

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close without masking a primary exception from the with body."""
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
            if not self._close_attempted and self.is_open:
                self._close_attempted = True
                try:
                    self._close_function(self._owner_pointer, None, 0, None)
                finally:
                    self._owner.value = None
        except BaseException:
            pass


def _submit_erase(parent, command: EraseCommand) -> EraseRequest:
    parent._require_open()
    # Frozen values can still be forcibly mutated. Encode only the checked fields.
    command_id, erase_type = _command_fields(command)
    request = None
    try:
        raw = _native.IrEraseCommandV1(command_id, int(erase_type))
        raw_pointer = ctypes.byref(raw)
        request = EraseRequest._from_owner(_native.IrEraseRequestHandle())
        # Native admission checks Enabled and acquires the Session/C2 claims.
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_submit_erase(
                parent._owner, raw_pointer, request._owner_pointer,
                diagnostic, len(diagnostic), ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "EraseCommand submission succeeded without a request owner")
    except BaseException as error:
        if request is not None:
            request.__del__()  # At most one cleanup; never mask the send failure.
        if isinstance(error, (MemoryError, OverflowError)):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate EraseCommand submission storage") from error
        raise
    return request


_WAIT_DIAGNOSTIC_CAPACITY = 512
_CACHED_ERROR_STATUSES = (
    _native.AMS_MEL_COMMAND_REJECTED, _native.AMS_MEL_PROVIDER_FAILED,
    _native.AMS_MEL_PROVIDER_EXCEPTION, _native.AMS_MEL_INTERNAL_ERROR,
)


def _wait(owner, timeout_ms):
    wait_native = _native.ams_mel_ir_erase_request_wait

    def read(timeout, capacity):
        result = _native.IrEraseResultV1()
        diagnostic = ctypes.create_string_buffer(capacity)
        ctypes.memset(diagnostic, 0xFF, capacity)
        required = ctypes.c_size_t()
        status = int(wait_native(owner, timeout, ctypes.byref(result), diagnostic,
                                 capacity, ctypes.byref(required)))
        return status, required.value, diagnostic, result

    status, length, diagnostic, result = read(timeout_ms, _WAIT_DIAGNOSTIC_CAPACITY)
    if not 0 < length <= sys.maxsize:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native EraseRequest wait returned an invalid diagnostic size")
    if status == _native.AMS_MEL_OK:
        if length != 1 or diagnostic[0] != b"\0" or result.error_code != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native EraseRequest wait returned an error or diagnostic")
        try:
            erase_type = EraseCommandType(result.erase_type)
        except ValueError as error:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native EraseRequest wait returned an unknown erase type") from error
        return EraseCompleted(erase_type)

    # Only rejection has an active result field; failures and timeout have none.
    rejected = status == _native.AMS_MEL_COMMAND_REJECTED
    code = result.error_code if rejected else None
    if length > len(diagnostic):
        if status not in _CACHED_ERROR_STATUSES:
            # Do not poll a nonterminal timeout to retrieve its diagnostic.
            raise _error_from_status(status, None, length)
        retry_status, retry_length, diagnostic, retry_result = read(0, length)
        if (retry_status != status or retry_length != length
                or (rejected and retry_result.error_code != code)):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native terminal EraseRequest wait changed during diagnostic reread")
    message = _decode_complete_buffer(diagnostic.raw[:length], "diagnostic")
    if rejected:
        return EraseRejected(MelErrorCode(code), message)
    raise _error_from_status(status, message or None, length)
