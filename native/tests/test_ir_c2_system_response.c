#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-008: compiled-C contract for C2Channel::send(SystemTrackDataResponse)
 * against the deterministic mock provider. Concurrency cases use the provider
 * promise gate, the test-only C2/Track request observers, Session admission
 * tokens and completion-owner probes, never sleeps as a synchronization
 * decision. Track SystemTrackDataResponse tests are unchanged and separate. */
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#include "admission_observation.h"

extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)
#define C2_RESPONSE_PROBE 11U   /* appended C2SystemResponse family */
#define TRACK_PROBE 5U          /* pre-existing Track family */
#define C2_RESPONSE_SEND 12U    /* mock gate family for the C2 overload */
#define TRACK_SEND 5U           /* mock gate family for Track sends */

/* The pre-existing frozen input layout is reused verbatim; the new C2 result
 * is a distinct type with the same generic CommandStatus shape. */
_Static_assert(sizeof(ams_mel_ir_system_track_data_response_v1) == 96, "response size");
_Static_assert(offsetof(ams_mel_ir_system_track_data_response_v1, az_el_valid) == 56, "bool");
_Static_assert(offsetof(ams_mel_ir_system_track_data_response_v1, inertial_az_el) == 64, "azel");
_Static_assert(sizeof(ams_mel_ir_c2_system_response_result_v1) == 40, "result size");
_Static_assert(offsetof(ams_mel_ir_c2_system_response_result_v1, status) == 0, "status first");
_Static_assert(offsetof(ams_mel_ir_c2_system_response_result_v1, error_code) == 32, "code");

typedef int (*release_fn)(unsigned, unsigned);
typedef int (*last_fn)(uint64_t *, unsigned *);
static release_fn release_family;
static last_fn last_response;

static int bind_provider(void)
{
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    *(void **)(&last_response) = dlsym(library, "mock_c2_system_response_last");
    CHECK(release_family && last_response);
    CHECK(dlclose(library) == 0);
    return EXIT_SUCCESS;
}

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

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

static uint64_t bits(double value)
{ uint64_t result; memcpy(&result, &value, sizeof result); return result; }
static double from_bits(uint64_t value)
{ double result; memcpy(&result, &value, sizeof result); return result; }

/* Distinctive values: nothing equals a default, an adjacent field or a
 * sign/scale mistake. */
static ams_mel_ir_system_track_data_response_v1 rich(void)
{
    ams_mel_ir_system_track_data_response_v1 v;
    memset(&v, 0, sizeof v);
    v.system_time_ns = INT64_C(-8765432109876);
    v.command_id = UINT32_C(0xF1234567);
    v.request_id = UINT32_C(0xE2345678);
    v.track_id = UINT32_C(0xD3456789);
    v.range_m = 123456.75;
    v.range_rate_mps = -456.125;
    v.range_error_m = 12.5;
    v.range_rate_error_mps = -0.875;
    v.az_el_valid = 1U;
    v.range_valid = 1U;
    v.inertial_az_el.azimuth_rad = -1.25;
    v.inertial_az_el.elevation_rad = 0.625;
    v.az_el_error.azimuth_rad = 0.03125;
    v.az_el_error.elevation_rad = -0.015625;
    return v;
}

static unsigned sends(void)
{
    unsigned count = 0;
    (void)last_response(NULL, &count);
    return count;
}

/* Exact bit-level fidelity of all twelve upstream fields (fourteen scalars). */
static int delivered(const ams_mel_ir_system_track_data_response_v1 *v)
{
    uint64_t f[14] = {0};
    CHECK(last_response(f, NULL) == 1);
    CHECK(f[0] == (uint64_t)v->system_time_ns);
    CHECK(f[1] == v->command_id);
    CHECK(f[2] == v->request_id);
    CHECK(f[3] == v->track_id);
    CHECK(f[4] == bits(v->range_m));
    CHECK(f[5] == bits(v->range_rate_mps));
    CHECK(f[6] == bits(v->range_error_m));
    CHECK(f[7] == bits(v->range_rate_error_mps));
    CHECK(f[8] == v->az_el_valid);
    CHECK(f[9] == v->range_valid);
    CHECK(f[10] == bits(v->inertial_az_el.azimuth_rad));
    CHECK(f[11] == bits(v->inertial_az_el.elevation_rad));
    CHECK(f[12] == bits(v->az_el_error.azimuth_rad));
    CHECK(f[13] == bits(v->az_el_error.elevation_rad));
    return EXIT_SUCCESS;
}

static ams_mel_status_t submit(struct owners *o, const ams_mel_ir_system_track_data_response_v1 *v,
                               ams_mel_ir_c2_system_response_request **request, char *d, size_t size)
{
    return ams_mel_ir_c2_submit_system_track_data_response(o->c2, v, request, d, size, NULL);
}

static int c2_requests(const struct owners *o, size_t expected)
{
    size_t count = 99;
    CHECK(ams_mel_test_c2_requests(o->c2, &count));
    if (count != expected) fprintf(stderr, "C2 requests %zu != %zu\n", count, expected);
    CHECK(count == expected);
    return EXIT_SUCCESS;
}

static int text_is(ams_mel_string_view_v1 value, const char *expected)
{
    CHECK(value.size == strlen(expected));
    CHECK(value.size == 0U || memcmp(value.data, expected, value.size) == 0);
    return EXIT_SUCCESS;
}

/* Submits, verifies exact delivery, then waits twice (cached, no resend) and
 * checks the echo CommandStatus while the request-owned text is still valid. */
static int round_trip(struct owners *o, const ams_mel_ir_system_track_data_response_v1 *v)
{
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_result_v1 result, again;
    const unsigned before = sends();
    CHECK(submit(o, v, &request, NULL, 0) == AMS_MEL_OK && request != NULL);
    CHECK(sends() == before + 1U);
    CHECK(delivered(v) == EXIT_SUCCESS);
    memset(&result, 0xa5, sizeof result);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &again, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(memcmp(&again, &result, sizeof again) == 0);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(result.status.command_id == v->command_id);
    CHECK(result.status.state == AMS_MEL_IR_COMMAND_ACCEPTED);
    CHECK(result.status.reason_id == AMS_MEL_IR_CANNOT_COMPLY_NOT_SET);
    CHECK(text_is(result.status.reason_description, "C2 system response accepted \xC2\xB5") == EXIT_SUCCESS);
    CHECK(sends() == before + 1U);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Every field round-trips bit-exactly through C2Channel::send: identifier
 * boundaries (0, UINT32_MAX, independent values), signed timestamp
 * boundaries, all four Boolean combinations with inactive-but-populated
 * fields, signed zero and non-finite doubles. */
static int test_input_fidelity(void)
{
    struct owners o;
    const int64_t times[] = {INT64_MIN, INT64_C(-1), 0, INT64_C(1), INT64_MAX,
                             INT64_C(-8765432109876)};
    const uint32_t ids[][3] = {{0U, 0U, 0U}, {UINT32_MAX, UINT32_MAX, UINT32_MAX},
                               {0U, UINT32_MAX, 1U}, {UINT32_MAX, 0U, 0x80000000U},
                               {0x80000001U, 0x7fffffffU, 0xfffffffeU}};
    const double special[] = {-0.0, 0.0, 1.0 / 0.0, -1.0 / 0.0, 4.9406564584124654e-324,
                              -1.7976931348623157e308};
    CHECK(open_owner("c2-sys-echo", 0, 1, &o) == EXIT_SUCCESS);
    {   /* The distinctive rich response. */
        const ams_mel_ir_system_track_data_response_v1 v = rich();
        CHECK(round_trip(&o, &v) == EXIT_SUCCESS);
    }
    for (size_t i = 0; i < sizeof times / sizeof times[0]; ++i) {
        ams_mel_ir_system_track_data_response_v1 v = rich();
        v.system_time_ns = times[i];
        CHECK(round_trip(&o, &v) == EXIT_SUCCESS);
    }
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; ++i) {
        ams_mel_ir_system_track_data_response_v1 v = rich();
        v.command_id = ids[i][0]; v.request_id = ids[i][1]; v.track_id = ids[i][2];
        CHECK(round_trip(&o, &v) == EXIT_SUCCESS);
    }
    for (unsigned flags = 0; flags < 4U; ++flags) {
        /* Inactive fields stay populated: nothing is zeroed for a false flag. */
        ams_mel_ir_system_track_data_response_v1 v = rich();
        v.az_el_valid = (uint8_t)(flags & 1U);
        v.range_valid = (uint8_t)((flags >> 1U) & 1U);
        CHECK(round_trip(&o, &v) == EXIT_SUCCESS);
    }
    /* Signed zero and non-finite values in every double position, rotated so
     * no two positions carry the same value in one send. */
    for (size_t shift = 0; shift < sizeof special / sizeof special[0]; ++shift) {
        ams_mel_ir_system_track_data_response_v1 v = rich();
        double *slots[8] = {&v.range_m, &v.range_rate_mps, &v.range_error_m,
                            &v.range_rate_error_mps, &v.inertial_az_el.azimuth_rad,
                            &v.inertial_az_el.elevation_rad, &v.az_el_error.azimuth_rad,
                            &v.az_el_error.elevation_rad};
        const size_t n = sizeof special / sizeof special[0];
        for (size_t slot = 0; slot < 8U; ++slot) *slots[slot] = special[(slot + shift) % n];
        v.range_valid = 0U;
        CHECK(round_trip(&o, &v) == EXIT_SUCCESS);
    }
    {   /* NaN: classification only; payload bits are not promised. */
        ams_mel_ir_system_track_data_response_v1 v = rich();
        uint64_t f[14] = {0};
        ams_mel_ir_c2_system_response_request *request = NULL;
        ams_mel_ir_c2_system_response_result_v1 result;
        v.range_rate_mps = from_bits(UINT64_C(0x7ff8000000000000));
        v.az_el_error.elevation_rad = from_bits(UINT64_C(0xfff8000000000000));
        CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(last_response(f, NULL) == 1);
        CHECK(from_bits(f[5]) != from_bits(f[5]) && from_bits(f[13]) != from_bits(f[13]));
        CHECK(f[4] == bits(v.range_m) && f[12] == bits(v.az_el_error.azimuth_rad));
        CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Invalid Boolean encodings and invalid arguments are refused before any
 * admission, accounting or provider call. */
static int test_invalid(void)
{
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_request *stale = (ams_mel_ir_c2_system_response_request *)(void *)&o;
    ams_mel_ir_c2_system_response_result_v1 result;
    const uint8_t bad[] = {2U, 3U, 0x80U, 255U};
    char d[64];
    unsigned before;
    CHECK(open_owner("c2-sys-echo", 0, 1, &o) == EXIT_SUCCESS);
    before = sends();
    for (size_t i = 0; i < sizeof bad; ++i) {
        ams_mel_ir_system_track_data_response_v1 v = rich();
        v.az_el_valid = bad[i];
        CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(strcmp(d, "invalid SystemTrackDataResponse input") == 0 && request == NULL);
        v = rich();
        v.range_valid = bad[i];
        CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_INVALID_ARGUMENT);
        v.az_el_valid = 0U;
        CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    }
    {
        const ams_mel_ir_system_track_data_response_v1 v = rich();
        CHECK(submit(&o, NULL, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(submit(&o, &v, NULL, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(submit(&o, &v, &stale, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(submit(&o, &v, &request, NULL, 4) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_c2_submit_system_track_data_response(NULL, &v, &request, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(sends() == before);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_system_response_request_wait(NULL, 0, &result, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_system_response_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Derived scenario: CommandState := requestId, CannotComply := trackId, text
 * selected by range. Returns the Wait status; fills result while open. */
static int derived(struct owners *o, uint32_t state, uint32_t reason, double text_code,
                   ams_mel_status_t expected, const char *diagnostic, const char *reason_text)
{
    ams_mel_ir_system_track_data_response_v1 v = rich();
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_result_v1 result;
    char d[128];
    v.command_id = state * 100U + reason;
    v.request_id = state;
    v.track_id = reason;
    v.range_m = text_code;
    CHECK(submit(o, &v, &request, NULL, 0) == AMS_MEL_OK);
    memset(&result, 0xa5, sizeof result);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, d, sizeof d, NULL) == expected);
    if (expected == AMS_MEL_OK) {
        CHECK(result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(result.status.command_id == v.command_id);
        CHECK(result.status.state == state && result.status.reason_id == reason);
        if (reason_text) CHECK(text_is(result.status.reason_description, reason_text) == EXIT_SUCCESS);
        CHECK(d[0] == '\0');
    } else {
        /* Never remapped to a known alternative or partially published. */
        CHECK(result.status.command_id == 0U && result.status.state == 0U);
        CHECK(result.status.reason_id == 0U && result.status.reason_description.size == 0U);
        CHECK(result.status.reason_description.data == NULL && result.error_code == 0U);
        CHECK(strcmp(d, diagnostic) == 0);
    }
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, d, sizeof d, NULL) == expected);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* All five CommandState and all forty-seven CannotComply alternatives,
 * CommandStatus Rejected as a successful future, complete long reason text,
 * empty text, and every malformed provider CommandStatus. */
static int test_response_fidelity(void)
{
    struct owners o;
    static const char malformed[] = "provider returned malformed C2 CommandStatus";
    CHECK(open_owner("c2-sys-derived", 0, 1, &o) == EXIT_SUCCESS);
    for (uint32_t state = 0; state <= AMS_MEL_IR_COMMAND_CANCELLED; ++state)
        CHECK(derived(&o, state, 0U, 0.0, AMS_MEL_OK, "", "C2 derived \xC2\xB5") == EXIT_SUCCESS);
    for (uint32_t reason = 0; reason <= AMS_MEL_IR_CANNOT_COMPLY_ALIGNMENT_MANEUVER; ++reason)
        CHECK(derived(&o, AMS_MEL_IR_COMMAND_REJECTED, reason, 0.0, AMS_MEL_OK, "",
                      "C2 derived \xC2\xB5") == EXIT_SUCCESS);
    /* Empty reason description: a null/zero view, not a dangling pointer. */
    CHECK(derived(&o, AMS_MEL_IR_COMMAND_ACCEPTED, 0U, 4.0, AMS_MEL_OK, "", "") == EXIT_SUCCESS);
    {   /* Complete long reason description through cached Wait. */
        ams_mel_ir_system_track_data_response_v1 v = rich();
        ams_mel_ir_c2_system_response_request *request = NULL;
        ams_mel_ir_c2_system_response_result_v1 result;
        v.request_id = AMS_MEL_IR_COMMAND_REJECTED; v.track_id = 44U; v.range_m = 1.0;
        CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
        for (int pass = 0; pass < 2; ++pass) {
            CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(result.status.reason_description.size == 613U);
            for (size_t i = 0; i < 510U; ++i) CHECK(result.status.reason_description.data[i] == 'x');
            CHECK(memcmp(result.status.reason_description.data + 510U, "\xe2\x82\xac", 3) == 0);
            for (size_t i = 513U; i < 613U; ++i) CHECK(result.status.reason_description.data[i] == 'y');
        }
        CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    }
    /* Unknown enumerations and malformed text are provider failures. */
    CHECK(derived(&o, 5U, 0U, 0.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(derived(&o, UINT32_MAX, 0U, 0.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(derived(&o, 2U, 47U, 0.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(derived(&o, 2U, UINT32_MAX, 0.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(derived(&o, 2U, 0U, 2.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(derived(&o, 2U, 0U, 3.0, AMS_MEL_PROVIDER_FAILED, malformed, NULL) == EXIT_SUCCESS);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int expect_outcome(const char *scenario, ams_mel_status_t status,
                          ams_mel_error_code_t code, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_result_v1 result;
    char d[1024];
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    CHECK(open_owner(scenario, 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, d, sizeof d, NULL) == status);
    CHECK(result.error_code == code);
    CHECK(result.status.state == 0U && result.status.reason_description.size == 0U);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, d, sizeof d, NULL) == status);
    CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* ErrorOr rejection versus CommandStatus Rejected (above) and every failure
 * category, each cached through repeated Wait. */
static int test_outcomes(void)
{
    static const char bad_text[] = "provider rejection description was invalid UTF-8 or contained NUL";
    CHECK(expect_outcome("no-c2-system-response", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_UNSUPPORTED, "") == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-reject", AMS_MEL_COMMAND_REJECTED, AMS_MEL_ERROR_INVALID_PARAMETERS,
                         "C2 system response rejected \xC2\xB5") == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-reject-utf8", AMS_MEL_PROVIDER_FAILED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS, bad_text) == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-reject-nul", AMS_MEL_PROVIDER_FAILED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS, bad_text) == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-unknown-error", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown MEL error code") == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-null", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned null successful CommandStatus") == EXIT_SUCCESS);
    CHECK(expect_outcome("c2-sys-future-throw", AMS_MEL_PROVIDER_EXCEPTION, 0,
                         "mock C2 system response future exception") == EXIT_SUCCESS);
    {   /* Long ErrorOr diagnostic: complete required size, cached, no resend. */
        struct owners o;
        ams_mel_ir_c2_system_response_request *request = NULL;
        ams_mel_ir_c2_system_response_result_v1 result;
        const ams_mel_ir_system_track_data_response_v1 v = rich();
        char small[16], complete[614];
        size_t required = 0, again = 0;
        unsigned before;
        CHECK(open_owner("c2-sys-reject-long", 0, 1, &o) == EXIT_SUCCESS);
        before = sends();
        CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, small, sizeof small,
              &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INSUFFICIENT_RESOURCES && required == 614U);
        CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, complete, sizeof complete,
              &again) == AMS_MEL_COMMAND_REJECTED);
        CHECK(again == required && strlen(complete) == 613U);
        for (size_t i = 0; i < 510U; ++i) CHECK(complete[i] == 'x');
        CHECK(memcmp(complete + 510U, "\xe2\x82\xac", 3) == 0);
        for (size_t i = 513U; i < 613U; ++i) CHECK(complete[i] == 'y');
        CHECK(sends() == before + 1U);
        CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

/* Send exception and an invalid future release their C2 slot; Attached-only
 * refuses with no provider call; closed C2 refuses. */
static int test_send_failures_and_lifecycle(void)
{
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    char d[128];
    unsigned before;
    CHECK(open_owner("c2-sys-send-throw", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strcmp(d, "mock C2 system response send exception") == 0 && request == NULL);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-invalid-future", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "provider returned an invalid C2 SystemTrackDataResponse future") == 0);
    CHECK(request == NULL);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-echo", 0, 0, &o) == EXIT_SUCCESS);
    before = sends();
    CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "C2 channel is not enabled") == 0 && request == NULL);
    CHECK(sends() == before);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_close(&o.c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_system_track_data_response(o.c2, &v, &request, d, sizeof d, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(sends() == before);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static uint64_t probe_value(unsigned family, unsigned index)
{
    uint64_t values[4] = {0};
    if (!ams_mel_test_completion_boundary(family, 3, 0, values)) return UINT64_MAX;
    return values[index];
}

/* Quiesce every earlier owner of the family (closed set) and return the base. */
static int quiesce(unsigned family, uint64_t *base)
{
    uint64_t observed = 0;
    const uint64_t created = probe_value(family, 2U);
    CHECK(created != UINT64_MAX);
    CHECK(ams_mel_test_completion_owner(family, created, &observed));
    CHECK(observed == created);
    *base = observed;
    return EXIT_SUCCESS;
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

/* Timeout does not consume or cancel and leaves the record untouched; the
 * graph and its admission permit are released exactly once after completion. */
static int test_timeout_then_success(void)
{
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_result_v1 result, untouched;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    uint64_t base = 0, observed = 0;
    unsigned before;
    CHECK(quiesce(C2_RESPONSE_PROBE, &base) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-held", 0, 1, &o) == EXIT_SUCCESS);
    before = sends();
    CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(sends() == before + 1U);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    memset(&result, 0x5a, sizeof result);
    untouched = result;
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 1, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(memcmp(&result, &untouched, sizeof result) == 0);
    CHECK(release_family(C2_RESPONSE_SEND, 1) == 1);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.status.command_id == v.command_id);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(C2_RESPONSE_PROBE, base + 1U, &observed));
    CHECK(observed == base + 1U);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(sends() == before + 1U);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Request Close, C2 Close and Session Close all precede completion: the
 * provider channel and library stay alive until the futures complete; nothing
 * is cancelled; the cached result survives; teardown order is preserved. */
static int test_pending_close(void)
{
    char log[] = "/tmp/ams-c2-sys-pending-XXXXXX";
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_request *observer = NULL;
    ams_mel_ir_c2_system_response_result_v1 result;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    uint64_t base = 0, observed = 0;
    void *pin;
    CHECK(quiesce(C2_RESPONSE_PROBE, &base) == EXIT_SUCCESS);
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(submit(&o, &v, &observer, NULL, 0) == AMS_MEL_OK);
    CHECK(probe_value(C2_RESPONSE_PROBE, 2U) == base + 2U);
    CHECK(probe_value(C2_RESPONSE_PROBE, 3U) == base);
    CHECK(c2_requests(&o, 2U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_system_response_request_wait(observer, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "control_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(release_family(C2_RESPONSE_SEND, 2) == 1);
    CHECK(ams_mel_ir_c2_system_response_request_wait(observer, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.status.command_id == v.command_id);
    CHECK(text_is(result.status.reason_description, "C2 system response accepted \xC2\xB5") == EXIT_SUCCESS);
    CHECK(ams_mel_test_completion_owner(C2_RESPONSE_PROBE, base + 2U, &observed));
    CHECK(observed == base + 2U);
    CHECK(occurrences(log, "c2_channel_destroyed") == 1U);
    CHECK(occurrences(log, "c2_system_track_data_response_sent") == 2U);
    CHECK(ams_mel_ir_c2_system_response_request_close(&observer, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    {
        const char *order[] = {"c2_system_track_data_response_sent", "c2_channel_destroyed",
                               "control_destroyed", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(occurrences(log, "library_unloaded") == 1U);
    CHECK(occurrences(log, "track_system_track_data_response_sent") == 0U);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

static int admission_is(ams_mel_test_admission_token **token, uint32_t expected)
{
    uint32_t active = 99, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 1, token, &active, &maximum));
    CHECK(active == expected && maximum == 1U);
    return EXIT_SUCCESS;
}

static int admission_released(ams_mel_test_admission_token **token)
{
    uint32_t active = 0, maximum = 0;
    CHECK(ams_mel_test_admission(NULL, 3, token, &active, &maximum));
    CHECK(active == 0U && maximum == 1U);
    return EXIT_SUCCESS;
}

static int refuse(struct owners *o)
{
    ams_mel_ir_c2_system_response_request *refused = NULL;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    const unsigned before = sends();
    char d[64];
    CHECK(submit(o, &v, &refused, d, sizeof d) == AMS_MEL_RESOURCE_EXHAUSTED);
    CHECK(strcmp(d, "async request limit reached") == 0 && refused == NULL);
    CHECK(sends() == before);
    return EXIT_SUCCESS;
}

/* Session admission is shared with every other request family. The permit
 * survives public request Close and is released exactly once, only after the
 * completion graph is gone. A second Session is unaffected (not global). */
static int test_admission(void)
{
    struct owners o, other;
    ams_mel_ir_c2_system_response_request *first = NULL, *independent = NULL;
    ams_mel_ir_c2_system_response_result_v1 result;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    ams_mel_test_admission_token *token = NULL;
    uint64_t base = 0, observed = 0;
    CHECK(open_owner("c2-sys-held", 1, 1, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_test_admission(o.session, 0, &token, NULL, NULL));
    CHECK(admission_is(&token, 0U) == EXIT_SUCCESS);
    CHECK(quiesce(C2_RESPONSE_PROBE, &base) == EXIT_SUCCESS);
    CHECK(submit(&o, &v, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    /* Negative control: the refused graph advances the family owner count
     * while this Session's slot is still held. */
    CHECK(ams_mel_test_completion_owner(C2_RESPONSE_PROBE, base + 1U, &observed));
    CHECK(observed == base + 1U);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    /* A separate Session with the same limit still admits: not global. */
    CHECK(open_owner("c2-sys-held", 1, 1, &other) == EXIT_SUCCESS);
    CHECK(submit(&other, &v, &independent, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_system_response_request_wait(first, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Public request Close is not cancellation and does not release the slot. */
    CHECK(ams_mel_ir_c2_system_response_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(admission_is(&token, 1U) == EXIT_SUCCESS);
    CHECK(refuse(&o) == EXIT_SUCCESS);
    CHECK(release_family(C2_RESPONSE_SEND, 1) == 1);
    CHECK(admission_released(&token) == EXIT_SUCCESS);
    CHECK(c2_requests(&o, 0U) == EXIT_SUCCESS);
    CHECK(c2_requests(&other, 1U) == EXIT_SUCCESS);
    CHECK(release_family(C2_RESPONSE_SEND, 1) == 1);
    CHECK(ams_mel_ir_c2_system_response_request_wait(independent, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_system_response_request_close(&independent, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_admission(NULL, 2, &token, NULL, NULL) && token == NULL);
    CHECK(close_owner(&other) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int open_track(const ams_mel_session *session, ams_mel_ir_track **track)
{
    ams_mel_ir_track_config_v1 value;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    value.channel_id.descriptive_label = text("track");
    value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    CHECK(ams_mel_ir_track_open(session, &value, track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_enable(*track, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int track_requests(const ams_mel_ir_track *track, size_t expected)
{
    size_t count = 99;
    CHECK(ams_mel_test_track_requests(track, &count));
    if (count != expected) fprintf(stderr, "Track requests %zu != %zu\n", count, expected);
    CHECK(count == expected);
    return EXIT_SUCCESS;
}

/* Both upstream overloads pending concurrently in one Session. Each request
 * is counted only in its own channel's accounting domain and probe family,
 * released only by its own provider gate, and closing either channel defers
 * only that channel. Proves the C2 path never uses Track dispatch/state. */
static int test_independent_c2_and_track(void)
{
    char log[] = "/tmp/ams-c2-sys-mixed-XXXXXX";
    struct owners o;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_c2_system_response_request *c2_request = NULL;
    ams_mel_ir_track_system_response_request *track_request = NULL;
    ams_mel_ir_c2_system_response_result_v1 c2_result;
    ams_mel_ir_track_system_response_result_v1 track_result;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    uint64_t c2_base = 0, track_base = 0, observed = 0;
    unsigned before;
    void *pin;
    CHECK(quiesce(C2_RESPONSE_PROBE, &c2_base) == EXIT_SUCCESS);
    CHECK(quiesce(TRACK_PROBE, &track_base) == EXIT_SUCCESS);
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-mixed", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(open_track(o.session, &track) == EXIT_SUCCESS);
    before = sends();
    /* C2 overload: only the C2 domain and C2 send observe it. */
    CHECK(submit(&o, &v, &c2_request, NULL, 0) == AMS_MEL_OK);
    CHECK(sends() == before + 1U);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    CHECK(track_requests(track, 0U) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_system_track_data_response_sent") == 1U);
    CHECK(occurrences(log, "track_system_track_data_response_sent") == 0U);
    CHECK(probe_value(C2_RESPONSE_PROBE, 2U) == c2_base + 1U);
    CHECK(probe_value(TRACK_PROBE, 2U) == track_base);
    /* Track overload: only the Track domain observes it; C2 send unchanged. */
    CHECK(ams_mel_ir_track_submit_system_track_data_response(track, &v, &track_request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(sends() == before + 1U);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    CHECK(track_requests(track, 1U) == EXIT_SUCCESS);
    CHECK(occurrences(log, "track_system_track_data_response_sent") == 1U);
    CHECK(probe_value(C2_RESPONSE_PROBE, 2U) == c2_base + 1U);
    CHECK(probe_value(TRACK_PROBE, 2U) == track_base + 1U);
    /* Release only the Track gate: the C2 request stays pending. */
    CHECK(release_family(TRACK_SEND, 1) == 1);
    CHECK(ams_mel_ir_track_system_response_request_wait(track_request, 5000, &track_result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track_result.status.command_id == UINT32_C(0xA1B2C3D4));
    CHECK(ams_mel_test_completion_owner(TRACK_PROBE, track_base + 1U, &observed));
    CHECK(track_requests(track, 0U) == EXIT_SUCCESS);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_system_response_request_wait(c2_request, 0, &c2_result, NULL, 0, NULL) ==
          AMS_MEL_TIMEOUT);
    CHECK(probe_value(C2_RESPONSE_PROBE, 3U) == c2_base);
    /* Closing Track tears Track down immediately; pending C2 is unaffected. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(occurrences(log, "track_channel_destroyed") == 1U);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    /* Closing C2 with its own request pending defers only C2 teardown. */
    CHECK(ams_mel_ir_c2_close(&o.c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(release_family(C2_RESPONSE_SEND, 1) == 1);
    CHECK(ams_mel_ir_c2_system_response_request_wait(c2_request, 5000, &c2_result, NULL, 0, NULL) ==
          AMS_MEL_OK);
    CHECK(c2_result.status.command_id == v.command_id);
    CHECK(ams_mel_test_completion_owner(C2_RESPONSE_PROBE, c2_base + 1U, &observed));
    CHECK(occurrences(log, "c2_channel_destroyed") == 1U);
    CHECK(ams_mel_ir_c2_system_response_request_close(&c2_request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_system_response_request_close(&track_request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(dlclose(pin) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(occurrences(log, "c2_system_track_data_response_sent") == 1U);
    CHECK(occurrences(log, "track_system_track_data_response_sent") == 1U);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* Isolated process: allocation or worker-launch failure after provider send
 * reports INTERNAL_ERROR, publishes no request and retains the accounted
 * future and provider graph rather than releasing or unloading it. */
static int isolated_post_send(const char *failure)
{
    char log[] = "/tmp/ams-c2-sys-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("c2-sys-held", 0, 1, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(submit(&o, &v, &request, d, sizeof d) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_C2_SYSTEM_RESPONSE_POST_SEND_FAILURE") == 0);
    CHECK(sends() == 1U);
    CHECK(c2_requests(&o, 1U) == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, "c2_system_track_data_response_sent") == 1U);
    CHECK(occurrences(log, "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated C2 SystemTrackDataResponse post-send %s retention\n", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}

/* Isolated process: deferred cleanup failure is never reported as success,
 * is cached, and never resends. */
static int isolated_finish_failure(void)
{
    struct owners o;
    ams_mel_ir_c2_system_response_request *request = NULL;
    ams_mel_ir_c2_system_response_result_v1 result;
    const ams_mel_ir_system_track_data_response_v1 v = rich();
    char d[96];
    void *pin;
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    CHECK(open_owner("c2-sys-held", 0, 1, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(&o, &v, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(C2_RESPONSE_SEND, 1) == 1);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "deferred C2 cleanup failed") == 0);
    CHECK(result.status.state == 0U && result.status.reason_description.size == 0U);
    CHECK(ams_mel_ir_c2_system_response_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(sends() == 1U);
    CHECK(ams_mel_ir_c2_system_response_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated C2 SystemTrackDataResponse deferred-cleanup failure\n");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "post-send") == 0)
        return isolated_post_send(argv[2]);
    if (argc == 2 && strcmp(argv[1], "finish-failure") == 0)
        return isolated_finish_failure();
    CHECK(test_input_fidelity() == EXIT_SUCCESS);
    CHECK(test_invalid() == EXIT_SUCCESS);
    CHECK(test_response_fidelity() == EXIT_SUCCESS);
    CHECK(test_outcomes() == EXIT_SUCCESS);
    CHECK(test_send_failures_and_lifecycle() == EXIT_SUCCESS);
    CHECK(test_timeout_then_success() == EXIT_SUCCESS);
    CHECK(test_pending_close() == EXIT_SUCCESS);
    CHECK(test_admission() == EXIT_SUCCESS);
    CHECK(test_independent_c2_and_track() == EXIT_SUCCESS);
    puts("PASS: C mock C2 SystemTrackDataResponse contract");
    return EXIT_SUCCESS;
}
