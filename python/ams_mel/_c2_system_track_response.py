"""Optional IR C2 SystemTrackDataResponse; no Track-channel operation."""

from __future__ import annotations

import ctypes
import sys
from dataclasses import dataclass

from . import _native
from ._c2_convert import _copy_command_status
from ._c2_types import C2CommandStatus
from ._common import _NativeOwner
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._image_types import AzimuthElevation, _az_el_fields
from ._types import MelErrorCode
from ._validation import _validate_double, _validate_int64, _validate_timeout, _validate_uint32


@dataclass(frozen=True)
class SystemTrackDataResponse:
    """Complete track data in signed nanoseconds, meters, meters/sec and radians.

    No clock or epoch is inferred. Both validity flags are independent of the
    corresponding numeric fields, which are always sent without normalization.
    """

    system_time_ns: int
    command_id: int
    request_id: int
    track_id: int
    range_m: float
    range_rate_mps: float
    range_error_m: float
    range_rate_error_mps: float
    az_el_valid: bool
    range_valid: bool
    inertial_az_el: AzimuthElevation
    az_el_error: AzimuthElevation

    def __post_init__(self) -> None:
        _response_fields(self)


@dataclass(frozen=True)
class C2SystemTrackDataCompleted:
    """Owned CommandStatus, not evidence of operational incorporation.

    Even a CommandStatus with state REJECTED is a completed future.
    """

    status: C2CommandStatus


@dataclass(frozen=True)
class C2SystemTrackDataRejected:
    """Explicit MEL ErrorOr rejection, with its complete owned description."""

    code: MelErrorCode
    description: str


def _response_fields(response: SystemTrackDataResponse) -> tuple:
    if type(response) is not SystemTrackDataResponse:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "response must be SystemTrackDataResponse")

    timestamp = getattr(response, "system_time_ns", None)
    id_names = ("command_id", "request_id", "track_id")
    ids = tuple(getattr(response, name, None) for name in id_names)
    if type(timestamp) is not int:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "system_time_ns must be an exact int")
    _validate_int64(timestamp, "system_time_ns")
    for name, value in zip(id_names, ids):
        if type(value) is not int:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be an exact int")
        _validate_uint32(value, name)

    az_el_valid = getattr(response, "az_el_valid", None)
    range_valid = getattr(response, "range_valid", None)
    if type(az_el_valid) is not bool or type(range_valid) is not bool:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "az_el_valid and range_valid must be bool")

    # Snapshot fields before numeric conversions, which can invoke Python code.
    inertial = getattr(response, "inertial_az_el", None)
    error = getattr(response, "az_el_error", None)
    range_names = ("range_m", "range_rate_mps", "range_error_m", "range_rate_error_mps")
    ranges = tuple(getattr(response, name, None) for name in range_names)
    return (
        timestamp, *ids,
        *(_validate_double(value, name) for name, value in zip(range_names, ranges)),
        int(az_el_valid), int(range_valid), _az_el_fields(inertial), _az_el_fields(error),
    )


class C2SystemTrackDataRequest(_NativeOwner):
    """Unique C2 observer; retains neither the Python channel nor Session.

    Externally serialize Wait and Close on this request. Neither timeout nor
    Close cancels provider work or releases its native admission early.
    """

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.submit_system_track_data_response")

    @classmethod
    def _from_owner(cls, owner):
        request = super()._from_owner(owner)
        request._close_function = _native.ams_mel_ir_c2_system_response_request_close
        request._close_attempted = False
        request._close_error = None
        return request

    def __copy__(self):
        raise TypeError("C2SystemTrackDataRequest is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("C2SystemTrackDataRequest is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("C2SystemTrackDataRequest cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def wait(self, timeout_ms: int = 0) -> C2SystemTrackDataCompleted | C2SystemTrackDataRejected:
        """Poll at zero or wait for finite uint32 milliseconds without consuming.

        A timeout raises MelError without cancellation. Repeated terminal Waits
        read the native cache. Status text is copied while this observer is open.
        """
        self._require_open()
        if type(timeout_ms) is not int:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "timeout_ms must be an exact int")
        _validate_timeout(timeout_ms)
        try:
            return _wait(self._owner, timeout_ms)
        except (MemoryError, OverflowError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate C2 system response result storage") from error

    def close(self) -> None:
        """Release the observer without cancelling admitted work.

        A failure before invocation leaves Close retryable. A failure after
        invocation disarms the owner and is reported again without native access.
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
                                   "C2 system response Close did not consume its owner")
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


def _submit_system_track_data_response(parent, response: SystemTrackDataResponse) -> C2SystemTrackDataRequest:
    parent._require_open()
    request = None
    try:
        *fields, inertial, error_pair = _response_fields(response)
        raw = _native.IrSystemTrackDataResponseV1(
            *fields, _native.IrAzElV1(*inertial), _native.IrAzElV1(*error_pair))
        raw_pointer = ctypes.byref(raw)
        request = C2SystemTrackDataRequest._from_owner(_native.IrC2SystemResponseRequestHandle())
        # Numeric input conversion can invoke Python; recheck the parent before send.
        parent._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_submit_system_track_data_response(
                parent._owner, raw_pointer, request._owner_pointer,
                diagnostic, len(diagnostic), ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "C2 system response submission succeeded without a request owner")
    except BaseException as error:
        if request is not None:
            request.__del__()  # One best-effort Close of an unexpectedly published owner.
        if isinstance(error, (MemoryError, OverflowError)):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate C2 system response submission storage") from error
        raise
    return request


_WAIT_DIAGNOSTIC_CAPACITY = 512
_CACHED_ERROR_STATUSES = (
    _native.AMS_MEL_COMMAND_REJECTED, _native.AMS_MEL_PROVIDER_FAILED,
    _native.AMS_MEL_PROVIDER_EXCEPTION, _native.AMS_MEL_INTERNAL_ERROR,
)


def _wait(owner, timeout_ms):
    wait_native = _native.ams_mel_ir_c2_system_response_request_wait

    def read(timeout, capacity):
        result = _native.IrC2SystemResponseResultV1()
        diagnostic = ctypes.create_string_buffer(capacity)
        ctypes.memset(diagnostic, 0xFF, capacity)
        required = ctypes.c_size_t()
        status = int(wait_native(owner, timeout, ctypes.byref(result), diagnostic,
                                 capacity, ctypes.byref(required)))
        return status, required.value, diagnostic, result

    status, length, diagnostic, result = read(timeout_ms, _WAIT_DIAGNOSTIC_CAPACITY)
    if not 0 < length <= sys.maxsize:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native C2 system response Wait returned an invalid diagnostic size")
    if status == _native.AMS_MEL_OK:
        if length != 1 or diagnostic[0] != b"\0" or result.error_code != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful C2 system response Wait returned an error or diagnostic")
        return C2SystemTrackDataCompleted(
            _copy_command_status(result.status, "C2 CommandStatus reason description"))

    # Only error_code is active on rejection; nothing is active on failure or timeout.
    rejected = status == _native.AMS_MEL_COMMAND_REJECTED
    code = result.error_code if rejected else None
    if length > len(diagnostic):
        if status not in _CACHED_ERROR_STATUSES:
            raise _error_from_status(status, None, length)  # Never reread a nonterminal poll.
        retry_status, retry_length, diagnostic, retry_result = read(0, length)
        if (retry_status != status or retry_length != length
                or (rejected and retry_result.error_code != code)):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native C2 system response outcome changed during diagnostic reread")
    message = _decode_complete_buffer(diagnostic.raw[:length], "diagnostic")
    if rejected:
        return C2SystemTrackDataRejected(MelErrorCode(code), message)
    raise _error_from_status(status, message or None, length)