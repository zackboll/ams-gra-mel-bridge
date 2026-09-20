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

static void fill_id(ams_mel_uci_id_v1 *id, uint8_t seed, const char *label)
{
    size_t index;
    for (index = 0; index < 16U; ++index)
        id->uuid[index] = (uint8_t)(seed + index * 3U);
    id->descriptive_label = view(label);
}

/* One distinctive TrackDataUpdate covering every upstream field, with 21
 * distinct covariance values so any swapped term is detectable. */
static ams_mel_ir_track_data_update_v1 rich_update(void)
{
    ams_mel_ir_track_data_update_v1 value;
    memset(&value, 0, sizeof value);
    value.platform_id = UINT32_C(0xF1234567);
    fill_id(&value.capability_uuid, 0x10U, "capability-\xCE\xB1");
    fill_id(&value.activity_uuid, 0x40U, "activity-\xCE\xB2");
    value.track_id = UINT32_C(0xE2345678);
    fill_id(&value.entity_uuid, 0x70U, "entity-\xE2\x82\xAC");
    value.track_status = AMS_MEL_IR_TRACK_STATUS_PREDICT;
    value.time_of_validity_seconds = -12345.25;
    value.time_of_last_update_seconds = 1700000000.875;

    value.track_position_ecef.x = -1.25;
    value.track_position_ecef.y = 2.5;
    value.track_position_ecef.z = -3.75;
    value.track_velocity_ecef.x = 4.125;
    value.track_velocity_ecef.y = -5.25;
    value.track_velocity_ecef.z = 6.5;

    value.covariance.xx = 1.01;
    value.covariance.xy = 2.02;
    value.covariance.xz = 3.03;
    value.covariance.x_vx = 4.04;
    value.covariance.x_vy = 5.05;
    value.covariance.x_vz = 6.06;
    value.covariance.yy = 7.07;
    value.covariance.yz = 8.08;
    value.covariance.y_vx = 9.09;
    value.covariance.y_vy = 10.10;
    value.covariance.y_vz = 11.11;
    value.covariance.zz = 12.12;
    value.covariance.z_vx = 13.13;
    value.covariance.z_vy = 14.14;
    value.covariance.z_vz = 15.15;
    value.covariance.vx_vx = 16.16;
    value.covariance.vx_vy = 17.17;
    value.covariance.vx_vz = 18.18;
    value.covariance.vy_vy = 19.19;
    value.covariance.vy_vz = 20.20;
    value.covariance.vz_vz = 21.21;

    value.maneuver_probability = 0.625;
    value.track_quality = 12.75;
    return value;
}

/* Exact fidelity of the distinctive successful provider CommandStatus. */
static int rich_status_matches(const ams_mel_ir_command_status_v1 *status)
{
    static const char expected[] = "Track update accepted \xC2\xB5";
    CHECK(status->command_id == UINT32_C(0xF0E1D2C3));
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

/* Complete input fidelity including all 21 covariance terms: the mock verifies
 * every field through the published getters and throws otherwise, so a
 * successful rich CommandStatus proves the whole mapping. */
static int test_rich_submit_and_fidelity(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    memset(&result, 0xff, sizeof result);
    CHECK(open_enabled("track-update-rich", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    /* The terminal result is cached; a repeated Wait(0) is identical and the
       reason description still points into request-owned storage. */
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    /* Close is idempotent. */
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update;
    static const char bad[] = {(char)0xC3, (char)0x28};
    CHECK(open_enabled("track-update-any", &session, &channel) == EXIT_SUCCESS);

    update = rich_update();
    CHECK(ams_mel_ir_track_submit_update(NULL, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_submit_update(channel, NULL, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, NULL,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);

    /* One past Delete: upstream declares no MaxExclusive TrackStatus value. */
    update = rich_update();
    update.track_status = AMS_MEL_IR_TRACK_STATUS_DELETE + 1U;
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);

    /* Each of the three UCI labels is validated. */
    update = rich_update();
    update.capability_uuid.descriptive_label.data = bad;
    update.capability_uuid.descriptive_label.size = sizeof bad;
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    update = rich_update();
    update.activity_uuid.descriptive_label.data = bad;
    update.activity_uuid.descriptive_label.size = sizeof bad;
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    update = rich_update();
    update.entity_uuid.descriptive_label.data = bad;
    update.entity_uuid.descriptive_label.size = sizeof bad;
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* An embedded NUL is rejected as well. */
    update = rich_update();
    update.entity_uuid.descriptive_label = view("label");
    update.entity_uuid.descriptive_label.size = 6U;
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);

    CHECK(ams_mel_ir_track_update_request_wait(NULL, 0, &result,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_update_request_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Submission requires Enabled; an attached-only channel is rejected. */
static int test_submit_before_enable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char diagnostic[128] = {0};
    CHECK(open_channel("track-update-any", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(strstr(diagnostic, "not enabled") != NULL);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* A successful future whose CommandStatus state is itself Rejected is STILL
 * AMS_MEL_OK: CommandStatus::Rejected is not an ErrorOr rejection. */
static int test_status_rejected_is_ok(void)
{
    static const char expected[] = "Track update parameters rejected \xC2\xB5";
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    memset(&result, 0xff, sizeof result);
    CHECK(open_enabled("track-update-status-rejected", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(result.status.command_id == UINT32_C(0xF0E1D2C3));
    CHECK(result.status.state == AMS_MEL_IR_COMMAND_REJECTED);
    CHECK(result.status.reason_id ==
          AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER);
    CHECK(result.status.reason_description.size == sizeof expected - 1U);
    CHECK(memcmp(result.status.reason_description.data, expected,
                 sizeof expected - 1U) == 0);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* ErrorOr rejection: the complete diagnostic is recoverable and the status
 * record is ignored. */
static int test_error_or_rejection(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char small[32] = {0};
    size_t required = 0;
    memset(&result, 0, sizeof result);
    CHECK(open_enabled("track-update-reject", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_wait(request, 2000, &result,
          small, sizeof small, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS);
    /* The truncated copy is still valid UTF-8 and the complete size is known. */
    CHECK(required > sizeof small);
    CHECK(strlen(small) < sizeof small);
    {
        /* The cached terminal result yields the complete diagnostic on retry. */
        char *complete = malloc(required);
        ams_mel_ir_track_update_result_v1 again;
        size_t again_required = 0;
        CHECK(complete != NULL);
        memset(complete, 0, required);
        memset(&again, 0, sizeof again);
        CHECK(ams_mel_ir_track_update_request_wait(request, 0, &again,
              complete, required, &again_required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(again_required == required);
        CHECK(again.error_code == result.error_code);
        CHECK(strlen(complete) == required - 1U);
        CHECK(strstr(complete, "Track update rejected \xC2\xB5") != NULL);
        free(complete);
    }
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Each malformed or failing provider outcome fails closed with its own
 * terminal status. */
static int test_terminal_outcome(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char diagnostic[256] = {0};
    memset(&result, 0, sizeof result);
    CHECK(open_enabled(scenario, &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    {
        const ams_mel_status_t status = ams_mel_ir_track_update_request_wait(
            request, 2000, &result, diagnostic, sizeof diagnostic, NULL);
        if (status != expected)
            fprintf(stderr, "%s returned %d: %s\n", scenario, (int)status, diagnostic);
        CHECK(status == expected);
    }
    /* Repeated Wait returns the identical cached terminal result. */
    {
        ams_mel_ir_track_update_result_v1 again;
        memset(&again, 0, sizeof again);
        CHECK(ams_mel_ir_track_update_request_wait(request, 0, &again,
              NULL, 0, NULL) == expected);
    }
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* A throwing provider send() never yields a request owner. */
static int test_send_throw(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char diagnostic[256] = {0};
    CHECK(open_enabled("track-update-send-throw", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(request == NULL);
    CHECK(strstr(diagnostic, "mock Track update send exception") != NULL);
    /* The failed send released its request accounting, so Close is synchronous. */
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Deterministic pending future: the mock completion waits on a test-controlled
 * barrier file, never on a sleep. Timeout is only "not ready yet". */
static int test_timeout_then_success(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char barrier[] = "/tmp/ams-mel-track-barrier-XXXXXX";
    char log[8192];
    char path[] = "/tmp/ams-mel-track-timeout-XXXXXX";
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    /* mkstemp creates the barrier, so remove it: its existence is the signal. */
    descriptor = mkstemp(barrier);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-update-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    /* The provider send already happened, but no result is ready. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_data_update_sent") != NULL);
    memset(&result, 0xab, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Timeout left the caller's record untouched. */
    CHECK(result.status.command_id == UINT32_C(0xABABABAB));
    CHECK(ams_mel_ir_track_update_request_wait(request, 20, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);

    /* Release the barrier; the same request now yields its terminal result. */
    CHECK(touch(barrier) == EXIT_SUCCESS);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 5000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);

    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* The provider invokes the registered IRSTTrackReport callback synchronously
 * from inside send(TrackDataUpdate). This is why the provider send must occur
 * outside TrackState::mutex: no deadlock, the report arrives, and the request
 * still completes. */
static int test_synchronous_metadata_during_send(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *snapshot = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char log[8192];
    char path[] = "/tmp/ams-mel-track-reentrant-XXXXXX";
    int descriptor = mkstemp(path);
    unsigned seen = 0;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);

    CHECK(open_enabled("track-update-reentrant", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(channel, 4U, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);

    /* The report emitted from inside send() is queued and readable. */
    while (ams_mel_ir_track_metadata_receive(metadata, 100, &event,
                                             NULL, 0, NULL) == AMS_MEL_OK) {
        CHECK(ams_mel_ir_track_metadata_event_view(event, &snapshot,
              NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(snapshot->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
        CHECK(snapshot->track_report.activity_id == UINT32_C(0xF1234567));
        CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        ++seen;
    }
    CHECK(seen >= 1U);

    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);

    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_update_send_callback_entered") != NULL);
    CHECK(strstr(log, "track_update_send_callback_returned") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    CHECK(strstr(log, "track_deferred_operation_invoked") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Parent/child lifetime: Session and Track public owners close successfully
 * while an update is pending because physical teardown is deferred behind the
 * request. The provider channel and library stay alive until completion. */
static int test_parent_first_close(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char barrier[] = "/tmp/ams-mel-track-life-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-lifetime-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    descriptor = mkstemp(barrier);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-update-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);

    /* Session first, then Track: both public owners close cleanly. */
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);

    /* Before completion, the provider channel and library are still alive. */
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);

    CHECK(touch(barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_wait(request, 5000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    /* The result is still readable from adapter-owned cached storage. */
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_status_matches(&result.status) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 300000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    /* Deferred cleanup ran: disable, detach, channel destruction (which is the
       callback-quiescence boundary), then provider library release, in order. */
    CHECK(strstr(log, "track_disabled") != NULL);
    CHECK(strstr(log, "channel_detached") != NULL);
    {
        const char *completed = strstr(log, "track_update_completed");
        const char *destroyed = strstr(log, "track_channel_destroyed");
        const char *manager = strstr(log, "manager_destroyed");
        const char *unloaded = strstr(log, "library_unloaded");
        CHECK(completed != NULL && destroyed != NULL);
        CHECK(manager != NULL && unloaded != NULL);
        CHECK(completed < destroyed);
        CHECK(destroyed < manager);
        CHECK(manager < unloaded);
    }
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}
/* Request Close is not cancellation: provider work still completes, deferred
 * Track cleanup still runs, and nothing is destroyed prematurely. */
static int test_request_close_is_not_cancel(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char barrier[] = "/tmp/ams-mel-track-cancel-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-cancel-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    descriptor = mkstemp(barrier);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-update-pending", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Drop the public request owner while the provider work is pending. */
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
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
    CHECK(strstr(log, "track_update_completed") != NULL);
    CHECK(strstr(log, "track_channel_destroyed") != NULL);
    CHECK(strstr(log, "library_unloaded") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* Task 029B1/B2 synchronous detach-failure semantics are unchanged when there
 * is no pending request: the public owner stays non-null and retry is possible. */
static int test_synchronous_detach_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char diagnostic[256] = {0};
    memset(&result, 0, sizeof result);
    CHECK(open_enabled("track-detach-fail", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* No request pending: the first detach fails synchronously and the owner
       survives so a second Close can retry. */
    CHECK(ams_mel_ir_track_close(&channel, diagnostic, sizeof diagnostic, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(channel != NULL);
    CHECK(strstr(diagnostic, "detach failed") != NULL);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Deferred detach failure after the public Track owner is already gone: the
 * complete graph is retained through the existing allocation-free emergency
 * root and the request's terminal result becomes AMS_MEL_PROVIDER_FAILED. */
static int test_deferred_detach_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_update_result_v1 result;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char barrier[] = "/tmp/ams-mel-track-defer-barrier-XXXXXX";
    char path[] = "/tmp/ams-mel-track-defer-XXXXXX";
    char diagnostic[256] = {0};
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    descriptor = mkstemp(barrier);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER", barrier, 1) == 0);

    CHECK(open_enabled("track-update-detach-fail", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_submit_update(channel, &update, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Teardown is deferred, so the public owner is released successfully. */
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(touch(barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_update_request_wait(request, 5000, &result,
          diagnostic, sizeof diagnostic, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strstr(diagnostic, "deferred Track cleanup failed") != NULL);
    /* The cached terminal result repeats identically. */
    CHECK(ams_mel_ir_track_update_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_track_update_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 300000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "channel_detach_failed") != NULL);
    /* The graph is retained permanently: nothing was destroyed or unloaded. */
    CHECK(strstr(log, "track_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

/* After the provider send returned a future, either failpoint must return
 * AMS_MEL_INTERNAL_ERROR, expose no public request owner, and retain the
 * future/provider graph safely. The retention is deliberately permanent: the
 * provider library is NOT unloaded and no recovery is claimed. */
static int test_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL; ams_mel_ir_track *channel = NULL;
    ams_mel_ir_track_update_request *request = NULL;
    ams_mel_ir_track_data_update_v1 update = rich_update();
    char path[] = "/tmp/ams-mel-track-post-send-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_enabled("track-update-rich", &session, &channel) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_TRACK_UPDATE_POST_SEND_FAILURE", failpoint, 1) == 0);
    {
        char diagnostic[256] = {0};
        const ams_mel_status_t status = ams_mel_ir_track_submit_update(
            channel, &update, &request, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_INTERNAL_ERROR)
            fprintf(stderr, "post-send %s returned %d: %s\n", failpoint,
                    (int)status, diagnostic);
        CHECK(status == AMS_MEL_INTERNAL_ERROR);
    }
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_UPDATE_POST_SEND_FAILURE") == 0);
    /* No public request escapes, but the provider send already happened. */
    CHECK(request == NULL);
    CHECK(ams_mel_ir_track_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "track_data_update_sent") != NULL);
    /* The provider future is retained safely: the library is NOT unloaded. */
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_rich_submit_and_fidelity() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_submit_before_enable() == EXIT_SUCCESS);
    CHECK(test_status_rejected_is_ok() == EXIT_SUCCESS);
    CHECK(test_error_or_rejection() == EXIT_SUCCESS);
    /* Provider-malformed and provider-failure outcomes each fail closed. */
    CHECK(test_terminal_outcome("track-update-null-status",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-update-bad-state",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-update-bad-reason",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-update-bad-description",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-update-unknown-error",
                                AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("track-update-future-throw",
                                AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_send_throw() == EXIT_SUCCESS);
    CHECK(test_timeout_then_success() == EXIT_SUCCESS);
    CHECK(test_synchronous_metadata_during_send() == EXIT_SUCCESS);
    CHECK(test_parent_first_close() == EXIT_SUCCESS);
    CHECK(test_request_close_is_not_cancel() == EXIT_SUCCESS);
    CHECK(test_synchronous_detach_failure() == EXIT_SUCCESS);
    /* These intentionally retain a provider graph permanently (fail-safe
       retention). Run them last so earlier library_unloaded assertions and
       ordering evidence are unaffected. */
    CHECK(test_deferred_detach_failure() == EXIT_SUCCESS);
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    puts("PASS: C IR Track TrackDataUpdate contract");
    return EXIT_SUCCESS;
}
