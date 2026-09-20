use std::env;
use std::mem::{align_of, offset_of, size_of};
use std::path::{Path, PathBuf};
use std::process::Command;

use ams_mel_sys::*;

macro_rules! layout {
    ($values:expr, $type:ty, $($field:ident),+ $(,)?) => {{
        $values.push(size_of::<$type>());
        $values.push(align_of::<$type>());
        $($values.push(offset_of!($type, $field));)+
    }};
}

#[test]
fn session_input_function_signatures_match_the_c_header() {
    let _: unsafe extern "C" fn(
        *const AmsMelSession,
        *mut AmsMelProviderVersionV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_session_get_provider_version;
    let _: unsafe extern "C" fn(
        *const AmsMelSession,
        *const AmsMelIrHealthConfigV1,
        *mut *mut AmsMelIrHealth,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_health_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        u32,
        *mut *mut AmsMelIrFrameSnapshot,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stream_receive_snapshot;
    let _: unsafe extern "C" fn(
        *const AmsMelIrFrameSnapshot,
        *mut *const AmsMelIrFrameSnapshotV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_frame_snapshot_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrFrameSnapshot,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_frame_snapshot_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        *mut *mut AmsMelIrChannelCapability,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stream_get_capabilities;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        usize,
        *mut *mut AmsMelIrImageMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrImageMetadata,
        u32,
        *mut *mut AmsMelIrImageMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrImageMetadataEvent,
        *mut *const AmsMelIrImageMetadataEventV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrImageMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_image_metadata_event_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrStream,
        *const AmsMelNavigationReportV1,
        *mut *mut AmsMelIrNavigationRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_stream_submit_navigation_report;
    let _: unsafe extern "C" fn(
        *const AmsMelIrNavigationRequest,
        u32,
        *mut AmsMelIrNavigationResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_navigation_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrNavigationRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_navigation_request_close;
    let _: unsafe extern "C" fn(
        *const AmsMelSession,
        *const AmsMelIrInstrumentationConfigV1,
        *mut *mut AmsMelIrInstrumentation,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrInstrumentation,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_enable;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrInstrumentation,
        *mut *mut AmsMelIrChannelCapability,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_get_capabilities;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrInstrumentation,
        *const AmsMelIrInstrumentationLevelCommandV1,
        *mut *mut AmsMelIrInstrumentationRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_submit_level;
    let _: unsafe extern "C" fn(
        *const AmsMelIrInstrumentationRequest,
        u32,
        *mut AmsMelIrInstrumentationResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrInstrumentationRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_request_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrInstrumentation,
        usize,
        *mut *mut AmsMelIrInstrumentationMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrInstrumentationMetadata,
        u32,
        *mut *mut AmsMelIrInstrumentationMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrInstrumentationMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrInstrumentationMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrInstrumentationMetadataEvent,
        *mut *const AmsMelIrInstrumentationMetadataEventV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrInstrumentationMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_metadata_event_close;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrInstrumentation,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_instrumentation_close;
    let _: unsafe extern "C" fn(
        *const AmsMelSession,
        *const AmsMelIrTrackConfigV1,
        *mut *mut AmsMelIrTrack,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrack,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_enable;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrack,
        *mut *mut AmsMelIrChannelCapability,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_get_capabilities;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrTrack,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrack,
        usize,
        *mut *mut AmsMelIrTrackMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_open;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrackMetadata,
        u32,
        *mut *mut AmsMelIrTrackMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_receive;
    let _: unsafe extern "C" fn(
        *const AmsMelIrTrackMetadata,
        *mut AmsMelIrC2MetadataCountersV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_get_counters;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrTrackMetadata,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_close;
    let _: unsafe extern "C" fn(
        *const AmsMelIrTrackMetadataEvent,
        *mut *const AmsMelIrTrackMetadataEventV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_event_view;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrTrackMetadataEvent,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_metadata_event_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrack,
        *const AmsMelIrTrackDataUpdateV1,
        *mut *mut AmsMelIrTrackUpdateRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_submit_update;
    let _: unsafe extern "C" fn(
        *const AmsMelIrTrackUpdateRequest,
        u32,
        *mut AmsMelIrTrackUpdateResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_update_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrTrackUpdateRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_update_request_close;
    let _: unsafe extern "C" fn(
        *mut AmsMelIrTrack,
        *const AmsMelIrSystemTrackDataResponseV1,
        *mut *mut AmsMelIrTrackSystemResponseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_submit_system_track_data_response;
    let _: unsafe extern "C" fn(
        *const AmsMelIrTrackSystemResponseRequest,
        u32,
        *mut AmsMelIrTrackSystemResponseResultV1,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_system_response_request_wait;
    let _: unsafe extern "C" fn(
        *mut *mut AmsMelIrTrackSystemResponseRequest,
        *mut std::ffi::c_char,
        usize,
        *mut usize,
    ) -> AmsMelStatus = ams_mel_ir_track_system_response_request_close;
}

#[test]
fn declarations_match_the_c_header() {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repository = manifest.join("../..");
    let output = env::temp_dir().join(format!("ams-mel-abi-probe-{}", std::process::id()));
    let library_dir = env::var_os("AMS_MEL_NATIVE_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repository.join("native/build/lib"));
    let library_dir = absolute(&library_dir);
    let compile = Command::new(env::var_os("CC").unwrap_or_else(|| "cc".into()))
        .arg("-std=c11")
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-Wpedantic")
        .arg("-Werror")
        .arg(format!("-I{}", repository.join("native/include").display()))
        .arg(manifest.join("tests/abi_probe.c"))
        .arg(format!("-L{}", library_dir.display()))
        .arg("-lams_mel_c")
        .arg(format!("-Wl,-rpath,{}", library_dir.display()))
        .arg("-o")
        .arg(&output)
        .output()
        .expect("C compiler must run");
    assert!(
        compile.status.success(),
        "{}",
        String::from_utf8_lossy(&compile.stderr)
    );
    let probe = Command::new(&output)
        .output()
        .expect("C ABI probe must run");
    let _ = std::fs::remove_file(&output);
    assert!(
        probe.status.success(),
        "{}",
        String::from_utf8_lossy(&probe.stderr)
    );
    let actual: Vec<usize> = String::from_utf8(probe.stdout)
        .expect("probe output is UTF-8")
        .split_whitespace()
        .map(|value| value.parse().expect("numeric probe value"))
        .collect();

    let mut expected = vec![
        AMS_MEL_OK as usize,
        AMS_MEL_INVALID_ARGUMENT as usize,
        AMS_MEL_LIBRARY_LOAD_FAILED as usize,
        AMS_MEL_SYMBOL_NOT_FOUND as usize,
        AMS_MEL_FACTORY_FAILED as usize,
        AMS_MEL_INITIALIZATION_FAILED as usize,
        AMS_MEL_PROVIDER_EXCEPTION as usize,
        AMS_MEL_BUFFER_TOO_SMALL as usize,
        AMS_MEL_INTERNAL_ERROR as usize,
        AMS_MEL_TIMEOUT as usize,
        AMS_MEL_STREAM_STOPPED as usize,
        AMS_MEL_PROVIDER_FAILED as usize,
        AMS_MEL_COMMAND_REJECTED as usize,
        AMS_MEL_IR_CHANNEL_IRST_IMAGE as usize,
        AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL as usize,
        AMS_MEL_IR_MFA_MODE_UNUSED as usize,
        AMS_MEL_IR_MFA_MODE_TASK_SCHED as usize,
        AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED as usize,
        AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED as usize,
        AMS_MEL_IR_MFA_STATE_NOT_SET as usize,
        AMS_MEL_IR_MFA_STATE_UNKNOWN as usize,
        AMS_MEL_IR_MFA_STATE_NOT_INSTALLED as usize,
        AMS_MEL_IR_MFA_STATE_OFF as usize,
        AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION as usize,
        AMS_MEL_IR_MFA_STATE_INITIALIZATION as usize,
        AMS_MEL_IR_MFA_STATE_STANDBY as usize,
        AMS_MEL_IR_MFA_STATE_OPERATE as usize,
        AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY as usize,
        AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY as usize,
        AMS_MEL_IR_MFA_STATE_MAINTENANCE as usize,
        AMS_MEL_IR_MFA_STATE_CALIBRATION as usize,
        AMS_MEL_IR_MFA_STATE_INITIATED_BIT as usize,
        AMS_MEL_IR_MFA_STATE_SHUTDOWN as usize,
        AMS_MEL_IR_MFA_STATE_DEGRADED as usize,
        AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE as usize,
        AMS_MEL_IR_COORD_FRAME_INERTIAL as usize,
        AMS_MEL_IR_COORD_FRAME_AIRCRAFT as usize,
        AMS_MEL_IR_DEGRADATION_CAPACITY as usize,
        AMS_MEL_IR_DEGRADATION_VOLUME as usize,
        AMS_MEL_IR_DEGRADATION_RANGE as usize,
        AMS_MEL_IR_DEGRADATION_REVISIT as usize,
        AMS_MEL_IR_RETURN_SUCCESS as usize,
        AMS_MEL_IR_RETURN_BAD_POINTER as usize,
        AMS_MEL_IR_RETURN_FAIL as usize,
        AMS_MEL_IR_RETURN_NOT_SUPPORTED as usize,
        AMS_MEL_IR_RETURN_NOT_IMPLEMENTED as usize,
        AMS_MEL_ERROR_NONE as usize,
        AMS_MEL_ERROR_INVALID_ID as usize,
        AMS_MEL_ERROR_INVALID_STATE as usize,
        AMS_MEL_ERROR_INVALID_PARAMETERS as usize,
        AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS as usize,
        AMS_MEL_ERROR_INSUFFICIENT_RESOURCES as usize,
        AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES as usize,
        AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES as usize,
        AMS_MEL_ERROR_UNSUPPORTED as usize,
        AMS_MEL_IR_PIXEL_MONO as usize,
        AMS_MEL_IR_IMAGE_STARING as usize,
        AMS_MEL_IR_IMAGE_SCANNING as usize,
        AMS_MEL_IR_FLIP_NONE as usize,
        AMS_MEL_IR_FLIP_VERTICAL as usize,
        AMS_MEL_IR_FLIP_HORIZONTAL as usize,
        AMS_MEL_IR_FLIP_BOTH as usize,
    ];
    layout!(expected, AmsMelAbiVersionV1, major, minor);
    layout!(
        expected,
        AmsMelProviderVersionV1,
        api_version,
        library_version,
        vendor,
        vendor_capacity,
        vendor_required,
        description,
        description_capacity,
        description_required
    );
    layout!(expected, AmsMelStringViewV1, data, size);
    layout!(expected, AmsMelU32SpanV1, data, size);
    layout!(expected, AmsMelStringViewSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrScanTypeV1,
        continuous_scan,
        returning,
        agile_scan
    );
    layout!(
        expected,
        AmsMelIrScanParamV1,
        elevation_defined_with_range_and_altitude,
        center_az_rad,
        center_el_rad,
        center_frame_ref_el,
        center_frame_ref_az,
        scan_width_rad,
        scan_height_rad,
        scan_type,
        scan_id,
        scan_rate_rad_per_second,
        preferred_revisit_interval_seconds,
        required_revisit_interval_seconds,
        max_range_of_interest_m,
        min_range_of_interest_m,
        elevation_scan_center_altitude_m,
        elevation_scan_center_range_m,
        degradation_method
    );
    layout!(
        expected,
        AmsMelIrModeCommandV1,
        command_id,
        state,
        mode,
        scan_parameters
    );
    layout!(
        expected,
        AmsMelIrBitCommandV1,
        command_id,
        initiate_bit_ids,
        cancel_bit_ids,
        clear_fault_codes
    );
    layout!(
        expected,
        AmsMelIrConfigSetCommandV1,
        command_id,
        system_time_ns,
        config
    );
    layout!(expected, AmsMelUciIdV1, uuid, descriptive_label);
    layout!(
        expected,
        AmsMelComponentLocationV1,
        offset_x_m,
        offset_y_m,
        offset_z_m,
        key,
        system_name
    );
    layout!(
        expected,
        AmsMelIrStreamConfigV1,
        channel_type,
        channel_id,
        platform_id,
        sensor_location,
        buffer_count,
        buffer_size,
        queue_capacity
    );
    layout!(
        expected,
        AmsMelIrC2ConfigV1,
        channel_type,
        channel_id,
        platform_id,
        sensor_location
    );
    layout!(expected, AmsMelIrModeResultV1, mode, error_code);
    layout!(expected, AmsMelIrReturnResultV1, value, error_code);
    layout!(
        expected,
        AmsMelIrFrameV1,
        system_time_ns,
        integration_time_ns,
        width,
        height,
        bits_per_pixel,
        number_of_bands,
        horizontal_fov_rad,
        vertical_fov_rad,
        pixel_format,
        frame_id,
        subframe_id,
        subframe_total,
        image_type,
        image_flip,
        image_flags,
        dither_row,
        dither_column,
        row_offset,
        column_offset,
        band_index,
        reserved,
        pixels,
        pixel_capacity,
        pixel_required
    );
    layout!(
        expected,
        AmsMelIrStreamCountersV1,
        frames_received,
        frames_dropped_queue_full,
        malformed_or_unsupported_frames
    );
    expected.extend([
        AMS_MEL_IR_C2_METADATA_COMMAND_STATUS as usize,
        AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION as usize,
        AMS_MEL_IR_C2_METADATA_BIT_STATUS as usize,
        AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST as usize,
        AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT as usize,
        AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER as usize,
        AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE as usize,
        AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN as usize,
        AMS_MEL_IR_COMMAND_NOT_SET as usize,
        AMS_MEL_IR_COMMAND_RECEIVED as usize,
        AMS_MEL_IR_COMMAND_ACCEPTED as usize,
        AMS_MEL_IR_COMMAND_REJECTED as usize,
        AMS_MEL_IR_COMMAND_CANCELLED as usize,
        AMS_MEL_IR_CANNOT_COMPLY_NOT_SET as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS as usize,
        AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES as usize,
        AMS_MEL_IR_CANNOT_COMPLY_RANKING as usize,
        AMS_MEL_IR_CANNOT_COMPLY_WEATHER as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR as usize,
        AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING as usize,
        AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET as usize,
        AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER as usize,
        AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER as usize,
        AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR as usize,
        AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE as usize,
        AMS_MEL_IR_CANNOT_COMPLY_CANCELLED as usize,
        AMS_MEL_IR_CANNOT_COMPLY_OTHER as usize,
        AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN as usize,
        AMS_MEL_IR_CANNOT_COMPLY_ABORTED as usize,
        AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER as usize,
        AMS_MEL_BIT_CONTROL_NOT_SET as usize,
        AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND as usize,
        AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND as usize,
        AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED as usize,
        AMS_MEL_BIT_RESULT_NOT_SET as usize,
        AMS_MEL_BIT_RESULT_PASS as usize,
        AMS_MEL_BIT_RESULT_FAIL as usize,
        AMS_MEL_BIT_RESULT_INTERRUPTED as usize,
        AMS_MEL_BIT_RESULT_NOT_TESTED as usize,
        AMS_MEL_FAULT_SEVERITY_NOT_SET as usize,
        AMS_MEL_FAULT_SEVERITY_NOMINAL as usize,
        AMS_MEL_FAULT_SEVERITY_CAUTION as usize,
        AMS_MEL_FAULT_SEVERITY_WARNING as usize,
        AMS_MEL_FAULT_SEVERITY_FAILED as usize,
        AMS_MEL_FAULT_STATE_NOT_SET as usize,
        AMS_MEL_FAULT_STATE_SET as usize,
        AMS_MEL_FAULT_STATE_CLEARED as usize,
        AMS_MEL_FAULT_STATE_UNKNOWN as usize,
    ]);
    layout!(expected, AmsMelUciIdSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrCommandStatusV1,
        command_id,
        state,
        reason_id,
        reason_description
    );
    layout!(
        expected,
        AmsMelBitTypeV1,
        bit_id,
        accepted_interface,
        bit_item_names,
        subsystem_component_ids,
        expected_duration_ns
    );
    layout!(expected, AmsMelBitTypeSpanV1, data, size);
    layout!(expected, AmsMelBitConfigurationV1, bit_types);
    layout!(
        expected,
        AmsMelActiveBitV1,
        bit_id,
        estimated_completion_time_ns,
        estimated_percent_complete
    );
    layout!(expected, AmsMelActiveBitSpanV1, data, size);
    layout!(
        expected,
        AmsMelCompletedBitItemV1,
        bit_item_name,
        result,
        fail_reason
    );
    layout!(expected, AmsMelCompletedBitItemSpanV1, data, size);
    layout!(
        expected,
        AmsMelCompletedBitV1,
        bit_id,
        time_tag_ns,
        result,
        fail_reason,
        bit_items
    );
    layout!(expected, AmsMelCompletedBitSpanV1, data, size);
    layout!(expected, AmsMelFaultDataV1, key, value, format, units);
    layout!(expected, AmsMelFaultDataSpanV1, data, size);
    layout!(
        expected,
        AmsMelFaultAmbiguityGroupV1,
        diagnostic_test_ids,
        component_ids
    );
    layout!(expected, AmsMelFaultAmbiguityGroupSpanV1, data, size);
    layout!(
        expected,
        AmsMelFaultV1,
        fault_id,
        severity,
        state,
        fault_data,
        detection_time_ns,
        fault_code,
        fault_description,
        component_ids,
        ambiguity_groups
    );
    layout!(expected, AmsMelFaultSpanV1, data, size);
    layout!(
        expected,
        AmsMelBitStatusV1,
        active_bits,
        completed_bits,
        faults
    );
    layout!(
        expected,
        AmsMelIrC2MetadataEventV1,
        kind,
        command_status,
        bit_configuration,
        bit_status,
        channel_comms_test
    );
    layout!(
        expected,
        AmsMelIrC2MetadataCountersV1,
        events_received,
        events_dropped_queue_full,
        malformed_or_unsupported
    );
    layout!(expected, AmsMelIrBadPixelV1, row, column, reason);
    layout!(expected, AmsMelIrBadPixelSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrBadPixelListV1,
        reported_size,
        reported_count,
        pixels
    );
    layout!(expected, AmsMelIrAzElV1, azimuth_rad, elevation_rad);
    layout!(
        expected,
        AmsMelIrLineOfSightReportV1,
        system_time_ns,
        pointing_angle,
        pointing_angle_rates,
        at_speed,
        in_tolerance,
        platform_attitude,
        validity_flag_bitfield,
        image_rotation_rad
    );
    layout!(
        expected,
        AmsMelIrLineOfSightEulerV1,
        system_time_ns,
        attitude,
        attitude_rates
    );
    layout!(
        expected,
        AmsMelIrNavigationResponseV1,
        system_time_ns,
        command_id,
        request_id
    );
    layout!(
        expected,
        AmsMelIrImageMetadataEventV1,
        kind,
        bad_pixel_list,
        line_of_sight_report,
        line_of_sight_euler,
        navigation_response
    );
    layout!(
        expected,
        AmsMelIrChannelCommsTestReportV1,
        command_id,
        request_id
    );
    layout!(
        expected,
        AmsMelIrChannelCommsTestRequestV1,
        command_id,
        channel_id,
        request_id
    );
    layout!(
        expected,
        AmsMelIrChannelCommsTestResultV1,
        command_id,
        request_id,
        error_code
    );
    layout!(
        expected,
        AmsMelIrBandInfoV1,
        kind,
        min_wavelength_m,
        max_wavelength_m
    );
    layout!(expected, AmsMelIrBandInfoSpanV1, data, size);
    layout!(expected, AmsMelIrImageBandV1, band_index, bands);
    layout!(expected, AmsMelIrImageBandSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrChannelCapabilityV1,
        channel_id,
        height,
        width,
        bit_depth,
        row_pitch,
        buffer_size,
        image_size,
        number_of_bands,
        pixel_format,
        sensor_types,
        platform_id,
        sensor_location,
        channel_types,
        task_schedule_depth,
        odc_available,
        nuc_available,
        metadata_capabilities,
        image_bands,
        nav_frames
    );
    layout!(
        expected,
        AmsMelIrHealthConfigV1,
        channel_id,
        channel_type,
        platform_id,
        sensor_location
    );
    layout!(expected, AmsMelEulerV1, roll, pitch, yaw);
    layout!(expected, AmsMelForeignKeyV1, key, system_name);
    layout!(
        expected,
        AmsMelInstallationDetailsV1,
        location,
        orientation,
        boresight
    );
    layout!(expected, AmsMelTemperatureStatusV1, temperature_c, state);
    layout!(
        expected,
        AmsMelMfaComponentV1,
        component_id,
        state,
        temperature,
        installation_location_id,
        installation_details
    );
    layout!(expected, AmsMelMfaComponentSpanV1, data, size);
    layout!(
        expected,
        AmsMelAboutV1,
        model,
        serial_number,
        software_version,
        bootloader_software_version,
        hardware_version
    );
    layout!(
        expected,
        AmsMelMfaStatusV1,
        state,
        state_description,
        mode_description,
        transition_status,
        about,
        components
    );
    layout!(
        expected,
        AmsMelIrSubsystemDepInfoV1,
        subsystem_id,
        criticality,
        failure
    );
    layout!(expected, AmsMelIrSubsystemDepInfoSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrVersionV1,
        source,
        major_revision,
        minor_revision,
        engineering_revision
    );
    layout!(
        expected,
        AmsMelIrSubsystemCsciInfoV1,
        csci,
        mode,
        version,
        criticality,
        failure,
        bit_report,
        connection_established
    );
    layout!(expected, AmsMelIrSubsystemCsciInfoSpanV1, data, size);
    layout!(
        expected,
        AmsMelIrSubsystemStatusV1,
        subsystem_id,
        criticality,
        status_sequence_number,
        failure,
        subsystem_count,
        subsystems,
        csci_count,
        csci
    );
    layout!(expected, AmsMelNameValuePairV1, name, value);
    layout!(expected, AmsMelNameValuePairSpanV1, data, size);
    layout!(
        expected,
        AmsMelSecurityArtifactV1,
        component_id,
        associated_id
    );
    layout!(expected, AmsMelSecurityArtifactSpanV1, data, size);
    layout!(
        expected,
        AmsMelSecurityEventV1,
        kind,
        category,
        details,
        subsystem_id,
        service_id,
        mdf_id
    );
    layout!(
        expected,
        AmsMelSecurityAuditRecordV1,
        security_event_id,
        event_timestamp_ns,
        subsystem_id,
        artifacts,
        event,
        outcome,
        severity
    );
    layout!(
        expected,
        AmsMelIrHealthMetadataEventV1,
        kind,
        mfa_status,
        bit_status,
        subsystem_status,
        discrete_status,
        security_audit,
        mfa_status_detailed
    );
    expected.extend([
        size_of::<*mut AmsMelIrC2Metadata>(),
        align_of::<*mut AmsMelIrC2Metadata>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrC2MetadataEvent>(),
        align_of::<*mut AmsMelIrC2MetadataEvent>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrImageMetadata>(),
        align_of::<*mut AmsMelIrImageMetadata>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrImageMetadataEvent>(),
        align_of::<*mut AmsMelIrImageMetadataEvent>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrChannelCommsRequest>(),
        align_of::<*mut AmsMelIrChannelCommsRequest>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrChannelCapability>(),
        align_of::<*mut AmsMelIrChannelCapability>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrHealth>(),
        align_of::<*mut AmsMelIrHealth>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrHealthMetadata>(),
        align_of::<*mut AmsMelIrHealthMetadata>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrHealthMetadataEvent>(),
        align_of::<*mut AmsMelIrHealthMetadataEvent>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrNavigationRequest>(),
        align_of::<*mut AmsMelIrNavigationRequest>(),
    ]);
    expected.extend([
        AMS_MEL_POSITION_SOLUTION_NOT_SET as usize,
        AMS_MEL_POSITION_SOLUTION_ALIGNING as usize,
        AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL as usize,
        AMS_MEL_POSITION_SOLUTION_GPS as usize,
        AMS_MEL_POSITION_SOLUTION_BLENDED as usize,
        AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE as usize,
    ]);
    layout!(expected, AmsMelNorthEastDownV1, north, east, down);
    layout!(
        expected,
        AmsMelAttitudeRateV1,
        attitude_rate,
        attitude_rate_time_ns
    );
    layout!(
        expected,
        AmsMelPositionVelocityCovarianceV1,
        position_position_pn_pn,
        position_position_pn_pe,
        position_position_pn_pd,
        position_position_pe_pe,
        position_position_pe_pd,
        position_position_pd_pd,
        position_velocity_pn_vn,
        position_velocity_pn_ve,
        position_velocity_pn_vd,
        position_velocity_pe_ve,
        position_velocity_pe_vd,
        position_velocity_pd_vd,
        velocity_velocity_vn_vn,
        velocity_velocity_vn_ve,
        velocity_velocity_vn_vd,
        velocity_velocity_ve_ve,
        velocity_velocity_ve_vd,
        velocity_velocity_vd_vd
    );
    layout!(
        expected,
        AmsMelNavigationReportV1,
        system_time_ns,
        state,
        latitude_rad,
        longitude_rad,
        altitude_m,
        attitude,
        attitude_rate,
        speed,
        acceleration,
        wander_angle_rad,
        magnetic_heading,
        altitude_msl,
        position_velocity_covariance_uncertainty
    );
    layout!(expected, AmsMelIrNavigationResultV1, response, error_code);
    expected.extend([
        size_of::<*mut AmsMelIrInstrumentation>(),
        align_of::<*mut AmsMelIrInstrumentation>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrInstrumentationRequest>(),
        align_of::<*mut AmsMelIrInstrumentationRequest>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrInstrumentationMetadata>(),
        align_of::<*mut AmsMelIrInstrumentationMetadata>(),
    ]);
    expected.extend([
        size_of::<*mut AmsMelIrInstrumentationMetadataEvent>(),
        align_of::<*mut AmsMelIrInstrumentationMetadataEvent>(),
    ]);
    expected.extend([
        AMS_MEL_IR_PRIORITY_NORMAL as usize,
        AMS_MEL_IR_PRIORITY_DEBUG as usize,
        AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT as usize,
        AMS_MEL_IR_CHANNEL_INSTRUMENTATION as usize,
    ]);
    layout!(
        expected,
        AmsMelIrInstrumentationConfigV1,
        channel_id,
        channel_type,
        platform_id,
        sensor_location
    );
    layout!(
        expected,
        AmsMelIrInstrumentationLevelCommandV1,
        command_id,
        priority
    );
    layout!(
        expected,
        AmsMelIrInstrumentationReportV1,
        command_id,
        size,
        timestamp_ns,
        priority
    );
    layout!(
        expected,
        AmsMelIrInstrumentationResultV1,
        report,
        error_code
    );
    layout!(
        expected,
        AmsMelIrInstrumentationMetadataEventV1,
        kind,
        report
    );
    expected.extend([
        size_of::<*mut AmsMelIrTrack>(),
        align_of::<*mut AmsMelIrTrack>(),
    ]);
    expected.extend([AMS_MEL_IR_CHANNEL_IRST_TRACK as usize]);
    layout!(
        expected,
        AmsMelIrTrackConfigV1,
        channel_id,
        channel_type,
        platform_id,
        sensor_location
    );
    expected.extend([
        size_of::<*mut AmsMelIrTrackMetadata>(),
        align_of::<*mut AmsMelIrTrackMetadata>(),
        size_of::<*mut AmsMelIrTrackMetadataEvent>(),
        align_of::<*mut AmsMelIrTrackMetadataEvent>(),
        AMS_MEL_IR_TRACK_STATE_IDLE as usize,
        AMS_MEL_IR_TRACK_STATE_DETECTED as usize,
        AMS_MEL_IR_TRACK_STATE_COAST as usize,
        AMS_MEL_IR_TRACK_STATE_DROPPED as usize,
        AMS_MEL_IR_TRACK_MODE_IDLE as usize,
        AMS_MEL_IR_TRACK_MODE_SCAN as usize,
        AMS_MEL_IR_TRACK_MODE_STARE as usize,
        AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT as usize,
    ]);
    layout!(
        expected,
        AmsMelIrTrackReportV1,
        system_time_ns,
        activity_id,
        measured_ned,
        measured_intensity,
        measured_snr,
        filtered_ned,
        filtered_intensity,
        filtered_snr,
        range_m,
        range_error_m,
        spatial_extent_rad,
        track_quality,
        clutter,
        age_ns,
        state,
        mode
    );
    layout!(expected, AmsMelIrTrackMetadataEventV1, kind, track_report);
    expected.extend([
        size_of::<*mut AmsMelIrTrackUpdateRequest>(),
        align_of::<*mut AmsMelIrTrackUpdateRequest>(),
        AMS_MEL_IR_TRACK_STATUS_CREATE as usize,
        AMS_MEL_IR_TRACK_STATUS_UPDATE as usize,
        AMS_MEL_IR_TRACK_STATUS_PREDICT as usize,
        AMS_MEL_IR_TRACK_STATUS_DELETE as usize,
    ]);
    layout!(
        expected,
        AmsMelIrTrackCovarianceV1,
        xx,
        xy,
        xz,
        x_vx,
        x_vy,
        x_vz,
        yy,
        yz,
        y_vx,
        y_vy,
        y_vz,
        zz,
        z_vx,
        z_vy,
        z_vz,
        vx_vx,
        vx_vy,
        vx_vz,
        vy_vy,
        vy_vz,
        vz_vz
    );
    layout!(
        expected,
        AmsMelIrTrackDataUpdateV1,
        platform_id,
        capability_uuid,
        activity_uuid,
        track_id,
        entity_uuid,
        track_status,
        time_of_validity_seconds,
        time_of_last_update_seconds,
        track_position_ecef,
        track_velocity_ecef,
        covariance,
        maneuver_probability,
        track_quality
    );
    layout!(expected, AmsMelIrTrackUpdateResultV1, status, error_code);
    expected.extend([
        size_of::<*mut AmsMelIrTrackSystemResponseRequest>(),
        align_of::<*mut AmsMelIrTrackSystemResponseRequest>(),
    ]);
    layout!(
        expected,
        AmsMelIrSystemTrackDataResponseV1,
        system_time_ns,
        command_id,
        request_id,
        track_id,
        range_m,
        range_rate_mps,
        range_error_m,
        range_rate_error_mps,
        az_el_valid,
        range_valid,
        inertial_az_el,
        az_el_error
    );
    layout!(
        expected,
        AmsMelIrTrackSystemResponseResultV1,
        status,
        error_code
    );
    expected.extend([
        AMS_MEL_OK as usize,
        AMS_MEL_ABI_VERSION_MAJOR as usize,
        AMS_MEL_ABI_VERSION_MINOR as usize,
    ]);
    assert_eq!(actual, expected);
}

fn absolute(path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_owned()
    } else {
        env::current_dir().expect("current directory").join(path)
    }
}
