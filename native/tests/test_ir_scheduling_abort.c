#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
extern int ams_mel_test_completion_owner_notify(unsigned, unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_scheduling_requests(const ams_mel_ir_scheduling *, size_t *);
typedef unsigned (*calls_fn)(void);
typedef int (*release_fn)(unsigned, unsigned);
typedef void (*reentry_fn)(void (*)(void));
typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef int (*ack_fn)(uint32_t, uint32_t, unsigned);
static ams_mel_ir_scheduling *channel;
static ams_mel_ir_channel *view;
static ams_mel_ir_return_request *request;
static ams_mel_ir_task_info_v1 info = {INT64_MIN, 0xf1234567U, 0U, UINT32_MAX, 0x80000001U};
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
    submitted = ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL);
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
    char scenario[100], path[] = "/tmp/ams-abort-XXXXXX", diagnostic[1200];
    const char *mode;
    ams_mel_session *session = NULL;
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
    void *pin; uint64_t observed = 0; size_t required = 0;
    unsigned dispatches = 1; int fd; pthread_t thread;
    CHECK(argc == 2); mode = argv[1];
    snprintf(scenario, sizeof scenario, "sched-abort-%s", mode);
    if (!strcmp(mode, "closure") || !strcmp(mode, "observer-close") || !strcmp(mode, "concurrent-close") ||
        !strncmp(mode, "post-", 5))
        strcpy(scenario, "sched-abort-delayed");
    if (!strcmp(mode, "failed")) strcpy(scenario, "sched-enable-fail");
    fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session, NULL, 0, NULL) == AMS_MEL_OK);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD); CHECK(pin);
    *(void **)(&calls) = dlsym(pin, "mock_scheduling_abort_calls");
    *(void **)(&other) = dlsym(pin, "mock_scheduling_specific_calls");
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&set_reentry) = dlsym(pin, "mock_scheduling_abort_reentry");
    *(void **)(&barrier) = dlsym(pin, "mock_c2_send_barrier");
    *(void **)(&deliver_ack) = dlsym(pin, "mock_scheduling_ack_deliver");
    CHECK(calls && other && release && set_reentry && barrier && deliver_ack);
    config.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_scheduling(channel, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(calls() == 0 && !request);
    if (!strcmp(mode, "failed")) {
        CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        dispatches = 0;
    } else {
        CHECK(ams_mel_ir_scheduling_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_scheduling_exec_abort(NULL, &info, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_exec_abort(channel, NULL, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        info.action = UINT32_MAX;
        CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(calls() == 0);
        info.action = 0;
        if (!strncmp(mode, "post-", 5)) {
            size_t pending = 0;
            CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE", mode + 5, 1) == 0);
            submit(NULL);
            CHECK(submitted == AMS_MEL_INTERNAL_ERROR && !request && calls() == 1);
            CHECK(ams_mel_test_scheduling_requests(channel, &pending) && pending == 1);
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(release(15, 1));
            CHECK(dlclose(pin) == 0);
            CHECK(count(path, "library_unloaded") == 0);
            CHECK(count(path, "scheduling_disabled") == 0);
            CHECK(count(path, "scheduling_channel_destroyed") == 0);
            /* No completion worker was launched; the existing engine retains
             * this uncertain graph permanently instead of unsafe teardown. */
            CHECK(unlink(path) == 0);
            return 0;
        }
        if (!strcmp(mode, "reentry")) set_reentry(reenter);
        if (!strcmp(mode, "concurrent-close")) {
            CHECK(barrier(15, 0, 0));
            CHECK(pthread_create(&thread, NULL, submit, NULL) == 0);
            CHECK(barrier(15, 1, 0));
            CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(count(path, "scheduling_disabled") == 0);
            CHECK(barrier(15, 2, 0));
            CHECK(pthread_join(thread, NULL) == 0);
        } else submit(NULL);
        if (!strcmp(mode, "exec-throw")) {
            CHECK(submitted == AMS_MEL_PROVIDER_EXCEPTION && !request);
        } else {
            CHECK(submitted == AMS_MEL_OK && request);
            if (!strcmp(scenario, "sched-abort-delayed") && channel) {
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
            }
            if (channel) CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
            if (!strcmp(scenario, "sched-abort-delayed")) {
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
                CHECK(release(15, 1));
            }
            if (request) {
                ams_mel_status_t status = ams_mel_ir_return_request_wait(request, 5000, &result, diagnostic, sizeof diagnostic, &required);
                ams_mel_status_t again = ams_mel_ir_return_request_wait(request, 0, &cached, diagnostic, sizeof diagnostic, &required);
                CHECK(status == again && result.value == cached.value && result.error_code == cached.error_code);
                if (!strcmp(mode, "unsupported") || !strcmp(mode, "invalid-state")) {
                    CHECK(status == AMS_MEL_COMMAND_REJECTED);
                    CHECK(result.error_code == (!strcmp(mode, "unsupported") ? AMS_MEL_ERROR_UNSUPPORTED : AMS_MEL_ERROR_INVALID_STATE));
                    CHECK(required == 1046 && strlen(diagnostic) == 1045);
                } else if (!strcmp(mode, "null") || !strcmp(mode, "invalid-return")) CHECK(status == AMS_MEL_PROVIDER_FAILED);
                else if (!strcmp(mode, "future-throw")) CHECK(status == AMS_MEL_PROVIDER_EXCEPTION);
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
                    CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_OK);
                    ++dispatches;
                    CHECK(ams_mel_ir_return_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
                    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
                }
            }
        }
    }
    if (pin) CHECK(calls() == dispatches && other() == 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&ack, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_close(&rep, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_abort(channel, &info, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
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
    CHECK(unlink(path) == 0);
    return 0;
}
