"""Opt-in C2 CalibrationConfiguration reception over the existing C facade."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span, _checked_span_address, _copy_string_view, _copy_uci_id
from ._c2_types import C2MetadataCounters
from ._types import CommandReturn, UciId
from ._validation import _validate_size_t, _validate_timeout


class CalibrationControlInterface(IntEnum):
    """Published control-interface alternatives; MaxExclusive is not data."""

    NOT_SET = 0
    CALIBRATION_COMMAND = 1
    CALIBRATION_STATE_COMMAND = 2


@dataclass(frozen=True)
class SubsystemCalibration:
    """Owned entry with three independent, ordered collections; duplicates remain."""

    calibration_id: UciId
    accepted_interface: CalibrationControlInterface
    calibration_item_names: tuple[str, ...]
    subsystem_component_ids: tuple[UciId, ...]
    capability_ids: tuple[UciId, ...]


@dataclass(frozen=True)
class CalibrationConfiguration:
    """Complete owned snapshot of the native vector, including an empty vector.

    Entries preserve order and duplicates. No command ID, timestamp, uniqueness
    or relationship between collection lengths is imposed by the contract.
    """

    entries: tuple[SubsystemCalibration, ...]


class CalibrationConfigurationOpenError(MelError):
    """Open failure with a separately published, known provider Return, if any."""

    registration: CommandReturn | None

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _check_count(size: int, field: str) -> None:
    # This native family limits every count and string byte length to INT32_MAX.
    # Keep this local: the shared FFI helpers serve other ABI count domains too.
    if size > 0x7FFFFFFF:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           f"CalibrationConfiguration {field} exceeds native count limit")


def _copy_name(raw: _native.StringViewV1) -> str:
    _check_count(raw.size, "calibration item name")
    return _copy_string_view(raw, "calibration item name")


def _copy_id(raw: _native.UciIdV1) -> UciId:
    _check_count(raw.descriptive_label.size, "UCI ID label")
    return _copy_uci_id(raw, "calibration UCI ID")


def _copy_entry(raw: _native.IrSubsystemCalibrationV1) -> SubsystemCalibration:
    try:
        interface = CalibrationControlInterface(raw.accepted_interface)
    except ValueError as error:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                           "invalid CalibrationConfiguration accepted_interface") from error
    spans = ((raw.calibration_item_names, _native.StringViewV1, "calibration item names"),
             (raw.subsystem_component_ids, _native.UciIdV1, "subsystem component IDs"),
             (raw.capability_ids, _native.UciIdV1, "capability IDs"))
    # Validate each independent span before reading any nested element.
    for span, element, field in spans:
        _check_count(span.size, field)
        _checked_span_address(span.data, span.size, element, field)
    return SubsystemCalibration(
        _copy_id(raw.calibration_id), interface,
        _checked_span(raw.calibration_item_names.data, raw.calibration_item_names.size,
                      _native.StringViewV1, _copy_name, "calibration item names"),
        _checked_span(raw.subsystem_component_ids.data, raw.subsystem_component_ids.size,
                      _native.UciIdV1, _copy_id, "subsystem component IDs"),
        _checked_span(raw.capability_ids.data, raw.capability_ids.size,
                      _native.UciIdV1, _copy_id, "capability IDs"))


def _configuration_from_raw(raw: _native.IrCalibrationConfigurationV1) -> CalibrationConfiguration:
    # Called only while the event is owned and its fixed view address validated.
    _check_count(raw.size, "entries")
    return CalibrationConfiguration(_checked_span(
        raw.data, raw.size, _native.IrSubsystemCalibrationV1, _copy_entry, "calibration entries"))


def _open_calibration_configuration(parent, queue_capacity: int) -> CalibrationConfigurationStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)  # Not a published MEL Return.
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        if getattr(parent, "_calibration_configuration_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CalibrationConfiguration registration is one-shot")
        # Own the output slot before registration can synchronously deliver.
        stream = CalibrationConfigurationStream._from_owner(_native.IrCalibrationConfigurationMetadataHandle())
        registration_pointer = ctypes.byref(registration)

        def register(diagnostic, required):
            required_pointer = ctypes.byref(required)
            parent._calibration_configuration_attempted = True
            return _native.ams_mel_ir_c2_calibration_configuration_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                registration_pointer, diagnostic, len(diagnostic), required_pointer)

        _call_with_diagnostic(register)
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "CalibrationConfiguration open needs an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()  # One attempt even for unexpected failed-call output.
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise CalibrationConfigurationOpenError(
                error.kind, error.diagnostic, error.diagnostic_required,
                error.native_status, result) from error
        raise
    return stream


class CalibrationConfigurationStream(_NativeOwner):
    """Unique consumer of a native bounded FIFO with DROP-INCOMING overflow.

    Externally serialize receive/counters/close, and registration versus parent
    C2 operations; the GIL is not sufficient. No Python parent is retained.
    Close only ends consumption: no unregister or immediate callback quiescence
    is promised. C2 teardown and provider retention are native responsibilities.
    """

    _close_function = staticmethod(_native.ams_mel_ir_calibration_configuration_close)

    def __init__(self) -> None:
        raise TypeError("use ControlChannel.open_calibration_configuration")

    def __copy__(self):
        raise TypeError("CalibrationConfigurationStream is a unique native owner")

    def __deepcopy__(self, memo):
        raise TypeError("CalibrationConfigurationStream is a unique native owner")

    def __reduce_ex__(self, protocol):
        raise TypeError("CalibrationConfigurationStream cannot be pickled")

    def _require_open(self) -> None:
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CalibrationConfiguration owner has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> CalibrationConfiguration:
        """Receive once, copy completely, release the event, then publish.

        Zero is a poll; positive timeouts are finite uint32 milliseconds.
        A timeout never cancels registration or provider work.
        Queued events drain before native stopped/failed state is reported.
        """
        self._require_open()
        _validate_timeout(timeout_ms)
        event = None
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_calibration_configuration_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            event = _native.IrCalibrationConfigurationEventHandle()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_calibration_configuration_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationConfiguration receive succeeded without an event")
            view = ctypes.POINTER(_native.IrCalibrationConfigurationV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_calibration_configuration_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrCalibrationConfigurationV1, "CalibrationConfiguration event view")
            result = _configuration_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationConfiguration event close did not consume its owner")
        except BaseException as error:
            # Preserve the primary error. Close may consume ownership even when
            # it fails or its diagnostic is malformed: never repeat an attempt.
            if event is not None and event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_calibration_configuration_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            if isinstance(error, (MemoryError, OverflowError)):
                raise _local_error(ErrorKind.INTERNAL_ERROR,
                                   "unable to allocate owned CalibrationConfiguration reception storage") from error
            raise
        return result

    def counters(self) -> C2MetadataCounters:
        """Snapshot all three uint64 callback counters without resetting them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_calibration_configuration_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return C2MetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                  int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Close only this consumer; never retry uncertain ownership consumption."""
        error = getattr(self, "_close_error", None)
        if error is not None:
            raise error
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "CalibrationConfiguration owner has uncertain closure")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        try:
            _call_with_diagnostic(close_owner)
            if self.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "CalibrationConfiguration close did not consume its owner")
        except BaseException as error:
            if getattr(self, "_close_attempted", False):
                self._close_error = error
            raise

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass
