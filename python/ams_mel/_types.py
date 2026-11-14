"""Private types implementation; import public names from ams_mel."""

from __future__ import annotations

import ctypes as _ctypes
from dataclasses import dataclass
from enum import Enum
from enum import IntEnum
from typing import Any

from . import _native
from ._errors import (
    ErrorKind,
    _local_error,
)
from ._validation import (
    _INT64_MAX,
    _UINT32_MAX,
    _utf8_input,
    _validate_int64,
    _validate_size_t,
    _validate_uint32,
)


@dataclass(frozen=True)
class AbiVersion:
    major: int
    minor: int


@dataclass(frozen=True)
class ConfigSetCommand:
    """Opaque configuration text and exact ID/time; no clock domain is inferred."""

    command_id: int
    system_time_ns: int = 0
    config: str = ""

    def __post_init__(self) -> None:
        _validate_uint32(self.command_id, "command_id")
        _validate_int64(self.system_time_ns, "system_time_ns")
        _utf8_input(self.config, "config")


@dataclass(frozen=True)
class ProviderVersion:
    api_version: int
    library_version: int
    vendor: str
    description: str


@dataclass(frozen=True)
class UciId:
    uuid: bytes
    descriptive_label: str

    def __post_init__(self) -> None:
        if not isinstance(self.uuid, bytes) or len(self.uuid) != 16:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "uuid must be exactly 16 bytes"
            )
        _utf8_input(self.descriptive_label, "descriptive_label")


@dataclass(frozen=True)
class ComponentLocation:
    offset_x_m: float
    offset_y_m: float
    offset_z_m: float
    key: str
    system_name: str

    def __post_init__(self) -> None:
        _utf8_input(self.key, "key")
        _utf8_input(self.system_name, "system_name")


@dataclass(frozen=True)
class ImageConfig:
    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation
    buffer_count: int = 3
    buffer_size: int = 1_048_576
    queue_capacity: int = 4

    def __post_init__(self) -> None:
        if not isinstance(self.channel_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "channel_id must be UciId")
        if not isinstance(self.platform_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "platform_id must be UciId")
        if not isinstance(self.sensor_location, ComponentLocation):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "sensor_location must be ComponentLocation",
            )
        for value, name in (
            (self.buffer_count, "buffer_count"),
            (self.buffer_size, "buffer_size"),
            (self.queue_capacity, "queue_capacity"),
        ):
            _validate_size_t(value, name)
            if value == 0:
                raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be positive")
        if self.buffer_size > _INT64_MAX:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, "buffer_size must not exceed INT64_MAX"
            )
        if self.buffer_count - 1 > _INT64_MAX:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "buffer_count - 1 must not exceed INT64_MAX",
            )

    @classmethod
    def new(
        cls,
        channel_id: UciId,
        platform_id: UciId,
        sensor_location: ComponentLocation,
        buffer_count: int = 3,
        buffer_size: int = 1_048_576,
        queue_capacity: int = 4,
    ) -> ImageConfig:
        return cls(
            channel_id,
            platform_id,
            sensor_location,
            buffer_count,
            buffer_size,
            queue_capacity,
        )


@dataclass(frozen=True)
class ControlConfig:
    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation

    def __post_init__(self) -> None:
        if not isinstance(self.channel_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "channel_id must be UciId")
        if not isinstance(self.platform_id, UciId):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "platform_id must be UciId")
        if not isinstance(self.sensor_location, ComponentLocation):
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT,
                "sensor_location must be ComponentLocation",
            )

    @classmethod
    def new(
        cls,
        channel_id: UciId,
        platform_id: UciId,
        sensor_location: ComponentLocation,
    ) -> ControlConfig:
        return cls(channel_id, platform_id, sensor_location)


@dataclass(frozen=True)
class InstrumentationConfig:
    """Complete attachment input; construction performs no provider work."""

    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation

    def __post_init__(self) -> None:
        _validate_attachment_config(self)

    @classmethod
    def new(
        cls, channel_id: UciId, platform_id: UciId,
        sensor_location: ComponentLocation,
    ) -> InstrumentationConfig:
        return cls(channel_id, platform_id, sensor_location)


@dataclass(frozen=True)
class HealthConfig:
    """Complete HealthAndStatus attachment input; no provider work at construction."""

    channel_id: UciId
    platform_id: UciId
    sensor_location: ComponentLocation

    def __post_init__(self) -> None:
        _validate_attachment_config(self)

    @classmethod
    def new(
        cls, channel_id: UciId, platform_id: UciId,
        sensor_location: ComponentLocation,
    ) -> HealthConfig:
        return cls(channel_id, platform_id, sensor_location)


def _validate_attachment_config(config: InstrumentationConfig | HealthConfig) -> None:
    ControlConfig.__post_init__(config)
    location = config.sensor_location
    for name in ("offset_x_m", "offset_y_m", "offset_z_m"):
        value = getattr(location, name)
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, f"{name} must be numeric")
        try:
            _ctypes.c_double(value)
        except (OverflowError, TypeError, ValueError) as error:
            raise _local_error(
                ErrorKind.INVALID_ARGUMENT, f"{name} must fit a C double"
            ) from error
    # Reuse validation even for values supplied by subclasses.
    UciId.__post_init__(config.channel_id)
    UciId.__post_init__(config.platform_id)
    _utf8_input(location.key, "key")
    _utf8_input(location.system_name, "system_name")


class InstrumentationPriority(IntEnum):
    NORMAL = _native.AMS_MEL_IR_PRIORITY_NORMAL
    DEBUG = _native.AMS_MEL_IR_PRIORITY_DEBUG


def _instrumentation_priority(value: int) -> InstrumentationPriority:
    _validate_uint32(value, "priority")
    try:
        return InstrumentationPriority(value)
    except ValueError as error:
        raise _local_error(
            ErrorKind.INVALID_ARGUMENT, "priority must be NORMAL (0) or DEBUG (1)"
        ) from error


@dataclass(frozen=True)
class InstrumentationLevelCommand:
    command_id: int
    priority: InstrumentationPriority

    def __post_init__(self) -> None:
        _validate_uint32(self.command_id, "command_id")
        object.__setattr__(self, "priority", _instrumentation_priority(self.priority))


@dataclass(frozen=True)
class InstrumentationReport:
    """Owned report; size and signed nanosecond timestamp are not interpreted."""

    command_id: int
    size: int
    timestamp_ns: int
    priority: InstrumentationPriority

    def __post_init__(self) -> None:
        _validate_uint32(self.command_id, "command_id")
        _validate_uint32(self.size, "size")
        if (isinstance(self.timestamp_ns, bool)
                or not isinstance(self.timestamp_ns, int)
                or not -(_INT64_MAX + 1) <= self.timestamp_ns <= _INT64_MAX):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "timestamp_ns must be int64")
        object.__setattr__(self, "priority", _instrumentation_priority(self.priority))


@dataclass(frozen=True)
class InstrumentationMetadataCounters:
    """Saturating callback activity counters, not successful receive counts."""

    events_received: int
    events_dropped_queue_full: int
    malformed_or_unsupported: int


@dataclass(frozen=True)
class InstrumentationCompleted:
    report: InstrumentationReport

    def __post_init__(self) -> None:
        if not isinstance(self.report, InstrumentationReport):
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "report must be InstrumentationReport")


@dataclass(frozen=True)
class InstrumentationRejected:
    code: MelErrorCode
    description: str

    def __post_init__(self) -> None:
        _validate_uint32(self.code, "code")
        if self.code > 8:
            raise _local_error(ErrorKind.INVALID_ARGUMENT, "Instrumentation error code must be 0..8")
        object.__setattr__(self, "code", MelErrorCode(self.code))
        _utf8_input(self.description, "description")


class MFAState(IntEnum):
    """Common MEL MFA state identifiers, not a provider transition model."""

    NOT_SET = _native.AMS_MEL_IR_MFA_STATE_NOT_SET
    UNKNOWN = _native.AMS_MEL_IR_MFA_STATE_UNKNOWN
    NOT_INSTALLED = _native.AMS_MEL_IR_MFA_STATE_NOT_INSTALLED
    OFF = _native.AMS_MEL_IR_MFA_STATE_OFF
    PRE_INITIALIZATION = _native.AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION
    INITIALIZATION = _native.AMS_MEL_IR_MFA_STATE_INITIALIZATION
    STANDBY = _native.AMS_MEL_IR_MFA_STATE_STANDBY
    OPERATE = _native.AMS_MEL_IR_MFA_STATE_OPERATE
    OPERATE_RX_ONLY = _native.AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY
    OPERATE_TX_ONLY = _native.AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY
    MAINTENANCE = _native.AMS_MEL_IR_MFA_STATE_MAINTENANCE
    CALIBRATION = _native.AMS_MEL_IR_MFA_STATE_CALIBRATION
    INITIATED_BIT = _native.AMS_MEL_IR_MFA_STATE_INITIATED_BIT
    SHUTDOWN = _native.AMS_MEL_IR_MFA_STATE_SHUTDOWN
    DEGRADED = _native.AMS_MEL_IR_MFA_STATE_DEGRADED


class MfaMode(IntEnum):
    UNUSED = _native.AMS_MEL_IR_MFA_MODE_UNUSED
    TASK_SCHED = _native.AMS_MEL_IR_MFA_MODE_TASK_SCHED
    SCAN_VOLUME_SCHED = _native.AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED
    SCAN_BAR_SCHED = _native.AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED


class CommandReturn(IntEnum):
    SUCCESS = _native.AMS_MEL_IR_RETURN_SUCCESS
    BAD_POINTER = _native.AMS_MEL_IR_RETURN_BAD_POINTER
    FAIL = _native.AMS_MEL_IR_RETURN_FAIL
    NOT_SUPPORTED = _native.AMS_MEL_IR_RETURN_NOT_SUPPORTED
    NOT_IMPLEMENTED = _native.AMS_MEL_IR_RETURN_NOT_IMPLEMENTED


class MelErrorCode(IntEnum):
    NONE = _native.AMS_MEL_ERROR_NONE
    INVALID_ID = _native.AMS_MEL_ERROR_INVALID_ID
    INVALID_STATE = _native.AMS_MEL_ERROR_INVALID_STATE
    INVALID_PARAMETERS = _native.AMS_MEL_ERROR_INVALID_PARAMETERS
    INSUFFICIENT_PERMISSIONS = _native.AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS
    INSUFFICIENT_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_RESOURCES
    INSUFFICIENT_LOCAL_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES
    INSUFFICIENT_REMOTE_RESOURCES = _native.AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES
    UNSUPPORTED = _native.AMS_MEL_ERROR_UNSUPPORTED

    @classmethod
    def _missing_(cls, value: object) -> MelErrorCode | None:
        if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= _UINT32_MAX:
            unknown = int.__new__(cls, value)
            unknown._name_ = f"UNKNOWN_0x{value:08X}"
            unknown._value_ = value
            return unknown
        return None


@dataclass(frozen=True)
class ModeSuccess:
    mode: MfaMode


@dataclass(frozen=True)
class ModeRejected:
    code: MelErrorCode
    description: str


@dataclass(frozen=True)
class ReturnCompleted:
    value: CommandReturn


@dataclass(frozen=True)
class ReturnRejected:
    code: MelErrorCode
    description: str


@dataclass(frozen=True)
class CommsTestRequest:
    """Inherited CommsTest input; all three uint32 IDs are sent exactly."""

    command_id: int
    channel_id: int
    request_id: int

    def __post_init__(self) -> None:
        _validate_uint32(self.command_id, "command_id")
        _validate_uint32(self.channel_id, "channel_id")
        _validate_uint32(self.request_id, "request_id")


@dataclass(frozen=True)
class CommsTestReport:
    """CommsTest reply; the upstream reply carries no channel ID."""

    command_id: int
    request_id: int


@dataclass(frozen=True)
class CommsCompleted:
    report: CommsTestReport


@dataclass(frozen=True)
class CommsRejected:
    code: MelErrorCode
    description: str


class _Uint32PreservingEnum(IntEnum):
    """Private base: unknown uint32 values become ``UNKNOWN_0x...`` members.

    This mirrors :meth:`MelErrorCode._missing_`, so a future native value is
    preserved (value and a descriptive name) rather than discarded or raised.
    """

    @classmethod
    def _missing_(cls, value: object) -> Any:
        if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= _UINT32_MAX:
            unknown = int.__new__(cls, value)
            unknown._name_ = f"UNKNOWN_0x{value:08X}"
            unknown._value_ = value
            return unknown
        return None


class PixelFormat(_Uint32PreservingEnum):
    MONO = _native.AMS_MEL_IR_PIXEL_MONO
    RGB = _native.AMS_MEL_IR_PIXEL_RGB
    BAYER = _native.AMS_MEL_IR_PIXEL_BAYER


class SensorType(_Uint32PreservingEnum):
    UNSPECIFIED = _native.AMS_MEL_IR_SENSOR_UNSPECIFIED
    GIMBAL_HORIZONTAL = _native.AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL
    GIMBAL_VERTICAL = _native.AMS_MEL_IR_SENSOR_GIMBAL_VERTICAL
    GIMBAL_ROTATION = _native.AMS_MEL_IR_SENSOR_GIMBAL_ROTATION
    STEP_STARE = _native.AMS_MEL_IR_SENSOR_STEP_STARE


class ChannelType(_Uint32PreservingEnum):
    IRST_TRACK = _native.AMS_MEL_IR_CHANNEL_IRST_TRACK
    IRST_IMAGE = _native.AMS_MEL_IR_CHANNEL_IRST_IMAGE
    COMMAND_AND_CONTROL = _native.AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL
    SCHEDULING = _native.AMS_MEL_IR_CHANNEL_SCHEDULING
    HEALTH_AND_STATUS = _native.AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS
    INSTRUMENTATION = _native.AMS_MEL_IR_CHANNEL_INSTRUMENTATION
    STACKED_IMAGE = _native.AMS_MEL_IR_CHANNEL_STACKED_IMAGE
    RESERVED_1 = _native.AMS_MEL_IR_CHANNEL_RESERVED_1
    RESERVED_2 = _native.AMS_MEL_IR_CHANNEL_RESERVED_2


class MetadataCapability(_Uint32PreservingEnum):
    BAD_PIXEL_LIST = _native.AMS_MEL_IR_METADATA_BAD_PIXEL_LIST
    OPTICAL_DISTORTION_MAP = _native.AMS_MEL_IR_METADATA_OPTICAL_DISTORTION_MAP
    LF_STATUS = _native.AMS_MEL_IR_METADATA_LF_STATUS
    LINE_OF_SIGHT_REPORT = _native.AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT
    LINE_OF_SIGHT_QUATERNION = _native.AMS_MEL_IR_METADATA_LINE_OF_SIGHT_QUATERNION
    LINE_OF_SIGHT_EULER = _native.AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER
    MFA_STATUS = _native.AMS_MEL_IR_METADATA_MFA_STATUS
    MFA_STATUS_DETAILED = _native.AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED
    BIT_CONFIGURATION = _native.AMS_MEL_IR_METADATA_BIT_CONFIGURATION
    COMMAND_STATUS = _native.AMS_MEL_IR_METADATA_COMMAND_STATUS
    BIT_STATUS = _native.AMS_MEL_IR_METADATA_BIT_STATUS
    CANDIDATE_OBJECT_MESSAGE = _native.AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_MESSAGE
    TASK_EXECUTING_REP = _native.AMS_MEL_IR_METADATA_TASK_EXECUTING_REP
    SUBSYSTEM_STATUS_RESP = _native.AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP
    EXECUTE_TASK_ACK = _native.AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK
    SCHED_CREATED_REP = _native.AMS_MEL_IR_METADATA_SCHED_CREATED_REP
    IRST_TRACK_REPORT = _native.AMS_MEL_IR_METADATA_IRST_TRACK_REPORT
    CHANNEL_COMMS_TEST_REP = _native.AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP
    CAMERA_COMMAND_RESP = _native.AMS_MEL_IR_METADATA_CAMERA_COMMAND_RESP
    CAMERA_PROTECT_CMD_RESP = _native.AMS_MEL_IR_METADATA_CAMERA_PROTECT_CMD_RESP
    INSTRUMENTATION_REPORT = _native.AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT
    NAVIGATION_REPORT_RESP = _native.AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP
    REQUEST_SYSTEM_TRACK_DATA = _native.AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA
    UPDATE_TRACK_LIST_RESPONSE = _native.AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE
    LOS_3D_KINEMATICS_TYPE = _native.AMS_MEL_IR_METADATA_LOS_3D_KINEMATICS_TYPE
    CANDIDATE_OBJECT_PREPROC_MESSAGE = (
        _native.AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE
    )
    TASK_EVENTS = _native.AMS_MEL_IR_METADATA_TASK_EVENTS
    SCAN_PERFORMANCE_REPORT = _native.AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT
    RESERVED_3 = _native.AMS_MEL_IR_METADATA_RESERVED_3
    RESERVED_5 = _native.AMS_MEL_IR_METADATA_RESERVED_5
    RESERVED_9 = _native.AMS_MEL_IR_METADATA_RESERVED_9
    RESERVED_10 = _native.AMS_MEL_IR_METADATA_RESERVED_10


class BandType(_Uint32PreservingEnum):
    INVALID = _native.AMS_MEL_IR_BAND_INVALID
    MULTIBAND = _native.AMS_MEL_IR_BAND_MULTIBAND
    IR_FAR = _native.AMS_MEL_IR_BAND_IR_FAR
    IR_NEAR = _native.AMS_MEL_IR_BAND_IR_NEAR
    IR_LONGWAVE = _native.AMS_MEL_IR_BAND_IR_LONGWAVE
    IR_MIDWAVE = _native.AMS_MEL_IR_BAND_IR_MIDWAVE
    IR_SHORTWAVE = _native.AMS_MEL_IR_BAND_IR_SHORTWAVE
    VISIBLE_WHITE = _native.AMS_MEL_IR_BAND_VISIBLE_WHITE
    VISIBLE_RED = _native.AMS_MEL_IR_BAND_VISIBLE_RED
    VISIBLE_GREEN = _native.AMS_MEL_IR_BAND_VISIBLE_GREEN
    VISIBLE_BLUE = _native.AMS_MEL_IR_BAND_VISIBLE_BLUE
    UVA = _native.AMS_MEL_IR_BAND_UVA
    UVB = _native.AMS_MEL_IR_BAND_UVB
    UVC = _native.AMS_MEL_IR_BAND_UVC
    UV_VACUUM = _native.AMS_MEL_IR_BAND_UV_VACUUM


class CoordinateSystem(_Uint32PreservingEnum):
    LLA = _native.AMS_MEL_IR_COORDINATE_LLA
    ECEF = _native.AMS_MEL_IR_COORDINATE_ECEF
    NED_PLATFORM = _native.AMS_MEL_IR_COORDINATE_NED_PLATFORM
    NED_SENSOR = _native.AMS_MEL_IR_COORDINATE_NED_SENSOR


@dataclass(frozen=True)
class BandInfo:
    kind: BandType
    min_wavelength_m: float
    max_wavelength_m: float


@dataclass(frozen=True)
class ImageBand:
    band_index: int
    bands: tuple[BandInfo, ...]


@dataclass(frozen=True)
class ChannelCapability:
    """Complete Python-owned copy of the native ChannelCapability record.

    Every string, UUID, span, and nested band span is copied before the native
    snapshot closes; nothing in this graph refers to native memory, so it stays
    valid after the View, typed owner, Session, and provider library are gone.
    """

    channel_id: UciId
    height: int
    width: int
    bit_depth: int
    row_pitch: int
    buffer_size: int
    image_size: int
    number_of_bands: int
    pixel_format: PixelFormat
    sensor_types: tuple[SensorType, ...]
    platform_id: UciId
    sensor_location: ComponentLocation
    channel_types: tuple[ChannelType, ...]
    task_schedule_depth: int
    odc_available: bool
    nuc_available: bool
    metadata_capabilities: tuple[MetadataCapability, ...]
    image_bands: tuple[ImageBand, ...]
    nav_frames: tuple[CoordinateSystem, ...]


class ImageType(Enum):
    STARING = "staring"
    SCANNING = "scanning"


class ImageFlip(Enum):
    NONE = "none"
    VERTICAL = "vertical"
    HORIZONTAL = "horizontal"
    BOTH = "both"


@dataclass(frozen=True)
class Frame:
    system_time_ns: int
    integration_time_ns: int
    width: int
    height: int
    bits_per_pixel: int
    number_of_bands: int
    horizontal_fov_rad: float
    vertical_fov_rad: float
    frame_id: int
    subframe_id: int
    subframe_total: int
    image_type: ImageType
    image_flip: ImageFlip
    image_flags: int
    dither_row: float
    dither_column: float
    row_offset: int
    column_offset: int
    band_index: int
    pixels: bytes


@dataclass(frozen=True)
class StreamCounters:
    frames_received: int
    frames_dropped_queue_full: int
    malformed_or_unsupported: int
