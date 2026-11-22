"""Opt-in C2 CalibrationStatus reception over the existing C facade."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._c2_types import C2MetadataCounters
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span, _checked_span_address, _copy_string_view, _copy_uci_id
from ._types import CommandReturn, UciId
from ._validation import _validate_size_t, _validate_timeout


class SubsystemCalibrationResult(IntEnum):
    """Published calibration results; MaxExclusive is not data."""

    NOT_SET = _native.AMS_MEL_IR_CALIBRATION_RESULT_NOT_SET
    COMPLETED = _native.AMS_MEL_IR_CALIBRATION_RESULT_COMPLETED
    FAILED = _native.AMS_MEL_IR_CALIBRATION_RESULT_FAILED
    INTERRUPTED = _native.AMS_MEL_IR_CALIBRATION_RESULT_INTERRUPTED
    NOT_CALIBRATED = _native.AMS_MEL_IR_CALIBRATION_RESULT_NOT_CALIBRATED


@dataclass(frozen=True)
class CompletedCalibrationItem:
    """Owned per-item name, result and complete failure reason."""

    item_name: str
    result: SubsystemCalibrationResult
    fail_reason: str


@dataclass(frozen=True)
class CompletedCalibration:
    """Owned record with exact signed nanoseconds and no implied clock epoch.

    The overall result is independent of item-level results. A record is not
    evidence of physical calibration validity beyond the published data.
    """

    calibration_id: UciId
    time_tag_ns: int
    result: SubsystemCalibrationResult
    fail_reason: str
    calibration_items: tuple[CompletedCalibrationItem, ...]


@dataclass(frozen=True)
class CalibrationStatus:
    """Owned snapshot of two independent ordered collections with duplicates."""

    active_calibration_ids: tuple[UciId, ...]
    completed_calibrations: tuple[CompletedCalibration, ...]


class CalibrationStatusOpenError(MelError):
    """Open failure with a separately published, known provider Return, if any."""

    registration: CommandReturn | None

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _check_size(size: int, field: str) -> None:
    # Only this ABI family imposes this limit on every count and string size.
    if size > 0x7FFFFFFF:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           f"CalibrationStatus {field} exceeds native INT32_MAX")


def _copy_text(raw: _native.StringViewV1, field: str) -> str:
    _check_size(raw.size, field)
    return _copy_string_view(raw, field)


def _copy_id(raw: _native.UciIdV1) -> UciId:
    _check_size(raw.descriptive_label.size, "UCI ID label")
    return _copy_uci_id(raw, "calibration UCI ID")


def _copy_result(value: int, field: str) -> SubsystemCalibrationResult:
    try:
        return SubsystemCalibrationResult(value)
    except ValueError as error:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           f"invalid CalibrationStatus {field} result: {value}") from error


def _copy_item(raw: _native.IrCompletedCalibrationItemV1) -> CompletedCalibrationItem:
    return CompletedCalibrationItem(
        _copy_text(raw.item_name, "calibration item name"),
        _copy_result(raw.result, "item"),
        _copy_text(raw.fail_reason, "calibration item failure reason"),
    )


def _copy_completed(raw: _native.IrCompletedCalibrationV1) -> CompletedCalibration:
    items = raw.calibration_items
    _check_size(items.size, "calibration items")
    return CompletedCalibration(
        _copy_id(raw.calibration_id),
        raw.time_tag_ns,  # ctypes int64_t field already yields the exact Python int.
        _copy_result(raw.result, "overall"),
        _copy_text(raw.fail_reason, "calibration failure reason"),
        _checked_span(items.data, items.size, _native.IrCompletedCalibrationItemV1,
                      _copy_item, "calibration items"),
    )


def _status_from_raw(raw: _native.IrCalibrationStatusV1) -> CalibrationStatus:
    """Copy only while the event owns the validated fixed view and its graph."""
    active, completed = raw.active_calibration_ids, raw.completed_calibrations
    _check_size(active.size, "active calibration IDs")
    _check_size(completed.size, "completed calibrations")
    return CalibrationStatus(
        _checked_span(active.data, active.size, _native.UciIdV1, _copy_id,
                      "active calibration IDs"),
        _checked_span(completed.data, completed.size, _native.IrCompletedCalibrationV1,
                      _copy_completed, "completed calibrations"),
    )


def _open_calibration_status(parent, queue_capacity: int) -> CalibrationStatusStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)  # Not a published MEL Return.
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_calibration_status_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CalibrationStatus registration is one-shot")

        # Prepare the owner and output slots before any synchronous callback.
        stream = CalibrationStatusStream._from_owner(_native.IrCalibrationStatusMetadataHandle())
        registration_pointer = ctypes.byref(registration)
        owner, output = parent._owner, stream._owner_pointer
        open_native = _native.ams_mel_ir_c2_calibration_status_open

        def register(diagnostic, required):
            capacity, required_pointer = len(diagnostic), ctypes.byref(required)
            parent._calibration_status_attempted = True
            return open_native(owner, queue_capacity, output, registration_pointer,
                               diagnostic, capacity, required_pointer)

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != CommandReturn.SUCCESS:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CalibrationStatus open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()  # One best-effort cleanup for unexpected output.
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise CalibrationStatusOpenError(
                error.kind, error.diagnostic, error.diagnostic_required,
                error.native_status, result) from error
        raise
    return stream


class CalibrationStatusStream(_NativeOwner):
    """Unique consumer of a native bounded FIFO with DROP-INCOMING overflow.

    Externally serialize receive/counters/close and registration versus parent
    C2 operations; the GIL is not sufficient. No Python parent is retained.
    Close ends consumption, not callback registration, and does not promise
    callback quiescence. C2 teardown and provider retention are native duties.
    """

    @classmethod
    def _from_owner(cls, owner):
        stream = super()._from_owner(owner)
        stream._close_function = _native.ams_mel_ir_calibration_status_close
        stream._close_attempted = False
        stream._close_error = None
        return stream

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.open_calibration_status")

    def __copy__(self):
        raise TypeError("CalibrationStatusStream is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CalibrationStatusStream is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CalibrationStatusStream cannot be pickled")

    def _require_open(self) -> None:
        if self._close_error is not None:
            raise self._close_error
        super()._require_open()

    def receive(self, timeout_ms: int = 0) -> CalibrationStatus:
        """Return a complete snapshot only after successfully releasing its event.

        Zero polls; positive timeouts are finite uint32 milliseconds. Timeout
        does not cancel provider work. Queued events drain before native
        stopped/failed state is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        close_attempted = False
        event = None
        try:
            event = _native.IrCalibrationStatusEventHandle()
            event_pointer = ctypes.byref(event)
            close_native = _native.ams_mel_ir_calibration_status_event_close

            def close_event(diagnostic, required):
                nonlocal close_attempted
                capacity, required_pointer = len(diagnostic), ctypes.byref(required)
                close_attempted = True
                return close_native(event_pointer, diagnostic, capacity, required_pointer)

            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_calibration_status_receive(
                    self._owner, timeout_ms, event_pointer, diagnostic, len(diagnostic),
                    ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationStatus receive succeeded without an event")
            view = ctypes.POINTER(_native.IrCalibrationStatusV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_calibration_status_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrCalibrationStatusV1, "CalibrationStatus event view")
            snapshot = _status_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationStatus event close did not consume its owner")
        except BaseException as error:
            # After invocation, assume the event may have been consumed even
            # if native Close failed or diagnostic validation itself raised.
            if event is not None and event.value and not close_attempted:
                try:
                    close_native(event_pointer, None, 0, None)
                except BaseException:
                    pass
            if isinstance(error, (MemoryError, OverflowError)):
                raise _local_error(ErrorKind.INTERNAL_ERROR,
                                   "unable to allocate owned CalibrationStatus reception storage") from error
            raise
        return snapshot

    def counters(self) -> C2MetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_calibration_status_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return C2MetadataCounters(raw.events_received, raw.events_dropped_queue_full,
                                  raw.malformed_or_unsupported)

    def close(self) -> None:
        """Close this consumer; cache any uncertain native consumption failure."""
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
                                   "CalibrationStatus close did not consume its owner")
        except BaseException as error:
            if self._close_attempted:
                self._close_error = error
            raise
        finally:
            if self._close_attempted:
                # Never leave an ownership-uncertain handle usable by Python.
                self._owner.value = None

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> bool:
        """Close on exit without masking the exception from the with body."""
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
