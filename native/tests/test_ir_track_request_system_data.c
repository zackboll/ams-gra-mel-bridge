#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int counters_are(const ams_mel_ir_track_metadata *metadata,
                        uint64_t received, uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 counters;
    memset(&counters, 0xff, sizeof counters);
    CHECK(ams_mel_ir_track_metadata_get_counters(metadata, &counters, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters.events_received == received);
    CHECK(counters.events_dropped_queue_full == dropped);
    CHECK(counters.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

/* Exact all-field fidelity of the distinctive rich RequestSystemTrackData.
 * The negative system time proves the signed nanosecond carrier, and each
 * identifier has its high bit set so a narrowing or a field swap is caught. */
static int check_rich_request(const ams_mel_ir_request_system_track_data_v1 *request)
{
    CHECK(request->system_time_ns == INT64_C(-8765432109876));
    CHECK(request->command_id == UINT32_C(0xC1234567));
    CHECK(request->request_id == UINT32_C(0xD2345678));
    CHECK(request->track_id == UINT32_C(0xE3456789));
    return EXIT_SUCCESS;
}

/* Receives one event, requires it to be the RequestSystemTrackData kind, and
 * checks every field. When request_id is non-NULL the caller inspects the
 * arrival ordinal instead of the distinctive value. */
static int receive_request(ams_mel_ir_track_metadata *metadata, uint32_t *request_id)
{
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(event != NULL);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view != NULL);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    if (request_id) *request_id = view->request_system_track_data.request_id;
    else CHECK(check_rich_request(&view->request_system_track_data) == EXIT_SUCCESS);
    /* The event carries only its own payload: the unselected report member of
     * the shared event stays entirely zeroed. */
    CHECK(view->track_report.activity_id == 0U);
    CHECK(view->track_report.system_time_ns == 0);
    CHECK(view->track_report.range_m == 0.0);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

/* The mock emits the rich request synchronously from inside
 * registerMetadataCallback, while the Track channel is only attached. */
static int test_rich_request(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track("track-request-rich", &session, &track) == EXIT_SUCCESS);
    /* Registration is valid before Enable. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(metadata != NULL);
    /* Exactly one event was received: the required report callback emits
     * nothing in this scenario, so the count isolates the optional kind. */
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(receive_request(metadata, NULL) == EXIT_SUCCESS);
    /* Zero timeout on an empty active queue is a non-blocking poll. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A default-constructed request is well formed. Upstream declares no field as
 * optional and no value as out of range, so an all-zero request must be
 * delivered rather than counted as malformed. */
static int test_zero_request(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(open_track("track-request-zero", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    CHECK(view->request_system_track_data.system_time_ns == 0);
    CHECK(view->request_system_track_data.command_id == 0U);
    CHECK(view->request_system_track_data.request_id == 0U);
    CHECK(view->request_system_track_data.track_id == 0U);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A null RequestSystemTrackData pointer is the only malformed payload this
 * type admits: it is counted, nothing is queued, and the subscription stays
 * usable for later events. */
static int test_null_request(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track("track-request-null", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 1U, 0U, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Six requests into a capacity-2 queue: the optional kind obeys the same
 * DROP-INCOMING policy, keeping the first two in arrival order. */
static int test_overflow_fifo(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    uint32_t request_id = 0xffffffffU;
    CHECK(open_track("track-request-overflow", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 2U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(counters_are(metadata, 6U, 4U, 0U) == EXIT_SUCCESS);
    /* The retained pair is the FIRST two, proving drop-incoming. */
    CHECK(receive_request(metadata, &request_id) == EXIT_SUCCESS);
    CHECK(request_id == 0U);
    CHECK(receive_request(metadata, &request_id) == EXIT_SUCCESS);
    CHECK(request_id == 1U);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Both implemented kinds share one queue, one capacity, and one counter set.
 * The interleaved order request/report/request must be preserved exactly, and
 * each delivered event must carry only its own discriminated payload. */
static int test_mixed_kinds(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(open_track("track-metadata-mixed", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 8U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Three events total across the two kinds, all through the one queue. */
    CHECK(counters_are(metadata, 3U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(receive_request(metadata, NULL) == EXIT_SUCCESS);
    /* The middle event is the @RequiredIfTrack report, whose own payload must
     * be intact and whose request member must stay zeroed. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    CHECK(view->track_report.activity_id == UINT32_C(0xF1234567));
    CHECK(view->track_report.system_time_ns == INT64_C(-1234567890123));
    CHECK(view->request_system_track_data.command_id == 0U);
    CHECK(view->request_system_track_data.request_id == 0U);
    CHECK(view->request_system_track_data.track_id == 0U);
    CHECK(view->request_system_track_data.system_time_ns == 0);
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_request(metadata, NULL) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* The @Optional request callback is allowed to be refused. Upstream documents
 * Return::NotSupported as the answer from a provider that does not implement
 * it, and pinned Squall is exactly such a provider. A refusal must NOT fail the
 * metadata open and must NOT disturb the @RequiredIfTrack report callback,
 * which stays fully functional. Fail and NotSupported are checked separately. */
static int optional_refusal_scenario(const char *scenario)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(open_track(scenario, &session, &track) == EXIT_SUCCESS);
    /* The open still succeeds: the required callback is what gates it. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(metadata != NULL);
    /* The required IRSTTrackReport was still delivered. */
    CHECK(counters_are(metadata, 1U, 0U, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT);
    CHECK(view->track_report.activity_id == UINT32_C(0xF1234567));
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_optional_refusals(void)
{
    /* The documented unsupported-provider answer. */
    CHECK(optional_refusal_scenario("track-request-register-not-supported") ==
          EXIT_SUCCESS);
    /* An outright failure is equally non-fatal for the optional kind. */
    CHECK(optional_refusal_scenario("track-request-register-fail") == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* An exception escaping the optional registration must not cross the C ABI.
 * The adapter reports the provider exception and no public owner escapes. */
static int test_optional_registration_throw(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    CHECK(open_track("track-request-register-throw", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(metadata == NULL);
    /* Registration remains one-shot even after the exceptional attempt. */
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Track close deactivates the shared subscription for the optional kind too:
 * an already-queued request is still drained first, and only then does Receive
 * report the stopped stream rather than a timeout. */
static int test_close_drains_then_stops(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(open_track("track-request-rich", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    /* The queued event survives the close of its channel. */
    CHECK(receive_request(metadata, NULL) == EXIT_SUCCESS);
    /* Now drained, the stream reports stopped, not timeout. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Caller-owned outputs must never be partially overwritten on a failure path.
 * Each invalid argument is checked independently against a prefilled sentinel. */
static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    ams_mel_ir_track_metadata_event *sentinel =
        (ams_mel_ir_track_metadata_event *)(void *)&session;
    const ams_mel_ir_track_metadata_event_v1 *view = NULL;
    CHECK(open_track("track-request-rich", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* A non-null output slot is rejected and left untouched. */
    event = sentinel;
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(event == sentinel);
    event = NULL;
    /* Null handles and null outputs are each rejected independently. */
    CHECK(ams_mel_ir_track_metadata_receive(NULL, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    /* A null event handle cannot produce a view, and the caller's view pointer
     * is left untouched. */
    CHECK(ams_mel_ir_track_metadata_event_view(NULL, &view, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(view == NULL);
    /* The queued request is still intact after every rejected call. */
    CHECK(receive_request(metadata, NULL) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
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
 * deterministic release signal for the provider's metadata thread. */
static int reserve(char *template_path)
{
    int descriptor = mkstemp(template_path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(unlink(template_path) == 0);
    return EXIT_SUCCESS;
}

/* Asynchronous delivery: the provider emits the request on its own thread
 * strictly AFTER registerMetadataCallback returned. Nothing is queued until the
 * barrier is released, and a positive-timeout Receive blocks until it arrives
 * rather than spinning. */
static int test_asynchronous_delivery(void)
{
    char barrier[] = "/tmp/ams-track-request-async-XXXXXX";
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_metadata *metadata = NULL;
    ams_mel_ir_track_metadata_event *event = NULL;
    CHECK(reserve(barrier) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_TRACK_REQUEST_BARRIER", barrier, 1) == 0);
    CHECK(open_track("track-request-async", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_open(track, 4U, &metadata, NULL, 0, NULL) ==
          AMS_MEL_OK);
    /* Registration has returned and nothing has been delivered yet: a zero
     * timeout is a nonblocking poll and must report a timeout. */
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 0U, &event, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(counters_are(metadata, 0U, 0U, 0U) == EXIT_SUCCESS);
    /* Release the provider thread, then block in a positive-timeout wait. */
    CHECK(touch(barrier) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_metadata_receive(metadata, 5000U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(event != NULL);
    {
        const ams_mel_ir_track_metadata_event_v1 *view = NULL;
        CHECK(ams_mel_ir_track_metadata_event_view(event, &view, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
        CHECK(check_rich_request(&view->request_system_track_data) == EXIT_SUCCESS);
        /* The acquired event is owned independently of the public metadata
         * owner: closing the subscription first must not invalidate it. */
        CHECK(ams_mel_ir_track_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(check_rich_request(&view->request_system_track_data) == EXIT_SUCCESS);
        CHECK(view->kind == AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA);
    }
    CHECK(ams_mel_ir_track_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_TRACK_REQUEST_BARRIER") == 0);
    CHECK(unlink(barrier) == 0);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_rich_request() == EXIT_SUCCESS);
    CHECK(test_asynchronous_delivery() == EXIT_SUCCESS);
    CHECK(test_zero_request() == EXIT_SUCCESS);
    CHECK(test_null_request() == EXIT_SUCCESS);
    CHECK(test_overflow_fifo() == EXIT_SUCCESS);
    CHECK(test_mixed_kinds() == EXIT_SUCCESS);
    CHECK(test_optional_refusals() == EXIT_SUCCESS);
    CHECK(test_optional_registration_throw() == EXIT_SUCCESS);
    CHECK(test_close_drains_then_stops() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    puts("PASS: native IR Track RequestSystemTrackData metadata contract");
    return EXIT_SUCCESS;
}
