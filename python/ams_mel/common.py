"""Shared MEL values, errors and native facade version query.

Use these values with both IR and RF APIs where their signatures require them.
MFAState is the same shared enum used by IR state/health values and RF Admin.
IR channel services and their CommandReturn/MelErrorCode results live in
ams_mel.ir.

Importing this namespace has the same AMS_MEL_NATIVE_LIB requirement as
importing ams_mel: the parent package loads the native facade. It creates no
Session, channel or RF Admin owner. abi_version() queries that loaded facade.

Exports are the existing ams_mel objects. Their canonical introspection and
pickle module remains ams_mel for compatibility with existing applications."""

from ._errors import (
    ErrorKind,
    MelError,
)

from ._types import (
    AbiVersion,
    ProviderVersion,
    UciId,
    ComponentLocation,
    MFAState,
)

from ._session import (
    abi_version,
)


__all__ = [
    "AbiVersion",
    "ProviderVersion",
    "UciId",
    "ComponentLocation",
    "ErrorKind",
    "MelError",
    "MFAState",
    "abi_version",
]
