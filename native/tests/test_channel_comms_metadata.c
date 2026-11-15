#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define CALL(fn, ...) CHECK(fn(__VA_ARGS__, NULL, 0, NULL) == AMS_MEL_OK)
extern int ams_mel_test_comms_metadata_probe(ams_mel_ir_channel_comms_metadata *, unsigned, void (*)(void));
typedef int (*release_fn)(unsigned, unsigned);

/* Strict C11 signature checks for every new export; the existing records are
 * reused without adding any event allocation or changing layouts. */
static ams_mel_status_t (*const open_signature)(const ams_mel_ir_channel *, size_t,
    ams_mel_ir_channel_comms_metadata **, ams_mel_ir_return_t *, char *, size_t, size_t *) =
    ams_mel_ir_channel_comms_metadata_open;
static ams_mel_status_t (*const receive_signature)(const ams_mel_ir_channel_comms_metadata *, uint32_t,
    ams_mel_ir_channel_comms_test_report_v1 *, char *, size_t, size_t *) =
    ams_mel_ir_channel_comms_metadata_receive;
static ams_mel_status_t (*const counters_signature)(const ams_mel_ir_channel_comms_metadata *,
    ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *) =
    ams_mel_ir_channel_comms_metadata_get_counters;
static ams_mel_status_t (*const close_signature)(ams_mel_ir_channel_comms_metadata **,
    char *, size_t, size_t *) = ams_mel_ir_channel_comms_metadata_close;
_Static_assert(sizeof(ams_mel_ir_channel_comms_test_report_v1) == 8, "two uint32 fields");
_Static_assert(sizeof(ams_mel_ir_metadata_counters_v1) == 24, "three uint64 counters");

static ams_mel_ir_health *health;
static ams_mel_ir_stream *image;
static ams_mel_ir_stacked *stacked;
static ams_mel_ir_channel *view;
static ams_mel_ir_channel_comms_metadata *metadata;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int entered, released;

static void barrier(void)
{
    CHECK(!pthread_mutex_lock(&mutex));
    entered = 1;
    CHECK(!pthread_cond_broadcast(&ready));
    while (!released) CHECK(!pthread_cond_wait(&ready, &mutex));
    CHECK(!pthread_mutex_unlock(&mutex));
}
static void reenter(void)
{
    ams_mel_ir_metadata_counters_v1 counters;
    CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
    CALL(ams_mel_ir_channel_close, &view);
}
static void close_typed(void)
{
    CALL(ams_mel_ir_health_close, &health);
    CALL(ams_mel_ir_stream_close, &image);
    CALL(ams_mel_ir_stacked_close, &stacked);
}
static void close_reentry(void)
{
    CALL(ams_mel_ir_channel_close, &view);
    close_typed();
}
static void *close_thread(void *unused)
{
    (void)unused;
    close_typed();
    return NULL;
}
static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r"); char line[256]; unsigned n = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++n;
    CHECK(!fclose(file)); return n;
}
static void touch(const char *base, const char *suffix)
{
    char path[256]; FILE *file;
    CHECK(snprintf(path, sizeof path, "%s%s", base, suffix) > 0);
    file = fopen(path, "w"); CHECK(file); CHECK(!fclose(file));
}
static void check_report(uint32_t command, uint32_t request)
{
    ams_mel_ir_channel_comms_test_report_v1 report;
    CALL(ams_mel_ir_channel_comms_metadata_receive, metadata, 0, &report);
    CHECK(report.command_id == command && report.request_id == request);
}
static void *concurrent_submit(void *argument)
{
    const unsigned i = *(const unsigned *)argument;
    ams_mel_ir_channel_comms_test_request_v1 input = {UINT32_MAX - i, 0, 0x80000000U + i};
    ams_mel_ir_channel_comms_request *request = NULL;
    ams_mel_ir_channel_comms_test_result_v1 result;
    CALL(ams_mel_ir_channel_submit_comms_test, view, &input, &request);
    CALL(ams_mel_ir_channel_comms_request_wait, request, 1000, &result);
    CHECK(result.command_id == input.command_id && result.request_id == input.request_id);
    CALL(ams_mel_ir_channel_comms_request_close, &request);
    return NULL;
}
int main(int argc, char **argv)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *c2_metadata = NULL;
    ams_mel_ir_channel *second = NULL, *c2_view = NULL;
    ams_mel_ir_channel_comms_metadata *duplicate = NULL;
    ams_mel_ir_channel_comms_test_report_v1 report;
    ams_mel_ir_metadata_counters_v1 counters;
    ams_mel_ir_return_t registration = UINT32_MAX;
    ams_mel_ir_health_config_v1 hc = {0};
    ams_mel_ir_stacked_config_v1 sc = {0};
    ams_mel_ir_image_control_config_v1 ic = {0};
    ams_mel_ir_stream_config_v1 ac = {0};
    ams_mel_ir_c2_config_v1 cc = {0};
    char scenario[80], log[] = "/tmp/ams-comms-log-XXXXXX", base[] = "/tmp/ams-comms-gate-XXXXXX";
    const char *which; unsigned family, i; int fd; void *pin = NULL;
    CHECK(argc == 3); which = argv[2];
    CHECK(open_signature && receive_signature && counters_signature && close_signature);
    CHECK(snprintf(scenario, sizeof scenario, "common-meta-%s", which) > 0);
    if (!strcmp(which, "reentry")) strcpy(scenario, "common-meta-parity");
    fd = mkstemp(log); CHECK(fd >= 0); CHECK(!close(fd));
    fd = mkstemp(base); CHECK(fd >= 0); CHECK(!close(fd)); CHECK(!unlink(base));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1));
    CHECK(!setenv("AMS_MEL_TEST_COMMS_BARRIER", base, 1));
    if (!strcmp(which, "register-reentry")) {
        void (*install)(void (*)(void)); void *symbol;
        pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin);
        symbol = dlsym(pin, "mock_comms_reentry"); CHECK(symbol); memcpy(&install, &symbol, sizeof install);
        install(close_reentry);
    }
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session);
    CHECK(ams_mel_ir_channel_comms_metadata_open(NULL, 1, &metadata, &registration, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_receive(NULL, 0, &report, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_get_counters(NULL, &counters, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    hc.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    sc.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
    ic.channel_type = ac.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    cc.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    if (!strcmp(argv[1], "health")) {
        family = 4; CALL(ams_mel_ir_health_open, session, &hc, &health);
        CALL(ams_mel_ir_channel_from_health, health, &view);
        CALL(ams_mel_ir_channel_from_health, health, &second);
    } else if (!strcmp(argv[1], "stacked")) {
        family = 6; CALL(ams_mel_ir_stacked_open, session, &sc, &stacked);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &view);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &second);
    } else {
        family = 1;
        if (!strcmp(argv[1], "acquisition")) {
            ac.buffer_count = 3; ac.buffer_size = 1024; ac.queue_capacity = 4;
            CALL(ams_mel_ir_stream_open, session, &ac, &image);
        } else CALL(ams_mel_ir_image_control_open, session, &ic, &image);
        CALL(ams_mel_ir_channel_from_stream, image, &view);
        CALL(ams_mel_ir_channel_from_stream, image, &second);
    }
    if (!strcmp(which, "disable-fail") || !strcmp(which, "enabled")) {
        if (health) CALL(ams_mel_ir_health_enable, health);
        if (stacked) CALL(ams_mel_ir_stacked_enable, stacked);
        if (image) CALL(ams_mel_ir_stream_start, image);
    }
    CHECK(ams_mel_ir_channel_comms_metadata_open(view, 0, &metadata, &registration, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_open(view, 2, NULL, &registration, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_open(view, 2, &metadata, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_open(view, 2, &metadata, &registration, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_comms_metadata_open(view, SIZE_MAX, &metadata, &registration, NULL, 0, NULL) == AMS_MEL_INTERNAL_ERROR);
    CHECK(!metadata && count(log, "common_comms_registration\n") == 0);
    if (!strcmp(which, "expired")) {
        close_typed();
        CHECK(ams_mel_ir_channel_comms_metadata_open(view, 2, &metadata, &registration, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(!metadata && count(log, "common_comms_registration\n") == 0);
        goto done;
    }
    if (!strcmp(which, "zero")) {
        ams_mel_ir_channel_capability *capability = NULL;
        const ams_mel_ir_channel_capability_v1 *cap = NULL;
        CALL(ams_mel_ir_channel_get_capabilities, view, &capability);
        CALL(ams_mel_ir_channel_capability_view, capability, &cap);
        CHECK(!cap->height && !cap->width && !cap->bit_depth && !cap->row_pitch &&
              !cap->buffer_size && !cap->image_size && !cap->number_of_bands);
        CALL(ams_mel_ir_channel_capability_close, &capability);
    }
    {
        ams_mel_status_t status = ams_mel_ir_channel_comms_metadata_open(view, 2, &metadata, &registration, NULL, 0, NULL);
        int failed = !strcmp(which, "refuse") || !strcmp(which, "fail") || !strcmp(which, "partial") || !strcmp(which, "throw");
        CHECK(status == (!strcmp(which, "throw") ? AMS_MEL_PROVIDER_EXCEPTION : failed ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
        CHECK((metadata != NULL) == !failed);
        if (failed && strcmp(which, "throw")) CHECK(registration == (!strcmp(which, "refuse") ? AMS_MEL_IR_RETURN_NOT_SUPPORTED : AMS_MEL_IR_RETURN_FAIL));
        CHECK(ams_mel_ir_channel_comms_metadata_open(second, 2, &duplicate, &registration, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(!duplicate); CHECK(count(log, "common_comms_registration\n") == 1);
        if (failed) goto done;
    }
    if (metadata) {
        CHECK(registration == AMS_MEL_IR_RETURN_SUCCESS);
        CHECK(ams_mel_ir_channel_comms_metadata_open(view, 2, &metadata, &registration, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_channel_comms_metadata_get_counters(metadata, NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    }
    if (!strcmp(which, "register-reentry")) {
        CHECK(!health && !image && !stacked && !view);
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    } else if (!strcmp(which, "disable-fail") || !strcmp(which, "detach-fail")) {
        ams_mel_status_t status;
        if (health) status = ams_mel_ir_health_close(&health, NULL, 0, NULL);
        else if (stacked) status = ams_mel_ir_stacked_close(&stacked, NULL, 0, NULL);
        else status = ams_mel_ir_stream_close(&image, NULL, 0, NULL);
        CHECK(status == AMS_MEL_PROVIDER_FAILED);
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(count(log, "library_unloaded\n") == 0);
        if (!strcmp(which, "detach-fail")) CHECK(health || stacked || image);
        if (image) {
            /* Image preserves the poisoned cleanup result even after a retry
             * succeeds physically. Do not weaken that established contract. */
            CHECK(ams_mel_ir_stream_close(&image, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
            CHECK(!image);
        } else close_typed();
    } else if (!strcmp(which, "sync")) {
        close_typed(); check_report(0xabcdef01U, 0xfedcba98U);
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    } else if (!strcmp(which, "null")) {
        CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
        CHECK(counters.events_received == 1 && counters.malformed_or_unsupported == 1);
    } else if (!strcmp(which, "overflow")) {
        check_report(0x80000000U, 0xf0000000U); check_report(0x80000001U, 0xf0000001U);
        CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
        CHECK(counters.events_received == 6 && counters.events_dropped_queue_full == 4);
    } else if (!strcmp(which, "concurrent")) {
        pthread_t threads[8]; unsigned indices[8];
        for (i = 0; i < 8; ++i) { indices[i] = i; CHECK(!pthread_create(&threads[i], NULL, concurrent_submit, &indices[i])); }
        for (i = 0; i < 8; ++i) CHECK(!pthread_join(threads[i], NULL));
        CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
        CHECK(counters.events_received == 8 && counters.events_dropped_queue_full == 6);
        for (i = 0; i < 2; ++i) {
            CALL(ams_mel_ir_channel_comms_metadata_receive, metadata, 0, &report);
            CHECK(report.command_id >= UINT32_MAX - 7);
            CHECK(report.request_id == 0x80000000U + (UINT32_MAX - report.command_id));
        }
    } else if (!strcmp(which, "async") || !strcmp(which, "async-typed")) {
        pthread_t closer;
        int typed = !strcmp(which, "async-typed");
        CHECK(ams_mel_test_comms_metadata_probe(metadata, 1, barrier));
        touch(base, ".start");
        CHECK(!pthread_mutex_lock(&mutex));
        while (!entered) CHECK(!pthread_cond_wait(&ready, &mutex));
        CHECK(!pthread_mutex_unlock(&mutex));
        if (typed) {
            ams_mel_status_t status;
            CHECK(!pthread_create(&closer, NULL, close_thread, NULL));
            do {
                status = ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL);
                if (status == AMS_MEL_TIMEOUT) sched_yield();
            } while (status == AMS_MEL_TIMEOUT);
            CHECK(status == AMS_MEL_STREAM_STOPPED);
            CHECK(count(log, "library_unloaded\n") == 0);
        } else CALL(ams_mel_ir_channel_comms_metadata_close, &metadata);
        CHECK(!pthread_mutex_lock(&mutex)); released = 1; CHECK(!pthread_cond_broadcast(&ready)); CHECK(!pthread_mutex_unlock(&mutex));
        if (typed) CHECK(!pthread_join(closer, NULL));
        close_typed(); CHECK(count(log, "common_comms_callback_returned\n") == 1);
    } else {
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 1, &report, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        if (!strcmp(which, "held")) { pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin); }
        if (!strcmp(which, "saturation")) CHECK(ams_mel_test_comms_metadata_probe(metadata, 0, NULL));
        if (!strcmp(which, "reentry")) CHECK(ams_mel_test_comms_metadata_probe(metadata, 1, reenter));
        CALL(ams_mel_ir_c2_open, session, &cc, &c2);
        CALL(ams_mel_ir_channel_from_c2, c2, &c2_view);
        CHECK(ams_mel_ir_channel_comms_metadata_open(c2_view, 2, &duplicate, &registration, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CALL(ams_mel_ir_c2_metadata_open, c2, 2, &c2_metadata);
        CALL(ams_mel_ir_c2_metadata_register_comms_test, c2_metadata);
        for (i = 0; i < (!strcmp(which, "saturation") ? 4U : 2U); ++i) {
            ams_mel_ir_channel_comms_test_request_v1 request = {0x80000001U + i, family, 0xf0000003U + i};
            ams_mel_ir_channel_comms_request *pending = NULL;
            ams_mel_ir_channel_comms_test_result_v1 result;
            if (i == 0) CALL(ams_mel_ir_channel_submit_comms_test, second, &request, &pending);
            else CALL(ams_mel_ir_c2_submit_comms_test, c2, &request, &pending);
            if (strcmp(which, "future-only") && strcmp(which, "saturation")) check_report(request.command_id, request.request_id);
            if (!strcmp(which, "held") && i == 0) {
                release_fn release; void *symbol;
                CHECK(ams_mel_ir_channel_comms_request_wait(pending, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
                close_typed(); CALL(ams_mel_session_close, &session);
                symbol = dlsym(pin, "mock_completion_release_family"); CHECK(symbol); memcpy(&release, &symbol, sizeof release);
                CHECK(release(family == 6 ? 14 : 2, 1));
            }
            CALL(ams_mel_ir_channel_comms_request_wait, pending, 1000, &result);
            CHECK(result.command_id == request.command_id && result.request_id == request.request_id);
            CALL(ams_mel_ir_channel_comms_request_close, &pending);
            if (!strcmp(which, "held")) break;
        }
        if (!strcmp(which, "saturation")) {
            CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
            CHECK(counters.events_received == UINT64_MAX && counters.events_dropped_queue_full == UINT64_MAX &&
                  counters.malformed_or_unsupported == UINT64_MAX);
            check_report(0x80000001U, 0xf0000003U); check_report(0x80000002U, 0xf0000004U);
        } else if (strcmp(which, "held")) {
            CHECK(ams_mel_ir_channel_comms_metadata_receive(metadata, 0, &report, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
        }
        {
            ams_mel_ir_c2_metadata_event *wrong_channel = NULL;
            CHECK(ams_mel_ir_c2_metadata_receive(c2_metadata, 0, &wrong_channel, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
            CHECK(!wrong_channel);
        }
        CALL(ams_mel_ir_c2_metadata_close, &c2_metadata); CALL(ams_mel_ir_channel_close, &c2_view); CALL(ams_mel_ir_c2_close, &c2);
    }
done:
    if (!strcmp(which, "owner-first")) CALL(ams_mel_ir_channel_comms_metadata_close, &metadata);
    CALL(ams_mel_ir_channel_close, &view); CALL(ams_mel_ir_channel_close, &second);
    close_typed(); CALL(ams_mel_session_close, &session);
    CALL(ams_mel_ir_channel_comms_metadata_close, &metadata); CALL(ams_mel_ir_channel_comms_metadata_close, &metadata);
    if (pin) CHECK(!dlclose(pin));
    if (!strcmp(which, "partial") || !strcmp(which, "throw"))
        CHECK(count(log, "common_comms_partial_callback_at_destruction\n") == 1);
    CHECK(count(log, "library_unloaded\n") == 1);
    CHECK(!unlink(log));
    { char path[256]; snprintf(path, sizeof path, "%s.start", base); (void)unlink(path); }
    return 0;
}
