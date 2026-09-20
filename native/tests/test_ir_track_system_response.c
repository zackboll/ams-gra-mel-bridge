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

static ams_mel_ir_track_config_v1 configuration(void)
{
    ams_mel_ir_track_config_v1 value;
    size_t index;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    value.channel_id.descriptive_label = view("IR track channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    for (index = 0; index < 16U; ++index) {
        value.channel_id.uuid[index] = (uint8_t)index;
        value.platform_id.uuid[index] = (uint8_t)(0xf0U + index);
    }
    return value;
}

/* One distinctive SystemTrackDataResponse covering every upstream field, with
 * values that cannot be confused with a default, an adjacent field, or a
 * sign/scale mistake. Both AzEl pairs are radians and both published bool
 * values start valid. */
static ams_mel_ir_system_track_data_response_v1 rich_response(void)
{
    ams_mel_ir_system_track_data_response_v1 value;
    memset(&value, 0, sizeof value);
    value.system_time_ns = INT64_C(-8765432109876);

    value.command_id = UINT32_C(0xF1234567);
    value.request_id = UINT32_C(0xE2345678);
    value.track_id = UINT32_C(0xD3456789);

    value.range_m = 123456.75;
    value.range_rate_mps = -456.125;
    value.range_error_m = 12.5;
    value.range_rate_error_mps = -0.875;

    value.az_el_valid = 1U;
    value.range_valid = 1U;

    value.inertial_az_el.azimuth_rad = -1.25;
    value.inertial_az_el.elevation_rad = 0.625;
    value.az_el_error.azimuth_rad = 0.03125;
    value.az_el_error.elevation_rad = -0.015625;
    return value;
}

/* The distinctive TrackDataUpdate reused only by the mixed-request test. Its
 * exact payload is not re-verified there; the update contract test owns that. */
static ams_mel_ir_track_data_update_v1 any_update(void)
{
    ams_mel_ir_track_data_update_v1 value;
    memset(&value, 0, sizeof value);
    value.track_status = AMS_MEL_IR_TRACK_STATUS_UPDATE;
    return value;
}

/* Exact fidelity of the distinctive successful provider CommandStatus. It is
 * deliberately different from the TrackDataUpdate status. */
static int rich_status_matches(const ams_mel_ir_command_status_v1 *status)
{
    static const char expected[] = "Track system response accepted \xC2\xB5";
    CHECK(status->command_id == UINT32_C(0xA1B2C3D4));
    CHECK(status->state == AMS_MEL_IR_COMMAND_ACCEPTED);
    CHECK(status->reason_id == AMS_MEL_IR_CANNOT_COMPLY_NOT_SET);
    CHECK(status->reason_description.size == sizeof expected - 1U);
    CHECK(status->reason_description.data != NULL);
    CHECK(memcmp(status->reason_description.data, expected,
                 sizeof expected - 1U) == 0);
    return EXIT_SUCCESS;
}

static int open_channel(const char *scenario, ams_mel_session **session,
                        ams_mel_ir_track **channel)
{
    ams_mel_ir_track_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(*session, &config, channel,
          NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int open_enabled(const char *scenario, ams_mel_session **session,
                        ams_mel_ir_track **channel)
{
    CHECK(open_channel(scenario, session, channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(*channel, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_all(ams_mel_session **session, ams_mel_ir_track **channel)
{
    CHECK(ams_mel_ir_track_close(channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*channel == NULL);
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

static int touch(const char *path)
{
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

/* Reserves a unique path that does not yet exist; its later creation is the
 * deterministic completion signal. */
static int reserve(char *template_path)
{
    int descriptor = mkstemp(template_path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(template_path) == 0);
    return EXIT_SUCCESS;
}

/* Complete input fidelity: the mock verifies every published getter exactly and
 * throws otherwise, so a successful rich CommandStatus proves the whole
 * mapping, including signed nanoseconds, both AzEl pairs, and both bools. */
static int test_rich_submit_and_fidelity(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    memset(&result, 0xff, sizeof result);
    CHECK(open_enabled("track-response-rich", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    /* The terminal result is cached; a repeated Wait(0) is identical and the
       reason description still points into request-owned storage. */
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    /* Close is idempotent. */
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Boolean coverage: az_el_valid = 0 with range_valid = 0 reaches the provider
 * as false/false while every numeric value stays identical. */
static int test_false_flags(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    response.az_el_valid = 0U;
    response.range_valid = 0U;
    memset(&result, 0, sizeof result);
    CHECK(open_enabled("track-response-flags-false", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char one[1] = {0};
    memset(&result, 0, sizeof result);
    CHECK(open_enabled("track-response-any", &session, &channel) == EXIT_SUCCESS);

    CHECK(ams_mel_ir_track_submit_system_track_data_response(NULL, &response,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, NULL,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* A non-null *out_request would be overwritten, so it is rejected. */
    {
        ams_mel_ir_track_system_response_request *occupied =
            (ams_mel_ir_track_system_response_request *)(void *)one;
        CHECK(ams_mel_ir_track_submit_system_track_data_response(channel,
              &response, &occupied, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    }
    /* A null diagnostic buffer with a non-zero capacity is rejected. */
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 16U, NULL) == AMS_MEL_INVALID_ARGUMENT);

    /* Both published bool values accept only 0 or 1; each is rejected
       independently so a shared check cannot hide one of them. */
    response = rich_response();
    response.az_el_valid = 2U;
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    response = rich_response();
    response.range_valid = 2U;
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* 255 is rejected as well: only 0 and 1 are accepted, not "non-zero". */
    response = rich_response();
    response.az_el_valid = 255U;
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    response = rich_response();
    response.range_valid = 255U;
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);

    CHECK(ams_mel_ir_track_system_response_request_wait(NULL, 0, &result,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_system_response_request_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Submission requires Enabled; an attached-only channel is rejected. */
static int test_submit_before_enable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char diagnostic[128] = {0};
    CHECK(open_channel("track-response-any", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(strstr(diagnostic, "not enabled") != NULL);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* A successful future whose CommandStatus state is itself Rejected is STILL
 * AMS_MEL_OK: CommandStatus::Rejected is not an ErrorOr rejection. */
static int test_status_rejected_is_ok(void)
{
    static const char expected[] = "Track system response rejected \xC2\xB5";
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    memset(&result, 0xff, sizeof result);
    CHECK(open_enabled("track-response-status-rejected", &session, &channel) ==
          EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(result.status.command_id == UINT32_C(0xA1B2C3D4));
    CHECK(result.status.state == AMS_MEL_IR_COMMAND_REJECTED);
    CHECK(result.status.reason_id ==
          AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER);
    CHECK(result.status.reason_description.size == sizeof expected - 1U);
    CHECK(memcmp(result.status.reason_description.data, expected,
                 sizeof expected - 1U) == 0);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* ErrorOr rejection: the complete diagnostic is recoverable and the status
 * record is ignored. */
static int test_error_or_rejection(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char small[32] = {0};
    size_t required = 0;
    memset(&result, 0, sizeof result);
    CHECK(open_enabled("track-response-reject", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 2000, &result,
          small, sizeof small, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS);
    /* The truncated copy is still valid UTF-8 and the complete size is known. */
    CHECK(required > sizeof small);
    CHECK(strlen(small) < sizeof small);
    {
        /* The cached terminal result yields the complete diagnostic on retry. */
        char *complete = malloc(required);
        ams_mel_ir_track_system_response_result_v1 again;
        size_t again_required = 0;
        CHECK(complete != NULL);
        memset(complete, 0, required);
        memset(&again, 0, sizeof again);
        CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &again,
              complete, required, &again_required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(again_required == required);
        CHECK(again.error_code == result.error_code);
        CHECK(strlen(complete) == required - 1U);
        CHECK(strstr(complete, "Track system response rejected \xC2\xB5") != NULL);
        free(complete);
    }
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Each malformed or failing provider outcome fails closed with its own
 * terminal status. */
static int test_terminal_outcome(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char diagnostic[256] = {0};
    memset(&result, 0, sizeof result);
    CHECK(open_enabled(scenario, &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const ams_mel_status_t status =
            ams_mel_ir_track_system_response_request_wait(
                request, 2000, &result, diagnostic, sizeof diagnostic, NULL);
        if (status != expected)
            fprintf(stderr, "%s returned %d: %s\n", scenario, (int)status, diagnostic);
        CHECK(status == expected);
    }
    /* Repeated Wait returns the identical cached terminal result. */
    {
        ams_mel_ir_track_system_response_result_v1 again;
        memset(&again, 0, sizeof again);
        CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &again,
              NULL, 0, NULL) == expected);
    }
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* A throwing provider send() never yields a request owner. */
static int test_send_throw(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char diagnostic[256] = {0};
    CHECK(open_enabled("track-response-send-throw", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(request == NULL);
    CHECK(strstr(diagnostic, "mock Track response send exception") != NULL);
    /* The failed send released its request accounting, so Close is synchronous. */
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Deterministic pending future: the mock completion waits on a test-controlled
 * barrier file, never on a sleep. Timeout is only "not ready yet". */
static int test_timeout_then_success(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char barrier[] = "/tmp/ams-mel-track-resp-barrier-XXXXXX";
    char log[8192];
    char path[] = "/tmp/ams-mel-track-resp-timeout-XXXXXX";
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(reserve(barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-response-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    /* The provider send already happened, but no result is ready. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_system_track_data_response_sent") != NULL);
    memset(&result, 0xab, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Timeout left the caller's record untouched. */
    CHECK(result.status.command_id == UINT32_C(0xABABABAB));
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 20, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(result.status.command_id == UINT32_C(0xABABABAB));

    /* Release the barrier; the same request now yields its terminal result. */
    CHECK(touch(barrier) == EXIT_SUCCESS);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 5000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);

    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Pending response lifetime: Session and Track public owners close while the
 * response is pending, the provider graph stays alive, and physical teardown
 * follows completion. */
static int test_pending_response_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char barrier[] = "/tmp/ams-mel-track-resp-life-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-resp-lifetime-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(reserve(barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-response-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);

    /* Session first, then Track: both public owners are released while the
       response is pending, so physical teardown is deferred. */
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    /* The provider graph is still alive: nothing was destroyed or unloaded. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    CHECK(touch(barrier) == EXIT_SUCCESS);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 5000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    /* Physical teardown follows the completion. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_response_completed") != NULL);
    CHECK(strstr(log, "track_disabled") != NULL);
    CHECK(strstr(log, "channel_detached") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Mixed-request regression. One TrackDataUpdate and one
 * SystemTrackDataResponse are pending at the same time on one Track. Closing
 * Session and Track must not tear anything down, and releasing only ONE future
 * must still not tear anything down, because the other request remains. Only
 * after the second future is released may disable, detach, channel destruction,
 * callback quiescence, and library unload happen. This proves both request
 * families really share one request-accounting domain instead of two counters.
 */
static int test_mixed_request_accounting(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    ams_mel_ir_track_update_request *update_request = NULL;
    ams_mel_ir_track_system_response_request *response_request = NULL;
    ams_mel_ir_track_update_result_v1 update_result;
    ams_mel_ir_track_system_response_result_v1 response_result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    ams_mel_ir_track_data_update_v1 update = any_update();
    char update_barrier[] = "/tmp/ams-mel-track-mix-upd-XXXXXX";
    char response_barrier[] = "/tmp/ams-mel-track-mix-rsp-XXXXXX";
    char path[] = "/tmp/ams-mel-track-mixed-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    /* Two independent barriers so each future is released separately. */
    CHECK(reserve(update_barrier) == EXIT_SUCCESS);
    CHECK(reserve(response_barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", update_barrier, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", response_barrier, 1) == 0);

    CHECK(open_enabled("track-mixed-requests", &session, &channel) == EXIT_SUCCESS);
    /* Cross-mechanism: open the metadata subscription so BOTH inbound kinds --
     * the @RequiredIfTrack IRSTTrackReport and the @Optional
     * RequestSystemTrackData -- are delivered while two RequestFor futures are
     * outstanding. Inbound metadata uses the callback/queue architecture and
     * must never participate in async request accounting. */
    CHECK(ams_mel_ir_track_metadata_open(channel, 8U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(metadata != NULL);
    /* One pending TrackDataUpdate and one pending SystemTrackDataResponse. */
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &update_request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &response_request, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&update_result, 0, sizeof update_result);
    memset(&response_result, 0, sizeof response_result);
    CHECK(ams_mel_ir_track_update_request_wait(update_request, 0, &update_result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_track_system_response_request_wait(response_request, 0,
          &response_result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);

    /* Both inbound metadata events were delivered and queued while the two
     * futures are still outstanding. Draining them must NOT change request
     * accounting: metadata is callback/queue based and never contributes an
     * outstanding request. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    CHECK(view->request_system_track_data.command_id == UINT32_C(0xC1234567));
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    /* Closing the metadata subscription is likewise not a request release: the
     * two pending futures alone still hold the entire provider graph. */
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Close both public parents while BOTH requests are pending. */
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Release ONLY the update future. One request still remains, so the
       provider graph must still be completely alive. */
    CHECK(touch(update_barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_wait(update_request, 5000,
          &update_result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_update_completed") != NULL);
    /* The decisive assertion: one request family completing is NOT enough. */
    CHECK(strstr(log, "track_disabled") == NULL);
    CHECK(strstr(log, "channel_detached") == NULL);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    /* Release the second future; only now may teardown proceed. */
    CHECK(touch(response_barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_wait(response_request, 5000,
          &response_result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&response_result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_close(&update_request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_close(&response_request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_response_completed") != NULL);
    CHECK(strstr(log, "track_disabled") != NULL);
    CHECK(strstr(log, "channel_detached") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);

    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(update_barrier) == 0);
    CHECK(unlink(response_barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* The provider invokes the registered IRSTTrackReport callback synchronously
 * from inside send(SystemTrackDataResponse). This is why the provider send must
 * occur outside TrackState::mutex: no deadlock, the report arrives correctly,
 * and the response request still completes correctly. */
static int test_synchronous_metadata_during_send(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *snapshot = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char log[8192];
    char path[] = "/tmp/ams-mel-track-resp-reentrant-XXXXXX";
    int descriptor = mkstemp(path);
    unsigned seen = 0;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);

    CHECK(open_enabled("track-response-reentrant", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(channel, 4U, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);

    /* The report emitted from inside send() is queued and readable. */
    while (ams_mel_ir_track_metadata_receive(metadata, 100, &event,
                                             NULL, 0, NULL) == AMS_MEL_OK) {
        CHECK(ams_mel_ir_track_metadata_event_view(event, &snapshot,
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(snapshot->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
        CHECK(snapshot->track_report.activity_id == UINT32_C(0xF1234567));
        CHECK(snapshot->track_report.range_m == 123456.75);
        CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        ++seen;
    }
    CHECK(seen >= 1U);

    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);

    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_response_send_callback_entered") != NULL);
    CHECK(strstr(log, "track_response_send_callback_returned") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    /* This scenario advertises no CandidateObjectMessage capability, so that
     * one registration must be skipped entirely. */
    CHECK(strstr(log, "track_candidate_object_registered") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Closing the public request owner is not cancellation: the pending provider
 * work still completes, deferred teardown then completes safely, and nothing is
 * unloaded prematurely. */
static int test_request_close_is_not_cancel(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char barrier[] = "/tmp/ams-mel-track-resp-cancel-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-resp-cancel-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(reserve(barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-response-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Drop the public request owner while the provider work is pending. */
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(touch(barrier) == EXIT_SUCCESS);
    {
        const struct timespec delay = {0, 500000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    /* The provider work was NOT cancelled; deferred cleanup then completed. */
    CHECK(strstr(log, "track_response_completed") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Deferred detach failure after the public Track owner is already gone: the
 * complete graph is retained through the existing allocation-free emergency
 * root and the response request's terminal result becomes
 * AMS_MEL_PROVIDER_FAILED with the shared diagnostic. The synchronous
 * detach-retry behavior is not modified by this task and is not exercised here;
 * the TrackDataUpdate contract test still owns that case. */
static int test_deferred_detach_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_track_system_response_result_v1 result;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char barrier[] = "/tmp/ams-mel-track-resp-defer-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-resp-defer-XXXXXX";
    char diagnostic[256] = {0};
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(reserve(barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-response-detach-fail", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_system_track_data_response(channel, &response,
          &request, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Teardown is deferred, so the public owner is released successfully. */
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(touch(barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 5000, &result,
          diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strstr(diagnostic, "deferred Track cleanup failed") != NULL);
    /* The cached terminal result repeats identically. */
    CHECK(ams_mel_ir_track_system_response_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_track_system_response_request_close(&request,
          NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 300000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detach_failed") != NULL);
    /* The graph is retained permanently: nothing was destroyed or unloaded. */
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* After the provider send returned a future, the System-response-specific
 * failpoint must return AMS_MEL_INTERNAL_ERROR, expose no public request owner,
 * and retain the future/provider graph safely. The retention is deliberately
 * permanent: the provider library is NOT unloaded and no recovery is claimed.
 * This uses its own environment variable, so the TrackDataUpdate post-send
 * coverage is untouched. */
static int test_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_system_response_request *request = NULL;
    ams_mel_ir_system_track_data_response_v1 response = rich_response();
    char path[] = "/tmp/ams-mel-track-resp-post-send-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_enabled("track-response-rich", &session, &channel) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_TRACK_RESPONSE_POST_SEND_FAILURE", failpoint, 1) == 0);
    {
        char diagnostic[256] = {0};
        const ams_mel_status_t status =
            ams_mel_ir_track_submit_system_track_data_response(
                channel, &response, &request, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_INTERNAL_ERROR)
            fprintf(stderr, "post-send %s returned %d: %s\n", failpoint,
                    (int)status, diagnostic);
        CHECK(status == AMS_MEL_INTERNAL_ERROR);
    }
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_RESPONSE_POST_SEND_FAILURE") == 0);
    /* No public request escapes, but the provider send already happened. */
    CHECK(request == NULL);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_system_track_data_response_sent") != NULL);
    /* The provider future is retained safely: the library is NOT unloaded. */
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_rich_submit_and_fidelity() == EXIT_SUCCESS);
    CHECK(test_false_flags() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_submit_before_enable() == EXIT_SUCCESS);
    CHECK(test_status_rejected_is_ok() == EXIT_SUCCESS);
    CHECK(test_error_or_rejection() == EXIT_SUCCESS);
    /* Provider-malformed and provider-failure outcomes each fail closed. */
    CHECK(test_terminal_outcome("track-response-null-status",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-response-bad-state",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-response-bad-reason",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-response-bad-description",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-response-unknown-error",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-response-future-throw",
                                AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_send_throw() == EXIT_SUCCESS);
    CHECK(test_timeout_then_success() == EXIT_SUCCESS);
    CHECK(test_pending_response_lifetime() == EXIT_SUCCESS);
    CHECK(test_mixed_request_accounting() == EXIT_SUCCESS);
    CHECK(test_synchronous_metadata_during_send() == EXIT_SUCCESS);
    CHECK(test_request_close_is_not_cancel() == EXIT_SUCCESS);
    /* These intentionally retain a provider graph permanently (fail-safe
       retention). Run them last so earlier library_unloaded assertions and
       ordering evidence are unaffected. */
    CHECK(test_deferred_detach_failure() == EXIT_SUCCESS);
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    puts("PASS: C IR Track SystemTrackDataResponse contract");
    return EXIT_SUCCESS;
}
