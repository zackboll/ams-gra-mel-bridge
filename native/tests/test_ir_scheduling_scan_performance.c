#define _GNU_SOURCE
#include <ams_mel/abi.h>
#include "scan_performance_fixture.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); _Exit(1); } } while (0)
#define CALL(name) ams_mel_ir_scheduling_scan_performance_##name
#define DIAG NULL, 0, NULL

typedef ams_mel_ir_scheduling_scan_performance_metadata stream_t;
typedef ams_mel_ir_scheduling_scan_performance_event event_t;
typedef ams_mel_ir_scan_performance_report_v1 view_t;
extern int ams_mel_test_scan_performance_probe(stream_t *, unsigned);
extern int ams_mel_test_task_events_probe(ams_mel_ir_scheduling_task_events_metadata *, unsigned);
extern int ams_mel_test_schedule_created_probe(ams_mel_ir_scheduling_schedule_created_metadata *, unsigned);
extern int ams_mel_test_completion_owner_notify(unsigned, unsigned, uint64_t, uint64_t *);
static ams_mel_session *session;
static ams_mel_ir_scheduling *channel;
static stream_t *stream;
static void *pin;
static int (*deliver)(unsigned, unsigned);
static char log_path[] = "/tmp/ams-performance-XXXXXX";
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
    CHECK(count("scheduling_scan_performance_registered") == 0);
    CHECK(count("scheduling_prioritized_track_list_sent") == 0);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin);
    void *address = dlsym(pin, "mock_scheduling_performance_deliver");
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

/* Compare field representations, never padding or floating equality.  The same
 * oracle is exercised once per mutated field on each side. */
#define SCAN_FIELDS(WORD, FLOAT) \
    WORD(elevation_defined_with_range_and_altitude); \
    WORD(center_frame_ref_el); \
    WORD(center_frame_ref_az); \
    WORD(scan_type.continuous_scan); \
    WORD(scan_type.returning); \
    WORD(scan_type.agile_scan); \
    WORD(scan_id); \
    WORD(max_range_of_interest_m); \
    WORD(min_range_of_interest_m); \
    WORD(elevation_scan_center_altitude_m); \
    WORD(elevation_scan_center_range_m); \
    WORD(degradation_method); \
    FLOAT(center_az_rad); \
    FLOAT(center_el_rad); \
    FLOAT(scan_width_rad); \
    FLOAT(scan_height_rad); \
    FLOAT(scan_rate_rad_per_second); \
    FLOAT(preferred_revisit_interval_seconds); \
    FLOAT(required_revisit_interval_seconds);

static int same_param(const ams_mel_ir_scan_param_v1 *a, const ams_mel_ir_scan_param_v1 *b)
{
#define EQUAL(member) if (memcmp(&a->member, &b->member, sizeof a->member) != 0) return 0
    SCAN_FIELDS(EQUAL, EQUAL);
#undef EQUAL
    return 1;
}

static int same_report(const view_t *left, const view_t *right)
{
    return same_param(&left->commanded, &right->commanded) &&
           same_param(&left->achieved, &right->achieved);
}

static void check_view(event_t *event, unsigned fixture)
{
    const view_t expected = {performance_param(fixture, 0), performance_param(fixture, 1)};
    const view_t *value = NULL;
    CHECK(CALL(event_view)(event, &value, DIAG) == AMS_MEL_OK && value);
    CHECK(same_report(value, &expected));
    for (unsigned side = 0; side < 2; ++side) {
        view_t mutant = expected;
        ams_mel_ir_scan_param_v1 *nested = side ? &mutant.achieved : &mutant.commanded;
#define ALTER(member) do { \
        ((unsigned char *)&nested->member)[0] ^= 1U; \
        CHECK(!same_report(value, &mutant)); \
        ((unsigned char *)&nested->member)[0] ^= 1U; \
    } while (0)
        SCAN_FIELDS(ALTER, ALTER);
#undef ALTER
    }
}
#undef SCAN_FIELDS

static event_t *next(unsigned fixture)
{
    event_t *event = NULL;
    CHECK(CALL(receive)(stream, 0, &event, DIAG) == AMS_MEL_OK);
    check_view(event, fixture);
    return event;
}

static const unsigned producer = 6, second = 1;

static void emit(unsigned fixture)
{
    CHECK(deliver(fixture, 0));
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
    void *address = dlsym(pin, "mock_scheduling_performance_reentry");
    memcpy(&set_reentry, &address, sizeof set_reentry);
    CHECK(set_reentry);
    set_reentry(capability_reentry);
    CHECK(CALL(open)(channel, 0, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(NULL, 14, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(channel, 14, NULL, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(CALL(open)(channel, 14, &stream, NULL, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(count("scheduling_scan_performance_registered") == 0);
    ams_mel_ir_return_t registration = UINT32_MAX;
    CHECK(CALL(open)(channel, 14, &stream, &registration, DIAG) == AMS_MEL_OK);
    CHECK(reentered == 1 && registration == AMS_MEL_IR_RETURN_SUCCESS);
    set_reentry(NULL);
    counters(21, 0, 7); // fourteen valid, six bad nested enums, one null, all synchronous
    CHECK(ams_mel_test_scan_performance_probe(stream, 0));
    emit(second); // full: must not allocate or displace an earlier event
    counters(22, 1, 7);
    event_t *held[15];
    held[0] = next(0);
    emit(second); // the allocation failpoint must still be armed
    counters(23, 1, 8);
    emit(second); // recover; duplicate report and non-monotonic scan IDs
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    counters(25, 1, 8); // destructor callback counted, but not admitted
    for (size_t i = 1; i < 14; ++i) held[i] = next((unsigned)i);
    held[14] = next(second);
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == 1);
    for (size_t i = 0; i < 15; ++i) {
        check_view(held[i], i < 14 ? (unsigned)i : second);
        close_event(&held[i]);
    }
}

static void saturation(void)
{
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    expect_empty(0, AMS_MEL_TIMEOUT);
    expect_empty(1, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_test_scan_performance_probe(stream, 1));
    for (unsigned i = 0; i < 3; ++i) {
        emit(second);
        CHECK(deliver(0, 7));
    }
    counters(UINT64_MAX, UINT64_MAX, UINT64_MAX);
    event_t *event = next(second);
    close_event(&event);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK && !stream);
    emit(producer);
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
    CHECK(ams_mel_test_scan_performance_probe(stream, 5));
    closer(NULL);
    CHECK(pthread_join(rx, NULL) == 0);
}

static void provider_owned_producer(int close_stream)
{
    pthread_t close_thread;
    CHECK(ams_mel_test_scan_performance_probe(NULL, 2));
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_test_scan_performance_probe(stream, 3));
    if (close_stream) CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(pthread_create(&close_thread, NULL, closer, NULL) == 0);
    CHECK(ams_mel_test_scan_performance_probe(stream, 6));
    CHECK(count("scan_performance_producer_returned") == 0);
    CHECK(count("channel_detached") == 0 && count("library_unloaded") == 0);
    if (!close_stream) expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_test_scan_performance_probe(stream, 4));
    CHECK(pthread_join(close_thread, NULL) == 0);
    CHECK(count("scan_performance_producer_returned") == 1 && count("library_unloaded") == 1);
    if (!close_stream) counters(2, 0, 0);
}

static void *deliver_second(void *arg)
{
    (void)arg;
    emit(second);
    return NULL;
}

static void overlapping(void)
{
    pthread_t thread;
    CHECK(ams_mel_test_scan_performance_probe(NULL, 2));
    CHECK(CALL(open)(channel, 2, &stream, NULL, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_test_scan_performance_probe(stream, 3));
    CHECK(pthread_create(&thread, NULL, deliver_second, NULL) == 0);
    CHECK(ams_mel_test_scan_performance_probe(stream, 7));
    counters(2, 0, 0);
    expect_empty(0, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_test_scan_performance_probe(stream, 4));
    CHECK(pthread_join(thread, NULL) == 0);
    counters(2, 0, 0);
    event_t *first = next(producer), *last = next(second);
    close_event(&first); close_event(&last);
}

#define FAMILY_(f, op) ams_mel_ir_scheduling_##f##_##op
#define FAMILY(f, op) FAMILY_(f, op)
#define OPEN_FAMILY(f) \
    FAMILY(f, metadata) *f = NULL, *duplicate_##f = NULL; \
    CHECK(FAMILY(f, open)(channel, 1, &f, NULL, DIAG) == AMS_MEL_OK); \
    CHECK(FAMILY(f, open)(channel, 1, &duplicate_##f, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT); \
    CHECK(!duplicate_##f)
#define CHECK_FAMILY_COUNTERS(f, received, dropped, malformed) do { \
    ams_mel_ir_metadata_counters_v1 counter; \
    CHECK(FAMILY(f, get_counters)(f, &counter, DIAG) == AMS_MEL_OK); \
    CHECK(counter.events_received == (received) && counter.events_dropped_queue_full == (dropped) && \
          counter.malformed_or_unsupported == (malformed)); \
} while (0)
#define RECEIVE_FAMILY(f, status) do { \
    FAMILY(f, event) *event = NULL; \
    CHECK(FAMILY(f, receive)(f, 0, &event, DIAG) == (status)); \
    CHECK((event != NULL) == ((status) == AMS_MEL_OK)); \
    CHECK(FAMILY(f, event_close)(&event, DIAG) == AMS_MEL_OK); \
} while (0)

static void all_families(void)
{
    OPEN_FAMILY(execute_task_ack);
    OPEN_FAMILY(task_executing);
    OPEN_FAMILY(schedule_created);
    OPEN_FAMILY(update_track_list);
    OPEN_FAMILY(task_events);
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_OK);
    counters(2, 1, 0);
    event_t *event = next(0);
    emit(second);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK); // discard queued event only here
    emit(producer); // late callback after public close
    CHECK(CALL(open)(channel, 1, &stream, NULL, DIAG) == AMS_MEL_INVALID_ARGUMENT);

    CHECK_FAMILY_COUNTERS(execute_task_ack, 4, 3, 0);
    CHECK_FAMILY_COUNTERS(task_executing, 4, 3, 0);
    CHECK_FAMILY_COUNTERS(schedule_created, 3, 2, 0);
    CHECK_FAMILY_COUNTERS(update_track_list, 4, 3, 0);
    CHECK_FAMILY_COUNTERS(task_events, 8, 7, 0);
    RECEIVE_FAMILY(execute_task_ack, AMS_MEL_OK);
    RECEIVE_FAMILY(task_executing, AMS_MEL_OK);
    RECEIVE_FAMILY(schedule_created, AMS_MEL_OK);
    RECEIVE_FAMILY(update_track_list, AMS_MEL_OK);
    RECEIVE_FAMILY(task_events, AMS_MEL_OK);
    // Deliver fresh data to the other five families, not just old queue entries.
    int (*ack)(uint32_t, uint32_t, unsigned);
    int (*rep)(void);
    int (*created)(unsigned);
    int (*update)(uint32_t, uint32_t, int);
    void *symbol = dlsym(pin, "mock_scheduling_ack_deliver");
    memcpy(&ack, &symbol, sizeof ack);
    symbol = dlsym(pin, "mock_scheduling_events_republish_rep");
    memcpy(&rep, &symbol, sizeof rep);
    symbol = dlsym(pin, "mock_scheduling_created_deliver");
    memcpy(&created, &symbol, sizeof created);
    symbol = dlsym(pin, "mock_scheduling_update_deliver");
    memcpy(&update, &symbol, sizeof update);
    int (*events)(uint32_t, uint32_t, uint32_t, int64_t, uint32_t, int);
    symbol = dlsym(pin, "mock_scheduling_events_deliver");
    memcpy(&events, &symbol, sizeof events);
    CHECK(ack && rep && created && update && events);
    CHECK(events(2, 101, 111, -87, UINT32_MAX, 0));
    CHECK_FAMILY_COUNTERS(task_events, 9, 7, 0);
    CHECK(ack(17, 18, 0) && rep() && created(1) && update(21, 2, 0));
    CHECK_FAMILY_COUNTERS(execute_task_ack, 5, 3, 0);
    CHECK_FAMILY_COUNTERS(task_executing, 5, 3, 0);
    CHECK_FAMILY_COUNTERS(schedule_created, 4, 2, 0);
    CHECK_FAMILY_COUNTERS(update_track_list, 5, 3, 0);
    CHECK(FAMILY(execute_task_ack, close)(&execute_task_ack, DIAG) == AMS_MEL_OK);
    CHECK(ack(23, 24, 0)); // late ack cannot disturb the remaining queues
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    RECEIVE_FAMILY(task_executing, AMS_MEL_OK);
    RECEIVE_FAMILY(schedule_created, AMS_MEL_OK);
    RECEIVE_FAMILY(update_track_list, AMS_MEL_OK);
    RECEIVE_FAMILY(task_events, AMS_MEL_OK);
    RECEIVE_FAMILY(task_executing, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(schedule_created, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(update_track_list, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(task_events, AMS_MEL_STREAM_STOPPED);
    CHECK(FAMILY(task_events, close)(&task_events, DIAG) == AMS_MEL_OK);

    CHECK(FAMILY(task_executing, close)(&task_executing, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(schedule_created, close)(&schedule_created, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(update_track_list, close)(&update_track_list, DIAG) == AMS_MEL_OK);
    check_view(event, 0);
    close_event(&event);
}

static void drain_all_families(void)
{
    pthread_t close_thread;
    OPEN_FAMILY(execute_task_ack);
    OPEN_FAMILY(task_executing);
    OPEN_FAMILY(update_track_list);
    CHECK(ams_mel_test_schedule_created_probe(NULL, 2));
    OPEN_FAMILY(schedule_created);
    CHECK(ams_mel_test_schedule_created_probe(schedule_created, 3));
    CHECK(ams_mel_test_task_events_probe(NULL, 2));
    CHECK(ams_mel_test_scan_performance_probe(NULL, 2));
    OPEN_FAMILY(task_events);
    OPEN_FAMILY(scan_performance);
    stream = scan_performance;
    CHECK(ams_mel_test_task_events_probe(task_events, 3));
    CHECK(ams_mel_test_scan_performance_probe(stream, 3));
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(pthread_create(&close_thread, NULL, closer, NULL) == 0);
    CHECK(ams_mel_test_schedule_created_probe(schedule_created, 6));
    expect_empty(0, AMS_MEL_STREAM_STOPPED);
    CHECK(count("channel_detached") == 0);
    CHECK(ams_mel_test_schedule_created_probe(schedule_created, 4));
    CHECK(ams_mel_test_task_events_probe(task_events, 6));
    CHECK(ams_mel_test_task_events_probe(task_events, 4));
    CHECK(ams_mel_test_scan_performance_probe(stream, 6));
    CHECK(count("channel_detached") == 0 && count("scan_performance_producer_returned") == 0);
    CHECK(ams_mel_test_scan_performance_probe(stream, 4));
    CHECK(pthread_join(close_thread, NULL) == 0);
    CHECK(count("created_producer_returned") == 1 && count("scan_performance_producer_returned") == 1);
    CHECK(count("channel_detached") == 1 && count("library_unloaded") == 1);
    counters(2, 0, 0);
    RECEIVE_FAMILY(execute_task_ack, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(task_executing, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(schedule_created, AMS_MEL_STREAM_STOPPED);
    RECEIVE_FAMILY(update_track_list, AMS_MEL_STREAM_STOPPED);
    CHECK(FAMILY(execute_task_ack, close)(&execute_task_ack, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(task_executing, close)(&task_executing, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(schedule_created, close)(&schedule_created, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(task_events, close)(&task_events, DIAG) == AMS_MEL_OK);
    CHECK(FAMILY(update_track_list, close)(&update_track_list, DIAG) == AMS_MEL_OK);
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
    emit(second);
    event_t *first = next(0), *last = next(1);
    close_event(&first); close_event(&last);
    expect_empty(0, AMS_MEL_PROVIDER_FAILED);
}

static void refusal(const char *scenario)
{
    ams_mel_ir_return_t expected = UINT32_MAX, actual = UINT32_MAX;
    if (strstr(scenario, "not-supported")) expected = AMS_MEL_IR_RETURN_NOT_SUPPORTED;
    if (strstr(scenario, "not-implemented")) expected = AMS_MEL_IR_RETURN_NOT_IMPLEMENTED;
    if (strstr(scenario, "bad-pointer")) expected = AMS_MEL_IR_RETURN_BAD_POINTER;
    if (strcmp(scenario, "sched-performance-fail") == 0) expected = AMS_MEL_IR_RETURN_FAIL;
    char diagnostic[120] = {0};
    size_t required = 0;
    CHECK(CALL(open)(channel, 1, &stream, &actual, diagnostic, sizeof diagnostic, &required) ==
          (strstr(scenario, "throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED));
    CHECK(!stream && actual == expected);
    CHECK(strstr(diagnostic, "ScanPerformanceReport") && required == strlen(diagnostic) + 1);
    emit(second); // mock retained the callback, even on failure
    CHECK(CALL(open)(channel, 1, &stream, &actual, DIAG) == AMS_MEL_INVALID_ARGUMENT && !stream);
    capability_reentry();
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    ams_mel_ir_channel *common = NULL;
    CHECK(ams_mel_ir_channel_from_scheduling(channel, &common, DIAG) == AMS_MEL_OK);
    if (expected == AMS_MEL_IR_RETURN_NOT_SUPPORTED) {
        ams_mel_ir_return_request *keepalive = NULL;
        ams_mel_ir_return_result_v1 result;
        CHECK(ams_mel_ir_channel_send_keepalive(common, &keepalive, DIAG) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_wait(keepalive, 5000, &result, DIAG) == AMS_MEL_OK);
        CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
        CHECK(ams_mel_ir_return_request_close(&keepalive, DIAG) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_channel_close(&common, DIAG) == AMS_MEL_OK);
}

static void independent(void)
{
    int (*release)(unsigned, unsigned);
    void *address = dlsym(pin, "mock_completion_release_family");
    memcpy(&release, &address, sizeof release);
    CHECK(release);
    CHECK(ams_mel_ir_scheduling_enable(channel, DIAG) == AMS_MEL_OK);
    ams_mel_ir_return_request *requests[4] = {0};
    ams_mel_ir_track_list_request *prioritized = NULL;
    const ams_mel_ir_task_info_v1 abort = {.request_id = 8001};
    const ams_mel_ir_scan_task_v1 scan = {.command_id = 8001};
    const ams_mel_ir_stare_task_v1 stare = {.command_id = 8001};
    const ams_mel_ir_track_task_v1 track = {.command_id = 8001};
    const ams_mel_ir_prioritized_track_list_cmd_v1 list = {.command_id = 8001};
    CHECK(ams_mel_ir_scheduling_exec_abort(channel, &abort, &requests[0], DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_stare(channel, &stare, &requests[1], DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_scan(channel, &scan, &requests[2], DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_exec_track(channel, &track, &requests[3], DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &list, &prioritized, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_scan_performance_registered") == 0);
    CHECK(CALL(open)(channel, 2, &stream, NULL, DIAG) == AMS_MEL_OK);
    const unsigned matching = 5;
    emit(matching);
    event_t *event = next(matching);
    close_event(&event);
    ams_mel_ir_return_result_v1 result;
    ams_mel_ir_track_list_result_v1 receipt;
    for (unsigned i = 0; i < 4; ++i)
        CHECK(ams_mel_ir_return_request_wait(requests[i], 0, &result, DIAG) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_track_list_request_wait(prioritized, 0, &receipt, DIAG) == AMS_MEL_TIMEOUT);
    counters(1, 0, 0);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    emit(matching); // late delivery is independent of all five futures too
    for (unsigned i = 0; i < 4; ++i)
        CHECK(ams_mel_ir_return_request_wait(requests[i], 0, &result, DIAG) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_track_list_request_wait(prioritized, 0, &receipt, DIAG) == AMS_MEL_TIMEOUT);
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(release(15 + i, 1));
        CHECK(ams_mel_ir_return_request_wait(requests[i], 5000, &result, DIAG) == AMS_MEL_OK);
        CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
        CHECK(ams_mel_ir_return_request_close(&requests[i], DIAG) == AMS_MEL_OK);
    }
    CHECK(release(19, 1));
    CHECK(ams_mel_ir_track_list_request_wait(prioritized, 5000, &receipt, DIAG) == AMS_MEL_OK);
    CHECK(receipt.value == AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED);
    CHECK(ams_mel_ir_track_list_request_close(&prioritized, DIAG) == AMS_MEL_OK);

    // An independently pending request retains the provider graph after both closes.
    CHECK(ams_mel_ir_scheduling_send_prioritized_tracks(channel, &list, &prioritized, DIAG) == AMS_MEL_OK);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_list_request_wait(prioritized, 0, &receipt, DIAG) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(count("channel_detached") == 0 && count("library_unloaded") == 0);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin && release(19, 1));
    CHECK(ams_mel_ir_track_list_request_wait(prioritized, 5000, &receipt, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_list_request_close(&prioritized, DIAG) == AMS_MEL_OK);
    uint64_t observed = 0;
    CHECK(ams_mel_test_completion_owner_notify(12, 0, 0, &observed));
    CHECK(count("channel_detached") == 1);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *scenario = argv[1];
    setup(scenario);
    if (strcmp(scenario, "sched-performance-sync") == 0) payload();
    else if (strcmp(scenario, "sched-performance-saturate") == 0) saturation();
    else if (strcmp(scenario, "sched-performance-wake") == 0) wake_receiver();
    else if (strcmp(scenario, "sched-performance-unpin") == 0) provider_owned_producer(0);
    else if (strcmp(scenario, "sched-performance-close-copy") == 0) provider_owned_producer(1);
    else if (strcmp(scenario, "sched-performance-overlap") == 0) overlapping();
    else if (strcmp(scenario, "sched-both-all-events") == 0) all_families();
    else if (strcmp(scenario, "sched-both-events-drain") == 0) drain_all_families();
    else if (strcmp(scenario, "sched-performance-detach-fail") == 0) detach_failure();
    else if (strcmp(scenario, "sched-performance-commands") == 0) independent();
    else refusal(scenario);
    CHECK(CALL(close)(&stream, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&channel, DIAG) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, DIAG) == AMS_MEL_OK);
    CHECK(count("scheduling_scan_performance_registered") == 1);
    const unsigned other = strncmp(scenario, "sched-both-", sizeof("sched-both-") - 1) == 0;
    CHECK(count("scheduling_execute_task_ack_registration_attempted") == other);
    CHECK(count("scheduling_task_executing_registration_attempted") == other);
    CHECK(count("scheduling_sched_created_registered") == other);
    const unsigned commands = strcmp(scenario, "sched-performance-commands") == 0;
    CHECK(count("scheduling_prioritized_track_list_sent") == 2U * commands);
    CHECK(count("scheduling_abort_task_exec") == commands && count("scheduling_stare_task_exec") == commands);
    CHECK(count("scheduling_scan_task_exec") == commands && count("scheduling_track_task_exec") == commands);
    CHECK(count("late_scan_performance_callback_invoked") == 1);
    if (pin) CHECK(dlclose(pin) == 0);
    CHECK(count("library_unloaded") == 1);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0 && unlink(log_path) == 0);
    return 0;
}
