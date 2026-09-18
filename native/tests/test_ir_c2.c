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
    command.state = AMS_MEL_IR_MFA_STATE_OPERATE;
    command.scan_parameters.elevation_defined_with_range_and_altitude = 2;
    CHECK(ams_mel_ir_c2_submit_mode(c2, &command, &request, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
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
    CHECK(ams_mel_ir_c2_submit_operate(c2, 9, &request, NULL, 0, NULL) ==
          AMS_MEL_INTERNAL_ERROR);
    CHECK(unsetenv("AMS_MEL_TEST_C2_POST_SEND_FAILURE") == 0);
    CHECK(request == NULL);
    CHECK(ams_mel_ir_c2_close(&c2, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 100000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
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
    {
        const struct timespec delay = {0, 100000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
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

int main(void)
{
    CHECK(test_arguments_and_config() == EXIT_SUCCESS);
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
    puts("PASS: C IR C2 Operate/TaskSched + BIT no-op async/lifetime contract");
    return EXIT_SUCCESS;
}
