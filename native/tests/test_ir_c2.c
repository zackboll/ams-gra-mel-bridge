#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef AMS_MEL_TEST_MOCK_PROVIDER
#error "mock provider path is required"
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

static ams_mel_string_view_v1 view(const char *text)
{
    ams_mel_string_view_v1 result = {text, strlen(text)};
    return result;
}

static ams_mel_ir_c2_config_v1 configuration(void)
{
    ams_mel_ir_c2_config_v1 value;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
    for (size_t i = 0; i < 16; ++i) {
        value.channel_id.uuid[i] = (uint8_t)i;
        value.platform_id.uuid[i] = (uint8_t)(UINT8_C(0xf0) + i);
    }
    value.channel_id.descriptive_label = view("IR C2 channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    return value;
}

static ams_mel_ir_stream_config_v1 image_configuration(void)
{
    ams_mel_ir_stream_config_v1 value;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    value.channel_id.descriptive_label = view("IR image channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    value.buffer_count = 2;
    value.buffer_size = 64;
    value.queue_capacity = 2;
    return value;
}

static int open_c2(const char *scenario, ams_mel_session **session,
                   ams_mel_ir_c2 **c2)
{
    ams_mel_ir_c2_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_open(*session, &config, c2, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_all(ams_mel_session **session, ams_mel_ir_c2 **c2)
{
    CHECK(ams_mel_ir_c2_close(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*c2 == NULL);
    CHECK(ams_mel_ir_c2_close(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int read_log(const char *path, char *buffer, size_t capacity);
static int check_order(const char *text, const char *first, const char *second);

static int text_is(ams_mel_string_view_v1 value, const char *expected)
{
    return value.size == strlen(expected) &&
           (value.size == 0U || memcmp(value.data, expected, value.size) == 0);
}

static int receive_metadata(ams_mel_ir_c2_metadata *metadata,
                            ams_mel_ir_c2_metadata_event **event,
                            const ams_mel_ir_c2_metadata_event_v1 **view_out)
{
    CHECK(ams_mel_ir_c2_metadata_receive(metadata, 1000, event,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_event_view(*event, view_out,
          NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_common_pre_enable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *keepalive = NULL;
    ams_mel_ir_channel_comms_request *comms = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    ams_mel_ir_c2_metadata_event *event = NULL;
    const ams_mel_ir_c2_metadata_event_v1 *event_view = NULL;
    ams_mel_ir_return_result_v1 returned = {99, 99};
    ams_mel_ir_channel_comms_test_result_v1 result = {0, 0, 99};
    const ams_mel_ir_channel_comms_test_request_v1 input = {
        UINT32_C(0x80000001), UINT32_C(0xf0000002), UINT32_C(0xe0000003)};
    CHECK(open_c2("comms-high", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_send_keepalive(c2, &keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(keepalive, 1000, &returned, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(returned.value == AMS_MEL_IR_RETURN_SUCCESS && returned.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_return_request_wait(keepalive, 0, &returned, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &comms, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_metadata(metadata, &event, &event_view) == EXIT_SUCCESS);
    CHECK(event_view->kind == AMS_MEL_IR_C2_METADATA_CHANNEL_COMMS_TEST);
    CHECK(event_view->channel_comms_test.command_id == input.command_id);
    CHECK(event_view->channel_comms_test.request_id == input.request_id);
    CHECK(event_view->command_status.command_id == 0U);
    CHECK(ams_mel_ir_channel_comms_request_wait(comms, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.command_id == input.command_id && result.request_id == input.request_id);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_channel_comms_request_wait(comms, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_request_close(&comms, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_common_failures(void)
{
    static const struct { const char *scenario; ams_mel_status_t status; } keepalive[] = {
        {"keepalive-null", AMS_MEL_PROVIDER_FAILED},
        {"keepalive-future-throw", AMS_MEL_PROVIDER_EXCEPTION}};
    for (size_t i = 0; i < sizeof keepalive / sizeof keepalive[0]; ++i) {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL; ams_mel_ir_return_result_v1 result = {0, 0};
        CHECK(open_c2(keepalive[i].scenario, &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == keepalive[i].status);
        CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL; ams_mel_ir_return_result_v1 result = {0, 0};
        CHECK(open_c2("keepalive-fail", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(result.value == AMS_MEL_IR_RETURN_FAIL);
        CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL; ams_mel_ir_return_result_v1 result = {0, 0};
        char text[700]; size_t required = 0;
        CHECK(open_c2("keepalive-reject", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, text,
              sizeof text, &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INVALID_STATE && required == 614U);
        CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL;
        CHECK(open_c2("keepalive-send-throw", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(request == NULL); CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        static const struct { const char *scenario; ams_mel_status_t status; } cases[] = {
            {"comms-null", AMS_MEL_PROVIDER_FAILED},
            {"comms-future-throw", AMS_MEL_PROVIDER_EXCEPTION}};
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
            ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
            ams_mel_ir_channel_comms_request *request = NULL;
            ams_mel_ir_channel_comms_test_result_v1 result = {0, 0, 0};
            const ams_mel_ir_channel_comms_test_request_v1 input = {1, 2, 3};
            CHECK(open_c2(cases[i].scenario, &session, &c2) == EXIT_SUCCESS);
            CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result,
                  NULL, 0, NULL) == cases[i].status);
            CHECK(ams_mel_ir_channel_comms_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
            CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
        }
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_channel_comms_request *request = NULL;
        const ams_mel_ir_channel_comms_test_request_v1 input = {1, 2, 3};
        CHECK(open_c2("comms-send-throw", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(request == NULL); CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_channel_comms_request *request = NULL;
        ams_mel_ir_channel_comms_test_result_v1 result = {0, 0, 0};
        const ams_mel_ir_channel_comms_test_request_v1 input = {1, 2, 3};
        char text[700]; size_t required = 0;
        CHECK(open_c2("comms-reject", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result, text,
              sizeof text, &required) == AMS_MEL_COMMAND_REJECTED);
        CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS && required == 614U);
        CHECK(ams_mel_ir_channel_comms_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int run_comms_registration_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    CHECK(open_c2("comms-register-retain-fail", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_metadata_register_comms_test(metadata, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_comms_registration_failure(const char *program)
{
    char path[] = "/tmp/ams-mel-comms-registration-XXXXXX";
    char command[1024];
    char log[4096];
    int descriptor = mkstemp(path);
    int status;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    {
        int length = snprintf(command, sizeof command,
            "AMS_MEL_TEST_LIFETIME_LOG='%s' '%s' comms-registration-child",
            path, program);
        CHECK(length > 0 && (size_t)length < sizeof command);
    }
    status = system(command);
    CHECK(status == 0);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(check_order(log, "retained_comms_callback_invoked",
                      "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed",
                      "library_unloaded") == EXIT_SUCCESS);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_common_delayed_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_channel_comms_request *request = NULL;
    ams_mel_ir_channel_comms_test_result_v1 result = {0, 0, 0};
    const ams_mel_ir_channel_comms_test_request_v1 input = {17, 7, 19};
    CHECK(open_c2("comms-delayed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_comms_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.command_id == 17U && result.request_id == 19U);
    CHECK(ams_mel_ir_channel_comms_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_keepalive_delayed_lifetime(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL; ams_mel_ir_return_result_v1 result = {0, 0};
    CHECK(open_c2("keepalive-delayed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_common_pending_close(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *keepalive = NULL;
    CHECK(open_c2("keepalive-delayed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_send_keepalive(c2, &keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&keepalive, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    { const struct timespec delay = {0, 100000000L}; CHECK(nanosleep(&delay, NULL) == 0); }
    {
        ams_mel_ir_channel_comms_request *comms = NULL;
        const ams_mel_ir_channel_comms_test_request_v1 input = {1, 2, 3};
        CHECK(open_c2("comms-delayed", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &comms, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_channel_comms_request_close(&comms, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
        { const struct timespec delay = {0, 100000000L}; CHECK(nanosleep(&delay, NULL) == 0); }
    }
    return EXIT_SUCCESS;
}

static int test_common_post_send_failure(const char *scenario, const char *failpoint,
                                         int comms)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    CHECK(open_c2(scenario, &session, &c2) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE", failpoint, 1) == 0);
    if (comms) {
        ams_mel_ir_channel_comms_request *request = NULL;
        const ams_mel_ir_channel_comms_test_request_v1 input = {1, 2, 3};
        CHECK(ams_mel_ir_c2_submit_comms_test(c2, &input, &request, NULL, 0, NULL) == AMS_MEL_INTERNAL_ERROR);
        CHECK(request == NULL);
    } else {
        ams_mel_ir_return_request *request = NULL;
        CHECK(ams_mel_ir_c2_send_keepalive(c2, &request, NULL, 0, NULL) == AMS_MEL_INTERNAL_ERROR);
        CHECK(request == NULL);
    }
    CHECK(unsetenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE") == 0);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_capability_snapshot(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_channel_capability *owner = NULL;
    const ams_mel_ir_channel_capability_v1 *value = NULL;
    CHECK(open_c2("capability-rich", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_get_capabilities(c2, &owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(owner, &value, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(value->channel_id.uuid[0] == 0x11U && text_is(value->channel_id.descriptive_label, "channel-\xCE\xB1"));
    CHECK(value->height == 1080U && value->width == 1920U && value->bit_depth == 12U);
    CHECK(value->row_pitch == 4096U && value->buffer_size == 8388608U && value->image_size == 4147200U);
    CHECK(value->number_of_bands == 3U && value->pixel_format == AMS_MEL_IR_PIXEL_RGB);
    CHECK(value->sensor_types.size == 2U && value->sensor_types.data[0] == AMS_MEL_IR_SENSOR_GIMBAL_HORIZONTAL);
    CHECK(value->sensor_types.data[1] == AMS_MEL_IR_SENSOR_STEP_STARE);
    CHECK(value->platform_id.uuid[0] == 0x31U && text_is(value->platform_id.descriptive_label, "platform-\xE2\x82\xAC"));
    CHECK(value->sensor_location.offset_x_m == 1.25 && value->sensor_location.offset_y_m == -2.5);
    CHECK(value->sensor_location.offset_z_m == 3.75 && text_is(value->sensor_location.key, "sensor-key"));
    CHECK(text_is(value->sensor_location.system_name, "system-\xCE\xB2"));
    CHECK(value->channel_types.size == 3U && value->channel_types.data[0] == AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL);
    CHECK(value->channel_types.data[1] == AMS_MEL_IR_CHANNEL_INSTRUMENTATION && value->channel_types.data[2] == AMS_MEL_IR_CHANNEL_RESERVED_2);
    CHECK(value->task_schedule_depth == 17U && value->odc_available == 1U && value->nuc_available == 1U);
    CHECK(value->metadata_capabilities.size == 4U && value->metadata_capabilities.data[0] == 0U);
    CHECK(value->metadata_capabilities.data[1] == 9U && value->metadata_capabilities.data[2] == 17U && value->metadata_capabilities.data[3] == 31U);
    CHECK(value->image_bands.size == 2U && value->image_bands.data[0].band_index == 2U);
    CHECK(value->image_bands.data[0].bands.size == 2U && value->image_bands.data[0].bands.data[0].type == AMS_MEL_IR_BAND_IR_LONGWAVE);
    CHECK(value->image_bands.data[0].bands.data[1].min_wavelength_m == 3.0e-6);
    CHECK(value->image_bands.data[1].band_index == 9U && value->image_bands.data[1].bands.data[0].type == AMS_MEL_IR_BAND_VISIBLE_RED);
    CHECK(value->nav_frames.size == 2U && value->nav_frames.data[0] == AMS_MEL_IR_COORDINATE_NED_SENSOR && value->nav_frames.data[1] == AMS_MEL_IR_COORDINATE_ECEF);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    CHECK(value->height == 1080U && value->image_bands.data[1].band_index == 9U);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_malformed_capabilities(void)
{
    static const char *scenarios[] = {"capability-bad-pixel", "capability-bad-sensor",
        "capability-bad-channel", "capability-bad-metadata", "capability-bad-band",
        "capability-bad-nav", "capability-bad-utf8"};
    for (size_t i = 0; i < sizeof scenarios / sizeof scenarios[0]; ++i) {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_channel_capability *owner = NULL;
        CHECK(open_c2(scenarios[i], &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_get_capabilities(c2, &owner, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
        CHECK(owner == NULL); CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_channel_capability *owner = NULL;
        CHECK(open_c2("capability-throw", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_get_capabilities(c2, &owner, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(owner == NULL); CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int test_metadata_rich_and_lifetime(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    ams_mel_ir_c2_metadata_event *config_event = NULL;
    ams_mel_ir_c2_metadata_event *status_event = NULL;
    const ams_mel_ir_c2_metadata_event_v1 *event = NULL;
    CHECK(open_c2("metadata-rich", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 8, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_metadata(metadata, &config_event, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION);
    CHECK(event->command_status.command_id == 0U);
    CHECK(event->bit_configuration.bit_types.size == 2U);
    {
        const ams_mel_bit_type_v1 *first = &event->bit_configuration.bit_types.data[0];
        const ams_mel_bit_type_v1 *second = &event->bit_configuration.bit_types.data[1];
        CHECK(first->bit_id.uuid[0] == UINT8_C(0x80));
        CHECK(text_is(first->bit_id.descriptive_label, "BIT-\xCE\xB1"));
        CHECK(first->accepted_interface == AMS_MEL_BIT_CONTROL_SUBSYSTEM_BIT_COMMAND);
        CHECK(first->bit_item_names.size == 2U);
        CHECK(text_is(first->bit_item_names.data[1], "optical-\xE2\x82\xAC"));
        CHECK(first->subsystem_component_ids.size == 2U);
        CHECK(first->subsystem_component_ids.data[1].uuid[0] == UINT8_C(0xa0));
        CHECK(text_is(first->subsystem_component_ids.data[1].descriptive_label,
                      "component-\xCE\xB2"));
        CHECK(first->expected_duration_ns == INT64_C(123456789));
        CHECK(second->accepted_interface == AMS_MEL_BIT_CONTROL_SUBSYSTEM_INITIATED);
        CHECK(second->bit_item_names.size == 0U);
        CHECK(second->subsystem_component_ids.size == 0U);
        CHECK(second->expected_duration_ns == 0);
    }
    CHECK(ams_mel_ir_c2_metadata_event_close(&config_event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_metadata(metadata, &status_event, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_BIT_STATUS);
    CHECK(event->bit_status.active_bits.size == 3U);
    CHECK(event->bit_status.active_bits.data[0].estimated_completion_time_ns == -5);
    CHECK(event->bit_status.active_bits.data[0].estimated_percent_complete == 1.25);
    CHECK(event->bit_status.active_bits.data[1].estimated_completion_time_ns == 0);
    CHECK(event->bit_status.active_bits.data[2].estimated_completion_time_ns == 987654321);
    CHECK(event->bit_status.completed_bits.size == 2U);
    CHECK(event->bit_status.completed_bits.data[0].bit_items.size == 2U);
    CHECK(event->bit_status.completed_bits.data[0].bit_items.data[1].result ==
          AMS_MEL_BIT_RESULT_FAIL);
    CHECK(text_is(event->bit_status.completed_bits.data[1].fail_reason,
                  "failure-\xE2\x82\xAC"));
    CHECK(event->bit_status.faults.size == 1U);
    CHECK(event->bit_status.faults.data[0].severity == AMS_MEL_FAULT_SEVERITY_WARNING);
    CHECK(event->bit_status.faults.data[0].state == AMS_MEL_FAULT_STATE_SET);
    CHECK(event->bit_status.faults.data[0].fault_data.size == 2U);
    CHECK(text_is(event->bit_status.faults.data[0].fault_data.data[0].units,
                  "\xC2\xB0" "C"));
    CHECK(event->bit_status.faults.data[0].detection_time_ns == -1234567);
    CHECK(event->bit_status.faults.data[0].component_ids.size == 2U);
    CHECK(event->bit_status.faults.data[0].ambiguity_groups.size == 2U);
    CHECK(event->bit_status.faults.data[0].ambiguity_groups.data[0].diagnostic_test_ids.size == 2U);
    CHECK(event->bit_status.faults.data[0].ambiguity_groups.data[0].component_ids.size == 2U);
    CHECK(ams_mel_ir_c2_metadata_receive(metadata, 0, &config_event,
          NULL, 0, NULL) == AMS_MEL_STREAM_STOPPED);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_event_view(status_event, &event,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event->bit_status.faults.data[0].ambiguity_groups.size == 2U);
    CHECK(ams_mel_ir_c2_metadata_event_close(&status_event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_event_close(&status_event, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_metadata_overflow_and_malformed(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL; ams_mel_ir_c2_metadata_event *owner = NULL;
    const ams_mel_ir_c2_metadata_event_v1 *event = NULL;
    ams_mel_ir_c2_metadata_counters_v1 counters = {0, 0, 0};
    CHECK(open_c2("metadata-overflow", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 3U && counters.events_dropped_queue_full == 1U);
    CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION);
    CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_COMMAND_STATUS && event->command_status.command_id == 1U);
    CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_receive(metadata, 0, &owner, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 2, &metadata,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(metadata == NULL);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);

    CHECK(open_c2("metadata-malformed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_get_counters(metadata, &counters, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 6U && counters.malformed_or_unsupported == 4U);
    CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_COMMAND_STATUS && event->command_status.command_id == 5U);
    CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_BIT_STATUS);
    CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int run_metadata_registration_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    CHECK(open_c2("metadata-register-fail", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 2, &metadata, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(metadata == NULL);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_registration_failure(const char *program)
{
    char path[] = "/tmp/ams-mel-metadata-registration-XXXXXX";
    char command[1024];
    char log[4096];
    int descriptor = mkstemp(path);
    int status;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    {
        int length = snprintf(command, sizeof command,
            "AMS_MEL_TEST_LIFETIME_LOG='%s' '%s' metadata-registration-child",
            path, program);
        CHECK(length > 0 && (size_t)length < sizeof command);
    }
    status = system(command);
    CHECK(status == 0);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(check_order(log, "retained_metadata_callback_invoked",
                      "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed",
                      "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_metadata_close_before_c2(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL; ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {0, 0};
    CHECK(open_c2("metadata-command-status", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, UINT32_C(0x80000001), &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_callback_allocation_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    ams_mel_ir_c2_metadata_event *owner = NULL;
    const ams_mel_ir_c2_metadata_event_v1 *event = NULL;
    ams_mel_ir_c2_metadata_counters_v1 counters = {0, 0, 0};
    CHECK(setenv("AMS_MEL_TEST_METADATA_CALLBACK_FAILURE",
                 "command-allocation", 1) == 0);
    CHECK(open_c2("metadata-overflow", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(unsetenv("AMS_MEL_TEST_METADATA_CALLBACK_FAILURE") == 0);
    CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
    CHECK(event->kind == AMS_MEL_IR_C2_METADATA_BIT_CONFIGURATION);
    CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_receive(metadata, 0, &owner,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_metadata_get_counters(metadata, &counters,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 3U);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int run_metadata_nonquiescing_disable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    CHECK(open_c2("metadata-nonquiescing-disable", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 2, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_metadata_nonquiescing_disable(const char *program)
{
    char path[] = "/tmp/ams-mel-metadata-lifetime-XXXXXX";
    char entered[128];
    char release[128];
    char command[1024];
    char log[4096];
    int descriptor = mkstemp(path);
    int status;
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    {
        int entered_length = snprintf(entered, sizeof entered, "%s.entered", path);
        int release_length = snprintf(release, sizeof release, "%s.release", path);
        CHECK(entered_length > 0 && (size_t)entered_length < sizeof entered);
        CHECK(release_length > 0 && (size_t)release_length < sizeof release);
    }
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_METADATA_CALLBACK_BARRIER", path, 1) == 0);
    {
        int length = snprintf(command, sizeof command,
            "AMS_MEL_TEST_LIFETIME_LOG='%s' "
            "AMS_MEL_TEST_METADATA_CALLBACK_BARRIER='%s' "
            "'%s' metadata-nonquiescing-child", path, path, program);
        CHECK(length > 0 && (size_t)length < sizeof command);
    }
    status = system(command);
    CHECK(status == 0);
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(check_order(log, "metadata_callback_entered",
                      "disable_returned_with_metadata_callback_active") == EXIT_SUCCESS);
    CHECK(check_order(log, "disable_returned_with_metadata_callback_active",
                      "metadata_callback_returned") == EXIT_SUCCESS);
    CHECK(check_order(log, "metadata_callback_returned",
                      "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed",
                      "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_METADATA_CALLBACK_BARRIER") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(entered) == 0);
    CHECK(unlink(release) == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_metadata_command_states(void)
{
    static const uint32_t states[] = {
        AMS_MEL_IR_COMMAND_ACCEPTED, AMS_MEL_IR_COMMAND_REJECTED,
        AMS_MEL_IR_COMMAND_CANCELLED, AMS_MEL_IR_COMMAND_RECEIVED};
    ams_mel_session *session = NULL; ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_metadata *metadata = NULL;
    CHECK(open_c2("metadata-command-states", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_metadata_open(c2, 4, &metadata, NULL, 0, NULL) == AMS_MEL_OK);
    for (size_t index = 0; index < 4U; ++index) {
        ams_mel_ir_c2_metadata_event *owner = NULL;
        const ams_mel_ir_c2_metadata_event_v1 *event = NULL;
        CHECK(receive_metadata(metadata, &owner, &event) == EXIT_SUCCESS);
        CHECK(event->kind == AMS_MEL_IR_C2_METADATA_COMMAND_STATUS);
        CHECK(event->command_status.command_id == UINT32_C(0x80000001) + index);
        CHECK(event->command_status.state == states[index]);
        if (index == 1U) {
            CHECK(event->command_status.reason_id ==
                  AMS_MEL_IR_CANNOT_COMPLY_INVALID_INPUT_PARAMETER);
            CHECK(event->command_status.reason_description.size == 613U);
        }
        CHECK(ams_mel_ir_c2_metadata_event_close(&owner, NULL, 0, NULL) == AMS_MEL_OK);
    }
    CHECK(ams_mel_ir_c2_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
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

/* Bounded wait for a lifetime-log marker.
   ======================================

   PR #42 third-review corrective, kept logically separate from the
   provider-buffer ownership work: this is pre-existing C2 test infrastructure.

   test_post_send_failure() and test_bit_post_send_failure() used to sleep for
   a fixed 100 ms and then assert that "mode_completed"/"bit_completed" was
   already in the log. That is a race, not a synchronization: the retained
   worker thread completes the provider future asynchronously, and the mock's
   delayed producer itself waits up to 40 ms before recording completion. On a
   loaded machine -- exactly the CI condition of a 50x parallel ctest repeat --
   100 ms is simply not enough, so the assertion failed although the retained
   worker was progressing normally. Reproduced on the PR base SHA, so it is a
   pre-existing load-sensitive flake, not a regression from this PR.

   The assertion itself is deliberately NOT weakened: the retained worker must
   still genuinely reach the completion marker. Only the waiting strategy
   changes, from "sleep a fixed 100 ms and hope" to "poll the explicit
   completion condition under a generous bounded deadline". A genuinely
   non-completing worker still fails, just after the deadline instead of after
   100 ms. */
static int wait_for_marker(const char *path, const char *marker, char *buffer,
                           size_t capacity)
{
    /* 20 s at 1 ms granularity. Generous enough that only a real failure to
       complete can exhaust it, and bounded so a hang is still a test failure
       rather than an indefinite stall. */
    for (unsigned attempt = 0; attempt < 20000U; ++attempt) {
        CHECK(read_log(path, buffer, capacity) == EXIT_SUCCESS);
        if (strstr(buffer, marker) != NULL) return EXIT_SUCCESS;
        {
            const struct timespec delay = {0, 1000000L};
            CHECK(nanosleep(&delay, NULL) == 0);
        }
    }
    /* Deadline exhausted: report the same failure the fixed sleep would have,
       with the marker named, and keep the caller's CHECK(strstr(...)) as the
       authoritative assertion. */
    fprintf(stderr, "marker \"%s\" never appeared in %s within the deadline\n",
            marker, path);
    return EXIT_FAILURE;
}

static int check_order(const char *text, const char *first, const char *second)
{
    const char *a = strstr(text, first);
    const char *b = strstr(text, second);
    CHECK(a != NULL && b != NULL && a < b);
    return EXIT_SUCCESS;
}

static int test_success(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {99, 99};
    CHECK(open_c2("c2-command-id", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_submit_operate(c2, UINT32_C(0x89abcdef), &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, UINT32_C(0x89abcdef), &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.mode == AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    result.mode = 99;
    CHECK(ams_mel_ir_mode_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.mode == AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static ams_mel_ir_mode_command_v1 full_mode_command(void)
{
    ams_mel_ir_mode_command_v1 command;
    memset(&command, 0, sizeof command);
    command.command_id = UINT32_C(0x89abcdef);
    command.state = AMS_MEL_IR_MFA_STATE_OPERATE;
    command.mode = AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED;
    command.scan_parameters.elevation_defined_with_range_and_altitude = 1;
    command.scan_parameters.center_az_rad = 0.25;
    command.scan_parameters.center_el_rad = -0.5;
    command.scan_parameters.center_frame_ref_el = AMS_MEL_IR_COORD_FRAME_AIRCRAFT;
    command.scan_parameters.center_frame_ref_az = AMS_MEL_IR_COORD_FRAME_INERTIAL;
    command.scan_parameters.scan_width_rad = 1.25;
    command.scan_parameters.scan_height_rad = 0.75;
    command.scan_parameters.scan_type.continuous_scan = 1;
    command.scan_parameters.scan_type.returning = 2;
    command.scan_parameters.scan_type.agile_scan = 3;
    command.scan_parameters.scan_id = UINT32_C(0x89abcdef);
    command.scan_parameters.scan_rate_rad_per_second = -0.125;
    command.scan_parameters.preferred_revisit_interval_seconds = 2.5;
    command.scan_parameters.required_revisit_interval_seconds = 3.5;
    command.scan_parameters.max_range_of_interest_m = 123456;
    command.scan_parameters.min_range_of_interest_m = 42;
    command.scan_parameters.elevation_scan_center_altitude_m = 7000;
    command.scan_parameters.elevation_scan_center_range_m = 9000;
    command.scan_parameters.degradation_method = AMS_MEL_IR_DEGRADATION_REVISIT;
    return command;
}

static int test_general_mode(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result;
    ams_mel_ir_mode_command_v1 command = full_mode_command();
    CHECK(open_c2("mode-full", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.mode == AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    command.state = AMS_MEL_IR_MFA_STATE_MAX_EXCLUSIVE;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.state = UINT32_MAX;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.state = AMS_MEL_IR_MFA_STATE_OPERATE;
    command.mode = AMS_MEL_IR_MFA_MODE_SCAN_BAR_SCHED + 1;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.mode = AMS_MEL_IR_MFA_MODE_SCAN_VOLUME_SCHED;
    command.scan_parameters.elevation_defined_with_range_and_altitude = 2;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.scan_parameters.elevation_defined_with_range_and_altitude = 1;
    command.scan_parameters.center_frame_ref_el = AMS_MEL_IR_COORD_FRAME_AIRCRAFT + 1;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.scan_parameters.center_frame_ref_el = AMS_MEL_IR_COORD_FRAME_AIRCRAFT;
    command.scan_parameters.center_frame_ref_az = AMS_MEL_IR_COORD_FRAME_AIRCRAFT + 1;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.scan_parameters.center_frame_ref_az = AMS_MEL_IR_COORD_FRAME_INERTIAL;
    command.scan_parameters.degradation_method = AMS_MEL_IR_DEGRADATION_REVISIT + 1;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(request == NULL);
    command.scan_parameters.degradation_method = AMS_MEL_IR_DEGRADATION_REVISIT;
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_general_bit(void)
{
    static const uint32_t initiate[] = {1, UINT32_C(0x80000001), UINT32_MAX};
    static const uint32_t cancel[] = {7, 9};
    static const char euro_fault[] = "fault-\xE2\x82\xAC";
    const ams_mel_string_view_v1 faults[] = {view("fault-alpha"), view(euro_fault)};
    ams_mel_ir_bit_command_v1 command = {UINT32_C(0xfedcba98),
        {initiate, 3}, {cancel, 2}, {faults, 2}};
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    ams_mel_ir_return_result_v1 result;
    CHECK(open_c2("bit-full", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    command.initiate_bit_ids.data = NULL;
    CHECK(ams_mel_ir_c2_submit_bit(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    command.initiate_bit_ids.data = initiate;
    {
        const char invalid[] = {(char)0xc3, '(', '\0'};
        const ams_mel_string_view_v1 invalid_fault = {invalid, 2};
        command.clear_fault_codes.data = &invalid_fault;
        command.clear_fault_codes.size = 1;
        CHECK(ams_mel_ir_c2_submit_bit(c2, &command, &request, NULL, 0, NULL) ==
              AMS_MEL_INVALID_ARGUMENT);
    }
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    command = (ams_mel_ir_bit_command_v1){0};
    CHECK(open_c2("bit-empty", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_config_set(void)
{
    static const char config_text[] = "configuration-\xE2\x82\xAC";
    ams_mel_ir_config_set_command_v1 command = {
        UINT32_C(0xfedcba98), INT64_C(-1234567890123), {config_text, sizeof config_text - 1U}};
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    ams_mel_ir_return_result_v1 result;
    CHECK(open_c2("config-full", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_config_set(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    command.config.data = "bad\0string";
    command.config.size = 10;
    CHECK(ams_mel_ir_c2_submit_config_set(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    command.command_id = 0;
    command.system_time_ns = 0;
    command.config = view("");
    CHECK(open_c2("config-empty", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_config_set(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    command.command_id = UINT32_C(0x80000000);
    command.system_time_ns = INT64_C(9876543210);
    command.config = view("positive");
    CHECK(open_c2("config-positive", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_config_set(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_timeout_and_parent_close(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {0, 0};
    CHECK(open_c2("c2-delayed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 7, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(c2 == NULL);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.mode == AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_request_close_pending(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    char path[] = "/tmp/ams-mel-c2-lifetime-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_c2("c2-lifetime", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 8, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < 100U; ++i) {
        CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
        if (strstr(log, "library_unloaded") != NULL) break;
        {
            const struct timespec delay = {0, 1000000L};
            CHECK(nanosleep(&delay, NULL) == 0);
        }
    }
    CHECK(check_order(log, "mode_completed", "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed", "control_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "control_destroyed", "manager_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "manager_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_rejection(const char *scenario, const char *expected)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {0, 0};
    char diagnostic[128] = {0};
    size_t required = 0;
    CHECK(open_c2(scenario, &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS);
    CHECK(strcmp(diagnostic, expected) == 0);
    CHECK(required == strlen(expected) + 1U);
    memset(diagnostic, 0, sizeof diagnostic);
    CHECK(ams_mel_ir_mode_request_wait(request, 0, &result, diagnostic,
          sizeof diagnostic, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strcmp(diagnostic, expected) == 0);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_long_rejection(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {0, 0};
    char expected[614];
    char short_diagnostic[512];
    char *complete;
    size_t required = 0;
    memset(expected, 'x', 510U);
    expected[510] = (char)0xe2; expected[511] = (char)0x82;
    expected[512] = (char)0xac;
    memset(expected + 513, 'y', 100U);
    expected[613] = '\0';
    CHECK(open_c2("c2-reject-long", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, NULL, 0,
          &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(required == sizeof expected);
    memset(short_diagnostic, 0x7f, sizeof short_diagnostic);
    CHECK(ams_mel_ir_mode_request_wait(request, 0, &result, short_diagnostic,
          sizeof short_diagnostic, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strlen(short_diagnostic) == 510U);
    CHECK(memcmp(short_diagnostic, expected, 510U) == 0);
    complete = (char *)malloc(required);
    CHECK(complete != NULL);
    CHECK(ams_mel_ir_mode_request_wait(request, 0, &result, complete,
          required, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strcmp(complete, expected) == 0);
    free(complete);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    char path[] = "/tmp/ams-mel-c2-post-send-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_c2("c2-lifetime", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE", failpoint, 1) == 0);
    {
        char diagnostic[128] = {0};
        const ams_mel_status_t status = ams_mel_ir_c2_submit_operate
          (c2, 9, &request, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_INTERNAL_ERROR)
            fprintf(stderr, "post-send %s returned %d: %s\n", failpoint,
                    (int)status, diagnostic);
        CHECK(status == AMS_MEL_INTERNAL_ERROR);
    }
    CHECK(unsetenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE") == 0);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* Wait for the explicit completion condition instead of a fixed sleep.
       The retained worker must still genuinely reach mode_completed; only the
       waiting strategy is bounded-deadline polling rather than timing luck. */
    CHECK(wait_for_marker(path, "mode_completed", log, sizeof log) ==
          EXIT_SUCCESS);
    CHECK(strstr(log, "mode_sent") != NULL);
    CHECK(strstr(log, "mode_completed") != NULL);
    CHECK(strstr(log, "c2_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_terminal_failure(const char *scenario, ams_mel_status_t expected,
                                 const char *text)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_mode_result_v1 result = {0, 0};
    char diagnostic[128] = {0};
    CHECK(open_c2(scenario, &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &result, diagnostic,
          sizeof diagnostic, NULL) == expected);
    CHECK(strstr(diagnostic, text) != NULL);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_open_failure(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == expected);
    CHECK(c2 == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_enable_send_cleanup_failures(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    CHECK(open_c2("c2-enable-fail", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(open_c2("c2-send-throw", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(request == NULL);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);

    CHECK(open_c2("c2-disable-fail", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(c2 == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(open_c2("c2-detach-fail", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(c2 != NULL);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(c2 == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_arguments_and_config(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_c2_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "c2-config", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "arguments", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    config.channel_type = 99;
    CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    config = configuration();
    config.channel_id.descriptive_label = view("bad\0ignored");
    config.channel_id.descriptive_label.size = 11;
    CHECK(ams_mel_ir_c2_open(session, &config, &c2, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_bit_result(const char *scenario, ams_mel_ir_return_t expected)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    ams_mel_ir_return_result_v1 result = {99, 99};
    CHECK(open_c2(scenario, &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, UINT32_C(0x89abcdef), &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, UINT32_C(0x89abcdef), &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == expected);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    result.value = 99;
    CHECK(ams_mel_ir_return_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == expected);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_bit_timeout_lifetime(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    ams_mel_ir_return_result_v1 result = {0, 0};
    CHECK(open_c2("bit-delayed", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 14, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 0, &result, NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.value == AMS_MEL_IR_RETURN_SUCCESS);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_bit_rejection(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    ams_mel_ir_return_result_v1 result = {0, 0};
    char small[8];
    char complete[700];
    size_t required = 0;
    CHECK(open_c2("bit-reject-long", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 15, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, small,
          sizeof small, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(result.error_code == AMS_MEL_ERROR_INVALID_PARAMETERS);
    CHECK(required == 614U);
    CHECK(ams_mel_ir_return_request_wait(request, 0, &result, complete,
          sizeof complete, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strlen(complete) == 613U);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_bit_failures(void)
{
    const char *scenarios[] = {"bit-null-result", "bit-future-throw", "bit-unknown-return"};
    const ams_mel_status_t statuses[] = {
        AMS_MEL_PROVIDER_FAILED, AMS_MEL_PROVIDER_EXCEPTION, AMS_MEL_PROVIDER_FAILED};
    for (size_t i = 0; i < 3U; ++i) {
        ams_mel_session *session = NULL;
        ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL;
        ams_mel_ir_return_result_v1 result = {0, 0};
        CHECK(open_c2(scenarios[i], &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_return_request_wait(request, 1000, &result, NULL, 0, NULL) == statuses[i]);
        CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    {
        ams_mel_session *session = NULL;
        ams_mel_ir_c2 *c2 = NULL;
        ams_mel_ir_return_request *request = NULL;
        CHECK(open_c2("bit-send-throw", &session, &c2) == EXIT_SUCCESS);
        CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
        CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 1, &request, NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(request == NULL);
        CHECK(close_all(&session, &c2) == EXIT_SUCCESS);
    }
    return EXIT_SUCCESS;
}

static int test_bit_request_close_pending(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    char path[] = "/tmp/ams-mel-bit-lifetime-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_c2("bit-lifetime", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 16, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_return_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < 1000U; ++i) {
        CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
        if (strstr(log, "library_unloaded") != NULL) break;
        {
            const struct timespec delay = {0, 1000000L};
            CHECK(nanosleep(&delay, NULL) == 0);
        }
    }
    CHECK(check_order(log, "bit_completed", "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_bit_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_return_request *request = NULL;
    char path[] = "/tmp/ams-mel-bit-post-send-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_c2("bit-lifetime", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(setenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE", failpoint, 1) == 0);
    CHECK(ams_mel_ir_c2_submit_bit_noop(c2, 17, &request, NULL, 0, NULL) ==
          AMS_MEL_INTERNAL_ERROR);
    CHECK(unsetenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE") == 0);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    /* Same bounded-deadline wait as the mode variant above, for the same
       pre-existing load-sensitivity reason. */
    CHECK(wait_for_marker(path, "bit_completed", log, sizeof log) ==
          EXIT_SUCCESS);
    CHECK(strstr(log, "bit_sent") != NULL);
    CHECK(strstr(log, "bit_completed") != NULL);
    CHECK(strstr(log, "c2_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

static int test_coexistence(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 image = image_configuration();
    ams_mel_ir_mode_result_v1 mode = {0, 0};
    ams_mel_ir_frame_v1 frame;
    uint8_t pixels[12];
    char path[] = "/tmp/ams-mel-coexist-XXXXXX";
    char log[8192];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_c2("c2-coexist", &session, &c2) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_stream_open(session, &image, &stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_enable(c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_c2_submit_operate(c2, 42, &request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_start(stream, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels; frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(frame.pixel_required == sizeof pixels);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_mode_request_wait(request, 1000, &mode, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(mode.mode == AMS_MEL_IR_MFA_MODE_TASK_SCHED);
    CHECK(ams_mel_ir_mode_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, NULL, 0, NULL) == AMS_MEL_OK);
    for (unsigned i = 0; i < 100U; ++i) {
        CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
        if (strstr(log, "library_unloaded") != NULL) break;
        {
            const struct timespec delay = {0, 1000000L};
            CHECK(nanosleep(&delay, NULL) == 0);
        }
    }
    CHECK(check_order(log, "mode_completed", "c2_channel_destroyed") == EXIT_SUCCESS);
    CHECK(check_order(log, "callbacks_quiesced_by_channel_destruction",
                      "library_unloaded") == EXIT_SUCCESS);
    CHECK(check_order(log, "c2_channel_destroyed", "library_unloaded") == EXIT_SUCCESS);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "comms-registration-child") == 0)
        return run_comms_registration_failure();
    if (argc == 2 && strcmp(argv[1], "metadata-registration-child") == 0)
        return run_metadata_registration_failure();
    if (argc == 2 && strcmp(argv[1], "metadata-nonquiescing-child") == 0)
        return run_metadata_nonquiescing_disable();
    CHECK(argc == 1);
    CHECK(test_arguments_and_config() == EXIT_SUCCESS);
    CHECK(test_common_pre_enable() == EXIT_SUCCESS);
    CHECK(test_common_failures() == EXIT_SUCCESS);
    CHECK(test_common_delayed_lifetime() == EXIT_SUCCESS);
    CHECK(test_keepalive_delayed_lifetime() == EXIT_SUCCESS);
    CHECK(test_common_pending_close() == EXIT_SUCCESS);
    CHECK(test_capability_snapshot() == EXIT_SUCCESS);
    CHECK(test_malformed_capabilities() == EXIT_SUCCESS);
    CHECK(test_comms_registration_failure(argv[0]) == EXIT_SUCCESS);
    CHECK(test_open_failure("c2-control-capability-wrong", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_open_failure("c2-attach-null", AMS_MEL_FACTORY_FAILED) == EXIT_SUCCESS);
    CHECK(test_open_failure("c2-wrong-type", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_open_failure("c2-channel-capability-wrong", AMS_MEL_INITIALIZATION_FAILED) == EXIT_SUCCESS);
    CHECK(test_success() == EXIT_SUCCESS);
    CHECK(test_general_mode() == EXIT_SUCCESS);
    CHECK(test_general_bit() == EXIT_SUCCESS);
    CHECK(test_config_set() == EXIT_SUCCESS);
    CHECK(test_timeout_and_parent_close() == EXIT_SUCCESS);
    CHECK(test_request_close_pending() == EXIT_SUCCESS);
    CHECK(test_rejection("c2-reject", "invalid task schedule") == EXIT_SUCCESS);
    CHECK(test_rejection("c2-reject-empty", "") == EXIT_SUCCESS);
    CHECK(test_rejection("c2-reject-invalid-utf8",
          "provider rejection description was invalid UTF-8 or contained NUL") == EXIT_SUCCESS);
    CHECK(test_long_rejection() == EXIT_SUCCESS);
    CHECK(test_terminal_failure("c2-null-result", AMS_MEL_PROVIDER_FAILED, "null") == EXIT_SUCCESS);
    CHECK(test_terminal_failure("c2-future-throw", AMS_MEL_PROVIDER_EXCEPTION, "future") == EXIT_SUCCESS);
    CHECK(test_coexistence() == EXIT_SUCCESS);
    CHECK(test_enable_send_cleanup_failures() == EXIT_SUCCESS);
    CHECK(test_bit_result("bit-command-id", AMS_MEL_IR_RETURN_SUCCESS) == EXIT_SUCCESS);
    CHECK(test_bit_result("bit-fail", AMS_MEL_IR_RETURN_FAIL) == EXIT_SUCCESS);
    CHECK(test_bit_timeout_lifetime() == EXIT_SUCCESS);
    CHECK(test_bit_rejection() == EXIT_SUCCESS);
    CHECK(test_bit_failures() == EXIT_SUCCESS);
    CHECK(test_bit_request_close_pending() == EXIT_SUCCESS);
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    CHECK(test_bit_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_bit_post_send_failure("worker-launch") == EXIT_SUCCESS);
    CHECK(test_metadata_rich_and_lifetime() == EXIT_SUCCESS);
    CHECK(test_metadata_overflow_and_malformed() == EXIT_SUCCESS);
    CHECK(test_metadata_registration_failure(argv[0]) == EXIT_SUCCESS);
    CHECK(test_metadata_close_before_c2() == EXIT_SUCCESS);
    CHECK(test_metadata_callback_allocation_failure() == EXIT_SUCCESS);
    CHECK(test_metadata_nonquiescing_disable(argv[0]) == EXIT_SUCCESS);
    CHECK(test_metadata_command_states() == EXIT_SUCCESS);
    /* Post-send launch failures deliberately retain emergency provider roots for
     * process lifetime, so they must follow every unload-order assertion. */
    CHECK(test_common_post_send_failure("keepalive-lifetime", "allocation", 0) == EXIT_SUCCESS);
    CHECK(test_common_post_send_failure("keepalive-lifetime", "worker-launch", 0) == EXIT_SUCCESS);
    CHECK(test_common_post_send_failure("comms-lifetime", "allocation", 1) == EXIT_SUCCESS);
    CHECK(test_common_post_send_failure("comms-lifetime", "worker-launch", 1) == EXIT_SUCCESS);
    puts("PASS: C IR C2 Operate/TaskSched + BIT no-op async/lifetime contract");
    return EXIT_SUCCESS;
}
