"""CalibrationStatus submission over the existing native completion observer."""

from __future__ import annotations

import ctypes
import sys
from dataclasses import dataclass

from . import _native
from ._calibration_status import CalibrationStatus, _status_from_raw
from ._common import _NativeOwner
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._types import MelErrorCode
from ._validation import _validate_timeout, _validate_uint32


@dataclass(frozen=True)
class CalibrationStatusCommand:
    """Independent, exact unsigned 32-bit command and request identifiers."""

    command_id: int
    request_id: int

    def __post_init__(self) -> None:
        _command_fields(self)


@dataclass(frozen=True)
class CalibrationStatusCompleted:
    """Owned calibration status data, not proof of physical calibration."""

    status: CalibrationStatus


@dataclass(frozen=True)
class CalibrationStatusRejected:
    """A MEL rejection with its exact code and complete UTF-8 description."""

    code: MelErrorCode
    description: str


def _command_fields(command: CalibrationStatusCommand) -> tuple[int, int]:
    if not isinstance(command, CalibrationStatusCommand):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "command must be CalibrationStatusCommand")
    command_id, request_id = command.command_id, command.request_id
    for value, name in ((command_id, "command_id"), (request_id, "request_id")):
        # Exclude int subclasses with overridable comparison or conversion.
        if type(value) is not int:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an exact int")
        _validate_uint32(value, name)
    return command_id, request_id


class CalibrationStatusRequest(_NativeOwner):
    """Unique native observer; does not retain the Python C2 or Session parent.

    Externally serialize Wait and Close on each request. Neither timeout nor
    Close cancels admitted work. The provider result is copied during Wait.
    """

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.submit_calibration_status")

    @classmethod
    def _from_owner(cls, owner):
        request = super()._from_owner(owner)
        request._close_function = _native.ams_mel_ir_calibration_status_request_close
        request._close_attempted = False
        request._close_error = None
        return request

    def __copy__(self):
        raise TypeError("CalibrationStatusRequest is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CalibrationStatusRequest is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CalibrationStatusRequest cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def wait(self, timeout_ms: int = 0) -> CalibrationStatusCompleted | CalibrationStatusRejected:
        """Wait for finite uint32 milliseconds (zero polls); leave the request open.

        Timeout and Python copy failures leave the native observer available.
        Repeated terminal Waits read the cached outcome, not another submission.
        """
        self._require_open()
        if type(timeout_ms) is not int:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "timeout_ms must be an exact int")
        _validate_timeout(timeout_ms)
        try:
            return _wait(self._owner, timeout_ms)
        except (MemoryError, OverflowError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CalibrationStatus result storage") from error

    def close(self) -> None:
        """Release the observer once without cancelling admitted provider work.

        Before native invocation, allocation failure leaves Close retryable.
        After invocation, any failure is retained and native access is disarmed.
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
                                   "CalibrationStatus request close did not consume its owner")
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


def _submit_calibration_status(parent, command: CalibrationStatusCommand) -> CalibrationStatusRequest:
    parent._require_open()
    # Frozen values can still be forcibly mutated. Encode only the checked fields.
    command_id, request_id = _command_fields(command)
    request = None
    try:
        raw = _native.IrCalibrationStatusCommandV1(command_id, request_id)
        raw_pointer = ctypes.byref(raw)
        request = CalibrationStatusRequest._from_owner(_native.IrCalibrationStatusRequestHandle())
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_submit_calibration_status(
                parent._owner, raw_pointer, request._owner_pointer,
                diagnostic, len(diagnostic), ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CalibrationStatus submission succeeded without a request owner")
    except BaseException as error:
        if request is not None:
            request.__del__()  # At most one cleanup; never mask the send failure.
        if isinstance(error, (MemoryError, OverflowError)):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CalibrationStatus submission storage") from error
        raise
    return request


_WAIT_DIAGNOSTIC_CAPACITY = 512
_CACHED_ERROR_STATUSES = (
    _native.AMS_MEL_COMMAND_REJECTED, _native.AMS_MEL_PROVIDER_FAILED,
    _native.AMS_MEL_PROVIDER_EXCEPTION, _native.AMS_MEL_INTERNAL_ERROR,
)


def _wait(owner, timeout_ms):
    wait_native = _native.ams_mel_ir_calibration_status_request_wait

    def read(timeout, capacity):
        result = _native.IrCalibrationStatusResultV1()
        diagnostic = ctypes.create_string_buffer(capacity)
        ctypes.memset(diagnostic, 0xFF, capacity)
        required = ctypes.c_size_t()
        status = int(wait_native(owner, timeout, ctypes.byref(result), diagnostic,
                                 capacity, ctypes.byref(required)))
        return status, required.value, diagnostic, result

    status, length, diagnostic, result = read(timeout_ms, _WAIT_DIAGNOSTIC_CAPACITY)
    if not 0 < length <= sys.maxsize:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native CalibrationStatus wait returned an invalid diagnostic size")
    if status == _native.AMS_MEL_OK:
        # Check the entire success diagnostic before even reading the nested view.
        if length != 1 or diagnostic[0] != b"\0" or result.error_code != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native CalibrationStatus wait returned an error or diagnostic")
        # The native snapshot (including every nested pointer) is borrowed until
        # request Close. Use the same checked, complete copy as metadata reception.
        return CalibrationStatusCompleted(_status_from_raw(result.status))

    # Only rejection has an active result field; failure and timeout have none.
    rejected = status == _native.AMS_MEL_COMMAND_REJECTED
    code = result.error_code if rejected else None
    if length > len(diagnostic):
        if status not in _CACHED_ERROR_STATUSES:
            # In particular, do not poll a nonterminal timeout for its diagnostic.
            raise _error_from_status(status, None, length)
        retry_status, retry_length, diagnostic, retry_result = read(0, length)
        if (retry_status != status or retry_length != length
                or (rejected and retry_result.error_code != code)):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native terminal CalibrationStatus wait changed during diagnostic reread")
    message = _decode_complete_buffer(diagnostic.raw[:length], "diagnostic")
    if rejected:
        return CalibrationStatusRejected(MelErrorCode(code), message)
    raise _error_from_status(status, message or None, length)
