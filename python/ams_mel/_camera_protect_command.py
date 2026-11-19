"""CameraProtect submission over the existing native completion observer."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from . import _native
from ._camera_protect_response import CameraProtectResponse, _response_from_raw
from ._common import _NativeOwner
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._types import MelErrorCode
from ._validation import _validate_int64, _validate_timeout, _validate_uint32


@dataclass(frozen=True)
class CameraProtectCommand:
    """Exact command fields; nanoseconds imply neither an epoch nor a deadline."""

    command_id: int
    response_time_ns: int
    protect_flag: bool

    def __post_init__(self) -> None:
        _validate_command(self)


@dataclass(frozen=True)
class CameraProtectCompleted:
    """Successful provider data, including UNAVAILABLE and UNSUPPORTED."""

    response: CameraProtectResponse


@dataclass(frozen=True)
class CameraProtectRejected:
    """MEL rejection, separate from native failure and response status."""

    code: MelErrorCode
    description: str


def _validate_command(command: CameraProtectCommand) -> None:
    if not isinstance(command, CameraProtectCommand):
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "command must be CameraProtectCommand")
    _validate_uint32(command.command_id, "command_id")
    _validate_int64(command.response_time_ns, "response_time_ns")
    if type(command.protect_flag) is not bool:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "protect_flag must be bool")


class CameraProtectRequest(_NativeOwner):
    """Unique native observer, independent of Python C2/Session parents.

    Externally serialize Wait and Close; the GIL does not provide thread safety.
    Timeout and Close do not cancel provider work. No Python thread is created.
    """

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.submit_camera_protect")

    @classmethod
    def _from_owner(cls, owner):
        request = super()._from_owner(owner)
        request._close_function = _native.ams_mel_ir_camera_protect_request_close
        request._close_attempted = False
        request._close_error = None
        return request

    def __copy__(self):
        raise TypeError("CameraProtectRequest is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CameraProtectRequest is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CameraProtectRequest cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def wait(self, timeout_ms: int = 0) -> CameraProtectCompleted | CameraProtectRejected:
        """One finite Wait; zero polls. Terminal results are cached natively."""
        self._require_open()
        _validate_timeout(timeout_ms)
        try:
            return _wait(self._owner, timeout_ms)
        except (MemoryError, OverflowError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CameraProtect result storage") from error

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
                                   "CameraProtect request close did not consume its owner")
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
                            "CameraProtect request close did not consume its owner")
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


def _submit_camera_protect(parent, command: CameraProtectCommand) -> CameraProtectRequest:
    parent._require_open()
    _validate_command(command)  # Frozen values can still be forcibly mutated.
    request = None
    try:
        raw = _native.IrCameraProtectCommandV1(
            command.command_id, command.response_time_ns, int(command.protect_flag))
        # All Python ownership allocations precede the single provider send.
        request = CameraProtectRequest._from_owner(_native.IrCameraProtectRequestHandle())
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_c2_submit_camera_protect(
                parent._owner, ctypes.byref(raw), request._owner_pointer,
                diagnostic, len(diagnostic), ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CameraProtect submission succeeded without a request owner")
    except BaseException as error:
        if request is not None:
            request.__del__()  # At most one cleanup, never mask the send failure.
        if isinstance(error, (MemoryError, OverflowError)):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate CameraProtect submission storage") from error
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
    if status == _native.AMS_MEL_OK:
        response = result.response
        return (int(result.error_code), int(response.command_id),
                int(response.response_time_ns), int(response.command_response))
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return (int(result.error_code),)
    return ()


def _wait(owner, timeout_ms):
    result = _native.IrCameraProtectResultV1()
    diagnostic = ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    ctypes.memset(diagnostic, 0xFF, len(diagnostic))
    required = ctypes.c_size_t()
    status = int(_native.ams_mel_ir_camera_protect_request_wait(
        owner, timeout_ms, ctypes.byref(result), diagnostic,
        len(diagnostic), ctypes.byref(required)))
    length = int(required.value)
    if length == 0:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native CameraProtect wait returned diagnostic size 0")
    if status not in _TERMINAL_STATUSES:
        message = None
        if length <= len(diagnostic):
            message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic") or None
        # Timeout is nonterminal; do not retry it to retrieve diagnostic text.
        raise _error_from_status(status, message, length)
    first = _active_result(status, result)
    if length <= len(diagnostic):
        message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic")
    else:
        try:
            complete = ctypes.create_string_buffer(length)
        except (MemoryError, OverflowError, ValueError) as error:
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate complete CameraProtect diagnostic storage") from error
        ctypes.memset(complete, 0xFF, length)
        retry_result = _native.IrCameraProtectResultV1()
        retry_required = ctypes.c_size_t()
        retry_status = int(_native.ams_mel_ir_camera_protect_request_wait(
            owner, 0, ctypes.byref(retry_result), complete,
            len(complete), ctypes.byref(retry_required)))
        if (retry_status != status or int(retry_required.value) != length
                or _active_result(status, retry_result) != first):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native terminal CameraProtect wait changed during diagnostic retry")
        result = retry_result
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")
    if status == _native.AMS_MEL_OK:
        if first[0] != _native.AMS_MEL_ERROR_NONE or length != 1 or message:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native CameraProtect wait returned an error or diagnostic")
        return CameraProtectCompleted(_response_from_raw(result.response))
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return CameraProtectRejected(MelErrorCode(first[0]), message)
    raise _error_from_status(status, message or None, length)
