"""Independent ordinary Image metadata owner over six existing C exports."""

from __future__ import annotations

import ctypes

from . import _native
from ._common import _NativeOwner
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._ffi import _checked_span_address
from ._image_convert import _event_from_raw
from ._image_types import ImageMetadataCounters, ImageMetadataEvent
from ._validation import _validate_size_t, _validate_timeout


def _open_metadata(parent, queue_capacity: int) -> ImageMetadataStream:
    parent._require_open()
    _validate_size_t(queue_capacity, "queue_capacity")
    if queue_capacity == 0:
        raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
    # Allocate the Python owner before registration can synchronously deliver.
    stream = ImageMetadataStream._from_owner(_native.IrImageMetadataHandle())
    try:
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_image_metadata_open(
                parent._owner, queue_capacity, stream._owner_pointer, diagnostic,
                len(diagnostic), ctypes.byref(required)))
        if not stream.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Image metadata open succeeded without an owner")
    except BaseException:
        stream.__del__()
        raise
    return stream


class ImageMetadataStream(_NativeOwner):
    """Native bounded DROP-INCOMING queue; no Python parent is retained.

    Externally serialize receive/counters/close, and registration versus parent
    Start/Stop/Close. Close ends consumption, not Image or callback registration,
    and promises no callback quiescence. Native code retains provider resources.
    """

    _close_function = staticmethod(_native.ams_mel_ir_image_metadata_close)

    def __init__(self) -> None:
        raise TypeError("use ImageStream.open_metadata")

    def _require_open(self) -> None:
        super()._require_open()
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.INVALID_ARGUMENT,
                               "Image metadata owner has uncertain closure")

    def receive(self, timeout_ms: int = 0) -> ImageMetadataEvent:
        """Receive exactly once, deep-copy the active payload, then release it."""
        self._require_open()
        _validate_timeout(timeout_ms)
        event = _native.IrImageMetadataEventHandle()
        close_attempted = False

        def close_event(diagnostic, required):
            nonlocal close_attempted
            close_attempted = True
            return _native.ams_mel_ir_image_metadata_event_close(
                ctypes.byref(event), diagnostic, len(diagnostic), ctypes.byref(required))

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_metadata_receive(
                    self._owner, timeout_ms, ctypes.byref(event), diagnostic,
                    len(diagnostic), ctypes.byref(required)))
            if not event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Image metadata receive succeeded without an event")
            view = ctypes.POINTER(_native.IrImageMetadataEventV1)()
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_image_metadata_event_view(
                    event, ctypes.byref(view), diagnostic, len(diagnostic), ctypes.byref(required)))
            _checked_span_address(view, 1, _native.IrImageMetadataEventV1, "Image event view")
            if ctypes.cast(view, ctypes.c_void_p).value > (
                    (1 << (8 * ctypes.sizeof(ctypes.c_void_p))) -
                    ctypes.sizeof(_native.IrImageMetadataEventV1)):
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY, "Image event view address overflow")
            result = _event_from_raw(view.contents)
            _call_with_diagnostic(close_event)
            if event.value:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Image event close did not consume its owner")
        except BaseException:
            # Preserve the first error, including MemoryError/KeyboardInterrupt.
            # Close may have consumed the handle even when it reports failure.
            if event.value and not close_attempted:
                try:
                    _native.ams_mel_ir_image_metadata_event_close(ctypes.byref(event), None, 0, None)
                except BaseException:
                    pass
            raise
        return result

    def counters(self) -> ImageMetadataCounters:
        """Full uint64 callback counters; receiving does not reset them."""
        self._require_open()
        raw = _native.IrC2MetadataCountersV1()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_image_metadata_get_counters(
                self._owner, ctypes.byref(raw), diagnostic, len(diagnostic), ctypes.byref(required)))
        return ImageMetadataCounters(int(raw.events_received), int(raw.events_dropped_queue_full),
                                     int(raw.malformed_or_unsupported))

    def close(self) -> None:
        """Idempotent after release; never retry a possibly consumed owner."""
        if not self.is_open:
            return
        if getattr(self, "_close_attempted", False):
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Image metadata ownership remains unresolved after Close")

        def close_owner(diagnostic, required):
            self._close_attempted = True
            return self._close_function(self._owner_pointer, diagnostic, len(diagnostic),
                                        ctypes.byref(required))

        _call_with_diagnostic(close_owner)
        if self.is_open:
            raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                               "Image metadata close did not consume its owner")

    def __del__(self) -> None:
        try:
            if getattr(self, "_close_attempted", False):
                return
            self._close_attempted = True
            super().__del__()
        except BaseException:
            pass
