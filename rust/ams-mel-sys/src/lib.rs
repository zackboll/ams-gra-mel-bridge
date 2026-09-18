//! Raw declarations for all current `ams_mel_c` ABI exports.

use std::ffi::{c_char, c_void};

pub type AmsMelStatus = i32;

pub const AMS_MEL_OK: AmsMelStatus = 0;
pub const AMS_MEL_INVALID_ARGUMENT: AmsMelStatus = 1;
pub const AMS_MEL_LIBRARY_LOAD_FAILED: AmsMelStatus = 2;
pub const AMS_MEL_SYMBOL_NOT_FOUND: AmsMelStatus = 3;
pub const AMS_MEL_FACTORY_FAILED: AmsMelStatus = 4;
pub const AMS_MEL_INITIALIZATION_FAILED: AmsMelStatus = 5;
pub const AMS_MEL_PROVIDER_EXCEPTION: AmsMelStatus = 6;
pub const AMS_MEL_BUFFER_TOO_SMALL: AmsMelStatus = 7;
pub const AMS_MEL_INTERNAL_ERROR: AmsMelStatus = 8;
pub const AMS_MEL_TIMEOUT: AmsMelStatus = 9;
pub const AMS_MEL_STREAM_STOPPED: AmsMelStatus = 10;
pub const AMS_MEL_PROVIDER_FAILED: AmsMelStatus = 11;
pub const AMS_MEL_COMMAND_REJECTED: AmsMelStatus = 12;

pub const AMS_MEL_IR_CHANNEL_IRST_IMAGE: u32 = 1;
pub const AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL: u32 = 2;
pub const AMS_MEL_IR_MFA_MODE_UNUSED: u32 = 0;
pub const AMS_MEL_IR_MFA_MODE_TASK_SCHED: u32 = 1;
pub const AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED: u32 = 2;
pub const AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED: u32 = 3;
pub const AMS_MEL_IR_MFA_STATE_NOT_SET: u32 = 0;
pub const AMS_MEL_IR_MFA_STATE_UNKNOWN: u32 = 1;
pub const AMS_MEL_IR_MFA_STATE_NOT_INSTALLED: u32 = 2;
pub const AMS_MEL_IR_MFA_STATE_OFF: u32 = 3;
pub const AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION: u32 = 4;
pub const AMS_MEL_IR_MFA_STATE_INITIALIZATION: u32 = 5;
pub const AMS_MEL_IR_MFA_STATE_STANDBY: u32 = 6;
pub const AMS_MEL_IR_MFA_STATE_OPERATE: u32 = 7;
pub const AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY: u32 = 8;
pub const AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY: u32 = 9;
pub const AMS_MEL_IR_MFA_STATE_MAINTENANCE: u32 = 10;
pub const AMS_MEL_IR_MFA_STATE_CALIBRATION: u32 = 11;
pub const AMS_MEL_IR_MFA_STATE_INITIATED_BIT: u32 = 12;
pub const AMS_MEL_IR_MFA_STATE_SHUTDOWN: u32 = 13;
pub const AMS_MEL_IR_MFA_STATE_DEGRADED: u32 = 14;
pub const AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE: u32 = 15;
pub const AMS_MEL_IR_COORD_FRAME_INERTIAL: u32 = 0;
pub const AMS_MEL_IR_COORD_FRAME_AIRCRAFT: u32 = 1;
pub const AMS_MEL_IR_DEGRADATION_CAPACITY: u32 = 0;
pub const AMS_MEL_IR_DEGRADATION_VOLUME: u32 = 1;
pub const AMS_MEL_IR_DEGRADATION_RANGE: u32 = 2;
pub const AMS_MEL_IR_DEGRADATION_REVISIT: u32 = 3;
pub type AmsMelIrReturn = u32;
pub const AMS_MEL_IR_RETURN_SUCCESS: AmsMelIrReturn = 0;
pub const AMS_MEL_IR_RETURN_BAD_POINTER: AmsMelIrReturn = 1;
pub const AMS_MEL_IR_RETURN_FAIL: AmsMelIrReturn = 2;
pub const AMS_MEL_IR_RETURN_NOT_SUPPORTED: AmsMelIrReturn = 3;
pub const AMS_MEL_IR_RETURN_NOT_IMPLEMENTED: AmsMelIrReturn = 4;
pub const AMS_MEL_ERROR_NONE: u32 = 0;
pub const AMS_MEL_ERROR_INVALID_ID: u32 = 1;
pub const AMS_MEL_ERROR_INVALID_STATE: u32 = 2;
pub const AMS_MEL_ERROR_INVALID_PARAMETERS: u32 = 3;
pub const AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS: u32 = 4;
pub const AMS_MEL_ERROR_INSUFFICIENT_RESOURCES: u32 = 5;
pub const AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES: u32 = 6;
pub const AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES: u32 = 7;
pub const AMS_MEL_ERROR_UNSUPPORTED: u32 = 8;
pub const AMS_MEL_IR_PIXEL_MONO: u32 = 0;
pub const AMS_MEL_IR_IMAGE_STARING: u32 = 0;
pub const AMS_MEL_IR_IMAGE_SCANNING: u32 = 1;
pub const AMS_MEL_IR_FLIP_NONE: u32 = 0;
pub const AMS_MEL_IR_FLIP_VERTICAL: u32 = 1;
pub const AMS_MEL_IR_FLIP_HORIZONTAL: u32 = 2;
pub const AMS_MEL_IR_FLIP_BOTH: u32 = 3;

pub const AMS_MEL_ABI_VERSION_MAJOR: u32 = 0;
pub const AMS_MEL_ABI_VERSION_MINOR: u32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct AmsMelAbiVersionV1 {
    pub major: u32,
    pub minor: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct AmsMelProviderVersionV1 {
    pub api_version: u32,
    pub library_version: u32,
    pub vendor: *mut c_char,
    pub vendor_capacity: usize,
    pub vendor_required: usize,
    pub description: *mut c_char,
    pub description_capacity: usize,
    pub description_required: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelStringViewV1 {
    pub data: *const c_char,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelU32SpanV1 {
    pub data: *const u32,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelStringViewSpanV1 {
    pub data: *const AmsMelStringViewV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrScanTypeV1 {
    pub continuous_scan: u32,
    pub returning: u32,
    pub agile_scan: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrScanParamV1 {
    pub elevation_defined_with_range_and_altitude: u32,
    pub center_az_rad: f64,
    pub center_el_rad: f64,
    pub center_frame_ref_el: u32,
    pub center_frame_ref_az: u32,
    pub scan_width_rad: f64,
    pub scan_height_rad: f64,
    pub scan_type: AmsMelIrScanTypeV1,
    pub scan_id: u32,
    pub scan_rate_rad_per_second: f64,
    pub preferred_revisit_interval_seconds: f64,
    pub required_revisit_interval_seconds: f64,
    pub max_range_of_interest_m: u32,
    pub min_range_of_interest_m: u32,
    pub elevation_scan_center_altitude_m: u32,
    pub elevation_scan_center_range_m: u32,
    pub degradation_method: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrModeCommandV1 {
    pub command_id: u32,
    pub state: u32,
    pub mode: u32,
    pub scan_parameters: AmsMelIrScanParamV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrBitCommandV1 {
    pub command_id: u32,
    pub initiate_bit_ids: AmsMelU32SpanV1,
    pub cancel_bit_ids: AmsMelU32SpanV1,
    pub clear_fault_codes: AmsMelStringViewSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrConfigSetCommandV1 {
    pub command_id: u32,
    pub system_time_ns: i64,
    pub config: AmsMelStringViewV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelUciIdV1 {
    pub uuid: [u8; 16],
    pub descriptive_label: AmsMelStringViewV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelComponentLocationV1 {
    pub offset_x_m: f64,
    pub offset_y_m: f64,
    pub offset_z_m: f64,
    pub key: AmsMelStringViewV1,
    pub system_name: AmsMelStringViewV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrStreamConfigV1 {
    pub channel_type: u32,
    pub channel_id: AmsMelUciIdV1,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
    pub buffer_count: usize,
    pub buffer_size: usize,
    pub queue_capacity: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrC2ConfigV1 {
    pub channel_type: u32,
    pub channel_id: AmsMelUciIdV1,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct AmsMelIrModeResultV1 {
    pub mode: u32,
    pub error_code: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct AmsMelIrReturnResultV1 {
    pub value: AmsMelIrReturn,
    pub error_code: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct AmsMelIrFrameV1 {
    pub system_time_ns: i64,
    pub integration_time_ns: i64,
    pub width: u32,
    pub height: u32,
    pub bits_per_pixel: u32,
    pub number_of_bands: u32,
    pub horizontal_fov_rad: f64,
    pub vertical_fov_rad: f64,
    pub pixel_format: u32,
    pub frame_id: u32,
    pub subframe_id: u32,
    pub subframe_total: u32,
    pub image_type: u32,
    pub image_flip: u32,
    pub image_flags: u32,
    pub dither_row: f64,
    pub dither_column: f64,
    pub row_offset: u32,
    pub column_offset: u32,
    pub band_index: u8,
    pub reserved: [u8; 7],
    pub pixels: *mut u8,
    pub pixel_capacity: usize,
    pub pixel_required: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct AmsMelIrStreamCountersV1 {
    pub frames_received: u64,
    pub frames_dropped_queue_full: u64,
    pub malformed_or_unsupported_frames: u64,
}

#[repr(C)]
pub struct AmsMelSession {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

#[repr(C)]
pub struct AmsMelIrStream {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

#[repr(C)]
pub struct AmsMelIrC2 {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

#[repr(C)]
pub struct AmsMelIrModeRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

#[repr(C)]
pub struct AmsMelIrReturnRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

extern "C" {
    pub fn ams_mel_get_abi_version(out_version: *mut AmsMelAbiVersionV1) -> AmsMelStatus;

    pub fn ams_mel_session_open(
        library_path: *const c_char,
        instance: *const c_char,
        aperture_config_id: *const c_char,
        out_session: *mut *mut AmsMelSession,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_session_get_provider_version(
        session: *const AmsMelSession,
        out_version: *mut AmsMelProviderVersionV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_session_close(
        session: *mut *mut AmsMelSession,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_open(
        session: *const AmsMelSession,
        config: *const AmsMelIrStreamConfigV1,
        out_stream: *mut *mut AmsMelIrStream,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_start(
        stream: *mut AmsMelIrStream,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_receive(
        stream: *mut AmsMelIrStream,
        timeout_ms: u32,
        out_frame: *mut AmsMelIrFrameV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_get_counters(
        stream: *const AmsMelIrStream,
        out_counters: *mut AmsMelIrStreamCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_stop(
        stream: *mut AmsMelIrStream,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_close(
        stream: *mut *mut AmsMelIrStream,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_c2_open(
        session: *const AmsMelSession,
        config: *const AmsMelIrC2ConfigV1,
        out_c2: *mut *mut AmsMelIrC2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_c2_enable(
        c2: *mut AmsMelIrC2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_c2_submit_operate(
        c2: *mut AmsMelIrC2,
        command_id: u32,
        out_request: *mut *mut AmsMelIrModeRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_submit_mode(
        c2: *mut AmsMelIrC2,
        command: *const AmsMelIrModeCommandV1,
        out_request: *mut *mut AmsMelIrModeRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_mode_request_wait(
        request: *const AmsMelIrModeRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrModeResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_mode_request_close(
        request: *mut *mut AmsMelIrModeRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_c2_submit_bit_noop(
        c2: *mut AmsMelIrC2,
        command_id: u32,
        out_request: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_submit_bit(
        c2: *mut AmsMelIrC2,
        command: *const AmsMelIrBitCommandV1,
        out_request: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_submit_config_set(
        c2: *mut AmsMelIrC2,
        command: *const AmsMelIrConfigSetCommandV1,
        out_request: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_return_request_wait(
        request: *const AmsMelIrReturnRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrReturnResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_return_request_close(
        request: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_c2_close(
        c2: *mut *mut AmsMelIrC2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}
