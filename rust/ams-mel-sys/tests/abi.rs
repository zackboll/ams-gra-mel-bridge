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
