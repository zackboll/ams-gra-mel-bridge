#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_wait(unsigned, unsigned, uint64_t);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned occurrences(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[192];
    unsigned count = 0;
    CHECK(file != NULL);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++count;
    CHECK(fclose(file) == 0);
    return count;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    unsigned family = (unsigned)strtoul(argv[1], NULL, 10);
    CHECK(family == 1U || family == 2U);
    char path[] = "/tmp/ams-c2-finish-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_C2_FINISH_FAILURE", "exception", 1) == 0);
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *channel = NULL;
    ams_mel_ir_return_request *ret = NULL;
    ams_mel_ir_channel_comms_request *comms = NULL;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
                               &session, NULL, 0, NULL) == AMS_MEL_OK);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    gate_fn gate = NULL;
    *(void **)(&gate) = dlsym(library, "mock_completion_gate");
    CHECK(gate != NULL);
    ams_mel_ir_c2_config_v1 config = {0};
    config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    config.channel_id.descriptive_label = (ams_mel_string_view_v1){"test", 4};
    config.platform_id.descriptive_label = (ams_mel_string_view_v1){"test", 4};
    config.sensor_location.key = (ams_mel_string_view_v1){"key", 3};
    config.sensor_location.system_name = (ams_mel_string_view_v1){"test", 4};
    CHECK(ams_mel_ir_c2_open(session, &config, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    if (family == 1U) {
        CHECK(ams_mel_ir_c2_send_keepalive(channel, &ret, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ret != NULL);
    } else {
        const ams_mel_ir_channel_comms_test_request_v1 command = {0};
        CHECK(ams_mel_ir_c2_submit_comms_test(channel, &command, &comms, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(comms != NULL);
    }
    uint64_t after[6], sent = 0, released = 0;
    CHECK(gate(0, 0, &sent, &released) && sent == 1U && released == 0U);
    CHECK(ams_mel_ir_c2_close(&channel, NULL, 0, NULL) == AMS_MEL_OK && channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK && session == NULL);
    CHECK(dlclose(library) == 0);
    CHECK(gate(2, 1, &sent, &released) && sent == 1U && released == 1U);
    ams_mel_status_t status;
    char diagnostic[128];
    if (family == 1U) {
        ams_mel_ir_return_result_v1 result = {0};
        status = ams_mel_ir_return_request_wait(ret, 15000, &result,
                                                diagnostic, sizeof diagnostic, NULL);
    } else {
        ams_mel_ir_channel_comms_test_result_v1 result = {0};
        status = ams_mel_ir_channel_comms_request_wait(comms, 15000, &result,
                                                       diagnostic, sizeof diagnostic, NULL);
    }
    CHECK(status == AMS_MEL_PROVIDER_FAILED);
    CHECK(strcmp(diagnostic, "deferred C2 cleanup failed") == 0);
    CHECK(ams_mel_test_completion_wait(family, 3, 1U));
    CHECK(ams_mel_test_completion_snapshot(family, after));
    CHECK(after[0] == 1U && after[1] == 0U && after[3] == 1U);
    CHECK(after[4] == 1U && after[5] == 1U);
    const char *get = family == 1U ? "get_returned_1\n" : "get_returned_2\n";
    CHECK(occurrences(path, get) == 1U);
    if (ret) CHECK(ams_mel_ir_return_request_close(&ret, NULL, 0, NULL) == AMS_MEL_OK);
    if (comms) CHECK(ams_mel_ir_channel_comms_request_close(&comms, NULL, 0, NULL) == AMS_MEL_OK);
    const char *destroyed[] = {"c2_channel_destroyed\n", "channel_destroyed\n",
        "control_destroyed\n", "manager_destroyed\n", "library_unloaded\n"};
    for (unsigned i = 0; i < sizeof destroyed / sizeof destroyed[0]; ++i)
        CHECK(occurrences(path, destroyed[i]) == 0U);
    CHECK(unlink(path) == 0);
    printf("family=%u: one get, failed closed, provider graph retained\n", family);
    return 0; /* The process boundary, not a rescue, releases the retention root. */
}
