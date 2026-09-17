//! Safe Rust provider sessions, IR Mono8 frames, and IR C2 through `ams_mel_c`.
//!
//! [`Session`], [`ImageStream`], [`ControlChannel`], and [`ModeRequest`] are
//! intentionally neither `Send` nor `Sync`. A zero receive or request timeout
//! polls. Request timeout and request drop do not cancel provider work.

use std::error;
use std::ffi::{c_char, CString};
use std::fmt;
use std::path::Path;
use std::ptr;
use std::rc::Rc;

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

impl Session {
    pub fn open(
        provider_library: impl AsRef<Path>,
        instance: impl AsRef<str>,
        aperture: impl AsRef<str>,
    ) -> Result<Self, Error> {
        let library = c_path(provider_library.as_ref())?;
        let instance = c_string(instance.as_ref(), "instance")?;
        let aperture = c_string(aperture.as_ref(), "aperture")?;
        let mut raw = ptr::null_mut();

        let status = call_with_diagnostic(|buffer, capacity, required| {
            // SAFETY: all C strings live through the call; `raw` is an initially
            // null writable owner and diagnostic storage matches its capacity.
            unsafe {
                sys::ams_mel_session_open(
                    library.as_ptr(),
                    instance.as_ptr(),
                    aperture.as_ptr(),
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
