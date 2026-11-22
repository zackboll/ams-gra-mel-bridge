//! Backward-compatible safe Rust MEL façade.
//!
//! Existing `ams_mel::...` imports retain the same concrete owner and value
//! types. Domain-specific applications can use `ams-mel-ir` or `ams-mel-rf`
//! directly; shared identities and errors remain in `ams-mel-common`.
//! This façade does not load providers or implement an additional native ABI.
//! Provider acceptance and API coverage do not prove physical execution.

#[doc(inline)]
pub use ams_mel_ir::{
    abi_version, ir_nuc, AbiVersion, About, ActiveBit, AttitudeRate, AzimuthElevation, BadPixel,
    BadPixelList, BadPixelReason, BandInfo, BandType, BitAction, BitCommand, BitConfiguration,
    BitControlInterface, BitResult, BitStatus, BitTypeDefinition, BufferFlag, BufferFlagsError,
    BufferInfo, BufferOpenError, C2MetadataCounters, C2MetadataEvent, C2MetadataStream,
    CannotComply, ChannelCapability, ChannelOperationError, ChannelType, ChannelView,
    CheckoutReleaseError, CommandReturn, CommandState, CommandStatus, CommsMetadataCounters,
    CommsMetadataOpenError, CommsMetadataStream, CommsRequest, CommsTestReport, CommsTestRequest,
    CommsTestResult, CompletedBit, CompletedBitItem, ComponentLocation, ComponentState,
    ConfigSetCommand, ControlChannel, ControlConfig, CoordinateFrameReference, CoordinateSystem,
    CsciInfo, CsciMode, CsciVersion, DegradationMethod, Error, ErrorKind, Euler, FailureLevel,
    Fault, FaultAmbiguityGroup, FaultData, FaultSeverity, FaultState, ForeignKey, ForwardRightDown,
    Frame, HealthConfig, HealthMetadataCounters, HealthMetadataEvent, HealthMetadataKind,
    HealthMetadataStream, HealthStatusChannel, HostBuffer, ImageBand, ImageCheckoutCounters,
    ImageCheckoutLease, ImageCheckoutReceiver, ImageConfig, ImageControlConfig, ImageFlip,
    ImageLocation, ImageMetadataCounters, ImageMetadataEvent, ImageMetadataStream, ImageStream,
    ImageType, InstallationDetails, LineOfSightEuler, LineOfSightQuaternion,
    LineOfSightQuaternionOpenError, LineOfSightQuaternionStream, LineOfSightReport, MelErrorCode,
    MetadataCapability, MfaComponent, MfaMode, MfaState, MfaStatus, ModeCommand, ModeRequest,
    ModeResult, NameValuePair, NavigationReport, NavigationRequest, NavigationResponse,
    NavigationResult, NorthEastDown, NucCounters, NucOpenError, NucPixelValue, NucPointType,
    NucStream, NucTempData, NucTempPoint, OpticalDistortionMap, OpticalDistortionMapOpenError,
    OpticalDistortionMapStream, PixelFormat, PositionSolutionState, PositionVelocityCovariance,
    ProviderVersion, Quaternion, ReturnRequest, ReturnResult, RowCol, ScanParameters, ScanType,
    SchedulingChannel, SchedulingConfig, SchedulingStareParameters, SchedulingStareTask,
    SchedulingTaskInfo, SecurityArtifact, SecurityAuditRecord, SecurityAuthenticationCategory,
    SecurityEvent, SecurityEventKind, SecurityFileManagementCategory, SecurityIntegrityCategory,
    SecurityKeyManagementCategory, SecurityOutcome, SecuritySanitizationCategory, SecuritySeverity,
    SecuritySystemCategory, SensorType, Session, SessionOptions, StackedChannel, StackedConfig,
    StateTransitionStatus, StreamCounters, SubsystemDependency, SubsystemStatus, TaskListAction,
    TemperatureState, TemperatureStatus, UciId,
};

#[doc(inline)]
pub use ams_mel_rf::{
    RfAdmin, RfData, RfFaceInfo, RfFrequencyRange, RfJobDataFormat, RfMfaInfo, RfMfaState,
};
