#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ams_mel_test_c2_submission(ams_mel_ir_c2 *, unsigned, void **, size_t *);
extern int ams_mel_test_completion_snapshot(unsigned, uint64_t *);
extern int ams_mel_test_completion_owner(unsigned, uint64_t, uint64_t *);
typedef int (*barrier_fn)(unsigned, unsigned, unsigned);
typedef int (*gate_fn)(unsigned, uint64_t, uint64_t *, uint64_t *);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

typedef struct {
    unsigned family;
    ams_mel_ir_c2 *channel;
    ams_mel_ir_mode_request *mode;
    ams_mel_ir_return_request *ret;
    ams_mel_ir_channel_comms_request *comms;
    ams_mel_status_t status;
} Submission;

static void *submit(void *arg)
{
    Submission *s = arg;
    const ams_mel_ir_channel_comms_test_request_v1 command = {0};
    if (s->family == 0)
        s->status = ams_mel_ir_c2_submit_operate(s->channel, 1, &s->mode, NULL, 0, NULL);
    else if (s->family == 1)
        s->status = ams_mel_ir_c2_send_keepalive(s->channel, &s->ret, NULL, 0, NULL);
    else s->status = ams_mel_ir_c2_submit_comms_test(s->channel, &command, &s->comms, NULL, 0, NULL);
    return NULL;
}

static void finish(Submission *s)
{
    if (s->mode) {
        ams_mel_ir_mode_result_v1 result = {0};
        CHECK(ams_mel_ir_mode_request_wait(s->mode, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_mode_request_close(&s->mode, NULL, 0, NULL) == AMS_MEL_OK);
    }
    if (s->ret) {
        ams_mel_ir_return_result_v1 result = {0};
        CHECK(ams_mel_ir_return_request_wait(s->ret, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_close(&s->ret, NULL, 0, NULL) == AMS_MEL_OK);
    }
    if (s->comms) {
        ams_mel_ir_channel_comms_test_result_v1 result = {0};
        CHECK(ams_mel_ir_channel_comms_request_wait(s->comms, 15000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_comms_request_close(&s->comms, NULL, 0, NULL) == AMS_MEL_OK);
    }
}

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

static void run(unsigned family, int throws, int close_during, const char *failure, int observe)
{
    ams_mel_session *session = NULL;
    Submission s = {0};
    s.family = family;
    char path[] = "/tmp/ams-c2-submit-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "completion-scale", "",
                              &session, NULL, 0, NULL) == AMS_MEL_OK);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_NOLOAD);
    CHECK(library != NULL);
    barrier_fn barrier = NULL;
    gate_fn gate = NULL;
    *(void **)(&barrier) = dlsym(library, "mock_c2_send_barrier");
    *(void **)(&gate) = dlsym(library, "mock_completion_gate");
    CHECK(barrier && gate);
    ams_mel_ir_c2_config_v1 config = {0};
    config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    config.channel_id.descriptive_label = (ams_mel_string_view_v1){"test", 4};
    config.platform_id.descriptive_label = (ams_mel_string_view_v1){"test", 4};
    config.sensor_location.key = (ams_mel_string_view_v1){"key", 3};
    config.sensor_location.system_name = (ams_mel_string_view_v1){"test", 4};
    CHECK(ams_mel_ir_c2_open(session, &config, &s.channel, NULL, 0, NULL) == AMS_MEL_OK);
    /* Mode requires Enabled; inherited Return/Comms deliberately use Attached. */
    if (family == 0) CHECK(ams_mel_ir_c2_enable(s.channel, NULL, 0, NULL) == AMS_MEL_OK);
    void *token = NULL;
    size_t requests = 99;
    uint64_t before[6], after[6], owners, observed, sent, released;
    CHECK(ams_mel_test_c2_submission(s.channel, 0, &token, NULL));
    CHECK(ams_mel_test_completion_snapshot(family, before));
    CHECK(ams_mel_test_completion_owner(family, 0, &owners));
    CHECK(barrier(family, 0, (unsigned)throws));
    if (failure) CHECK(setenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE", failure, 1) == 0);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, submit, &s) == 0);
    CHECK(barrier(family, 1, 0));
    /* Direct try_lock evidence: the old implementation fails here, not by timeout. */
    CHECK(ams_mel_test_c2_submission(NULL, 1, &token, &requests) && requests == 1);
    if (close_during) {
        /* Separate variable: the submit thread's raw pointer is never read again
         * after the provider barrier; this is not a general wrapper race API. */
        ams_mel_ir_c2 *public_owner = s.channel;
        CHECK(ams_mel_ir_c2_close(&public_owner, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(public_owner == NULL);
        CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_test_c2_submission(NULL, 1, &token, &requests) && requests == 1);
    }
    CHECK(occurrences(path, "c2_channel_destroyed\n") == 0);
    CHECK(occurrences(path, "control_destroyed\n") == 0);
    CHECK(occurrences(path, "manager_destroyed\n") == 0);
    CHECK(occurrences(path, "library_unloaded\n") == 0);
    if (!observe) CHECK(ams_mel_test_c2_submission(NULL, 2, &token, NULL));
    CHECK(barrier(family, 2, 0));
    CHECK(pthread_join(thread, NULL) == 0);
    if (close_during) s.channel = NULL;
    CHECK(gate(0, 0, &sent, &released));
    CHECK(sent == (throws ? 0U : 1U) && released == 0);
    if (failure) {
        CHECK(s.status == AMS_MEL_INTERNAL_ERROR && !s.mode && !s.ret && !s.comms);
        CHECK(ams_mel_test_c2_submission(NULL, 1, &token, &requests) && requests == 1);
        CHECK(ams_mel_test_completion_snapshot(family, after));
        CHECK(after[0] == before[0] && after[4] == before[4]);
        CHECK(ams_mel_test_c2_submission(NULL, 2, &token, NULL));
        CHECK(dlclose(library) == 0);
        CHECK(occurrences(path, "c2_channel_destroyed\n") == 0);
        CHECK(occurrences(path, "control_destroyed\n") == 0);
        CHECK(occurrences(path, "library_unloaded\n") == 0);
        CHECK(unlink(path) == 0);
        return; /* isolated process: intentionally retained future is never consumed */
    }
    if (throws) {
        CHECK(s.status == AMS_MEL_PROVIDER_EXCEPTION && !s.mode && !s.ret && !s.comms);
        if (observe) CHECK(ams_mel_test_c2_submission(NULL, 1, &token, &requests) && requests == 0);
        CHECK(ams_mel_test_completion_owner(family, owners + 1, &observed));
        CHECK(ams_mel_test_completion_snapshot(family, after));
        CHECK(after[0] == before[0] && after[4] == before[4]);
        if (!close_during) {
            submit(&s);
            CHECK(s.status == AMS_MEL_OK);
            CHECK(gate(2, 1, NULL, NULL));
            finish(&s);
            CHECK(ams_mel_test_completion_owner(family, owners + 2, &observed));
        }
    } else {
        CHECK(s.status == AMS_MEL_OK);
        CHECK(gate(2, 1, NULL, NULL));
        finish(&s);
        CHECK(ams_mel_test_completion_owner(family, owners + 1, &observed));
    }
    if (observe) CHECK(ams_mel_test_c2_submission(NULL, 1, &token, &requests) && requests == 0);
    CHECK(ams_mel_test_completion_snapshot(family, after));
    unsigned completed = throws && close_during ? 0U : 1U;
    CHECK(after[1] == 0 && after[4] == before[4] + completed && after[5] == before[5] + completed);
    if (s.channel) CHECK(ams_mel_ir_c2_close(&s.channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(occurrences(path, "c2_channel_destroyed\n") == 1);
    if (observe) CHECK(ams_mel_test_c2_submission(NULL, 2, &token, NULL));
    CHECK(dlclose(library) == 0);
    const char *events[] = {"c2_channel_destroyed\n", "control_destroyed\n",
                            "manager_destroyed\n", "library_unloaded\n"};
    for (unsigned i = 0; i < 4; ++i) CHECK(occurrences(path, events[i]) == 1);
    FILE *log = fopen(path, "r");
    CHECK(log != NULL);
    char line[192];
    unsigned next = 0;
    while (fgets(line, sizeof line, log)) {
        for (unsigned i = 0; i < 4; ++i)
            if (!strcmp(line, events[i])) { CHECK(i == next); ++next; }
    }
    CHECK(next == 4 && fclose(log) == 0);
    CHECK(unlink(path) == 0);
    printf("family=%u throw=%d close-during=%d: unlocked, accounting and teardown passed\n",
           family, throws, close_during);
}

int main(int argc, char **argv)
{
    if (argc == 3) run((unsigned)strtoul(argv[1], NULL, 10), 0, 1, argv[2], 1);
    else for (unsigned family = 0; family < 3; ++family) {
        run(family, 0, 1, NULL, 1);
        run(family, 0, 1, NULL, 0);
        run(family, 1, 1, NULL, 1);
        run(family, 1, 1, NULL, 0);
        run(family, 1, 0, NULL, 1);
    }
    return 0;
}
