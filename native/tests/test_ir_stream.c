#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <limits.h>
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

static int test_capability_snapshot_lifetime(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_channel_capability *owner = NULL;
    const ams_mel_ir_channel_capability_v1 *value = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream("image-capability-rich", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_get_capabilities(stream, &owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner, &value, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(value->height == 200U && value->width == 320U &&
          value->metadata_capabilities.size == 4U &&
          value->metadata_capabilities.data[0] == AMS_MEL_IR_METADATA_BAD_PIXEL_LIST &&
          value->metadata_capabilities.data[1] == AMS_MEL_IR_METADATA_LINE_OF_SIGHT_REPORT &&
          value->metadata_capabilities.data[2] == AMS_MEL_IR_METADATA_LINE_OF_SIGHT_EULER &&
          value->metadata_capabilities.data[3] == AMS_MEL_IR_METADATA_NAVIGATION_REPORT_RESP);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(value->height == 200U && value->metadata_capabilities.data[0] ==
          AMS_MEL_IR_METADATA_BAD_PIXEL_LIST);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
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

static int test_rich_snapshot_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream("full-frame-rich", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(snapshot, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->contributing_sensor.sensor_id == UINT32_C(0xf1234567));
    CHECK(view->contributing_sensor.location.key.size == 7 && memcmp(view->contributing_sensor.location.key.data, "face-\xce\xb1", 7) == 0);
    CHECK(view->contributing_sensor.location.system_name.size == 10 && memcmp(view->contributing_sensor.location.system_name.data, "system-\xe2\x82\xac", 10) == 0);
    CHECK(view->image_flags.size == 4 && view->image_flags.data[0] == 2 && view->image_flags.data[1] == 0 && view->image_flags.data[2] == 2 && view->image_flags.data[3] == 1);
    CHECK(view->sensor_inertial_states.size == 2 && view->sensor_inertial_states.data[0].q_xyzw.w == 4 && view->sensor_inertial_states.data[0].sensor_position.z == 11 && view->sensor_inertial_states.data[0].uncertainties.platform_uncertainties == UINT32_C(0xfedcba98));
    CHECK(view->sensor_nav_states.size == 2 && view->sensor_nav_states.data[0].coordinate_system == AMS_MEL_IR_COORDINATE_NED_PLATFORM && view->sensor_nav_states.data[0].orientation.kind == AMS_MEL_IR_ORIENTATION_EULER && view->sensor_nav_states.data[0].orientation.euler.roll == 52 && view->sensor_nav_states.data[0].orientation_velocity.kind == AMS_MEL_IR_ORIENTATION_QUATERNION && view->sensor_nav_states.data[0].orientation_velocity.quaternion.w == 62);
    CHECK(view->pixels.size == 12 && view->pixels.data[0] == 0xa0 && view->pixels.data[11] == 0xab);
    CHECK(ams_mel_ir_frame_snapshot_close(&snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Task 030A: the snapshot view's pixel span must point into storage the
 * snapshot itself owns and must stay stable for the snapshot's whole life, so
 * a consumer binding can borrow it without copying. The test-only address log
 * publishes exactly that address; this asserts the C view agrees with it, and
 * that two outstanding snapshots own distinct storage. */
/* Task 030B deterministic backpressure and provider-buffer identity.
 *
 * These are implemented by the mock provider and are not part of the MEL
 * provider interface or of the public C ABI. They exist so buffer reuse can
 * be asserted from explicit provider state instead of from sleeps.
 *
 * The mock provider is loaded at runtime by the facade, so these are resolved
 * with dlsym against the already-loaded image rather than link-time. The
 * RTLD_NOLOAD handle is closed immediately, so observing the pool never
 * extends provider lifetime and cannot mask a teardown-ordering defect. */
static void *mock_pool_symbol(const char *name)
{
    void *handle = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    void *symbol = NULL;
    if (handle == NULL) return NULL;
    symbol = dlsym(handle, name);
    (void)dlclose(handle);
    return symbol;
}

static unsigned long mock_pool_query(const char *name)
{
    unsigned long (*fn)(void);
    *(void **)&fn = mock_pool_symbol(name);
    return fn == NULL ? ULONG_MAX : fn();
}

static unsigned long ams_mel_mock_pool_available(void)
{ return mock_pool_query("ams_mel_mock_pool_available"); }
static unsigned long ams_mel_mock_pool_produced(void)
{ return mock_pool_query("ams_mel_mock_pool_produced"); }
static unsigned long ams_mel_mock_pool_starved(void)
{ return mock_pool_query("ams_mel_mock_pool_starved"); }
static int ams_mel_mock_pool_produce_once(void)
{
    int (*fn)(void);
    *(void **)&fn = mock_pool_symbol("ams_mel_mock_pool_produce_once");
    return fn == NULL ? 0 : fn();
}

/* Proves the thing Task 030B exists to prove, deterministically:
 *
 *   1-3. produce and acquire three frames from a three-buffer pool;
 *   4.   none of those buffers has been returned to the provider pool;
 *   5.   another production cycle cannot reuse any of them;
 *   6-7. releasing exactly one lease returns exactly one buffer;
 *   8.   a further frame is produced using that returned buffer;
 *   9.   the still-live leases are unaffected.
 *
 * Buffer reuse is controlled by lease release and nothing else. */
static int test_lease_backpressure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *leases[3] = {NULL, NULL, NULL};
    const ams_mel_ir_frame_snapshot_v1 *views[3] = {NULL, NULL, NULL};
    const uint8_t *addresses[3] = {NULL, NULL, NULL};
    ams_mel_ir_frame_snapshot *extra = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long starved_before;
    size_t i;
    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("lease-pool", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* All three registered buffers start reusable. */
    CHECK(ams_mel_mock_pool_available() == 3);

    /* 1-3: three frames, three live leases. */
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &leases[i],
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_frame_snapshot_view(leases[i], &views[i], NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(views[i]->pixels.size == 12U && views[i]->pixels.data != NULL);
        addresses[i] = views[i]->pixels.data;
    }
    CHECK(ams_mel_mock_pool_produced() == 3);
    /* Three distinct provider buffers are in use simultaneously. */
    CHECK(addresses[0] != addresses[1] && addresses[1] != addresses[2] &&
          addresses[0] != addresses[2]);
    /* 4: not one buffer has gone back to the provider. */
    CHECK(ams_mel_mock_pool_available() == 0);

    /* 5: with every buffer checked out, the provider cannot produce. This is
       real provider-level backpressure, not a bridge queue-full drop. */
    starved_before = ams_mel_mock_pool_starved();
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_starved() == starved_before + 1);
    CHECK(ams_mel_mock_pool_produced() == 3);
    CHECK(ams_mel_mock_pool_available() == 0);
    /* A provider-side "no buffer available" event is not a bridge callback,
       so it must not be counted as a bridge queue-full drop. */
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.frames_dropped_queue_full == 0);
    CHECK(counters.frames_received == 3);

    /* 6-7: release exactly one lease, and exactly one buffer comes back. */
    CHECK(ams_mel_ir_frame_snapshot_close(&leases[0], NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(leases[0] == NULL);
    CHECK(ams_mel_mock_pool_available() == 1);

    /* 8: the next frame reuses exactly the returned buffer. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_produced() == 4);
    CHECK(ams_mel_mock_pool_available() == 0);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &extra, NULL, 0, NULL) ==
          AMS_MEL_OK);
    {
        const ams_mel_ir_frame_snapshot_v1 *extra_view = NULL;
        CHECK(ams_mel_ir_frame_snapshot_view(extra, &extra_view, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(extra_view->pixels.data == addresses[0]);
    }

    /* 9: the two still-live leases are untouched by any of that. */
    CHECK(views[1]->pixels.data == addresses[1]);
    CHECK(views[2]->pixels.data == addresses[2]);
    CHECK(views[1]->pixels.data[0] == 32U && views[2]->pixels.data[0] == 48U);

    CHECK(ams_mel_ir_frame_snapshot_close(&extra, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&leases[1], NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&leases[2], NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 3);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Proves the defining Task 030B identity for the C facade:
 *
 *     Buffer::getImageAddress() == snapshot pixels.data
 *
 * using the test-only address log, which records the provider's own image
 * address alongside the published span. No ABI change and no timing. */
static int test_provider_buffer_address_identity(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    char path[] = "/tmp/ams-mel-address-XXXXXX";
    int descriptor = mkstemp(path);
    FILE *file;
    unsigned long frame_id = 0;
    unsigned long long published = 0, size = 0, provider = 0;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG", path, 1) == 0);
    config.buffer_count = 3;
    CHECK(open_stream("lease-pool-identity", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &snapshot, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(snapshot, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->pixels.size == 12U && view->pixels.data != NULL);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fscanf(file, "%lu %llu %llu %llu", &frame_id, &published, &size, &provider) == 4);
    CHECK(fclose(file) == 0);
    /* The published span is the provider's own image memory, byte for byte. */
    CHECK(provider != 0);
    CHECK(published == provider);
    CHECK(published == (unsigned long long)(uintptr_t)view->pixels.data);
    CHECK(size == 12);
    CHECK(ams_mel_ir_frame_snapshot_close(&snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* A live lease survives public stream Close and public Session close, and it
 * does so by DEFERRING the actual provider teardown, not by copying bytes:
 *
 *     public owners closed
 *         -> channel NOT yet destroyed, library NOT yet unloaded
 *         -> lease close calls Buffer::release()
 *         -> only then channel destruction and library unload
 *
 * This replaces Task 030A's "copied bytes survive actual provider unload"
 * evidence for the lease path with the stronger correct property. */
static int test_lease_defers_provider_teardown(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    const uint8_t *address = NULL;
    char path[] = "/tmp/ams-mel-defer-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    FILE *file;
    size_t count;
    char *released, *channel_destroyed, *unloaded;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    config.buffer_count = 3;
    CHECK(open_stream("lease-pool-defer", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(lease, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->pixels.size == 12U);
    address = view->pixels.data;
    CHECK(address[0] == 16U && address[11] == 27U);

    /* Public stream Close and public Session close, with the lease live. */
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    /* The borrowed bytes are still valid and still at the same address. */
    CHECK(view->pixels.data == address);
    CHECK(address[0] == 16U && address[11] == 27U);
    /* And the provider has demonstrably NOT been torn down yet:
       public Session owner closed YES, provider actually unloaded NO. */
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(log, 1, sizeof log - 1U, file);
    log[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Closing the final lease releases the buffer and only then completes
       physical teardown. */
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(log, 1, sizeof log - 1U, file);
    log[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(unlink(path) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    released = strstr(log, "buffer_released");
    channel_destroyed = strstr(log, "channel_destroyed");
    CHECK(released != NULL && channel_destroyed != NULL);
    CHECK(released < channel_destroyed);
    unloaded = strstr(log, "library_unloaded");
    if (unloaded != NULL) CHECK(channel_destroyed < unloaded);
    return EXIT_SUCCESS;
}

/* Close must not strand a queued, never-acquired frame's provider buffer in
 * an unreachable queue: the buffer must go back to the provider, while an
 * already-dequeued live lease is preserved. */
static int test_close_discards_queued_leases(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *held = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    const uint8_t *address = NULL;
    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("lease-pool-discard", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* One acquired lease and two frames left queued. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &held, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(held, &view, NULL, 0, NULL) == AMS_MEL_OK);
    address = view->pixels.data;
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_available() == 0);

    /* Close discards the two queued frames and releases their buffers, but
       preserves the already-dequeued live lease. */
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 2);
    CHECK(view->pixels.data == address);
    CHECK(address[0] == 16U && address[11] == 27U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->pixels.data == address && address[0] == 16U);
    CHECK(ams_mel_ir_frame_snapshot_close(&held, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Several live leases across Stop, Close and Session close, then all leases
 * closing, the last of which triggers the deferred physical teardown. */
static int test_multiple_leases_close(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *leases[3] = {NULL, NULL, NULL};
    const ams_mel_ir_frame_snapshot_v1 *views[3] = {NULL, NULL, NULL};
    size_t i;
    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("lease-pool-multi", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &leases[i],
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_frame_snapshot_view(leases[i], &views[i], NULL, 0, NULL) ==
              AMS_MEL_OK);
    }
    /* Stop keeps live leases valid; Close and Session close do too. */
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(views[0]->pixels.data[0] == 16U);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(views[0]->pixels.data[0] == 16U);
    CHECK(views[1]->pixels.data[0] == 32U);
    CHECK(views[2]->pixels.data[0] == 48U);
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_ir_frame_snapshot_close(&leases[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(leases[i] == NULL);
        /* Explicit Close then a second close stays idempotent. */
        CHECK(ams_mel_ir_frame_snapshot_close(&leases[i], NULL, 0, NULL) == AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}

/* Legacy owned Receive still copies into caller storage and promptly returns
 * the provider buffer, so it acquires no provider lifetime dependency. */
static int test_legacy_receive_releases_buffer(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 frame;
    uint8_t pixels[12];
    size_t i;
    config.buffer_count = 3;
    CHECK(open_stream("lease-pool-legacy", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_available() == 2);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    for (i = 0; i < sizeof pixels; ++i) CHECK(pixels[i] == 16U + i);
    /* Copy-then-release: the buffer is reusable immediately after Receive. */
    CHECK(ams_mel_mock_pool_available() == 3);
    /* A too-small caller buffer must NOT consume or release the queued frame. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_available() == 2);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels; frame.pixel_capacity = 4;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) ==
          AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(frame.pixel_required == 12);
    CHECK(ams_mel_mock_pool_available() == 2);
    frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 3);
    /* Owned data outlives the provider entirely. */
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    for (i = 0; i < sizeof pixels; ++i) CHECK(pixels[i] == 32U + i);
    return EXIT_SUCCESS;
}

static int test_snapshot_pixel_storage_identity(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_frame_snapshot *first = NULL, *second = NULL;
    const ams_mel_ir_frame_snapshot_v1 *first_view = NULL, *second_view = NULL;
    const uint8_t *first_data = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(open_stream("success", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &second, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(first, &first_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(second, &second_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(first_view->pixels.size == 12U && second_view->pixels.size == 12U);
    CHECK(first_view->pixels.data != NULL && second_view->pixels.data != NULL);
    /* Two live leases own separate payload storage. */
    CHECK(first_view->pixels.data != second_view->pixels.data);
    first_data = first_view->pixels.data;
    /* Repeating view never republishes or relocates the payload. */
    first_view = NULL;
    CHECK(ams_mel_ir_frame_snapshot_view(first, &first_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(first_view->pixels.data == first_data);
    CHECK(first_view->pixels.data[0] == 16U && first_view->pixels.data[11] == 27U);
    CHECK(second_view->pixels.data[0] == 32U);
    /* Releasing one snapshot leaves the other's storage untouched. */
    CHECK(ams_mel_ir_frame_snapshot_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(first_view->pixels.data == first_data && first_view->pixels.data[0] == 16U);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* And survives public stream and Session close. Since Task 030B the
       bytes are the provider's own buffer memory, so this holds because
       actual provider teardown is DEFERRED while the lease is live, not
       because the payload was copied. See
       test_lease_defers_provider_teardown for the ordering evidence. */
    CHECK(first_view->pixels.data == first_data && first_view->pixels.data[11] == 27U);
    CHECK(ams_mel_ir_frame_snapshot_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* The test facade can publish degenerate pixel spans that a real provider
 * cannot produce, so every consumer binding can prove it fails closed. This
 * asserts only that the facade actually publishes them; rejection itself is a
 * binding-level contract asserted in the Ada suite. */
static int test_snapshot_malformed_pixel_span(const char *shape, int expect_null,
                                              int expect_zero)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    const ams_mel_ir_frame_snapshot_v1 *view = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(setenv("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN", shape, 1) == 0);
    CHECK(open_stream("success", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(snapshot, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK((view->pixels.data == NULL) == (expect_null != 0));
    CHECK((view->pixels.size == 0U) == (expect_zero != 0));
    CHECK(ams_mel_ir_frame_snapshot_close(&snapshot, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN") == 0);
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

struct capability_arguments {
    ams_mel_ir_stream *stream;
    ams_mel_ir_channel_capability *capability;
    ams_mel_status_t status;
};

static int capability_query(void *argument)
{
    struct capability_arguments *args = argument;
    args->status = ams_mel_ir_stream_get_capabilities(
        args->stream, &args->capability, NULL, 0, NULL);
    return 0;
}

static int test_capability_during_inflight_callback(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct capability_arguments args = {0};
    thrd_t query;
    CHECK(open_stream("capability-inflight", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    args.stream = stream;
    args.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&query, capability_query, &args) == thrd_success);
    CHECK(thrd_join(query, NULL) == thrd_success);
    CHECK(args.status == AMS_MEL_OK);
    CHECK(args.capability != NULL);
    CHECK(ams_mel_ir_channel_capability_close(&args.capability, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

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
    /* Stop logically stops the stream before the in-flight callback's release
       failure poisons it, and a logical stop is terminal for Receive. The
       blocked receiver therefore unblocks immediately with STREAM_STOPPED,
       or with PROVIDER_FAILED if it is only scheduled after the failure is
       recorded. It must never return TIMEOUT or a frame; the provider failure
       itself is still reported by Stop and Close below. */
    CHECK(args.status == AMS_MEL_STREAM_STOPPED ||
          args.status == AMS_MEL_PROVIDER_FAILED);
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
    CHECK(test_capability_snapshot_lifetime() == EXIT_SUCCESS);
    CHECK(test_snapshot_fifo_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_snapshot_pixel_storage_identity() == EXIT_SUCCESS);
    /* Task 030B provider-buffer zero copy. */
    CHECK(test_provider_buffer_address_identity() == EXIT_SUCCESS);
    CHECK(test_lease_backpressure() == EXIT_SUCCESS);
    CHECK(test_lease_defers_provider_teardown() == EXIT_SUCCESS);
    CHECK(test_close_discards_queued_leases() == EXIT_SUCCESS);
    CHECK(test_multiple_leases_close() == EXIT_SUCCESS);
    CHECK(test_legacy_receive_releases_buffer() == EXIT_SUCCESS);
    /* Stress the new lifetime and release paths repeatedly. */
    for (unsigned i = 0; i < 20; ++i) {
        CHECK(test_lease_backpressure() == EXIT_SUCCESS);
        CHECK(test_multiple_leases_close() == EXIT_SUCCESS);
        CHECK(test_close_discards_queued_leases() == EXIT_SUCCESS);
    }
    CHECK(test_snapshot_malformed_pixel_span("empty", 1, 1) == EXIT_SUCCESS);
    CHECK(test_snapshot_malformed_pixel_span("null-nonzero", 1, 0) == EXIT_SUCCESS);
    CHECK(test_snapshot_malformed_pixel_span("oversize", 0, 0) == EXIT_SUCCESS);
    CHECK(test_full_snapshot_rich() == EXIT_SUCCESS);
    CHECK(test_rich_snapshot_lifetime() == EXIT_SUCCESS);
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
    CHECK(test_capability_during_inflight_callback() == EXIT_SUCCESS);
    CHECK(test_concurrent_receive_stop() == EXIT_SUCCESS);
    CHECK(test_cleanup_failure("disable-fail") == EXIT_SUCCESS);
    CHECK(test_cleanup_failure("detach-fail") == EXIT_SUCCESS);
    CHECK(test_blocked_receive_provider_failure() == EXIT_SUCCESS);
    CHECK(test_release_failure("release-throw") == EXIT_SUCCESS);
    for (unsigned i = 0; i < 10; ++i) CHECK(test_idle() == EXIT_SUCCESS);
    puts("PASS: C IR Mono8 stream receive/queue/lifetime contract");
    return EXIT_SUCCESS;
}
