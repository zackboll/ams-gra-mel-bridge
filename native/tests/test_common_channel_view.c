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
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static ams_mel_string_view_v1 view(const char *s)
{ ams_mel_string_view_v1 v = {s, strlen(s)}; return v; }

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

typedef struct {
    ams_mel_ir_stream **stream;
    ams_mel_status_t status;
} close_input;
static void *close_worker(void *argument)
{
    close_input *input = argument;
    input->status = ams_mel_ir_stream_close(input->stream, NULL, 0, NULL);
    return NULL;
}
static void marker(const char *path)
{
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    CHECK(fclose(file) == 0);
}
static void reached(const char *path)
{
    /* The barrier itself has a bounded watchdog. This wait is observation,
     * not a sleep used as proof of ordering. */
    for (unsigned i = 0; i < 15000U; ++i) {
        if (access(path, F_OK) == 0) return;
        struct timespec duration = {0, 1000000L};
        CHECK(nanosleep(&duration, NULL) == 0);
    }
    CHECK(0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    const int image = !strncmp(name, "image-", 6);
    const int comms = strstr(name, "comms") != NULL;
    const int weak = strstr(name, "weak") != NULL;
    const int first = strstr(name, "close-first") != NULL;
    const int parent = strstr(name, "parent") != NULL;
    const int running = strstr(name, "running") != NULL;
    const int throws = strstr(name, "throw") != NULL;
    const int failure = strstr(name, "allocation") || strstr(name, "worker-launch");
    const int finish = strstr(name, "finish") != NULL;
    const int cross = strstr(name, "cross") != NULL;
    const int race = strstr(name, "race") != NULL;
    const char *scenario = (throws ? (comms ? "comms-send-throw" : "keepalive-send-throw") :
                            comms && !parent && !failure ? "comms-high" : "completion-scale");
    char path[] = "/tmp/ams-common-view-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    char barrier[128] = {0}, close_arm[160], close_reached[160], close_release[160];
    char decrement_arm[160], decrement_reached[160], decrement_release[160];
    if (race) {
        CHECK(snprintf(barrier, sizeof barrier, "%s-barrier", path) > 0);
#define STAGE(field, name, suffix) CHECK(snprintf(field, sizeof field, "%s.%s.%s", barrier, name, suffix) > 0)
        STAGE(close_arm, "close-decision", "arm");
        STAGE(close_reached, "close-decision", "reached");
        STAGE(close_release, "close-decision", "release");
        STAGE(decrement_arm, "post-decrement", "arm");
        STAGE(decrement_reached, "post-decrement", "reached");
        STAGE(decrement_release, "post-decrement", "release");
#undef STAGE
        CHECK(setenv("AMS_MEL_TEST_IMAGE_CLEANUP_BARRIER", barrier, 1) == 0);
    }
    if (failure) CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE",
        strstr(name, "allocation") ? "allocation" : "worker-launch", 1) == 0);
    if (finish) CHECK(setenv("AMS_MEL_TEST_IMAGE_FINISH_FAILURE", "exception", 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_test_common_channel *common = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_session_options_v1 options = {1};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
        &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    gate_fn gate = NULL;
    release_fn release_family = NULL;
    *(void **)(&gate) = dlsym(library, "mock_completion_gate");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    CHECK(gate != NULL && release_family != NULL);
    if (image) {
        ams_mel_ir_stream_config_v1 config = {0};
        config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        config.channel_id.descriptive_label = view("test");
        config.platform_id.descriptive_label = view("test");
        config.sensor_location.key = view("key");
        config.sensor_location.system_name = view("test");
        config.buffer_count = 2; config.buffer_size = 64; config.queue_capacity = 2;
        CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_stream(stream, &common) == AMS_MEL_OK);
        if (running) CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_c2_config_v1 config = {0};
        config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        config.channel_id.descriptive_label = view("test");
        config.platform_id.descriptive_label = view("test");
        config.sensor_location.key = view("key");
        config.sensor_location.system_name = view("test");
        CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_c2(c2, &common) == AMS_MEL_OK);
    }
    if (cross && image) {
        ams_mel_ir_c2_config_v1 config = {0};
        config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        config.channel_id.descriptive_label = view("test");
        config.platform_id.descriptive_label = view("test");
        config.sensor_location.key = view("key");
        config.sensor_location.system_name = view("test");
        CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_test_admission(session, 0, &token, NULL, NULL));
    if (!strcmp(name, "image-cross-reverse")) {
        ams_mel_navigation_report_v1 report = {0};
        ams_mel_ir_navigation_request *navigation = NULL;
        ams_mel_ir_return_request *refused = NULL;
        size_t requests = 99;
        uint64_t sent = 0, released = 0;
        report.state = AMS_MEL_POSITION_SOLUTION_NOT_SET;
        CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report,
            &navigation, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_completion_wait(3, 4, 1U));
        CHECK(ams_mel_test_common_send_keepalive(common, &refused, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
        CHECK(!refused && ams_mel_test_navigation_requests(stream, &requests) && requests == 1U);
        CHECK(count(path, "keepalive_sent\n") == 0U);
        CHECK(gate(0, 0, &sent, &released) && sent == 1U);
        CHECK(release_family(3, 1));
        ams_mel_ir_navigation_result_v1 result = {0};
        CHECK(ams_mel_ir_navigation_request_wait(navigation, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_navigation_request_close(&navigation, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_completion_wait(3, 5, 1U));
        CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
        CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(dlclose(library) == 0);
        ams_mel_test_common_close(&common);
        CHECK(unlink(path) == 0);
        return 0;
    }
    if (first) {
        ams_mel_test_common_close(&common);
        ams_mel_test_common_close(&common);
        if (image) CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
        else CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    }
    if (weak || first) {
        if (image) CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
        else CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(dlclose(library) == 0);
        if (weak) {
            ams_mel_ir_channel_comms_test_request_v1 command = {0};
            CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
            CHECK(ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
            CHECK(!ret && !reply);
            CHECK(count(path, image ? "channel_destroyed\n" : "c2_channel_destroyed\n") == 1U);
            CHECK(count(path, "control_destroyed\n") == 1U);
            CHECK(count(path, "manager_destroyed\n") == 1U);
            CHECK(count(path, "library_unloaded\n") == 1U);
            ams_mel_test_common_close(&common);
        }
        CHECK(unlink(path) == 0);
        return 0;
    }
    ams_mel_ir_channel_comms_test_request_v1 command = {0x80000001U, 0xf0000002U, 0xe0000003U};
    ams_mel_status_t status = comms ?
        ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) :
        ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL);
    CHECK(status == (failure ? AMS_MEL_INTERNAL_ERROR : throws ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_OK));
    CHECK((reply != NULL || ret != NULL) == (!failure && !throws));
    CHECK(count(path, comms ? "comms_sent\n" : "keepalive_sent\n") == 1U);
    size_t requests = 99;
    CHECK(image ? ams_mel_test_navigation_requests(stream, &requests) : ams_mel_test_c2_requests(c2, &requests));
    CHECK(requests == (throws ? 0U : 1U));
    uint32_t active = 99, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    CHECK(active == (throws ? 0U : 1U) && maximum == 1U);
    uint64_t sent = 0, released = 0;
    CHECK(gate(0, 0, &sent, &released) && sent == (!throws && !strcmp(scenario, "completion-scale") ? 1U : 0U));
    if (cross) {
        ams_mel_ir_mode_request *mode = NULL;
        ams_mel_ir_navigation_request *navigation = NULL;
        ams_mel_navigation_report_v1 report = {0};
        report.state = AMS_MEL_POSITION_SOLUTION_NOT_SET;
        if (image) {
            CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &mode, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
            CHECK(!mode);
            CHECK(ams_mel_test_c2_requests(c2, &requests) && requests == 0U);
        } else {
            ams_mel_ir_stream_config_v1 config = {0};
            config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
            config.channel_id.descriptive_label = view("test");
            config.platform_id.descriptive_label = view("test");
            config.sensor_location.key = view("key"); config.sensor_location.system_name = view("test");
            config.buffer_count = 2; config.buffer_size = 64; config.queue_capacity = 2;
            CHECK(ams_mel_ir_stream_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report,
                &navigation, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
            CHECK(!navigation && ams_mel_test_navigation_requests(stream, &requests) && requests == 0U);
        }
        CHECK(gate(0, 0, &sent, &released) && sent == 1U);
    }
    if (failure) {
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_RESOURCE_EXHAUSTED);
        CHECK(!ret && gate(0, 0, &sent, &released) && sent == 1U);
    }
    if (parent || finish || failure) {
        if (parent) {
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_test_completion_wait(1, 1, 1U));
        }
        if (finish) CHECK(ams_mel_test_completion_wait(1, 1, 1U));
        if (image) CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
        else CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        if (!failure) CHECK(count(path, image ? "channel_destroyed\n" : "c2_channel_destroyed\n") == 0U);
        CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum) && active == 1U);
    }
    if (failure) {
        CHECK(dlclose(library) == 0);
        CHECK(count(path, image ? "channel_destroyed\n" : "c2_channel_destroyed\n") == 0U);
        ams_mel_test_common_close(&common);
        CHECK(unlink(path) == 0);
        return 0;
    }
    if (race) {
        close_input closer = {&stream, AMS_MEL_INTERNAL_ERROR};
        pthread_t thread;
        marker(close_arm);
        marker(decrement_arm);
        CHECK(pthread_create(&thread, NULL, close_worker, &closer) == 0);
        reached(close_reached);
        CHECK(count(path, "channel_detached\n") == 0U);
        CHECK(gate(2, 1, &sent, &released));
        reached(decrement_reached);
        marker(close_release);
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(closer.status == AMS_MEL_OK && !stream);
        CHECK(count(path, "channel_detached\n") == 1U);
        marker(decrement_release);
        CHECK(ams_mel_test_completion_wait(1, 3, 1U));
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_completion_wait(1, 5, 1U));
        CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(dlclose(library) == 0);
        ams_mel_test_common_close(&common);
        CHECK(count(path, "channel_destroyed\n") == 1U);
        CHECK(unlink(close_arm) == 0 && unlink(close_reached) == 0 && unlink(close_release) == 0);
        CHECK(unlink(decrement_arm) == 0 && unlink(decrement_reached) == 0 && unlink(decrement_release) == 0);
        CHECK(unlink(path) == 0);
        return 0;
    }
    if (!throws && !comms) {
        if (!parent && !finish) CHECK(gate(2, 1, &sent, &released));
        else { CHECK(dlclose(library) == 0); library = NULL; CHECK(gate(2, 1, &sent, &released)); }
        if (!parent) {
            ams_mel_ir_return_result_v1 result = {0};
            char diagnostic[128] = {0};
            CHECK(ams_mel_ir_return_request_wait(ret, 15000, &result, diagnostic, sizeof diagnostic, NULL) ==
                  (finish ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
            if (finish) CHECK(strcmp(diagnostic, "deferred Image cleanup failed") == 0);
            else CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(ams_mel_test_completion_wait(1, 3, 1U));
        if (parent) {
            CHECK(count(path, image ? "channel_destroyed\n" : "c2_channel_destroyed\n") == 1U);
            CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        }
    } else if (!throws) {
        ams_mel_ir_channel_comms_test_result_v1 result = {0};
        CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.command_id == command.command_id && result.request_id == command.request_id);
        CHECK(ams_mel_ir_channel_comms_request_wait(reply, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_completion_wait(2, 3, 1U));
    }
    if (!parent && !finish) {
        CHECK(image ? ams_mel_test_navigation_requests(stream, &requests) : ams_mel_test_c2_requests(c2, &requests));
        CHECK(requests == 0U);
    }
    CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    if (!throws) {
        CHECK(ams_mel_test_completion_wait(comms ? 2U : 1U, 5, 1U));
        /* Completed is published before WorkerInput drops its permit. Its
         * FinalOwner is destroyed last, after that permit. */
        uint64_t observed = 0;
        CHECK(ams_mel_test_completion_owner(comms ? 2U : 1U, 1U, &observed));
        CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
    }
    CHECK(active == 0U);
    CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
    if (image && stream) CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    if (c2) CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (session) CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    if (library) CHECK(dlclose(library) == 0);
    ams_mel_test_common_close(&common);
    CHECK(count(path, image ? "channel_destroyed\n" : "c2_channel_destroyed\n") == (finish ? 0U : 1U));
    if (finish) {
        CHECK(count(path, "control_destroyed\n") == 0U);
        CHECK(count(path, "manager_destroyed\n") == 0U);
        CHECK(count(path, "library_unloaded\n") == 0U);
    }
    CHECK(unlink(path) == 0);
    return 0;
}
