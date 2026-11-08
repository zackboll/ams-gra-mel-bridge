"""Shared opt-in NUC reception over the seven published native functions."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from enum import IntEnum

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, MelError, _call_with_diagnostic, _local_error
from ._ffi import _checked_span, _checked_span_address, _copy_band_info
from ._types import BandInfo, BandType, CommandReturn
from ._validation import _validate_size_t, _validate_timeout


class NucPointType(IntEnum):
    NUC_POINT = 0
    MFA_ESTIMATE = 1
    OTHER = 2


class NucPixelKind(IntEnum):
    UINT8 = 1
    UINT16 = 2
    UINT32 = 3


@dataclass(frozen=True)
class NucPixelValue:
    """Original unsigned alternative; never infer width from magnitude."""

    kind: NucPixelKind
    value: int

    def __post_init__(self) -> None:
        if not isinstance(self.kind, NucPixelKind):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "kind must be NucPixelKind")
        maximum = {NucPixelKind.UINT8: 255, NucPixelKind.UINT16: 65535,
                   NucPixelKind.UINT32: 4294967295}[self.kind]
        if (isinstance(self.value, bool) or not isinstance(self.value, int)
                or not 0 <= self.value <= maximum):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "pixel value exceeds its tagged width")


@dataclass(frozen=True)
class NucTempPoint:
    """Public TempPoint fields; timestamp epoch unspecified, temperature Kelvin.

    The upstream private optional tempError has no public accessor and is not
    represented or inferred. Native floating-point values are not normalized.
    """

    timestamp_ns: int
    frame_band: BandInfo
    pixel: NucPixelValue
    temperature_k: float
    point_type: NucPointType


@dataclass(frozen=True)
class NucTempData:
    """Complete owned ordered points, including duplicates and empty messages."""

    points: tuple[NucTempPoint, ...]

    def __post_init__(self) -> None:
        if not isinstance(self.points, tuple) or not all(
                isinstance(point, NucTempPoint) for point in self.points):
            raise TypeError("points must be a tuple of NucTempPoint values")


@dataclass(frozen=True)
class NucCounters:
    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int


class NucOpenError(MelError):
    """Optional registration failure with an independent provider Return."""

    def __init__(self, kind: ErrorKind, diagnostic: str | None,
                 diagnostic_required: int | None = None, native_status: int | None = None,
                 registration: CommandReturn | None = None) -> None:
        super().__init__(kind, diagnostic, diagnostic_required, native_status)
        self.registration = registration


def _point_from_raw(raw: _native.IrTempPointV1) -> NucTempPoint:
    # BandType intentionally admits future values for capabilities, but NUC
    # must accept only the published domain, independent of prior enum use.
    if int(raw.frame_band.kind) not in {int(band) for band in BandType}:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "unpublished NUC band type")
    try:
        kind = NucPixelKind(int(raw.pixel_value.kind))
        point_type = NucPointType(int(raw.point_type))
    except ValueError:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "unpublished NUC pixel/point type") from None
    try:
        pixel = NucPixelValue(kind, int(raw.pixel_value.value))
    except MelError:
        raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "NUC pixel exceeds its tagged width") from None
    return NucTempPoint(int(raw.timestamp_ns), _copy_band_info(raw.frame_band), pixel,
                        float(raw.temperature_k), point_type)


def _open_nuc(parent, queue_capacity: int, native_open) -> NucStream:
    registration = ctypes.c_uint32(0xFFFFFFFF)
    stream = None
    try:
        parent._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        # Allocate the Python owner before registration can synchronously deliver.
        stream = NucStream._from_owner(_native.IrNucMetadataHandle())
        _call_with_diagnostic(
            lambda diagnostic, required: native_open(
                parent._owner, queue_capacity, stream._owner_pointer,
                ctypes.byref(registration), diagnostic, len(diagnostic), ctypes.byref(required)))
        if not stream.is_open or registration.value != int(CommandReturn.SUCCESS):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "NUC open succeeded without an owner and SUCCESS registration")
    except BaseException as error:
        if stream is not None:
            stream.__del__()
        if isinstance(error, MelError):
            try:
                result = CommandReturn(registration.value)
            except ValueError:
                result = None
            raise NucOpenError(error.kind, error.diagnostic, error.diagnostic_required,
                               error.native_status, result) from error
        raise
    return stream


class NucStream(_NativeOwner):
    """Independent native bounded FIFO, DROP-INCOMING; no Python parent retained.

    Externally serialize receive, counters and close; the GIL is not a native
    ownership lock. Close deactivates consumption, not callback registration,
    parent channels, requests or producer quiescence. Native state retains the
    provider/library as needed. Queued events drain after Image stop or Health
    teardown before STREAM_STOPPED (or PROVIDER_FAILED on uncertain teardown).
    """

    _close_function = staticmethod(_native.ams_mel_ir_nuc_close)

    def __init__(self) -> None:
        raise TypeError("use ImageStream.open_nuc or HealthStatusChannel.open_nuc")

    def receive(self, timeout_ms: int = 0) -> NucTempData:
        """Receive once and copy all points before releasing the native event."""
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrNucEventHandle()
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_nuc_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_nuc_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "NUC receive succeeded without an event")
            view = ctypes.POINTER(_native.IrNucTempDataEventV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_nuc_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrNucTempDataEventV1, "NUC event view")
            span = view.contents.temp_points
            result = NucTempData(_checked_span(span.data, span.size, _native.IrTempPointV1,
                                             _point_from_raw, "NUC points"))
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "NUC event close did not consume its owner")
        except BaseException:
            if event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_nuc_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            raise
        return result

    def counters(self) -> NucCounters:
        """Full uint64 callback counts, never reset by receive.

        Callback allocation/copy failures reject that event and increment
        malformed_or_unsupported; they do not put the NUC stream into Failed.
        """
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_nuc_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return NucCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                           int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Idempotently close; never retry an uncertain native Close attempt."""
        if not self.is_open or getattr(self, "_close_attempted", False):
            return

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        _call_with_diagnostic(close_owner)
        if self.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "NUC close did not consume its owner")

    def _require_open(self) -> None:
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "NUC owner has uncertain closure")

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass
