#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ams_mel_string_view_v1 view(const char *text)
{
    ams_mel_string_view_v1 result = {text, strlen(text)};
    return result;
}

static ams_mel_ir_stream_config_v1 configuration(void)
{
    ams_mel_ir_stream_config_v1 value;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    for (size_t i = 0; i < 16; ++i) {
        value.channel_id.uuid[i] = (uint8_t)i;
        value.platform_id.uuid[i] = (uint8_t)(UINT8_C(0xf0) + i);
    }
    value.channel_id.descriptive_label = view("IR image channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    value.buffer_count = 3;
    value.buffer_size = 64;
    value.queue_capacity = 4;
    return value;
}

static int open_stream(const char *scenario, ams_mel_session **session,
                       ams_mel_ir_stream **stream,
                       ams_mel_ir_stream_config_v1 *config)
{
    char diagnostic[256] = {0};
    size_t required = 0;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, diagnostic, sizeof diagnostic, &required) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(*session, config, stream, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_all(ams_mel_session **session, ams_mel_ir_stream **stream)
{
    CHECK(ams_mel_ir_stream_close(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*stream == NULL);
    CHECK(ams_mel_ir_stream_close(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_success(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    uint8_t pixels[12] = {0};
    ams_mel_ir_frame_v1 frame;
    ams_mel_ir_stream_counters_v1 counters;
    CHECK(open_stream("success", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);

    memset(&frame, 0, sizeof frame);
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) ==
          AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(frame.pixel_required == sizeof pixels);
    frame.pixels = pixels;
    frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(frame.system_time_ns == 1000001 && frame.integration_time_ns == 20001);
    CHECK(frame.width == 4 && frame.height == 3 && frame.bits_per_pixel == 8);
    CHECK(frame.number_of_bands == 1 && frame.pixel_format == AMS_MEL_IR_PIXEL_MONO);
    CHECK(frame.frame_id == 1 && frame.subframe_id == 2 && frame.subframe_total == 4);
    CHECK(frame.image_type == AMS_MEL_IR_IMAGE_STARING);
    CHECK(frame.image_flip == AMS_MEL_IR_FLIP_HORIZONTAL);
    CHECK(frame.image_flags == (UINT32_C(1) << 2));
    CHECK(frame.horizontal_fov_rad == 0.25 && frame.vertical_fov_rad == 0.125);
    CHECK(frame.dither_row == 0.5 && frame.dither_column == -0.25);
    CHECK(frame.row_offset == 7 && frame.column_offset == 9 && frame.band_index == 3);
    for (size_t i = 0; i < sizeof pixels; ++i) CHECK(pixels[i] == 16U + i);

    memset(&frame, 0, sizeof frame); frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(frame.frame_id == 2 && pixels[0] == 32 && pixels[11] == 43);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.frames_received >= 2 && counters.malformed_or_unsupported_frames == 0);

    /* Parent owner may close first; stream retains provider/library ownership. */
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    {
        ams_mel_status_t status = ams_mel_ir_stream_receive(
            stream, 0, &frame, NULL, 0, NULL);
        CHECK(status == AMS_MEL_OK || status == AMS_MEL_STREAM_STOPPED);
    }
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_snapshot_fifo_and_lifetime(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 legacy;
    uint8_t pixels[12] = {0};
    CHECK(open_stream("success", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&legacy, 0, sizeof legacy); legacy.pixels = pixels; legacy.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &legacy, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(legacy.frame_id == 1U);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(snapshot, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->frame_id == 2U && view->pixels.size == 12U);
    CHECK(view->image_flags.size == 1U && view->image_flags.data[0] == AMS_MEL_IR_IMAGE_FLAG_STARE_SNAPSHOT);
    memset(&legacy, 0, sizeof legacy); legacy.pixels = pixels; legacy.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &legacy, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(legacy.frame_id == 3U);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->frame_id == 2U && view->pixels.data[0] == 32U);
    CHECK(ams_mel_ir_frame_snapshot_close(&snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(snapshot == NULL);
    return EXIT_SUCCESS;
}

static int test_full_snapshot_rich(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    const ams_mel_ir_frame_snapshot_v1 *frame = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream("full-frame-rich", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(snapshot, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(frame->system_time_ns == -123456789 && frame->integration_time_ns == 987654321);
    CHECK(frame->width == 4 && frame->height == 3 && frame->bits_per_pixel == 8 && frame->number_of_bands == 1);
    CHECK(frame->horizontal_fov_rad == 1.25 && frame->vertical_fov_rad == 2.5 && frame->pixel_format == AMS_MEL_IR_PIXEL_MONO);
    CHECK(frame->frame_id == UINT32_C(0xfedcba98) && frame->subframe_id == 17 && frame->subframe_total == 19);
    CHECK(frame->image_type == AMS_MEL_IR_IMAGE_RESERVED13 && frame->image_flip == AMS_MEL_IR_FLIP_BOTH);
    CHECK(frame->contributing_sensor.location.offset_x_m == 1.25 && frame->contributing_sensor.location.offset_y_m == -2.5 && frame->contributing_sensor.location.offset_z_m == 3.75);
    CHECK(frame->contributing_sensor.sensor_id == UINT32_C(0xf1234567));
    CHECK(frame->contributing_sensor.location.key.size == 7 && memcmp(frame->contributing_sensor.location.key.data, "face-\xce\xb1", 7) == 0);
    CHECK(frame->contributing_sensor.location.system_name.size == 10 && memcmp(frame->contributing_sensor.location.system_name.data, "system-\xe2\x82\xac", 10) == 0);
    CHECK(frame->image_flags.size == 4 && frame->image_flags.data[0] == 2 && frame->image_flags.data[1] == 0 && frame->image_flags.data[2] == 2 && frame->image_flags.data[3] == 1);
    CHECK(frame->dither_row == -0.75 && frame->dither_column == 0.625 && frame->row_offset == 23 && frame->column_offset == 29 && frame->band_index == 31);
    CHECK(frame->sensor_inertial_states.size == 2 && frame->sensor_inertial_states.data[0].system_time_ns == -101);
    CHECK(frame->sensor_inertial_states.data[0].q_xyzw.x == 1 && frame->sensor_inertial_states.data[0].q_xyzw.y == 2 && frame->sensor_inertial_states.data[0].q_xyzw.z == 3 && frame->sensor_inertial_states.data[0].q_xyzw.w == 4);
    CHECK(frame->sensor_inertial_states.data[1].q_ecef_xyzw.x == 19 && frame->sensor_inertial_states.data[1].q_ecef_xyzw.w == 22 && frame->sensor_inertial_states.data[1].sensor_velocity.z == 28);
    CHECK(frame->sensor_inertial_states.data[0].uncertainties.sensor_uncertainties == UINT32_C(0x81234567) && frame->sensor_inertial_states.data[0].uncertainties.platform_uncertainties == UINT32_C(0xfedcba98));
    CHECK(frame->sensor_nav_states.size == 2 && frame->sensor_nav_states.data[0].coordinate_system == AMS_MEL_IR_COORDINATE_NED_PLATFORM && frame->sensor_nav_states.data[1].coordinate_system == AMS_MEL_IR_COORDINATE_NED_SENSOR);
    CHECK(frame->sensor_nav_states.data[0].position.x == 31 && frame->sensor_nav_states.data[0].position_error.w == 37 && frame->sensor_nav_states.data[0].acceleration_error.z == 50);
    CHECK(frame->sensor_nav_states.data[0].orientation.kind == AMS_MEL_IR_ORIENTATION_EULER && frame->sensor_nav_states.data[0].orientation.euler.roll == 52 && frame->sensor_nav_states.data[0].orientation_velocity.kind == AMS_MEL_IR_ORIENTATION_QUATERNION && frame->sensor_nav_states.data[0].orientation_velocity.quaternion.w == 62 && frame->sensor_nav_states.data[0].orientation_acceleration.kind == AMS_MEL_IR_ORIENTATION_EULER && frame->sensor_nav_states.data[0].orientation_acceleration.euler.yaw == 69);
    CHECK(frame->sensor_nav_states.data[1].orientation.kind == AMS_MEL_IR_ORIENTATION_QUATERNION && frame->sensor_nav_states.data[1].orientation.quaternion.x == 95 && frame->sensor_nav_states.data[1].orientation_velocity.kind == AMS_MEL_IR_ORIENTATION_EULER && frame->sensor_nav_states.data[1].orientation_velocity.euler.pitch == 104 && frame->sensor_nav_states.data[1].orientation_acceleration.kind == AMS_MEL_IR_ORIENTATION_QUATERNION && frame->sensor_nav_states.data[1].orientation_acceleration.quaternion.z == 112 && frame->sensor_nav_states.data[1].orientation_acceleration_error.w == 117);
    CHECK(frame->pixels.size == 12 && frame->pixels.data[0] == 0xa0 && frame->pixels.data[11] == 0xab);
    CHECK(ams_mel_ir_frame_snapshot_close(&snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_nested_malformed_recovery(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 frame; uint8_t pixels[12]; ams_mel_ir_stream_counters_v1 counters;
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&frame, 0, sizeof frame); frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(frame.frame_id == 2U);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.malformed_or_unsupported_frames >= 1U);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_idle(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 frame; memset(&frame, 0, sizeof frame);
    CHECK(open_stream("idle", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive(stream, 2, &frame, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_overflow(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_stream_counters_v1 counters;
    ams_mel_ir_frame_v1 frame;
    uint8_t pixels[12];
    struct timespec delay = {0, 50000000};
    config.queue_capacity = 2;
    CHECK(open_stream("overflow", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    (void)nanosleep(&delay, NULL);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.frames_received == 20);
    CHECK(counters.frames_dropped_queue_full == 18);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_malformed(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_stream_counters_v1 counters;
    ams_mel_ir_frame_v1 frame; memset(&frame, 0, sizeof frame);
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive(stream, 20, &frame, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.frames_received >= 1 && counters.malformed_or_unsupported_frames >= 1);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_start_failure(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_start_allocation_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream("buffer-factory-bad-alloc", &session, &stream, &config) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_attach_failure(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == expected);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_shutdown_callback(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct timespec delay = {0, 1000000};
    CHECK(open_stream("shutdown-callback", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    (void)nanosleep(&delay, NULL);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

struct receiver_arguments {
    ams_mel_ir_stream *stream;
    ams_mel_status_t status;
};

static int blocked_receiver(void *argument)
{
    struct receiver_arguments *args = argument;
    ams_mel_ir_frame_v1 frame;
    memset(&frame, 0, sizeof frame);
    args->status = ams_mel_ir_stream_receive(
        args->stream, UINT32_C(10000), &frame, NULL, 0, NULL);
    return 0;
}

static int test_concurrent_receive_stop(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct receiver_arguments args;
    struct timespec delay = {0, 2000000};
    thrd_t receiver;
    CHECK(open_stream("idle", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    args.stream = stream; args.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&receiver, blocked_receiver, &args) == thrd_success);
    (void)nanosleep(&delay, NULL);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(thrd_join(receiver, NULL) == thrd_success);
    CHECK(args.status == AMS_MEL_STREAM_STOPPED);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_cleanup_failure(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_release_failure(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 frame;
    ams_mel_status_t start_status;
    memset(&frame, 0, sizeof frame);
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    /* The provider starts its producer in enable(). The asynchronous release
       failure may therefore poison the stream before Start returns, or just
       after it returns successfully; the provider contract orders neither. */
    start_status = ams_mel_ir_stream_start(stream, NULL, 0, NULL);
    CHECK(start_status == AMS_MEL_OK || start_status == AMS_MEL_PROVIDER_FAILED);
    if (start_status == AMS_MEL_OK) {
        CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED);
    }
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_blocked_receive_provider_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct receiver_arguments args;
    struct timespec delay = {0, 2000000};
    thrd_t receiver;
    CHECK(open_stream("release-fail-blocked", &session, &stream, &config) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    args.stream = stream; args.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&receiver, blocked_receiver, &args) == thrd_success);
    (void)nanosleep(&delay, NULL);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(thrd_join(receiver, NULL) == thrd_success);
    CHECK(args.status == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_nonquiescing_disable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    char path[] = "/tmp/ams-mel-lifetime-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    FILE *file;
    size_t count;
    char *disable_event;
    char *callback_returned;
    char *channel_destroyed;
    char *buffer_destroyed;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("nonquiescing-disable", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(log, 1, sizeof log - 1U, file);
    log[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(unlink(path) == 0);
    disable_event = strstr(log, "disable_returned_with_callback_active");
    callback_returned = strstr(log, "callback_returned");
    channel_destroyed = strstr(log, "channel_destroyed");
    buffer_destroyed = strstr(log, "buffer_destroyed");
    CHECK(disable_event && callback_returned && channel_destroyed && buffer_destroyed);
    CHECK(disable_event < callback_returned);
    CHECK(callback_returned < channel_destroyed);
    CHECK(channel_destroyed < buffer_destroyed);
    CHECK(strstr(log, "buffer_released") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(buffer_destroyed < strstr(log, "library_unloaded"));
    return EXIT_SUCCESS;
}

static int test_capability_failure(const char *scenario,
                                   ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == expected);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_arguments(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "arguments", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    stream = (ams_mel_ir_stream *)(uintptr_t)1U;
    CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    stream = NULL;
    config.channel_type = 99;
    CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    config = configuration(); config.channel_id.descriptive_label = view("bad\0ignored");
    config.channel_id.descriptive_label.size = 11;
    CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    if (SIZE_MAX > INT64_MAX) {
        config = configuration(); config.buffer_size = (size_t)INT64_MAX + 1U;
        CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        config = configuration(); config.buffer_count = (size_t)INT64_MAX + 2U;
        CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

int main(void)
{
    static const char *malformed[] = {"null-buffer", "null-image", "image-before",
        "image-after", "invalid-dimensions", "overflow-dimensions",
        "unsupported-bpp", "unsupported-bands", "unsupported-format"};
    CHECK(test_arguments() == EXIT_SUCCESS);
    CHECK(test_success() == EXIT_SUCCESS);
    CHECK(test_snapshot_fifo_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_full_snapshot_rich() == EXIT_SUCCESS);
    CHECK(test_nested_malformed_recovery("malformed-image-type") == EXIT_SUCCESS);
    CHECK(test_nested_malformed_recovery("malformed-image-flip") == EXIT_SUCCESS);
    CHECK(test_nested_malformed_recovery("malformed-image-flag") == EXIT_SUCCESS);
    CHECK(test_nested_malformed_recovery("malformed-coordinate") == EXIT_SUCCESS);
    CHECK(test_nested_malformed_recovery("frame-copy-allocation") == EXIT_SUCCESS);
    CHECK(test_idle() == EXIT_SUCCESS);
    CHECK(test_overflow() == EXIT_SUCCESS);
    for (size_t i = 0; i < sizeof malformed / sizeof malformed[0]; ++i)
        CHECK(test_malformed(malformed[i]) == EXIT_SUCCESS);
    CHECK(test_start_failure("buffer-factory-null") == EXIT_SUCCESS);
    CHECK(test_start_allocation_failure() == EXIT_SUCCESS);
    CHECK(test_start_failure("register-fail") == EXIT_SUCCESS);
    CHECK(test_start_failure("enable-fail") == EXIT_SUCCESS);
    CHECK(test_attach_failure("attach-null", AMS_MEL_FACTORY_FAILED) == EXIT_SUCCESS);
    CHECK(test_attach_failure("attach-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_capability_failure("capability-format", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_capability_failure("capability-depth", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_capability_failure("capability-bands", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_capability_failure("capability-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_shutdown_callback() == EXIT_SUCCESS);
    CHECK(test_nonquiescing_disable() == EXIT_SUCCESS);
    CHECK(test_concurrent_receive_stop() == EXIT_SUCCESS);
    CHECK(test_cleanup_failure("disable-fail") == EXIT_SUCCESS);
    CHECK(test_cleanup_failure("detach-fail") == EXIT_SUCCESS);
    CHECK(test_blocked_receive_provider_failure() == EXIT_SUCCESS);
    CHECK(test_release_failure("release-throw") == EXIT_SUCCESS);
    for (unsigned i = 0; i < 10; ++i) CHECK(test_idle() == EXIT_SUCCESS);
    puts("PASS: C IR Mono8 stream receive/queue/lifetime contract");
    return EXIT_SUCCESS;
}
