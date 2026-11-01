#define _POSIX_C_SOURCE 200809L
/* IRMEL-ADA-003: compiled-C contract for the two independent CameraCommand
 * sends (C2Channel and ImageChannel) and the opt-in Image CameraCommandResp
 * metadata owner, against the deterministic mock provider. Concurrency cases
 * use the provider's promise gate and send-entry barrier, never sleeps. */
#include <ams_mel/abi.h>

#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
extern int ams_mel_test_c2_requests(const ams_mel_ir_c2 *, size_t *);
extern int ams_mel_test_navigation_requests(const ams_mel_ir_stream *, size_t *);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return EXIT_FAILURE; } } while (0)
#define CAMERA_PROBE_FAMILY 6U
#define C2_SEND 6U
#define IMAGE_SEND 7U

/* Pre-existing frozen v1 layouts, including the IRMEL-ADA-002 NUC records. */
_Static_assert(sizeof(ams_mel_ir_image_metadata_event_v1) == 192, "image v1 size");
_Static_assert(offsetof(ams_mel_ir_image_metadata_event_v1, navigation_response) == 176, "image v1");
_Static_assert(sizeof(ams_mel_ir_navigation_result_v1) == 24, "navigation result");
_Static_assert(sizeof(ams_mel_ir_mode_result_v1) == 8, "mode result");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "counters");
_Static_assert(sizeof(ams_mel_ir_pixel_value_v1) == 8, "nuc pixel value");
_Static_assert(sizeof(ams_mel_ir_temp_point_v1) == 56, "nuc temp point");
_Static_assert(offsetof(ams_mel_ir_temp_point_v1, point_type) == 48, "nuc temp point");
_Static_assert(offsetof(ams_mel_ir_nuc_temp_data_event_v1, temp_points) == 0, "nuc event");
/* New IRMEL-ADA-003 records. */
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, command_id) == 0, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, system_time_ns) == 8, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, image_type) == 16, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, width) == 20, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, height) == 24, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, frame_rate_hz) == 28, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, integration_time_ns) == 32, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, row_offset) == 40, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, column_offset) == 44, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, direction) == 48, "camera command");
_Static_assert(offsetof(ams_mel_ir_camera_command_v1, number_of_stages) == 52, "camera command");
_Static_assert(sizeof(ams_mel_ir_camera_command_v1) == 56, "camera command size");
_Static_assert(offsetof(ams_mel_ir_camera_command_response_v1, size) == 4, "camera response");
_Static_assert(offsetof(ams_mel_ir_camera_command_response_v1, timestamp_ns) == 8, "camera response");
_Static_assert(offsetof(ams_mel_ir_camera_command_response_v1, command_success) == 16, "camera response");
_Static_assert(sizeof(ams_mel_ir_camera_command_response_v1) == 24, "camera response size");
_Static_assert(offsetof(ams_mel_ir_camera_command_result_v1, error_code) == 24, "camera result");
_Static_assert(sizeof(ams_mel_ir_camera_command_result_v1) == 32, "camera result size");
_Static_assert(_Generic(((ams_mel_ir_camera_command_v1 *)0)->system_time_ns, int64_t: 1, default: 0), "signed ns");
_Static_assert(_Generic(((ams_mel_ir_camera_command_v1 *)0)->integration_time_ns, int64_t: 1, default: 0), "signed ns");
_Static_assert(_Generic(((ams_mel_ir_camera_command_response_v1 *)0)->timestamp_ns, int64_t: 1, default: 0), "signed ns");
_Static_assert(AMS_MEL_IR_IMAGE_STARING == 0 && AMS_MEL_IR_IMAGE_SCANNING == 1 &&
               AMS_MEL_IR_IMAGE_RESERVED13 == 2, "ImageType");
_Static_assert(AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ == 0 && AMS_MEL_IR_SCAN_DIR_POS_TO_NEG_AZ == 1, "ScanDir");

typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
typedef int (*release_fn)(unsigned, unsigned);
typedef int (*last_fn)(unsigned, uint64_t *, unsigned *);
static barrier_fn send_barrier;
static gate_fn gate;
static release_fn release_family;
static last_fn last_command;

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

/* Resolves test-only provider controls. The returned pointers are used only
 * while a Session (or a pending request graph) keeps the provider loaded. */
static int bind_provider(void)
{
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    *(void **)(&send_barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&gate) = dlsym(library, "mock_completion_gate");
    *(void **)(&release_family) = dlsym(library, "mock_completion_release_family");
    *(void **)(&last_command) = dlsym(library, "mock_camera_last_command");
    CHECK(send_barrier && gate && release_family && last_command);
    CHECK(dlclose(library) == 0);
    return EXIT_SUCCESS;
}

struct owners {
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
    ams_mel_ir_stream *stream;
};

/* image = 1: Image stream (Attached; start optional). image = 0: Enabled C2. */
static int open_owner(int image, const char *scenario, uint32_t limit, struct owners *o)
{
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
    if (image) {
        ams_mel_ir_stream_config_v1 value;
        memset(&value, 0, sizeof value);
        value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        value.channel_id.descriptive_label = text("image");
        value.platform_id.descriptive_label = text("platform");
        value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
        value.buffer_count = 2; value.buffer_size = 64; value.queue_capacity = 2;
        CHECK(ams_mel_ir_stream_open(o->session, &value, &o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    } else {
        ams_mel_ir_c2_config_v1 value;
        memset(&value, 0, sizeof value);
        value.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        value.channel_id.descriptive_label = text("c2");
        value.platform_id.descriptive_label = text("platform");
        value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
        CHECK(ams_mel_ir_c2_open(o->session, &value, &o->c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_enable(o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    }
    return EXIT_SUCCESS;
}

static int close_owner(struct owners *o)
{
    if (o->stream) CHECK(ams_mel_ir_stream_close(&o->stream, NULL, 0, NULL) == AMS_MEL_OK);
    if (o->c2) CHECK(ams_mel_ir_c2_close(&o->c2, NULL, 0, NULL) == AMS_MEL_OK);
    if (o->session) CHECK(ams_mel_session_close(&o->session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static ams_mel_status_t submit(int image, struct owners *o, const ams_mel_ir_camera_command_v1 *c,
                               ams_mel_ir_camera_command_request **r, char *d, size_t n)
{
    return image ? ams_mel_ir_stream_submit_camera_command(o->stream, c, r, d, n, NULL)
                 : ams_mel_ir_c2_submit_camera_command(o->c2, c, r, d, n, NULL);
}

static ams_mel_ir_camera_command_v1 command(uint32_t id)
{
    ams_mel_ir_camera_command_v1 c;
    memset(&c, 0, sizeof c);
    c.command_id = id; c.system_time_ns = 123456789; c.image_type = AMS_MEL_IR_IMAGE_STARING;
    c.width = 640; c.height = 512; c.frame_rate_hz = 60; c.integration_time_ns = 5000;
    c.row_offset = 4; c.column_offset = 8; c.direction = AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ;
    c.number_of_stages = 1;
    return c;
}

/* The provider received exactly these 11 fields on the expected overload. */
static int delivered(unsigned family, const ams_mel_ir_camera_command_v1 *c)
{
    uint64_t v[11] = {0};
    CHECK(last_command(family, v, NULL) == 1);
    CHECK(v[0] == c->command_id);
    CHECK((int64_t)v[1] == c->system_time_ns);
    CHECK(v[2] == c->image_type && v[3] == c->width && v[4] == c->height);
    CHECK(v[5] == c->frame_rate_hz && (int64_t)v[6] == c->integration_time_ns);
    CHECK(v[7] == c->row_offset && v[8] == c->column_offset);
    CHECK(v[9] == c->direction && v[10] == c->number_of_stages);
    return EXIT_SUCCESS;
}

static unsigned sends(unsigned family)
{
    unsigned count = 0;
    (void)last_command(family, NULL, &count);
    return count;
}

/* Full round trip: both meaningful image modes, both scan directions, unsigned
 * 32-bit boundaries and signed nanosecond boundaries; distinct command IDs. */
static int test_fidelity(int image)
{
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    const unsigned other = image ? C2_SEND : IMAGE_SEND;
    unsigned other_before;
    ams_mel_ir_camera_command_v1 cases[3];
    ams_mel_ir_camera_command_request *requests[3] = {NULL, NULL, NULL};
    CHECK(open_owner(image, "camera-echo", 0, &o) == EXIT_SUCCESS);
    other_before = sends(other);
    cases[0] = command(UINT32_C(0xffffffff));
    cases[0].system_time_ns = INT64_MIN; cases[0].integration_time_ns = INT64_MAX;
    cases[0].image_type = AMS_MEL_IR_IMAGE_SCANNING; cases[0].direction = AMS_MEL_IR_SCAN_DIR_POS_TO_NEG_AZ;
    cases[0].width = UINT32_MAX; cases[0].height = UINT32_MAX; cases[0].frame_rate_hz = UINT32_MAX;
    cases[0].row_offset = UINT32_MAX; cases[0].column_offset = UINT32_MAX;
    cases[0].number_of_stages = UINT32_MAX;
    cases[1] = command(0);
    cases[1].system_time_ns = INT64_MAX; cases[1].integration_time_ns = INT64_MIN;
    cases[1].width = 0; cases[1].height = 0; cases[1].frame_rate_hz = 0; cases[1].row_offset = 0;
    cases[1].column_offset = 0; cases[1].number_of_stages = 0;
    cases[2] = command(UINT32_C(0x80000001));
    cases[2].system_time_ns = -1; cases[2].integration_time_ns = 0;
    cases[2].image_type = AMS_MEL_IR_IMAGE_SCANNING; cases[2].direction = AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ;
    for (unsigned i = 0; i < 3; ++i) {
        ams_mel_ir_camera_command_result_v1 result;
        memset(&result, 0xa5, sizeof result);
        CHECK(submit(image, &o, &cases[i], &requests[i], NULL, 0) == AMS_MEL_OK);
        CHECK(delivered(family, &cases[i]) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_camera_command_request_wait(requests[i], 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.error_code == AMS_MEL_ERROR_NONE);
        CHECK(result.response.command_id == cases[i].command_id);
        CHECK(result.response.size == cases[i].width);
        CHECK(result.response.timestamp_ns == cases[i].system_time_ns);
        CHECK(result.response.command_success == 1U);
    }
    /* Requests are independent; distinct IDs survive repeated cached waits. */
    for (unsigned i = 0; i < 3; ++i) {
        ams_mel_ir_camera_command_result_v1 result;
        CHECK(ams_mel_ir_camera_command_request_wait(requests[i], 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.response.command_id == cases[i].command_id);
        CHECK(ams_mel_ir_camera_command_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(requests[i] == NULL);
        CHECK(ams_mel_ir_camera_command_request_close(&requests[i], NULL, 0, NULL) == AMS_MEL_OK);
    }
    /* Never routed through the other channel family's overload. */
    CHECK(sends(other) == other_before);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* CommandSuccess=false is a successful response, not a MEL rejection. */
static int test_unsatisfied(int image)
{
    struct owners o;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(42);
    CHECK(open_owner(image, "camera-unsatisfied", 0, &o) == EXIT_SUCCESS);
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_success == 0U && result.response.command_id == 42U);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int expect_outcome(int image, const char *scenario, ams_mel_status_t status,
                          ams_mel_error_code_t code, const char *diagnostic)
{
    struct owners o;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    char d[1024];
    size_t required = 0;
    const ams_mel_ir_camera_command_v1 c = command(7);
    CHECK(open_owner(image, scenario, 0, &o) == EXIT_SUCCESS);
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, d, sizeof d, &required) == status);
    if (status == AMS_MEL_COMMAND_REJECTED) CHECK(result.error_code == code);
    if (diagnostic) CHECK(strcmp(d, diagnostic) == 0);
    /* Repeated terminal Wait returns the identical cached outcome. */
    CHECK(ams_mel_ir_camera_command_request_wait(request, 0, &result, d, sizeof d, NULL) == status);
    if (diagnostic) CHECK(strcmp(d, diagnostic) == 0);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_outcomes(int image)
{
    /* A provider without programmable camera control: MEL Unsupported. */
    CHECK(expect_outcome(image, "no-camera-control", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_UNSUPPORTED, "") == EXIT_SUCCESS);
    CHECK(expect_outcome(image, "camera-reject", AMS_MEL_COMMAND_REJECTED,
                         AMS_MEL_ERROR_INVALID_PARAMETERS, "camera command rejected") == EXIT_SUCCESS);
    CHECK(expect_outcome(image, "camera-unknown-error", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned unknown MEL error code") == EXIT_SUCCESS);
    CHECK(expect_outcome(image, "camera-null", AMS_MEL_PROVIDER_FAILED, 0,
                         "provider returned null successful CameraCommandResp") == EXIT_SUCCESS);
    CHECK(expect_outcome(image, "camera-future-throw", AMS_MEL_PROVIDER_EXCEPTION, 0,
                         "mock camera future exception") == EXIT_SUCCESS);
    /* Long rejection: required size reports the complete description. */
    {
        struct owners o;
        ams_mel_ir_camera_command_request *request = NULL;
        ams_mel_ir_camera_command_result_v1 result;
        char small[16];
        size_t required = 0;
        const ams_mel_ir_camera_command_v1 c = command(8);
        CHECK(open_owner(image, "camera-reject-long", 0, &o) == EXIT_SUCCESS);
        CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
        CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, small, sizeof small,
              &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INSUFFICIENT_RESOURCES && required == 614U);
        CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_owner(&o) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int request_count(int image, const struct owners *o, size_t *count)
{
    return image ? ams_mel_test_navigation_requests(o->stream, count)
                 : ams_mel_test_c2_requests(o->c2, count);
}

/* Reserved13, unknown image types and directions are rejected before send;
 * argument rejection consumes no accounting slot. */
static int test_invalid(int image)
{
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_request *stale = (ams_mel_ir_camera_command_request *)(void *)&o;
    ams_mel_ir_camera_command_v1 c = command(1);
    size_t count = 99;
    char d[64];
    unsigned before;
    CHECK(open_owner(image, "camera-echo", 0, &o) == EXIT_SUCCESS);
    before = sends(family);
    c.image_type = AMS_MEL_IR_IMAGE_RESERVED13;
    CHECK(submit(image, &o, &c, &request, d, sizeof d) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(strcmp(d, "invalid CameraCommand input") == 0 && request == NULL);
    c.image_type = 3U; CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.image_type = UINT32_MAX; CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.image_type = AMS_MEL_IR_IMAGE_STARING; c.direction = 2U;
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    c.direction = AMS_MEL_IR_SCAN_DIR_NEG_TO_POS_AZ;
    CHECK(submit(image, &o, NULL, &request, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(image, &o, &c, NULL, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(image, &o, &c, &stale, NULL, 0) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(submit(image, &o, &c, &request, NULL, 4) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_submit_camera_command(NULL, &c, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_stream_submit_camera_command(NULL, &c, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(sends(family) == before);
    CHECK(request_count(image, &o, &count) && count == 0U);
    CHECK(ams_mel_ir_camera_command_request_wait(NULL, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_command_request_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Provider send exception and an invalid (state-less) provider future are
 * distinct failures and release their accounting slot. */
static int test_send_failures(int image)
{
    struct owners o;
    ams_mel_ir_camera_command_request *request = NULL;
    const ams_mel_ir_camera_command_v1 c = command(2);
    size_t count = 99;
    char d[128];
    CHECK(open_owner(image, "camera-send-throw", 0, &o) == EXIT_SUCCESS);
    CHECK(submit(image, &o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(strcmp(d, "mock camera send exception") == 0 && request == NULL);
    CHECK(request_count(image, &o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(open_owner(image, "camera-invalid-future", 0, &o) == EXIT_SUCCESS);
    CHECK(submit(image, &o, &c, &request, d, sizeof d) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "provider returned an invalid CameraCommand future") == 0 && request == NULL);
    CHECK(request_count(image, &o, &count) && count == 0U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* C2 requires Enabled; Image admits Attached and Running, refuses Stopped. */
static int test_lifecycle_eligibility(void)
{
    struct owners o;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(3);
    ams_mel_ir_c2_config_v1 value;
    char d[64];
    unsigned before;
    memset(&o, 0, sizeof o);
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    value.channel_id.descriptive_label = text("c2");
    value.platform_id.descriptive_label = text("platform");
    value.sensor_location.key = text("key"); value.sensor_location.system_name = text("system");
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "camera-echo", "", &o.session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(bind_provider() == EXIT_SUCCESS);
    before = sends(C2_SEND);
    CHECK(ams_mel_ir_c2_open(o.session, &value, &o.c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_camera_command(o.c2, &c, &request, d, sizeof d, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "C2 channel is not enabled") == 0 && request == NULL);
    CHECK(sends(C2_SEND) == before);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner(1, "camera-echo", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_start(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_camera_command(o.stream, &c, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_camera_command(o.stream, &c, &request, d, sizeof d, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, "Image stream is not Attached or Running") == 0 && request == NULL);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

extern int ams_mel_test_completion_boundary(unsigned, unsigned, uint64_t, uint64_t *);

static uint64_t owners_destroyed(void)
{
    uint64_t values[4] = {0};
    if (!ams_mel_test_completion_boundary(CAMERA_PROBE_FAMILY, 3, 0, values)) return UINT64_MAX;
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

/* Timeout does not consume; repeated terminal Wait is cached. */
static int test_timeout_then_success(int image)
{
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(0x1234U);
    size_t count = 0;
    uint64_t base;
    CHECK(open_owner(image, "camera-held", 0, &o) == EXIT_SUCCESS);
    base = owners_destroyed();
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(request_count(image, &o, &count) && count == 1U);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 1, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(release_family(family, 1) == 1);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0x1234U);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0x1234U);
    {
        uint64_t observed = 0;
        CHECK(ams_mel_test_completion_owner(CAMERA_PROBE_FAMILY, base + 1U, &observed));
    }
    CHECK(request_count(image, &o, &count) && count == 0U);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Request Close, channel Close (C2) or Stop+Close (Image) and Session Close
 * all precede completion: the provider channel and library stay alive until
 * the future completes; nothing is cancelled; teardown order is preserved. */
static int test_pending_close(int image, int stop_first)
{
    char log[] = "/tmp/ams-camera-pending-XXXXXX";
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    const char *destroyed = image ? "channel_destroyed" : "c2_channel_destroyed";
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_request *observer = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(0xabcU);
    uint64_t base, observed = 0;
    void *pin;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(image, "camera-held", 0, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    base = owners_destroyed();
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(submit(image, &o, &c, &observer, NULL, 0) == AMS_MEL_OK);
    /* Public request Close before completion is not cancellation. */
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    if (image && stop_first) CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_command_request_wait(observer, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, destroyed) == 0U);
    CHECK(occurrences(log, "control_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(release_family(family, 2) == 1);
    CHECK(ams_mel_ir_camera_command_request_wait(observer, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.command_id == 0xabcU);
    CHECK(ams_mel_test_completion_owner(CAMERA_PROBE_FAMILY, base + 2U, &observed));
    CHECK(occurrences(log, destroyed) == 1U);
    CHECK(ams_mel_ir_camera_command_request_close(&observer, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(dlclose(pin) == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    {
        const char *order[] = {image ? "image_camera_command_sent" : "c2_camera_command_sent",
                               destroyed, "control_destroyed", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(occurrences(log, "library_unloaded") == 1U);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* Session admission is shared with every other request family. */
static int test_admission(int image)
{
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    ams_mel_ir_camera_command_request *first = NULL, *refused = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(5);
    uint64_t base, observed = 0;
    size_t count = 0;
    char d[64];
    unsigned before;
    CHECK(open_owner(image, "camera-held", 1, &o) == EXIT_SUCCESS);
    base = owners_destroyed();
    CHECK(submit(image, &o, &c, &first, NULL, 0) == AMS_MEL_OK);
    before = sends(family);
    CHECK(submit(image, &o, &c, &refused, d, sizeof d) == AMS_MEL_RESOURCE_EXHAUSTED);
    CHECK(strcmp(d, "async request limit reached") == 0 && refused == NULL);
    CHECK(sends(family) == before);
    CHECK(request_count(image, &o, &count) && count == 1U);
    CHECK(release_family(family, 1) == 1);
    CHECK(ams_mel_ir_camera_command_request_wait(first, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(CAMERA_PROBE_FAMILY, base + 1U, &observed));
    CHECK(ams_mel_ir_camera_command_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    /* The permit is released only after the completion owner is destroyed. */
    CHECK(submit(image, &o, &c, &first, NULL, 0) == AMS_MEL_OK);
    CHECK(release_family(family, 1) == 1);
    CHECK(ams_mel_ir_camera_command_request_wait(first, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_test_completion_owner(CAMERA_PROBE_FAMILY, base + 2U, &observed));
    CHECK(ams_mel_ir_camera_command_request_close(&first, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Isolated process: allocation or worker-launch failure after provider send
 * reports INTERNAL_ERROR, publishes no request and retains the accounted
 * future and provider graph rather than releasing or unloading it. */
static int isolated_post_send(int image, const char *failure)
{
    char log[] = "/tmp/ams-camera-post-send-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_command_request *request = NULL;
    const ams_mel_ir_camera_command_v1 c = command(6);
    size_t count = 0;
    char d[96];
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(image, "camera-held", 0, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_CAMERA_POST_SEND_FAILURE", failure, 1) == 0);
    CHECK(submit(image, &o, &c, &request, d, sizeof d) == AMS_MEL_INTERNAL_ERROR);
    CHECK(request == NULL);
    CHECK(strcmp(d, strcmp(failure, "allocation") == 0 ?
                 "facade allocation failed after provider send" : "facade worker launch failed") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_CAMERA_POST_SEND_FAILURE") == 0);
    CHECK(request_count(image, &o, &count) && count == 1U);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(occurrences(log, image ? "channel_destroyed" : "c2_channel_destroyed") == 0U);
    CHECK(occurrences(log, "library_unloaded") == 0U);
    CHECK(unlink(log) == 0);
    printf("PASS: isolated %s CameraCommand post-send %s retention\n", image ? "Image" : "C2", failure);
    return EXIT_SUCCESS; /* retained future intentionally never consumed */
}

/* Isolated process: deferred cleanup failure is never reported as success. */
static int isolated_finish_failure(int image)
{
    struct owners o;
    const unsigned family = image ? IMAGE_SEND : C2_SEND;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(7);
    char d[96];
    void *pin;
    CHECK(setenv(image ? "AMS_MEL_TEST_IMAGE_FINISH_FAILURE" : "AMS_MEL_TEST_C2_FINISH_FAILURE",
                 "exception", 1) == 0);
    CHECK(open_owner(image, "camera-held", 0, &o) == EXIT_SUCCESS);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(pin != NULL);
    CHECK(submit(image, &o, &c, &request, NULL, 0) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(release_family(family, 1) == 1);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 15000, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(d, image ? "deferred Image cleanup failed" : "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 0, &result, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    printf("PASS: isolated %s CameraCommand deferred-cleanup failure\n", image ? "Image" : "C2");
    return EXIT_SUCCESS; /* graph intentionally retained; pin never released */
}

static int counters_are(ams_mel_ir_camera_response_metadata *m, uint64_t received,
                        uint64_t dropped, uint64_t malformed)
{
    ams_mel_ir_metadata_counters_v1 counters;
    memset(&counters, 0, sizeof counters);
    CHECK(ams_mel_ir_camera_response_get_counters(m, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == received);
    CHECK(counters.events_dropped_queue_full == dropped);
    CHECK(counters.malformed_or_unsupported == malformed);
    return EXIT_SUCCESS;
}

static int receive_response(ams_mel_ir_camera_response_metadata *m, uint32_t id, uint32_t size,
                            int64_t ns, uint32_t success)
{
    ams_mel_ir_camera_response_event *event = NULL;
    const ams_mel_ir_camera_command_response_v1 *view = NULL;
    CHECK(ams_mel_ir_camera_response_receive(m, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_response_event_view(event, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->command_id == id && view->size == size && view->timestamp_ns == ns);
    CHECK(view->command_success == success);
    CHECK(ams_mel_ir_camera_response_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    return EXIT_SUCCESS;
}

/* Complete fields, synchronous registration delivery, ordering and an event
 * that outlives the owner, stream, Session and provider library. */
static int test_callback_rich(void)
{
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_camera_response_event *kept = NULL;
    const ams_mel_ir_camera_command_response_v1 *view = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(open_owner(1, "camera-callback-rich", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 4, &m, &reg, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(reg == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(counters_are(m, 3, 0, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 1U, 0U, INT64_MIN, 1U) == EXIT_SUCCESS);
    CHECK(receive_response(m, UINT32_MAX, UINT32_MAX, INT64_MAX, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_receive(m, 0, &kept, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_camera_response_receive(m, 0, &kept, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    {
        ams_mel_ir_camera_response_event *none = NULL;
        CHECK(ams_mel_ir_camera_response_receive(m, 1, &none, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    }
    /* Repeated registration attempt is refused (one shot). */
    {
        ams_mel_ir_camera_response_metadata *again = NULL;
        CHECK(ams_mel_ir_image_camera_response_open(o.stream, 4, &again, NULL, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
        CHECK(again == NULL);
    }
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(m == NULL);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_event_view(kept, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->command_id == 0U && view->size == 7U && view->timestamp_ns == -1 &&
          view->command_success == 1U);
    CHECK(ams_mel_ir_camera_response_event_close(&kept, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Opt-in only: a provider without camera control answers NotSupported, which
 * is reported distinctly, while existing Image metadata and requests keep
 * working on the same stream. */
static int test_callback_not_supported(void)
{
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_image_metadata *existing = NULL;
    ams_mel_ir_return_t reg = 99U;
    char d[128];
    CHECK(open_owner(1, "image-metadata-sync", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_metadata_open(o.stream, 4, &existing, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &m, &reg, d, sizeof d, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(reg == AMS_MEL_IR_RETURN_NOT_SUPPORTED && m == NULL);
    CHECK(strcmp(d, "provider does not support programmable camera control CameraCommandResp") == 0);
    {
        ams_mel_ir_image_metadata_event *event = NULL;
        CHECK(ams_mel_ir_image_metadata_receive(existing, 1000, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_stream_start(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&existing, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_callback_registration_failure(const char *scenario, ams_mel_status_t expected,
                                              int has_return)
{
    char log[] = "/tmp/ams-camera-register-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_return_t reg = 99U;
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(1, scenario, 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &m, &reg, NULL, 0, NULL) == expected);
    CHECK(m == NULL && reg == (has_return ? AMS_MEL_IR_RETURN_FAIL : 99U));
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &m, &reg, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    if (has_return) {
        const char *order[] = {"camera_registration_attempted", "late_camera_callback_invoked",
                               "late_camera_callback_returned", "library_unloaded"};
        CHECK(log_order(log, order, 4U) == EXIT_SUCCESS);
    }
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* Null payload counted malformed; bounded drop-incoming overflow ordering;
 * allocation failure rejects one event without crossing the boundary. */
static int test_callback_bounds(void)
{
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    CHECK(open_owner(1, "camera-callback-malformed", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 2, 0, 1) == EXIT_SUCCESS);
    CHECK(receive_response(m, 9U, 9U, 9, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner(1, "camera-callback-overflow", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 5, 3, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0U, 0U, 0, 1U) == EXIT_SUCCESS);
    CHECK(receive_response(m, 1U, 1U, 1, 0U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);

    CHECK(open_owner(1, "camera-callback-allocation", 0, &o) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE", "allocation", 1) == 0);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 2, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(getenv("AMS_MEL_TEST_CAMERA_CALLBACK_FAILURE") == NULL);
    CHECK(counters_are(m, 2, 0, 1) == EXIT_SUCCESS);
    CHECK(receive_response(m, 2U, 2U, 2, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Stopped state: queued events drain, then STREAM_STOPPED. A late callback
 * after owner and stream closure is memory-safe and returns before unload. */
static int test_callback_stop_and_late(void)
{
    char log[] = "/tmp/ams-camera-late-XXXXXX";
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_camera_response_event *event = NULL;
    const char *order[] = {"late_camera_callback_invoked", "late_camera_callback_returned",
                           "library_unloaded"};
    CHECK(make_log(log) == EXIT_SUCCESS);
    CHECK(open_owner(1, "camera-late", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(o.stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_response(m, 1U, 0U, INT64_MIN, 1U) == EXIT_SUCCESS);
    CHECK(receive_response(m, UINT32_MAX, UINT32_MAX, INT64_MAX, 0U) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0U, 7U, -1, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_receive(m, 1000, &event, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(log_order(log, order, 3U) == EXIT_SUCCESS);
    CHECK(unlink(log) == 0);
    return EXIT_SUCCESS;
}

/* The callback and the request future are independent sources: a response
 * delivered synchronously inside send does not complete or alter the future. */
static int test_callback_independent_of_request(void)
{
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_camera_command_request *request = NULL;
    ams_mel_ir_camera_command_result_v1 result;
    const ams_mel_ir_camera_command_v1 c = command(0x5151U);
    CHECK(open_owner(1, "camera-sync", 0, &o) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 4, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 0, 0, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_submit_camera_command(o.stream, &c, &request, NULL, 0, NULL) == AMS_MEL_OK);
    /* Delivered during send, before submission returned, with no deadlock. */
    CHECK(counters_are(m, 1, 0, 0) == EXIT_SUCCESS);
    CHECK(receive_response(m, 0x5151U, 77U, -77, 1U) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_command_request_wait(request, 5000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.response.size == c.width && result.response.timestamp_ns == c.system_time_ns);
    CHECK(ams_mel_ir_camera_command_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* No C2 metadata callback exists: C2 sends deliver nothing to Image. */
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_callback_invalid_arguments(void)
{
    struct owners o;
    ams_mel_ir_camera_response_metadata *m = NULL;
    ams_mel_ir_camera_response_event *event = NULL;
    const ams_mel_ir_camera_command_response_v1 *view = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(ams_mel_ir_image_camera_response_open(NULL, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_receive(NULL, 0, &event, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_event_view(NULL, &view, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_event_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_camera_response_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(open_owner(1, "camera-callback-rich", 0, &o) == EXIT_SUCCESS);
    /* Argument rejection does not consume the one shot. */
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 0, &m, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 1, NULL, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_image_camera_response_open(o.stream, 1, &m, NULL, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters_are(m, 3, 2, 0) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_camera_response_close(&m, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_owner(&o) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "post-send") == 0)
        return isolated_post_send(strcmp(argv[2], "image") == 0, argv[3]);
    if (argc == 3 && strcmp(argv[1], "finish-failure") == 0)
        return isolated_finish_failure(strcmp(argv[2], "image") == 0);
    for (int image = 0; image < 2; ++image) {
        const char *family = image ? "Image" : "C2";
        if (test_fidelity(image) != EXIT_SUCCESS || test_unsatisfied(image) != EXIT_SUCCESS ||
            test_outcomes(image) != EXIT_SUCCESS || test_invalid(image) != EXIT_SUCCESS ||
            test_send_failures(image) != EXIT_SUCCESS ||
            test_timeout_then_success(image) != EXIT_SUCCESS ||
            test_pending_close(image, 0) != EXIT_SUCCESS ||
            (image && test_pending_close(image, 1) != EXIT_SUCCESS) ||
            test_admission(image) != EXIT_SUCCESS) {
            fprintf(stderr, "FAIL: %s CameraCommand contract\n", family);
            return EXIT_FAILURE;
        }
    }
    if (test_lifecycle_eligibility() != EXIT_SUCCESS || test_callback_rich() != EXIT_SUCCESS ||
        test_callback_not_supported() != EXIT_SUCCESS ||
        test_callback_registration_failure("camera-register-fail", AMS_MEL_PROVIDER_FAILED, 1) != EXIT_SUCCESS ||
        test_callback_registration_failure("camera-register-throw", AMS_MEL_PROVIDER_EXCEPTION, 0) != EXIT_SUCCESS ||
        test_callback_bounds() != EXIT_SUCCESS || test_callback_stop_and_late() != EXIT_SUCCESS ||
        test_callback_independent_of_request() != EXIT_SUCCESS ||
        test_callback_invalid_arguments() != EXIT_SUCCESS) {
        fprintf(stderr, "FAIL: Image CameraCommandResp contract\n");
        return EXIT_FAILURE;
    }
    puts("PASS: C IR C2/Image CameraCommand and Image CameraCommandResp contract");
    return EXIT_SUCCESS;
}
