#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <pthread.h>
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

static int create_marker(const char *path)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fputs("release\n", file) >= 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

static int wait_for_marker(const char *path)
{
    const struct timespec delay = {0, 1000000L};
    unsigned attempt;
    for (attempt = 0; attempt < 5000U; ++attempt) {
        if (access(path, F_OK) == 0) return EXIT_SUCCESS;
        (void)nanosleep(&delay, NULL);
    }
    return EXIT_FAILURE;
}

struct hold_barrier {
    char base[64];
    char release[96];
    char callback_done[96];
    char complete[96];
};

static int make_hold_barrier(struct hold_barrier *barrier)
{
    int descriptor;
    CHECK(snprintf(barrier->base, sizeof barrier->base,
                   "/tmp/ams-mel-nav-hold-XXXXXX") > 0);
    descriptor = mkstemp(barrier->base);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier->base) == 0);
    CHECK(snprintf(barrier->release, sizeof barrier->release, "%s.release", barrier->base) > 0);
    CHECK(snprintf(barrier->callback_done, sizeof barrier->callback_done,
                   "%s.callback-done", barrier->base) > 0);
    CHECK(snprintf(barrier->complete, sizeof barrier->complete, "%s.complete", barrier->base) > 0);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER", barrier->base, 1) == 0);
    return EXIT_SUCCESS;
}

/* Regression: a logical Stop must be terminal for frame Receive even while a
   Navigation request keeps physical teardown deferred. Before this was fixed,
   Lifecycle::Stopping was not terminal and both Receive entry points returned
   TIMEOUT instead of STREAM_STOPPED until physical cleanup ran. */
static int test_logical_stop_frame_receive_terminal(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    ams_mel_ir_frame_v1 frame;
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    struct hold_barrier barrier;
    unsigned char pixels[64];
    char path[] = "/tmp/ams-mel-nav-logical-stop-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(make_hold_barrier(&barrier) == EXIT_SUCCESS);
    CHECK(open_stream("navigation-hold", &session, &stream) == EXIT_SUCCESS);
    /* Never started: no frame is queued, so Receive can only report the
       lifecycle state. */
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* Physical teardown is still deferred behind the pending request. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") == NULL);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels;
    frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 0, &snapshot, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(snapshot == NULL);
    /* The request is still outstanding, so this was logical-stop behavior and
       not post-cleanup behavior. */
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    CHECK(create_marker(barrier.release) == EXIT_SUCCESS);
    CHECK(wait_for_marker(barrier.callback_done) == EXIT_SUCCESS);
    CHECK(create_marker(barrier.complete) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier.release) == 0);
    CHECK(unlink(barrier.callback_done) == 0);
    CHECK(unlink(barrier.complete) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Regression: Image metadata must stop logically with the stream. Already
   queued events still drain, but a provider metadata callback delivered after
   the logical Stop (while the provider channel is still attached because a
   Navigation request is pending) must remain safe and must not enqueue. */
static int test_logical_stop_metadata_no_enqueue(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_metadata_event *event = NULL;
    const ams_mel_ir_image_metadata_event_v1 *view = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_ir_metadata_counters_v1 before, after;
    ams_mel_navigation_report_v1 report = rich_report();
    struct hold_barrier barrier;
    char path[] = "/tmp/ams-mel-nav-metadata-stop-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(make_hold_barrier(&barrier) == EXIT_SUCCESS);
    CHECK(open_stream("navigation-hold", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(stream, 8, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    /* Two registration-time events were delivered; drain one now and keep the
       other queued across the logical Stop. */
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 1000, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") == NULL);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    /* Already queued metadata still drains after the logical Stop. */
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE);
    CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &before, NULL, 0, NULL) == AMS_MEL_OK);
    /* The provider now invokes a registered metadata callback after Stop. */
    CHECK(create_marker(barrier.release) == EXIT_SUCCESS);
    CHECK(wait_for_marker(barrier.callback_done) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_get_counters(metadata, &after, NULL, 0, NULL) == AMS_MEL_OK);
    /* The callback entered and returned (counter policy unchanged) but nothing
       was enqueued. */
    CHECK(after.events_received == before.events_received + 1U);
    CHECK(ams_mel_ir_image_metadata_receive(metadata, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL);
    CHECK(create_marker(barrier.complete) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_navigation_request_wait(request, 1000, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &stream) == EXIT_SUCCESS);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "post_stop_metadata_callback_entered") != NULL);
    CHECK(strstr(log, "post_stop_metadata_callback_returned") != NULL);
    CHECK(check_order(log, "post_stop_metadata_callback_entered",
                      "post_stop_metadata_callback_returned") == EXIT_SUCCESS);
    CHECK(check_order(log, "post_stop_metadata_callback_returned",
                      "channel_destroyed") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier.release) == 0);
    CHECK(unlink(barrier.callback_done) == 0);
    CHECK(unlink(barrier.complete) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* ---------------------------------------------------------------------
   Corrective regression: Navigation completion vs. stream Close teardown.

   Before the correction, the asynchronous Navigation completion worker could
   run deferred physical teardown (reading and resetting
   ImageStreamState::channel) concurrently with a public Close that inspected
   the same shared_ptr member, and Close could return the earlier AMS_MEL_OK
   from its logical Stop even though the deferred detach had just failed and
   left the channel attached.

   These tests force the interleaving deterministically with armed barriers
   inside the adapter's cleanup path instead of relying on timing luck.
   --------------------------------------------------------------------- */

struct cleanup_barrier {
    char base[64];
    char close_arm[112];
    char close_reached[112];
    char close_release[112];
    char detach_arm[112];
    char detach_reached[112];
    char detach_release[112];
};

static int make_cleanup_barrier(struct cleanup_barrier *barrier)
{
    int descriptor;
    CHECK(snprintf(barrier->base, sizeof barrier->base,
                   "/tmp/ams-mel-nav-cleanup-XXXXXX") > 0);
    descriptor = mkstemp(barrier->base);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier->base) == 0);
    CHECK(snprintf(barrier->close_arm, sizeof barrier->close_arm,
                   "%s.close-decision.arm", barrier->base) > 0);
    CHECK(snprintf(barrier->close_reached, sizeof barrier->close_reached,
                   "%s.close-decision.reached", barrier->base) > 0);
    CHECK(snprintf(barrier->close_release, sizeof barrier->close_release,
                   "%s.close-decision.release", barrier->base) > 0);
    CHECK(snprintf(barrier->detach_arm, sizeof barrier->detach_arm,
                   "%s.before-detach.arm", barrier->base) > 0);
    CHECK(snprintf(barrier->detach_reached, sizeof barrier->detach_reached,
                   "%s.before-detach.reached", barrier->base) > 0);
    CHECK(snprintf(barrier->detach_release, sizeof barrier->detach_release,
                   "%s.before-detach.release", barrier->base) > 0);
    CHECK(setenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER", barrier->base, 1) == 0);
    return EXIT_SUCCESS;
}

static void remove_marker(const char *path) { (void)unlink(path); }

static void clear_cleanup_barrier(const struct cleanup_barrier *barrier)
{
    remove_marker(barrier->close_arm);
    remove_marker(barrier->close_reached);
    remove_marker(barrier->close_release);
    remove_marker(barrier->detach_arm);
    remove_marker(barrier->detach_reached);
    remove_marker(barrier->detach_release);
    (void)unsetenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER");
}

/* Drives a public Close on a second thread so it can genuinely overlap the
   adapter's own Navigation completion/cleanup thread. */
struct race_close_input {
    ams_mel_ir_stream **stream;
    ams_mel_status_t status;
};

static void *race_close_worker(void *argument)
{
    struct race_close_input *input = (struct race_close_input *)argument;
    input->status = ams_mel_ir_stream_close(input->stream, NULL, 0, NULL);
    return NULL;
}

static int count_occurrences(const char *text, const char *needle)
{
    int total = 0;
    const char *cursor = text;
    while ((cursor = strstr(cursor, needle)) != NULL) { ++total; ++cursor; }
    return total;
}

/* Race A. Deferred cleanup starts from the final Navigation completion and
   fails detachChannel once while a public Close is still in flight and has not
   yet committed its owner-release decision.

   Required: the racing Close must report the failed cleanup rather than a
   stale success and must keep the owner; the provider library must not be
   unloaded while detach ownership is uncertain; a later Close must retry
   detach, clear the owner, and destroy the channel before library unload. Per
   the corrected contract the retry still reports AMS_MEL_PROVIDER_FAILED: the
   retry establishes ownership safety, it does not erase the original provider
   failure. */
static int test_close_races_failed_deferred_cleanup(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    struct hold_barrier hold;
    struct cleanup_barrier barrier;
    struct race_close_input input;
    pthread_t closer;
    char path[] = "/tmp/ams-mel-nav-close-race-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(make_hold_barrier(&hold) == EXIT_SUCCESS);
    CHECK(make_cleanup_barrier(&barrier) == EXIT_SUCCESS);
    CHECK(open_stream("navigation-hold-detach-fail", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    /* Genuinely outstanding: it completes only when this test releases the
       provider-side hold barrier. */
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);

    /* Arm the adapter so the first Close parks after its logical Stop but
       before it commits the owner-release decision. This is the exact window
       in which the old implementation could return a stale AMS_MEL_OK. */
    CHECK(create_marker(barrier.close_arm) == EXIT_SUCCESS);
    input.stream = &stream;
    input.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(pthread_create(&closer, NULL, race_close_worker, &input) == 0);
    /* Close's logical Stop defers physical teardown (a request is pending) and
       the call is now parked at the close-decision barrier. */
    CHECK(wait_for_marker(barrier.close_reached) == EXIT_SUCCESS);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") == NULL);
    CHECK(strstr(log, "channel_detach_failed") == NULL);

    /* With Close still uncommitted, the final request completes, deferred
       physical cleanup starts, and its detachChannel fails exactly once. */
    CHECK(create_marker(hold.release) == EXIT_SUCCESS);
    CHECK(wait_for_marker(hold.callback_done) == EXIT_SUCCESS);
    CHECK(create_marker(hold.complete) == EXIT_SUCCESS);
    /* Request completion itself remains safe and reports the failed deferred
       cleanup instead of crashing, hanging, or claiming success. */
    CHECK(ams_mel_ir_navigation_request_wait(request, 2000, &result, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);

    /* Detach failed, so the channel is still attached and nothing may have
       been destroyed or unloaded. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detach_failed") != NULL);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Release the parked Close. It now synchronizes with the failed cleanup
       and must not return the stale AMS_MEL_OK from its own logical Stop. */
    CHECK(create_marker(barrier.close_release) == EXIT_SUCCESS);
    CHECK(pthread_join(closer, NULL) == 0);
    CHECK(input.status == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream != NULL);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Second Close retries detach, which now succeeds: ownership safety is
       established and the public owner is released. */
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") != NULL);
    CHECK(strstr(log, "channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    /* The failed detach precedes the successful retry, the channel is
       destroyed only once ownership is proven, and the provider library is
       unloaded only after channel destruction. */
    CHECK(check_order(log, "channel_detach_failed", "channel_detached") == EXIT_SUCCESS);
    CHECK(check_order(log, "channel_detached", "channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "channel_destroyed", "library_unloaded") == EXIT_SUCCESS);

    clear_cleanup_barrier(&barrier);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    remove_marker(hold.release);
    remove_marker(hold.callback_done);
    remove_marker(hold.complete);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Race B. The final request completion's deferred cleanup is genuinely
   in progress (parked inside the adapter immediately before detachChannel,
   holding cleanup ownership) when a public Close arrives on another thread,
   and that cleanup ultimately succeeds.

   Required: Close synchronizes with the in-progress cleanup instead of
   inspecting channel concurrently or returning early on stale state, cleanup
   occurs exactly once, and the owner is cleared exactly once. */
static int test_close_races_successful_deferred_cleanup(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *request = NULL;
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_navigation_report_v1 report = rich_report();
    struct hold_barrier hold;
    struct cleanup_barrier barrier;
    struct race_close_input input;
    pthread_t closer;
    char path[] = "/tmp/ams-mel-nav-close-race-ok-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(make_hold_barrier(&hold) == EXIT_SUCCESS);
    CHECK(make_cleanup_barrier(&barrier) == EXIT_SUCCESS);
    CHECK(open_stream("navigation-hold", &session, &stream) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(request, 0, &result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    /* Logical Stop while the request is pending: physical teardown is deferred
       to the final completion. */
    CHECK(ams_mel_ir_stream_stop(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detached") == NULL);

    /* Park the deferred cleanup just before detachChannel, so it provably
       holds cleanup ownership while Close runs. */
    CHECK(create_marker(barrier.detach_arm) == EXIT_SUCCESS);
    CHECK(create_marker(hold.release) == EXIT_SUCCESS);
    CHECK(wait_for_marker(hold.callback_done) == EXIT_SUCCESS);
    CHECK(create_marker(hold.complete) == EXIT_SUCCESS);
    /* The completion worker is now inside cleanup, before detach. */
    CHECK(wait_for_marker(barrier.detach_reached) == EXIT_SUCCESS);

    /* Close now arrives concurrently with that in-progress cleanup. It must
       block on the cleanup rather than race it on channel. */
    input.stream = &stream;
    input.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(pthread_create(&closer, NULL, race_close_worker, &input) == 0);
    {
        /* Close is now contending with cleanup ownership. Release the parked
           cleanup so it can finish detach and publish its result. */
        const struct timespec delay = {0, 50000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(create_marker(barrier.detach_release) == EXIT_SUCCESS);
    CHECK(pthread_join(closer, NULL) == 0);

    /* Close did not return early on stale state; it adopted the successful
       cleanup outcome and cleared the owner exactly once. */
    CHECK(input.status == AMS_MEL_OK);
    CHECK(stream == NULL);
    CHECK(ams_mel_ir_navigation_request_wait(request, 2000, &result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* Idempotent: a repeat Close on the cleared owner must not run cleanup
       a second time. */
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(stream == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    /* Cleanup occurred exactly once. */
    CHECK(count_occurrences(log, "channel_detached") == 1);
    CHECK(count_occurrences(log, "channel_destroyed") == 1);
    CHECK(strstr(log, "channel_detach_failed") == NULL);
    CHECK(check_order(log, "navigation_completed", "channel_detached") == EXIT_SUCCESS);
    CHECK(check_order(log, "channel_destroyed", "library_unloaded") == EXIT_SUCCESS);

    clear_cleanup_barrier(&barrier);
    CHECK(unsetenv("AMS_MEL_TEST_IMAGE_NAVIGATION_HOLD_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    remove_marker(hold.release);
    remove_marker(hold.callback_done);
    remove_marker(hold.complete);
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
    CHECK(test_logical_stop_frame_receive_terminal() == EXIT_SUCCESS);
    CHECK(test_logical_stop_metadata_no_enqueue() == EXIT_SUCCESS);
    CHECK(test_two_simultaneous_requests() == EXIT_SUCCESS);
    CHECK(test_close_races_successful_deferred_cleanup() == EXIT_SUCCESS);
    CHECK(test_close_races_failed_deferred_cleanup() == EXIT_SUCCESS);
    /* These intentionally leak a permanently-retained SessionState/provider
       library reference (Task 027B fail-safe retention). Run them last so
       earlier library_unloaded assertions are not affected by the shared
       per-process dlopen reference count. */
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    puts("PASS: C IR Image NavigationReport request contract");
    return EXIT_SUCCESS;
}
