#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    value.channel_id.descriptive_label = view("IR image channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    value.buffer_count = 2;
    value.buffer_size = 64;
    value.queue_capacity = 2;
    return value;
}

static ams_mel_navigation_report_v1 rich_report(void)
{
    ams_mel_navigation_report_v1 value;
    memset(&value, 0, sizeof value);
    value.system_time_ns = -123456789012LL;
    value.state = AMS_MEL_POSITION_SOLUTION_BLENDED;
    value.latitude_rad = 0.523598;
    value.longitude_rad = -1.308997;
    value.altitude_m = 987.5;
    value.attitude.roll = 0.1; value.attitude.pitch = 0.2; value.attitude.yaw = 0.3;
    value.attitude_rate.attitude_rate.roll = 0.11;
    value.attitude_rate.attitude_rate.pitch = -0.22;
    value.attitude_rate.attitude_rate.yaw = 0.33;
    value.attitude_rate.attitude_rate_time_ns = -424242;
    value.speed.north = 10.0; value.speed.east = -20.0; value.speed.down = 30.0;
    value.acceleration.north = -1.0; value.acceleration.east = 2.0; value.acceleration.down = -3.0;
    value.wander_angle_rad = 0.05;
    value.magnetic_heading = 12.5;
    value.altitude_msl = 1000.25;
    value.position_velocity_covariance_uncertainty.position_position_pn_pn = 1.5;
    value.position_velocity_covariance_uncertainty.position_position_pn_pe = 2.5;
    value.position_velocity_covariance_uncertainty.position_position_pn_pd = 3.5;
    value.position_velocity_covariance_uncertainty.position_position_pe_pe = 4.5;
    value.position_velocity_covariance_uncertainty.position_position_pe_pd = 5.5;
    value.position_velocity_covariance_uncertainty.position_position_pd_pd = 6.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pn_vn = 7.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pn_ve = 8.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pn_vd = 9.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pe_ve = 10.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pe_vd = 11.5;
    value.position_velocity_covariance_uncertainty.position_velocity_pd_vd = 12.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_vn_vn = 13.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_vn_ve = 14.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_vn_vd = 15.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_ve_ve = 16.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_ve_vd = 17.5;
    value.position_velocity_covariance_uncertainty.velocity_velocity_vd_vd = 18.5;
    return value;
}

static int open_stream(const char *scenario, ams_mel_session **session,
                       ams_mel_ir_stream **stream)
{
    ams_mel_ir_stream_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(*session, &config, stream, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_all(ams_mel_session **session, ams_mel_ir_stream **stream)
{
    CHECK(ams_mel_ir_stream_close(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*stream == NULL);
    CHECK(ams_mel_session_close(session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int read_log(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t size;
    CHECK(file != NULL);
    size = fread(buffer, 1, capacity - 1U, file);
    CHECK(!ferror(file));
    CHECK(fclose(file) == 0);
    buffer[size] = '\0';
    return EXIT_SUCCESS;
}

static int check_order(const char *text, const char *first, const char *second)
{
    const char *a = strstr(text, first);
    const char *b = strstr(text, second);
    CHECK(a != NULL && b != NULL && a < b);
    return EXIT_SUCCESS;
}

static int test_success_and_fidelity(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    memset(&result, 0xff, sizeof result);
    CHECK(open_stream("navigation-fidelity", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(result.response.system_time_ns == -8765432109LL);
    CHECK(result.response.command_id == 0xf1234567U);
    CHECK(result.response.request_id == 0x89abcdefU);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.system_time_ns == -8765432109LL);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_submission_before_start(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    /* Submission must be valid while Attached, before Start. */
    CHECK(open_stream("navigation-sync", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_submission_after_stop(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-sync", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-sync", &session, &stream) == EXIT_SUCCESS);
    report.state = AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE;
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    report.state = AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE + 10U;
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    report.state = AMS_MEL_POSITION_SOLUTION_BLENDED;
    CHECK(ams_mel_ir_stream_submit_navigation_report(NULL, &report, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, NULL, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, NULL,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    request = (ams_mel_ir_navigation_request *)(void *)1;
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    request = NULL;
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_send_throw(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-send-throw", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(request == NULL);
    /* A failed submission must not leak a request accounting slot. */
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_null_success(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-null-result", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_rejection(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    char diagnostic[128] = {0};
    CHECK(open_stream("navigation-reject", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, diagnostic,
          sizeof diagnostic, NULL) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS);
    CHECK(strcmp(diagnostic, "invalid navigation report") == 0);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_long_rejection(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result = {{0}, 0};
    ams_mel_navigation_report_v1 report = rich_report();
    char expected[614];
    char short_diagnostic[512];
    char *complete;
    size_t required = 0;
    memset(expected, 'x', 510U);
    expected[510] = (char)0xe2; expected[511] = (char)0x82; expected[512] = (char)0xac;
    memset(expected + 513, 'y', 100U);
    expected[613] = '\0';
    CHECK(open_stream("navigation-reject-long", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0,
          &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(required == sizeof expected);
    memset(short_diagnostic, 0x7f, sizeof short_diagnostic);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, short_diagnostic,
          sizeof short_diagnostic, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strlen(short_diagnostic) == 510U);
    CHECK(memcmp(short_diagnostic, expected, 510U) == 0);
    complete = (char *)malloc(required);
    CHECK(complete != NULL);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, complete,
          required, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strcmp(complete, expected) == 0);
    free(complete);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_future_throw(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-future-throw", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    char path[] = "/tmp/ams-mel-nav-post-send-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("navigation-lifetime", &session, &stream) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_NAVIGATION_POST_SEND_FAILURE", failpoint, 1) == 0);
    {
        char diagnostic[128] = {0};
        const ams_mel_status_t status = ams_mel_ir_stream_submit_navigation_report(
            stream, &report, &request, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_INTERNAL_ERROR)
            fprintf(stderr, "post-send %s returned %d: %s\n", failpoint,
                    (int)status, diagnostic);
        CHECK(status == AMS_MEL_INTERNAL_ERROR);
    }
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_NAVIGATION_POST_SEND_FAILURE") == 0);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 100000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "navigation_sent") != NULL);
    CHECK(strstr(log, "navigation_completed") != NULL);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}


static int test_session_parent_first_close(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    char path[] = "/tmp/ams-mel-nav-parent-first-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("navigation-lifetime", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(stream == NULL);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(result.response.command_id == 0xf1234567U);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(result.response.command_id == 0xf1234567U);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 100000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(check_order(log, "channel_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}


static int test_request_close_while_pending(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_navigation_report_v1 report = rich_report();
    char path[] = "/tmp/ams-mel-nav-close-pending-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("navigation-pending-close", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "navigation_sent") != NULL);
    CHECK(strstr(log, "navigation_completed") != NULL);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(check_order(log, "navigation_sent", "navigation_completed") == EXIT_SUCCESS);
    CHECK(check_order(log, "navigation_completed", "channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "channel_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}


static int test_synchronous_metadata_callback(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *event = NULL;
    const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    CHECK(open_stream("navigation-sync", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 8, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    /* Submission is synchronous with a metadata callback registered; no
       deadlock must occur, and the submit call itself must return normally. */
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 1000, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE);
    CHECK(view->navigation_response.system_time_ns == -8765432109LL);
    CHECK(view->navigation_response.command_id == 0xf1234567U);
    CHECK(view->navigation_response.request_id == 0x89abcdefU);
    {
        const ams_mel_ir_navigation_response_v1 callback_response = view->navigation_response;
        CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.response.system_time_ns == callback_response.system_time_ns);
        CHECK(result.response.command_id == callback_response.command_id);
        CHECK(result.response.request_id == callback_response.request_id);
    }
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Regression: a Navigation request that completes while the stream is still
   logically Attached/Running must never perform provider teardown. Before this
   was guarded, final request completion detached the channel underneath a
   still-usable stream, breaking a subsequent Start/Receive. */
static int test_completion_without_stop_keeps_stream_usable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    ams_mel_ir_frame_v1 frame;
    char path[] = "/tmp/ams-mel-nav-no-stop-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("navigation-sync", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* No Stop/Close happened, so the provider channel must still be attached. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") == NULL);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    /* The stream must remain fully usable afterwards. */
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&frame, 0, sizeof frame);
    frame.pixel_capacity = 0;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) ==
          AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_two_simultaneous_requests(void)
{
    /* The mock provider supports only one delayed thread per channel
       instance, so drive two independent requests on the same channel by
       submitting a delayed request and a synchronous request while the first
       remains outstanding; both must be independently accounted and the
       stream must only fully tear down once both complete. */
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *first = NULL;
    ams_mel_ir_navigation_request *second = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    char path[] = "/tmp/ams-mel-nav-two-pending-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_stream("navigation-delayed", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &first,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(first, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &second,
          NULL, 0, NULL) == AMS_MEL_OK);
    /* Close defers teardown while either request remains outstanding. */
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(second, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(first, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_success_and_fidelity() == EXIT_SUCCESS);
    CHECK(test_submission_before_start() == EXIT_SUCCESS);
    CHECK(test_submission_after_stop() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_send_throw() == EXIT_SUCCESS);
    CHECK(test_null_success() == EXIT_SUCCESS);
    CHECK(test_rejection() == EXIT_SUCCESS);
    CHECK(test_long_rejection() == EXIT_SUCCESS);
    CHECK(test_future_throw() == EXIT_SUCCESS);
    CHECK(test_session_parent_first_close() == EXIT_SUCCESS);
    CHECK(test_request_close_while_pending() == EXIT_SUCCESS);
    CHECK(test_synchronous_metadata_callback() == EXIT_SUCCESS);
    CHECK(test_completion_without_stop_keeps_stream_usable() == EXIT_SUCCESS);
    CHECK(test_two_simultaneous_requests() == EXIT_SUCCESS);
    /* These intentionally leak a permanently-retained SessionState/provider
       library reference (Task 027B fail-safe retention). Run them last so
       earlier library_unloaded assertions are not affected by the shared
       per-process dlopen reference count. */
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    puts("PASS: C IR Image NavigationReport request contract");
    return EXIT_SUCCESS;
}
