#define _GNU_SOURCE
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); _Exit(1); } } while (0)
#define CALL(name) ams_mel_ir_scheduling_schedule_created_##name
#define DIAG NULL, 0, NULL

typedef ams_mel_ir_scheduling_schedule_created_metadata stream_t;
typedef ams_mel_ir_scheduling_schedule_created_event event_t;
typedef ams_mel_ir_sched_created_rep_v1 view_t;
extern int ams_mel_test_schedule_created_probe(stream_t *, unsigned);
static ams_mel_session *session;
static ams_mel_ir_scheduling *channel;
static stream_t *stream;
static void *pin;
static int (*deliver)(unsigned);
static int (*matches)(unsigned, const view_t *);
static char log_path[] = "/tmp/ams-created-XXXXXX";
static unsigned reentered;

static void capability_reentry(void)
{
    ams_mel_ir_channel_capability *capabilities = NULL;
    CHECK(ams_mel_ir_scheduling_get_capabilities(channel, &capabilities, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_close(&capabilities, DIAG) == AMS_MEL_OK);
    ++reentered;
}

static unsigned count(const char *event)
{
    FILE *file = fopen(log_path, "r");
    CHECK(file);
    unsigned result = 0;
    char line[160];
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, event) == 0) ++result;
    }
    CHECK(fclose(file) == 0);
    return result;
}

static void setup(const char *scenario)
{
    const int fd = mkstemp(log_path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1) == 0);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session, DIAG) == AMS_MEL_OK);
    ams_mel_ir_scheduling_config_v1 cfg = {0};
    cfg.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    CHECK(ams_mel_ir_scheduling_open(session, &cfg, &channel, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_sched_created_registered") == 0);
    CHECK(count("scheduling_execute_task_ack_registration_attempted") == 0);
    CHECK(count("scheduling_task_executing_registration_attempted") == 0);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    void *address = dlsym(pin, "mock_scheduling_created_deliver");
    memcpy(&deliver, &address, sizeof deliver);
    address = dlsym(pin, "mock_scheduling_created_matches");
    memcpy(&matches, &address, sizeof matches);
    CHECK(deliver && matches);
}

static void counters(uint64_t received, uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 c;
    CHECK(CALL(get_counters)(stream, &c, DIAG) == AMS_MEL_OK);
    CHECK(c.events_received == received && c.events_dropped_queue_full == dropped &&
          c.malformed_or_unsupported == malformed);
}

static const view_t *view(event_t *event)
{
    const view_t *value = NULL;
    CHECK(CALL(event_view)(event, &value, DIAG) == AMS_MEL_OK);
    return value;
}

static event_t *next(unsigned fixture)
{
    event_t *event = NULL;
    CHECK(CALL(receive)(stream, 0, &event, DIAG) == AMS_MEL_OK);
    CHECK(matches(fixture, view(event)));
    return event;
}

static void expect_empty(uint32_t timeout, ams_mel_status_t status)
{
    event_t *event = NULL;
    CHECK(CALL(receive)(stream, timeout, &event, DIAG) == status && !event);
}

static void close_event(event_t **event)
{
    CHECK(CALL(event_close)(event, DIAG) == AMS_MEL_OK && !*event);
    CHECK(CALL(event_close)(event, DIAG) == AMS_MEL_OK);
}

static void payload(void)
{
    void (*set_reentry)(void (*)(void));
    void *address = dlsym(pin, "mock_scheduling_created_reentry");
    memcpy(&set_reentry, &address, sizeof set_reentry);
    CHECK(set_reentry);
    set_reentry(capability_reentry);
    CHECK(CALL(open)(channel, 0, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(channel, 3, &stream, NULL, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(channel, 3, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(reentered == 1);
    set_reentry(NULL);
    CHECK(count("scheduling_sched_created_registered") == 1);
    counters(3, 0, 0);
    CHECK(ams_mel_test_schedule_created_probe(stream, 0));
    CHECK(deliver(2) && deliver(3) && deliver(4) && deliver(5));
    counters(7, 1, 3);
    event_t *e0 = next(0);
    // Full/invalid callbacks did not use the allocation failpoint.
    CHECK(deliver(2));
    counters(8, 1, 4);
    CHECK(deliver(2));
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    counters(10, 1, 4); // includes the late destructor callback
    event_t *e1 = next(1), *e2 = next(2), *e3 = next(2);
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == 1);
    // Exercise the same immutable event-owned view after unloading the DSO.
    CHECK(view(e0)->activities.data == NULL && view(e0)->activity_count == UINT32_MAX);
    CHECK(view(e1)->activities.size == 1 && view(e1)->activity_count == 7);
    CHECK(view(e2)->activities.size == 300 && view(e2)->activity_count == 0);
    CHECK(view(e3)->activities.data[299].task_id == 10U);
    close_event(&e0); close_event(&e1); close_event(&e2); close_event(&e3);
}

static void saturation(void)
{
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    expect_empty(0, AMS_MEL_TIMEOUT);
    expect_empty(2, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_test_schedule_created_probe(stream, 1));
    CHECK(deliver(1) && deliver(0) && deliver(2) && deliver(3) && deliver(5));
    counters(UINT64_MAX, UINT64_MAX, UINT64_MAX);
    event_t *event = next(1);
    close_event(&event);
}

static void *receiver(void *arg)
{
    (void)arg;
    expect_empty(30000, AMS_MEL_STREAM_STOPPED);
    return NULL;
}

static void *producer(void *arg)
{
    (void)arg;
    CHECK(deliver(2));
    return NULL;
}

static void *closer(void *arg)
{
    (void)arg;
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    return NULL;
}

static void concurrent(int copy)
{
    pthread_t rx, tx, close_thread;
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    if (copy) {
        CHECK(ams_mel_test_schedule_created_probe(stream, 2));
        CHECK(pthread_create(&tx, NULL, producer, NULL) == 0);
        CHECK(ams_mel_test_schedule_created_probe(stream, 3));
        // An in-progress reservation fills a capacity-one queue.
        CHECK(deliver(1) && deliver(3));
        counters(3, 1, 1);
    }
    CHECK(pthread_create(&rx, NULL, receiver, NULL) == 0);
    CHECK(ams_mel_test_schedule_created_probe(stream, 5));
    CHECK(pthread_create(&close_thread, NULL, closer, NULL) == 0);
    if (copy) {
        CHECK(ams_mel_test_schedule_created_probe(stream, 6));
        CHECK(pthread_join(rx, NULL) == 0);
        CHECK(count("scheduling_channel_destroyed") == 0 && count("library_unloaded") == 0);
        CHECK(ams_mel_test_schedule_created_probe(stream, 4));
        CHECK(pthread_join(tx, NULL) == 0);
    } else CHECK(pthread_join(rx, NULL) == 0);
    CHECK(pthread_join(close_thread, NULL) == 0);
    CHECK(count("late_created_callback_invoked") == 1);
    counters(copy ? 4U : 1U, copy ? 1U : 0U, copy ? 1U : 0U);
}

static void all_families(void)
{
    ams_mel_ir_scheduling_execute_task_ack_metadata *ack = NULL;
    ams_mel_ir_scheduling_task_executing_metadata *rep = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(channel, 4, &ack, NULL, DIAG) == AMS_MEL_OK);
    CHECK(CALL(open)(channel, 3, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_open(channel, 4, &rep, NULL, DIAG) == AMS_MEL_OK);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(deliver(2)); // retained registration, inactive consumer
    CHECK(CALL(open)(channel, 3, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    ams_mel_ir_scheduling_execute_task_ack_event *ae = NULL;
    ams_mel_ir_scheduling_task_executing_event *re = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &ae, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_receive(rep, 0, &re, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&ae, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_event_close(&re, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&ack, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_close(&rep, DIAG) == AMS_MEL_OK);
}

static void provider_owned_producer(void)
{
    pthread_t close_thread;
    CHECK(ams_mel_test_schedule_created_probe(NULL, 2));
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_test_schedule_created_probe(stream, 3));
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(pthread_create(&close_thread, NULL, closer, NULL) == 0);
    CHECK(ams_mel_test_schedule_created_probe(stream, 6));
    CHECK(count("created_producer_returned") == 0);
    CHECK(count("scheduling_channel_destroyed") == 0 && count("control_destroyed") == 0);
    CHECK(count("library_unloaded") == 0);
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_test_schedule_created_probe(stream, 4));
    CHECK(pthread_join(close_thread, NULL) == 0);
    CHECK(count("created_producer_returned") == 1 && count("library_unloaded") == 1);
    counters(2, 0, 0);
}

static void detach_failure(void)
{
    CHECK(CALL(open)(channel, 2, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_PROVIDER_FAILED && channel);
    CHECK(count("scheduling_channel_destroyed") == 0);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == 0);
    CHECK(deliver(2)); // graph retained despite Session and external DSO pin closing
    for (unsigned i = 0; i < 2; ++i) { event_t *event = next(i); close_event(&event); }
    expect_empty(0, AMS_MEL_PROVIDER_FAILED);
}

static void refusal(const char *scenario)
{
    ams_mel_ir_return_t expected = UINT32_MAX, actual = UINT32_MAX;
    if (strstr(scenario, "not-supported")) expected = AMS_MEL_IR_RETURN_NOT_SUPPORTED;
    if (strstr(scenario, "not-implemented")) expected = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED;
    if (strstr(scenario, "bad-pointer")) expected = AMS_MEL_IR_RETURN_BAD_POINTER;
    if (strcmp(scenario, "sched-created-fail") == 0) expected = AMS_MEL_IR_RETURN_FAIL;
    CHECK(CALL(open)(channel, 1, &stream, &actual, DIAG) ==
          (strstr(scenario, "throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
    CHECK(!stream && actual == expected);
    CHECK(CALL(open)(channel, 1, &stream, &actual, DIAG) == AMS_MEL_INVALID_ARGUMENT && !stream);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *scenario = argv[1];
    setup(scenario);
    if (strcmp(scenario, "sched-created-sync") == 0) payload();
    else if (strcmp(scenario, "sched-created-saturate") == 0) saturation();
    else if (strcmp(scenario, "sched-created-wake") == 0) concurrent(0);
    else if (strcmp(scenario, "sched-created-copy") == 0) concurrent(1);
    else if (strcmp(scenario, "sched-created-unpin") == 0) provider_owned_producer();
    else if (strcmp(scenario, "sched-both-sync") == 0) all_families();
    else if (strcmp(scenario, "sched-created-detach-fail") == 0) detach_failure();
    else refusal(scenario);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_sched_created_registered") == 1);
    const unsigned other = strcmp(scenario, "sched-both-sync") == 0 ? 1U : 0U;
    CHECK(count("scheduling_execute_task_ack_registration_attempted") == other);
    CHECK(count("scheduling_task_executing_registration_attempted") == other);
    CHECK(count("scheduling_prioritized_track_list_sent") == 0);
    CHECK(count("scheduling_update_track_list_registered") == 0);
    CHECK(count("scheduling_task_events_registered") == 0);
    CHECK(count("scheduling_scan_performance_registered") == 0);
    CHECK(count("scheduling_abort_task_exec") == 0 && count("scheduling_stare_task_exec") == 0);
    CHECK(count("scheduling_scan_task_exec") == 0 && count("scheduling_track_task_exec") == 0);
    if (pin) CHECK(dlclose(pin) == 0);
    CHECK(count("library_unloaded") == 1);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0 && unlink(log_path) == 0);
    return 0;
}
