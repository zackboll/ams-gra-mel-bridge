#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-009: compiled-C11 contract for opt-in Image OpticalDistortionMap
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

/* Pre-existing layouts unchanged; new records fixed on LP64. */
_Static_assert(sizeof(ams_mel_ir_row_col_v1) == 16, "canonical RowCol unchanged");
_Static_assert(sizeof(ams_mel_ir_image_metadata_event_v1) == 192, "image v1 unchanged");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "counters unchanged");
_Static_assert(sizeof(ams_mel_ir_forward_right_down_v1) == 24, "FRD");
_Static_assert(offsetof(ams_mel_ir_forward_right_down_v1, forward) == 0, "FRD forward");
_Static_assert(offsetof(ams_mel_ir_forward_right_down_v1, right) == 8, "FRD right");
_Static_assert(offsetof(ams_mel_ir_forward_right_down_v1, down) == 16, "FRD down");
_Static_assert(sizeof(ams_mel_ir_optical_distortion_map_v1) == 88, "ODM");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, right_min) == 24, "ODM extrema");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, frd) == 56, "ODM FRD");
_Static_assert(offsetof(ams_mel_ir_optical_distortion_map_v1, row_col) == 72, "ODM RowCol");

extern int ams_mel_test_image_odm_probe(unsigned operation, size_t value);
typedef int (*deliver_fn)(unsigned, uint32_t, unsigned);
typedef unsigned (*registrations_fn)(void);
static deliver_fn deliver;
static registrations_fn registrations;

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
        *(void **)(&deliver) = dlsym(library, "mock_odm_deliver");
        *(void **)(&registrations) = dlsym(library, "mock_odm_registrations");
        CHECK(deliver != NULL && registrations != NULL);
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

static int counters_are(const ams_mel_ir_image_odm_metadata *m, uint64_t received,
                        uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 c;
    memset(&c, 0xff, sizeof c);
    CHECK(ams_mel_ir_image_odm_get_counters(m, &c, NULL, 0, NULL) == AMS_MEL_OK);
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

static int take(ams_mel_ir_image_odm_metadata *m, ams_mel_ir_image_odm_event **event,
                const ams_mel_ir_optical_distortion_map_v1 **view)
{
    CHECK(ams_mel_ir_image_odm_receive(m, 0, event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*event != NULL);
    CHECK(ams_mel_ir_image_odm_event_view(*event, view, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int scalars(const ams_mel_ir_optical_distortion_map_v1 *v, uint32_t width, uint32_t height,
                   uint32_t right, uint32_t down, uint32_t pixels, double rmin, double rmax,
                   double dmin, double dmax)
{
    CHECK(v->width == width && v->height == height);
    CHECK(v->right_size == right && v->down_size == down && v->pixels_per_odm == pixels);
    CHECK(same(v->right_min, rmin));
    CHECK(same(v->right_max, rmax));
    CHECK(same(v->down_min, dmin));
    CHECK(same(v->down_max, dmax));
    return EXIT_SUCCESS;
}

static int frd_is(const ams_mel_ir_optical_distortion_map_v1 *v, size_t index, double f, double r,
                  double d)
{
    CHECK(index < v->frd.size && v->frd.data != NULL);
    CHECK(same(v->frd.data[index].forward, f));
    CHECK(same(v->frd.data[index].right, r));
    CHECK(same(v->frd.data[index].down, d));
    return EXIT_SUCCESS;
}

static int rc_is(const ams_mel_ir_optical_distortion_map_v1 *v, size_t index, double row, double col)
{
    CHECK(index < v->row_col.size && v->row_col.data != NULL);
    CHECK(same(v->row_col.data[index].row, row));
    CHECK(same(v->row_col.data[index].column, col));
    return EXIT_SUCCESS;
}

/* Fixture 0: empty map, every scalar zero, both collections {NULL, 0}. */
static int check_empty(const ams_mel_ir_optical_distortion_map_v1 *v)
{
    CHECK(scalars(v, 0U, 0U, 0U, 0U, 0U, 0.0, 0.0, 0.0, 0.0) == EXIT_SUCCESS);
    CHECK(v->frd.size == 0U && v->frd.data == NULL);
    CHECK(v->row_col.size == 0U && v->row_col.data == NULL);
    return EXIT_SUCCESS;
}

/* Fixture 1: uint32 max/zero, signed zero, +/-DBL_MAX, denormal; four FRD with
 * an adjacent duplicate, three RowCol: lengths differ and match no dimension
 * product. Every element and component is checked in order. */
static int check_rich(const ams_mel_ir_optical_distortion_map_v1 *v)
{
    const double big = 1.7976931348623157e308;
    CHECK(scalars(v, UINT32_MAX, 0U, 0x80000001U, 1U, 0x7fffffffU, -0.0, big, -big,
                  4.9406564584124654e-324) == EXIT_SUCCESS);
    CHECK(signbit(v->right_min) && v->right_min == 0.0);
    CHECK(v->frd.size == 4U && v->row_col.size == 3U);
    CHECK(frd_is(v, 0, 1.0, 0.0, 0.0) == EXIT_SUCCESS);
    CHECK(frd_is(v, 1, 0.5, -0.25, 0.125) == EXIT_SUCCESS);
    CHECK(frd_is(v, 2, 0.5, -0.25, 0.125) == EXIT_SUCCESS); /* duplicate kept */
    CHECK(frd_is(v, 3, -0.75, 1.0e-300, -2.5) == EXIT_SUCCESS);
    CHECK(rc_is(v, 0, 0.0, 639.5) == EXIT_SUCCESS);
    CHECK(rc_is(v, 1, 479.25, -0.0) == EXIT_SUCCESS);
    CHECK(rc_is(v, 2, 1.0e300, -1.0e-300) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Fixture 2: FRD-only. Fixture 3: RowCol-only. */
static int check_frd_only(const ams_mel_ir_optical_distortion_map_v1 *v)
{
    CHECK(scalars(v, 640U, 480U, 33U, 25U, 20U, -0.5, 0.5, -0.375, 0.375) == EXIT_SUCCESS);
    CHECK(v->frd.size == 1U && v->row_col.size == 0U && v->row_col.data == NULL);
    CHECK(frd_is(v, 0, 0.25, 0.5, 0.75) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int check_rowcol_only(const ams_mel_ir_optical_distortion_map_v1 *v)
{
    CHECK(scalars(v, 1U, 2U, 3U, 4U, 5U, 1.0, 2.0, 3.0, 4.0) == EXIT_SUCCESS);
    CHECK(v->frd.size == 0U && v->frd.data == NULL && v->row_col.size == 1U);
    CHECK(rc_is(v, 0, 12.5, 7.25) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Fixture 4: non-finite values forwarded as IEEE values, never zeroed. */
static int check_non_finite(const ams_mel_ir_optical_distortion_map_v1 *v)
{
    CHECK(isinf(v->right_min) && !signbit(v->right_min));
    CHECK(isinf(v->right_max) && signbit(v->right_max));
    CHECK(isnan(v->down_min));
    CHECK(v->down_max == 0.0 && signbit(v->down_max));
    CHECK(v->frd.size == 1U && v->row_col.size == 2U);
    CHECK(isnan(v->frd.data[0].forward));
    CHECK(isinf(v->frd.data[0].right) && !signbit(v->frd.data[0].right));
    CHECK(isinf(v->frd.data[0].down) && signbit(v->frd.data[0].down));
    CHECK(isinf(v->row_col.data[0].row) && signbit(v->row_col.data[0].row));
    CHECK(isnan(v->row_col.data[0].column));
    CHECK(rc_is(v, 1, 0.0, -0.0) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Fixture 5 (tag): one FRD, four RowCol; fixture 6 (tag): one of each. */
static int check_tagged5(const ams_mel_ir_optical_distortion_map_v1 *v, uint32_t tag)
{
    CHECK(scalars(v, 5U, 5U, 5U, 5U, tag, -1.0, 1.0, -2.0, 2.0) == EXIT_SUCCESS);
    CHECK(v->frd.size == 1U && v->row_col.size == 4U);
    CHECK(frd_is(v, 0, (double)tag, 0.0, 0.0) == EXIT_SUCCESS);
    for (size_t i = 0; i < 4U; ++i)
        CHECK(rc_is(v, i, 1.0 + 2.0 * (double)i, 2.0 + 2.0 * (double)i) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int check_tagged6(const ams_mel_ir_optical_distortion_map_v1 *v, uint32_t tag)
{
    CHECK(scalars(v, tag, 6U, 6U, 6U, 6U, -6.0, 6.0, -6.5, 6.5) == EXIT_SUCCESS);
    CHECK(v->frd.size == 1U && v->row_col.size == 1U);
    CHECK(frd_is(v, 0, (double)tag, 1.0, 2.0) == EXIT_SUCCESS);
    CHECK(rc_is(v, 0, (double)tag, 3.0) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Synchronous delivery during registration on an Attached stream; complete
 * fidelity; values survive owner, stream, Session and provider unload. */
static int test_fidelity(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *events[8] = {0};
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    ams_mel_ir_image_odm_event *extra = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner("odm-sync", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 16, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS && m != NULL);
    CHECK(counters_are(m, 4, 0, 0) == EXIT_SUCCESS); /* delivered before open returned */
    for (unsigned fixture = 4U; fixture <= 6U; ++fixture)
        CHECK(deliver(fixture, 100U + fixture, 0U) == 1);
    for (unsigned i = 0; i < 7U; ++i) CHECK(take(m, &events[i], &v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, &extra, NULL, 0, NULL) == AMS_MEL_TIMEOUT && !extra);
    CHECK(counters_are(m, 7, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK && m == NULL);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    /* Every provider owner is gone; the events still own complete copies. */
    CHECK(ams_mel_ir_image_odm_event_view(events[0], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_empty(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[1], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_rich(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[2], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_frd_only(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[3], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_rowcol_only(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[4], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_non_finite(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[5], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_tagged5(v, 105U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_view(events[6], &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_tagged6(v, 106U) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 7U; ++i) {
        CHECK(ams_mel_ir_image_odm_event_close(&events[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(events[i] == NULL);
    }
    CHECK(ams_mel_ir_image_odm_event_close(&events[0], NULL, 0, NULL) == AMS_MEL_OK);
    puts("PASS: ODM complete scalar/FRD/RowCol fidelity outlives all provider owners");
    return EXIT_SUCCESS;
}

/* NotSupported, Fail, NotImplemented, unknown Return and exception are each
 * distinguishable; every attempt (success or failure) consumes the one shot,
 * publishes no owner on failure, and a retained callback stays inactive. */
static int registration_case(const char *scenario, ams_mel_status_t status, int has_return,
                             ams_mel_ir_return_t expected, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_return_t reg = 99U;
    char d[160] = {0};
    unsigned before;
    CHECK(open_owner(scenario, &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_odm_open(o.stream, 4, &m, &reg, d, sizeof d, NULL) == status);
    CHECK(m == NULL && registrations() == before + 1U);
    CHECK(reg == (has_return ? expected : 99U));
    CHECK(strcmp(d, diagnostic) == 0);
    /* One-shot after failure: refused before any provider call. */
    CHECK(ams_mel_ir_image_odm_open(o.stream, 4, &m, &reg, d, sizeof d, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && registrations() == before + 1U);
    CHECK(strcmp(d, "Image stream is not eligible or OpticalDistortionMap was already attempted") == 0);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_registration_outcomes(void)
{
    CHECK(registration_case("image-metadata-sync", AMS_MEL_PROVIDER_FAILED, 1,
          AMS_MEL_IR_RETURN_NOT_SUPPORTED,
          "provider does not support optional OpticalDistortionMap callback") == EXIT_SUCCESS);
    CHECK(registration_case("odm-register-fail", AMS_MEL_PROVIDER_FAILED, 1, AMS_MEL_IR_RETURN_FAIL,
          "OpticalDistortionMap callback registration failed") == EXIT_SUCCESS);
    CHECK(registration_case("odm-register-not-implemented", AMS_MEL_PROVIDER_FAILED, 1,
          AMS_MEL_IR_RETURN_NOT_IMPLEMENTED,
          "OpticalDistortionMap callback registration failed") == EXIT_SUCCESS);
    CHECK(registration_case("odm-register-unknown", AMS_MEL_PROVIDER_FAILED, 0, 0U,
          "provider returned an unknown OpticalDistortionMap registration result") == EXIT_SUCCESS);
    CHECK(registration_case("odm-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0, 0U,
          "provider exception during OpticalDistortionMap registration") == EXIT_SUCCESS);
    puts("PASS: ODM NotSupported/Fail/NotImplemented/unknown/exception distinct and one-shot");
    return EXIT_SUCCESS;
}

/* The ODM one-shot is independent of the four-kind metadata, NUC and
 * CameraCommandResp owners in both directions, and none of those owners ever
 * registers the ODM overload. A successful ODM open is also one-shot. */
static int test_independence(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_metadata *again = NULL;
    ams_mel_ir_image_metadata *base = NULL;
    ams_mel_ir_nuc_metadata *nuc = NULL;
    ams_mel_ir_camera_response_metadata *camera = NULL;
    ams_mel_ir_return_t reg = 99U;
    unsigned before;

    /* Base metadata opens on a provider WITHOUT ODM; it never touches ODM. */
    CHECK(open_owner("image-metadata-four-mixed", &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_metadata_open(o.stream, 8, &base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(registrations() == before);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 4, &m, &reg, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(reg == AMS_MEL_IR_RETURN_NOT_SUPPORTED && m == NULL && registrations() == before + 1U);
    {
        ams_mel_ir_image_metadata_event *event = NULL;
        CHECK(ams_mel_ir_image_metadata_receive(base, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_image_metadata_close(&base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    /* On an ODM-capable provider: base metadata, NUC and CameraCommandResp
     * opens do not register ODM, and do not consume the ODM one-shot. */
    CHECK(open_owner("odm-sync", &o) == EXIT_SUCCESS);
    before = registrations();
    CHECK(ams_mel_ir_image_metadata_open(o.stream, 8, &base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_nuc_open(o.stream, 2, &nuc, NULL, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &camera, NULL, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(registrations() == before);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 8, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS && registrations() == before + 1U);
    CHECK(counters_are(m, 4, 0, 0) == EXIT_SUCCESS);
    /* One-shot after success. */
    CHECK(ams_mel_ir_image_odm_open(o.stream, 8, &again, &reg, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(again == NULL && registrations() == before + 1U);
    /* Closing the ODM owner never affects the base owner. */
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    {
        ams_mel_ir_image_metadata_event *event = NULL;
        CHECK(ams_mel_ir_image_metadata_receive(base, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    }
    CHECK(ams_mel_ir_image_metadata_close(&base, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: ODM owner independent of base metadata, NUC and CameraCommandResp owners");
    return EXIT_SUCCESS;
}

/* Registration on a Running stream; ineligible once logically stopped. */
static int test_running_and_eligibility(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    unsigned before;
    CHECK(open_owner("odm-running", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 0, 0, 0) == EXIT_SUCCESS);
    CHECK(deliver(6U, 21U, 0U) == 1);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);
    CHECK(check_tagged6(v, 21U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner("odm-running", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    before = registrations();
    CHECK(ams_mel_ir_image_odm_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(m == NULL && registrations() == before);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: ODM registers on Running streams and refuses stopped streams");
    return EXIT_SUCCESS;
}

static void *deliver_when_receive_waits(void *argument)
{
    const uint32_t tag = *(const uint32_t *)argument;
    if (!ams_mel_test_image_odm_probe(8U, 0U)) abort();
    if (deliver(6U, tag, 0U) != 1) abort();
    return NULL;
}

/* Bounded DROP-INCOMING with saturating counters. A full queue classifies a
 * valid map without constructing a snapshot (the armed copy failure stays
 * armed and no copy is attempted) yet still counts malformed maps as
 * malformed. Validation precedes allocation; a copy failure rejects the whole
 * event atomically; reception then recovers. */
static int test_capacity(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    pthread_t thread;
    uint32_t tag = 31U;
    int copies;
    CHECK(open_owner("odm-capacity", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(deliver(6U, 1U, 0U) == 1);                         /* copied, queue now full */
    CHECK(counters_are(m, 1, 0, 0) == EXIT_SUCCESS);
    copies = ams_mel_test_image_odm_probe(5U, 0U);
    CHECK(ams_mel_test_image_odm_probe(6U, 1U) == 1);        /* arm copy failure */
    CHECK(deliver(1U, 0U, 0U) == 1);                         /* valid, full: dropped */
    CHECK(counters_are(m, 2, 1, 0) == EXIT_SUCCESS);
    CHECK(deliver(0U, 0U, 1U) == 1);                         /* null while full: malformed */
    CHECK(counters_are(m, 3, 1, 1) == EXIT_SUCCESS);
    CHECK(ams_mel_test_image_odm_probe(4U, 3U) == 1);        /* element limit 3 */
    CHECK(deliver(5U, 2U, 0U) == 1);                         /* 4 RowCol > 3: malformed, not full */
    CHECK(counters_are(m, 4, 1, 2) == EXIT_SUCCESS);
    CHECK(deliver(1U, 0U, 0U) == 1);                         /* 4 FRD > 3: malformed */
    CHECK(counters_are(m, 5, 1, 3) == EXIT_SUCCESS);
    CHECK(deliver(6U, 2U, 0U) == 1);                         /* within limit, full: dropped */
    CHECK(counters_are(m, 6, 2, 3) == EXIT_SUCCESS);
    CHECK(ams_mel_test_image_odm_probe(7U, 0U) == 1);        /* still armed */
    CHECK(ams_mel_test_image_odm_probe(5U, 0U) == copies);   /* no snapshot was built */
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);              /* original retained */
    CHECK(check_tagged6(v, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    /* With space: invalid counts are rejected before allocation. */
    CHECK(deliver(5U, 3U, 0U) == 1);
    CHECK(counters_are(m, 7, 2, 4) == EXIT_SUCCESS);
    CHECK(ams_mel_test_image_odm_probe(7U, 0U) == 1 &&
          ams_mel_test_image_odm_probe(5U, 0U) == copies);
    /* Exactly at the limit is valid: the copy is attempted and its allocation
     * failure rejects the whole event; nothing partial is published. */
    CHECK(ams_mel_test_image_odm_probe(4U, 4U) == 1);
    CHECK(deliver(5U, 4U, 0U) == 1);
    CHECK(ams_mel_test_image_odm_probe(7U, 0U) == 0);
    CHECK(ams_mel_test_image_odm_probe(5U, 0U) == copies + 1);
    CHECK(counters_are(m, 8, 2, 5) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT && !event);
    /* Recovery: the same boundary map now copies completely. */
    CHECK(deliver(5U, 5U, 0U) == 1);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS);
    CHECK(check_tagged5(v, 5U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_image_odm_probe(4U, 0U) == 1);        /* restore INT32_MAX */
    CHECK(counters_are(m, 9, 2, 5) == EXIT_SUCCESS);
    /* Finite timeout on an active empty queue is TIMEOUT, not cancellation. */
    CHECK(ams_mel_ir_image_odm_receive(m, 20, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT && !event);
    /* A blocked Receive is woken by delivery (ordered by the probe). */
    CHECK(ams_mel_test_image_odm_probe(9U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, deliver_when_receive_waits, &tag) == 0);
    CHECK(ams_mel_ir_image_odm_receive(m, 30000, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(ams_mel_ir_image_odm_event_view(event, &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_tagged6(v, 31U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 10, 2, 5) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: ODM bounded queue, full-queue copy avoidance, atomic rejection and recovery");
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
    if (!ams_mel_test_image_odm_probe(8U, 0U)) abort();
    if (ams_mel_ir_stream_stop(stream, NULL, 0, NULL) != AMS_MEL_OK) abort();
    return NULL;
}

/* Image Stop wakes a blocked receiver; Stop preserves queued events, which
 * drain before STREAM_STOPPED; a late callback during channel destruction is
 * counted but never queued and returns before the provider library unloads. */
static int test_stop_and_drain(void)
{
    char log[] = "/tmp/ams-odm-drain-XXXXXX";
    const char *order[] = {"odm_sync_delivered", "late_odm_callback_invoked",
                           "late_odm_callback_returned", "library_unloaded"};
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    pthread_t thread;

    CHECK(open_owner("odm-capacity", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_image_odm_probe(9U, 0U) == 1);
    CHECK(pthread_create(&thread, NULL, stop_when_receive_waits, o.stream) == 0);
    CHECK(ams_mel_ir_image_odm_receive(m, 30000, &event, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(event == NULL && pthread_join(thread, NULL) == 0);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("odm-late", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 8, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_empty(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    /* Stopped: delivery now ignored for queueing, queued events drain. */
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_rich(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_frd_only(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_rowcol_only(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_image_odm_receive(m, 50, &event, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(counters_are(m, 5, 0, 0) == EXIT_SUCCESS); /* late callback counted, not queued */
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: ODM Stop wakes receivers and queued events drain after Stop/Close");
    return EXIT_SUCCESS;
}

/* Public owner Close before Image Close: consumption is deactivated (Close
 * cannot unregister); events already received stay valid; the provider's late
 * callback during channel destruction is memory-safe and returns first. */
static int test_owner_closed_first(void)
{
    char log[] = "/tmp/ams-odm-closed-XXXXXX";
    const char *order[] = {"odm_owner_closed", "late_odm_callback_invoked",
                           "late_odm_callback_returned", "library_unloaded"};
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner("odm-late-closed", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 8, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK && m == NULL);
    CHECK(append_log(log, "odm_owner_closed") == EXIT_SUCCESS);
    CHECK(deliver(6U, 9U, 0U) == 1); /* callback still registered: ignored safely */
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(ams_mel_ir_image_odm_event_view(event, &v, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_empty(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: ODM owner Close before Image Close; late callback memory-safe");
    return EXIT_SUCCESS;
}

/* A failed provider detach leaves ownership uncertain: the ODM owner fails,
 * queued events still drain, then Receive reports PROVIDER_FAILED. The stream
 * retains the provider graph until the retried detach succeeds. */
static int test_detach_failure(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    char d[128] = {0};
    CHECK(open_owner("odm-detach-fail", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_rich(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_frd_only(v) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_receive(m, 10, &event, d, sizeof d, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(strcmp(d, "Image channel cleanup failed; OpticalDistortionMap owner failed") == 0);
    CHECK(counters_are(m, 2, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_close(&o.stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(o.stream == NULL);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: ODM owner fails closed on provider detach failure and still drains");
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
    char log[] = "/tmp/ams-odm-in-flight-XXXXXX";
    const char *order[] = {"odm_in_flight_callback_entered", "channel_destroyed",
                           "odm_teardown_waiting_for_callback", "odm_callback_released",
                           "odm_in_flight_callback_returned", "library_unloaded"};
    struct owners o;
    struct close_job job;
    ams_mel_ir_image_odm_metadata *m = NULL;
    pthread_t thread;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(ams_mel_test_image_odm_probe(0U, 0U) == 1);          /* park next callback */
    CHECK(open_owner("odm-in-flight", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_image_odm_probe(1U, 0U) == 1);          /* inside the bridge */
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK); /* never waits */
    job.owners = &o; atomic_init(&job.returned, 0); job.status = AMS_MEL_INTERNAL_ERROR;
    CHECK(pthread_create(&thread, NULL, close_stream, &job) == 0);
    CHECK(ams_mel_test_image_odm_probe(3U, 0U) == 1);          /* teardown drains */
    CHECK(append_log(log, "odm_teardown_waiting_for_callback") == EXIT_SUCCESS);
    CHECK(atomic_load(&job.returned) == 0);
    CHECK(append_log(log, "odm_callback_released") == EXIT_SUCCESS);
    CHECK(ams_mel_test_image_odm_probe(2U, 0U) == 1);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(job.status == AMS_MEL_OK && o.stream == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);                    /* unload joins provider thread */
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 6U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    puts("PASS: ODM teardown drains an in-flight callback before releasing its resources");
    return EXIT_SUCCESS;
}

/* Whatever the outcome, never leave the parked provider thread blocked: a
 * failed check must end the process with a diagnostic, not hang it at the
 * provider's unload-time join. */
static int callback_in_flight(void)
{
    const int result = callback_in_flight_body();
    (void)ams_mel_test_image_odm_probe(2U, 0U);
    return result;
}

static int test_invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_image_odm_metadata *m = NULL;
    ams_mel_ir_image_odm_metadata *stale = (ams_mel_ir_image_odm_metadata *)&o;
    ams_mel_ir_image_odm_event *event = NULL;
    const ams_mel_ir_optical_distortion_map_v1 *v = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    char d[4];
    CHECK(ams_mel_ir_image_odm_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_event_view(NULL, &v, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_event_close(&event, d, sizeof d, NULL) == AMS_MEL_OK);
    CHECK(open_owner("odm-sync", &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 1, NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 1, &stale, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_open(o.stream, 1, &m, NULL, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* Argument rejection does not consume the one shot; capacity 1 keeps one. */
    CHECK(ams_mel_ir_image_odm_open(o.stream, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 4, 3, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_odm_receive(m, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_odm_get_counters(m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(take(m, &event, &v) == EXIT_SUCCESS && check_empty(v) == EXIT_SUCCESS);
    {
        ams_mel_ir_image_odm_event *occupied = event;
        CHECK(ams_mel_ir_image_odm_receive(m, 0, &occupied, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_image_odm_event_view(event, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(ams_mel_ir_image_odm_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_odm_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    puts("PASS: ODM argument validation");
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
    CHECK(test_capacity() == EXIT_SUCCESS);
    CHECK(test_stop_and_drain() == EXIT_SUCCESS);
    CHECK(test_owner_closed_first() == EXIT_SUCCESS);
    CHECK(test_detach_failure() == EXIT_SUCCESS);
    puts("PASS: C IR Image OpticalDistortionMap contract");
    return EXIT_SUCCESS;
}
