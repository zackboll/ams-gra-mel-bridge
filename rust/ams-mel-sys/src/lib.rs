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
/// The Track metadata event kinds defined by this release. The
/// `CandidateObjectPreProcMessage` callback remains unimplemented and has no
/// kind.
pub const AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT: u32 = 1;
pub const AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA: u32 = 2;
pub const AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE: u32 = 3;

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
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AmsMelIrTrackMetadataEventV1 {
    pub kind: u32,
    pub track_report: AmsMelIrTrackReportV1,
    pub request_system_track_data: AmsMelIrRequestSystemTrackDataV1,
    pub candidate_object_message: AmsMelIrCandidateObjectMessageV1,
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
}
