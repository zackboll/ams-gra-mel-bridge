//! Safe Rust provider sessions, IR Mono8 frames, IR C2, and the common Channel
//! façade for C2 and Image through `ams_mel_c`.
//!
//! [`Session`], [`ImageStream`], [`ControlChannel`], [`ModeRequest`],
//! [`ReturnRequest`], [`ChannelView`], and [`CommsRequest`] are intentionally
//! neither `Send` nor `Sync`. A zero receive or request timeout polls. Request
//! timeout and request close or drop do not cancel provider work. BIT support is
//! limited to the empty/no-op profile; payload-bearing BIT is not exposed.
//!
//! Common Channel coverage (Task 032B3):
//!
//! ```text
//! safe Rust common Channel:      C2 + Image
//! native C / safe Ada common:    C2/Image/Health/Instrumentation/Track
//! ```
//!
//! Safe Rust has no typed Health, Instrumentation, or Track owners, so no
//! common Channel view of those families is offered here.

use std::error;
use std::ffi::{c_char, CString};
use std::fmt;
use std::mem;
use std::path::Path;
use std::ptr;
use std::rc::Rc;
use std::slice;

use ams_mel_sys as sys;

const DIAGNOSTIC_CAPACITY: usize = 4096;
const WAIT_DIAGNOSTIC_CAPACITY: usize = 512;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct AbiVersion {
    pub major: u32,
    pub minor: u32,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ProviderVersion {
    pub api_version: u32,
    pub library_version: u32,
    pub vendor: String,
    pub description: String,
}

#[derive(Clone, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum ErrorKind {
    InvalidArgument,
    LibraryLoadFailed,
    SymbolNotFound,
    FactoryFailed,
    InitializationFailed,
    ProviderException,
    BufferTooSmall,
    ProviderFailed,
    InternalError,
    Timeout,
    StreamStopped,
    ResourceExhausted,
    InvalidUtf8,
    ProtocolInconsistency,
    Unknown(i32),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct UciId {
    uuid: [u8; 16],
    descriptive_label: String,
}

impl UciId {
    pub fn new(uuid: [u8; 16], descriptive_label: impl Into<String>) -> Result<Self, Error> {
        let descriptive_label = descriptive_label.into();
        reject_nul(&descriptive_label, "descriptive_label")?;
        Ok(Self {
            uuid,
            descriptive_label,
        })
    }

    pub fn uuid(&self) -> &[u8; 16] {
        &self.uuid
    }

    pub fn descriptive_label(&self) -> &str {
        &self.descriptive_label
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ComponentLocation {
    offset_x_m: f64,
    offset_y_m: f64,
    offset_z_m: f64,
    key: String,
    system_name: String,
}

impl ComponentLocation {
    pub fn new(
        offset_x_m: f64,
        offset_y_m: f64,
        offset_z_m: f64,
        key: impl Into<String>,
        system_name: impl Into<String>,
    ) -> Result<Self, Error> {
        let key = key.into();
        let system_name = system_name.into();
        reject_nul(&key, "key")?;
        reject_nul(&system_name, "system_name")?;
        Ok(Self {
            offset_x_m,
            offset_y_m,
            offset_z_m,
            key,
            system_name,
        })
    }

    pub fn offset_x_m(&self) -> f64 {
        self.offset_x_m
    }

    pub fn offset_y_m(&self) -> f64 {
        self.offset_y_m
    }

    pub fn offset_z_m(&self) -> f64 {
        self.offset_z_m
    }

    pub fn key(&self) -> &str {
        &self.key
    }

    pub fn system_name(&self) -> &str {
        &self.system_name
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ImageConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
    buffer_count: usize,
    buffer_size: usize,
    queue_capacity: usize,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ControlConfig {
    channel_id: UciId,
    platform_id: UciId,
    sensor_location: ComponentLocation,
}

impl ControlConfig {
    pub fn new(channel_id: UciId, platform_id: UciId, sensor_location: ComponentLocation) -> Self {
        Self {
            channel_id,
            platform_id,
            sensor_location,
        }
    }
}

impl ImageConfig {
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

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageType {
    Staring,
    Scanning,
    Unknown(u32),
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ImageFlip {
    None,
    Vertical,
    Horizontal,
    Both,
    Unknown(u32),
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MfaMode {
    Unused,
    TaskSched,
    ScanVolumeSched,
    ScanBarSched,
    Unknown(u32),
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CommandReturn {
    Success,
    BadPointer,
    Fail,
    NotSupported,
    NotImplemented,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MelErrorCode {
    None,
    InvalidId,
    InvalidState,
    InvalidParameters,
    InsufficientPermissions,
    InsufficientResources,
    InsufficientLocalResources,
    InsufficientRemoteResources,
    Unsupported,
    Unknown(u32),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ModeResult {
    Success {
        mode: MfaMode,
    },
    Rejected {
        code: MelErrorCode,
        description: String,
    },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ReturnResult {
    Completed {
        value: CommandReturn,
    },
    Rejected {
        code: MelErrorCode,
        description: String,
    },
}

/// Outbound inherited `ChannelCommsTestReq`. Named fields prevent accidental
/// Command/Channel/Request ID ordering mistakes. All 32 bits are preserved.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommsTestRequest {
    pub command_id: u32,
    pub channel_id: u32,
    pub request_id: u32,
}

/// Inherited `ChannelCommsTestRep`. The upstream reply carries no Channel ID,
/// so none is reported.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommsTestReport {
    pub command_id: u32,
    pub request_id: u32,
}

/// Terminal CommsTest outcome. MEL command rejection is a normal terminal
/// result, not a Rust `Err`, matching [`ModeResult`] and [`ReturnResult`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum CommsTestResult {
    Completed {
        report: CommsTestReport,
    },
    Rejected {
        code: MelErrorCode,
        description: String,
    },
}

/// `ams_mel_ir_pixel_format_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum PixelFormat {
    Mono,
    Rgb,
    Bayer,
    Unknown(u32),
}

/// `ams_mel_ir_sensor_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum SensorType {
    Unspecified,
    GimbalHorizontal,
    GimbalVertical,
    GimbalRotation,
    StepStare,
    Unknown(u32),
}

/// `ams_mel_ir_channel_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum ChannelType {
    IrstTrack,
    IrstImage,
    CommandAndControl,
    Scheduling,
    HealthAndStatus,
    Instrumentation,
    StackedImage,
    Reserved1,
    Reserved2,
    Unknown(u32),
}

/// `ams_mel_ir_channel_metadata_capability_t`. Future raw values are preserved
/// as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum MetadataCapability {
    BadPixelList,
    OpticalDistortionMap,
    LfStatus,
    LineOfSightReport,
    LineOfSightQuaternion,
    LineOfSightEuler,
    MfaStatus,
    MfaStatusDetailed,
    BitConfiguration,
    CommandStatus,
    BitStatus,
    CandidateObjectMessage,
    TaskExecutingRep,
    SubsystemStatusResp,
    ExecuteTaskAck,
    SchedCreatedRep,
    IrstTrackReport,
    ChannelCommsTestRep,
    CameraCommandResp,
    CameraProtectCmdResp,
    InstrumentationReport,
    NavigationReportResp,
    RequestSystemTrackData,
    UpdateTrackListResponse,
    Los3dKinematicsType,
    CandidateObjectPreprocMessage,
    TaskEvents,
    ScanPerformanceReport,
    Reserved3,
    Reserved5,
    Reserved9,
    Reserved10,
    Unknown(u32),
}

/// `ams_mel_ir_band_type_t`. Future raw values are preserved as `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum BandType {
    Invalid,
    Multiband,
    IrFar,
    IrNear,
    IrLongwave,
    IrMidwave,
    IrShortwave,
    VisibleWhite,
    VisibleRed,
    VisibleGreen,
    VisibleBlue,
    Uva,
    Uvb,
    Uvc,
    UvVacuum,
    Unknown(u32),
}

/// `ams_mel_ir_coordinate_system_type_t`. Future raw values are preserved as
/// `Unknown`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[non_exhaustive]
pub enum CoordinateSystem {
    Lla,
    Ecef,
    NedPlatform,
    NedSensor,
    Unknown(u32),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BandInfo {
    pub kind: BandType,
    pub min_wavelength_m: f64,
    pub max_wavelength_m: f64,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ImageBand {
    pub band_index: u32,
    pub bands: Vec<BandInfo>,
}

/// Complete, fully Rust-owned copy of `ams_mel_ir_channel_capability_v1`. No
/// native pointer survives [`ChannelView::capabilities`]; the value remains
/// valid after the View, typed owner, Session, and provider library are gone.
#[derive(Clone, Debug, PartialEq)]
pub struct ChannelCapability {
    pub channel_id: UciId,
    pub height: u32,
    pub width: u32,
    pub bit_depth: u32,
    pub row_pitch: u32,
    pub buffer_size: u32,
    pub image_size: u32,
    pub number_of_bands: u32,
    pub pixel_format: PixelFormat,
    pub sensor_types: Vec<SensorType>,
    pub platform_id: UciId,
    pub sensor_location: ComponentLocation,
    pub channel_types: Vec<ChannelType>,
    pub task_schedule_depth: u32,
    pub odc_available: bool,
    pub nuc_available: bool,
    pub metadata_capabilities: Vec<MetadataCapability>,
    pub image_bands: Vec<ImageBand>,
    pub nav_frames: Vec<CoordinateSystem>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Frame {
    pub system_time_ns: i64,
    pub integration_time_ns: i64,
    pub width: u32,
    pub height: u32,
    pub bits_per_pixel: u32,
    pub number_of_bands: u32,
    pub horizontal_fov_rad: f64,
    pub vertical_fov_rad: f64,
    pub frame_id: u32,
    pub subframe_id: u32,
    pub subframe_total: u32,
    pub image_type: ImageType,
    pub image_flip: ImageFlip,
    pub image_flags: u32,
    pub dither_row: f64,
    pub dither_column: f64,
    pub row_offset: u32,
    pub column_offset: u32,
    pub band_index: u8,
    pub pixels: Vec<u8>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct StreamCounters {
    pub frames_received: u64,
    pub frames_dropped_queue_full: u64,
    pub malformed_or_unsupported: u64,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Error {
    kind: ErrorKind,
    diagnostic: Option<String>,
    diagnostic_required: Option<usize>,
}

impl Error {
    pub fn kind(&self) -> &ErrorKind {
        &self.kind
    }

    pub fn diagnostic(&self) -> Option<&str> {
        self.diagnostic.as_deref()
    }

    /// Returns the native diagnostic capacity required, including its trailing
    /// NUL. A value larger than the binding's capture capacity means that
    /// [`diagnostic`](Self::diagnostic) is `None`, because a partial diagnostic
    /// is never exposed as complete.
    pub fn diagnostic_required(&self) -> Option<usize> {
        self.diagnostic_required
    }

    fn new(kind: ErrorKind, diagnostic: impl Into<String>) -> Self {
        let diagnostic = diagnostic.into();
        Self {
            kind,
            diagnostic: (!diagnostic.is_empty()).then_some(diagnostic),
            diagnostic_required: None,
        }
    }

    fn nul(name: &str) -> Self {
        Self::new(
            ErrorKind::InvalidArgument,
            format!("{name} contains an embedded NUL"),
        )
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.diagnostic {
            Some(message) => write!(formatter, "{:?}: {message}", self.kind),
            None => write!(formatter, "{:?}", self.kind),
        }
    }
}

impl error::Error for Error {}

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

#[derive(Debug)]
pub struct Session {
    raw: *mut sys::AmsMelSession,
    _not_send_sync: Rc<()>,
}

/// Session-wide asynchronous admission limit. Zero (the default) is unlimited.
/// Refusal is explicit backpressure; the bridge neither queues nor retries.
#[derive(Clone, Copy, Debug, Eq, PartialEq, Default)]
pub struct SessionOptions {
    pub max_async_requests: u32,
}

impl Session {
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

    pub fn provider_version(&self) -> Result<ProviderVersion, Error> {
        let mut raw = empty_version();
        let first = call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: the session owner is live and `raw` is writable. Null/zero
            // string buffers are the documented required-size query.
            unsafe {
                sys::ams_mel_session_get_provider_version(
                    self.raw, &mut raw, buffer, capacity, required,
                )
            }
        });
        match first {
            Err(error) if error.kind == ErrorKind::BufferTooSmall => {}
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

        let mut vendor = vec![0_u8; raw.vendor_required];
        let mut description = vec![0_u8; raw.description_required];
        raw.vendor = vendor.as_mut_ptr().cast::<c_char>();
        raw.vendor_capacity = vendor.len();
        raw.description = description.as_mut_ptr().cast::<c_char>();
        raw.description_capacity = description.len();
        let expected_vendor = raw.vendor_required;
        let expected_description = raw.description_required;

        call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: both output vectors remain allocated with the advertised
            // capacities, and the live session and record pointers are valid.
            unsafe {
                sys::ams_mel_session_get_provider_version(
                    self.raw, &mut raw, buffer, capacity, required,
                )
            }
        })?;
        if raw.vendor_required != expected_vendor
            || raw.description_required != expected_description
        {
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

#[derive(Debug)]
pub struct ImageStream {
    raw: *mut sys::AmsMelIrStream,
    _not_send_sync: Rc<()>,
}

impl ImageStream {
    pub fn start(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live stream handle.
            unsafe { sys::ams_mel_ir_stream_start(self.raw, diagnostic, capacity, required) }
        })
    }

    /// Receives one owned Mono8 frame, waiting at most `timeout_ms`. Zero polls.
    pub fn receive(&mut self, timeout_ms: u32) -> Result<Frame, Error> {
        let mut raw = empty_frame();
        let first = receive_raw(self.raw, timeout_ms, &mut raw);
        match first {
            Err(error) if error.kind == ErrorKind::BufferTooSmall => {}
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
            Err(error) if error.kind == ErrorKind::BufferTooSmall => {
                return Err(protocol(
                    "queued frame pixel requirement changed during receive",
                ));
            }
            Err(error) if matches!(error.kind, ErrorKind::Timeout | ErrorKind::StreamStopped) => {
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

#[derive(Debug)]
pub struct ControlChannel {
    raw: *mut sys::AmsMelIrC2,
    _not_send_sync: Rc<()>,
}

impl ControlChannel {
    pub fn enable(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: this wrapper uniquely owns the live C2 handle.
            unsafe { sys::ams_mel_ir_c2_enable(self.raw, diagnostic, capacity, required) }
        })
    }

    /// Submits exactly Operate/TaskSched with native default scan parameters.
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

    /// Submits only the empty/no-op BIT profile; payload-bearing BIT is not exposed.
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
    pub fn close(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: native close may clear or retain this unique owner according
            // to its documented retryable-detach contract.
            unsafe { sys::ams_mel_ir_c2_close(&mut self.raw, diagnostic, capacity, required) }
        })
    }

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

#[derive(Debug)]
pub struct ModeRequest {
    raw: *mut sys::AmsMelIrModeRequest,
    _not_send_sync: Rc<()>,
}

impl ModeRequest {
    /// Waits finitely for a cached terminal result. Zero polls; timeout is not cancellation.
    pub fn wait(&mut self, timeout_ms: u32) -> Result<ModeResult, Error> {
        wait_for_mode(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
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
/// - [`ControlChannel::submit_bit_noop`] (C2 BIT, empty/no-op profile), and
/// - [`ChannelView::send_keepalive`] (common Channel KeepAlive, C2 or Image).
///
/// The native request owns its admitted family graph independently of the
/// View, typed owner, and Session that created it. Wait is cached; timeout is
/// not cancellation; `Return::Fail` is a normal completion; MEL rejection is
/// [`ReturnResult::Rejected`]. Close/Drop never cancel provider work.
#[derive(Debug)]
pub struct ReturnRequest {
    raw: *mut sys::AmsMelIrReturnRequest,
    _not_send_sync: Rc<()>,
}

impl ReturnRequest {
    /// Waits finitely for a cached terminal result. Zero polls; timeout is not cancellation.
    pub fn wait(&mut self, timeout_ms: u32) -> Result<ReturnResult, Error> {
        wait_for_return(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
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

/// Weak common Channel view of one safe typed owner ([`ControlChannel`] or
/// [`ImageStream`]).
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
/// provider teardown. After the typed owner is gone, every operation returns
/// [`ErrorKind::ProviderFailed`].
///
/// Requests created through a View ([`ReturnRequest`], [`CommsRequest`]) own
/// their admitted native family graph; closing or dropping the View does not
/// cancel or invalidate them.
///
/// The View is intentionally neither `Send` nor `Sync`, and every operation
/// takes `&mut self`, which statically serializes operations and Close on one
/// View as the native contract requires.
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
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error> {
        let channel = self.live("capabilities")?;
        let mut owner = CapabilityOwner {
            raw: ptr::null_mut(),
        };
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `channel` is this wrapper's live weak view, exclusively
            // used through `&mut self`; `owner.raw` is an initially null owner
            // which the guard closes on every return path.
            unsafe {
                sys::ams_mel_ir_channel_get_capabilities(
                    channel,
                    &mut owner.raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })?;
        if owner.raw.is_null() {
            return Err(protocol(
                "capability query succeeded without a capability owner",
            ));
        }
        let mut view: *const sys::AmsMelIrChannelCapabilityV1 = ptr::null();
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `owner.raw` is a live capability owner and `view` is
            // writable storage for the borrowed record pointer.
            unsafe {
                sys::ams_mel_ir_channel_capability_view(
                    owner.raw, &mut view, diagnostic, capacity, required,
                )
            }
        })?;
        if view.is_null() {
            return Err(protocol(
                "capability view succeeded without a capability record",
            ));
        }
        // SAFETY: `view` is non-null and points to a record owned by the live
        // `owner`; it is copied before `owner` is closed.
        let raw = unsafe { *view };
        let value = capability_from_raw(&raw)?;
        owner.close()?;
        Ok(value)
    }

    /// Destroys only the weak native view. Idempotent; it never closes the
    /// typed owner, cancels requests, or changes the family lifecycle.
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
    pub fn wait(&mut self, timeout_ms: u32) -> Result<CommsTestResult, Error> {
        wait_for_comms(self.raw, timeout_ms)
    }

    /// Drops only the public request owner; pending provider work is not cancelled.
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

/// Private RAII owner of one native capability snapshot. Drop closes the
/// native owner on every early return, including conversion/UTF-8 failures.
struct CapabilityOwner {
    raw: *mut sys::AmsMelIrChannelCapability,
}

impl CapabilityOwner {
    fn close(&mut self) -> Result<(), Error> {
        call_with_diagnostic(|diagnostic, capacity, required| {
            // SAFETY: `raw` is this guard's unique capability owner; no borrowed
            // record from it is used after this call.
            unsafe {
                sys::ams_mel_ir_channel_capability_close(
                    &mut self.raw,
                    diagnostic,
                    capacity,
                    required,
                )
            }
        })
    }
}

impl Drop for CapabilityOwner {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            best_effort_close_capability(&mut self.raw);
        }
    }
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
        uuid: value.uuid,
        descriptive_label: string_view(&value.descriptive_label),
    }
}

fn raw_image_config(value: &ImageConfig) -> sys::AmsMelIrStreamConfigV1 {
    sys::AmsMelIrStreamConfigV1 {
        channel_type: sys::AMS_MEL_IR_CHANNEL_IRST_IMAGE,
        channel_id: raw_uci_id(&value.channel_id),
        platform_id: raw_uci_id(&value.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: value.sensor_location.offset_x_m,
            offset_y_m: value.sensor_location.offset_y_m,
            offset_z_m: value.sensor_location.offset_z_m,
            key: string_view(&value.sensor_location.key),
            system_name: string_view(&value.sensor_location.system_name),
        },
        buffer_count: value.buffer_count,
        buffer_size: value.buffer_size,
        queue_capacity: value.queue_capacity,
    }
}

fn raw_control_config(value: &ControlConfig) -> sys::AmsMelIrC2ConfigV1 {
    sys::AmsMelIrC2ConfigV1 {
        channel_type: sys::AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL,
        channel_id: raw_uci_id(&value.channel_id),
        platform_id: raw_uci_id(&value.platform_id),
        sensor_location: sys::AmsMelComponentLocationV1 {
            offset_x_m: value.sensor_location.offset_x_m,
            offset_y_m: value.sensor_location.offset_y_m,
            offset_z_m: value.sensor_location.offset_z_m,
            key: string_view(&value.sensor_location.key),
            system_name: string_view(&value.sensor_location.system_name),
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
    let mut result = sys::AmsMelIrModeResultV1::default();
    let mut diagnostic = [0_u8; WAIT_DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    // SAFETY: the request is live and exclusively accessed through `&mut self`;
    // result and diagnostic storage are writable for their advertised sizes.
    let status = unsafe {
        sys::ams_mel_ir_mode_request_wait(
            request,
            timeout_ms,
            &mut result,
            diagnostic.as_mut_ptr().cast::<c_char>(),
            diagnostic.len(),
            &mut required,
        )
    };
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
        // SAFETY: terminal request results are cached and repeatable. The same
        // live request is polled with exact writable diagnostic storage.
        let retry_status = unsafe {
            sys::ams_mel_ir_mode_request_wait(
                request,
                0,
                &mut retry_result,
                complete.as_mut_ptr().cast::<c_char>(),
                complete.len(),
                &mut retry_required,
            )
        };
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
    let mut result = sys::AmsMelIrReturnResultV1::default();
    let mut diagnostic = [0_u8; WAIT_DIAGNOSTIC_CAPACITY];
    let mut required = 0_usize;
    // SAFETY: the request is live and exclusively accessed through `&mut self`;
    // result and diagnostic storage are writable for their advertised sizes.
    let status = unsafe {
        sys::ams_mel_ir_return_request_wait(
            request,
            timeout_ms,
            &mut result,
            diagnostic.as_mut_ptr().cast::<c_char>(),
            diagnostic.len(),
            &mut required,
        )
    };
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
        // SAFETY: terminal request results are cached and repeatable. The same
        // live request is polled with exact writable diagnostic storage.
        let retry_status = unsafe {
            sys::ams_mel_ir_return_request_wait(
                request,
                0,
                &mut retry_result,
                complete.as_mut_ptr().cast::<c_char>(),
                complete.len(),
                &mut retry_required,
            )
        };
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
    Ok(UciId {
        uuid: raw.uuid,
        descriptive_label: owned_text(raw.descriptive_label, field)?,
    })
}

fn owned_location(raw: &sys::AmsMelComponentLocationV1) -> Result<ComponentLocation, Error> {
    Ok(ComponentLocation {
        offset_x_m: raw.offset_x_m,
        offset_y_m: raw.offset_y_m,
        offset_z_m: raw.offset_z_m,
        key: owned_text(raw.key, "sensor_location.key")?,
        system_name: owned_text(raw.system_name, "sensor_location.system_name")?,
    })
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

fn reject_nul(value: &str, name: &str) -> Result<(), Error> {
    if value.as_bytes().contains(&0) {
        Err(Error::nul(name))
    } else {
        Ok(())
    }
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
    Error {
        kind,
        diagnostic: diagnostic.filter(|message| !message.is_empty()),
        diagnostic_required,
    }
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

fn best_effort_close_capability(raw: &mut *mut sys::AmsMelIrChannelCapability) {
    // SAFETY: called only for a unique capability snapshot owner. Null
    // diagnostics are supported and the result cannot be reported here.
    let _ = unsafe {
        sys::ams_mel_ir_channel_capability_close(raw, ptr::null_mut(), 0, ptr::null_mut())
    };
}

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
