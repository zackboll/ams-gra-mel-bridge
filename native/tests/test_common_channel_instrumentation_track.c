#define _POSIX_C_SOURCE 200809L
#include "common_channel_view.h"
#include "admission_observation.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
typedef int (*release_fn)(unsigned, unsigned);
typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef unsigned (*live_fn)(unsigned);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
static ams_mel_string_view_v1 text(const char *s)
{ ams_mel_string_view_v1 v = {s, strlen(s)}; return v; }
static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r"); char line[160]; unsigned result = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++result;
    CHECK(fclose(file) == 0); return result;
}
static void marker(const char *path)
{
    FILE *file = fopen(path, "w");
    CHECK(file && fclose(file) == 0);
}
static void reached(const char *path)
{
    for (unsigned attempt = 0; attempt < 15000U; ++attempt) {
        if (access(path, F_OK) == 0) return;
        struct timespec interval = {0, 1000000L};
        CHECK(nanosleep(&interval, NULL) == 0);
    }
    CHECK(0);
}
static void logged(const char *path, const char *event)
{
    for (unsigned attempt = 0; attempt < 15000U; ++attempt) {
        if (count(path, event)) return;
        struct timespec interval = {0, 1000000L};
        CHECK(nanosleep(&interval, NULL) == 0);
    }
    CHECK(0);
}
static void ordered(const char *path, const char *first, const char *second)
{
    FILE *file = fopen(path, "r"); char line[160]; unsigned seen = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) {
        if (seen == 0U && !strcmp(line, first)) seen = 1U;
        else if (seen == 1U && !strcmp(line, second)) seen = 2U;
    }
    CHECK(fclose(file) == 0 && seen == 2U);
}
typedef struct {
    ams_mel_test_common_channel *common;
    ams_mel_ir_return_request *request;
    ams_mel_ir_channel_comms_request *reply;
    int comms;
    ams_mel_status_t status;
} submission;
static void *submit_keepalive(void *argument)
{
    submission *value = argument;
    if (value->comms) {
        ams_mel_ir_channel_comms_test_request_v1 command = {1U, 2U, 3U};
        value->status = ams_mel_test_common_submit_comms_test(value->common,
            &command, &value->reply, NULL, 0, NULL);
    } else value->status = ams_mel_test_common_send_keepalive(value->common,
        &value->request, NULL, 0, NULL);
    return NULL;
}
static void observe(ams_mel_test_admission_token **token, uint32_t expected)
{
    uint32_t active = 99, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 1, token, &active, &maximum));
    CHECK(active == expected);
}
static void no_cleanup(const char *path, int track)
{
    CHECK(count(path, "channel_detached\n") == 0U);
    CHECK(count(path, track ? "track_channel_destroyed\n" :
        "instrumentation_channel_destroyed\n") == 0U);
    CHECK(count(path, "control_destroyed\n") == 0U);
    CHECK(count(path, "manager_destroyed\n") == 0U);
    CHECK(count(path, track ? "track_disabled\n" : "instrumentation_disabled\n") == 0U);
}
/* A weak observation handle cannot be the reason a closed provider remains alive. */
static void observer_idle(const char *name)
{
    const int track = !strncmp(name, "track-", 6);
    char path[] = "/tmp/ams-common-observer-XXXXXX";
    int fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_track *t = NULL;
    ams_mel_ir_instrumentation *i = NULL;
    ams_mel_test_track_observer *to = NULL;
    ams_mel_test_instrumentation_observer *io = NULL;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
        &session, NULL, 0, NULL) == AMS_MEL_OK);
    if (track) {
        ams_mel_ir_track_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_track_open(session, &c, &t, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_track_observer_from(t, &to));
    } else {
        ams_mel_ir_instrumentation_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_instrumentation_open(session, &c, &i, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_instrumentation_observer_from(i, &io));
    }
    size_t requests = 99;
    CHECK(track ? ams_mel_test_track_observer_requests(to, &requests) :
        ams_mel_test_instrumentation_observer_requests(io, &requests));
    CHECK(requests == 0U);
    CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
        ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(path, "channel_detached\n") == 1U);
    CHECK(count(path, track ? "track_channel_destroyed\n" :
        "instrumentation_channel_destroyed\n") == 1U);
    CHECK(count(path, "control_destroyed\n") == 1U);
    CHECK(count(path, "manager_destroyed\n") == 1U);
    CHECK(!(track ? ams_mel_test_track_observer_requests(to, &requests) :
        ams_mel_test_instrumentation_observer_requests(io, &requests)));
    ams_mel_test_track_observer_close(&to);
    ams_mel_test_instrumentation_observer_close(&io);
    CHECK(unlink(path) == 0);
}
/* Four isolated directions through the same limit-one Session permit. */
static void cross_admission(const char *name)
{
    const int track_first = strstr(name, "track-first") != NULL;
    const int typed_first = strstr(name, "typed-first") != NULL;
    char path[] = "/tmp/ams-common-cross-XXXXXX";
    int fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    ams_mel_session_options_v1 options = {1U};
    ams_mel_session *session = NULL;
    ams_mel_ir_track *t = NULL;
    ams_mel_ir_instrumentation *i = NULL;
    ams_mel_test_common_channel *tc = NULL, *ic = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_instrumentation_request *level_request = NULL;
    ams_mel_ir_track_update_request *update_request = NULL;
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER,
        "completion-scale", "", &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0U, &token, NULL, NULL));
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    release_fn release = NULL;
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    CHECK(release);
    ams_mel_ir_track_config_v1 tc_config = {0};
    tc_config.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    tc_config.channel_id.descriptive_label = text("test");
    tc_config.platform_id.descriptive_label = text("test");
    tc_config.sensor_location.key = text("key");
    tc_config.sensor_location.system_name = text("test");
    CHECK(ams_mel_ir_track_open(session, &tc_config, &t, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_enable(t, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_common_from_track(t, &tc) == AMS_MEL_OK);
    ams_mel_ir_instrumentation_config_v1 ic_config = {0};
    ic_config.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    ic_config.channel_id.descriptive_label = text("test");
    ic_config.platform_id.descriptive_label = text("test");
    ic_config.sensor_location.key = text("key");
    ic_config.sensor_location.system_name = text("test");
    CHECK(ams_mel_ir_instrumentation_open(session, &ic_config, &i, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_enable(i, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_common_from_instrumentation(i, &ic) == AMS_MEL_OK);
    ams_mel_ir_track_data_update_v1 update = {0};
    update.capability_uuid.descriptive_label = text("cap");
    update.activity_uuid.descriptive_label = text("act");
    update.entity_uuid.descriptive_label = text("entity");
    ams_mel_ir_instrumentation_level_command_v1 level = {7U, AMS_MEL_IR_PRIORITY_NORMAL};
    if (typed_first) {
        CHECK((track_first ? ams_mel_ir_track_submit_update(t, &update, &update_request,
            NULL, 0, NULL) : ams_mel_ir_instrumentation_submit_level(i, &level,
            &level_request, NULL, 0, NULL)) == AMS_MEL_OK);
    } else CHECK(ams_mel_test_common_send_keepalive(track_first ? tc : ic,
        &ret, NULL, 0, NULL) == AMS_MEL_OK);
    observe(&token, 1U);
    size_t requests = 99;
    CHECK(ams_mel_test_track_requests(t, &requests) &&
        requests == (track_first ? 1U : 0U));
    CHECK(ams_mel_test_instrumentation_requests(i, &requests) &&
        requests == (track_first ? 0U : 1U));
    CHECK((typed_first ? ams_mel_test_common_send_keepalive(track_first ? ic : tc,
        &ret, NULL, 0, NULL) : track_first ?
        ams_mel_ir_instrumentation_submit_level(i, &level, &level_request, NULL, 0, NULL) :
        ams_mel_ir_track_submit_update(t, &update, &update_request, NULL, 0, NULL)) ==
        AMS_MEL_RESOURCE_EXHAUSTED);
    CHECK(typed_first ? !ret : track_first ? !level_request : !update_request);
    CHECK(ams_mel_test_track_requests(t, &requests) &&
        requests == (track_first ? 1U : 0U));
    CHECK(ams_mel_test_instrumentation_requests(i, &requests) &&
        requests == (track_first ? 0U : 1U));
    CHECK(count(path, track_first ? "instrumentation_level_sent\n" :
        "track_data_update_sent\n") == 0U);
    CHECK(count(path, "keepalive_sent\n") == (typed_first ? 0U : 1U));
    unsigned first_family = typed_first ? (track_first ? 5U : 4U) : 1U;
    CHECK(release(first_family, 1U));
    uint64_t owners = 0;
    CHECK(ams_mel_test_completion_owner(first_family, 1U, &owners));
    observe(&token, 0U);
    CHECK((typed_first ? ams_mel_test_common_send_keepalive(track_first ? ic : tc,
        &ret, NULL, 0, NULL) : track_first ?
        ams_mel_ir_instrumentation_submit_level(i, &level, &level_request, NULL, 0, NULL) :
        ams_mel_ir_track_submit_update(t, &update, &update_request, NULL, 0, NULL)) == AMS_MEL_OK);
    observe(&token, 1U);
    CHECK(count(path, track_first ? "instrumentation_level_sent\n" :
        "track_data_update_sent\n") == (typed_first ? 0U : 1U));
    CHECK(count(path, "keepalive_sent\n") == 1U);
    CHECK(release(typed_first ? 1U : track_first ? 4U : 5U, 1U));
    CHECK(ams_mel_test_completion_owner(typed_first ? 1U : track_first ? 4U : 5U,
        2U, &owners));
    observe(&token, 0U);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_update_request_close(&update_request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_close(&level_request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&t, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(NULL, 2U, &token, NULL, NULL));
    CHECK(dlclose(pin) == 0);
    ams_mel_test_common_close(&tc);
    ams_mel_test_common_close(&ic);
    CHECK(unlink(path) == 0);
}
/* Isolated process per scenario: provider gate totals and failpoints cannot leak. */
static void combined(const char *name)
{
    const int track = !strncmp(name, "track-", 6);
    const int closed = strstr(name, "closed") != NULL;
    const int race = strstr(name, "close-wins") != NULL;
    const int comms = strstr(name, "comms") != NULL;
    const int post = strstr(name, "post-") != NULL;
    const int metadata_case = strstr(name, "metadata-held") != NULL;
    const int allocation = strstr(name, "allocation") != NULL;
    char path[] = "/tmp/ams-common-combined-XXXXXX";
    int fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    char base[180] = {0}, start[196] = {0}, entered[196] = {0}, resume[196] = {0};
    if (metadata_case) {
        CHECK(snprintf(base, sizeof base, "%s-callback", path) > 0);
        CHECK(snprintf(start, sizeof start, "%s.start", base) > 0);
        CHECK(snprintf(entered, sizeof entered, "%s.entered", base) > 0);
        CHECK(snprintf(resume, sizeof resume, "%s.release", base) > 0);
        CHECK(setenv(track ? "AMS_MEL_TEST_TRACK_CALLBACK_BARRIER" :
            "AMS_MEL_TEST_INSTRUMENTATION_CALLBACK_BARRIER", base, 1) == 0);
    }
    if (post) CHECK(setenv("AMS_MEL_TEST_COMMON_POST_SEND_FAILURE",
        allocation ? "allocation" : "worker-launch", 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_track *t = NULL;
    ams_mel_ir_instrumentation *i = NULL;
    ams_mel_test_common_channel *common = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_ir_instrumentation_request *level_request = NULL;
    ams_mel_ir_track_update_request *update_request = NULL;
    ams_mel_ir_track_system_response_request *response_request = NULL;
    ams_mel_test_instrumentation_observer *i_observer = NULL;
    ams_mel_test_track_observer *t_observer = NULL;
    ams_mel_ir_track_metadata *t_metadata = NULL;
    ams_mel_ir_instrumentation_metadata *i_metadata = NULL;
    ams_mel_session_options_v1 options = {track && !race && !post ? 3U :
        !race && !post && !metadata_case ? 2U : 1U};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER,
        metadata_case ? (track ? "track-common-metadata" : "instr-common-metadata") :
        "completion-scale", "", &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0, &token, NULL, NULL));
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    gate_fn gate = NULL; release_fn release = NULL; barrier_fn barrier = NULL;
    live_fn live = NULL;
    *(void **)(&gate) = dlsym(pin, "mock_completion_gate");
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&barrier) = dlsym(pin, "mock_c2_send_barrier");
    *(void **)(&live) = dlsym(pin, "mock_completion_live_results");
    CHECK(gate && release && barrier && live);
    if (track) {
        ams_mel_ir_track_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_track_open(session, &c, &t, NULL, 0, NULL) == AMS_MEL_OK);
        if (metadata_case) CHECK(ams_mel_ir_track_metadata_open(t, 4U, &t_metadata,
            NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_track_enable(t, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_track(t, &common) == AMS_MEL_OK);
        CHECK(ams_mel_test_track_observer_from(t, &t_observer));
    } else {
        ams_mel_ir_instrumentation_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_instrumentation_open(session, &c, &i, NULL, 0, NULL) == AMS_MEL_OK);
        if (metadata_case) CHECK(ams_mel_ir_instrumentation_metadata_open(i, 4U,
            &i_metadata, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_instrumentation_enable(i, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_instrumentation(i, &common) == AMS_MEL_OK);
        CHECK(ams_mel_test_instrumentation_observer_from(i, &i_observer));
    }
    if (metadata_case) {
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_completion_wait(1U, 4U, 1U));
        marker(start); reached(entered);
        size_t callbacks = 99, requests = 99;
        CHECK(track ? ams_mel_test_track_callbacks(t_metadata, &callbacks) :
            ams_mel_test_instrumentation_callbacks(i_metadata, &callbacks));
        CHECK(callbacks == 1U);
        CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
            ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
            ams_mel_test_instrumentation_observer_requests(i_observer, &requests));
        CHECK(requests == 1U); observe(&token, 1U); no_cleanup(path, track);
        ams_mel_ir_track_metadata_event *t_event = NULL;
        ams_mel_ir_instrumentation_metadata_event *i_event = NULL;
        CHECK((track ? ams_mel_ir_track_metadata_receive(t_metadata, 0U, &t_event,
            NULL, 0, NULL) : ams_mel_ir_instrumentation_metadata_receive(i_metadata,
            0U, &i_event, NULL, 0, NULL)) == AMS_MEL_STREAM_STOPPED);
        CHECK(!t_event && !i_event);
        marker(resume);
        logged(path, track ? "track_callback_returned\n" : "instrumentation_callback_returned\n");
        CHECK(track ? ams_mel_test_track_callbacks(t_metadata, &callbacks) :
            ams_mel_test_instrumentation_callbacks(i_metadata, &callbacks));
        CHECK(callbacks == 0U);
        ams_mel_ir_metadata_counters_v1 counters = {0};
        CHECK((track ? ams_mel_ir_track_metadata_get_counters(t_metadata, &counters,
            NULL, 0, NULL) : ams_mel_ir_instrumentation_metadata_get_counters(
            i_metadata, &counters, NULL, 0, NULL)) == AMS_MEL_OK);
        CHECK(counters.events_received == 1U);
        CHECK((track ? ams_mel_ir_track_metadata_receive(t_metadata, 0U, &t_event,
            NULL, 0, NULL) : ams_mel_ir_instrumentation_metadata_receive(i_metadata,
            0U, &i_event, NULL, 0, NULL)) == AMS_MEL_STREAM_STOPPED);
        CHECK(!t_event && !i_event);
        no_cleanup(path, track);
        CHECK(release(1U, 1U));
        uint64_t owners = 0;
        CHECK(ams_mel_test_completion_owner(1U, 1U, &owners));
        observe(&token, 0U);
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, track ? "track_disabled\n" : "instrumentation_disabled\n") == 1U);
        CHECK(count(path, track ? "track_channel_destroyed\n" :
            "instrumentation_channel_destroyed\n") == 1U);
        CHECK(count(path, "control_destroyed\n") == 1U);
        CHECK(count(path, "manager_destroyed\n") == 1U);
        ordered(path, track ? "track_callback_returned\n" :
            "instrumentation_callback_returned\n", track ? "track_channel_destroyed\n" :
            "instrumentation_channel_destroyed\n");
        CHECK(!(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
            ams_mel_test_instrumentation_observer_requests(i_observer, &requests)));
        CHECK((track ? ams_mel_ir_track_metadata_close(&t_metadata, NULL, 0, NULL) :
            ams_mel_ir_instrumentation_metadata_close(&i_metadata, NULL, 0, NULL)) == AMS_MEL_OK);
        CHECK(unlink(start) == 0 && unlink(entered) == 0 && unlink(resume) == 0);
    } else if (race) {
        submission work = {common, NULL, NULL, comms, AMS_MEL_OK}; pthread_t thread;
        const unsigned family = comms ? 2U : 1U;
        uint64_t before[6] = {0};
        CHECK(ams_mel_test_completion_snapshot(family, before));
        CHECK(barrier(family, 0U, 1U));
        CHECK(pthread_create(&thread, NULL, submit_keepalive, &work) == 0);
        CHECK(barrier(family, 1U, 0U));
        size_t requests = 99;
        CHECK(track ? ams_mel_test_track_requests(t, &requests) :
            ams_mel_test_instrumentation_requests(i, &requests));
        CHECK(requests == 1U); observe(&token, 1U);
        CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
            ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        no_cleanup(path, track);
        CHECK(barrier(family, 2U, 0U));
        CHECK(pthread_join(thread, NULL) == 0);
        CHECK(work.status == AMS_MEL_PROVIDER_EXCEPTION && !work.request && !work.reply);
        uint64_t after[6] = {0};
        CHECK(ams_mel_test_completion_snapshot(family, after));
        CHECK(after[0] == before[0] && after[3] == before[3] &&
            after[4] == before[4] && after[5] == before[5]);
        observe(&token, 0U);
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, track ? "track_disabled\n" : "instrumentation_disabled\n") == 1U);
        CHECK(count(path, track ? "track_channel_destroyed\n" :
            "instrumentation_channel_destroyed\n") == 1U);
        CHECK(count(path, "control_destroyed\n") == 1U);
        CHECK(count(path, "manager_destroyed\n") == 1U);
    } else if (post) {
        ams_mel_ir_channel_comms_test_request_v1 command = {1U, 2U, 3U};
        uint64_t before[6] = {0}, after[6] = {0}, sent = 0, released = 0;
        CHECK(ams_mel_test_completion_snapshot(comms ? 2U : 1U, before));
        CHECK((comms ? ams_mel_test_common_submit_comms_test(common, &command,
            &reply, NULL, 0, NULL) : ams_mel_test_common_send_keepalive(common,
            &ret, NULL, 0, NULL)) == AMS_MEL_INTERNAL_ERROR);
        CHECK(!ret && !reply);
        CHECK(gate(0U, 0U, &sent, &released) && sent == 1U && released == 0U);
        CHECK(live(comms ? 2U : 1U) == 1U);
        CHECK(ams_mel_test_completion_snapshot(comms ? 2U : 1U, after));
        CHECK(after[0] == before[0] && after[3] == before[3] &&
            after[4] == before[4] && after[5] == before[5]);
        size_t requests = 99;
        CHECK(track ? ams_mel_test_track_requests(t, &requests) :
            ams_mel_test_instrumentation_requests(i, &requests));
        CHECK(requests == 1U); observe(&token, 1U);
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) ==
            AMS_MEL_RESOURCE_EXHAUSTED && !ret);
        CHECK(count(path, "keepalive_sent\n") == (comms ? 0U : 1U));
        CHECK(count(path, "comms_sent\n") == (comms ? 1U : 0U));
        CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
            ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        no_cleanup(path, track);
        CHECK(live(comms ? 2U : 1U) == 1U);
    } else {
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_OK);
        if (track) {
            ams_mel_ir_track_data_update_v1 update = {0};
            ams_mel_ir_system_track_data_response_v1 response = {0};
            update.capability_uuid.descriptive_label = text("cap");
            update.activity_uuid.descriptive_label = text("act");
            update.entity_uuid.descriptive_label = text("entity");
            CHECK(ams_mel_ir_track_submit_update(t, &update, &update_request,
                NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_track_submit_system_track_data_response(t, &response,
                &response_request, NULL, 0, NULL) == AMS_MEL_OK);
        } else {
            ams_mel_ir_instrumentation_level_command_v1 level = {7U, AMS_MEL_IR_PRIORITY_NORMAL};
            CHECK(ams_mel_ir_instrumentation_submit_level(i, &level, &level_request,
                NULL, 0, NULL) == AMS_MEL_OK);
        }
        size_t requests = 99;
        CHECK(track ? ams_mel_test_track_requests(t, &requests) :
            ams_mel_test_instrumentation_requests(i, &requests));
        CHECK(requests == options.max_async_requests);
        observe(&token, options.max_async_requests);
        CHECK(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
            ams_mel_test_instrumentation_observer_requests(i_observer, &requests));
        CHECK(requests == options.max_async_requests);
        CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        if (track) {
            CHECK(ams_mel_ir_track_update_request_close(&update_request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_track_system_response_request_close(&response_request, NULL, 0, NULL) == AMS_MEL_OK);
        } else CHECK(ams_mel_ir_instrumentation_request_close(&level_request, NULL, 0, NULL) == AMS_MEL_OK);
        if (closed) {
            CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
                ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(!t && !i && !session);
            CHECK(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
                ams_mel_test_instrumentation_observer_requests(i_observer, &requests));
            CHECK(requests == options.max_async_requests);
            no_cleanup(path, track);
        }
        const unsigned families[] = {1U, track ? 5U : 4U, 5U};
        for (uint32_t step = 1U; step <= options.max_async_requests; ++step) {
            CHECK(release(families[step - 1U], 1U));
            CHECK(ams_mel_test_completion_wait(families[step - 1U], 3U,
                families[step - 1U] == 5U && step == 3U ? 2U : 1U));
            /* Completed/get observations precede permit release; FinalOwner follows it. */
            uint64_t owners = 0;
            CHECK(ams_mel_test_completion_owner(families[step - 1U],
                families[step - 1U] == 5U && step == 3U ? 2U : 1U, &owners));
            if (closed && step == options.max_async_requests) {
                CHECK(!(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
                    ams_mel_test_instrumentation_observer_requests(i_observer, &requests)));
            } else {
                CHECK(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
                    ams_mel_test_instrumentation_observer_requests(i_observer, &requests));
                CHECK(requests == options.max_async_requests - step);
            }
            observe(&token, options.max_async_requests - step);
            if (!closed || step < options.max_async_requests) no_cleanup(path, track);
        }
        if (!closed) {
            CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
                ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
            CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(count(path, "channel_detached\n") == 1U);
        CHECK(count(path, track ? "track_disabled\n" : "instrumentation_disabled\n") == 1U);
        CHECK(count(path, track ? "track_channel_destroyed\n" :
            "instrumentation_channel_destroyed\n") == 1U);
        CHECK(count(path, "control_destroyed\n") == 1U);
        CHECK(count(path, "manager_destroyed\n") == 1U);
    }
    if (!post) {
        size_t requests = 99;
        CHECK(!(track ? ams_mel_test_track_observer_requests(t_observer, &requests) :
            ams_mel_test_instrumentation_observer_requests(i_observer, &requests)));
    }
    ams_mel_test_instrumentation_observer_close(&i_observer);
    ams_mel_test_track_observer_close(&t_observer);
    CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL));
    CHECK(dlclose(pin) == 0);
    ams_mel_test_common_close(&common);
    CHECK(unlink(path) == 0);
}
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (strstr(argv[1], "cross-")) { cross_admission(argv[1]); return 0; }
    if (strstr(argv[1], "observer-idle")) { observer_idle(argv[1]); return 0; }
    if (strstr(argv[1], "shared") || strstr(argv[1], "metadata-held") ||
        strstr(argv[1], "close-wins") ||
        strstr(argv[1], "post-")) { combined(argv[1]); return 0; }
    const int track = !strncmp(argv[1], "track-", 6);
    const int weak = strstr(argv[1], "weak") != NULL;
    const int first = strstr(argv[1], "close-first") != NULL;
    const int parent = strstr(argv[1], "parent") != NULL;
    const int enabled = strstr(argv[1], "enabled") != NULL;
    const int comms = strstr(argv[1], "comms") != NULL;
    const int finish = strstr(argv[1], "finish") != NULL;
    const int detach = strstr(argv[1], "detach") != NULL;
    const int throwing = strstr(argv[1], "throw") != NULL;
    const char *scenario = detach ? (track ? "track-detach-fail" : "instr-detach-fail") :
        throwing ? (comms ? "comms-send-throw" : "keepalive-send-throw") :
        comms ? "comms-high-held" : "completion-scale";
    char path[] = "/tmp/ams-common-families-XXXXXX";
    int fd = mkstemp(path); CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    if (finish) CHECK(setenv(track ? "AMS_MEL_TEST_TRACK_COMMON_FINISH_FAILURE" :
        "AMS_MEL_TEST_INSTRUMENTATION_COMMON_FINISH_FAILURE", "exception", 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_track *t = NULL;
    ams_mel_ir_instrumentation *i = NULL;
    ams_mel_test_common_channel *common = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_session_options_v1 options = {1};
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
        &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0, &token, NULL, NULL));
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    gate_fn gate = NULL; release_fn release = NULL;
    *(void **)(&gate) = dlsym(pin, "mock_completion_gate");
    *(void **)(&release) = dlsym(pin, "mock_completion_release_family");
    CHECK(gate && release);
    if (track) {
        ams_mel_ir_track_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_track_open(session, &c, &t, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_track(t, &common) == AMS_MEL_OK);
        if (enabled) CHECK(ams_mel_ir_track_enable(t, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_instrumentation_config_v1 c = {0};
        c.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
        c.channel_id.descriptive_label = text("test"); c.platform_id.descriptive_label = text("test");
        c.sensor_location.key = text("key"); c.sensor_location.system_name = text("test");
        CHECK(ams_mel_ir_instrumentation_open(session, &c, &i, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_common_from_instrumentation(i, &common) == AMS_MEL_OK);
        if (enabled) CHECK(ams_mel_ir_instrumentation_enable(i, NULL, 0, NULL) == AMS_MEL_OK);
    }
    if (first) {
        ams_mel_test_common_close(&common);
        CHECK((track ? ams_mel_ir_track_enable(t, NULL, 0, NULL) :
            ams_mel_ir_instrumentation_enable(i, NULL, 0, NULL)) == AMS_MEL_OK);
    }
    if (!weak && !first) {
        ams_mel_ir_channel_comms_test_request_v1 command = {0x80000001U, 0xf0000002U, 0xe0000003U};
        ams_mel_status_t status = comms ?
            ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) :
            ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL);
        CHECK(status == (throwing ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_OK));
        CHECK((ret != NULL || reply != NULL) == !throwing);
        size_t requests = 99;
        CHECK(track ? ams_mel_test_track_requests(t, &requests) :
            ams_mel_test_instrumentation_requests(i, &requests));
        CHECK(requests == (throwing ? 0U : 1U));
        uint32_t active = 99;
        uint32_t maximum = 0;
        CHECK(ams_mel_test_admission(NULL, 1, &token, &active, &maximum));
        CHECK(active == (throwing ? 0U : 1U));
        CHECK(count(path, comms ? "comms_sent\n" : "keepalive_sent\n") == 1U);
        if (!throwing) {
            CHECK(ams_mel_test_completion_wait(comms ? 2U : 1U, 4, 1U));
            if (parent) CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        }
    }
    CHECK((track ? ams_mel_ir_track_close(&t, NULL, 0, NULL) :
        ams_mel_ir_instrumentation_close(&i, NULL, 0, NULL)) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    if (!weak && !first && !throwing) {
        CHECK(count(path, track ? "track_channel_destroyed\n" : "instrumentation_channel_destroyed\n") == 0U);
        uint64_t sent = 0, released = 0;
        CHECK(gate(0, 0, &sent, &released) && sent == 1U);
        CHECK(release(comms ? 2U : 1U, 1U));
        if (comms) {
            ams_mel_ir_channel_comms_test_result_v1 result = {0};
            CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000, &result, NULL, 0, NULL) ==
                  (finish || detach ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
            if (!finish && !detach) {
                CHECK(result.command_id == 0x80000001U && result.request_id == 0xe0000003U);
                CHECK(ams_mel_ir_channel_comms_request_wait(reply, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
            }
            CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK);
        } else if (!parent) {
            ams_mel_ir_return_result_v1 result = {0}; char diagnostic[128] = {0};
            CHECK(ams_mel_ir_return_request_wait(ret, 15000, &result, diagnostic, sizeof diagnostic, NULL) ==
                  (finish || detach ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
            if (finish) CHECK(!strcmp(diagnostic, track ? "deferred Track cleanup failed" :
                "deferred Instrumentation cleanup failed"));
            if (!finish && !detach) CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
            CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(ams_mel_test_completion_wait(comms ? 2U : 1U, 5, 1U));
        uint64_t observed = 0;
        CHECK(ams_mel_test_completion_owner(comms ? 2U : 1U, 1U, &observed));
        uint32_t active = 99, maximum = 0;
        CHECK(ams_mel_test_admission(NULL, 1U, &token, &active, &maximum) && active == 0U);
        CHECK(ams_mel_test_admission(NULL, 2U, &token, NULL, NULL));
        if (finish || detach) CHECK(count(path, "control_destroyed\n") == 0U);
        else {
            CHECK(count(path, track ? "track_channel_destroyed\n" : "instrumentation_channel_destroyed\n") == 1U);
            CHECK(count(path, "control_destroyed\n") == 1U);
        }
    }
    if (weak) {
        CHECK(count(path, track ? "track_channel_destroyed\n" : "instrumentation_channel_destroyed\n") == 1U);
        CHECK(ams_mel_test_common_send_keepalive(common, &ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        ams_mel_ir_channel_comms_test_request_v1 command = {0};
        CHECK(ams_mel_test_common_submit_comms_test(common, &command, &reply, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(!ret && !reply);
    }
    CHECK(dlclose(pin) == 0);
    if (weak) CHECK(count(path, "library_unloaded\n") == 1U);
    ams_mel_test_common_close(&common);
    CHECK(unlink(path) == 0);
    return 0;
}
