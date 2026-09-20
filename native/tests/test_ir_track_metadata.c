#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return EXIT_FAILURE; } } while (0)

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

static ams_mel_ir_track_config_v1 configuration(void)
{
    ams_mel_ir_track_config_v1 value;
    size_t index;
    memset(&value, 0, sizeof value);
    value.channel_id.descriptive_label = text("IR track channel");
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    value.platform_id.descriptive_label = text("test platform");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    value.sensor_location.key = text("station-1");
    value.sensor_location.system_name = text("mock-aircraft");
    for (index = 0; index < 16U; ++index) {
        value.channel_id.uuid[index] = (uint8_t)index;
        value.platform_id.uuid[index] = (uint8_t)(0xf0U + index);
    }
    return value;
}

static int open_track(const char *scenario, ams_mel_session **session,
                      ams_mel_ir_track **track)
{
    ams_mel_ir_track_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(*session, &config, track, NULL, 0, NULL) == AMS_MEL_OK);
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

static int ordered(const char *data, const char *first, const char *second)
{
    const char *a = strstr(data, first);
    const char *b = strstr(data, second);
    CHECK(a != NULL && b != NULL && a < b);
    return EXIT_SUCCESS;
}

/* Only the two CandidateObject callbacks remain deferred; the mock records each
 * one, so their absence is provable. These metadata-only scenarios additionally
 * never submit a send, so no send is expected here either. The
 * @Optional RequestSystemTrackData registration is now a legitimate,
 * positively implemented surface and is deliberately NOT a violation: the
 * adapter registers it alongside the required report callback on every
 * metadata open. Its delivery is asserted separately, per scenario. */
static int no_deferred_track_operations(const char *data)
{
    CHECK(strstr(data, "track_deferred_operation_invoked") == NULL);
    CHECK(strstr(data, "track_candidate_object_registered") == NULL);
    CHECK(strstr(data, "track_candidate_object_preproc_registered") == NULL);
    CHECK(strstr(data, "track_data_update_sent") == NULL);
    CHECK(strstr(data, "track_system_track_data_response_sent") == NULL);
    return EXIT_SUCCESS;
}

/* Exact all-field fidelity of the distinctive rich IRSTTrackReport. */
static int check_rich_report(const ams_mel_ir_track_report_v1 *report)
{
    CHECK(report->system_time_ns == INT64_C(-1234567890123));
    CHECK(report->activity_id == UINT32_C(0xF1234567));

    CHECK(report->measured_ned.north == -1.25);
    CHECK(report->measured_ned.east == 2.5);
    CHECK(report->measured_ned.down == -3.75);
    CHECK(report->measured_intensity == 4.125);
    CHECK(report->measured_snr == -5.25);

    CHECK(report->filtered_ned.north == 6.5);
    CHECK(report->filtered_ned.east == -7.75);
    CHECK(report->filtered_ned.down == 8.875);
    CHECK(report->filtered_intensity == -9.125);
    CHECK(report->filtered_snr == 10.25);

    CHECK(report->range_m == 123456.75);
    CHECK(report->range_error_m == 654.5);
    CHECK(report->spatial_extent_rad == 0.0125);
    CHECK(report->track_quality == 0.875);
    CHECK(report->clutter == -0.5);

    CHECK(report->age_ns == INT64_C(9876543210));

    CHECK(report->state == AMS_MEL_IR_TRACK_STATE_COAST);
    CHECK(report->mode == AMS_MEL_IR_TRACK_MODE_STARE);
    return EXIT_SUCCESS;
}

static int counters_are(ams_mel_ir_track_metadata *metadata, uint64_t received,
                        uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 counters;
    memset(&counters, 0, sizeof counters);
    CHECK(ams_mel_ir_track_metadata_get_counters(metadata, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.events_received == received);
    CHECK(counters.events_dropped_queue_full == dropped);
    CHECK(counters.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

/* Receives one event and closes it. With out_activity the arrival index is
 * reported instead of asserting the unmodified rich activity_id. */
static int receive_rich(ams_mel_ir_track_metadata *metadata, uint32_t *out_activity)
{
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(event != NULL);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    if (out_activity) *out_activity = view->track_report.activity_id;
    else CHECK(check_rich_report(&view->track_report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

/* The mock emits the rich report synchronously from inside
 * registerMetadataCallback, while the Track channel is only attached. */
static int test_attached_registration_rich_report(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    /* Registration is valid before Enable. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(metadata != NULL);
    /* The synchronously delivered report is already queued. */
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    /* Zero timeout on an empty active queue is a non-blocking poll. */
    {
        ams_mel_ir_track_metadata_event *event = NULL;
        CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
              AMS_MEL_TIMEOUT);
        CHECK(event == NULL);
    }
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(metadata == NULL);
    /* Close is idempotent. */
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Registration is equally valid while enabled, and remains one-shot. */
static int test_enabled_registration_and_one_shot(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata *second = NULL;
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    /* Upstream declares no unregister, so a second attempt is refused. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &second, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(second == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    /* Still refused after the public owner is gone. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &second, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(second == NULL);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A non-Success registration and a throwing registration both leave no public
 * owner, and both still consume the single permitted attempt. */
static int test_registration_failures(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track("track-report-register-fail", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    session = NULL; track = NULL; metadata = NULL;
    CHECK(open_track("track-report-register-throw", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A null payload and each unknown enum value are malformed independently: each
 * is counted, none is queued, and the subscription stays usable. */
static int malformed_scenario(const char *scenario)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track(scenario, &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 1U, 0U, 1U) == EXIT_SUCCESS);
    /* Nothing was queued, and the subscription is not poisoned. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_malformed_payloads(void)
{
    /* A null IRSTTrackReport pointer is malformed. */
    CHECK(malformed_scenario("track-report-null") == EXIT_SUCCESS);
    /* Unknown state and unknown mode are tested independently. */
    CHECK(malformed_scenario("track-report-bad-state") == EXIT_SUCCESS);
    CHECK(malformed_scenario("track-report-bad-mode") == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Six reports into a capacity-2 queue: DROP-INCOMING keeps the first two in
 * arrival order and drops the last four. */
static int test_overflow_fifo(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    uint32_t activity = 0xffffffffU;
    CHECK(open_track("track-report-overflow", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 2U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 6U, 4U, 0U) == EXIT_SUCCESS);
    /* The retained pair is the FIRST two, proving drop-incoming and not
     * drop-oldest, and it is returned in arrival order. */
    CHECK(receive_rich(metadata, &activity) == EXIT_SUCCESS);
    CHECK(activity == 0U);
    CHECK(receive_rich(metadata, &activity) == EXIT_SUCCESS);
    CHECK(activity == 1U);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A failing adapter event allocation inside the provider callback must not let
 * an exception escape; the metadata fails and Receive reports it. */
static int test_allocation_failpoint(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(setenv("AMS_MEL_TEST_TRACK_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_CALLBACK_FAILURE") == 0);
    return EXIT_SUCCESS;
}

/* Track Close while metadata is still open stops public consumption. */
static int test_track_close_stops_metadata(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    /* The already queued event still drains first. */
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL);
    /* Counters remain readable after the channel is gone. */
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Session parent-first close: the Track owner keeps the graph alive and the
 * metadata remains usable until the Track owner releases it. */
static int test_parent_first_close(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Argument validation for all six new entry points. */
static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    char buffer[8];
    CHECK(ams_mel_ir_track_metadata_open(NULL, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_receive(NULL, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_get_counters(NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_event_view(NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_event_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);

    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    /* A zero capacity queue is rejected. */
    CHECK(ams_mel_ir_track_metadata_open(track, 0U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    /* A null diagnostic buffer with a non-zero capacity is rejected. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, sizeof buffer,
          NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    /* Neither rejection consumed the single permitted attempt. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, buffer, sizeof buffer,
          NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* A non-null output owner is rejected rather than overwritten. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A failed detach preserves the complete callback graph, fails the metadata,
 * and permits a second Close to retry detach successfully. */
static int test_detach_failure_preserves_callback_state(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track("track-detach-fail", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(track != NULL);
    /* The queued event survives the failed detach. */
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    /* Quiescence was never established, so the metadata is failed. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* The second Close retries detach, which now succeeds. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* The provider may still invoke the retained IRSTTrackReport callback after the
 * public metadata owner is closed. The callback must enter and return safely,
 * queue nothing, and complete strictly before TrackChannel destruction. The
 * mock uses an ordered event log rather than any timing-sensitive sleep. */
static int run_late_callback_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-report-late", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    /* Close public consumption before the provider emits again. */
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_callback_after_metadata_close(void)
{
    char path[] = "/tmp/ams-track-late-XXXXXX";
    char data[8192];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_late_callback_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    /* Registration happened, and the synchronous first report was emitted. */
    CHECK(ordered(data, "track_report_registered", "track_report_emitted_rich") ==
          EXIT_SUCCESS);
    /* The late callback ran after public close and returned safely. */
    CHECK(ordered(data, "track_late_callback_entered",
                  "track_late_callback_returned") == EXIT_SUCCESS);
    /* The provider callback returned strictly before TrackChannel destruction,
     * which is the callback-quiescence boundary. */
    CHECK(ordered(data, "track_late_callback_returned",
                  "track_channel_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "track_channel_destroyed", "control_destroyed") ==
          EXIT_SUCCESS);
    /* Provider unload remains strictly last. */
    CHECK(ordered(data, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(no_deferred_track_operations(data) == EXIT_SUCCESS);
    unlink(path);
    return EXIT_SUCCESS;
}

/* Writes a test-side marker into the same ordered lifetime log the mock uses,
 * so parent-side ordering checks can place adapter actions relative to provider
 * destruction events without any sleep. */
static int mark(const char *log, const char *event)
{
    FILE *file = fopen(log, "ab");
    CHECK(file != NULL);
    CHECK(fprintf(file, "%s\n", event) > 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

/* An open public metadata wrapper must NOT extend provider lifetime. With the
 * public Session already closed, Track Close alone must destroy the provider
 * TrackChannel, the Control, the manager, and unload the provider library --
 * all strictly before metadata_close -- while the still-open wrapper keeps
 * draining its own already-owned events and counters. Runs in a child process
 * so a provider already loaded by the parent cannot hide unload behavior. */
static int run_metadata_does_not_retain_provider_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-report", &session, &track) == EXIT_SUCCESS);
    /* Synchronous registration queues the rich report. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    /* Session is closed first; the Track owner still holds the graph. */
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    /* Track Close with the metadata owner still OPEN must release everything. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track == NULL);
    CHECK(mark(log, "track_close_returned_metadata_open") == EXIT_SUCCESS);
    /* Provider code is already unloaded here; only adapter-owned storage is
     * touched from this point on. */
    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL);
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(mark(log, "metadata_close_begin") == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(metadata == NULL);
    return EXIT_SUCCESS;
}

static int test_metadata_does_not_retain_provider(void)
{
    char path[] = "/tmp/ams-track-owner-XXXXXX";
    char data[8192];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_metadata_does_not_retain_provider_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    CHECK(ordered(data, "track_report_registered", "track_report_emitted_rich") ==
          EXIT_SUCCESS);
    /* Track Close alone tore the whole provider graph down, in order. */
    CHECK(ordered(data, "track_channel_destroyed", "control_destroyed") ==
          EXIT_SUCCESS);
    CHECK(ordered(data, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    /* Every one of those happened before Track Close returned, and therefore
     * strictly before metadata_close, while the wrapper was still OPEN. */
    CHECK(ordered(data, "library_unloaded", "track_close_returned_metadata_open") ==
          EXIT_SUCCESS);
    CHECK(ordered(data, "track_close_returned_metadata_open",
                  "metadata_close_begin") == EXIT_SUCCESS);
    CHECK(no_deferred_track_operations(data) == EXIT_SUCCESS);
    unlink(path);
    return EXIT_SUCCESS;
}

int main(void)
{
    /* The forking lifetime tests run before any test that can permanently
     * retain a provider graph in this process: emergency retention keeps the
     * provider library loaded, and a forked child would inherit it. */
    CHECK(test_callback_after_metadata_close() == EXIT_SUCCESS);
    CHECK(test_metadata_does_not_retain_provider() == EXIT_SUCCESS);
    CHECK(test_attached_registration_rich_report() == EXIT_SUCCESS);
    CHECK(test_enabled_registration_and_one_shot() == EXIT_SUCCESS);
    CHECK(test_registration_failures() == EXIT_SUCCESS);
    CHECK(test_malformed_payloads() == EXIT_SUCCESS);
    CHECK(test_overflow_fifo() == EXIT_SUCCESS);
    CHECK(test_allocation_failpoint() == EXIT_SUCCESS);
    CHECK(test_track_close_stops_metadata() == EXIT_SUCCESS);
    CHECK(test_parent_first_close() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_detach_failure_preserves_callback_state() == EXIT_SUCCESS);
    puts("PASS: native IR Track IRSTTrackReport metadata contract");
    return EXIT_SUCCESS;
}
