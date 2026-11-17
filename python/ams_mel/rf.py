"""RF MEL Admin owner with its independent provider factory.

RFAdmin supports explicit state-command acceptance and shutdown. It has no IR
Session parent. Import MFAState, MelError and ErrorKind from ams_mel.common.
RF data, product reception, virtual apertures and job APIs are not currently
exposed by the safe Python binding.

Importing this namespace requires AMS_MEL_NATIVE_LIB and loads the native
facade through the parent package. It does not invoke an RF factory or request
a state. RFAdmin is the same class available from ams_mel, with unchanged
ownership, serialization, introspection and pickle behavior."""

from ._rf_admin import (
    RFAdmin,
)


__all__ = [
    "RFAdmin",
]
