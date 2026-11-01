"""Private instrumentation implementation; import public names from ams_mel."""

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
    _validate_size_t,
    _validate_timeout,
)
from ._types import (
    ChannelCapability,
    InstrumentationCompleted,
    InstrumentationLevelCommand,
    InstrumentationMetadataCounters,
    InstrumentationPriority,
    InstrumentationRejected,
    InstrumentationReport,
    MelErrorCode,
)
from ._ffi import (
    _channel_capabilities,
)
from ._common import (
    ChannelView,
    _NativeOwner,
    _open_channel_view,
)


_WAIT_DIAGNOSTIC_CAPACITY = 512


class InstrumentationChannel(_NativeOwner):
    """Independent typed owner. Externally serialize enable/submit/close."""

    _close_function = staticmethod(_native.ams_mel_ir_instrumentation_close)

    def __init__(self) -> None:
        raise TypeError("use Session.open_instrumentation_channel")

    def enable(self) -> None:
        """Explicit enable; native caches success and refuses failed-state commands."""
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_instrumentation_enable(
                self._owner, diagnostic, len(diagnostic), _ctypes.byref(required),
            )
        )

    def capabilities(self) -> ChannelCapability:
        """Complete owned snapshot, available while Attached or Enabled."""
        self._require_open()
        return _channel_capabilities(
            self._owner, _native.ams_mel_ir_instrumentation_get_capabilities,
        )

    def channel_view(self) -> ChannelView:
        """Return a weak common view; no Python or native parent is retained."""
        self._require_open()
        return _open_channel_view(
            _native.ams_mel_ir_channel_from_instrumentation, self._owner,
        )

    def open_metadata(self, queue_capacity: int = 16) -> InstrumentationMetadata:
        """Register once while Attached or Enabled, without implicitly enabling.

        Failed native attempts also consume registration; closing the metadata
        owner never permits re-registration. Callbacks stay entirely in native
        code and may run synchronously during this call.
        """
        self._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        metadata = InstrumentationMetadata._from_owner(
            _native.IrInstrumentationMetadataHandle(),
        )
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_instrumentation_metadata_open(
                    self._owner, queue_capacity, metadata._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not metadata.is_open:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Instrumentation metadata open succeeded without returning an owner",
                )
        except BaseException:
            metadata.__del__()
            raise
        return metadata

    def submit_level(self, command: InstrumentationLevelCommand) -> InstrumentationRequest:
        """Submit only; successful send is not recording-level command completion."""
        self._require_open()
        if not isinstance(command, InstrumentationLevelCommand):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "command must be InstrumentationLevelCommand",
            )
        InstrumentationLevelCommand.__post_init__(command)
        raw = _native.IrInstrumentationLevelCommandV1(command.command_id, command.priority)
        request = InstrumentationRequest._from_owner(_native.IrInstrumentationRequestHandle())
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_instrumentation_submit_level(
                    self._owner, _ctypes.byref(raw), request._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not request.is_open:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Instrumentation submission succeeded without returning a request",
                )
        except BaseException:
            request.__del__()
            raise
        return request


class InstrumentationMetadata(_NativeOwner):
    """Independent polling owner; externally serialize receive/counters/close.

    Native channel-owned callbacks feed a bounded FIFO DROP-INCOMING queue.
    Queued reports drain after channel close or callback failure, followed by
    STREAM_STOPPED or PROVIDER_FAILED respectively. No Python parent is retained.
    """

    _close_function = staticmethod(_native.ams_mel_ir_instrumentation_metadata_close)

    def __init__(self) -> None:
        raise TypeError("use InstrumentationChannel.open_metadata")

    def receive(self, timeout_ms: int = 0) -> InstrumentationReport:
        """Copy a complete report before releasing its native event; zero polls."""
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrInstrumentationMetadataEventHandle()
        copied = False
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_instrumentation_metadata_receive(
                    self._owner, timeout_ms, _ctypes.byref(event),
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not event.value:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Instrumentation receive succeeded without returning an event",
                )
            record = _ctypes.POINTER(_native.IrInstrumentationMetadataEventV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_instrumentation_metadata_event_view(
                    event, _ctypes.byref(record),
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not record:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Instrumentation event view succeeded without returning a record",
                )
            raw = record.contents
            if raw.kind != _native.AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY, "invalid Instrumentation event kind",
                )
            report = raw.report
            if report.priority not in (0, 1):
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY, "invalid Instrumentation report priority",
                )
            result = InstrumentationReport(
                int(report.command_id), int(report.size), int(report.timestamp_ns),
                InstrumentationPriority(report.priority),
            )
            copied = True
            return result
        finally:
            if event.value:
                # Also release an unexpected event published on a failed call.
                # Cleanup must never replace a receive/view/conversion exception.
                try:
                    # Event close has no diagnostic payload. Do not allocate a
                    # diagnostic buffer before releasing, especially on OOM.
                    status = int(_native.ams_mel_ir_instrumentation_metadata_event_close(
                        _ctypes.byref(event), None, 0, None,
                    ))
                    if status != _native.AMS_MEL_OK:
                        raise _error_from_status(status, None, None)
                    if event.value:
                        raise _local_error(
                            ErrorKind.PROTOCOL_INCONSISTENCY,
                            "native event close succeeded without consuming the owner",
                        )
                except BaseException:
                    if copied:
                        raise

    def counters(self) -> InstrumentationMetadataCounters:
        """Owned uint64 snapshot; reading reports never resets these counters."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_instrumentation_metadata_get_counters(
                self._owner, _ctypes.byref(raw),
                diagnostic, len(diagnostic), _ctypes.byref(required),
            )
        )
        return InstrumentationMetadataCounters(
            int(raw.events_received), int(raw.events_dropped_queue_full),
            int(raw.malformed_or_unsupported),
        )

    def close(self) -> None:
        """Deactivate consumption only; no unregister, cancellation, or quiescence."""
        super().close()


class InstrumentationRequest(_NativeOwner):
    """Independent request. Close is not cancellation and must not race Wait."""

    _close_function = staticmethod(_native.ams_mel_ir_instrumentation_request_close)

    def __init__(self) -> None:
        raise TypeError("use InstrumentationChannel.submit_level")

    def wait(self, timeout_ms: int) -> InstrumentationCompleted | InstrumentationRejected:
        """Zero polls; timeout preserves the owner and native terminal results are cached."""
        self._require_open()
        _validate_timeout(timeout_ms)
        return _wait_for_instrumentation(self._owner, timeout_ms)


def _wait_for_instrumentation(
    owner: _native.IrInstrumentationRequestHandle, timeout_ms: int,
) -> InstrumentationCompleted | InstrumentationRejected:
    result = _native.IrInstrumentationResultV1()
    _ctypes.memset(_ctypes.byref(result), 0xFF, _ctypes.sizeof(result))
    diagnostic = _ctypes.create_string_buffer(_WAIT_DIAGNOSTIC_CAPACITY)
    _ctypes.memset(diagnostic, 0xFF, len(diagnostic))
    required = _ctypes.c_size_t()
    status = int(_native.ams_mel_ir_instrumentation_request_wait(
        owner, timeout_ms, _ctypes.byref(result), diagnostic,
        len(diagnostic), _ctypes.byref(required),
    ))
    size = int(required.value)
    if size == 0:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY, "invalid Instrumentation diagnostic size",
        )
    if status == _native.AMS_MEL_COMMAND_REJECTED and result.error_code > 8:
        raise _local_error(
            ErrorKind.PROTOCOL_INCONSISTENCY, "unknown Instrumentation MEL error code",
        )
    if size <= len(diagnostic):
        message = _decode_complete_buffer(bytes(diagnostic.raw[:size]), "diagnostic")
    elif status == _native.AMS_MEL_COMMAND_REJECTED:
        # Only this known terminal status permits a cached Wait(0) diagnostic
        # retrieval. Never repeat send, enable, or another side-effecting call.
        try:
            complete = _ctypes.create_string_buffer(size)
            _ctypes.memset(complete, 0xFF, size)
        except (MemoryError, OverflowError, ValueError) as error:
            raise _local_error(
                ErrorKind.INTERNAL_ERROR, "unable to allocate Instrumentation diagnostic",
            ) from error
        retry = _native.IrInstrumentationResultV1()
        _ctypes.memset(_ctypes.byref(retry), 0xFF, _ctypes.sizeof(retry))
        retry_required = _ctypes.c_size_t()
        retry_status = int(_native.ams_mel_ir_instrumentation_request_wait(
            owner, 0, _ctypes.byref(retry), complete, size, _ctypes.byref(retry_required),
        ))

        def identity(raw: _native.IrInstrumentationResultV1) -> tuple[int, ...]:
            report = raw.report
            return (report.command_id, report.size, report.timestamp_ns,
                    report.priority, raw.error_code)

        if (retry_status != status or retry_required.value != size
                or identity(retry) != identity(result)):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native Instrumentation rejection changed during diagnostic retry",
            )
        message = _decode_complete_buffer(bytes(complete.raw), "diagnostic")
    else:
        message = None

    if status == _native.AMS_MEL_OK:
        if size != 1 or message or result.error_code != _native.AMS_MEL_ERROR_NONE:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "successful Instrumentation wait returned diagnostic or error code",
            )
        report = result.report
        if report.priority not in (0, 1):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY, "invalid Instrumentation report priority",
            )
        return InstrumentationCompleted(InstrumentationReport(
            int(report.command_id), int(report.size), int(report.timestamp_ns),
            InstrumentationPriority(report.priority),
        ))
    if status == _native.AMS_MEL_COMMAND_REJECTED:
        return InstrumentationRejected(MelErrorCode(result.error_code), message)
    raise _error_from_status(status, message or None, size)
