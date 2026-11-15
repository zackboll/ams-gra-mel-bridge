#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
#define CALL(fn, ...) CHECK(fn(__VA_ARGS__, ARGS) == AMS_MEL_OK)
static ams_mel_status_t (*const signature)(const ams_mel_ir_channel *,
    ams_mel_ir_return_t *, char *, size_t, size_t *) = ams_mel_ir_channel_disable;
static ams_mel_ir_c2 *c2;
static ams_mel_ir_health *health;
static ams_mel_ir_stacked *stacked;
static ams_mel_ir_stream *image;
static ams_mel_ir_channel *view, *second;
static ams_mel_session *session;
static ams_mel_ir_channel_comms_metadata *metadata;
static ams_mel_ir_c2_metadata *c2_metadata;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int entered, released;
static char log_path[] = "/tmp/ams-disable-XXXXXX";
static unsigned count(const char *event)
{
    FILE *f = fopen(log_path, "r"); char line[256]; unsigned n = 0;
    CHECK(f);
    while (fgets(line, sizeof line, f)) if (!strcmp(line, event)) ++n;
    CHECK(!fclose(f)); return n;
}
static unsigned position(const char *event)
{
    FILE *f = fopen(log_path, "r"); char line[256]; unsigned n = 0;
    CHECK(f);
    while (fgets(line, sizeof line, f)) {
        ++n;
        if (!strcmp(line, event)) { CHECK(!fclose(f)); return n; }
    }
    CHECK(!fclose(f)); return 0;
}
static ams_mel_status_t enable(void)
{
    if (c2) return ams_mel_ir_c2_enable(c2, ARGS);
    if (health) return ams_mel_ir_health_enable(health, ARGS);
    if (stacked) return ams_mel_ir_stacked_enable(stacked, ARGS);
    return ams_mel_ir_stream_start(image, ARGS);
}
static ams_mel_status_t close_typed(void)
{
    if (c2) return ams_mel_ir_c2_close(&c2, ARGS);
    if (health) return ams_mel_ir_health_close(&health, ARGS);
    if (stacked) return ams_mel_ir_stacked_close(&stacked, ARGS);
    return ams_mel_ir_stream_close(&image, ARGS);
}
static void reenter(void)
{
    ams_mel_ir_return_t result = 0;
    CHECK(ams_mel_ir_channel_disable(second, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CHECK(enable() == AMS_MEL_PROVIDER_FAILED);
    if (metadata) {
        ams_mel_ir_metadata_counters_v1 counters;
        CALL(ams_mel_ir_channel_comms_metadata_get_counters, metadata, &counters);
        CHECK(counters.events_received == 1);
    }
}
static void close_reentry(void)
{
    reenter();
    CHECK(close_typed() == AMS_MEL_OK);
    CALL(ams_mel_session_close, &session);
    CALL(ams_mel_ir_channel_close, &view);
    CHECK(count("channel_detached\n") == 0);
}
static void enable_inflight(void)
{
    ams_mel_ir_return_t result;
    CHECK(ams_mel_ir_channel_disable(second, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX && enable() == AMS_MEL_PROVIDER_FAILED);
}
static void barrier(void)
{
    CHECK(!pthread_mutex_lock(&mutex)); entered = 1;
    CHECK(!pthread_cond_broadcast(&ready));
    while (!released) CHECK(!pthread_cond_wait(&ready, &mutex));
    CHECK(!pthread_mutex_unlock(&mutex));
}
static void *disable_thread(void *unused)
{
    ams_mel_ir_return_t result;
    (void)unused;
    CALL(ams_mel_ir_channel_disable, second, &result);
    CHECK(result == AMS_MEL_IR_RETURN_SUCCESS);
    return NULL;
}
int main(int argc, char **argv)
{
    ams_mel_ir_c2_config_v1 cc = {0};
    ams_mel_ir_health_config_v1 hc = {0};
    ams_mel_ir_stacked_config_v1 sc = {0};
    ams_mel_ir_image_control_config_v1 ic = {0};
    ams_mel_ir_return_t result = 0;
    ams_mel_ir_channel_capability *capability = NULL;
    ams_mel_ir_channel_comms_request *comms = NULL;
    ams_mel_ir_return_request *keep = NULL;
    ams_mel_ir_channel_comms_test_request_v1 request = {0x80000001U, 0xf0000002U, 0xe0000003U};
    ams_mel_ir_channel_comms_test_result_v1 cr;
    ams_mel_ir_return_result_v1 kr;
    char scenario[96]; const char *which; int fd, failure;
    void *pin, *symbol;
    void (*install)(void (*)(void));
    void (*install_enable)(void (*)(void));
    int (*release_family)(unsigned, unsigned);
    CHECK(argc == 3 && signature); which = argv[2];
    CHECK(snprintf(scenario, sizeof scenario, "common-meta-disable019-%s", which) > 0);
    fd = mkstemp(log_path); CHECK(fd >= 0); CHECK(!close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1));
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin);
    symbol = dlsym(pin, "mock_disable_reentry"); CHECK(symbol); memcpy(&install, &symbol, sizeof install);
    symbol = dlsym(pin, "mock_enable_reentry"); CHECK(symbol); memcpy(&install_enable, &symbol, sizeof install_enable);
    symbol = dlsym(pin, "mock_completion_release_family"); CHECK(symbol); memcpy(&release_family, &symbol, sizeof release_family);
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session);
    cc.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    hc.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    sc.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
    ic.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    if (!strcmp(argv[1], "c2")) {
        CALL(ams_mel_ir_c2_open, session, &cc, &c2);
        CALL(ams_mel_ir_channel_from_c2, c2, &view);
        CALL(ams_mel_ir_channel_from_c2, c2, &second);
    } else if (!strcmp(argv[1], "health")) {
        CALL(ams_mel_ir_health_open, session, &hc, &health);
        CALL(ams_mel_ir_channel_from_health, health, &view);
        CALL(ams_mel_ir_channel_from_health, health, &second);
    } else if (!strcmp(argv[1], "stacked")) {
        CALL(ams_mel_ir_stacked_open, session, &sc, &stacked);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &view);
        CALL(ams_mel_ir_channel_from_stacked, stacked, &second);
    } else {
        CALL(ams_mel_ir_image_control_open, session, &ic, &image);
        CALL(ams_mel_ir_channel_from_stream, image, &view);
        CALL(ams_mel_ir_channel_from_stream, image, &second);
    }
    CHECK(ams_mel_ir_channel_disable(NULL, &result, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_channel_disable(view, NULL, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_channel_disable(view, &result, NULL, 1, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CALL(ams_mel_ir_channel_disable, view, &result); CHECK(result == UINT32_MAX);
    CHECK(count("standalone_disable_entered\n") == 0);
    if (!strcmp(which, "never")) {
        CHECK(close_typed() == AMS_MEL_OK);
        goto done;
    }
    if (!strcmp(which, "enable-inflight")) install_enable(enable_inflight);
    CHECK(enable() == AMS_MEL_OK);
    if (strcmp(argv[1], "c2")) {
        CALL(ams_mel_ir_channel_comms_metadata_open, view, 8, &metadata, &result);
    } else {
        CALL(ams_mel_ir_c2_metadata_open, c2, 8, &c2_metadata);
        CALL(ams_mel_ir_c2_metadata_register_comms_test, c2_metadata);
    }
    if (!strcmp(which, "pending")) {
        CALL(ams_mel_ir_channel_send_keepalive, view, &keep);
        CALL(ams_mel_ir_channel_submit_comms_test, view, &request, &comms);
        CHECK(ams_mel_ir_return_request_wait(keep, 0, &kr, ARGS) == AMS_MEL_TIMEOUT);
        CHECK(ams_mel_ir_channel_comms_request_wait(comms, 0, &cr, ARGS) == AMS_MEL_TIMEOUT);
    }
    if (!strcmp(which, "reentry")) install(reenter);
    if (!strcmp(which, "close")) install(close_reentry);
    failure = !strcmp(which, "fail") || !strcmp(which, "refuse") || !strcmp(which, "throw") || !strcmp(which, "failed-detach");
    if (!strcmp(which, "concurrent")) {
        pthread_t thread; install(barrier);
        CHECK(!pthread_create(&thread, NULL, disable_thread, NULL));
        CHECK(!pthread_mutex_lock(&mutex));
        while (!entered) CHECK(!pthread_cond_wait(&ready, &mutex));
        CHECK(!pthread_mutex_unlock(&mutex));
        reenter();
        CHECK(close_typed() == AMS_MEL_OK);
        CALL(ams_mel_session_close, &session);
        CHECK(count("channel_detached\n") == 0);
        CHECK(!pthread_mutex_lock(&mutex)); released = 1;
        CHECK(!pthread_cond_broadcast(&ready)); CHECK(!pthread_mutex_unlock(&mutex));
        CHECK(!pthread_join(thread, NULL));
    } else {
        ams_mel_status_t status = ams_mel_ir_channel_disable(view, &result, ARGS);
        CHECK(status == (!strcmp(which, "throw") ? AMS_MEL_PROVIDER_EXCEPTION :
            failure ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
        CHECK(result == (!strcmp(which, "throw") ? UINT32_MAX :
            (!strcmp(which, "fail") || !strcmp(which, "failed-detach")) ? AMS_MEL_IR_RETURN_FAIL :
            !strcmp(which, "refuse") ? AMS_MEL_IR_RETURN_NOT_SUPPORTED : AMS_MEL_IR_RETURN_SUCCESS));
    }
    CHECK(count("standalone_disable_entered\n") == 1);
    CHECK(count("standalone_disable_returning\n") == 1);
    CHECK(position("standalone_disable_entered\n") < position("standalone_disable_callback_returned\n"));
    CHECK(position("standalone_disable_callback_returned\n") < position("standalone_disable_returning\n"));
    if (!strcmp(which, "close") || !strcmp(which, "concurrent")) goto done;
    if (failure) {
        CHECK(ams_mel_ir_channel_disable(second, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
        CHECK(result == UINT32_MAX && enable() == AMS_MEL_PROVIDER_FAILED);
        CHECK(close_typed() == AMS_MEL_PROVIDER_FAILED);
        if (!strcmp(which, "failed-detach")) {
            CHECK(c2 || health || stacked || image);
            CHECK(count("channel_detach_failed\n") == 1 && count("channel_detached\n") == 0);
            CHECK(close_typed() == AMS_MEL_PROVIDER_FAILED);
        }
        CHECK(!c2 && !health && !stacked && !image);
        goto done;
    }
    CHECK(count("channel_detached\n") == 0);
    CALL(ams_mel_ir_channel_disable, second, &result); CHECK(result == UINT32_MAX);
    CALL(ams_mel_ir_channel_get_capabilities, second, &capability);
    CALL(ams_mel_ir_channel_capability_close, &capability);
    if (metadata) {
        ams_mel_ir_channel_comms_test_report_v1 report;
        if (keep) {
            CALL(ams_mel_ir_channel_comms_metadata_receive, metadata, 0, &report);
            CHECK(report.command_id == request.command_id && report.request_id == request.request_id);
        }
        CALL(ams_mel_ir_channel_comms_metadata_receive, metadata, 0, &report);
        CHECK(report.command_id == 91 && report.request_id == 92);
    }
    if (c2_metadata) {
        ams_mel_ir_c2_metadata_event *event = NULL;
        const ams_mel_ir_c2_metadata_event_v1 *data = NULL;
        CALL(ams_mel_ir_c2_metadata_receive, c2_metadata, 0, &event);
        CALL(ams_mel_ir_c2_metadata_event_view, event, &data);
        CHECK(data->channel_comms_test.command_id == 91 && data->channel_comms_test.request_id == 92);
        CALL(ams_mel_ir_c2_metadata_event_close, &event);
    }
    if (keep) {
        unsigned k = stacked ? 13U : 1U, c = stacked ? 14U : 2U;
        CHECK(ams_mel_ir_return_request_wait(keep, 0, &kr, ARGS) == AMS_MEL_TIMEOUT);
        CHECK(ams_mel_ir_channel_comms_request_wait(comms, 0, &cr, ARGS) == AMS_MEL_TIMEOUT);
        CHECK(release_family(k, 1) && release_family(c, 1));
        CALL(ams_mel_ir_return_request_wait, keep, 1000, &kr);
        CALL(ams_mel_ir_channel_comms_request_wait, comms, 1000, &cr);
        CHECK(cr.command_id == request.command_id && cr.request_id == request.request_id);
        CALL(ams_mel_ir_return_request_close, &keep);
        CALL(ams_mel_ir_channel_comms_request_close, &comms);
    }
    if (!strcmp(which, "cycles")) {
        CHECK(enable() == AMS_MEL_OK);
        CALL(ams_mel_ir_channel_disable, second, &result);
        CHECK(result == AMS_MEL_IR_RETURN_SUCCESS);
        CHECK(count("standalone_disable_entered\n") == 2);
    }
    if (!strcmp(which, "detach-fail")) {
        CHECK(close_typed() == AMS_MEL_PROVIDER_FAILED);
        CHECK(count("standalone_disable_entered\n") == 1 && count("channel_detached\n") == 0);
        const ams_mel_status_t expected = image ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK;
        CHECK(close_typed() == expected);
    } else CHECK(close_typed() == AMS_MEL_OK);
done:
    CHECK(ams_mel_ir_channel_disable(second, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
    CHECK(result == UINT32_MAX);
    CALL(ams_mel_ir_channel_comms_metadata_close, &metadata);
    CALL(ams_mel_ir_c2_metadata_close, &c2_metadata);
    CALL(ams_mel_ir_channel_close, &view);
    CALL(ams_mel_ir_channel_close, &second);
    CALL(ams_mel_session_close, &session);
    CHECK(count("channel_detached\n") == 1);
    if (strcmp(which, "never"))
        CHECK(position("standalone_disable_returning\n") < position("channel_detached\n"));
    CHECK(count("standalone_disable_entered\n") == (!strcmp(which, "never") ? 0U : !strcmp(which, "cycles") ? 2U : 1U));
    CHECK(count("buffer_factory_called\n") == 0 && count("buffer_registered\n") == 0 && count("buffer_initialized\n") == 0);
    CHECK(count("stacked_forbidden_registration\n") == 0);
    CHECK(count("control_destroyed\n") == 1);
    CHECK(!dlclose(pin));
    CHECK(count("library_unloaded\n") == 1);
    CHECK(!unlink(log_path));
    return 0;
}
