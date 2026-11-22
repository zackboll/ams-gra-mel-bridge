#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "admission_observation.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
enum { PROBE_FAMILY = 12, MOCK_FAMILY = 19, TRACKS = 4097, FIELDS = 19 };
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_owner_notify(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_scheduling_requests(const ams_mel_ir_scheduling *, size_t *);

static ams_mel_ir_scheduling *channel;
static ams_mel_ir_track_list_request *request;
static ams_mel_ir_prioritized_track_list_cmd_v1 input;
static ams_mel_status_t submitted;

static void *submit(void *ignored)
{
    (void)ignored;
    submitted = ams_mel_ir_scheduling_send_prioritized_tracks(channel, &input, &request, NULL, 0, NULL);
    return NULL;
}

static void reentry(void)
{
    size_t pending = 0;
    CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 1);
    CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
}

static double from_bits(uint64_t bits)
{
    double number;
    memcpy(&number, &bits, sizeof number);
    return number;
}

static uint64_t bits(double number)
{
    uint64_t result;
    memcpy(&result, &number, sizeof result);
    return result;
}

static void initialize(ams_mel_ir_track_data_v1 *tracks)
{
    for (unsigned i = 0; i < TRACKS; ++i) {
        tracks[i] = (ams_mel_ir_track_data_v1){
            i % 2 ? INT64_MIN : INT64_MAX, -(int32_t)(i % 17),
            0x80000000U + i, UINT32_MAX - i, 0xf1234567U + i, 0xa0000023U - i,
            i & 1U, (i >> 1U) & 1U, i % 3 ? -71 : INT32_MIN,
            {from_bits(UINT64_C(0x8000000000000000)), from_bits(UINT64_C(0x7ff0000000000000)),
             from_bits(UINT64_C(0x7ff8000000040000) + i), (double)i + 0.25},
            {from_bits(UINT64_C(0xfff0000000000000)), from_bits(UINT64_C(0x8000000000000001) + i),
             -(double)i - 0.125, from_bits(UINT64_C(1) + i)},
            i % 17U, 0xf0000021U + i};
    }
}

typedef size_t (*values_fn)(uint64_t *, size_t);
static void verify_values(values_fn get)
{
    const uint64_t command[] = {(uint64_t)input.schedule_time_ns, (uint64_t)input.time_to_live_ns,
        input.command_id, input.track_count, input.tracks.size};
    const size_t words = 5U + input.tracks.size * FIELDS;
    uint64_t *actual = malloc(words * sizeof *actual);
    CHECK(actual && get(actual, words) == words);
    CHECK(memcmp(actual, command, sizeof command) == 0);
    for (size_t i = 0; i < input.tracks.size; ++i) {
        const ams_mel_ir_track_data_v1 track = input.tracks.data[i];
        const ams_mel_ir_range_az_el_v1 p = track.position, v = track.velocity;
        const uint64_t expected[] = {
            (uint64_t)track.timestamp_ns, (uint64_t)track.track_id, track.irst_priority, track.reserved_priority,
            track.irst_precedence, track.reserved_precedence, track.is_skippable, track.maintain_outside_fos,
            (uint64_t)track.irst_track_id, bits(p.range_m), bits(p.azimuth_rad), bits(p.elevation_rad),
            bits(p.range_rate_mps), bits(v.range_m), bits(v.azimuth_rad), bits(v.elevation_rad), bits(v.range_rate_mps),
            track.task, track.command_id};
        CHECK(memcmp(actual + 5 + i * FIELDS, expected, sizeof expected) == 0);
    }
    free(actual);
}

static unsigned events(const char *path, const char *name)
{
    unsigned n = 0;
    char line[128];
    FILE *file = fopen(path, "r");
    CHECK(file);
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, name)) ++n;
    }
    CHECK(fclose(file) == 0);
    return n;
}

static void verify_wait(const char *mode)
{
    ams_mel_ir_track_list_result_v1 got = {UINT32_MAX, UINT32_MAX}, cached = got;
    char small[3], entire[1100];
    size_t required = 0, second_required = 0;
    ams_mel_status_t status = ams_mel_ir_track_list_request_wait(request, 5000, &got, small, sizeof small, &required);
    CHECK(ams_mel_ir_track_list_request_wait(request, 0, &cached, entire, sizeof entire, &second_required) == status);
    CHECK(got.value == cached.value && got.error_code == cached.error_code && required == second_required);
    if (!strcmp(mode, "mel-error")) {
        CHECK(status == AMS_MEL_COMMAND_REJECTED && got.error_code == AMS_MEL_ERROR_INVALID_STATE);
        CHECK(required == 1030U && memcmp(small, "er\0", sizeof small) == 0);
        CHECK(memcmp(entire, "err\0\xff", 5) == 0 && entire[1029] == '\0');
        for (size_t i = 5; i < 1029; ++i) CHECK(entire[i] == 'x');
    } else if (!strcmp(mode, "null") || !strcmp(mode, "unowned") || !strcmp(mode, "bad-enum") ||
               !strcmp(mode, "detach-fail")) {
        CHECK(status == AMS_MEL_PROVIDER_FAILED && required > 1);
    } else if (!strcmp(mode, "future-throw")) {
        CHECK(status == AMS_MEL_PROVIDER_EXCEPTION && strstr(entire, "mock prioritized future exception"));
    } else {
        uint32_t expected = !strcmp(mode, "none") ? AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE :
            !strcmp(mode, "rejected") ? AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED :
            !strcmp(mode, "expired") ? AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED :
            AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED;
        CHECK(status == AMS_MEL_OK && got.value == expected && got.error_code == AMS_MEL_ERROR_NONE);
    }
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *mode = argv[1];
    char scenario[100], path[] = "/tmp/ams-prioritized-XXXXXX";
    const int concurrent = !strcmp(mode, "concurrent-close");
    const int publication = !strcmp(mode, "post-publication");
    const int delayed = concurrent || publication || !strcmp(mode, "observer-close") || !strcmp(mode, "resource") ||
        !strcmp(mode, "detach-fail");
    snprintf(scenario, sizeof scenario, "sched-prioritized-%s", delayed && strcmp(mode, "detach-fail") ? "delayed" : mode);
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0 && setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    ams_mel_session *session = NULL;
    const ams_mel_session_options_v1 options = {1};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    unsigned (*calls)(void), (*other)(void);
    int (*release)(unsigned, unsigned), (*barrier)(unsigned, unsigned, unsigned);
    void (*set_reentry)(void (*)(void));
    values_fn values;
    *(void **)(&calls) = dlsym(pin, "mock_prioritized_calls");
    *(void **)(&other) = dlsym(pin, "mock_scheduling_specific_calls");
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&barrier) = dlsym(pin, "mock_c2_send_barrier");
    *(void **)(&set_reentry) = dlsym(pin, "mock_prioritized_reentry");
    *(void **)(&values) = dlsym(pin, "mock_prioritized_values");
    CHECK(calls && other && release && barrier && set_reentry && values);

    ams_mel_ir_scheduling_config_v1 config = {0};
    config.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    submit(NULL);
    CHECK(submitted == AMS_MEL_PROVIDER_FAILED && !request && calls() == 0);
    CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
    ams_mel_ir_track_data_v1 *tracks = malloc(TRACKS * sizeof *tracks);
    CHECK(tracks);
    initialize(tracks);
    input = (ams_mel_ir_prioritized_track_list_cmd_v1){INT64_MIN, INT64_MAX, UINT32_MAX, 0, {NULL, 0}};
    if (!strcmp(mode, "fidelity")) {
        const size_t sizes[] = {0, 0, 3, TRACKS};
        const uint32_t counts[] = {0, 999, UINT32_MAX, 2};
        for (unsigned i = 0; i < 4; ++i) {
            input.tracks = (ams_mel_ir_track_data_span_v1){i ? tracks : NULL, sizes[i]};
            input.track_count = counts[i];
            if (i == 3) { input.schedule_time_ns = INT64_MAX; input.time_to_live_ns = INT64_MIN; }
            submit(NULL);
            CHECK(submitted == AMS_MEL_OK && request && calls() == i + 1);
            verify_values(values);
            verify_wait(mode);
            CHECK(ams_mel_ir_track_list_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK && !request);
            uint64_t observed;
            CHECK(ams_mel_test_completion_owner_notify(PROBE_FAMILY, 0, 0, &observed));
        }
    } else if (!strcmp(mode, "invalid-input")) {
        CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(NULL, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, NULL, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &input, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        input.tracks = (ams_mel_ir_track_data_span_v1){NULL, 1};
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        input.tracks = (ams_mel_ir_track_data_span_v1){tracks, SIZE_MAX};
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        input.tracks.size = TRACKS;
        tracks[TRACKS-1].task = 17;
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        tracks[TRACKS-1].task = UINT32_MAX;
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        tracks[TRACKS-1].task = 16;
        tracks[TRACKS-1].is_skippable = 2;
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        tracks[TRACKS-1].is_skippable = 1;
        tracks[TRACKS-1].maintain_outside_fos = UINT32_MAX;
        submit(NULL); CHECK(submitted == AMS_MEL_INVALID_ARGUMENT);
        tracks[TRACKS-1].maintain_outside_fos = 0;
        CHECK(setenv("AMS_MEL_TEST_TRACK_LIST_CONVERSION_FAILURE", "allocation", 1) == 0);
        submit(NULL); CHECK(submitted == AMS_MEL_RESOURCE_EXHAUSTED);
        CHECK(calls() == 0 && !request);
    } else if (!strncmp(mode, "pre-", 4)) {
        CHECK(setenv("AMS_MEL_TEST_COMMON_PRE_SEND_FAILURE", mode + 4, 1) == 0);
        submit(NULL);
        CHECK(submitted == (!strcmp(mode, "pre-allocation") || !strcmp(mode, "pre-after-claim") ?
            AMS_MEL_RESOURCE_EXHAUSTED : AMS_MEL_INTERNAL_ERROR) && !request && calls() == 0);
        size_t pending = 99;
        CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 0);
    } else {
        input.tracks = (ams_mel_ir_track_data_span_v1){tracks, 17};
        if (publication) CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", "allocation", 1) == 0);
        if (!strcmp(mode, "reentry")) set_reentry(reentry);
        if (concurrent) {
            pthread_t thread;
            CHECK(barrier(MOCK_FAMILY, 0, 0) && pthread_create(&thread, NULL, submit, NULL) == 0);
            CHECK(barrier(MOCK_FAMILY, 1, 0));
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(barrier(MOCK_FAMILY, 2, 0) && pthread_join(thread, NULL) == 0);
        } else submit(NULL);
        CHECK(calls() == 1);
        verify_values(values);
        if (!strcmp(mode, "send-throw")) CHECK(submitted == AMS_MEL_PROVIDER_EXCEPTION && !request);
        else if (!strcmp(mode, "invalid-future")) CHECK(submitted == AMS_MEL_PROVIDER_FAILED && !request);
        else if (publication) CHECK(submitted == AMS_MEL_INTERNAL_ERROR && !request);
        else CHECK(submitted == AMS_MEL_OK && request);
        if (channel && request) {
            ams_mel_ir_track_list_request *saved = request;
            CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(request == saved && calls() == 1);
        }
        if (delayed) {
            ams_mel_ir_track_list_result_v1 result = {UINT32_MAX, UINT32_MAX};
            if (request) {
                CHECK(ams_mel_ir_track_list_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                CHECK(result.value == UINT32_MAX && result.error_code == UINT32_MAX);
            }
            if (!strcmp(mode, "resource")) {
                ams_mel_ir_track_list_request *refused = NULL;
                CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &input, &refused, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
                CHECK(!refused && calls() == 1);
            }
            if (!strcmp(mode, "observer-close")) CHECK(ams_mel_ir_track_list_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(events(path, "scheduling_disabled") == 0 && other() == 0);
            CHECK(dlclose(pin) == 0 && events(path, "library_unloaded") == 0);
            /* Pin only while test-provider release code is itself executing. */
            pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
            CHECK(pin);
            CHECK(ams_mel_test_completion_boundary(PROBE_FAMILY, 0, 0, NULL));
            free(tracks);
            tracks = NULL;
            CHECK(release(MOCK_FAMILY, 1));
            CHECK(ams_mel_test_completion_boundary(PROBE_FAMILY, 1, 1, NULL));
            CHECK(events(path, "scheduling_disabled") == 0);
            CHECK(ams_mel_test_completion_boundary(PROBE_FAMILY, 2, 0, NULL));
        }
        if (request) verify_wait(mode);
    }
    free(tracks);
    CHECK(ams_mel_ir_track_list_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK && !request);
    CHECK(ams_mel_ir_track_list_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    uint64_t observed = 0;
    CHECK(ams_mel_test_completion_owner_notify(PROBE_FAMILY, 0, 0, &observed));
    CHECK(other() == 0 && events(path, "scheduling_disabled") == 1);
    CHECK(events(path, "library_unloaded") == 0 && dlclose(pin) == 0);
    CHECK(events(path, "library_unloaded") == (strcmp(mode, "detach-fail") ? 1U : 0U));
    CHECK(unlink(path) == 0);
    return 0;
}
