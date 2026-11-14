#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-012: compiled-C11 contract for opt-in Scheduling ExecuteTaskAck
 * reception. Each scenario runs in its own process (argv[1]) so lifetime
 * logs, provider gates and retained provider graphs never leak between cases.
 * Every ordering is established by mock hooks or test-facade probes, never by
 * sleeping. Every scenario ends with the negative control that none of the nine
 * remaining Scheduling-specific methods (four exec, PrioritizedTrackListCmd
 * send, four other registrations) was reached, with an exact count of
 * ExecuteTaskAck registration attempts and (IRMEL-ADA-013) with zero
 * TaskExecutingRep registration attempts: ExecuteTaskAck never implies it. */
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

/* The new record is fixed; pre-existing layouts used here are unchanged. */
_Static_assert(sizeof(ams_mel_ir_execute_task_ack_v1) == 8, "8-byte ExecuteTaskAck");
_Static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, command_id) == 0, "command first");
_Static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, request_id) == 4, "request second");
_Static_assert(_Generic(((ams_mel_ir_execute_task_ack_v1 *)0)->command_id, uint32_t: 1,
                        default: 0), "full 32-bit command identifier");
_Static_assert(_Generic(((ams_mel_ir_execute_task_ack_v1 *)0)->request_id, uint32_t: 1,
                        default: 0), "full 32-bit request identifier");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "counters unchanged");
_Static_assert(sizeof(ams_mel_ir_scheduling_config_v1) == sizeof(ams_mel_ir_track_config_v1),
               "Scheduling configuration unchanged");
_Static_assert(AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK == 14U, "existing capability constant");

extern int ams_mel_test_scheduling_execute_task_ack_probe(unsigned operation, size_t value);
extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_scheduling_requests(const ams_mel_ir_scheduling *, size_t *);
#define PROBE ams_mel_test_scheduling_execute_task_ack_probe

typedef int (*deliver_fn)(uint32_t, uint32_t, unsigned);
typedef unsigned (*calls_fn)(void);
typedef int (*release_fn)(unsigned, unsigned);

static void failed(const char *file, int line, const char *text)
{
    fprintf(stderr, "FAIL %s:%d: %s\n", file, line, text);
    fflush(stderr);
    /* Never leave a parked provider callback blocked at the provider's
     * unload-time join: a failed check ends the process with a diagnostic. */
    (void)PROBE(2U, 0U);
    exit(1);
}
#define CHECK(x) do { if (!(x)) failed(__FILE__, __LINE__, #x); } while (0)

typedef ams_mel_ir_scheduling_execute_task_ack_metadata ack_owner;
typedef ams_mel_ir_scheduling_execute_task_ack_event ack_event;
typedef ams_mel_ir_execute_task_ack_v1 ack_value;

static char log_path[64];
static void *pin;
static deliver_fn deliver;
static calls_fn specific_calls;
static calls_fn ack_registrations;
static calls_fn rep_registrations;
static release_fn release_family;
static const ams_mel_ir_channel_comms_test_request_v1 high_ids = {
    0x80000001U, 0xf0000002U, 0xe0000003U};

/* The nine Scheduling-specific methods that stay unimplemented. */
static const char *const specific_events[] = {
    "scheduling_prioritized_track_list_sent", "scheduling_sched_created_registered", "scheduling_update_track_list_registered",
    "scheduling_task_events_registered", "scheduling_scan_performance_registered",
    "scheduling_abort_task_exec", "scheduling_scan_task_exec", "scheduling_stare_task_exec",
    "scheduling_track_task_exec"};
_Static_assert(sizeof specific_events / sizeof *specific_events == 9U, "nine methods");

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

static ams_mel_ir_scheduling_config_v1 configuration(void)
{
    ams_mel_ir_scheduling_config_v1 value;
    memset(&value, 0, sizeof value);
    value.channel_id.descriptive_label = text("IR scheduling ack channel");
    value.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    value.platform_id.descriptive_label = text("test platform");
    value.sensor_location.key = text("station-1");
    value.sensor_location.system_name = text("mock-aircraft");
    return value;
}

/* Counts exact lifetime-log lines. */
static unsigned count(const char *event)
{
    FILE *file = fopen(log_path, "r");
    char line[160];
    unsigned result = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, event)) ++result;
    }
    CHECK(fclose(file) == 0);
    return result;
}

static void append_log(const char *value)
{
    FILE *file = fopen(log_path, "ab");
    CHECK(file != NULL);
    CHECK(fprintf(file, "%s\n", value) > 0);
    CHECK(fclose(file) == 0);
}

/* The named records occur in this relative order (others may interleave). */
static void log_order(const char *const *order, size_t total)
{
    char line[160];
    size_t next = 0;
    FILE *file = fopen(log_path, "rb");
    CHECK(file != NULL);
    while (next < total && fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line, order[next])) ++next;
    }
    CHECK(fclose(file) == 0);
    if (next != total) fprintf(stderr, "missing ordered record: %s\n", order[next]);
    CHECK(next == total);
}

/* The provider is pinned for the whole scenario so hooks and counters stay
 * resolvable after the Session (and the bridge's own reference) closes. */
static ams_mel_session *open_session(const char *scenario, uint32_t limit)
{
    ams_mel_session *session = NULL;
    ams_mel_session_options_v1 options = {limit};
    int fd;
    strcpy(log_path, "/tmp/ams-sched-ack-XXXXXX");
    fd = mkstemp(log_path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1) == 0);
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    *(void **)(&deliver) = dlsym(pin, "mock_scheduling_ack_deliver");
    *(void **)(&specific_calls) = dlsym(pin, "mock_scheduling_specific_calls");
    *(void **)(&ack_registrations) = dlsym(pin, "mock_scheduling_ack_registrations");
    *(void **)(&release_family) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&rep_registrations) = dlsym(pin, "mock_scheduling_rep_registrations");
    CHECK(deliver && specific_calls && ack_registrations && release_family && rep_registrations);
    return session;
}

static ams_mel_ir_scheduling *open_scheduling(const ams_mel_session *session)
{
    ams_mel_ir_scheduling_config_v1 config = configuration();
    ams_mel_ir_scheduling *scheduling = NULL;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(scheduling != NULL);
    return scheduling;
}

/* Negative control, exact registration-attempt count, then drop the pin; the
 * provider library may unload afterwards. The lifetime log is kept so the
 * caller can still check record order; finish() removes it. */
static void release_pin(unsigned attempts, int expect_unload)
{
    size_t index;
    CHECK(specific_calls() == 0U);
    for (index = 0; index < sizeof specific_events / sizeof *specific_events; ++index)
        CHECK(count(specific_events[index]) == 0U);
    CHECK(ack_registrations() == attempts);
    CHECK(count("scheduling_execute_task_ack_registration_attempted") == attempts);
    CHECK(rep_registrations() == 0U);
    CHECK(count("scheduling_task_executing_registration_attempted") == 0U);
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == (expect_unload ? 1U : 0U));
}

static void remove_log(void)
{
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(log_path) == 0);
}

static void finish(unsigned attempts, int expect_unload)
{
    release_pin(attempts, expect_unload);
    remove_log();
}

static void counters_are(const ack_owner *m, uint64_t received, uint64_t dropped,
                         uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 c;
    memset(&c, 0xff, sizeof c);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_get_counters(m, &c, NULL, 0, NULL) ==
          AMS_MEL_OK);
    if (c.events_received != received || c.events_dropped_queue_full != dropped ||
        c.malformed_or_unsupported != malformed)
        fprintf(stderr, "counters %llu/%llu/%llu, expected %llu/%llu/%llu\n",
                (unsigned long long)c.events_received,
                (unsigned long long)c.events_dropped_queue_full,
                (unsigned long long)c.malformed_or_unsupported, (unsigned long long)received,
                (unsigned long long)dropped, (unsigned long long)malformed);
    CHECK(c.events_received == received);
    CHECK(c.events_dropped_queue_full == dropped);
    CHECK(c.malformed_or_unsupported == malformed);
}

static ack_owner *open_ack(ams_mel_ir_scheduling *scheduling, size_t capacity)
{
    ack_owner *m = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, capacity, &m, &reg, NULL, 0,
                                                      NULL) == AMS_MEL_OK);
    CHECK(m != NULL && reg == AMS_MEL_IR_RETURN_SUCCESS);
    return m;
}

static ack_event *take(ack_owner *m, const ack_value **view)
{
    ack_event *event = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(event != NULL);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(event, view, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(*view != NULL);
    return event;
}

/* Both identifiers compared by name and full width: a swap or a 16-bit
 * truncation of either identifier fails a distinct named assertion. */
static void expect_ack(const ack_value *v, uint32_t command, uint32_t request)
{
    if (v->command_id != command || v->request_id != request)
        fprintf(stderr, "ack %08x/%08x, expected %08x/%08x\n", (unsigned)v->command_id,
                (unsigned)v->request_id, (unsigned)command, (unsigned)request);
    CHECK(v->command_id == command);
    CHECK(v->request_id == request);
}

/* Takes the next event, checks it and closes it. */
static void next_ack(ack_owner *m, uint32_t command, uint32_t request)
{
    const ack_value *v = NULL;
    ack_event *event = take(m, &v);
    expect_ack(v, command, request);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
}

static void expect_receive(ack_owner *m, uint32_t timeout, ams_mel_status_t status)
{
    ack_event *event = NULL;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, timeout, &event, NULL, 0, NULL) ==
          status);
    CHECK(event == NULL);
}

/* The mock's synchronous registration delivery, in arrival order. */
static void expect_sync_four(ack_owner *m)
{
    next_ack(m, 0U, 0U);
    next_ack(m, UINT32_MAX, UINT32_MAX);
    next_ack(m, 0x80000000U, 0x80000001U);
    next_ack(m, 0x00000007U, 0xfffffff0U);
}

/* Field fidelity: both zero, both UINT32_MAX, high bits, unequal values,
 * duplicates and arrival order are preserved; every event owns its two
 * identifiers after the provider payload is overwritten and destroyed, and
 * after the metadata owner, Scheduling owner, Session and provider library
 * are all closed. The two identifiers are never required to match. */
static void scenario_fidelity(void)
{
    static const uint32_t values[][2] = {
        {0U, 0U}, {UINT32_MAX, UINT32_MAX}, {0x80000000U, 0x00000001U},
        {0x00010000U, 0x0000ffffU}, {0x12345678U, 0x9abcdef0U}, {0xfffe0001U, 0x0001fffeU},
        {0x12345678U, 0x9abcdef0U}, {0x12345678U, 0x9abcdef0U}, {7U, 7U}};
    enum { total = sizeof values / sizeof *values };
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, total);
    ack_event *events[total];
    const ack_value *v = NULL;
    size_t index;
    /* Registration made no provider delivery for this scenario. */
    counters_are(m, 0, 0, 0);
    expect_receive(m, 0, AMS_MEL_TIMEOUT);
    for (index = 0; index < total; ++index)
        CHECK(deliver(values[index][0], values[index][1], 0U) == 1);
    counters_are(m, total, 0, 0);
    for (index = 0; index < total; ++index) {
        events[index] = take(m, &v);
        /* The provider payload was overwritten after callback return. */
        expect_ack(v, values[index][0], values[index][1]);
    }
    expect_receive(m, 0, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK && !m);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK && !session);
    finish(1U, 1);
    /* All owners closed and the provider unloaded: every event still owns
     * both full-width identifiers in arrival order, duplicates included. */
    for (index = 0; index < total; ++index) {
        CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(events[index], &v, NULL, 0,
                                                                NULL) == AMS_MEL_OK);
        expect_ack(v, values[index][0], values[index][1]);
        CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&events[index], NULL, 0,
                                                                 NULL) == AMS_MEL_OK);
        CHECK(events[index] == NULL);
    }
}

/* Successful Attached registration with synchronous delivery during the
 * provider call: all four acknowledgements are queued in arrival order; then
 * one shot after success; Enable afterwards leaves the owner usable. */
static void scenario_attached_sync(void)
{
    ams_mel_session *session = open_session("sched-ack-sync", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 8U), *again = NULL;
    ams_mel_ir_return_t reg = 99U;
    char d[128] = {0};
    CHECK(count("scheduling_execute_task_ack_sync_delivered") == 1U);
    CHECK(count("scheduling_enabled") == 0U);
    counters_are(m, 4, 0, 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 8U, &again, &reg, d, sizeof d,
                                                      NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(!again && reg == 99U && ack_registrations() == 1U);
    CHECK(!strcmp(d, "Scheduling owner is not eligible or ExecuteTaskAck was already attempted"));
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    expect_sync_four(m);
    CHECK(deliver(0xdeadbeefU, 0x00c0ffeeU, 0U) == 1);
    next_ack(m, 0xdeadbeefU, 0x00c0ffeeU);
    counters_are(m, 5, 0, 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* Successful registration on an Enabled owner. */
static void scenario_enabled(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m;
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    m = open_ack(scheduling, 2U);
    CHECK(deliver(0x00000001U, 0xffffffffU, 0U) == 1);
    next_ack(m, 0x00000001U, 0xffffffffU);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_disabled") == 1U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* NotSupported, Fail, NotImplemented, unknown Return and provider exception
 * are each distinguishable; every attempt consumes the one shot, publishes no
 * owner, and the retained callback stays inactive (never queued). The
 * Scheduling owner and its inherited services stay usable. */
static void registration_case(const char *scenario, ams_mel_status_t status, int has_return,
                              ams_mel_ir_return_t expected, const char *diagnostic)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    ams_mel_ir_return_t reg = 99U;
    char d[160] = {0};
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 4U, &m, &reg, d, sizeof d,
                                                      NULL) == status);
    CHECK(m == NULL && ack_registrations() == 1U);
    CHECK(reg == (has_return ? expected : 99U));
    if (strcmp(d, diagnostic)) fprintf(stderr, "diagnostic: %s\n", d);
    CHECK(!strcmp(d, diagnostic));
    /* One shot after failure: refused before any provider call. */
    reg = 99U;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 4U, &m, &reg, d, sizeof d,
                                                      NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && reg == 99U && ack_registrations() == 1U);
    CHECK(!strcmp(d, "Scheduling owner is not eligible or ExecuteTaskAck was already attempted"));
    /* A provider that still holds the callback cannot enqueue anything. */
    (void)deliver(5U, 6U, 0U);
    /* The Scheduling owner is unaffected by the callback refusal. */
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_get_capabilities(scheduling, &capability, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

static void scenario_registration(const char *name)
{
    if (!strcmp(name, "not-supported"))
        /* A provider with no "sched-ack-" scenario does not offer the
         * conditional callback at all. */
        registration_case("sched-basic", AMS_MEL_PROVIDER_FAILED, 1,
                          AMS_MEL_IR_RETURN_NOT_SUPPORTED,
                          "provider does not support ExecuteTaskAck callback");
    else if (!strcmp(name, "fail"))
        registration_case("sched-ack-register-fail", AMS_MEL_PROVIDER_FAILED, 1,
                          AMS_MEL_IR_RETURN_FAIL, "ExecuteTaskAck callback registration failed");
    else if (!strcmp(name, "not-implemented"))
        registration_case("sched-ack-register-not-implemented", AMS_MEL_PROVIDER_FAILED, 1,
                          AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
                          "ExecuteTaskAck callback registration failed");
    else if (!strcmp(name, "unknown"))
        registration_case("sched-ack-register-unknown", AMS_MEL_PROVIDER_FAILED, 0, 0U,
                          "provider returned an unknown ExecuteTaskAck registration result");
    else if (!strcmp(name, "throw"))
        registration_case("sched-ack-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0, 0U,
                          "provider exception during ExecuteTaskAck registration");
    else failed(__FILE__, __LINE__, name);
}

/* A Failed (enable refused) owner is ineligible; nothing is registered. */
static void scenario_failed_owner(void)
{
    ams_mel_session *session = open_session("sched-enable-fail", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = NULL;
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 2U, &m, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(0U, 1);
}

/* Invalid local arguments are rejected before the one shot is consumed or the
 * provider is reached; zero capacity is refused before registration. */
static void scenario_invalid(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = NULL, *stale = (ack_owner *)&session;
    ack_event *event = NULL, *occupied;
    const ack_value *v = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    char d[4];
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(NULL, 1, &m, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 0, &m, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 1, NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 1, &stale, NULL, NULL, 0,
                                                      NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_open(scheduling, 1, &m, NULL, NULL, 1, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && ack_registrations() == 0U);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(NULL, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_get_counters(NULL, &counters, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 1, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, d, sizeof d, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(NULL, &v, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, d, sizeof d, NULL) ==
          AMS_MEL_OK);
    /* After all rejections the first real attempt still succeeds. */
    m = open_ack(scheduling, 1U);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, 0, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_get_counters(m, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(deliver(1U, 2U, 0U) == 1);
    event = take(m, &v);
    occupied = event;
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, 0, &occupied, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(event, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

static void *deliver_when_receive_waits(void *argument)
{
    const uint32_t tag = *(const uint32_t *)argument;
    if (!PROBE(7U, 0U)) abort();
    if (deliver(tag, ~tag, 0U) != 1) abort();
    return NULL;
}

/* Capacity one, bounded DROP-INCOMING with saturating counters. A full queue
 * counts a valid payload queue-full without allocating a snapshot (the armed
 * allocation failure stays armed and no copy is attempted) and still counts a
 * null payload malformed. An allocation failure rejects one event atomically;
 * reception then recovers. A finite timeout is TIMEOUT, not cancellation, and
 * a blocked Receive is woken by delivery. */
static void scenario_capacity_one(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 1U);
    ack_event *event = NULL;
    const ack_value *v = NULL;
    pthread_t thread;
    uint32_t tag = 0x0000f00dU;
    int copies;
    CHECK(deliver(0x10000001U, 0x20000002U, 0U) == 1);       /* copied, queue now full */
    counters_are(m, 1, 0, 0);
    copies = PROBE(4U, 0U);
    CHECK(PROBE(5U, 1U) == 1);                               /* arm allocation failure */
    CHECK(deliver(0x30000003U, 0x40000004U, 0U) == 1);       /* valid, full: dropped */
    counters_are(m, 2, 1, 0);
    CHECK(deliver(0U, 0U, 1U) == 1);                         /* null while full: malformed */
    counters_are(m, 3, 1, 1);
    CHECK(deliver(UINT32_MAX, UINT32_MAX, 0U) == 1);         /* valid, full: dropped */
    counters_are(m, 4, 2, 1);
    CHECK(PROBE(6U, 0U) == 1);                               /* still armed */
    CHECK(PROBE(4U, 0U) == copies);                          /* no snapshot was allocated */
    next_ack(m, 0x10000001U, 0x20000002U);                   /* original retained */
    /* With space: a null payload is malformed and allocates nothing. */
    CHECK(deliver(0U, 0U, 1U) == 1);
    counters_are(m, 5, 2, 2);
    CHECK(PROBE(6U, 0U) == 1 && PROBE(4U, 0U) == copies);
    /* The allocation is attempted and its failure rejects the whole event;
     * nothing partial is published. */
    CHECK(deliver(0x50000005U, 0x60000006U, 0U) == 1);
    CHECK(PROBE(6U, 0U) == 0);
    CHECK(PROBE(4U, 0U) == copies + 1);
    counters_are(m, 6, 2, 3);
    expect_receive(m, 0, AMS_MEL_TIMEOUT);
    /* Recovery: the same payload now copies completely. */
    CHECK(deliver(0x50000005U, 0x60000006U, 0U) == 1);
    next_ack(m, 0x50000005U, 0x60000006U);
    counters_are(m, 7, 2, 3);
    /* Finite timeout on an active empty queue is TIMEOUT, not cancellation. */
    expect_receive(m, 20U, AMS_MEL_TIMEOUT);
    /* A blocked Receive is woken by delivery (ordered by the probe). */
    CHECK(PROBE(8U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, deliver_when_receive_waits, &tag) == 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, 30000U, &event, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(event, &v, NULL, 0, NULL) ==
          AMS_MEL_OK);
    expect_ack(v, tag, ~tag);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    counters_are(m, 8, 2, 3);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* A larger queue keeps FIFO order up to capacity, copies only what it
 * queues and drops only the excess incoming events. */
static void scenario_fifo(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 3U);
    const int copies = PROBE(4U, 0U);
    uint32_t tag;
    for (tag = 40U; tag < 45U; ++tag) CHECK(deliver(tag, 0x80000000U | tag, 0U) == 1);
    counters_are(m, 5, 2, 0);
    CHECK(PROBE(4U, 0U) == copies + 3);
    for (tag = 40U; tag < 43U; ++tag) next_ack(m, tag, 0x80000000U | tag);
    expect_receive(m, 0, AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    /* Public owner Close: STREAM_STOPPED semantics are no longer reachable,
     * and the retained callback never enqueues. */
    CHECK(deliver(1U, 1U, 0U) == 1);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

struct receive_job { ack_owner *owner; ams_mel_status_t status; atomic_int returned; };

static void *blocked_receive(void *argument)
{
    struct receive_job *job = argument;
    ack_event *event = NULL;
    job->status = ams_mel_ir_scheduling_execute_task_ack_receive(job->owner, 30000U, &event,
                                                                NULL, 0, NULL);
    if (event) abort();
    atomic_store(&job->returned, 1);
    return NULL;
}

/* Scheduling Close wakes a Receive blocked on an empty active queue with
 * STREAM_STOPPED (ordered by the probe, no sleep), long before its timeout. */
static void scenario_close_wakes(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 2U);
    struct receive_job job;
    pthread_t thread;
    job.owner = m; job.status = AMS_MEL_INTERNAL_ERROR; atomic_init(&job.returned, 0);
    CHECK(PROBE(8U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, blocked_receive, &job) == 0);
    CHECK(PROBE(7U, 0U) == 1);                               /* receiver is blocked */
    CHECK(atomic_load(&job.returned) == 0);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(job.status == AMS_MEL_STREAM_STOPPED);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* Events queued before Scheduling Close (here the four synchronous ones)
 * still drain in order after Scheduling Close and Session Close, then Receive
 * reports STREAM_STOPPED. */
static void scenario_drain_after_close(void)
{
    ams_mel_session *session = open_session("sched-ack-sync", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 8U);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_channel_destroyed") == 1U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("control_destroyed") == 1U);
    counters_are(m, 4, 0, 0);
    expect_sync_four(m);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    expect_receive(m, 10U, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* Public metadata Close before Scheduling Close: consumption is deactivated
 * (no unregister exists), an event already received stays valid, and the
 * provider's late callback during channel destruction is memory-safe and
 * returns before the channel is gone and the library unloads. */
static void scenario_owner_closed_first(void)
{
    const char *order[] = {"ack_owner_closed", "late_ack_callback_invoked",
                           "late_ack_callback_returned", "scheduling_channel_destroyed",
                           "control_destroyed", "library_unloaded"};
    ams_mel_session *session = open_session("sched-ack-late-closed", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 8U);
    const ack_value *v = NULL;
    ack_event *event = take(m, &v);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK && !m);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    append_log("ack_owner_closed");
    CHECK(deliver(9U, 10U, 0U) == 1);          /* callback still registered: ignored */
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    release_pin(1U, 1);
    log_order(order, sizeof order / sizeof *order);
    remove_log();
    /* The event received before owner Close outlives every owner. */
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_view(event, &v, NULL, 0, NULL) ==
          AMS_MEL_OK);
    expect_ack(v, 0U, 0U);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
}

/* The parent Session closes first: the Scheduling owner keeps the provider
 * graph and library alive, ExecuteTaskAck reception keeps working, and the
 * Scheduling Close then destroys the channel before the Control. */
static void scenario_parent_first(void)
{
    const char *order[] = {"channel_detached", "scheduling_channel_destroyed",
                           "control_destroyed", "manager_destroyed"};
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 4U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK && !session);
    CHECK(count("control_destroyed") == 0U && count("scheduling_channel_destroyed") == 0U);
    CHECK(deliver(0xcafef00dU, 0x0000beefU, 0U) == 1);
    next_ack(m, 0xcafef00dU, 0x0000beefU);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    counters_are(m, 1, 0, 0);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    release_pin(1U, 1);
    log_order(order, sizeof order / sizeof *order);
    remove_log();
}

/* A late provider callback during channel destruction, with the owner still
 * open: it runs after Scheduling Close stopped consumption, is counted but
 * never queued, returns before the channel is destroyed, and the four events
 * queued earlier still drain. */
static void scenario_late(void)
{
    const char *order[] = {"late_ack_callback_invoked", "late_ack_callback_returned",
                           "scheduling_channel_destroyed", "control_destroyed",
                           "library_unloaded"};
    ams_mel_session *session = open_session("sched-ack-late", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 8U);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("late_ack_callback_returned") == 1U);
    counters_are(m, 5, 0, 0);
    expect_sync_four(m);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    release_pin(1U, 1);
    log_order(order, sizeof order / sizeof *order);
    remove_log();
}

static size_t scheduling_requests(const ams_mel_ir_scheduling *scheduling)
{
    size_t requests = 99U;
    CHECK(ams_mel_test_scheduling_requests(scheduling, &requests));
    return requests;
}

/* Independence from the inherited common Channel operations: registering
 * ExecuteTaskAck is not a common request, KeepAlive and CommsTest through the
 * weak view neither register nor consume acknowledgements, and an
 * acknowledgement is never matched to a CommsTest request with the same
 * identifiers. */
static void scenario_common(void)
{
    ams_mel_session *session = open_session("sched-ack-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    ams_mel_ir_channel_comms_test_result_v1 report = {0U, 0U, 99U};
    uint64_t observed = 0U;
    ack_owner *m;
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ack_registrations() == 0U);
    m = open_ack(scheduling, 4U);
    CHECK(scheduling_requests(scheduling) == 0U);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 0, NULL) ==
          AMS_MEL_OK && reply);
    CHECK(scheduling_requests(scheduling) == 2U);
    /* An acknowledgement carrying the CommsTest identifiers is only data. */
    CHECK(deliver(high_ids.command_id, high_ids.request_id, 0U) == 1);
    counters_are(m, 1, 0, 0);
    CHECK(release_family(1U, 1U) && release_family(2U, 1U));
    CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000U, &report, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(report.command_id == high_ids.command_id && report.request_id == high_ids.request_id);
    CHECK(ams_mel_test_completion_owner(1U, 1U, &observed));
    CHECK(ams_mel_test_completion_owner(2U, 1U, &observed));
    CHECK(scheduling_requests(scheduling) == 0U);
    counters_are(m, 1, 0, 0);
    next_ack(m, high_ids.command_id, high_ids.request_id);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

/* An inherited common request pending during Scheduling Close: Close returns
 * OK at once and stops ExecuteTaskAck consumption (receivers see the queued
 * event, then STREAM_STOPPED) but physical teardown stays deferred. The
 * provider channel and its retained callback stay alive, so a delivery is
 * counted yet never queued. Only the final completion disables, detaches,
 * destroys the channel and then drains callbacks; admission and cancellation
 * semantics are unchanged (timeout is not cancellation). */
static void scenario_pending(int detach_fails)
{
    const char *order[] = {"keepalive_sent", "scheduling_disabled", "channel_detached",
                           "scheduling_channel_destroyed", "control_destroyed"};
    ams_mel_session *session = open_session(
        detach_fails ? "sched-ack-deferred-detach-fail" : "sched-ack-basic", 1U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    uint64_t observed = 0U;
    ack_owner *m;
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    m = open_ack(scheduling, 4U);
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
    CHECK(ams_mel_test_completion_wait(1U, 4U, 1U));
    CHECK(deliver(0x00000abcU, 0x00000defU, 0U) == 1);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* Deferred: nothing physical happened yet. */
    CHECK(count("scheduling_disabled") == 0U && count("channel_detached") == 0U);
    CHECK(count("scheduling_channel_destroyed") == 0U && count("control_destroyed") == 0U);
    /* Logical stop is immediate: the queued event drains, then stopped. */
    next_ack(m, 0x00000abcU, 0x00000defU);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    /* The provider still holds the callback; it is counted, never queued. */
    CHECK(deliver(0x00000123U, 0x00000456U, 0U) == 1);
    counters_are(m, 2, 0, 0);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(release_family(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, NULL, 0, NULL) ==
          (detach_fails ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
    CHECK(ams_mel_test_completion_owner(1U, 1U, &observed));
    if (detach_fails) {
        /* The public Scheduling owner is gone: uncertain deferred detach must
         * retain the whole graph permanently, including the callback. */
        CHECK(count("channel_detach_failed") == 1U);
        CHECK(count("scheduling_channel_destroyed") == 0U && count("control_destroyed") == 0U);
        CHECK(deliver(7U, 8U, 0U) == 1);
        counters_are(m, 3, 0, 0);
        expect_receive(m, 0, AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
        finish(1U, 0);
        return;
    }
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    /* Deferred cleanup ran exactly once; the hook is gone with the channel. */
    CHECK(count("scheduling_channel_destroyed") == 1U && count("control_destroyed") == 1U);
    CHECK(deliver(1U, 1U, 0U) == 0);
    counters_are(m, 2, 0, 0);
    expect_receive(m, 0, AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    release_pin(1U, 1);
    log_order(order, sizeof order / sizeof *order);
    remove_log();
}

/* A failed provider detach leaves ownership uncertain: the Scheduling owner
 * and graph are retained (no destruction, no unload even after Session
 * Close), the ExecuteTaskAck owner fails, queued events still drain first and
 * then Receive reports PROVIDER_FAILED. The retained callback stays
 * memory-safe and never enqueues. The retried Close detaches, destroys the
 * channel and only then may the library unload; the terminal failure stays. */
static void scenario_detach_failure(void)
{
    ams_mel_session *session = open_session("sched-ack-detach-fail", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ack_owner *m = open_ack(scheduling, 4U);
    ack_event *event = NULL;
    char d[128] = {0};
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(scheduling != NULL && count("scheduling_channel_destroyed") == 0U);
    next_ack(m, 0x11111111U, 0x22222222U);
    next_ack(m, 0x33333333U, 0x44444444U);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_receive(m, 10U, &event, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(!strcmp(d, "Scheduling channel cleanup failed; ExecuteTaskAck owner failed"));
    CHECK(deliver(5U, 6U, 0U) == 1);
    counters_are(m, 3, 0, 0);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("control_destroyed") == 0U && count("library_unloaded") == 0U);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(count("scheduling_channel_destroyed") == 1U && count("control_destroyed") == 1U);
    expect_receive(m, 0, AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1U, 1);
}

struct close_job { ams_mel_ir_scheduling *scheduling; atomic_int returned;
                   ams_mel_status_t status; };

static void *close_scheduling(void *argument)
{
    struct close_job *job = argument;
    job->status = ams_mel_ir_scheduling_close(&job->scheduling, NULL, 0, NULL);
    atomic_store(&job->returned, 1);
    return NULL;
}

/* Isolated process. A provider callback is inside the bridge (counted in
 * flight) while the public owner, the Session and then the Scheduling owner
 * close. The provider is non-quiescing: channel destruction does NOT join its
 * thread, so only the bridge drain holds physical teardown. Teardown must wait
 * in that drain until the callback is released, the Scheduling graph (and
 * thus the Control) is released only afterwards, and the provider callback
 * returns before the provider library unloads. */
static void scenario_in_flight(void)
{
    const char *teardown[] = {"ack_in_flight_callback_entered", "scheduling_channel_destroyed",
                              "ack_teardown_waiting_for_callback", "ack_callback_released",
                              "control_destroyed"};
    const char *unload[] = {"ack_in_flight_callback_entered", "ack_in_flight_callback_returned",
                            "library_unloaded"};
    ams_mel_session *session;
    ack_owner *m;
    struct close_job job;
    pthread_t thread;
    CHECK(PROBE(0U, 0U) == 1);                                   /* park next callback */
    session = open_session("sched-ack-in-flight", 4U);
    job.scheduling = open_scheduling(session);
    m = open_ack(job.scheduling, 2U);
    CHECK(PROBE(1U, 0U) == 1);                                   /* inside the bridge */
    CHECK(ams_mel_ir_scheduling_execute_task_ack_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("control_destroyed") == 0U);
    atomic_init(&job.returned, 0); job.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(pthread_create(&thread, NULL, close_scheduling, &job) == 0);
    CHECK(PROBE(3U, 0U) == 1);                                   /* teardown drains */
    append_log("ack_teardown_waiting_for_callback");
    CHECK(atomic_load(&job.returned) == 0);
    CHECK(count("control_destroyed") == 0U);
    append_log("ack_callback_released");
    CHECK(PROBE(2U, 0U) == 1);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(job.status == AMS_MEL_OK && job.scheduling == NULL);
    CHECK(count("control_destroyed") == 1U);
    release_pin(1U, 1);                                          /* unload joins provider */
    log_order(teardown, sizeof teardown / sizeof *teardown);
    log_order(unload, sizeof unload / sizeof *unload);
    remove_log();
}

int main(int argc, char **argv)
{
    const char *name;
    CHECK(argc == 2);
    name = argv[1];
    if (!strcmp(name, "fidelity")) scenario_fidelity();
    else if (!strcmp(name, "attached-sync")) scenario_attached_sync();
    else if (!strcmp(name, "enabled")) scenario_enabled();
    else if (!strncmp(name, "register-", 9)) scenario_registration(name + 9);
    else if (!strcmp(name, "failed-owner")) scenario_failed_owner();
    else if (!strcmp(name, "invalid")) scenario_invalid();
    else if (!strcmp(name, "capacity-one")) scenario_capacity_one();
    else if (!strcmp(name, "fifo")) scenario_fifo();
    else if (!strcmp(name, "close-wakes")) scenario_close_wakes();
    else if (!strcmp(name, "drain-after-close")) scenario_drain_after_close();
    else if (!strcmp(name, "owner-closed-first")) scenario_owner_closed_first();
    else if (!strcmp(name, "parent-first")) scenario_parent_first();
    else if (!strcmp(name, "late")) scenario_late();
    else if (!strcmp(name, "common")) scenario_common();
    else if (!strcmp(name, "pending")) scenario_pending(0);
    else if (!strcmp(name, "pending-detach-fail")) scenario_pending(1);
    else if (!strcmp(name, "detach-fail")) scenario_detach_failure();
    else if (!strcmp(name, "in-flight")) scenario_in_flight();
    else { fprintf(stderr, "unknown scenario: %s\n", name); return 2; }
    printf("PASS: native IR Scheduling ExecuteTaskAck %s\n", name);
    return 0;
}
