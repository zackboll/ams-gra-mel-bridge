#define _GNU_SOURCE
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); _Exit(1); } } while (0)
#define CALL(name) ams_mel_ir_scheduling_update_track_list_##name
#define DIAG NULL, 0, NULL

typedef ams_mel_ir_scheduling_update_track_list_metadata stream_t;
typedef ams_mel_ir_scheduling_update_track_list_event event_t;
typedef ams_mel_ir_update_track_list_response_v1 view_t;
extern int ams_mel_test_update_track_list_probe(stream_t *, unsigned);
extern int ams_mel_test_completion_owner_notify(unsigned, unsigned, uint64_t, uint64_t *);
static ams_mel_session *session;
static ams_mel_ir_scheduling *channel;
static stream_t *stream;
static void *pin;
static int (*deliver)(uint32_t, uint32_t, int);
static char log_path[] = "/tmp/ams-update-XXXXXX";
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
    CHECK(count("scheduling_update_track_list_registered") == 0);
    CHECK(count("scheduling_prioritized_track_list_sent") == 0);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    void *address = dlsym(pin, "mock_scheduling_update_deliver");
    memcpy(&deliver, &address, sizeof deliver);
    CHECK(deliver);
}

static void counters(uint64_t received, uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 c;
    CHECK(CALL(get_counters)(stream, &c, DIAG) == AMS_MEL_OK);
    CHECK(c.events_received == received && c.events_dropped_queue_full == dropped &&
          c.malformed_or_unsupported == malformed);
}

static void check_view(event_t *event, uint32_t id, uint32_t receipt)
{
    const view_t *value = NULL;
    CHECK(CALL(event_view)(event, &value, DIAG) == AMS_MEL_OK && value);
    CHECK(value->command_id == id && value->response == receipt);
}

static event_t *next(uint32_t id, uint32_t receipt)
{
    event_t *event = NULL;
    CHECK(CALL(receive)(stream, 0, &event, DIAG) == AMS_MEL_OK);
    check_view(event, id, receipt);
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
    void *address = dlsym(pin, "mock_scheduling_update_reentry");
    memcpy(&set_reentry, &address, sizeof set_reentry);
    CHECK(set_reentry);
    set_reentry(capability_reentry);
    CHECK(CALL(open)(channel, 0, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(channel, 4, &stream, NULL, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    ams_mel_ir_return_t registration = UINT32_MAX;
    CHECK(CALL(open)(channel, 4, &stream, &registration, DIAG) == AMS_MEL_OK);
    CHECK(reentered == 1 && registration == AMS_MEL_IR_RETURN_SUCCESS);
    set_reentry(NULL);
    counters(4, 0, 0);
    CHECK(ams_mel_test_update_track_list_probe(stream, 0));
    CHECK(deliver(123, 3, 0) && deliver(0, 4, 0) && deliver(0, UINT32_MAX, 0) && deliver(0, 0, 1));
    counters(8, 1, 3);
    event_t *e0 = next(0, AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE);
    // Full and malformed events did not consume the allocation failpoint.
    CHECK(deliver(456, 3, 0));
    counters(9, 1, 4);
    CHECK(deliver(0, 2, 0));
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    counters(11, 1, 4); // includes the late destructor callback
    event_t *e1 = next(UINT32_MAX, AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED);
    event_t *e2 = next(UINT32_C(0x80000001), AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED);
    event_t *e3 = next(UINT32_MAX, AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED);
    event_t *e4 = next(0, AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED);
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == 1);
    check_view(e0, 0, 0); check_view(e1, UINT32_MAX, 1);
    check_view(e2, UINT32_C(0x80000001), 2); check_view(e3, UINT32_MAX, 3); check_view(e4, 0, 2);
    close_event(&e0); close_event(&e1); close_event(&e2); close_event(&e3); close_event(&e4);
}

static void saturation(void)
{
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    expect_empty(0, AMS_MEL_TIMEOUT);
    expect_empty(2, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_test_update_track_list_probe(stream, 1));
    CHECK(deliver(7, 1, 0) && deliver(8, 0, 0) && deliver(9, 2, 0));
    CHECK(deliver(0, 4, 0) && deliver(0, 0, 1));
    counters(UINT64_MAX, UINT64_MAX, UINT64_MAX);
    event_t *event = next(7, 1);
    close_event(&event);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK && !stream); // empty stream
    CHECK(deliver(7, 1, 0));
}

static void *receiver(void *arg)
{
    (void)arg;
    expect_empty(25000, AMS_MEL_STREAM_STOPPED);
    return NULL;
}

static void *closer(void *arg)
{
    (void)arg;
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    return NULL;
}

static void wake_receiver(void)
{
    pthread_t rx;
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(pthread_create(&rx, NULL, receiver, NULL) == 0);
    CHECK(ams_mel_test_update_track_list_probe(stream, 5));
    closer(NULL);
    CHECK(pthread_join(rx, NULL) == 0);
}

static void provider_owned_producer(int close_stream)
{
    pthread_t close_thread;
    CHECK(ams_mel_test_update_track_list_probe(NULL, 2));
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_test_update_track_list_probe(stream, 3));
    if (close_stream) CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(pthread_create(&close_thread, NULL, closer, NULL) == 0);
    CHECK(ams_mel_test_update_track_list_probe(stream, 6));
    CHECK(count("update_producer_returned") == 0);
    CHECK(count("channel_detached") == 0 && count("library_unloaded") == 0);
    if (!close_stream) expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_test_update_track_list_probe(stream, 4));
    CHECK(pthread_join(close_thread, NULL) == 0);
    CHECK(count("update_producer_returned") == 1 && count("library_unloaded") == 1);
    if (!close_stream) counters(2, 0, 0);
}

static void *deliver_second(void *arg)
{
    (void)arg;
    CHECK(deliver(UINT32_MAX, 1, 0));
    return NULL;
}

static void overlapping(void)
{
    pthread_t second;
    CHECK(ams_mel_test_update_track_list_probe(NULL, 2));
    CHECK(CALL(open)(channel, 2, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_test_update_track_list_probe(stream, 3));
    CHECK(pthread_create(&second, NULL, deliver_second, NULL) == 0);
    CHECK(ams_mel_test_update_track_list_probe(stream, 7));
    counters(2, 0, 0);
    expect_empty(0, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_test_update_track_list_probe(stream, 4));
    CHECK(pthread_join(second, NULL) == 0);
    counters(2, 0, 0);
    event_t *first = next(UINT32_C(0x80000001), 2), *last = next(UINT32_MAX, 1);
    close_event(&first); close_event(&last);
}

static void all_families(void)
{
    ams_mel_ir_scheduling_execute_task_ack_metadata *ack = NULL;
    ams_mel_ir_scheduling_task_executing_metadata *rep = NULL;
    ams_mel_ir_scheduling_schedule_created_metadata *created = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(channel, 4, &ack, NULL, DIAG) == AMS_MEL_OK);
    CHECK(CALL(open)(channel, 4, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_schedule_created_open(channel, 4, &created, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_open(channel, 4, &rep, NULL, DIAG) == AMS_MEL_OK);
    ams_mel_ir_scheduling_execute_task_ack_event *ae = NULL;
    ams_mel_ir_scheduling_task_executing_event *re = NULL;
    ams_mel_ir_scheduling_schedule_created_event *ce = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(ack, 0, &ae, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&ae, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&ack, DIAG) == AMS_MEL_OK);
    event_t *event = next(0, 0);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK); // still has three queued events
    CHECK(deliver(2, 3, 0));
    CHECK(CALL(open)(channel, 4, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_schedule_created_receive(created, 0, &ce, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_schedule_created_event_close(&ce, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_schedule_created_close(&created, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_receive(rep, 0, &re, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_event_close(&re, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_task_executing_close(&rep, DIAG) == AMS_MEL_OK);
    check_view(event, 0, 0);
    close_event(&event);
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
    CHECK(deliver(2, 3, 0));
    event_t *first = next(0, 0), *second = next(UINT32_MAX, 1);
    close_event(&first); close_event(&second);
    expect_empty(0, AMS_MEL_PROVIDER_FAILED);
}

static void refusal(const char *scenario)
{
    ams_mel_ir_return_t expected = UINT32_MAX, actual = UINT32_MAX;
    if (strstr(scenario, "not-supported")) expected = AMS_MEL_IR_RETURN_NOT_SUPPORTED;
    if (strstr(scenario, "not-implemented")) expected = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED;
    if (strstr(scenario, "bad-pointer")) expected = AMS_MEL_IR_RETURN_BAD_POINTER;
    if (strcmp(scenario, "sched-update-fail") == 0) expected = AMS_MEL_IR_RETURN_FAIL;
    CHECK(CALL(open)(channel, 1, &stream, &actual, DIAG) ==
          (strstr(scenario, "throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
    CHECK(!stream && actual == expected);
    CHECK(deliver(0, 3, 0)); // mock retained the callback, even on failure
    CHECK(CALL(open)(channel, 1, &stream, &actual, DIAG) == AMS_MEL_INVALID_ARGUMENT && !stream);
    capability_reentry();
}

static ams_mel_ir_track_list_request *send(uint32_t id)
{
    const ams_mel_ir_prioritized_track_list_cmd_v1 command = {.command_id = id};
    ams_mel_ir_track_list_request *request = NULL;
    CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &command, &request, DIAG) == AMS_MEL_OK);
    return request;
}

static void wait_receipt(ams_mel_ir_track_list_request *request, uint32_t timeout, ams_mel_status_t expected)
{
    ams_mel_ir_track_list_result_v1 result = {0};
    CHECK(ams_mel_ir_track_list_request_wait(request, timeout, &result, DIAG) == expected);
    if (expected == AMS_MEL_OK) CHECK(result.value == AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED);
}

static void independent(void)
{
    int (*release)(unsigned, unsigned);
    void *address = dlsym(pin, "mock_completion_release_family");
    memcpy(&release, &address, sizeof release);
    CHECK(release);
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    ams_mel_ir_track_list_request *request = send(42);
    CHECK(count("scheduling_update_track_list_registered") == 0);
    CHECK(CALL(open)(channel, 2, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_prioritized_track_list_sent") == 1);
    for (unsigned i = 0; i < 3; ++i) {
        const uint32_t id = i == 0 ? 99U : 42U;
        if (i != 0) request = send(42);
        wait_receipt(request, 0, AMS_MEL_TIMEOUT);
        if (i == 2) {
            CHECK(release(19, 1));
            wait_receipt(request, 5000, AMS_MEL_OK);
            expect_empty(0, AMS_MEL_TIMEOUT); // completion did not enqueue metadata
        }
        CHECK(deliver(id, 3, 0));
        event_t *event = next(id, 3);
        if (i < 2) {
            wait_receipt(request, 0, AMS_MEL_TIMEOUT); // even a matching ID cannot complete it
            CHECK(release(19, 1));
            wait_receipt(request, 5000, AMS_MEL_OK);
        }
        CHECK(deliver(id, 2, 0));
        event_t *duplicate = next(id, 2);
        wait_receipt(request, 0, AMS_MEL_OK); // callback cannot mutate cached Rejected
        close_event(&event); close_event(&duplicate);
        expect_empty(0, AMS_MEL_TIMEOUT);
        CHECK(ams_mel_ir_track_list_request_close(&request, DIAG) == AMS_MEL_OK);
    }

    request = send(0);
    CHECK(deliver(42, 1, 0));
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    event_t *queued = next(42, 1);
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    close_event(&queued);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(count("channel_detached") == 0 && count("library_unloaded") == 0);
    wait_receipt(request, 0, AMS_MEL_TIMEOUT);
    // Pin only while calling provider test code; the pending request retained the DSO.
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin && release(19, 1));
    wait_receipt(request, 5000, AMS_MEL_OK);
    CHECK(ams_mel_ir_track_list_request_close(&request, DIAG) == AMS_MEL_OK);
    uint64_t observed = 0;
    CHECK(ams_mel_test_completion_owner_notify(12, 0, 0, &observed));
    CHECK(count("channel_detached") == 1);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *scenario = argv[1];
    setup(scenario);
    if (strcmp(scenario, "sched-update-sync") == 0) payload();
    else if (strcmp(scenario, "sched-update-saturate") == 0) saturation();
    else if (strcmp(scenario, "sched-update-wake") == 0) wake_receiver();
    else if (strcmp(scenario, "sched-update-unpin") == 0) provider_owned_producer(0);
    else if (strcmp(scenario, "sched-update-close-copy") == 0) provider_owned_producer(1);
    else if (strcmp(scenario, "sched-update-overlap") == 0) overlapping();
    else if (strcmp(scenario, "sched-both-sync") == 0) all_families();
    else if (strcmp(scenario, "sched-update-detach-fail") == 0) detach_failure();
    else if (strcmp(scenario, "sched-prioritized-update-independent") == 0) independent();
    else refusal(scenario);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_update_track_list_registered") == 1);
    const unsigned other = strcmp(scenario, "sched-both-sync") == 0 ? 1U : 0U;
    CHECK(count("scheduling_execute_task_ack_registration_attempted") == other);
    CHECK(count("scheduling_task_executing_registration_attempted") == other);
    CHECK(count("scheduling_sched_created_registered") == other);
    CHECK(count("scheduling_prioritized_track_list_sent") ==
          (strcmp(scenario, "sched-prioritized-update-independent") == 0 ? 4U : 0U));
    CHECK(count("scheduling_task_events_registered") == 0 && count("scheduling_scan_performance_registered") == 0);
    CHECK(count("scheduling_abort_task_exec") == 0 && count("scheduling_stare_task_exec") == 0);
    CHECK(count("scheduling_scan_task_exec") == 0 && count("scheduling_track_task_exec") == 0);
    if (pin) CHECK(dlclose(pin) == 0);
    CHECK(count("library_unloaded") == 1);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0 && unlink(log_path) == 0);
    return 0;
}
