"""Private, synchronous Control discovery; no provider channel is attached."""

from __future__ import annotations

import ctypes
import sys
from contextlib import contextmanager
from typing import Any, Iterator

from . import _ffi, _native
from ._errors import ErrorKind, _call_with_diagnostic, _local_error
from ._types import ChannelCapability


@contextmanager
def _temporary_owner(owner: Any, close: Any) -> Iterator[Any]:
    """One lexical owner, one Close attempt, no finalizer or retry.

    Start guarding before acquisition: even an output published with a failing
    status must be closed. A failing Close may have consumed ownership without
    clearing the slot, so neither that slot nor its value is ever used again.
    Cleanup errors must not replace an exception from acquisition or copying.
    """

    failed = False
    try:
        yield owner
    except BaseException:
        failed = True
        raise
    finally:
        if owner.value:
            attempted = False

            def release(diagnostic: Any, required: Any) -> int:
                nonlocal attempted
                attempted = True
                return close(
                    ctypes.byref(owner), diagnostic, len(diagnostic),
                    ctypes.byref(required),
                )

            try:
                _call_with_diagnostic(release)
                if owner.value:
                    raise _local_error(
                        ErrorKind.PROTOCOL_INCONSISTENCY,
                        "capability Close succeeded without clearing its owner",
                    )
            except BaseException:
                # Diagnostic storage allocation can fail before native Close.
                # Only in that case is a diagnostic-free attempt still safe.
                if not attempted:
                    try:
                        close(ctypes.byref(owner), None, 0, None)
                    except BaseException:
                        pass
                if not failed:
                    raise


def _capabilities(session: _native.SessionHandle) -> tuple[ChannelCapability, ...]:
    with _temporary_owner(
        _native.IrCapabilityListHandle(), _native.ams_mel_ir_capability_list_close
    ) as listing:
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_capability_list_create(
                session, ctypes.byref(listing), diagnostic, len(diagnostic),
                ctypes.byref(required),
            )
        )
        if not listing.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "capability list creation succeeded without returning an owner",
            )
        count = ctypes.c_size_t()
        _call_with_diagnostic(
            lambda diagnostic, required: _native.ams_mel_ir_capability_list_count(
                listing, ctypes.byref(count), diagnostic, len(diagnostic),
                ctypes.byref(required),
            )
        )
        if count.value > sys.maxsize // ctypes.sizeof(ctypes.py_object):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native capability count is not representable as a Python tuple",
            )
        values = []
        for index in range(count.value):
            with _temporary_owner(
                _native.IrChannelCapabilityHandle(),
                _native.ams_mel_ir_channel_capability_close,
            ) as entry:
                _call_with_diagnostic(
                    lambda diagnostic, required: _native.ams_mel_ir_capability_list_get(
                        listing, index, ctypes.byref(entry), diagnostic,
                        len(diagnostic), ctypes.byref(required),
                    )
                )
                if not entry.value:
                    raise _local_error(
                        ErrorKind.PROTOCOL_INCONSISTENCY,
                        "capability list get succeeded without returning an owner",
                    )
                record = ctypes.POINTER(_native.IrChannelCapabilityV1)()
                _call_with_diagnostic(
                    lambda diagnostic, required: _native.ams_mel_ir_channel_capability_view(
                        entry, ctypes.byref(record), diagnostic, len(diagnostic),
                        ctypes.byref(required),
                    )
                )
                if not record:
                    raise _local_error(
                        ErrorKind.PROTOCOL_INCONSISTENCY,
                        "capability view succeeded without returning a record",
                    )
                _ffi._checked_span_address(
                    record, 1, _native.IrChannelCapabilityV1, "capability view"
                )
                values.append(_ffi._capability_from_raw(record.contents))
                del record
        return tuple(values)
