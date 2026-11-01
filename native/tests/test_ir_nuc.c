#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-002: compiled-C contract for opt-in NUC_TempData reception on
 * both the Image and the Health channel families. */
#include <ams_mel/abi.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)

/* Frozen v1 layouts of the pre-existing metadata events are unchanged. */
_Static_assert(sizeof(ams_mel_ir_image_metadata_event_v1) == 192, "image v1 size");
_Static_assert(offsetof(ams_mel_ir_image_metadata_event_v1, navigation_response) == 176, "image v1");
_Static_assert(sizeof(ams_mel_ir_health_metadata_event_v1) == 512, "health v1 size");
_Static_assert(offsetof(ams_mel_ir_health_metadata_event_v1, mfa_status_detailed) == 496, "health v1");
_Static_assert(sizeof(ams_mel_ir_band_info_v1) == 24, "band info");
/* New records. */
_Static_assert(sizeof(ams_mel_ir_pixel_value_v1) == 8, "pixel value");
_Static_assert(offsetof(ams_mel_ir_pixel_value_v1, value) == 4, "pixel value");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, timestamp_ns) == 0, "temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, frame_band) == 8, "temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, pixel_value) == 32, "temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, temperature_k) == 40, "temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, point_type) == 48, "temp point");
_Static_assert(sizeof(ams_mel_ir_temp_point_v1) == 56, "temp point");
_Static_assert(offsetof(ams_mel_ir_nuc_temp_data_event_v1, temp_points) == 0, "nuc event");
_Static_assert(_Generic(((ams_mel_ir_temp_point_v1 *)0)->timestamp_ns, int64_t: 1, default: 0),
               "signed nanoseconds");
_Static_assert(_Generic(((ams_mel_ir_temp_point_v1 *)0)->temperature_k, double: 1, default: 0),
               "double Kelvin");

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

struct owners {
    ams_mel_session *session;
    ams_mel_ir_stream *stream;
    ams_mel_ir_health *health;
};

static int open_owner(int image, const char *scenario, struct owners *o)
{
    memset(o, 0, sizeof *o);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &o->session, NULL, 0, NULL) == AMS_MEL_OK);
    if (image) {
        ams_mel_ir_stream_config_v1 value;
        memset(&value, 0, sizeof value);
        value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        value.channel_id.descriptive_label = text("image");
        value.platform_id.descriptive_label = text("platform");
        value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
        value.buffer_count = 3; value.buffer_size = 64; value.queue_capacity = 2;
        CHECK(ams_mel_ir_stream_open(o->session, &value, &o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_health_config_v1 value;
        memset(&value, 0, sizeof value);
        value.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
        value.channel_id.descriptive_label = text("health");
        value.platform_id.descriptive_label = text("platform");
        value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
        CHECK(ams_mel_ir_health_open(o->session, &value, &o->health, NULL, 0, NULL) == AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}

static ams_mel_status_t nuc_open(int image, struct owners *o, size_t capacity,
                                 ams_mel_ir_nuc_metadata **metadata, ams_mel_ir_return_t *reg)
{
    return image ? ams_mel_ir_image_nuc_open(o->stream, capacity, metadata, reg, NULL, 0, NULL)
                 : ams_mel_ir_health_nuc_open(o->health, capacity, metadata, reg, NULL, 0, NULL);
}

static int close_owner(struct owners *o)
{
    if (o->stream) CHECK(ams_mel_ir_stream_close(&o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    if (o->health) CHECK(ams_mel_ir_health_close(&o->health, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&o->session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int receive(ams_mel_ir_nuc_metadata *metadata, ams_mel_ir_nuc_event **event,
                   const ams_mel_ir_nuc_temp_data_event_v1 **view)
{
    CHECK(ams_mel_ir_nuc_receive(metadata, 0, event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_nuc_event_view(*event, view, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int counters_are(ams_mel_ir_nuc_metadata *metadata, uint64_t received, uint64_t dropped,
                        uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 counters = {0};
    CHECK(ams_mel_ir_nuc_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == received);
    CHECK(counters.events_dropped_queue_full == dropped);
    CHECK(counters.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

static int check_point(const ams_mel_ir_temp_point_v1 *p, int64_t ns, uint32_t band, double min,
                       double max, uint32_t kind, uint32_t value, double kelvin, uint32_t type)
{
    CHECK(p->timestamp_ns == ns);
    CHECK(p->frame_band.type == band);
    CHECK(p->frame_band.min_wavelength_m == min && p->frame_band.max_wavelength_m == max);
    CHECK(p->pixel_value.kind == kind && p->pixel_value.value == value);
    CHECK(p->temperature_k == kelvin);
    CHECK(p->point_type == type);
    return EXIT_SUCCESS;
}

/* The three rich messages: empty, one point, three ordered points. */
static int check_rich(const ams_mel_ir_nuc_temp_data_event_v1 *const views[3])
{
    const ams_mel_ir_temp_point_v1 *p;
    CHECK(views[0]->temp_points.size == 0U && views[0]->temp_points.data == NULL);
    CHECK(views[1]->temp_points.size == 1U);
    CHECK(check_point(&views[1]->temp_points.data[0], INT64_MIN, AMS_MEL_IR_BAND_IR_MIDWAVE, 3.0e-6,
                      5.0e-6, AMS_MEL_IR_PIXEL_VALUE_UINT8, 0xffU, 300.25,
                      AMS_MEL_IR_TEMP_POINT_NUC_POINT) == EXIT_SUCCESS);
    CHECK(views[2]->temp_points.size == 3U);
    p = views[2]->temp_points.data;
    CHECK(check_point(&p[0], -1, AMS_MEL_IR_BAND_IR_LONGWAVE, 8.0e-6, 1.2e-5,
                      AMS_MEL_IR_PIXEL_VALUE_UINT16, 0xffffU, 0.0,
                      AMS_MEL_IR_TEMP_POINT_MFA_ESTIMATE) == EXIT_SUCCESS);
    CHECK(check_point(&p[1], INT64_MAX, AMS_MEL_IR_BAND_UV_VACUUM, 1.0e-8, 2.0e-7,
                      AMS_MEL_IR_PIXEL_VALUE_UINT32, 0xffffffffU, 1.0e6,
                      AMS_MEL_IR_TEMP_POINT_OTHER) == EXIT_SUCCESS);
    CHECK(check_point(&p[2], 42, AMS_MEL_IR_BAND_INVALID, -1.0, 0.0, AMS_MEL_IR_PIXEL_VALUE_UINT8,
                      0U, -5.5, AMS_MEL_IR_TEMP_POINT_NUC_POINT) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Synchronous delivery during registration, all fields and ordering, and
 * event ownership surviving NUC owner, channel and Session closure. */
static int test_rich(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_event *events[3] = {0};
    const ams_mel_ir_nuc_temp_data_event_v1 *views[3] = {0};
    ams_mel_ir_nuc_event *extra = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner(image, "nuc-rich", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 8, &metadata, &reg) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS && metadata != NULL);
    CHECK(counters_are(metadata, 3, 0, 0) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 3U; ++i) CHECK(receive(metadata, &events[i], &views[i]) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_nuc_receive(metadata, 0, &extra, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK && metadata == NULL);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 3U; ++i)
        CHECK(ams_mel_ir_nuc_event_view(events[i], &views[i], NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(check_rich(views) == EXIT_SUCCESS);
    for (unsigned i = 0; i < 3U; ++i) {
        CHECK(ams_mel_ir_nuc_event_close(&events[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(events[i] == NULL);
    }
    return EXIT_SUCCESS;
}

/* Existing four/six-kind metadata owners still open on a NUC-capable
 * provider, and NUC opens independently afterwards. */
static int test_coexists_with_existing_metadata(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    CHECK(open_owner(image, "nuc-rich", &o) == EXIT_SUCCESS);
    if (image) {
        ams_mel_ir_image_metadata *existing = NULL;
        CHECK(ams_mel_ir_image_metadata_open(o.stream, 2, &existing, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_image_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_health_metadata *existing = NULL;
        CHECK(ams_mel_ir_health_metadata_open(o.health, 2, &existing, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_health_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(nuc_open(image, &o, 4, &metadata, NULL) == AMS_MEL_OK);
    CHECK(counters_are(metadata, 3, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Providers without NUC: the opt-in fails with an observable NOT_SUPPORTED,
 * publishes nothing, and existing metadata owners remain fully usable. */
static int test_not_supported(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner(image, image ? "image-metadata-four-mixed" : "health-rich", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 4, &metadata, &reg) == AMS_MEL_PROVIDER_FAILED);
    CHECK(metadata == NULL && reg == AMS_MEL_IR_RETURN_NOT_SUPPORTED);
    CHECK(nuc_open(image, &o, 4, &metadata, &reg) == AMS_MEL_INVALID_ARGUMENT);
    if (image) {
        ams_mel_ir_image_metadata *existing = NULL;
        ams_mel_ir_image_metadata_event *event = NULL;
        const ams_mel_ir_image_metadata_event_v1 *view = NULL;
        CHECK(ams_mel_ir_image_metadata_open(o.stream, 4, &existing, NULL, 0, NULL) == AMS_MEL_OK);
        for (uint32_t kind = AMS_MEL_IR_IMAGE_METADATA_BAD_PIXEL_LIST;
             kind <= AMS_MEL_IR_IMAGE_METADATA_NAVIGATION_RESPONSE; ++kind) {
            CHECK(ams_mel_ir_image_metadata_receive(existing, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_image_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(view->kind == kind);
            CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        }
        CHECK(ams_mel_ir_image_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_health_metadata *existing = NULL;
        ams_mel_ir_health_metadata_event *event = NULL;
        const ams_mel_ir_health_metadata_event_v1 *view = NULL;
        CHECK(ams_mel_ir_health_metadata_open(o.health, 32, &existing, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_health_metadata_receive(existing, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_health_metadata_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(view->kind == AMS_MEL_IR_HEALTH_METADATA_MFA_STATUS);
        CHECK(ams_mel_ir_health_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_health_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Null payload, unknown BandType and unknown TempPointType are rejected and
 * counted; the subsequent valid event is retained. */
static int test_malformed(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_event *event = NULL;
    const ams_mel_ir_nuc_temp_data_event_v1 *view = NULL;
    CHECK(open_owner(image, "nuc-malformed", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 4, &metadata, NULL) == AMS_MEL_OK);
    CHECK(counters_are(metadata, 4, 0, 3) == EXIT_SUCCESS);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
    CHECK(view->temp_points.size == 1U && view->temp_points.data[0].timestamp_ns == 5);
    CHECK(view->temp_points.data[0].pixel_value.kind == AMS_MEL_IR_PIXEL_VALUE_UINT16);
    CHECK(view->temp_points.data[0].pixel_value.value == 1234U);
    CHECK(ams_mel_ir_nuc_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_nuc_receive(metadata, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Bounded DROP-INCOMING: the first two retained in order, three dropped. */
static int test_overflow(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_event *event = NULL;
    const ams_mel_ir_nuc_temp_data_event_v1 *view = NULL;
    CHECK(open_owner(image, "nuc-overflow", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 2, &metadata, NULL) == AMS_MEL_OK);
    CHECK(counters_are(metadata, 5, 3, 0) == EXIT_SUCCESS);
    for (int64_t expected = 0; expected < 2; ++expected) {
        CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
        CHECK(view->temp_points.data[0].timestamp_ns == expected);
        CHECK(ams_mel_ir_nuc_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_nuc_receive(metadata, 0, &event, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* An allocation failure while copying rejects one event, not the owner. */
static int test_allocation(int image)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_event *event = NULL;
    const ams_mel_ir_nuc_temp_data_event_v1 *view = NULL;
    CHECK(open_owner(image, "nuc-allocation", &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_NUC_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(nuc_open(image, &o, 2, &metadata, NULL) == AMS_MEL_OK);
    CHECK(getenv("AMS_MEL_TEST_NUC_CALLBACK_FAILURE") == NULL);
    CHECK(counters_are(metadata, 2, 0, 1) == EXIT_SUCCESS);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
    CHECK(view->temp_points.data[0].timestamp_ns == 2);
    CHECK(ams_mel_ir_nuc_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int log_order(const char *path, const char *const *names, unsigned count)
{
    char data[8192];
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

/* Fail after a synchronous re-entrant delivery, and a provider exception:
 * no owner, one-shot consumed, and the retained callback invoked late from
 * channel destruction stays memory-safe before library unload. */
static int test_registration_failure(int image, const char *scenario, ams_mel_status_t expected,
                                     int has_return)
{
    char log[] = "/tmp/ams-nuc-register-XXXXXX";
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(image, scenario, &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 2, &metadata, &reg) == expected);
    CHECK(metadata == NULL);
    CHECK(reg == (has_return ? AMS_MEL_IR_RETURN_FAIL : 99U));
    CHECK(nuc_open(image, &o, 2, &metadata, &reg) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    if (has_return) {
        const char *order[] = {"nuc_registration_attempted", "late_nuc_callback_invoked",
                               "late_nuc_callback_returned", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* Late callback after both the public NUC owner and the channel are closed:
 * queued events retained before close stay owned; the late event is counted
 * but not queued, and the callback returns before library unload. */
static int test_late_callback(int image)
{
    char log[] = "/tmp/ams-nuc-late-XXXXXX";
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_event *event = NULL;
    const ams_mel_ir_nuc_temp_data_event_v1 *view = NULL;
    const char *order[] = {"late_nuc_callback_invoked", "late_nuc_callback_returned",
                           "library_unloaded"};
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(image, "nuc-late", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 4, &metadata, NULL) == AMS_MEL_OK);
    CHECK(receive(metadata, &event, &view) == EXIT_SUCCESS);
    if (image) CHECK(ams_mel_ir_stream_close(&o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    else CHECK(ams_mel_ir_health_close(&o.health, NULL, 0, NULL) == AMS_MEL_OK);
    /* Retained queue drains after channel close; then STREAM_STOPPED. */
    {
        ams_mel_ir_nuc_event *second = NULL;
        CHECK(ams_mel_ir_nuc_receive(metadata, 0, &second, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_nuc_event_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_nuc_receive(metadata, 0, &second, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_nuc_event_close(&second, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_nuc_receive(metadata, 0, &second, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    }
    CHECK(counters_are(metadata, 4, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(ams_mel_ir_nuc_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->temp_points.size == 0U);
    CHECK(ams_mel_ir_nuc_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(log_order(log, order, 3U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
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

/* Public NUC Close while a provider callback is active: Close does not wait
 * for or claim quiescence; the channel destructor is the boundary, and the
 * in-flight callback returns safely before the provider library unloads. */
static int test_close_while_active(int image)
{
    char log[] = "/tmp/ams-nuc-active-XXXXXX";
    char barrier[] = "/tmp/ams-nuc-barrier-XXXXXX";
    char start[256], entered[256], release[256];
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    int descriptor;
    const char *order[] = {"nuc_callback_entered", "nuc_owner_closed", "nuc_callback_returned",
                           "library_unloaded"};
    CHECK(make_log(log) == EXIT_SUCCESS);
    descriptor = mkstemp(barrier); CHECK(descriptor >= 0); CHECK(close(descriptor) == 0);
    CHECK(unlink(barrier) == 0);
    CHECK(snprintf(start, sizeof start, "%s.start", barrier) > 0);
    CHECK(snprintf(entered, sizeof entered, "%s.entered", barrier) > 0);
    CHECK(snprintf(release, sizeof release, "%s.release", barrier) > 0);
    CHECK(setenv("AMS_MEL_TEST_NUC_CALLBACK_BARRIER", barrier, 1) == 0);
    CHECK(open_owner(image, "nuc-nonquiescing", &o) == EXIT_SUCCESS);
    CHECK(nuc_open(image, &o, 2, &metadata, NULL) == AMS_MEL_OK);
    CHECK(append_log(start, "start") == EXIT_SUCCESS);
    CHECK(wait_for_file(entered) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(append_log(log, "nuc_owner_closed") == EXIT_SUCCESS);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_NUC_CALLBACK_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    CHECK(unlink(start) == 0); CHECK(unlink(entered) == 0); CHECK(unlink(release) == 0);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

static int test_invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_nuc_metadata *metadata = NULL;
    ams_mel_ir_nuc_metadata *stale = (ams_mel_ir_nuc_metadata *)&o;
    ams_mel_ir_nuc_event *event = NULL;
    const ams_mel_ir_nuc_temp_data_event_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    char text_buffer[4];
    CHECK(ams_mel_ir_image_nuc_open(NULL, 1, &metadata, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_health_nuc_open(NULL, 1, &metadata, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_event_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_nuc_event_close(&event, text_buffer, sizeof text_buffer, NULL) == AMS_MEL_OK);
    for (int image = 0; image < 2; ++image) {
        CHECK(open_owner(image, "nuc-rich", &o) == EXIT_SUCCESS);
        CHECK(nuc_open(image, &o, 0, &metadata, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(nuc_open(image, &o, 1, NULL, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(nuc_open(image, &o, 1, &stale, NULL) == AMS_MEL_INVALID_ARGUMENT);
        /* Argument rejection does not consume the one shot. */
        CHECK(nuc_open(image, &o, 1, &metadata, NULL) == AMS_MEL_OK);
        CHECK(counters_are(metadata, 3, 2, 0) == EXIT_SUCCESS);
        CHECK(nuc_open(image, &o, 1, &metadata, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_nuc_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    for (int image = 1; image >= 0; --image) {
        const char *family = image ? "Image" : "Health";
        if (test_rich(image) != EXIT_SUCCESS ||
            test_coexists_with_existing_metadata(image) != EXIT_SUCCESS ||
            test_not_supported(image) != EXIT_SUCCESS ||
            test_malformed(image) != EXIT_SUCCESS ||
            test_overflow(image) != EXIT_SUCCESS ||
            test_allocation(image) != EXIT_SUCCESS ||
            test_registration_failure(image, "nuc-register-fail", AMS_MEL_PROVIDER_FAILED, 1) != EXIT_SUCCESS ||
            test_registration_failure(image, "nuc-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0) != EXIT_SUCCESS ||
            test_late_callback(image) != EXIT_SUCCESS ||
            test_close_while_active(image) != EXIT_SUCCESS) {
            fprintf(stderr, "FAIL: %s NUC_TempData contract\n", family);
            return EXIT_FAILURE;
        }
    }
    puts("PASS: C IR Image/Health NUC_TempData contract");
    return EXIT_SUCCESS;
}
