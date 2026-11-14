#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
extern int ams_mel_test_comms_metadata(unsigned, const ams_mel_ir_channel_comms_metadata *);
_Static_assert(sizeof(ams_mel_ir_channel_comms_test_report_v1) == 8, "two uint32 fields only");
_Static_assert(offsetof(ams_mel_ir_channel_comms_test_report_v1, request_id) == 4, "request offset");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "existing counters");
/* Strict C11 signature probes, executed as well as compiled. */
static ams_mel_status_t (*const open_signature)(const ams_mel_ir_channel *, size_t,
    ams_mel_ir_channel_comms_metadata **, char *, size_t, size_t *) = ams_mel_ir_channel_comms_metadata_open;
static ams_mel_status_t (*const receive_signature)(const ams_mel_ir_channel_comms_metadata *, uint32_t,
    ams_mel_ir_channel_comms_test_report_v1 *, char *, size_t, size_t *) = ams_mel_ir_channel_comms_metadata_receive;
static ams_mel_status_t (*const counters_signature)(const ams_mel_ir_channel_comms_metadata *,
    ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) = ams_mel_ir_channel_comms_metadata_get_counters;
static ams_mel_status_t (*const close_signature)(ams_mel_ir_channel_comms_metadata **,
    char *, size_t, size_t *) = ams_mel_ir_channel_comms_metadata_close;

typedef struct {
    unsigned family;
    ams_mel_session *session;
    ams_mel_ir_c2 *c2;
    ams_mel_ir_health *health;
    ams_mel_ir_stream *image;
    ams_mel_ir_stacked *stacked;
    ams_mel_ir_channel *view;
    ams_mel_ir_channel_comms_metadata *metadata;
} fixture;

static void start(fixture *f, unsigned family, const char *scenario)
{
    memset(f, 0, sizeof *f); f->family = family;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &f->session, ARGS) == AMS_MEL_OK);
    switch (family) {
    case 0: {
        ams_mel_ir_c2_config_v1 config = {0}; config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        CHECK(ams_mel_ir_c2_open(f->session, &config, &f->c2, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_from_c2(f->c2, &f->view, ARGS) == AMS_MEL_OK); break;
    }
    case 1: {
        ams_mel_ir_health_config_v1 config = {0}; config.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
        CHECK(ams_mel_ir_health_open(f->session, &config, &f->health, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_from_health(f->health, &f->view, ARGS) == AMS_MEL_OK); break;
    }
    case 2: {
        ams_mel_ir_image_control_config_v1 config = {0}; config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        CHECK(ams_mel_ir_image_control_open(f->session, &config, &f->image, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_from_stream(f->image, &f->view, ARGS) == AMS_MEL_OK); break;
    }
    case 3: {
        ams_mel_ir_stacked_config_v1 config = {0}; config.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
        CHECK(ams_mel_ir_stacked_open(f->session, &config, &f->stacked, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_from_stacked(f->stacked, &f->view, ARGS) == AMS_MEL_OK); break;
    }
    case 4: {
        ams_mel_ir_stream_config_v1 config = {0}; config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        config.buffer_count = 2; config.buffer_size = 64; config.queue_capacity = 2;
        CHECK(ams_mel_ir_stream_open(f->session, &config, &f->image, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_from_stream(f->image, &f->view, ARGS) == AMS_MEL_OK); break;
    }
    default: CHECK(0);
    }
}
static ams_mel_status_t close_typed(fixture *f)
{
    switch (f->family) {
    case 0: return ams_mel_ir_c2_close(&f->c2, ARGS);
    case 1: return ams_mel_ir_health_close(&f->health, ARGS);
    case 2: case 4: return ams_mel_ir_stream_close(&f->image, ARGS);
    case 3: return ams_mel_ir_stacked_close(&f->stacked, ARGS);
    default: return AMS_MEL_INTERNAL_ERROR;
    }
}
static void finish(fixture *f)
{
    CHECK(close_typed(f) == AMS_MEL_OK);
    CHECK(close_typed(f) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_metadata_close(&f->metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_metadata_close(&f->metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&f->view, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&f->session, ARGS) == AMS_MEL_OK);
}
static void submit(fixture *f, uint32_t command, uint32_t request_id, int event)
{
    ams_mel_ir_channel_comms_test_request_v1 input = {command, 0xf0000002U, request_id};
    ams_mel_ir_channel_comms_request *request = NULL;
    ams_mel_ir_channel_comms_test_result_v1 result = {0};
    ams_mel_ir_channel_comms_test_report_v1 report = {17, 19};
    CHECK(ams_mel_ir_channel_submit_comms_test(f->view, &input, &request, ARGS) == AMS_MEL_OK);
    /* Metadata can already be present before Wait. */
    if (event) {
        CHECK(receive_signature(f->metadata, 0, &report, ARGS) == AMS_MEL_OK);
        CHECK(report.command_id == command && report.request_id == request_id);
    }
    CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result, ARGS) == AMS_MEL_OK);
    CHECK(result.command_id == command && result.request_id == request_id);
    CHECK(ams_mel_ir_channel_comms_request_close(&request, ARGS) == AMS_MEL_OK);
}
static void *concurrent_submit(void *arg)
{ submit(arg, UINT32_MAX, 0x80000001U, 0); return NULL; }

typedef struct { fixture *f; ams_mel_status_t status; } operation;
static void *close_worker(void *arg)
{ operation *op = arg; op->status = close_typed(op->f); return NULL; }
static void *receive_worker(void *arg)
{
    operation *op = arg;
    ams_mel_ir_channel_comms_test_report_v1 report = {17, 19};
    op->status = receive_signature(op->f->metadata, UINT32_MAX, &report, ARGS);
    CHECK(report.command_id == 17 && report.request_id == 19);
    return NULL;
}
static void enable(fixture *f)
{
    switch (f->family) {
    case 0: CHECK(ams_mel_ir_c2_enable(f->c2, ARGS) == AMS_MEL_OK); break;
    case 1: CHECK(ams_mel_ir_health_enable(f->health, ARGS) == AMS_MEL_OK); break;
    case 2: case 4: CHECK(ams_mel_ir_stream_start(f->image, ARGS) == AMS_MEL_OK); break;
    case 3: CHECK(ams_mel_ir_stacked_enable(f->stacked, ARGS) == AMS_MEL_OK); break;
    }
}

static fixture *reentry_fixture;
static void close_reentry(void)
{ CHECK(close_typed(reentry_fixture) == AMS_MEL_OK); }
static void registration_reentry(unsigned family)
{
    fixture f; start(&f, family, "comms-meta-reentry");
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    void *symbol = dlsym(pin, "mock_comms_reentry");
    void (*configure)(void (*)(void));
    CHECK(pin && symbol && sizeof symbol == sizeof configure);
    memcpy(&configure, &symbol, sizeof configure);
    reentry_fixture = &f; configure(close_reentry);
    CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_OK);
    ams_mel_ir_channel_comms_test_report_v1 report = {17, 19};
    CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_STREAM_STOPPED);
    CHECK(close_signature(&f.metadata, ARGS) == AMS_MEL_OK);
    finish(&f); CHECK(!dlclose(pin));
}

static unsigned logged(const char *path, const char *event)
{
    FILE *file = fopen(path, "r"); char line[256]; unsigned count = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++count;
    CHECK(!fclose(file)); return count;
}

static void inflight(unsigned family)
{
    fixture f; pthread_t thread; operation op = {&f, AMS_MEL_INTERNAL_ERROR};
    char log[] = "/tmp/ams-comms-inflight-XXXXXX";
    int fd = mkstemp(log); CHECK(fd >= 0 && !close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1));
    CHECK(ams_mel_test_comms_metadata(0, NULL));
    start(&f, family, "comms-meta-inflight");
    CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_test_comms_metadata(1, NULL));
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    void *symbol = dlsym(pin, "mock_c2_send_barrier");
    int (*barrier)(unsigned, unsigned, unsigned);
    CHECK(pin && symbol && sizeof symbol == sizeof barrier);
    memcpy(&barrier, &symbol, sizeof barrier);
    CHECK(barrier(0, 0, 0));
    CHECK(ams_mel_session_close(&f.session, ARGS) == AMS_MEL_OK);
    CHECK(!pthread_create(&thread, NULL, close_worker, &op));
    CHECK(barrier(0, 1, 0));
    CHECK(logged(log, "common_comms_destructor_joining\n") == 1);
    CHECK(logged(log, "library_unloaded\n") == 0);
    CHECK(ams_mel_test_comms_metadata(2, NULL));
    CHECK(barrier(0, 2, 0));
    CHECK(!pthread_join(thread, NULL) && op.status == AMS_MEL_OK);
    CHECK(logged(log, "common_comms_producer_returned\n") == 1);
    CHECK(logged(log, "common_comms_destructor_joined\n") == 1);
    CHECK(!dlclose(pin));
    CHECK(logged(log, "library_unloaded\n") == 1);
    finish(&f);
    CHECK(!unlink(log));
}

static void held(unsigned family)
{
    fixture f; start(&f, family, "comms-meta-held");
    ams_mel_ir_channel_comms_request *request = NULL;
    ams_mel_ir_channel_comms_test_request_v1 input = {UINT32_MAX, 0xf0000002U, 0x80000001U};
    ams_mel_ir_channel_comms_test_result_v1 result = {0};
    ams_mel_ir_channel_comms_test_report_v1 report;
    CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_submit_comms_test(f.view, &input, &request, ARGS) == AMS_MEL_OK);
    CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_OK);
    CHECK(report.command_id == input.command_id && report.request_id == input.request_id);
    CHECK(ams_mel_ir_channel_comms_request_wait(request, 0, &result, ARGS) == AMS_MEL_TIMEOUT);
    void *pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    void *symbol = dlsym(pin, "mock_completion_release_family");
    int (*release)(unsigned, unsigned);
    CHECK(pin && symbol && sizeof symbol == sizeof release);
    memcpy(&release, &symbol, sizeof release);
    CHECK(close_typed(&f) == AMS_MEL_OK);
    CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_STREAM_STOPPED);
    CHECK(close_signature(&f.metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&f.session, ARGS) == AMS_MEL_OK);
    CHECK(release(family == 3 ? 14 : 2, 1));
    CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result, ARGS) == AMS_MEL_OK);
    CHECK(result.command_id == input.command_id && result.request_id == input.request_id);
    CHECK(ams_mel_ir_channel_comms_request_close(&request, ARGS) == AMS_MEL_OK);
    finish(&f); CHECK(!dlclose(pin));
}

static void exercise(unsigned family, const char *scenario)
{
    fixture f; start(&f, family, scenario);
    ams_mel_ir_channel_comms_metadata *second = NULL;
    ams_mel_ir_channel_comms_test_report_v1 report = {17, 19};
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(open_signature(f.view, 0, &f.metadata, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(open_signature(f.view, 2, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(open_signature(f.view, 2, &f.metadata, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* SIZE_MAX cannot be represented as a vector of complete reports. */
    CHECK(open_signature(f.view, SIZE_MAX, &f.metadata, ARGS) == AMS_MEL_INTERNAL_ERROR);
    if (!strcmp(scenario, "comms-meta-enabled")) enable(&f);
    if (!strcmp(scenario, "comms-meta-expired")) {
        CHECK(close_typed(&f) == AMS_MEL_OK);
        CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_PROVIDER_FAILED);
        finish(&f); return;
    }
    const ams_mel_status_t expected = !strcmp(scenario, "comms-meta-fail") ? AMS_MEL_PROVIDER_FAILED :
        !strcmp(scenario, "comms-meta-throw") ? AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_OK;
    char diagnostic[128] = {0};
    CHECK(open_signature(f.view, 2, &f.metadata, diagnostic, sizeof diagnostic, NULL) == expected);
    CHECK(open_signature(f.view, 2, &second, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(!second);
    if (expected != AMS_MEL_OK) {
        CHECK(!f.metadata);
        if (expected == AMS_MEL_PROVIDER_EXCEPTION) CHECK(strstr(diagnostic, "registration exception"));
        finish(&f); return;
    }
    CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    /* Another weak view must observe the same family-owned one-shot. */
    ams_mel_ir_channel *another = NULL;
    if (f.c2) CHECK(ams_mel_ir_channel_from_c2(f.c2, &another, ARGS) == AMS_MEL_OK);
    if (f.health) CHECK(ams_mel_ir_channel_from_health(f.health, &another, ARGS) == AMS_MEL_OK);
    if (f.image) CHECK(ams_mel_ir_channel_from_stream(f.image, &another, ARGS) == AMS_MEL_OK);
    if (f.stacked) CHECK(ams_mel_ir_channel_from_stacked(f.stacked, &another, ARGS) == AMS_MEL_OK);
    CHECK(open_signature(another, 2, &second, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_close(&another, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_metadata_get_counters(f.metadata, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(receive_signature(f.metadata, 0, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    if (!strcmp(scenario, "comms-meta-sync") || !strcmp(scenario, "comms-meta-overflow")) {
        CHECK(ams_mel_ir_channel_comms_metadata_get_counters(f.metadata, &counters, ARGS) == AMS_MEL_OK);
        CHECK(counters.events_received == 4 && counters.events_dropped_queue_full == 1 && counters.malformed_or_unsupported == 1);
        for (uint32_t i = 0; i < 2; ++i) {
            CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_OK);
            CHECK(report.command_id == UINT32_MAX - i && report.request_id == 0x80000000U + i);
        }
    }
    report.command_id = 17; report.request_id = 19;
    CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_TIMEOUT);
    CHECK(receive_signature(f.metadata, 1, &report, ARGS) == AMS_MEL_TIMEOUT);
    CHECK(report.command_id == 17 && report.request_id == 19);
    if (!strcmp(scenario, "comms-meta-wait-close")) {
        pthread_t thread; operation op = {&f, AMS_MEL_INTERNAL_ERROR};
        CHECK(ams_mel_test_comms_metadata(5, NULL));
        CHECK(!pthread_create(&thread, NULL, receive_worker, &op));
        CHECK(ams_mel_test_comms_metadata(4, NULL));
        if (family == 2 || family == 4) CHECK(ams_mel_ir_stream_stop(f.image, ARGS) == AMS_MEL_OK);
        else CHECK(close_typed(&f) == AMS_MEL_OK);
        CHECK(!pthread_join(thread, NULL) && op.status == AMS_MEL_STREAM_STOPPED);
        finish(&f); return;
    }
    if (!strcmp(scenario, "comms-meta-poison")) {
        CHECK(ams_mel_test_comms_metadata(6, NULL));
        submit(&f, UINT32_MAX, 0x80000001U, 0);
        CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_PROVIDER_FAILED);
        finish(&f); return;
    }
    if (!strcmp(scenario, "comms-meta-drain") || !strcmp(scenario, "comms-meta-saturate")) {
        if (!strcmp(scenario, "comms-meta-saturate")) CHECK(ams_mel_test_comms_metadata(7, f.metadata));
        for (unsigned i = 0; i < 4; ++i) submit(&f, UINT32_MAX - i, 0x80000001U + i, 0);
        if (!strcmp(scenario, "comms-meta-saturate")) {
            CHECK(counters_signature(f.metadata, &counters, ARGS) == AMS_MEL_OK);
            CHECK(counters.events_received == UINT64_MAX && counters.events_dropped_queue_full == UINT64_MAX);
        }
        CHECK(close_typed(&f) == AMS_MEL_OK);
        for (unsigned i = 0; i < 2; ++i) {
            CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_OK);
            CHECK(report.command_id == UINT32_MAX - i && report.request_id == 0x80000001U + i);
        }
        CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_STREAM_STOPPED);
        finish(&f); return;
    }
    if (!strcmp(scenario, "comms-meta-concurrent")) {
        pthread_t threads[8];
        for (unsigned i = 0; i < 8; ++i) CHECK(!pthread_create(&threads[i], NULL, concurrent_submit, &f));
        for (unsigned i = 0; i < 8; ++i) CHECK(!pthread_join(threads[i], NULL));
        CHECK(ams_mel_ir_channel_comms_metadata_get_counters(f.metadata, &counters, ARGS) == AMS_MEL_OK);
        CHECK(counters.events_received == 8 && counters.events_dropped_queue_full == 6);
        for (unsigned i = 0; i < 2; ++i) CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_OK);
    } else submit(&f, UINT32_MAX, 0x80000001U, strcmp(scenario, "comms-meta-no-event") != 0);
    if (!strcmp(scenario, "comms-meta-no-event")) {
        CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_TIMEOUT);
    }
    if (!strcmp(scenario, "comms-meta-owner-first"))
        CHECK(ams_mel_ir_channel_comms_metadata_close(&f.metadata, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_close(&f.view, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&f.session, ARGS) == AMS_MEL_OK);
    if (!strcmp(scenario, "comms-meta-detach-fail")) {
        CHECK(close_typed(&f) == AMS_MEL_PROVIDER_FAILED);
        CHECK(receive_signature(f.metadata, 0, &report, ARGS) == AMS_MEL_PROVIDER_FAILED);
    }
    /* Image retains its sticky Failed state after an unproven detach, even
     * when the subsequent physical release succeeds. Preserve that policy. */
    CHECK(close_typed(&f) == (((family == 2 || family == 4) && !strcmp(scenario, "comms-meta-detach-fail")) ?
          AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
    if (f.metadata) CHECK(receive_signature(f.metadata, UINT32_MAX, &report, ARGS) ==
        (!strcmp(scenario, "comms-meta-detach-fail") ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_STREAM_STOPPED));
    finish(&f);
}

static void collision(int old_first)
{
    fixture f; start(&f, 0, "comms-meta-basic");
    ams_mel_ir_c2_metadata *old = NULL;
    CHECK(ams_mel_ir_c2_metadata_open(f.c2, 4, &old, ARGS) == AMS_MEL_OK);
    if (old_first) {
        CHECK(ams_mel_ir_c2_metadata_register_comms_test(old, ARGS) == AMS_MEL_OK);
        CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    } else {
        CHECK(open_signature(f.view, 2, &f.metadata, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_metadata_register_comms_test(old, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(ams_mel_ir_c2_metadata_close(&old, ARGS) == AMS_MEL_OK);
    finish(&f);
}
static void routed(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_health *health = NULL; ams_mel_ir_stream *image = NULL;
    ams_mel_ir_stacked *stacked = NULL; ams_mel_ir_channel *views[4] = {0};
    ams_mel_ir_channel_comms_metadata *owners[4] = {0};
    ams_mel_ir_c2_config_v1 c = {0}; c.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    ams_mel_ir_health_config_v1 h = {0}; h.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    ams_mel_ir_image_control_config_v1 i = {0}; i.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    ams_mel_ir_stacked_config_v1 s = {0}; s.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
    c.channel_id.uuid[0] = 1; h.channel_id.uuid[0] = 2; i.channel_id.uuid[0] = 3; s.channel_id.uuid[0] = 4;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "comms-meta-routed", "", &session, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_open(session, &c, &c2, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_health_open(session, &h, &health, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_control_open(session, &i, &image, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stacked_open(session, &s, &stacked, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_c2(c2, &views[0], ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_health(health, &views[1], ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_stream(image, &views[2], ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_stacked(stacked, &views[3], ARGS) == AMS_MEL_OK);
    for (unsigned n = 0; n < 4; ++n) CHECK(open_signature(views[n], 2, &owners[n], ARGS) == AMS_MEL_OK);
    for (unsigned target = 0; target < 4; ++target) {
        ams_mel_ir_channel_comms_test_request_v1 input = {UINT32_MAX - target, target + 1, 0x80000001U + target};
        ams_mel_ir_channel_comms_request *request = NULL;
        ams_mel_ir_channel_comms_test_result_v1 result;
        CHECK(ams_mel_ir_channel_submit_comms_test(views[0], &input, &request, ARGS) == AMS_MEL_OK);
        for (unsigned n = 0; n < 4; ++n) {
            ams_mel_ir_channel_comms_test_report_v1 report = {17, 19};
            CHECK(receive_signature(owners[n], 0, &report, ARGS) == (n == target ? AMS_MEL_OK : AMS_MEL_TIMEOUT));
            CHECK(report.command_id == (n == target ? input.command_id : 17));
            CHECK(report.request_id == (n == target ? input.request_id : 19));
        }
        CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result, ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_comms_request_close(&request, ARGS) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_c2_close(&c2, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_health_close(&health, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&image, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stacked_close(&stacked, ARGS) == AMS_MEL_OK);
    for (unsigned n = 0; n < 4; ++n) {
        CHECK(close_signature(&owners[n], ARGS) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_close(&views[n], ARGS) == AMS_MEL_OK);
    }
    CHECK(ams_mel_session_close(&session, ARGS) == AMS_MEL_OK);
}
static void unsupported(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_instrumentation *instrumentation = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_scheduling *scheduling = NULL;
    ams_mel_ir_channel *views[3] = {0};
    ams_mel_ir_channel_comms_metadata *metadata = NULL;
    ams_mel_ir_instrumentation_config_v1 i = {0}; i.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    ams_mel_ir_track_config_v1 t = {0}; t.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    ams_mel_ir_scheduling_config_v1 s = {0}; s.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "comms-meta-unsupported", "", &session, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_open(session, &i, &instrumentation, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &t, &track, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_open(session, &s, &scheduling, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_instrumentation(instrumentation, &views[0], ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_track(track, &views[1], ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_from_scheduling(scheduling, &views[2], ARGS) == AMS_MEL_OK);
    for (unsigned n = 0; n < 3; ++n) {
        char diagnostic[128];
        CHECK(open_signature(views[n], 2, &metadata, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_INITIALIZATION_FAILED);
        CHECK(!metadata && strstr(diagnostic, "unsupported"));
        CHECK(ams_mel_ir_channel_close(&views[n], ARGS) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_instrumentation_close(&instrumentation, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_ir_scheduling_close(&scheduling, ARGS) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, ARGS) == AMS_MEL_OK);
}
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    CHECK(open_signature(NULL, 1, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(receive_signature(NULL, 0, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_close(NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    if (!strcmp(argv[2], "collision")) { collision(0); collision(1); }
    else if (!strcmp(argv[2], "routed")) routed();
    else if (!strcmp(argv[2], "unsupported")) unsupported();
    else if (!strcmp(argv[2], "comms-meta-reentry")) registration_reentry((unsigned)strtoul(argv[1], NULL, 10));
    else if (!strcmp(argv[2], "comms-meta-inflight")) inflight((unsigned)strtoul(argv[1], NULL, 10));
    else if (!strcmp(argv[2], "comms-meta-held")) held((unsigned)strtoul(argv[1], NULL, 10));
    else exercise((unsigned)strtoul(argv[1], NULL, 10), argv[2]);
    return 0;
}
