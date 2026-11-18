#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "admission_observation.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_owner_notify(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_scheduling_requests(const ams_mel_ir_scheduling *, size_t *);
typedef unsigned (*calls_fn)(void);
typedef int (*release_fn)(unsigned, unsigned);
typedef void (*reentry_fn)(void (*)(void));
typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef int (*ack_fn)(uint32_t, uint32_t, unsigned);
typedef struct mock_rep {
    uint32_t command, activity, request, task;
    int64_t duration;
    int32_t track;
    uint32_t previous_activity, previous_request, previous_task, previous_status;
} mock_rep;
typedef int (*rep_fn)(const mock_rep *, unsigned);
static ams_mel_ir_scheduling *channel;
static ams_mel_ir_channel *view;
static ams_mel_ir_return_request *request;
static ams_mel_ir_stare_task_v1 input = {-0.0, 1.23456789012345, INT64_MIN, 0, 1,
    UINT32_MAX, {INT64_MIN, 0xf1234567U, 0U, UINT32_MAX, 0x80000001U}};
#define info input.task_info
static void (*values)(uint64_t *);
static void verify_values(void)
{
    uint64_t actual[11], az, el;
    memcpy(&az, &input.center_azimuth_rad, sizeof az);
    memcpy(&el, &input.center_elevation_rad, sizeof el);
    values(actual);
    CHECK(actual[0] == input.command_id && actual[1] == az && actual[2] == el);
    CHECK(actual[3] == input.elevation_frame && actual[4] == input.azimuth_frame);
    CHECK(actual[5] == (uint64_t)input.duration_ns);
    CHECK(actual[6] == (uint64_t)info.system_time_ns && actual[7] == info.request_id);
    CHECK(actual[8] == info.action && actual[9] == info.precedence_within_priority && actual[10] == info.priority);
}
static ams_mel_status_t submitted;
static void reenter(void)
{
    size_t pending = 0;
    CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 1);
    CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
}
static void *submit(void *unused)
{
    (void)unused;
    submitted = ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL);
    return NULL;
}
static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[160]; unsigned result = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, event)) ++result;
    }
    CHECK(fclose(file) == 0);
    return result;
}
int main(int argc, char **argv)
{
    char scenario[100], path[] = "/tmp/ams-stare-XXXXXX", diagnostic[1200];
    const char *mode;
    ams_mel_session *session = NULL;
    ams_mel_test_admission_token *admission = NULL;
    ams_mel_ir_scheduling_config_v1 config = {0};
    ams_mel_ir_return_result_v1 result = {0}, cached = {0};
    ams_mel_ir_return_request *keepalive = NULL;
    ams_mel_ir_channel_comms_request *comms = NULL;
    ams_mel_ir_channel_comms_test_request_v1 ids = {0x80000001U, 0xf0000002U, 0xe0000003U};
    ams_mel_ir_scheduling_execute_task_ack_metadata *ack = NULL;
    ams_mel_ir_scheduling_task_executing_metadata *rep = NULL;
    calls_fn calls, other;
    release_fn release;
    reentry_fn set_reentry;
    barrier_fn barrier;
    ack_fn deliver_ack;
    rep_fn deliver_rep;
    void *pin; uint64_t observed = 0; size_t required = 0;
    unsigned dispatches = 1; int fd; pthread_t thread;
    CHECK(argc == 2); mode = argv[1];
    snprintf(scenario, sizeof scenario, "sched-stare-%s", mode);
    if (!strcmp(mode, "closure") || !strcmp(mode, "observer-close") || !strcmp(mode, "concurrent-close") ||
        !strncmp(mode, "post-", 5))
        strcpy(scenario, "sched-stare-delayed");
    if (!strcmp(mode, "failed")) strcpy(scenario, "sched-enable-fail");
    fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    const ams_mel_session_options_v1 options = {8};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0, &admission, NULL, NULL));
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD); CHECK(pin);
    *(void **)(&calls) = dlsym(pin, "mock_scheduling_stare_calls");
    *(void **)(&other) = dlsym(pin, "mock_scheduling_specific_calls");
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&set_reentry) = dlsym(pin, "mock_scheduling_stare_reentry");
    *(void **)(&barrier) = dlsym(pin, "mock_c2_send_barrier");
    *(void **)(&deliver_ack) = dlsym(pin, "mock_scheduling_ack_deliver");
    *(void **)(&deliver_rep) = dlsym(pin, "mock_scheduling_rep_deliver");
    *(void **)(&values) = dlsym(pin, "mock_scheduling_stare_values");
    CHECK(values);
    CHECK(calls && other && release && set_reentry && barrier && deliver_ack && deliver_rep);
    config.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_scheduling(channel, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(calls() == 0 && !request);
    if (!strcmp(mode, "failed")) {
        CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        dispatches = 0;
    } else {
        CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_scheduling_exec_stare(NULL, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, NULL, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        input.elevation_frame = 2;
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(!request && calls() == 0);
        input.elevation_frame = 0;
        input.azimuth_frame = UINT32_MAX;
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(!request && calls() == 0);
        input.azimuth_frame = 1;
        info.action = UINT32_MAX;
        CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(calls() == 0);
        info.action = 0;
        if (!strcmp(mode, "return-invalid-future") || !strcmp(mode, "comms-invalid-future")) {
            const unsigned family = !strncmp(mode, "comms", 5) ? 2U : 1U;
            uint64_t counters[6] = {0};
            size_t pending = 99;
            submitted = family == 2U ?
                ams_mel_ir_channel_submit_comms_test(view, &ids, &comms, NULL, 0, NULL) :
                ams_mel_ir_channel_send_keepalive(view, &keepalive, NULL, 0, NULL);
            CHECK(submitted == AMS_MEL_PROVIDER_FAILED && !comms && !keepalive);
            CHECK(ams_mel_test_completion_owner_notify(family, 0, 0, &observed));
            CHECK(ams_mel_test_completion_snapshot(family, counters));
            CHECK(counters[0] == 0 && counters[4] == 0 && counters[5] == 0);
            CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 0);
            CHECK(count(path, family == 2U ? "comms_sent" : "keepalive_sent") == 1);
            dispatches = 0;
            goto cleanup;
        }
        if (!strncmp(mode, "pre-", 4)) {
            size_t pending = 99;
            const char *fault = mode + 4;
            const int common_comms = !strncmp(fault, "comms-", 6);
            const int common_return = !strncmp(fault, "return-", 7);
            if (common_comms) fault += 6;
            if (common_return) fault += 7;
            CHECK(setenv("AMS_MEL_TEST_COMMON_PRE_SEND_FAILURE", fault, 1) == 0);
            if (common_comms)
                submitted = ams_mel_ir_channel_submit_comms_test(view, &ids, &comms, NULL, 0, NULL);
            else if (common_return)
                submitted = ams_mel_ir_channel_send_keepalive(view, &keepalive, NULL, 0, NULL);
            else submit(NULL);
            CHECK(!comms && !keepalive);
            CHECK(count(path, "comms_sent") == 0 && count(path, "keepalive_sent") == 0);
            CHECK(submitted == AMS_MEL_INTERNAL_ERROR && !request && calls() == 0);
            CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 0);
            uint32_t active = 99, maximum = 0;
            CHECK(ams_mel_test_completion_owner_notify(1, 0, 0, &observed));
            CHECK(ams_mel_test_completion_owner_notify(2, 0, 0, &observed));
            CHECK(ams_mel_test_admission(NULL, 1, &admission, &active, &maximum) && active == 0);
            CHECK(unsetenv("AMS_MEL_TEST_COMMON_PRE_SEND_FAILURE") == 0);
            dispatches = 0;
        } else if (!strncmp(mode, "post-", 5)) {
            size_t pending = 0;
            CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", mode + 5, 1) == 0);
            submit(NULL);
            CHECK(submitted == AMS_MEL_INTERNAL_ERROR && !request && calls() == 1);
            CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 1);
            uint32_t active = 99, maximum = 0;
            CHECK(ams_mel_test_admission(NULL, 1, &admission, &active, &maximum) && active == 1);
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_test_completion_boundary(1, 0, 0, NULL));
            CHECK(dlclose(pin) == 0);
            CHECK(count(path, "library_unloaded") == 0);
            CHECK(count(path, "scheduling_disabled") == 0);
            /* The admitted graph pins this function until release returns. */
            CHECK(release(16, 1));
            CHECK(ams_mel_test_completion_boundary(1, 1, 1, NULL));
            CHECK(ams_mel_test_completion_boundary(1, 2, 0, NULL));
            CHECK(ams_mel_test_completion_owner_notify(1, 0, 0, &observed));
            CHECK(count(path, "library_unloaded") == 1);
            CHECK(count(path, "scheduling_disabled") == 1);
            CHECK(count(path, "scheduling_channel_destroyed") == 1);
            CHECK(count(path, "channel_detached") == 1);
            CHECK(ams_mel_test_admission(NULL, 1, &admission, &active, &maximum) && active == 0);
            CHECK(ams_mel_test_admission(NULL, 2, &admission, NULL, NULL));
            CHECK(unlink(path) == 0);
            return 0;
        }
        if (!strncmp(mode, "pre-", 4)) goto cleanup;
        if (!strcmp(mode, "detach-fail")) {
            submit(NULL);
            CHECK(submitted == AMS_MEL_OK && request && calls() == 1);
            CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(release(16, 1));
            CHECK(ams_mel_test_completion_owner_notify(1, 0, 0, &observed));
            CHECK(count(path, "channel_detach_failed") == 1);
            CHECK(count(path, "scheduling_disabled") == 1);
            CHECK(count(path, "scheduling_channel_destroyed") == 0);
            CHECK(dlclose(pin) == 0);
            CHECK(count(path, "library_unloaded") == 0);
            uint32_t active = 99, maximum = 0;
            CHECK(ams_mel_test_admission(NULL, 1, &admission, &active, &maximum) && active == 0);
            CHECK(ams_mel_test_admission(NULL, 2, &admission, NULL, NULL));
            CHECK(unlink(path) == 0);
            return 0; /* Real detach uncertainty still conservatively retains. */
        }
        if (!strcmp(mode, "reentry")) set_reentry(reenter);
        if (!strcmp(mode, "concurrent-close")) {
            CHECK(barrier(16, 0, 0));
            CHECK(pthread_create(&thread, NULL, submit, NULL) == 0);
            CHECK(barrier(16, 1, 0));
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(count(path, "scheduling_disabled") == 0);
            CHECK(barrier(16, 2, 0));
            CHECK(pthread_join(thread, NULL) == 0);
        } else submit(NULL);
        verify_values();
        if (!strcmp(mode, "exec-throw") || !strcmp(mode, "invalid-future")) {
            CHECK(submitted == (!strcmp(mode, "exec-throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED) && !request);
        } else {
            CHECK(submitted == AMS_MEL_OK && request);
            if (!strcmp(scenario, "sched-stare-delayed") && channel) {
                CHECK(ams_mel_ir_scheduling_execute_task_ack_open(channel, 2, &ack, NULL, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_ir_scheduling_task_executing_open(channel, 2, &rep, NULL, NULL, 0, NULL) == AMS_MEL_OK);
                { ams_mel_ir_scheduling_execute_task_ack_event *event = NULL;
                  CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT); }
                { ams_mel_ir_scheduling_task_executing_event *event = NULL;
                  CHECK(ams_mel_ir_scheduling_task_executing_receive(rep, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT); }
                /* An actual provider acknowledgement neither matches nor
                 * completes this future. Deliberately use unrelated IDs. */
                CHECK(deliver_ack(17, 93, 0));
                { ams_mel_ir_scheduling_execute_task_ack_event *event = NULL;
                  CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
                  CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK); }
                CHECK(ams_mel_ir_return_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                { const mock_rep independent = {71, 82, 93, 6, -1, -1, 0, 0, 0, 0};
                  ams_mel_ir_scheduling_task_executing_event *event = NULL;
                  CHECK(deliver_rep(&independent, 0));
                  CHECK(ams_mel_ir_scheduling_task_executing_receive(rep, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
                  CHECK(ams_mel_ir_scheduling_task_executing_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
                  CHECK(ams_mel_ir_return_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT); }
            }
            if (channel) {
                ams_mel_ir_return_request *saved = request;
                CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
                CHECK(request == saved);
            }
            if (!strcmp(scenario, "sched-stare-delayed")) {
                CHECK(ams_mel_ir_return_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                if (channel) {
                    CHECK(ams_mel_ir_channel_send_keepalive(view, &keepalive, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(ams_mel_ir_channel_submit_comms_test(view, &ids, &comms, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
                }
                CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(count(path, "scheduling_disabled") == 0);
                /* Remove the test DSO pin while the future is still pending.
                 * Only the admitted completion graph may now protect code. */
                CHECK(dlclose(pin) == 0);
                pin = NULL;
                CHECK(count(path, "library_unloaded") == 0);
                /* Re-pin before calling test-only release functions: bridge
                 * retention protects pending work, not the harness's own code
                 * executing after it makes the final promise ready. */
                pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
                CHECK(pin);
                if (!strcmp(mode, "observer-close"))
                    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
                if (keepalive) {
                    CHECK(release(1, 1)); CHECK(release(2, 1));
                    CHECK(ams_mel_ir_return_request_wait(keepalive, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(ams_mel_ir_return_request_close(&keepalive, NULL, 0, NULL) == AMS_MEL_OK);
                    { ams_mel_ir_channel_comms_test_result_v1 cr = {0};
                      CHECK(ams_mel_ir_channel_comms_request_wait(comms, 5000, &cr, NULL, 0, NULL) == AMS_MEL_OK); }
                    CHECK(ams_mel_ir_channel_comms_request_close(&comms, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(count(path, "scheduling_disabled") == 0);
                }
                CHECK(calls() == dispatches && other() == 0);
                CHECK(release(16, 1));
            }
            if (request) {
                ams_mel_status_t status = ams_mel_ir_return_request_wait(request, 5000, &result, diagnostic, sizeof diagnostic, &required);
                ams_mel_status_t again = ams_mel_ir_return_request_wait(request, 0, &cached, diagnostic, sizeof diagnostic, &required);
                CHECK(status == again && result.value == cached.value && result.error_code == cached.error_code);
                if (!strcmp(mode, "unsupported") || !strcmp(mode, "invalid-state")) {
                    CHECK(status == AMS_MEL_COMMAND_REJECTED);
                    CHECK(result.error_code == (!strcmp(mode, "unsupported") ? AMS_MEL_ERROR_UNSUPPORTED : AMS_MEL_ERROR_INVALID_STATE));
                    CHECK(required == 1046 && strlen(diagnostic) == 1045);
                } else if (!strcmp(mode, "null") || !strcmp(mode, "invalid-return") || !strcmp(mode, "malformed")) CHECK(status == AMS_MEL_PROVIDER_FAILED);
                else if (!strcmp(mode, "future-throw") || !strcmp(mode, "invalid-future")) CHECK(status == AMS_MEL_PROVIDER_EXCEPTION);
                else {
                    CHECK(status == AMS_MEL_OK);
                    CHECK(result.value == (!strcmp(mode, "fail") ? AMS_MEL_IR_RETURN_FAIL :
                        !strcmp(mode, "not-supported") ? AMS_MEL_IR_RETURN_NOT_SUPPORTED :
                        !strcmp(mode, "not-implemented") ? AMS_MEL_IR_RETURN_NOT_IMPLEMENTED : AMS_MEL_IR_RETURN_SUCCESS));
                }
                CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            }
            if (!strcmp(mode, "success")) {
                for (info.action = 1; info.action != 4; ++info.action) {
                    info.system_time_ns = info.action == 1 ? INT64_MAX : 0;
                    input.command_id = info.action == 1 ? 0 : info.action == 2 ? 0x80000000U : UINT32_MAX;
                    input.duration_ns = info.action == 1 ? INT64_MAX : info.action == 2 ? 0 : -987654321;
                    input.elevation_frame = info.action & 1U;
                    input.azimuth_frame = info.action == 3 ? 1U : 0U;
                    input.center_azimuth_rad = info.action == 1 ? 2.34567890123456 : 0.0;
                    input.center_elevation_rad = -0.0;
                    info.request_id = input.command_id;
                    info.precedence_within_priority = info.action == 1 ? 0 : UINT32_MAX;
                    info.priority = info.action == 2 ? 0 : 0x80000000U;
                    CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_OK);
                    verify_values();
                    ++dispatches;
                    CHECK(ams_mel_ir_return_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
                }
            }
        }
    }
cleanup:
    if (pin) CHECK(calls() == dispatches && other() == 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&ack, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_close(&rep, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_stare(channel, &input, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner_notify(1, 0, 0, &observed));
    CHECK(ams_mel_test_completion_owner_notify(2, 0, 0, &observed));
    CHECK(count(path, "scheduling_disabled") == 1 && count(path, "scheduling_channel_destroyed") == 1);
    CHECK(count(path, "channel_detached") == 1);
    if (pin) {
        CHECK(count(path, "library_unloaded") == 0);
        CHECK(dlclose(pin) == 0);
    }
    CHECK(count(path, "library_unloaded") == 1);
    CHECK(ams_mel_test_admission(NULL, 2, &admission, NULL, NULL));
    CHECK(unlink(path) == 0);
    return 0;
}
