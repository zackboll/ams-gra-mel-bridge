"""IR MEL sessions, channels, commands, requests and owned metadata values.

Session opens the IR Control factory and provides discovery and typed channel
factories. This namespace also includes the inherited IR ChannelView services,
Image/StackedImage, C2, Health, Instrumentation, navigation and metadata APIs.
Shared values such as UciId, ComponentLocation and MFAState, plus MelError and
ErrorKind, are available from ams_mel.common.

Importing this namespace requires AMS_MEL_NATIVE_LIB and loads the native
facade through the parent package. It does not open a provider or enable work.
All existing ownership, external-serialization, timeout and cleanup contracts
apply. Exports are the same objects available from ams_mel; their canonical
introspection and pickle module remains ams_mel."""

from ._comms_metadata import (
    CommsMetadataCounters,
    CommsMetadataStream,
)

from ._image_types import (
    AzimuthElevation,
    BadPixel,
    BadPixelList,
    BadPixelReason,
    ImageMetadataCounters,
    ImageMetadataEvent,
    ImageMetadataKind,
    LineOfSightEuler,
    LineOfSightReport,
    NavigationResponse,
)

from ._image_metadata import (
    ImageMetadataStream,
)

from ._los_quaternion import (
    Quaternion,
    LineOfSightQuaternion,
    LineOfSightQuaternionOpenError,
    LineOfSightQuaternionStream,
)

from ._optical_distortion_map import (
    ForwardRightDown,
    RowCol,
    OpticalDistortionMap,
    OpticalDistortionMapOpenError,
    OpticalDistortionMapStream,
)

from ._navigation import (
    NavigationRequest,
)

from ._navigation_types import (
    AttitudeRate,
    NavigationCompleted,
    NavigationRejected,
    NavigationReport,
    NorthEastDown,
    PositionSolutionState,
    PositionVelocityCovariance,
)

from ._calibration_configuration import (
    CalibrationControlInterface,
    SubsystemCalibration,
    CalibrationConfiguration,
    CalibrationConfigurationOpenError,
    CalibrationConfigurationStream,
)

from ._calibration_configuration_command import (
    CalibrationConfigurationCommand,
    CalibrationConfigurationCompleted,
    CalibrationConfigurationRejected,
    CalibrationConfigurationRequest,
)
from ._calibration_status import (
    CalibrationStatus,
    CalibrationStatusOpenError,
    CalibrationStatusStream,
    CompletedCalibration,
    CompletedCalibrationItem,
    SubsystemCalibrationResult,
)
from ._calibration_status_command import (
    CalibrationStatusCommand,
    CalibrationStatusCompleted,
    CalibrationStatusRejected,
    CalibrationStatusRequest,
)
from ._camera_protect_command import (
    CameraProtectCommand, CameraProtectCompleted, CameraProtectRejected, CameraProtectRequest,
)
from ._erase_command import EraseCommandType, EraseCommand, EraseCompleted, EraseRejected, EraseRequest
from ._c2_system_track_response import (
    C2SystemTrackDataCompleted,
    C2SystemTrackDataRejected,
    C2SystemTrackDataRequest,
    SystemTrackDataResponse,
)
from ._camera_protect_response import (
    CameraProtectStatus,
    CameraProtectResponse,
    CameraProtectResponseOpenError,
    CameraProtectResponseStream,
)

from ._camera_response import (
    CameraCommandResponse,
    CameraResponseCounters,
    CameraResponseOpenError,
    CameraResponseStream,
)

from ._nuc import (
    NucPointType,
    NucPixelKind,
    NucPixelValue,
    NucTempPoint,
    NucTempData,
    NucCounters,
    NucOpenError,
    NucStream,
)

from ._types import (
    ConfigSetCommand,
    CoordinateFrameReference,
    DegradationMethod,
    ModeCommand,
    ScanParameters,
    ScanType,
    ImageConfig,
    ImageControlConfig,
    StackedConfig,
    ControlConfig,
    InstrumentationConfig,
    HealthConfig,
    InstrumentationPriority,
    InstrumentationLevelCommand,
    InstrumentationReport,
    InstrumentationMetadataCounters,
    InstrumentationCompleted,
    InstrumentationRejected,
    MfaMode,
    CommandReturn,
    MelErrorCode,
    ModeSuccess,
    ModeRejected,
    ReturnCompleted,
    ReturnRejected,
    CommsTestRequest,
    CommsTestReport,
    CommsCompleted,
    CommsRejected,
    PixelFormat,
    SensorType,
    ChannelType,
    MetadataCapability,
    BandType,
    CoordinateSystem,
    BandInfo,
    ImageBand,
    ChannelCapability,
    ImageType,
    ImageFlip,
    Frame,
    StreamCounters,
)

from ._requests import (
    ModeRequest,
    ReturnRequest,
    CommsRequest,
)

from ._common import (
    ChannelView,
)

from ._image import (
    ImageStream,
)

from ._c2 import (
    ControlChannel,
)

from ._c2_metadata import (
    C2MetadataStream,
)

from ._c2_types import (
    C2MetadataKind,
    C2MetadataEvent,
    C2MetadataCounters,
    C2CommandState,
    C2CannotComply,
    C2CommandStatus,
    C2BitControlInterface,
    C2BitType,
    C2BitConfiguration,
)

from ._instrumentation import (
    InstrumentationChannel,
    InstrumentationMetadata,
    InstrumentationRequest,
)

from ._health import (
    HealthStatusChannel,
)

from ._health_metadata import (
    HealthMetadataStream,
)

from ._stacked import (
    StackedChannel,
)

from ._health_types import (
    About,
    ActiveBit,
    BitResult,
    BitStatus,
    CompletedBit,
    CompletedBitItem,
    ComponentState,
    CsciInfo,
    CsciMode,
    CsciVersion,
    DiscreteStatus,
    Euler,
    FailureLevel,
    Fault,
    FaultAmbiguityGroup,
    FaultData,
    FaultSeverity,
    FaultState,
    ForeignKey,
    HealthMetadataCounters,
    HealthMetadataEvent,
    HealthMetadataKind,
    InstallationDetails,
    MfaComponent,
    MfaStatus,
    MfaStatusDetailed,
    NameValuePair,
    SecurityArtifact,
    SecurityAuditRecord,
    SecurityAuthentication,
    SecurityAuthenticationCategory,
    SecurityEventKind,
    SecurityFileManagement,
    SecurityFileManagementCategory,
    SecurityIntegrity,
    SecurityIntegrityCategory,
    SecurityKeyManagement,
    SecurityKeyManagementCategory,
    SecurityNone,
    SecurityOutcome,
    SecuritySanitization,
    SecuritySanitizationCategory,
    SecuritySeverity,
    SecuritySystem,
    SecuritySystemCategory,
    StateTransitionStatus,
    SubsystemDependency,
    SubsystemStatus,
    TemperatureState,
    TemperatureStatus,
)

from ._session import (
    Session,
)


__all__ = [
    "CalibrationControlInterface",
    "SubsystemCalibration",
    "CalibrationConfiguration",
    "CalibrationConfigurationOpenError",
    "CalibrationConfigurationStream",
    "CalibrationConfigurationCommand",
    "CalibrationConfigurationCompleted",
    "CalibrationConfigurationRejected",
    "CalibrationConfigurationRequest",
    "CalibrationStatus",
    "CalibrationStatusCommand",
    "CalibrationStatusCompleted",
    "CalibrationStatusRejected",
    "CalibrationStatusRequest",
    "CalibrationStatusOpenError",
    "CalibrationStatusStream",
    "CompletedCalibration",
    "CompletedCalibrationItem",
    "SubsystemCalibrationResult",
    "CameraProtectCommand",
    "CameraProtectCompleted",
    "CameraProtectRejected",
    "CameraProtectRequest",
    "EraseCommandType",
    "EraseCommand",
    "EraseCompleted",
    "EraseRejected",
    "EraseRequest",
    "SystemTrackDataResponse",
    "C2SystemTrackDataCompleted",
    "C2SystemTrackDataRejected",
    "C2SystemTrackDataRequest",
    "CameraProtectStatus",
    "CameraProtectResponse",
    "CameraProtectResponseOpenError",
    "CameraProtectResponseStream",
    "Quaternion",
    "LineOfSightQuaternion",
    "LineOfSightQuaternionOpenError",
    "LineOfSightQuaternionStream",
    "ForwardRightDown",
    "RowCol",
    "OpticalDistortionMap",
    "OpticalDistortionMapOpenError",
    "OpticalDistortionMapStream",
    "AttitudeRate",
    "NavigationCompleted",
    "NavigationRejected",
    "NavigationReport",
    "NavigationRequest",
    "NorthEastDown",
    "PositionSolutionState",
    "PositionVelocityCovariance",
    "CameraCommandResponse",
    "CameraResponseCounters",
    "CameraResponseOpenError",
    "CameraResponseStream",
    "C2MetadataStream",
    "C2MetadataKind",
    "C2MetadataEvent",
    "C2MetadataCounters",
    "C2CommandState",
    "C2CannotComply",
    "C2CommandStatus",
    "C2BitControlInterface",
    "C2BitType",
    "C2BitConfiguration",
    "AzimuthElevation",
    "BadPixel",
    "BadPixelList",
    "BadPixelReason",
    "ImageMetadataCounters",
    "ImageMetadataEvent",
    "ImageMetadataKind",
    "ImageMetadataStream",
    "LineOfSightEuler",
    "LineOfSightReport",
    "NavigationResponse",
    "NucPointType",
    "NucPixelKind",
    "NucPixelValue",
    "NucTempPoint",
    "NucTempData",
    "NucCounters",
    "NucOpenError",
    "NucStream",
    "About",
    "ActiveBit",
    "BitResult",
    "BitStatus",
    "CompletedBit",
    "CompletedBitItem",
    "ComponentState",
    "CsciInfo",
    "CsciMode",
    "CsciVersion",
    "DiscreteStatus",
    "Euler",
    "FailureLevel",
    "Fault",
    "FaultAmbiguityGroup",
    "FaultData",
    "FaultSeverity",
    "FaultState",
    "ForeignKey",
    "HealthMetadataCounters",
    "HealthMetadataEvent",
    "HealthMetadataKind",
    "HealthMetadataStream",
    "InstallationDetails",
    "MfaComponent",
    "MfaStatus",
    "MfaStatusDetailed",
    "NameValuePair",
    "SecurityArtifact",
    "SecurityAuditRecord",
    "SecurityAuthentication",
    "SecurityAuthenticationCategory",
    "SecurityEventKind",
    "SecurityFileManagement",
    "SecurityFileManagementCategory",
    "SecurityIntegrity",
    "SecurityIntegrityCategory",
    "SecurityKeyManagement",
    "SecurityKeyManagementCategory",
    "SecurityNone",
    "SecurityOutcome",
    "SecuritySanitization",
    "SecuritySanitizationCategory",
    "SecuritySeverity",
    "SecuritySystem",
    "SecuritySystemCategory",
    "StateTransitionStatus",
    "SubsystemDependency",
    "SubsystemStatus",
    "TemperatureState",
    "TemperatureStatus",
    "BandInfo",
    "BandType",
    "ChannelCapability",
    "ChannelType",
    "ChannelView",
    "CommandReturn",
    "CommsCompleted",
    "CommsRejected",
    "CommsRequest",
    "CommsMetadataCounters",
    "CommsMetadataStream",
    "CommsTestReport",
    "CommsTestRequest",
    "ConfigSetCommand",
    "CoordinateFrameReference",
    "DegradationMethod",
    "ModeCommand",
    "ScanParameters",
    "ScanType",
    "ControlChannel",
    "ControlConfig",
    "CoordinateSystem",
    "Frame",
    "HealthConfig",
    "HealthStatusChannel",
    "StackedConfig",
    "StackedChannel",
    "ImageBand",
    "ImageConfig",
    "ImageControlConfig",
    "ImageFlip",
    "ImageStream",
    "ImageType",
    "InstrumentationChannel",
    "InstrumentationCompleted",
    "InstrumentationConfig",
    "InstrumentationLevelCommand",
    "InstrumentationMetadata",
    "InstrumentationMetadataCounters",
    "InstrumentationPriority",
    "InstrumentationRejected",
    "InstrumentationReport",
    "InstrumentationRequest",
    "MelErrorCode",
    "MetadataCapability",
    "MfaMode",
    "ModeRejected",
    "ModeRequest",
    "ModeSuccess",
    "PixelFormat",
    "ReturnCompleted",
    "ReturnRejected",
    "ReturnRequest",
    "SensorType",
    "Session",
    "StreamCounters",
]
