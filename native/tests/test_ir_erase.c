#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-007: compiled-C contract for C2Channel::send(EraseCommand), against the
 * deterministic mock provider. Concurrency cases use the provider's promise
 * gate, file barriers and test-only admission/probe observers, never sleeps
 * as a synchronization decision. */
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#include "admission_observation.h"

extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)
#define ERASE_PROBE_FAMILY 10U
#define ERASE_SEND 11U

/* Pre-existing frozen v1 layouts: CameraCommand (IRMEL-ADA-003), NUC
 * (IRMEL-ADA-002), counters, mode/navigation results. */
_Static_assert(sizeof(ams_mel_ir_camera_command_v1) == 56, "camera command size");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, system_time_ns) == 8, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, number_of_stages) == 52, "camera command");
_Static_assert(sizeof(ams_mel_ir_camera_command_response_v1) == 24, "camera response size");
_Static_assert(offsetof(ams_mel_ir_camera_command_response_v1, command_success) == 16, "camera response");
_Static_assert(sizeof(ams_mel_ir_camera_command_result_v1) == 32, "camera result size");
_Static_assert(offsetof(ams_mel_ir_camera_command_result_v1, error_code) == 24, "camera result");
_Static_assert(sizeof(ams_mel_ir_temp_point_v1) == 56, "nuc temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, point_type) == 48, "nuc temp point");
_Static_assert(sizeof(ams_mel_ir_pixel_value_v1) == 8, "nuc pixel value");
_Static_assert(offsetof(ams_mel_ir_nuc_temp_data_event_v1, temp_points) == 0, "nuc event");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "counters");
_Static_assert(sizeof(ams_mel_ir_mode_result_v1) == 8, "mode result");
_Static_assert(sizeof(ams_mel_ir_navigation_result_v1) == 24, "navigation result");
_Static_assert(AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ == 0 && AMS_MEL_IR_SCAN_DIR_POS_TO_NEG_AZ == 1, "ScanDir");
/* New fixed-width records; C11 compilation and release-active checks. */
_Static_assert(sizeof(ams_mel_ir_erase_command_v1) == 8, "command size");
_Static_assert(offsetof(ams_mel_ir_erase_command_v1, erase_type) == 4, "type offset");
_Static_assert(sizeof(ams_mel_ir_erase_result_v1) == 8, "result size");
_Static_assert(offsetof(ams_mel_ir_erase_result_v1, error_code) == 4, "error offset");
_Static_assert(_Generic(((ams_mel_ir_erase_command_v1 *)0)->command_id, uint32_t: 1, default: 0), "u32 ID");
_Static_assert(AMS_MEL_IR_ERASE_ZEROIZE == 0 && AMS_MEL_IR_ERASE_CLASSIFIED_DATA_ERASE == 1 &&
               AMS_MEL_IR_ERASE_DISABLE == 2 && AMS_MEL_IR_ERASE_CDE_AND_ZEROIZE == 3, "enum");

typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef int (*release_fn)(unsigned, unsigned);
typedef int (*last_fn)(uint64_t *, unsigned *);
static barrier_fn send_barrier;
static release_fn release_family;
static last_fn last_command;

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }
/* Resolves test-only provider controls; used only while a Session (or a
 * pending request graph) keeps the provider loaded. */
static int bind_provider(void)
{
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    *(void **)(&send_barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    *(void **)(&last_command) = dlsym(library, "mock_erase_last_command");
    CHECK(send_barrier && release_family && last_command);
    CHECK(dlclose(library) == 0);
    return EXIT_SUCCESS;
}

struct owners {
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
};

/* enable = 1: Enabled C2 (send-eligible); 0: Attached only. */
static int open_owner(const char *scenario, uint32_t limit, int enable, struct owners *o)
{
    ams_mel_ir_c2_config_v1 value;
    memset(o, 0, sizeof *o);
    if (limit) {
        ams_mel_session_options_v1 options = {limit};
        CHECK(ams_mel_session_open_with_options(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
              &options, &o->session, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &o->session,
                                   NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(bind_provider() == EXIT_SUCCESS);
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    value.channel_id.descriptive_label = text("c2");
    value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    CHECK(ams_mel_ir_c2_open(o->session, &value, &o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (enable) CHECK(ams_mel_ir_c2_enable(o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_owner(struct owners *o)
{
    if (o->c2) CHECK(ams_mel_ir_c2_close(&o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (o->session) CHECK(ams_mel_session_close(&o->session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static ams_mel_ir_erase_command_v1 command(uint32_t id, uint32_t operation)
{
    ams_mel_ir_erase_command_v1 value;
    memset(&value, 0, sizeof value);
    value.command_id = id;
    value.erase_type = operation;
    return value;
}

static unsigned sends(void)
{
    unsigned count = 0;
    (void)last_command(NULL, &count);
    return count;
}

static int delivered(const ams_mel_ir_erase_command_v1 *c)
{
    uint64_t values[3] = {0};
    CHECK(last_command(values, NULL) == 1);
    CHECK(values[0] == c->command_id);
    CHECK(values[1] == c->erase_type);
    return EXIT_SUCCESS;
}

static ams_mel_status_t submit(struct owners *o, const ams_mel_ir_erase_command_v1 *c,
                               ams_mel_ir_erase_request **request, char *d, size_t size)
{
    return ams_mel_ir_c2_submit_erase(o->c2, c, request, d, size, NULL);
}

static int request_count(const struct owners *o, size_t *count)
{
    return ams_mel_test_c2_requests(o->c2, count);
}
/* Every field round-trips exactly through C2Channel::send(EraseCommand)
 * for all four operations and uint32 boundaries; independent
 * requests keep their identities through repeated cached waits. */
static int test_fidelity(void)
{
    struct owners o;
    ams_mel_ir_erase_command_v1 cases[4];
    ams_mel_ir_erase_request *requests[4] = {NULL, NULL, NULL, NULL};
    CHECK(open_owner("erase-echo", 0, 1, &o) == EXIT_SUCCESS);
    cases[0] = command(UINT32_C(0xffffffff), 1U);
    cases[1] = command(0U, 0U);
    cases[2] = command(UINT32_C(0x80000001), 2U);
    cases[3] = command(7U, 3U);
    for (unsigned i = 0; i < 4; ++i) {
        ams_mel_ir_erase_result_v1 result;
        memset(&result, 0xa5, sizeof result);
        CHECK(submit(&o, &cases[i], &requests[i], NULL, 0) == AMS_MEL_OK);
        CHECK(delivered(&cases[i]) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_erase_request_wait(requests[i], 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(result.erase_type == cases[i].erase_type);
    }
    for (unsigned i = 0; i < 4; ++i) {
        ams_mel_ir_erase_result_v1 result;
        CHECK(ams_mel_ir_erase_request_wait(requests[i], 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.erase_type == cases[i].erase_type);
        CHECK(ams_mel_ir_erase_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(requests[i] == NULL);
        CHECK(ams_mel_ir_erase_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* All four EraseCommandType alternatives are successful completions, preserved
 * exactly, and distinct from MEL rejection. */
static int test_alternatives(void)
{
    for (uint32_t alternative = 0; alternative < 4; ++alternative) {
        struct owners o;
        ams_mel_ir_erase_request *request = NULL;
        ams_mel_ir_erase_result_v1 result;
        char scenario[32];
        const ams_mel_ir_erase_command_v1 c = command(0x80000001U, (alternative + 1U) % 4U);
        CHECK(snprintf(scenario, sizeof scenario, "erase-response-%u", alternative) > 0);
        CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
        CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(delivered(&c) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_erase_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.erase_type == alternative && result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
static int expect_outcome(const char *scenario, ams_mel_status_t status,
                          ams_mel_error_code_t code, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    ams_mel_ir_erase_result_v1 result;
    char d[1024];
    const ams_mel_ir_erase_command_v1 c = command(7U, 0U);
    CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_erase_request_wait(request, 5000, &result, d, sizeof d, NULL) == status);
    CHECK(result.error_code == code);
    CHECK(result.erase_type == 0U);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_erase_request_wait(request, 0, &result, d, sizeof d, NULL) == status);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Explicit MEL rejection versus success and every failure category. */
static int test_outcomes(void)
{
    /* Unsupported optional operation is an explicit MEL rejection. */
    CHECK(expect_outcome("no-erase", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_UNSUPPORTED, "") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-reject", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS, "erase rejected") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-reject-nul", AMS_MEL_PROVIDER_FAILED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS,
                         "provider rejection description was invalid UTF-8 or contained NUL") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-reject-utf8", AMS_MEL_PROVIDER_FAILED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS,
                         "provider rejection description was invalid UTF-8 or contained NUL") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-unknown-error", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown MEL error code") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-response-99", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown EraseCommandType value") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-response-4", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown EraseCommandType value") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-response-4294967295", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown EraseCommandType value") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-null", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned null successful EraseCommandType") == EXIT_SUCCESS);
    CHECK(expect_outcome("erase-future-throw", AMS_MEL_PROVIDER_EXCEPTION, 0,
                         "mock erase future exception") == EXIT_SUCCESS);
    {   /* Long rejection: required size reports the complete description. */
        struct owners o;
        ams_mel_ir_erase_request *request = NULL;
        ams_mel_ir_erase_result_v1 result;
        char small[16];
        size_t required = 0;
        const ams_mel_ir_erase_command_v1 c = command(8U, 1U);
        CHECK(open_owner("erase-reject-long", 0, 1, &o) == EXIT_SUCCESS);
        CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(ams_mel_ir_erase_request_wait(request, 5000, &result, small, sizeof small,
              &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INSUFFICIENT_RESOURCES && required == 614U);
        {
            char complete[614]; size_t again = 0;
            CHECK(ams_mel_ir_erase_request_wait(request, 0, &result, complete, sizeof complete,
                  &again) == AMS_MEL_COMMAND_REJECTED);
            CHECK(again == required && strlen(complete) == 613U);
            for (size_t i = 0; i < 510U; ++i) CHECK(complete[i] == 'x');
            CHECK(memcmp(complete + 510U, "\xe2\x82\xac", 3) == 0);
            for (size_t i = 513U; i < 613U; ++i) CHECK(complete[i] == 'y');
        }
        CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
/* Unknown enum representations and invalid arguments are rejected before send
 * and consume no accounting slot. */
static int test_invalid(void)
{
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    ams_mel_ir_erase_request *stale = (ams_mel_ir_erase_request *)(void *)&o;
    ams_mel_ir_erase_command_v1 c = command(1U, 1U);
    ams_mel_ir_erase_result_v1 result;
    size_t count = 99;
    char d[64];
    unsigned before;
    CHECK(open_owner("erase-echo", 0, 1, &o) == EXIT_SUCCESS);
    before = sends();
    c.erase_type = 4U;
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(strcmp(d, "invalid EraseCommand input") == 0 && request == NULL);
    c.erase_type = UINT32_MAX; CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.erase_type = 0x100U; CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.erase_type = 1U;
    CHECK(submit(&o, NULL, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, NULL, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, &stale, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, &request, NULL, 4) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_erase(NULL, &c, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(sends() == before);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(ams_mel_ir_erase_request_wait(NULL, 0, &result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_erase_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_erase_request_close(&request, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Send exception and an invalid provider future are distinct failures and
 * release their accounting slot; the channel must be Enabled. */
static int test_send_failures_and_lifecycle(void)
{
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    const ams_mel_ir_erase_command_v1 c = command(2U, 1U);
    size_t count = 99;
    char d[128];
    unsigned before;
    CHECK(open_owner("erase-send-throw", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strcmp(d, "mock erase send exception") == 0 && request == NULL);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(open_owner("erase-invalid-future", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "provider returned an invalid EraseCommand future") == 0 && request == NULL);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    /* Attached (not Enabled): refused with no provider call. */
    CHECK(open_owner("erase-echo", 0, 0, &o) == EXIT_SUCCESS);
    before = sends();
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "C2 channel is not enabled") == 0 && request == NULL);
    CHECK(sends() == before);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static uint64_t owners_destroyed(void)
{
    uint64_t values[4] = {0};
    if (!ams_mel_test_completion_boundary(ERASE_PROBE_FAMILY, 3, 0, values)) return UINT64_MAX;
    return values[3];
}

static unsigned occurrences(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[192];
    unsigned count = 0;
    if (!file) return 0;
    while (fgets(line, sizeof line, file))
        if (strncmp(line, event, strlen(event)) == 0 && line[strlen(event)] == '\n') ++count;
    fclose(file);
    return count;
}

static int log_order(const char *path, const char *const *names, unsigned count)
{
    char data[16384];
    size_t size;
    const char *previous = data;
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    size = fread(data, 1, sizeof data - 1U, file);
    CHECK(fclose(file) == 0);
    data[size] = '\0';
    for (unsigned i = 0; i < count; ++i) {
        const char *found = strstr(previous, names[i]);
        if (!found) fprintf(stderr, "missing/out of order: %s\n", names[i]);
        CHECK(found != NULL);
        previous = found + strlen(names[i]);
    }
    return EXIT_SUCCESS;
}

static int make_log(char *path)
{
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    return EXIT_SUCCESS;
}

/* Timeout does not consume; repeated terminal Wait is cached; the graph
 * (and its admission permit) is released exactly once after completion. */
static int test_timeout_then_success(void)
{
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    ams_mel_ir_erase_result_v1 result;
    const ams_mel_ir_erase_command_v1 c = command(0x1234U, 1U);
    size_t count = 0;
    uint64_t base, observed = 0;
    uint64_t values[4] = {0};
    /* Quiesce every earlier Erase owner (closed set) before recording
     * the base, so only this request's teardown can satisfy base + 1. */
    CHECK(ams_mel_test_completion_boundary(ERASE_PROBE_FAMILY, 3, 0, values));
    CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, values[2], &observed));
    CHECK(observed == values[2]);
    base = observed;
    CHECK(open_owner("erase-held", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(request_count(&o, &count) && count == 1U);
    result.erase_type = UINT32_MAX; result.error_code = UINT32_MAX;
    CHECK(ams_mel_ir_erase_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(result.erase_type == UINT32_MAX && result.error_code == UINT32_MAX);
    CHECK(ams_mel_ir_erase_request_wait(request, 1, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(release_family(ERASE_SEND, 1) == 1);
    CHECK(ams_mel_ir_erase_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_erase_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, base + 1U, &observed));
    CHECK(observed == base + 1U);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Request Close, C2 Close and Session Close all precede completion: the
 * provider channel and library stay alive until the future completes;
 * nothing is cancelled; teardown order is preserved. */
static int test_pending_close(void)
{
    char log[] = "/tmp/ams-erase-pending-XXXXXX";
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    ams_mel_ir_erase_request *observer = NULL;
    ams_mel_ir_erase_result_v1 result;
    const ams_mel_ir_erase_command_v1 c = command(0xabcU, 0U);
    uint64_t base, observed = 0;
    uint64_t values[4] = {0};
    size_t count = 0;
    void *pin;
    /* Same quiescence as IR-CAMERA-CI-001: earlier Wait calls publish results
     * before their detached workers drop the final owner, so drain every
     * earlier Erase owner first; otherwise an unrelated teardown could
     * satisfy base + 2. No other thread submits here: created is a closed set. */
    CHECK(ams_mel_test_completion_boundary(ERASE_PROBE_FAMILY, 3, 0, values));
    CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, values[2], &observed));
    CHECK(observed == values[2]);
    base = observed;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("erase-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(submit(&o, &c, &observer, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_boundary(ERASE_PROBE_FAMILY, 3, 0, values));
    CHECK(values[2] == base + 2U && values[3] == base);
    CHECK(request_count(&o, &count) && count == 2U);
    CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_erase_request_wait(observer, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "control_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(release_family(ERASE_SEND, 2) == 1);
    CHECK(ams_mel_ir_erase_request_wait(observer, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, base + 2U, &observed));
    CHECK(observed == base + 2U);
    CHECK(occurrences(log, "c2_channel_destroyed") == 1U);
    CHECK(ams_mel_ir_erase_request_close(&observer, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    {
        const char *order[] = {"c2_erase_sent", "c2_channel_destroyed",
                               "control_destroyed", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(occurrences(log, "library_unloaded") == 1U);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}
/* This Session's own admission slot count, read through a test-only token. */
static int admission_is(ams_mel_test_admission_token **token, uint32_t expected)
{
    uint32_t active = 99, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 1, token, &active, &maximum));
    CHECK(active == expected && maximum == 1U);
    return EXIT_SUCCESS;
}

/* Session-specific deterministic wait (IRMEL-ADA-003 correction): waits for
 * this Session's slot, not a family-global worker count, to be released
 * exactly once; the count is 0, never wrapped. */
static int admission_released(ams_mel_test_admission_token **token)
{
    uint32_t active = 0, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 3, token, &active, &maximum));
    CHECK(active == 0U && maximum == 1U);
    return EXIT_SUCCESS;
}

static int refuse(struct owners *o)
{
    ams_mel_ir_erase_request *refused = NULL;
    const ams_mel_ir_erase_command_v1 c = command(9U, 0U);
    const unsigned before = sends();
    char d[64];
    CHECK(submit(o, &c, &refused, d, sizeof d) == AMS_MEL_RESOURCE_EXHAUSTED);
    CHECK(strcmp(d, "async request limit reached") == 0 && refused == NULL);
    CHECK(sends() == before);
    return EXIT_SUCCESS;
}

/* Session admission is shared with every other request family. The permit
 * survives public request Close and is released exactly once, only after the
 * completion graph is gone. Negative control: a refused submission advances
 * the family-global FinalOwner count while this Session's slot is still
 * held, so that count can never prove release of this Session's permit. */
static int test_admission(void)
{
    struct owners o;
    ams_mel_ir_erase_request *first = NULL;
    ams_mel_ir_erase_result_v1 result;
    const ams_mel_ir_erase_command_v1 c = command(5U, 1U);
    ams_mel_test_admission_token *token = NULL;
    uint64_t base, observed = 0;
    size_t count = 0;
    CHECK(open_owner("erase-held", 1, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_admission(o.session, 0, &token, NULL, NULL));
    CHECK(admission_is(&token, 0U) == EXIT_SUCCESS);
    {   /* Quiesce: every earlier Erase graph in this process is gone. */
        uint64_t values[4] = {0};
        CHECK(ams_mel_test_completion_boundary(ERASE_PROBE_FAMILY, 3, 0, values));
        CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, values[2], &observed));
        base = owners_destroyed();
        CHECK(base == values[2]);
    }
    CHECK(submit(&o, &c, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(request_count(&o, &count) && count == 1U);
    CHECK(ams_mel_test_completion_owner(ERASE_PROBE_FAMILY, base + 1U, &observed));
    CHECK(observed == base + 1U);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_erase_request_wait(first, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Public request Close is not cancellation and does not release the slot. */
    CHECK(ams_mel_ir_erase_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(release_family(ERASE_SEND, 1) == 1);
    CHECK(admission_released(&token) == EXIT_SUCCESS);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(submit(&o, &c, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(release_family(ERASE_SEND, 1) == 1);
    CHECK(ams_mel_ir_erase_request_wait(first, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_erase_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(admission_released(&token) == EXIT_SUCCESS);
    CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL) && token == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
/* Isolated process: allocation or worker-launch failure after provider send
 * reports INTERNAL_ERROR, publishes no request and retains the accounted
 * future and provider graph rather than releasing or unloading it. */
static int isolated_post_send(const char *failure)
{
    char log[] = "/tmp/ams-erase-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    const ams_mel_ir_erase_command_v1 c = command(6U, 1U);
    size_t count = 0;
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("erase-held", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_ERASE_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_ERASE_POST_SEND_FAILURE") == 0);
    CHECK(request_count(&o, &count) && count == 1U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated C2 EraseCommand post-send %s retention\n", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}

/* Isolated process: deferred cleanup failure is never reported as success. */
static int isolated_finish_failure(void)
{
    struct owners o;
    ams_mel_ir_erase_request *request = NULL;
    ams_mel_ir_erase_result_v1 result;
    const ams_mel_ir_erase_command_v1 c = command(7U, 1U);
    char d[96];
    void *pin;
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    CHECK(open_owner("erase-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(ERASE_SEND, 1) == 1);
    CHECK(ams_mel_ir_erase_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_ir_erase_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_erase_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated C2 EraseCommand deferred-cleanup failure\n");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "post-send") == 0)
        return isolated_post_send(argv[2]);
    if (argc == 2 && strcmp(argv[1], "finish-failure") == 0)
        return isolated_finish_failure();
    CHECK(test_fidelity() == EXIT_SUCCESS);
    CHECK(test_alternatives() == EXIT_SUCCESS);
    CHECK(test_outcomes() == EXIT_SUCCESS);
    CHECK(test_invalid() == EXIT_SUCCESS);
    CHECK(test_send_failures_and_lifecycle() == EXIT_SUCCESS);
    CHECK(test_timeout_then_success() == EXIT_SUCCESS);
    CHECK(test_pending_close() == EXIT_SUCCESS);
    CHECK(test_admission() == EXIT_SUCCESS);
    puts("PASS: C mock EraseCommand contract");
    return EXIT_SUCCESS;
}
