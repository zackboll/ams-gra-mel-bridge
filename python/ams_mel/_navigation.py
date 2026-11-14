"""Navigation request ownership over the existing native completion engine."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._errors import (
    ErrorKind, _call_with_diagnostic, _decode_complete_buffer,
    _error_from_status, _local_error,
)
from ._image_types import NavigationResponse
from ._navigation_types import (
    NavigationCompleted, NavigationRejected, NavigationReport, _report_to_raw,
)
from ._types import MelErrorCode
from ._validation import _validate_timeout


_WAIT_DIAGNOSTIC_CAPACITY = 512
_TERMINAL_STATUSES = (
    _native.AMS_MEL_OK, _native.AMS_MEL_COMMAND_REJECTED,
    _native.AMS_MEL_PROVIDER_EXCEPTION, _native.AMS_MEL_PROVIDER_FAILED,
    _native.AMS_MEL_INTERNAL_ERROR,
)


class NavigationRequest:
    """Unique owner; externally serialize Wait/Close (the GIL is not a lock).

    No Python ImageStream or Session is retained. Timeout and Close do not
    cancel native work. Native completion retains the provider graph as needed.
    An uncertain release is attempted at most once, never retried.
    """

    def __init__(self) -> None:
        raise TypeError("NavigationRequest objects must be created by ImageStream.submit_navigation_report")

    @classmethod
    def _from_owner(cls, owner: _native.IrNavigationRequestHandle) -> NavigationRequest:
        request = cls.__new__(cls)
        request._owner = owner
        request._owner_pointer = _ctypes.pointer(owner)
        request._close_function = _native.ams_mel_ir_navigation_request_close
        return request

    @property
    def is_open(self) -> bool:
        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "NavigationRequest is closed")

    def wait(self, timeout_ms: int) -> NavigationCompleted | NavigationRejected:
        """Wait for a cached result; zero polls, timeout neither cancels nor consumes."""
        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait(self._owner, timeout_ms)

    def close(self) -> None:
        """Release once without cancellation, including on uncertain native failure."""
        if not self.is_open:
            return
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: self._close_function(
                    self._owner_pointer, diagnostic, len(diagnostic), _ctypes.byref(required)))
        finally:
            self._owner.value = None

    def __enter__(self) -> NavigationRequest:
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
            if owner is not None and owner.value:
                try:
                    self._close_function(self._owner_pointer, None, 0, None)
                finally:
                    owner.value = None
        except BaseException:
            pass

    def __copy__(self) -> NavigationRequest:
        raise TypeError("NavigationRequest is a unique owner")

    def __deepcopy__(self, memo: object) -> NavigationRequest:
        raise TypeError("NavigationRequest is a unique owner")

    def __reduce_ex__(self, protocol: int) -> object:
        raise TypeError("NavigationRequest owners cannot be pickled")


def _submit_navigation_report(stream: object, report: NavigationReport) -> NavigationRequest:
    raw = _report_to_raw(report)
    stream._require_open()
    request = NavigationRequest._from_owner(_native.IrNavigationRequestHandle())
    try:
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_stream_submit_navigation_report(
                stream._owner, _ctypes.byref(raw), request._owner_pointer,
                diagnostic, len(diagnostic), _ctypes.byref(required)))
        if not request.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "NavigationReport submission succeeded without a request owner")
    except BaseException:
        request.__del__()
        raise
    return request


def _valid_result(status: int, result: _native.IrNavigationResultV1) -> tuple[int, ...]:
    # Never read inactive response bytes, including during diagnostic retrieval.
    if status == _native.AMS_MEL_OK:
        response = result.response
        return (int(result.error_code), int(response.system_time_ns),
                int(response.command_id), int(response.request_id))
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return (int(result.error_code),)
    return ()


def _wait(owner: _native.IrNavigationRequestHandle,
          timeout_ms: int) -> NavigationCompleted | NavigationRejected:
    result = _native.IrNavigationResultV1()
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, len(diagnostic))
    required = _ctypes.c_size_t()
    status = int(_native.ams_mel_ir_navigation_request_wait(
        owner, timeout_ms, _ctypes.byref(result), diagnostic,
        len(diagnostic), _ctypes.byref(required)))
    length = int(required.value)
    if length == 0:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "native Navigation wait returned invalid diagnostic size 0")
    if status not in _TERMINAL_STATUSES:
        message = None
        if length <= len(diagnostic):
            message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic") or None
        # In particular, never retry a nonterminal timeout for diagnostic text.
        raise _error_from_status(status, message, length)
    first = _valid_result(status, result)
    if length <= len(diagnostic):
        message = _decode_complete_buffer(bytes(diagnostic.raw[:length]), "diagnostic")
    else:
        try:
            complete = _ctypes.create_string_buffer(length)
        except (MemoryError, OverflowError, ValueError):
            raise _local_error(ErrorKind.INTERNAL_ERROR,
                               "unable to allocate complete Navigation diagnostic storage") from None
        _ctypes.memset(complete, 0xFF, length)
        retry_result = _native.IrNavigationResultV1()
        retry_required = _ctypes.c_size_t()
        retry_status = int(_native.ams_mel_ir_navigation_request_wait(
            owner, 0, _ctypes.byref(retry_result), complete,
            len(complete), _ctypes.byref(retry_required)))
        if (retry_status != status or int(retry_required.value) != length
                or _valid_result(status, retry_result) != first):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "native terminal Navigation wait changed during diagnostic retry")
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")
    if status == _native.AMS_MEL_OK:
        if first[0] != _native.AMS_MEL_ERROR_NONE or length != 1 or message:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "successful native Navigation wait returned an error or diagnostic")
        return NavigationCompleted(NavigationResponse(*first[1:]))
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return NavigationRejected(MelErrorCode(first[0]), message)
    raise _error_from_status(status, message or None, length)
