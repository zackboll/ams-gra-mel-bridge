#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "admission_observation.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path required"
#endif

extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);
typedef unsigned (*live_fn)(unsigned);
typedef int (*wave_fn)(unsigned, unsigned);

enum { MODE, RETURN, COMMS, IMAGE, INSTRUMENTATION, TRACK, FAMILIES };
enum { RETURN_N = 15, MODE_N = 15, COMMS_N = 14, IMAGE_N = 14,
       INSTR_N = 14, UPDATE_N = 14, RESPONSE_N = 14, TOTAL = 100 };

typedef struct {
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
    ams_mel_ir_stream *image;
    ams_mel_ir_instrumentation *instrumentation;
    ams_mel_ir_track *track;
    ams_mel_ir_return_request *returns[RETURN_N];
    ams_mel_ir_mode_request *modes[MODE_N];
    ams_mel_ir_channel_comms_request *comms[COMMS_N];
    ams_mel_ir_navigation_request *navigation[IMAGE_N];
    ams_mel_ir_instrumentation_request *levels[INSTR_N];
    ams_mel_ir_track_update_request *updates[UPDATE_N];
    ams_mel_ir_track_system_response_request *responses[RESPONSE_N];
    unsigned counts[7];
    gate_fn gate;
    wave_fn wave;
    live_fn live;
    uint64_t base[FAMILIES][6];
    uint64_t owners[FAMILIES];
    uint64_t submitted;
    char log_path[64];
    unsigned admission_limit;
} Fixture;

static ams_mel_string_view_v1 view(const char *s)
{ ams_mel_string_view_v1 result = {s, strlen(s)}; return result; }

static int log_has(const Fixture *f, const char *event)
{
    FILE *file = fopen(f->log_path, "r");
    char line[192];
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) {
        if (strcmp(line, event) == 0) { fclose(file); return 1; }
    }
    fclose(file);
    return 0;
}

static unsigned log_count(const Fixture *f, const char *event)
{
    FILE *file = fopen(f->log_path, "r");
    char line[192];
    unsigned count = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) if (strcmp(line, event) == 0) ++count;
    fclose(file);
    return count;
}

static int log_order(const Fixture *f, const char *before, const char *after)
{
    FILE *file = fopen(f->log_path, "r");
    char line[192];
    int seen_before = 0, seen_after = 0, valid = 1;
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) {
        if (strcmp(line, before) == 0) {
            seen_before = 1;
            if (seen_after) valid = 0;
        }
        if (strcmp(line, after) == 0) {
            seen_after = 1;
            if (!seen_before) valid = 0;
        }
    }
    fclose(file);
    return seen_before && seen_after && valid;
}

static unsigned expected(unsigned family)
{
    const unsigned counts[FAMILIES] = {MODE_N, RETURN_N, COMMS_N, IMAGE_N,
                                       INSTR_N, UPDATE_N + RESPONSE_N};
    return counts[family];
}

static int probe(const Fixture *f, unsigned family, unsigned completed)
{
    uint64_t values[6];
    if (!ams_mel_test_completion_snapshot(family, values)) return 0;
    return values[0] == f->base[family][0] + expected(family) &&
           values[1] == expected(family) - completed &&
           values[2] >= expected(family) &&
           values[3] == f->base[family][3] + completed &&
           values[4] == f->base[family][4] + expected(family) &&
           values[5] == f->base[family][5] + completed;
}

static void release_remaining(Fixture *f)
{
    uint64_t submitted = 0, released = 0;
    if (f->gate && f->gate(0, 0, &submitted, &released) && submitted > released)
        (void)f->gate(2, submitted - released, NULL, NULL);
}

static void close_requests(Fixture *f)
{
    for (unsigned i = 0; i < RETURN_N; ++i)
        if (f->returns[i]) (void)ams_mel_ir_return_request_close(&f->returns[i], NULL, 0, NULL);
    for (unsigned i = 0; i < MODE_N; ++i)
        if (f->modes[i]) (void)ams_mel_ir_mode_request_close(&f->modes[i], NULL, 0, NULL);
    for (unsigned i = 0; i < COMMS_N; ++i)
        if (f->comms[i]) (void)ams_mel_ir_channel_comms_request_close(&f->comms[i], NULL, 0, NULL);
    for (unsigned i = 0; i < IMAGE_N; ++i)
        if (f->navigation[i]) (void)ams_mel_ir_navigation_request_close(&f->navigation[i], NULL, 0, NULL);
    for (unsigned i = 0; i < INSTR_N; ++i)
        if (f->levels[i]) (void)ams_mel_ir_instrumentation_request_close(&f->levels[i], NULL, 0, NULL);
    for (unsigned i = 0; i < UPDATE_N; ++i)
        if (f->updates[i]) (void)ams_mel_ir_track_update_request_close(&f->updates[i], NULL, 0, NULL);
    for (unsigned i = 0; i < RESPONSE_N; ++i)
        if (f->responses[i]) (void)ams_mel_ir_track_system_response_request_close(&f->responses[i], NULL, 0, NULL);
}

static void cleanup(Fixture *f)
{
    /* A failed assertion must not leave detached workers blocked. The facade
     * holds the mock library while requests are pending; stop on unexpected
     * premature unload instead of invoking a stale provider function pointer. */
    if (!log_has(f, "library_unloaded\n")) release_remaining(f);
    close_requests(f);
    if (f->c2) (void)ams_mel_ir_c2_close(&f->c2, NULL, 0, NULL);
    if (f->image) (void)ams_mel_ir_stream_close(&f->image, NULL, 0, NULL);
    if (f->instrumentation)
        (void)ams_mel_ir_instrumentation_close(&f->instrumentation, NULL, 0, NULL);
    if (f->track) (void)ams_mel_ir_track_close(&f->track, NULL, 0, NULL);
    if (f->session) (void)ams_mel_session_close(&f->session, NULL, 0, NULL);
    if (f->log_path[0]) { (void)unsetenv("AMS_MEL_TEST_LIFETIME_LOG");
        FILE *file = fopen(f->log_path, "r");
        char line[192];
        if (file) {
            while (fgets(line, sizeof line, file)) fputs(line, stderr);
            fclose(file);
        }
        unlink(f->log_path);
    }
}

static int open_all(Fixture *f)
{
    ams_mel_ir_c2_config_v1 c2 = {0};
    ams_mel_ir_stream_config_v1 image = {0};
    ams_mel_ir_instrumentation_config_v1 instr = {0};
    ams_mel_ir_track_config_v1 track = {0};
    c2.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    image.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    instr.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    track.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
#define FILL(c) do { (c).channel_id.descriptive_label = view("scale"); \
    (c).platform_id.descriptive_label = view("test"); \
    (c).sensor_location.key = view("key"); \
    (c).sensor_location.system_name = view("system"); } while (0)
    FILL(c2); FILL(image); FILL(instr); FILL(track);
#undef FILL
    image.buffer_count = 2; image.buffer_size = 64; image.queue_capacity = 2;
    const ams_mel_session_options_v1 options = {f->admission_limit};
    const ams_mel_status_t opened = f->admission_limit
        ? ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
            &options, &f->session, NULL, 0, NULL)
        : ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
            &f->session, NULL, 0, NULL);
    if (opened != AMS_MEL_OK ||
        ams_mel_ir_c2_open(f->session, &c2, &f->c2, NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_stream_open(f->session, &image, &f->image, NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_instrumentation_open(f->session, &instr, &f->instrumentation,
            NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_track_open(f->session, &track, &f->track, NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_c2_enable(f->c2, NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_instrumentation_enable(f->instrumentation, NULL, 0, NULL) != AMS_MEL_OK ||
        ams_mel_ir_track_enable(f->track, NULL, 0, NULL) != AMS_MEL_OK) return 0;
    return 1;
}

static int submit_all(Fixture *f, int single)
{
    ams_mel_ir_channel_comms_test_request_v1 comms = {0};
    ams_mel_navigation_report_v1 navigation = {0};
    ams_mel_ir_instrumentation_level_command_v1 level = {0};
    ams_mel_ir_track_data_update_v1 update = {0};
    ams_mel_ir_system_track_data_response_v1 response = {0};
    char diagnostic[256] = {0};
    navigation.state = AMS_MEL_POSITION_SOLUTION_NOT_SET;
    level.priority = AMS_MEL_IR_PRIORITY_DEBUG;
    update.track_status = AMS_MEL_IR_TRACK_STATUS_UPDATE;
    comms.command_id = 0x80000001U;
    comms.request_id = 0xe0000003U;
    for (unsigned i = 0; i < (single ? 1U : RETURN_N); ++i) {
        if (ams_mel_ir_c2_send_keepalive(f->c2, &f->returns[i], NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Return submit %u\n", i);
            return 0;
        }
        ++f->counts[0];
    }
    for (unsigned i = 0; i < (single ? 1U : MODE_N); ++i) {
        if (ams_mel_ir_c2_submit_operate(f->c2, 1, &f->modes[i], NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Mode submit %u\n", i);
            return 0;
        }
        ++f->counts[1];
    }
    for (unsigned i = 0; i < (single ? 1U : COMMS_N); ++i) {
        if (ams_mel_ir_c2_submit_comms_test(f->c2, &comms, &f->comms[i], NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Comms submit %u\n", i);
            return 0;
        }
        ++f->counts[2];
    }
    for (unsigned i = 0; i < (single ? 1U : IMAGE_N); ++i) {
        if (ams_mel_ir_stream_submit_navigation_report(f->image, &navigation,
                &f->navigation[i], NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Navigation submit %u\n", i); return 0;
        }
        ++f->counts[3];
    }
    for (unsigned i = 0; i < (single ? 1U : INSTR_N); ++i) {
        if (ams_mel_ir_instrumentation_submit_level(f->instrumentation, &level,
                &f->levels[i], NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Instrumentation submit %u\n", i); return 0;
        }
        ++f->counts[4];
    }
    for (unsigned i = 0; i < (single ? 1U : UPDATE_N); ++i) {
        if (ams_mel_ir_track_submit_update(f->track, &update, &f->updates[i],
                NULL, 0, NULL) != AMS_MEL_OK) {
            fprintf(stderr, "Track update submit %u\n", i); return 0;
        }
        ++f->counts[5];
    }
    for (unsigned i = 0; i < (single ? 1U : RESPONSE_N); ++i) {
        const ams_mel_status_t status = ams_mel_ir_track_submit_system_track_data_response(
            f->track, &response, &f->responses[i], diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) {
            fprintf(stderr, "Track response submit %u: %u %s\n", i, status, diagnostic); return 0;
        }
        ++f->counts[6];
    }
    return 1;
}

static int run_stress(int exceptional)
{
    Fixture f = {0};
    uint64_t observed = 0, released = 0;
    unsigned active = 0;
    int ok = 0;
    void *library = NULL;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "cross-family: %s failed at line %d\n", #expr, __LINE__); \
    goto done; } } while (0)
    strcpy(f.log_path, "/tmp/ams-mel-scale-XXXXXX");
    const int fd = mkstemp(f.log_path);
    REQUIRE(fd >= 0);
    REQUIRE(close(fd) == 0);
    REQUIRE(setenv("AMS_MEL_TEST_LIFETIME_LOG", f.log_path, 1) == 0);
    for (unsigned family = 0; family < FAMILIES; ++family) {
        REQUIRE(ams_mel_test_completion_snapshot(family, f.base[family]));
        REQUIRE(ams_mel_test_completion_owner(family, 0, &f.owners[family]));
    }
    REQUIRE(open_all(&f));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    *(void **)(&f.wave) = dlsym(library, "mock_completion_release_family");
    *(void **)(&f.live) = dlsym(library, "mock_completion_live_results");
    REQUIRE(f.gate && f.wave && f.live);
    if (exceptional) REQUIRE(f.gate(3, 0, NULL, NULL));
    for (unsigned family = 0; family < FAMILIES; ++family)
        REQUIRE(ams_mel_test_completion_boundary(family, 0, 0, NULL));
    REQUIRE(f.gate(0, 0, &f.submitted, &released));
    /* Drop the test's dlopen reference: the bridge must retain the provider. */
    REQUIRE(dlclose(library) == 0);
    library = NULL;
    REQUIRE(submit_all(&f, 0));
    for (unsigned i = 0; i < 7; ++i) REQUIRE(f.counts[i] == (i == 0 || i == 1 ? 15U : 14U));
    REQUIRE(f.gate(1, f.submitted + TOTAL, &observed, &released));
    REQUIRE(observed == f.submitted + TOTAL && released == f.submitted);
    for (unsigned family = 0; family < FAMILIES; ++family) {
        REQUIRE(ams_mel_test_completion_wait(family, 4, f.base[family][4] + expected(family)));
        REQUIRE(probe(&f, family, 0));
        active += expected(family);
    }
    REQUIRE(active == TOTAL);
    close_requests(&f);
    REQUIRE(ams_mel_ir_c2_close(&f.c2, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_stream_close(&f.image, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_instrumentation_close(&f.instrumentation, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_track_close(&f.track, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_session_close(&f.session, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned family = 0; family < FAMILIES; ++family) REQUIRE(probe(&f, family, 0));
    REQUIRE(!log_has(&f, "c2_channel_destroyed\n") &&
            !log_has(&f, "channel_destroyed\n") &&
            !log_has(&f, "instrumentation_channel_destroyed\n") &&
            !log_has(&f, "track_channel_destroyed\n") &&
            !log_has(&f, "control_destroyed\n") &&
            !log_has(&f, "manager_destroyed\n") &&
            !log_has(&f, "library_unloaded\n"));
    /* Wave 1: three C2 worker implementations, leaving 56 pending. */
    REQUIRE(f.wave(MODE, MODE_N) && f.wave(RETURN, RETURN_N) && f.wave(COMMS, COMMS_N));
    for (unsigned family = MODE; family <= COMMS; ++family) {
        REQUIRE(ams_mel_test_completion_boundary(family, 1, expected(family), NULL));
        REQUIRE(f.live(family) == 0);
        REQUIRE(ams_mel_test_completion_boundary(family, 2, 0, NULL));
        REQUIRE(ams_mel_test_completion_wait(family, 3, f.base[family][3] + expected(family)));
        REQUIRE(ams_mel_test_completion_owner(family, f.owners[family] + expected(family), &observed));
        REQUIRE(probe(&f, family, expected(family)));
    }
    for (unsigned family = IMAGE; family < FAMILIES; ++family) REQUIRE(probe(&f, family, 0));
    for (unsigned family = IMAGE; family < FAMILIES; ++family)
        REQUIRE(f.live(family) == expected(family));
    REQUIRE(!log_has(&f, "channel_destroyed\n") &&
            !log_has(&f, "instrumentation_channel_destroyed\n") &&
            !log_has(&f, "track_channel_destroyed\n") &&
            !log_has(&f, "control_destroyed\n") &&
            !log_has(&f, "library_unloaded\n"));
    /* Wave 2: Image and Instrumentation, leaving 28 Track futures pending. */
    REQUIRE(f.wave(IMAGE, IMAGE_N) && f.wave(INSTRUMENTATION, INSTR_N));
    for (unsigned family = IMAGE; family <= INSTRUMENTATION; ++family) {
        REQUIRE(ams_mel_test_completion_boundary(family, 1, expected(family), NULL));
        REQUIRE(f.live(family) == 0);
        REQUIRE(ams_mel_test_completion_boundary(family, 2, 0, NULL));
        REQUIRE(ams_mel_test_completion_wait(family, 3, f.base[family][3] + expected(family)));
        REQUIRE(ams_mel_test_completion_owner(family, f.owners[family] + expected(family), &observed));
        REQUIRE(probe(&f, family, expected(family)));
    }
    REQUIRE(probe(&f, TRACK, 0));
    REQUIRE(f.live(TRACK) == expected(TRACK));
    REQUIRE(!log_has(&f, "track_channel_destroyed\n") &&
            !log_has(&f, "control_destroyed\n") && !log_has(&f, "library_unloaded\n"));
    /* Both Track types share the one Track worker/counting domain. */
    REQUIRE(f.wave(TRACK, UPDATE_N));
    REQUIRE(ams_mel_test_completion_boundary(TRACK, 1, UPDATE_N, NULL));
    REQUIRE(f.live(TRACK) == RESPONSE_N);
    REQUIRE(ams_mel_test_completion_boundary(TRACK, 2, 0, NULL));
    REQUIRE(ams_mel_test_completion_wait(TRACK, 3, f.base[TRACK][3] + UPDATE_N));
    REQUIRE(probe(&f, TRACK, UPDATE_N));
    REQUIRE(!log_has(&f, "track_channel_destroyed\n"));
    REQUIRE(ams_mel_test_completion_boundary(TRACK, 0, 0, NULL));
    REQUIRE(f.wave(TRACK, RESPONSE_N));
    REQUIRE(ams_mel_test_completion_boundary(TRACK, 1, UPDATE_N + RESPONSE_N, NULL));
    REQUIRE(f.live(TRACK) == 0);
    REQUIRE(!log_has(&f, "library_unloaded\n"));
    REQUIRE(ams_mel_test_completion_boundary(TRACK, 2, 0, NULL));
    REQUIRE(ams_mel_test_completion_wait(TRACK, 3, f.base[TRACK][3] + UPDATE_N + RESPONSE_N));
    for (unsigned family = 0; family < FAMILIES; ++family) {
        REQUIRE(ams_mel_test_completion_owner(family, f.owners[family] + expected(family), &observed));
        REQUIRE(probe(&f, family, expected(family)));
    }
    REQUIRE(log_has(&f, "c2_channel_destroyed\n") &&
            log_has(&f, "channel_destroyed\n") &&
            log_has(&f, "instrumentation_channel_destroyed\n") &&
            log_has(&f, "track_channel_destroyed\n") &&
            log_has(&f, "control_destroyed\n") &&
            log_has(&f, "manager_destroyed\n") &&
            log_has(&f, "library_unloaded\n"));
    REQUIRE(log_count(&f, "library_unloaded\n") == 1 &&
            log_count(&f, "control_destroyed\n") == 1 &&
            log_count(&f, "manager_destroyed\n") == 1 &&
            log_count(&f, "c2_channel_destroyed\n") == 1 &&
            log_count(&f, "channel_destroyed\n") == 1 &&
            log_count(&f, "instrumentation_channel_destroyed\n") == 1 &&
            log_count(&f, "track_channel_destroyed\n") == 1);
    for (unsigned family = 0; family < FAMILIES; ++family) {
        char owner[48];
        snprintf(owner, sizeof owner, "completion_owner_destroyed_%u\n", family);
        REQUIRE(log_count(&f, owner) == expected(family));
        snprintf(owner, sizeof owner, "safe_owner_destroyed_%u\n", family);
        REQUIRE(log_count(&f, owner) == expected(family));
        snprintf(owner, sizeof owner, "%s_%u\n",
                 exceptional ? "get_threw" : "get_returned", family);
        REQUIRE(log_count(&f, owner) == expected(family));
        uint64_t safety[4];
        REQUIRE(ams_mel_test_completion_boundary(family, 3, 0, safety));
        REQUIRE(safety[0] == expected(family) && safety[1] == expected(family) &&
                safety[2] == expected(family) && safety[3] == expected(family));
        const char *markers[] = {"future_consumed", "provider_result_scope_exited",
                                 "completion_graph_released", "worker_return_boundary"};
        for (unsigned i = 0; i < 4; ++i) {
            char marker[80];
            snprintf(marker, sizeof marker, "%s_%u\n", markers[i], family);
            REQUIRE(log_count(&f, marker) == expected(family));
            if (i < 3) REQUIRE(log_order(&f, marker, "library_unloaded\n"));
        }
    }
    REQUIRE(log_order(&f, "track_channel_destroyed\n", "control_destroyed\n") &&
            log_order(&f, "channel_destroyed\n", "control_destroyed\n") &&
            log_order(&f, "control_destroyed\n", "manager_destroyed\n") &&
            log_order(&f, "manager_destroyed\n", "library_unloaded\n"));
    puts("cross-family 15/15/14/14/14/14/14: 100 get exits, 100 final owners destroyed");
    ok = 1;
done:
    for (unsigned family = 0; family < FAMILIES; ++family)
        (void)ams_mel_test_completion_boundary(family, 2, 0, NULL);
    if (library) dlclose(library);
    cleanup(&f);
    return ok;
#undef REQUIRE
}

static int status_matches(const ams_mel_ir_command_status_v1 *s, int response)
{
    const char *text = response ? "Track system response accepted \xC2\xB5" :
                                  "Track update accepted \xC2\xB5";
    return s->command_id == (response ? 0xa1b2c3d4U : 0xf0e1d2c3U) &&
        s->state == AMS_MEL_IR_COMMAND_ACCEPTED &&
        s->reason_id == AMS_MEL_IR_CANNOT_COMPLY_NOT_SET &&
        s->reason_description.size == strlen(text) &&
        s->reason_description.data &&
        memcmp(s->reason_description.data, text, strlen(text)) == 0;
}

static int run_retained(int exceptional)
{
    Fixture f = {0};
    void *library = NULL;
    int ok = 0;
    uint64_t sent = 0, released = 0, values[6], observed;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "retained: %s failed at line %d\n", #expr, __LINE__); \
    goto done; } } while (0)
    REQUIRE(open_all(&f));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    REQUIRE(f.gate && f.gate(0, 0, &sent, &released));
    if (exceptional) REQUIRE(f.gate(3, 0, NULL, NULL));
    for (unsigned family = 0; family < FAMILIES; ++family) {
        REQUIRE(ams_mel_test_completion_snapshot(family, f.base[family]));
        REQUIRE(ams_mel_test_completion_owner(family, 0, &f.owners[family]));
    }
    REQUIRE(submit_all(&f, 1));
    REQUIRE(f.gate(1, sent + 7, NULL, NULL));
    for (unsigned family = 0; family < FAMILIES; ++family)
        REQUIRE(ams_mel_test_completion_wait(family, 4,
            f.base[family][4] + (family == TRACK ? 2 : 1)));
    /* Check semantic fields rather than padding or borrowed pointer identity.
     * Every call below uses the same retained handle, including both timeouts. */
#define WAIT_ALL(timeout, expected_status, payload) do { \
    ams_mel_ir_return_result_v1 r = {0}; \
    ams_mel_ir_mode_result_v1 m = {0}; \
    ams_mel_ir_channel_comms_test_result_v1 c = {0}; \
    ams_mel_ir_navigation_result_v1 n = {0}; \
    ams_mel_ir_instrumentation_result_v1 l = {0}; \
    ams_mel_ir_track_update_result_v1 u = {0}; \
    ams_mel_ir_track_system_response_result_v1 s = {0}; \
    REQUIRE(ams_mel_ir_return_request_wait(f.returns[0], timeout, &r, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_mode_request_wait(f.modes[0], timeout, &m, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_channel_comms_request_wait(f.comms[0], timeout, &c, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_navigation_request_wait(f.navigation[0], timeout, &n, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_instrumentation_request_wait(f.levels[0], timeout, &l, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_track_update_request_wait(f.updates[0], timeout, &u, NULL, 0, NULL) == expected_status); \
    REQUIRE(ams_mel_ir_track_system_response_request_wait(f.responses[0], timeout, &s, NULL, 0, NULL) == expected_status); \
    if (payload) { \
        REQUIRE(r.error_code == AMS_MEL_ERROR_NONE && r.value == AMS_MEL_IR_RETURN_SUCCESS); \
        REQUIRE(m.error_code == AMS_MEL_ERROR_NONE && m.mode == AMS_MEL_IR_MFA_MODE_TASK_SCHED); \
        REQUIRE(c.error_code == AMS_MEL_ERROR_NONE && c.command_id == 0x80000001U && c.request_id == 0xe0000003U); \
        REQUIRE(n.error_code == AMS_MEL_ERROR_NONE && n.response.system_time_ns == -8765432109LL && \
                n.response.command_id == 0xf1234567U && n.response.request_id == 0x89abcdefU); \
        REQUIRE(l.error_code == AMS_MEL_ERROR_NONE && l.report.command_id == 0xf1234567U && \
                l.report.size == 0x89abcdefU && l.report.timestamp_ns == -8765432109LL && \
                l.report.priority == AMS_MEL_IR_PRIORITY_DEBUG); \
        REQUIRE(u.error_code == AMS_MEL_ERROR_NONE && status_matches(&u.status, 0)); \
        REQUIRE(s.error_code == AMS_MEL_ERROR_NONE && status_matches(&s.status, 1)); \
    } \
} while (0)
    WAIT_ALL(0, AMS_MEL_TIMEOUT, 0);
    WAIT_ALL(1, AMS_MEL_TIMEOUT, 0);
    REQUIRE(f.gate(0, 0, &sent, &released) && sent == 7 && released == 0);
    REQUIRE(f.gate(2, 7, NULL, NULL));
    const ams_mel_status_t terminal = exceptional ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_OK;
    WAIT_ALL(15000, terminal, !exceptional);
    WAIT_ALL(0, terminal, !exceptional);
    close_requests(&f);
    for (unsigned family = 0; family < FAMILIES; ++family) {
        const unsigned count = family == TRACK ? 2 : 1;
        REQUIRE(ams_mel_test_completion_owner(family, f.owners[family] + count, &observed));
        REQUIRE(ams_mel_test_completion_snapshot(family, values));
        REQUIRE(values[1] == 0 && values[4] == f.base[family][4] + count &&
                values[5] == f.base[family][5] + count);
    }
    puts("retained seven-type timeout, cached terminal result, exactly-once get: passed");
    ok = 1;
done:
    cleanup(&f);
    if (library) dlclose(library);
    return ok;
#undef WAIT_ALL
#undef REQUIRE
}

/* Only the test client selects and retries logical work. No bridge queue. */
static ams_mel_status_t submit_one(Fixture *f, unsigned type)
{
    const ams_mel_ir_channel_comms_test_request_v1 comms = {0};
    const ams_mel_navigation_report_v1 navigation = {0};
    ams_mel_ir_instrumentation_level_command_v1 level = {0};
    ams_mel_ir_track_data_update_v1 update = {0};
    const ams_mel_ir_system_track_data_response_v1 response = {0};
    level.priority = AMS_MEL_IR_PRIORITY_DEBUG;
    update.track_status = AMS_MEL_IR_TRACK_STATUS_UPDATE;
    switch (type) {
    case 0: return ams_mel_ir_c2_send_keepalive(f->c2, &f->returns[0], NULL, 0, NULL);
    case 1: return ams_mel_ir_c2_submit_operate(f->c2, 1, &f->modes[0], NULL, 0, NULL);
    case 2: return ams_mel_ir_c2_submit_comms_test(f->c2, &comms, &f->comms[0], NULL, 0, NULL);
    case 3: return ams_mel_ir_stream_submit_navigation_report(f->image, &navigation,
        &f->navigation[0], NULL, 0, NULL);
    case 4: return ams_mel_ir_instrumentation_submit_level(f->instrumentation, &level,
        &f->levels[0], NULL, 0, NULL);
    case 5: return ams_mel_ir_track_submit_update(f->track, &update, &f->updates[0], NULL, 0, NULL);
    default: return ams_mel_ir_track_submit_system_track_data_response(f->track, &response,
        &f->responses[0], NULL, 0, NULL);
    }
}

static int no_requests(const Fixture *f)
{
    return !f->returns[0] && !f->modes[0] && !f->comms[0] && !f->navigation[0] &&
           !f->levels[0] && !f->updates[0] && !f->responses[0];
}

static int reclaim_all(void)
{
    for (unsigned family = 0; family < FAMILIES; ++family) {
        uint64_t values[4], observed;
        if (!ams_mel_test_completion_boundary(family, 3, 0, values) ||
            !ams_mel_test_completion_owner(family, values[2], &observed)) return 0;
    }
    return 1;
}

static int run_bounded_workload(void)
{
    Fixture f = {0};
    void *library = NULL;
    unsigned accepted = 0, pending = 0, refused = 0, peak = 0;
    unsigned gets[FAMILIES] = {0};
    const unsigned family_for_type[7] = {RETURN, MODE, COMMS, IMAGE, INSTRUMENTATION, TRACK, TRACK};
    int ok = 0;
    f.admission_limit = 8;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "bounded workload: %s failed at %d\n", #expr, __LINE__); goto done; \
} } while (0)
    REQUIRE(open_all(&f));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    REQUIRE(f.gate);
    while (accepted < TOTAL || pending) {
        ams_mel_status_t status = AMS_MEL_RESOURCE_EXHAUSTED;
        if (accepted < TOTAL) status = submit_one(&f, accepted % 7);
        if (status == AMS_MEL_OK) {
            ++gets[family_for_type[accepted % 7]];
            ++accepted;
            ++pending;
            /* Public owner closure is deliberately NOT permit release. */
            close_requests(&f);
            REQUIRE(pending <= 8);
        } else {
            REQUIRE(status == AMS_MEL_RESOURCE_EXHAUSTED && no_requests(&f));
            if (accepted < TOTAL) ++refused;
            uint64_t sent = 0;
            REQUIRE(f.gate(0, 0, &sent, NULL) && sent == accepted);
            unsigned active = 0;
            for (unsigned family = 0; family < FAMILIES; ++family) {
                uint64_t values[6];
                REQUIRE(ams_mel_test_completion_wait(family, 4, gets[family]));
                REQUIRE(ams_mel_test_completion_snapshot(family, values));
                active += (unsigned)values[1];
            }
            REQUIRE(active == pending && active <= 8);
            if (active > peak) peak = active;
            REQUIRE(f.gate(2, pending, NULL, NULL));
            REQUIRE(reclaim_all());
            pending = 0;
            /* Retry the same logical operation only after reclamation. */
        }
    }
    for (unsigned family = 0; family < FAMILIES; ++family) {
        uint64_t values[6];
        REQUIRE(ams_mel_test_completion_snapshot(family, values));
        REQUIRE(values[0] == gets[family] && values[1] == 0 &&
                values[4] == gets[family] && values[5] == gets[family]);
    }
    printf("seven-type limit=8 logical=%u sends=%u gets=100 refused=%u peak-active=%u\n",
           accepted, accepted, refused, peak);
    ok = 1;
done:
    cleanup(&f);
    if (library) dlclose(library);
    return ok;
#undef REQUIRE
}

static int run_parent_admission(void)
{
    Fixture f = {0};
    void *library = NULL;
    ams_mel_test_admission_token *token = NULL;
    uint32_t active = 0, maximum = 0;
    int ok = 0;
    f.admission_limit = 1;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "parent admission: %s failed at %d\n", #expr, __LINE__); goto done; \
} } while (0)
    strcpy(f.log_path, "/tmp/ams-mel-parent-XXXXXX");
    const int fd = mkstemp(f.log_path);
    REQUIRE(fd >= 0);
    REQUIRE(close(fd) == 0);
    REQUIRE(setenv("AMS_MEL_TEST_LIFETIME_LOG", f.log_path, 1) == 0);
    REQUIRE(open_all(&f));
    REQUIRE(ams_mel_test_admission(f.session, 0, &token, NULL, NULL));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    REQUIRE(f.gate);
    REQUIRE(dlclose(library) == 0);
    library = NULL;
    REQUIRE(submit_one(&f, 0) == AMS_MEL_OK);
    REQUIRE(ams_mel_test_completion_wait(RETURN, 4, 1));
    close_requests(&f);
    REQUIRE(ams_mel_ir_c2_close(&f.c2, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_stream_close(&f.image, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_instrumentation_close(&f.instrumentation, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_ir_track_close(&f.track, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_session_close(&f.session, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    REQUIRE(active == 1 && maximum == 1);
    REQUIRE(!log_has(&f, "c2_channel_destroyed\n"));
    REQUIRE(!log_has(&f, "control_destroyed\n"));
    REQUIRE(!log_has(&f, "manager_destroyed\n"));
    REQUIRE(!log_has(&f, "library_unloaded\n"));
    /* Park the worker before graph cleanup, so the provider gate returns before
     * its DSO can unload. No test dlopen owner masks the lifetime proof. */
    REQUIRE(ams_mel_test_completion_boundary(RETURN, 0, 0, NULL));
    REQUIRE(f.gate(2, 1, NULL, NULL));
    REQUIRE(ams_mel_test_completion_boundary(RETURN, 1, 1, NULL));
    REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    REQUIRE(active == 1);
    REQUIRE(ams_mel_test_completion_boundary(RETURN, 2, 0, NULL));
    REQUIRE(reclaim_all());
    REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    REQUIRE(active == 0 && maximum == 1);
    /* Stronger than checking after observer release: provider unload already
     * occurred while the provider-independent observation token was alive. */
    REQUIRE(log_order(&f, "c2_channel_destroyed\n", "control_destroyed\n"));
    REQUIRE(log_order(&f, "control_destroyed\n", "manager_destroyed\n"));
    REQUIRE(log_order(&f, "manager_destroyed\n", "library_unloaded\n"));
    REQUIRE(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
    puts("parent close: admission survives owners; observer does not retain provider");
    ok = 1;
done:
    (void)ams_mel_test_completion_boundary(RETURN, 2, 0, NULL);
    (void)ams_mel_test_admission(NULL, 2, &token, NULL, NULL);
    cleanup(&f);
    if (library) dlclose(library);
    return ok;
#undef REQUIRE
}

static int admission_is(ams_mel_test_admission_token *token, uint32_t expected)
{
    uint32_t active = 0, maximum = 0;
    return ams_mel_test_admission(NULL, 1, &token, &active, &maximum) &&
           active == expected && maximum == 1;
}

static int reclaim_family(unsigned family)
{
    uint64_t values[4], observed;
    return ams_mel_test_completion_boundary(family, 3, 0, values) &&
           ams_mel_test_completion_owner(family, values[2], &observed);
}

static int run_isolated_admission(void)
{
    Fixture a = {0}, b = {0};
    ams_mel_test_admission_token *ta = NULL, *tb = NULL;
    void *library = NULL;
    int ok = 0;
    a.admission_limit = b.admission_limit = 1;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "isolated admission: %s failed at %d\n", #expr, __LINE__); goto done; \
} } while (0)
    REQUIRE(open_all(&a) && open_all(&b));
    REQUIRE(ams_mel_test_admission(a.session, 0, &ta, NULL, NULL));
    REQUIRE(ams_mel_test_admission(b.session, 0, &tb, NULL, NULL));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&a.gate) = dlsym(library, "mock_completion_gate");
    *(void **)(&a.wave) = dlsym(library, "mock_completion_release_family");
    REQUIRE(a.gate && a.wave);
    b.gate = a.gate;
    REQUIRE(submit_one(&a, 0) == AMS_MEL_OK);
    REQUIRE(admission_is(ta, 1) && admission_is(tb, 0));
    REQUIRE(submit_one(&b, 3) == AMS_MEL_OK);
    REQUIRE(admission_is(ta, 1) && admission_is(tb, 1));
    /* Distinct empty output slots: do not overwrite the admitted owners. */
    REQUIRE(submit_one(&a, 1) == AMS_MEL_RESOURCE_EXHAUSTED);
    REQUIRE(!a.modes[0]);
    REQUIRE(submit_one(&b, 1) == AMS_MEL_RESOURCE_EXHAUSTED);
    REQUIRE(!b.modes[0]);
    close_requests(&a);
    REQUIRE(a.wave(RETURN, 1));
    REQUIRE(reclaim_family(RETURN));
    REQUIRE(admission_is(ta, 0) && admission_is(tb, 1));
    REQUIRE(submit_one(&a, 0) == AMS_MEL_OK);
    REQUIRE(admission_is(ta, 1) && admission_is(tb, 1));
    REQUIRE(submit_one(&b, 1) == AMS_MEL_RESOURCE_EXHAUSTED);
    REQUIRE(!b.modes[0]);
    close_requests(&a);
    close_requests(&b);
    REQUIRE(a.wave(RETURN, 1) && a.wave(IMAGE, 1));
    REQUIRE(reclaim_all());
    REQUIRE(admission_is(ta, 0) && admission_is(tb, 0));
    puts("two Sessions: independent saturation and independent reclamation passed");
    ok = 1;
done:
    cleanup(&a);
    cleanup(&b);
    (void)ams_mel_test_admission(NULL, 2, &ta, NULL, NULL);
    (void)ams_mel_test_admission(NULL, 2, &tb, NULL, NULL);
    if (library) dlclose(library);
    return ok;
#undef REQUIRE
}

typedef struct {
    pthread_barrier_t *barrier;
    ams_mel_ir_c2 *c2;
    ams_mel_ir_return_request *request;
    ams_mel_status_t status;
} Submitter;

static void *concurrent_submit(void *argument)
{
    Submitter *s = argument;
    (void)pthread_barrier_wait(s->barrier);
    s->status = ams_mel_ir_c2_send_keepalive(s->c2, &s->request, NULL, 0, NULL);
    return NULL;
}

static int run_concurrent_admission(void)
{
    Fixture f = {0};
    Submitter submitters[8] = {0};
    pthread_t threads[8];
    pthread_barrier_t barrier;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_ir_c2_config_v1 config = {0};
    void *library = NULL;
    unsigned successes = 0, refusals = 0;
    uint32_t active = 0, maximum = 0;
    uint64_t sends = 0, values[6];
    int ok = 0;
    f.admission_limit = 4;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "concurrent admission: %s failed at %d\n", #expr, __LINE__); goto done; \
} } while (0)
    REQUIRE(open_all(&f));
    REQUIRE(ams_mel_test_admission(f.session, 0, &token, NULL, NULL));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    REQUIRE(f.gate);
    config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    config.channel_id.descriptive_label = view("race");
    config.platform_id.descriptive_label = view("test");
    config.sensor_location.key = view("key");
    config.sensor_location.system_name = view("system");
    for (unsigned i = 0; i < 8; ++i) {
        REQUIRE(ams_mel_ir_c2_open(f.session, &config, &submitters[i].c2,
                                  NULL, 0, NULL) == AMS_MEL_OK);
        submitters[i].barrier = &barrier;
    }
    REQUIRE(pthread_barrier_init(&barrier, NULL, 9) == 0);
    for (unsigned i = 0; i < 8; ++i) {
        /* A failed thread creation cannot satisfy the start barrier. Terminate
         * this isolated test rather than strand threads or fake a result. */
        if (pthread_create(&threads[i], NULL, concurrent_submit, &submitters[i]) != 0)
            abort();
    }
    (void)pthread_barrier_wait(&barrier);
    for (unsigned i = 0; i < 8; ++i)
        if (pthread_join(threads[i], NULL) != 0) abort();
    REQUIRE(pthread_barrier_destroy(&barrier) == 0);
    for (unsigned i = 0; i < 8; ++i) {
        if (submitters[i].status == AMS_MEL_OK) {
            REQUIRE(submitters[i].request);
            ++successes;
        } else {
            REQUIRE(submitters[i].status == AMS_MEL_RESOURCE_EXHAUSTED);
            REQUIRE(!submitters[i].request);
            ++refusals;
        }
    }
    REQUIRE(successes == 4 && refusals == 4);
    REQUIRE(f.gate(0, 0, &sends, NULL) && sends == 4);
    REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    /* No future has been released: active is monotonic throughout the race.
     * Therefore this final value is also its maximum, not a sampled estimate. */
    REQUIRE(active == 4 && maximum == 4);
    REQUIRE(ams_mel_test_completion_wait(RETURN, 4, 4));
    REQUIRE(ams_mel_test_completion_snapshot(RETURN, values));
    REQUIRE(values[1] == 4);
    REQUIRE(f.gate(2, 4, NULL, NULL));
    for (unsigned i = 0; i < 8; ++i)
        if (submitters[i].request)
            REQUIRE(ams_mel_ir_return_request_close(&submitters[i].request,
                                                    NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE(reclaim_all());
    REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    REQUIRE(active == 0);
    puts("limit=4 concurrent: 4 successes, 4 refusals, 4 sends, 4 permits, 4 workers");
    ok = 1;
done:
    release_remaining(&f);
    for (unsigned i = 0; i < 8; ++i) {
        if (submitters[i].request)
            (void)ams_mel_ir_return_request_close(&submitters[i].request, NULL, 0, NULL);
        if (submitters[i].c2)
            (void)ams_mel_ir_c2_close(&submitters[i].c2, NULL, 0, NULL);
    }
    cleanup(&f);
    (void)ams_mel_test_admission(NULL, 2, &token, NULL, NULL);
    if (library) dlclose(library);
    return ok;
#undef REQUIRE
}

static int run_admission(void)
{
    Fixture f = {0};
    void *library = NULL;
    ams_mel_test_admission_token *token = NULL;
    uint32_t active = 0, maximum = 0;
    size_t counts_before[4], counts_after[4];
    int ok = 0;
    uint64_t sent = 0, observed = 0, base[6], boundary[4];
    f.admission_limit = 1;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "admission: %s failed at %d\n", #expr, __LINE__); goto done; \
} } while (0)
    REQUIRE(open_all(&f));
    REQUIRE(ams_mel_test_admission(f.session, 0, &token, NULL, NULL));
    library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(library);
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    REQUIRE(f.gate);
    /* Alternate the occupying family. Closing its public request must not
     * release admission; every subsequent refusal must leave ALL outputs null. */
    for (unsigned pass = 0; pass < 2; ++pass) {
        const unsigned type = pass ? 3 : 0;
        const unsigned family = pass ? IMAGE : RETURN;
        REQUIRE(ams_mel_test_completion_snapshot(family, base));
        REQUIRE(submit_one(&f, type) == AMS_MEL_OK);
        REQUIRE(ams_mel_test_completion_wait(family, 4, base[4] + 1));
        REQUIRE(f.gate(0, 0, &sent, NULL));
        close_requests(&f);
        REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
        REQUIRE(active == 1 && maximum == 1);
        REQUIRE(ams_mel_test_c2_requests(f.c2, &counts_before[0]));
        REQUIRE(ams_mel_test_navigation_requests(f.image, &counts_before[1]));
        REQUIRE(ams_mel_test_instrumentation_requests(f.instrumentation, &counts_before[2]));
        REQUIRE(ams_mel_test_track_requests(f.track, &counts_before[3]));
        REQUIRE(counts_before[0] == (pass ? 0U : 1U));
        REQUIRE(counts_before[1] == (pass ? 1U : 0U));
        REQUIRE(counts_before[2] == 0 && counts_before[3] == 0);
        for (unsigned other = 0; other < 7; ++other) {
            REQUIRE(submit_one(&f, other) == AMS_MEL_RESOURCE_EXHAUSTED);
            REQUIRE(no_requests(&f));
            REQUIRE(f.gate(0, 0, &observed, NULL) && observed == sent);
            REQUIRE(ams_mel_test_c2_requests(f.c2, &counts_after[0]));
            REQUIRE(ams_mel_test_navigation_requests(f.image, &counts_after[1]));
            REQUIRE(ams_mel_test_instrumentation_requests(f.instrumentation, &counts_after[2]));
            REQUIRE(ams_mel_test_track_requests(f.track, &counts_after[3]));
            REQUIRE(memcmp(counts_before, counts_after, sizeof counts_before) == 0);
        }
        /* Consumption is not reclamation: park after get and result scope exit,
         * before accounting/graph cleanup, and recheck cross-family refusal. */
        REQUIRE(ams_mel_test_completion_boundary(family, 3, 0, boundary));
        REQUIRE(ams_mel_test_completion_boundary(family, 0, 0, NULL));
        REQUIRE(f.gate(2, 1, NULL, NULL));
        REQUIRE(ams_mel_test_completion_boundary(family, 1, boundary[0] + 1, NULL));
        REQUIRE(submit_one(&f, pass ? 0 : 3) == AMS_MEL_RESOURCE_EXHAUSTED);
        REQUIRE(no_requests(&f));
        REQUIRE(f.gate(0, 0, &observed, NULL) && observed == sent);
        REQUIRE(ams_mel_test_completion_boundary(family, 2, 0, NULL));
        REQUIRE(reclaim_all());
        REQUIRE(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
        REQUIRE(active == 0 && maximum == 1);
    }
    /* Explicit retry succeeds after FinalOwner proves permit destruction. */
    REQUIRE(submit_one(&f, 5) == AMS_MEL_OK);
    REQUIRE(f.gate(2, 1, NULL, NULL));
    close_requests(&f);
    REQUIRE(reclaim_all());
    puts("limit=1: seven-type refusal, reverse owner, request close, post-get boundary, retry passed");
    ok = 1;
done:
    (void)ams_mel_test_admission(NULL, 2, &token, NULL, NULL);
    for (unsigned family = 0; family < FAMILIES; ++family)
        (void)ams_mel_test_completion_boundary(family, 2, 0, NULL);
    cleanup(&f);
    if (library) dlclose(library);
    return ok;
#undef REQUIRE
}

/* Each invocation is a fresh process: sticky mock exceptions and intentional
 * emergency roots must never contaminate another scenario. */
#define FAILURE_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "bounded failure line %d: %s\n", __LINE__, #x); exit(EXIT_FAILURE); \
} } while (0)

typedef int (*send_barrier_fn)(unsigned, unsigned, unsigned);
typedef struct {
    Fixture *fixture;
    unsigned type;
    ams_mel_status_t status;
} FailureSubmission;

static void *failure_submit(void *arg)
{
    FailureSubmission *s = arg;
    s->status = submit_one(s->fixture, s->type);
    return NULL;
}

static void failure_counts(Fixture *f, unsigned family, size_t expected_count)
{
    size_t counts[4];
    FAILURE_CHECK(ams_mel_test_c2_requests(f->c2, &counts[0]));
    FAILURE_CHECK(ams_mel_test_navigation_requests(f->image, &counts[1]));
    FAILURE_CHECK(ams_mel_test_instrumentation_requests(f->instrumentation, &counts[2]));
    FAILURE_CHECK(ams_mel_test_track_requests(f->track, &counts[3]));
    const unsigned domain = family < IMAGE ? 0 : family - 2;
    for (unsigned i = 0; i < 4; ++i)
        FAILURE_CHECK(counts[i] == (i == domain ? expected_count : 0));
}

static ams_mel_status_t failure_wait(Fixture *f, unsigned type, uint32_t timeout,
                                    int rejection)
{
    char diagnostic[128] = {0};
    if (type == 0) {
        ams_mel_ir_return_result_v1 result = {0};
        return ams_mel_ir_return_request_wait(f->returns[0], timeout, &result, NULL, 0, NULL);
    }
    if (type == 1) {
        ams_mel_ir_mode_result_v1 result = {0};
        ams_mel_status_t status = ams_mel_ir_mode_request_wait(f->modes[0], timeout,
            &result, diagnostic, sizeof diagnostic, NULL);
        if (rejection) {
            FAILURE_CHECK(result.error_code == AMS_MEL_ERROR_INSUFFICIENT_RESOURCES);
            FAILURE_CHECK(strstr(diagnostic, "provider resource rejection"));
        }
        return status;
    }
    ams_mel_ir_navigation_result_v1 result = {0};
    return ams_mel_ir_navigation_request_wait(f->navigation[0], timeout, &result, NULL, 0, NULL);
}

static int run_failure(unsigned type, const char *kind)
{
    const unsigned families[] = {RETURN, MODE, COMMS, IMAGE, INSTRUMENTATION, TRACK, TRACK};
    const char *send_events[] = {"keepalive_sent\n", "mode_sent\n", "comms_sent\n",
        "navigation_sent\n", "instrumentation_level_sent\n", "track_data_update_sent\n",
        "track_system_track_data_response_sent\n"};
    const char *failpoints[] = {"AMS_MEL_TEST_C2_POST_SEND_FAILURE",
        "AMS_MEL_TEST_C2_POST_SEND_FAILURE", "AMS_MEL_TEST_C2_POST_SEND_FAILURE",
        "AMS_MEL_TEST_IMAGE_NAVIGATION_POST_SEND_FAILURE",
        "AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE",
        "AMS_MEL_TEST_TRACK_UPDATE_POST_SEND_FAILURE",
        "AMS_MEL_TEST_TRACK_RESPONSE_POST_SEND_FAILURE"};
    FAILURE_CHECK(type < 7);
    const unsigned family = families[type];
    const int permanent = !strcmp(kind, "allocation") || !strcmp(kind, "worker-launch");
    const int synchronous = !strcmp(kind, "send-throw");
    const int exceptional = !strcmp(kind, "exception");
    const int rejection = !strcmp(kind, "rejection");
    FAILURE_CHECK(permanent || synchronous || exceptional || rejection);
    Fixture f = {0};
    f.admission_limit = 1;
    strcpy(f.log_path, "/tmp/ams-mel-failure-XXXXXX");
    int fd = mkstemp(f.log_path);
    FAILURE_CHECK(fd >= 0 && close(fd) == 0);
    FAILURE_CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", f.log_path, 1) == 0);
    FAILURE_CHECK(open_all(&f));
    ams_mel_test_admission_token *token = NULL;
    FAILURE_CHECK(ams_mel_test_admission(f.session, 0, &token, NULL, NULL));
    FAILURE_CHECK(admission_is(token, 0));
    failure_counts(&f, family, 0);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    FAILURE_CHECK(library);
    send_barrier_fn barrier = NULL;
    *(void **)(&barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&f.gate) = dlsym(library, "mock_completion_gate");
    FAILURE_CHECK(barrier && f.gate);
    uint64_t values[6], sent = 0, observed = 0, owner_target = 0;
    if (synchronous) {
        FAILURE_CHECK(barrier(family, 0, 1));
        FailureSubmission submission = {&f, type, AMS_MEL_OK};
        pthread_t thread;
        FAILURE_CHECK(pthread_create(&thread, NULL, failure_submit, &submission) == 0);
        FAILURE_CHECK(barrier(family, 1, 0));
        FAILURE_CHECK(barrier(family, 3, 0) == 1);
        FAILURE_CHECK(admission_is(token, 1));
        failure_counts(&f, family, 1);
        FAILURE_CHECK(barrier(family, 2, 0));
        FAILURE_CHECK(pthread_join(thread, NULL) == 0);
        FAILURE_CHECK(submission.status == AMS_MEL_PROVIDER_EXCEPTION && no_requests(&f));
        FAILURE_CHECK(ams_mel_test_completion_owner(family, ++owner_target, &observed));
        FAILURE_CHECK(observed == owner_target && admission_is(token, 0));
        failure_counts(&f, family, 0);
        FAILURE_CHECK(ams_mel_test_completion_snapshot(family, values));
        FAILURE_CHECK(values[0] == 0 && values[1] == 0 && values[4] == 0 && values[5] == 0);
        FAILURE_CHECK(f.gate(0, 0, &sent, NULL) && sent == 0);
    }
    if (permanent) FAILURE_CHECK(setenv(failpoints[type], kind, 1) == 0);
    if (exceptional) FAILURE_CHECK(f.gate(3, 0, NULL, NULL));
    if (rejection) FAILURE_CHECK(f.gate(4, 0, NULL, NULL));
    FAILURE_CHECK(submit_one(&f, type) == (permanent ? AMS_MEL_INTERNAL_ERROR : AMS_MEL_OK));
    FAILURE_CHECK(permanent ? no_requests(&f) : !no_requests(&f));
    FAILURE_CHECK(f.gate(0, 0, &sent, NULL) && sent == 1);
    FAILURE_CHECK(admission_is(token, 1));
    failure_counts(&f, family, 1);
    if (synchronous) FAILURE_CHECK(barrier(family, 3, 0) == 2);
    if (permanent) {
        FAILURE_CHECK(log_count(&f, send_events[type]) == 1);
        FAILURE_CHECK(no_requests(&f));
        FAILURE_CHECK(ams_mel_test_completion_snapshot(family, values));
        FAILURE_CHECK(values[0] == 0 && values[1] == 0 && values[4] == 0 && values[5] == 0);
        FAILURE_CHECK(ams_mel_test_completion_owner(family, 0, &observed) && observed == 0);
        FAILURE_CHECK(unsetenv(failpoints[type]) == 0);
        FAILURE_CHECK(submit_one(&f, type) == AMS_MEL_RESOURCE_EXHAUSTED && no_requests(&f));
        FAILURE_CHECK(log_count(&f, send_events[type]) == 1);
        FAILURE_CHECK(f.gate(0, 0, &sent, NULL) && sent == 1);
        FAILURE_CHECK(admission_is(token, 1));
        failure_counts(&f, family, 1);
        printf("type=%u %s: sends=1 requests=1 permit=1 workers=0 gets=0; second send refused\n", type, kind);
        /* No release, rescue, or fixture cleanup: process lifetime retention. */
        FAILURE_CHECK(unlink(f.log_path) == 0);
        return 1;
    }
    FAILURE_CHECK(ams_mel_test_completion_wait(family, 4, 1));
    FAILURE_CHECK(ams_mel_test_completion_snapshot(family, values));
    FAILURE_CHECK(values[0] == 1 && values[1] == 1 && values[4] == 1 && values[5] == 0);
    /* A separate public output avoids overwriting the retained first handle. */
    ams_mel_ir_return_request *refused = NULL;
    FAILURE_CHECK(ams_mel_ir_c2_send_keepalive(f.c2, &refused, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
    FAILURE_CHECK(!refused && f.gate(0, 0, &sent, NULL) && sent == 1);
    if (family == RETURN) ++owner_target; /* refused preallocated input */
    FAILURE_CHECK(ams_mel_test_completion_boundary(family, 0, 0, NULL));
    FAILURE_CHECK(f.gate(2, 1, NULL, NULL));
    FAILURE_CHECK(ams_mel_test_completion_boundary(family, 1, 1, NULL));
    FAILURE_CHECK(admission_is(token, 1));
    failure_counts(&f, family, 1);
    FAILURE_CHECK(ams_mel_test_completion_snapshot(family, values));
    FAILURE_CHECK(values[4] == 1 && values[5] == 1);
    char event[48];
    snprintf(event, sizeof event, "%s_%u\n", exceptional ? "get_threw" : "get_returned", family);
    FAILURE_CHECK(log_count(&f, event) == 1);
    FAILURE_CHECK(ams_mel_test_completion_boundary(family, 2, 0, NULL));
    const ams_mel_status_t terminal = exceptional ? AMS_MEL_PROVIDER_EXCEPTION :
        rejection ? AMS_MEL_COMMAND_REJECTED : AMS_MEL_OK;
    /* Retain the public handle through final reclamation and cached Wait. */
    if (type == 0 || type == 1 || type == 3) {
        FAILURE_CHECK(failure_wait(&f, type, 15000, rejection) == terminal);
        FAILURE_CHECK(failure_wait(&f, type, 0, rejection) == terminal);
    }
    FAILURE_CHECK(ams_mel_test_completion_owner(family, ++owner_target, &observed));
    FAILURE_CHECK(admission_is(token, 0));
    failure_counts(&f, family, 0);
    FAILURE_CHECK(ams_mel_test_completion_snapshot(family, values));
    FAILURE_CHECK(values[0] == 1 && values[1] == 0 && values[4] == 1 && values[5] == 1);
    close_requests(&f);
    FAILURE_CHECK(submit_one(&f, type) == AMS_MEL_OK);
    FAILURE_CHECK(admission_is(token, 1));
    failure_counts(&f, family, 1);
    FAILURE_CHECK(f.gate(0, 0, &sent, NULL) && sent == 2);
    FAILURE_CHECK(f.gate(2, 1, NULL, NULL));
    FAILURE_CHECK(ams_mel_test_completion_owner(family, ++owner_target, &observed));
    FAILURE_CHECK(admission_is(token, 0));
    failure_counts(&f, family, 0);
    FAILURE_CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
    cleanup(&f);
    FAILURE_CHECK(dlclose(library) == 0);
    printf("type=%u %s: recovered same Session; requests=0 permits=0; cached terminal=%u\n", type, kind, terminal);
    return 1;
}
#undef FAILURE_CHECK

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "failure") == 0)
        return run_failure((unsigned)strtoul(argv[2], NULL, 10), argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "concurrent-admission") == 0)
        return run_concurrent_admission() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "isolated-admission") == 0)
        return run_isolated_admission() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "parent-admission") == 0)
        return run_parent_admission() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "bounded-workload") == 0)
        return run_bounded_workload() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "admission") == 0)
        return run_admission() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc > 1 && strcmp(argv[1], "retained") == 0)
        return run_retained(argc > 2) ? EXIT_SUCCESS : EXIT_FAILURE;
    return run_stress(argc > 1) ? EXIT_SUCCESS : EXIT_FAILURE;
}
