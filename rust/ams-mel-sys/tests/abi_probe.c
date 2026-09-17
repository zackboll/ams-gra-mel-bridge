#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdio.h>

#define VALUE(value) printf("%llu\n", (unsigned long long)(value))
#define LAYOUT(type) VALUE(sizeof(type)); VALUE(_Alignof(type))
#define FIELD(type, field) VALUE(offsetof(type, field))

int main(void)
{
    ams_mel_abi_version_v1 version = {0, 0};

    VALUE(AMS_MEL_OK); VALUE(AMS_MEL_INVALID_ARGUMENT);
    VALUE(AMS_MEL_LIBRARY_LOAD_FAILED); VALUE(AMS_MEL_SYMBOL_NOT_FOUND);
    VALUE(AMS_MEL_FACTORY_FAILED); VALUE(AMS_MEL_INITIALIZATION_FAILED);
    VALUE(AMS_MEL_PROVIDER_EXCEPTION); VALUE(AMS_MEL_BUFFER_TOO_SMALL);
    VALUE(AMS_MEL_INTERNAL_ERROR); VALUE(AMS_MEL_TIMEOUT);
    VALUE(AMS_MEL_STREAM_STOPPED); VALUE(AMS_MEL_PROVIDER_FAILED);
    VALUE(AMS_MEL_IR_CHANNEL_IRST_IMAGE); VALUE(AMS_MEL_IR_PIXEL_MONO);
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

    VALUE(ams_mel_get_abi_version(&version));
    VALUE(version.major); VALUE(version.minor);
    return 0;
}
