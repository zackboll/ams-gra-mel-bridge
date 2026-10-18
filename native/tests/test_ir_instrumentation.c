#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>

#include <fcntl.h>
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

static ams_mel_ir_instrumentation_config_v1 configuration(void)
{
    ams_mel_ir_instrumentation_config_v1 value;
    size_t index;
    memset(&value, 0, sizeof value);
    value.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    value.channel_id.descriptive_label = view("IR instrumentation channel");
    value.platform_id.descriptive_label = view("test platform");
    value.sensor_location.key = view("station-1");
    value.sensor_location.system_name = view("mock-aircraft");
    value.sensor_location.offset_x_m = 1.25;
    value.sensor_location.offset_y_m = -2.5;
    value.sensor_location.offset_z_m = 3.75;
    for (index = 0; index < 16U; ++index) {
        value.channel_id.uuid[index] = (uint8_t)index;
        value.platform_id.uuid[index] = (uint8_t)(0xf0U + index);
    }
    return value;
}

static ams_mel_ir_instrumentation_level_command_v1 rich_command(void)
{
    ams_mel_ir_instrumentation_level_command_v1 value;
    memset(&value, 0, sizeof value);
    value.command_id = 0xe1234567U;
    value.priority = AMS_MEL_IR_PRIORITY_DEBUG;
    return value;
}

static int rich_report_matches(const ams_mel_ir_instrumentation_report_v1 *report)
{
    CHECK(report->command_id == 0xf1234567U);
    CHECK(report->size == 0x89abcdefU);
    CHECK(report->timestamp_ns == -8765432109LL);
    CHECK(report->priority == AMS_MEL_IR_PRIORITY_DEBUG);
    return EXIT_SUCCESS;
}

static int open_channel(const char *scenario, ams_mel_session **session,
                        ams_mel_ir_instrumentation **channel)
{
    ams_mel_ir_instrumentation_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "",
          session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_open(*session, &config, channel,
          NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int open_enabled(const char *scenario, ams_mel_session **session,
                        ams_mel_ir_instrumentation **channel)
{
    CHECK(open_channel(scenario, session, channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_enable(*channel, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int close_all(ams_mel_session **session, ams_mel_ir_instrumentation **channel)
{
    CHECK(ams_mel_ir_instrumentation_close(channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(*channel == NULL);
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

static int test_open_enable_capabilities(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_channel_capability *capability = NULL;
    const ams_mel_ir_channel_capability_v1 *snapshot = NULL;
    size_t index;
    int found = 0;
    CHECK(open_channel("instr-config", &session, &channel) == EXIT_SUCCESS);
    /* Capabilities are valid while Attached, before Enable. */
    CHECK(ams_mel_ir_instrumentation_get_capabilities(channel, &capability,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_view(capability, &snapshot,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(snapshot->width == 1920U && snapshot->height == 1080U);
    CHECK(snapshot->channel_types.size == 1U);
    CHECK(snapshot->channel_types.data[0] == AMS_MEL_IR_CHANNEL_INSTRUMENTATION);
    for (index = 0; index < snapshot->metadata_capabilities.size; ++index)
        if (snapshot->metadata_capabilities.data[index] ==
            AMS_MEL_IR_METADATA_INSTRUMENTATION_REPORT) found = 1;
    CHECK(found);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
    /* Enable is idempotent and capabilities remain valid while Enabled. */
    CHECK(ams_mel_ir_instrumentation_enable(channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_get_capabilities(channel, &capability,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_channel_capability_close(&capability, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_rich_submit_and_fidelity(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    memset(&result, 0xff, sizeof result);
    CHECK(open_enabled("instr-failpoint", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 1000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(result.error_code == AMS_MEL_ERROR_NONE);
    CHECK(rich_report_matches(&result.report) == EXIT_SUCCESS);
    /* Terminal results are cached; a later Wait(0) is identical. */
    memset(&result, 0, sizeof result);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_report_matches(&result.report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_normal_priority(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command;
    memset(&command, 0, sizeof command);
    command.command_id = 7U;
    command.priority = AMS_MEL_IR_PRIORITY_NORMAL;
    CHECK(open_enabled("instr-normal-priority", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 1000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_invalid_arguments(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    ams_mel_ir_instrumentation_config_v1 config = configuration();
    ams_mel_ir_instrumentation *rejected = NULL;
    CHECK(open_enabled("instr-config", &session, &channel) == EXIT_SUCCESS);
    /* Upstream Priority defines no value above Debug. */
    command.priority = AMS_MEL_IR_PRIORITY_DEBUG + 1U;
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    command.priority = 0xffffffffU;
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    command.priority = AMS_MEL_IR_PRIORITY_DEBUG;
    CHECK(ams_mel_ir_instrumentation_submit_level(NULL, &command, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, NULL, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, NULL,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    request = (ams_mel_ir_instrumentation_request *)(void *)1;
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    request = NULL;
    CHECK(ams_mel_ir_instrumentation_enable(NULL, NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_get_capabilities(NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_request_wait(NULL, 0, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_metadata_receive(NULL, 0, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_metadata_event_view(NULL, NULL, NULL, 0, NULL) ==
          AMS_MEL_INVALID_ARGUMENT);
    /* Wrong ChannelType is rejected without touching the provider. */
    config.channel_type = AMS_MEL_IR_CHANNEL_HEALTH_AND_STATUS;
    CHECK(ams_mel_ir_instrumentation_open(session, &config, &rejected,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(rejected == NULL);
    CHECK(ams_mel_ir_instrumentation_open(session, NULL, &rejected,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_open(NULL, &config, &rejected,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_submit_before_enable(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    CHECK(open_channel("instr-failpoint", &session, &channel) == EXIT_SUCCESS);
    /* Instrumentation-specific submission requires Enabled. */
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_enable_failure(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    const ams_mel_status_t expected = strcmp(scenario, "instr-enable-throw") == 0 ?
        AMS_MEL_PROVIDER_EXCEPTION : AMS_MEL_PROVIDER_FAILED;
    CHECK(open_channel(scenario, &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_enable(channel, NULL, 0, NULL) == expected);
    /* A Failed channel neither re-enables nor accepts submissions. */
    CHECK(ams_mel_ir_instrumentation_enable(channel, NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(request == NULL);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_open_failures(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_config_v1 config = configuration();
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "instr-attach-null", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_open(session, &config, &channel,
          NULL, 0, NULL) == AMS_MEL_FACTORY_FAILED);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "instr-wrong-type", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_open(session, &config, &channel,
          NULL, 0, NULL) == AMS_MEL_INITIALIZATION_FAILED);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);

    /* A provider whose channel does not advertise Instrumentation capability. */
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, "instr-capability-wrong", "",
          &session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_open(session, &config, &channel,
          NULL, 0, NULL) == AMS_MEL_INITIALIZATION_FAILED);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    return EXIT_SUCCESS;
}

static int test_terminal_outcome(const char *scenario, ams_mel_status_t expected)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    memset(&result, 0, sizeof result);
    CHECK(open_enabled(scenario, &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 1000, &result,
          NULL, 0, NULL) == expected);
    if (expected == AMS_MEL_COMMAND_REJECTED)
        CHECK(result.error_code == AMS_MEL_ERROR_INVALID_STATE);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_send_throw(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    CHECK(open_enabled("instr-send-throw", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_EXCEPTION);
    CHECK(request == NULL);
    /* A failed submission must not leak a request accounting slot. */
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_long_rejection(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    char expected[614];
    char small[512];
    char *complete;
    size_t required = 0;
    memset(expected, 'x', 510U);
    expected[510] = (char)0xe2; expected[511] = (char)0x82; expected[512] = (char)0xac;
    memset(expected + 513, 'y', 100U);
    expected[613] = '\0';
    CHECK(open_enabled("instr-reject", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 1000, &result, NULL, 0,
          &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(required == sizeof expected);
    memset(small, 0x7f, sizeof small);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result, small,
          sizeof small, &required) == AMS_MEL_COMMAND_REJECTED);
    /* Truncation stops at a complete UTF-8 sequence boundary. */
    CHECK(strlen(small) == 510U);
    CHECK(memcmp(small, expected, 510U) == 0);
    complete = (char *)malloc(required);
    CHECK(complete != NULL);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result, complete,
          required, &required) == AMS_MEL_COMMAND_REJECTED);
    CHECK(strcmp(complete, expected) == 0);
    free(complete);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_synchronous_callbacks(void)
{
    /* The mock emits one InstrumentationReport synchronously inside
       registerMetadataCallback and one synchronously inside send(). Neither
       may deadlock, and the future result is delivered independently. */
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    ams_mel_ir_instrumentation_metadata_event *event = NULL;
    const ams_mel_ir_instrumentation_metadata_event_v1 *snapshot = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_metadata_counters_v1 counters;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    CHECK(open_enabled("instr-rich", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 8, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    /* Registration-time synchronous callback. */
    CHECK(ams_mel_ir_instrumentation_metadata_receive(metadata, 1000, &event,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_event_view(event, &snapshot,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(snapshot->kind == AMS_MEL_IR_INSTRUMENTATION_METADATA_REPORT);
    CHECK(rich_report_matches(&snapshot->report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(event == NULL);
    /* Send-time synchronous callback plus an independent future result. */
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 1000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_report_matches(&result.report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_receive(metadata, 1000, &event,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_event_view(event, &snapshot,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_report_matches(&snapshot->report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_event_close(&event, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_get_counters(metadata, &counters,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 2U);
    CHECK(counters.events_dropped_queue_full == 0U);
    CHECK(counters.malformed_or_unsupported == 0U);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    /* Metadata Close only deactivates public consumption. */
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_overflow_and_counters(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    ams_mel_ir_instrumentation_metadata_event *event = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    unsigned drained = 0;
    CHECK(open_enabled("instr-overflow", &session, &channel) == EXIT_SUCCESS);
    /* Six synchronous registration events into a capacity-2 DROP-INCOMING queue. */
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 2, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_get_counters(metadata, &counters,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 6U);
    CHECK(counters.events_dropped_queue_full == 4U);
    CHECK(counters.malformed_or_unsupported == 0U);
    while (ams_mel_ir_instrumentation_metadata_receive(metadata, 0, &event,
           NULL, 0, NULL) == AMS_MEL_OK) {
        const ams_mel_ir_instrumentation_metadata_event_v1 *snapshot = NULL;
        CHECK(ams_mel_ir_instrumentation_metadata_event_view(event, &snapshot,
              NULL, 0, NULL) == AMS_MEL_OK);
        /* FIFO: the first two of six retained, not the last two. */
        CHECK(snapshot->report.command_id == drained);
        CHECK(ams_mel_ir_instrumentation_metadata_event_close(&event,
              NULL, 0, NULL) == AMS_MEL_OK);
        ++drained;
    }
    CHECK(drained == 2U);
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_malformed(const char *scenario)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    ams_mel_ir_instrumentation_metadata_event *event = NULL;
    ams_mel_ir_metadata_counters_v1 counters;
    CHECK(open_enabled(scenario, &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 4, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_get_counters(metadata, &counters,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(counters.events_received == 1U);
    CHECK(counters.malformed_or_unsupported == 1U);
    CHECK(ams_mel_ir_instrumentation_metadata_receive(metadata, 0, &event,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_callback_allocation_failure(void)
{
    /* A callback allocation failure must not escape into provider code; it
       fails the queue rather than crossing the boundary. */
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    ams_mel_ir_instrumentation_metadata_event *event = NULL;
    CHECK(open_enabled("instr-callback-allocation", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 4, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_metadata_receive(metadata, 0, &event,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(event == NULL);
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_metadata_registration_failure(void)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    CHECK(open_enabled("instr-register-fail", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 4, &metadata,
          NULL, 0, NULL) == AMS_MEL_PROVIDER_FAILED);
    CHECK(metadata == NULL);
    /* A second attempt is refused; registration is once per channel. */
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 4, &metadata,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 0, &metadata,
          NULL, 0, NULL) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(close_all(&session, &channel) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

/* Test-only observers; no production ABI or provider ownership is added. */
extern int ams_mel_test_completion_boundary(unsigned family, unsigned operation,
                                            uint64_t target, uint64_t *values);
extern int ams_mel_test_completion_owner(unsigned family, uint64_t target,
                                         uint64_t *observed);
enum { instrumentation_family = 4U, completion_snapshot = 3U };

/* Per-test external files confer no channel, request or DSO ownership. Keep
 * files/environment through provider unload AND final-owner retirement: its
 * release/acquire observation follows the relevant probe calls and file closes,
 * not necessarily operating-system thread exit. Drain earlier owners before
 * installing this controller's environment. Intentionally retained cases run last. */
struct lifetime_control {
    char directory[128];
    char log[160];
    uint64_t owners_created_before;
    uint64_t owner_target;
};

static int control_marker(const struct lifetime_control *control, const char *name,
                          int create)
{
    char path[192];
    CHECK(snprintf(path, sizeof path, "%s/%s", control->directory, name) > 0);
    if (create) {
        int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        CHECK(descriptor >= 0);
        CHECK(close(descriptor) == 0);
    } else CHECK(access(path, F_OK) != 0);
    return EXIT_SUCCESS;
}

static int control_event(const struct lifetime_control *control, const char *event)
{
    FILE *stream = fopen(control->log, "ab");
    CHECK(stream != NULL);
    CHECK(fprintf(stream, "%s\n", event) > 0);
    CHECK(fclose(stream) == 0);
    return EXIT_SUCCESS;
}

static int control_open(struct lifetime_control *control)
{
    int descriptor;
    uint64_t values[4], observed;
    CHECK(ams_mel_test_completion_boundary(instrumentation_family,
          completion_snapshot, 0, values) == 1);
    CHECK(ams_mel_test_completion_owner(instrumentation_family, values[2],
          &observed) == 1);
    CHECK(observed == values[2]);
    control->owners_created_before = values[2];
    control->owner_target = values[2];
    strcpy(control->directory, "/tmp/ams-mel-instr-held-XXXXXX");
    CHECK(mkdtemp(control->directory) != NULL);
    CHECK(snprintf(control->log, sizeof control->log, "%s/log", control->directory) > 0);
    descriptor = open(control->log, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", control->log, 1) == 0);
    CHECK(setenv("AMS_MEL_TEST_INSTRUMENTATION_HOLD", control->directory, 1) == 0);
    return EXIT_SUCCESS;
}

static int control_submitted(struct lifetime_control *control)
{
    uint64_t values[4];
    CHECK(ams_mel_test_completion_boundary(instrumentation_family,
          completion_snapshot, 0, values) == 1);
    CHECK(control->owners_created_before < UINT64_MAX);
    CHECK(values[2] == control->owners_created_before + 1U);
    control->owner_target = values[2];
    return EXIT_SUCCESS;
}

static int control_wait(const struct lifetime_control *control, const char *event,
                        int marker)
{
    struct timespec start, now;
    char path[192], log[4096];
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(snprintf(path, sizeof path, "%s/%s", control->directory, event) > 0);
    for (;;) {
        if (marker) {
            if (access(path, F_OK) == 0) return EXIT_SUCCESS;
        } else {
            CHECK(read_log(control->log, log, sizeof log) == EXIT_SUCCESS);
            if (strstr(log, event) != NULL) return EXIT_SUCCESS;
        }
        CHECK(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
        if (now.tv_sec - start.tv_sec >= 5) {
            fprintf(stderr, "Instrumentation hold timed out waiting for %s in %s\n",
                    event, control->directory);
            return EXIT_FAILURE;
        }
        {
            const struct timespec pause = {0, 1000000L};
            CHECK(nanosleep(&pause, NULL) == 0);
        }
    }
}

static int control_held(const struct lifetime_control *control)
{
    char log[4096];
    CHECK(control_marker(control, "release", 0) == EXIT_SUCCESS);
    CHECK(control_marker(control, "closed", 0) == EXIT_SUCCESS);
    CHECK(read_log(control->log, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "instrumentation_completed") == NULL);
    CHECK(strstr(log, "instrumentation_disabled") == NULL);
    CHECK(strstr(log, "instrumentation_channel_destroyed") == NULL);
    CHECK(strstr(log, "library_unloaded") == NULL);
    return EXIT_SUCCESS;
}

static int control_release(const struct lifetime_control *control)
{
    CHECK(control_held(control) == EXIT_SUCCESS);
    CHECK(control_event(control, "public_closes_completed") == EXIT_SUCCESS);
    CHECK(control_marker(control, "closed", 1) == EXIT_SUCCESS);
    CHECK(control_event(control, "controller_released") == EXIT_SUCCESS);
    CHECK(control_marker(control, "release", 1) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int control_finish(const struct lifetime_control *control)
{
    char log[4096], path[192];
    uint64_t observed;
    const char *completed, *destroyed, *unloaded, *released, *closed;
    CHECK(control_wait(control, "library_unloaded\n", 0) == EXIT_SUCCESS);
    CHECK(control->owners_created_before < UINT64_MAX);
    CHECK(control->owner_target == control->owners_created_before + 1U);
    CHECK(ams_mel_test_completion_owner(instrumentation_family,
          control->owner_target, &observed) == 1);
    CHECK(observed == control->owner_target);
    CHECK(read_log(control->log, log, sizeof log) == EXIT_SUCCESS);
    closed = strstr(log, "public_closes_completed\n");
    released = strstr(log, "controller_released\n");
    completed = strstr(log, "instrumentation_completed\n");
    destroyed = strstr(log, "instrumentation_channel_destroyed\n");
    unloaded = strstr(log, "library_unloaded\n");
    CHECK(closed != NULL && released != NULL && completed != NULL &&
          destroyed != NULL && unloaded != NULL);
    CHECK(closed < released && released < completed);
    CHECK(completed < destroyed && destroyed < unloaded);
    CHECK(strstr(log, "instrumentation_disabled\n") != NULL);
    CHECK(unsetenv("AMS_MEL_TEST_INSTRUMENTATION_HOLD") == 0);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    for (size_t index = 0; index < 4U; ++index) {
        const char *names[] = {"entered", "closed", "release", "log"};
        CHECK(snprintf(path, sizeof path, "%s/%s", control->directory, names[index]) > 0);
        CHECK(unlink(path) == 0);
    }
    CHECK(rmdir(control->directory) == 0);
    return EXIT_SUCCESS;
}

static int test_parent_first_close(void)
{
    /* Session and channel close before the future is ready. The provider and
       library must remain owned until the request completes. */
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    ams_mel_ir_instrumentation_metadata *metadata = NULL;
    struct lifetime_control control;
    char log[4096];
    CHECK(control_open(&control) == EXIT_SUCCESS);
    CHECK(open_enabled("instr-lifetime-held", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_metadata_open(channel, 8, &metadata,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(control_submitted(&control) == EXIT_SUCCESS);
    CHECK(control_wait(&control, "entered", 1) == EXIT_SUCCESS);
    CHECK(control_held(&control) == EXIT_SUCCESS);
    CHECK(read_log(control.log, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "instrumentation_registration_callback_returned") != NULL);
    CHECK(strstr(log, "instrumentation_send_callback_returned") != NULL);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    /* Close the metadata owner, the channel, and the Session parent first. */
    CHECK(ams_mel_ir_instrumentation_metadata_close(&metadata, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(metadata == NULL);
    CHECK(ams_mel_ir_instrumentation_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(control_held(&control) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(control_release(&control) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 2000, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_report_matches(&result.report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(rich_report_matches(&result.report) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(control_finish(&control) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_request_close_while_pending(void)
{
    /* Request close is not cancellation: provider work still completes and the
       deferred channel teardown still runs. */
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_result_v1 result;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    struct lifetime_control control;
    CHECK(control_open(&control) == EXIT_SUCCESS);
    CHECK(open_enabled("instr-lifetime-held", &session, &channel) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_submit_level(channel, &command, &request,
          NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(control_submitted(&control) == EXIT_SUCCESS);
    CHECK(control_wait(&control, "entered", 1) == EXIT_SUCCESS);
    CHECK(control_held(&control) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_request_wait(request, 0, &result,
          NULL, 0, NULL) == AMS_MEL_TIMEOUT);
    CHECK(ams_mel_ir_instrumentation_request_close(&request, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(request == NULL);
    CHECK(control_held(&control) == EXIT_SUCCESS);
    CHECK(ams_mel_ir_instrumentation_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(channel == NULL);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(session == NULL);
    CHECK(control_held(&control) == EXIT_SUCCESS);
    CHECK(control_release(&control) == EXIT_SUCCESS);
    CHECK(control_finish(&control) == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static int test_post_send_failure(const char *failpoint)
{
    ams_mel_session *session = NULL; ams_mel_ir_instrumentation *channel = NULL;
    ams_mel_ir_instrumentation_request *request = NULL;
    ams_mel_ir_instrumentation_level_command_v1 command = rich_command();
    char path[] = "/tmp/ams-mel-instr-post-send-XXXXXX";
    char log[4096];
    int descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    CHECK(open_enabled("instr-lifetime", &session, &channel) == EXIT_SUCCESS);
    CHECK(setenv("AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE", failpoint, 1) == 0);
    {
        char diagnostic[128] = {0};
        const ams_mel_status_t status = ams_mel_ir_instrumentation_submit_level(
            channel, &command, &request, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_INTERNAL_ERROR)
            fprintf(stderr, "post-send %s returned %d: %s\n", failpoint,
                    (int)status, diagnostic);
        CHECK(status == AMS_MEL_INTERNAL_ERROR);
    }
    CHECK(unsetenv("AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE") == 0);
    /* No public request escapes, but the provider send already happened. */
    CHECK(request == NULL);
    CHECK(ams_mel_ir_instrumentation_close(&channel, NULL, 0, NULL) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&session, NULL, 0, NULL) == AMS_MEL_OK);
    {
        const struct timespec delay = {0, 200000000L};
        CHECK(nanosleep(&delay, NULL) == 0);
    }
    CHECK(read_log(path, log, sizeof log) == EXIT_SUCCESS);
    CHECK(strstr(log, "instrumentation_level_sent") != NULL);
    /* The provider future is retained safely: the library is NOT unloaded. */
    CHECK(strstr(log, "library_unloaded") == NULL);
    CHECK(unsetenv("AMS_MEL_TEST_LIFETIME_LOG") == 0);
    CHECK(unlink(path) == 0);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(test_open_enable_capabilities() == EXIT_SUCCESS);
    CHECK(test_rich_submit_and_fidelity() == EXIT_SUCCESS);
    CHECK(test_normal_priority() == EXIT_SUCCESS);
    CHECK(test_invalid_arguments() == EXIT_SUCCESS);
    CHECK(test_submit_before_enable() == EXIT_SUCCESS);
    CHECK(test_enable_failure("instr-enable-fail") == EXIT_SUCCESS);
    CHECK(test_enable_failure("instr-enable-throw") == EXIT_SUCCESS);
    CHECK(test_open_failures() == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("instr-reject", AMS_MEL_COMMAND_REJECTED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("instr-unknown-error", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("instr-null", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("instr-invalid-priority", AMS_MEL_PROVIDER_FAILED) == EXIT_SUCCESS);
    CHECK(test_terminal_outcome("instr-future-throw", AMS_MEL_PROVIDER_EXCEPTION) == EXIT_SUCCESS);
    CHECK(test_send_throw() == EXIT_SUCCESS);
    CHECK(test_long_rejection() == EXIT_SUCCESS);
    CHECK(test_synchronous_callbacks() == EXIT_SUCCESS);
    CHECK(test_metadata_overflow_and_counters() == EXIT_SUCCESS);
    CHECK(test_metadata_malformed("instr-callback-null") == EXIT_SUCCESS);
    CHECK(test_metadata_malformed("instr-callback-invalid-priority") == EXIT_SUCCESS);
    CHECK(test_metadata_callback_allocation_failure() == EXIT_SUCCESS);
    CHECK(test_metadata_registration_failure() == EXIT_SUCCESS);
    CHECK(test_parent_first_close() == EXIT_SUCCESS);
    CHECK(test_request_close_while_pending() == EXIT_SUCCESS);
    /* These intentionally leak a permanently-retained provider library
       reference (fail-safe retention). Run them last so earlier
       library_unloaded assertions are unaffected. */
    CHECK(test_post_send_failure("allocation") == EXIT_SUCCESS);
    CHECK(test_post_send_failure("worker-launch") == EXIT_SUCCESS);
    puts("PASS: C IR Instrumentation channel contract");
    return EXIT_SUCCESS;
}
