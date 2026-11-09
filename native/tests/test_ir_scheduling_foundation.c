/* IRMEL-ADA-011: SchedulingChannel ownership/lifecycle foundation contract.
 *
 * Each scenario runs in its own process (argv[1]) so lifetime logs, provider
 * gates and permanently retained provider graphs never leak between cases.
 * Only the separate mock provider is used. Every scenario ends with the
 * negative control that no Scheduling-specific exec, send or metadata
 * registration was reached. */
#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include "admission_observation.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef struct ams_mel_test_scheduling_observer ams_mel_test_scheduling_observer;
extern int ams_mel_test_scheduling_requests(const ams_mel_ir_scheduling *, size_t *);
extern int ams_mel_test_scheduling_observer_from(const ams_mel_ir_scheduling *,
                                                 ams_mel_test_scheduling_observer **);
extern int ams_mel_test_scheduling_observer_state(const ams_mel_test_scheduling_observer *,
                                                  int *, size_t *);
extern void ams_mel_test_scheduling_observer_close(ams_mel_test_scheduling_observer **);
extern int ams_mel_test_track_requests(const ams_mel_ir_track *, size_t *);
typedef int (*release_fn)(unsigned, unsigned);
typedef unsigned (*calls_fn)(void);

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static char log_path[64];
static void *pin;
static release_fn release_family;
static calls_fn specific_calls;
static const ams_mel_ir_channel_comms_test_request_v1 high_ids = {
    0x80000001U, 0xf0000002U, 0xe0000003U};

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

static int same_text(ams_mel_string_view_v1 value, const char *expected)
{
    return value.size == strlen(expected) &&
           (value.size == 0U || memcmp(value.data, expected, value.size) == 0);
}

static ams_mel_ir_scheduling_config_v1 configuration(void)
{
    ams_mel_ir_scheduling_config_v1 value;
    size_t index;
    memset(&value, 0, sizeof value);
    value.channel_id.descriptive_label = text("IR scheduling \xCE\xBC channel");
    value.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    value.platform_id.descriptive_label = text("test platform");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    value.sensor_location.key = text("station-1");
    value.sensor_location.system_name = text("mock-aircraft");
    for (index = 0; index < 16U; ++index) {
        value.channel_id.uuid[index] = (uint8_t)(0xa0U + index);
        value.platform_id.uuid[index] = (uint8_t)(0xf0U + index);
    }
    return value;
}

/* Counts exact lifetime-log lines; NULL counts every non-probe line. */
static unsigned count(const char *event)
{
    FILE *file = fopen(log_path, "r");
    char line[160];
    unsigned result = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (event ? !strcmp(line, event) : strncmp(line, "completion_", 11) != 0) ++result;
    }
    CHECK(fclose(file) == 0);
    return result;
}

/* Strict ordering of the first occurrence of two events. */
static int before(const char *first, const char *second)
{
    FILE *file = fopen(log_path, "r");
    char line[160];
    long index = 0, a = -1, b = -1;
    CHECK(file != NULL);
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (a < 0 && !strcmp(line, first)) a = index;
        if (b < 0 && !strcmp(line, second)) b = index;
        ++index;
    }
    CHECK(fclose(file) == 0);
    return a >= 0 && b >= 0 && a < b;
}
static const char *const specific_events[] = {
    "scheduling_prioritized_track_list_sent", "scheduling_task_executing_registered",
    "scheduling_execute_task_ack_registered", "scheduling_sched_created_registered",
    "scheduling_update_track_list_registered", "scheduling_task_events_registered",
    "scheduling_scan_performance_registered", "scheduling_abort_task_exec",
    "scheduling_scan_task_exec", "scheduling_stare_task_exec", "scheduling_track_task_exec"};

/* Negative control: the foundation never reaches any of the eleven
 * Scheduling-specific exec/send/registration methods. The counter lives in
 * the mock and is read through the pin before the pin is dropped. */
static void no_scheduling_specific_calls(void)
{
    size_t index;
    CHECK(specific_calls() == 0U);
    for (index = 0; index < sizeof specific_events / sizeof *specific_events; ++index)
        CHECK(count(specific_events[index]) == 0U);
}

static ams_mel_session *open_session(const char *scenario, uint32_t limit)
{
    ams_mel_session *session = NULL;
    ams_mel_session_options_v1 options = {limit};
    int fd;
    strcpy(log_path, "/tmp/ams-scheduling-XXXXXX");
    fd = mkstemp(log_path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1) == 0);
    CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          &options, &session, NULL, 0, NULL) == AMS_MEL_OK);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    *(void **)(&release_family) = dlsym(pin, "mock_completion_release_family");
    *(void **)(&specific_calls) = dlsym(pin, "mock_scheduling_specific_calls");
    CHECK(release_family != NULL && specific_calls != NULL);
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

/* Drops the test pin last; afterwards provider code may unload. */
static void finish(int expect_unload)
{
    no_scheduling_specific_calls();
    CHECK(dlclose(pin) == 0);
    pin = NULL;
    CHECK(count("library_unloaded") == (expect_unload ? 1U : 0U));
    CHECK(unlink(log_path) == 0);
}

static const ams_mel_ir_channel_capability_v1 *viewed(const ams_mel_ir_channel_capability *owner)
{
    const ams_mel_ir_channel_capability_v1 *value = NULL;
    CHECK(ams_mel_ir_channel_capability_view(owner, &value, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(value != NULL);
    return value;
}

/* The complete owned snapshot of the mock Scheduling capability: shared rich
 * fields plus the Scheduling channel types, TaskScheduleDepth and the exact
 * (ordered, set-derived) Scheduling metadata capabilities. */
static void check_scheduling_capability(const ams_mel_ir_channel_capability_v1 *value,
                                        uint32_t depth)
{
    static const uint32_t metadata[] = {
        AMS_MEL_IR_METADATA_TASK_EXECUTING_REP, AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK,
        AMS_MEL_IR_METADATA_SCHED_CREATED_REP, AMS_MEL_IR_METADATA_CHANNEL_COMMS_TEST_REP,
        AMS_MEL_IR_METADATA_UPDATE_TRACK_LIST_RESPONSE, AMS_MEL_IR_METADATA_TASK_EVENTS,
        AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT};
    size_t index;
    CHECK(value->channel_types.size == 2U);
    CHECK(value->channel_types.data[0] == AMS_MEL_IR_CHANNEL_SCHEDULING);
    CHECK(value->channel_types.data[0] == 3U);
    CHECK(value->channel_types.data[1] == AMS_MEL_IR_CHANNEL_RESERVED_1);
    CHECK(value->task_schedule_depth == depth);
    CHECK(value->metadata_capabilities.size == sizeof metadata / sizeof *metadata);
    for (index = 0; index < sizeof metadata / sizeof *metadata; ++index)
        CHECK(value->metadata_capabilities.data[index] == metadata[index]);
    CHECK(value->channel_id.uuid[0] == 0x11U && value->platform_id.uuid[0] == 0x31U);
    CHECK(same_text(value->channel_id.descriptive_label, "channel-\xCE\xB1"));
    CHECK(same_text(value->platform_id.descriptive_label, "platform-\xE2\x82\xAC"));
    CHECK(value->height == 1080U && value->width == 1920U && value->bit_depth == 12U);
    CHECK(value->row_pitch == 4096U && value->buffer_size == 8388608U);
    CHECK(value->image_size == 4147200U && value->number_of_bands == 3U);
    CHECK(value->pixel_format == AMS_MEL_IR_PIXEL_RGB);
    CHECK(value->sensor_types.size == 2U);
    CHECK(value->sensor_location.offset_x_m == 1.25 && value->sensor_location.offset_y_m == -2.5);
    CHECK(value->sensor_location.offset_z_m == 3.75);
    CHECK(same_text(value->sensor_location.key, "sensor-key"));
    CHECK(same_text(value->sensor_location.system_name, "system-\xCE\xB2"));
    CHECK(value->odc_available == 1U && value->nuc_available == 1U);
    CHECK(value->image_bands.size == 2U && value->image_bands.data[0].bands.size == 2U);
    CHECK(value->image_bands.data[0].band_index == 2U && value->image_bands.data[1].band_index == 9U);
    CHECK(value->nav_frames.size == 2U);
}

static ams_mel_ir_channel_capability *typed_capability(ams_mel_ir_scheduling *scheduling)
{
    ams_mel_ir_channel_capability *owner = NULL;
    CHECK(ams_mel_ir_scheduling_get_capabilities(scheduling, &owner, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(owner != NULL);
    return owner;
}

static void expect_capability_refused(ams_mel_ir_scheduling *scheduling)
{
    ams_mel_ir_channel_capability *owner = NULL;
    CHECK(ams_mel_ir_scheduling_get_capabilities(scheduling, &owner, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(owner == NULL);
}
/* Valid complete identities/location, Scheduling type 3 propagation (the mock
 * throws on any conversion mismatch), Attached capabilities, Enable,
 * idempotent Enable, Enabled capabilities, and Close after Enable with the
 * exact disable -> detach -> destruction -> unload order. */
static void scenario_open_enable_close(void)
{
    ams_mel_session *session = open_session("sched-config", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel_capability *owner;
    CHECK(count("scheduling_attach_requested") == 1U);
    CHECK(count("scheduling_config_verified") == 1U);
    /* Open returns Attached, not Enabled. */
    CHECK(count("scheduling_enabled") == 0U);
    owner = typed_capability(scheduling);
    check_scheduling_capability(viewed(owner), 23U);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_enabled") == 1U);
    owner = typed_capability(scheduling);
    /* The snapshot outlives typed owner, Session and provider unload. */
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(count("scheduling_disabled") == 1U);
    CHECK(before("scheduling_enabled", "scheduling_disabled"));
    CHECK(before("scheduling_disabled", "channel_detached"));
    CHECK(before("channel_detached", "scheduling_channel_destroyed"));
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(before("scheduling_channel_destroyed", "control_destroyed"));
    finish(1);
    check_scheduling_capability(viewed(owner), 23U);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
}

/* Close before Enable never calls the inherited disable(). */
static void scenario_close_before_enable(void)
{
    ams_mel_session *session = open_session("sched-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(count("scheduling_enabled") == 0U && count("scheduling_disabled") == 0U);
    CHECK(count("channel_detached") == 1U && count("scheduling_channel_destroyed") == 1U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

/* TaskScheduleDepth is a verbatim scalar: 0 (application-managed priority
 * mode) still opens and is reported as 0; UINT32_MAX is not narrowed. */
static void scenario_depth(const char *scenario, uint32_t depth)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel_capability *owner = typed_capability(scheduling);
    check_scheduling_capability(viewed(owner), depth);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    owner = typed_capability(scheduling);
    check_scheduling_capability(viewed(owner), depth);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

/* The parent Session closes first; the Scheduling owner keeps the provider
 * graph and library alive, remains fully usable, and its own Close unloads. */
static void scenario_parent_first(void)
{
    ams_mel_session *session = open_session("sched-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel_capability *owner;
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK && !session);
    CHECK(count("control_destroyed") == 0U && count("manager_destroyed") == 0U);
    owner = typed_capability(scheduling);
    check_scheduling_capability(viewed(owner), 23U);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(before("scheduling_disabled", "channel_detached"));
    CHECK(before("scheduling_channel_destroyed", "control_destroyed"));
    CHECK(before("control_destroyed", "manager_destroyed"));
    finish(1);
}
/* Enable refusal and provider exception: Failed lifecycle, no silent retry,
 * capabilities refused, and Close still disables (enable was attempted). */
static void scenario_enable_failure(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    char diagnostic[96] = {0};
    CHECK(ams_mel_ir_scheduling_enable(scheduling, diagnostic, sizeof diagnostic, NULL) ==
          expected);
    CHECK(diagnostic[0] != '\0');
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(count("scheduling_enabled") == 1U);
    expect_capability_refused(scheduling);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(count("scheduling_disabled") == 1U && count("channel_detached") == 1U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

/* Provider disable refusal/exception: detach is still attempted; a proven
 * detach consumes the owner but the failure is reported. */
static void scenario_disable_failure(const char *scenario)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(scheduling == NULL);
    CHECK(count("scheduling_disabled") == 1U && count("channel_detached") == 1U);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

/* Failed detach: owner non-null, graph retained (no destruction, no unload
 * even after Session Close), refused while logically closed; the retry
 * detaches and unloads. Disable is not repeated on retry. */
static void scenario_detach_failure(const char *scenario, int enable)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    char diagnostic[96] = {0};
    if (enable) CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, diagnostic, sizeof diagnostic, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(!strcmp(diagnostic, "Scheduling detach failed; provider state retained"));
    CHECK(scheduling != NULL);
    CHECK(count("channel_detach_failed") == 1U && count("scheduling_channel_destroyed") == 0U);
    expect_capability_refused(scheduling);
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("control_destroyed") == 0U && count("library_unloaded") == 0U);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(count("scheduling_disabled") == (enable ? 1U : 0U));
    CHECK(before("channel_detached", "scheduling_channel_destroyed"));
    CHECK(before("scheduling_channel_destroyed", "control_destroyed"));
    finish(1);
}
/* One Open failure: exact status, NULL owner, number of attach attempts, and
 * a Session that remains usable afterwards (Scheduling is conditional; its
 * absence is not a fatal bridge failure). */
static void open_failure(const char *scenario, ams_mel_status_t expected,
                         unsigned attaches, unsigned destroyed, int retained)
{
    ams_mel_session *session = open_session(scenario, 4U);
    ams_mel_ir_scheduling_config_v1 config = configuration();
    ams_mel_ir_scheduling *scheduling = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_config_v1 c2_config;
    char diagnostic[128] = {0};
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, diagnostic,
          sizeof diagnostic, NULL) == expected);
    CHECK(scheduling == NULL && diagnostic[0] != '\0');
    CHECK(count("scheduling_attach_requested") == attaches);
    /* An incompatible Scheduling channel is destroyed only after a proven
     * detach; an ownership-uncertain one never is. */
    CHECK(count("scheduling_channel_destroyed") == destroyed);
    CHECK(count("channel_detach_failed") == (retained ? 1U : 0U));
    /* The same Session still serves another family. */
    memset(&c2_config, 0, sizeof c2_config);
    c2_config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    CHECK(ams_mel_ir_c2_open(session, &c2_config, &c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* A retained (ownership-uncertain) provider graph keeps the library. */
    finish(!retained);
}

static void scenario_open_failures(void)
{
    /* Not advertised: refused before any attachment. */
    open_failure("sched-not-advertised", AMS_MEL_INITIALIZATION_FAILED, 0U, 0U, 0);
    open_failure("sched-attach-null", AMS_MEL_FACTORY_FAILED, 1U, 0U, 0);
    open_failure("sched-attach-throw", AMS_MEL_PROVIDER_EXCEPTION, 1U, 0U, 0);
    /* Wrong concrete type (a HealthStatusChannel); rollback detach succeeds. */
    open_failure("sched-wrong-concrete", AMS_MEL_INITIALIZATION_FAILED, 1U, 0U, 0);
    /* Concrete SchedulingChannel whose capability omits Scheduling. */
    open_failure("sched-capability-wrong", AMS_MEL_INITIALIZATION_FAILED, 1U, 1U, 0);
    /* Provider exception from the attached channel's getCapabilities. */
    open_failure("sched-capability-throw", AMS_MEL_INITIALIZATION_FAILED, 1U, 1U, 0);
}

/* Ownership-uncertain rollback: the graph is permanently retained and the
 * provider library never unloads. Each runs in its own process. */
static void scenario_open_detach_failure(void)
{
    open_failure("sched-open-detach-fail", AMS_MEL_PROVIDER_FAILED, 1U, 0U, 1);
}
static void scenario_open_exception_detach_failure(void)
{
    open_failure("sched-capability-throw-detach-fail", AMS_MEL_PROVIDER_FAILED, 1U, 0U, 1);
}

/* Argument validation never reaches the provider. */
static void scenario_invalid(void)
{
    ams_mel_session *session = open_session("sched-basic", 4U);
    ams_mel_ir_scheduling_config_v1 config = configuration();
    ams_mel_ir_scheduling *scheduling = NULL, *sentinel = (ams_mel_ir_scheduling *)&config;
    ams_mel_ir_channel_capability *owner = NULL;
    static const char bad[] = {'b', (char)0xC3, '(', 0};
    const unsigned lines = count(NULL);
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && !scheduling);
    config.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    config.platform_id.descriptive_label = text(bad);
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && !scheduling);
    config = configuration();
    config.sensor_location.key.data = NULL;
    config.sensor_location.key.size = 1U;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && !scheduling);
    config = configuration();
    CHECK(ams_mel_ir_scheduling_open(NULL, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_open(session, NULL, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_open(session, &config, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    scheduling = sentinel;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && scheduling == sentinel);
    scheduling = NULL;
    CHECK(ams_mel_ir_scheduling_open(session, &config, &scheduling, NULL, 4U, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && !scheduling);
    CHECK(ams_mel_ir_scheduling_enable(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_get_capabilities(NULL, &owner, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT && !owner);
    CHECK(ams_mel_ir_scheduling_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(NULL) == lines);
    scheduling = open_scheduling(session);
    CHECK(ams_mel_ir_scheduling_get_capabilities(scheduling, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    owner = (ams_mel_ir_channel_capability *)&config;
    CHECK(ams_mel_ir_scheduling_get_capabilities(scheduling, &owner, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    owner = NULL;
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}
static size_t scheduling_requests(const ams_mel_ir_scheduling *scheduling)
{
    size_t requests = 99U;
    CHECK(ams_mel_test_scheduling_requests(scheduling, &requests));
    return requests;
}

static void expired(const ams_mel_ir_channel *view)
{
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *reply = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_channel_get_capabilities(view, &capability, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(!ret && !reply && !capability);
}

/* Weak view lifecycle, typed/weak capability equality, KeepAlive and
 * CommsTest with cached repeated Wait, while Attached and while Enabled.
 * Provider futures are held by the deterministic completion gate. */
static void scenario_common(void)
{
    ams_mel_session *session = open_session("sched-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_channel_capability *generic = NULL, *typed = NULL;
    uint64_t returns = 0U, comms = 0U, observed = 0U;
    int enabled;
    const unsigned lines = count(NULL);
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    /* View creation makes no provider call. */
    CHECK(view != NULL && count(NULL) == lines);
    for (enabled = 0; enabled < 2; ++enabled) {
        ams_mel_ir_return_request *ret = NULL;
        ams_mel_ir_channel_comms_request *reply = NULL;
        ams_mel_ir_return_result_v1 result = {99U, 99U};
        ams_mel_ir_channel_comms_test_result_v1 report = {0U, 0U, 99U};
        if (enabled) CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_get_capabilities(view, &generic, NULL, 0, NULL) == AMS_MEL_OK);
        typed = typed_capability(scheduling);
        CHECK(generic != typed);
        check_scheduling_capability(viewed(generic), 23U);
        check_scheduling_capability(viewed(typed), 23U);
        CHECK(ams_mel_ir_channel_capability_close(&generic, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_capability_close(&typed, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
        ++returns;
        CHECK(scheduling_requests(scheduling) == 1U);
        /* Timeout is not cancellation: the request stays pending. */
        CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(result.value == 99U);
        CHECK(release_family(1U, 1U));
        CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS && result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK && !ret);
        CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &reply, NULL, 0, NULL) ==
              AMS_MEL_OK && reply);
        ++comms;
        CHECK(release_family(2U, 1U));
        CHECK(ams_mel_ir_channel_comms_request_wait(reply, 15000U, &report, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(report.command_id == 0x80000001U && report.request_id == 0xe0000003U);
        memset(&report, 0, sizeof report);
        /* Cached terminal result. */
        CHECK(ams_mel_ir_channel_comms_request_wait(reply, 0U, &report, NULL, 0, NULL) ==
              AMS_MEL_OK);
        CHECK(report.command_id == 0x80000001U && report.request_id == 0xe0000003U);
        CHECK(report.error_code == AMS_MEL_ERROR_NONE);
        CHECK(ams_mel_ir_channel_comms_request_close(&reply, NULL, 0, NULL) == AMS_MEL_OK);
        /* Completion-owner quiescence before checking the count again. */
        CHECK(ams_mel_test_completion_owner(1U, returns, &observed));
        CHECK(ams_mel_test_completion_owner(2U, comms, &observed));
        CHECK(scheduling_requests(scheduling) == 0U);
    }
    CHECK(count("keepalive_sent") == 2U && count("comms_sent") == 2U);
    /* Closing the view leaves the typed owner fully usable. */
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    typed = typed_capability(scheduling);
    CHECK(ams_mel_ir_channel_capability_close(&typed, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_channel_destroyed") == 0U);
    /* A new view after typed Close is refused: no admission, no send. */
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    /* The weak view did not keep the typed owner's provider channel alive. */
    CHECK(count("scheduling_channel_destroyed") == 1U);
    expired(view);
    CHECK(count("keepalive_sent") == 2U && count("comms_sent") == 2U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(1U, returns, &observed));
    CHECK(ams_mel_test_completion_owner(2U, comms, &observed));
    finish(1);
}
/* An admitted KeepAlive survives view Close, typed Scheduling Close and
 * Session Close; the Session admission limit of one refuses a second request;
 * typed Close is deferred (no disable/detach/destruction) until the final
 * admitted request completes; then the deferred cleanup runs exactly once and
 * the provider library unloads only after the request owner is gone. */
static void scenario_pending(void)
{
    ams_mel_session *session = open_session("sched-basic", 1U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *ret = NULL, *refused = NULL;
    ams_mel_ir_channel_comms_request *refused_comms = NULL;
    ams_mel_test_scheduling_observer *observer = NULL;
    ams_mel_test_admission_token *token = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    uint32_t active = 99U, maximum = 0U;
    uint64_t observed = 0U;
    size_t requests = 99U;
    int alive = 0;
    CHECK(ams_mel_ir_scheduling_enable(scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(session, 0U, &token, NULL, NULL));
    CHECK(ams_mel_test_scheduling_observer_from(scheduling, &observer));
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) == AMS_MEL_OK && ret);
    CHECK(ams_mel_test_completion_wait(1U, 4U, 1U));
    /* Admission limit: refused before any provider call. */
    CHECK(ams_mel_ir_channel_send_keepalive(view, &refused, NULL, 0, NULL) ==
          AMS_MEL_RESOURCE_EXHAUSTED && !refused);
    CHECK(ams_mel_ir_channel_submit_comms_test(view, &high_ids, &refused_comms, NULL, 0, NULL) ==
          AMS_MEL_RESOURCE_EXHAUSTED && !refused_comms);
    CHECK(count("keepalive_sent") == 1U && count("comms_sent") == 0U);
    CHECK(scheduling_requests(scheduling) == 1U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK && !view);
    /* Typed Close returns OK and clears the owner, but is deferred. */
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK && !scheduling);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK && !session);
    CHECK(ams_mel_test_scheduling_observer_state(observer, &alive, &requests));
    CHECK(alive == 1 && requests == 1U);
    CHECK(count("scheduling_disabled") == 0U && count("channel_detached") == 0U);
    CHECK(count("scheduling_channel_destroyed") == 0U && count("control_destroyed") == 0U);
    CHECK(ams_mel_test_admission(NULL, 1U, &token, &active, &maximum));
    CHECK(active == 1U && maximum == 1U);
    /* Timeout is not cancellation, and closing the owners cancelled nothing. */
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(release_family(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS && result.error_code == AMS_MEL_ERROR_NONE);
    /* Quiesce: one admitted owner plus one refused preallocation per engine. */
    CHECK(ams_mel_test_completion_owner(1U, 2U, &observed));
    CHECK(ams_mel_test_completion_owner(2U, 1U, &observed));
    CHECK(ams_mel_test_admission(NULL, 1U, &token, &active, &maximum) && active == 0U);
    /* Deferred cleanup ran exactly once, in order, after the completion. */
    CHECK(count("scheduling_disabled") == 1U && count("channel_detached") == 1U);
    CHECK(count("scheduling_channel_destroyed") == 1U && count("control_destroyed") == 1U);
    CHECK(before("keepalive_sent", "scheduling_disabled"));
    CHECK(before("scheduling_disabled", "channel_detached"));
    CHECK(before("channel_detached", "scheduling_channel_destroyed"));
    CHECK(before("scheduling_channel_destroyed", "control_destroyed"));
    CHECK(ams_mel_test_scheduling_observer_state(observer, &alive, &requests) && alive == 0);
    /* The cached result remains readable after all owners are gone. */
    CHECK(ams_mel_ir_return_request_wait(ret, 0U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK && !ret);
    ams_mel_test_scheduling_observer_close(&observer);
    CHECK(ams_mel_test_admission(NULL, 2U, &token, NULL, NULL));
    finish(1);
}
/* Scheduling request accounting is separate from Track on the same Session.
 * The provider gate releases the oldest pending family-1 future first, which
 * is deterministically the Scheduling KeepAlive submitted first. */
static void scenario_accounting(void)
{
    ams_mel_session *session = open_session("sched-basic", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_config_v1 track_config;
    ams_mel_ir_channel *sched_view = NULL, *track_view = NULL;
    ams_mel_ir_return_request *sched_ret = NULL, *track_ret = NULL;
    ams_mel_ir_return_result_v1 result = {99U, 99U};
    uint64_t observed = 0U;
    size_t requests = 99U;
    memset(&track_config, 0, sizeof track_config);
    track_config.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    CHECK(ams_mel_ir_track_open(session, &track_config, &track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &sched_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_track(track, &track_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(sched_view, &sched_ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(scheduling_requests(scheduling) == 1U);
    CHECK(ams_mel_test_track_requests(track, &requests) && requests == 0U);
    CHECK(ams_mel_ir_channel_send_keepalive(track_view, &track_ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(scheduling_requests(scheduling) == 1U);
    CHECK(ams_mel_test_track_requests(track, &requests) && requests == 1U);
    CHECK(release_family(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(sched_ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(1U, 1U, &observed));
    CHECK(scheduling_requests(scheduling) == 0U);
    CHECK(ams_mel_test_track_requests(track, &requests) && requests == 1U);
    /* A pending Track request does not defer Scheduling teardown. */
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_channel_destroyed") == 1U && count("track_channel_destroyed") == 0U);
    CHECK(release_family(1U, 1U));
    CHECK(ams_mel_ir_return_request_wait(track_ret, 15000U, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(1U, 2U, &observed));
    CHECK(ams_mel_ir_return_request_close(&sched_ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&track_ret, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&sched_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&track_view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

/* Provider KeepAlive send exception through a Scheduling view: the claim is
 * finished immediately, accounting returns to zero, Close is not deferred. */
static void scenario_send_exception(void)
{
    ams_mel_session *session = open_session("sched-keepalive-throw", 4U);
    ams_mel_ir_scheduling *scheduling = open_scheduling(session);
    ams_mel_ir_channel *view = NULL;
    ams_mel_ir_return_request *ret = NULL;
    uint64_t observed = 0U;
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(view, &ret, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_EXCEPTION && !ret);
    CHECK(ams_mel_test_completion_owner(1U, 1U, &observed));
    CHECK(scheduling_requests(scheduling) == 0U);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count("scheduling_channel_destroyed") == 1U);
    CHECK(ams_mel_ir_channel_close(&view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    finish(1);
}

int main(int argc, char **argv)
{
    const char *name;
    CHECK(argc == 2);
    name = argv[1];
    if (!strcmp(name, "open-enable-close")) scenario_open_enable_close();
    else if (!strcmp(name, "close-before-enable")) scenario_close_before_enable();
    else if (!strcmp(name, "depth-zero")) scenario_depth("sched-depth-zero", 0U);
    else if (!strcmp(name, "depth-max")) scenario_depth("sched-depth-max", UINT32_MAX);
    else if (!strcmp(name, "parent-first")) scenario_parent_first();
    else if (!strcmp(name, "enable-fail"))
        scenario_enable_failure("sched-enable-fail", AMS_MEL_PROVIDER_FAILED);
    else if (!strcmp(name, "enable-throw"))
        scenario_enable_failure("sched-enable-throw", AMS_MEL_PROVIDER_EXCEPTION);
    else if (!strcmp(name, "disable-fail")) scenario_disable_failure("sched-disable-fail");
    else if (!strcmp(name, "disable-throw")) scenario_disable_failure("sched-disable-throw");
    else if (!strcmp(name, "detach-fail")) scenario_detach_failure("sched-detach-fail", 0);
    else if (!strcmp(name, "enabled-detach-fail"))
        scenario_detach_failure("sched-enabled-detach-fail", 1);
    else if (!strcmp(name, "open-failures")) scenario_open_failures();
    else if (!strcmp(name, "open-detach-fail")) scenario_open_detach_failure();
    else if (!strcmp(name, "open-exception-detach-fail")) scenario_open_exception_detach_failure();
    else if (!strcmp(name, "invalid")) scenario_invalid();
    else if (!strcmp(name, "common")) scenario_common();
    else if (!strcmp(name, "pending")) scenario_pending();
    else if (!strcmp(name, "accounting")) scenario_accounting();
    else if (!strcmp(name, "send-exception")) scenario_send_exception();
    else { fprintf(stderr, "unknown scenario: %s\n", name); return 2; }
    printf("PASS: native IR Scheduling foundation %s\n", name);
    return 0;
}
