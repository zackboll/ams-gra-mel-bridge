"""Private health implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _local_error,
)
from ._health_metadata import HealthMetadataStream
from ._validation import _validate_size_t
from ._types import (
    ChannelCapability,
)
from ._ffi import (
    _channel_capabilities,
)
from ._common import (
    ChannelView,
    _NativeOwner,
    _open_channel_view,
)


class HealthStatusChannel(_NativeOwner):
    """Independent HealthAndStatus owner, initially Attached, not Enabled.

    Externally serialize enable, metadata open, capabilities, view conversion,
    and close. Native state implements idempotent enable and failed-state rejection. Close may
    defer teardown for common requests; it never cancels them. A failed detach
    retains the handle for explicit retry; consumed owners are never retried.
    Six-family metadata registration is opt-in; NUC reception is not exposed.
    """

    _close_function = staticmethod(_native.ams_mel_ir_health_close)

    def __init__(self) -> None:
        raise TypeError("use Session.open_health_channel")

    def enable(self) -> None:
        """Enable explicitly; repeated success does not reach the provider again."""
        self._require_open()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_health_enable(
                self._owner, diagnostic, len(diagnostic), _ctypes.byref(required),
            )
        )

    def capabilities(self) -> ChannelCapability:
        """Complete Python-owned snapshot, available while Attached or Enabled."""
        self._require_open()
        return _channel_capabilities(
            self._owner, _native.ams_mel_ir_health_get_capabilities,
        )

    def channel_view(self) -> ChannelView:
        """Weak inherited services; retains neither this owner nor its Session."""
        self._require_open()
        return _open_channel_view(_native.ams_mel_ir_channel_from_health, self._owner)

    def open_metadata(self, queue_capacity: int = 16) -> HealthMetadataStream:
        """Register all six callbacks once, while Attached or Enabled.

        Does not enable Health or register NUC. Registration may invoke native
        callbacks synchronously. Failed/partial attempts consume registration;
        closing metadata never permits another attempt on this channel.
        """
        self._require_open()
        _validate_size_t(queue_capacity, "queue_capacity")
        if queue_capacity == 0:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "queue_capacity must be positive")
        metadata = HealthMetadataStream._from_owner(_native.IrHealthMetadataHandle())
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_health_metadata_open(
                    self._owner, queue_capacity, metadata._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required)))
            if not metadata.is_open:
                raise _local_error(ErrorKind.PROTOCOL_INCONSISTENCY,
                                   "Health metadata open succeeded without an owner")
        except BaseException:
            metadata.__del__()
            raise
        return metadata

    def __del__(self) -> None:
        # Exception-path cleanup and automatic finalization must not silently
        # retry an uncertain retained handle. Explicit close remains retryable.
        try:
            if getattr(self, "_finalized", False):
                return
            self._finalized = True
            super().__del__()
        except BaseException:
            pass
