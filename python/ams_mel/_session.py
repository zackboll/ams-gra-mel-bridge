"""Private session implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
import os

from . import _native
from ._errors import (
    ErrorKind,
    MelError,
    _call_with_diagnostic,
    _decode_complete_buffer,
    _error_from_status,
    _local_error,
)
from ._validation import (
    _utf8_input,
    _validate_uint32,
)
from ._types import (
    AbiVersion,
    ControlConfig,
    HealthConfig,
    ImageConfig,
    InstrumentationConfig,
    ProviderVersion,
)
from ._ffi import (
    _RawControlConfig,
    _RawHealthConfig,
    _RawImageConfig,
    _RawInstrumentationConfig,
)
from ._image import (
    ImageStream,
)
from ._c2 import (
    ControlChannel,
)
from ._instrumentation import (
    InstrumentationChannel,
)
from ._health import (
    HealthStatusChannel,
)


def abi_version() -> AbiVersion:
    """Return the version of the C façade without loading a provider."""

    raw = _native.AbiVersionV1()
    status = int(_native.ams_mel_get_abi_version(_ctypes.byref(raw)))
    if status != _native.AMS_MEL_OK:
        raise _error_from_status(status, None, None)
    return AbiVersion(int(raw.major), int(raw.minor))


class Session:
    """Unique owner of one native provider session."""

    def __init__(self) -> None:
        raise TypeError("Session objects must be created with Session.open")

    @classmethod
    def _from_owner(cls, owner: _native.SessionHandle) -> Session:
        session = cls.__new__(cls)
        session._owner = owner
        session._owner_pointer = _ctypes.pointer(owner)
        session._close_function = _native.ams_mel_session_close
        return session

    @classmethod
    def open(
        cls,
        provider_library: str | os.PathLike[str],
        instance: str,
        aperture: str,
        *,
        max_async_requests: int = 0,
    ) -> Session:
        """Open a session; zero admission limit is unlimited, with no queue/retry."""

        _validate_uint32(max_async_requests, "max_async_requests")
        options = _native.SessionOptionsV1(max_async_requests)

        try:
            path = os.fspath(provider_library)
        except TypeError as error:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "provider_library must be str or return str from os.fspath",
            ) from error
        if not isinstance(path, str):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "provider_library must be str; bytes paths are not accepted",
            )
        library_bytes = _utf8_input(path, "provider_library")
        instance_bytes = _utf8_input(instance, "instance")
        aperture_bytes = _utf8_input(aperture, "aperture")
        owner = _native.SessionHandle()

        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_session_open_with_options(
                    library_bytes,
                    instance_bytes,
                    aperture_bytes,
                    _ctypes.byref(options),
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_session_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "session open succeeded without returning an owner",
            )
        return cls._from_owner(owner)

    @property
    def is_open(self) -> bool:
        """Whether this object still owns a native session."""

        return bool(self._owner.value)

    def _require_open(self) -> None:
        if not self.is_open:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "Session is closed")

    def provider_version(self) -> ProviderVersion:
        """Return the provider's complete version information."""

        self._require_open()
        raw = _native.ProviderVersionV1()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: (
                    _native.ams_mel_session_get_provider_version(
                        self._owner,
                        _ctypes.byref(raw),
                        diagnostic,
                        len(diagnostic),
                        _ctypes.byref(required),
                    )
                )
            )
        except MelError as error:
            if error.kind is not ErrorKind.BUFFER_TOO_SMALL:
                raise
        else:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "provider version size query unexpectedly succeeded",
            )

        vendor_required = int(raw.vendor_required)
        description_required = int(raw.description_required)
        if vendor_required == 0 or description_required == 0:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "native provider returned a zero version string size",
            )
        vendor = _ctypes.create_string_buffer(vendor_required)
        description = _ctypes.create_string_buffer(description_required)
        _ctypes.memset(vendor, 0xFF, vendor_required)
        _ctypes.memset(description, 0xFF, description_required)
        raw.vendor = _ctypes.cast(vendor, _native.CharPointer)
        raw.vendor_capacity = vendor_required
        raw.description = _ctypes.cast(description, _native.CharPointer)
        raw.description_capacity = description_required

        _call_with_diagnostic(
            lambda diagnostic, required: (
                _native.ams_mel_session_get_provider_version(
                    self._owner,
                    _ctypes.byref(raw),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        )
        if (
            int(raw.vendor_required) != vendor_required
            or int(raw.description_required) != description_required
        ):
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "provider version sizes changed between query and copy",
            )
        return ProviderVersion(
            int(raw.api_version),
            int(raw.library_version),
            _decode_complete_buffer(bytes(vendor.raw), "vendor"),
            _decode_complete_buffer(bytes(description.raw), "description"),
        )

    def open_image_stream(self, config: ImageConfig) -> ImageStream:
        """Open a host-memory IRSTImage Mono8 stream without retaining this object."""

        self._require_open()
        if not isinstance(config, ImageConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be ImageConfig")
        bundle = _RawImageConfig(config)
        owner = _native.IrStreamHandle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_stream_open(
                    self._owner,
                    _ctypes.byref(bundle.raw),
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_stream_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "stream open succeeded without returning an owner",
            )
        return ImageStream._from_owner(owner)

    def open_instrumentation_channel(
        self, config: InstrumentationConfig,
    ) -> InstrumentationChannel:
        """Attach without enabling; the child does not retain this Python Session."""

        self._require_open()
        if not isinstance(config, InstrumentationConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be InstrumentationConfig")
        InstrumentationConfig.__post_init__(config)
        bundle = _RawInstrumentationConfig(config)
        channel = InstrumentationChannel._from_owner(_native.IrInstrumentationHandle())
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_instrumentation_open(
                    self._owner, _ctypes.byref(bundle.raw), channel._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not channel.is_open:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Instrumentation open succeeded without returning an owner",
                )
        except BaseException:
            channel.__del__()
            raise
        return channel

    def open_health_channel(self, config: HealthConfig) -> HealthStatusChannel:
        """Attach without enabling; native state, not a Python parent, owns lifetime."""

        self._require_open()
        if not isinstance(config, HealthConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be HealthConfig")
        HealthConfig.__post_init__(config)
        bundle = _RawHealthConfig(config)
        channel = HealthStatusChannel._from_owner(_native.IrHealthHandle())
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_health_open(
                    self._owner, _ctypes.byref(bundle.raw), channel._owner_pointer,
                    diagnostic, len(diagnostic), _ctypes.byref(required),
                )
            )
            if not channel.is_open:
                raise _local_error(
                    ErrorKind.PROTOCOL_INCONSISTENCY,
                    "Health open succeeded without returning an owner",
                )
        except BaseException:
            channel.__del__()
            raise
        return channel

    def open_control_channel(self, config: ControlConfig) -> ControlChannel:
        """Open an IR CommandAndControl channel without retaining this object."""

        self._require_open()
        if not isinstance(config, ControlConfig):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "config must be ControlConfig")
        bundle = _RawControlConfig(config)
        owner = _native.IrC2Handle()
        try:
            _call_with_diagnostic(
                lambda diagnostic, required: _native.ams_mel_ir_c2_open(
                    self._owner,
                    _ctypes.byref(bundle.raw),
                    _ctypes.byref(owner),
                    diagnostic,
                    len(diagnostic),
                    _ctypes.byref(required),
                )
            )
        except BaseException:
            if owner.value:
                _native.ams_mel_ir_c2_close(
                    _ctypes.byref(owner), None, 0, None
                )
            raise
        if not owner.value:
            raise _local_error(
                ErrorKind.PROTOCOL_INCONSISTENCY,
                "control channel open succeeded without returning an owner",
            )
        return ControlChannel._from_owner(owner)

    def close(self) -> None:
        """Release this owner. Repeated successful close is harmless."""

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

    def __enter__(self) -> Session:
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
