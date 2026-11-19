"""CalibrationConfiguration submission over the existing native completion observer."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._calibration_configuration import CalibrationConfiguration, _configuration_from_raw
from ._common import _NativeOwner
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._types import MelErrorCode
from ._validation import _validate_timeout, _validate_uint32


@dataclass(frozen=True)
class CalibrationConfigurationCommand:
    """Independent, exact unsigned 32-bit command and request identifiers."""

    command_id: int
    request_id: int

    def __post_init__(self) -> None:
        _validate_command(self)


@dataclass(frozen=True)
class CalibrationConfigurationCompleted:
    """Owned configuration data, not evidence of physical calibration execution."""

    configuration: CalibrationConfiguration


@dataclass(frozen=True)
class CalibrationConfigurationRejected:
    """MEL rejection, separate from native failure and response status."""

    code: MelErrorCode
    description: str


def _validate_command(command: CalibrationConfigurationCommand) -> None:
    if not isinstance(command, CalibrationConfigurationCommand):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "command must be CalibrationConfigurationCommand")
    _validate_uint32(command.command_id, "command_id")
    _validate_uint32(command.request_id, "request_id")


class CalibrationConfigurationRequest(_NativeOwner):
    """Unique native observer, independent of Python C2/Session parents.

    Externally serialize Wait and Close; the GIL does not provide thread safety.
    Timeout and Close do not cancel provider work. No Python thread is created.
    """

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.submit_calibration_configuration")

    @classmethod
    def _from_owner(cls, owner):
        request = super()._from_owner(owner)
        request._close_function = _native.ams_mel_ir_calibration_configuration_request_close
        request._close_attempted = False
        request._close_error = None
        return request

    def __copy__(self):
        raise TypeError("CalibrationConfigurationRequest is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CalibrationConfigurationRequest is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CalibrationConfigurationRequest cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def wait(self, timeout_ms: int = 0) -> CalibrationConfigurationCompleted | CalibrationConfigurationRejected:
        """One finite Wait; zero polls. Terminal results are cached natively."""
        self._require_open()
        _validate_timeout(timeout_ms)
        try:
            return _wait(self._owner, timeout_ms)
        except (MemoryError, OverflowError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CalibrationConfiguration result storage") from error

    def close(self) -> None:
        """Release once, retaining errors after uncertain native consumption.

        Allocation failure before native invocation leaves the owner retryable.
        Successful repeated Close calls perform no additional native work.
        """
        if self._close_error is not None:
            raise self._close_error
        if not self.is_open:
            return

        def close_owner(diagnostic, required):
            # Prepare even ctypes argument wrappers before marking invocation.
            # A local allocation failure here has not consumed ownership.
            capacity = len(diagnostic)
            required_pointer = ctypes.byref(required)
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, capacity,
                                        required_pointer)

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationConfiguration request close did not consume its owner")
        except BaseException as error:
            if self._close_attempted:
                self._close_error = error
            raise
        finally:
            if self._close_attempted:
                self._owner.value = None

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            owner = getattr(self, "_owner", None)
            if owner is not None and owner.value:
                self._close_attempted = True
                try:
                    status = int(self._close_function(self._owner_pointer, None, 0, None))
                    if status != _native.AMS_MEL_OK:
                        self._close_error = _error_from_status(status, None, None)
                    elif owner.value:
                        self._close_error = _local_error(
                            ErrorKind.PROTOCOL_INCONSISTENCY,
                            "CalibrationConfiguration request close did not consume its owner")
                except BaseException as error:
                    self._close_error = error
                finally:
                    owner.value = None
        except BaseException:
            pass

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close without masking a primary exception raised inside the context."""
        if exception_type is None:
            self.close()
        else:
            try:
                self.close()
            except BaseException:
                pass
        return False


def _submit_calibration_configuration(parent, command: CalibrationConfigurationCommand) -> CalibrationConfigurationRequest:
    parent._require_open()
    _validate_command(command)  # Frozen values can still be forcibly mutated.
    request = None
    try:
        raw = _native.IrCalibrationConfigurationCommandV1(
            command.command_id, command.request_id)
        # All Python ownership allocations precede the single provider send.
        request = CalibrationConfigurationRequest._from_owner(_native.IrCalibrationConfigurationRequestHandle())
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_submit_calibration_configuration(
                parent._owner, ctypes.byref(raw), request._owner_pointer,
                diagnostic, len(diagnostic), ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CalibrationConfiguration submission succeeded without a request owner")
    except BaseException as error:
        if request is not None:
            request.__del__()  # At most one cleanup, never mask the send failure.
        if isinstance(error, (MemoryError, OverflowError)):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CalibrationConfiguration submission storage") from error
        raise
    return request


_WAIT_DIAGNOSTIC_CAPACITY = 512
_TERMINAL_STATUSES = (
    _native.AMS_MEL_OK, _native.AMS_MEL_COMMAND_REJECTED,
    _native.AMS_MEL_PROVIDER_FAILED, _native.AMS_MEL_PROVIDER_EXCEPTION,
    _native.AMS_MEL_INTERNAL_ERROR,
)


def _active_result(status, result):
    # Never inspect inactive fields, even when comparing cached diagnostic reads.
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return (int(result.error_code),)
    return ()


def _wait(owner, timeout_ms):
    result = _native.IrCalibrationConfigurationResultV1()
    diagnostic = ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    ctypes.memset(diagnostic, 0xFF, len(diagnostic))
    required = ctypes.c_size_t()
    status = int(_native.ams_mel_ir_calibration_configuration_request_wait(
        owner, timeout_ms, ctypes.byref(result), diagnostic,
        len(diagnostic), ctypes.byref(required)))
    length = int(required.value)
    if length == 0:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native CalibrationConfiguration wait returned diagnostic size 0")
    if status not in _TERMINAL_STATUSES:
        message = None
        if length <= len(diagnostic):
            message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic") or None
        # Timeout is nonterminal; do not retry it to retrieve diagnostic text.
        raise _error_from_status(status, message, length)
    if status == _native.AMS_MEL_OK:
        # Success has exactly one diagnostic byte. Reject before any reread or
        # nested access; the snapshot stays borrowed until this request closes.
        if int(result.error_code) != _native.AMS_MEL_ERROR_NONE or length != 1:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native CalibrationConfiguration wait returned an error or diagnostic")
        message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic")
        if message:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native CalibrationConfiguration wait returned a diagnostic")
        # The PY-MEL-021 converter checks the outer span and every nested span,
        # enum and string, and returns only independently owned Python values.
        return CalibrationConfigurationCompleted(_configuration_from_raw(result.configuration))
    first = _active_result(status, result)
    if length <= len(diagnostic):
        message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic")
    else:
        try:
            complete = ctypes.create_string_buffer(length)
        except (MemoryError, OverflowError, ValueError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate complete CalibrationConfiguration diagnostic storage") from error
        ctypes.memset(complete, 0xFF, length)
        retry_result = _native.IrCalibrationConfigurationResultV1()
        retry_required = ctypes.c_size_t()
        retry_status = int(_native.ams_mel_ir_calibration_configuration_request_wait(
            owner, 0, ctypes.byref(retry_result), complete,
            len(complete), ctypes.byref(retry_required)))
        if (retry_status != status or int(retry_required.value) != length
                or _active_result(status, retry_result) != first):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native terminal CalibrationConfiguration wait changed during diagnostic retry")
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return CalibrationConfigurationRejected(MelErrorCode(first[0]), message)
    raise _error_from_status(status, message or None, length)
