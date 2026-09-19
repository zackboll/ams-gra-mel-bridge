#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return EXIT_FAILURE; } } while (0)

static ams_mel_string_view_v1 text(const char *value)
{ ams_mel_string_view_v1 result = {value, strlen(value)}; return result; }

static int text_is(ams_mel_string_view_v1 value, const char *expected)
{
    return value.size == strlen(expected) &&
           (value.size == 0U || memcmp(value.data, expected, value.size) == 0);
}

static ams_mel_ir_track_config_v1 configuration(void)
{
    ams_mel_ir_track_config_v1 value;
    size_t index;
    memset(&value, 0, sizeof value);
    value.channel_id.descriptive_label = text("IR track channel");
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    value.platform_id.descriptive_label = text("test platform");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    value.sensor_location.key = text("station-1");
    value.sensor_location.system_name = text("mock-aircraft");
    for (index = 0; index < 16U; ++index) {
        value.channel_id.uuid[index] = (uint8_t)index;
        value.platform_id.uuid[index] = (uint8_t)(0xf0U + index);
    }
    return value;
}

static int open_track(const char *scenario, ams_mel_session **session,
                      ams_mel_ir_track **track)
{
    ams_mel_ir_track_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(*session, &config, track, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int read_log(const char *path, char *buffer, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t size;
    CHECK(file != NULL);
    size = fread(buffer, 1, capacity - 1U, file);
    CHECK(!ferror(file));
    CHECK(fclose(file) == 0);
    buffer[size] = '\0';
    return EXIT_SUCCESS;
}

/* Task 029B1 implements no Track metadata or Track command surface; the mock
 * records every deferred Track operation, so their absence is provable. */
static int no_deferred_track_operations(const char *data)
{
    CHECK(strstr(data, "track_deferred_operation_invoked") == NULL);
    CHECK(strstr(data, "track_report_registered") == NULL);
    CHECK(strstr(data, "track_candidate_object_registered") == NULL);
    CHECK(strstr(data, "track_candidate_object_preproc_registered") == NULL);
    CHECK(strstr(data, "track_request_system_track_data_registered") == NULL);
    CHECK(strstr(data, "track_data_update_sent") == NULL);
    CHECK(strstr(data, "track_system_track_data_response_sent") == NULL);
    return EXIT_SUCCESS;
}

/* Open success, exact IRSTTrack Config propagation, capabilities while both
 * attached and enabled, Enable idempotence, and Close after Enable. */
static int test_open_capability_enable(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_channel_capability *owner = NULL;
    const ams_mel_ir_channel_capability_v1 *view = NULL;
    /* "track-config" makes the mock assert the complete converted Config. */
    CHECK(open_track("track-config", &session, &track) == EXIT_SUCCESS);

    CHECK(ams_mel_ir_track_get_capabilities(track, &owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->channel_types.size == 1U);
    CHECK(view->channel_types.data[0] == AMS_MEL_IR_CHANNEL_IRST_TRACK);
    CHECK(view->metadata_capabilities.size == 2U);
    CHECK(view->metadata_capabilities.data[0] == AMS_MEL_IR_METADATA_IRST_TRACK_REPORT);
    CHECK(text_is(view->channel_id.descriptive_label, "channel-\xCE\xB1"));
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    /* Already enabled is a successful no-op. */
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);

    owner = NULL; view = NULL;
    CHECK(ams_mel_ir_track_get_capabilities(track, &owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->channel_types.data[0] == AMS_MEL_IR_CHANNEL_IRST_TRACK);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Session parent-first close: the Track owner keeps the provider/session graph
 * alive and remains usable until it releases it. */
static int test_parent_first_close(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_channel_capability *owner = NULL;
    const ams_mel_ir_channel_capability_v1 *view = NULL;
    CHECK(open_track("track-parent-first", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    /* Still a valid child operation after the parent handle is gone. */
    CHECK(ams_mel_ir_track_get_capabilities(track, &owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->channel_types.data[0] == AMS_MEL_IR_CHANNEL_IRST_TRACK);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Invalid arguments, wrong ChannelType, and every Open rejection path. */
static int test_open_failures(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_track_config_v1 config = configuration();

    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-config", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    /* Wrong ChannelType is rejected without touching the provider. */
    config.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(track == NULL);
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    CHECK(ams_mel_ir_track_open(session, NULL, &track, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_open(NULL, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_open(session, &config, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    track = (ams_mel_ir_track *)(void *)1;
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    track = NULL;
    CHECK(ams_mel_ir_track_enable(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_get_capabilities(NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_track_close(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    /* An already null owner closes successfully. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* attachChannel returns null. */
    config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-attach-null", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_FACTORY_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* Wrong concrete channel type; rollback detach succeeds. */
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-wrong-concrete", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INITIALIZATION_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* Capability omits IRSTTrack; rollback detach succeeds. */
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-capability-wrong", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INITIALIZATION_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* getCapabilities throws; safe rollback with a successful detach. */
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-capability-throw", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_INITIALIZATION_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* Open-time detach failure: the graph is retained by the allocation-free
     * emergency root and no public owner is ever produced. */
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "track-open-detach-fail", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_open(session, &config, &track, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Provider enable returning Fail, and a throwing provider enable. */
static int test_enable_failures(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    CHECK(open_track("track-enable-fail", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    /* Failed is terminal for Enable and invalidates capability queries. */
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    {
        ams_mel_ir_channel_capability *owner = NULL;
        CHECK(ams_mel_ir_track_get_capabilities(track, &owner, NULL, 0, NULL) ==
              AMS_MEL_PROVIDER_FAILED);
        CHECK(owner == NULL);
    }
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(open_track("track-enable-throw", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* Close without Enable must not call provider disable. */
static int run_close_without_enable_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-no-enable", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_close_without_enable(void)
{
    char path[] = "/tmp/ams-track-no-enable-XXXXXX";
    char data[4096];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_close_without_enable_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    CHECK(strstr(data, "track_enabled") == NULL);
    CHECK(strstr(data, "track_disabled") == NULL);
    CHECK(strstr(data, "channel_detached") != NULL);
    CHECK(no_deferred_track_operations(data) == EXIT_SUCCESS);
    /* Provider TrackChannel destruction precedes provider library unload. */
    {
        const char *destroyed = strstr(data, "track_channel_destroyed");
        const char *unloaded = strstr(data, "library_unloaded");
        CHECK(destroyed != NULL && unloaded != NULL && destroyed < unloaded);
    }
    unlink(path);
    return EXIT_SUCCESS;
}

/* Close after Enable disables, detaches, and destroys the provider channel
 * before the provider library is unloaded. */
static int run_close_after_enable_child(const char *log)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1) == 0);
    CHECK(open_track("track-lifetime", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int ordered(const char *data, const char *first, const char *second)
{
    const char *a = strstr(data, first);
    const char *b = strstr(data, second);
    CHECK(a != NULL && b != NULL && a < b);
    return EXIT_SUCCESS;
}

static int test_close_after_enable(void)
{
    char path[] = "/tmp/ams-track-lifetime-XXXXXX";
    char data[4096];
    int fd = mkstemp(path);
    pid_t child;
    int status = 0;
    CHECK(fd >= 0);
    close(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) _exit(run_close_after_enable_child(path));
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
          WEXITSTATUS(status) == 0);
    CHECK(read_log(path, data, sizeof data) == EXIT_SUCCESS);
    CHECK(ordered(data, "track_enabled", "track_disabled") == EXIT_SUCCESS);
    CHECK(ordered(data, "track_disabled", "channel_detached") == EXIT_SUCCESS);
    CHECK(ordered(data, "channel_detached", "track_channel_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "track_channel_destroyed", "control_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(ordered(data, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(no_deferred_track_operations(data) == EXIT_SUCCESS);
    unlink(path);
    return EXIT_SUCCESS;
}

/* A failed disable does not prove ownership safety: detach is still attempted,
 * and a successful detach clears the caller owner while reporting the failure. */
static int test_disable_failure(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    CHECK(open_track("track-disable-fail", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(track == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

/* A failed detach leaves the caller owner non-null, retains the complete graph,
 * and permits a later Close to retry detach successfully. */
static int test_detach_failure_retry(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_channel_capability *owner = NULL;
    CHECK(open_track("track-detach-fail", &session, &track) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_track_enable(track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(track != NULL);
    /* The retained owner is logically closed; capability queries are refused. */
    CHECK(ams_mel_ir_track_get_capabilities(track, &owner, NULL, 0, NULL) ==
          AMS_MEL_PROVIDER_FAILED);
    CHECK(owner == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* The second Close retries detach, which now succeeds. */
    CHECK(ams_mel_ir_track_close(&track, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(track == NULL);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_open_capability_enable() == EXIT_SUCCESS);
    CHECK(test_parent_first_close() == EXIT_SUCCESS);
    /* The provider-unload ordering tests fork, so they must run before any
     * test that permanently retains a provider graph: emergency retention
     * deliberately keeps the provider library loaded in this process, and a
     * forked child would then inherit an already-loaded library. */
    CHECK(test_close_without_enable() == EXIT_SUCCESS);
    CHECK(test_close_after_enable() == EXIT_SUCCESS);
    CHECK(test_enable_failures() == EXIT_SUCCESS);
    CHECK(test_disable_failure() == EXIT_SUCCESS);
    CHECK(test_detach_failure_retry() == EXIT_SUCCESS);
    CHECK(test_open_failures() == EXIT_SUCCESS);
    puts("PASS: native IR Track channel foundation contract");
    return EXIT_SUCCESS;
}
