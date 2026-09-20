#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stddef.h>
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

/* Only the @Optional CandidateObjectPreProcMessage callback remains deferred;
 * the mock records it, so its absence is provable. These metadata-only
 * scenarios additionally never submit a send, so no send is expected here
 * either.
 *
 * The @Optional RequestSystemTrackData registration and the
 * @RequiredIfDetectCandidateObjects CandidateObjectMessage registration are
 * both legitimate, positively implemented surfaces and are deliberately NOT
 * violations. These scenarios do not advertise CandidateObjectMessage, so the
 * adapter must skip that registration entirely, which is asserted here. */
static int no_deferred_track_operations(const char *data)
{
    CHECK(strstr(data, "track_deferred_operation_invoked") == NULL);
    CHECK(strstr(data, "track_candidate_object_registered") == NULL);
    CHECK(strstr(data, "track_candidate_object_preproc_registered") == NULL);
    CHECK(strstr(data, "track_data_update_sent") == NULL);
    CHECK(strstr(data, "track_system_track_data_response_sent") == NULL);
    return EXIT_SUCCESS;
}

/* Exact all-field fidelity of the distinctive rich CandidateObjectMessage.
 * Every published getter of the header, of each HotRegion, of the
 * SensorInertialState, and of each exposed CandidateObject is asserted, and the
 * exposed prefix length is asserted to be exactly numberOfCOs. */
static int check_rich_candidate(const ams_mel_ir_candidate_object_message_v1 *m)
{
    size_t index;
    CHECK(m->header.number_of_cos == 3U);
    CHECK(m->header.stack_frame_index == 0xBEEFU);
    /* binary32 fidelity: compared against the exact float literal, so any
     * widening to double and back would be visible. */
    CHECK(m->header.cfar == 1.5309e-7F);
    CHECK(m->header.validity_flag_bitfield == 0xA5C3U);
    CHECK(m->header.tov_utc_ns == INT64_C(-4433221100998877));

    /* Complete HotRegion vector, in published order. */
    CHECK(m->hot_regions.size == 3U);
    CHECK(m->hot_regions.data != NULL);
    CHECK(m->hot_regions.data[0].type == AMS_MEL_IR_HOT_REGION_FLARE);
    CHECK(m->hot_regions.data[0].size == 1111U);
    CHECK(m->hot_regions.data[0].top == 2222U);
    CHECK(m->hot_regions.data[0].left == 3333U);
    CHECK(m->hot_regions.data[0].right == 4444U);
    CHECK(m->hot_regions.data[0].bottom == 5555U);
    CHECK(m->hot_regions.data[1].type == AMS_MEL_IR_HOT_REGION_SOLAR);
    CHECK(m->hot_regions.data[1].size == 6666U);
    CHECK(m->hot_regions.data[1].top == 7777U);
    CHECK(m->hot_regions.data[1].left == 8888U);
    CHECK(m->hot_regions.data[1].right == 9999U);
    CHECK(m->hot_regions.data[1].bottom == 10111U);
    CHECK(m->hot_regions.data[2].type == AMS_MEL_IR_HOT_REGION_MASK);
    CHECK(m->hot_regions.data[2].size == 12222U);
    CHECK(m->hot_regions.data[2].top == 13333U);
    CHECK(m->hot_regions.data[2].left == 14444U);
    CHECK(m->hot_regions.data[2].right == 15555U);
    CHECK(m->hot_regions.data[2].bottom == 16666U);

    /* Complete SensorInertialState through the canonical shared record. */
    CHECK(m->inertial_state.system_time_ns == INT64_C(-1122334455667788));
    CHECK(m->inertial_state.q_xyzw.x == 0.125);
    CHECK(m->inertial_state.q_xyzw.y == -0.25);
    CHECK(m->inertial_state.q_xyzw.z == 0.375);
    CHECK(m->inertial_state.q_xyzw.w == -0.5);
    CHECK(m->inertial_state.q_ecef_xyzw.x == -0.625);
    CHECK(m->inertial_state.q_ecef_xyzw.y == 0.75);
    CHECK(m->inertial_state.q_ecef_xyzw.z == -0.875);
    CHECK(m->inertial_state.q_ecef_xyzw.w == 1.125);
    CHECK(m->inertial_state.sensor_position.x == 1234567.25);
    CHECK(m->inertial_state.sensor_position.y == -2345678.5);
    CHECK(m->inertial_state.sensor_position.z == 3456789.75);
    CHECK(m->inertial_state.sensor_velocity.x == -11.125);
    CHECK(m->inertial_state.sensor_velocity.y == 22.25);
    CHECK(m->inertial_state.sensor_velocity.z == -33.375);
    CHECK(m->inertial_state.uncertainties.sensor_uncertainties == 0xC0FFEE01U);
    CHECK(m->inertial_state.uncertainties.platform_uncertainties == 0xDEADBE02U);

    /* Exactly the meaningful prefix is exposed: 3, not the 900 storage
     * slots. The sentinel in slot 3 is therefore unreachable. */
    CHECK(m->candidate_objects.size == 3U);
    CHECK(m->candidate_objects.data != NULL);
    for (index = 0; index < 3U; ++index) {
        const ams_mel_ir_candidate_object_v1 *object =
            &m->candidate_objects.data[index];
        CHECK(object->system_time_ns ==
              INT64_C(-1000000000000) - (int64_t)index * INT64_C(7));
        CHECK(object->detection_category == 0x11110000U + (uint32_t)index);
        CHECK(object->sensor_index == 0x22220000U + (uint32_t)index);
        CHECK(object->subpixel.row == 100.5 + (double)index);
        CHECK(object->subpixel.column == 200.25 + (double)index);
        CHECK(object->intensity == 3000.125 + (double)index);
        CHECK(object->sensor_relative_unit.x == 0.1 + (double)index);
        CHECK(object->sensor_relative_unit.y == -0.2 - (double)index);
        CHECK(object->sensor_relative_unit.z == 0.3 + (double)index);
        CHECK(object->signal_to_interference_ratio == 40.5 + (double)index);
        CHECK(object->signal_to_noise_ratio == -50.75 - (double)index);
        /* No exposed entry may be the sentinel that lives in slot 3. */
        CHECK(object->detection_category != 0xFFFFFFFFU);
        CHECK(object->intensity != -77777.5);
    }
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
    /* The v2 view shares the v1 view's exact argument-validation contract. */
    CHECK(ams_mel_ir_track_metadata_event_view_v2(NULL, NULL, NULL, 0, NULL) ==
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

/* Receives one CandidateObjectMessage and asserts complete fidelity through
 * the v2 view, which is the only view that declares the candidate payload.
 * The same event is also viewed through the unchanged frozen v1 record: v1
 * reports kind 3 and keeps its own members zeroed, and it deliberately has no
 * candidate payload at all. */
static int receive_rich_candidate(ams_mel_ir_track_metadata *metadata)
{
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    const ams_mel_ir_track_metadata_event_v2 *view2 = NULL;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(event != NULL);
    CHECK(ams_mel_ir_track_metadata_event_view_v2(event, &view2, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view2->base.kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
    CHECK(check_rich_candidate(&view2->candidate_object_message) == EXIT_SUCCESS);
    /* The unselected members stay zeroed, including every unselected span. */
    CHECK(view2->base.track_report.activity_id == 0U);
    CHECK(view2->base.request_system_track_data.command_id == 0U);
    /* The unchanged v1 view still works and still returns the same storage. */
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view == &view2->base);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
    CHECK(view->track_report.activity_id == 0U);
    CHECK(view->request_system_track_data.command_id == 0U);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

/* Frozen-v1 compatibility proof, independent of any provider interaction.
 *
 * ABI policy prohibits appending fields to an existing fixed-layout record.
 * Task 029E had already appended request_system_track_data to v1; that layout
 * is grandfathered and frozen here, and the candidate payload went into v2
 * instead. These assertions fail if v1 ever grows again. */
static int test_v1_layout_is_frozen(void)
{
    ams_mel_ir_track_metadata_event_v1 v1;
    ams_mel_ir_track_metadata_event_v2 v2;
    memset(&v1, 0, sizeof v1);
    memset(&v2, 0, sizeof v2);
    /* v1 contains exactly kind, track_report, and request_system_track_data:
     * its size is the padded end of exactly those three members. Appending a
     * fourth member necessarily grows sizeof v1 past that bound. */
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v1, kind) == 0U);
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v1, track_report) >=
          sizeof v1.kind);
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v1, request_system_track_data) >=
          offsetof(ams_mel_ir_track_metadata_event_v1, track_report) +
              sizeof v1.track_report);
    CHECK(sizeof v1 ==
          offsetof(ams_mel_ir_track_metadata_event_v1, request_system_track_data) +
              sizeof v1.request_system_track_data);
    CHECK(sizeof(ams_mel_ir_track_metadata_event_v1) <
          sizeof(ams_mel_ir_track_metadata_event_v2));

    /* v2 is additive over the frozen v1 and reuses it verbatim. */
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v2, base) == 0U);
    CHECK(sizeof v2.base == sizeof v1);
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v2, candidate_object_message) >=
          sizeof v1);
    CHECK(sizeof v2 ==
          offsetof(ams_mel_ir_track_metadata_event_v2, candidate_object_message) +
              sizeof v2.candidate_object_message);
    /* base.kind is the one discriminator and sits at offset 0 of both. */
    CHECK(offsetof(ams_mel_ir_track_metadata_event_v2, base) +
              offsetof(ams_mel_ir_track_metadata_event_v1, kind) == 0U);
    return EXIT_SUCCESS;
}

/* Capability advertised and registration succeeds: the message is delivered
 * synchronously from inside registration and is complete. */
static int test_candidate_advertised_success(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track("track-candidate-rich", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(receive_rich_candidate(metadata) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Advertised but refused: NotSupported, Fail, and an unknown/future Return
 * value each FAIL CLOSED, because the advertisement promised the type. A
 * throwing registration maps to PROVIDER_EXCEPTION. In every case no public
 * owner escapes and the one-shot attempt stays consumed. */
static int candidate_registration_failure(const char *scenario,
                                          ams_mel_status_t expected)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track(scenario, &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          expected);
    CHECK(metadata == NULL);
    /* One-shot rule remains consumed after a registration failure. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    /* Teardown with the previously registered IRST callback still retained
     * remains safe. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_candidate_registration_failures(void)
{
    CHECK(candidate_registration_failure("track-candidate-register-not-supported",
                                         AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(candidate_registration_failure("track-candidate-register-fail",
                                         AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(candidate_registration_failure("track-candidate-register-unknown",
                                         AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(candidate_registration_failure("track-candidate-register-throw",
                                         AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Each malformed CandidateObjectMessage is counted once, enqueues nothing, and
 * leaves the subscription usable. Reuses the shared malformed harness, which
 * asserts exactly received=1, dropped=0, malformed=1. */
static int test_candidate_malformed(void)
{
    /* Null payload. */
    CHECK(malformed_scenario("track-candidate-null") == EXIT_SUCCESS);
    /* numberOfCOs = 901 > MAX_CANDIDATE_OBJECTS. */
    CHECK(malformed_scenario("track-candidate-too-many") == EXIT_SUCCESS);
    /* HotRegion enum one past MASK. */
    CHECK(malformed_scenario("track-candidate-bad-region") == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Asynchronous delivery strictly after registration returned. */
static int test_candidate_async(void)
{
    char path[] = "/tmp/ams-track-cand-XXXXXX";
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    close(fd);
    unlink(path);
    CHECK(setenv("AMS_MEL_TEST_TRACK_CANDIDATE_BARRIER", path, 1) == 0);
    CHECK(open_track("track-candidate-async", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Nothing arrived during registration. */
    CHECK(counters_are(metadata, 0U, 0U, 0U) == EXIT_SUCCESS);
    {
        FILE *file = fopen(path, "wb");
        CHECK(file != NULL);
        CHECK(fclose(file) == 0);
    }
    /* Blocking receive: the adapter waits on a condition variable. */
    {
        ams_mel_ir_track_metadata_event *event = NULL;
        const ams_mel_ir_track_metadata_event_v2 *view = NULL;
        CHECK(ams_mel_ir_track_metadata_receive(metadata, 10000U, &event, NULL, 0,
              NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_track_metadata_event_view_v2(event, &view, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(view->base.kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
        CHECK(check_rich_candidate(&view->candidate_object_message) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) ==
              AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_CANDIDATE_BARRIER") == 0);
    unlink(path);
    return EXIT_SUCCESS;
}

/* Six candidate messages into a capacity-2 queue: DROP-INCOMING keeps the
 * first two in arrival order on the one shared queue. */
static int test_candidate_overflow(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    uint16_t index;
    CHECK(open_track("track-candidate-overflow", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 2U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 6U, 4U, 0U) == EXIT_SUCCESS);
    for (index = 0; index < 2U; ++index) {
        const ams_mel_ir_track_metadata_event_v2 *view = NULL;
        CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0,
              NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_track_metadata_event_view_v2(event, &view, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(view->base.kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
        /* Arrival order is carried in stackFrameIndex. */
        CHECK(view->candidate_object_message.header.stack_frame_index == index);
        /* Each retained event still owns its own complete storage. */
        CHECK(view->candidate_object_message.hot_regions.size == 3U);
        CHECK(view->candidate_object_message.candidate_objects.size == 3U);
        CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) ==
              AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* The three implemented kinds through the two views side by side:
 * - the v1 view still works for IRSTTrackReport and RequestSystemTrackData
 * - a candidate event through v1 exposes kind 3 with no candidate payload
 * - the v2 view exposes the complete CandidateObjectMessage
 * - v2.base.kind equals the CandidateObjectMessage kind
 * The v1 view's output contract does not grow. */
static int test_v1_view_compatibility(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    const ams_mel_ir_track_metadata_event_v2 *view2 = NULL;
    CHECK(open_track("track-candidate-mixed", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 8U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);

    /* IRSTTrackReport through the unchanged v1 view. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    CHECK(check_rich_report(&view->track_report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);

    /* CandidateObjectMessage through BOTH views. */
    view = NULL;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* v1 sees the discriminator but carries no candidate payload. */
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
    CHECK(view->track_report.activity_id == 0U);
    CHECK(view->request_system_track_data.system_time_ns == 0);
    CHECK(ams_mel_ir_track_metadata_event_view_v2(event, &view2, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view2->base.kind == AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE);
    CHECK(&view2->base == view);
    CHECK(check_rich_candidate(&view2->candidate_object_message) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);

    /* RequestSystemTrackData through the unchanged v1 view. */
    view = NULL;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    CHECK(view->request_system_track_data.command_id == 0xC1234567U);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* All three implemented kinds on the one shared FIFO, in a deterministic
 * order: IRSTTrackReport, CandidateObjectMessage, RequestSystemTrackData. */
static int test_candidate_mixed_fifo(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(open_track("track-candidate-mixed", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 8U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* One shared counter set across all three kinds. */
    CHECK(counters_are(metadata, 3U, 0U, 0U) == EXIT_SUCCESS);

    CHECK(receive_rich(metadata, NULL) == EXIT_SUCCESS);
    CHECK(receive_rich_candidate(metadata) == EXIT_SUCCESS);

    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    CHECK(view->request_system_track_data.command_id == 0xC1234567U);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A candidate event acquired before teardown must remain fully readable after
 * metadata close, Track close, Session close, provider channel destruction, and
 * provider library unload. Runs in a child process so a provider already loaded
 * by the parent cannot hide unload behavior. */
static int run_candidate_lifetime_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v2 *view = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-candidate-lifetime", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Acquire the event owner, then tear everything else down. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view_v2(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(check_rich_candidate(&view->candidate_object_message) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mark(log, "candidate_event_only_owner") == EXIT_SUCCESS);
    /* Provider code is unloaded here; only event-owned storage is touched.
     * Header, both spans, and the inertial state must be unchanged. */
    CHECK(check_rich_candidate(&view->candidate_object_message) == EXIT_SUCCESS);
    CHECK(mark(log, "candidate_event_close_begin") == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

static int test_candidate_event_lifetime(void)
{
    char path[] = "/tmp/ams-track-cand-life-XXXXXX";
    char data[8192];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_candidate_lifetime_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    CHECK(ordered(data, "track_candidate_object_registered",
                  "track_candidate_emitted_lifetime") == EXIT_SUCCESS);
    /* The whole provider graph was destroyed and the library unloaded strictly
     * before the event became the only remaining owner. */
    CHECK(ordered(data, "track_channel_destroyed", "control_destroyed") ==
          EXIT_SUCCESS);
    CHECK(ordered(data, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(ordered(data, "library_unloaded", "candidate_event_only_owner") ==
          EXIT_SUCCESS);
    CHECK(ordered(data, "candidate_event_only_owner",
                  "candidate_event_close_begin") == EXIT_SUCCESS);
    /* CandidateObjectPreProcMessage remains the only deferred surface. */
    CHECK(strstr(data, "track_candidate_object_preproc_registered") == NULL);
    CHECK(strstr(data, "track_deferred_operation_invoked") == NULL);
    unlink(path);
    return EXIT_SUCCESS;
}

/* After public metadata close the provider invokes the retained candidate
 * callback: it enters, returns, queues nothing, lets no exception escape, and
 * returns strictly before TrackChannel destruction. */
static int run_candidate_late_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-candidate-late", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Nothing was emitted during registration for this scenario. */
    CHECK(counters_are(metadata, 0U, 0U, 0U) == EXIT_SUCCESS);
    /* Close public consumption before the provider emits. */
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_candidate_late_callback(void)
{
    char path[] = "/tmp/ams-track-cand-late-XXXXXX";
    char data[8192];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_candidate_late_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    /* The late callback entered and returned safely after public close. */
    CHECK(ordered(data, "track_candidate_late_callback_entered",
                  "track_candidate_late_callback_returned") == EXIT_SUCCESS);
    /* It returned strictly before TrackChannel destruction, which is the
     * callback-quiescence boundary, and unload remains strictly last. */
    CHECK(ordered(data, "track_candidate_late_callback_returned",
                  "track_channel_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "track_channel_destroyed", "control_destroyed") ==
          EXIT_SUCCESS);
    CHECK(ordered(data, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(strstr(data, "track_candidate_object_preproc_registered") == NULL);
    CHECK(strstr(data, "track_deferred_operation_invoked") == NULL);
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
    CHECK(test_candidate_event_lifetime() == EXIT_SUCCESS);
    CHECK(test_candidate_late_callback() == EXIT_SUCCESS);
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
    /* @RequiredIfDetectCandidateObjects CandidateObjectMessage. */
    CHECK(test_candidate_advertised_success() == EXIT_SUCCESS);
    CHECK(test_candidate_async() == EXIT_SUCCESS);
    CHECK(test_candidate_registration_failures() == EXIT_SUCCESS);
    CHECK(test_candidate_malformed() == EXIT_SUCCESS);
    CHECK(test_candidate_overflow() == EXIT_SUCCESS);
    CHECK(test_candidate_mixed_fifo() == EXIT_SUCCESS);
    /* Task 029F ABI versioning: v1 is frozen, v2 is additive. */
    CHECK(test_v1_layout_is_frozen() == EXIT_SUCCESS);
    CHECK(test_v1_view_compatibility() == EXIT_SUCCESS);
    puts("PASS: native IR Track metadata contract");
    return EXIT_SUCCESS;
}
