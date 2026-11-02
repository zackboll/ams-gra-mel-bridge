"""Private health implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes

from . import _native
from ._errors import (
    _call_with_diagnostic,
)
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

    Externally serialize enable, capabilities, view conversion, and close. Native
    state implements idempotent enable and failed-state rejection. Close may
    defer teardown for common requests; it never cancels them. A failed detach
    retains the handle for explicit retry; consumed owners are never retried.
    No Health metadata or NUC reception is exposed.
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
