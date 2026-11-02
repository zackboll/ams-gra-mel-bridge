#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-004: compiled-C contract for C2Channel::send(CameraProtectCmd)
 * and the opt-in C2 CameraProtectCmdResp metadata owner, against the
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
#define PROTECT_PROBE_FAMILY 7U
#define PROTECT_SEND 8U

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
/* New IRMEL-ADA-004 records. */
_Static_assert(offsetof(ams_mel_ir_camera_protect_command_v1, command_id) == 0, "protect command");
_Static_assert(offsetof(ams_mel_ir_camera_protect_command_v1, response_time_ns) == 8, "protect command");
_Static_assert(offsetof(ams_mel_ir_camera_protect_command_v1, protect_flag) == 16, "protect command");
_Static_assert(sizeof(ams_mel_ir_camera_protect_command_v1) == 24, "protect command size");
_Static_assert(offsetof(ams_mel_ir_camera_protect_response_v1, command_id) == 0, "protect response");
_Static_assert(offsetof(ams_mel_ir_camera_protect_response_v1, response_time_ns) == 8, "protect response");
_Static_assert(offsetof(ams_mel_ir_camera_protect_response_v1, command_response) == 16, "protect response");
_Static_assert(sizeof(ams_mel_ir_camera_protect_response_v1) == 24, "protect response size");
_Static_assert(offsetof(ams_mel_ir_camera_protect_result_v1, error_code) == 24, "protect result");
_Static_assert(sizeof(ams_mel_ir_camera_protect_result_v1) == 32, "protect result size");
_Static_assert(_Generic(((ams_mel_ir_camera_protect_command_v1 *)0)->response_time_ns, int64_t: 1, default: 0), "signed ns");
_Static_assert(_Generic(((ams_mel_ir_camera_protect_response_v1 *)0)->response_time_ns, int64_t: 1, default: 0), "signed ns");
_Static_assert(_Generic(((ams_mel_ir_camera_protect_command_v1 *)0)->command_id, uint32_t: 1, default: 0), "u32 id");
_Static_assert(AMS_MEL_IR_CAMERA_PROTECT_OK == 0 && AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE == 1 &&
               AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED == 2, "CmdResp");

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
    *(void **)(&last_command) = dlsym(library, "mock_camera_protect_last_command");
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

static ams_mel_ir_camera_protect_command_v1 command(uint32_t id, int64_t ns, uint32_t flag)
{
    ams_mel_ir_camera_protect_command_v1 value;
    memset(&value, 0, sizeof value);
    value.command_id = id;
    value.response_time_ns = ns;
    value.protect_flag = flag;
    return value;
}

static unsigned sends(void)
{
    unsigned count = 0;
    (void)last_command(NULL, &count);
    return count;
}

static int delivered(const ams_mel_ir_camera_protect_command_v1 *c)
{
    uint64_t values[3] = {0};
    CHECK(last_command(values, NULL) == 1);
    CHECK(values[0] == c->command_id);
    CHECK((int64_t)values[1] == c->response_time_ns);
    CHECK(values[2] == c->protect_flag);
    return EXIT_SUCCESS;
}

static ams_mel_status_t submit(struct owners *o, const ams_mel_ir_camera_protect_command_v1 *c,
                               ams_mel_ir_camera_protect_request **request, char *d, size_t size)
{
    return ams_mel_ir_c2_submit_camera_protect(o->c2, c, request, d, size, NULL);
}

static int request_count(const struct owners *o, size_t *count)
{
    return ams_mel_test_c2_requests(o->c2, count);
}
/* Every field round-trips exactly through C2Channel::send(CameraProtectCmd)
 * for both ProtectFlag values and unsigned/signed boundaries; independent
 * requests keep their identities through repeated cached waits. */
static int test_fidelity(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_command_v1 cases[4];
    ams_mel_ir_camera_protect_request *requests[4] = {NULL, NULL, NULL, NULL};
    CHECK(open_owner("protect-echo", 0, 1, &o) == EXIT_SUCCESS);
    cases[0] = command(UINT32_C(0xffffffff), INT64_MIN, 1U);
    cases[1] = command(0U, INT64_MAX, 0U);
    cases[2] = command(UINT32_C(0x80000001), -1, 1U);
    cases[3] = command(7U, 0, 0U);
    for (unsigned i = 0; i < 4; ++i) {
        ams_mel_ir_camera_protect_result_v1 result;
        memset(&result, 0xa5, sizeof result);
        CHECK(submit(&o, &cases[i], &requests[i], NULL, 0) == AMS_MEL_OK);
        CHECK(delivered(&cases[i]) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_camera_protect_request_wait(requests[i], 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(result.response.command_id == cases[i].command_id);
        CHECK(result.response.response_time_ns == cases[i].response_time_ns);
        CHECK(result.response.command_response == AMS_MEL_IR_CAMERA_PROTECT_OK);
    }
    for (unsigned i = 0; i < 4; ++i) {
        ams_mel_ir_camera_protect_result_v1 result;
        CHECK(ams_mel_ir_camera_protect_request_wait(requests[i], 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.response.command_id == cases[i].command_id);
        CHECK(ams_mel_ir_camera_protect_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(requests[i] == NULL);
        CHECK(ams_mel_ir_camera_protect_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* All three CmdResp alternatives are successful completions, preserved
 * exactly, and distinct from MEL rejection. */
static int expect_response(const char *scenario, uint32_t alternative)
{
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    char d[64];
    const ams_mel_ir_camera_protect_command_v1 c = command(0x4242U, -5, 1U);
    CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, d, sizeof d, NULL) == AMS_MEL_OK);
    CHECK(d[0] == '\0');
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(result.response.command_response == alternative);
    CHECK(result.response.command_id == 0x4242U && result.response.response_time_ns == -5);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_alternatives(void)
{
    CHECK(expect_response("protect-echo", AMS_MEL_IR_CAMERA_PROTECT_OK) == EXIT_SUCCESS);
    CHECK(expect_response("protect-unavailable", AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(expect_response("protect-unsupported", AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
static int expect_outcome(const char *scenario, ams_mel_status_t status,
                          ams_mel_error_code_t code, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    char d[1024];
    const ams_mel_ir_camera_protect_command_v1 c = command(7U, 7, 0U);
    CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, d, sizeof d, NULL) == status);
    CHECK(result.error_code == code);
    CHECK(result.response.command_response == 0U && result.response.command_id == 0U);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 0, &result, d, sizeof d, NULL) == status);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* MEL rejection versus CmdResp::Unsupported, and every failure category. */
static int test_outcomes(void)
{
    /* A provider without camera self-protection: MEL Unsupported rejection,
     * not the CmdResp::Unsupported successful response. */
    CHECK(expect_outcome("no-camera-protect", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_UNSUPPORTED, "") == EXIT_SUCCESS);
    CHECK(expect_outcome("protect-reject", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS, "camera protect rejected") == EXIT_SUCCESS);
    CHECK(expect_outcome("protect-unknown-error", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown MEL error code") == EXIT_SUCCESS);
    CHECK(expect_outcome("protect-unknown-response", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown CameraProtectCmdResp CmdResp value") == EXIT_SUCCESS);
    CHECK(expect_outcome("protect-null", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned null successful CameraProtectCmdResp") == EXIT_SUCCESS);
    CHECK(expect_outcome("protect-future-throw", AMS_MEL_PROVIDER_EXCEPTION, 0,
                         "mock camera protect future exception") == EXIT_SUCCESS);
    {   /* Long rejection: required size reports the complete description. */
        struct owners o;
        ams_mel_ir_camera_protect_request *request = NULL;
        ams_mel_ir_camera_protect_result_v1 result;
        char small[16];
        size_t required = 0;
        const ams_mel_ir_camera_protect_command_v1 c = command(8U, 8, 1U);
        CHECK(open_owner("protect-reject-long", 0, 1, &o) == EXIT_SUCCESS);
        CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, small, sizeof small,
              &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INSUFFICIENT_RESOURCES && required == 614U);
        CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}
/* Invalid Boolean representations and arguments are rejected before send
 * and consume no accounting slot. */
static int test_invalid(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_request *stale = (ams_mel_ir_camera_protect_request *)(void *)&o;
    ams_mel_ir_camera_protect_command_v1 c = command(1U, 1, 1U);
    ams_mel_ir_camera_protect_result_v1 result;
    size_t count = 99;
    char d[64];
    unsigned before;
    CHECK(open_owner("protect-echo", 0, 1, &o) == EXIT_SUCCESS);
    before = sends();
    c.protect_flag = 2U;
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(strcmp(d, "invalid CameraProtectCmd input") == 0 && request == NULL);
    c.protect_flag = UINT32_MAX; CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.protect_flag = 0x100U; CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.protect_flag = 1U;
    CHECK(submit(&o, NULL, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, NULL, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, &stale, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(&o, &c, &request, NULL, 4) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_camera_protect(NULL, &c, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(sends() == before);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(ams_mel_ir_camera_protect_request_wait(NULL, 0, &result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Send exception and an invalid provider future are distinct failures and
 * release their accounting slot; the channel must be Enabled. */
static int test_send_failures_and_lifecycle(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    const ams_mel_ir_camera_protect_command_v1 c = command(2U, 2, 1U);
    size_t count = 99;
    char d[128];
    unsigned before;
    CHECK(open_owner("protect-send-throw", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strcmp(d, "mock camera protect send exception") == 0 && request == NULL);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(open_owner("protect-invalid-future", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "provider returned an invalid CameraProtectCmd future") == 0 && request == NULL);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    /* Attached (not Enabled): refused with no provider call. */
    CHECK(open_owner("protect-echo", 0, 0, &o) == EXIT_SUCCESS);
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
    if (!ams_mel_test_completion_boundary(PROTECT_PROBE_FAMILY, 3, 0, values)) return UINT64_MAX;
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
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    const ams_mel_ir_camera_protect_command_v1 c = command(0x1234U, 99, 1U);
    size_t count = 0;
    uint64_t base, observed = 0;
    CHECK(open_owner("protect-held", 0, 1, &o) == EXIT_SUCCESS);
    base = owners_destroyed();
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(request_count(&o, &count) && count == 1U);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 1, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(release_family(PROTECT_SEND, 1) == 1);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0x1234U && result.response.response_time_ns == 99);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0x1234U);
    CHECK(ams_mel_test_completion_owner(PROTECT_PROBE_FAMILY, base + 1U, &observed));
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Request Close, C2 Close and Session Close all precede completion: the
 * provider channel and library stay alive until the future completes;
 * nothing is cancelled; teardown order is preserved. */
static int test_pending_close(void)
{
    char log[] = "/tmp/ams-protect-pending-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_request *observer = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    const ams_mel_ir_camera_protect_command_v1 c = command(0xabcU, -9, 0U);
    uint64_t base, observed = 0;
    void *pin;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("protect-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    base = owners_destroyed();
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(submit(&o, &c, &observer, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_request_wait(observer, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "control_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(release_family(PROTECT_SEND, 2) == 1);
    CHECK(ams_mel_ir_camera_protect_request_wait(observer, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0xabcU && result.response.response_time_ns == -9);
    CHECK(ams_mel_test_completion_owner(PROTECT_PROBE_FAMILY, base + 2U, &observed));
    CHECK(occurrences(log, "c2_channel_destroyed") == 1U);
    CHECK(ams_mel_ir_camera_protect_request_close(&observer, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    {
        const char *order[] = {"c2_camera_protect_sent", "c2_channel_destroyed",
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
    ams_mel_ir_camera_protect_request *refused = NULL;
    const ams_mel_ir_camera_protect_command_v1 c = command(9U, 9, 0U);
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
    ams_mel_ir_camera_protect_request *first = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    const ams_mel_ir_camera_protect_command_v1 c = command(5U, 5, 1U);
    ams_mel_test_admission_token *token = NULL;
    uint64_t base, observed = 0;
    size_t count = 0;
    CHECK(open_owner("protect-held", 1, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_admission(o.session, 0, &token, NULL, NULL));
    CHECK(admission_is(&token, 0U) == EXIT_SUCCESS);
    {   /* Quiesce: every earlier CameraProtect graph in this process is gone. */
        uint64_t values[4] = {0};
        CHECK(ams_mel_test_completion_boundary(PROTECT_PROBE_FAMILY, 3, 0, values));
        CHECK(ams_mel_test_completion_owner(PROTECT_PROBE_FAMILY, values[2], &observed));
        base = owners_destroyed();
        CHECK(base == values[2]);
    }
    CHECK(submit(&o, &c, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(request_count(&o, &count) && count == 1U);
    CHECK(ams_mel_test_completion_owner(PROTECT_PROBE_FAMILY, base + 1U, &observed));
    CHECK(observed == base + 1U);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_request_wait(first, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Public request Close is not cancellation and does not release the slot. */
    CHECK(ams_mel_ir_camera_protect_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(release_family(PROTECT_SEND, 1) == 1);
    CHECK(admission_released(&token) == EXIT_SUCCESS);
    CHECK(request_count(&o, &count) && count == 0U);
    CHECK(submit(&o, &c, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(release_family(PROTECT_SEND, 1) == 1);
    CHECK(ams_mel_ir_camera_protect_request_wait(first, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 5U && result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_camera_protect_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
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
    char log[] = "/tmp/ams-protect-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    const ams_mel_ir_camera_protect_command_v1 c = command(6U, 6, 1U);
    size_t count = 0;
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("protect-held", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_CAMERA_PROTECT_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(submit(&o, &c, &request, d, sizeof d) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_CAMERA_PROTECT_POST_SEND_FAILURE") == 0);
    CHECK(request_count(&o, &count) && count == 1U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated C2 CameraProtectCmd post-send %s retention\n", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}

/* Isolated process: deferred cleanup failure is never reported as success. */
static int isolated_finish_failure(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    const ams_mel_ir_camera_protect_command_v1 c = command(7U, 7, 1U);
    char d[96];
    void *pin;
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    CHECK(open_owner("protect-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(PROTECT_SEND, 1) == 1);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated C2 CameraProtectCmd deferred-cleanup failure\n");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}
static int counters_are(ams_mel_ir_camera_protect_metadata *m, uint64_t received,
                        uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 counters;
    memset(&counters, 0, sizeof counters);
    CHECK(ams_mel_ir_camera_protect_get_counters(m, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == received);
    CHECK(counters.events_dropped_queue_full == dropped);
    CHECK(counters.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

static int receive_response(ams_mel_ir_camera_protect_metadata *m, uint32_t id, int64_t ns,
                            uint32_t response)
{
    ams_mel_ir_camera_protect_event *event = NULL;
    const ams_mel_ir_camera_protect_response_v1 *view = NULL;
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->command_id == id && view->response_time_ns == ns);
    CHECK(view->command_response == response);
    CHECK(ams_mel_ir_camera_protect_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

/* Complete fields, all three alternatives, synchronous registration
 * delivery, ordering, one-shot, and an event that outlives the owner, C2,
 * Session and provider library. Registration works while only Attached. */
static int test_callback_rich(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_camera_protect_event *kept = NULL;
    const ams_mel_ir_camera_protect_response_v1 *view = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner("protect-callback-rich", 0, 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 4, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(counters_are(m, 3, 0, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 1U, INT64_MIN, AMS_MEL_IR_CAMERA_PROTECT_OK) == EXIT_SUCCESS);
    CHECK(receive_response(m, UINT32_MAX, INT64_MAX, AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &kept, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &kept, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    {
        ams_mel_ir_camera_protect_event *none = NULL;
        CHECK(ams_mel_ir_camera_protect_receive(m, 1, &none, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(none == NULL);
    }
    {   /* Repeated registration attempt is refused (one shot per C2). */
        ams_mel_ir_camera_protect_metadata *again = NULL;
        CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 4, &again, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(again == NULL);
    }
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(m == NULL);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_event_view(kept, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->command_id == 0U && view->response_time_ns == -1 &&
          view->command_response == AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED);
    CHECK(ams_mel_ir_camera_protect_event_close(&kept, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Opt-in only: a provider without camera self-protection answers
 * NotSupported, reported distinctly, while existing C2 metadata, enable and
 * typed commands keep working on the same channel. */
static int test_callback_not_supported(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_c2_metadata *existing = NULL;
    ams_mel_ir_return_request *keepalive = NULL;
    ams_mel_ir_return_result_v1 keepalive_result;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    const ams_mel_ir_camera_protect_command_v1 c = command(3U, 3, 1U);
    ams_mel_ir_return_t reg = 99U;
    char d[128];
    CHECK(open_owner("metadata-rich", 0, 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(o.c2, 8, &existing, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, &reg, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(reg == AMS_MEL_IR_RETURN_NOT_SUPPORTED && m == NULL);
    CHECK(strcmp(d, "provider does not support camera self-protection CameraProtectCmdResp") == 0);
    {
        ams_mel_ir_c2_metadata_event *event = NULL;
        CHECK(ams_mel_ir_c2_metadata_receive(existing, 1000, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_c2_enable(o.c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_send_keepalive(o.c2, &keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(keepalive, 5000, &keepalive_result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    /* The send itself is a MEL Unsupported rejection, not CmdResp::Unsupported. */
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, NULL, 0, NULL) ==
          AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_UNSUPPORTED);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
/* Registration Fail vs exception: distinct statuses; out_registration only
 * when a Return exists; the one shot is consumed; the retained callback stays
 * inactive and a late callback from the channel destructor is safe. */
static int test_callback_registration_failure(const char *scenario, ams_mel_status_t expected,
                                              int has_return)
{
    char log[] = "/tmp/ams-protect-register-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, &reg, NULL, 0, NULL) == expected);
    CHECK(m == NULL && reg == (has_return ? AMS_MEL_IR_RETURN_FAIL : 99U));
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, &reg, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    if (has_return) {
        const char *order[] = {"protect_registration_attempted", "late_protect_callback_invoked",
                               "late_protect_callback_returned", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* Null payload and unknown CmdResp values counted malformed, never remapped;
 * bounded drop-incoming overflow preserves the first arrivals in order;
 * allocation failure rejects one event without crossing the boundary. */
static int test_callback_bounds(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_camera_protect_event *event = NULL;
    CHECK(open_owner("protect-callback-malformed", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 4, 0, 3) == EXIT_SUCCESS);
    CHECK(receive_response(m, 9U, 9, AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner("protect-callback-overflow", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 5, 3, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0U, 0, AMS_MEL_IR_CAMERA_PROTECT_OK) == EXIT_SUCCESS);
    CHECK(receive_response(m, 1U, 1, AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner("protect-callback-allocation", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(getenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_FAILURE") == NULL);
    CHECK(counters_are(m, 2, 0, 1) == EXIT_SUCCESS);
    CHECK(receive_response(m, 2U, 2, AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Stopped state: C2 Close stops the owner; queued events drain, then
 * STREAM_STOPPED. A late callback after owner and C2 closure is memory-safe
 * and returns before provider unload. */
static int test_callback_stop_and_late(void)
{
    char log[] = "/tmp/ams-protect-late-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_camera_protect_event *event = NULL;
    const char *order[] = {"late_protect_callback_invoked", "late_protect_callback_returned",
                           "library_unloaded"};
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("protect-late", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&o.c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_response(m, 1U, INT64_MIN, AMS_MEL_IR_CAMERA_PROTECT_OK) == EXIT_SUCCESS);
    CHECK(receive_response(m, UINT32_MAX, INT64_MAX, AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0U, -1, AMS_MEL_IR_CAMERA_PROTECT_UNSUPPORTED) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_receive(m, 1000, &event, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    /* The late destructor callback hit a stopped owner: counted, not queued. */
    CHECK(counters_are(m, 4, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 3U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}
/* The callback and the request future are independent sources: a response
 * delivered synchronously inside send (no bridge lock held) does not complete
 * or alter the future, and the future does not enqueue metadata. */
static int test_callback_independent_of_request(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_camera_protect_request *request = NULL;
    ams_mel_ir_camera_protect_result_v1 result;
    ams_mel_ir_camera_protect_event *event = NULL;
    const ams_mel_ir_camera_protect_command_v1 c = command(0x5151U, 31, 1U);
    CHECK(open_owner("protect-sync", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 0, 0, 0) == EXIT_SUCCESS);
    CHECK(submit(&o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(counters_are(m, 1, 0, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0x5151U, -77, AMS_MEL_IR_CAMERA_PROTECT_UNAVAILABLE) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.response_time_ns == 31 &&
          result.response.command_response == AMS_MEL_IR_CAMERA_PROTECT_OK);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(counters_are(m, 1, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int wait_for_file(const char *path)
{
    struct timespec delay = {0, 1000000};
    for (unsigned attempt = 0; attempt < 5000U; ++attempt) {
        if (access(path, F_OK) == 0) return EXIT_SUCCESS;
        (void)nanosleep(&delay, NULL);
    }
    return EXIT_FAILURE;
}

static int append_log(const char *path, const char *value)
{
    FILE *file = fopen(path, "ab");
    CHECK(file != NULL);
    CHECK(fprintf(file, "%s\n", value) > 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

/* Isolated process: public owner Close while a provider callback is pending
 * does not claim quiescence; C2 physical release is the boundary, and the
 * in-flight callback returns safely before the C2 channel is destroyed and
 * the provider library unloads. File barriers, not sleeps, order events. */
static int isolated_callback_in_flight(void)
{
    char log[] = "/tmp/ams-protect-active-XXXXXX";
    char barrier[] = "/tmp/ams-protect-barrier-XXXXXX";
    char start[256], entered[256], release[256];
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    int descriptor;
    const char *order[] = {"protect_callback_entered", "protect_owner_closed",
                           "protect_callback_returned", "c2_channel_destroyed",
                           "library_unloaded"};
    CHECK(make_log(log) == EXIT_SUCCESS);
    descriptor = mkstemp(barrier); CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(snprintf(start, sizeof start, "%s.start", barrier) > 0);
    CHECK(snprintf(entered, sizeof entered, "%s.entered", barrier) > 0);
    CHECK(snprintf(release, sizeof release, "%s.release", barrier) > 0);
    CHECK(setenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_BARRIER", barrier, 1) == 0);
    CHECK(open_owner("protect-in-flight", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(start, "start") == EXIT_SUCCESS);
    CHECK(wait_for_file(entered) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(log, "protect_owner_closed") == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_CAMERA_PROTECT_CALLBACK_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 5U) == EXIT_SUCCESS);
    CHECK(unlink(start) == 0); CHECK(unlink(entered) == 0); CHECK(unlink(release) == 0);
    CHECK(unlink(log) == 0);
    puts("PASS: isolated C2 CameraProtectCmdResp in-flight callback teardown");
    return EXIT_SUCCESS;
}
static int test_callback_invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_camera_protect_metadata *m = NULL;
    ams_mel_ir_camera_protect_metadata *stale = (ams_mel_ir_camera_protect_metadata *)(void *)&o;
    ams_mel_ir_camera_protect_event *event = NULL;
    const ams_mel_ir_camera_protect_response_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(ams_mel_ir_c2_camera_protect_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_event_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(open_owner("protect-callback-rich", 0, 1, &o) == EXIT_SUCCESS);
    /* Argument rejection does not consume the one shot. */
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 1, NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 1, &stale, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 1, &m, NULL, NULL, 4, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_camera_protect_open(o.c2, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 3, 2, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_protect_get_counters(m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_receive(m, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_protect_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "post-send") == 0) return isolated_post_send(argv[2]);
    if (argc == 2 && strcmp(argv[1], "finish-failure") == 0) return isolated_finish_failure();
    if (argc == 2 && strcmp(argv[1], "callback-in-flight") == 0) return isolated_callback_in_flight();
    if (test_fidelity() != EXIT_SUCCESS || test_alternatives() != EXIT_SUCCESS ||
        test_outcomes() != EXIT_SUCCESS || test_invalid() != EXIT_SUCCESS ||
        test_send_failures_and_lifecycle() != EXIT_SUCCESS ||
        test_timeout_then_success() != EXIT_SUCCESS || test_pending_close() != EXIT_SUCCESS ||
        test_admission() != EXIT_SUCCESS) {
        fprintf(stderr, "FAIL: C2 CameraProtectCmd contract\n");
        return EXIT_FAILURE;
    }
    if (test_callback_rich() != EXIT_SUCCESS || test_callback_not_supported() != EXIT_SUCCESS ||
        test_callback_registration_failure("protect-register-fail", AMS_MEL_PROVIDER_FAILED, 1) != EXIT_SUCCESS ||
        test_callback_registration_failure("protect-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0) != EXIT_SUCCESS ||
        test_callback_bounds() != EXIT_SUCCESS || test_callback_stop_and_late() != EXIT_SUCCESS ||
        test_callback_independent_of_request() != EXIT_SUCCESS ||
        test_callback_invalid_arguments() != EXIT_SUCCESS) {
        fprintf(stderr, "FAIL: C2 CameraProtectCmdResp contract\n");
        return EXIT_FAILURE;
    }
    puts("PASS: C IR C2 CameraProtectCmd and CameraProtectCmdResp contract");
    return EXIT_SUCCESS;
}
