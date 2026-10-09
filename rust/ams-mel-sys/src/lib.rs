//! Raw declarations for all current `ams_mel_c` ABI exports.

use std::ffi::{c_char, c_void};

extern "C" {
    pub fn ams_mel_rf_virtual_aperture_get_tx_radiated_power(
        va: *const AmsMelRfVa,
        tx_element_group_id: u64,
        tx_power_mode_id: u32,
        tx_attenuation_db: f64,
        tx_weight_type: u64,
        center_frequency_hz: f64,
        u: f64,
        v: f64,
        va_instance_id: u32,
        out_power_dbw: *mut f64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power(
        va: *const AmsMelRfVa,
        tx_element_group_id: u64,
        tx_power_mode_id: u32,
        tx_attenuation_db: f64,
        center_frequency_hz: f64,
        va_instance_id: u32,
        out_power_dbw: *mut f64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_tx_aperture_gain(
        va: *const AmsMelRfVa,
        tx_element_group_id: u64,
        tx_power_mode_id: u32,
        tx_weight_type: u64,
        center_frequency_hz: f64,
        u: f64,
        v: f64,
        va_instance_id: u32,
        out_gain_db: *mut f64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_max_tx_attenuation(
        va: *const AmsMelRfVa,
        tx_element_group_id: u64,
        tx_power_mode_id: u32,
        va_instance_id: u32,
        out_attenuation_db: *mut f64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}
#[repr(C)]
pub struct AmsMelRfVaDataPipeConnectionsSnapshot {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfVaDataPipeGroupV1 {
    pub element_group_lookup_label: AmsMelStringViewV1,
    pub data_pipes: AmsMelRfDataPipeInfoSpanV1,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfVaDataPipeGroupSpanV1 {
    pub data: *const AmsMelRfVaDataPipeGroupV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfVaDataPipeConnectionsSnapshotV1 {
    pub groups: AmsMelRfVaDataPipeGroupSpanV1,
}
unsafe extern "C" {
    pub fn ams_mel_rf_virtual_aperture_get_data_pipes(
        va: *const AmsMelRfVa,
        output: *mut *mut AmsMelRfVaDataPipeConnectionsSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_data_pipe_connections_snapshot_view(
        owner: *const AmsMelRfVaDataPipeConnectionsSnapshot,
        output: *mut *const AmsMelRfVaDataPipeConnectionsSnapshotV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_data_pipe_connections_snapshot_close(
        owner: *mut *mut AmsMelRfVaDataPipeConnectionsSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint(
        va: *mut AmsMelRfVa,
        group: AmsMelStringViewV1,
        pipe: AmsMelStringViewV1,
        endpoint: u64,
        accepted: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints(
        va: *mut AmsMelRfVa,
        group: AmsMelStringViewV1,
        pipe: AmsMelStringViewV1,
        endpoints: AmsMelU64SpanV1,
        accepted: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}
#[repr(C)]
pub struct AmsMelRfVaLocalFunctionList {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVaLocalFunctionStatus {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfVaLocalFunctionInfoV1 {
    pub local_function_type_id: u32,
    pub instance_count: u64,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfVaLocalFunctionInfoSpanV1 {
    pub data: *const AmsMelRfVaLocalFunctionInfoV1,
    pub size: usize,
}
unsafe extern "C" {
    pub fn ams_mel_rf_virtual_aperture_is_cached_waveform_supported(
        va: *const AmsMelRfVa,
        output: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_dynamic_weights_supported(
        va: *const AmsMelRfVa,
        output: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_local_functions(
        va: *const AmsMelRfVa,
        output: *mut *mut AmsMelRfVaLocalFunctionList,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_local_function_list_view(
        owner: *const AmsMelRfVaLocalFunctionList,
        output: *mut AmsMelRfVaLocalFunctionInfoSpanV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_local_function_list_close(
        owner: *mut *mut AmsMelRfVaLocalFunctionList,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_local_function_status(
        va: *const AmsMelRfVa,
        instance: u32,
        type_id: u32,
        output: *mut *mut AmsMelRfVaLocalFunctionStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_local_function_status_view(
        owner: *const AmsMelRfVaLocalFunctionStatus,
        output: *mut AmsMelU32SpanV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_local_function_status_close(
        owner: *mut *mut AmsMelRfVaLocalFunctionStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

#[repr(C)]
pub struct AmsMelRfElementGroupSnapshot {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
pub type AmsMelRfElementGroupMode = u32;
pub const AMS_MEL_RF_ELEMENT_GROUP_MODE_RX: AmsMelRfElementGroupMode = 0;
pub const AMS_MEL_RF_ELEMENT_GROUP_MODE_TX: AmsMelRfElementGroupMode = 1;
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfElementGroupSnapshotOptionsV1 {
    pub include_data_pipes: u32,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfDataPipeInfoV1 {
    pub lookup_label: AmsMelStringViewV1,
    pub label: AmsMelStringViewV1,
    pub associated_endpoint_ids: AmsMelU64SpanV1,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfDataPipeInfoSpanV1 {
    pub data: *const AmsMelRfDataPipeInfoV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfElementGroupDescriptorV1 {
    pub lookup_label: AmsMelStringViewV1,
    pub label: AmsMelStringViewV1,
    pub mode: AmsMelRfElementGroupMode,
    pub max_rf_bandwidth_hz: f64,
    pub max_sample_rate_samples_per_second: f64,
    pub max_data_rate_bits_per_second: f64,
    pub max_duty_factor: f64,
    pub data_pipes: AmsMelRfDataPipeInfoSpanV1,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfElementGroupDescriptorSpanV1 {
    pub data: *const AmsMelRfElementGroupDescriptorV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct AmsMelRfElementGroupSnapshotV1 {
    pub data_pipes_included: u32,
    pub descriptors: AmsMelRfElementGroupDescriptorSpanV1,
}
unsafe extern "C" {
    pub fn ams_mel_rf_virtual_aperture_get_element_groups(
        va: *const AmsMelRfVa,
        options: *const AmsMelRfElementGroupSnapshotOptionsV1,
        output: *mut *mut AmsMelRfElementGroupSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_element_group_snapshot_view(
        snapshot: *const AmsMelRfElementGroupSnapshot,
        output: *mut *const AmsMelRfElementGroupSnapshotV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_element_group_snapshot_close(
        snapshot: *mut *mut AmsMelRfElementGroupSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

/// Signal-only observer; no provider resource ownership. Raw ABI only.
#[repr(C)]
pub struct AmsMelRfVaStatusSubscription {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct AmsMelRfVaStatusSubscriptionStatisticsV1 {
    pub callback_entries: u64,
    pub callbacks_coalesced: u64,
    pub notifications_delivered: u64,
    pub callbacks_after_stop: u64,
    pub pending: u32,
    pub stopped: u32,
}
unsafe extern "C" {
    pub fn ams_mel_rf_va_status_subscription_open(
        va: *mut AmsMelRfVa,
        output: *mut *mut AmsMelRfVaStatusSubscription,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_status_subscription_wait(
        subscription: *mut AmsMelRfVaStatusSubscription,
        timeout_ms: u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_status_subscription_get_statistics(
        subscription: *const AmsMelRfVaStatusSubscription,
        output: *mut AmsMelRfVaStatusSubscriptionStatisticsV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_status_subscription_unsubscribe(
        va: *mut AmsMelRfVa,
        subscription: *mut AmsMelRfVaStatusSubscription,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_status_subscription_close(
        subscription: *mut *mut AmsMelRfVaStatusSubscription,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

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
pub const AMS_MEL_RESOURCE_EXHAUSTED: AmsMelStatus = 13;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelSessionOptionsV1 {
    pub max_async_requests: u32,
}

pub const AMS_MEL_IR_CHANNEL_IRST_TRACK: u32 = 0;
pub const AMS_MEL_IR_CHANNEL_IRST_IMAGE: u32 = 1;
pub const AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL: u32 = 2;
pub const AMS_MEL_IR_CHANNEL_SCHEDULING: u32 = 3;
pub const AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS: u32 = 4;
pub const AMS_MEL_IR_CHANNEL_INSTRUMENTATION: u32 = 5;
pub const AMS_MEL_IR_CHANNEL_STACKED_IMAGE: u32 = 6;
pub const AMS_MEL_IR_CHANNEL_RESERVED_1: u32 = 7;
pub const AMS_MEL_IR_CHANNEL_RESERVED_2: u32 = 8;
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
pub const AMS_MEL_STATE_TRANSITION_NOT_SET: u32 = 0;
pub const AMS_MEL_STATE_TRANSITION_NOT_TRANSITIONING: u32 = 1;
pub const AMS_MEL_STATE_TRANSITION_SHUTTING_DOWN: u32 = 2;
pub const AMS_MEL_STATE_TRANSITION_TRANSITIONING: u32 = 3;
pub const AMS_MEL_COMPONENT_STATE_NOT_SET: u32 = 0;
pub const AMS_MEL_COMPONENT_STATE_UNKNOWN: u32 = 1;
pub const AMS_MEL_COMPONENT_STATE_NOT_INSTALLED: u32 = 2;
pub const AMS_MEL_COMPONENT_STATE_OFF: u32 = 3;
pub const AMS_MEL_COMPONENT_STATE_INITIALIZING: u32 = 4;
pub const AMS_MEL_COMPONENT_STATE_OPERATIONAL: u32 = 5;
pub const AMS_MEL_COMPONENT_STATE_DEGRADED: u32 = 6;
pub const AMS_MEL_COMPONENT_STATE_DISABLED: u32 = 7;
pub const AMS_MEL_COMPONENT_STATE_FAULTED: u32 = 8;
pub const AMS_MEL_TEMPERATURE_STATE_NOT_SET: u32 = 0;
pub const AMS_MEL_TEMPERATURE_STATE_UNDER_TEMP: u32 = 1;
pub const AMS_MEL_TEMPERATURE_STATE_NORMAL: u32 = 2;
pub const AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_WARNING: u32 = 3;
pub const AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_DEGRADED: u32 = 4;
pub const AMS_MEL_TEMPERATURE_STATE_OVER_TEMP_SHUTDOWN: u32 = 5;
pub const AMS_MEL_IR_FAILURE_NA: u32 = 0;
pub const AMS_MEL_IR_FAILURE_CRITICAL: u32 = 1;
pub const AMS_MEL_IR_FAILURE_MAJOR: u32 = 2;
pub const AMS_MEL_IR_FAILURE_PARAMETRIC: u32 = 3;
pub const AMS_MEL_IR_FAILURE_INFORMATIONAL: u32 = 4;
pub const AMS_MEL_IR_FAILURE_AVAILABLE: u32 = 5;
pub const AMS_MEL_IR_FAILURE_NOT_PRESENT: u32 = 6;
pub const AMS_MEL_IR_CSCI_MODE_UNKNOWN: u32 = 0;
pub const AMS_MEL_IR_CSCI_MODE_UNUSED: u32 = 1;
pub const AMS_MEL_IR_CSCI_MODE_INITIALIZATION: u32 = 2;
pub const AMS_MEL_IR_CSCI_MODE_MAINTENANCE: u32 = 3;
pub const AMS_MEL_IR_CSCI_MODE_IDLE: u32 = 4;
pub const AMS_MEL_IR_CSCI_MODE_OPERATIONAL: u32 = 5;
pub const AMS_MEL_IR_CSCI_MODE_VSA: u32 = 6;
pub const AMS_MEL_IR_CSCI_MODE_QUICK_LOOK: u32 = 7;
pub const AMS_MEL_IR_CSCI_MODE_TRACK: u32 = 8;
pub const AMS_MEL_IR_CSCI_MODE_IMAGING: u32 = 9;
pub const AMS_MEL_IR_CSCI_MODE_NOISE: u32 = 10;
pub const AMS_MEL_SECURITY_EVENT_NONE: u32 = 0;
pub const AMS_MEL_SECURITY_EVENT_AUTHENTICATION: u32 = 1;
pub const AMS_MEL_SECURITY_EVENT_INTEGRITY: u32 = 2;
pub const AMS_MEL_SECURITY_EVENT_FILE_MANAGEMENT: u32 = 3;
pub const AMS_MEL_SECURITY_EVENT_KEY_MANAGEMENT: u32 = 4;
pub const AMS_MEL_SECURITY_EVENT_SYSTEM: u32 = 5;
pub const AMS_MEL_SECURITY_EVENT_SANITIZATION: u32 = 6;
pub const AMS_MEL_SECURITY_OUTCOME_NOT_SET: u32 = 0;
pub const AMS_MEL_SECURITY_OUTCOME_FAILURE: u32 = 1;
pub const AMS_MEL_SECURITY_OUTCOME_SUCCESS: u32 = 2;
pub const AMS_MEL_SECURITY_SEVERITY_NOT_SET: u32 = 0;
pub const AMS_MEL_SECURITY_SEVERITY_CRITICAL: u32 = 1;
pub const AMS_MEL_SECURITY_SEVERITY_ERROR: u32 = 2;
pub const AMS_MEL_SECURITY_SEVERITY_INFORMATIONAL: u32 = 3;
pub const AMS_MEL_SECURITY_SEVERITY_WARNING: u32 = 4;
pub const AMS_MEL_IR_COORD_FRAME_INERTIAL: u32 = 0;
pub const AMS_MEL_IR_COORD_FRAME_AIRCRAFT: u32 = 1;
/* Upstream Priority defines exactly Normal and Debug; no MaxExclusive. */
pub const AMS_MEL_IR_PRIORITY_NORMAL: u32 = 0;
pub const AMS_MEL_IR_PRIORITY_DEBUG: u32 = 1;
pub const AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT: u32 = 1;
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
pub const AMS_MEL_IR_PIXEL_RGB: u32 = 1;
pub const AMS_MEL_IR_PIXEL_BAYER: u32 = 2;
pub const AMS_MEL_IR_SENSOR_UNSPECIFIED: u32 = 0;
pub const AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL: u32 = 1;
pub const AMS_MEL_IR_SENSOR_GIMBAL_VERTICAL: u32 = 2;
pub const AMS_MEL_IR_SENSOR_GIMBAL_ROTATION: u32 = 3;
pub const AMS_MEL_IR_SENSOR_STEP_STARE: u32 = 4;
pub const AMS_MEL_IR_BAND_INVALID: u32 = 0;
pub const AMS_MEL_IR_BAND_MULTIBAND: u32 = 1;
pub const AMS_MEL_IR_BAND_IR_FAR: u32 = 2;
pub const AMS_MEL_IR_BAND_IR_NEAR: u32 = 3;
pub const AMS_MEL_IR_BAND_IR_LONGWAVE: u32 = 4;
pub const AMS_MEL_IR_BAND_IR_MIDWAVE: u32 = 5;
pub const AMS_MEL_IR_BAND_IR_SHORTWAVE: u32 = 6;
pub const AMS_MEL_IR_BAND_VISIBLE_WHITE: u32 = 7;
pub const AMS_MEL_IR_BAND_VISIBLE_RED: u32 = 8;
pub const AMS_MEL_IR_BAND_VISIBLE_GREEN: u32 = 9;
pub const AMS_MEL_IR_BAND_VISIBLE_BLUE: u32 = 10;
pub const AMS_MEL_IR_BAND_UVA: u32 = 11;
pub const AMS_MEL_IR_BAND_UVB: u32 = 12;
pub const AMS_MEL_IR_BAND_UVC: u32 = 13;
pub const AMS_MEL_IR_BAND_UV_VACUUM: u32 = 14;
pub const AMS_MEL_IR_COORDINATE_LLA: u32 = 0;
pub const AMS_MEL_IR_COORDINATE_ECEF: u32 = 1;
pub const AMS_MEL_IR_COORDINATE_NED_PLATFORM: u32 = 2;
pub const AMS_MEL_IR_COORDINATE_NED_SENSOR: u32 = 3;
pub const AMS_MEL_IR_METADATA_BAD_PIXEL_LIST: u32 = 0;
pub const AMS_MEL_IR_METADATA_OPTICAL_DISTORTION_MAP: u32 = 1;
pub const AMS_MEL_IR_METADATA_LF_STATUS: u32 = 2;
pub const AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT: u32 = 3;
pub const AMS_MEL_IR_METADATA_LINE_OF_SIGHT_QUATERNION: u32 = 4;
pub const AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER: u32 = 5;
pub const AMS_MEL_IR_METADATA_MFA_STATUS: u32 = 6;
pub const AMS_MEL_IR_METADATA_MFA_STATUS_DETAILED: u32 = 7;
pub const AMS_MEL_IR_METADATA_BIT_CONFIGURATION: u32 = 8;
pub const AMS_MEL_IR_METADATA_COMMAND_STATUS: u32 = 9;
pub const AMS_MEL_IR_METADATA_BIT_STATUS: u32 = 10;
pub const AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_MESSAGE: u32 = 11;
pub const AMS_MEL_IR_METADATA_TASK_EXECUTING_REP: u32 = 12;
pub const AMS_MEL_IR_METADATA_SUBSYSTEM_STATUS_RESP: u32 = 13;
pub const AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK: u32 = 14;
pub const AMS_MEL_IR_METADATA_SCHED_CREATED_REP: u32 = 15;
pub const AMS_MEL_IR_METADATA_IRST_TRACK_REPORT: u32 = 16;
pub const AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP: u32 = 17;
pub const AMS_MEL_IR_METADATA_CAMERA_COMMAND_RESP: u32 = 18;
pub const AMS_MEL_IR_METADATA_CAMERA_PROTECT_CMD_RESP: u32 = 19;
pub const AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT: u32 = 20;
pub const AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP: u32 = 21;
pub const AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA: u32 = 22;
pub const AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE: u32 = 23;
pub const AMS_MEL_IR_METADATA_LOS_3D_KINEMATICS_TYPE: u32 = 24;
pub const AMS_MEL_IR_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE: u32 = 25;
pub const AMS_MEL_IR_METADATA_TASK_EVENTS: u32 = 26;
pub const AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT: u32 = 27;
pub const AMS_MEL_IR_METADATA_RESERVED_3: u32 = 28;
pub const AMS_MEL_IR_METADATA_RESERVED_5: u32 = 29;
pub const AMS_MEL_IR_METADATA_RESERVED_9: u32 = 30;
pub const AMS_MEL_IR_METADATA_RESERVED_10: u32 = 31;
pub const AMS_MEL_IR_IMAGE_STARING: u32 = 0;
pub const AMS_MEL_IR_IMAGE_SCANNING: u32 = 1;
pub const AMS_MEL_IR_FLIP_NONE: u32 = 0;
pub const AMS_MEL_IR_FLIP_VERTICAL: u32 = 1;
pub const AMS_MEL_IR_FLIP_HORIZONTAL: u32 = 2;
pub const AMS_MEL_IR_FLIP_BOTH: u32 = 3;
pub const AMS_MEL_IR_C2_METADATA_COMMAND_STATUS: u32 = 1;
pub const AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION: u32 = 2;
pub const AMS_MEL_IR_C2_METADATA_BIT_STATUS: u32 = 3;
pub const AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST: u32 = 4;
pub const AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS: u32 = 1;
pub const AMS_MEL_IR_HEALTH_METADATA_BIT_STATUS: u32 = 2;
pub const AMS_MEL_IR_HEALTH_METADATA_SUBSYSTEM_STATUS: u32 = 3;
pub const AMS_MEL_IR_HEALTH_METADATA_DISCRETE_STATUS: u32 = 4;
pub const AMS_MEL_IR_HEALTH_METADATA_SECURITY_AUDIT: u32 = 5;
pub const AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS_DETAILED: u32 = 6;
pub const AMS_MEL_IR_COMMAND_NOT_SET: u32 = 0;
pub const AMS_MEL_IR_COMMAND_RECEIVED: u32 = 1;
pub const AMS_MEL_IR_COMMAND_ACCEPTED: u32 = 2;
pub const AMS_MEL_IR_COMMAND_REJECTED: u32 = 3;
pub const AMS_MEL_IR_COMMAND_CANCELLED: u32 = 4;
pub const AMS_MEL_IR_CANNOT_COMPLY_NOT_SET: u32 = 0;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS: u32 = 1;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE: u32 = 2;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION: u32 = 3;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT: u32 = 4;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING: u32 = 5;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT: u32 = 6;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP: u32 = 7;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION: u32 = 8;
pub const AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE: u32 = 9;
pub const AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE: u32 = 10;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF: u32 = 11;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE: u32 = 12;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY: u32 = 13;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE: u32 = 14;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME: u32 = 15;
pub const AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM: u32 = 16;
pub const AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE: u32 = 17;
pub const AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT: u32 = 18;
pub const AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS: u32 = 19;
pub const AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE: u32 = 20;
pub const AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE: u32 = 21;
pub const AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT: u32 = 22;
pub const AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT: u32 = 23;
pub const AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE: u32 = 24;
pub const AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT: u32 = 25;
pub const AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT: u32 = 26;
pub const AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE: u32 = 27;
pub const AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE: u32 = 28;
pub const AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES: u32 = 29;
pub const AMS_MEL_IR_CANNOT_COMPLY_RANKING: u32 = 30;
pub const AMS_MEL_IR_CANNOT_COMPLY_WEATHER: u32 = 31;
pub const AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE: u32 = 32;
pub const AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR: u32 = 33;
pub const AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING: u32 = 34;
pub const AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR: u32 = 35;
pub const AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET: u32 = 36;
pub const AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID: u32 = 37;
pub const AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER: u32 = 38;
pub const AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER: u32 = 39;
pub const AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR: u32 = 40;
pub const AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE: u32 = 41;
pub const AMS_MEL_IR_CANNOT_COMPLY_CANCELLED: u32 = 42;
pub const AMS_MEL_IR_CANNOT_COMPLY_OTHER: u32 = 43;
pub const AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN: u32 = 44;
pub const AMS_MEL_IR_CANNOT_COMPLY_ABORTED: u32 = 45;
pub const AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER: u32 = 46;
pub const AMS_MEL_BIT_CONTROL_NOT_SET: u32 = 0;
pub const AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND: u32 = 1;
pub const AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND: u32 = 2;
pub const AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED: u32 = 3;
pub const AMS_MEL_BIT_RESULT_NOT_SET: u32 = 0;
pub const AMS_MEL_BIT_RESULT_PASS: u32 = 1;
pub const AMS_MEL_BIT_RESULT_FAIL: u32 = 2;
pub const AMS_MEL_BIT_RESULT_INTERRUPTED: u32 = 3;
pub const AMS_MEL_BIT_RESULT_NOT_TESTED: u32 = 4;
pub const AMS_MEL_FAULT_SEVERITY_NOT_SET: u32 = 0;
pub const AMS_MEL_FAULT_SEVERITY_NOMINAL: u32 = 1;
pub const AMS_MEL_FAULT_SEVERITY_CAUTION: u32 = 2;
pub const AMS_MEL_FAULT_SEVERITY_WARNING: u32 = 3;
pub const AMS_MEL_FAULT_SEVERITY_FAILED: u32 = 4;
pub const AMS_MEL_FAULT_STATE_NOT_SET: u32 = 0;
pub const AMS_MEL_FAULT_STATE_SET: u32 = 1;
pub const AMS_MEL_FAULT_STATE_CLEARED: u32 = 2;
pub const AMS_MEL_FAULT_STATE_UNKNOWN: u32 = 3;

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
pub struct AmsMelU64SpanV1 {
    pub data: *const u64,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelU8SpanV1 {
    pub data: *const u8,
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
macro_rules! span {
    ($n:ident,$t:ty) => {
        #[repr(C)]
        #[derive(Clone, Copy, Debug)]
        pub struct $n {
            pub data: *const $t,
            pub size: usize,
        }
    };
}
span!(AmsMelUciIdSpanV1, AmsMelUciIdV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrCommandStatusV1 {
    pub command_id: u32,
    pub state: u32,
    pub reason_id: u32,
    pub reason_description: AmsMelStringViewV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelBitTypeV1 {
    pub bit_id: AmsMelUciIdV1,
    pub accepted_interface: u32,
    pub bit_item_names: AmsMelStringViewSpanV1,
    pub subsystem_component_ids: AmsMelUciIdSpanV1,
    pub expected_duration_ns: i64,
}
span!(AmsMelBitTypeSpanV1, AmsMelBitTypeV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelBitConfigurationV1 {
    pub bit_types: AmsMelBitTypeSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelActiveBitV1 {
    pub bit_id: AmsMelUciIdV1,
    pub estimated_completion_time_ns: i64,
    pub estimated_percent_complete: f64,
}
span!(AmsMelActiveBitSpanV1, AmsMelActiveBitV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelCompletedBitItemV1 {
    pub bit_item_name: AmsMelStringViewV1,
    pub result: u32,
    pub fail_reason: AmsMelStringViewV1,
}
span!(AmsMelCompletedBitItemSpanV1, AmsMelCompletedBitItemV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelCompletedBitV1 {
    pub bit_id: AmsMelUciIdV1,
    pub time_tag_ns: i64,
    pub result: u32,
    pub fail_reason: AmsMelStringViewV1,
    pub bit_items: AmsMelCompletedBitItemSpanV1,
}
span!(AmsMelCompletedBitSpanV1, AmsMelCompletedBitV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelFaultDataV1 {
    pub key: AmsMelStringViewV1,
    pub value: AmsMelStringViewV1,
    pub format: AmsMelStringViewV1,
    pub units: AmsMelStringViewV1,
}
span!(AmsMelFaultDataSpanV1, AmsMelFaultDataV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelFaultAmbiguityGroupV1 {
    pub diagnostic_test_ids: AmsMelUciIdSpanV1,
    pub component_ids: AmsMelUciIdSpanV1,
}
span!(AmsMelFaultAmbiguityGroupSpanV1, AmsMelFaultAmbiguityGroupV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelFaultV1 {
    pub fault_id: AmsMelUciIdV1,
    pub severity: u32,
    pub state: u32,
    pub fault_data: AmsMelFaultDataSpanV1,
    pub detection_time_ns: i64,
    pub fault_code: AmsMelStringViewV1,
    pub fault_description: AmsMelStringViewV1,
    pub component_ids: AmsMelUciIdSpanV1,
    pub ambiguity_groups: AmsMelFaultAmbiguityGroupSpanV1,
}
span!(AmsMelFaultSpanV1, AmsMelFaultV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelBitStatusV1 {
    pub active_bits: AmsMelActiveBitSpanV1,
    pub completed_bits: AmsMelCompletedBitSpanV1,
    pub faults: AmsMelFaultSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrChannelCommsTestReportV1 {
    pub command_id: u32,
    pub request_id: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrC2MetadataEventV1 {
    pub kind: u32,
    pub command_status: AmsMelIrCommandStatusV1,
    pub bit_configuration: AmsMelBitConfigurationV1,
    pub bit_status: AmsMelBitStatusV1,
    pub channel_comms_test: AmsMelIrChannelCommsTestReportV1,
}
pub const AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST: u32 = 1;
pub const AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT: u32 = 2;
pub const AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER: u32 = 3;
pub const AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE: u32 = 4;
pub const AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN: u32 = 0;
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrBadPixelV1 {
    pub row: u32,
    pub column: u32,
    pub reason: u32,
}
span!(AmsMelIrBadPixelSpanV1, AmsMelIrBadPixelV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrBadPixelListV1 {
    pub reported_size: u32,
    pub reported_count: u32,
    pub pixels: AmsMelIrBadPixelSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrAzElV1 {
    pub azimuth_rad: f64,
    pub elevation_rad: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrLineOfSightReportV1 {
    pub system_time_ns: i64,
    pub pointing_angle: AmsMelIrAzElV1,
    pub pointing_angle_rates: AmsMelIrAzElV1,
    pub at_speed: u8,
    pub in_tolerance: u8,
    pub platform_attitude: AmsMelEulerV1,
    pub validity_flag_bitfield: u32,
    pub image_rotation_rad: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrLineOfSightEulerV1 {
    pub system_time_ns: i64,
    pub attitude: AmsMelEulerV1,
    pub attitude_rates: AmsMelEulerV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrNavigationResponseV1 {
    pub system_time_ns: i64,
    pub command_id: u32,
    pub request_id: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrImageMetadataEventV1 {
    pub kind: u32,
    pub bad_pixel_list: AmsMelIrBadPixelListV1,
    pub line_of_sight_report: AmsMelIrLineOfSightReportV1,
    pub line_of_sight_euler: AmsMelIrLineOfSightEulerV1,
    pub navigation_response: AmsMelIrNavigationResponseV1,
}

pub const AMS_MEL_POSITION_SOLUTION_NOT_SET: u32 = 0;
pub const AMS_MEL_POSITION_SOLUTION_ALIGNING: u32 = 1;
pub const AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL: u32 = 2;
pub const AMS_MEL_POSITION_SOLUTION_GPS: u32 = 3;
pub const AMS_MEL_POSITION_SOLUTION_BLENDED: u32 = 4;
pub const AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE: u32 = 5;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelNorthEastDownV1 {
    pub north: f64,
    pub east: f64,
    pub down: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelAttitudeRateV1 {
    pub attitude_rate: AmsMelEulerV1,
    pub attitude_rate_time_ns: i64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelPositionVelocityCovarianceV1 {
    pub position_position_pn_pn: f64,
    pub position_position_pn_pe: f64,
    pub position_position_pn_pd: f64,
    pub position_position_pe_pe: f64,
    pub position_position_pe_pd: f64,
    pub position_position_pd_pd: f64,
    pub position_velocity_pn_vn: f64,
    pub position_velocity_pn_ve: f64,
    pub position_velocity_pn_vd: f64,
    pub position_velocity_pe_ve: f64,
    pub position_velocity_pe_vd: f64,
    pub position_velocity_pd_vd: f64,
    pub velocity_velocity_vn_vn: f64,
    pub velocity_velocity_vn_ve: f64,
    pub velocity_velocity_vn_vd: f64,
    pub velocity_velocity_ve_ve: f64,
    pub velocity_velocity_ve_vd: f64,
    pub velocity_velocity_vd_vd: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelNavigationReportV1 {
    pub system_time_ns: i64,
    pub state: u32,
    pub latitude_rad: f64,
    pub longitude_rad: f64,
    pub altitude_m: f64,
    pub attitude: AmsMelEulerV1,
    pub attitude_rate: AmsMelAttitudeRateV1,
    pub speed: AmsMelNorthEastDownV1,
    pub acceleration: AmsMelNorthEastDownV1,
    pub wander_angle_rad: f64,
    pub magnetic_heading: f64,
    pub altitude_msl: f64,
    pub position_velocity_covariance_uncertainty: AmsMelPositionVelocityCovarianceV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrNavigationResultV1 {
    pub response: AmsMelIrNavigationResponseV1,
    pub error_code: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrC2MetadataCountersV1 {
    pub events_received: u64,
    pub events_dropped_queue_full: u64,
    pub malformed_or_unsupported: u64,
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
pub struct AmsMelIrContributingSensorV1 {
    pub location: AmsMelComponentLocationV1,
    pub sensor_id: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrDirectionalV1 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrQuaternionV1 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub w: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrNavErrorV1 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub w: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrUncertaintyV1 {
    pub sensor_uncertainties: u32,
    pub platform_uncertainties: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrOrientationV1 {
    pub kind: u32,
    pub euler: AmsMelEulerV1,
    pub quaternion: AmsMelIrQuaternionV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrSensorInertialStateV1 {
    pub system_time_ns: i64,
    pub q_xyzw: AmsMelIrQuaternionV1,
    pub q_ecef_xyzw: AmsMelIrQuaternionV1,
    pub sensor_position: AmsMelIrDirectionalV1,
    pub sensor_velocity: AmsMelIrDirectionalV1,
    pub uncertainties: AmsMelIrUncertaintyV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrSensorNavStateV1 {
    pub position: AmsMelIrDirectionalV1,
    pub position_error: AmsMelIrNavErrorV1,
    pub velocity: AmsMelIrDirectionalV1,
    pub velocity_error: AmsMelIrNavErrorV1,
    pub acceleration: AmsMelIrDirectionalV1,
    pub acceleration_error: AmsMelIrNavErrorV1,
    pub orientation: AmsMelIrOrientationV1,
    pub orientation_error: AmsMelIrNavErrorV1,
    pub orientation_velocity: AmsMelIrOrientationV1,
    pub orientation_velocity_error: AmsMelIrNavErrorV1,
    pub orientation_acceleration: AmsMelIrOrientationV1,
    pub orientation_acceleration_error: AmsMelIrNavErrorV1,
    pub coordinate_system: u32,
}
span!(
    AmsMelIrSensorInertialStateSpanV1,
    AmsMelIrSensorInertialStateV1
);
span!(AmsMelIrSensorNavStateSpanV1, AmsMelIrSensorNavStateV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrFrameSnapshotV1 {
    pub system_time_ns: i64,
    pub integration_time_ns: i64,
    pub width: u32,
    pub height: u32,
    pub bits_per_pixel: u32,
    pub number_of_bands: u32,
    pub horizontal_fov_rad: f64,
    pub vertical_fov_rad: f64,
    pub contributing_sensor: AmsMelIrContributingSensorV1,
    pub pixel_format: u32,
    pub frame_id: u32,
    pub subframe_id: u32,
    pub subframe_total: u32,
    pub image_type: u32,
    pub image_flip: u32,
    pub image_flags: AmsMelU32SpanV1,
    pub dither_row: f64,
    pub dither_column: f64,
    pub row_offset: u32,
    pub column_offset: u32,
    pub sensor_inertial_states: AmsMelIrSensorInertialStateSpanV1,
    pub sensor_nav_states: AmsMelIrSensorNavStateSpanV1,
    pub band_index: u8,
    pub pixels: AmsMelU8SpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrChannelCommsTestRequestV1 {
    pub command_id: u32,
    pub channel_id: u32,
    pub request_id: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrChannelCommsTestResultV1 {
    pub command_id: u32,
    pub request_id: u32,
    pub error_code: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrBandInfoV1 {
    pub kind: u32,
    pub min_wavelength_m: f64,
    pub max_wavelength_m: f64,
}
span!(AmsMelIrBandInfoSpanV1, AmsMelIrBandInfoV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrImageBandV1 {
    pub band_index: u32,
    pub bands: AmsMelIrBandInfoSpanV1,
}
span!(AmsMelIrImageBandSpanV1, AmsMelIrImageBandV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrChannelCapabilityV1 {
    pub channel_id: AmsMelUciIdV1,
    pub height: u32,
    pub width: u32,
    pub bit_depth: u32,
    pub row_pitch: u32,
    pub buffer_size: u32,
    pub image_size: u32,
    pub number_of_bands: u32,
    pub pixel_format: u32,
    pub sensor_types: AmsMelU32SpanV1,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
    pub channel_types: AmsMelU32SpanV1,
    pub task_schedule_depth: u32,
    pub odc_available: u32,
    pub nuc_available: u32,
    pub metadata_capabilities: AmsMelU32SpanV1,
    pub image_bands: AmsMelIrImageBandSpanV1,
    pub nav_frames: AmsMelU32SpanV1,
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
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrHealthConfigV1 {
    pub channel_id: AmsMelUciIdV1,
    pub channel_type: u32,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrInstrumentationConfigV1 {
    pub channel_id: AmsMelUciIdV1,
    pub channel_type: u32,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
}
/// Track channel configuration. `channel_type` must be
/// `AMS_MEL_IR_CHANNEL_IRST_TRACK`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrTrackConfigV1 {
    pub channel_id: AmsMelUciIdV1,
    pub channel_type: u32,
    pub platform_id: AmsMelUciIdV1,
    pub sensor_location: AmsMelComponentLocationV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrInstrumentationLevelCommandV1 {
    pub command_id: u32,
    pub priority: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrInstrumentationReportV1 {
    pub command_id: u32,
    pub size: u32,
    pub timestamp_ns: i64,
    pub priority: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrInstrumentationResultV1 {
    pub report: AmsMelIrInstrumentationReportV1,
    pub error_code: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrInstrumentationMetadataEventV1 {
    pub kind: u32,
    pub report: AmsMelIrInstrumentationReportV1,
}
/// Upstream `IrstTrackState`. No `MaxExclusive` value exists upstream, so any
/// provider value above `DROPPED` is malformed.
pub const AMS_MEL_IR_TRACK_STATE_IDLE: u32 = 0;
pub const AMS_MEL_IR_TRACK_STATE_DETECTED: u32 = 1;
pub const AMS_MEL_IR_TRACK_STATE_COAST: u32 = 2;
pub const AMS_MEL_IR_TRACK_STATE_DROPPED: u32 = 3;
/// Upstream `IrstTrackMode`. No `MaxExclusive` value exists upstream, so any
/// provider value above `STARE` is malformed.
pub const AMS_MEL_IR_TRACK_MODE_IDLE: u32 = 0;
pub const AMS_MEL_IR_TRACK_MODE_SCAN: u32 = 1;
pub const AMS_MEL_IR_TRACK_MODE_STARE: u32 = 2;
/// The Track metadata event kinds defined by this release. Every published
/// `TrackChannel`-specific metadata callback has a kind.
pub const AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT: u32 = 1;
pub const AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA: u32 = 2;
pub const AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE: u32 = 3;
pub const AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE: u32 = 4;

/// Upstream `CandidateObjectPreProc::candidateObjectWithBackground` is exactly
/// `std::array<std::array<std::int16_t, 3>, 3>`.
pub const AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE: u32 = 3;
pub const AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES: u32 = 9;

/// Upstream `MAX_CANDIDATE_OBJECTS`: the fixed storage length of the published
/// `std::array<CandidateObject, 900>`. `numberOfCOs` selects the meaningful
/// prefix; a larger count is malformed.
pub const AMS_MEL_IR_MAX_CANDIDATE_OBJECTS: u32 = 900;

/// Upstream `HotRegionTypeEnum`. No `MaxExclusive` value exists upstream, so
/// any provider value above `MASK` is malformed.
pub const AMS_MEL_IR_HOT_REGION_INVALID: u32 = 0;
pub const AMS_MEL_IR_HOT_REGION_FLARE: u32 = 1;
pub const AMS_MEL_IR_HOT_REGION_SOLAR: u32 = 2;
pub const AMS_MEL_IR_HOT_REGION_MASK: u32 = 3;

/// The one canonical row/column pair, matching upstream `RowCol`. It is
/// deliberately not an XYZ triple with a meaningless third component.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrRowColV1 {
    pub row: f64,
    pub column: f64,
}

/// Complete `HotRegion`. The geometry keeps its upstream `uint16_t` width.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrHotRegionV1 {
    pub kind: u32,
    pub size: u16,
    pub top: u16,
    pub left: u16,
    pub right: u16,
    pub bottom: u16,
}
span!(AmsMelIrHotRegionSpanV1, AmsMelIrHotRegionV1);

/// Complete `CandidateObjectHeader`. `cfar` is upstream `float` and stays
/// binary32; it is deliberately not widened to `f64`. The validity bitfield is
/// carried verbatim and is deliberately not decoded.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrCandidateObjectHeaderV1 {
    pub number_of_cos: u16,
    pub stack_frame_index: u16,
    pub cfar: f32,
    pub validity_flag_bitfield: u16,
    pub tov_utc_ns: i64,
}

/// Complete `CandidateObject`. Reuses the canonical row/column and XYZ
/// records; no value is clamped, normalized, or renormalized.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrCandidateObjectV1 {
    pub system_time_ns: i64,
    pub detection_category: u32,
    pub sensor_index: u32,
    pub subpixel: AmsMelIrRowColV1,
    pub intensity: f64,
    pub sensor_relative_unit: AmsMelIrDirectionalV1,
    pub signal_to_interference_ratio: f64,
    pub signal_to_noise_ratio: f64,
}
span!(AmsMelIrCandidateObjectSpanV1, AmsMelIrCandidateObjectV1);

/// Complete `CandidateObjectMessage`. The message class is annotated
/// @RequiredIfBuiltInTracker, the `TrackChannel` callback that delivers it is
/// @RequiredIfDetectCandidateObjects, and the contained `CandidateObject` class
/// is @RequiredIfTrack; these are three distinct upstream conditions.
///
/// Upstream declares no `send(CandidateObjectMessage)` and no
/// `RequestFor<CandidateObjectMessage>`, so this is inbound callback metadata.
/// Both spans borrow storage owned by the native event owner and stay valid
/// until event close.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrCandidateObjectMessageV1 {
    pub header: AmsMelIrCandidateObjectHeaderV1,
    pub inertial_state: AmsMelIrSensorInertialStateV1,
    pub hot_regions: AmsMelIrHotRegionSpanV1,
    pub candidate_objects: AmsMelIrCandidateObjectSpanV1,
}

impl Default for AmsMelIrCandidateObjectMessageV1 {
    /// Matches the zeroed native event: every unselected span keeps a null
    /// data pointer and a zero size. A raw pointer has no `Default`, so this
    /// impl is written out rather than derived.
    fn default() -> Self {
        Self {
            header: AmsMelIrCandidateObjectHeaderV1::default(),
            inertial_state: AmsMelIrSensorInertialStateV1::default(),
            hot_regions: AmsMelIrHotRegionSpanV1 {
                data: core::ptr::null(),
                size: 0,
            },
            candidate_objects: AmsMelIrCandidateObjectSpanV1 {
                data: core::ptr::null(),
                size: 0,
            },
        }
    }
}

/// Complete `RequestSystemTrackData`. Upstream declares this @Optional type
/// only as an inbound `registerMetadataCallback` overload on `TrackChannel`
/// and declares no matching `send`, so it arrives through the Track metadata
/// queue rather than through a request handle. `systemTime` is
/// `std::chrono::nanoseconds`, whose representation is signed.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrRequestSystemTrackDataV1 {
    pub system_time_ns: i64,
    pub command_id: u32,
    pub request_id: u32,
    pub track_id: u32,
}

/// Complete `IRSTTrackReport`. Reuses the canonical `AmsMelNorthEastDownV1`
/// for both NED vectors; no value is clamped or normalized.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackReportV1 {
    pub system_time_ns: i64,
    pub activity_id: u32,
    pub measured_ned: AmsMelNorthEastDownV1,
    pub measured_intensity: f64,
    pub measured_snr: f64,
    pub filtered_ned: AmsMelNorthEastDownV1,
    pub filtered_intensity: f64,
    pub filtered_snr: f64,
    pub range_m: f64,
    pub range_error_m: f64,
    pub spatial_extent_rad: f64,
    pub track_quality: f64,
    pub clutter: f64,
    pub age_ns: i64,
    pub state: u32,
    pub mode: u32,
}
/// FROZEN Track metadata event v1. Exactly these three members; the layout is
/// permanently fixed and nothing may be appended to it again. Later Track
/// metadata payloads use a new version record.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackMetadataEventV1 {
    pub kind: u32,
    pub track_report: AmsMelIrTrackReportV1,
    pub request_system_track_data: AmsMelIrRequestSystemTrackDataV1,
}

/// FROZEN Track metadata event v2: the complete frozen v1 record first, then
/// the additive `CandidateObjectMessage` payload. `base.kind` stays the one
/// discriminator and `offsetof(v2, base)` is 0. Exactly these two members; the
/// `CandidateObjectPreProcMessage` payload went into v3 instead.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackMetadataEventV2 {
    pub base: AmsMelIrTrackMetadataEventV1,
    pub candidate_object_message: AmsMelIrCandidateObjectMessageV1,
}

/// The one explicit fixed representation of the upstream 3 by 3 background
/// patch. The mapping is row-major: `samples[row * 3 + column]` is
/// `upstream[row][column]`. The element width stays upstream `i16`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrCandidateBackgroundV1 {
    pub samples: [i16; 9],
}

/// Complete `CandidateObjectPreProc`. Reuses the canonical row/column, XYZ, and
/// `SensorInertialState` records; each entry carries its OWN nested inertial
/// state. No value is clamped, normalized, or renormalized:
/// `candidate_object_quality` is documented upstream as "0 to 1" but its setter
/// enforces nothing. `edge` is upstream `bool`, normalized to exactly 0 or 1.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrCandidateObjectPreProcV1 {
    pub system_time_ns: i64,
    pub detection_category: u32,
    pub sensor_index: u32,
    pub subpixel: AmsMelIrRowColV1,
    pub intensity: f64,
    pub sensor_relative_unit: AmsMelIrDirectionalV1,
    pub signal_to_interference_ratio: f64,
    pub signal_to_noise_ratio: f64,
    pub candidate_object_with_background: AmsMelIrCandidateBackgroundV1,
    pub clutter: f64,
    pub candidate_object_quality: f64,
    pub sir_delta: f64,
    pub inertial_state: AmsMelIrSensorInertialStateV1,
    pub edge: u8,
    pub az_sigma: f64,
    pub el_sigma: f64,
    pub background_normalizer: f64,
}
span!(
    AmsMelIrCandidateObjectPreProcSpanV1,
    AmsMelIrCandidateObjectPreProcV1
);

/// Complete `CandidateObjectPreProcMessage`. The `TrackChannel` callback that
/// delivers it is annotated @Optional and is documented as intended for IR
/// MFAs that use `CandidateObjectPreProc`.
///
/// Upstream declares no `send(CandidateObjectPreProcMessage)` and no
/// `RequestFor<CandidateObjectPreProcMessage>`, so this is inbound callback
/// metadata. Unlike `CandidateObjectMessage`, the container is a
/// `std::vector`, so `candidate_object_preprocs.size` is the vector's own size
/// and `header.number_of_cos` is deliberately not used to truncate it: no such
/// invariant is published upstream. Both spans borrow event-owned storage
/// valid until event close.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrCandidateObjectPreProcMessageV1 {
    pub header: AmsMelIrCandidateObjectHeaderV1,
    pub inertial_state: AmsMelIrSensorInertialStateV1,
    pub hot_regions: AmsMelIrHotRegionSpanV1,
    pub candidate_object_preprocs: AmsMelIrCandidateObjectPreProcSpanV1,
}

impl Default for AmsMelIrCandidateObjectPreProcMessageV1 {
    /// Matches the zeroed native event: every unselected span keeps a null
    /// data pointer and a zero size. A raw pointer has no `Default`, so this
    /// impl is written out rather than derived.
    fn default() -> Self {
        Self {
            header: AmsMelIrCandidateObjectHeaderV1::default(),
            inertial_state: AmsMelIrSensorInertialStateV1::default(),
            hot_regions: AmsMelIrHotRegionSpanV1 {
                data: core::ptr::null(),
                size: 0,
            },
            candidate_object_preprocs: AmsMelIrCandidateObjectPreProcSpanV1 {
                data: core::ptr::null(),
                size: 0,
            },
        }
    }
}

/// Track metadata event v3: the complete frozen v2 record first, then the
/// additive `CandidateObjectPreProcMessage` payload. `base.base.kind` stays
/// the one discriminator, `offsetof(v3, base)` is 0, and
/// `size_of(v3.base) == size_of(v2)`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackMetadataEventV3 {
    pub base: AmsMelIrTrackMetadataEventV2,
    pub candidate_object_preproc_message: AmsMelIrCandidateObjectPreProcMessageV1,
}

/// Upstream `TrackStatus` (@RequiredIfTrackUpdate). No `MaxExclusive` value
/// exists upstream, so any input above `DELETE` is `INVALID_ARGUMENT`.
pub const AMS_MEL_IR_TRACK_STATUS_CREATE: u32 = 0;
pub const AMS_MEL_IR_TRACK_STATUS_UPDATE: u32 = 1;
pub const AMS_MEL_IR_TRACK_STATUS_PREDICT: u32 = 2;
pub const AMS_MEL_IR_TRACK_STATUS_DELETE: u32 = 3;

/// Every published `TrackDataUpdate` covariance term, exactly 21 doubles.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackCovarianceV1 {
    pub xx: f64,
    pub xy: f64,
    pub xz: f64,
    pub x_vx: f64,
    pub x_vy: f64,
    pub x_vz: f64,
    pub yy: f64,
    pub yz: f64,
    pub y_vx: f64,
    pub y_vy: f64,
    pub y_vz: f64,
    pub zz: f64,
    pub z_vx: f64,
    pub z_vy: f64,
    pub z_vz: f64,
    pub vx_vx: f64,
    pub vx_vy: f64,
    pub vx_vz: f64,
    pub vy_vy: f64,
    pub vy_vz: f64,
    pub vz_vz: f64,
}

/// Complete `TrackDataUpdate` input. The two times stay in upstream epoch
/// seconds and the canonical `AmsMelIrDirectionalV1` is reused for both ECEF
/// vectors; no value is clamped or normalized.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrTrackDataUpdateV1 {
    pub platform_id: u32,
    pub capability_uuid: AmsMelUciIdV1,
    pub activity_uuid: AmsMelUciIdV1,
    pub track_id: u32,
    pub entity_uuid: AmsMelUciIdV1,
    pub track_status: u32,
    pub time_of_validity_seconds: f64,
    pub time_of_last_update_seconds: f64,
    pub track_position_ecef: AmsMelIrDirectionalV1,
    pub track_velocity_ecef: AmsMelIrDirectionalV1,
    pub covariance: AmsMelIrTrackCovarianceV1,
    pub maneuver_probability: f64,
    pub track_quality: f64,
}

/// Terminal `TrackDataUpdate` outcome. Reuses the one generic
/// `AmsMelIrCommandStatusV1` layout. A successful `CommandStatus` whose own
/// state is `REJECTED` is still `AMS_MEL_OK`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrTrackUpdateResultV1 {
    pub status: AmsMelIrCommandStatusV1,
    pub error_code: u32,
}

/// Complete `SystemTrackDataResponse` input (@Optional). This is neither
/// `@RequiredIfTrack` nor `@RequiredIfTrackUpdate`.
///
/// `system_time_ns` stays signed nanoseconds, the ranges and rates stay in
/// upstream meters and meters/second, and the angles stay in radians; no value
/// is clamped or normalized. `az_el_valid` and `range_valid` use the
/// established `u8` representation for published bool values and accept only 0
/// or 1. The canonical `AmsMelIrAzElV1` is reused for both angle pairs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrSystemTrackDataResponseV1 {
    pub system_time_ns: i64,
    pub command_id: u32,
    pub request_id: u32,
    pub track_id: u32,
    pub range_m: f64,
    pub range_rate_mps: f64,
    pub range_error_m: f64,
    pub range_rate_error_mps: f64,
    pub az_el_valid: u8,
    pub range_valid: u8,
    pub inertial_az_el: AmsMelIrAzElV1,
    pub az_el_error: AmsMelIrAzElV1,
}

/// Terminal `SystemTrackDataResponse` outcome. It intentionally matches the
/// `TrackDataUpdate` result shape, because both upstream operations return
/// `RequestFor<CommandStatus>`, but it remains a semantically distinct public
/// type. A successful `CommandStatus` whose own state is `REJECTED` is still
/// `AMS_MEL_OK`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrTrackSystemResponseResultV1 {
    pub status: AmsMelIrCommandStatusV1,
    pub error_code: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelEulerV1 {
    pub roll: f64,
    pub pitch: f64,
    pub yaw: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelForeignKeyV1 {
    pub key: AmsMelStringViewV1,
    pub system_name: AmsMelStringViewV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelInstallationDetailsV1 {
    pub location: AmsMelComponentLocationV1,
    pub orientation: AmsMelEulerV1,
    pub boresight: AmsMelEulerV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelTemperatureStatusV1 {
    pub temperature_c: f64,
    pub state: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelMfaComponentV1 {
    pub component_id: AmsMelUciIdV1,
    pub state: u32,
    pub temperature: AmsMelTemperatureStatusV1,
    pub installation_location_id: AmsMelForeignKeyV1,
    pub installation_details: AmsMelInstallationDetailsV1,
}
span!(AmsMelMfaComponentSpanV1, AmsMelMfaComponentV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelAboutV1 {
    pub model: AmsMelStringViewV1,
    pub serial_number: AmsMelStringViewV1,
    pub software_version: AmsMelStringViewV1,
    pub bootloader_software_version: AmsMelStringViewV1,
    pub hardware_version: AmsMelStringViewV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelMfaStatusV1 {
    pub state: u32,
    pub state_description: AmsMelStringViewV1,
    pub mode_description: AmsMelStringViewV1,
    pub transition_status: u32,
    pub about: AmsMelAboutV1,
    pub components: AmsMelMfaComponentSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrSubsystemDepInfoV1 {
    pub subsystem_id: u32,
    pub criticality: u32,
    pub failure: u32,
}
span!(AmsMelIrSubsystemDepInfoSpanV1, AmsMelIrSubsystemDepInfoV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrVersionV1 {
    pub source: u32,
    pub major_revision: u32,
    pub minor_revision: u32,
    pub engineering_revision: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrSubsystemCsciInfoV1 {
    pub csci: AmsMelStringViewV1,
    pub mode: u32,
    pub version: AmsMelIrVersionV1,
    pub criticality: u32,
    pub failure: u32,
    pub bit_report: u32,
    pub connection_established: u32,
}
span!(AmsMelIrSubsystemCsciInfoSpanV1, AmsMelIrSubsystemCsciInfoV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrSubsystemStatusV1 {
    pub subsystem_id: u32,
    pub criticality: u32,
    pub status_sequence_number: u32,
    pub failure: u32,
    pub subsystem_count: u32,
    pub subsystems: AmsMelIrSubsystemDepInfoSpanV1,
    pub csci_count: u32,
    pub csci: AmsMelIrSubsystemCsciInfoSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelNameValuePairV1 {
    pub name: AmsMelStringViewV1,
    pub value: AmsMelStringViewV1,
}
span!(AmsMelNameValuePairSpanV1, AmsMelNameValuePairV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelSecurityArtifactV1 {
    pub component_id: AmsMelUciIdV1,
    pub associated_id: AmsMelUciIdV1,
}
span!(AmsMelSecurityArtifactSpanV1, AmsMelSecurityArtifactV1);
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelSecurityEventV1 {
    pub kind: u32,
    pub category: u32,
    pub details: AmsMelStringViewV1,
    pub subsystem_id: AmsMelUciIdV1,
    pub service_id: AmsMelUciIdV1,
    pub mdf_id: AmsMelUciIdV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelSecurityAuditRecordV1 {
    pub security_event_id: AmsMelUciIdV1,
    pub event_timestamp_ns: i64,
    pub subsystem_id: AmsMelUciIdV1,
    pub artifacts: AmsMelSecurityArtifactSpanV1,
    pub event: AmsMelSecurityEventV1,
    pub outcome: u32,
    pub severity: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelIrHealthMetadataEventV1 {
    pub kind: u32,
    pub mfa_status: AmsMelMfaStatusV1,
    pub bit_status: AmsMelBitStatusV1,
    pub subsystem_status: AmsMelIrSubsystemStatusV1,
    pub discrete_status: AmsMelNameValuePairSpanV1,
    pub security_audit: AmsMelSecurityAuditRecordV1,
    pub mfa_status_detailed: AmsMelNameValuePairSpanV1,
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
pub struct AmsMelIrFrameSnapshot {
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
#[repr(C)]
pub struct AmsMelIrChannelCommsRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrChannelCapability {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Opaque weak common Channel view (`ams_mel_ir_channel`, Task 032B1). Raw
/// declarations only; no safe Rust common Channel API is provided yet.
#[repr(C)]
pub struct AmsMelIrChannel {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrC2Metadata {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrC2MetadataEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrHealth {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrHealthMetadata {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrHealthMetadataEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrImageMetadata {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrImageMetadataEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrNavigationRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrInstrumentation {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrInstrumentationRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrInstrumentationMetadata {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrInstrumentationMetadataEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Opaque Track channel owner. This release provides the ownership/lifecycle
/// foundation, the @RequiredIfTrack IRSTTrackReport callback, and the
/// @RequiredIfTrackUpdate TrackDataUpdate send.
#[repr(C)]
pub struct AmsMelIrTrack {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Opaque public consumption owner for the retained IRSTTrackReport queue.
#[repr(C)]
pub struct AmsMelIrTrackMetadata {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelIrTrackMetadataEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Opaque owner of one asynchronous `send(TrackDataUpdate)` outcome. It owns a
/// shared terminal completion state and never a raw `AmsMelIrTrack` pointer.
#[repr(C)]
pub struct AmsMelIrTrackUpdateRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Opaque owner of one asynchronous `send(SystemTrackDataResponse)` outcome
/// (@Optional). It is a deliberately distinct public type from
/// `AmsMelIrTrackUpdateRequest`, and like it owns a shared terminal completion
/// state and never a raw `AmsMelIrTrack` pointer.
#[repr(C)]
pub struct AmsMelIrTrackSystemResponseRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

// Task 033B RF DataMEL foundation (raw only; no safe Rust RF API).

/// Opaque unique owner of one RF provider `DataMEL` and its provider DSO.
#[repr(C)]
pub struct AmsMelRfData {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Raw RF AdminMEL owner; state commands use the canonical Common MEL values.
#[repr(C)]
pub struct AmsMelRfAdmin {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Raw RF C2 lifecycle owner; no safe Rust RF API.
#[repr(C)]
pub struct AmsMelRfC2 {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVaRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVa {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVaConfigV1 {
    pub va_definition_id: u32,
    pub priority: u32,
    pub local_function_info: AmsMelStringViewSpanV1,
    pub va_definition_file_info: AmsMelStringViewV1,
    pub capability_ids: AmsMelUciIdSpanV1,
}
#[repr(C)]
pub struct AmsMelRfVaResultV1 {
    pub error_code: u32,
}
#[repr(C)]
pub struct AmsMelRfVaInfoV1 {
    pub va_instance_ids: AmsMelU32SpanV1,
    pub element_group_labels: AmsMelStringViewSpanV1,
    pub is_single_group: u32,
}
pub type AmsMelRfVirtualApertureStatus = u32;
pub const AMS_MEL_RF_VA_STATUS_NONE: AmsMelRfVirtualApertureStatus = 0;
pub const AMS_MEL_RF_VA_STATUS_OPERATIONAL: AmsMelRfVirtualApertureStatus = 1;
pub const AMS_MEL_RF_VA_STATUS_DEGRADED: AmsMelRfVirtualApertureStatus = 2;
pub const AMS_MEL_RF_VA_STATUS_FAILED: AmsMelRfVirtualApertureStatus = 3;
#[repr(C)]
pub struct AmsMelRfVaInstanceList {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVaInstanceStatusReport {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfVaLocalFunctionStatusV1 {
    pub local_function_type_id: u32,
    pub statuses: AmsMelU32SpanV1,
}
#[repr(C)]
pub struct AmsMelRfVaLocalFunctionStatusSpanV1 {
    pub data: *const AmsMelRfVaLocalFunctionStatusV1,
    pub size: usize,
}
#[repr(C)]
pub struct AmsMelRfVaInstanceStatusReportV1 {
    pub va_instance_id: u32,
    pub status: AmsMelRfVirtualApertureStatus,
    pub local_functions: AmsMelRfVaLocalFunctionStatusSpanV1,
}
#[repr(C)]
pub struct AmsMelRfJobRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfJob {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxElementGroupConfigV1 {
    pub label: AmsMelStringViewV1,
    pub desired_duty_factor: f64,
    pub expected_center_frequencies: AmsMelRfFrequencyRangeSpanV1,
    pub endpoint_ids: AmsMelU64SpanV1,
    pub data_pipe_label: AmsMelStringViewV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobRequestConfigV1 {
    pub request_id: u32,
    pub priority: u32,
    pub precedence_within_priority: u32,
    pub is_interruptable: u32,
    pub instance_selection: AmsMelU32SpanV1,
    pub rx_group: AmsMelRfRxElementGroupConfigV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfUtcTimeV1 {
    pub seconds: i64,
    pub fractional_femtoseconds: i64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxDataPipeEndpointConfigV1 {
    pub data_pipe_label: AmsMelStringViewV1,
    pub endpoint_ids: AmsMelU64SpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxDataPipeEndpointConfigSpanV1 {
    pub data: *const AmsMelRfRxDataPipeEndpointConfigV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxElementGroupConfigV2 {
    pub label: AmsMelStringViewV1,
    pub desired_duty_factor: f64,
    pub expected_center_frequencies: AmsMelRfFrequencyRangeSpanV1,
    pub data_pipe_endpoint_configs: AmsMelRfRxDataPipeEndpointConfigSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxElementGroupConfigSpanV2 {
    pub data: *const AmsMelRfRxElementGroupConfigV2,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobRequestConfigV2 {
    pub request_id: u32,
    pub priority: u32,
    pub precedence_within_priority: u32,
    pub is_interruptable: u32,
    pub instance_selection: AmsMelU32SpanV1,
    pub rx_groups: AmsMelRfRxElementGroupConfigSpanV2,
    pub min_start_time: AmsMelRfUtcTimeV1,
    pub max_complete_time: AmsMelRfUtcTimeV1,
    pub duration_femtoseconds: i64,
    pub capability_id: AmsMelU8SpanV1,
    pub activity_id: AmsMelU8SpanV1,
    pub tx_power_mode_ids: AmsMelU32SpanV1,
    pub lookahead_femtoseconds: i64,
}
pub type AmsMelRfPointingKind = u32;
pub const AMS_MEL_RF_POINTING_ECEF: AmsMelRfPointingKind = 0;
pub const AMS_MEL_RF_POINTING_LLA: AmsMelRfPointingKind = 1;
pub const AMS_MEL_RF_POINTING_PLATFORM_RELATIVE: AmsMelRfPointingKind = 2;
pub const AMS_MEL_RF_POINTING_FACE_RELATIVE: AmsMelRfPointingKind = 3;
pub const AMS_MEL_RF_POINTING_BASELINE_RELATIVE: AmsMelRfPointingKind = 4;
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfVector3V1 {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfAzElV1 {
    pub azimuth_rad: f64,
    pub elevation_rad: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfEcefPointingV1 {
    pub location_m: AmsMelRfVector3V1,
    pub velocity_mps: AmsMelRfVector3V1,
    pub time_of_validity: AmsMelRfUtcTimeV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfLlaPointingV1 {
    pub latitude_rad: f64,
    pub longitude_rad: f64,
    pub altitude_m: f64,
    pub velocity_north_mps: f64,
    pub velocity_east_mps: f64,
    pub velocity_down_mps: f64,
    pub time_of_validity: AmsMelRfUtcTimeV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPointingV1 {
    pub kind: AmsMelRfPointingKind,
    pub ecef: AmsMelRfEcefPointingV1,
    pub lla: AmsMelRfLlaPointingV1,
    pub platform_relative: AmsMelRfAzElV1,
    pub face_relative: AmsMelRfAzElV1,
    pub baseline_relative_conic_rad: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPointingSpanV1 {
    pub data: *const AmsMelRfPointingV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxElementGroupConfigV3 {
    pub group: AmsMelRfRxElementGroupConfigV2,
    pub expected_pointing_angles: AmsMelRfPointingSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfRxElementGroupConfigSpanV3 {
    pub data: *const AmsMelRfRxElementGroupConfigV3,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobRequestConfigV3 {
    pub request_id: u32,
    pub priority: u32,
    pub precedence_within_priority: u32,
    pub is_interruptable: u32,
    pub instance_selection: AmsMelU32SpanV1,
    pub rx_groups: AmsMelRfRxElementGroupConfigSpanV3,
    pub min_start_time: AmsMelRfUtcTimeV1,
    pub max_complete_time: AmsMelRfUtcTimeV1,
    pub duration_femtoseconds: i64,
    pub capability_id: AmsMelU8SpanV1,
    pub activity_id: AmsMelU8SpanV1,
    pub tx_power_mode_ids: AmsMelU32SpanV1,
    pub lookahead_femtoseconds: i64,
    pub has_estimated_stab_point: u32,
    pub estimated_stab_point: AmsMelRfPointingV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfTxElementGroupConfigV1 {
    pub label: AmsMelStringViewV1,
    pub tx_power_level: u32,
    pub desired_duty_factor: f64,
    pub expected_center_frequencies: AmsMelRfFrequencyRangeSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobElementGroupConfigV4 {
    pub mode: AmsMelRfElementGroupMode,
    pub rx: AmsMelRfRxElementGroupConfigV3,
    pub tx: AmsMelRfTxElementGroupConfigV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobElementGroupConfigSpanV4 {
    pub data: *const AmsMelRfJobElementGroupConfigV4,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobRequestConfigV4 {
    pub request_id: u32,
    pub priority: u32,
    pub precedence_within_priority: u32,
    pub is_interruptable: u32,
    pub instance_selection: AmsMelU32SpanV1,
    pub element_groups: AmsMelRfJobElementGroupConfigSpanV4,
    pub min_start_time: AmsMelRfUtcTimeV1,
    pub max_complete_time: AmsMelRfUtcTimeV1,
    pub duration_femtoseconds: i64,
    pub capability_id: AmsMelU8SpanV1,
    pub activity_id: AmsMelU8SpanV1,
    pub tx_power_mode_ids: AmsMelU32SpanV1,
    pub lookahead_femtoseconds: i64,
    pub has_estimated_stab_point: u32,
    pub estimated_stab_point: AmsMelRfPointingV1,
}

// Pinned sentinel aliases ordinary zero relative start; not i64::MAX.
pub const AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS: i64 = 0;
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigV1 {
    pub event_id: u32,
    pub element_group_label: AmsMelStringViewV1,
    pub start_femtoseconds: i64,
    pub duration_femtoseconds: i64,
    pub center_frequency_hz: f64,
    pub sample_frequency_hz: f64,
    pub agc_processing_iterations: u64,
    pub ignored_post_agc_iterations: u64,
    pub max_extension_femtoseconds: i64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigSpanV1 {
    pub data: *const AmsMelRfReceiveEventConfigV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV1 {
    pub interval_start_femtoseconds: i64,
    pub interval_id: u32,
    pub interval_starting_gap_femtoseconds: i64,
    pub sequence_duration_femtoseconds: i64,
    pub sequence_repeat_count: u64,
    pub calibration_duration_femtoseconds: i64,
    pub interval_ending_gap_femtoseconds: i64,
    pub phase_coherence_with_prior: u32,
    pub iterations_per_signal: u64,
    pub max_data_rate_bps: f64,
    pub max_sample_rate_hz: f64,
    pub job_details_id: u32,
    pub receive_events: AmsMelRfReceiveEventConfigSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV1 {
    pub data: *const AmsMelRfJobIntervalConfigV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobResultV1 {
    pub error_code: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobInfoV1 {
    pub actual_start_seconds: i64,
    pub actual_start_femtoseconds: i64,
    pub total_job_duration_femtoseconds: i64,
    pub va_instance_id: u32,
    pub va_definition_id: u32,
    pub job_details_id: u32,
    pub job_request_id: u32,
    pub lookahead_femtoseconds: i64,
    pub rx_stream_ids: AmsMelU32SpanV1,
}

#[repr(C)]
pub struct AmsMelRfPhysicalData {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfTxPowerModeSnapshot {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AmsMelRfTxPowerModeV1 {
    pub tx_power_mode_id: u32,
    pub is_linear_operation: u32,
    pub tx_power_level: u32,
    pub tx_frequency_ranges: AmsMelRfFrequencyRangeSpanV1,
    pub max_tx_duty_factor: f64,
    pub max_tx_pulse_width_ns: i64,
    pub max_tx_atten: f64,
    pub tx_atten_step_size: f64,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AmsMelRfTxPowerModeSpanV1 {
    pub data: *const AmsMelRfTxPowerModeV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AmsMelRfEulerV1 {
    pub roll_rad: f64,
    pub pitch_rad: f64,
    pub yaw_rad: f64,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AmsMelRfComponentLocationV1 {
    pub offset_x_m: f64,
    pub offset_y_m: f64,
    pub offset_z_m: f64,
    pub key: AmsMelStringViewV1,
    pub system_name: AmsMelStringViewV1,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct AmsMelRfPhysicalDataV1 {
    pub antenna_height_m: f64,
    pub antenna_width_m: f64,
    pub lattice_angle_rad: f64,
    pub location: AmsMelRfComponentLocationV1,
    pub orientation: AmsMelRfEulerV1,
    pub boresight: AmsMelRfEulerV1,
}
pub type AmsMelRfMfaState = u32;
/// Opaque, fully owned RFMFAInfo snapshot, independent of the provider.
#[repr(C)]
pub struct AmsMelRfMfaInfo {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

/// Raw upstream `rfmel::JobDataFormat`; unknown values are preserved.
pub type AmsMelRfJobDataFormat = u32;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8: AmsMelRfJobDataFormat = 0;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16: AmsMelRfJobDataFormat = 1;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8: AmsMelRfJobDataFormat = 2;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16: AmsMelRfJobDataFormat = 3;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL: AmsMelRfJobDataFormat = 4;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM: AmsMelRfJobDataFormat = 5;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE: AmsMelRfJobDataFormat = 6;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE: AmsMelRfJobDataFormat = 7;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1: AmsMelRfJobDataFormat = 8;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2: AmsMelRfJobDataFormat = 9;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3: AmsMelRfJobDataFormat = 10;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1: AmsMelRfJobDataFormat = 11;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2: AmsMelRfJobDataFormat = 12;
pub const AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3: AmsMelRfJobDataFormat = 13;
pub type AmsMelRfJobStatus = u32;
pub const AMS_MEL_RF_JOB_STATUS_NONE: AmsMelRfJobStatus = 0;
pub const AMS_MEL_RF_JOB_STATUS_IN_PROGRESS: AmsMelRfJobStatus = 1;
pub const AMS_MEL_RF_JOB_STATUS_COMPLETE: AmsMelRfJobStatus = 2;
pub const AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_ID: AmsMelRfJobStatus = 3;
pub const AMS_MEL_RF_JOB_STATUS_FAILED_INTERRUPTED: AmsMelRfJobStatus = 4;
pub const AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_STATE: AmsMelRfJobStatus = 5;
pub type AmsMelRfCancelError = u32;
pub const AMS_MEL_RF_CANCEL_ERROR_NONE: AmsMelRfCancelError = 0;
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfJobCancelResultV1 {
    pub cancelled: u32,
    pub error_code: AmsMelRfCancelError,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct AmsMelRfFrequencyRangeV1 {
    pub min_hz: f64,
    pub max_hz: f64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfFrequencyRangeSpanV1 {
    pub data: *const AmsMelRfFrequencyRangeV1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfFaceInfoV1 {
    pub face_id: u32,
    pub supports_receive: u32,
    pub supports_transmit: u32,
    pub requires_endpoint_association: u32,
    pub agc_processing_time_fs: i64,
    pub min_job_request_lead_time_fs: i64,
    pub max_job_request_lead_time_fs: i64,
    pub min_job_detail_lead_time_fs: i64,
    pub tx_rx_switching_time_fs: i64,
    pub rx_tx_switching_time_fs: i64,
    pub tx_tx_switching_time_fs: i64,
    pub rx_rx_switching_time_fs: i64,
    pub rx_frequency_ranges: AmsMelRfFrequencyRangeSpanV1,
    pub tx_frequency_ranges: AmsMelRfFrequencyRangeSpanV1,
    pub sample_frequency_ranges: AmsMelRfFrequencyRangeSpanV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfFaceInfoSpanV1 {
    pub data: *const AmsMelRfFaceInfoV1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfMfaInfoV1 {
    pub reported_num_faces: u64,
    pub contains_open_additions: u32,
    pub scheduler_resolution_fs: i64,
    pub max_user_defined_context_bytes: u64,
    pub supported_data_formats: AmsMelU32SpanV1,
    pub faces: AmsMelRfFaceInfoSpanV1,
}

/// Task 033D: opaque asynchronous createProductRxEndpoint completion.
#[repr(C)]
pub struct AmsMelRfProductRxRequest {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Task 033D: opaque claimed ProductRxEndpoint lifecycle.
#[repr(C)]
pub struct AmsMelRfProductRxEndpoint {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
/// Task 033D: opaque immutable copied callback product.
#[repr(C)]
pub struct AmsMelRfProductRxEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfProductRxConfigV1 {
    pub data_format: AmsMelRfJobDataFormat,
    pub region_size_bytes: u64,
    pub queue_capacity: usize,
    pub max_samples_per_event: usize,
}

/// The facade's own ComplexINT16 value (no MELComplex layout claim).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfComplexI16V1 {
    pub real: i16,
    pub imag: i16,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfComplexI16SpanV1 {
    pub data: *const AmsMelRfComplexI16V1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfProductRxMetadataV1 {
    pub mel_protocol_version_id: u32,
    pub va_definition_id: u32,
    pub va_instance_id: u32,
    pub job_details_id: u32,
    pub job_interval_id: u32,
    pub lf_type_id: u32,
    pub lf_instance_id: u32,
    pub phase_coherence_with_prior: u32,
    pub first_rx_event_start_s: i64,
    pub first_rx_event_start_fs: i64,
    pub rx_stream_ids: AmsMelU32SpanV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfProductRxEventV1 {
    pub endpoint_id: u64,
    pub data_format: AmsMelRfJobDataFormat,
    pub samples: AmsMelRfComplexI16SpanV1,
    pub metadata: AmsMelRfProductRxMetadataV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfProductRxInfoV1 {
    pub endpoint_id: u64,
    pub assigned_data_format: AmsMelRfJobDataFormat,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfProductRxRequestResultV1 {
    pub error_code: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AmsMelRfProductRxCountersV1 {
    pub callbacks_received: u64,
    pub products_queued: u64,
    pub products_dropped_queue_full: u64,
    pub malformed_or_unsupported: u64,
    pub allocation_failures: u64,
    pub callbacks_after_close: u64,
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

    pub fn ams_mel_session_open_with_options(
        library_path: *const c_char,
        instance: *const c_char,
        aperture_config_id: *const c_char,
        options: *const AmsMelSessionOptionsV1,
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
    pub fn ams_mel_ir_stream_get_capabilities(
        stream: *mut AmsMelIrStream,
        out: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_open(
        stream: *mut AmsMelIrStream,
        queue_capacity: usize,
        out: *mut *mut AmsMelIrImageMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_receive(
        metadata: *mut AmsMelIrImageMetadata,
        timeout_ms: u32,
        out: *mut *mut AmsMelIrImageMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_get_counters(
        metadata: *const AmsMelIrImageMetadata,
        out: *mut AmsMelIrC2MetadataCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_close(
        metadata: *mut *mut AmsMelIrImageMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_event_view(
        event: *const AmsMelIrImageMetadataEvent,
        out: *mut *const AmsMelIrImageMetadataEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_image_metadata_event_close(
        event: *mut *mut AmsMelIrImageMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_submit_navigation_report(
        stream: *mut AmsMelIrStream,
        report: *const AmsMelNavigationReportV1,
        out_request: *mut *mut AmsMelIrNavigationRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_navigation_request_wait(
        request: *const AmsMelIrNavigationRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrNavigationResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_navigation_request_close(
        request: *mut *mut AmsMelIrNavigationRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_stream_receive_snapshot(
        stream: *mut AmsMelIrStream,
        timeout_ms: u32,
        out_snapshot: *mut *mut AmsMelIrFrameSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_frame_snapshot_view(
        snapshot: *const AmsMelIrFrameSnapshot,
        out_view: *mut *const AmsMelIrFrameSnapshotV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_ir_frame_snapshot_close(
        snapshot: *mut *mut AmsMelIrFrameSnapshot,
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
    pub fn ams_mel_ir_c2_send_keepalive(
        c2: *mut AmsMelIrC2,
        out: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_submit_comms_test(
        c2: *mut AmsMelIrC2,
        request: *const AmsMelIrChannelCommsTestRequestV1,
        out: *mut *mut AmsMelIrChannelCommsRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_comms_request_wait(
        request: *const AmsMelIrChannelCommsRequest,
        timeout_ms: u32,
        out: *mut AmsMelIrChannelCommsTestResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_comms_request_close(
        request: *mut *mut AmsMelIrChannelCommsRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_get_capabilities(
        c2: *mut AmsMelIrC2,
        out: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_capability_view(
        capability: *const AmsMelIrChannelCapability,
        out: *mut *const AmsMelIrChannelCapabilityV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_capability_close(
        capability: *mut *mut AmsMelIrChannelCapability,
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
    pub fn ams_mel_ir_c2_metadata_open(
        c2: *mut AmsMelIrC2,
        queue_capacity: usize,
        out: *mut *mut AmsMelIrC2Metadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_receive(
        metadata: *mut AmsMelIrC2Metadata,
        timeout_ms: u32,
        out: *mut *mut AmsMelIrC2MetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_register_comms_test(
        metadata: *mut AmsMelIrC2Metadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_get_counters(
        metadata: *const AmsMelIrC2Metadata,
        out: *mut AmsMelIrC2MetadataCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_close(
        metadata: *mut *mut AmsMelIrC2Metadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_event_view(
        event: *const AmsMelIrC2MetadataEvent,
        out: *mut *const AmsMelIrC2MetadataEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_c2_metadata_event_close(
        event: *mut *mut AmsMelIrC2MetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_open(
        session: *const AmsMelSession,
        config: *const AmsMelIrHealthConfigV1,
        out: *mut *mut AmsMelIrHealth,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_enable(
        health: *mut AmsMelIrHealth,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_get_capabilities(
        health: *mut AmsMelIrHealth,
        out: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_close(
        health: *mut *mut AmsMelIrHealth,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_open(
        health: *mut AmsMelIrHealth,
        queue_capacity: usize,
        out: *mut *mut AmsMelIrHealthMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_receive(
        metadata: *mut AmsMelIrHealthMetadata,
        timeout_ms: u32,
        out: *mut *mut AmsMelIrHealthMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_get_counters(
        metadata: *const AmsMelIrHealthMetadata,
        out: *mut AmsMelIrC2MetadataCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_close(
        metadata: *mut *mut AmsMelIrHealthMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_event_view(
        event: *const AmsMelIrHealthMetadataEvent,
        out: *mut *const AmsMelIrHealthMetadataEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_health_metadata_event_close(
        event: *mut *mut AmsMelIrHealthMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_open(
        session: *const AmsMelSession,
        config: *const AmsMelIrInstrumentationConfigV1,
        out_instrumentation: *mut *mut AmsMelIrInstrumentation,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_enable(
        instrumentation: *mut AmsMelIrInstrumentation,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_get_capabilities(
        instrumentation: *mut AmsMelIrInstrumentation,
        out_capability: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_submit_level(
        instrumentation: *mut AmsMelIrInstrumentation,
        command: *const AmsMelIrInstrumentationLevelCommandV1,
        out_request: *mut *mut AmsMelIrInstrumentationRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_request_wait(
        request: *const AmsMelIrInstrumentationRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrInstrumentationResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_request_close(
        request: *mut *mut AmsMelIrInstrumentationRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_open(
        instrumentation: *mut AmsMelIrInstrumentation,
        queue_capacity: usize,
        out_metadata: *mut *mut AmsMelIrInstrumentationMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_receive(
        metadata: *mut AmsMelIrInstrumentationMetadata,
        timeout_ms: u32,
        out_event: *mut *mut AmsMelIrInstrumentationMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_get_counters(
        metadata: *const AmsMelIrInstrumentationMetadata,
        out_counters: *mut AmsMelIrC2MetadataCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_close(
        metadata: *mut *mut AmsMelIrInstrumentationMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_event_view(
        event: *const AmsMelIrInstrumentationMetadataEvent,
        out_view: *mut *const AmsMelIrInstrumentationMetadataEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_metadata_event_close(
        event: *mut *mut AmsMelIrInstrumentationMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_instrumentation_close(
        instrumentation: *mut *mut AmsMelIrInstrumentation,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_open(
        session: *const AmsMelSession,
        config: *const AmsMelIrTrackConfigV1,
        out_track: *mut *mut AmsMelIrTrack,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_enable(
        track: *mut AmsMelIrTrack,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_get_capabilities(
        track: *mut AmsMelIrTrack,
        out_capability: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_close(
        track: *mut *mut AmsMelIrTrack,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_open(
        track: *mut AmsMelIrTrack,
        queue_capacity: usize,
        out_metadata: *mut *mut AmsMelIrTrackMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_receive(
        metadata: *mut AmsMelIrTrackMetadata,
        timeout_ms: u32,
        out_event: *mut *mut AmsMelIrTrackMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_get_counters(
        metadata: *const AmsMelIrTrackMetadata,
        out_counters: *mut AmsMelIrC2MetadataCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_close(
        metadata: *mut *mut AmsMelIrTrackMetadata,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_event_view(
        event: *const AmsMelIrTrackMetadataEvent,
        out_view: *mut *const AmsMelIrTrackMetadataEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_event_view_v3(
        event: *const AmsMelIrTrackMetadataEvent,
        out_view: *mut *const AmsMelIrTrackMetadataEventV3,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_event_view_v2(
        event: *const AmsMelIrTrackMetadataEvent,
        out_view: *mut *const AmsMelIrTrackMetadataEventV2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_metadata_event_close(
        event: *mut *mut AmsMelIrTrackMetadataEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_submit_update(
        track: *mut AmsMelIrTrack,
        update: *const AmsMelIrTrackDataUpdateV1,
        out_request: *mut *mut AmsMelIrTrackUpdateRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_update_request_wait(
        request: *const AmsMelIrTrackUpdateRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrTrackUpdateResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_update_request_close(
        request: *mut *mut AmsMelIrTrackUpdateRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_submit_system_track_data_response(
        track: *mut AmsMelIrTrack,
        response: *const AmsMelIrSystemTrackDataResponseV1,
        out_request: *mut *mut AmsMelIrTrackSystemResponseRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_system_response_request_wait(
        request: *const AmsMelIrTrackSystemResponseRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelIrTrackSystemResponseResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_track_system_response_request_close(
        request: *mut *mut AmsMelIrTrackSystemResponseRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_from_c2(
        source: *const AmsMelIrC2,
        out_channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_from_stream(
        source: *const AmsMelIrStream,
        out_channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_from_health(
        source: *const AmsMelIrHealth,
        out_channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_from_instrumentation(
        source: *const AmsMelIrInstrumentation,
        out_channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_from_track(
        source: *const AmsMelIrTrack,
        out_channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_send_keepalive(
        channel: *const AmsMelIrChannel,
        out_request: *mut *mut AmsMelIrReturnRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_submit_comms_test(
        channel: *const AmsMelIrChannel,
        request: *const AmsMelIrChannelCommsTestRequestV1,
        out_request: *mut *mut AmsMelIrChannelCommsRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_get_capabilities(
        channel: *const AmsMelIrChannel,
        out_capability: *mut *mut AmsMelIrChannelCapability,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_ir_channel_close(
        channel: *mut *mut AmsMelIrChannel,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_rf_admin_open(
        library_path: *const c_char,
        configuration: *const c_char,
        out_admin: *mut *mut AmsMelRfAdmin,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_c2_open(
        library_path: *const c_char,
        configuration: *const c_char,
        out_c2: *mut *mut AmsMelRfC2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_c2_close(
        c2: *mut *mut AmsMelRfC2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_c2_submit_virtual_aperture(
        c2: *mut AmsMelRfC2,
        config: *const AmsMelRfVaConfigV1,
        out_request: *mut *mut AmsMelRfVaRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_request_wait(
        request: *const AmsMelRfVaRequest,
        timeout_ms: u32,
        result: *mut AmsMelRfVaResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_request_claim(
        request: *mut AmsMelRfVaRequest,
        out_va: *mut *mut AmsMelRfVa,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_request_close(
        request: *mut *mut AmsMelRfVaRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_view(
        va: *const AmsMelRfVa,
        out_info: *mut *const AmsMelRfVaInfoV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_close(
        va: *mut *mut AmsMelRfVa,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_id(
        va: *const AmsMelRfVa,
        out_id: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_status(
        va: *const AmsMelRfVa,
        out_status: *mut AmsMelRfVirtualApertureStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_instance_status(
        va: *const AmsMelRfVa,
        instance_id: u32,
        out_status: *mut AmsMelRfVirtualApertureStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_all_instances(
        va: *const AmsMelRfVa,
        out_list: *mut *mut AmsMelRfVaInstanceList,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_instances(
        va: *const AmsMelRfVa,
        face_id: u32,
        out_list: *mut *mut AmsMelRfVaInstanceList,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_instance_list_view(
        list: *const AmsMelRfVaInstanceList,
        out_view: *mut AmsMelU32SpanV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_instance_list_close(
        list: *mut *mut AmsMelRfVaInstanceList,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_get_instance_status_report(
        va: *const AmsMelRfVa,
        instance_id: u32,
        out_report: *mut *mut AmsMelRfVaInstanceStatusReport,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_instance_status_report_view(
        report: *const AmsMelRfVaInstanceStatusReport,
        out_view: *mut *const AmsMelRfVaInstanceStatusReportV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_va_instance_status_report_close(
        report: *mut *mut AmsMelRfVaInstanceStatusReport,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_submit_job(
        va: *mut AmsMelRfVa,
        config: *const AmsMelRfJobRequestConfigV1,
        out_request: *mut *mut AmsMelRfJobRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_submit_job_v2(
        va: *mut AmsMelRfVa,
        config: *const AmsMelRfJobRequestConfigV2,
        out_request: *mut *mut AmsMelRfJobRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_submit_job_v3(
        va: *mut AmsMelRfVa,
        config: *const AmsMelRfJobRequestConfigV3,
        out_request: *mut *mut AmsMelRfJobRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_virtual_aperture_submit_job_v4(
        va: *mut AmsMelRfVa,
        config: *const AmsMelRfJobRequestConfigV4,
        out_request: *mut *mut AmsMelRfJobRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_request_wait(
        request: *const AmsMelRfJobRequest,
        timeout_ms: u32,
        result: *mut AmsMelRfJobResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_request_claim(
        request: *mut AmsMelRfJobRequest,
        out_job: *mut *mut AmsMelRfJob,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_request_close(
        request: *mut *mut AmsMelRfJobRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_view(
        job: *const AmsMelRfJob,
        out_info: *mut *const AmsMelRfJobInfoV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_add_rx_intervals(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_flush(
        job: *mut AmsMelRfJob,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_cancel_remaining_intervals(
        job: *mut AmsMelRfJob,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_finalize(
        job: *mut AmsMelRfJob,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_extend_event(
        job: *mut AmsMelRfJob,
        interval_id: u32,
        event_id: u32,
        added_duration_femtoseconds: i64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_wait_status(
        job: *const AmsMelRfJob,
        timeout_ms: u32,
        out_status: *mut AmsMelRfJobStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_cancel(
        job: *mut AmsMelRfJob,
        out_result: *mut AmsMelRfJobCancelResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_close(
        job: *mut *mut AmsMelRfJob,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_admin_command_state(
        admin: *mut AmsMelRfAdmin,
        state: AmsMelRfMfaState,
        accepted: *mut u32,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_admin_close(
        admin: *mut *mut AmsMelRfAdmin,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_rf_data_open(
        library_path: *const c_char,
        configuration: *const c_char,
        out_data: *mut *mut AmsMelRfData,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_get_provider_version(
        data: *const AmsMelRfData,
        out_version: *mut AmsMelProviderVersionV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_get_tx_power_modes(
        data: *const AmsMelRfData,
        face_id: u32,
        out_snapshot: *mut *mut AmsMelRfTxPowerModeSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_get_tx_power_mode(
        data: *const AmsMelRfData,
        face_id: u32,
        tx_power_mode_id: u32,
        out_snapshot: *mut *mut AmsMelRfTxPowerModeSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_tx_power_mode_snapshot_view(
        snapshot: *const AmsMelRfTxPowerModeSnapshot,
        out_view: *mut AmsMelRfTxPowerModeSpanV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_tx_power_mode_snapshot_close(
        snapshot: *mut *mut AmsMelRfTxPowerModeSnapshot,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_get_physical_data(
        data: *const AmsMelRfData,
        face_id: u32,
        out_physical: *mut *mut AmsMelRfPhysicalData,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_physical_data_view(
        physical: *const AmsMelRfPhysicalData,
        out_view: *mut *const AmsMelRfPhysicalDataV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_physical_data_close(
        physical: *mut *mut AmsMelRfPhysicalData,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_get_mfa_info(
        data: *const AmsMelRfData,
        out_info: *mut *mut AmsMelRfMfaInfo,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_quantize_duration(
        data: *const AmsMelRfData,
        unquantized_femtoseconds: i64,
        out_quantized_femtoseconds: *mut i64,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_mfa_info_view(
        info: *const AmsMelRfMfaInfo,
        out_view: *mut *const AmsMelRfMfaInfoV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_mfa_info_close(
        info: *mut *mut AmsMelRfMfaInfo,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_data_close(
        data: *mut *mut AmsMelRfData,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;

    pub fn ams_mel_rf_data_submit_product_rx(
        data: *mut AmsMelRfData,
        config: *const AmsMelRfProductRxConfigV1,
        out_request: *mut *mut AmsMelRfProductRxRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_request_wait(
        request: *const AmsMelRfProductRxRequest,
        timeout_ms: u32,
        out_result: *mut AmsMelRfProductRxRequestResultV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_request_claim(
        request: *mut AmsMelRfProductRxRequest,
        out_endpoint: *mut *mut AmsMelRfProductRxEndpoint,
        out_info: *mut AmsMelRfProductRxInfoV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_request_close(
        request: *mut *mut AmsMelRfProductRxRequest,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_receive(
        endpoint: *mut AmsMelRfProductRxEndpoint,
        timeout_ms: u32,
        out_event: *mut *mut AmsMelRfProductRxEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_get_counters(
        endpoint: *const AmsMelRfProductRxEndpoint,
        out_counters: *mut AmsMelRfProductRxCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_close(
        endpoint: *mut *mut AmsMelRfProductRxEndpoint,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_event_view(
        event: *const AmsMelRfProductRxEvent,
        out_view: *mut *const AmsMelRfProductRxEventV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_product_rx_event_close(
        event: *mut *mut AmsMelRfProductRxEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

// Raw interval-status ABI only; no safe Rust callback API.
#[repr(C)]
pub struct AmsMelRfJobIntervalStatus {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
#[repr(C)]
pub struct AmsMelRfJobIntervalStatusEvent {
    _private: [u8; 0],
    _not_send_sync: std::marker::PhantomData<*mut c_void>,
}
pub const AMS_MEL_RF_INTERVAL_STATUS_NEVER: u32 = 0;
pub const AMS_MEL_RF_INTERVAL_STATUS_ALWAYS: u32 = 1;
pub const AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION: u32 = 2;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_NONE: u32 = 0;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL: u32 = 1;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED: u32 = 2;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA: u32 = 3;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA: u32 = 4;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA: u32 = 5;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA: u32 = 6;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA: u32 = 7;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA: u32 = 8;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA: u32 = 9;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA: u32 = 10;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA: u32 = 11;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA: u32 = 12;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA: u32 = 13;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA: u32 = 14;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA: u32 =
    15;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA: u32 = 16;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA: u32 = 17;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED: u32 = 18;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED: u32 = 19;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS: u32 = 20;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS: u32 = 21;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR: u32 = 22;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED: u32 = 23;
pub const AMS_MEL_RF_INTERVAL_COMPLETION_STARTED: u32 = 24;
pub const AMS_MEL_RF_LOG_TRIGGER_NONE: u32 = 0;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED: u32 = 1;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED: u32 = 2;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED: u32 = 3;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED: u32 = 4;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED: u32 = 5;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START: u32 = 6;
pub const AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED: u32 = 7;
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV2 {
    pub interval: AmsMelRfJobIntervalConfigV1,
    pub status_enable: u32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV2 {
    pub data: *const AmsMelRfJobIntervalConfigV2,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobEventLogEntryV1 {
    pub event_id: u32,
    pub trigger: u32,
    pub time_seconds: i64,
    pub time_fractional_femtoseconds: i64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobEventLogSpanV1 {
    pub data: *const AmsMelRfJobEventLogEntryV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalStatusV1 {
    pub interval_id: u32,
    pub completion_status: u32,
    pub event_log: AmsMelRfJobEventLogSpanV1,
    pub activity_id: AmsMelU8SpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalStatusOptionsV1 {
    pub queue_capacity: usize,
    pub max_event_log_entries: usize,
    pub max_activity_id_bytes: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalStatusCountersV1 {
    pub callback_entries: u64,
    pub events_queued: u64,
    pub events_delivered: u64,
    pub queue_full_drops: u64,
    pub malformed_drops: u64,
    pub oversize_drops: u64,
    pub allocation_failures: u64,
    pub callbacks_after_close: u64,
}
unsafe extern "C" {
    pub fn ams_mel_rf_job_interval_status_open(
        job: *mut AmsMelRfJob,
        options: *const AmsMelRfJobIntervalStatusOptionsV1,
        out_stream: *mut *mut AmsMelRfJobIntervalStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_interval_status_receive(
        stream: *mut AmsMelRfJobIntervalStatus,
        timeout_ms: u32,
        out_event: *mut *mut AmsMelRfJobIntervalStatusEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_interval_status_get_counters(
        stream: *const AmsMelRfJobIntervalStatus,
        out_counters: *mut AmsMelRfJobIntervalStatusCountersV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_interval_status_close(
        stream: *mut *mut AmsMelRfJobIntervalStatus,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_interval_status_event_view(
        event: *const AmsMelRfJobIntervalStatusEvent,
        out_view: *mut *const AmsMelRfJobIntervalStatusV1,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_interval_status_event_close(
        event: *mut *mut AmsMelRfJobIntervalStatusEvent,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
    pub fn ams_mel_rf_job_add_rx_intervals_v2(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV2,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigV2 {
    pub event: AmsMelRfReceiveEventConfigV1,
    pub stab_point_index: u64,
    pub applicable_rx_element_groups: AmsMelU64SpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigSpanV2 {
    pub data: *const AmsMelRfReceiveEventConfigV2,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV3 {
    pub interval_start_femtoseconds: i64,
    pub interval_id: u32,
    pub interval_starting_gap_femtoseconds: i64,
    pub sequence_duration_femtoseconds: i64,
    pub sequence_repeat_count: u64,
    pub calibration_duration_femtoseconds: i64,
    pub interval_ending_gap_femtoseconds: i64,
    pub phase_coherence_with_prior: u32,
    pub iterations_per_signal: u64,
    pub max_data_rate_bps: f64,
    pub max_sample_rate_hz: f64,
    pub job_details_id: u32,
    pub status_enable: u32,
    pub stab_points: AmsMelRfPointingSpanV1,
    pub receive_events: AmsMelRfReceiveEventConfigSpanV2,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV3 {
    pub data: *const AmsMelRfJobIntervalConfigV3,
    pub size: usize,
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v3(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV3,
        diagnostic: *mut std::ffi::c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

pub type AmsMelRfExecutionTypeT = u32;
pub type AmsMelRfEventTerminationTypeT = u32;
pub const AMS_MEL_RF_EXECUTION_NORMAL: AmsMelRfExecutionTypeT = 0;
pub const AMS_MEL_RF_EXECUTION_CONDITIONAL: AmsMelRfExecutionTypeT = 1;
pub const AMS_MEL_RF_EVENT_TERMINATION_INHIBIT: AmsMelRfEventTerminationTypeT = 0;
pub const AMS_MEL_RF_EVENT_TERMINATION_CANCEL: AmsMelRfEventTerminationTypeT = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfStokesVectorV1 {
    pub s0: f64,
    pub s1: f64,
    pub s2: f64,
    pub s3: f64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfStokesVectorSpanV1 {
    pub data: *const AmsMelRfStokesVectorV1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigV3 {
    pub event: AmsMelRfReceiveEventConfigV2,
    pub polarization: AmsMelRfStokesVectorSpanV1,
    pub polarization_beam_steer_correction: u32,
    pub phase_offset_rad: f64,
    pub execution_type: AmsMelRfExecutionTypeT,
    pub termination_type: AmsMelRfEventTerminationTypeT,
    pub allow_delay_start: u32,
    pub iteration_hold_count: u64,
    pub iteration_termination_count: u64,
    pub channelization_enabled: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigSpanV3 {
    pub data: *const AmsMelRfReceiveEventConfigV3,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV4 {
    pub interval_start_femtoseconds: i64,
    pub interval_id: u32,
    pub interval_starting_gap_femtoseconds: i64,
    pub sequence_duration_femtoseconds: i64,
    pub sequence_repeat_count: u64,
    pub calibration_duration_femtoseconds: i64,
    pub interval_ending_gap_femtoseconds: i64,
    pub phase_coherence_with_prior: u32,
    pub iterations_per_signal: u64,
    pub max_data_rate_bps: f64,
    pub max_sample_rate_hz: f64,
    pub job_details_id: u32,
    pub status_enable: u32,
    pub stab_points: AmsMelRfPointingSpanV1,
    pub receive_events: AmsMelRfReceiveEventConfigSpanV3,
    pub tx_power_mode_id: u32,
    pub activity_id: AmsMelU8SpanV1,
    pub execution_type: AmsMelRfExecutionTypeT,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV4 {
    pub data: *const AmsMelRfJobIntervalConfigV4,
    pub size: usize,
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v4(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV4,
        diagnostic: *mut std::ffi::c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

pub type AmsMelRfPulseThresholdReferenceT = u32;
pub const AMS_MEL_RF_PD_REFERENCE_DBQ: u32 = 0;
pub const AMS_MEL_RF_PD_REFERENCE_DB_ABOVE_NOISE: u32 = 1;
pub const AMS_MEL_RF_PD_REFERENCE_DB_BELOW_SATURATION: u32 = 2;
pub type AmsMelRfPulseTimetagThresholdT = u32;
pub const AMS_MEL_RF_PD_TIMETAG_50_PERCENT: u32 = 0;
pub const AMS_MEL_RF_PD_TIMETAG_90_PERCENT: u32 = 1;
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPulseMOfNV1 {
    pub m: u8,
    pub n: u8,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPulseThresholdV1 {
    pub leading_edge_db: f64,
    pub trailing_edge_db: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPulseThresholdSpanV1 {
    pub data: *const AmsMelRfPulseThresholdV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfPulseDetectionSettingsV1 {
    pub reference: AmsMelRfPulseThresholdReferenceT,
    pub leading_edge_m_of_n: AmsMelRfPulseMOfNV1,
    pub trailing_edge_m_of_n: AmsMelRfPulseMOfNV1,
    pub min_pulse_width_femtoseconds: i64,
    pub timetag_amplitude_threshold: AmsMelRfPulseTimetagThresholdT,
    pub thresholds: AmsMelRfPulseThresholdSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigV4 {
    pub event: AmsMelRfReceiveEventConfigV3,
    pub has_pulse_detection_settings: u32,
    pub pulse_detection_settings: AmsMelRfPulseDetectionSettingsV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfReceiveEventConfigSpanV4 {
    pub data: *const AmsMelRfReceiveEventConfigV4,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV5 {
    pub interval_start_femtoseconds: i64,
    pub interval_id: u32,
    pub interval_starting_gap_femtoseconds: i64,
    pub sequence_duration_femtoseconds: i64,
    pub sequence_repeat_count: u64,
    pub calibration_duration_femtoseconds: i64,
    pub interval_ending_gap_femtoseconds: i64,
    pub phase_coherence_with_prior: u32,
    pub iterations_per_signal: u64,
    pub max_data_rate_bps: f64,
    pub max_sample_rate_hz: f64,
    pub job_details_id: u32,
    pub status_enable: u32,
    pub stab_points: AmsMelRfPointingSpanV1,
    pub receive_events: AmsMelRfReceiveEventConfigSpanV4,
    pub tx_power_mode_id: u32,
    pub activity_id: AmsMelU8SpanV1,
    pub execution_type: AmsMelRfExecutionTypeT,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV5 {
    pub data: *const AmsMelRfJobIntervalConfigV5,
    pub size: usize,
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v5(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV5,
        diagnostic: *mut std::ffi::c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfLfAddressValueV1 {
    pub address: u64,
    pub value: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfLfAddressValueSpanV1 {
    pub data: *const AmsMelRfLfAddressValueV1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfLfCommandV1 {
    pub local_function_type_id: u32,
    pub local_function_instance: u64,
    pub address_values: AmsMelRfLfAddressValueSpanV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfLfCommandSpanV1 {
    pub data: *const AmsMelRfLfCommandV1,
    pub size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV6 {
    pub interval: AmsMelRfJobIntervalConfigV5,
    pub local_function_commands: AmsMelRfLfCommandSpanV1,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV6 {
    pub data: *const AmsMelRfJobIntervalConfigV6,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfProductStreamEndpointV1 {
    pub endpoint_id: u64,
    pub start_address: u64,
    pub max_bytes: u64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfProductStreamEndpointSpanV1 {
    pub data: *const AmsMelRfProductStreamEndpointV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfProductStreamParamsV1 {
    pub applicable_rx_element_groups: AmsMelU64SpanV1,
    pub endpoints: AmsMelRfProductStreamEndpointSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV7 {
    pub interval: AmsMelRfJobIntervalConfigV6,
    pub has_product_stream_params: u32,
    pub product_stream_params: AmsMelRfProductStreamParamsV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV7 {
    pub data: *const AmsMelRfJobIntervalConfigV7,
    pub size: usize,
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v7(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV7,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v6(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV6,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfIntervalEndpointConnectionV1 {
    pub element_group_label: AmsMelStringViewV1,
    pub data_pipe_label: AmsMelStringViewV1,
    pub endpoint_id: u64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfIntervalEndpointConnectionSpanV1 {
    pub data: *const AmsMelRfIntervalEndpointConnectionV1,
    pub size: usize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigV8 {
    pub interval: AmsMelRfJobIntervalConfigV7,
    pub has_endpoint_connections: u32,
    pub endpoint_connections: AmsMelRfIntervalEndpointConnectionSpanV1,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AmsMelRfJobIntervalConfigSpanV8 {
    pub data: *const AmsMelRfJobIntervalConfigV8,
    pub size: usize,
}
extern "C" {
    pub fn ams_mel_rf_job_add_rx_intervals_v8(
        job: *mut AmsMelRfJob,
        intervals: AmsMelRfJobIntervalConfigSpanV8,
        diagnostic: *mut c_char,
        diagnostic_capacity: usize,
        diagnostic_required: *mut usize,
    ) -> AmsMelStatus;
}
