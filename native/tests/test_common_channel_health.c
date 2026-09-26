#define _POSIX_C_SOURCE 200809L
#include "common_channel_view.h"
#include "admission_observation.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
typedef int (*release_fn)(unsigned, unsigned);
typedef int (*send_barrier_fn)(unsigned, unsigned, unsigned);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static ams_mel_string_view_v1 view(const char *text)
{ ams_mel_string_view_v1 result = {text, strlen(text)}; return result; }
static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[160];
    unsigned result = 0;
    CHECK(file != NULL);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++result;
    CHECK(fclose(file) == 0);
    return result;
}
static void ordered(const char *path, const char *first, const char *second,
                    const char *third, const char *fourth)
{
    FILE *file = fopen(path, "r");
    CHECK(file != NULL);
    const char *events[4] = {first, second, third, fourth};
    char line[160];
    unsigned index = 0;
    while (fgets(line, sizeof line, file)) {
        if (index < 4U && !strcmp(line, events[index])) ++index;
    }
    CHECK(fclose(file) == 0 && index == 4U);
}
static void marker(const char *path)
{
    FILE *file = fopen(path, "w");
    CHECK(file != NULL && fclose(file) == 0);
}
static void reached(const char *path)
{
    for (unsigned i = 0; i < 15000U; ++i) {
        if (access(path, F_OK) == 0) return;
        struct timespec interval = {0, 1000000L};
        CHECK(nanosleep(&interval, NULL) == 0);
    }
    CHECK(0);
}
static void logged(const char *path, const char *event)
{
    for (unsigned i = 0; i < 15000U; ++i) {
        if (count(path, event) != 0U) return;
        struct timespec interval = {0, 1000000L};
        CHECK(nanosleep(&interval, NULL) == 0);
    }
    CHECK(0);
}
typedef struct {
    ams_mel_test_common_channel *common;
    ams_mel_ir_return_request *request;
    ams_mel_status_t status;
} submit_input;
static void *submit_worker(void *argument)
{
    submit_input *input = argument;
    input->status = ams_mel_test_common_send_keepalive(input->common,
        &input->request, NULL, 0, NULL);
    return NULL;
}
static void verify_admission(ams_mel_test_admission_token **token, uint32_t expected)
{
    uint32_t active = 99, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 1, token, &active, &maximum));
    CHECK(active == expected);
}
static void verify_final_owner(unsigned family, uint64_t owners,
                               ams_mel_test_admission_token **token)
{
    uint64_t observed = 0;
    CHECK(ams_mel_test_completion_wait(family, 3, 1U));
    CHECK(ams_mel_test_completion_owner(family, owners, &observed));
    verify_admission(token, 0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    const int comms = strstr(name, "comms") != NULL;
    const int weak = !strcmp(name, "weak");
    const int first = !strcmp(name, "close-first");
    const int parent = strstr(name, "parent") != NULL;
    const int enabled = strstr(name, "enabled") != NULL;
    const int metadata_stop = !strcmp(name, "metadata-stop");
    const int inflight = !strcmp(name, "metadata-inflight");
    const int race = strstr(name, "close-wins") != NULL;
    const int cross = strstr(name, "cross") != NULL;
    const int reverse = !strcmp(name, "cross-reverse");
    const int throws = strstr(name, "throw") != NULL;
    const int post = strstr(name, "allocation") || strstr(name, "worker-launch");
    const int finish = !strcmp(name, "finish-exception");
    const int detach = !strcmp(name, "deferred-detach-fail");
    const char *scenario = inflight ? "health-metadata-nonquiescing-disable" :
        detach ? "health-detach-fail" :
        throws ? (comms ? "health-comms-throw" : "health-keepalive-throw") :
        "health-common";
    char path[] = "/tmp/ams-common-health-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    char barrier_base[160] = {0}, callback_start[180], callback_entered[180];
    if (inflight) {
        CHECK(snprintf(barrier_base, sizeof barrier_base, "%s-callback", path) > 0);
        CHECK(setenv("AMS_MEL_TEST_HEALTH_CALLBACK_BARRIER", barrier_base, 1) == 0);
        CHECK(snprintf(callback_start, sizeof callback_start, "%s.start", barrier_base) > 0);
        CHECK(snprintf(callback_entered, sizeof callback_entered, "%s.entered", barrier_base) > 0);
    }
    if (post) CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE",
        strstr(name, "allocation") ? "allocation" : "worker-launch", 1) == 0);
    if (finish) CHECK(setenv("AMS_MEL_TEST_HEALTH_FINISH_FAILURE", "exception", 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_health *health = NULL;
    ams_mel_ir_health_metadata *metadata = NULL;
    ams_mel_test_common_channel *common = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_session_options_v1 options = {1};
    ams_mel_ir_channel_comms_test_request_v1 command = {0x80000001U, 0xf0000002U, 0xe0000003U};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
        &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    gate_fn gate = NULL;
    release_fn release_family = NULL;
    *(void **)(&gate) = dlsym(library, "mock_completion_gate");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    CHECK(gate != NULL && release_family != NULL);
    send_barrier_fn send_barrier = NULL;
    *(void **)(&send_barrier) = dlsym(library, "mock_c2_send_barrier");
    CHECK(send_barrier != NULL);
    ams_mel_ir_health_config_v1 config = {0};
    config.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    config.channel_id.descriptive_label = view("health");
    config.platform_id.descriptive_label = view("platform");
    config.sensor_location.key = view("key");
    config.sensor_location.system_name = view("system");
    CHECK(ams_mel_ir_health_open(session, &config, &health, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_common_from_health(health, &common) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0, &token, NULL, NULL));
    if (metadata_stop || inflight) CHECK(ams_mel_ir_health_metadata_open(health, 4, &metadata,
        NULL, 0, NULL) == AMS_MEL_OK);
    if (enabled || inflight || (race && !strstr(name, "attached")))
        CHECK(ams_mel_ir_health_enable(health, NULL, 0, NULL) == AMS_MEL_OK);
    if (race) {
        submit_input input = {common, NULL, AMS_MEL_INTERNAL_ERROR};
        pthread_t thread;
        CHECK(send_barrier(1, 0, 1));
        CHECK(pthread_create(&thread, NULL, submit_worker, &input) == 0);
        CHECK(send_barrier(1, 1, 0));
        size_t requests = 0;
        CHECK(ams_mel_test_health_requests(health, &requests) && requests == 1U);
        verify_admission(&token, 1);
        CHECK(count(path, "keepalive_sent\n") == 0U);
        CHECK(ams_mel_ir_health_close(&health, NULL, 0, NULL) == AMS_MEL_OK && !health);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(count(path, "health_disabled\n") == 0U && count(path, "channel_detached\n") == 0U);
        CHECK(count(path, "health_channel_destroyed\n") == 0U);
        CHECK(count(path, "control_destroyed\n") == 0U && count(path, "manager_destroyed\n") == 0U);
        CHECK(send_barrier(1, 2, 0));
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(input.status == AMS_MEL_PROVIDER_EXCEPTION && !input.request);
        verify_admission(&token, 0);
        uint64_t counters[6] = {0};
        CHECK(ams_mel_test_completion_snapshot(1, counters));
        CHECK(counters[0] == 0U && counters[2] == 0U && counters[3] == 0U);
        CHECK(count(path, "health_disabled\n") == (strstr(name, "attached") ? 0U : 1U));
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, "health_channel_destroyed\n") == 1U);
        CHECK(count(path, "control_destroyed\n") == 1U && count(path, "manager_destroyed\n") == 1U);
        CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
        CHECK(dlclose(library) == 0);
        CHECK(count(path, "library_unloaded\n") == 1U);
        ams_mel_test_common_close(&common);
        CHECK(unlink(path) == 0);
        return 0;
    }
    if (cross) {
        ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_mode_request *mode = NULL;
        ams_mel_ir_c2_config_v1 c2_config = {0};
        c2_config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        c2_config.channel_id.descriptive_label = view("c2");
        c2_config.platform_id.descriptive_label = view("platform");
        c2_config.sensor_location.key = view("key");
        c2_config.sensor_location.system_name = view("system");
        CHECK(ams_mel_ir_c2_open(session, &c2_config, &c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
        if (reverse) {
            CHECK(ams_mel_ir_c2_submit_operate(c2, 7U, &mode, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(gate(0, 1, NULL, NULL));
            CHECK(ams_mel_test_completion_wait(0, 1, 1U));
        } else {
            CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(gate(0, 1, NULL, NULL));
            CHECK(ams_mel_test_completion_wait(1, 1, 1U));
        }
        size_t requests = 99;
        CHECK(ams_mel_test_health_requests(health, &requests) && requests == (reverse ? 0U : 1U));
        CHECK(ams_mel_test_c2_requests(c2, &requests) && requests == (reverse ? 1U : 0U));
        verify_admission(&token, 1);
        CHECK(reverse ?
            ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED :
            ams_mel_ir_c2_submit_operate(c2, 8U, &mode, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
        CHECK(reverse ? (ret == NULL && mode != NULL) : (ret != NULL && mode == NULL));
        CHECK(count(path, reverse ? "keepalive_sent\n" : "mode_sent\n") == 0U);
        CHECK(ams_mel_test_health_requests(health, &requests) && requests == (reverse ? 0U : 1U));
        CHECK(ams_mel_test_c2_requests(c2, &requests) && requests == (reverse ? 1U : 0U));
        CHECK(release_family(reverse ? 0U : 1U, 1U));
        if (reverse) {
            ams_mel_ir_mode_result_v1 result = {0};
            CHECK(ams_mel_ir_mode_request_wait(mode, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_mode_request_close(&mode, NULL, 0, NULL) == AMS_MEL_OK);
        } else {
            ams_mel_ir_return_result_v1 result = {0};
            CHECK(ams_mel_ir_return_request_wait(ret, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        }
        /* Wait for the first accepted input to release its permit. */
        verify_final_owner(reverse ? 0U : 1U, 1U, &token);
        CHECK(reverse ?
            ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_OK :
            ams_mel_ir_c2_submit_operate(c2, 9U, &mode, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(gate(0, 2, NULL, NULL));
        CHECK(release_family(reverse ? 1U : 0U, 1U));
        if (reverse) {
            ams_mel_ir_return_result_v1 result = {0};
            CHECK(ams_mel_ir_return_request_wait(ret, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        } else {
            ams_mel_ir_mode_result_v1 result = {0};
            CHECK(ams_mel_ir_mode_request_wait(mode, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_mode_request_close(&mode, NULL, 0, NULL) == AMS_MEL_OK);
        }
        /* The refused operation preallocated an input of this family. */
        verify_final_owner(reverse ? 1U : 0U, 2U, &token);
        CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_health_close(&health, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
        CHECK(dlclose(library) == 0);
        ams_mel_test_common_close(&common);
        CHECK(count(path, "library_unloaded\n") == 1U);
        CHECK(unlink(path) == 0);
        return 0;
    }
    if (first) {
        ams_mel_test_common_close(&common);
        ams_mel_test_common_close(&common);
        CHECK(ams_mel_ir_health_enable(health, NULL, 0, NULL) == AMS_MEL_OK);
    } else if (!weak) {
        ams_mel_status_t status = comms ?
            ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) :
            ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL);
        if (throws) {
            size_t requests = 99;
            CHECK(status == AMS_MEL_PROVIDER_EXCEPTION && !ret && !reply);
            CHECK(ams_mel_test_health_requests(health, &requests) && requests == 0U);
        } else if (post) {
            CHECK(status == AMS_MEL_INTERNAL_ERROR && !ret && !reply);
        } else CHECK(status == AMS_MEL_OK);
    }
    unsigned family = comms ? 2U : 1U;
    uint64_t sent = 0, released = 0, counters[6] = {0};
    uint32_t active = 99, maximum = 0;
    if (!weak && !first && !throws) {
        size_t requests = 0;
        CHECK(gate(0, 1, &sent, &released) && sent == 1 && released == 0);
        CHECK(ams_mel_test_health_requests(health, &requests) && requests == 1U);
        CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum) && active == 1U);
        if (post) {
            CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) ==
                  AMS_MEL_RESOURCE_EXHAUSTED && !ret);
            CHECK(count(path, "keepalive_sent\n") == (comms ? 0U : 1U));
            CHECK(ams_mel_test_completion_snapshot(family, counters));
            CHECK(counters[0] == 0U && counters[3] == 0U &&
                  counters[4] == 0U && counters[5] == 0U);
            CHECK(count(path, "control_destroyed\n") == 0U &&
                  count(path, "manager_destroyed\n") == 0U);
        } else {
            CHECK(ams_mel_test_completion_wait(family, 1, 1U));
            CHECK(ams_mel_test_completion_snapshot(family, counters) && counters[2] == 1U);
        }
    }
    if (inflight) {
        CHECK(ams_mel_test_completion_wait(1, 1, 1U));
        marker(callback_start);
        reached(callback_entered);
        size_t callbacks = 0, requests = 0;
        CHECK(ams_mel_test_health_callbacks(metadata, &callbacks) && callbacks == 1U);
        CHECK(ams_mel_test_health_requests(health, &requests) && requests == 1U);
        verify_admission(&token, 1);
    }
    if (parent || metadata_stop || inflight || finish || detach || weak || first || post || throws) {
        CHECK(ams_mel_ir_health_close(&health, NULL, 0, NULL) == AMS_MEL_OK && !health);
        if (metadata_stop || inflight) {
            ams_mel_ir_health_metadata_event *event = NULL;
            CHECK(ams_mel_ir_health_metadata_receive(metadata, 0, &event, NULL, 0, NULL) ==
                  AMS_MEL_STREAM_STOPPED && !event);
        }
        if (parent || metadata_stop || inflight || finish || detach || post) {
            CHECK(count(path, "health_channel_destroyed\n") == 0U);
            CHECK(count(path, "channel_detached\n") == 0U);
            CHECK(count(path, "health_disabled\n") == 0U);
            CHECK(count(path, "control_destroyed\n") == 0U &&
                  count(path, "manager_destroyed\n") == 0U);
        }
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        if (inflight) {
            CHECK(count(path, "health_disabled\n") == 0U);
            CHECK(count(path, "control_destroyed\n") == 0U && count(path, "manager_destroyed\n") == 0U);
            verify_admission(&token, 1);
            size_t callbacks = 0;
            CHECK(ams_mel_test_health_callbacks(metadata, &callbacks) && callbacks == 1U);
        }
    }
    if (!weak && !first && !throws && !post) {
        if (parent) {
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(release_family(1, 1));
        } else {
            CHECK(release_family(family, 1));
            if (inflight) {
                logged(path, "disable_returned_with_health_callback_active\n");
            }
            if (comms) {
                ams_mel_ir_channel_comms_test_result_v1 value = {0};
                CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000, &value, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(value.command_id == command.command_id && value.request_id == command.request_id);
                CHECK(ams_mel_ir_channel_comms_request_wait(reply, 0, &value, NULL, 0, NULL) == AMS_MEL_OK);
                CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK);
            } else {
                ams_mel_ir_return_result_v1 value = {0};
                char diagnostic[128] = {0};
                ams_mel_status_t status = ams_mel_ir_return_request_wait(ret, 15000, &value,
                    diagnostic, sizeof diagnostic, NULL);
                CHECK(status == (finish || detach ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
                if (finish || detach) CHECK(!strcmp(diagnostic, "deferred Health cleanup failed"));
                else CHECK(value.value == AMS_MEL_IR_RETURN_SUCCESS);
                CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
            }
        }
        CHECK(ams_mel_test_completion_wait(family, 3, 1U));
        uint64_t observed = 0;
        CHECK(ams_mel_test_completion_owner(family, 1U, &observed));
        if (health) {
            size_t requests = 99;
            CHECK(ams_mel_test_health_requests(health, &requests) && requests == 0U);
        }
    }
    CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    CHECK(active == (post ? 1U : 0U));
    CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
    if (inflight) {
        ordered(path, "health_callback_entered\n",
            "disable_returned_with_health_callback_active\n",
            "health_callback_returned\n", "health_channel_destroyed\n");
        CHECK(count(path, "health_disabled\n") == 1U);
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, "health_callback_returned\n") == 1U);
        CHECK(count(path, "health_channel_destroyed\n") == 1U);
        size_t callbacks = 1;
        CHECK(ams_mel_test_health_callbacks(metadata, &callbacks) && callbacks == 0U);
        ams_mel_ir_health_metadata_event *event = NULL;
        CHECK(ams_mel_ir_health_metadata_receive(metadata, 0, &event, NULL, 0, NULL) ==
              AMS_MEL_STREAM_STOPPED && !event);
        ams_mel_ir_metadata_counters_v1 counters = {0};
        CHECK(ams_mel_ir_health_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(counters.events_received == 1U && counters.events_dropped_queue_full == 0U);
        CHECK(count(path, "control_destroyed\n") == 1U && count(path, "manager_destroyed\n") == 1U);
    }
    if (metadata) CHECK(ams_mel_ir_health_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    if (health) CHECK(ams_mel_ir_health_close(&health, NULL, 0, NULL) == AMS_MEL_OK);
    if (session) CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    if (weak || parent) {
        CHECK(count(path, "health_disabled\n") == ((enabled || first) ? 1U : 0U));
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, "health_channel_destroyed\n") == 1U);
        CHECK(count(path, "control_destroyed\n") == 1U);
        CHECK(count(path, "manager_destroyed\n") == 1U);
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED && !ret);
        CHECK(ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED && !reply);
    }
    CHECK(dlclose(library) == 0);
    ams_mel_test_common_close(&common);
    if (weak || parent || inflight) CHECK(count(path, "library_unloaded\n") == 1U);
    if (finish || detach || post) {
        CHECK(count(path, "health_channel_destroyed\n") == 0U);
        CHECK(count(path, "control_destroyed\n") == 0U);
        CHECK(count(path, "manager_destroyed\n") == 0U);
        CHECK(count(path, "library_unloaded\n") == 0U);
        if (detach) CHECK(count(path, "channel_detach_failed\n") == 1U);
    }
    CHECK(unlink(path) == 0);
    if (inflight) {
        char suffix[180];
        CHECK(snprintf(suffix, sizeof suffix, "%s.release", barrier_base) > 0);
        CHECK(unlink(suffix) == 0);
        CHECK(unlink(callback_start) == 0 && unlink(callback_entered) == 0);
    }
    return 0;
}
