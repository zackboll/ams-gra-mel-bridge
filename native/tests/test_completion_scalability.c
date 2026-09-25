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

int main(void)
{
    return run_case(1, 0, 0) && run_case(10, 0, 0) &&
           run_case(100, 0, 0) && run_case(100, 1, 1) && run_mixed() ? EXIT_SUCCESS : EXIT_FAILURE;
}
