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

/* CORRECTIVE release/reuse handoff control surface. Test-only mock facilities;
   no production C export exists or was added for any of these. */
static int mock_handoff_int(const char *name)
{
    int (*fn)(void);
    *(void **)&fn = mock_pool_symbol(name);
    return fn == NULL ? -1 : fn();
}

static void mock_handoff_void(const char *name)
{
    void (*fn)(void);
    *(void **)&fn = mock_pool_symbol(name);
    if (fn != NULL) fn();
}

static void mock_handoff_arm(void)
{ mock_handoff_void("ams_mel_mock_handoff_arm"); }
static void mock_handoff_disarm(void)
{ mock_handoff_void("ams_mel_mock_handoff_disarm"); }
static void mock_handoff_release_window(void)
{ mock_handoff_void("ams_mel_mock_handoff_release_window"); }
static int mock_handoff_wait_window(void)
{ return mock_handoff_int("ams_mel_mock_handoff_wait_window"); }
static int mock_handoff_window_reached(void)
{ return mock_handoff_int("ams_mel_mock_handoff_window_reached"); }
static unsigned long mock_handoff_reentrant_callbacks(void)
{ return mock_pool_query("ams_mel_mock_handoff_reentrant_callbacks"); }
static void mock_reentry_configure(unsigned long budget)
{
    void (*fn)(unsigned long);
    *(void **)&fn = mock_pool_symbol("ams_mel_mock_reentry_configure");
    if (fn != NULL) fn(budget);
}
static unsigned long mock_reentry_callbacks(void)
{ return mock_pool_query("ams_mel_mock_reentry_callbacks"); }
static unsigned long mock_max_release_depth(void)
{ return mock_pool_query("ams_mel_mock_max_release_depth"); }
static int mock_handoff_wait_count(unsigned long target)
{
    int (*fn)(unsigned long);
    *(void **)&fn = mock_pool_symbol("ams_mel_mock_handoff_wait_count");
    return fn == NULL ? 0 : fn(target);
}
static unsigned long mock_handoff_reached_count(void)
{ return mock_pool_query("ams_mel_mock_handoff_reached_count"); }
static void mock_handoff_release_count(unsigned long target)
{
    void (*fn)(unsigned long);
    *(void **)&fn = mock_pool_symbol("ams_mel_mock_handoff_release_count");
    if (fn != NULL) fn(target);
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
    ams_mel_ir_frame_v1 frame;
    uint8_t pixels[12];
    unsigned drained;
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* Since Task 030B a queued frame holds a provider buffer checked out, and
       physical teardown is deferred while any buffer is retained. Stop would
       then legitimately return OK with the cleanup still owed, which is the
       same deferral already used for a pending Navigation request. Drain the
       scenario's three frames first -- each Receive copies and releases its
       buffer -- so that no buffer is retained and Stop is required to perform
       the physical teardown here and report the disable failure. */
    for (drained = 0; drained < 3U; ++drained) {
        memset(&frame, 0, sizeof frame);
        frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
        CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    }
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

/* Task 030B corrective observation surface (PR #42 review). Provided by the
   mock provider, resolved with dlsym exactly like the pool controls, and not
   part of the MEL provider interface or the public C ABI. */
static int mock_flag(const char *name)
{
    int (*fn)(void);
    *(void **)&fn = mock_pool_symbol(name);
    return fn == NULL ? -1 : fn();
}

static int mock_failed_release_recorded(void)
{ return mock_flag("ams_mel_mock_failed_release_recorded"); }
static int mock_failed_buffer_alive(void)
{ return mock_flag("ams_mel_mock_failed_buffer_alive"); }
static int mock_failed_buffer_storage_intact(void)
{ return mock_flag("ams_mel_mock_failed_buffer_storage_intact"); }
static unsigned long mock_failed_release_attempts(void)
{ return mock_pool_query("ams_mel_mock_failed_release_attempts"); }
static unsigned long mock_failed_buffer_destroyed(void)
{ return mock_pool_query("ams_mel_mock_failed_buffer_destroyed"); }
/* Failed-wrapper destructions the negative control deliberately provoked,
   counted apart so that deliberate defect cannot mask a genuine one. */
static unsigned long mock_expected_failed_buffer_destroyed(void)
{ return mock_pool_query("ams_mel_mock_expected_failed_buffer_destroyed"); }
/* Per-CHANNEL release/callback counts. The process-global equivalents are no
   longer safe to difference across tests: since an uncertain release
   permanently blocks physical teardown, an earlier poisoned stream's channel
   is deliberately never destroyed and its producer thread is never joined, so
   it can still advance the global counters while a later test captures its
   baseline. These belong to exactly one channel. */
static unsigned long mock_pool_releases(void)
{ return mock_pool_query("ams_mel_mock_pool_releases"); }
static unsigned long mock_pool_callbacks(void)
{ return mock_pool_query("ams_mel_mock_pool_callbacks"); }
static unsigned long mock_image_channels_destroyed(void)
{ return mock_pool_query("ams_mel_mock_image_channels_destroyed"); }
static unsigned long mock_controls_destroyed(void)
{ return mock_pool_query("ams_mel_mock_controls_destroyed"); }

/* Corrective (PR #42 third review) observation surface. */
static int mock_callback_blocked_inside(void)
{ return mock_flag("ams_mel_mock_callback_blocked_inside"); }
static int mock_failed_registered_buffer_alive(void)
{ return mock_flag("ams_mel_mock_failed_registered_buffer_alive"); }
static unsigned long mock_registered_buffers_destroyed(void)
{ return mock_pool_query("ams_mel_mock_registered_buffers_destroyed"); }
static unsigned long mock_destructor_driven_releases(void);

static void mock_void_call(const char *name)
{
    void (*fn)(void);
    *(void **)&fn = mock_pool_symbol(name);
    if (fn != NULL) fn();
}
static void mock_release_blocked_callback(void)
{ mock_void_call("ams_mel_mock_release_blocked_callback"); }
static void mock_arm_blocked_callback(void)
{ mock_void_call("ams_mel_mock_arm_blocked_callback"); }

/* Clears only the current failed-release observation, so each regression
   observes its own buffer. The "never destroyed" evidence stays cumulative
   across the whole process. */
static void mock_failed_release_reset(void)
{
    void (*fn)(void);
    *(void **)&fn = mock_pool_symbol("ams_mel_mock_failed_release_reset");
    if (fn != NULL) fn();
}

/* PR #42 corrective regression: failed release WITH forced allocation failure
   at exactly the failed-buffer-parking point.
   =========================================================================

   This is the case the review found. Before the correction, the only owner of
   a failed callback Buffer was

       state->retained_failed_buffers.push_back(std::move(buffer));

   a potentially allocating insertion. When that allocation throws, the local
   `buffer` is destroyed and, for pinned Squall, that wrapper is a
   RequeueBuffer whose destructor calls release() a second time -- violating
   the no-retry invariant. image_stream_retain_failed(state) retains the stream
   graph but is NOT by itself evidence that the wrapper survives.

   The scenario is driven deterministically:

     - one pooled frame is produced on demand and acquired as a lease;
     - AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION=fail arms the parking
       allocation failpoint, which throws std::bad_alloc at exactly that
       historical insertion point;
     - the provider deliberately fails or throws from release();
     - closing the lease therefore runs the whole fail-safe path with the
       parking allocation failing.

   It then proves, from provider-side state rather than from the bridge:

     - release() was attempted exactly once;
     - it failed/threw as configured;
     - the exact callback Buffer wrapper is still alive (the provider holds it
       only weakly, so a dropped last reference would show up immediately);
     - that wrapper's destructor never ran, so no destructor-driven second
       release() is possible;
     - its registered host backing storage is intact and unmodified;
     - the provider channel and the Control are never destroyed, i.e. the
       provider/channel/library graph stays retained and physical teardown
       stays permanently blocked. */
static int test_release_failure_with_park_allocation_failure(const char *scenario)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    const ams_mel_ir_frame_snapshot_v1 *lease_view = NULL;
    unsigned long channels_before, controls_before;
    unsigned long releases_before, callbacks_before;
    config.buffer_count = 2;
    config.queue_capacity = 4;
    /* The mock provider is only resolvable once a session has loaded it, so
       the observation baselines are taken after the stream is open and before
       any frame is produced. */
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    controls_before = mock_controls_destroyed();
    releases_before = mock_pool_releases();
    callbacks_before = mock_pool_callbacks();
    CHECK(channels_before != ULONG_MAX && controls_before != ULONG_MAX);
    CHECK(releases_before != ULONG_MAX && callbacks_before != ULONG_MAX);
    /* Observe this regression's own failed buffer; earlier ones stay under
       the cumulative never-destroyed check below. */
    mock_failed_release_reset();
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);

    /* Exactly one frame, acquired as a live zero-copy lease. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(lease, &lease_view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(lease_view->pixels.size == 12U && lease_view->pixels.data != NULL);
    /* The buffer is checked out; nothing released yet. */
    CHECK(mock_pool_releases() == releases_before);
    CHECK(mock_pool_callbacks() == callbacks_before + 1UL);

    /* Arm the deterministic allocation failure at the parking point, then
       trigger the failing release by closing the lease. */
    CHECK(setenv("AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION", "fail", 1) == 0);
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(lease == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION") == 0);

    /* The provider was asked to release exactly once, and it failed/threw. */
    CHECK(mock_failed_release_recorded() == 1);
    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_pool_releases() == releases_before + 1UL);
    /* The exact callback wrapper survives, and its destructor never ran, so
       no destructor-driven second release() is possible. */
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    /* Host backing storage is still registered, intact, and unmodified. */
    CHECK(mock_failed_buffer_storage_intact() == 1);

    /* The failure is reported truthfully and never retried. */
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* After both public owners are gone: still exactly one release attempt,
       the wrapper still alive and never destroyed, its storage still intact,
       and physical teardown permanently blocked -- the provider channel and
       the Control were never destroyed. */
    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_pool_releases() == releases_before + 1UL);
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_failed_buffer_storage_intact() == 1);
    CHECK(mock_image_channels_destroyed() == channels_before);
    CHECK(mock_controls_destroyed() == controls_before);
    return EXIT_SUCCESS;
}

/* Companion regression for the ordinary release-failure path, i.e. the same
   uncertain-ownership handling WITHOUT any forced parking allocation failure.
   Behavior must be identical, which is what makes the test above a proof about
   the allocation-failure path specifically rather than about release failure
   in general. */
static int test_release_failure_without_park_allocation_failure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long channels_before, controls_before, releases_before;
    config.buffer_count = 2;
    config.queue_capacity = 4;
    /* The parking failpoint is explicitly NOT armed here. */
    CHECK(getenv("AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION") == NULL);
    CHECK(open_stream("release-fail-observed", &session, &stream, &config) ==
          EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    controls_before = mock_controls_destroyed();
    releases_before = mock_pool_releases();
    CHECK(channels_before != ULONG_MAX && releases_before != ULONG_MAX);
    mock_failed_release_reset();
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(lease == NULL);

    CHECK(mock_failed_release_recorded() == 1);
    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_pool_releases() == releases_before + 1UL);
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_failed_buffer_storage_intact() == 1);

    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_pool_releases() == releases_before + 1UL);
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_failed_buffer_storage_intact() == 1);
    CHECK(mock_image_channels_destroyed() == channels_before);
    CHECK(mock_controls_destroyed() == controls_before);
    return EXIT_SUCCESS;
}

static int mock_last_callback_wrapper_alive(void)
{ return mock_flag("ams_mel_mock_last_callback_wrapper_alive"); }
static unsigned long mock_destructor_driven_releases(void)
{ return mock_pool_query("ams_mel_mock_destructor_driven_releases"); }

/* Shared closing half of the callback-release-failure regressions: release
   both public owners and prove the graph survives them.

   This is the part that would have failed before the correction. Once the
   rejected frame's release is uncertain, Stop and Close must report the
   provider failure and must NOT physically tear the graph down, so the exact
   wrapper stays alive, its host bytes stay intact, release is never retried,
   and the provider channel/Control -- and therefore the loaded provider
   library -- are never destroyed. */
static int callback_release_failure_teardown(
    ams_mel_session **session, ams_mel_ir_stream **stream,
    ams_mel_ir_frame_snapshot **lease, unsigned long channels_before,
    unsigned long controls_before, unsigned long destructor_before)
{
    CHECK(ams_mel_ir_stream_stop(*stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_frame_snapshot_close(lease, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*lease == NULL);
    CHECK(ams_mel_ir_stream_close(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(*stream == NULL);
    CHECK(ams_mel_session_close(session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*session == NULL);
    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_failed_buffer_storage_intact() == 1);
    CHECK(mock_destructor_driven_releases() == destructor_before);
    CHECK(mock_image_channels_destroyed() == channels_before);
    CHECK(mock_controls_destroyed() == controls_before);
    return EXIT_SUCCESS;
}

/* PR #42 SECOND corrective regression: CALLBACK-SIDE release failure.
   ==================================================================

   The first correction only covered frames already ACCEPTED into the receive
   queue. CallbackState::image() also releases a frame the bridge REJECTS
   before acceptance -- malformed/unsupported, queue-full, or not-accepting.
   That rejected frame was never counted in retained_frames and the path never
   called image_stream_retain_failed(), so after an uncertain release a later
   Stop/Close could still observe

       requests == 0 && retained_frames == 0

   and physically tear down the provider graph, clear the registered host
   storage, and unload the provider library while Buffer ownership was
   uncertain.

   Each scenario drives one bridge rejection arm deterministically: a first
   healthy frame is produced and leased, the test puts the stream into the
   state that arm requires, and a second frame is then produced, rejected by
   that arm, and its callback-side release() is failed by the provider.

   Proven from provider-side state rather than from the bridge:

     - release() was attempted exactly once for that rejected frame;
     - the exact callback wrapper is still alive (the provider holds it only
       weakly, so a dropped last reference shows up immediately);
     - no wrapper destructor ran, so no destructor-driven second release is
       possible and release is never retried;
     - the registered host backing bytes are intact and unmodified;
     - the provider channel and the Control are never destroyed, i.e. physical
       cleanup never runs and the provider library stays loaded;
     - the public Stream and Session owners can both be closed anyway. */
static int test_callback_release_failure(const char *scenario, const char *arm)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long channels_before, controls_before;
    unsigned long releases_before, destructor_before;
    ams_mel_ir_stream_counters_v1 counters;
    config.buffer_count = 4;
    /* Capacity 1 makes the queue-full arm reachable with a single extra
       queued frame, with no sleeping and no race. */
    config.queue_capacity = 1;
    CHECK(open_stream(scenario, &session, &stream, &config) == EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    controls_before = mock_controls_destroyed();
    releases_before = mock_pool_releases();
    destructor_before = mock_destructor_driven_releases();
    CHECK(channels_before != ULONG_MAX && controls_before != ULONG_MAX);
    CHECK(releases_before != ULONG_MAX && destructor_before != ULONG_MAX);
    mock_failed_release_reset();
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);

    /* Frame 1: healthy, accepted, and taken as a live zero-copy lease. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(lease != NULL);

    if (strcmp(arm, "queue-full") == 0) {
        /* Fill the single queue slot so the NEXT callback hits the capacity
           rejection arm rather than being accepted. */
        CHECK(ams_mel_mock_pool_produce_once() == 1);
    } else if (strcmp(arm, "not-accepting") == 0) {
        /* Logical Stop clears accepting, so a later callback reaches the
           not-accepting rejection arm. Stop must still succeed here: a live
           lease on a HEALTHY graph defers physical teardown and returns OK. */
        CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    }

    /* Next frame: rejected by the arm under test, with its callback-side
       release deliberately failed by the provider. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);

    CHECK(mock_failed_release_recorded() == 1);
    CHECK(mock_failed_release_attempts() == 1UL);
    CHECK(mock_failed_buffer_alive() == 1);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_failed_buffer_storage_intact() == 1);
    CHECK(mock_destructor_driven_releases() == destructor_before);

    /* The rejection itself is still counted honestly. */
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_received >= 2U);
    if (strcmp(arm, "malformed") == 0)
        CHECK(counters.malformed_or_unsupported_frames >= 1U);
    if (strcmp(arm, "queue-full") == 0)
        CHECK(counters.frames_dropped_queue_full >= 1U);
    return callback_release_failure_teardown(
        &session, &stream, &lease, channels_before, controls_before,
        destructor_before);
}

/* NEGATIVE CONTROL for the regression above.
   =========================================

   Arms AMS_MEL_TEST_DROP_FAILED_BUFFER=drop, so the fail-safe path
   deliberately does NOT move the exact callback wrapper into its retention
   slot, reintroducing precisely the defect the corrective task forbids. The
   properties the positive regression asserts must then be VIOLATED: the
   wrapper must be destroyed and its destructor must perform the forbidden
   second release. Without this, "wrapper still alive" could pass vacuously. */
static int test_callback_release_failure_negative_control(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long destructor_before;
    config.buffer_count = 4;
    config.queue_capacity = 1;
    CHECK(setenv("AMS_MEL_TEST_DROP_FAILED_BUFFER", "drop", 1) == 0);
    CHECK(open_stream("callback-reject-negative-control", &session, &stream,
                      &config) == EXIT_SUCCESS);
    destructor_before = mock_destructor_driven_releases();
    CHECK(destructor_before != ULONG_MAX);
    mock_failed_release_reset();
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);

    /* The bridge's own release attempt happened and failed... */
    CHECK(mock_failed_release_recorded() == 1);
    /* ...but because the wrapper was dropped, every property the positive
       regression proves is now violated, which is the whole point: if these
       ever started holding while the failpoint is armed, the positive
       regression would be vacuous.

       The wrapper is destroyed, its destructor performs the forbidden SECOND
       release, and the failed-release attempt count therefore rises above the
       exactly-one the corrective task requires. */
    CHECK(mock_failed_buffer_alive() == 0);
    /* Counted in the EXPECTED bucket, so this deliberate defect can never mask
       a genuine wrapper loss in any other regression: the cumulative
       never-destroyed evidence those assert stays untouched. */
    CHECK(mock_expected_failed_buffer_destroyed() >= 1UL);
    CHECK(mock_failed_buffer_destroyed() == 0UL);
    CHECK(mock_destructor_driven_releases() > destructor_before);
    CHECK(mock_failed_release_attempts() > 1UL);

    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_DROP_FAILED_BUFFER") == 0);
    /* Clear the observation so the cumulative never-destroyed assertions in
       the positive regressions are not polluted by this deliberate defect. */
    mock_failed_release_reset();
    return EXIT_SUCCESS;
}

/* Retention-slot / preallocation failure combined with release failure.
   ====================================================================

   Arms AMS_MEL_TEST_RETENTION_SLOTS=exhaust, so the stream's preallocated
   retention slot pool reports empty for every callback. The corrected bridge
   must then refuse to call Buffer::release() at all, rather than risk an
   uncertain result it could not own.

   Proven here:

     - the frame is dropped and never enqueued;
     - release() is NEVER attempted for it, so the release attempt count stays
       flat and no failed release is ever recorded -- there is nothing to
       retry, and no destructor-driven release occurred;
     - the exact callback wrapper is still alive, held by the bridge;
     - the graph is retained: the provider channel and the Control are never
       destroyed even after both public owners are closed, so the provider
       library remains loaded and physical cleanup never runs. */
static int test_retention_slot_exhaustion(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long channels_before, controls_before;
    unsigned long releases_before, destructor_before;
    ams_mel_ir_stream_counters_v1 counters;
    config.buffer_count = 2;
    config.queue_capacity = 4;
    CHECK(setenv("AMS_MEL_TEST_RETENTION_SLOTS", "exhaust", 1) == 0);
    CHECK(open_stream("callback-reject-slot-exhaustion", &session, &stream,
                      &config) == EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    controls_before = mock_controls_destroyed();
    releases_before = mock_pool_releases();
    destructor_before = mock_destructor_driven_releases();
    CHECK(channels_before != ULONG_MAX && controls_before != ULONG_MAX);
    CHECK(releases_before != ULONG_MAX && destructor_before != ULONG_MAX);
    mock_failed_release_reset();
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);

    /* No slot, so the bridge refused to release and kept the frame. */
    CHECK(mock_pool_releases() == releases_before);
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(mock_last_callback_wrapper_alive() == 1);
    CHECK(mock_destructor_driven_releases() == destructor_before);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_received >= 1U);
    /* Nothing was enqueued. */
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 5, &lease, NULL, 0, NULL) !=
          AMS_MEL_OK);
    CHECK(lease == NULL);

    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_RETENTION_SLOTS") == 0);

    /* Still no release attempt at all, the wrapper still alive, and the graph
       never physically torn down. */
    CHECK(mock_pool_releases() == releases_before);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(mock_last_callback_wrapper_alive() == 1);
    CHECK(mock_destructor_driven_releases() == destructor_before);
    CHECK(mock_image_channels_destroyed() == channels_before);
    CHECK(mock_controls_destroyed() == controls_before);
    return EXIT_SUCCESS;
}

/* PR #42 THIRD-review corrective regression 1: close teardown TOCTOU after an
   in-flight callback publishes an uncertain release.
   =======================================================================

   image_stream_cleanup() checks retained_frames / uncertain_release once,
   under the teardown lock, before it claims physical cleanup. It then destroys
   and detaches the provider channel, waits for callbacks_in_flight to reach
   zero, and clears the registered ranges, the registered Buffers, and the
   backing host storage.

   A callback that was ALREADY in flight when the gate was passed can publish
   an uncertain Buffer::release() result after that gate but before the drain
   completes. Cleanup could therefore free registered host memory although:

     - uncertain_release == true;
     - the failed callback wrapper is retained;
     - provider ownership hand-back is uncertain.

   The interleaving proven here is exactly the one the corrective task
   specifies, driven by a barrier/failpoint and NEVER by sleeping:

       cleanup passes initial gate
       callback is still in flight
       callback release fails
       callback publishes uncertainty
       callback drains
       cleanup must NOT free host storage

   Mechanism. The "teardown-race-late-uncertain" scenario blocks inside
   getImageAddress(), i.e. inside the bridge listener with callbacks_in_flight
   already incremented, and publishes that fact. The test waits for that
   published flag, then starts Close on another thread -- at which point
   retained_frames == 0 and uncertain_release == false, so cleanup passes its
   initial gate -- and the armed before-detach barrier holds cleanup while the
   test releases the blocked callback. The callback then returns a null image
   address, the bridge rejects the frame, its callback-side release fails,
   retain_uncertain_buffer() publishes the uncertainty, and only then does the
   callback drain.

   The assertion is about HOST STORAGE, not about the channel: the channel has
   already crossed its documented quiescence/destruction boundary by the drain
   point, which is why the corrective design documents retention of the host
   storage/Buffer/library graph rather than claiming the channel stayed
   attached. */
struct race_close_args {
    ams_mel_ir_stream **stream;
    ams_mel_status_t status;
};

static int race_close_entry(void *argument)
{
    struct race_close_args *args = (struct race_close_args *)argument;
    args->status = ams_mel_ir_stream_close(args->stream, NULL, 0, NULL);
    return 0;
}

static int produce_once_entry(void *argument)
{
    (void)argument;
    return ams_mel_mock_pool_produce_once();
}

static int wait_for_file(const char *path)
{
    unsigned attempt;
    for (attempt = 0; attempt < 20000U; ++attempt) {
        FILE *file = fopen(path, "rb");
        if (file != NULL) { (void)fclose(file); return EXIT_SUCCESS; }
        {
            const struct timespec delay = {0, 1000000L};
            (void)nanosleep(&delay, NULL);
        }
    }
    return EXIT_FAILURE;
}

static int create_file(const char *path)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fputs("x\n", file) >= 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

/* skip_recheck != 0 arms AMS_MEL_TEST_SKIP_POST_DRAIN_RECHECK=skip, the
   NEGATIVE MUTATION: cleanup then omits the post-drain recheck and the
   host-storage assertion below MUST fail, proving it is not vacuous.
   Returns EXIT_SUCCESS iff the host storage was kept. */
static int run_teardown_race_late_uncertain(int skip_recheck)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct race_close_args args;
    thrd_t closer, producer;
    char base[64] = "/tmp/ams-mel-race-XXXXXX";
    char detach_arm[128], detach_reached[128], detach_release[128];
    unsigned long registered_destroyed_before;
    unsigned attempt;
    int descriptor, host_storage_kept;
    config.buffer_count = 2;
    config.queue_capacity = 4;

    descriptor = mkstemp(base);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(base) == 0);
    CHECK(snprintf(detach_arm, sizeof detach_arm, "%s.before-detach.arm",
                   base) > 0);
    CHECK(snprintf(detach_reached, sizeof detach_reached,
                   "%s.before-detach.reached", base) > 0);
    CHECK(snprintf(detach_release, sizeof detach_release,
                   "%s.before-detach.release", base) > 0);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER", base, 1) == 0);
    CHECK(create_file(detach_arm) == EXIT_SUCCESS);
    if (skip_recheck)
        CHECK(setenv("AMS_MEL_TEST_SKIP_POST_DRAIN_RECHECK", "skip", 1) == 0);

    mock_arm_blocked_callback();
    mock_failed_release_reset();
    CHECK(open_stream("teardown-race-late-uncertain", &session, &stream,
                      &config) == EXIT_SUCCESS);
    registered_destroyed_before = mock_registered_buffers_destroyed();
    CHECK(registered_destroyed_before != ULONG_MAX);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);

    /* Ask for one frame; its callback blocks INSIDE the bridge listener.
       Production is driven on another thread so this one can proceed. */
    CHECK(thrd_create(&producer, produce_once_entry, NULL) == thrd_success);
    /* Explicit published condition, no sleeping decides anything: the
       callback is genuinely in flight. */
    for (attempt = 0; attempt < 20000U; ++attempt) {
        if (mock_callback_blocked_inside() == 1) break;
        { const struct timespec d = {0, 1000000L}; (void)nanosleep(&d, NULL); }
    }
    CHECK(mock_callback_blocked_inside() == 1);

    /* At this instant retained_frames == 0 and uncertain_release == false, so
       a Close starting now passes cleanup's INITIAL gate while the callback is
       still in flight. Close parks on the armed before-detach barrier. */
    args.stream = &stream;
    args.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&closer, race_close_entry, &args) == thrd_success);
    CHECK(wait_for_file(detach_reached) == EXIT_SUCCESS);

    /* Cleanup has passed its initial gate and owns cleanup. NOW let the
       in-flight callback fail its release and publish the uncertainty. */
    mock_release_blocked_callback();
    for (attempt = 0; attempt < 20000U; ++attempt) {
        if (mock_failed_release_recorded() == 1) break;
        { const struct timespec d = {0, 1000000L}; (void)nanosleep(&d, NULL); }
    }
    CHECK(mock_failed_release_recorded() == 1);

    /* Release cleanup; it detaches, destroys the channel, drains the callback,
       and reaches the post-drain recheck. */
    CHECK(create_file(detach_release) == EXIT_SUCCESS);
    CHECK(thrd_join(closer, NULL) == thrd_success);
    CHECK(thrd_join(producer, NULL) == thrd_success);

    /* THE ASSERTION: registered host storage must NOT have been freed. */
    host_storage_kept =
        mock_failed_registered_buffer_alive() == 1 &&
        mock_registered_buffers_destroyed() == registered_destroyed_before;

    if (!skip_recheck) {
        CHECK(host_storage_kept);
        /* Retained Buffer ownership is not destroyed, the uncertain release is
           never retried, and the failure is reported truthfully. */
        CHECK(mock_failed_buffer_alive() == 1);
        CHECK(mock_failed_buffer_destroyed() == 0UL);
        CHECK(mock_failed_release_attempts() == 1UL);
        CHECK(args.status == AMS_MEL_PROVIDER_FAILED);
    }

    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    (void)unlink(detach_arm);
    (void)unlink(detach_reached);
    (void)unlink(detach_release);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER") == 0);
    if (skip_recheck)
        CHECK(unsetenv("AMS_MEL_TEST_SKIP_POST_DRAIN_RECHECK") == 0);
    mock_arm_blocked_callback();
    return host_storage_kept ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int test_teardown_race_late_uncertain(void)
{
    CHECK(run_teardown_race_late_uncertain(0) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* NEGATIVE MUTATION for the regression above. With the post-drain recheck
   removed, cleanup frees the registered host storage the uncertain release
   forbids freeing, so the host-storage assertion MUST fail. */
static int test_teardown_race_negative_mutation(void)
{
    CHECK(run_teardown_race_late_uncertain(1) == EXIT_FAILURE);
    mock_failed_release_reset();
    return EXIT_SUCCESS;
}

/* PR #42 THIRD-review corrective regression 2: the callback Releaser must stay
   ARMED until the enqueue succeeds.
   ====================================================================

   std::deque::push_back may allocate and therefore may throw. The previous
   code disarmed the Releaser BEFORE that insertion, so a throwing enqueue
   bypassed the explicit release/uncertain-retention policy entirely and the
   owned provider buffer was simply dropped.

   AMS_MEL_TEST_ENQUEUE_ALLOCATION=fail throws std::bad_alloc at exactly the
   enqueue-allocation boundary. Proven here:

     - a failed enqueue invokes EXACTLY ONE bridge-controlled release;
     - success returns the retention slot;
     - a failed/throwing release on that path follows the uncertain-ownership
       path;
     - no wrapper is accidentally destroyed after a failed release;
     - no retention slot is leaked. */
static int test_enqueue_allocation_failure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long releases_before;
    unsigned i;
    config.buffer_count = 2;
    config.queue_capacity = 4;

    /* --- Arm A: enqueue throws, the release SUCCEEDS. ----------------- */
    CHECK(open_stream("enqueue-alloc-fail", &session, &stream, &config) ==
          EXIT_SUCCESS);
    releases_before = mock_pool_releases();
    CHECK(releases_before != ULONG_MAX);
    mock_failed_release_reset();
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION", "fail", 1) == 0);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(unsetenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION") == 0);
    /* EXACTLY ONE bridge-controlled release for the failed enqueue, and it
       succeeded, so no uncertain-ownership state was created. Under the old
       order the releaser was already disarmed and this would be +0. */
    CHECK(mock_pool_releases() == releases_before + 1UL);
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    /* Nothing was enqueued. */
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 5, &lease, NULL, 0, NULL) !=
          AMS_MEL_OK);
    CHECK(lease == NULL);
    /* NO RETENTION SLOT WAS LEAKED: a successful release returns the slot, so
       the pool keeps serving frames. buffer_count is 2, so 20 further frames
       completing proves recycling rather than a leak. */
    for (i = 0; i < 20U; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0,
                                                 NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(lease == NULL);
    }
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_received >= 21U);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);

    /* --- Arm B: enqueue throws AND the release then FAILS. ------------ */
    {
        unsigned long channels_before, controls_before;
        ams_mel_ir_stream_config_v1 failing = configuration();
        failing.buffer_count = 2;
        failing.queue_capacity = 4;
        mock_failed_release_reset();
        CHECK(open_stream("enqueue-alloc-fail-release-fail", &session, &stream,
                          &failing) == EXIT_SUCCESS);
        channels_before = mock_image_channels_destroyed();
        controls_before = mock_controls_destroyed();
        CHECK(channels_before != ULONG_MAX && controls_before != ULONG_MAX);
        CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(setenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION", "fail", 1) == 0);
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(unsetenv("AMS_MEL_TEST_ENQUEUE_ALLOCATION") == 0);
        /* The still-armed Releaser performed EXACTLY ONE release, it failed,
           and the uncertain-ownership path ran: the wrapper is owned by its
           retention slot, never destroyed, never retried. */
        CHECK(mock_failed_release_recorded() == 1);
        CHECK(mock_failed_release_attempts() == 1UL);
        CHECK(mock_failed_buffer_alive() == 1);
        CHECK(mock_failed_buffer_destroyed() == 0UL);
        CHECK(mock_failed_buffer_storage_intact() == 1);
        /* Uncertain ownership permanently blocks teardown, truthfully. */
        CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED);
        CHECK(stream == NULL);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(mock_failed_release_attempts() == 1UL);
        CHECK(mock_failed_buffer_alive() == 1);
        CHECK(mock_failed_buffer_destroyed() == 0UL);
        CHECK(mock_image_channels_destroyed() == channels_before);
        CHECK(mock_controls_destroyed() == controls_before);
        mock_failed_release_reset();
    }
    return EXIT_SUCCESS;
}

/* PR #42 THIRD-review corrective regression 3: a NULL callback Buffer must not
   consume a retention slot.
   ====================================================================

   The callback used to acquire a retention slot before testing !buffer. The
   Releaser destructor returns immediately when value is null, so that slot was
   never recycled: every null-Buffer callback permanently consumed one of the
   stream-sized slots.

   The corrected code rejects a null Buffer BEFORE acquiring a slot -- the
   simpler design the task prefers, never acquiring emergency ownership for an
   object that does not exist.

   The scenario emits substantially more null-buffer callbacks than
   buffer_count (40 vs 2), then valid frames. Under the defect the pool would
   be long exhausted and a later valid frame would hit the no-slot branch: the
   bridge would refuse to release it, drop it, and poison the stream. Here the
   later valid frames must instead acquire slots and complete normally. */
static int test_null_buffer_no_retention_slot(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long releases_before, destructor_before;
    unsigned i;
    /* Far fewer slots than null callbacks, which is the whole point. */
    config.buffer_count = 2;
    config.queue_capacity = 4;
    mock_failed_release_reset();
    CHECK(open_stream("null-buffer-callbacks", &session, &stream, &config) ==
          EXIT_SUCCESS);
    releases_before = mock_pool_releases();
    destructor_before = mock_destructor_driven_releases();
    CHECK(releases_before != ULONG_MAX && destructor_before != ULONG_MAX);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);

    /* 40 null-Buffer callbacks, i.e. 20x buffer_count. */
    for (i = 0; i < 40U; ++i)
        CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_received >= 40U);
    /* Each was rejected as malformed/unsupported... */
    CHECK(counters.malformed_or_unsupported_frames >= 40U);
    /* ...with no release attempted, since there is no Buffer to release, and
       no uncertain ownership created. */
    CHECK(mock_pool_releases() == releases_before);
    CHECK(mock_failed_release_recorded() == 0);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(mock_destructor_driven_releases() == destructor_before);

    /* LATER VALID FRAMES STILL ACQUIRE SLOTS AND COMPLETE NORMALLY. */
    for (i = 0; i < 5U; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0,
                                                 NULL) == AMS_MEL_OK);
        CHECK(lease != NULL);
        CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(lease == NULL);
    }
    /* Released normally, so slots were genuinely available and recycled. */
    CHECK(mock_pool_releases() >= releases_before + 5UL);
    CHECK(mock_failed_release_recorded() == 0);
    /* The stream stays healthy: no uncertain ownership was ever created, so
       ordinary Stop/Close still succeed and teardown still runs. */
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Bounded by CONFIGURED provider-buffer capacity, not by a global constant.
   =======================================================================

   Opens a stream whose buffer_count deliberately exceeds the old fixed
   64-node reserve and sustains far more than 64 checkouts, proving the stream
   preallocates a dedicated retention slot for every registered provider
   buffer and recycles them on successful release, without ever depending on a
   process-global reserve. Under the old design correctness leaned on
   retained_reserve_size == 64; it is no longer a correctness dependency. */
static int test_retention_capacity_exceeds_legacy_reserve(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long releases_before;
    unsigned i;
    /* Both dimensions exceed the old 64-slot assumption. */
    config.buffer_count = 96;
    config.queue_capacity = 8;
    CHECK(open_stream("lease-pool-capacity", &session, &stream, &config) ==
          EXIT_SUCCESS);
    releases_before = mock_pool_releases();
    CHECK(releases_before != ULONG_MAX);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* 200 successful checkout/lease/release cycles, i.e. more than three times
       the old global reserve, all served by this stream's own recycled slots. */
    for (i = 0; i < 200U; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0,
                                                 NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(lease == NULL);
    }
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_received >= 200U);
    CHECK(counters.malformed_or_unsupported_frames == 0U);
    CHECK(counters.frames_dropped_queue_full == 0U);
    CHECK(mock_pool_releases() >= releases_before + 200UL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Stop status must distinguish a HEALTHY deferral from a POISONED graph.
   =====================================================================

     healthy graph + outstanding lease -> AMS_MEL_OK, physical teardown
                                          deferred to the last lease close;
     already poisoned graph            -> AMS_MEL_PROVIDER_FAILED.

   Both halves are asserted here explicitly so the distinction cannot regress
   into "any deferral is OK" or "any deferral is a failure". */
static int test_stop_status_healthy_versus_poisoned(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long channels_before;
    config.buffer_count = 4;
    config.queue_capacity = 4;

    /* Healthy graph, one outstanding lease: Stop defers and reports OK. */
    CHECK(open_stream("lease-pool-stopstatus", &session, &stream, &config) ==
          EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    CHECK(channels_before != ULONG_MAX);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* Deferred, not performed: the channel is still alive. */
    CHECK(mock_image_channels_destroyed() == channels_before);
    /* Closing the last lease performs the deferred physical teardown. */
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mock_image_channels_destroyed() == channels_before + 1UL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);

    /* Poisoned graph, one outstanding lease: Stop must report the failure. */
    lease = NULL;
    config.queue_capacity = 1;
    CHECK(open_stream("callback-reject-malformed", &session, &stream, &config) ==
          EXIT_SUCCESS);
    channels_before = mock_image_channels_destroyed();
    mock_failed_release_reset();
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 1000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Poison the graph with a callback-side uncertain release. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(mock_failed_release_recorded() == 1);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
    /* Teardown stays permanently blocked even after the last lease closes. */
    CHECK(mock_image_channels_destroyed() == channels_before);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mock_image_channels_destroyed() == channels_before);
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

/* ===================================================================
   CORRECTIVE: provider-buffer release/reuse handoff.

   The defect. The bridge held the old callback's emergency-ownership
   (retention) slot for the whole duration of Buffer::release() and returned it
   only after that provider call completed. A conforming provider republishes
   the physical buffer into its reusable pool as soon as the release succeeds --
   pinned Squall does so INSIDE release(), with the pool mutex released, before
   the call returns. With every retention slot occupied, a brand new callback
   for that successfully returned physical buffer therefore found an EMPTY slot
   free list, and the bridge treated that healthy reuse as a non-conforming
   provider: it refused to release, counted a malformed frame, published
   uncertain_release, poisoned the lifecycle to Failed, and retained the graph
   permanently.

   Why physical buffer_count did not bound this. A retention slot is keyed to a
   PER-CALLBACK Buffer WRAPPER generation, not to a physical buffer. During the
   window above, one physical buffer legitimately has TWO live wrapper
   generations: the one whose release is still executing and the new one the
   provider just delivered. buffer_count bounds simultaneous physical
   CHECKOUTS; it never bounded overlapping wrapper ownership.

   The reproduction is fully deterministic. The mock pauses inside release()
   AFTER the physical buffer has been republished, and the test proves the
   window was actually reached before driving the reusing callback. Nothing
   sleeps to create the interleaving and no probabilistic stress is involved.

   These regressions deliberately do NOT use AMS_MEL_TEST_RETENTION_SLOTS:
   manufacturing exhaustion that way would bypass the real handoff defect.

   Distinct live snapshot owners are used for the internal concurrency; the
   same public handle is never used or closed from two threads.
   =================================================================== */

struct handoff_closer {
    ams_mel_ir_frame_snapshot *snapshot;
    ams_mel_status_t status;
};

/* All state the shared scenario and its verification step exchange, so the
   verification can live in its own function without a long parameter list. */
struct handoff_ctx {
    ams_mel_session *session;
    ams_mel_ir_stream *stream;
    ams_mel_ir_frame_snapshot *leases[3];
    const ams_mel_ir_frame_snapshot_v1 *views[3];
    const uint8_t *addresses[3];
    uint8_t bytes1;
    uint8_t bytes2;
    ams_mel_ir_stream_counters_v1 before;
    unsigned long destructor_releases_before;
    unsigned long failed_destroyed_before;
    unsigned long failed_attempts_before;
    unsigned long callbacks_before;
    unsigned long releases_before;
};

static int handoff_close_thread(void *argument)
{
    struct handoff_closer *closer = (struct handoff_closer *)argument;
    /* This thread is the sole owner of this snapshot handle. */
    closer->status = ams_mel_ir_frame_snapshot_close(&closer->snapshot, NULL, 0, NULL);
    return 0;
}

/* Shared scenario:

     1. three provider buffers, three live snapshots, all checked out;
     2. close one snapshot on a dedicated thread, so its release blocks in the
        forced window with the physical buffer already republished;
     3. prove the window was reached;
     4. drive a new callback for that same physical buffer while the old
        retention slot is, in the defective build, still held;
     5. let the paused release return and join;
     6. assert the new frame was accepted normally and reuses the address. */
static int handoff_verify(struct handoff_ctx *ctx);

static int test_release_reuse_handoff(const char *scenario)
{
    ams_mel_ir_stream_config_v1 config = configuration();
    struct handoff_ctx ctx;
    struct handoff_closer closer;
    thrd_t closer_thread;
    const int reuse_inside = strcmp(scenario, "handoff-reuse-inside") == 0;
    size_t i;

    memset(&ctx, 0, sizeof ctx);
    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream(scenario, &ctx.session, &ctx.stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(ctx.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 3);

    ctx.destructor_releases_before = mock_destructor_driven_releases();
    ctx.failed_destroyed_before = mock_failed_buffer_destroyed();
    ctx.failed_attempts_before = mock_failed_release_attempts();
    ctx.callbacks_before = mock_pool_callbacks();
    ctx.releases_before = mock_pool_releases();

    /* 1: three live snapshots; every physical buffer is checked out. */
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(ctx.stream, 2000, &ctx.leases[i],
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_frame_snapshot_view(ctx.leases[i], &ctx.views[i],
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ctx.views[i]->pixels.size == 12U && ctx.views[i]->pixels.data != NULL);
        ctx.addresses[i] = ctx.views[i]->pixels.data;
    }
    CHECK(ctx.addresses[0] != ctx.addresses[1] &&
          ctx.addresses[1] != ctx.addresses[2] &&
          ctx.addresses[0] != ctx.addresses[2]);
    CHECK(ams_mel_mock_pool_available() == 0);
    /* Every retention slot is now occupied: exactly the precondition under
       which the defect turns a healthy reuse into a false exhaustion. */
    ctx.bytes1 = ctx.views[1]->pixels.data[0];
    ctx.bytes2 = ctx.views[2]->pixels.data[0];
    CHECK(ams_mel_ir_stream_get_counters(ctx.stream, &ctx.before, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ctx.before.malformed_or_unsupported_frames == 0);
    CHECK(ctx.before.frames_dropped_queue_full == 0);

    /* 2: close ONE snapshot on its own thread. Its release republishes the
       physical buffer and then enters the forced window. */
    mock_handoff_arm();
    closer.snapshot = ctx.leases[0];
    closer.status = AMS_MEL_INTERNAL_ERROR;
    ctx.leases[0] = NULL;
    CHECK(thrd_create(&closer_thread, handoff_close_thread, &closer) == thrd_success);

    if (reuse_inside) {
        /* The provider itself drives the reusing callback from INSIDE the
           still-executing release call, with its pool mutex released. The fix
           therefore cannot depend on an unsupported return-before-reuse
           assumption. Join first, then verify reentry actually happened. */
        CHECK(thrd_join(closer_thread, NULL) == thrd_success);
        CHECK(closer.status == AMS_MEL_OK);
        CHECK(closer.snapshot == NULL);
        CHECK(mock_handoff_window_reached() == 1);
        CHECK(mock_handoff_reentrant_callbacks() == 1UL);
    } else {
        /* 3: prove the window was ACTUALLY reached -- the physical buffer is
           already back in the provider pool while the bridge has not yet
           reconciled its bookkeeping for the old wrapper. */
        CHECK(mock_handoff_wait_window() == 1);
        CHECK(ams_mel_mock_pool_available() == 1);

        /* 4: force a brand new callback for that SAME physical buffer now,
           inside the window. In the defective build this is where the bridge
           finds an empty retention free list. */
        CHECK(ams_mel_mock_pool_produce_once() == 1);

        /* 5: let the paused release return and join its owner thread. */
        mock_handoff_release_window();
        CHECK(thrd_join(closer_thread, NULL) == thrd_success);
        CHECK(closer.status == AMS_MEL_OK);
        CHECK(closer.snapshot == NULL);
    }
    mock_handoff_disarm();
    return handoff_verify(&ctx);
}

/* Step 6 and the full post-conditions, shared by both handoff windows. */
static int handoff_verify(struct handoff_ctx *ctx)
{
    ams_mel_ir_frame_snapshot *reused = NULL;
    const ams_mel_ir_frame_snapshot_v1 *reused_view = NULL;
    ams_mel_ir_stream_counters_v1 after;
    unsigned long channels_before, controls_before;

    /* THE BASELINE FAILING ASSERTIONS. The new callback must be accepted
       normally. Before the fix the bridge rejected it as a non-conforming
       provider, so this returned AMS_MEL_PROVIDER_FAILED -- the lifecycle had
       been poisoned to Failed -- instead of AMS_MEL_OK. */
    CHECK(ams_mel_ir_stream_receive_snapshot(ctx->stream, 2000, &reused,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_view(reused, &reused_view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(reused_view->pixels.size == 12U);
    /* The new snapshot uses the returned physical buffer's address. */
    CHECK(reused_view->pixels.data == ctx->addresses[0]);

    /* The two still-live snapshots keep their original addresses AND bytes. */
    CHECK(ctx->views[1]->pixels.data == ctx->addresses[1]);
    CHECK(ctx->views[2]->pixels.data == ctx->addresses[2]);
    CHECK(ctx->views[1]->pixels.data[0] == ctx->bytes1);
    CHECK(ctx->views[2]->pixels.data[0] == ctx->bytes2);

    /* No false malformed count and no false queue-full count. Before the fix
       the refused-release branch incremented malformed_or_unsupported_frames
       for a perfectly healthy frame. */
    CHECK(ams_mel_ir_stream_get_counters(ctx->stream, &after, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(after.malformed_or_unsupported_frames == 0);
    CHECK(after.frames_dropped_queue_full == 0);
    CHECK(after.frames_received > ctx->before.frames_received);

    /* No permanent uncertain-ownership state was introduced: no release
       failed, no failed wrapper was destroyed, and no wrapper destructor had
       to perform a second release. Each callback wrapper therefore received
       exactly ONE bridge-controlled release attempt. */
    CHECK(mock_failed_release_attempts() == ctx->failed_attempts_before);
    CHECK(mock_failed_buffer_destroyed() == ctx->failed_destroyed_before);
    CHECK(mock_destructor_driven_releases() == ctx->destructor_releases_before);

    /* After all legitimate owners close, provider capacity is restored. */
    CHECK(ams_mel_ir_frame_snapshot_close(&reused, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&ctx->leases[1], NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&ctx->leases[2], NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 3);

    /* Exactly one release per callback wrapper, across the whole scenario. */
    CHECK(mock_pool_releases() - ctx->releases_before ==
          mock_pool_callbacks() - ctx->callbacks_before);

    /* Channel destruction and provider unload occur in the correct order and
       no emergency retention leaked: a poisoned stream would have kept the
       channel attached forever, so these counters would NOT advance.
       Baselines are captured immediately before Close, so only this stream's
       own teardown is observed. */
    channels_before = mock_image_channels_destroyed();
    controls_before = mock_controls_destroyed();
    /* Stream close first, while the provider library is still loaded, so the
       mock's counters remain observable. A poisoned stream would have kept
       the channel attached, so this counter would not advance. */
    CHECK(ams_mel_ir_stream_close(&ctx->stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ctx->stream == NULL);
    CHECK(mock_image_channels_destroyed() > channels_before);
    /* Physical teardown released every registered Buffer, so still no
       destructor-driven second release and no failed-wrapper destruction. */
    CHECK(mock_destructor_driven_releases() == ctx->destructor_releases_before);
    CHECK(mock_failed_buffer_destroyed() == ctx->failed_destroyed_before);
    /* Only now the Session: Control is destroyed and the provider unloaded,
       strictly after the channel, which is the required ordering. */
    CHECK(ams_mel_ir_stream_close(&ctx->stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&ctx->session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(controls_before != ULONG_MAX);
    return EXIT_SUCCESS;
}

/* Concurrent close with DISTINCT snapshot owners. The counted provider barrier
   proves the corrected protocol intentionally admits one provider release and
   backpressures the second owner until that release resolves. The same public
   handle is never touched by two threads. */
static int test_concurrent_release_distinct_owners(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *leases[3] = {NULL, NULL, NULL};
    ams_mel_ir_frame_snapshot *extra = NULL;
    struct handoff_closer closers[2];
    thrd_t threads[2];
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long failed_attempts_before, destructor_before;
    size_t i;

    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("handoff-concurrent", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    failed_attempts_before = mock_failed_release_attempts();
    destructor_before = mock_destructor_driven_releases();
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 2000, &leases[i],
              NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_mock_pool_available() == 0);

    mock_handoff_arm();
    closers[0].snapshot = leases[0];
    closers[0].status = AMS_MEL_INTERNAL_ERROR;
    leases[0] = NULL;
    CHECK(thrd_create(&threads[0], handoff_close_thread, &closers[0]) == thrd_success);
    CHECK(mock_handoff_wait_count(1UL));
    closers[1].snapshot = leases[1];
    closers[1].status = AMS_MEL_INTERNAL_ERROR;
    leases[1] = NULL;
    CHECK(thrd_create(&threads[1], handoff_close_thread, &closers[1]) == thrd_success);
    /* The second distinct owner is genuinely backpressured: only one provider
       call has entered. */
    CHECK(mock_handoff_reached_count() == 1UL);
    mock_handoff_release_count(1UL);
    CHECK(thrd_join(threads[0], NULL) == thrd_success);
    CHECK(mock_handoff_wait_count(2UL));
    CHECK(closers[0].status == AMS_MEL_OK && closers[0].snapshot == NULL);
    mock_handoff_release_count(2UL);
    CHECK(thrd_join(threads[1], NULL) == thrd_success);
    mock_handoff_disarm();
    for (i = 0; i < 2; ++i) {
        CHECK(closers[i].status == AMS_MEL_OK);
        CHECK(closers[i].snapshot == NULL);
    }
    CHECK(ams_mel_mock_pool_available() == 2);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.malformed_or_unsupported_frames == 0);
    CHECK(counters.frames_dropped_queue_full == 0);
    CHECK(mock_failed_release_attempts() == failed_attempts_before);
    CHECK(mock_destructor_driven_releases() == destructor_before);

    /* Reuse still works afterwards, so no slot leaked in either pool. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 2000, &extra, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&extra, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_frame_snapshot_close(&leases[2], NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_available() == 3);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Repeated successful reuse reaches the real execution bound (one active
   provider release), then proves later explicit closes wait with safe HOLD
   ownership. Releasing the active window admits each waiter in turn. Eight A
   generations exceed the reviewed six-slot guess without exceeding the
   corrected execution limit. */
static int test_repeated_generation_release_backpressure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *current = NULL;
    ams_mel_ir_frame_snapshot *siblings[2] = {NULL, NULL};
    const ams_mel_ir_frame_snapshot_v1 *current_view = NULL;
    const ams_mel_ir_frame_snapshot_v1 *sibling_views[2] = {NULL, NULL};
    const uint8_t *address_a = NULL;
    const uint8_t *sibling_addresses[2] = {NULL, NULL};
    uint8_t sibling_bytes[2][12];
    struct handoff_closer closers[8];
    thrd_t threads[8];
    int joined[8] = {0};
    size_t started = 0U;
    int result = EXIT_FAILURE;
    size_t generation;
    unsigned long releases_before = 0UL;

    memset(closers, 0, sizeof closers);
    config.buffer_count = 3;
    config.queue_capacity = 8;
    if (open_stream("handoff-repeat-saturation", &session, &stream, &config) !=
        EXIT_SUCCESS) goto cleanup;
    if (ams_mel_ir_stream_start(stream, NULL, 0, NULL) != AMS_MEL_OK) goto cleanup;
    mock_handoff_arm();
    releases_before = mock_pool_releases();

    /* Acquire A, B, and C before any close. Receives are serialized here on
       this one test thread. */
    if (ams_mel_mock_pool_produce_once() != 1 ||
        ams_mel_ir_stream_receive_snapshot(stream, 2000, &current, NULL, 0, NULL) !=
            AMS_MEL_OK ||
        ams_mel_ir_frame_snapshot_view(current, &current_view, NULL, 0, NULL) !=
            AMS_MEL_OK) goto cleanup;
    address_a = current_view->pixels.data;
    for (size_t i = 0; i < 2U; ++i) {
        if (ams_mel_mock_pool_produce_once() != 1 ||
            ams_mel_ir_stream_receive_snapshot(stream, 2000, &siblings[i],
                NULL, 0, NULL) != AMS_MEL_OK ||
            ams_mel_ir_frame_snapshot_view(siblings[i], &sibling_views[i],
                NULL, 0, NULL) != AMS_MEL_OK) goto cleanup;
        sibling_addresses[i] = sibling_views[i]->pixels.data;
        memcpy(sibling_bytes[i], sibling_views[i]->pixels.data, 12U);
    }
    if (address_a == sibling_addresses[0] || address_a == sibling_addresses[1] ||
        sibling_addresses[0] == sibling_addresses[1] ||
        ams_mel_mock_pool_available() != 0UL) goto cleanup;

    for (generation = 0U; generation < 8U; ++generation) {
        ams_mel_ir_frame_snapshot *next = NULL;
        const ams_mel_ir_frame_snapshot_v1 *next_view = NULL;
        closers[started].snapshot = current;
        closers[started].status = AMS_MEL_INTERNAL_ERROR;
        current = NULL;
        if (thrd_create(&threads[started], handoff_close_thread,
                        &closers[started]) != thrd_success) goto cleanup;
        ++started;
        if (generation != 0U) {
            /* The new closer is backpressured behind the one active provider
               call; it has not recursively or concurrently entered release. */
            if (mock_handoff_reached_count() != (unsigned long)generation ||
                ams_mel_mock_pool_available() != 0UL) goto cleanup;
            mock_handoff_release_count((unsigned long)generation);
            if (thrd_join(threads[generation - 1U], NULL) != thrd_success)
                goto cleanup;
            joined[generation - 1U] = 1;
            if (closers[generation - 1U].status != AMS_MEL_OK ||
                closers[generation - 1U].snapshot != NULL) goto cleanup;
        }
        if (!mock_handoff_wait_count((unsigned long)(generation + 1U)) ||
            ams_mel_mock_pool_available() != 1UL) goto cleanup;
        if (generation == 7U) break;
        if (ams_mel_mock_pool_produce_once() != 1 ||
            ams_mel_ir_stream_receive_snapshot(stream, 2000, &next,
                NULL, 0, NULL) != AMS_MEL_OK ||
            ams_mel_ir_frame_snapshot_view(next, &next_view, NULL, 0, NULL) !=
                AMS_MEL_OK) goto cleanup;
        if (next_view->pixels.data != address_a ||
            next_view->frame_id != (uint32_t)(4U + generation)) goto cleanup;
        for (size_t i = 0; i < 2U; ++i) {
            if (sibling_views[i]->pixels.data != sibling_addresses[i] ||
                memcmp(sibling_views[i]->pixels.data, sibling_bytes[i], 12U) != 0)
                goto cleanup;
        }
        current = next;
    }

    mock_handoff_release_count(8UL);
    if (thrd_join(threads[7], NULL) != thrd_success) goto cleanup;
    joined[7] = 1;
    for (generation = 0U; generation < 8U; ++generation)
        if (closers[generation].status != AMS_MEL_OK ||
            closers[generation].snapshot != NULL) goto cleanup;
    if (mock_pool_releases() - releases_before != 8UL ||
        ams_mel_mock_pool_available() != 1UL) goto cleanup;
    for (size_t i = 0; i < 2U; ++i) {
        if (sibling_views[i]->pixels.data != sibling_addresses[i] ||
            memcmp(sibling_views[i]->pixels.data, sibling_bytes[i], 12U) != 0)
            goto cleanup;
    }
    mock_handoff_release_count(ULONG_MAX);
    for (size_t i = 0; i < 2U; ++i) {
        if (ams_mel_ir_frame_snapshot_close(&siblings[i], NULL, 0, NULL) != AMS_MEL_OK)
            goto cleanup;
    }
    if (ams_mel_mock_pool_available() != 3UL ||
        close_all(&session, &stream) != EXIT_SUCCESS) goto cleanup;
    result = EXIT_SUCCESS;

cleanup:
    mock_handoff_release_count(ULONG_MAX);
    mock_handoff_disarm();
    for (size_t i = 0; i < started; ++i)
        if (!joined[i]) (void)thrd_join(threads[i], NULL);
    if (current != NULL)
        (void)ams_mel_ir_frame_snapshot_close(&current, NULL, 0, NULL);
    for (size_t i = 0; i < 2U; ++i)
        if (siblings[i] != NULL)
            (void)ams_mel_ir_frame_snapshot_close(&siblings[i], NULL, 0, NULL);
    if (stream != NULL)
        (void)ams_mel_ir_stream_close(&stream, NULL, 0, NULL);
    if (session != NULL)
        (void)ams_mel_session_close(&session, NULL, 0, NULL);
    return result;
}

/* A queue-full callback is released from inside an enclosing release. Each
   successful deferred release republishes A and drives another queue-full
   callback. The finite budget exceeds the reviewed six-slot guess, while the
   bridge drains iteratively and keeps provider release nesting at one. */
static int test_callback_rejection_reentry_chain(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    const unsigned long budget = 9UL;
    unsigned long releases_before;

    config.buffer_count = 3;
    config.queue_capacity = 1;
    CHECK(open_stream("handoff-reentry-chain", &session, &stream, &config) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    mock_reentry_configure(budget);
    releases_before = mock_pool_releases();
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 2000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Fill the only queue entry so every reentrant callback takes the real
       queue-full rejection path. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mock_reentry_callbacks() == budget);
    CHECK(mock_max_release_depth() == 1UL);
    CHECK(mock_pool_releases() - releases_before == budget + 1UL);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.frames_dropped_queue_full == budget);
    CHECK(mock_failed_release_attempts() == 0UL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

struct discard_close_args {
    ams_mel_ir_stream **stream;
    ams_mel_status_t status;
};

static int discard_close_entry(void *argument)
{
    struct discard_close_args *args = (struct discard_close_args *)argument;
    args->status = ams_mel_ir_stream_close(args->stream, NULL, 0, NULL);
    return 0;
}

static int test_close_discard_release_backpressure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    struct discard_close_args args;
    thrd_t thread;
    char path[] = "/tmp/ams-mel-close-discard-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    FILE *file;
    size_t count;
    char *channel_destroyed, *control_destroyed, *unloaded;
    size_t released_count = 0U;

    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    config.buffer_count = 3;
    config.queue_capacity = 4;
    CHECK(open_stream("handoff-close-discard", &session, &stream, &config) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    for (size_t i = 0; i < 3U; ++i) CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_mock_pool_available() == 0UL);
    mock_handoff_arm();
    args.stream = &stream;
    args.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&thread, discard_close_entry, &args) == thrd_success);
    CHECK(mock_handoff_wait_count(1UL));
    CHECK(mock_handoff_reached_count() == 1UL);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(log, 1, sizeof log - 1U, file);
    log[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    for (unsigned long ordinal = 1UL; ordinal <= 3UL; ++ordinal) {
        mock_handoff_release_count(ordinal);
        if (ordinal != 3UL) CHECK(mock_handoff_wait_count(ordinal + 1UL));
    }
    CHECK(thrd_join(thread, NULL) == thrd_success);
    mock_handoff_disarm();
    CHECK(args.status == AMS_MEL_OK && stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(log, 1, sizeof log - 1U, file);
    log[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(unlink(path) == 0);
    channel_destroyed = strstr(log, "channel_destroyed");
    control_destroyed = strstr(log, "control_destroyed");
    unloaded = strstr(log, "library_unloaded");
    CHECK(channel_destroyed != NULL && control_destroyed != NULL && unloaded != NULL);
    CHECK(channel_destroyed < control_destroyed && control_destroyed < unloaded);
    for (char *event = log; (event = strstr(event, "buffer_released\n")) != NULL;
         event += strlen("buffer_released\n"))
        ++released_count;
    CHECK(released_count == 3U);
    return EXIT_SUCCESS;
}

/* The same release/reuse window reached through the LEGACY owned-copy Receive
   and through Close-time queue discard, i.e. the other two entry points into
   the common release helper. Deliberately small: it reuses the existing
   handoff scenario machinery instead of duplicating test infrastructure. */
static int test_release_reuse_handoff_common_paths(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_v1 frame;
    uint8_t pixels[12] = {0};
    ams_mel_ir_stream_counters_v1 counters;
    unsigned long failed_attempts_before, destructor_before;

    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("handoff-reuse-inside", &session, &stream, &config) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* Captured only after the provider library is loaded: the mock counters
       are resolved with dlsym against the loaded image, so a baseline taken
       before the Session exists would read the not-loaded sentinel. */
    failed_attempts_before = mock_failed_release_attempts();
    destructor_before = mock_destructor_driven_releases();

    /* Legacy Receive copies the bytes out and then releases the provider
       buffer through the same common helper. Arming the window makes that
       release republish and be reused before it returns. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels;
    frame.pixel_capacity = sizeof pixels;
    mock_handoff_arm();
    CHECK(ams_mel_ir_stream_receive(stream, 2000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mock_handoff_window_reached() == 1);
    CHECK(mock_handoff_reentrant_callbacks() == 1UL);
    mock_handoff_disarm();
    /* Owned-copy behavior is unchanged. */
    CHECK(frame.width == 4 && frame.height == 3 && frame.pixel_required == 12);

    /* The reentrant callback left a frame queued but never acquired. Close
       discards it and releases its provider buffer through the same helper,
       with no false malformed or queue-full accounting. */
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.malformed_or_unsupported_frames == 0);
    CHECK(counters.frames_dropped_queue_full == 0);
    /* Checked while the provider is still loaded: no release failed and no
       wrapper destructor performed a second release on either path. */
    CHECK(mock_failed_release_attempts() == failed_attempts_before);
    CHECK(mock_destructor_driven_releases() == destructor_before);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* NEGATIVE CONTROL / MUTATION CHECK.

   AMS_MEL_TEST_SKIP_RELEASE_HANDOFF=skip removes exactly the essential
   corrective step: begin_release() still moves the wrapper into its dedicated
   release slot, but no longer hands the HOLD slot back before the provider
   call. That is precisely the defective protocol, so the reusing callback must
   again observe a falsely exhausted hold pool and be rejected.

   This proves the positive regressions above detect the defect rather than
   passing vacuously. The mutation is a test-only failpoint compiled out of
   production builds; no temporary source mutation is left behind. */
static int test_release_reuse_handoff_negative_mutation(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *leases[3] = {NULL, NULL, NULL};
    ams_mel_ir_frame_snapshot *reused = NULL;
    ams_mel_ir_stream_counters_v1 counters;
    struct handoff_closer closer;
    thrd_t closer_thread;
    ams_mel_status_t reuse_status;
    size_t i;

    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(setenv("AMS_MEL_TEST_SKIP_RELEASE_HANDOFF", "skip", 1) == 0);
    CHECK(open_stream("handoff-pause-after", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    for (i = 0; i < 3; ++i) {
        CHECK(ams_mel_mock_pool_produce_once() == 1);
        CHECK(ams_mel_ir_stream_receive_snapshot(stream, 2000, &leases[i],
              NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_mock_pool_available() == 0);

    mock_handoff_arm();
    closer.snapshot = leases[0];
    closer.status = AMS_MEL_INTERNAL_ERROR;
    leases[0] = NULL;
    CHECK(thrd_create(&closer_thread, handoff_close_thread, &closer) == thrd_success);
    CHECK(mock_handoff_wait_window() == 1);
    CHECK(ams_mel_mock_pool_available() == 1);
    /* The reusing callback arrives while the mutated build still holds the
       old hold slot. */
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    mock_handoff_release_window();
    CHECK(thrd_join(closer_thread, NULL) == thrd_success);
    mock_handoff_disarm();

    reuse_status = ams_mel_ir_stream_receive_snapshot(stream, 500, &reused,
                                                      NULL, 0, NULL);
    /* THE MUTATION EVIDENCE. With the corrective handoff removed the healthy
       reuse is NOT accepted: the stream was poisoned by the false exhaustion
       branch, so no snapshot is produced and the frame was miscounted as
       malformed. */
    CHECK(reuse_status != AMS_MEL_OK);
    CHECK(reused == NULL);
    CHECK(ams_mel_ir_stream_get_counters(stream, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.malformed_or_unsupported_frames >= 1);
    CHECK(unsetenv("AMS_MEL_TEST_SKIP_RELEASE_HANDOFF") == 0);

    /* The mutated run deliberately poisoned the stream, so it is permanently
       un-teardownable by design. Release the remaining public owners; the
       graph stays retained, which is the documented uncertain-release policy
       and exactly what the positive regressions assert does NOT happen. */
    (void)ams_mel_ir_frame_snapshot_close(&leases[1], NULL, 0, NULL);
    (void)ams_mel_ir_frame_snapshot_close(&leases[2], NULL, 0, NULL);
    (void)ams_mel_ir_stream_close(&stream, NULL, 0, NULL);
    (void)ams_mel_session_close(&session, NULL, 0, NULL);
    return EXIT_SUCCESS;
}

/* Release-slot admission control. AMS_MEL_TEST_RELEASE_SLOTS=exhaust forces
   the DISJOINT release pool empty -- a different pool and a different branch
   from AMS_MEL_TEST_RETENTION_SLOTS. With no dedicated permanent owner
   available for a potentially uncertain result, the bridge must refuse to
   release at all rather than risk an unownable outcome: the close reports the
   failure truthfully, the exact wrapper is never destroyed, no release is
   attempted, and the provider graph is retained. */
static int test_release_slot_admission_control(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config = configuration();
    ams_mel_ir_frame_snapshot *lease = NULL;
    unsigned long releases_before, destructor_before, failed_destroyed_before;

    config.buffer_count = 3;
    config.queue_capacity = 8;
    CHECK(open_stream("handoff-concurrent", &session, &stream, &config) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_mock_pool_produce_once() == 1);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 2000, &lease, NULL, 0, NULL) ==
          AMS_MEL_OK);
    releases_before = mock_pool_releases();
    destructor_before = mock_destructor_driven_releases();
    failed_destroyed_before = mock_failed_buffer_destroyed();

    CHECK(setenv("AMS_MEL_TEST_RELEASE_SLOTS", "exhaust", 1) == 0);
    /* Refused, and reported truthfully rather than silently succeeding. */
    CHECK(ams_mel_ir_frame_snapshot_close(&lease, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(lease == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_RELEASE_SLOTS") == 0);

    /* No release() was attempted at all, so no uncertain result could arise
       and no forbidden retry is possible. */
    CHECK(mock_pool_releases() == releases_before);
    /* The exact wrapper survives: parked, never destroyed, so its destructor
       never performed a release either. */
    CHECK(mock_last_callback_wrapper_alive() == 1);
    CHECK(mock_destructor_driven_releases() == destructor_before);
    CHECK(mock_failed_buffer_destroyed() == failed_destroyed_before);
    /* The buffer is permanently withheld: provider capacity is NOT restored
       and physical teardown stays blocked, exactly as for any other
       unresolved ownership obligation. */
    CHECK(ams_mel_mock_pool_available() == 2);
    (void)ams_mel_ir_stream_close(&stream, NULL, 0, NULL);
    (void)ams_mel_session_close(&session, NULL, 0, NULL);
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
    /* CORRECTIVE: provider-buffer release/reuse handoff. Both forced windows:
       paused after a successful provider release, and reuse driven from
       inside the still-executing release call. */
    CHECK(test_release_reuse_handoff("handoff-pause-after") == EXIT_SUCCESS);
    CHECK(test_release_reuse_handoff("handoff-reuse-inside") == EXIT_SUCCESS);
    CHECK(test_repeated_generation_release_backpressure() == EXIT_SUCCESS);
    CHECK(test_callback_rejection_reentry_chain() == EXIT_SUCCESS);
    CHECK(test_close_discard_release_backpressure() == EXIT_SUCCESS);
    CHECK(test_concurrent_release_distinct_owners() == EXIT_SUCCESS);
    CHECK(test_release_reuse_handoff_common_paths() == EXIT_SUCCESS);
    /* Repetition SUPPLEMENTS the deterministic coverage above; it does not
       replace it. The exact interleaving is already forced and proven. */
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(test_release_reuse_handoff("handoff-pause-after") == EXIT_SUCCESS);
        CHECK(test_release_reuse_handoff("handoff-reuse-inside") == EXIT_SUCCESS);
        CHECK(test_concurrent_release_distinct_owners() == EXIT_SUCCESS);
    }
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
    /* PR #42 corrective: failed-buffer retention must be genuinely
       allocation-free, and the exact callback wrapper must survive. Run both
       the forced-parking-allocation-failure cases and the ordinary
       release-failure case repeatedly, so the process-lifetime
       never-destroyed and exactly-once-release invariants are exercised
       against accumulated retained state rather than a single clean run. */
    CHECK(test_release_failure_with_park_allocation_failure(
              "release-fail-park-alloc") == EXIT_SUCCESS);
    CHECK(test_release_failure_with_park_allocation_failure(
              "release-throw-park-alloc") == EXIT_SUCCESS);
    CHECK(test_release_failure_without_park_allocation_failure() == EXIT_SUCCESS);
    /* PR #42 SECOND corrective: callback-side release failure, i.e. a frame
       rejected BEFORE queue acceptance whose in-callback release is
       uncertain, must reach the same graph retention as an accepted lease. */
    CHECK(test_callback_release_failure("callback-reject-malformed",
                                        "malformed") == EXIT_SUCCESS);
    CHECK(test_callback_release_failure("callback-reject-queue-full",
                                        "queue-full") == EXIT_SUCCESS);
    CHECK(test_callback_release_failure("callback-reject-not-accepting",
                                        "not-accepting") == EXIT_SUCCESS);
    CHECK(test_retention_slot_exhaustion() == EXIT_SUCCESS);
    CHECK(test_retention_capacity_exceeds_legacy_reserve() == EXIT_SUCCESS);
    CHECK(test_stop_status_healthy_versus_poisoned() == EXIT_SUCCESS);
    /* PR #42 THIRD corrective. These run BEFORE the negative controls below,
       which deliberately violate the cumulative invariants. */
    CHECK(test_null_buffer_no_retention_slot() == EXIT_SUCCESS);
    CHECK(test_enqueue_allocation_failure() == EXIT_SUCCESS);
    CHECK(test_teardown_race_late_uncertain() == EXIT_SUCCESS);
    /* Repeat, so the process-lifetime never-destroyed and exactly-once
       invariants hold against accumulated retained state. */
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(test_callback_release_failure("callback-reject-malformed",
                                            "malformed") == EXIT_SUCCESS);
        CHECK(test_callback_release_failure("callback-reject-queue-full",
                                            "queue-full") == EXIT_SUCCESS);
        CHECK(test_callback_release_failure("callback-reject-not-accepting",
                                            "not-accepting") == EXIT_SUCCESS);
        CHECK(test_retention_slot_exhaustion() == EXIT_SUCCESS);
    }
    /* Negative control LAST among the corrective cases: it deliberately
       violates the invariants, so it must not run before the positive
       regressions that assert them cumulatively. */
    CHECK(test_callback_release_failure_negative_control() == EXIT_SUCCESS);
    /* PR #42 THIRD corrective NEGATIVE MUTATION: removing the post-drain
       teardown recheck must make the host-storage assertion fail. Also placed
       after the positive regressions for the same reason. */
    CHECK(test_teardown_race_negative_mutation() == EXIT_SUCCESS);
    /* CORRECTIVE release/reuse handoff. Admission control deliberately
       poisons its stream, and the mutation deliberately reintroduces the
       defect, so both run AFTER every positive regression that asserts the
       cumulative invariants. */
    CHECK(test_release_slot_admission_control() == EXIT_SUCCESS);
    CHECK(test_release_reuse_handoff_negative_mutation() == EXIT_SUCCESS);
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(test_release_failure_with_park_allocation_failure(
                  "release-fail-park-alloc") == EXIT_SUCCESS);
        CHECK(test_release_failure_with_park_allocation_failure(
                  "release-throw-park-alloc") == EXIT_SUCCESS);
        CHECK(test_release_failure_without_park_allocation_failure() ==
              EXIT_SUCCESS);
    }
    for (unsigned i = 0; i < 10; ++i) CHECK(test_idle() == EXIT_SUCCESS);
    puts("PASS: C IR Mono8 stream receive/queue/lifetime contract");
    return EXIT_SUCCESS;
}
