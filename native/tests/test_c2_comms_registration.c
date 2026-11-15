#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
#define CALL(fn, ...) CHECK(fn(__VA_ARGS__, ARGS) == AMS_MEL_OK)

static ams_mel_session *session;
static ams_mel_ir_c2 *c2;
static ams_mel_ir_c2_metadata *metadata;
static const char *mode;
static char log_path[] = "/tmp/ams-c2-comms-r1-XXXXXX";

static unsigned count(const char *event)
{
    FILE *file = fopen(log_path, "r");
    char line[256]; unsigned n = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++n;
    CHECK(!fclose(file)); return n;
}

static void reenter(void)
{
    fprintf(stderr, "provider registration entered; reentering C2 lifecycle\n");
    CHECK(count("c2_comms_r1_registration\n") == 1);
    /* No test pin may hide premature unload. The family claim must retain
     * provider code even after reentrant typed and Session Close. */
    if (strstr(mode, "close")) {
        CALL(ams_mel_ir_c2_close, &c2);
        CHECK(!c2);
        CALL(ams_mel_session_close, &session);
        CHECK(count("channel_detached\n") == 0);
        CHECK(count("channel_detach_failed\n") == 0);
        CHECK(count("c2_channel_destroyed\n") == 0);
        CHECK(count("library_unloaded\n") == 0);
    } else {
        /* The flag must already be consumed before recursive registration. */
        CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, ARGS) == AMS_MEL_INVALID_ARGUMENT);
    }
}

int main(int argc, char **argv)
{
    ams_mel_ir_c2_config_v1 config = {0};
    ams_mel_ir_c2_metadata_event *event = NULL;
    const ams_mel_ir_c2_metadata_event_v1 *report = NULL;
    ams_mel_ir_c2_metadata_counters_v1 counters;
    ams_mel_status_t expected, status;
    char scenario[128], diagnostic[256];
    void *pin, *symbol;
    void (*configure)(void (*)(void));
    int fd;
    CHECK(argc == 2); mode = argv[1];
    fd = mkstemp(log_path); CHECK(fd >= 0 && !close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log_path, 1));
    CHECK(snprintf(scenario, sizeof scenario, "c2-comms-r1-%s", mode) > 0);
    config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session);
    CALL(ams_mel_ir_c2_open, session, &config, &c2);
    if (strstr(mode, "enabled")) CALL(ams_mel_ir_c2_enable, c2);
    CALL(ams_mel_ir_c2_metadata_open, c2, 4, &metadata);
    pin = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL); CHECK(pin);
    symbol = dlsym(pin, "mock_comms_reentry"); CHECK(symbol && sizeof symbol == sizeof configure);
    memcpy(&configure, &symbol, sizeof configure); configure(reenter);
    CHECK(!dlclose(pin));
    expected = strstr(mode, "throw") ? AMS_MEL_PROVIDER_EXCEPTION :
        (strstr(mode, "fail") || strstr(mode, "refuse") || strstr(mode, "detach") ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK);
    status = ams_mel_ir_c2_metadata_register_comms_test(metadata, diagnostic, sizeof diagnostic, NULL);
    CHECK(status == expected);
    if (strstr(mode, "unknown-throw")) CHECK(!strcmp(diagnostic, "unknown metadata registration exception"));
    else if (strstr(mode, "throw")) CHECK(!strcmp(diagnostic, "C2 comms registration exception"));
    else if (strstr(mode, "fail") || strstr(mode, "refuse"))
        CHECK(!strcmp(diagnostic, "ChannelCommsTest callback registration failed"));
    CHECK(count("c2_comms_r1_registration\n") == 1);
    CHECK(count("c2_comms_r1_registration_returning\n") == 1);
    /* Both synchronous reports must remain independent of any request/future. */
    for (unsigned i = 0; i < 2; ++i) {
        CALL(ams_mel_ir_c2_metadata_receive, metadata, 0, &event);
        CALL(ams_mel_ir_c2_metadata_event_view, event, &report);
        CHECK(report->kind == AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST);
        CHECK(report->channel_comms_test.command_id == (i ? UINT32_MAX : 0x80000001U));
        CHECK(report->channel_comms_test.request_id == (i ? 0x80000000U : UINT32_MAX));
        CALL(ams_mel_ir_c2_metadata_event_close, &event);
    }
    if (c2) {
        CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, ARGS) == AMS_MEL_INVALID_ARGUMENT);
        /* Closing the receiver cannot unregister the provider-retained callback. */
        CALL(ams_mel_ir_c2_metadata_close, &metadata);
        CALL(ams_mel_ir_c2_close, &c2);
        CALL(ams_mel_session_close, &session);
    } else {
        CALL(ams_mel_ir_c2_metadata_get_counters, metadata, &counters);
        CHECK(counters.events_received == (strstr(mode, "detach") ? 2U : 3U));
        CHECK(ams_mel_ir_c2_metadata_receive(metadata, 0, &event, ARGS) ==
              (strstr(mode, "detach") ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_STREAM_STOPPED));
        CALL(ams_mel_ir_c2_metadata_close, &metadata);
    }
    CALL(ams_mel_ir_c2_close, &c2);
    CHECK(count("comms_sent\n") == 0);
    if (strstr(mode, "detach")) {
        CHECK(count("channel_detach_failed\n") == 1);
        CHECK(count("channel_detached\n") == 0);
        CHECK(count("c2_channel_destroyed\n") == 0);
    } else {
        CHECK(count("channel_detached\n") == 1);
        CHECK(count("c2_comms_r1_late_callback\n") == 1);
        CHECK(count("c2_channel_destroyed\n") == 1);
    }
    CHECK(count("library_unloaded\n") == (strstr(mode, "detach") ? 0U : 1U));
    if (!strstr(mode, "detach")) {
        FILE *file = fopen(log_path, "r"); char line[256]; unsigned phase = 0;
        CHECK(file);
        while (fgets(line, sizeof line, file)) {
            if (!strcmp(line, "c2_comms_r1_registration_returning\n")) { CHECK(phase == 0); phase = 1; }
            if (!strcmp(line, "channel_detached\n")) { CHECK(phase == 1); phase = 2; }
            if (!strcmp(line, "c2_comms_r1_late_callback\n")) { CHECK(phase == 2); phase = 3; }
            if (!strcmp(line, "c2_channel_destroyed\n")) { CHECK(phase == 3); phase = 4; }
            if (!strcmp(line, "library_unloaded\n")) { CHECK(phase == 4); phase = 5; }
        }
        CHECK(!fclose(file)); CHECK(phase == 5);
    }
    CHECK(!unlink(log_path));
    return 0;
}
