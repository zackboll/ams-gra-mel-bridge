#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <stddef.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
_Static_assert(offsetof(ams_mel_ir_image_control_config_v1, channel_type) == 0,
               "C11 control config prefix");
_Static_assert(sizeof(ams_mel_ir_image_control_config_v1) ==
               offsetof(ams_mel_ir_stream_config_v1, buffer_count), "identity-only config");

static unsigned count(const char *path, const char *event)
{
    FILE *file = fopen(path, "r");
    char line[256]; unsigned n = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (!strcmp(line, event)) ++n;
    CHECK(!fclose(file)); return n;
}

static void exercise(const char *scenario)
{
    char log[] = "/tmp/ams-image-control-XXXXXX", diagnostic[256];
    int fd = mkstemp(log);
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_channel *channel = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    const ams_mel_ir_channel_capability_v1 *view = NULL;
    ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_navigation_request *nav = NULL;
    ams_mel_ir_return_request *keep = NULL;
    ams_mel_ir_channel_comms_request *comms = NULL;
    ams_mel_ir_image_control_config_v1 config = {0};
    ams_mel_navigation_report_v1 report = {0};
    ams_mel_ir_navigation_result_v1 result;
    ams_mel_ir_return_result_v1 kept;
    ams_mel_ir_channel_comms_test_request_v1 test = {0x80000001U, 0xf0000002U, 0xe0000003U};
    ams_mel_ir_channel_comms_test_result_v1 tested;
    ams_mel_ir_frame_v1 frame = {0};
    ams_mel_ir_frame_snapshot *snapshot = NULL;
    int uncertain = strstr(scenario, "release-") != NULL;
    CHECK(fd >= 0); CHECK(!close(fd));
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1));
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session,
                              NULL, 0, NULL) == AMS_MEL_OK);
    if (!strcmp(scenario, "image-control-zero")) {
        ams_mel_ir_stream_config_v1 acquisition = {0};
        acquisition.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        acquisition.buffer_count = 2; acquisition.buffer_size = 64; acquisition.queue_capacity = 2;
        CHECK(ams_mel_ir_stream_open(session, &acquisition, &stream, NULL, 0, NULL) ==
              AMS_MEL_INITIALIZATION_FAILED && !stream);
    }
    CHECK(ams_mel_ir_image_control_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_get_capabilities(stream, &capability, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(capability, &view, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(view->height == 0 && view->width == 0 && view->bit_depth == 0 &&
          view->row_pitch == 0 && view->buffer_size == 0 && view->image_size == 0 &&
          view->number_of_bands == 0 && view->pixel_format == 0);
    CHECK(view->channel_id.uuid[0] == 0x11 && view->platform_id.uuid[0] == 0x31);
    CHECK(view->sensor_location.offset_x_m == 1.25 && view->sensor_location.offset_y_m == -2.5 &&
          view->sensor_location.offset_z_m == 3.75);
    CHECK(view->sensor_types.size == 2 && view->channel_types.size == 1 &&
          view->channel_types.data[0] == AMS_MEL_IR_CHANNEL_IRST_IMAGE &&
          view->task_schedule_depth == 17 && view->odc_available == 1 && view->nuc_available == 1 &&
          view->metadata_capabilities.size == 4 && view->image_bands.size == 2 && view->nav_frames.size == 2);
    CHECK(ams_mel_ir_image_metadata_open(stream, 8, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < 4; ++i) {
        ams_mel_ir_image_metadata_event *event = NULL;
        const ams_mel_ir_image_metadata_event_v1 *value = NULL;
        CHECK(ams_mel_ir_image_metadata_receive(metadata, 0, &event, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_image_metadata_event_view(event, &value, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(value->kind == i + 1U);
        CHECK(ams_mel_ir_image_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_channel_from_stream(stream, &channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_send_keepalive(channel, &keep, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(keep, 1000, &kept, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&keep, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_submit_comms_test(channel, &test, &comms, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_request_wait(comms, 1000, &tested, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(tested.command_id == test.command_id && tested.request_id == test.request_id);
    CHECK(ams_mel_ir_channel_comms_request_close(&comms, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &nav, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(nav, 1000, &result, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_STATE && !strcmp(diagnostic, "no navigation hardware"));
    CHECK(ams_mel_ir_navigation_request_close(&nav, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == (uncertain ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
    CHECK(ams_mel_ir_stream_receive(stream, 0, &frame, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_INITIALIZATION_FAILED);
    CHECK(strstr(diagnostic, "control-only"));
    CHECK(ams_mel_ir_stream_receive_snapshot(stream, 0, &snapshot, diagnostic, sizeof diagnostic, NULL) == AMS_MEL_INITIALIZATION_FAILED);
    CHECK(snapshot == NULL);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    if (!strcmp(scenario, "image-control-detach-fail")) {
        CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(stream && count(log, "channel_destroyed\n") == 0 && count(log, "library_unloaded\n") == 0);
    }
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) ==
          (uncertain || !strcmp(scenario, "image-control-detach-fail") ? AMS_MEL_PROVIDER_FAILED : AMS_MEL_OK));
    CHECK(ams_mel_ir_channel_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(log, "buffer_factory_called\n") == 0 && count(log, "buffer_initialized\n") == 0 &&
          count(log, "buffer_registered\n") == 0 && count(log, "buffer_unregistered\n") == 0);
    CHECK(count(log, "bad_pixel_registration_attempted\n") == 1 &&
          count(log, "line_of_sight_report_registration_attempted\n") == 1 &&
          count(log, "line_of_sight_euler_registration_attempted\n") == 1 &&
          count(log, "navigation_response_registration_attempted\n") == 1);
    if (strstr(scenario, "unexpected") || uncertain) {
        CHECK(count(log, "control_buffer_released\n") == 2);
        CHECK(count(log, "control_reentry_returned\n") == 1);
    }
    if (!strcmp(scenario, "image-control-attach-reentry")) {
        CHECK(count(log, "control_buffer_released\n") == 1);
        CHECK(count(log, "control_attach_callback_returned\n") == 1);
    }
    if (uncertain) CHECK(count(log, "channel_destroyed\n") == 0 && count(log, "control_buffer_destroyed\n") == 1);
    else CHECK(count(log, "channel_destroyed\n") ==
               (!strcmp(scenario, "image-control-zero") ? 2U : 1U) &&
               count(log, "control_shutdown_callbacks_returned\n") == 1);
    /* Independent capability bytes survive provider-owner closure. */
    CHECK(view->image_bands.data[0].bands.size == 2 && view->nav_frames.size == 2);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    if (!uncertain) CHECK(count(log, "library_unloaded\n") == 1);
    else CHECK(count(log, "library_unloaded\n") == 0);
    CHECK(!unlink(log));
}

static void failed_open(const char *scenario, ams_mel_status_t expected)
{
    char log[] = "/tmp/ams-image-control-open-XXXXXX";
    int fd = mkstemp(log);
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_image_control_config_v1 config = {0};
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1));
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        char diagnostic[128];
        CHECK(ams_mel_ir_image_control_open(session, NULL, &stream, diagnostic,
              sizeof diagnostic, NULL) == AMS_MEL_INVALID_ARGUMENT && !stream);
        config.channel_type = AMS_MEL_IR_CHANNEL_STACKED_IMAGE;
        CHECK(ams_mel_ir_image_control_open(session, &config, &stream, diagnostic,
              sizeof diagnostic, NULL) == AMS_MEL_INVALID_ARGUMENT && !stream);
        config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    }
    CHECK(ams_mel_ir_image_control_open(session, &config, &stream, NULL, 0, NULL) == expected);
    CHECK(!stream);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    if (!strcmp(scenario, "image-control-open-detach-fail")) {
        CHECK(count(log, "health_channel_destroyed\n") == 0);
        CHECK(count(log, "control_destroyed\n") == 0 && count(log, "library_unloaded\n") == 0);
    }
    if (!strcmp(scenario, "image-control-attach-reentry-throw")) {
        CHECK(count(log, "control_attach_callback_returned\n") == 1 &&
              count(log, "control_buffer_released\n") == 1);
        CHECK(count(log, "control_destroyed\n") == 0 && count(log, "library_unloaded\n") == 0);
    }
    CHECK(!unlink(log));
}

static void pending(void)
{
    char log[] = "/tmp/ams-image-control-pending-XXXXXX";
    int fd = mkstemp(log);
    void *library = dlopen(AMS_MEL_TEST_MOCK_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int (*release)(unsigned, unsigned);
    void *symbol = dlsym(library, "mock_completion_release_family");
    ams_mel_session *session = NULL; ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_navigation_request *nav = NULL;
    ams_mel_ir_image_metadata *metadata = NULL;
    ams_mel_ir_image_control_config_v1 config = {0};
    ams_mel_navigation_report_v1 report = {0};
    ams_mel_ir_navigation_result_v1 result;
    CHECK(fd >= 0 && !close(fd) && library && symbol);
    CHECK(sizeof release == sizeof symbol); memcpy(&release, &symbol, sizeof release);
    CHECK(!setenv("AMS_MEL_TEST_LIFETIME_LOG", log, 1));
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "image-control-pending", "", &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_control_open(session, &config, &stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_open(stream, 8, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_submit_navigation_report(stream, &report, &nav, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_wait(nav, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK && !stream);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_image_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(log, "channel_detached\n") == 0 && count(log, "channel_destroyed\n") == 0);
    CHECK(release(3, 1));
    CHECK(ams_mel_ir_navigation_request_wait(nav, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_navigation_request_close(&nav, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(count(log, "channel_detached\n") == 1 && count(log, "channel_destroyed\n") == 1);
    CHECK(!dlclose(library)); CHECK(!unlink(log));
}

int main(void)
{
    const char *scenarios[] = {"image-control-zero", "image-control-unexpected", "image-control-detach-fail", "image-control-attach-reentry",
                              "image-control-release-fail", "image-control-release-throw"};
    for (size_t i = 0; i < sizeof scenarios / sizeof scenarios[0]; ++i) {
        pid_t child = fork(); int status;
        CHECK(child >= 0);
        if (!child) { exercise(scenarios[i]); _exit(0); }
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    {
        pid_t child = fork(); int status;
        CHECK(child >= 0);
        if (!child) { pending(); _exit(0); }
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    const char *failures[] = {"attach-null", "attach-throw", "image-control-wrong-type", "image-control-open-detach-fail", "image-control-attach-reentry-throw"};
    const ams_mel_status_t results[] = {AMS_MEL_FACTORY_FAILED, AMS_MEL_PROVIDER_EXCEPTION,
                                      AMS_MEL_INITIALIZATION_FAILED, AMS_MEL_PROVIDER_FAILED,
                                      AMS_MEL_PROVIDER_EXCEPTION};
    for (size_t i = 0; i < sizeof failures / sizeof failures[0]; ++i) {
        pid_t child = fork(); int status;
        CHECK(child >= 0);
        if (!child) { failed_open(failures[i], results[i]); _exit(0); }
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    puts("PASS: C11 control-only Image contract"); return 0;
}
