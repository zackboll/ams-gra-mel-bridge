//! Safe Rust provider sessions, IR Mono8 frames, IR C2, IR HealthAndStatus
//! lifecycle, and the common Channel façade for C2, Image, Health and Stacked through
//! `ams_mel_c`.
//!
//! [`Session`], [`ImageStream`], [`ControlChannel`], [`HealthStatusChannel`],
//! [`ModeRequest`], [`ReturnRequest`], [`ChannelView`], and [`CommsRequest`]
//! are intentionally neither `Send` nor `Sync`. A zero receive or request
//! timeout polls. Request timeout and request close or drop do not cancel
//! provider work. [`BitCommand`] supports one-choice NoOp, initiate, cancel and
//! clear-fault payloads, retaining numeric IDs and opaque UTF-8 bytes.
//!
//! # Core ownership and workflow
//!
//! Open a [`Session`] using a compatible provider library and its selection
//! strings. [`abi_version`] describes the consumer façade;
//! [`Session::provider_version`] describes that provider. Channel creation is
//! separate from enabling: use [`ImageStream::start`] or [`ControlChannel::enable`]
//! explicitly. [`ImageConfig`] requests Mono8 acquisition; [`ImageControlConfig`]
//! attaches without an image source or acquisition buffers.
//!
//! Values such as [`Frame`], [`ChannelCapability`] and request results own their
//! data. Channel/request owners instead retain native resources independently of
//! their parents. [`ChannelView`] is the exception: it is weak and cannot keep a
//! provider alive. Explicit close reports errors; Drop is best-effort. Neither
//! promises immediate physical teardown when native resources must be retained.
//! Submission, provider acceptance, terminal completion and actual hardware
//! execution are distinct; an `Ok` request is not proof of any later stage.
//!
//! Common Channel coverage (Task 032B3, RUST-IR-001):
//!
//! ```text
//! safe Rust common Channel:      C2 + Image + Health + Stacked
//! native C / safe Ada common:    C2/Image/Health/Instrumentation/Track/Scheduling/Stacked
//! ```
//!
//! Safe Rust has no typed Instrumentation or Track owners, so no common
//! Channel view of those families is offered here.
//!
//! IR HealthAndStatus (RUST-IR-001): [`HealthStatusChannel`] covers Open,
//! explicit Enable, capability snapshots, retryable Close, and its weak
//! [`ChannelView`].
//! [`StackedChannel`] provides the required fourth-channel foundation: owned
//! [`StackedConfig`], discovery-derived attachment, explicit Enable, complete
//! capabilities and weak common requests. It does not provide acquisition or
//! metadata callback registration.
//!
//! IR HealthStatus metadata (RUST-IR-002):
//! [`HealthStatusChannel::open_metadata`] returns an independent
//! [`HealthMetadataStream`] that polls the native bounded queue and returns
//! fully Rust-owned [`HealthMetadataEvent`]s for all six HealthStatus metadata
//! families (MFA Status, BIT Status, Subsystem Status, Discrete Status,
//! Security Audit, MFA Status Detailed), plus [`HealthMetadataCounters`].
//! IR Image metadata (RUST-IR-004): [`ImageStream::open_metadata`] returns an
//! independent [`ImageMetadataStream`] with fully owned [`ImageMetadataEvent`]s
//! for BadPixelList, LineOfSightReport, LineOfSightEuler and NavigationReportResp.
//! This covers the four currently C-bridged families, not every upstream Image
//! callback.
//!
//! IR NavigationReport submission (RUST-IR-008):
//! [`ImageStream::submit_navigation_report`] copies a complete [`NavigationReport`]
//! and returns a unique [`NavigationRequest`]. Its cached [`NavigationResult`]
//! distinguishes a complete response from explicit provider rejection. Zero wait
//! polls; timeout and Close/Drop do not cancel work. The request can outlive its
//! ImageStream and Session, and is neither Clone, Send nor Sync.
//!
//! IR C2 metadata (RUST-IR-005): [`ControlChannel::open_metadata`] returns an
//! independent [`C2MetadataStream`] for owned CommandStatus, BIT configuration,
//! canonical Health BIT status and separately opted-in communications reports.
//! Accepting a command is not proof of physical completion.
//!
//! Optional Image/Health NUC_TempData (RUST-IR-003): [`ImageStream::open_nuc`]
//! and [`HealthStatusChannel::open_nuc`] return the same [`NucStream`], copying
//! all publicly readable TempPoint fields into owned values. The inaccessible
//! upstream optional `tempError` is not represented or inferred. Optional
//! registration returns are available through [`NucOpenError::registration`].
//!
//! Optional Image OpticalDistortionMap (RUST-IR-006):
//! [`ImageStream::open_optical_distortion_map`] returns an independent
//! [`OpticalDistortionMapStream`] with complete owned [`OpticalDistortionMap`]
//! values, reusable [`ForwardRightDown`] and [`RowCol`] components, and structured
//! optional-registration failures via [`OpticalDistortionMapOpenError`].
//!
//! RF Admin state control (RUST-MEL-001): [`RfAdmin`] is an independent RF
//! `createAdminMEL` owner (not a Session, RF DataMEL, or RF C2 owner) that
//! commands a published [`RfMfaState`]. Its Boolean result is provider
//! acceptance only, not evidence of a hardware state transition.
//!
//! RF DataMEL foundation (RUST-MEL-002): [`RfData`] is an independent RF
//! `createDataMEL` owner (not a Session or RF Admin owner) offering provider
//! version, live provider-defined duration quantization, and fully Rust-owned
//! [`RfMfaInfo`] capability snapshots that outlive the owner and the provider
//! library. No other RF MEL facility (ProductRx, C2, VA, Job, PhysicalData,
//! Tx power modes) is exposed by this crate.

use std::ffi::{c_char, CString};
use std::mem;
use std::path::Path;
use std::ptr;
use std::rc::Rc;
use std::slice;

use ams_mel_sys as sys;

mod ir_c2_metadata;
mod ir_comms_metadata;
mod ir_discovery;
mod ir_health;
mod ir_health_metadata;
mod ir_image_metadata;
mod ir_los_quaternion;
mod ir_navigation;
mod ir_stacked;
pub use ir_navigation::{
    AttitudeRate, NavigationReport, NavigationRequest, NavigationResult, NorthEastDown,
    PositionSolutionState, PositionVelocityCovariance,
};
pub mod ir_nuc;
mod ir_optical_distortion_map;
#[cfg(test)]
mod test_allocator;
pub use ir_los_quaternion::{
    LineOfSightQuaternion, LineOfSightQuaternionOpenError, LineOfSightQuaternionStream, Quaternion,
};
pub use ir_optical_distortion_map::{
    ForwardRightDown, OpticalDistortionMap, OpticalDistortionMapOpenError,
    OpticalDistortionMapStream, RowCol,
};
mod ir_bit;
mod ir_config_set;
mod ir_mode;
#[doc(inline)]
pub use ams_mel_rf::{RfAdmin, RfMfaState};
#[doc(inline)]
pub use ams_mel_rf::{RfData, RfFaceInfo, RfFrequencyRange, RfJobDataFormat, RfMfaInfo};
pub use ir_bit::{BitAction, BitCommand};
pub use ir_c2_metadata::{
    BitConfiguration, BitControlInterface, BitTypeDefinition, C2MetadataCounters, C2MetadataEvent,
    C2MetadataStream, CannotComply, CommandState, CommandStatus,
};
pub use ir_comms_metadata::{CommsMetadataCounters, CommsMetadataOpenError, CommsMetadataStream};
pub use ir_config_set::ConfigSetCommand;
pub use ir_health::{HealthConfig, HealthStatusChannel};
pub use ir_health_metadata::{
    About, ActiveBit, BitResult, BitStatus, CompletedBit, CompletedBitItem, ComponentState,
    CsciInfo, CsciMode, CsciVersion, Euler, FailureLevel, Fault, FaultAmbiguityGroup, FaultData,
    FaultSeverity, FaultState, ForeignKey, HealthMetadataCounters, HealthMetadataEvent,
    HealthMetadataKind, HealthMetadataStream, InstallationDetails, MfaComponent, MfaStatus,
    NameValuePair, SecurityArtifact, SecurityAuditRecord, SecurityAuthenticationCategory,
    SecurityEvent, SecurityEventKind, SecurityFileManagementCategory, SecurityIntegrityCategory,
    SecurityKeyManagementCategory, SecurityOutcome, SecuritySanitizationCategory, SecuritySeverity,
    SecuritySystemCategory, StateTransitionStatus, SubsystemDependency, SubsystemStatus,
    TemperatureState, TemperatureStatus,
};
pub use ir_image_metadata::{
    AzimuthElevation, BadPixel, BadPixelList, BadPixelReason, ImageMetadataCounters,
    ImageMetadataEvent, ImageMetadataStream, LineOfSightEuler, LineOfSightReport,
    NavigationResponse,
};
pub use ir_mode::{
    CoordinateFrameReference, DegradationMethod, MfaState, ModeCommand, ScanParameters, ScanType,
};
pub use ir_nuc::{
    NucCounters, NucOpenError, NucPixelValue, NucPointType, NucStream, NucTempData, NucTempPoint,
};
pub use ir_stacked::{StackedChannel, StackedConfig};

const DIAGNOSTIC_CAPACITY: usize = 4096;
const WAIT_DIAGNOSTIC_CAPACITY: usize = 512;

pub use ams_mel_common::{AbiVersion, ComponentLocation, Error, ErrorKind, ProviderVersion, UciId};

/// Owned configuration for strict Mono8 acquisition via [`Session::open_image_stream`].
///
/// This configures host buffers and a bounded frame FIFO, not an image format
/// conversion. Use [`ImageControlConfig`] when only attachment/control is needed.
/// Construction is provider-free; opening checks the provider's actual layout.
///
/// ```
/// use ams_mel::{ComponentLocation, ImageConfig, UciId};
/// let channel = UciId::new([1; 16], "image")?;
/// let platform = UciId::new([2; 16], "platform")?;
/// let location = ComponentLocation::new(0.0, 0.0, 0.0, "sensor", "platform")?;
/// let config = ImageConfig::with_limits(channel, platform, location, 3, 1_048_576, 4)?;
/// # let _ = config;
/// # Ok::<(), ams_mel::Error>(())
/// ```
#[derive(Clone, Debug, PartialEq)]
pub struct ImageConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
    buffer_count: usize,
    buffer_size: usize,
    queue_capacity: usize,
}

/// Owned identity and location for an Image control-only attachment.
///
/// There is deliberately no acquisition layout or buffer configuration. Labels
/// and location text use the existing [`UciId`] and [`ComponentLocation`]
/// validation and are retained verbatim, alongside UUID bytes and metre offsets.
#[derive(Clone, Debug, PartialEq)]
pub struct ImageControlConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl ImageControlConfig {
    /// Takes ownership of identities and location; does not contact a provider.
    /// Open with [`Session::open_image_control`], not the acquisition method.
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

/// Owned identities/location for a CommandAndControl attachment.
///
/// Open with [`Session::open_control_channel`], then explicitly
/// [`enable`](ControlChannel::enable) before submitting commands.
#[derive(Clone, Debug, PartialEq)]
pub struct ControlConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl ControlConfig {
    /// Takes ownership of identities and location without contacting a provider.
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

impl ImageConfig {
    /// Uses three host buffers of 1,048,576 bytes each and a four-frame FIFO.
    /// No provider is contacted and no host image buffers are allocated here.
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
            buffer_count: 3,
            buffer_size: 1_048_576,
            queue_capacity: 4,
        }
    }

    /// Sets buffer count, bytes per buffer, and frame FIFO capacity explicitly.
    ///
    /// Each must be in `1..=usize::MAX`. The constructor does not check whether
    /// storage fits a provider's image or whether allocations can succeed at Start.
    ///
    /// # Errors
    /// Returns [`ErrorKind::InvalidArgument`] if any limit is zero.
    pub fn with_limits(
        channel_id: UciId,
        platform_id: UciId,
        sensor_location: ComponentLocation,
        buffer_count: usize,
        buffer_size: usize,
        queue_capacity: usize,
    ) -> Result<Self, Error> {
        for (value, name) in [
            (buffer_count, "buffer_count"),
            (buffer_size, "buffer_size"),
            (queue_capacity, "queue_capacity"),
        ] {
            if value == 0 {
                return Err(Error::new(
                    ErrorKind::InvalidArgument,
                    format!("{name} must be nonzero"),
                ));
            }
        }
        Ok(Self {
            channel_id,
            platform_id,
            sensor_location,
            buffer_count,
            buffer_size,
            queue_capacity,
        })
    }
}

/// Provider-reported acquisition geometry; unrecognized values remain numeric.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageType {
    /// Staring acquisition.
    Staring,
    /// Scanning acquisition.
    Scanning,
    /// Unrecognized raw 32-bit image type.
    Unknown(u32),
}

/// Provider-reported flip metadata; the binding does not rearrange pixels.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageFlip {
    /// No flip reported.
    None,
    /// Vertical flip reported.
    Vertical,
    /// Horizontal flip reported.
    Horizontal,
    /// Both vertical and horizontal flips reported.
    Both,
    /// Unrecognized raw 32-bit flip value.
    Unknown(u32),
}

/// Multifunction-aperture mode returned by a mode request.
///
/// A returned mode is a provider response, not independent evidence of hardware state.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MfaMode {
    /// Provider's unused mode.
    Unused,
    /// Task scheduling mode.
    TaskSched,
    /// Scan-volume scheduling mode.
    ScanVolumeSched,
    /// Scan-bar scheduling mode.
    ScanBarSched,
    /// Unrecognized raw 32-bit mode, preserved rather than guessed.
    Unknown(u32),
}

/// Published MEL `Return` value inside [`ReturnResult::Completed`].
///
/// Completion can carry a failure value. Unknown raw values are reported as
/// [`ErrorKind::ProviderFailed`], not coerced to [`Self::Fail`].
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CommandReturn {
    /// Provider-reported success; not proof of physical execution.
    Success,
    /// Provider-reported bad pointer.
    BadPointer,
    /// Provider-reported failure, still a normal completed result.
    Fail,
    /// Operation is not supported by this provider.
    NotSupported,
    /// Operation is not implemented by this provider.
    NotImplemented,
}

/// Provider MEL rejection code, distinct from the façade's [`ErrorKind`].
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MelErrorCode {
    /// No MEL error indicated.
    None,
    /// An identifier was invalid.
    InvalidId,
    /// The provider state did not permit the command.
    InvalidState,
    /// Command parameters were invalid.
    InvalidParameters,
    /// Required permissions were absent.
    InsufficientPermissions,
    /// Required resources were unavailable.
    InsufficientResources,
    /// Required local resources were unavailable.
    InsufficientLocalResources,
    /// Required remote resources were unavailable.
    InsufficientRemoteResources,
    /// The command was unsupported.
    Unsupported,
    /// Unrecognized raw 32-bit rejection code.
    Unknown(u32),
}

/// Cached terminal mode outcome; rejection is an `Ok` result of [`ModeRequest::wait`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ModeResult {
    /// The provider returned a mode successfully, without proving hardware execution.
    Success {
        /// Mode supplied by the provider.
        mode: MfaMode,
    },
    /// The provider explicitly rejected the command.
    Rejected {
        /// MEL rejection category, including unknown numeric codes.
        code: MelErrorCode,
        /// Complete UTF-8 rejection description.
        description: String,
    },
}

/// Cached terminal `Return` outcome, distinct from submission and wait failures.
///
/// ```
/// use ams_mel::{CommandReturn, ReturnResult};
/// let result = ReturnResult::Completed { value: CommandReturn::Fail };
/// assert!(matches!(result, ReturnResult::Completed { value: CommandReturn::Fail }));
/// // A completed future is not necessarily a successful command.
/// ```
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ReturnResult {
    /// The future completed with a MEL `Return` (which may indicate failure).
    Completed {
        /// Provider's return value, not a hardware-execution certificate.
        value: CommandReturn,
    },
    /// The provider explicitly rejected the command rather than returning a value.
    Rejected {
        /// MEL rejection category.
        code: MelErrorCode,
        /// Complete UTF-8 rejection description.
        description: String,
    },
}

/// Outbound inherited `ChannelCommsTestReq`. Named fields prevent accidental
/// Command/Channel/Request ID ordering mistakes. All 32 bits are preserved.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommsTestRequest {
    /// Caller-supplied command identifier; all `u32` values are preserved.
    pub command_id: u32,
    /// Numeric target channel identifier, not a [`UciId`] UUID.
    pub channel_id: u32,
    /// Caller-supplied communications-test correlation identifier.
    pub request_id: u32,
}

/// Inherited `ChannelCommsTestRep`. The upstream reply carries no Channel ID,
/// so none is reported.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommsTestReport {
    /// Command identifier returned by the provider, without invented correlation.
    pub command_id: u32,
    /// Request identifier returned by the provider.
    pub request_id: u32,
}

/// Terminal CommsTest outcome. MEL command rejection is a normal terminal
/// result, not a Rust `Err`, matching [`ModeResult`] and [`ReturnResult`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CommsTestResult {
    /// The provider supplied a communications report, not proof of hardware operation.
    Completed {
        /// Owned reply identifiers; upstream supplies no Channel ID here.
        report: CommsTestReport,
    },
    /// The provider explicitly rejected the communications test.
    Rejected {
        /// MEL rejection category.
        code: MelErrorCode,
        /// Complete UTF-8 rejection description.
        description: String,
    },
}

/// `ams_mel_ir_pixel_format_t`. Future raw values are preserved as `Unknown`.
///
/// ```
/// use ams_mel::PixelFormat;
/// let format = PixelFormat::Unknown(u32::MAX);
/// let raw = match format {
///     PixelFormat::Unknown(raw) => Some(raw),
///     _ => None, // also handles known and future variants of this non-exhaustive enum
/// };
/// assert_eq!(raw, Some(u32::MAX));
/// ```
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum PixelFormat {
    /// Monochrome format; this tag alone does not specify bit depth.
    Mono,
    /// Red/green/blue format; not supported by the current Mono8 receiver.
    Rgb,
    /// Bayer mosaic format; not converted by the current receiver.
    Bayer,
    /// Unrecognized raw 32-bit format; not silently converted to Mono.
    Unknown(u32),
}

/// `ams_mel_ir_sensor_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum SensorType {
    /// Provider does not specify a sensor capability.
    Unspecified,
    /// Horizontal gimbal capability.
    GimbalHorizontal,
    /// Vertical gimbal capability.
    GimbalVertical,
    /// Gimbal rotation capability.
    GimbalRotation,
    /// Step-and-stare capability.
    StepStare,
    /// Unrecognized raw 32-bit sensor capability.
    Unknown(u32),
}

/// `ams_mel_ir_channel_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum ChannelType {
    /// IRST track channel (no safe Rust typed owner here).
    IrstTrack,
    /// IRST image channel, represented by [`ImageStream`].
    IrstImage,
    /// C2 channel, represented by [`ControlChannel`].
    CommandAndControl,
    /// Scheduling channel (no safe Rust typed owner here).
    Scheduling,
    /// Health/status channel, represented by [`HealthStatusChannel`].
    HealthAndStatus,
    /// Instrumentation channel (no safe Rust typed owner here).
    Instrumentation,
    /// Stacked-image channel; not a Mono8 acquisition promise.
    StackedImage,
    /// Upstream reserved channel tag 1.
    Reserved1,
    /// Upstream reserved channel tag 2.
    Reserved2,
    /// Unrecognized raw 32-bit channel tag.
    Unknown(u32),
}

/// `ams_mel_ir_channel_metadata_capability_t`. Future raw values are preserved
/// as `Unknown`.
///
/// These are provider-advertised callback families, not promises that this Rust
/// crate exposes every family or that a callback has been registered.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum MetadataCapability {
    /// Bad-pixel list metadata.
    BadPixelList,
    /// Optical distortion map metadata.
    OpticalDistortionMap,
    /// Local-function status metadata.
    LfStatus,
    /// Line-of-sight report metadata.
    LineOfSightReport,
    /// Quaternion line-of-sight metadata.
    LineOfSightQuaternion,
    /// Euler-angle line-of-sight metadata.
    LineOfSightEuler,
    /// Multifunction-aperture status metadata.
    MfaStatus,
    /// Detailed multifunction-aperture status metadata.
    MfaStatusDetailed,
    /// Built-in-test configuration metadata.
    BitConfiguration,
    /// Command status metadata, separate from request acceptance/completion.
    CommandStatus,
    /// Built-in-test status metadata.
    BitStatus,
    /// Candidate-object message metadata.
    CandidateObjectMessage,
    /// Task-executing report metadata.
    TaskExecutingRep,
    /// Subsystem status response metadata.
    SubsystemStatusResp,
    /// Execute-task acknowledgement metadata.
    ExecuteTaskAck,
    /// Schedule-created report metadata.
    SchedCreatedRep,
    /// IRST track report metadata.
    IrstTrackReport,
    /// Channel communications-test report metadata.
    ChannelCommsTestRep,
    /// Camera-command response metadata.
    CameraCommandResp,
    /// Camera-protect-command response metadata.
    CameraProtectCmdResp,
    /// Instrumentation report metadata.
    InstrumentationReport,
    /// Navigation-report response metadata.
    NavigationReportResp,
    /// System-track-data request metadata.
    RequestSystemTrackData,
    /// Track-list-update response metadata.
    UpdateTrackListResponse,
    /// Three-dimensional line-of-sight kinematics metadata.
    Los3dKinematicsType,
    /// Preprocessed candidate-object message metadata.
    CandidateObjectPreprocMessage,
    /// Task-event metadata.
    TaskEvents,
    /// Scan-performance report metadata.
    ScanPerformanceReport,
    /// Upstream reserved metadata tag 3; no payload semantics inferred.
    Reserved3,
    /// Upstream reserved metadata tag 5; no payload semantics inferred.
    Reserved5,
    /// Upstream reserved metadata tag 9; no payload semantics inferred.
    Reserved9,
    /// Upstream reserved metadata tag 10; no payload semantics inferred.
    Reserved10,
    /// Unrecognized raw 32-bit metadata capability.
    Unknown(u32),
}

/// `ams_mel_ir_band_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum BandType {
    /// Provider marks the band invalid.
    Invalid,
    /// Multiple spectral bands.
    Multiband,
    /// Far infrared band; no wavelength boundary is inferred from this tag.
    IrFar,
    /// Near infrared band.
    IrNear,
    /// Long-wave infrared band.
    IrLongwave,
    /// Mid-wave infrared band.
    IrMidwave,
    /// Short-wave infrared band.
    IrShortwave,
    /// Visible white band.
    VisibleWhite,
    /// Visible red band.
    VisibleRed,
    /// Visible green band.
    VisibleGreen,
    /// Visible blue band.
    VisibleBlue,
    /// Ultraviolet A band.
    Uva,
    /// Ultraviolet B band.
    Uvb,
    /// Ultraviolet C band.
    Uvc,
    /// Vacuum ultraviolet band.
    UvVacuum,
    /// Unrecognized raw 32-bit band type.
    Unknown(u32),
}

/// `ams_mel_ir_coordinate_system_type_t`. Future raw values are preserved as
/// `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum CoordinateSystem {
    /// Latitude/longitude/altitude navigation frame.
    Lla,
    /// Earth-centred, Earth-fixed navigation frame.
    Ecef,
    /// North/east/down platform navigation frame.
    NedPlatform,
    /// North/east/down sensor navigation frame.
    NedSensor,
    /// Unrecognized raw 32-bit coordinate-system tag.
    Unknown(u32),
}

/// Provider-reported spectral band and wavelength bounds, without normalization.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BandInfo {
    /// Spectral classification; does not replace the numeric wavelength bounds.
    pub kind: BandType,
    /// Provider's minimum wavelength in metres, copied as `f64`.
    pub min_wavelength_m: f64,
    /// Provider's maximum wavelength in metres, copied as `f64`.
    pub max_wavelength_m: f64,
}

/// One image-band map entry, associating a frame band index with spectral bands.
#[derive(Clone, Debug, PartialEq)]
pub struct ImageBand {
    /// Band-map key, correlating to [`Frame::band_index`].
    pub band_index: u32,
    /// Ordered provider band descriptions for that key; may be empty.
    pub bands: Vec<BandInfo>,
}

/// Complete, fully Rust-owned copy of `ams_mel_ir_channel_capability_v1`. No
/// native pointer survives [`ChannelView::capabilities`]; the value remains
/// valid after the View, typed owner, Session, and provider library are gone.
///
/// Zero/default layout fields are preserved, not treated as an operational
/// image source. Numeric fields are not physical validation or support checks.
#[derive(Clone, Debug, PartialEq)]
pub struct ChannelCapability {
    /// Channel's complete UUID and descriptive label.
    pub channel_id: UciId,
    /// Provider-reported image-buffer height in pixels.
    pub height: u32,
    /// Provider-reported image-buffer width in pixels.
    pub width: u32,
    /// Dynamic-range bit depth (for example, ADC precision), not byte stride.
    pub bit_depth: u32,
    /// Image-buffer row pitch in bytes.
    pub row_pitch: u32,
    /// Provider-reported buffer size in bytes.
    pub buffer_size: u32,
    /// Provider-reported image-size value; no unit conversion is performed.
    /// The pinned getter does not specify a unit independently of the provider.
    pub image_size: u32,
    /// Number of EO/IR bands produced by the aperture.
    pub number_of_bands: u32,
    /// Advertised pixel format; independent of acquisition support in this crate.
    pub pixel_format: PixelFormat,
    /// Ordered sensor capability tags, including unspecified/unknown values.
    pub sensor_types: Vec<SensorType>,
    /// Platform's complete UUID and descriptive label.
    pub platform_id: UciId,
    /// Sensor offsets in metres and provider reference text.
    pub sensor_location: ComponentLocation,
    /// Provider-advertised channel types; not all have safe Rust owners.
    pub channel_types: Vec<ChannelType>,
    /// Queued-task depth. Zero denotes application-managed scheduling, not absence.
    /// In that mode a new IMMEDIATE task replaces the executing task upstream.
    pub task_schedule_depth: u32,
    /// Provider indicates an optical distortion map is available.
    pub odc_available: bool,
    /// Provider indicates that it performs non-uniformity correction.
    pub nuc_available: bool,
    /// Advertised callback families; reading this does not register callbacks.
    pub metadata_capabilities: Vec<MetadataCapability>,
    /// Image-band map copied into owned entries, without invented missing bands.
    pub image_bands: Vec<ImageBand>,
    /// Ordered navigation frame tags; empty can mean no internal navigation data.
    pub nav_frames: Vec<CoordinateSystem>,
}

/// Fully Rust-owned Mono8 frame returned by [`ImageStream::receive`].
///
/// Pixels are copied, not a borrowed provider buffer or native lease. The frame
/// remains valid after stream/session closure and does not keep a provider alive.
/// Metadata is preserved without image flipping, coordinate conversion or an
/// inferred time epoch. All integer fields retain their full declared width.
#[derive(Clone, Debug, PartialEq)]
pub struct Frame {
    /// Provider system time in signed nanoseconds; no epoch is inferred.
    /// Upstream associates staring frames with integration start and scanning
    /// (TDI) frames with the first image line.
    pub system_time_ns: i64,
    /// Integration duration in signed nanoseconds, without clamping.
    pub integration_time_ns: i64,
    /// Image width in pixels.
    pub width: u32,
    /// Image height in pixels.
    pub height: u32,
    /// Storage bits per pixel; eight for the supported acquisition profile.
    pub bits_per_pixel: u32,
    /// Verbatim upstream `getNumBands()` value. The pin's field name suggests a
    /// count but its comment describes a spectral-band indication; not reinterpreted.
    pub number_of_bands: u32,
    /// Horizontal field of view in radians.
    pub horizontal_fov_rad: f64,
    /// Vertical field of view in radians.
    pub vertical_fov_rad: f64,
    /// Provider's frame identifier, without wraparound interpretation.
    pub frame_id: u32,
    /// Provider's subframe identifier, without an inferred indexing convention.
    pub subframe_id: u32,
    /// Provider's total subframe count.
    pub subframe_total: u32,
    /// Staring/scanning classification, retaining unknown values.
    pub image_type: ImageType,
    /// Reported flip state; pixels are not rearranged by the binding.
    pub image_flip: ImageFlip,
    /// Bridge bitmask of provider image flags, not an unvalidated provider integer.
    /// Bit `n` represents upstream flag `n` (the native bridge accepts 0 through 3).
    pub image_flags: u32,
    /// Microscanning row displacement in row coordinates, copied unchanged.
    /// Positive `d` means an object at row Y appears at Y minus `d` upstream.
    pub dither_row: f64,
    /// Microscanning column displacement in column coordinates, copied unchanged.
    /// Positive `d` means an object at column X appears at X minus `d` upstream.
    pub dither_column: f64,
    /// Offset in rows to the start of this frame in sensor coordinates.
    pub row_offset: u32,
    /// Offset in columns to the start of this frame in sensor coordinates.
    pub column_offset: u32,
    /// Band-map key correlating to [`ChannelCapability::image_bands`].
    pub band_index: u8,
    /// Owned, row-major, tightly packed Mono8 bytes (`width * height` for accepted frames).
    pub pixels: Vec<u8>,
}

/// Saturating native image callback counters, not hardware frame-loss telemetry.
///
/// These count bridge observations over the stream lifetime, not only successful
/// [`ImageStream::receive`] calls. Provider-side buffer starvation is not a
/// queue-full drop counted here.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct StreamCounters {
    /// Image callbacks observed by the bridge, including rejected/dropped frames.
    pub frames_received: u64,
    /// Incoming frames discarded because the bounded bridge FIFO was full.
    pub frames_dropped_queue_full: u64,
    /// Frames rejected for malformed or unsupported headers, layouts or payloads.
    pub malformed_or_unsupported: u64,
}

/// Queries the loaded consumer façade's ABI version without loading a provider.
///
/// # Errors
/// Returns the native failure category if the façade query fails.
pub fn abi_version() -> Result<AbiVersion, Error> {
    let mut raw = sys::AmsMelAbiVersionV1::default();
    // SAFETY: `raw` is valid writable storage for the exact C record.
    let status = unsafe { sys::ams_mel_get_abi_version(&mut raw) };
    if status == sys::AMS_MEL_OK {
        Ok(AbiVersion {
            major: raw.major,
            minor: raw.minor,
        })
    } else {
        Err(error_from_status(status, Some(String::new()), None))
    }
}

/// Unique owner of an initialized IR provider session.
///
/// Loading uses the published C++ MEL interface behind the C façade; a compatible
/// provider/toolchain is still required. Instance and aperture selection are
/// provider-defined. Opening is not proof of hardware readiness or support for
/// every channel. This owner is neither `Send` nor `Sync`.
///
/// Child owners retain the native provider graph independently, so they may
/// outlive this Rust value. [`Self::close`] consumes the session; Drop attempts
/// best-effort cleanup without reporting errors. Neither guarantees immediate
/// provider unload: pending work and conservatively retained resources can defer
/// or prevent teardown. Prefer explicit closure when errors matter.
#[derive(Debug)]
pub struct Session {
    raw: *mut sys::AmsMelSession,
    _not_send_sync: Rc<()>,
}

/// Session-wide asynchronous admission limit. Zero (the default) is unlimited.
/// Refusal is explicit backpressure; the bridge neither queues nor retries.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Default)]
pub struct SessionOptions {
    /// Maximum admitted asynchronous requests across this session's families.
    /// Zero is unlimited; `1..=u32::MAX` applies a shared bound. Refusal is
    /// [`ErrorKind::ResourceExhausted`] before provider submission. Closing a
    /// pending request is not cancellation and need not free its admission slot.
    pub max_async_requests: u32,
}

impl Session {
    /// Loads and initializes a provider with unlimited asynchronous admission.
    ///
    /// Equivalent to [`Self::open_with_options`] with default [`SessionOptions`].
    /// The path and selection strings are borrowed only during this call.
    ///
    /// # Errors
    /// Rejects non-UTF-8 paths and embedded NUL as [`ErrorKind::InvalidArgument`].
    /// Loading, symbol, factory, initialization and caught provider failures retain
    /// their native category/diagnostic. A failed open returns no session owner.
    pub fn open(
        provider_library: impl AsRef<Path>,
        instance: impl AsRef<str>,
        aperture: impl AsRef<str>,
    ) -> Result<Self, Error> {
        Self::open_with_options(
            provider_library,
            instance,
            aperture,
            SessionOptions::default(),
        )
    }

    /// Loads the provider, obtains its manager/control and initializes selection.
    ///
    /// Options are copied at open. No download, provider installation, or automatic
    /// retry is performed. Success establishes a session, not hardware execution.
    ///
    /// This example is **compile-only**: an application supplies a compatible
    /// provider and its real selection strings; qualification does not run it.
    ///
    /// ```no_run
    /// use ams_mel::{Error, Session, SessionOptions};
    /// fn connect(path: &std::path::Path, instance: &str, aperture: &str)
    ///     -> Result<(), Error>
    /// {
    ///     let session = Session::open_with_options(
    ///         path, instance, aperture, SessionOptions { max_async_requests: 8 })?;
    ///     let version = session.provider_version()?;
    ///     println!("{}: {}", version.vendor, version.description);
    ///     session.close()
    /// }
    /// ```
    ///
    /// # Errors
    /// Has the path/text and provider failure contracts of [`Self::open`].
    /// Malformed successful output is [`ErrorKind::ProtocolInconsistency`]; any
    /// unexpected output owner on failure gets best-effort cleanup, not a guarantee
    /// of resource release when native ownership is uncertain.
    pub fn open_with_options(
        provider_library: impl AsRef<Path>,
        instance: impl AsRef<str>,
        aperture: impl AsRef<str>,
        options: SessionOptions,
    ) -> Result<Self, Error> {
        let options = sys::AmsMelSessionOptionsV1 {
            max_async_requests: options.max_async_requests,
        };
        let library = c_path(provider_library.as_ref())?;
        let instance = c_string(instance.as_ref(), "instance")?;
        let aperture = c_string(aperture.as_ref(), "aperture")?;
        let mut raw = ptr::null_mut();

        let status = call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: all C strings live through the call; `raw` is an initially
            // null writable owner and diagnostic storage matches its capacity.
            unsafe {
                sys::ams_mel_session_open_with_options(
                    library.as_ptr(),
                    instance.as_ptr(),
                    aperture.as_ptr(),
                    &options,
                    &mut raw,
                    buffer,
                    capacity,
                    required,
                )
            }
        });
        match status {
            Ok(()) if raw.is_null() => Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "session open succeeded without returning an owner",
            )),
            Ok(()) => Ok(Self {
                raw,
                _not_send_sync: Rc::new(()),
            }),
            Err(error) => {
                if !raw.is_null() {
                    // A malformed native implementation must still be given the
                    // opportunity to release an owner it published on failure.
                    best_effort_close(&mut raw);
                }
                Err(error)
            }
        }
    }

    /// Copies complete provider version information into Rust-owned values.
    /// The result does not borrow the session or keep the provider loaded.
    ///
    /// # Errors
    /// Propagates native/provider failures; malformed size/output protocols and
    /// invalid UTF-8 are errors, not truncated or lossy version strings.
    pub fn provider_version(&self) -> Result<ProviderVersion, Error> {
        let session = self.raw;
        query_provider_version(|version, buffer, capacity, required| {
            // SAFETY: the session owner is live and the helper passes a valid,
            // writable version record whose buffers match their capacities
            // (null/zero buffers are the documented required-size query).
            unsafe {
                sys::ams_mel_session_get_provider_version(
                    session, version, buffer, capacity, required,
                )
            }
        })
    }

    /// Opens strict Mono8 acquisition with nonzero buffer and queue settings.
    /// For attachment without acquisition, use [`Self::open_image_control`].
    ///
    /// Returns an attached, not yet started, independent owner. Call
    /// [`ImageStream::start`] explicitly. Identity/location values are copied;
    /// the configuration need not outlive this call.
    ///
    /// # Errors
    /// Unsupported/malformed provider layouts or attachment failures return native
    /// errors. No stream is published on failure; malformed success is
    /// [`ErrorKind::ProtocolInconsistency`]. Unexpected owners get best-effort cleanup.
    pub fn open_image_stream(&self, config: &ImageConfig) -> Result<ImageStream, Error> {
        let raw_config = raw_image_config(config);
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: the Session is live, the exact C record and all borrowed
            // string bytes remain valid for this call, and `raw` is writable.
            unsafe {
                sys::ams_mel_ir_stream_open(
                    self.raw,
                    &raw_config,
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_stream(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "stream open succeeded without a stream owner",
            ));
        }
        Ok(ImageStream {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Attaches an Image channel without acquisition buffers or a frame queue.
    ///
    /// The returned existing owner supports capabilities, required metadata,
    /// NavigationReport and common Channel operations. [`ImageStream::start`]
    /// enables the channel only; [`ImageStream::receive`] always returns the
    /// native control-only error. Zero capability layout values are preserved,
    /// not interpreted as a pixel format or an operational detector/image source.
    /// No optional metadata callbacks are registered automatically.
    ///
    /// Native errors retain their status category and diagnostic. Failed Open
    /// never publishes an ImageStream; any unexpected output owner gets one
    /// best-effort Close, leaving uncertain native state conservatively retained.
    ///
    /// # Errors
    /// Returns provider attachment/capability failures or binding protocol errors.
    /// Unlike acquisition Open, zero image layout fields alone are not rejected.
    ///
    /// ```no_run
    /// use ams_mel::{Error, ImageControlConfig, NavigationReport, Session};
    ///
    /// // The caller supplies the provider's identities and a complete report.
    /// // NotSet in that report does not assert an actual INS position solution.
    /// fn control_only(session: Session, config: ImageControlConfig,
    ///                 report: NavigationReport) -> Result<(), Error> {
    ///     let mut image = session.open_image_control(&config)?;
    ///     let mut view = image.channel_view()?;
    ///     let _actual_capabilities = view.capabilities()?; // zeros are preserved
    ///     let mut metadata = image.open_metadata(8)?; // four required families
    ///     let mut navigation = image.submit_navigation_report(&report)?;
    ///     let _outcome = navigation.wait(1_000)?; // inspect Success vs Rejected
    ///     navigation.close()?;
    ///     let mut keepalive = view.send_keepalive()?;
    ///     let _keepalive_result = keepalive.wait(1_000)?;
    ///     keepalive.close()?;
    ///     image.start()?; // enable only, never acquisition
    ///     assert!(image.receive(0).is_err());
    ///     // metadata.receive(0) polls: a provider need not deliver an event.
    ///     metadata.close()?;
    ///     view.close()?;
    ///     image.stop()?;
    ///     image.close()?;
    ///     session.close()
    /// }
    /// ```
    pub fn open_image_control(&self, config: &ImageControlConfig) -> Result<ImageStream, Error> {
        open_image_control_with(
            config,
            |config, out, diagnostic, capacity, required| {
                // SAFETY: live Session, exact writable output/diagnostic storage.
                // The borrowed config strings live through this one native call.
                unsafe {
                    sys::ams_mel_ir_image_control_open(
                        self.raw, config, out, diagnostic, capacity, required,
                    )
                }
            },
            best_effort_close_stream,
        )
    }

    /// Attaches a provider-advertised C2 channel without enabling it.
    ///
    /// The returned owner retains the provider independently of this session and
    /// copies the configuration. Call [`ControlChannel::enable`] explicitly.
    /// Capabilities are available through [`ControlChannel::channel_view`].
    ///
    /// # Errors
    /// Returns provider capability/attachment failures; no owner is published on
    /// failure. Malformed success is [`ErrorKind::ProtocolInconsistency`].
    pub fn open_control_channel(&self, config: &ControlConfig) -> Result<ControlChannel, Error> {
        let raw_config = raw_control_config(config);
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: the Session is live, the exact C record and all borrowed
            // string bytes remain valid for this call, and `raw` is writable.
            unsafe {
                sys::ams_mel_ir_c2_open(
                    self.raw,
                    &raw_config,
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_c2(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol("C2 open succeeded without a channel owner"));
        }
        Ok(ControlChannel {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Consumes the public session owner without invalidating independent children.
    ///
    /// Native destruction is ordered Control, manager, then library, once no
    /// retained graph needs them. This is not cancellation or an immediate-unload
    /// guarantee. Drop provides a best-effort fallback if an owner remains.
    ///
    /// # Errors
    /// Reports native cleanup failures. Because this method consumes `self`, an
    /// error does not return a Rust session for an explicit retry.
    pub fn close(mut self) -> Result<(), Error> {
        self.close_inner()
    }

    fn close_inner(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: `raw` is this Session's unique native owner. The facade
            // clears it when ownership is released.
            unsafe { sys::ams_mel_session_close(&mut self.raw, buffer, capacity, required) }
        })
    }
}

/// Unique Image attachment owner, for either Mono8 acquisition or control only.
/// Control-only attachment is not evidence of a working detector/image source.
/// Close/Drop use the same native deferred teardown and conservative retention
/// contract in both modes; requests can outlive this owner and its Session.
///
/// Open returns Attached. [`Self::start`] enables once, [`Self::receive`] copies
/// frames in acquisition mode, and [`Self::stop`] is terminal (not pause/restart).
/// [`Self::close`] consumes this Rust owner; Drop is best-effort and cannot report
/// cleanup failures. Neither this owner nor its requests are `Send` or `Sync`.
#[derive(Debug)]
pub struct ImageStream {
    raw: *mut sys::AmsMelIrStream,
    _not_send_sync: Rc<()>,
}

impl ImageStream {
    /// Registers BadPixelList, LineOfSightReport, LineOfSightEuler and
    /// NavigationReportResp, in that order, into one native drop-incoming FIFO.
    /// Valid while Attached or Running; prefer opening before Start to avoid
    /// missing early events. Callbacks may deliver synchronously during Open.
    ///
    /// Capacity is independent of the frame queue. Zero is rejected without
    /// consuming registration. Every actual attempt is one-shot, even failure;
    /// partial failure returns no owner. This does not affect NUC registration.
    /// A closed Image owner returns ProviderFailed before capacity validation.
    ///
    /// # Errors
    /// Returns native registration/lifecycle failures or invalid capacity errors.
    /// Failed actual registration still consumes the one-shot attempt; this method
    /// does not guarantee rollback of provider callback registration.
    pub fn open_metadata(&mut self, queue_capacity: usize) -> Result<ImageMetadataStream, Error> {
        if self.raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProviderFailed,
                "image stream is closed",
            ));
        }
        ImageMetadataStream::open_with(queue_capacity, |out, d, c, r| {
            // SAFETY: exclusively borrowed typed owner and writable output slots.
            unsafe { sys::ams_mel_ir_image_metadata_open(self.raw, queue_capacity, out, d, c, r) }
        })
    }

    /// Registers the optional NUC callback independently of ordinary metadata.
    /// Zero capacity is rejected before registration; every actual attempt
    /// consumes the native one-shot, including failure. No automatic retries.
    /// Synchronous registration events are available immediately after Open.
    pub fn open_nuc(&mut self, queue_capacity: usize) -> Result<NucStream, NucOpenError> {
        NucStream::open_with(queue_capacity, |out, registration, d, c, r| {
            // SAFETY: exclusively borrowed typed owner, writable output slots.
            unsafe {
                sys::ams_mel_ir_image_nuc_open(self.raw, queue_capacity, out, registration, d, c, r)
            }
        })
    }

    /// Registers optional OpticalDistortionMap independently of other metadata.
    /// Zero capacity fails locally without consuming registration. Every actual
    /// attempt is one-shot, including failure. Does not start acquisition.
    /// Closed owners fail locally with ProviderFailed before capacity validation.
    pub fn open_optical_distortion_map(
        &mut self,
        queue_capacity: usize,
    ) -> Result<OpticalDistortionMapStream, OpticalDistortionMapOpenError> {
        if self.raw.is_null() {
            return Err(OpticalDistortionMapOpenError::preflight(Error::new(
                ErrorKind::ProviderFailed,
                "image stream is closed",
            )));
        }
        OpticalDistortionMapStream::open_with(queue_capacity, |out, registration, d, c, r| {
            // SAFETY: exclusively borrowed Image owner and writable output slots.
            unsafe {
                sys::ams_mel_ir_image_odm_open(self.raw, queue_capacity, out, registration, d, c, r)
            }
        })
    }

    /// Registers conditional LineOfSightQuaternion independently of other metadata.
    /// Attached and Running streams are eligible; this does not start acquisition.
    /// Zero capacity fails locally without consuming the native one-shot. Every
    /// actual registration attempt consumes it, including failure. Closed owners
    /// fail locally with ProviderFailed before capacity validation.
    pub fn open_line_of_sight_quaternion(
        &mut self,
        queue_capacity: usize,
    ) -> Result<LineOfSightQuaternionStream, LineOfSightQuaternionOpenError> {
        if self.raw.is_null() {
            return Err(LineOfSightQuaternionOpenError::preflight(Error::new(
                ErrorKind::ProviderFailed,
                "image stream is closed",
            )));
        }
        LineOfSightQuaternionStream::open_with(queue_capacity, |out, registration, d, c, r| {
            // SAFETY: exclusively borrowed Image owner and writable output slots.
            unsafe {
                sys::ams_mel_ir_image_los_quaternion_open(
                    self.raw,
                    queue_capacity,
                    out,
                    registration,
                    d,
                    c,
                    r,
                )
            }
        })
    }

    /// Enables the channel. A control-only attachment allocates/registers no
    /// acquisition buffers and cannot become an image source through Start.
    ///
    /// Acquisition mode initializes/registers configured host buffers before
    /// enable. Repeating Start while Running succeeds without another enable;
    /// failed provider setup cannot be retried on this owner.
    ///
    /// # Errors
    /// Returns [`ErrorKind::StreamStopped`] after Stop, provider/setup failures
    /// on failed initialization, or native internal failures. Success does not
    /// establish that hardware will actually supply frames.
    pub fn start(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live stream handle.
            unsafe { sys::ams_mel_ir_stream_start(self.raw, diagnostic, capacity, required) }
        })
    }

    /// Receives one owned Mono8 frame, waiting at most `timeout_ms`. Zero polls.
    /// Control-only attachments return native `InitializationFailed` with a
    /// control-only diagnostic, never a frame or an ordinary timeout.
    ///
    /// `timeout_ms` is a finite `u32` millisecond interval (`0..=u32::MAX`), not
    /// an infinite-wait sentinel. It bounds the native queue wait, not all
    /// allocation/copy/cleanup time. Already queued frames drain before stopped
    /// or failed state is reported. Returned pixels have no provider dependency.
    ///
    /// # Errors
    /// [`ErrorKind::Timeout`] means no frame arrived; it does not stop acquisition.
    /// [`ErrorKind::StreamStopped`] means no queued frame remains after Stop.
    /// Other errors include provider failure, allocation failure and inconsistent
    /// native size/output protocols. No partial frame is returned.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<Frame, Error> {
        let mut raw = empty_frame();
        let first = receive_raw(self.raw, timeout_ms, &mut raw);
        match first {
            Err(error) if error.kind() == &ErrorKind::BufferTooSmall => {}
            Err(error) => return Err(error),
            Ok(()) => return Err(protocol("empty receive unexpectedly succeeded")),
        }
        if raw.pixel_required == 0 {
            return Err(protocol("native frame requires zero pixel bytes"));
        }
        let required = raw.pixel_required;
        let mut pixels = Vec::new();
        pixels.try_reserve_exact(required).map_err(|_| {
            Error::new(
                ErrorKind::InternalError,
                "unable to allocate frame pixel storage",
            )
        })?;
        pixels.resize(required, 0);
        raw.pixels = pixels.as_mut_ptr();
        raw.pixel_capacity = pixels.len();

        match receive_raw(self.raw, 0, &mut raw) {
            Ok(()) => {}
            Err(error) if error.kind() == &ErrorKind::BufferTooSmall => {
                return Err(protocol(
                    "queued frame pixel requirement changed during receive",
                ));
            }
            Err(error) if matches!(error.kind(), ErrorKind::Timeout | ErrorKind::StreamStopped) => {
                return Err(protocol("queued frame disappeared during receive"));
            }
            Err(error) => return Err(error),
        }
        if raw.pixel_required != required {
            return Err(protocol(
                "delivered frame pixel size changed during receive",
            ));
        }
        frame_from_raw(raw, pixels)
    }

    /// Copies the current bridge counters without consuming a frame.
    ///
    /// # Errors
    /// Returns native query failures. Counters are observations, not evidence of
    /// physical detector activity or complete provider-side loss accounting.
    pub fn counters(&self) -> Result<StreamCounters, Error> {
        let mut raw = sys::AmsMelIrStreamCountersV1::default();
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: the stream is live and `raw` is exact writable storage.
            unsafe {
                sys::ams_mel_ir_stream_get_counters(
                    self.raw, &mut raw, diagnostic, capacity, required,
                )
            }
        })?;
        Ok(StreamCounters {
            frames_received: raw.frames_received,
            frames_dropped_queue_full: raw.frames_dropped_queue_full,
            malformed_or_unsupported: raw.malformed_or_unsupported_frames,
        })
    }

    /// Stops accepting frames and initiates disable/detach teardown.
    ///
    /// Stop is terminal and idempotent after success; it is not a pause. Queued
    /// frames remain receivable. Pending requests/retained native buffers can
    /// defer physical teardown. Disable alone is not callback quiescence, and
    /// uncertain native resources are retained rather than unsafely released.
    ///
    /// # Errors
    /// Reports cleanup/provider failures (including [`ErrorKind::ProviderFailed`])
    /// or internal failures. An error is not a guarantee that resources were freed.
    pub fn stop(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: the stream handle remains uniquely owned by this wrapper.
            unsafe { sys::ams_mel_ir_stream_stop(self.raw, diagnostic, capacity, required) }
        })
    }

    /// Creates an independent weak common [`ChannelView`] of this Image stream.
    ///
    /// The View does not borrow or retain this `ImageStream`, its Session, or
    /// any provider object, and it does not alter Start/Stop/Close, frame
    /// receive, or provider lifetime.
    ///
    /// # Errors
    /// Returns native view-creation failures or malformed-output protocol errors.
    pub fn channel_view(&self) -> Result<ChannelView, Error> {
        if self.raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProviderFailed,
                "image stream is closed",
            ));
        }
        let source = self.raw;
        channel_view_from(
            |out, diagnostic, capacity, required| {
                // SAFETY: `source` is this wrapper's live stream owner, borrowed
                // only for the duration of the call; `out` is an initially null
                // writable view owner.
                unsafe {
                    sys::ams_mel_ir_channel_from_stream(source, out, diagnostic, capacity, required)
                }
            },
            "Image",
        )
    }

    /// Consumes the stream, stopping if needed and discarding unreceived frames.
    ///
    /// Requests survive; Close/Drop do not cancel them. Physical teardown may be
    /// deferred or resources conservatively retained. Unlike [`Self::stop`],
    /// this makes the queue inaccessible to the caller.
    ///
    /// # Errors
    /// Returns native cleanup failures. This consuming API cannot return an owner
    /// for retry; Drop attempts cleanup again only if the native handle remains.
    pub fn close(mut self) -> Result<(), Error> {
        self.close_inner()
    }

    fn close_inner(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: native close receives this wrapper's unique owner and
            // clears it according to the documented cleanup contract.
            unsafe { sys::ams_mel_ir_stream_close(&mut self.raw, diagnostic, capacity, required) }
        })
    }
}

impl Drop for ImageStream {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_stream(&mut self.raw);
        }
    }
}

/// Unique C2 attachment owner with explicit enable and asynchronous commands.
///
/// Neither `Send` nor `Sync`. The native owner retains the provider independently
/// of [`Session`]. Submission returns a request, not a claim of acceptance,
/// completion or physical execution. Inspect its terminal result separately.
/// Use [`Self::channel_view`] for common capabilities/KeepAlive/CommsTest.
///
/// [`Self::close`] is retryable while the handle remains open; Drop uses best-effort
/// cleanup. Requests may outlive both channel and session, deferring teardown.
#[derive(Debug)]
pub struct ControlChannel {
    raw: *mut sys::AmsMelIrC2,
    _not_send_sync: Rc<()>,
}

impl ControlChannel {
    /// Explicitly enables the attached C2 channel; submission never enables it.
    /// Repeating a successful Enable is idempotent.
    ///
    /// # Errors
    /// Reports provider refusal/exception or invalid lifecycle state. Failed
    /// enable is not automatically retried and is not evidence of hardware readiness.
    pub fn enable(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live C2 handle.
            unsafe { sys::ams_mel_ir_c2_enable(self.raw, diagnostic, capacity, required) }
        })
    }

    /// Submits exactly Operate/TaskSched with native default scan parameters.
    ///
    /// Preserves all 32 command-ID bits. For other profiles use [`Self::submit_mode`].
    /// Success publishes an independent [`ModeRequest`] without waiting for its
    /// outcome; use [`ModeRequest::wait`] to distinguish response from rejection.
    ///
    /// # Errors
    /// Returns lifecycle/provider/submission failures or
    /// [`ErrorKind::ResourceExhausted`] when session admission refuses work.
    pub fn submit_operate(&mut self, command_id: u32) -> Result<ModeRequest, Error> {
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live C2 handle and `raw` is
            // an initially null writable request owner.
            unsafe {
                sys::ams_mel_ir_c2_submit_operate(
                    self.raw, command_id, &mut raw, diagnostic, capacity, required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_request(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol("C2 submit succeeded without a request owner"));
        }
        Ok(ModeRequest {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Submits the empty/no-op BIT profile. For one-choice payloads use
    /// [`Self::submit_bit`]. This source-compatible convenience API is unchanged.
    ///
    /// The command ID retains all 32 bits. A returned request is not a successful
    /// BIT result; inspect [`ReturnRequest::wait`], including completed failure values.
    ///
    /// # Errors
    /// Returns lifecycle/provider failures, admission refusal
    /// ([`ErrorKind::ResourceExhausted`]), or malformed native output errors.
    pub fn submit_bit_noop(&mut self, command_id: u32) -> Result<ReturnRequest, Error> {
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live C2 handle and `raw` is
            // an initially null writable request owner.
            unsafe {
                sys::ams_mel_ir_c2_submit_bit_noop(
                    self.raw, command_id, &mut raw, diagnostic, capacity, required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_return_request(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol("BIT submit succeeded without a request owner"));
        }
        Ok(ReturnRequest {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Creates an independent weak common [`ChannelView`] of this C2 owner.
    ///
    /// The View does not borrow or retain this `ControlChannel`, its Session,
    /// or any provider object. A closed owner yields `ProviderFailed` without
    /// calling native code.
    ///
    /// # Errors
    /// Returns [`ErrorKind::ProviderFailed`] for a closed owner, native creation
    /// failures, or binding protocol errors for malformed successful output.
    pub fn channel_view(&self) -> Result<ChannelView, Error> {
        if self.raw.is_null() {
            return Err(Error::new(
                ErrorKind::ProviderFailed,
                "C2 channel is closed",
            ));
        }
        let source = self.raw;
        channel_view_from(
            |out, diagnostic, capacity, required| {
                // SAFETY: `source` is this wrapper's live C2 owner, borrowed only
                // for the duration of the call; `out` is an initially null
                // writable view owner.
                unsafe {
                    sys::ams_mel_ir_channel_from_c2(source, out, diagnostic, capacity, required)
                }
            },
            "C2",
        )
    }

    /// Closes this owner. A detach failure can leave it open for an explicit retry.
    ///
    /// Idempotent after success. Pending requests are not cancelled and may defer
    /// actual provider teardown. Use [`Self::is_open`] after an error; uncertain
    /// resources may remain retained even when cleanup cannot be completed.
    ///
    /// # Errors
    /// Reports native disable/detach or other cleanup failures. Unlike consuming
    /// close methods, this method leaves the Rust value available for retry.
    pub fn close(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: native close may clear or retain this unique owner according
            // to its documented retryable-detach contract.
            unsafe { sys::ams_mel_ir_c2_close(&mut self.raw, diagnostic, capacity, required) }
        })
    }

    /// Reports whether a native C2 handle remains, not whether Enable succeeded
    /// or the provider/hardware is healthy.
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }
}

impl Drop for ControlChannel {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_c2(&mut self.raw);
        }
    }
}

/// Unique asynchronous mode-response owner from C2 submission.
///
/// The admitted native graph outlives the originating [`ControlChannel`] and
/// [`Session`] as needed. [`Self::wait`] returns a cached terminal [`ModeResult`];
/// explicit provider rejection is a normal result, not a Rust error. Timeout and
/// Close/Drop never cancel provider work. Neither `Send` nor `Sync`.
///
/// Compile-only polling example: the caller supplies an already submitted request.
///
/// ```no_run
/// use ams_mel::{Error, ErrorKind, ModeRequest, ModeResult};
/// fn poll(request: &mut ModeRequest) -> Result<Option<ModeResult>, Error> {
///     match request.wait(0) {
///         Ok(result) => Ok(Some(result)), // inspect Success versus Rejected
///         Err(error) if error.kind() == &ErrorKind::Timeout => Ok(None),
///         Err(error) => Err(error),
///     }
/// }
/// ```
#[derive(Debug)]
pub struct ModeRequest {
    raw: *mut sys::AmsMelIrModeRequest,
    _not_send_sync: Rc<()>,
}

impl ModeRequest {
    /// Waits finitely for a cached terminal result. Zero polls; timeout is not cancellation.
    ///
    /// `timeout_ms` is milliseconds in `0..=u32::MAX`, with no infinite sentinel.
    /// Repeated terminal waits return the cached outcome without resubmission.
    /// [`ModeResult::Rejected`] is `Ok`, distinct from a wait/protocol failure.
    ///
    /// # Errors
    /// [`ErrorKind::Timeout`] leaves the request usable for another wait. Other
    /// errors preserve native failures or report invalid text/output protocols.
    pub fn wait(&mut self, timeout_ms: u32) -> Result<ModeResult, Error> {
        wait_for_mode(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
    ///
    /// Consumes `self` without waiting for completion. A provider future that
    /// never finishes may retain its native graph indefinitely. Drop is a
    /// best-effort fallback, not cancellation either.
    ///
    /// # Errors
    /// Returns native close failures; no Rust owner is returned for retry.
    pub fn close(mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the request and no wait can race
            // this consuming close through the safe API.
            unsafe {
                sys::ams_mel_ir_mode_request_close(&mut self.raw, diagnostic, capacity, required)
            }
        })
    }
}

impl Drop for ModeRequest {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_request(&mut self.raw);
        }
    }
}

/// Safe owner of one public asynchronous `RequestFor<Return>` outcome
/// (`ams_mel_ir_return_request *`).
///
/// It currently owns requests produced by:
///
/// - [`ControlChannel::submit_bit`] (one-choice C2 BIT),
/// - [`ControlChannel::submit_bit_noop`] (C2 BIT, empty/no-op convenience), and
/// - [`ControlChannel::submit_config_set`] (C2 opaque configuration text), and
/// - [`ChannelView::send_keepalive`] (common Channel KeepAlive, C2/Image/Health/Stacked).
///
/// The native request owns its admitted family graph independently of the
/// View, typed owner, and Session that created it. Wait is cached; timeout is
/// not cancellation; `Return::Fail` is a normal completion; MEL rejection is
/// [`ReturnResult::Rejected`]. Close/Drop never cancel provider work.
/// This owner is neither `Send` nor `Sync`; cleanup on Drop is best-effort and
/// cannot report errors. Completion is not proof of hardware execution.
#[derive(Debug)]
pub struct ReturnRequest {
    raw: *mut sys::AmsMelIrReturnRequest,
    _not_send_sync: Rc<()>,
}

impl ReturnRequest {
    /// Waits finitely for a cached terminal result. Zero polls; timeout is not cancellation.
    ///
    /// `timeout_ms` is milliseconds in `0..=u32::MAX`, with no infinite sentinel.
    /// Repeated terminal waits return the cached outcome. Both
    /// [`ReturnResult::Rejected`] and [`CommandReturn::Fail`] inside a completed
    /// result are `Ok` outcomes, not wait errors.
    ///
    /// # Errors
    /// [`ErrorKind::Timeout`] leaves the request usable for another wait. Other
    /// errors include provider failures, invalid UTF-8 and malformed output.
    /// Unknown `Return` values yield [`ErrorKind::ProviderFailed`].
    pub fn wait(&mut self, timeout_ms: u32) -> Result<ReturnResult, Error> {
        wait_for_return(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
    ///
    /// Consumes `self` without waiting. A native worker retains required provider
    /// state, possibly indefinitely if work never ends. Drop attempts best-effort
    /// cleanup and cannot report failures.
    ///
    /// # Errors
    /// Returns native close failures; no Rust owner is returned for retry.
    pub fn close(mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the request and no wait can race
            // this consuming close through the safe API.
            unsafe {
                sys::ams_mel_ir_return_request_close(&mut self.raw, diagnostic, capacity, required)
            }
        })
    }
}

impl Drop for ReturnRequest {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_return_request(&mut self.raw);
        }
    }
}

/// Weak common Channel view of one safe typed owner ([`ControlChannel`],
/// [`ImageStream`], [`HealthStatusChannel`], or [`StackedChannel`]).
///
/// Ownership model:
///
/// ```text
/// ChannelView
///     owns native ams_mel_ir_channel only
///
/// native ams_mel_ir_channel
///     owns only weak family state
/// ```
///
/// A `ChannelView` therefore retains no `ControlChannel`, `ImageStream`,
/// `Session`, provider Channel, Control, or provider library, and it has no
/// Rust lifetime tied to its source. Keeping a View alive never delays
/// provider teardown. After typed teardown, work/capability operations return
/// [`ErrorKind::ProviderFailed`]; [`Self::is_open`] still reports only view-handle
/// presence and [`Self::close`] remains an idempotent view-only operation.
///
/// Requests created through a View ([`ReturnRequest`], [`CommsRequest`]) own
/// their admitted native family graph; closing or dropping the View does not
/// cancel or invalidate them.
///
/// The View is intentionally neither `Send` nor `Sync`. Work, capability and
/// close operations take `&mut self`, serializing them on one view as required
/// by the native contract; [`Self::is_open`] is only a local handle check.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::ChannelView>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::ChannelView>();
/// ```
#[derive(Debug)]
pub struct ChannelView {
    raw: *mut sys::AmsMelIrChannel,
    _not_send_sync: Rc<()>,
}

impl ChannelView {
    /// Returns whether this Rust wrapper still owns a native weak view.
    ///
    /// This does NOT report whether the underlying typed family still exists:
    /// after typed owner teardown `is_open()` can remain `true` while
    /// operations return [`ErrorKind::ProviderFailed`].
    pub fn is_open(&self) -> bool {
        !self.raw.is_null()
    }

    /// Submits inherited `sendKeepAliveRep` and returns the existing public
    /// [`ReturnRequest`] owner.
    ///
    /// Submission is not a completed KeepAlive result. The request independently
    /// retains its admitted family graph, unlike this weak view.
    ///
    /// # Errors
    /// Returns [`ErrorKind::ProviderFailed`] for closed/expired views, admission
    /// refusal as [`ErrorKind::ResourceExhausted`], or native/protocol failures.
    pub fn send_keepalive(&mut self) -> Result<ReturnRequest, Error> {
        let channel = self.live("send_keepalive")?;
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `channel` is this wrapper's live weak view, exclusively
            // used through `&mut self`; `raw` is an initially null request owner.
            unsafe {
                sys::ams_mel_ir_channel_send_keepalive(
                    channel, &mut raw, diagnostic, capacity, required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_return_request(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol(
                "KeepAlive submit succeeded without a request owner",
            ));
        }
        Ok(ReturnRequest {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Submits inherited `ChannelCommsTestReq` with all three IDs preserved.
    ///
    /// Success yields an independent [`CommsRequest`], not proof that a test
    /// completed or any hardware communication occurred. Inspect its outcome.
    ///
    /// # Errors
    /// Returns [`ErrorKind::ProviderFailed`] for closed/expired views, admission
    /// refusal as [`ErrorKind::ResourceExhausted`], or native/protocol failures.
    pub fn submit_comms_test(&mut self, request: CommsTestRequest) -> Result<CommsRequest, Error> {
        let channel = self.live("submit_comms_test")?;
        let raw_request = sys::AmsMelIrChannelCommsTestRequestV1 {
            command_id: request.command_id,
            channel_id: request.channel_id,
            request_id: request.request_id,
        };
        let mut raw = ptr::null_mut();
        let result = call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `channel` is this wrapper's live weak view, exclusively
            // used through `&mut self`; the request record lives through the
            // call and `raw` is an initially null request owner.
            unsafe {
                sys::ams_mel_ir_channel_submit_comms_test(
                    channel,
                    &raw_request,
                    &mut raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        });
        if let Err(error) = result {
            if !raw.is_null() {
                best_effort_close_comms_request(&mut raw);
            }
            return Err(error);
        }
        if raw.is_null() {
            return Err(protocol(
                "CommsTest submit succeeded without a request owner",
            ));
        }
        Ok(CommsRequest {
            raw,
            _not_send_sync: Rc::new(()),
        })
    }

    /// Returns a complete, fully Rust-owned ChannelCapability snapshot. The
    /// native snapshot is converted while alive and then closed; no native
    /// pointer survives this call.
    ///
    /// Snapshotting does not enable the channel, register callbacks or establish
    /// hardware state. Zero/default values and unknown enum tags remain visible.
    ///
    /// # Errors
    /// Returns [`ErrorKind::ProviderFailed`] for a closed/expired view, native
    /// query failures, or binding errors for malformed spans, UTF-8 or records.
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error> {
        let channel = self.live("capabilities")?;
        capability_snapshot(|owner, diagnostic, capacity, required| {
            // SAFETY: `channel` is this wrapper's live weak view, exclusively
            // used through `&mut self`; `owner` is the helper's initially null
            // writable capability owner.
            unsafe {
                sys::ams_mel_ir_channel_get_capabilities(
                    channel, owner, diagnostic, capacity, required,
                )
            }
        })
    }

    /// Destroys only the weak native view. Idempotent; it never closes the
    /// typed owner, cancels requests, or changes the family lifecycle.
    ///
    /// Drop attempts the same view-only cleanup without reporting errors.
    ///
    /// # Errors
    /// Returns native close failures or [`ErrorKind::ProtocolInconsistency`] if
    /// success fails to clear the native owner. The value remains available.
    pub fn close(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this wrapper's unique weak view owner, or null,
            // which native Close accepts idempotently; native clears it.
            unsafe { sys::ams_mel_ir_channel_close(&mut self.raw, diagnostic, capacity, required) }
        })?;
        if !self.raw.is_null() {
            return Err(protocol(
                "channel view close succeeded without clearing the owner",
            ));
        }
        Ok(())
    }

    fn live(&self, operation: &str) -> Result<*mut sys::AmsMelIrChannel, Error> {
        if self.raw.is_null() {
            Err(Error::new(
                ErrorKind::ProviderFailed,
                format!("{operation} on a closed channel view"),
            ))
        } else {
            Ok(self.raw)
        }
    }
}

impl Drop for ChannelView {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_channel_view(&mut self.raw);
        }
    }
}

/// Safe owner of one public asynchronous inherited CommsTest outcome
/// (`ams_mel_ir_channel_comms_request *`).
///
/// The native request owns its admitted family graph independently of the
/// View, typed owner, and Session. Wait is cached and timeout is not
/// cancellation. Close and Drop are non-cancelling: pending provider work
/// survives public request Drop. It is intentionally neither `Send` nor `Sync`.
///
/// ```compile_fail
/// fn require_send<T: Send>() {}
/// require_send::<ams_mel::CommsRequest>();
/// ```
///
/// ```compile_fail
/// fn require_sync<T: Sync>() {}
/// require_sync::<ams_mel::CommsRequest>();
/// ```
#[derive(Debug)]
pub struct CommsRequest {
    raw: *mut sys::AmsMelIrChannelCommsRequest,
    _not_send_sync: Rc<()>,
}

impl CommsRequest {
    /// Waits finitely for a cached terminal result. Zero polls; timeout is not cancellation.
    ///
    /// `timeout_ms` is milliseconds in `0..=u32::MAX`, with no infinite sentinel.
    /// Repeated terminal waits return the cached report/rejection. Explicit MEL
    /// rejection is [`CommsTestResult::Rejected`] inside `Ok`, not a Rust error.
    /// The report contains only the identifiers the provider actually returned.
    ///
    /// # Errors
    /// [`ErrorKind::Timeout`] leaves the request usable. Other errors include
    /// provider failures, invalid UTF-8, and malformed native output protocols.
    pub fn wait(&mut self, timeout_ms: u32) -> Result<CommsTestResult, Error> {
        wait_for_comms(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
    ///
    /// Consumes `self` without waiting. The native worker retains necessary
    /// provider state, possibly indefinitely if work never finishes. Drop is a
    /// best-effort non-cancelling fallback and cannot report cleanup errors.
    ///
    /// # Errors
    /// Returns native close failures; no Rust owner is returned for retry.
    pub fn close(mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the request and no wait can race
            // this consuming close through the safe API.
            unsafe {
                sys::ams_mel_ir_channel_comms_request_close(
                    &mut self.raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })
    }
}

impl Drop for CommsRequest {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_comms_request(&mut self.raw);
        }
    }
}

// Private call seam for deterministic hostile-output qualification. Production
// uses the existing ABI; tests use live, independently controlled owner tokens.
trait CapabilityCalls {
    fn view(
        &self,
        owner: *mut sys::AmsMelIrChannelCapability,
        view: *mut *const sys::AmsMelIrChannelCapabilityV1,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> i32;
    fn close(
        &self,
        owner: *mut *mut sys::AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> i32;
}

struct NativeCapabilityCalls;

impl CapabilityCalls for NativeCapabilityCalls {
    fn view(
        &self,
        owner: *mut sys::AmsMelIrChannelCapability,
        view: *mut *const sys::AmsMelIrChannelCapabilityV1,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> i32 {
        // SAFETY: private callers supply a live owner and exact output storage.
        unsafe {
            sys::ams_mel_ir_channel_capability_view(owner, view, diagnostic, capacity, required)
        }
    }

    fn close(
        &self,
        owner: *mut *mut sys::AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        capacity: usize,
        required: *mut usize,
    ) -> i32 {
        // SAFETY: private callers transfer their unique owner exactly once.
        unsafe { sys::ams_mel_ir_channel_capability_close(owner, diagnostic, capacity, required) }
    }
}

/// Private RAII owner of one native capability snapshot. Drop closes the
/// native owner on every early return, including conversion/UTF-8 failures.
struct CapabilityOwner<'a, C: CapabilityCalls> {
    raw: *mut sys::AmsMelIrChannelCapability,
    calls: &'a C,
}

impl<C: CapabilityCalls> CapabilityOwner<'_, C> {
    fn close(&mut self) -> Result<(), Error> {
        // Disarm before release: even an error or malformed diagnostic may
        // follow consumption without clearing the output. Never retry it.
        let mut raw = mem::replace(&mut self.raw, ptr::null_mut());
        call_with_diagnostic(|diagnostic, capacity, required| {
            self.calls.close(&mut raw, diagnostic, capacity, required)
        })?;
        if !raw.is_null() {
            return Err(protocol(
                "capability close succeeded without clearing the owner",
            ));
        }
        Ok(())
    }
}

impl<C: CapabilityCalls> Drop for CapabilityOwner<'_, C> {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            let mut raw = mem::replace(&mut self.raw, ptr::null_mut());
            // Diagnostic-free secondary cleanup cannot replace the primary error.
            self.calls
                .close(&mut raw, ptr::null_mut(), 0, ptr::null_mut());
        }
    }
}

/// Shared complete ChannelCapability snapshot protocol used by
/// [`ChannelView::capabilities`] and typed owners such as
/// [`HealthStatusChannel::capabilities`].
///
/// `query` receives an initially null writable capability owner. The native
/// snapshot is guarded so that it is closed on every return path, its borrowed
/// record is converted into a fully Rust-owned value while alive, and no native
/// pointer survives this call.
fn capability_snapshot(
    query: impl FnOnce(*mut *mut sys::AmsMelIrChannelCapability, *mut c_char, usize, *mut usize) -> i32,
) -> Result<ChannelCapability, Error> {
    capability_snapshot_with(query, &NativeCapabilityCalls)
}

fn capability_snapshot_with(
    query: impl FnOnce(*mut *mut sys::AmsMelIrChannelCapability, *mut c_char, usize, *mut usize) -> i32,
    calls: &impl CapabilityCalls,
) -> Result<ChannelCapability, Error> {
    let mut owner = CapabilityOwner {
        raw: ptr::null_mut(),
        calls,
    };
    call_with_diagnostic(|diagnostic, capacity, required| {
        query(&mut owner.raw, diagnostic, capacity, required)
    })?;
    if owner.raw.is_null() {
        return Err(protocol(
            "capability query succeeded without a capability owner",
        ));
    }
    let mut view: *const sys::AmsMelIrChannelCapabilityV1 = ptr::null();
    call_with_diagnostic(|diagnostic, capacity, required| {
        calls.view(owner.raw, &mut view, diagnostic, capacity, required)
    })?;
    if view.is_null() {
        return Err(protocol(
            "capability view succeeded without a capability record",
        ));
    }
    if !(view as usize).is_multiple_of(mem::align_of::<sys::AmsMelIrChannelCapabilityV1>()) {
        return Err(protocol("capability view returned a misaligned record"));
    }
    // SAFETY: `view` is non-null and points to a record owned by the live
    // `owner`; it is copied before `owner` is closed.
    let raw = unsafe { *view };
    let value = capability_from_raw(&raw)?;
    owner.close()?;
    Ok(value)
}

fn channel_view_from(
    convert: impl FnOnce(*mut *mut sys::AmsMelIrChannel, *mut c_char, usize, *mut usize) -> i32,
    family: &str,
) -> Result<ChannelView, Error> {
    let mut raw = ptr::null_mut();
    let result = call_with_diagnostic(|diagnostic, capacity, required| {
        convert(&mut raw, diagnostic, capacity, required)
    });
    if let Err(error) = result {
        if !raw.is_null() {
            best_effort_close_channel_view(&mut raw);
        }
        return Err(error);
    }
    if raw.is_null() {
        return Err(protocol(&format!(
            "{family} channel view conversion succeeded without a view owner"
        )));
    }
    Ok(ChannelView {
        raw,
        _not_send_sync: Rc::new(()),
    })
}

impl Drop for Session {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close(&mut self.raw);
        }
    }
}

fn empty_version() -> sys::AmsMelProviderVersionV1 {
    sys::AmsMelProviderVersionV1 {
        api_version: 0,
        library_version: 0,
        vendor: ptr::null_mut(),
        vendor_capacity: 0,
        vendor_required: 0,
        description: ptr::null_mut(),
        description_capacity: 0,
        description_required: 0,
    }
}

/// Shared two-stage `ams_mel_provider_version_v1` protocol used by the IR
/// [`Session`] and RF [`RfData`] owners.
///
/// The first `query` passes null/zero string buffers and must report
/// `AMS_MEL_BUFFER_TOO_SMALL` with nonzero `*_required` counts (each including
/// the terminal NUL). Buffers of exactly those sizes are then allocated
/// fallibly, and the second `query` must succeed with unchanged counts. Each
/// copied buffer must hold exactly `required - 1` bytes of NUL-free UTF-8 and
/// a terminal NUL. Every other native status keeps its category and complete
/// diagnostic; malformed output is `ProtocolInconsistency` or `InvalidUtf8`.
fn query_provider_version(
    mut query: impl FnMut(*mut sys::AmsMelProviderVersionV1, *mut c_char, usize, *mut usize) -> i32,
) -> Result<ProviderVersion, Error> {
    let mut raw = empty_version();
    let first = call_with_diagnostic(|buffer, capacity, required| {
        query(&mut raw, buffer, capacity, required)
    });
    match first {
        Err(error) if error.kind() == &ErrorKind::BufferTooSmall => {}
        Err(error) => return Err(error),
        Ok(()) => {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "provider version size query unexpectedly succeeded",
            ))
        }
    }
    validate_required(raw.vendor_required, "vendor")?;
    validate_required(raw.description_required, "description")?;

    let expected_vendor = raw.vendor_required;
    let expected_description = raw.description_required;
    let mut vendor = zeroed_buffer(expected_vendor, "vendor")?;
    let mut description = zeroed_buffer(expected_description, "description")?;
    raw.vendor = vendor.as_mut_ptr().cast::<c_char>();
    raw.vendor_capacity = vendor.len();
    raw.description = description.as_mut_ptr().cast::<c_char>();
    raw.description_capacity = description.len();

    call_with_diagnostic(|buffer, capacity, required| {
        // Both output vectors stay allocated with the advertised capacities
        // for the duration of this call.
        query(&mut raw, buffer, capacity, required)
    })?;
    if raw.vendor_required != expected_vendor || raw.description_required != expected_description {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            "provider version sizes changed between query and copy",
        ));
    }

    Ok(ProviderVersion {
        api_version: raw.api_version,
        library_version: raw.library_version,
        vendor: decode_c_buffer(&vendor, "vendor")?,
        description: decode_c_buffer(&description, "description")?,
    })
}

/// Fallibly allocates exactly `size` zeroed bytes.
fn zeroed_buffer(size: usize, field: &str) -> Result<Vec<u8>, Error> {
    let mut buffer = reserve_owned(size, field)?;
    buffer.resize(size, 0);
    Ok(buffer)
}

fn string_view(value: &str) -> sys::AmsMelStringViewV1 {
    sys::AmsMelStringViewV1 {
        data: if value.is_empty() {
            ptr::null()
        } else {
            value.as_ptr().cast::<c_char>()
        },
        size: value.len(),
    }
}

fn raw_uci_id(value: &UciId) -> sys::AmsMelUciIdV1 {
    sys::AmsMelUciIdV1 {
        uuid: *value.uuid(),
        descriptive_label: string_view(value.descriptive_label()),
    }
}

fn raw_image_config(value: &ImageConfig) -> sys::AmsMelIrStreamConfigV1 {
    sys::AmsMelIrStreamConfigV1 {
        channel_type: sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE,
        channel_id: raw_uci_id(&value.channel_id),
        platform_id: raw_uci_id(&value.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: value.sensor_location.offset_x_m(),
            offset_y_m: value.sensor_location.offset_y_m(),
            offset_z_m: value.sensor_location.offset_z_m(),
            key: string_view(value.sensor_location.key()),
            system_name: string_view(value.sensor_location.system_name()),
        },
        buffer_count: value.buffer_count,
        buffer_size: value.buffer_size,
        queue_capacity: value.queue_capacity,
    }
}

fn open_image_control_with(
    config: &ImageControlConfig,
    open: impl FnOnce(
        &sys::AmsMelIrImageControlConfigV1,
        *mut *mut sys::AmsMelIrStream,
        *mut c_char,
        usize,
        *mut usize,
    ) -> i32,
    close: impl FnOnce(&mut *mut sys::AmsMelIrStream),
) -> Result<ImageStream, Error> {
    let raw_config = sys::AmsMelIrImageControlConfigV1 {
        channel_type: sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE,
        channel_id: raw_uci_id(&config.channel_id),
        platform_id: raw_uci_id(&config.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: config.sensor_location.offset_x_m(),
            offset_y_m: config.sensor_location.offset_y_m(),
            offset_z_m: config.sensor_location.offset_z_m(),
            key: string_view(config.sensor_location.key()),
            system_name: string_view(config.sensor_location.system_name()),
        },
    };
    let mut raw = ptr::null_mut();
    // The borrowed owned configuration stays alive throughout Open. Never
    // retry a side-effecting attachment, even on diagnostic decoding failure.
    let result = call_with_diagnostic(|d, c, r| open(&raw_config, &mut raw, d, c, r));
    if let Err(error) = result {
        if !raw.is_null() {
            close(&mut raw);
        }
        return Err(error);
    }
    if raw.is_null() {
        return Err(protocol(
            "Image control open succeeded without a stream owner",
        ));
    }
    Ok(ImageStream {
        raw,
        _not_send_sync: Rc::new(()),
    })
}

fn raw_control_config(value: &ControlConfig) -> sys::AmsMelIrC2ConfigV1 {
    sys::AmsMelIrC2ConfigV1 {
        channel_type: sys::AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
        channel_id: raw_uci_id(&value.channel_id),
        platform_id: raw_uci_id(&value.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: value.sensor_location.offset_x_m(),
            offset_y_m: value.sensor_location.offset_y_m(),
            offset_z_m: value.sensor_location.offset_z_m(),
            key: string_view(value.sensor_location.key()),
            system_name: string_view(value.sensor_location.system_name()),
        },
    }
}

fn empty_frame() -> sys::AmsMelIrFrameV1 {
    sys::AmsMelIrFrameV1 {
        system_time_ns: 0,
        integration_time_ns: 0,
        width: 0,
        height: 0,
        bits_per_pixel: 0,
        number_of_bands: 0,
        horizontal_fov_rad: 0.0,
        vertical_fov_rad: 0.0,
        pixel_format: 0,
        frame_id: 0,
        subframe_id: 0,
        subframe_total: 0,
        image_type: 0,
        image_flip: 0,
        image_flags: 0,
        dither_row: 0.0,
        dither_column: 0.0,
        row_offset: 0,
        column_offset: 0,
        band_index: 0,
        reserved: [0; 7],
        pixels: ptr::null_mut(),
        pixel_capacity: 0,
        pixel_required: 0,
    }
}

fn receive_raw(
    stream: *mut sys::AmsMelIrStream,
    timeout_ms: u32,
    frame: &mut sys::AmsMelIrFrameV1,
) -> Result<(), Error> {
    call_with_diagnostic(|diagnostic, capacity, required| {
        // SAFETY: the stream is live and `frame` points to exact writable C
        // storage. Any advertised pixel storage remains allocated for the call.
        unsafe {
            sys::ams_mel_ir_stream_receive(
                stream, timeout_ms, frame, diagnostic, capacity, required,
            )
        }
    })
}

fn wait_for_mode(
    request: *mut sys::AmsMelIrModeRequest,
    timeout_ms: u32,
) -> Result<ModeResult, Error> {
    wait_for_mode_with(
        timeout_ms,
        |timeout, result, diagnostic, capacity, required| {
            // SAFETY: the unique request is borrowed for this call; all output
            // storage is initialized and valid for the advertised capacity.
            unsafe {
                sys::ams_mel_ir_mode_request_wait(
                    request, timeout, result, diagnostic, capacity, required,
                )
            }
        },
    )
}

fn wait_for_mode_with(
    timeout_ms: u32,
    mut wait: impl FnMut(u32, &mut sys::AmsMelIrModeResultV1, *mut c_char, usize, &mut usize) -> i32,
) -> Result<ModeResult, Error> {
    let mut result = sys::AmsMelIrModeResultV1::default();
    let mut diagnostic = [0_u8; WAIT_DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    let status = wait(
        timeout_ms,
        &mut result,
        diagnostic.as_mut_ptr().cast::<c_char>(),
        diagnostic.len(),
        &mut required,
    );
    if required == 0 {
        return Err(protocol("native wait returned an invalid diagnostic size"));
    }
    if status == sys::AMS_MEL_TIMEOUT {
        return if required <= diagnostic.len() {
            let message = decode_diagnostic(&diagnostic[..required])?;
            Err(error_from_status(status, Some(message), Some(required)))
        } else {
            Err(error_from_status(status, None, Some(required)))
        };
    }

    let message = if required <= diagnostic.len() {
        decode_diagnostic(&diagnostic[..required])?
    } else {
        let first_result = result;
        let first_required = required;
        let mut complete = Vec::new();
        complete.try_reserve_exact(first_required).map_err(|_| {
            Error::new(
                ErrorKind::InternalError,
                "unable to allocate complete wait diagnostic storage",
            )
        })?;
        complete.resize(first_required, 0);
        let mut retry_result = sys::AmsMelIrModeResultV1::default();
        let mut retry_required = 0_usize;
        // Terminal request results are cached and repeatable.
        let retry_status = wait(
            0,
            &mut retry_result,
            complete.as_mut_ptr().cast::<c_char>(),
            complete.len(),
            &mut retry_required,
        );
        if retry_status != status
            || retry_result != first_result
            || retry_required != first_required
        {
            return Err(protocol(
                "native terminal wait changed during diagnostic retry",
            ));
        }
        result = retry_result;
        decode_diagnostic(&complete)?
    };

    match status {
        sys::AMS_MEL_OK => {
            if required != 1 || !message.is_empty() {
                return Err(protocol("successful native wait returned a diagnostic"));
            }
            if result.error_code != sys::AMS_MEL_ERROR_NONE {
                return Err(protocol("successful native wait returned an error code"));
            }
            let mode = mfa_mode(result.mode);
            if matches!(mode, MfaMode::Unknown(_)) {
                return Err(Error::new(
                    ErrorKind::ProviderFailed,
                    "native provider returned an unknown MFA mode",
                ));
            }
            Ok(ModeResult::Success { mode })
        }
        sys::AMS_MEL_COMMAND_REJECTED => Ok(ModeResult::Rejected {
            code: mel_error_code(result.error_code),
            description: message,
        }),
        _ => Err(error_from_status(status, Some(message), Some(required))),
    }
}

fn wait_for_return(
    request: *mut sys::AmsMelIrReturnRequest,
    timeout_ms: u32,
) -> Result<ReturnResult, Error> {
    wait_for_return_with(
        timeout_ms,
        |timeout, result, diagnostic, capacity, required| {
            // SAFETY: the request is exclusively borrowed; all output buffers are
            // writable for their advertised sizes. Terminal retries only poll.
            unsafe {
                sys::ams_mel_ir_return_request_wait(
                    request, timeout, result, diagnostic, capacity, required,
                )
            }
        },
    )
}

fn wait_for_return_with(
    timeout_ms: u32,
    mut wait: impl FnMut(u32, &mut sys::AmsMelIrReturnResultV1, *mut c_char, usize, &mut usize) -> i32,
) -> Result<ReturnResult, Error> {
    let mut result = sys::AmsMelIrReturnResultV1::default();
    let mut diagnostic = [0_u8; WAIT_DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    let status = wait(
        timeout_ms,
        &mut result,
        diagnostic.as_mut_ptr().cast::<c_char>(),
        diagnostic.len(),
        &mut required,
    );
    if required == 0 {
        return Err(protocol("native wait returned an invalid diagnostic size"));
    }
    if status == sys::AMS_MEL_TIMEOUT {
        return if required <= diagnostic.len() {
            let message = decode_diagnostic(&diagnostic[..required])?;
            Err(error_from_status(status, Some(message), Some(required)))
        } else {
            Err(error_from_status(status, None, Some(required)))
        };
    }

    let message = if required <= diagnostic.len() {
        decode_diagnostic(&diagnostic[..required])?
    } else {
        let first_result = result;
        let first_required = required;
        let mut complete = Vec::new();
        complete.try_reserve_exact(first_required).map_err(|_| {
            Error::new(
                ErrorKind::InternalError,
                "unable to allocate complete wait diagnostic storage",
            )
        })?;
        complete.resize(first_required, 0);
        let mut retry_result = sys::AmsMelIrReturnResultV1::default();
        let mut retry_required = 0_usize;
        let retry_status = wait(
            0,
            &mut retry_result,
            complete.as_mut_ptr().cast::<c_char>(),
            complete.len(),
            &mut retry_required,
        );
        if retry_status != status
            || retry_result != first_result
            || retry_required != first_required
        {
            return Err(protocol(
                "native terminal wait changed during diagnostic retry",
            ));
        }
        result = retry_result;
        decode_diagnostic(&complete)?
    };

    match status {
        sys::AMS_MEL_OK => {
            if required != 1 || !message.is_empty() {
                return Err(protocol("successful native wait returned a diagnostic"));
            }
            if result.error_code != sys::AMS_MEL_ERROR_NONE {
                return Err(protocol("successful native wait returned an error code"));
            }
            Ok(ReturnResult::Completed {
                value: command_return(result.value)?,
            })
        }
        sys::AMS_MEL_COMMAND_REJECTED => Ok(ReturnResult::Rejected {
            code: mel_error_code(result.error_code),
            description: message,
        }),
        _ => Err(error_from_status(status, Some(message), Some(required))),
    }
}

fn raw_comms_wait(
    request: *mut sys::AmsMelIrChannelCommsRequest,
    timeout_ms: u32,
    result: &mut sys::AmsMelIrChannelCommsTestResultV1,
    diagnostic: &mut [u8],
    required: &mut usize,
) -> i32 {
    // SAFETY: the request is live and exclusively accessed through `&mut self`;
    // result and diagnostic storage are writable for their advertised sizes.
    unsafe {
        sys::ams_mel_ir_channel_comms_request_wait(
            request,
            timeout_ms,
            result,
            diagnostic.as_mut_ptr().cast::<c_char>(),
            diagnostic.len(),
            required,
        )
    }
}

fn same_comms_result(
    first: &sys::AmsMelIrChannelCommsTestResultV1,
    second: &sys::AmsMelIrChannelCommsTestResultV1,
) -> bool {
    first.command_id == second.command_id
        && first.request_id == second.request_id
        && first.error_code == second.error_code
}

fn wait_for_comms(
    request: *mut sys::AmsMelIrChannelCommsRequest,
    timeout_ms: u32,
) -> Result<CommsTestResult, Error> {
    let mut result = sys::AmsMelIrChannelCommsTestResultV1::default();
    let mut diagnostic = [0_u8; WAIT_DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    let status = raw_comms_wait(
        request,
        timeout_ms,
        &mut result,
        &mut diagnostic,
        &mut required,
    );
    if required == 0 {
        return Err(protocol("native wait returned an invalid diagnostic size"));
    }
    if status == sys::AMS_MEL_TIMEOUT {
        return if required <= diagnostic.len() {
            let message = decode_diagnostic(&diagnostic[..required])?;
            Err(error_from_status(status, Some(message), Some(required)))
        } else {
            Err(error_from_status(status, None, Some(required)))
        };
    }

    let message = if required <= diagnostic.len() {
        decode_diagnostic(&diagnostic[..required])?
    } else {
        let first_result = result;
        let first_required = required;
        let mut complete = Vec::new();
        complete.try_reserve_exact(first_required).map_err(|_| {
            Error::new(
                ErrorKind::InternalError,
                "unable to allocate complete wait diagnostic storage",
            )
        })?;
        complete.resize(first_required, 0);
        let mut retry_result = sys::AmsMelIrChannelCommsTestResultV1::default();
        let mut retry_required = 0_usize;
        // Terminal results are cached and repeatable, so the same live request
        // is polled again with exact diagnostic storage.
        let retry_status = raw_comms_wait(
            request,
            0,
            &mut retry_result,
            &mut complete,
            &mut retry_required,
        );
        if retry_status != status
            || !same_comms_result(&retry_result, &first_result)
            || retry_required != first_required
        {
            return Err(protocol(
                "native terminal wait changed during diagnostic retry",
            ));
        }
        result = retry_result;
        decode_diagnostic(&complete)?
    };

    match status {
        sys::AMS_MEL_OK => {
            if required != 1 || !message.is_empty() {
                return Err(protocol("successful native wait returned a diagnostic"));
            }
            if result.error_code != sys::AMS_MEL_ERROR_NONE {
                return Err(protocol("successful native wait returned an error code"));
            }
            Ok(CommsTestResult::Completed {
                report: CommsTestReport {
                    command_id: result.command_id,
                    request_id: result.request_id,
                },
            })
        }
        sys::AMS_MEL_COMMAND_REJECTED => Ok(CommsTestResult::Rejected {
            code: mel_error_code(result.error_code),
            description: message,
        }),
        _ => Err(error_from_status(status, Some(message), Some(required))),
    }
}

/// Borrows a checked C span as a Rust slice. This is the only place a native
/// capability span or string view becomes a slice.
///
/// `size == 0` is empty whether `data` is null or not. `size > 0` requires a
/// non-null, aligned pointer and a length within the Rust slice bound
/// (`isize::MAX / size_of::<T>()`). Malformed spans are
/// `ProtocolInconsistency`.
///
/// # Safety
///
/// For a non-empty, well-formed span, `data` must point to `size` initialized
/// `T` values that remain valid and unmodified for `'a`.
unsafe fn checked_span<'a, T>(data: *const T, size: usize, field: &str) -> Result<&'a [T], Error> {
    if size == 0 {
        return Ok(&[]);
    }
    if data.is_null() {
        return Err(protocol(&format!(
            "native {field} span has a null pointer with nonzero size"
        )));
    }
    if !(data as usize).is_multiple_of(mem::align_of::<T>()) {
        return Err(protocol(&format!("native {field} span is misaligned")));
    }
    if size > isize::MAX as usize / mem::size_of::<T>().max(1) {
        return Err(protocol(&format!(
            "native {field} span length is too large"
        )));
    }
    // SAFETY: non-null, aligned, and within the Rust slice size bound; the
    // caller guarantees `size` initialized elements live for `'a`.
    Ok(unsafe { slice::from_raw_parts(data, size) })
}

fn reserve_owned<U>(size: usize, field: &str) -> Result<Vec<U>, Error> {
    let mut values = Vec::new();
    values.try_reserve_exact(size).map_err(|_| {
        Error::new(
            ErrorKind::InternalError,
            format!("unable to allocate owned {field} storage"),
        )
    })?;
    Ok(values)
}

fn copy_mapped<T, U>(
    values: &[T],
    field: &str,
    mut map: impl FnMut(&T) -> Result<U, Error>,
) -> Result<Vec<U>, Error> {
    let mut owned = reserve_owned(values.len(), field)?;
    for value in values {
        owned.push(map(value)?);
    }
    Ok(owned)
}

/// Copies a native string view into owned UTF-8 text. Embedded NUL is rejected
/// to keep the safe `UciId`/`ComponentLocation` text invariant.
fn owned_text(view: sys::AmsMelStringViewV1, field: &str) -> Result<String, Error> {
    // SAFETY: the view belongs to a native record kept alive by the caller.
    let bytes = unsafe { checked_span(view.data.cast::<u8>(), view.size, field)? };
    if bytes.contains(&0) {
        return Err(protocol(&format!(
            "native provider returned embedded NUL in {field}"
        )));
    }
    let mut owned = reserve_owned(bytes.len(), field)?;
    owned.extend_from_slice(bytes);
    String::from_utf8(owned).map_err(|_| {
        Error::new(
            ErrorKind::InvalidUtf8,
            format!("native provider returned invalid UTF-8 in {field}"),
        )
    })
}

fn owned_uci_id(raw: &sys::AmsMelUciIdV1, field: &str) -> Result<UciId, Error> {
    UciId::new(raw.uuid, owned_text(raw.descriptive_label, field)?)
}

fn owned_location(raw: &sys::AmsMelComponentLocationV1) -> Result<ComponentLocation, Error> {
    ComponentLocation::new(
        raw.offset_x_m,
        raw.offset_y_m,
        raw.offset_z_m,
        owned_text(raw.key, "sensor_location.key")?,
        owned_text(raw.system_name, "sensor_location.system_name")?,
    )
}

fn checked_bool(value: u32, field: &str) -> Result<bool, Error> {
    match value {
        0 => Ok(false),
        1 => Ok(true),
        other => Err(protocol(&format!(
            "native {field} has invalid boolean value {other}"
        ))),
    }
}

fn owned_u32_mapped<U>(
    span: sys::AmsMelU32SpanV1,
    field: &str,
    map: impl Fn(u32) -> U,
) -> Result<Vec<U>, Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller.
    let values = unsafe { checked_span(span.data, span.size, field)? };
    copy_mapped(values, field, |value| Ok(map(*value)))
}

fn owned_image_band(raw: &sys::AmsMelIrImageBandV1) -> Result<ImageBand, Error> {
    // SAFETY: the nested span belongs to a native record kept alive by the caller.
    let bands = unsafe { checked_span(raw.bands.data, raw.bands.size, "image_bands.bands")? };
    Ok(ImageBand {
        band_index: raw.band_index,
        bands: copy_mapped(bands, "image_bands.bands", |band| {
            Ok(BandInfo {
                kind: band_type(band.kind),
                min_wavelength_m: band.min_wavelength_m,
                max_wavelength_m: band.max_wavelength_m,
            })
        })?,
    })
}

/// Converts a complete native capability record into fully owned Rust data.
/// Every span and string is validated and copied; no native pointer escapes.
fn capability_from_raw(raw: &sys::AmsMelIrChannelCapabilityV1) -> Result<ChannelCapability, Error> {
    // SAFETY: the span belongs to a native record kept alive by the caller.
    let image_bands =
        unsafe { checked_span(raw.image_bands.data, raw.image_bands.size, "image_bands")? };
    Ok(ChannelCapability {
        channel_id: owned_uci_id(&raw.channel_id, "channel_id")?,
        height: raw.height,
        width: raw.width,
        bit_depth: raw.bit_depth,
        row_pitch: raw.row_pitch,
        buffer_size: raw.buffer_size,
        image_size: raw.image_size,
        number_of_bands: raw.number_of_bands,
        pixel_format: pixel_format(raw.pixel_format),
        sensor_types: owned_u32_mapped(raw.sensor_types, "sensor_types", sensor_type)?,
        platform_id: owned_uci_id(&raw.platform_id, "platform_id")?,
        sensor_location: owned_location(&raw.sensor_location)?,
        channel_types: owned_u32_mapped(raw.channel_types, "channel_types", channel_type)?,
        task_schedule_depth: raw.task_schedule_depth,
        odc_available: checked_bool(raw.odc_available, "odc_available")?,
        nuc_available: checked_bool(raw.nuc_available, "nuc_available")?,
        metadata_capabilities: owned_u32_mapped(
            raw.metadata_capabilities,
            "metadata_capabilities",
            metadata_capability,
        )?,
        image_bands: copy_mapped(image_bands, "image_bands", owned_image_band)?,
        nav_frames: owned_u32_mapped(raw.nav_frames, "nav_frames", coordinate_system)?,
    })
}

fn pixel_format(value: u32) -> PixelFormat {
    match value {
        sys::AMS_MEL_IR_PIXEL_MONO => PixelFormat::Mono,
        sys::AMS_MEL_IR_PIXEL_RGB => PixelFormat::Rgb,
        sys::AMS_MEL_IR_PIXEL_BAYER => PixelFormat::Bayer,
        unknown => PixelFormat::Unknown(unknown),
    }
}

fn sensor_type(value: u32) -> SensorType {
    match value {
        sys::AMS_MEL_IR_SENSOR_UNSPECIFIED => SensorType::Unspecified,
        sys::AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL => SensorType::GimbalHorizontal,
        sys::AMS_MEL_IR_SENSOR_GIMBAL_VERTICAL => SensorType::GimbalVertical,
        sys::AMS_MEL_IR_SENSOR_GIMBAL_ROTATION => SensorType::GimbalRotation,
        sys::AMS_MEL_IR_SENSOR_STEP_STARE => SensorType::StepStare,
        unknown => SensorType::Unknown(unknown),
    }
}

fn channel_type(value: u32) -> ChannelType {
    match value {
        sys::AMS_MEL_IR_CHANNEL_IRST_TRACK => ChannelType::IrstTrack,
        sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE => ChannelType::IrstImage,
        sys::AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL => ChannelType::CommandAndControl,
        sys::AMS_MEL_IR_CHANNEL_SCHEDULING => ChannelType::Scheduling,
        sys::AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS => ChannelType::HealthAndStatus,
        sys::AMS_MEL_IR_CHANNEL_INSTRUMENTATION => ChannelType::Instrumentation,
        sys::AMS_MEL_IR_CHANNEL_STACKED_IMAGE => ChannelType::StackedImage,
        sys::AMS_MEL_IR_CHANNEL_RESERVED_1 => ChannelType::Reserved1,
        sys::AMS_MEL_IR_CHANNEL_RESERVED_2 => ChannelType::Reserved2,
        unknown => ChannelType::Unknown(unknown),
    }
}

fn band_type(value: u32) -> BandType {
    match value {
        sys::AMS_MEL_IR_BAND_INVALID => BandType::Invalid,
        sys::AMS_MEL_IR_BAND_MULTIBAND => BandType::Multiband,
        sys::AMS_MEL_IR_BAND_IR_FAR => BandType::IrFar,
        sys::AMS_MEL_IR_BAND_IR_NEAR => BandType::IrNear,
        sys::AMS_MEL_IR_BAND_IR_LONGWAVE => BandType::IrLongwave,
        sys::AMS_MEL_IR_BAND_IR_MIDWAVE => BandType::IrMidwave,
        sys::AMS_MEL_IR_BAND_IR_SHORTWAVE => BandType::IrShortwave,
        sys::AMS_MEL_IR_BAND_VISIBLE_WHITE => BandType::VisibleWhite,
        sys::AMS_MEL_IR_BAND_VISIBLE_RED => BandType::VisibleRed,
        sys::AMS_MEL_IR_BAND_VISIBLE_GREEN => BandType::VisibleGreen,
        sys::AMS_MEL_IR_BAND_VISIBLE_BLUE => BandType::VisibleBlue,
        sys::AMS_MEL_IR_BAND_UVA => BandType::Uva,
        sys::AMS_MEL_IR_BAND_UVB => BandType::Uvb,
        sys::AMS_MEL_IR_BAND_UVC => BandType::Uvc,
        sys::AMS_MEL_IR_BAND_UV_VACUUM => BandType::UvVacuum,
        unknown => BandType::Unknown(unknown),
    }
}

fn coordinate_system(value: u32) -> CoordinateSystem {
    match value {
        sys::AMS_MEL_IR_COORDINATE_LLA => CoordinateSystem::Lla,
        sys::AMS_MEL_IR_COORDINATE_ECEF => CoordinateSystem::Ecef,
        sys::AMS_MEL_IR_COORDINATE_NED_PLATFORM => CoordinateSystem::NedPlatform,
        sys::AMS_MEL_IR_COORDINATE_NED_SENSOR => CoordinateSystem::NedSensor,
        unknown => CoordinateSystem::Unknown(unknown),
    }
}

fn metadata_capability(value: u32) -> MetadataCapability {
    use MetadataCapability as M;
    match value {
        sys::AMS_MEL_IR_METADATA_BAD_PIXEL_LIST => M::BadPixelList,
        sys::AMS_MEL_IR_METADATA_OPTICAL_DISTORTION_MAP => M::OpticalDistortionMap,
        sys::AMS_MEL_IR_METADATA_LF_STATUS => M::LfStatus,
        sys::AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT => M::LineOfSightReport,
        sys::AMS_MEL_IR_METADATA_LINE_OF_SIGHT_QUATERNION => M::LineOfSightQuaternion,
        sys::AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER => M::LineOfSightEuler,
        sys::AMS_MEL_IR_METADATA_MFA_STATUS => M::MfaStatus,
        sys::AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED => M::MfaStatusDetailed,
        sys::AMS_MEL_IR_METADATA_BIT_CONFIGURATION => M::BitConfiguration,
        sys::AMS_MEL_IR_METADATA_COMMAND_STATUS => M::CommandStatus,
        sys::AMS_MEL_IR_METADATA_BIT_STATUS => M::BitStatus,
        sys::AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_MESSAGE => M::CandidateObjectMessage,
        sys::AMS_MEL_IR_METADATA_TASK_EXECUTING_REP => M::TaskExecutingRep,
        sys::AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP => M::SubsystemStatusResp,
        sys::AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK => M::ExecuteTaskAck,
        sys::AMS_MEL_IR_METADATA_SCHED_CREATED_REP => M::SchedCreatedRep,
        sys::AMS_MEL_IR_METADATA_IRST_TRACK_REPORT => M::IrstTrackReport,
        sys::AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP => M::ChannelCommsTestRep,
        sys::AMS_MEL_IR_METADATA_CAMERA_COMMAND_RESP => M::CameraCommandResp,
        sys::AMS_MEL_IR_METADATA_CAMERA_PROTECT_CMD_RESP => M::CameraProtectCmdResp,
        sys::AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT => M::InstrumentationReport,
        sys::AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP => M::NavigationReportResp,
        sys::AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA => M::RequestSystemTrackData,
        sys::AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE => M::UpdateTrackListResponse,
        sys::AMS_MEL_IR_METADATA_LOS_3D_KINEMATICS_TYPE => M::Los3dKinematicsType,
        sys::AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE => {
            M::CandidateObjectPreprocMessage
        }
        sys::AMS_MEL_IR_METADATA_TASK_EVENTS => M::TaskEvents,
        sys::AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT => M::ScanPerformanceReport,
        sys::AMS_MEL_IR_METADATA_RESERVED_3 => M::Reserved3,
        sys::AMS_MEL_IR_METADATA_RESERVED_5 => M::Reserved5,
        sys::AMS_MEL_IR_METADATA_RESERVED_9 => M::Reserved9,
        sys::AMS_MEL_IR_METADATA_RESERVED_10 => M::Reserved10,
        unknown => M::Unknown(unknown),
    }
}

fn command_return(value: u32) -> Result<CommandReturn, Error> {
    match value {
        sys::AMS_MEL_IR_RETURN_SUCCESS => Ok(CommandReturn::Success),
        sys::AMS_MEL_IR_RETURN_BAD_POINTER => Ok(CommandReturn::BadPointer),
        sys::AMS_MEL_IR_RETURN_FAIL => Ok(CommandReturn::Fail),
        sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED => Ok(CommandReturn::NotSupported),
        sys::AMS_MEL_IR_RETURN_NOT_IMPLEMENTED => Ok(CommandReturn::NotImplemented),
        unknown => Err(Error::new(
            ErrorKind::ProviderFailed,
            format!("native provider returned unknown IR Return value {unknown}"),
        )),
    }
}

fn mfa_mode(value: u32) -> MfaMode {
    match value {
        sys::AMS_MEL_IR_MFA_MODE_UNUSED => MfaMode::Unused,
        sys::AMS_MEL_IR_MFA_MODE_TASK_SCHED => MfaMode::TaskSched,
        sys::AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED => MfaMode::ScanVolumeSched,
        sys::AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED => MfaMode::ScanBarSched,
        unknown => MfaMode::Unknown(unknown),
    }
}

fn mel_error_code(value: u32) -> MelErrorCode {
    match value {
        sys::AMS_MEL_ERROR_NONE => MelErrorCode::None,
        sys::AMS_MEL_ERROR_INVALID_ID => MelErrorCode::InvalidId,
        sys::AMS_MEL_ERROR_INVALID_STATE => MelErrorCode::InvalidState,
        sys::AMS_MEL_ERROR_INVALID_PARAMETERS => MelErrorCode::InvalidParameters,
        sys::AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS => MelErrorCode::InsufficientPermissions,
        sys::AMS_MEL_ERROR_INSUFFICIENT_RESOURCES => MelErrorCode::InsufficientResources,
        sys::AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES => MelErrorCode::InsufficientLocalResources,
        sys::AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES => {
            MelErrorCode::InsufficientRemoteResources
        }
        sys::AMS_MEL_ERROR_UNSUPPORTED => MelErrorCode::Unsupported,
        unknown => MelErrorCode::Unknown(unknown),
    }
}

fn frame_from_raw(raw: sys::AmsMelIrFrameV1, pixels: Vec<u8>) -> Result<Frame, Error> {
    if raw.pixel_format != sys::AMS_MEL_IR_PIXEL_MONO
        || raw.bits_per_pixel != 8
        || raw.number_of_bands != 1
    {
        return Err(protocol("native frame violates the Mono8 profile"));
    }
    let expected = (raw.width as usize)
        .checked_mul(raw.height as usize)
        .ok_or_else(|| protocol("native frame dimensions overflow"))?;
    if expected != pixels.len() {
        return Err(protocol("native frame dimensions do not match pixel count"));
    }
    Ok(Frame {
        system_time_ns: raw.system_time_ns,
        integration_time_ns: raw.integration_time_ns,
        width: raw.width,
        height: raw.height,
        bits_per_pixel: raw.bits_per_pixel,
        number_of_bands: raw.number_of_bands,
        horizontal_fov_rad: raw.horizontal_fov_rad,
        vertical_fov_rad: raw.vertical_fov_rad,
        frame_id: raw.frame_id,
        subframe_id: raw.subframe_id,
        subframe_total: raw.subframe_total,
        image_type: match raw.image_type {
            sys::AMS_MEL_IR_IMAGE_STARING => ImageType::Staring,
            sys::AMS_MEL_IR_IMAGE_SCANNING => ImageType::Scanning,
            unknown => ImageType::Unknown(unknown),
        },
        image_flip: match raw.image_flip {
            sys::AMS_MEL_IR_FLIP_NONE => ImageFlip::None,
            sys::AMS_MEL_IR_FLIP_VERTICAL => ImageFlip::Vertical,
            sys::AMS_MEL_IR_FLIP_HORIZONTAL => ImageFlip::Horizontal,
            sys::AMS_MEL_IR_FLIP_BOTH => ImageFlip::Both,
            unknown => ImageFlip::Unknown(unknown),
        },
        image_flags: raw.image_flags,
        dither_row: raw.dither_row,
        dither_column: raw.dither_column,
        row_offset: raw.row_offset,
        column_offset: raw.column_offset,
        band_index: raw.band_index,
        pixels,
    })
}

fn protocol(message: &str) -> Error {
    Error::new(ErrorKind::ProtocolInconsistency, message)
}

fn c_path(path: &Path) -> Result<CString, Error> {
    let path = path.to_str().ok_or_else(|| {
        Error::new(
            ErrorKind::InvalidArgument,
            "provider_library must be valid UTF-8",
        )
    })?;
    c_string(path, "provider_library")
}

fn c_string(value: &str, name: &str) -> Result<CString, Error> {
    CString::new(value).map_err(|_| Error::nul(name))
}

fn validate_required(required: usize, field: &str) -> Result<(), Error> {
    if required == 0 {
        Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned zero {field} size"),
        ))
    } else {
        Ok(())
    }
}

fn decode_c_buffer(buffer: &[u8], field: &str) -> Result<String, Error> {
    let Some((&0, bytes)) = buffer.split_last() else {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned unterminated {field}"),
        ));
    };
    if bytes.contains(&0) {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native provider returned embedded NUL in {field}"),
        ));
    }
    String::from_utf8(bytes.to_vec()).map_err(|_| {
        Error::new(
            ErrorKind::InvalidUtf8,
            format!("native provider returned invalid UTF-8 in {field}"),
        )
    })
}

fn call_with_diagnostic(
    call: impl FnOnce(*mut c_char, usize, *mut usize) -> i32,
) -> Result<(), Error> {
    let mut diagnostic = [0_u8; DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    let status = call(
        diagnostic.as_mut_ptr().cast::<c_char>(),
        diagnostic.len(),
        &mut required,
    );
    if required == 0 {
        return Err(Error::new(
            ErrorKind::ProtocolInconsistency,
            format!("native diagnostic requires invalid capacity {required}"),
        ));
    }
    if required > diagnostic.len() {
        if status == sys::AMS_MEL_OK {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "successful native call returned an oversized diagnostic",
            ));
        }
        return Err(error_from_status(status, None, Some(required)));
    }
    let message = decode_diagnostic(&diagnostic[..required])?;
    if status == sys::AMS_MEL_OK {
        if required != 1 || !message.is_empty() {
            return Err(Error::new(
                ErrorKind::ProtocolInconsistency,
                "successful native call returned a diagnostic",
            ));
        }
        Ok(())
    } else {
        Err(error_from_status(status, Some(message), Some(required)))
    }
}

fn decode_diagnostic(buffer: &[u8]) -> Result<String, Error> {
    decode_c_buffer(buffer, "diagnostic")
}

fn error_from_status(
    status: i32,
    diagnostic: Option<String>,
    diagnostic_required: Option<usize>,
) -> Error {
    let kind = match status {
        sys::AMS_MEL_INVALID_ARGUMENT => ErrorKind::InvalidArgument,
        sys::AMS_MEL_LIBRARY_LOAD_FAILED => ErrorKind::LibraryLoadFailed,
        sys::AMS_MEL_SYMBOL_NOT_FOUND => ErrorKind::SymbolNotFound,
        sys::AMS_MEL_FACTORY_FAILED => ErrorKind::FactoryFailed,
        sys::AMS_MEL_INITIALIZATION_FAILED => ErrorKind::InitializationFailed,
        sys::AMS_MEL_PROVIDER_EXCEPTION => ErrorKind::ProviderException,
        sys::AMS_MEL_BUFFER_TOO_SMALL => ErrorKind::BufferTooSmall,
        sys::AMS_MEL_PROVIDER_FAILED => ErrorKind::ProviderFailed,
        sys::AMS_MEL_INTERNAL_ERROR => ErrorKind::InternalError,
        sys::AMS_MEL_TIMEOUT => ErrorKind::Timeout,
        sys::AMS_MEL_STREAM_STOPPED => ErrorKind::StreamStopped,
        sys::AMS_MEL_RESOURCE_EXHAUSTED => ErrorKind::ResourceExhausted,
        unknown => ErrorKind::Unknown(unknown),
    };
    Error::from_native_parts(kind, diagnostic, diagnostic_required)
}

fn best_effort_close(raw: &mut *mut sys::AmsMelSession) {
    // SAFETY: called only for this wrapper's unique owner. Null diagnostics are
    // explicitly supported. The result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_session_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_stream(raw: &mut *mut sys::AmsMelIrStream) {
    // SAFETY: called only for a unique stream owner. Null diagnostics are
    // supported. The result is ignored because cleanup cannot be reported here.
    let _ = unsafe { sys::ams_mel_ir_stream_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_c2(raw: &mut *mut sys::AmsMelIrC2) {
    // SAFETY: called only for this wrapper's unique C2 owner. Native close may
    // retain it when safe detach cannot be established; Drop does not fabricate
    // cleanup or retry further.
    let _ = unsafe { sys::ams_mel_ir_c2_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_request(raw: &mut *mut sys::AmsMelIrModeRequest) {
    // SAFETY: called only for this wrapper's unique request owner. Public request
    // close is nonblocking and does not cancel pending provider work.
    let _ = unsafe { sys::ams_mel_ir_mode_request_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_return_request(raw: &mut *mut sys::AmsMelIrReturnRequest) {
    // SAFETY: called only for this wrapper's unique request owner. Public request
    // close is nonblocking and does not cancel pending provider work.
    let _ =
        unsafe { sys::ams_mel_ir_return_request_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_channel_view(raw: &mut *mut sys::AmsMelIrChannel) {
    // SAFETY: called only for this wrapper's unique weak view owner. View Close
    // is idempotent and nonblocking; it never touches the typed owner or
    // cancels requests. The result is ignored because Drop cannot report it.
    let _ = unsafe { sys::ams_mel_ir_channel_close(raw, ptr::null_mut(), 0, ptr::null_mut()) };
}

fn best_effort_close_comms_request(raw: &mut *mut sys::AmsMelIrChannelCommsRequest) {
    // SAFETY: called only for this wrapper's unique request owner. Public request
    // close is nonblocking and does not cancel pending provider work.
    let _ = unsafe {
        sys::ams_mel_ir_channel_comms_request_close(raw, ptr::null_mut(), 0, ptr::null_mut())
    };
}

#[cfg(test)]
#[path = "ir_image_control_tests.rs"]
mod ir_image_control_tests;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_known_command_returns_and_rejects_unknown_values() {
        for (raw, expected) in [
            (sys::AMS_MEL_IR_RETURN_SUCCESS, CommandReturn::Success),
            (
                sys::AMS_MEL_IR_RETURN_BAD_POINTER,
                CommandReturn::BadPointer,
            ),
            (sys::AMS_MEL_IR_RETURN_FAIL, CommandReturn::Fail),
            (
                sys::AMS_MEL_IR_RETURN_NOT_SUPPORTED,
                CommandReturn::NotSupported,
            ),
            (
                sys::AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
                CommandReturn::NotImplemented,
            ),
        ] {
            assert_eq!(command_return(raw).expect("known Return"), expected);
        }
        let error = command_return(u32::MAX).expect_err("unknown Return");
        assert_eq!(error.kind(), &ErrorKind::ProviderFailed);
        assert!(error
            .diagnostic()
            .is_some_and(|message| message.contains("4294967295")));
    }

    fn view(bytes: &[u8]) -> sys::AmsMelStringViewV1 {
        sys::AmsMelStringViewV1 {
            data: bytes.as_ptr().cast::<c_char>(),
            size: bytes.len(),
        }
    }

    fn u32_span(values: &[u32]) -> sys::AmsMelU32SpanV1 {
        sys::AmsMelU32SpanV1 {
            data: values.as_ptr(),
            size: values.len(),
        }
    }

    fn null_u32_span(size: usize) -> sys::AmsMelU32SpanV1 {
        sys::AmsMelU32SpanV1 {
            data: ptr::null(),
            size,
        }
    }

    const LABEL: &[u8] = b"label";
    const KEY: &[u8] = b"key";
    const SYSTEM: &[u8] = b"system";
    const SENSORS: &[u32] = &[sys::AMS_MEL_IR_SENSOR_STEP_STARE, 77];
    const CHANNELS: &[u32] = &[sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE, 1234];
    const METADATA: &[u32] = &[sys::AMS_MEL_IR_METADATA_RESERVED_10, 32];
    const NAV: &[u32] = &[sys::AMS_MEL_IR_COORDINATE_LLA, 4];
    const BANDS: &[sys::AmsMelIrBandInfoV1] = &[sys::AmsMelIrBandInfoV1 {
        kind: 15,
        min_wavelength_m: 1.0,
        max_wavelength_m: 2.0,
    }];

    fn raw_capability(
        image_bands: &[sys::AmsMelIrImageBandV1],
    ) -> sys::AmsMelIrChannelCapabilityV1 {
        let id = sys::AmsMelUciIdV1 {
            uuid: [7; 16],
            descriptive_label: view(LABEL),
        };
        sys::AmsMelIrChannelCapabilityV1 {
            channel_id: id,
            height: u32::MAX,
            width: 2,
            bit_depth: 3,
            row_pitch: 4,
            buffer_size: 5,
            image_size: 6,
            number_of_bands: 7,
            pixel_format: 99,
            sensor_types: u32_span(SENSORS),
            platform_id: id,
            sensor_location: sys::AmsMelComponentLocationV1 {
                offset_x_m: 1.5,
                offset_y_m: -2.5,
                offset_z_m: 3.5,
                key: view(KEY),
                system_name: view(SYSTEM),
            },
            channel_types: u32_span(CHANNELS),
            task_schedule_depth: 8,
            odc_available: 1,
            nuc_available: 0,
            metadata_capabilities: u32_span(METADATA),
            image_bands: sys::AmsMelIrImageBandSpanV1 {
                data: image_bands.as_ptr(),
                size: image_bands.len(),
            },
            nav_frames: u32_span(NAV),
        }
    }

    fn one_band() -> [sys::AmsMelIrImageBandV1; 1] {
        [sys::AmsMelIrImageBandV1 {
            band_index: 0xffff_fff0,
            bands: sys::AmsMelIrBandInfoSpanV1 {
                data: BANDS.as_ptr(),
                size: BANDS.len(),
            },
        }]
    }

    #[test]
    fn converts_complete_capability_and_preserves_unknown_enums() {
        let bands = one_band();
        let value = capability_from_raw(&raw_capability(&bands)).expect("valid capability");
        assert_eq!(value.channel_id.uuid(), &[7; 16]);
        assert_eq!(value.channel_id.descriptive_label(), "label");
        assert_eq!(value.platform_id.descriptive_label(), "label");
        assert_eq!(value.height, u32::MAX);
        assert_eq!((value.width, value.bit_depth, value.row_pitch), (2, 3, 4));
        assert_eq!(
            (value.buffer_size, value.image_size, value.number_of_bands),
            (5, 6, 7)
        );
        assert_eq!(value.pixel_format, PixelFormat::Unknown(99));
        assert_eq!(
            value.sensor_types,
            vec![SensorType::StepStare, SensorType::Unknown(77)]
        );
        assert_eq!(
            value.channel_types,
            vec![ChannelType::IrstImage, ChannelType::Unknown(1234)]
        );
        assert_eq!(
            value.metadata_capabilities,
            vec![
                MetadataCapability::Reserved10,
                MetadataCapability::Unknown(32)
            ]
        );
        assert_eq!(
            value.nav_frames,
            vec![CoordinateSystem::Lla, CoordinateSystem::Unknown(4)]
        );
        assert_eq!(value.sensor_location.offset_x_m(), 1.5);
        assert_eq!(value.sensor_location.offset_y_m(), -2.5);
        assert_eq!(value.sensor_location.offset_z_m(), 3.5);
        assert_eq!(value.sensor_location.key(), "key");
        assert_eq!(value.sensor_location.system_name(), "system");
        assert_eq!(value.task_schedule_depth, 8);
        assert!(value.odc_available);
        assert!(!value.nuc_available);
        assert_eq!(
            value.image_bands,
            vec![ImageBand {
                band_index: 0xffff_fff0,
                bands: vec![BandInfo {
                    kind: BandType::Unknown(15),
                    min_wavelength_m: 1.0,
                    max_wavelength_m: 2.0,
                }],
            }]
        );
    }

    #[test]
    fn maps_every_published_enum_constant() {
        for raw in 0..=2 {
            assert!(!matches!(pixel_format(raw), PixelFormat::Unknown(_)));
        }
        assert_eq!(pixel_format(3), PixelFormat::Unknown(3));
        for raw in 0..=4 {
            assert!(!matches!(sensor_type(raw), SensorType::Unknown(_)));
        }
        assert_eq!(sensor_type(5), SensorType::Unknown(5));
        for raw in 0..=8 {
            assert!(!matches!(channel_type(raw), ChannelType::Unknown(_)));
        }
        assert_eq!(channel_type(9), ChannelType::Unknown(9));
        for raw in 0..=31 {
            assert!(!matches!(
                metadata_capability(raw),
                MetadataCapability::Unknown(_)
            ));
        }
        assert_eq!(metadata_capability(32), MetadataCapability::Unknown(32));
        for raw in 0..=14 {
            assert!(!matches!(band_type(raw), BandType::Unknown(_)));
        }
        assert_eq!(band_type(u32::MAX), BandType::Unknown(u32::MAX));
        for raw in 0..=3 {
            assert!(!matches!(
                coordinate_system(raw),
                CoordinateSystem::Unknown(_)
            ));
        }
        assert_eq!(coordinate_system(4), CoordinateSystem::Unknown(4));
    }

    #[test]
    fn empty_spans_with_null_or_non_null_data_are_empty() {
        let mut raw = raw_capability(&[]);
        raw.sensor_types = null_u32_span(0);
        raw.channel_types = u32_span(&[]);
        raw.metadata_capabilities = null_u32_span(0);
        raw.nav_frames = null_u32_span(0);
        raw.image_bands = sys::AmsMelIrImageBandSpanV1 {
            data: ptr::null(),
            size: 0,
        };
        raw.channel_id.descriptive_label = sys::AmsMelStringViewV1 {
            data: ptr::null(),
            size: 0,
        };
        let value = capability_from_raw(&raw).expect("empty spans");
        assert!(value.sensor_types.is_empty() && value.channel_types.is_empty());
        assert!(value.metadata_capabilities.is_empty() && value.nav_frames.is_empty());
        assert!(value.image_bands.is_empty());
        assert_eq!(value.channel_id.descriptive_label(), "");
    }

    #[test]
    fn nonzero_span_with_null_pointer_is_protocol_inconsistency() {
        let bands = one_band();
        let mut cases = Vec::new();
        let mut raw = raw_capability(&bands);
        raw.sensor_types = null_u32_span(1);
        cases.push(raw);
        let mut raw = raw_capability(&bands);
        raw.nav_frames = null_u32_span(usize::MAX);
        cases.push(raw);
        let mut raw = raw_capability(&bands);
        raw.image_bands = sys::AmsMelIrImageBandSpanV1 {
            data: ptr::null(),
            size: 2,
        };
        cases.push(raw);
        let mut raw = raw_capability(&bands);
        raw.sensor_location.key = sys::AmsMelStringViewV1 {
            data: ptr::null(),
            size: 3,
        };
        cases.push(raw);
        let null_nested = [sys::AmsMelIrImageBandV1 {
            band_index: 1,
            bands: sys::AmsMelIrBandInfoSpanV1 {
                data: ptr::null(),
                size: 1,
            },
        }];
        cases.push(raw_capability(&null_nested));
        for raw in cases {
            assert_eq!(
                capability_from_raw(&raw)
                    .expect_err("malformed span")
                    .kind(),
                &ErrorKind::ProtocolInconsistency
            );
        }
    }

    #[test]
    fn oversized_span_length_is_rejected_before_slicing() {
        let values = [1_u32];
        // SAFETY: the helper rejects this length before forming a slice; no
        // memory beyond `values` is read.
        let error = unsafe { checked_span(values.as_ptr(), usize::MAX / 2, "test") }
            .expect_err("length beyond isize::MAX bytes");
        assert_eq!(error.kind(), &ErrorKind::ProtocolInconsistency);
    }

    #[test]
    fn invalid_utf8_and_embedded_nul_are_rejected() {
        const BAD_UTF8: &[u8] = b"bad\xC3\x28";
        const WITH_NUL: &[u8] = b"a\0b";
        let mut raw = raw_capability(&[]);
        raw.platform_id.descriptive_label = view(BAD_UTF8);
        assert_eq!(
            capability_from_raw(&raw).expect_err("invalid UTF-8").kind(),
            &ErrorKind::InvalidUtf8
        );
        let mut raw = raw_capability(&[]);
        raw.sensor_location.system_name = view(WITH_NUL);
        assert_eq!(
            capability_from_raw(&raw).expect_err("embedded NUL").kind(),
            &ErrorKind::ProtocolInconsistency
        );
    }

    #[test]
    fn booleans_accept_exactly_zero_or_one() {
        for (odc, nuc) in [(2, 0), (0, 2), (u32::MAX, 1)] {
            let mut raw = raw_capability(&[]);
            raw.odc_available = odc;
            raw.nuc_available = nuc;
            assert_eq!(
                capability_from_raw(&raw)
                    .expect_err("invalid boolean")
                    .kind(),
                &ErrorKind::ProtocolInconsistency
            );
        }
        let mut raw = raw_capability(&[]);
        raw.odc_available = 0;
        raw.nuc_available = 1;
        let value = capability_from_raw(&raw).expect("valid booleans");
        assert!(!value.odc_available && value.nuc_available);
    }
}
