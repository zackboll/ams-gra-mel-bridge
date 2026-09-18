#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdio.h>

#define VALUE(value) printf("%llu\n", (unsigned long long)(value))
#define LAYOUT(type) VALUE(sizeof(type)); VALUE(_Alignof(type))
#define FIELD(type, field) VALUE(offsetof(type, field))

int main(void)
{
    ams_mel_abi_version_v1 version = {0, 0};
    ams_mel_status_t (*get_abi_version)(ams_mel_abi_version_v1 *) = ams_mel_get_abi_version;
    ams_mel_status_t (*session_open)(const char *, const char *, const char *,
        ams_mel_session **, char *, size_t, size_t *) = ams_mel_session_open;
    ams_mel_status_t (*session_get_provider_version)(const ams_mel_session *,
        ams_mel_provider_version_v1 *, char *, size_t, size_t *) =
        ams_mel_session_get_provider_version;
    ams_mel_status_t (*session_close)(ams_mel_session **, char *, size_t,
        size_t *) = ams_mel_session_close;
    ams_mel_status_t (*stream_open)(const ams_mel_session *,
        const ams_mel_ir_stream_config_v1 *, ams_mel_ir_stream **, char *,
        size_t, size_t *) = ams_mel_ir_stream_open;
    ams_mel_status_t (*stream_start)(ams_mel_ir_stream *, char *, size_t,
        size_t *) = ams_mel_ir_stream_start;
    ams_mel_status_t (*stream_receive)(ams_mel_ir_stream *, uint32_t,
        ams_mel_ir_frame_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_stream_receive;
    ams_mel_status_t (*stream_get_counters)(const ams_mel_ir_stream *,
        ams_mel_ir_stream_counters_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_stream_get_counters;
    ams_mel_status_t (*stream_stop)(ams_mel_ir_stream *, char *, size_t,
        size_t *) = ams_mel_ir_stream_stop;
    ams_mel_status_t (*stream_close)(ams_mel_ir_stream **, char *, size_t,
        size_t *) = ams_mel_ir_stream_close;
    ams_mel_status_t (*c2_open)(const ams_mel_session *,
        const ams_mel_ir_c2_config_v1 *, ams_mel_ir_c2 **, char *, size_t,
        size_t *) = ams_mel_ir_c2_open;
    ams_mel_status_t (*c2_enable)(ams_mel_ir_c2 *, char *, size_t, size_t *) =
        ams_mel_ir_c2_enable;
    ams_mel_status_t (*c2_submit_operate)(ams_mel_ir_c2 *, uint32_t,
        ams_mel_ir_mode_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_operate;
    ams_mel_status_t (*c2_submit_mode)(ams_mel_ir_c2 *, const ams_mel_ir_mode_command_v1 *, ams_mel_ir_mode_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_mode;
    ams_mel_status_t (*c2_submit_full_bit)(ams_mel_ir_c2 *, const ams_mel_ir_bit_command_v1 *, ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_bit;
    ams_mel_status_t (*c2_submit_config)(ams_mel_ir_c2 *, const ams_mel_ir_config_set_command_v1 *, ams_mel_ir_return_request **, char *, size_t, size_t *) = ams_mel_ir_c2_submit_config_set;
    ams_mel_status_t (*mode_request_wait)(const ams_mel_ir_mode_request *,
        uint32_t, ams_mel_ir_mode_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_mode_request_wait;
    ams_mel_status_t (*mode_request_close)(ams_mel_ir_mode_request **, char *,
        size_t, size_t *) = ams_mel_ir_mode_request_close;
    ams_mel_status_t (*c2_submit_bit_noop)(ams_mel_ir_c2 *, uint32_t,
        ams_mel_ir_return_request **, char *, size_t, size_t *) =
        ams_mel_ir_c2_submit_bit_noop;
    ams_mel_status_t (*return_request_wait)(const ams_mel_ir_return_request *,
        uint32_t, ams_mel_ir_return_result_v1 *, char *, size_t, size_t *) =
        ams_mel_ir_return_request_wait;
    ams_mel_status_t (*return_request_close)(ams_mel_ir_return_request **,
        char *, size_t, size_t *) = ams_mel_ir_return_request_close;
    ams_mel_status_t (*c2_close)(ams_mel_ir_c2 **, char *, size_t, size_t *) =
        ams_mel_ir_c2_close;

    (void)get_abi_version;
    (void)session_open;
    (void)session_get_provider_version;
    (void)session_close;
    (void)stream_open;
    (void)stream_start;
    (void)stream_receive;
    (void)stream_get_counters;
    (void)stream_stop;
    (void)stream_close;
    (void)c2_open;
    (void)c2_enable;
    (void)c2_submit_operate;
    (void)c2_submit_mode; (void)c2_submit_full_bit; (void)c2_submit_config;
    (void)mode_request_wait;
    (void)mode_request_close;
    (void)c2_submit_bit_noop;
    (void)return_request_wait;
    (void)return_request_close;
    (void)c2_close;

    VALUE(AMS_MEL_OK);
    VALUE(AMS_MEL_INVALID_ARGUMENT);
    VALUE(AMS_MEL_LIBRARY_LOAD_FAILED);
    VALUE(AMS_MEL_SYMBOL_NOT_FOUND);
    VALUE(AMS_MEL_FACTORY_FAILED);
    VALUE(AMS_MEL_INITIALIZATION_FAILED);
    VALUE(AMS_MEL_PROVIDER_EXCEPTION);
    VALUE(AMS_MEL_BUFFER_TOO_SMALL);
    VALUE(AMS_MEL_INTERNAL_ERROR);
    VALUE(AMS_MEL_TIMEOUT);
    VALUE(AMS_MEL_STREAM_STOPPED);
    VALUE(AMS_MEL_PROVIDER_FAILED);
    VALUE(AMS_MEL_COMMAND_REJECTED);
    VALUE(AMS_MEL_IR_CHANNEL_IRST_IMAGE);
    VALUE(AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL);
    VALUE(AMS_MEL_IR_MFA_MODE_UNUSED);
    VALUE(AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    VALUE(AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED);
    VALUE(AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED);
    VALUE(AMS_MEL_IR_MFA_STATE_NOT_SET); VALUE(AMS_MEL_IR_MFA_STATE_UNKNOWN); VALUE(AMS_MEL_IR_MFA_STATE_NOT_INSTALLED); VALUE(AMS_MEL_IR_MFA_STATE_OFF); VALUE(AMS_MEL_IR_MFA_STATE_PRE_INITIALIZATION); VALUE(AMS_MEL_IR_MFA_STATE_INITIALIZATION); VALUE(AMS_MEL_IR_MFA_STATE_STANDBY); VALUE(AMS_MEL_IR_MFA_STATE_OPERATE); VALUE(AMS_MEL_IR_MFA_STATE_OPERATE_RX_ONLY); VALUE(AMS_MEL_IR_MFA_STATE_OPERATE_TX_ONLY); VALUE(AMS_MEL_IR_MFA_STATE_MAINTENANCE); VALUE(AMS_MEL_IR_MFA_STATE_CALIBRATION); VALUE(AMS_MEL_IR_MFA_STATE_INITIATED_BIT); VALUE(AMS_MEL_IR_MFA_STATE_SHUTDOWN); VALUE(AMS_MEL_IR_MFA_STATE_DEGRADED); VALUE(AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE); VALUE(AMS_MEL_IR_COORD_FRAME_INERTIAL); VALUE(AMS_MEL_IR_COORD_FRAME_AIRCRAFT); VALUE(AMS_MEL_IR_DEGRADATION_CAPACITY); VALUE(AMS_MEL_IR_DEGRADATION_VOLUME); VALUE(AMS_MEL_IR_DEGRADATION_RANGE); VALUE(AMS_MEL_IR_DEGRADATION_REVISIT);
    VALUE(AMS_MEL_IR_RETURN_SUCCESS);
    VALUE(AMS_MEL_IR_RETURN_BAD_POINTER);
    VALUE(AMS_MEL_IR_RETURN_FAIL);
    VALUE(AMS_MEL_IR_RETURN_NOT_SUPPORTED);
    VALUE(AMS_MEL_IR_RETURN_NOT_IMPLEMENTED);
    VALUE(AMS_MEL_ERROR_NONE);
    VALUE(AMS_MEL_ERROR_INVALID_ID);
    VALUE(AMS_MEL_ERROR_INVALID_STATE);
    VALUE(AMS_MEL_ERROR_INVALID_PARAMETERS);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_PERMISSIONS);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_RESOURCES);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_LOCAL_RESOURCES);
    VALUE(AMS_MEL_ERROR_INSUFFICIENT_REMOTE_RESOURCES);
    VALUE(AMS_MEL_ERROR_UNSUPPORTED);
    VALUE(AMS_MEL_IR_PIXEL_MONO);
    VALUE(AMS_MEL_IR_IMAGE_STARING);
    VALUE(AMS_MEL_IR_IMAGE_SCANNING);
    VALUE(AMS_MEL_IR_FLIP_NONE);
    VALUE(AMS_MEL_IR_FLIP_VERTICAL);
    VALUE(AMS_MEL_IR_FLIP_HORIZONTAL);
    VALUE(AMS_MEL_IR_FLIP_BOTH);

    LAYOUT(ams_mel_abi_version_v1);
    FIELD(ams_mel_abi_version_v1, major);
    FIELD(ams_mel_abi_version_v1, minor);
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
    FIELD(ams_mel_string_view_v1, data);
    FIELD(ams_mel_string_view_v1, size);
    LAYOUT(ams_mel_u32_span_v1); FIELD(ams_mel_u32_span_v1, data); FIELD(ams_mel_u32_span_v1, size);
    LAYOUT(ams_mel_string_view_span_v1); FIELD(ams_mel_string_view_span_v1, data); FIELD(ams_mel_string_view_span_v1, size);
    LAYOUT(ams_mel_ir_scan_type_v1); FIELD(ams_mel_ir_scan_type_v1, continuous_scan); FIELD(ams_mel_ir_scan_type_v1, returning); FIELD(ams_mel_ir_scan_type_v1, agile_scan);
    LAYOUT(ams_mel_ir_scan_param_v1); FIELD(ams_mel_ir_scan_param_v1, elevation_defined_with_range_and_altitude); FIELD(ams_mel_ir_scan_param_v1, center_az_rad); FIELD(ams_mel_ir_scan_param_v1, center_el_rad); FIELD(ams_mel_ir_scan_param_v1, center_frame_ref_el); FIELD(ams_mel_ir_scan_param_v1, center_frame_ref_az); FIELD(ams_mel_ir_scan_param_v1, scan_width_rad); FIELD(ams_mel_ir_scan_param_v1, scan_height_rad); FIELD(ams_mel_ir_scan_param_v1, scan_type); FIELD(ams_mel_ir_scan_param_v1, scan_id); FIELD(ams_mel_ir_scan_param_v1, scan_rate_rad_per_second); FIELD(ams_mel_ir_scan_param_v1, preferred_revisit_interval_seconds); FIELD(ams_mel_ir_scan_param_v1, required_revisit_interval_seconds); FIELD(ams_mel_ir_scan_param_v1, max_range_of_interest_m); FIELD(ams_mel_ir_scan_param_v1, min_range_of_interest_m); FIELD(ams_mel_ir_scan_param_v1, elevation_scan_center_altitude_m); FIELD(ams_mel_ir_scan_param_v1, elevation_scan_center_range_m); FIELD(ams_mel_ir_scan_param_v1, degradation_method);
    LAYOUT(ams_mel_ir_mode_command_v1); FIELD(ams_mel_ir_mode_command_v1, command_id); FIELD(ams_mel_ir_mode_command_v1, state); FIELD(ams_mel_ir_mode_command_v1, mode); FIELD(ams_mel_ir_mode_command_v1, scan_parameters);
    LAYOUT(ams_mel_ir_bit_command_v1); FIELD(ams_mel_ir_bit_command_v1, command_id); FIELD(ams_mel_ir_bit_command_v1, initiate_bit_ids); FIELD(ams_mel_ir_bit_command_v1, cancel_bit_ids); FIELD(ams_mel_ir_bit_command_v1, clear_fault_codes);
    LAYOUT(ams_mel_ir_config_set_command_v1); FIELD(ams_mel_ir_config_set_command_v1, command_id); FIELD(ams_mel_ir_config_set_command_v1, system_time_ns); FIELD(ams_mel_ir_config_set_command_v1, config);
    LAYOUT(ams_mel_uci_id_v1);
    FIELD(ams_mel_uci_id_v1, uuid);
    FIELD(ams_mel_uci_id_v1, descriptive_label);
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
    FIELD(ams_mel_ir_frame_v1, width);
    FIELD(ams_mel_ir_frame_v1, height);
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

    VALUE(ams_mel_get_abi_version(&version));
    VALUE(AMS_MEL_ABI_VERSION_MAJOR);
    VALUE(AMS_MEL_ABI_VERSION_MINOR);
    VALUE(version.major);
    VALUE(version.minor);
    return 0;
}
