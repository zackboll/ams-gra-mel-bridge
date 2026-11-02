"""Private rf admin implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
import os

from . import _native
from ._errors import (
    ErrorKind,
    _call_with_diagnostic,
    _local_error,
)
from ._validation import (
    _UINT32_MAX,
    _utf8_input,
    _validate_uint32,
)
from ._types import (
    MFAState,
)


class RFAdmin:
    """Independent AdminMEL owner; externally serialize all owner operations.

    This is not an IR Session and retains no Python parent. Native code owns
    provider/library retention, including permanent retention if shutdown throws.
    """

    def __init__(self) -> None:
        raise TypeError("RFAdmin objects must be created with RFAdmin.open")

    @classmethod
    def open(
        cls,
        provider_library: str | os.PathLike[str],
        configuration: str,
    ) -> RFAdmin:
        """Invoke createAdminMEL once with provider-defined UTF-8 configuration."""

        try:
            path = os.fspath(provider_library)
        except TypeError as error:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "provider_library must be str or return str from os.fspath",
            ) from error
        library_bytes = _utf8_input(path, "provider_library")
        configuration_bytes = _utf8_input(configuration, "configuration")

        # Prepare the complete Python owner before native ownership can publish.
        admin = cls.__new__(cls)
        admin._owner = _native.RfAdminHandle()
        admin._owner_pointer = _ctypes.pointer(admin._owner)
        admin._close_function = _native.ams_mel_rf_admin_close
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_rf_admin_open(
                    library_bytes,
                    configuration_bytes,
                    admin._owner_pointer,
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
            if not admin.is_open:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "RF Admin open succeeded without returning an owner",
                )
        except BaseException:
            # Includes diagnostic decoding failures after native publication.
            # Cleanup must not replace the original exception.
            admin.__del__()
            raise
        return admin

    @property
    def is_open(self) -> bool:
        """Inspect the local public handle, not provider health or hardware state."""

        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "RFAdmin is closed")

    def command_state(self, state: MFAState | int) -> bool:
        """Return provider acceptance, never proof of a hardware state transition.

        Accept MFAState or an integer in 0..14 (not bool). False is a normal
        rejection; missing interfaces, native failures and malformed results raise
        MelError. No transition table, polling, retries or implicit enable exists.
        """

        self._require_open()
        _validate_uint32(state, "state")
        if state > int(MFAState.DEGRADED):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "state must be in 0..14")
        accepted = _ctypes.c_uint32(_UINT32_MAX)
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_rf_admin_command_state(
                self._owner,
                int(state),
                _ctypes.byref(accepted),
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )
        if accepted.value not in (0, 1):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY, "invalid native RF Admin boolean"
            )
        return accepted.value == 1

    def close(self) -> None:
        """Consume the handle and shut down once, even when shutdown fails.

        Repeated close is harmless and never retries an uncertain shutdown.
        Explicit close reports failures; finalization suppresses them.
        """

        if not self.is_open:
            return
        _call_with_diagnostic(
            lambda diagnostic, required: self._close_function(
                self._owner_pointer,
                diagnostic,
                len(diagnostic),
                _ctypes.byref(required),
            )
        )

    def __enter__(self) -> RFAdmin:
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
            close_function = getattr(self, "_close_function", None)
            owner_pointer = getattr(self, "_owner_pointer", None)
            if owner is not None and owner.value and close_function is not None:
                close_function(owner_pointer, None, 0, None)
        except BaseException:
            pass
