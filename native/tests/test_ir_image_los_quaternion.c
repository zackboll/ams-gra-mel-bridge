#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-010: compiled-C11 contract for opt-in Image LineOfSightQuaternion
 * reception. Every ordering is established by mock hooks or test-facade
 * probes, never by sleeping. */
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <math.h>
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

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)

/* Pre-existing layouts unchanged; the new record fixed on LP64. */
_Static_assert(sizeof(ams_mel_ir_quaternion_v1) == 32, "canonical quaternion unchanged");
_Static_assert(sizeof(ams_mel_ir_forward_right_down_v1) == 24, "canonical FRD unchanged");
_Static_assert(sizeof(ams_mel_ir_image_metadata_event_v1) == 192, "image v1 unchanged");
_Static_assert(sizeof(ams_mel_ir_line_of_sight_euler_v1) == 56, "Euler v1 unchanged");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "counters unchanged");
_Static_assert(sizeof(ams_mel_ir_optical_distortion_map_v1) == 88, "ODM unchanged");
_Static_assert(sizeof(ams_mel_ir_line_of_sight_quaternion_v1) == 64, "LOS quaternion");
_Static_assert(offsetof(ams_mel_ir_line_of_sight_quaternion_v1, q_xyzw) == 8, "quaternion");
_Static_assert(offsetof(ams_mel_ir_line_of_sight_quaternion_v1, pqr_rps) == 40, "rates");

extern int ams_mel_test_image_los_quaternion_probe(unsigned operation, size_t value);
#define PROBE ams_mel_test_image_los_quaternion_probe
typedef int (*deliver_fn)(unsigned, uint32_t, unsigned);
typedef unsigned (*registrations_fn)(void);
static deliver_fn deliver;
static registrations_fn registrations;
static registrations_fn odm_registrations;

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

static uint64_t bits(double value)
{ uint64_t result; memcpy(&result, &value, sizeof result); return result; }
/* Bit-exact for every finite value and infinity, so signed zero cannot be
 * confused with zero; NaN only by classification (payload not promised). */
static int same(double actual, double expected)
{ return isnan(expected) ? isnan(actual) : bits(actual) == bits(expected); }

struct owners { ams_mel_session *session; ams_mel_ir_stream *stream; };

static int open_owner(const char *scenario, struct owners *o)
{
    ams_mel_ir_stream_config_v1 value;
    memset(o, 0, sizeof *o);
    memset(&value, 0, sizeof value);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &o->session, NULL, 0, NULL) == AMS_MEL_OK);
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    value.channel_id.descriptive_label = text("image");
    value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    value.buffer_count = 3; value.buffer_size = 64; value.queue_capacity = 2;
    CHECK(ams_mel_ir_stream_open(o->session, &value, &o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* The provider is loaded per Session, so its hooks are resolved again for
     * every Session and never reused after that Session closes. */
    {
        void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
        CHECK(library != NULL);
        *(void **)(&deliver) = dlsym(library, "mock_losq_deliver");
        *(void **)(&registrations) = dlsym(library, "mock_losq_registrations");
        *(void **)(&odm_registrations) = dlsym(library, "mock_odm_registrations");
        CHECK(deliver != NULL && registrations != NULL && odm_registrations != NULL);
        CHECK(dlclose(library) == 0);
    }
    return EXIT_SUCCESS;
}

static int close_owner(struct owners *o)
{
    if (o->stream) CHECK(ams_mel_ir_stream_close(&o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&o->session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int counters_are(const ams_mel_ir_image_los_quaternion_metadata *m, uint64_t received,
                        uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 c;
    memset(&c, 0xff, sizeof c);
    CHECK(ams_mel_ir_image_los_quaternion_get_counters(m, &c, NULL, 0, NULL) == AMS_MEL_OK);
    if (c.events_received != received || c.events_dropped_queue_full != dropped ||
        c.malformed_or_unsupported != malformed)
        fprintf(stderr, "counters %llu/%llu/%llu, expected %llu/%llu/%llu\n",
                (unsigned long long)c.events_received, (unsigned long long)c.events_dropped_queue_full,
                (unsigned long long)c.malformed_or_unsupported, (unsigned long long)received,
                (unsigned long long)dropped, (unsigned long long)malformed);
    CHECK(c.events_received == received);
    CHECK(c.events_dropped_queue_full == dropped);
    CHECK(c.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

static int take(ams_mel_ir_image_los_quaternion_metadata *m,
                ams_mel_ir_image_los_quaternion_event **event,
                const ams_mel_ir_line_of_sight_quaternion_v1 **view)
{
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*event != NULL);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(*event, view, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}
/* Exact signed time and every component, each compared by name. */
static int check(const ams_mel_ir_line_of_sight_quaternion_v1 *v, int64_t time, double x, double y,
                 double z, double w, double forward, double right, double down)
{
    CHECK(v->system_time_ns == time);
    CHECK(same(v->q_xyzw.x, x));
    CHECK(same(v->q_xyzw.y, y));
    CHECK(same(v->q_xyzw.z, z));
    CHECK(same(v->q_xyzw.w, w));
    CHECK(same(v->pqr_rps.forward, forward));
    CHECK(same(v->pqr_rps.right, right));
    CHECK(same(v->pqr_rps.down, down));
    return EXIT_SUCCESS;
}

/* Fixture 0: default-constructed upstream value, everything zero. */
static int check_zero(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{ return check(v, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0); }

/* Fixture 1: INT64_MIN time; asymmetric near-unit quaternion; small and
 * negative rates. Fixture 2 is its opposite-sign representation, time -1; it
 * must stay negated, never canonicalized to fixture 1. */
#define Q1W 0.8227241335952167
static int check_first(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{ return check(v, INT64_MIN, 0.125, -0.25, 0.5, Q1W, 1.0e-9, -2.5e-3, 0.0625); }
static int check_opposite(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{ return check(v, -1, -0.125, 0.25, -0.5, -Q1W, -1.0e-9, 2.5e-3, -0.0625); }

/* Fixture 3: time 0; a non-unit quaternion (norm^2 = 348) delivered unchanged,
 * never normalized; large and negative rates delivered unclamped. */
static int check_non_unit(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{
    const double norm2 = v->q_xyzw.x * v->q_xyzw.x + v->q_xyzw.y * v->q_xyzw.y +
                         v->q_xyzw.z * v->q_xyzw.z + v->q_xyzw.w * v->q_xyzw.w;
    CHECK(norm2 == 348.0);
    return check(v, 0, 3.0, -7.0, 11.0, -13.0, 6283.185307179586, -1.5e6, 42.0);
}

/* Fixture 4: time 1; signed zero only in x and in the down rate. */
static int check_signed_zero(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{
    CHECK(signbit(v->q_xyzw.x) && v->q_xyzw.x == 0.0);
    CHECK(signbit(v->pqr_rps.down) && v->pqr_rps.down == 0.0);
    return check(v, 1, -0.0, 0.375, -0.625, 0.75, 0.875, -1.125, -0.0);
}

/* Fixture 5: INT64_MAX time; NaN, +inf, -inf in the quaternion and in the
 * rates, never replaced; finite w stays exact. */
static int check_non_finite(const ams_mel_ir_line_of_sight_quaternion_v1 *v)
{
    CHECK(isnan(v->q_xyzw.x) && isinf(v->q_xyzw.y) && v->q_xyzw.y > 0.0);
    CHECK(isinf(v->q_xyzw.z) && v->q_xyzw.z < 0.0);
    CHECK(isinf(v->pqr_rps.forward) && v->pqr_rps.forward < 0.0);
    CHECK(isnan(v->pqr_rps.right) && isinf(v->pqr_rps.down) && v->pqr_rps.down > 0.0);
    return check(v, INT64_MAX, NAN, INFINITY, -INFINITY, 0.5, -INFINITY, NAN, INFINITY);
}

/* Fixture 6: subnormal and extreme finite values; time = tag. */
#define TINY 4.9406564584124654e-324
#define BIG 1.7976931348623157e308
static int check_extreme(const ams_mel_ir_line_of_sight_quaternion_v1 *v, uint32_t tag)
{ return check(v, (int64_t)tag, TINY, -TINY, BIG, -BIG, -TINY, BIG, -BIG); }

/* Fixture 7: tagged; time = tag and x = tag. */
static int check_tagged(const ams_mel_ir_line_of_sight_quaternion_v1 *v, uint32_t tag)
{ return check(v, (int64_t)tag, (double)tag, 0.5, -0.5, 0.25, -4.0, 8.0, -16.0); }
/* Synchronous delivery during registration (fixtures 0..3), then on-demand
 * delivery of 4..7. The mock scribbles over and destroys each payload after
 * the callback returns, so every view here is an owned copy. Every event is
 * read again after the owner, the Image stream and the Session have closed and
 * the provider library has unloaded. */
static int test_fidelity(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *events[8] = {0};
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner("losq-sync", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 8, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS && m != NULL);
    CHECK(counters_are(m, 4, 0, 0) == EXIT_SUCCESS);
    CHECK(deliver(4U, 0U, 0U) == 1);
    CHECK(deliver(5U, 0U, 0U) == 1);
    CHECK(deliver(6U, 106U, 0U) == 1);
    CHECK(deliver(7U, 107U, 0U) == 1);
    CHECK(counters_are(m, 8, 0, 0) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 8U; ++i) CHECK(take(m, &events[i], &v) == EXIT_SUCCESS);
    {
        ams_mel_ir_image_los_quaternion_event *none = NULL;
        CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &none, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(none == NULL);
    }
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK && m == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    /* All owners closed and the provider unloaded: events remain valid. */
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[0], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_zero(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[1], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_first(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[2], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_opposite(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[3], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_non_unit(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[4], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_signed_zero(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[5], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_non_finite(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[6], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_extreme(v, 106U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(events[7], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_tagged(v, 107U) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 8U; ++i) {
        CHECK(ams_mel_ir_image_los_quaternion_event_close(&events[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(events[i] == NULL);
    }
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&events[0], NULL, 0, NULL) == AMS_MEL_OK);
    puts("PASS: LOS quaternion time/xyzw/FRD fidelity outlives payload and all owners");
    return EXIT_SUCCESS;
}

/* NotSupported, Fail, NotImplemented, unknown Return and exception are each
 * distinguishable; every attempt (success or failure) consumes the one shot,
 * publishes no owner on failure, and a retained callback stays inactive. */
static int registration_case(const char *scenario, ams_mel_status_t status, int has_return,
                             ams_mel_ir_return_t expected, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_return_t reg = 99U;
    char d[160] = {0};
    unsigned before;
    CHECK(open_owner(scenario, &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 4, &m, &reg, d, sizeof d, NULL) == status);
    CHECK(m == NULL && registrations() == before + 1U);
    CHECK(reg == (has_return ? expected : 99U));
    CHECK(strcmp(d, diagnostic) == 0);
    /* One-shot after failure: refused before any provider call. */
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 4, &m, &reg, d, sizeof d, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && registrations() == before + 1U);
    CHECK(strcmp(d, "Image stream is not eligible or LineOfSightQuaternion was already attempted") == 0);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_registration_outcomes(void)
{
    CHECK(registration_case("image-metadata-sync", AMS_MEL_PROVIDER_FAILED, 1,
          AMS_MEL_IR_RETURN_NOT_SUPPORTED,
          "provider does not support LineOfSightQuaternion callback") == EXIT_SUCCESS);
    CHECK(registration_case("losq-register-fail", AMS_MEL_PROVIDER_FAILED, 1, AMS_MEL_IR_RETURN_FAIL,
          "LineOfSightQuaternion callback registration failed") == EXIT_SUCCESS);
    CHECK(registration_case("losq-register-not-implemented", AMS_MEL_PROVIDER_FAILED, 1,
          AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
          "LineOfSightQuaternion callback registration failed") == EXIT_SUCCESS);
    CHECK(registration_case("losq-register-unknown", AMS_MEL_PROVIDER_FAILED, 0, 0U,
          "provider returned an unknown LineOfSightQuaternion registration result") == EXIT_SUCCESS);
    CHECK(registration_case("losq-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0, 0U,
          "provider exception during LineOfSightQuaternion registration") == EXIT_SUCCESS);
    puts("PASS: LOS quaternion NotSupported/Fail/NotImplemented/unknown/exception distinct, one-shot");
    return EXIT_SUCCESS;
}
/* The quaternion one-shot is independent of the four-kind metadata, NUC,
 * CameraCommandResp and OpticalDistortionMap owners in both directions; none
 * of those owners ever registers the quaternion overload, and the quaternion
 * owner never registers ODM. The base four-kind registration is unchanged. */
static int test_independence(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_metadata *again = NULL;
    ams_mel_ir_image_metadata *base = NULL;
    ams_mel_ir_nuc_metadata *nuc = NULL;
    ams_mel_ir_camera_response_metadata *camera = NULL;
    ams_mel_ir_image_odm_metadata *odm = NULL;
    ams_mel_ir_metadata_counters_v1 c;
    ams_mel_ir_return_t reg = 99U;
    unsigned before, odm_before;

    /* Base metadata on a provider WITHOUT quaternion LOS: never touches it,
     * receives exactly its existing four-kind mix, and is unaffected by the
     * quaternion NotSupported attempt. */
    CHECK(open_owner("image-metadata-four-mixed", &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_metadata_open(o.stream, 4, &base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(registrations() == before);
    CHECK(ams_mel_ir_image_metadata_get_counters(base, &c, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(c.events_received == 6U && c.events_dropped_queue_full == 2U &&
          c.malformed_or_unsupported == 0U);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 4, &m, &reg, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(reg == AMS_MEL_IR_RETURN_NOT_SUPPORTED && m == NULL && registrations() == before + 1U);
    CHECK(ams_mel_ir_image_metadata_get_counters(base, &c, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(c.events_received == 6U && c.events_dropped_queue_full == 2U);
    {
        ams_mel_ir_image_metadata_event *event = NULL;
        const ams_mel_ir_image_metadata_event_v1 *view = NULL;
        CHECK(ams_mel_ir_image_metadata_receive(base, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_image_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->kind == AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST);
        CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_image_metadata_close(&base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    /* On a quaternion-capable provider: base metadata, NUC, CameraCommandResp
     * and ODM opens do not register the quaternion overload and do not
     * consume its one-shot. */
    CHECK(open_owner("losq-sync", &o) == EXIT_SUCCESS);
    before = registrations();
    odm_before = odm_registrations();
    CHECK(ams_mel_ir_image_metadata_open(o.stream, 8, &base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_nuc_open(o.stream, 2, &nuc, NULL, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &camera, NULL, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 2, &odm, NULL, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(odm_registrations() == odm_before + 1U);
    CHECK(registrations() == before);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 8, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS && registrations() == before + 1U);
    /* Quaternion registration did not register ODM either. */
    CHECK(odm_registrations() == odm_before + 1U);
    CHECK(counters_are(m, 4, 0, 0) == EXIT_SUCCESS);
    /* One-shot after success. */
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 8, &again, &reg, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(again == NULL && registrations() == before + 1U);
    /* Quaternion delivery never appears on the base owner, and closing the
     * quaternion owner never affects the base owner. */
    CHECK(ams_mel_ir_image_metadata_get_counters(base, &c, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const uint64_t base_received = c.events_received;
        CHECK(deliver(7U, 3U, 0U) == 1);
        CHECK(counters_are(m, 5, 0, 0) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_image_metadata_get_counters(base, &c, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(c.events_received == base_received);
    }
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion owner independent of base metadata, NUC, CameraCommandResp, ODM");
    return EXIT_SUCCESS;
}

/* Registration on a Running stream; ineligible once logically stopped. */
static int test_running_and_eligibility(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    unsigned before;
    CHECK(open_owner("losq-running", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 0, 0, 0) == EXIT_SUCCESS);
    CHECK(deliver(7U, 21U, 0U) == 1);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);
    CHECK(check_tagged(v, 21U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner("losq-running", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    before = registrations();
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 2, &m, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && registrations() == before);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion registers on Running streams and refuses stopped streams");
    return EXIT_SUCCESS;
}
static void *deliver_when_receive_waits(void *argument)
{
    const uint32_t tag = *(const uint32_t *)argument;
    if (!PROBE(7U, 0U)) abort();
    if (deliver(7U, tag, 0U) != 1) abort();
    return NULL;
}

/* Capacity one, bounded DROP-INCOMING with saturating counters. A full queue
 * counts a non-null payload queue-full without allocating a snapshot (the
 * armed allocation failure stays armed and no copy is attempted) and still
 * counts a null payload malformed. An allocation failure rejects one event
 * atomically; reception then recovers. */
static int test_capacity_one(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    pthread_t thread;
    uint32_t tag = 31U;
    int copies;
    CHECK(open_owner("losq-capacity", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(deliver(7U, 1U, 0U) == 1);                         /* copied, queue now full */
    CHECK(counters_are(m, 1, 0, 0) == EXIT_SUCCESS);
    copies = PROBE(4U, 0U);
    CHECK(PROBE(5U, 1U) == 1);                               /* arm allocation failure */
    CHECK(deliver(1U, 0U, 0U) == 1);                         /* valid, full: dropped */
    CHECK(counters_are(m, 2, 1, 0) == EXIT_SUCCESS);
    CHECK(deliver(0U, 0U, 1U) == 1);                         /* null while full: malformed */
    CHECK(counters_are(m, 3, 1, 1) == EXIT_SUCCESS);
    CHECK(deliver(3U, 0U, 0U) == 1);                         /* non-unit, full: dropped */
    CHECK(counters_are(m, 4, 2, 1) == EXIT_SUCCESS);
    CHECK(PROBE(6U, 0U) == 1);                               /* still armed */
    CHECK(PROBE(4U, 0U) == copies);                          /* no snapshot was allocated */
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);              /* original retained */
    CHECK(check_tagged(v, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    /* With space: a null payload is malformed and allocates nothing. */
    CHECK(deliver(0U, 0U, 1U) == 1);
    CHECK(counters_are(m, 5, 2, 2) == EXIT_SUCCESS);
    CHECK(PROBE(6U, 0U) == 1 && PROBE(4U, 0U) == copies);
    /* The allocation is attempted and its failure rejects the whole event;
     * nothing partial is published. */
    CHECK(deliver(1U, 0U, 0U) == 1);
    CHECK(PROBE(6U, 0U) == 0);
    CHECK(PROBE(4U, 0U) == copies + 1);
    CHECK(counters_are(m, 6, 2, 3) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT &&
          !event);
    /* Recovery: the same payload now copies completely. */
    CHECK(deliver(1U, 0U, 0U) == 1);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);
    CHECK(check_first(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 7, 2, 3) == EXIT_SUCCESS);
    /* Finite timeout on an active empty queue is TIMEOUT, not cancellation. */
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 20, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT &&
          !event);
    /* A blocked Receive is woken by delivery (ordered by the probe). */
    CHECK(PROBE(8U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, deliver_when_receive_waits, &tag) == 0);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 30000, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(event, &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_tagged(v, 31U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 8, 2, 3) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion capacity-one queue, full-queue allocation avoidance, recovery");
    return EXIT_SUCCESS;
}

/* A larger queue keeps FIFO order up to capacity and drops only the excess. */
static int test_capacity_fifo(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    int copies;
    CHECK(open_owner("losq-capacity", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 3, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    copies = PROBE(4U, 0U);
    for (uint32_t tag = 40U; tag < 45U; ++tag) CHECK(deliver(7U, tag, 0U) == 1);
    CHECK(counters_are(m, 5, 2, 0) == EXIT_SUCCESS);
    CHECK(PROBE(4U, 0U) == copies + 3);                      /* only queued events copied */
    for (uint32_t tag = 40U; tag < 43U; ++tag) {
        CHECK(take(m, &event, &v) == EXIT_SUCCESS);
        CHECK(check_tagged(v, tag) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion larger queue preserves FIFO order and drops only the excess");
    return EXIT_SUCCESS;
}

static int make_log(char *path)
{
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    return EXIT_SUCCESS;
}

static int append_log(const char *path, const char *value)
{
    FILE *file = fopen(path, "ab");
    CHECK(file != NULL);
    CHECK(fprintf(file, "%s\n", value) > 0);
    CHECK(fclose(file) == 0);
    return EXIT_SUCCESS;
}

/* The named records occur in this relative order (other records may interleave). */
static int log_order(const char *path, const char *const *order, size_t count)
{
    char line[256];
    size_t next = 0;
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    while (next < count && fgets(line, sizeof line, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, order[next]) == 0) ++next;
    }
    CHECK(fclose(file) == 0);
    if (next != count) fprintf(stderr, "missing ordered record: %s\n", order[next]);
    CHECK(next == count);
    return EXIT_SUCCESS;
}
static void *stop_when_receive_waits(void *argument)
{
    ams_mel_ir_stream *stream = argument;
    if (!PROBE(7U, 0U)) abort();
    if (ams_mel_ir_stream_stop(stream, NULL, 0, NULL) != AMS_MEL_OK) abort();
    return NULL;
}

/* Image Stop wakes a blocked receiver; Stop preserves queued events, which
 * drain before STREAM_STOPPED; a late callback during channel destruction is
 * counted but never queued and returns before the provider library unloads. */
static int test_stop_and_drain(void)
{
    char log[] = "/tmp/ams-losq-drain-XXXXXX";
    const char *order[] = {"losq_sync_delivered", "late_losq_callback_invoked",
                           "late_losq_callback_returned", "library_unloaded"};
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    pthread_t thread;
    /* A blocked Receive on an empty active queue is woken by Image Stop. */
    CHECK(open_owner("losq-running", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(PROBE(8U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, stop_when_receive_waits, o.stream) == 0);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 30000, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(pthread_join(thread, NULL) == 0 && event == NULL);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("losq-late", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 8, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_zero(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    /* Stop destroys the provider channel; its late callback during
     * destruction is counted but not queued; queued events still drain. */
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(deliver(7U, 8U, 0U) == 0);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_first(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_opposite(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_non_unit(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 50, &event, NULL, 0, NULL) ==
          AMS_MEL_STREAM_STOPPED);
    /* The late callback is counted, never queued. */
    CHECK(counters_are(m, 5, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: LOS quaternion Stop wakes receivers and queued events drain after Stop/Close");
    return EXIT_SUCCESS;
}

/* Public owner Close before Image Close: consumption is deactivated (Close
 * cannot unregister); events already received stay valid; the provider's late
 * callback during channel destruction is memory-safe and returns first. */
static int test_owner_closed_first(void)
{
    char log[] = "/tmp/ams-losq-closed-XXXXXX";
    const char *order[] = {"losq_owner_closed", "late_losq_callback_invoked",
                           "late_losq_callback_returned", "library_unloaded"};
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("losq-late-closed", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 8, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK && m == NULL);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(log, "losq_owner_closed") == EXIT_SUCCESS);
    CHECK(deliver(7U, 9U, 0U) == 1); /* callback still registered: ignored safely */
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(event, &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_zero(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: LOS quaternion owner Close before Image Close; late callback memory-safe");
    return EXIT_SUCCESS;
}

/* A failed provider detach leaves ownership uncertain: the owner fails, queued
 * events still drain, then Receive reports PROVIDER_FAILED. The stream retains
 * the provider graph until the retried detach succeeds. */
static int test_detach_failure(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    char d[128] = {0};
    CHECK(open_owner("losq-detach-fail", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_first(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_non_unit(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 10, &event, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(strcmp(d, "Image channel cleanup failed; LineOfSightQuaternion owner failed") == 0);
    CHECK(counters_are(m, 2, 0, 0) == EXIT_SUCCESS);
    /* The retained callback stays registered and memory-safe; never queued. */
    CHECK(deliver(7U, 5U, 0U) == 1);
    CHECK(counters_are(m, 3, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_close(&o.stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(o.stream == NULL);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &event, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion owner fails closed on provider detach failure and still drains");
    return EXIT_SUCCESS;
}
struct close_job { struct owners *owners; atomic_int returned; ams_mel_status_t status; };

static void *close_stream(void *argument)
{
    struct close_job *job = argument;
    job->status = ams_mel_ir_stream_close(&job->owners->stream, NULL, 0, NULL);
    atomic_store(&job->returned, 1);
    return NULL;
}

/* Isolated process. A provider callback is inside the bridge (counted in
 * flight) while the public owner and the Image stream close. The provider is
 * non-quiescing: channel destruction does NOT join its thread, so only the
 * bridge drain can hold physical teardown. Teardown must wait in that drain
 * until the callback is released, and the callback returns before the
 * provider library unloads. */
static int callback_in_flight_body(void)
{
    char log[] = "/tmp/ams-losq-in-flight-XXXXXX";
    const char *order[] = {"losq_in_flight_callback_entered", "channel_destroyed",
                           "losq_teardown_waiting_for_callback", "losq_callback_released",
                           "losq_in_flight_callback_returned", "library_unloaded"};
    struct owners o;
    struct close_job job;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    pthread_t thread;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(PROBE(0U, 0U) == 1);                                 /* park next callback */
    CHECK(open_owner("losq-in-flight", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(PROBE(1U, 0U) == 1);                                 /* inside the bridge */
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK); /* never waits */
    job.owners = &o; atomic_init(&job.returned, 0); job.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(pthread_create(&thread, NULL, close_stream, &job) == 0);
    CHECK(PROBE(3U, 0U) == 1);                                 /* teardown drains */
    CHECK(append_log(log, "losq_teardown_waiting_for_callback") == EXIT_SUCCESS);
    CHECK(atomic_load(&job.returned) == 0);
    CHECK(append_log(log, "losq_callback_released") == EXIT_SUCCESS);
    CHECK(PROBE(2U, 0U) == 1);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(job.status == AMS_MEL_OK && o.stream == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);                    /* unload joins provider thread */
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 6U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: LOS quaternion teardown drains an in-flight callback before releasing resources");
    return EXIT_SUCCESS;
}

/* Whatever the outcome, never leave the parked provider thread blocked: a
 * failed check must end the process with a diagnostic, not hang it at the
 * provider's unload-time join. */
static int callback_in_flight(void)
{
    const int result = callback_in_flight_body();
    (void)PROBE(2U, 0U);
    return result;
}

static int test_invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_image_los_quaternion_metadata *m = NULL;
    ams_mel_ir_image_los_quaternion_metadata *stale = (ams_mel_ir_image_los_quaternion_metadata *)&o;
    ams_mel_ir_image_los_quaternion_event *event = NULL;
    const ams_mel_ir_line_of_sight_quaternion_v1 *v = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    char d[4];
    unsigned before;
    CHECK(ams_mel_ir_image_los_quaternion_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_get_counters(NULL, &counters, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_event_view(NULL, &v, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, d, sizeof d, NULL) == AMS_MEL_OK);
    CHECK(open_owner("losq-sync", &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 1, NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 1, &stale, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 1, &m, NULL, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* Argument rejection does not consume the one shot or reach the provider;
     * capacity 1 keeps the first synchronous event and drops three. */
    CHECK(registrations() == before);
    CHECK(ams_mel_ir_image_los_quaternion_open(o.stream, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 4, 3, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_los_quaternion_get_counters(m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_zero(v) == EXIT_SUCCESS);
    {
        ams_mel_ir_image_los_quaternion_event *occupied = event;
        CHECK(ams_mel_ir_image_los_quaternion_receive(m, 0, &occupied, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_image_los_quaternion_event_view(event, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(ams_mel_ir_image_los_quaternion_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_los_quaternion_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: LOS quaternion argument validation");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "callback-in-flight") == 0) return callback_in_flight();
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_fidelity() == EXIT_SUCCESS);
    CHECK(test_registration_outcomes() == EXIT_SUCCESS);
    CHECK(test_independence() == EXIT_SUCCESS);
    CHECK(test_running_and_eligibility() == EXIT_SUCCESS);
    CHECK(test_capacity_one() == EXIT_SUCCESS);
    CHECK(test_capacity_fifo() == EXIT_SUCCESS);
    CHECK(test_stop_and_drain() == EXIT_SUCCESS);
    CHECK(test_owner_closed_first() == EXIT_SUCCESS);
    CHECK(test_detach_failure() == EXIT_SUCCESS);
    puts("PASS: C IR Image LineOfSightQuaternion contract");
    return EXIT_SUCCESS;
}
