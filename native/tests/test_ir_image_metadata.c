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

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)

static ams_mel_string_view_v1 text(const char *value) { return (ams_mel_string_view_v1){value, strlen(value)}; }
static ams_mel_ir_stream_config_v1 config(void)
{
    ams_mel_ir_stream_config_v1 value = {0};
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    value.channel_id.descriptive_label = text("image"); value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    value.buffer_count = 3; value.buffer_size = 64; value.queue_capacity = 2;
    return value;
}
static int open_all(const char *scenario, ams_mel_session **session, ams_mel_ir_stream **stream)
{
    ams_mel_ir_stream_config_v1 value = config();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(*session, &value, stream, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int close_all(ams_mel_session **session, ams_mel_ir_stream **stream)
{
    CHECK(ams_mel_ir_stream_close(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int receive(ams_mel_ir_image_metadata *metadata, ams_mel_ir_image_metadata_event **event,
                   const ams_mel_ir_image_metadata_event_v1 **view)
{
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 0, event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_view(*event, view, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int wait_for_file(const char *path)
{
    struct timespec delay = {0, 1000000};
    unsigned attempt;
    for (attempt = 0; attempt < 5000U; ++attempt) {
        if (access(path, F_OK) == 0) return EXIT_SUCCESS;
        (void)nanosleep(&delay, NULL);
    }
    return EXIT_FAILURE;
}
static int append_log(const char *path, const char *value)
{
    FILE *file = fopen(path, "ab");
    CHECK(file != NULL);
    CHECK(fprintf(file, "%s\n", value) > 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}
struct metadata_open_arguments {
    ams_mel_ir_stream *stream;
    ams_mel_ir_image_metadata *metadata;
    ams_mel_status_t status;
};
static int metadata_open(void *argument)
{
    struct metadata_open_arguments *args = argument;
    args->status = ams_mel_ir_image_metadata_open(
        args->stream, 2, &args->metadata, NULL, 0, NULL);
    return 0;
}
static int test_concurrent_open_one_shot(void)
{
    char log[] = "/tmp/ams-image-register-log-XXXXXX";
    char barrier[] = "/tmp/ams-image-register-barrier-XXXXXX";
    char entered[256], release[256], contents[4096];
    int descriptor = mkstemp(log);
    FILE *file;
    size_t count;
    struct metadata_open_arguments first = {0};
    ams_mel_ir_image_metadata *second = NULL;
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    thrd_t opener;
    CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    descriptor = mkstemp(barrier); CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(snprintf(entered, sizeof entered, "%s.entered", barrier) > 0);
    CHECK(snprintf(release, sizeof release, "%s.release", barrier) > 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_METADATA_REGISTER_BARRIER", barrier, 1) == 0);
    CHECK(open_all("image-metadata-register-blocked", &session, &stream) == EXIT_SUCCESS);
    first.stream = stream; first.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(thrd_create(&opener, metadata_open, &first) == thrd_success);
    CHECK(wait_for_file(entered) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &second, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(second == NULL);
    CHECK(append_log(release, "release") == EXIT_SUCCESS);
    CHECK(thrd_join(opener, NULL) == thrd_success);
    CHECK(first.status == AMS_MEL_OK && first.metadata != NULL);
    CHECK(ams_mel_ir_image_metadata_close(&first.metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_METADATA_REGISTER_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    file = fopen(log, "rb"); CHECK(file != NULL);
    count = fread(contents, 1, sizeof contents - 1U, file); contents[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(strstr(contents, "bad_pixel_registration_attempted") != NULL);
    CHECK(strstr(strstr(contents, "bad_pixel_registration_attempted") + 1,
                 "bad_pixel_registration_attempted") == NULL);
    CHECK(unlink(entered) == 0); CHECK(unlink(release) == 0); CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}
static int test_rich_and_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_image_metadata *metadata = NULL; ams_mel_ir_image_metadata_event *event = NULL;
    const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    CHECK(open_all("image-metadata-sync", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST);
    CHECK(view->bad_pixel_list.reported_size == UINT32_C(0xa5a50001));
    CHECK(view->bad_pixel_list.reported_count == UINT32_C(0x5a5a0003));
    CHECK(view->bad_pixel_list.pixels.size == 3U && view->bad_pixel_list.pixels.data[0].column == UINT32_C(0x80000001));
    CHECK(view->bad_pixel_list.pixels.data[1].row == UINT32_C(0xf0000002));
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(view->bad_pixel_list.pixels.data[2].row == 123U);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int check_report(const ams_mel_ir_image_metadata_event_v1 *view)
{
    const ams_mel_ir_line_of_sight_report_v1 *value = &view->line_of_sight_report;
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_REPORT);
    CHECK(value->system_time_ns == INT64_C(-123456789));
    CHECK(value->pointing_angle.azimuth_rad == 1.25 && value->pointing_angle.elevation_rad == -2.5);
    CHECK(value->pointing_angle_rates.azimuth_rad == 0.125 && value->pointing_angle_rates.elevation_rad == -0.25);
    CHECK(value->at_speed == 1U && value->in_tolerance == 0U);
    CHECK(value->platform_attitude.roll == 0.5 && value->platform_attitude.pitch == -0.75 && value->platform_attitude.yaw == 1.0);
    CHECK(value->validity_flag_bitfield == UINT32_C(0xa5a50003) && value->image_rotation_rad == -1.5);
    return EXIT_SUCCESS;
}
static int check_euler(const ams_mel_ir_image_metadata_event_v1 *view)
{
    const ams_mel_ir_line_of_sight_euler_v1 *value = &view->line_of_sight_euler;
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_LINE_OF_SIGHT_EULER);
    CHECK(value->system_time_ns == INT64_C(987654321));
    CHECK(value->attitude.roll == -0.5 && value->attitude.pitch == 1.25 && value->attitude.yaw == -2.0);
    CHECK(value->attitude_rates.roll == 0.25 && value->attitude_rates.pitch == -0.125 && value->attitude_rates.yaw == 0.0625);
    return EXIT_SUCCESS;
}
static int test_los_rich_and_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *report = NULL; ams_mel_ir_image_metadata_event *euler = NULL;
    const ams_mel_ir_image_metadata_event_v1 *report_view = NULL, *euler_view = NULL;
    CHECK(open_all("image-metadata-los-rich", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive(metadata, &report, &report_view) == EXIT_SUCCESS);
    CHECK(receive(metadata, &euler, &euler_view) == EXIT_SUCCESS);
    CHECK(check_report(report_view) == EXIT_SUCCESS); CHECK(check_euler(euler_view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(check_report(report_view) == EXIT_SUCCESS); CHECK(check_euler(euler_view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_close(&report, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_close(&euler, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
static int test_mixed_los_fifo(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *first = NULL, *second = NULL;
    const ams_mel_ir_image_metadata_event_v1 *first_view = NULL, *second_view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(open_all("image-metadata-los-mixed", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 5U && counters.events_dropped_queue_full == 3U && counters.malformed_or_unsupported == 0U);
    CHECK(receive(metadata, &first, &first_view) == EXIT_SUCCESS); CHECK(receive(metadata, &second, &second_view) == EXIT_SUCCESS);
    CHECK(first_view->kind == AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST);
    CHECK(first_view->bad_pixel_list.pixels.data[0].row == 1U);
    CHECK(check_report(second_view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int test_los_recovery(const char *scenario, int report)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *event = NULL;
    const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(open_all(scenario, &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 2U && counters.malformed_or_unsupported == 1U);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
    CHECK((report ? check_report(view) : check_euler(view)) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int test_los_allocation_recovery(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *event = NULL;
    const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(setenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(open_all("image-metadata-los-report-allocation", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE") == 0);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 2U && counters.malformed_or_unsupported == 1U);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS); CHECK(check_report(view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int test_counters(const char *scenario, uint64_t received, uint64_t dropped, uint64_t malformed)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *first = NULL; ams_mel_ir_image_metadata_event *second = NULL;
    const ams_mel_ir_image_metadata_event_v1 *first_view = NULL; const ams_mel_ir_image_metadata_event_v1 *second_view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(open_all(scenario, &session, &stream) == EXIT_SUCCESS);
    if (strcmp(scenario, "image-metadata-allocation") == 0) CHECK(setenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == received && counters.events_dropped_queue_full == dropped && counters.malformed_or_unsupported == malformed);
    CHECK(receive(metadata, &first, &first_view) == EXIT_SUCCESS);
    CHECK(first_view->bad_pixel_list.pixels.size == 3U);
    if (strcmp(scenario, "image-metadata-allocation") == 0)
        CHECK(first_view->bad_pixel_list.pixels.data[0].row == 10U);
    else
        CHECK(first_view->bad_pixel_list.pixels.data[0].row == 1U);
    if (strcmp(scenario, "image-metadata-overflow") == 0) {
        CHECK(receive(metadata, &second, &second_view) == EXIT_SUCCESS);
        CHECK(second_view->bad_pixel_list.pixels.data[0].row == 2U);
        CHECK(ams_mel_ir_image_metadata_event_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_image_metadata_event_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int test_metadata_close_before_callback(void)
{
    char log[] = "/tmp/ams-image-metadata-log-XXXXXX";
    char barrier[] = "/tmp/ams-image-metadata-barrier-XXXXXX";
    char start[256], entered[256];
    char contents[4096];
    FILE *file;
    size_t count;
    int descriptor = mkstemp(log);
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_image_metadata *metadata = NULL;
    CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    descriptor = mkstemp(barrier); CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(snprintf(start, sizeof start, "%s.start", barrier) > 0);
    CHECK(snprintf(entered, sizeof entered, "%s.entered", barrier) > 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER", barrier, 1) == 0);
    CHECK(open_all("image-metadata-nonquiescing", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(start, "start") == EXIT_SUCCESS);
    CHECK(wait_for_file(entered) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(log, "image_metadata_owner_closed") == EXIT_SUCCESS);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_METADATA_CALLBACK_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    file = fopen(log, "rb"); CHECK(file != NULL);
    count = fread(contents, 1, sizeof contents - 1U, file); contents[count] = '\0';
    CHECK(fclose(file) == 0);
    CHECK(strstr(contents, "image_metadata_callback_entered") != NULL);
    CHECK(strstr(contents, "image_metadata_owner_closed") != NULL);
    CHECK(strstr(contents, "image_metadata_callback_returned") != NULL);
    CHECK(strstr(contents, "image_metadata_callback_entered") < strstr(contents, "image_metadata_owner_closed"));
    CHECK(strstr(contents, "image_metadata_owner_closed") < strstr(contents, "image_metadata_callback_returned"));
    CHECK(strstr(contents, "image_metadata_callback_returned") < strstr(contents, "channel_destroyed"));
    CHECK(strstr(contents, "channel_destroyed") < strstr(contents, "library_unloaded"));
    CHECK(unlink(start) == 0); CHECK(unlink(entered) == 0); CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}
static int test_registration_failure(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    CHECK(open_all(scenario, &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == expected);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int check_navigation(const ams_mel_ir_image_metadata_event_v1 *view)
{
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE);
    CHECK(view->navigation_response.system_time_ns == INT64_C(-8765432109));
    CHECK(view->navigation_response.command_id == UINT32_C(0xf1234567));
    CHECK(view->navigation_response.request_id == UINT32_C(0x89abcdef));
    return EXIT_SUCCESS;
}
static int test_navigation_recovery(const char *scenario, int allocation)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *event = NULL; const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    if (allocation) CHECK(setenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(open_all(scenario, &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    if (allocation) CHECK(unsetenv("AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE") == 0);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 2U && counters.malformed_or_unsupported == 1U);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS); CHECK(check_navigation(view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS); return EXIT_SUCCESS;
}
static int test_navigation_fifo_and_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL; ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *events[4] = {0}; const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(open_all("image-metadata-four-mixed", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 6U && counters.events_dropped_queue_full == 2U);
    CHECK(receive(metadata, &events[0], &view) == EXIT_SUCCESS);
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST);
    CHECK(receive(metadata, &events[1], &view) == EXIT_SUCCESS); CHECK(check_report(view) == EXIT_SUCCESS);
    CHECK(receive(metadata, &events[2], &view) == EXIT_SUCCESS); CHECK(check_euler(view) == EXIT_SUCCESS);
    CHECK(receive(metadata, &events[3], &view) == EXIT_SUCCESS); CHECK(check_navigation(view) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_event_view(events[3], &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_navigation(view) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 4U; ++i)
        CHECK(ams_mel_ir_image_metadata_event_close(&events[i], NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
int main(void)
{
    CHECK(test_rich_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_los_rich_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_mixed_los_fifo() == EXIT_SUCCESS);
    CHECK(test_los_recovery("image-metadata-los-report-null", 1) == EXIT_SUCCESS);
    CHECK(test_los_recovery("image-metadata-los-euler-null", 0) == EXIT_SUCCESS);
    CHECK(test_los_allocation_recovery() == EXIT_SUCCESS);
    CHECK(test_navigation_recovery("image-metadata-navigation-null", 0) == EXIT_SUCCESS);
    CHECK(test_navigation_recovery("image-metadata-navigation-allocation", 1) == EXIT_SUCCESS);
    CHECK(test_navigation_fifo_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_counters("image-metadata-malformed", 2, 0, 1) == EXIT_SUCCESS);
    CHECK(test_counters("image-metadata-overflow", 5, 3, 0) == EXIT_SUCCESS);
    CHECK(test_counters("image-metadata-allocation", 2, 0, 1) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-register-fail", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-register-not-supported", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-register-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-report-register-fail", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-euler-register-fail", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-euler-register-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-navigation-register-fail", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_registration_failure("image-metadata-navigation-register-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_concurrent_open_one_shot() == EXIT_SUCCESS);
    CHECK(test_metadata_close_before_callback() == EXIT_SUCCESS);
    puts("PASS: C IR Image metadata contract");
    return EXIT_SUCCESS;
}
