#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path required"
#endif

extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);

static ams_mel_ir_c2_config_v1 config(void)
{
    ams_mel_ir_c2_config_v1 c;
    memset(&c, 0, sizeof c);
    c.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    c.channel_id.descriptive_label = (ams_mel_string_view_v1){"scale", 5};
    c.platform_id.descriptive_label = (ams_mel_string_view_v1){"test", 4};
    c.sensor_location.key = (ams_mel_string_view_v1){"key", 3};
    c.sensor_location.system_name = (ams_mel_string_view_v1){"system", 6};
    return c;
}

static int run_case(unsigned count, int early_close, int parent_first)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *channel = NULL;
    ams_mel_ir_return_request *requests[100] = {0};
    uint64_t before[6] = {0}, pending[6] = {0}, after[6] = {0};
    uint64_t submitted = 0, released = 0;
    unsigned accepted = 0;
    void *provider = NULL;
    gate_fn gate = NULL;
    int ok = 0;
#define REQUIRE(expr) do { if (!(expr)) { \
    fprintf(stderr, "scalability %u: %s failed at %d\n", count, #expr, __LINE__); \
    goto cleanup; } } while (0)
    REQUIRE(ams_mel_test_completion_snapshot(1, before));
    REQUIRE(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
                &session, NULL, 0, NULL) == AMS_MEL_OK);
    provider = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE(provider != NULL);
    *(void **)(&gate) = dlsym(provider, "mock_completion_gate");
    REQUIRE(gate != NULL);
    ams_mel_ir_c2_config_v1 c = config();
    REQUIRE(ams_mel_ir_c2_open(session, &c, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < count; ++i) {
        REQUIRE(ams_mel_ir_c2_send_keepalive(channel, &requests[i], NULL, 0, NULL) == AMS_MEL_OK);
        ++accepted;
    }
    REQUIRE(gate(1, submitted + count, &submitted, &released));
    REQUIRE(ams_mel_test_completion_wait(1, 4, before[4] + count));
    REQUIRE(ams_mel_test_completion_snapshot(1, pending));
    REQUIRE(pending[0] == before[0] + count && pending[1] == count);
    REQUIRE(pending[2] >= count && pending[3] == before[3]);
    REQUIRE(pending[4] == before[4] + count && pending[5] == before[5]);
    if (early_close) {
        for (unsigned i = 0; i < count; ++i)
            REQUIRE(ams_mel_ir_return_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
    }
    if (parent_first) {
        REQUIRE(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        REQUIRE(ams_mel_ir_c2_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    }
    REQUIRE(ams_mel_test_completion_snapshot(1, pending) && pending[1] == count);
    REQUIRE(gate(2, count, &submitted, &released));
    REQUIRE(ams_mel_test_completion_wait(1, 3, before[3] + count));
    for (unsigned i = 0; i < count; ++i) {
        if (requests[i]) {
            ams_mel_ir_return_result_v1 result = {0};
            REQUIRE(ams_mel_ir_return_request_wait(requests[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            REQUIRE(ams_mel_ir_return_request_wait(requests[i], 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
        }
    }
    REQUIRE(ams_mel_test_completion_snapshot(1, after));
    REQUIRE(after[1] == 0 && after[0] == before[0] + count &&
            after[3] == before[3] + count && after[4] == before[4] + count &&
            after[5] == before[5] + count);
    printf("N=%u started=%llu peak=%llu gets=%llu early=%d parent=%d\n", count,
           (unsigned long long)(after[0] - before[0]), (unsigned long long)pending[2],
           (unsigned long long)(after[5] - before[5]), early_close, parent_first);
    ok = 1;
cleanup:
    /* Never leave detached workers waiting if a structural assertion fails. */
    if (gate) {
        uint64_t s = 0, r = 0;
        if (gate(0, 0, &s, &r) && s > r) (void)gate(2, s - r, NULL, NULL);
    }
    for (unsigned i = 0; i < accepted; ++i) {
        if (requests[i]) {
            ams_mel_ir_return_result_v1 result = {0};
            (void)ams_mel_ir_return_request_wait(requests[i], 15000, &result, NULL, 0, NULL);
            (void)ams_mel_ir_return_request_close(&requests[i], NULL, 0, NULL);
        }
    }
    if (channel) (void)ams_mel_ir_c2_close(&channel, NULL, 0, NULL);
    if (session) (void)ams_mel_session_close(&session, NULL, 0, NULL);
    if (provider) dlclose(provider);
    return ok;
#undef REQUIRE
}

static int run_mixed(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *channel = NULL;
    ams_mel_ir_return_request *returns[34] = {0};
    ams_mel_ir_mode_request *modes[33] = {0};
    ams_mel_ir_channel_comms_request *comms[33] = {0};
    uint64_t before[3][6] = {{0}}, after[3][6] = {{0}}, sent = 0, released = 0;
    gate_fn gate = NULL;
    void *provider = NULL;
    unsigned nr = 0, nm = 0, nc = 0;
    int ok = 0;
#define REQUIRE_MIXED(expr) do { if (!(expr)) { \
    fprintf(stderr, "mixed: %s failed at %d\n", #expr, __LINE__); goto cleanup; \
} } while (0)
    for (unsigned f = 0; f < 3; ++f)
        REQUIRE_MIXED(ams_mel_test_completion_snapshot(f, before[f]));
    REQUIRE_MIXED(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
                  &session, NULL, 0, NULL) == AMS_MEL_OK);
    provider = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE_MIXED(provider);
    *(void **)(&gate) = dlsym(provider, "mock_completion_gate");
    REQUIRE_MIXED(gate && gate(0, 0, &sent, &released));
    ams_mel_ir_c2_config_v1 c = config();
    REQUIRE_MIXED(ams_mel_ir_c2_open(session, &c, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE_MIXED(ams_mel_ir_c2_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < 34; ++i) {
        REQUIRE_MIXED(ams_mel_ir_c2_send_keepalive(channel, &returns[i], NULL, 0, NULL) == AMS_MEL_OK);
        ++nr;
        if (i < 33) {
            REQUIRE_MIXED(ams_mel_ir_c2_submit_operate(channel, 1, &modes[i], NULL, 0, NULL) == AMS_MEL_OK);
            ++nm;
            ams_mel_ir_channel_comms_test_request_v1 request = {0};
            REQUIRE_MIXED(ams_mel_ir_c2_submit_comms_test(channel, &request, &comms[i], NULL, 0, NULL) == AMS_MEL_OK);
            ++nc;
        }
    }
    REQUIRE_MIXED(gate(1, sent + 100, NULL, NULL));
    for (unsigned f = 0; f < 3; ++f) {
        const unsigned expected = f == 1 ? 34 : 33;
        REQUIRE_MIXED(ams_mel_test_completion_wait(f, 4, before[f][4] + expected));
        REQUIRE_MIXED(ams_mel_test_completion_snapshot(f, after[f]));
        REQUIRE_MIXED(after[f][1] == expected && after[f][5] == before[f][5]);
    }
    REQUIRE_MIXED(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE_MIXED(ams_mel_ir_c2_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    REQUIRE_MIXED(gate(2, 100, NULL, NULL));
    for (unsigned f = 0; f < 3; ++f)
        REQUIRE_MIXED(ams_mel_test_completion_wait(f, 3, before[f][3] + (f == 1 ? 34 : 33)));
    for (unsigned i = 0; i < nr; ++i) {
        ams_mel_ir_return_result_v1 result = {0};
        REQUIRE_MIXED(ams_mel_ir_return_request_wait(returns[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    }
    for (unsigned i = 0; i < nm; ++i) {
        ams_mel_ir_mode_result_v1 result = {0};
        REQUIRE_MIXED(ams_mel_ir_mode_request_wait(modes[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    }
    for (unsigned i = 0; i < nc; ++i) {
        ams_mel_ir_channel_comms_test_result_v1 result = {0};
        REQUIRE_MIXED(ams_mel_ir_channel_comms_request_wait(comms[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    }
    for (unsigned f = 0; f < 3; ++f) {
        REQUIRE_MIXED(ams_mel_test_completion_snapshot(f, after[f]));
        REQUIRE_MIXED(after[f][1] == 0 && after[f][5] == before[f][5] + (f == 1 ? 34 : 33));
    }
    puts("mixed C2: Return=34 Mode=33 Comms=33; gets=100; active=0");
    ok = 1;
cleanup:
    if (gate) {
        uint64_t s = 0, r = 0;
        if (gate(0, 0, &s, &r) && s > r) (void)gate(2, s - r, NULL, NULL);
    }
    for (unsigned i = 0; i < nr; ++i) if (returns[i]) {
        ams_mel_ir_return_result_v1 value = {0};
        (void)ams_mel_ir_return_request_wait(returns[i], 15000, &value, NULL, 0, NULL);
        (void)ams_mel_ir_return_request_close(&returns[i], NULL, 0, NULL);
    }
    for (unsigned i = 0; i < nm; ++i) if (modes[i]) {
        ams_mel_ir_mode_result_v1 value = {0};
        (void)ams_mel_ir_mode_request_wait(modes[i], 15000, &value, NULL, 0, NULL);
        (void)ams_mel_ir_mode_request_close(&modes[i], NULL, 0, NULL);
    }
    for (unsigned i = 0; i < nc; ++i) if (comms[i]) {
        ams_mel_ir_channel_comms_test_result_v1 value = {0};
        (void)ams_mel_ir_channel_comms_request_wait(comms[i], 15000, &value, NULL, 0, NULL);
        (void)ams_mel_ir_channel_comms_request_close(&comms[i], NULL, 0, NULL);
    }
    if (channel) (void)ams_mel_ir_c2_close(&channel, NULL, 0, NULL);
    if (session) (void)ams_mel_session_close(&session, NULL, 0, NULL);
    if (provider) dlclose(provider);
    return ok;
#undef REQUIRE_MIXED
}

/* Rejection is caller-visible backpressure, not a bridge queue. The caller
 * deliberately completes one wave before submitting the next. */
static int run_bounded(unsigned limit, unsigned total)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *channel = NULL;
    ams_mel_ir_return_request *requests[8] = {0};
    void *provider = NULL;
    gate_fn gate = NULL;
    uint64_t before[6] = {0}, after[6] = {0}, sent = 0, released = 0;
    unsigned accepted = 0, refused = 0;
    uint64_t owners[4] = {0}, observed_owner = 0;
    int ok = 0;
#define REQUIRE_BOUNDED(expr) do { if (!(expr)) { \
    fprintf(stderr, "bounded: %s failed at %d\n", #expr, __LINE__); goto cleanup; \
} } while (0)
    ams_mel_session_options_v1 options = {limit};
    REQUIRE_BOUNDED(ams_mel_test_completion_snapshot(1, before));
    /* Each options case runs in a fresh process: never subtract cumulative
     * high-water marks, which can hide oversubscription after unlimited work. */
    REQUIRE_BOUNDED(before[0] == 0 && before[2] == 0);
    REQUIRE_BOUNDED(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER,
                    "completion-scale", "", &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    provider = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    REQUIRE_BOUNDED(provider != NULL);
    *(void **)(&gate) = dlsym(provider, "mock_completion_gate");
    REQUIRE_BOUNDED(gate && gate(0, 0, &sent, &released));
    ams_mel_ir_c2_config_v1 c = config();
    REQUIRE_BOUNDED(ams_mel_ir_c2_open(session, &c, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    if (limit == 0) {
        /* Zero must retain the existing 100 simultaneously pending/gets. */
        REQUIRE_BOUNDED(total == 100);
        ams_mel_ir_return_request *all[100] = {0};
        for (unsigned i = 0; i < 100; ++i) {
            REQUIRE_BOUNDED(ams_mel_ir_c2_send_keepalive(channel, &all[i], NULL, 0, NULL) == AMS_MEL_OK);
            REQUIRE_BOUNDED(all[i] != NULL);
        }
        REQUIRE_BOUNDED(gate(1, sent + 100, NULL, NULL));
        REQUIRE_BOUNDED(ams_mel_test_completion_wait(1, 4, before[4] + 100));
        REQUIRE_BOUNDED(ams_mel_test_completion_snapshot(1, after) && after[1] == 100);
        REQUIRE_BOUNDED(gate(2, 100, NULL, NULL));
        REQUIRE_BOUNDED(ams_mel_test_completion_wait(1, 5, before[5] + 100));
        for (unsigned i = 0; i < 100; ++i) {
            ams_mel_ir_return_result_v1 result = {0};
            REQUIRE_BOUNDED(ams_mel_ir_return_request_wait(all[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            REQUIRE_BOUNDED(ams_mel_ir_return_request_close(&all[i], NULL, 0, NULL) == AMS_MEL_OK);
        }
        REQUIRE_BOUNDED(ams_mel_test_completion_boundary(1, 3, 0, owners));
        REQUIRE_BOUNDED(ams_mel_test_completion_owner(1, owners[2], &observed_owner));
        REQUIRE_BOUNDED(ams_mel_test_completion_snapshot(1, after));
        REQUIRE_BOUNDED(after[1] == 0 && after[2] == 100 && after[5] == 100);
        ok = 1;
        goto cleanup;
    }
    REQUIRE_BOUNDED(limit <= 8);
    for (unsigned wave = 0; accepted < total; ++wave) {
        const unsigned wave_size = total - accepted < limit ? total - accepted : limit;
        for (unsigned i = 0; i < wave_size; ++i) {
            REQUIRE_BOUNDED(ams_mel_ir_c2_send_keepalive(channel, &requests[i], NULL, 0, NULL) == AMS_MEL_OK);
            ++accepted;
        }
        REQUIRE_BOUNDED(gate(1, sent + wave_size, &sent, &released));
        REQUIRE_BOUNDED(ams_mel_test_completion_wait(1, 4, before[4] + accepted));
        REQUIRE_BOUNDED(ams_mel_test_completion_snapshot(1, after));
        REQUIRE_BOUNDED(after[1] == wave_size && after[2] <= limit);
        ams_mel_ir_return_request *rejected = NULL;
        char diagnostic[80] = {0};
        uint64_t observed = 0;
        if (wave_size == limit) {
            REQUIRE_BOUNDED(ams_mel_ir_c2_send_keepalive(channel, &rejected,
                            diagnostic, sizeof diagnostic, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
            ++refused;
            REQUIRE_BOUNDED(rejected == NULL && strcmp(diagnostic, "async request limit reached") == 0);
            REQUIRE_BOUNDED(gate(0, 0, &observed, NULL) && observed == sent);
        }
        if (wave == 0) {
            REQUIRE_BOUNDED(ams_mel_ir_return_request_close(&requests[0], NULL, 0, NULL) == AMS_MEL_OK);
            REQUIRE_BOUNDED(ams_mel_ir_c2_send_keepalive(channel, &rejected,
                            diagnostic, sizeof diagnostic, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
            REQUIRE_BOUNDED(rejected == NULL && gate(0, 0, &observed, NULL) && observed == sent);
        }
        REQUIRE_BOUNDED(gate(2, wave_size, NULL, NULL));
        REQUIRE_BOUNDED(ams_mel_test_completion_wait(1, 3, before[3] + accepted));
        REQUIRE_BOUNDED(ams_mel_test_completion_wait(1, 5, before[5] + accepted));
        /* Refused attempts also preallocate WorkerInputs. Snapshot the total
         * created count after submissions stop, then await ALL final owners.
         * Worker-return counters alone precede captured-input destruction. */
        REQUIRE_BOUNDED(ams_mel_test_completion_boundary(1, 3, 0, owners));
        REQUIRE_BOUNDED(ams_mel_test_completion_owner(1, owners[2], &observed_owner));
        for (unsigned i = 0; i < wave_size; ++i) if (requests[i]) {
            ams_mel_ir_return_result_v1 result = {0};
            REQUIRE_BOUNDED(ams_mel_ir_return_request_wait(requests[i], 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            REQUIRE_BOUNDED(ams_mel_ir_return_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
        }
    }
    REQUIRE_BOUNDED(ams_mel_test_completion_snapshot(1, after) && after[1] == 0);
    REQUIRE_BOUNDED(after[0] == before[0] + total && after[5] == before[5] + total);
    printf("bounded limit=%u admitted=%u refused=%u peak=%llu gets=%llu\n",
           limit, accepted, refused, (unsigned long long)after[2],
           (unsigned long long)(after[5] - before[5]));
    ok = 1;
cleanup:
    if (gate) {
        uint64_t s = 0, r = 0;
        if (gate(0, 0, &s, &r) && s > r) (void)gate(2, s - r, NULL, NULL);
    }
    for (unsigned i = 0; i < 8; ++i) if (requests[i]) {
        ams_mel_ir_return_result_v1 result = {0};
        (void)ams_mel_ir_return_request_wait(requests[i], 15000, &result, NULL, 0, NULL);
        (void)ams_mel_ir_return_request_close(&requests[i], NULL, 0, NULL);
    }
    if (channel) (void)ams_mel_ir_c2_close(&channel, NULL, 0, NULL);
    if (session) (void)ams_mel_session_close(&session, NULL, 0, NULL);
    if (provider) dlclose(provider);
    return ok;
#undef REQUIRE_BOUNDED
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "options-zero") == 0) return run_bounded(0, 100) ? EXIT_SUCCESS : EXIT_FAILURE;
        if (strcmp(argv[1], "bounded-one") == 0) return run_bounded(1, 1) ? EXIT_SUCCESS : EXIT_FAILURE;
        if (strcmp(argv[1], "bounded-eight") == 0) return run_bounded(8, 100) ? EXIT_SUCCESS : EXIT_FAILURE;
        return EXIT_FAILURE;
    }
    return run_case(1, 0, 0) && run_case(10, 0, 0) &&
           run_case(100, 0, 0) && run_case(100, 1, 1) && run_mixed()
           ? EXIT_SUCCESS : EXIT_FAILURE;
}
