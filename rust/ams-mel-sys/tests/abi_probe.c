#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdio.h>

#define VALUE(value) printf("%llu\n", (unsigned long long)(value))
#define LAYOUT(type) VALUE(sizeof(type)); VALUE(_Alignof(type))
#define FIELD(type, field) VALUE(offsetof(type, field))

int main(void)
{
    ams_mel_abi_version_v1 version = {0, 0};
    ams_mel_status_t (*submit_bit)(ams_mel_ir_c2 *, uint32_t,
        ams_mel_ir_return_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_bit_noop;
    ams_mel_status_t (*wait_return)(const ams_mel_ir_return_request *, uint32_t,
        ams_mel_ir_return_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_return_request_wait;
    ams_mel_status_t (*close_return)(ams_mel_ir_return_request **, char *,
        size_t, size_t *) = ams_mel_ir_return_request_close;
    (void)submit_bit; (void)wait_return; (void)close_return;
    ams_mel_status_t (*submit_mode)(ams_mel_ir_c2 *, const ams_mel_ir_mode_command_v1 *,
        ams_mel_ir_mode_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_mode;
    ams_mel_status_t (*submit_full_bit)(ams_mel_ir_c2 *, const ams_mel_ir_bit_command_v1 *,
        ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_bit;
    ams_mel_status_t (*submit_config)(ams_mel_ir_c2 *, const ams_mel_ir_config_set_command_v1 *,
        ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_config_set;
    (void)submit_mode; (void)submit_full_bit; (void)submit_config;
    ams_mel_status_t (*metadata_open)(ams_mel_ir_c2 *, size_t, ams_mel_ir_c2_metadata **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_open;
    ams_mel_status_t (*metadata_receive)(ams_mel_ir_c2_metadata *, uint32_t, ams_mel_ir_c2_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_receive;
    ams_mel_status_t (*metadata_counters)(const ams_mel_ir_c2_metadata *, ams_mel_ir_c2_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_get_counters;
    ams_mel_status_t (*metadata_close)(ams_mel_ir_c2_metadata **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_close;
    ams_mel_status_t (*event_view)(const ams_mel_ir_c2_metadata_event *, const ams_mel_ir_c2_metadata_event_v1 **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_event_view;
    ams_mel_status_t (*event_close)(ams_mel_ir_c2_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_event_close;
    (void)metadata_open; (void)metadata_receive; (void)metadata_counters; (void)metadata_close; (void)event_view; (void)event_close;

    VALUE(AMS_MEL_OK); VALUE(AMS_MEL_INVALID_ARGUMENT);
    VALUE(AMS_MEL_LIBRARY_LOAD_FAILED); VALUE(AMS_MEL_SYMBOL_NOT_FOUND);
    VALUE(AMS_MEL_FACTORY_FAILED); VALUE(AMS_MEL_INITIALIZATION_FAILED);
    VALUE(AMS_MEL_PROVIDER_EXCEPTION); VALUE(AMS_MEL_BUFFER_TOO_SMALL);
    VALUE(AMS_MEL_INTERNAL_ERROR); VALUE(AMS_MEL_TIMEOUT);
    VALUE(AMS_MEL_STREAM_STOPPED); VALUE(AMS_MEL_PROVIDER_FAILED);
    VALUE(AMS_MEL_COMMAND_REJECTED);
    VALUE(AMS_MEL_IR_CHANNEL_IRST_IMAGE);
    VALUE(AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL);
    VALUE(AMS_MEL_IR_MFA_MODE_UNUSED); VALUE(AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    VALUE(AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED);
    VALUE(AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED);
    VALUE(AMS_MEL_IR_MFA_STATE_NOT_SET); VALUE(AMS_MEL_IR_MFA_STATE_UNKNOWN);
    VALUE(AMS_MEL_IR_MFA_STATE_NOT_INSTALLED); VALUE(AMS_MEL_IR_MFA_STATE_OFF);
    VALUE(AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION); VALUE(AMS_MEL_IR_MFA_STATE_INITIALIZATION);
    VALUE(AMS_MEL_IR_MFA_STATE_STANDBY); VALUE(AMS_MEL_IR_MFA_STATE_OPERATE);
    VALUE(AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY); VALUE(AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY);
    VALUE(AMS_MEL_IR_MFA_STATE_MAINTENANCE); VALUE(AMS_MEL_IR_MFA_STATE_CALIBRATION);
    VALUE(AMS_MEL_IR_MFA_STATE_INITIATED_BIT); VALUE(AMS_MEL_IR_MFA_STATE_SHUTDOWN);
    VALUE(AMS_MEL_IR_MFA_STATE_DEGRADED); VALUE(AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE);
    VALUE(AMS_MEL_IR_COORD_FRAME_INERTIAL); VALUE(AMS_MEL_IR_COORD_FRAME_AIRCRAFT);
    VALUE(AMS_MEL_IR_DEGRADATION_CAPACITY); VALUE(AMS_MEL_IR_DEGRADATION_VOLUME);
    VALUE(AMS_MEL_IR_DEGRADATION_RANGE); VALUE(AMS_MEL_IR_DEGRADATION_REVISIT);
    VALUE(AMS_MEL_IR_RETURN_SUCCESS); VALUE(AMS_MEL_IR_RETURN_BAD_POINTER);
    VALUE(AMS_MEL_IR_RETURN_FAIL); VALUE(AMS_MEL_IR_RETURN_NOT_SUPPORTED);
    VALUE(AMS_MEL_IR_RETURN_NOT_IMPLEMENTED);
    VALUE(AMS_MEL_ERROR_NONE); VALUE(AMS_MEL_ERROR_INVALID_ID);
    VALUE(AMS_MEL_ERROR_INVALID_STATE); VALUE(AMS_MEL_ERROR_INVALID_PARAMETERS);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_RESOURCES);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES);
    VALUE(AMS_MEL_ERROR_UNSUPPORTED);
    VALUE(AMS_MEL_IR_PIXEL_MONO);
    VALUE(AMS_MEL_IR_IMAGE_STARING); VALUE(AMS_MEL_IR_IMAGE_SCANNING);
    VALUE(AMS_MEL_IR_FLIP_NONE); VALUE(AMS_MEL_IR_FLIP_VERTICAL);
    VALUE(AMS_MEL_IR_FLIP_HORIZONTAL); VALUE(AMS_MEL_IR_FLIP_BOTH);

    LAYOUT(ams_mel_abi_version_v1);
    FIELD(ams_mel_abi_version_v1, major); FIELD(ams_mel_abi_version_v1, minor);
    LAYOUT(ams_mel_provider_version_v1);
    FIELD(ams_mel_provider_version_v1, api_version);
    FIELD(ams_mel_provider_version_v1, library_version);
    FIELD(ams_mel_provider_version_v1, vendor);
    FIELD(ams_mel_provider_version_v1, vendor_capacity);
    FIELD(ams_mel_provider_version_v1, vendor_required);
    FIELD(ams_mel_provider_version_v1, description);
    FIELD(ams_mel_provider_version_v1, description_capacity);
    FIELD(ams_mel_provider_version_v1, description_required);
    LAYOUT(ams_mel_string_view_v1);
    FIELD(ams_mel_string_view_v1, data); FIELD(ams_mel_string_view_v1, size);
    LAYOUT(ams_mel_u32_span_v1); FIELD(ams_mel_u32_span_v1, data); FIELD(ams_mel_u32_span_v1, size);
    LAYOUT(ams_mel_string_view_span_v1); FIELD(ams_mel_string_view_span_v1, data); FIELD(ams_mel_string_view_span_v1, size);
    LAYOUT(ams_mel_ir_scan_type_v1); FIELD(ams_mel_ir_scan_type_v1, continuous_scan); FIELD(ams_mel_ir_scan_type_v1, returning); FIELD(ams_mel_ir_scan_type_v1, agile_scan);
    LAYOUT(ams_mel_ir_scan_param_v1);
    FIELD(ams_mel_ir_scan_param_v1, elevation_defined_with_range_and_altitude); FIELD(ams_mel_ir_scan_param_v1, center_az_rad); FIELD(ams_mel_ir_scan_param_v1, center_el_rad); FIELD(ams_mel_ir_scan_param_v1, center_frame_ref_el); FIELD(ams_mel_ir_scan_param_v1, center_frame_ref_az); FIELD(ams_mel_ir_scan_param_v1, scan_width_rad); FIELD(ams_mel_ir_scan_param_v1, scan_height_rad); FIELD(ams_mel_ir_scan_param_v1, scan_type); FIELD(ams_mel_ir_scan_param_v1, scan_id); FIELD(ams_mel_ir_scan_param_v1, scan_rate_rad_per_second); FIELD(ams_mel_ir_scan_param_v1, preferred_revisit_interval_seconds); FIELD(ams_mel_ir_scan_param_v1, required_revisit_interval_seconds); FIELD(ams_mel_ir_scan_param_v1, max_range_of_interest_m); FIELD(ams_mel_ir_scan_param_v1, min_range_of_interest_m); FIELD(ams_mel_ir_scan_param_v1, elevation_scan_center_altitude_m); FIELD(ams_mel_ir_scan_param_v1, elevation_scan_center_range_m); FIELD(ams_mel_ir_scan_param_v1, degradation_method);
    LAYOUT(ams_mel_ir_mode_command_v1); FIELD(ams_mel_ir_mode_command_v1, command_id); FIELD(ams_mel_ir_mode_command_v1, state); FIELD(ams_mel_ir_mode_command_v1, mode); FIELD(ams_mel_ir_mode_command_v1, scan_parameters);
    LAYOUT(ams_mel_ir_bit_command_v1); FIELD(ams_mel_ir_bit_command_v1, command_id); FIELD(ams_mel_ir_bit_command_v1, initiate_bit_ids); FIELD(ams_mel_ir_bit_command_v1, cancel_bit_ids); FIELD(ams_mel_ir_bit_command_v1, clear_fault_codes);
    LAYOUT(ams_mel_ir_config_set_command_v1); FIELD(ams_mel_ir_config_set_command_v1, command_id); FIELD(ams_mel_ir_config_set_command_v1, system_time_ns); FIELD(ams_mel_ir_config_set_command_v1, config);
    LAYOUT(ams_mel_uci_id_v1);
    FIELD(ams_mel_uci_id_v1, uuid); FIELD(ams_mel_uci_id_v1, descriptive_label);
    LAYOUT(ams_mel_component_location_v1);
    FIELD(ams_mel_component_location_v1, offset_x_m);
    FIELD(ams_mel_component_location_v1, offset_y_m);
    FIELD(ams_mel_component_location_v1, offset_z_m);
    FIELD(ams_mel_component_location_v1, key);
    FIELD(ams_mel_component_location_v1, system_name);
    LAYOUT(ams_mel_ir_stream_config_v1);
    FIELD(ams_mel_ir_stream_config_v1, channel_type);
    FIELD(ams_mel_ir_stream_config_v1, channel_id);
    FIELD(ams_mel_ir_stream_config_v1, platform_id);
    FIELD(ams_mel_ir_stream_config_v1, sensor_location);
    FIELD(ams_mel_ir_stream_config_v1, buffer_count);
    FIELD(ams_mel_ir_stream_config_v1, buffer_size);
    FIELD(ams_mel_ir_stream_config_v1, queue_capacity);
    LAYOUT(ams_mel_ir_c2_config_v1);
    FIELD(ams_mel_ir_c2_config_v1, channel_type);
    FIELD(ams_mel_ir_c2_config_v1, channel_id);
    FIELD(ams_mel_ir_c2_config_v1, platform_id);
    FIELD(ams_mel_ir_c2_config_v1, sensor_location);
    LAYOUT(ams_mel_ir_mode_result_v1);
    FIELD(ams_mel_ir_mode_result_v1, mode);
    FIELD(ams_mel_ir_mode_result_v1, error_code);
    LAYOUT(ams_mel_ir_return_result_v1);
    FIELD(ams_mel_ir_return_result_v1, value);
    FIELD(ams_mel_ir_return_result_v1, error_code);
    LAYOUT(ams_mel_ir_frame_v1);
    FIELD(ams_mel_ir_frame_v1, system_time_ns);
    FIELD(ams_mel_ir_frame_v1, integration_time_ns);
    FIELD(ams_mel_ir_frame_v1, width); FIELD(ams_mel_ir_frame_v1, height);
    FIELD(ams_mel_ir_frame_v1, bits_per_pixel);
    FIELD(ams_mel_ir_frame_v1, number_of_bands);
    FIELD(ams_mel_ir_frame_v1, horizontal_fov_rad);
    FIELD(ams_mel_ir_frame_v1, vertical_fov_rad);
    FIELD(ams_mel_ir_frame_v1, pixel_format);
    FIELD(ams_mel_ir_frame_v1, frame_id);
    FIELD(ams_mel_ir_frame_v1, subframe_id);
    FIELD(ams_mel_ir_frame_v1, subframe_total);
    FIELD(ams_mel_ir_frame_v1, image_type);
    FIELD(ams_mel_ir_frame_v1, image_flip);
    FIELD(ams_mel_ir_frame_v1, image_flags);
    FIELD(ams_mel_ir_frame_v1, dither_row);
    FIELD(ams_mel_ir_frame_v1, dither_column);
    FIELD(ams_mel_ir_frame_v1, row_offset);
    FIELD(ams_mel_ir_frame_v1, column_offset);
    FIELD(ams_mel_ir_frame_v1, band_index);
    FIELD(ams_mel_ir_frame_v1, reserved);
    FIELD(ams_mel_ir_frame_v1, pixels);
    FIELD(ams_mel_ir_frame_v1, pixel_capacity);
    FIELD(ams_mel_ir_frame_v1, pixel_required);
    LAYOUT(ams_mel_ir_stream_counters_v1);
    FIELD(ams_mel_ir_stream_counters_v1, frames_received);
    FIELD(ams_mel_ir_stream_counters_v1, frames_dropped_queue_full);
    FIELD(ams_mel_ir_stream_counters_v1, malformed_or_unsupported_frames);
    VALUE(AMS_MEL_IR_C2_METADATA_COMMAND_STATUS); VALUE(AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION); VALUE(AMS_MEL_IR_C2_METADATA_BIT_STATUS);
    VALUE(AMS_MEL_IR_COMMAND_NOT_SET); VALUE(AMS_MEL_IR_COMMAND_RECEIVED); VALUE(AMS_MEL_IR_COMMAND_ACCEPTED); VALUE(AMS_MEL_IR_COMMAND_REJECTED); VALUE(AMS_MEL_IR_COMMAND_CANCELLED);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_NOT_SET);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ATTEMPTS);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ENDURANCE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_CLASSIFICATION);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_FOR_FOV_LIMIT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_GATING);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_MANEUVER_LIMIT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OP);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_OCCLUSION);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_RANGE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PERFORMANCE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_RF);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_ROUTE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SAFETY);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TARGET_ANGLE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_TIME);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CONSTRAINT_SYSTEM);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INFEASIBLE_ROUTE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_MISSION_EVENT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_STATE_OR_SETTINGS_CHANGE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_UNAVAILABLE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_FAULT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_SYSTEM_CONFLICT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_UNAVAILABLE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_SUBSYSTEM_FAULT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_FAULT);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_PRECEDENCE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CAPABILITY_UNAVAILABLE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INSUFFICIENT_RESOURCES);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_RANKING);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_WEATHER);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INELIGIBLE_CONTROL_SOURCE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_PREDECESSOR);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_ALL_OR_NOTHING);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_DEPENDENCY_EITHER_OR);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INIT_CRITERIA_NOT_MET);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN_ID);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_INPUT_OTHER);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_MDF_ACTIVATION_ERROR);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_MULTIPLE);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_CANCELLED);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_OTHER);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_UNKNOWN);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_ABORTED);
    VALUE(AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER);
    VALUE(AMS_MEL_BIT_CONTROL_NOT_SET); VALUE(AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND); VALUE(AMS_MEL_BIT_CONTROL_SUBSYSTEM_STATE_COMMAND); VALUE(AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED);
    VALUE(AMS_MEL_BIT_RESULT_NOT_SET); VALUE(AMS_MEL_BIT_RESULT_PASS); VALUE(AMS_MEL_BIT_RESULT_FAIL); VALUE(AMS_MEL_BIT_RESULT_INTERRUPTED); VALUE(AMS_MEL_BIT_RESULT_NOT_TESTED);
    VALUE(AMS_MEL_FAULT_SEVERITY_NOT_SET); VALUE(AMS_MEL_FAULT_SEVERITY_NOMINAL); VALUE(AMS_MEL_FAULT_SEVERITY_CAUTION); VALUE(AMS_MEL_FAULT_SEVERITY_WARNING); VALUE(AMS_MEL_FAULT_SEVERITY_FAILED);
    VALUE(AMS_MEL_FAULT_STATE_NOT_SET); VALUE(AMS_MEL_FAULT_STATE_SET); VALUE(AMS_MEL_FAULT_STATE_CLEARED); VALUE(AMS_MEL_FAULT_STATE_UNKNOWN);
#define RECORD(type, ...) LAYOUT(type); __VA_ARGS__
    RECORD(ams_mel_uci_id_span_v1, FIELD(ams_mel_uci_id_span_v1,data); FIELD(ams_mel_uci_id_span_v1,size));
    RECORD(ams_mel_ir_command_status_v1, FIELD(ams_mel_ir_command_status_v1,command_id); FIELD(ams_mel_ir_command_status_v1,state); FIELD(ams_mel_ir_command_status_v1,reason_id); FIELD(ams_mel_ir_command_status_v1,reason_description));
    RECORD(ams_mel_bit_type_v1, FIELD(ams_mel_bit_type_v1,bit_id); FIELD(ams_mel_bit_type_v1,accepted_interface); FIELD(ams_mel_bit_type_v1,bit_item_names); FIELD(ams_mel_bit_type_v1,subsystem_component_ids); FIELD(ams_mel_bit_type_v1,expected_duration_ns));
    RECORD(ams_mel_bit_type_span_v1, FIELD(ams_mel_bit_type_span_v1,data); FIELD(ams_mel_bit_type_span_v1,size));
    RECORD(ams_mel_bit_configuration_v1, FIELD(ams_mel_bit_configuration_v1,bit_types));
    RECORD(ams_mel_active_bit_v1, FIELD(ams_mel_active_bit_v1,bit_id); FIELD(ams_mel_active_bit_v1,estimated_completion_time_ns); FIELD(ams_mel_active_bit_v1,estimated_percent_complete));
    RECORD(ams_mel_active_bit_span_v1, FIELD(ams_mel_active_bit_span_v1,data); FIELD(ams_mel_active_bit_span_v1,size));
    RECORD(ams_mel_completed_bit_item_v1, FIELD(ams_mel_completed_bit_item_v1,bit_item_name); FIELD(ams_mel_completed_bit_item_v1,result); FIELD(ams_mel_completed_bit_item_v1,fail_reason));
    RECORD(ams_mel_completed_bit_item_span_v1, FIELD(ams_mel_completed_bit_item_span_v1,data); FIELD(ams_mel_completed_bit_item_span_v1,size));
    RECORD(ams_mel_completed_bit_v1, FIELD(ams_mel_completed_bit_v1,bit_id); FIELD(ams_mel_completed_bit_v1,time_tag_ns); FIELD(ams_mel_completed_bit_v1,result); FIELD(ams_mel_completed_bit_v1,fail_reason); FIELD(ams_mel_completed_bit_v1,bit_items));
    RECORD(ams_mel_completed_bit_span_v1, FIELD(ams_mel_completed_bit_span_v1,data); FIELD(ams_mel_completed_bit_span_v1,size));
    RECORD(ams_mel_fault_data_v1, FIELD(ams_mel_fault_data_v1,key); FIELD(ams_mel_fault_data_v1,value); FIELD(ams_mel_fault_data_v1,format); FIELD(ams_mel_fault_data_v1,units));
    RECORD(ams_mel_fault_data_span_v1, FIELD(ams_mel_fault_data_span_v1,data); FIELD(ams_mel_fault_data_span_v1,size));
    RECORD(ams_mel_fault_ambiguity_group_v1, FIELD(ams_mel_fault_ambiguity_group_v1,diagnostic_test_ids); FIELD(ams_mel_fault_ambiguity_group_v1,component_ids));
    RECORD(ams_mel_fault_ambiguity_group_span_v1, FIELD(ams_mel_fault_ambiguity_group_span_v1,data); FIELD(ams_mel_fault_ambiguity_group_span_v1,size));
    RECORD(ams_mel_fault_v1, FIELD(ams_mel_fault_v1,fault_id); FIELD(ams_mel_fault_v1,severity); FIELD(ams_mel_fault_v1,state); FIELD(ams_mel_fault_v1,fault_data); FIELD(ams_mel_fault_v1,detection_time_ns); FIELD(ams_mel_fault_v1,fault_code); FIELD(ams_mel_fault_v1,fault_description); FIELD(ams_mel_fault_v1,component_ids); FIELD(ams_mel_fault_v1,ambiguity_groups));
    RECORD(ams_mel_fault_span_v1, FIELD(ams_mel_fault_span_v1,data); FIELD(ams_mel_fault_span_v1,size));
    RECORD(ams_mel_bit_status_v1, FIELD(ams_mel_bit_status_v1,active_bits); FIELD(ams_mel_bit_status_v1,completed_bits); FIELD(ams_mel_bit_status_v1,faults));
    RECORD(ams_mel_ir_c2_metadata_event_v1, FIELD(ams_mel_ir_c2_metadata_event_v1,kind); FIELD(ams_mel_ir_c2_metadata_event_v1,command_status); FIELD(ams_mel_ir_c2_metadata_event_v1,bit_configuration); FIELD(ams_mel_ir_c2_metadata_event_v1,bit_status));
    RECORD(ams_mel_ir_c2_metadata_counters_v1, FIELD(ams_mel_ir_c2_metadata_counters_v1,events_received); FIELD(ams_mel_ir_c2_metadata_counters_v1,events_dropped_queue_full); FIELD(ams_mel_ir_c2_metadata_counters_v1,malformed_or_unsupported));
    LAYOUT(ams_mel_ir_c2_metadata *); LAYOUT(ams_mel_ir_c2_metadata_event *);

    VALUE(ams_mel_get_abi_version(&version));
    VALUE(version.major); VALUE(version.minor);
    return 0;
}
