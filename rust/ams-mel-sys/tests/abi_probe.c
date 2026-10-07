#include <ams_mel/abi.h>
#include "../../../native/tests/rf_local_functions_abi_probe.h"
#include "../../../native/tests/rf_transmit_power_abi_probe.h"

#include <stddef.h>
#include <stdio.h>

#define VALUE(value) printf("%llu\n", (unsigned long long)(value))
#define LAYOUT(type) VALUE(sizeof(type)); VALUE(_Alignof(type))
#define FIELD(type, field) VALUE(offsetof(type, field))

int main(void)
{
    check_rf_local_function_signatures();
    check_rf_transmit_power_signatures();
    ams_mel_status_t (*submit_v4)(ams_mel_rf_virtual_aperture *,
        const ams_mel_rf_job_request_config_v4 *, ams_mel_rf_job_request **,
        char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_submit_job_v4;
    (void)submit_v4;
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
    ams_mel_status_t (*keepalive)(ams_mel_ir_c2 *, ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_send_keepalive;
    ams_mel_status_t (*submit_comms)(ams_mel_ir_c2 *, const ams_mel_ir_channel_comms_test_request_v1 *, ams_mel_ir_channel_comms_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_comms_test;
    ams_mel_status_t (*wait_comms)(const ams_mel_ir_channel_comms_request *, uint32_t, ams_mel_ir_channel_comms_test_result_v1 *, char *, size_t, size_t *) = ams_mel_ir_channel_comms_request_wait;
    ams_mel_status_t (*close_comms)(ams_mel_ir_channel_comms_request **, char *, size_t, size_t *) = ams_mel_ir_channel_comms_request_close;
    ams_mel_status_t (*register_comms)(ams_mel_ir_c2_metadata *, char *, size_t, size_t *) = ams_mel_ir_c2_metadata_register_comms_test;
    ams_mel_status_t (*get_cap)(ams_mel_ir_c2 *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_c2_get_capabilities;
    ams_mel_status_t (*view_cap)(const ams_mel_ir_channel_capability *, const ams_mel_ir_channel_capability_v1 **, char *, size_t, size_t *) = ams_mel_ir_channel_capability_view;
    ams_mel_status_t (*close_cap)(ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_channel_capability_close;
    (void)keepalive; (void)submit_comms; (void)wait_comms; (void)close_comms; (void)register_comms; (void)get_cap; (void)view_cap; (void)close_cap;
    ams_mel_status_t (*health_open)(const ams_mel_session *, const ams_mel_ir_health_config_v1 *, ams_mel_ir_health **, char *, size_t, size_t *) = ams_mel_ir_health_open;
    ams_mel_status_t (*health_enable)(ams_mel_ir_health *, char *, size_t, size_t *) = ams_mel_ir_health_enable;
    ams_mel_status_t (*health_cap)(ams_mel_ir_health *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_health_get_capabilities;
    ams_mel_status_t (*health_close)(ams_mel_ir_health **, char *, size_t, size_t *) = ams_mel_ir_health_close;
    ams_mel_status_t (*health_metadata_open)(ams_mel_ir_health *, size_t, ams_mel_ir_health_metadata **, char *, size_t, size_t *) = ams_mel_ir_health_metadata_open;
    ams_mel_status_t (*health_receive)(ams_mel_ir_health_metadata *, uint32_t, ams_mel_ir_health_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_health_metadata_receive;
    ams_mel_status_t (*health_counters)(const ams_mel_ir_health_metadata *, ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_health_metadata_get_counters;
    ams_mel_status_t (*health_metadata_close)(ams_mel_ir_health_metadata **, char *, size_t, size_t *) = ams_mel_ir_health_metadata_close;
    ams_mel_status_t (*health_view)(const ams_mel_ir_health_metadata_event *, const ams_mel_ir_health_metadata_event_v1 **, char *, size_t, size_t *) = ams_mel_ir_health_metadata_event_view;
    ams_mel_status_t (*health_event_close)(ams_mel_ir_health_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_health_metadata_event_close;
    (void)health_open; (void)health_enable; (void)health_cap; (void)health_close; (void)health_metadata_open; (void)health_receive; (void)health_counters; (void)health_metadata_close; (void)health_view; (void)health_event_close;
    ams_mel_status_t (*image_cap)(ams_mel_ir_stream *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_stream_get_capabilities;
    ams_mel_status_t (*image_open)(ams_mel_ir_stream *, size_t, ams_mel_ir_image_metadata **, char *, size_t, size_t *) = ams_mel_ir_image_metadata_open;
    ams_mel_status_t (*image_receive)(ams_mel_ir_image_metadata *, uint32_t, ams_mel_ir_image_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_image_metadata_receive;
    ams_mel_status_t (*image_counters)(const ams_mel_ir_image_metadata *, ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_image_metadata_get_counters;
    ams_mel_status_t (*image_close)(ams_mel_ir_image_metadata **, char *, size_t, size_t *) = ams_mel_ir_image_metadata_close;
    ams_mel_status_t (*image_view)(const ams_mel_ir_image_metadata_event *, const ams_mel_ir_image_metadata_event_v1 **, char *, size_t, size_t *) = ams_mel_ir_image_metadata_event_view;
    ams_mel_status_t (*image_event_close)(ams_mel_ir_image_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_image_metadata_event_close;
    ams_mel_status_t (*navigation_submit)(ams_mel_ir_stream *, const ams_mel_navigation_report_v1 *, ams_mel_ir_navigation_request **, char *, size_t, size_t *) = ams_mel_ir_stream_submit_navigation_report;
    ams_mel_status_t (*navigation_wait)(const ams_mel_ir_navigation_request *, uint32_t, ams_mel_ir_navigation_result_v1 *, char *, size_t, size_t *) = ams_mel_ir_navigation_request_wait;
    ams_mel_status_t (*navigation_close)(ams_mel_ir_navigation_request **, char *, size_t, size_t *) = ams_mel_ir_navigation_request_close;
    (void)image_cap; (void)image_open; (void)image_receive; (void)image_counters; (void)image_close; (void)image_view; (void)image_event_close;
    (void)navigation_submit; (void)navigation_wait; (void)navigation_close;

    VALUE(AMS_MEL_OK); VALUE(AMS_MEL_INVALID_ARGUMENT);
    VALUE(AMS_MEL_LIBRARY_LOAD_FAILED); VALUE(AMS_MEL_SYMBOL_NOT_FOUND);
    VALUE(AMS_MEL_FACTORY_FAILED); VALUE(AMS_MEL_INITIALIZATION_FAILED);
    VALUE(AMS_MEL_PROVIDER_EXCEPTION); VALUE(AMS_MEL_BUFFER_TOO_SMALL);
    VALUE(AMS_MEL_INTERNAL_ERROR); VALUE(AMS_MEL_TIMEOUT);
    VALUE(AMS_MEL_STREAM_STOPPED); VALUE(AMS_MEL_PROVIDER_FAILED);
    VALUE(AMS_MEL_COMMAND_REJECTED);
    VALUE(AMS_MEL_RESOURCE_EXHAUSTED);
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
    LAYOUT(ams_mel_session_options_v1);
    FIELD(ams_mel_session_options_v1, max_async_requests);
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
    VALUE(AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST); VALUE(AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT); VALUE(AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER); VALUE(AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE); VALUE(AMS_MEL_IR_BAD_PIXEL_REASON_UNKNOWN);
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
    ams_mel_status_t (*instr_open)(const ams_mel_session *, const ams_mel_ir_instrumentation_config_v1 *, ams_mel_ir_instrumentation **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_open;
    ams_mel_status_t (*instr_enable)(ams_mel_ir_instrumentation *, char *, size_t, size_t *) = ams_mel_ir_instrumentation_enable;
    ams_mel_status_t (*instr_cap)(ams_mel_ir_instrumentation *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_get_capabilities;
    ams_mel_status_t (*instr_submit)(ams_mel_ir_instrumentation *, const ams_mel_ir_instrumentation_level_command_v1 *, ams_mel_ir_instrumentation_request **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_submit_level;
    ams_mel_status_t (*instr_wait)(const ams_mel_ir_instrumentation_request *, uint32_t, ams_mel_ir_instrumentation_result_v1 *, char *, size_t, size_t *) = ams_mel_ir_instrumentation_request_wait;
    ams_mel_status_t (*instr_req_close)(ams_mel_ir_instrumentation_request **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_request_close;
    ams_mel_status_t (*instr_meta_open)(ams_mel_ir_instrumentation *, size_t, ams_mel_ir_instrumentation_metadata **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_open;
    ams_mel_status_t (*instr_meta_recv)(ams_mel_ir_instrumentation_metadata *, uint32_t, ams_mel_ir_instrumentation_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_receive;
    ams_mel_status_t (*instr_meta_counters)(const ams_mel_ir_instrumentation_metadata *, ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_get_counters;
    ams_mel_status_t (*instr_meta_close)(ams_mel_ir_instrumentation_metadata **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_close;
    ams_mel_status_t (*instr_ev_view)(const ams_mel_ir_instrumentation_metadata_event *, const ams_mel_ir_instrumentation_metadata_event_v1 **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_event_view;
    ams_mel_status_t (*instr_ev_close)(ams_mel_ir_instrumentation_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_metadata_event_close;
    ams_mel_status_t (*instr_close)(ams_mel_ir_instrumentation **, char *, size_t, size_t *) = ams_mel_ir_instrumentation_close;
    (void)instr_open; (void)instr_enable; (void)instr_cap; (void)instr_submit;
    (void)instr_wait; (void)instr_req_close; (void)instr_meta_open; (void)instr_meta_recv;
    (void)instr_meta_counters; (void)instr_meta_close; (void)instr_ev_view;
    (void)instr_ev_close; (void)instr_close;
    ams_mel_status_t (*track_open)(const ams_mel_session *, const ams_mel_ir_track_config_v1 *, ams_mel_ir_track **, char *, size_t, size_t *) = ams_mel_ir_track_open;
    ams_mel_status_t (*track_enable)(ams_mel_ir_track *, char *, size_t, size_t *) = ams_mel_ir_track_enable;
    ams_mel_status_t (*track_caps)(ams_mel_ir_track *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_track_get_capabilities;
    ams_mel_status_t (*track_close)(ams_mel_ir_track **, char *, size_t, size_t *) = ams_mel_ir_track_close;
    (void)track_open; (void)track_enable; (void)track_caps; (void)track_close;
    /* Task 032B1: the nine public common Channel functions (no output). */
    ams_mel_status_t (*channel_from_c2)(const ams_mel_ir_c2 *, ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_c2;
    ams_mel_status_t (*channel_from_stream)(const ams_mel_ir_stream *, ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_stream;
    ams_mel_status_t (*channel_from_health)(const ams_mel_ir_health *, ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_health;
    ams_mel_status_t (*channel_from_instr)(const ams_mel_ir_instrumentation *, ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_instrumentation;
    ams_mel_status_t (*channel_from_track)(const ams_mel_ir_track *, ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_from_track;
    ams_mel_status_t (*channel_keepalive)(const ams_mel_ir_channel *, ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_channel_send_keepalive;
    ams_mel_status_t (*channel_comms)(const ams_mel_ir_channel *, const ams_mel_ir_channel_comms_test_request_v1 *, ams_mel_ir_channel_comms_request **, char *, size_t, size_t *) = ams_mel_ir_channel_submit_comms_test;
    ams_mel_status_t (*channel_caps)(const ams_mel_ir_channel *, ams_mel_ir_channel_capability **, char *, size_t, size_t *) = ams_mel_ir_channel_get_capabilities;
    ams_mel_status_t (*channel_close)(ams_mel_ir_channel **, char *, size_t, size_t *) = ams_mel_ir_channel_close;
    (void)channel_from_c2; (void)channel_from_stream; (void)channel_from_health; (void)channel_from_instr;
    (void)channel_from_track; (void)channel_keepalive; (void)channel_comms; (void)channel_caps; (void)channel_close;
    ams_mel_status_t (*track_meta_open)(ams_mel_ir_track *, size_t, ams_mel_ir_track_metadata **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_open;
    ams_mel_status_t (*track_meta_recv)(ams_mel_ir_track_metadata *, uint32_t, ams_mel_ir_track_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_receive;
    ams_mel_status_t (*track_meta_counters)(const ams_mel_ir_track_metadata *, ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_track_metadata_get_counters;
    ams_mel_status_t (*track_meta_close)(ams_mel_ir_track_metadata **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_close;
    ams_mel_status_t (*track_ev_view)(const ams_mel_ir_track_metadata_event *, const ams_mel_ir_track_metadata_event_v1 **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_event_view;
    ams_mel_status_t (*track_ev_view2)(const ams_mel_ir_track_metadata_event *, const ams_mel_ir_track_metadata_event_v2 **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_event_view_v2;
    ams_mel_status_t (*track_ev_view3)(const ams_mel_ir_track_metadata_event *, const ams_mel_ir_track_metadata_event_v3 **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_event_view_v3;
    ams_mel_status_t (*track_ev_close)(ams_mel_ir_track_metadata_event **, char *, size_t, size_t *) = ams_mel_ir_track_metadata_event_close;
    (void)track_meta_open; (void)track_meta_recv; (void)track_meta_counters;
    (void)track_meta_close; (void)track_ev_view; (void)track_ev_view2;
    (void)track_ev_view3;
    (void)track_ev_close;
    ams_mel_status_t (*track_submit)(ams_mel_ir_track *, const ams_mel_ir_track_data_update_v1 *, ams_mel_ir_track_update_request **, char *, size_t, size_t *) = ams_mel_ir_track_submit_update;
    ams_mel_status_t (*track_upd_wait)(const ams_mel_ir_track_update_request *, uint32_t, ams_mel_ir_track_update_result_v1 *, char *, size_t, size_t *) = ams_mel_ir_track_update_request_wait;
    ams_mel_status_t (*track_upd_close)(ams_mel_ir_track_update_request **, char *, size_t, size_t *) = ams_mel_ir_track_update_request_close;
    (void)track_submit; (void)track_upd_wait; (void)track_upd_close;
    ams_mel_status_t (*track_rsp_submit)(ams_mel_ir_track *, const ams_mel_ir_system_track_data_response_v1 *, ams_mel_ir_track_system_response_request **, char *, size_t, size_t *) = ams_mel_ir_track_submit_system_track_data_response;
    ams_mel_status_t (*track_rsp_wait)(const ams_mel_ir_track_system_response_request *, uint32_t, ams_mel_ir_track_system_response_result_v1 *, char *, size_t, size_t *) = ams_mel_ir_track_system_response_request_wait;
    ams_mel_status_t (*track_rsp_close)(ams_mel_ir_track_system_response_request **, char *, size_t, size_t *) = ams_mel_ir_track_system_response_request_close;
    (void)track_rsp_submit; (void)track_rsp_wait; (void)track_rsp_close;
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
    RECORD(ams_mel_ir_c2_metadata_event_v1, FIELD(ams_mel_ir_c2_metadata_event_v1,kind); FIELD(ams_mel_ir_c2_metadata_event_v1,command_status); FIELD(ams_mel_ir_c2_metadata_event_v1,bit_configuration); FIELD(ams_mel_ir_c2_metadata_event_v1,bit_status); FIELD(ams_mel_ir_c2_metadata_event_v1,channel_comms_test));
    RECORD(ams_mel_ir_c2_metadata_counters_v1, FIELD(ams_mel_ir_c2_metadata_counters_v1,events_received); FIELD(ams_mel_ir_c2_metadata_counters_v1,events_dropped_queue_full); FIELD(ams_mel_ir_c2_metadata_counters_v1,malformed_or_unsupported));
    RECORD(ams_mel_ir_bad_pixel_v1, FIELD(ams_mel_ir_bad_pixel_v1,row); FIELD(ams_mel_ir_bad_pixel_v1,column); FIELD(ams_mel_ir_bad_pixel_v1,reason));
    RECORD(ams_mel_ir_bad_pixel_span_v1, FIELD(ams_mel_ir_bad_pixel_span_v1,data); FIELD(ams_mel_ir_bad_pixel_span_v1,size));
    RECORD(ams_mel_ir_bad_pixel_list_v1, FIELD(ams_mel_ir_bad_pixel_list_v1,reported_size); FIELD(ams_mel_ir_bad_pixel_list_v1,reported_count); FIELD(ams_mel_ir_bad_pixel_list_v1,pixels));
    RECORD(ams_mel_ir_az_el_v1, FIELD(ams_mel_ir_az_el_v1,azimuth_rad); FIELD(ams_mel_ir_az_el_v1,elevation_rad));
    RECORD(ams_mel_ir_line_of_sight_report_v1, FIELD(ams_mel_ir_line_of_sight_report_v1,system_time_ns); FIELD(ams_mel_ir_line_of_sight_report_v1,pointing_angle); FIELD(ams_mel_ir_line_of_sight_report_v1,pointing_angle_rates); FIELD(ams_mel_ir_line_of_sight_report_v1,at_speed); FIELD(ams_mel_ir_line_of_sight_report_v1,in_tolerance); FIELD(ams_mel_ir_line_of_sight_report_v1,platform_attitude); FIELD(ams_mel_ir_line_of_sight_report_v1,validity_flag_bitfield); FIELD(ams_mel_ir_line_of_sight_report_v1,image_rotation_rad));
    RECORD(ams_mel_ir_line_of_sight_euler_v1, FIELD(ams_mel_ir_line_of_sight_euler_v1,system_time_ns); FIELD(ams_mel_ir_line_of_sight_euler_v1,attitude); FIELD(ams_mel_ir_line_of_sight_euler_v1,attitude_rates));
    RECORD(ams_mel_ir_navigation_response_v1, FIELD(ams_mel_ir_navigation_response_v1,system_time_ns); FIELD(ams_mel_ir_navigation_response_v1,command_id); FIELD(ams_mel_ir_navigation_response_v1,request_id));
    RECORD(ams_mel_ir_image_metadata_event_v1, FIELD(ams_mel_ir_image_metadata_event_v1,kind); FIELD(ams_mel_ir_image_metadata_event_v1,bad_pixel_list); FIELD(ams_mel_ir_image_metadata_event_v1,line_of_sight_report); FIELD(ams_mel_ir_image_metadata_event_v1,line_of_sight_euler); FIELD(ams_mel_ir_image_metadata_event_v1,navigation_response));
    RECORD(ams_mel_ir_channel_comms_test_report_v1, FIELD(ams_mel_ir_channel_comms_test_report_v1,command_id); FIELD(ams_mel_ir_channel_comms_test_report_v1,request_id));
    RECORD(ams_mel_ir_channel_comms_test_request_v1, FIELD(ams_mel_ir_channel_comms_test_request_v1,command_id); FIELD(ams_mel_ir_channel_comms_test_request_v1,channel_id); FIELD(ams_mel_ir_channel_comms_test_request_v1,request_id));
    RECORD(ams_mel_ir_channel_comms_test_result_v1, FIELD(ams_mel_ir_channel_comms_test_result_v1,command_id); FIELD(ams_mel_ir_channel_comms_test_result_v1,request_id); FIELD(ams_mel_ir_channel_comms_test_result_v1,error_code));
    RECORD(ams_mel_ir_band_info_v1, FIELD(ams_mel_ir_band_info_v1,type); FIELD(ams_mel_ir_band_info_v1,min_wavelength_m); FIELD(ams_mel_ir_band_info_v1,max_wavelength_m));
    RECORD(ams_mel_ir_band_info_span_v1, FIELD(ams_mel_ir_band_info_span_v1,data); FIELD(ams_mel_ir_band_info_span_v1,size));
    RECORD(ams_mel_ir_image_band_v1, FIELD(ams_mel_ir_image_band_v1,band_index); FIELD(ams_mel_ir_image_band_v1,bands));
    RECORD(ams_mel_ir_image_band_span_v1, FIELD(ams_mel_ir_image_band_span_v1,data); FIELD(ams_mel_ir_image_band_span_v1,size));
    RECORD(ams_mel_ir_channel_capability_v1, FIELD(ams_mel_ir_channel_capability_v1,channel_id); FIELD(ams_mel_ir_channel_capability_v1,height); FIELD(ams_mel_ir_channel_capability_v1,width); FIELD(ams_mel_ir_channel_capability_v1,bit_depth); FIELD(ams_mel_ir_channel_capability_v1,row_pitch); FIELD(ams_mel_ir_channel_capability_v1,buffer_size); FIELD(ams_mel_ir_channel_capability_v1,image_size); FIELD(ams_mel_ir_channel_capability_v1,number_of_bands); FIELD(ams_mel_ir_channel_capability_v1,pixel_format); FIELD(ams_mel_ir_channel_capability_v1,sensor_types); FIELD(ams_mel_ir_channel_capability_v1,platform_id); FIELD(ams_mel_ir_channel_capability_v1,sensor_location); FIELD(ams_mel_ir_channel_capability_v1,channel_types); FIELD(ams_mel_ir_channel_capability_v1,task_schedule_depth); FIELD(ams_mel_ir_channel_capability_v1,odc_available); FIELD(ams_mel_ir_channel_capability_v1,nuc_available); FIELD(ams_mel_ir_channel_capability_v1,metadata_capabilities); FIELD(ams_mel_ir_channel_capability_v1,image_bands); FIELD(ams_mel_ir_channel_capability_v1,nav_frames));
    RECORD(ams_mel_ir_health_config_v1, FIELD(ams_mel_ir_health_config_v1,channel_id); FIELD(ams_mel_ir_health_config_v1,channel_type); FIELD(ams_mel_ir_health_config_v1,platform_id); FIELD(ams_mel_ir_health_config_v1,sensor_location));
    RECORD(ams_mel_euler_v1, FIELD(ams_mel_euler_v1,roll); FIELD(ams_mel_euler_v1,pitch); FIELD(ams_mel_euler_v1,yaw));
    RECORD(ams_mel_foreign_key_v1, FIELD(ams_mel_foreign_key_v1,key); FIELD(ams_mel_foreign_key_v1,system_name));
    RECORD(ams_mel_installation_details_v1, FIELD(ams_mel_installation_details_v1,location); FIELD(ams_mel_installation_details_v1,orientation); FIELD(ams_mel_installation_details_v1,boresight));
    RECORD(ams_mel_temperature_status_v1, FIELD(ams_mel_temperature_status_v1,temperature_c); FIELD(ams_mel_temperature_status_v1,state));
    RECORD(ams_mel_mfa_component_v1, FIELD(ams_mel_mfa_component_v1,component_id); FIELD(ams_mel_mfa_component_v1,state); FIELD(ams_mel_mfa_component_v1,temperature); FIELD(ams_mel_mfa_component_v1,installation_location_id); FIELD(ams_mel_mfa_component_v1,installation_details));
    RECORD(ams_mel_mfa_component_span_v1, FIELD(ams_mel_mfa_component_span_v1,data); FIELD(ams_mel_mfa_component_span_v1,size));
    RECORD(ams_mel_about_v1, FIELD(ams_mel_about_v1,model); FIELD(ams_mel_about_v1,serial_number); FIELD(ams_mel_about_v1,software_version); FIELD(ams_mel_about_v1,bootloader_software_version); FIELD(ams_mel_about_v1,hardware_version));
    RECORD(ams_mel_mfa_status_v1, FIELD(ams_mel_mfa_status_v1,state); FIELD(ams_mel_mfa_status_v1,state_description); FIELD(ams_mel_mfa_status_v1,mode_description); FIELD(ams_mel_mfa_status_v1,transition_status); FIELD(ams_mel_mfa_status_v1,about); FIELD(ams_mel_mfa_status_v1,components));
    RECORD(ams_mel_ir_subsystem_dep_info_v1, FIELD(ams_mel_ir_subsystem_dep_info_v1,subsystem_id); FIELD(ams_mel_ir_subsystem_dep_info_v1,criticality); FIELD(ams_mel_ir_subsystem_dep_info_v1,failure));
    RECORD(ams_mel_ir_subsystem_dep_info_span_v1, FIELD(ams_mel_ir_subsystem_dep_info_span_v1,data); FIELD(ams_mel_ir_subsystem_dep_info_span_v1,size));
    RECORD(ams_mel_ir_version_v1, FIELD(ams_mel_ir_version_v1,source); FIELD(ams_mel_ir_version_v1,major_revision); FIELD(ams_mel_ir_version_v1,minor_revision); FIELD(ams_mel_ir_version_v1,engineering_revision));
    RECORD(ams_mel_ir_subsystem_csci_info_v1, FIELD(ams_mel_ir_subsystem_csci_info_v1,csci); FIELD(ams_mel_ir_subsystem_csci_info_v1,mode); FIELD(ams_mel_ir_subsystem_csci_info_v1,version); FIELD(ams_mel_ir_subsystem_csci_info_v1,criticality); FIELD(ams_mel_ir_subsystem_csci_info_v1,failure); FIELD(ams_mel_ir_subsystem_csci_info_v1,bit_report); FIELD(ams_mel_ir_subsystem_csci_info_v1,connection_established));
    RECORD(ams_mel_ir_subsystem_csci_info_span_v1, FIELD(ams_mel_ir_subsystem_csci_info_span_v1,data); FIELD(ams_mel_ir_subsystem_csci_info_span_v1,size));
    RECORD(ams_mel_ir_subsystem_status_v1, FIELD(ams_mel_ir_subsystem_status_v1,subsystem_id); FIELD(ams_mel_ir_subsystem_status_v1,criticality); FIELD(ams_mel_ir_subsystem_status_v1,status_sequence_number); FIELD(ams_mel_ir_subsystem_status_v1,failure); FIELD(ams_mel_ir_subsystem_status_v1,subsystem_count); FIELD(ams_mel_ir_subsystem_status_v1,subsystems); FIELD(ams_mel_ir_subsystem_status_v1,csci_count); FIELD(ams_mel_ir_subsystem_status_v1,csci));
    RECORD(ams_mel_name_value_pair_v1, FIELD(ams_mel_name_value_pair_v1,name); FIELD(ams_mel_name_value_pair_v1,value)); RECORD(ams_mel_name_value_pair_span_v1, FIELD(ams_mel_name_value_pair_span_v1,data); FIELD(ams_mel_name_value_pair_span_v1,size));
    RECORD(ams_mel_security_artifact_v1, FIELD(ams_mel_security_artifact_v1,component_id); FIELD(ams_mel_security_artifact_v1,associated_id)); RECORD(ams_mel_security_artifact_span_v1, FIELD(ams_mel_security_artifact_span_v1,data); FIELD(ams_mel_security_artifact_span_v1,size));
    RECORD(ams_mel_security_event_v1, FIELD(ams_mel_security_event_v1,kind); FIELD(ams_mel_security_event_v1,category); FIELD(ams_mel_security_event_v1,details); FIELD(ams_mel_security_event_v1,subsystem_id); FIELD(ams_mel_security_event_v1,service_id); FIELD(ams_mel_security_event_v1,mdf_id));
    RECORD(ams_mel_security_audit_record_v1, FIELD(ams_mel_security_audit_record_v1,security_event_id); FIELD(ams_mel_security_audit_record_v1,event_timestamp_ns); FIELD(ams_mel_security_audit_record_v1,subsystem_id); FIELD(ams_mel_security_audit_record_v1,artifacts); FIELD(ams_mel_security_audit_record_v1,event); FIELD(ams_mel_security_audit_record_v1,outcome); FIELD(ams_mel_security_audit_record_v1,severity));
    RECORD(ams_mel_ir_health_metadata_event_v1, FIELD(ams_mel_ir_health_metadata_event_v1,kind); FIELD(ams_mel_ir_health_metadata_event_v1,mfa_status); FIELD(ams_mel_ir_health_metadata_event_v1,bit_status); FIELD(ams_mel_ir_health_metadata_event_v1,subsystem_status); FIELD(ams_mel_ir_health_metadata_event_v1,discrete_status); FIELD(ams_mel_ir_health_metadata_event_v1,security_audit); FIELD(ams_mel_ir_health_metadata_event_v1,mfa_status_detailed));
    LAYOUT(ams_mel_ir_c2_metadata *); LAYOUT(ams_mel_ir_c2_metadata_event *);
    LAYOUT(ams_mel_ir_image_metadata *); LAYOUT(ams_mel_ir_image_metadata_event *);
    LAYOUT(ams_mel_ir_channel_comms_request *); LAYOUT(ams_mel_ir_channel_capability *);
    LAYOUT(ams_mel_ir_health *); LAYOUT(ams_mel_ir_health_metadata *); LAYOUT(ams_mel_ir_health_metadata_event *);
    LAYOUT(ams_mel_ir_navigation_request *);

    VALUE(AMS_MEL_POSITION_SOLUTION_NOT_SET); VALUE(AMS_MEL_POSITION_SOLUTION_ALIGNING);
    VALUE(AMS_MEL_POSITION_SOLUTION_FREE_INERTIAL); VALUE(AMS_MEL_POSITION_SOLUTION_GPS);
    VALUE(AMS_MEL_POSITION_SOLUTION_BLENDED); VALUE(AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE);
    RECORD(ams_mel_north_east_down_v1, FIELD(ams_mel_north_east_down_v1,north); FIELD(ams_mel_north_east_down_v1,east); FIELD(ams_mel_north_east_down_v1,down));
    RECORD(ams_mel_attitude_rate_v1, FIELD(ams_mel_attitude_rate_v1,attitude_rate); FIELD(ams_mel_attitude_rate_v1,attitude_rate_time_ns));
    RECORD(ams_mel_position_velocity_covariance_v1,
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pn_pn);
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pn_pe);
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pn_pd);
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pe_pe);
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pe_pd);
        FIELD(ams_mel_position_velocity_covariance_v1,position_position_pd_pd);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pn_vn);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pn_ve);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pn_vd);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pe_ve);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pe_vd);
        FIELD(ams_mel_position_velocity_covariance_v1,position_velocity_pd_vd);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_vn_vn);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_vn_ve);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_vn_vd);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_ve_ve);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_ve_vd);
        FIELD(ams_mel_position_velocity_covariance_v1,velocity_velocity_vd_vd));
    RECORD(ams_mel_navigation_report_v1,
        FIELD(ams_mel_navigation_report_v1,system_time_ns);
        FIELD(ams_mel_navigation_report_v1,state);
        FIELD(ams_mel_navigation_report_v1,latitude_rad);
        FIELD(ams_mel_navigation_report_v1,longitude_rad);
        FIELD(ams_mel_navigation_report_v1,altitude_m);
        FIELD(ams_mel_navigation_report_v1,attitude);
        FIELD(ams_mel_navigation_report_v1,attitude_rate);
        FIELD(ams_mel_navigation_report_v1,speed);
        FIELD(ams_mel_navigation_report_v1,acceleration);
        FIELD(ams_mel_navigation_report_v1,wander_angle_rad);
        FIELD(ams_mel_navigation_report_v1,magnetic_heading);
        FIELD(ams_mel_navigation_report_v1,altitude_msl);
        FIELD(ams_mel_navigation_report_v1,position_velocity_covariance_uncertainty));
    RECORD(ams_mel_ir_navigation_result_v1, FIELD(ams_mel_ir_navigation_result_v1,response); FIELD(ams_mel_ir_navigation_result_v1,error_code));
    LAYOUT(ams_mel_ir_instrumentation *); LAYOUT(ams_mel_ir_instrumentation_request *);
    LAYOUT(ams_mel_ir_instrumentation_metadata *);
    LAYOUT(ams_mel_ir_instrumentation_metadata_event *);
    VALUE(AMS_MEL_IR_PRIORITY_NORMAL); VALUE(AMS_MEL_IR_PRIORITY_DEBUG);
    VALUE(AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT);
    VALUE(AMS_MEL_IR_CHANNEL_INSTRUMENTATION);
    RECORD(ams_mel_ir_instrumentation_config_v1,
        FIELD(ams_mel_ir_instrumentation_config_v1,channel_id);
        FIELD(ams_mel_ir_instrumentation_config_v1,channel_type);
        FIELD(ams_mel_ir_instrumentation_config_v1,platform_id);
        FIELD(ams_mel_ir_instrumentation_config_v1,sensor_location));
    RECORD(ams_mel_ir_instrumentation_level_command_v1,
        FIELD(ams_mel_ir_instrumentation_level_command_v1,command_id);
        FIELD(ams_mel_ir_instrumentation_level_command_v1,priority));
    RECORD(ams_mel_ir_instrumentation_report_v1,
        FIELD(ams_mel_ir_instrumentation_report_v1,command_id);
        FIELD(ams_mel_ir_instrumentation_report_v1,size);
        FIELD(ams_mel_ir_instrumentation_report_v1,timestamp_ns);
        FIELD(ams_mel_ir_instrumentation_report_v1,priority));
    RECORD(ams_mel_ir_instrumentation_result_v1,
        FIELD(ams_mel_ir_instrumentation_result_v1,report);
        FIELD(ams_mel_ir_instrumentation_result_v1,error_code));
    RECORD(ams_mel_ir_instrumentation_metadata_event_v1,
        FIELD(ams_mel_ir_instrumentation_metadata_event_v1,kind);
        FIELD(ams_mel_ir_instrumentation_metadata_event_v1,report));

    LAYOUT(ams_mel_ir_track *);
    VALUE(AMS_MEL_IR_CHANNEL_IRST_TRACK);
    RECORD(ams_mel_ir_track_config_v1,
        FIELD(ams_mel_ir_track_config_v1,channel_id);
        FIELD(ams_mel_ir_track_config_v1,channel_type);
        FIELD(ams_mel_ir_track_config_v1,platform_id);
        FIELD(ams_mel_ir_track_config_v1,sensor_location));

    LAYOUT(ams_mel_ir_track_metadata *);
    LAYOUT(ams_mel_ir_track_metadata_event *);
    VALUE(AMS_MEL_IR_TRACK_STATE_IDLE); VALUE(AMS_MEL_IR_TRACK_STATE_DETECTED);
    VALUE(AMS_MEL_IR_TRACK_STATE_COAST); VALUE(AMS_MEL_IR_TRACK_STATE_DROPPED);
    VALUE(AMS_MEL_IR_TRACK_MODE_IDLE); VALUE(AMS_MEL_IR_TRACK_MODE_SCAN);
    VALUE(AMS_MEL_IR_TRACK_MODE_STARE);
    VALUE(AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    VALUE(AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    VALUE(AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
    VALUE(AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE);
    VALUE(AMS_MEL_IR_MAX_CANDIDATE_OBJECTS);
    VALUE(AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE);
    VALUE(AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES);
    VALUE(AMS_MEL_IR_HOT_REGION_INVALID); VALUE(AMS_MEL_IR_HOT_REGION_FLARE);
    VALUE(AMS_MEL_IR_HOT_REGION_SOLAR); VALUE(AMS_MEL_IR_HOT_REGION_MASK);
    RECORD(ams_mel_ir_row_col_v1,
        FIELD(ams_mel_ir_row_col_v1,row);
        FIELD(ams_mel_ir_row_col_v1,column));
    RECORD(ams_mel_ir_hot_region_v1,
        FIELD(ams_mel_ir_hot_region_v1,type);
        FIELD(ams_mel_ir_hot_region_v1,size);
        FIELD(ams_mel_ir_hot_region_v1,top);
        FIELD(ams_mel_ir_hot_region_v1,left);
        FIELD(ams_mel_ir_hot_region_v1,right);
        FIELD(ams_mel_ir_hot_region_v1,bottom));
    RECORD(ams_mel_ir_hot_region_span_v1,
        FIELD(ams_mel_ir_hot_region_span_v1,data);
        FIELD(ams_mel_ir_hot_region_span_v1,size));
    RECORD(ams_mel_ir_candidate_object_header_v1,
        FIELD(ams_mel_ir_candidate_object_header_v1,number_of_cos);
        FIELD(ams_mel_ir_candidate_object_header_v1,stack_frame_index);
        FIELD(ams_mel_ir_candidate_object_header_v1,cfar);
        FIELD(ams_mel_ir_candidate_object_header_v1,validity_flag_bitfield);
        FIELD(ams_mel_ir_candidate_object_header_v1,tov_utc_ns));
    RECORD(ams_mel_ir_candidate_object_v1,
        FIELD(ams_mel_ir_candidate_object_v1,system_time_ns);
        FIELD(ams_mel_ir_candidate_object_v1,detection_category);
        FIELD(ams_mel_ir_candidate_object_v1,sensor_index);
        FIELD(ams_mel_ir_candidate_object_v1,subpixel);
        FIELD(ams_mel_ir_candidate_object_v1,intensity);
        FIELD(ams_mel_ir_candidate_object_v1,sensor_relative_unit);
        FIELD(ams_mel_ir_candidate_object_v1,signal_to_interference_ratio);
        FIELD(ams_mel_ir_candidate_object_v1,signal_to_noise_ratio));
    RECORD(ams_mel_ir_candidate_object_span_v1,
        FIELD(ams_mel_ir_candidate_object_span_v1,data);
        FIELD(ams_mel_ir_candidate_object_span_v1,size));
    RECORD(ams_mel_ir_candidate_object_message_v1,
        FIELD(ams_mel_ir_candidate_object_message_v1,header);
        FIELD(ams_mel_ir_candidate_object_message_v1,inertial_state);
        FIELD(ams_mel_ir_candidate_object_message_v1,hot_regions);
        FIELD(ams_mel_ir_candidate_object_message_v1,candidate_objects));
    RECORD(ams_mel_ir_track_report_v1,
        FIELD(ams_mel_ir_track_report_v1,system_time_ns);
        FIELD(ams_mel_ir_track_report_v1,activity_id);
        FIELD(ams_mel_ir_track_report_v1,measured_ned);
        FIELD(ams_mel_ir_track_report_v1,measured_intensity);
        FIELD(ams_mel_ir_track_report_v1,measured_snr);
        FIELD(ams_mel_ir_track_report_v1,filtered_ned);
        FIELD(ams_mel_ir_track_report_v1,filtered_intensity);
        FIELD(ams_mel_ir_track_report_v1,filtered_snr);
        FIELD(ams_mel_ir_track_report_v1,range_m);
        FIELD(ams_mel_ir_track_report_v1,range_error_m);
        FIELD(ams_mel_ir_track_report_v1,spatial_extent_rad);
        FIELD(ams_mel_ir_track_report_v1,track_quality);
        FIELD(ams_mel_ir_track_report_v1,clutter);
        FIELD(ams_mel_ir_track_report_v1,age_ns);
        FIELD(ams_mel_ir_track_report_v1,state);
        FIELD(ams_mel_ir_track_report_v1,mode));
    RECORD(ams_mel_ir_request_system_track_data_v1,
        FIELD(ams_mel_ir_request_system_track_data_v1,system_time_ns);
        FIELD(ams_mel_ir_request_system_track_data_v1,command_id);
        FIELD(ams_mel_ir_request_system_track_data_v1,request_id);
        FIELD(ams_mel_ir_request_system_track_data_v1,track_id));
    /* Frozen v1: exactly kind, track_report, request_system_track_data. */
    RECORD(ams_mel_ir_track_metadata_event_v1,
        FIELD(ams_mel_ir_track_metadata_event_v1,kind);
        FIELD(ams_mel_ir_track_metadata_event_v1,track_report);
        FIELD(ams_mel_ir_track_metadata_event_v1,request_system_track_data));
    /* Frozen v2: exactly base and candidate_object_message. */
    RECORD(ams_mel_ir_track_metadata_event_v2,
        FIELD(ams_mel_ir_track_metadata_event_v2,base);
        FIELD(ams_mel_ir_track_metadata_event_v2,candidate_object_message));
    RECORD(ams_mel_ir_candidate_background_v1,
        FIELD(ams_mel_ir_candidate_background_v1,samples));
    RECORD(ams_mel_ir_candidate_object_preproc_v1,
        FIELD(ams_mel_ir_candidate_object_preproc_v1,system_time_ns);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,detection_category);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,sensor_index);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,subpixel);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,intensity);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,sensor_relative_unit);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,signal_to_interference_ratio);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,signal_to_noise_ratio);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,candidate_object_with_background);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,clutter);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,candidate_object_quality);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,sir_delta);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,inertial_state);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,edge);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,az_sigma);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,el_sigma);
        FIELD(ams_mel_ir_candidate_object_preproc_v1,background_normalizer));
    RECORD(ams_mel_ir_candidate_object_preproc_span_v1,
        FIELD(ams_mel_ir_candidate_object_preproc_span_v1,data);
        FIELD(ams_mel_ir_candidate_object_preproc_span_v1,size));
    RECORD(ams_mel_ir_candidate_object_preproc_message_v1,
        FIELD(ams_mel_ir_candidate_object_preproc_message_v1,header);
        FIELD(ams_mel_ir_candidate_object_preproc_message_v1,inertial_state);
        FIELD(ams_mel_ir_candidate_object_preproc_message_v1,hot_regions);
        FIELD(ams_mel_ir_candidate_object_preproc_message_v1,candidate_object_preprocs));
    RECORD(ams_mel_ir_track_metadata_event_v3,
        FIELD(ams_mel_ir_track_metadata_event_v3,base);
        FIELD(ams_mel_ir_track_metadata_event_v3,candidate_object_preproc_message));

    LAYOUT(ams_mel_ir_track_update_request *);
    VALUE(AMS_MEL_IR_TRACK_STATUS_CREATE); VALUE(AMS_MEL_IR_TRACK_STATUS_UPDATE);
    VALUE(AMS_MEL_IR_TRACK_STATUS_PREDICT); VALUE(AMS_MEL_IR_TRACK_STATUS_DELETE);
    RECORD(ams_mel_ir_track_covariance_v1,
        FIELD(ams_mel_ir_track_covariance_v1,xx);
        FIELD(ams_mel_ir_track_covariance_v1,xy);
        FIELD(ams_mel_ir_track_covariance_v1,xz);
        FIELD(ams_mel_ir_track_covariance_v1,x_vx);
        FIELD(ams_mel_ir_track_covariance_v1,x_vy);
        FIELD(ams_mel_ir_track_covariance_v1,x_vz);
        FIELD(ams_mel_ir_track_covariance_v1,yy);
        FIELD(ams_mel_ir_track_covariance_v1,yz);
        FIELD(ams_mel_ir_track_covariance_v1,y_vx);
        FIELD(ams_mel_ir_track_covariance_v1,y_vy);
        FIELD(ams_mel_ir_track_covariance_v1,y_vz);
        FIELD(ams_mel_ir_track_covariance_v1,zz);
        FIELD(ams_mel_ir_track_covariance_v1,z_vx);
        FIELD(ams_mel_ir_track_covariance_v1,z_vy);
        FIELD(ams_mel_ir_track_covariance_v1,z_vz);
        FIELD(ams_mel_ir_track_covariance_v1,vx_vx);
        FIELD(ams_mel_ir_track_covariance_v1,vx_vy);
        FIELD(ams_mel_ir_track_covariance_v1,vx_vz);
        FIELD(ams_mel_ir_track_covariance_v1,vy_vy);
        FIELD(ams_mel_ir_track_covariance_v1,vy_vz);
        FIELD(ams_mel_ir_track_covariance_v1,vz_vz));
    RECORD(ams_mel_ir_track_data_update_v1,
        FIELD(ams_mel_ir_track_data_update_v1,platform_id);
        FIELD(ams_mel_ir_track_data_update_v1,capability_uuid);
        FIELD(ams_mel_ir_track_data_update_v1,activity_uuid);
        FIELD(ams_mel_ir_track_data_update_v1,track_id);
        FIELD(ams_mel_ir_track_data_update_v1,entity_uuid);
        FIELD(ams_mel_ir_track_data_update_v1,track_status);
        FIELD(ams_mel_ir_track_data_update_v1,time_of_validity_seconds);
        FIELD(ams_mel_ir_track_data_update_v1,time_of_last_update_seconds);
        FIELD(ams_mel_ir_track_data_update_v1,track_position_ecef);
        FIELD(ams_mel_ir_track_data_update_v1,track_velocity_ecef);
        FIELD(ams_mel_ir_track_data_update_v1,covariance);
        FIELD(ams_mel_ir_track_data_update_v1,maneuver_probability);
        FIELD(ams_mel_ir_track_data_update_v1,track_quality));
    RECORD(ams_mel_ir_track_update_result_v1,
        FIELD(ams_mel_ir_track_update_result_v1,status);
        FIELD(ams_mel_ir_track_update_result_v1,error_code));

    LAYOUT(ams_mel_ir_track_system_response_request *);
    RECORD(ams_mel_ir_system_track_data_response_v1,
        FIELD(ams_mel_ir_system_track_data_response_v1,system_time_ns);
        FIELD(ams_mel_ir_system_track_data_response_v1,command_id);
        FIELD(ams_mel_ir_system_track_data_response_v1,request_id);
        FIELD(ams_mel_ir_system_track_data_response_v1,track_id);
        FIELD(ams_mel_ir_system_track_data_response_v1,range_m);
        FIELD(ams_mel_ir_system_track_data_response_v1,range_rate_mps);
        FIELD(ams_mel_ir_system_track_data_response_v1,range_error_m);
        FIELD(ams_mel_ir_system_track_data_response_v1,range_rate_error_mps);
        FIELD(ams_mel_ir_system_track_data_response_v1,az_el_valid);
        FIELD(ams_mel_ir_system_track_data_response_v1,range_valid);
        FIELD(ams_mel_ir_system_track_data_response_v1,inertial_az_el);
        FIELD(ams_mel_ir_system_track_data_response_v1,az_el_error));
    RECORD(ams_mel_ir_track_system_response_result_v1,
        FIELD(ams_mel_ir_track_system_response_result_v1,status);
        FIELD(ams_mel_ir_track_system_response_result_v1,error_code));

    /* Task 033B RF DataMEL. */
    LAYOUT(ams_mel_rf_admin *);
    LAYOUT(ams_mel_rf_c2 *);
    LAYOUT(ams_mel_rf_virtual_aperture_request *);
    LAYOUT(ams_mel_rf_virtual_aperture *);
    RECORD(ams_mel_rf_virtual_aperture_config_v1,
        FIELD(ams_mel_rf_virtual_aperture_config_v1,va_definition_id);
        FIELD(ams_mel_rf_virtual_aperture_config_v1,priority);
        FIELD(ams_mel_rf_virtual_aperture_config_v1,local_function_info);
        FIELD(ams_mel_rf_virtual_aperture_config_v1,va_definition_file_info);
        FIELD(ams_mel_rf_virtual_aperture_config_v1,capability_ids));
    RECORD(ams_mel_rf_virtual_aperture_result_v1,
        FIELD(ams_mel_rf_virtual_aperture_result_v1,error_code));
    RECORD(ams_mel_rf_virtual_aperture_info_v1,
        FIELD(ams_mel_rf_virtual_aperture_info_v1,va_instance_ids);
        FIELD(ams_mel_rf_virtual_aperture_info_v1,element_group_labels);
        FIELD(ams_mel_rf_virtual_aperture_info_v1,is_single_group));
    LAYOUT(ams_mel_rf_virtual_aperture_status_t);
    LAYOUT(ams_mel_rf_va_instance_list *);
    LAYOUT(ams_mel_rf_va_instance_status_report *);
    RECORD(ams_mel_rf_va_local_function_status_v1,
        FIELD(ams_mel_rf_va_local_function_status_v1,local_function_type_id);
        FIELD(ams_mel_rf_va_local_function_status_v1,statuses));
    RECORD(ams_mel_rf_va_local_function_status_span_v1,
        FIELD(ams_mel_rf_va_local_function_status_span_v1,data);
        FIELD(ams_mel_rf_va_local_function_status_span_v1,size));
    RECORD(ams_mel_rf_va_instance_status_report_v1,
        FIELD(ams_mel_rf_va_instance_status_report_v1,va_instance_id);
        FIELD(ams_mel_rf_va_instance_status_report_v1,status);
        FIELD(ams_mel_rf_va_instance_status_report_v1,local_functions));
    LAYOUT(ams_mel_rf_job_request *);
    LAYOUT(ams_mel_rf_job *);
    RECORD(ams_mel_u64_span_v1,
        FIELD(ams_mel_u64_span_v1,data);
        FIELD(ams_mel_u64_span_v1,size));
    RECORD(ams_mel_rf_rx_element_group_config_v1,
        FIELD(ams_mel_rf_rx_element_group_config_v1,label);
        FIELD(ams_mel_rf_rx_element_group_config_v1,desired_duty_factor);
        FIELD(ams_mel_rf_rx_element_group_config_v1,expected_center_frequencies);
        FIELD(ams_mel_rf_rx_element_group_config_v1,endpoint_ids);
        FIELD(ams_mel_rf_rx_element_group_config_v1,data_pipe_label));
    RECORD(ams_mel_rf_job_request_config_v1,
        FIELD(ams_mel_rf_job_request_config_v1,request_id);
        FIELD(ams_mel_rf_job_request_config_v1,priority);
        FIELD(ams_mel_rf_job_request_config_v1,precedence_within_priority);
        FIELD(ams_mel_rf_job_request_config_v1,is_interruptable);
        FIELD(ams_mel_rf_job_request_config_v1,instance_selection);
        FIELD(ams_mel_rf_job_request_config_v1,rx_group));
    RECORD(ams_mel_rf_utc_time_v1,
        FIELD(ams_mel_rf_utc_time_v1,seconds);
        FIELD(ams_mel_rf_utc_time_v1,fractional_femtoseconds));
    RECORD(ams_mel_rf_rx_data_pipe_endpoint_config_v1,
        FIELD(ams_mel_rf_rx_data_pipe_endpoint_config_v1,data_pipe_label);
        FIELD(ams_mel_rf_rx_data_pipe_endpoint_config_v1,endpoint_ids));
    RECORD(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1,
        FIELD(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1,data);
        FIELD(ams_mel_rf_rx_data_pipe_endpoint_config_span_v1,size));
    RECORD(ams_mel_rf_rx_element_group_config_v2,
        FIELD(ams_mel_rf_rx_element_group_config_v2,label);
        FIELD(ams_mel_rf_rx_element_group_config_v2,desired_duty_factor);
        FIELD(ams_mel_rf_rx_element_group_config_v2,expected_center_frequencies);
        FIELD(ams_mel_rf_rx_element_group_config_v2,data_pipe_endpoint_configs));
    RECORD(ams_mel_rf_rx_element_group_config_span_v2,
        FIELD(ams_mel_rf_rx_element_group_config_span_v2,data);
        FIELD(ams_mel_rf_rx_element_group_config_span_v2,size));
    RECORD(ams_mel_rf_job_request_config_v2,
        FIELD(ams_mel_rf_job_request_config_v2,request_id);
        FIELD(ams_mel_rf_job_request_config_v2,priority);
        FIELD(ams_mel_rf_job_request_config_v2,precedence_within_priority);
        FIELD(ams_mel_rf_job_request_config_v2,is_interruptable);
        FIELD(ams_mel_rf_job_request_config_v2,instance_selection);
        FIELD(ams_mel_rf_job_request_config_v2,rx_groups);
        FIELD(ams_mel_rf_job_request_config_v2,min_start_time);
        FIELD(ams_mel_rf_job_request_config_v2,max_complete_time);
        FIELD(ams_mel_rf_job_request_config_v2,duration_femtoseconds);
        FIELD(ams_mel_rf_job_request_config_v2,capability_id);
        FIELD(ams_mel_rf_job_request_config_v2,activity_id);
        FIELD(ams_mel_rf_job_request_config_v2,tx_power_mode_ids);
        FIELD(ams_mel_rf_job_request_config_v2,lookahead_femtoseconds));
    LAYOUT(ams_mel_rf_pointing_kind_t);
    VALUE(AMS_MEL_RF_POINTING_ECEF);
    VALUE(AMS_MEL_RF_POINTING_LLA);
    VALUE(AMS_MEL_RF_POINTING_PLATFORM_RELATIVE);
    VALUE(AMS_MEL_RF_POINTING_FACE_RELATIVE);
    VALUE(AMS_MEL_RF_POINTING_BASELINE_RELATIVE);
    RECORD(ams_mel_rf_vector3_v1,
        FIELD(ams_mel_rf_vector3_v1,x);
        FIELD(ams_mel_rf_vector3_v1,y);
        FIELD(ams_mel_rf_vector3_v1,z));
    RECORD(ams_mel_rf_az_el_v1,
        FIELD(ams_mel_rf_az_el_v1,azimuth_rad);
        FIELD(ams_mel_rf_az_el_v1,elevation_rad));
    RECORD(ams_mel_rf_ecef_pointing_v1,
        FIELD(ams_mel_rf_ecef_pointing_v1,location_m);
        FIELD(ams_mel_rf_ecef_pointing_v1,velocity_mps);
        FIELD(ams_mel_rf_ecef_pointing_v1,time_of_validity));
    RECORD(ams_mel_rf_lla_pointing_v1,
        FIELD(ams_mel_rf_lla_pointing_v1,latitude_rad);
        FIELD(ams_mel_rf_lla_pointing_v1,longitude_rad);
        FIELD(ams_mel_rf_lla_pointing_v1,altitude_m);
        FIELD(ams_mel_rf_lla_pointing_v1,velocity_north_mps);
        FIELD(ams_mel_rf_lla_pointing_v1,velocity_east_mps);
        FIELD(ams_mel_rf_lla_pointing_v1,velocity_down_mps);
        FIELD(ams_mel_rf_lla_pointing_v1,time_of_validity));
    RECORD(ams_mel_rf_pointing_v1,
        FIELD(ams_mel_rf_pointing_v1,kind);
        FIELD(ams_mel_rf_pointing_v1,ecef);
        FIELD(ams_mel_rf_pointing_v1,lla);
        FIELD(ams_mel_rf_pointing_v1,platform_relative);
        FIELD(ams_mel_rf_pointing_v1,face_relative);
        FIELD(ams_mel_rf_pointing_v1,baseline_relative_conic_rad));
    RECORD(ams_mel_rf_pointing_span_v1,
        FIELD(ams_mel_rf_pointing_span_v1,data);
        FIELD(ams_mel_rf_pointing_span_v1,size));
    RECORD(ams_mel_rf_rx_element_group_config_v3,
        FIELD(ams_mel_rf_rx_element_group_config_v3,group);
        FIELD(ams_mel_rf_rx_element_group_config_v3,expected_pointing_angles));
    RECORD(ams_mel_rf_rx_element_group_config_span_v3,
        FIELD(ams_mel_rf_rx_element_group_config_span_v3,data);
        FIELD(ams_mel_rf_rx_element_group_config_span_v3,size));
    RECORD(ams_mel_rf_job_request_config_v3,
        FIELD(ams_mel_rf_job_request_config_v3,request_id);
        FIELD(ams_mel_rf_job_request_config_v3,priority);
        FIELD(ams_mel_rf_job_request_config_v3,precedence_within_priority);
        FIELD(ams_mel_rf_job_request_config_v3,is_interruptable);
        FIELD(ams_mel_rf_job_request_config_v3,instance_selection);
        FIELD(ams_mel_rf_job_request_config_v3,rx_groups);
        FIELD(ams_mel_rf_job_request_config_v3,min_start_time);
        FIELD(ams_mel_rf_job_request_config_v3,max_complete_time);
        FIELD(ams_mel_rf_job_request_config_v3,duration_femtoseconds);
        FIELD(ams_mel_rf_job_request_config_v3,capability_id);
        FIELD(ams_mel_rf_job_request_config_v3,activity_id);
        FIELD(ams_mel_rf_job_request_config_v3,tx_power_mode_ids);
        FIELD(ams_mel_rf_job_request_config_v3,lookahead_femtoseconds);
        FIELD(ams_mel_rf_job_request_config_v3,has_estimated_stab_point);
        FIELD(ams_mel_rf_job_request_config_v3,estimated_stab_point));
    RECORD(ams_mel_rf_tx_element_group_config_v1,
        FIELD(ams_mel_rf_tx_element_group_config_v1,label);
        FIELD(ams_mel_rf_tx_element_group_config_v1,tx_power_level);
        FIELD(ams_mel_rf_tx_element_group_config_v1,desired_duty_factor);
        FIELD(ams_mel_rf_tx_element_group_config_v1,expected_center_frequencies));
    RECORD(ams_mel_rf_job_element_group_config_v4,
        FIELD(ams_mel_rf_job_element_group_config_v4,mode);
        FIELD(ams_mel_rf_job_element_group_config_v4,rx);
        FIELD(ams_mel_rf_job_element_group_config_v4,tx));
    RECORD(ams_mel_rf_job_element_group_config_span_v4,
        FIELD(ams_mel_rf_job_element_group_config_span_v4,data);
        FIELD(ams_mel_rf_job_element_group_config_span_v4,size));
    RECORD(ams_mel_rf_job_request_config_v4,
        FIELD(ams_mel_rf_job_request_config_v4,request_id);
        FIELD(ams_mel_rf_job_request_config_v4,priority);
        FIELD(ams_mel_rf_job_request_config_v4,precedence_within_priority);
        FIELD(ams_mel_rf_job_request_config_v4,is_interruptable);
        FIELD(ams_mel_rf_job_request_config_v4,instance_selection);
        FIELD(ams_mel_rf_job_request_config_v4,element_groups);
        FIELD(ams_mel_rf_job_request_config_v4,min_start_time);
        FIELD(ams_mel_rf_job_request_config_v4,max_complete_time);
        FIELD(ams_mel_rf_job_request_config_v4,duration_femtoseconds);
        FIELD(ams_mel_rf_job_request_config_v4,capability_id);
        FIELD(ams_mel_rf_job_request_config_v4,activity_id);
        FIELD(ams_mel_rf_job_request_config_v4,tx_power_mode_ids);
        FIELD(ams_mel_rf_job_request_config_v4,lookahead_femtoseconds);
        FIELD(ams_mel_rf_job_request_config_v4,has_estimated_stab_point);
        FIELD(ams_mel_rf_job_request_config_v4,estimated_stab_point));
    RECORD(ams_mel_rf_job_result_v1,
        FIELD(ams_mel_rf_job_result_v1,error_code));
    RECORD(ams_mel_rf_job_info_v1,
        FIELD(ams_mel_rf_job_info_v1,actual_start_seconds);
        FIELD(ams_mel_rf_job_info_v1,actual_start_femtoseconds);
        FIELD(ams_mel_rf_job_info_v1,total_job_duration_femtoseconds);
        FIELD(ams_mel_rf_job_info_v1,va_instance_id);
        FIELD(ams_mel_rf_job_info_v1,va_definition_id);
        FIELD(ams_mel_rf_job_info_v1,job_details_id);
        FIELD(ams_mel_rf_job_info_v1,job_request_id);
        FIELD(ams_mel_rf_job_info_v1,lookahead_femtoseconds);
        FIELD(ams_mel_rf_job_info_v1,rx_stream_ids));
    LAYOUT(ams_mel_rf_job_status_t);
    LAYOUT(ams_mel_rf_cancel_error_t);
    VALUE(AMS_MEL_RF_JOB_STATUS_NONE);
    VALUE(AMS_MEL_RF_JOB_STATUS_IN_PROGRESS);
    VALUE(AMS_MEL_RF_JOB_STATUS_COMPLETE);
    VALUE(AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_ID);
    VALUE(AMS_MEL_RF_JOB_STATUS_FAILED_INTERRUPTED);
    VALUE(AMS_MEL_RF_JOB_STATUS_FAILED_INVALID_STATE);
    VALUE(AMS_MEL_RF_CANCEL_ERROR_NONE);
    VALUE(AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS);
    RECORD(ams_mel_rf_receive_event_config_v1,
        FIELD(ams_mel_rf_receive_event_config_v1,event_id);
        FIELD(ams_mel_rf_receive_event_config_v1,element_group_label);
        FIELD(ams_mel_rf_receive_event_config_v1,start_femtoseconds);
        FIELD(ams_mel_rf_receive_event_config_v1,duration_femtoseconds);
        FIELD(ams_mel_rf_receive_event_config_v1,center_frequency_hz);
        FIELD(ams_mel_rf_receive_event_config_v1,sample_frequency_hz);
        FIELD(ams_mel_rf_receive_event_config_v1,agc_processing_iterations);
        FIELD(ams_mel_rf_receive_event_config_v1,ignored_post_agc_iterations);
        FIELD(ams_mel_rf_receive_event_config_v1,max_extension_femtoseconds));
    RECORD(ams_mel_rf_receive_event_config_span_v1,
        FIELD(ams_mel_rf_receive_event_config_span_v1,data);
        FIELD(ams_mel_rf_receive_event_config_span_v1,size));
    RECORD(ams_mel_rf_job_interval_config_v1,
        FIELD(ams_mel_rf_job_interval_config_v1,interval_start_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v1,interval_id);
        FIELD(ams_mel_rf_job_interval_config_v1,interval_starting_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v1,sequence_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v1,sequence_repeat_count);
        FIELD(ams_mel_rf_job_interval_config_v1,calibration_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v1,interval_ending_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v1,phase_coherence_with_prior);
        FIELD(ams_mel_rf_job_interval_config_v1,iterations_per_signal);
        FIELD(ams_mel_rf_job_interval_config_v1,max_data_rate_bps);
        FIELD(ams_mel_rf_job_interval_config_v1,max_sample_rate_hz);
        FIELD(ams_mel_rf_job_interval_config_v1,job_details_id);
        FIELD(ams_mel_rf_job_interval_config_v1,receive_events));
    RECORD(ams_mel_rf_job_interval_config_span_v1,
        FIELD(ams_mel_rf_job_interval_config_span_v1,data);
        FIELD(ams_mel_rf_job_interval_config_span_v1,size));
    {
        ams_mel_status_t (*add)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v1, char *, size_t, size_t *) = ams_mel_rf_job_add_rx_intervals;
        ams_mel_status_t (*flush)(ams_mel_rf_job *, char *, size_t, size_t *) = ams_mel_rf_job_flush;
        ams_mel_status_t (*remaining)(ams_mel_rf_job *, char *, size_t, size_t *) = ams_mel_rf_job_cancel_remaining_intervals;
        ams_mel_status_t (*extend)(ams_mel_rf_job *, uint32_t, uint32_t, int64_t, char *, size_t, size_t *) = ams_mel_rf_job_extend_event;
        (void)add; (void)flush; (void)remaining; (void)extend;
    }
    RECORD(ams_mel_rf_job_cancel_result_v1,
        FIELD(ams_mel_rf_job_cancel_result_v1,cancelled);
        FIELD(ams_mel_rf_job_cancel_result_v1,error_code));
    LAYOUT(ams_mel_rf_mfa_state_t);
    LAYOUT(ams_mel_rf_data *);
    LAYOUT(ams_mel_rf_mfa_info *);
    LAYOUT(ams_mel_rf_job_data_format_t);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT8);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_DIRECT_INT16);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_SMALL);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_MEDIUM);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_LARGE);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_AMS_VITA_EXTRA_LARGE);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE1);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE2);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_PDW_TYPE3);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE1);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE2);
    VALUE(AMS_MEL_RF_JOB_DATA_FORMAT_LF_TYPE3);
    RECORD(ams_mel_rf_frequency_range_v1,
        FIELD(ams_mel_rf_frequency_range_v1,min_hz);
        FIELD(ams_mel_rf_frequency_range_v1,max_hz));
    RECORD(ams_mel_rf_frequency_range_span_v1,
        FIELD(ams_mel_rf_frequency_range_span_v1,data);
        FIELD(ams_mel_rf_frequency_range_span_v1,size));
    RECORD(ams_mel_rf_face_info_v1,
        FIELD(ams_mel_rf_face_info_v1,face_id);
        FIELD(ams_mel_rf_face_info_v1,supports_receive);
        FIELD(ams_mel_rf_face_info_v1,supports_transmit);
        FIELD(ams_mel_rf_face_info_v1,requires_endpoint_association);
        FIELD(ams_mel_rf_face_info_v1,agc_processing_time_fs);
        FIELD(ams_mel_rf_face_info_v1,min_job_request_lead_time_fs);
        FIELD(ams_mel_rf_face_info_v1,max_job_request_lead_time_fs);
        FIELD(ams_mel_rf_face_info_v1,min_job_detail_lead_time_fs);
        FIELD(ams_mel_rf_face_info_v1,tx_rx_switching_time_fs);
        FIELD(ams_mel_rf_face_info_v1,rx_tx_switching_time_fs);
        FIELD(ams_mel_rf_face_info_v1,tx_tx_switching_time_fs);
        FIELD(ams_mel_rf_face_info_v1,rx_rx_switching_time_fs);
        FIELD(ams_mel_rf_face_info_v1,rx_frequency_ranges);
        FIELD(ams_mel_rf_face_info_v1,tx_frequency_ranges);
        FIELD(ams_mel_rf_face_info_v1,sample_frequency_ranges));
    RECORD(ams_mel_rf_face_info_span_v1,
        FIELD(ams_mel_rf_face_info_span_v1,data);
        FIELD(ams_mel_rf_face_info_span_v1,size));
    RECORD(ams_mel_rf_euler_v1,
        FIELD(ams_mel_rf_euler_v1,roll_rad);
        FIELD(ams_mel_rf_euler_v1,pitch_rad);
        FIELD(ams_mel_rf_euler_v1,yaw_rad));
    RECORD(ams_mel_rf_component_location_v1,
        FIELD(ams_mel_rf_component_location_v1,offset_x_m);
        FIELD(ams_mel_rf_component_location_v1,offset_y_m);
        FIELD(ams_mel_rf_component_location_v1,offset_z_m);
        FIELD(ams_mel_rf_component_location_v1,key);
        FIELD(ams_mel_rf_component_location_v1,system_name));
    RECORD(ams_mel_rf_physical_data_v1,
        FIELD(ams_mel_rf_physical_data_v1,antenna_height_m);
        FIELD(ams_mel_rf_physical_data_v1,antenna_width_m);
        FIELD(ams_mel_rf_physical_data_v1,lattice_angle_rad);
        FIELD(ams_mel_rf_physical_data_v1,location);
        FIELD(ams_mel_rf_physical_data_v1,orientation);
        FIELD(ams_mel_rf_physical_data_v1,boresight));
    RECORD(ams_mel_rf_tx_power_mode_v1,
        FIELD(ams_mel_rf_tx_power_mode_v1,tx_power_mode_id);
        FIELD(ams_mel_rf_tx_power_mode_v1,is_linear_operation);
        FIELD(ams_mel_rf_tx_power_mode_v1,tx_power_level);
        FIELD(ams_mel_rf_tx_power_mode_v1,tx_frequency_ranges);
        FIELD(ams_mel_rf_tx_power_mode_v1,max_tx_duty_factor);
        FIELD(ams_mel_rf_tx_power_mode_v1,max_tx_pulse_width_ns);
        FIELD(ams_mel_rf_tx_power_mode_v1,max_tx_atten);
        FIELD(ams_mel_rf_tx_power_mode_v1,tx_atten_step_size));
    RECORD(ams_mel_rf_tx_power_mode_span_v1,
        FIELD(ams_mel_rf_tx_power_mode_span_v1,data);
        FIELD(ams_mel_rf_tx_power_mode_span_v1,size));
    {
        ams_mel_status_t (*all)(const ams_mel_rf_data *, uint32_t, ams_mel_rf_tx_power_mode_snapshot **, char *, size_t, size_t *) = ams_mel_rf_data_get_tx_power_modes;
        ams_mel_status_t (*direct)(const ams_mel_rf_data *, uint32_t, uint32_t, ams_mel_rf_tx_power_mode_snapshot **, char *, size_t, size_t *) = ams_mel_rf_data_get_tx_power_mode;
        ams_mel_status_t (*view)(const ams_mel_rf_tx_power_mode_snapshot *, ams_mel_rf_tx_power_mode_span_v1 *, char *, size_t, size_t *) = ams_mel_rf_tx_power_mode_snapshot_view;
        ams_mel_status_t (*close_snapshot)(ams_mel_rf_tx_power_mode_snapshot **, char *, size_t, size_t *) = ams_mel_rf_tx_power_mode_snapshot_close;
        (void)all; (void)direct; (void)view; (void)close_snapshot;
    }
    RECORD(ams_mel_rf_mfa_info_v1,
        FIELD(ams_mel_rf_mfa_info_v1,reported_num_faces);
        FIELD(ams_mel_rf_mfa_info_v1,contains_open_additions);
        FIELD(ams_mel_rf_mfa_info_v1,scheduler_resolution_fs);
        FIELD(ams_mel_rf_mfa_info_v1,max_user_defined_context_bytes);
        FIELD(ams_mel_rf_mfa_info_v1,supported_data_formats);
        FIELD(ams_mel_rf_mfa_info_v1,faces));
    {
        ams_mel_status_t (*admin_open)(const char *, const char *, ams_mel_rf_admin **, char *, size_t, size_t *) = ams_mel_rf_admin_open;
        ams_mel_status_t (*admin_command)(ams_mel_rf_admin *, ams_mel_rf_mfa_state_t, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_admin_command_state;
        ams_mel_status_t (*admin_close)(ams_mel_rf_admin **, char *, size_t, size_t *) = ams_mel_rf_admin_close;
        (void)admin_open; (void)admin_command; (void)admin_close;
        ams_mel_status_t (*rf_open)(const char *, const char *, ams_mel_rf_data **, char *, size_t, size_t *) = ams_mel_rf_data_open;
        ams_mel_status_t (*rf_version)(const ams_mel_rf_data *, ams_mel_provider_version_v1 *, char *, size_t, size_t *) = ams_mel_rf_data_get_provider_version;
        ams_mel_status_t (*rf_mfa)(const ams_mel_rf_data *, ams_mel_rf_mfa_info **, char *, size_t, size_t *) = ams_mel_rf_data_get_mfa_info;
        ams_mel_status_t (*rf_view)(const ams_mel_rf_mfa_info *, const ams_mel_rf_mfa_info_v1 **, char *, size_t, size_t *) = ams_mel_rf_mfa_info_view;
        ams_mel_status_t (*rf_info_close)(ams_mel_rf_mfa_info **, char *, size_t, size_t *) = ams_mel_rf_mfa_info_close;
        ams_mel_status_t (*rf_close)(ams_mel_rf_data **, char *, size_t, size_t *) = ams_mel_rf_data_close;
        (void)rf_open; (void)rf_version; (void)rf_mfa; (void)rf_view; (void)rf_info_close; (void)rf_close;
    }

    /* Task 033D RF ProductRxEndpoint ComplexINT16 receive. */
    LAYOUT(ams_mel_rf_product_rx_request *);
    LAYOUT(ams_mel_rf_product_rx *);
    LAYOUT(ams_mel_rf_product_rx_event *);
    RECORD(ams_mel_rf_product_rx_config_v1,
        FIELD(ams_mel_rf_product_rx_config_v1,data_format);
        FIELD(ams_mel_rf_product_rx_config_v1,region_size_bytes);
        FIELD(ams_mel_rf_product_rx_config_v1,queue_capacity);
        FIELD(ams_mel_rf_product_rx_config_v1,max_samples_per_event));
    RECORD(ams_mel_rf_complex_i16_v1,
        FIELD(ams_mel_rf_complex_i16_v1,real);
        FIELD(ams_mel_rf_complex_i16_v1,imag));
    RECORD(ams_mel_rf_complex_i16_span_v1,
        FIELD(ams_mel_rf_complex_i16_span_v1,data);
        FIELD(ams_mel_rf_complex_i16_span_v1,size));
    RECORD(ams_mel_rf_product_rx_metadata_v1,
        FIELD(ams_mel_rf_product_rx_metadata_v1,mel_protocol_version_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,va_definition_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,va_instance_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,job_details_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,job_interval_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,lf_type_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,lf_instance_id);
        FIELD(ams_mel_rf_product_rx_metadata_v1,phase_coherence_with_prior);
        FIELD(ams_mel_rf_product_rx_metadata_v1,first_rx_event_start_s);
        FIELD(ams_mel_rf_product_rx_metadata_v1,first_rx_event_start_fs);
        FIELD(ams_mel_rf_product_rx_metadata_v1,rx_stream_ids));
    RECORD(ams_mel_rf_product_rx_event_v1,
        FIELD(ams_mel_rf_product_rx_event_v1,endpoint_id);
        FIELD(ams_mel_rf_product_rx_event_v1,data_format);
        FIELD(ams_mel_rf_product_rx_event_v1,samples);
        FIELD(ams_mel_rf_product_rx_event_v1,metadata));
    RECORD(ams_mel_rf_product_rx_info_v1,
        FIELD(ams_mel_rf_product_rx_info_v1,endpoint_id);
        FIELD(ams_mel_rf_product_rx_info_v1,assigned_data_format));
    RECORD(ams_mel_rf_product_rx_request_result_v1,
        FIELD(ams_mel_rf_product_rx_request_result_v1,error_code));
    RECORD(ams_mel_rf_product_rx_counters_v1,
        FIELD(ams_mel_rf_product_rx_counters_v1,callbacks_received);
        FIELD(ams_mel_rf_product_rx_counters_v1,products_queued);
        FIELD(ams_mel_rf_product_rx_counters_v1,products_dropped_queue_full);
        FIELD(ams_mel_rf_product_rx_counters_v1,malformed_or_unsupported);
        FIELD(ams_mel_rf_product_rx_counters_v1,allocation_failures);
        FIELD(ams_mel_rf_product_rx_counters_v1,callbacks_after_close));
    {
        ams_mel_status_t (*rx_submit)(ams_mel_rf_data *, const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **, char *, size_t, size_t *) = ams_mel_rf_data_submit_product_rx;
        ams_mel_status_t (*rx_wait)(const ams_mel_rf_product_rx_request *, uint32_t, ams_mel_rf_product_rx_request_result_v1 *, char *, size_t, size_t *) = ams_mel_rf_product_rx_request_wait;
        ams_mel_status_t (*rx_claim)(ams_mel_rf_product_rx_request *, ams_mel_rf_product_rx **, ams_mel_rf_product_rx_info_v1 *, char *, size_t, size_t *) = ams_mel_rf_product_rx_request_claim;
        ams_mel_status_t (*rx_request_close)(ams_mel_rf_product_rx_request **, char *, size_t, size_t *) = ams_mel_rf_product_rx_request_close;
        ams_mel_status_t (*rx_receive)(ams_mel_rf_product_rx *, uint32_t, ams_mel_rf_product_rx_event **, char *, size_t, size_t *) = ams_mel_rf_product_rx_receive;
        ams_mel_status_t (*rx_counters)(const ams_mel_rf_product_rx *, ams_mel_rf_product_rx_counters_v1 *, char *, size_t, size_t *) = ams_mel_rf_product_rx_get_counters;
        ams_mel_status_t (*rx_close)(ams_mel_rf_product_rx **, char *, size_t, size_t *) = ams_mel_rf_product_rx_close;
        ams_mel_status_t (*rx_view)(const ams_mel_rf_product_rx_event *, const ams_mel_rf_product_rx_event_v1 **, char *, size_t, size_t *) = ams_mel_rf_product_rx_event_view;
        ams_mel_status_t (*rx_event_close)(ams_mel_rf_product_rx_event **, char *, size_t, size_t *) = ams_mel_rf_product_rx_event_close;
        (void)rx_submit; (void)rx_wait; (void)rx_claim; (void)rx_request_close; (void)rx_receive;
        (void)rx_counters; (void)rx_close; (void)rx_view; (void)rx_event_close;
    }

    {
        ams_mel_status_t (*get)(const ams_mel_rf_data *, uint32_t, ams_mel_rf_physical_data **, char *, size_t, size_t *) = ams_mel_rf_data_get_physical_data;
        ams_mel_status_t (*view)(const ams_mel_rf_physical_data *, const ams_mel_rf_physical_data_v1 **, char *, size_t, size_t *) = ams_mel_rf_physical_data_view;
        ams_mel_status_t (*close_snapshot)(ams_mel_rf_physical_data **, char *, size_t, size_t *) = ams_mel_rf_physical_data_close;
        (void)get; (void)view; (void)close_snapshot;
    }
    RECORD(ams_mel_rf_job_interval_config_v2,
        FIELD(ams_mel_rf_job_interval_config_v2,interval);
        FIELD(ams_mel_rf_job_interval_config_v2,status_enable));
    RECORD(ams_mel_rf_job_interval_config_span_v2,
        FIELD(ams_mel_rf_job_interval_config_span_v2,data);
        FIELD(ams_mel_rf_job_interval_config_span_v2,size));
    RECORD(ams_mel_rf_receive_event_config_v2,
        FIELD(ams_mel_rf_receive_event_config_v2,event);
        FIELD(ams_mel_rf_receive_event_config_v2,stab_point_index);
        FIELD(ams_mel_rf_receive_event_config_v2,applicable_rx_element_groups));
    RECORD(ams_mel_rf_receive_event_config_span_v2,
        FIELD(ams_mel_rf_receive_event_config_span_v2,data);
        FIELD(ams_mel_rf_receive_event_config_span_v2,size));
    RECORD(ams_mel_rf_job_interval_config_v3,
        FIELD(ams_mel_rf_job_interval_config_v3,interval_start_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v3,interval_id);
        FIELD(ams_mel_rf_job_interval_config_v3,interval_starting_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v3,sequence_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v3,sequence_repeat_count);
        FIELD(ams_mel_rf_job_interval_config_v3,calibration_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v3,interval_ending_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v3,phase_coherence_with_prior);
        FIELD(ams_mel_rf_job_interval_config_v3,iterations_per_signal);
        FIELD(ams_mel_rf_job_interval_config_v3,max_data_rate_bps);
        FIELD(ams_mel_rf_job_interval_config_v3,max_sample_rate_hz);
        FIELD(ams_mel_rf_job_interval_config_v3,job_details_id);
        FIELD(ams_mel_rf_job_interval_config_v3,status_enable);
        FIELD(ams_mel_rf_job_interval_config_v3,stab_points);
        FIELD(ams_mel_rf_job_interval_config_v3,receive_events));
    RECORD(ams_mel_rf_job_interval_config_span_v3,
        FIELD(ams_mel_rf_job_interval_config_span_v3,data);
        FIELD(ams_mel_rf_job_interval_config_span_v3,size));
    { ams_mel_status_t (*add_v3)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v3, char *, size_t, size_t *) = ams_mel_rf_job_add_rx_intervals_v3; (void)add_v3; }

    RECORD(ams_mel_rf_stokes_vector_v1,
        FIELD(ams_mel_rf_stokes_vector_v1,s0);
        FIELD(ams_mel_rf_stokes_vector_v1,s1);
        FIELD(ams_mel_rf_stokes_vector_v1,s2);
        FIELD(ams_mel_rf_stokes_vector_v1,s3));
    RECORD(ams_mel_rf_stokes_vector_span_v1,
        FIELD(ams_mel_rf_stokes_vector_span_v1,data);
        FIELD(ams_mel_rf_stokes_vector_span_v1,size));
    RECORD(ams_mel_rf_receive_event_config_v3,
        FIELD(ams_mel_rf_receive_event_config_v3,event);
        FIELD(ams_mel_rf_receive_event_config_v3,polarization);
        FIELD(ams_mel_rf_receive_event_config_v3,polarization_beam_steer_correction);
        FIELD(ams_mel_rf_receive_event_config_v3,phase_offset_rad);
        FIELD(ams_mel_rf_receive_event_config_v3,execution_type);
        FIELD(ams_mel_rf_receive_event_config_v3,termination_type);
        FIELD(ams_mel_rf_receive_event_config_v3,allow_delay_start);
        FIELD(ams_mel_rf_receive_event_config_v3,iteration_hold_count);
        FIELD(ams_mel_rf_receive_event_config_v3,iteration_termination_count);
        FIELD(ams_mel_rf_receive_event_config_v3,channelization_enabled));
    RECORD(ams_mel_rf_receive_event_config_span_v3,
        FIELD(ams_mel_rf_receive_event_config_span_v3,data);
        FIELD(ams_mel_rf_receive_event_config_span_v3,size));
    RECORD(ams_mel_rf_job_interval_config_v4,
        FIELD(ams_mel_rf_job_interval_config_v4,interval_start_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v4,interval_id);
        FIELD(ams_mel_rf_job_interval_config_v4,interval_starting_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v4,sequence_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v4,sequence_repeat_count);
        FIELD(ams_mel_rf_job_interval_config_v4,calibration_duration_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v4,interval_ending_gap_femtoseconds);
        FIELD(ams_mel_rf_job_interval_config_v4,phase_coherence_with_prior);
        FIELD(ams_mel_rf_job_interval_config_v4,iterations_per_signal);
        FIELD(ams_mel_rf_job_interval_config_v4,max_data_rate_bps);
        FIELD(ams_mel_rf_job_interval_config_v4,max_sample_rate_hz);
        FIELD(ams_mel_rf_job_interval_config_v4,job_details_id);
        FIELD(ams_mel_rf_job_interval_config_v4,status_enable);
        FIELD(ams_mel_rf_job_interval_config_v4,stab_points);
        FIELD(ams_mel_rf_job_interval_config_v4,receive_events);
        FIELD(ams_mel_rf_job_interval_config_v4,tx_power_mode_id);
        FIELD(ams_mel_rf_job_interval_config_v4,activity_id);
        FIELD(ams_mel_rf_job_interval_config_v4,execution_type));
    RECORD(ams_mel_rf_job_interval_config_span_v4,
        FIELD(ams_mel_rf_job_interval_config_span_v4,data);
        FIELD(ams_mel_rf_job_interval_config_span_v4,size));
    { ams_mel_status_t (*add_v4)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v4, char *, size_t, size_t *) = ams_mel_rf_job_add_rx_intervals_v4; (void)add_v4; }
    RECORD(ams_mel_rf_job_event_log_entry_v1,
        FIELD(ams_mel_rf_job_event_log_entry_v1,event_id);
        FIELD(ams_mel_rf_job_event_log_entry_v1,trigger);
        FIELD(ams_mel_rf_job_event_log_entry_v1,time_seconds);
        FIELD(ams_mel_rf_job_event_log_entry_v1,time_fractional_femtoseconds));
    RECORD(ams_mel_rf_job_event_log_span_v1,
        FIELD(ams_mel_rf_job_event_log_span_v1,data);
        FIELD(ams_mel_rf_job_event_log_span_v1,size));
    RECORD(ams_mel_rf_job_interval_status_v1,
        FIELD(ams_mel_rf_job_interval_status_v1,interval_id);
        FIELD(ams_mel_rf_job_interval_status_v1,completion_status);
        FIELD(ams_mel_rf_job_interval_status_v1,event_log);
        FIELD(ams_mel_rf_job_interval_status_v1,activity_id));
    RECORD(ams_mel_rf_job_interval_status_options_v1,
        FIELD(ams_mel_rf_job_interval_status_options_v1,queue_capacity);
        FIELD(ams_mel_rf_job_interval_status_options_v1,max_event_log_entries);
        FIELD(ams_mel_rf_job_interval_status_options_v1,max_activity_id_bytes));
    RECORD(ams_mel_rf_job_interval_status_counters_v1,
        FIELD(ams_mel_rf_job_interval_status_counters_v1,callback_entries);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,events_queued);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,events_delivered);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,queue_full_drops);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,malformed_drops);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,oversize_drops);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,allocation_failures);
        FIELD(ams_mel_rf_job_interval_status_counters_v1,callbacks_after_close));
    VALUE(AMS_MEL_RF_INTERVAL_STATUS_NEVER);
    VALUE(AMS_MEL_RF_INTERVAL_STATUS_ALWAYS);
    VALUE(AMS_MEL_RF_INTERVAL_STATUS_ON_EXCEPTION);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_NONE);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_READY_FOR_NEXT_JOB_INTERVAL);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INTERRUPTED);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SPATIAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_SIGNAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_TEMPORAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_TX_EVENT_IDENTIFIER_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SPATIAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_SIGNAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_TEMPORAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_RX_EVENT_IDENTIFIER_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_SEQUENCE_TEMPORAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_SPATIAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_SIGNAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_TEMPORAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_INTERVAL_EVENT_IDENTIFIER_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_TEMPORAL_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_FAILED_INVALID_JOB_IDENTIFIER_DATA);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_COMPLETED);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_CANCELLED);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_LATE_CONTROLS);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_INVALID_CONTROLS);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_ANTENNA_FOV_ERROR);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_TRANSMIT_RF_INHIBITED);
    VALUE(AMS_MEL_RF_INTERVAL_COMPLETION_STARTED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_NONE);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_EXTENDED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_TRIGGERED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_RESUMED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_CANCELLED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_INHIBITED);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_DELAYED_START);
    VALUE(AMS_MEL_RF_LOG_TRIGGER_EVENT_TYPE_NOT_SUPPORTED);
    {
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_open)(
    ams_mel_rf_job *job, const ams_mel_rf_job_interval_status_options_v1 *options,
    ams_mel_rf_job_interval_status **out_stream, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_open;
        (void)probe_ams_mel_rf_job_interval_status_open;
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_receive)(
    ams_mel_rf_job_interval_status *stream, uint32_t timeout_ms,
    ams_mel_rf_job_interval_status_event **out_event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_receive;
        (void)probe_ams_mel_rf_job_interval_status_receive;
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_get_counters)(
    const ams_mel_rf_job_interval_status *stream,
    ams_mel_rf_job_interval_status_counters_v1 *out_counters, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_get_counters;
        (void)probe_ams_mel_rf_job_interval_status_get_counters;
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_close)(
    ams_mel_rf_job_interval_status **stream, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_close;
        (void)probe_ams_mel_rf_job_interval_status_close;
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_event_view)(
    const ams_mel_rf_job_interval_status_event *event,
    const ams_mel_rf_job_interval_status_v1 **out_view, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_event_view;
        (void)probe_ams_mel_rf_job_interval_status_event_view;
        ams_mel_status_t (*probe_ams_mel_rf_job_interval_status_event_close)(
    ams_mel_rf_job_interval_status_event **event, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required) = ams_mel_rf_job_interval_status_event_close;
        (void)probe_ams_mel_rf_job_interval_status_event_close;
        ams_mel_status_t (*probe_ams_mel_rf_job_add_rx_intervals_v2)(
    ams_mel_rf_job *job, ams_mel_rf_job_interval_config_span_v2 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required) = ams_mel_rf_job_add_rx_intervals_v2;
        (void)probe_ams_mel_rf_job_add_rx_intervals_v2;
    }
    {
        ams_mel_status_t (*id)(const ams_mel_rf_virtual_aperture *, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_id;
        ams_mel_status_t (*status)(const ams_mel_rf_virtual_aperture *, ams_mel_rf_virtual_aperture_status_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_status;
        ams_mel_status_t (*instance)(const ams_mel_rf_virtual_aperture *, uint32_t, ams_mel_rf_virtual_aperture_status_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_instance_status;
        ams_mel_status_t (*all)(const ams_mel_rf_virtual_aperture *, ams_mel_rf_va_instance_list **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_all_instances;
        ams_mel_status_t (*face)(const ams_mel_rf_virtual_aperture *, uint32_t, ams_mel_rf_va_instance_list **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_instances;
        ams_mel_status_t (*list_view)(const ams_mel_rf_va_instance_list *, ams_mel_u32_span_v1 *, char *, size_t, size_t *) = ams_mel_rf_va_instance_list_view;
        ams_mel_status_t (*list_close)(ams_mel_rf_va_instance_list **, char *, size_t, size_t *) = ams_mel_rf_va_instance_list_close;
        ams_mel_status_t (*report)(const ams_mel_rf_virtual_aperture *, uint32_t, ams_mel_rf_va_instance_status_report **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_instance_status_report;
        ams_mel_status_t (*report_view)(const ams_mel_rf_va_instance_status_report *, const ams_mel_rf_va_instance_status_report_v1 **, char *, size_t, size_t *) = ams_mel_rf_va_instance_status_report_view;
        ams_mel_status_t (*report_close)(ams_mel_rf_va_instance_status_report **, char *, size_t, size_t *) = ams_mel_rf_va_instance_status_report_close;
        (void)id; (void)status; (void)instance; (void)all; (void)face;
        (void)list_view; (void)list_close; (void)report; (void)report_view; (void)report_close;
    }
    VALUE(ams_mel_get_abi_version(&version));
    LAYOUT(ams_mel_rf_va_status_subscription *);
    RECORD(ams_mel_rf_va_status_subscription_statistics_v1,
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,callback_entries);
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,callbacks_coalesced);
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,notifications_delivered);
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,callbacks_after_stop);
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,pending);
        FIELD(ams_mel_rf_va_status_subscription_statistics_v1,stopped));
    {
        ams_mel_status_t (*open)(ams_mel_rf_virtual_aperture *, ams_mel_rf_va_status_subscription **, char *, size_t, size_t *) = ams_mel_rf_va_status_subscription_open;
        ams_mel_status_t (*wait)(ams_mel_rf_va_status_subscription *, uint32_t, char *, size_t, size_t *) = ams_mel_rf_va_status_subscription_wait;
        ams_mel_status_t (*statistics)(const ams_mel_rf_va_status_subscription *, ams_mel_rf_va_status_subscription_statistics_v1 *, char *, size_t, size_t *) = ams_mel_rf_va_status_subscription_get_statistics;
        ams_mel_status_t (*unsubscribe)(ams_mel_rf_virtual_aperture *, ams_mel_rf_va_status_subscription *, char *, size_t, size_t *) = ams_mel_rf_va_status_subscription_unsubscribe;
        ams_mel_status_t (*close)(ams_mel_rf_va_status_subscription **, char *, size_t, size_t *) = ams_mel_rf_va_status_subscription_close;
        (void)open; (void)wait; (void)statistics; (void)unsubscribe; (void)close;
    }
    LAYOUT(ams_mel_rf_element_group_snapshot *);
    LAYOUT(ams_mel_rf_element_group_mode_t);
    VALUE(AMS_MEL_RF_ELEMENT_GROUP_MODE_RX); VALUE(AMS_MEL_RF_ELEMENT_GROUP_MODE_TX);
    RECORD(ams_mel_rf_element_group_snapshot_options_v1,
        FIELD(ams_mel_rf_element_group_snapshot_options_v1,include_data_pipes));
    RECORD(ams_mel_rf_data_pipe_info_v1,
        FIELD(ams_mel_rf_data_pipe_info_v1,lookup_label);
        FIELD(ams_mel_rf_data_pipe_info_v1,label);
        FIELD(ams_mel_rf_data_pipe_info_v1,associated_endpoint_ids));
    RECORD(ams_mel_rf_data_pipe_info_span_v1,
        FIELD(ams_mel_rf_data_pipe_info_span_v1,data);
        FIELD(ams_mel_rf_data_pipe_info_span_v1,size));
    RECORD(ams_mel_rf_element_group_descriptor_v1,
        FIELD(ams_mel_rf_element_group_descriptor_v1,lookup_label);
        FIELD(ams_mel_rf_element_group_descriptor_v1,label);
        FIELD(ams_mel_rf_element_group_descriptor_v1,mode);
        FIELD(ams_mel_rf_element_group_descriptor_v1,max_rf_bandwidth_hz);
        FIELD(ams_mel_rf_element_group_descriptor_v1,max_sample_rate_samples_per_second);
        FIELD(ams_mel_rf_element_group_descriptor_v1,max_data_rate_bits_per_second);
        FIELD(ams_mel_rf_element_group_descriptor_v1,max_duty_factor);
        FIELD(ams_mel_rf_element_group_descriptor_v1,data_pipes));
    RECORD(ams_mel_rf_element_group_descriptor_span_v1,
        FIELD(ams_mel_rf_element_group_descriptor_span_v1,data);
        FIELD(ams_mel_rf_element_group_descriptor_span_v1,size));
    RECORD(ams_mel_rf_element_group_snapshot_v1,
        FIELD(ams_mel_rf_element_group_snapshot_v1,data_pipes_included);
        FIELD(ams_mel_rf_element_group_snapshot_v1,descriptors));
    {
        ams_mel_status_t (*create)(const ams_mel_rf_virtual_aperture *, const ams_mel_rf_element_group_snapshot_options_v1 *, ams_mel_rf_element_group_snapshot **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_element_groups;
        ams_mel_status_t (*view)(const ams_mel_rf_element_group_snapshot *, const ams_mel_rf_element_group_snapshot_v1 **, char *, size_t, size_t *) = ams_mel_rf_element_group_snapshot_view;
        ams_mel_status_t (*close)(ams_mel_rf_element_group_snapshot **, char *, size_t, size_t *) = ams_mel_rf_element_group_snapshot_close;
        (void)create; (void)view; (void)close;
    }
    LAYOUT(ams_mel_rf_va_local_function_list *);
    LAYOUT(ams_mel_rf_va_local_function_status *);
    LAYOUT(ams_mel_rf_va_local_function_info_v1);
    FIELD(ams_mel_rf_va_local_function_info_v1,local_function_type_id);
    FIELD(ams_mel_rf_va_local_function_info_v1,instance_count);
    LAYOUT(ams_mel_rf_va_local_function_info_span_v1);
    FIELD(ams_mel_rf_va_local_function_info_span_v1,data);
    FIELD(ams_mel_rf_va_local_function_info_span_v1,size);
    LAYOUT(ams_mel_rf_va_data_pipe_connections_snapshot *);
    RECORD(ams_mel_rf_va_data_pipe_group_v1,
        FIELD(ams_mel_rf_va_data_pipe_group_v1,element_group_lookup_label);
        FIELD(ams_mel_rf_va_data_pipe_group_v1,data_pipes));
    RECORD(ams_mel_rf_va_data_pipe_group_span_v1,
        FIELD(ams_mel_rf_va_data_pipe_group_span_v1,data);
        FIELD(ams_mel_rf_va_data_pipe_group_span_v1,size));
    RECORD(ams_mel_rf_va_data_pipe_connections_snapshot_v1,
        FIELD(ams_mel_rf_va_data_pipe_connections_snapshot_v1,groups));
    {
        ams_mel_status_t (*create)(const ams_mel_rf_virtual_aperture *, ams_mel_rf_va_data_pipe_connections_snapshot **, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_get_data_pipes;
        ams_mel_status_t (*view)(const ams_mel_rf_va_data_pipe_connections_snapshot *, const ams_mel_rf_va_data_pipe_connections_snapshot_v1 **, char *, size_t, size_t *) = ams_mel_rf_va_data_pipe_connections_snapshot_view;
        ams_mel_status_t (*close)(ams_mel_rf_va_data_pipe_connections_snapshot **, char *, size_t, size_t *) = ams_mel_rf_va_data_pipe_connections_snapshot_close;
        ams_mel_status_t (*single)(ams_mel_rf_virtual_aperture *, ams_mel_string_view_v1, ams_mel_string_view_v1, uint64_t, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint;
        ams_mel_status_t (*many)(ams_mel_rf_virtual_aperture *, ams_mel_string_view_v1, ams_mel_string_view_v1, ams_mel_u64_span_v1, uint32_t *, char *, size_t, size_t *) = ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints;
        (void)create; (void)view; (void)close; (void)single; (void)many;
    }
    VALUE(version.major); VALUE(version.minor);
    return 0;
}
