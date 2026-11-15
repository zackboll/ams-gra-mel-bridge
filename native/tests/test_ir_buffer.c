#define _POSIX_C_SOURCE 200809L
#include <ams_mel/abi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define D NULL, 0, NULL
static ams_mel_session *session(const char *scenario)
{
    ams_mel_session *s = NULL;
    CHECK(ams_mel_session_open(AMS_MEL_TEST_MOCK_PROVIDER, scenario, "", &s, D) == AMS_MEL_OK);
    return s;
}
static size_t event_count(const char *path, const char *event)
{
    FILE *log = fopen(path, "r");
    char events[32768];
    size_t count = 0;
    CHECK(log);
    const size_t length = fread(events, 1, sizeof events - 1, log);
    CHECK(!ferror(log) && feof(log));
    events[length] = '\0';
    CHECK(fclose(log) == 0);
    for (const char *next = events; (next = strstr(next, event)) != NULL; next += strlen(event))
        ++count;
    return count;
}
static void check_preserved_buffer(ams_mel_ir_buffer *b, ams_mel_ir_buffer *saved)
{
    ams_mel_ir_buffer_info_v1 info;
    uint8_t bytes[16];
    CHECK(b == saved);
    CHECK(ams_mel_ir_buffer_get_info(b, &info, D) == AMS_MEL_OK);
    CHECK(info.storage_size == sizeof bytes && info.provider_size == 16);
    CHECK(info.initialized_context == -7 && info.provider_context == -7);
    CHECK(info.buffer_address_matches == 1);
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, bytes, sizeof bytes, D) == AMS_MEL_OK);
    for (size_t i = 0; i < sizeof bytes; ++i) CHECK(bytes[i] == 0xa5);
}
static void output_slot_preservation(const char *path)
{
    ams_mel_session *s = session("foundation-normal");
    ams_mel_ir_buffer *b = NULL;
    uint32_t result = 55;
    uint8_t bytes[16];
    const uint64_t sizes[] = {16, 0, UINT64_MAX, (uint64_t)INT64_MAX + 1};
    CHECK(ams_mel_ir_buffer_open(s, 16, -7, NULL, &result, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(result == UINT32_MAX);
    CHECK(ams_mel_ir_buffer_open(s, 16, -7, &b, NULL, D) == AMS_MEL_INVALID_ARGUMENT && !b);
    CHECK(event_count(path, "buffer_factory_called") == 0);
    CHECK(event_count(path, "foundation_buffer_initialized") == 0);
    CHECK(ams_mel_ir_buffer_open(s, 16, -7, &b, &result, D) == AMS_MEL_OK);
    CHECK(b && result == 0);
    ams_mel_ir_buffer *const saved = b;
    memset(bytes, 0xa5, sizeof bytes);
    CHECK(ams_mel_ir_buffer_write_storage(b, 0, bytes, sizeof bytes, D) == AMS_MEL_OK);
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
        result = 55;
        CHECK(ams_mel_ir_buffer_open(s, sizes[i], 99, &b, &result, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(result == UINT32_MAX);
        check_preserved_buffer(b, saved);
        CHECK(event_count(path, "buffer_factory_called") == 1);
        CHECK(event_count(path, "foundation_buffer_initialized") == 1);
        CHECK(event_count(path, "foundation_buffer_destroyed_with_storage") == 0);
    }
    CHECK(ams_mel_ir_buffer_open(s, 16, 99, &b, NULL, D) == AMS_MEL_INVALID_ARGUMENT);
    check_preserved_buffer(b, saved);
    CHECK(event_count(path, "buffer_factory_called") == 1);
    CHECK(event_count(path, "foundation_buffer_initialized") == 1);
    CHECK(event_count(path, "foundation_buffer_destroyed_with_storage") == 0);
    CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK && !b);
    CHECK(event_count(path, "foundation_buffer_destroyed_with_storage") == 1);
    CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK && !s);
    CHECK(event_count(path, "control_destroyed") == 1);
    CHECK(event_count(path, "manager_destroyed") == 1);
    CHECK(event_count(path, "library_unloaded") == 1);
}
static void success(const char *scenario, int64_t context, int parent_first)
{
    ams_mel_session *s = session(scenario);
    ams_mel_ir_buffer *b = NULL;
    ams_mel_ir_buffer_info_v1 info;
    uint32_t result = UINT32_MAX;
    size_t count = 99;
    uint32_t flags[8] = {99};
    const uint32_t repeated[] = {2, 0, 1, 2, 2, 0};
    uint8_t input[32], output[32];
    memset(input, 0xa5, sizeof input);
    CHECK(ams_mel_ir_buffer_open(s, 32, context, &b, &result, D) == AMS_MEL_OK);
    CHECK(result == 0 && b);
    if (parent_first) CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_get_info(b, &info, D) == AMS_MEL_OK);
    CHECK(info.storage_size == 32 && info.initialized_context == context);
    CHECK(info.provider_context == (strcmp(scenario, "foundation-properties") == 0 ? INT64_MIN : context));
    CHECK(info.provider_size == (strcmp(scenario, "foundation-properties") == 0 ? -42 : 32));
    CHECK(info.buffer_address_matches == (strcmp(scenario, "foundation-address-mismatch") != 0));
    CHECK(info.image_location == (strcmp(scenario, "foundation-image-null") == 0 ? 0U :
        strcmp(scenario, "foundation-image-outside") == 0 ? 2U : 1U));
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, output, 32, D) == AMS_MEL_OK);
    for (size_t i = 0; i < 32; ++i) CHECK(output[i] == 0);
    CHECK(ams_mel_ir_buffer_write_storage(b, 0, input, 32, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_get_flags(b, NULL, 0, &count, &result, D) == AMS_MEL_OK);
    CHECK(count == 0 && result == 0);
    CHECK(ams_mel_ir_buffer_add_flag(b, AMS_MEL_IR_BUFFER_HOST, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_get_flags(b, flags, 8, &count, &result, D) == AMS_MEL_OK);
    CHECK(count == 1 && flags[0] == 0);
    CHECK(ams_mel_ir_buffer_set_flags(b, repeated, 6, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_add_flag(b, 1, D) == AMS_MEL_OK);
    flags[0] = 99;
    CHECK(ams_mel_ir_buffer_get_flags(b, flags, 1, &count, &result, D) == AMS_MEL_BUFFER_TOO_SMALL);
    CHECK(count == 7 && flags[0] == 99);
    CHECK(ams_mel_ir_buffer_get_flags(b, flags, 8, &count, &result, D) == AMS_MEL_OK);
    CHECK(count == 7 && flags[6] == 1 && memcmp(flags, repeated, sizeof repeated) == 0);
    CHECK(ams_mel_ir_buffer_set_flags(b, NULL, 0, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_get_flags(b, NULL, 0, &count, &result, D) == AMS_MEL_OK && count == 0);
    CHECK(ams_mel_ir_buffer_add_flag(b, 3, D) == AMS_MEL_INVALID_ARGUMENT);
    flags[0] = UINT32_MAX;
    CHECK(ams_mel_ir_buffer_set_flags(b, flags, 1, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_set_flags(b, NULL, 1, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, output, 32, D) == AMS_MEL_OK);
    CHECK(memcmp(input, output, 32) == 0);
    CHECK(ams_mel_ir_buffer_write_storage(b, 31, input, 2, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_read_storage(b, UINT64_MAX, output, 1, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_read_storage(b, 1, output, SIZE_MAX, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_write_storage(b, 32, NULL, 0, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_read_storage(b, 33, NULL, 0, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, NULL, 1, D) == AMS_MEL_INVALID_ARGUMENT);
    /* The mock always returns the original initialized address: repeated getter
     * equality proves storage stability across flags, reads and writes. */
    CHECK(ams_mel_ir_buffer_get_info(b, &info, D) == AMS_MEL_OK);
    CHECK(info.buffer_address_matches == (strcmp(scenario, "foundation-address-mismatch") != 0));
    CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK && !b);
    CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, output, 1, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_ir_buffer_get_info(b, &info, D) == AMS_MEL_INVALID_ARGUMENT);
    CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
}
static void refusal(const char *scenario, int expected, uint32_t expected_return)
{
    ams_mel_session *s = session(scenario);
    ams_mel_ir_buffer *b = NULL;
    uint32_t result = 55;
    CHECK(ams_mel_ir_buffer_open(s, 16, -7, &b, &result, D) == expected);
    CHECK(!b && result == expected_return);
    CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
}
static void operation_failure(const char *scenario, int operation, int expected, uint32_t expected_return)
{
    ams_mel_session *s = session(scenario);
    ams_mel_ir_buffer *b = NULL;
    uint32_t result, flag = 0;
    size_t count;
    ams_mel_ir_buffer_info_v1 info;
    CHECK(ams_mel_ir_buffer_open(s, 16, -7, &b, &result, D) == AMS_MEL_OK);
    if (operation == 0) {
        CHECK(ams_mel_ir_buffer_get_flags(b, &flag, 1, &count, &result, D) == expected);
        CHECK(result == expected_return && flag == 0 && count == 0);
    } else if (operation == 1) CHECK(ams_mel_ir_buffer_add_flag(b, 0, D) == expected);
    else if (operation == 2) CHECK(ams_mel_ir_buffer_set_flags(b, &flag, 1, D) == expected);
    else CHECK(ams_mel_ir_buffer_get_info(b, &info, D) == expected);
    CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
}
static void acquisition_independence(void)
{
    /* The same Session creates an ordinary Mono8 stream and an unregistered
     * standalone owner. The original mock pool aborts if the standalone owner
     * is incorrectly released (it never has an outstanding checkout). */
    ams_mel_session *s = session("success");
    ams_mel_ir_buffer *b = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_stream_config_v1 config;
    uint32_t result;
    uint8_t pattern[64], actual[64], pixels[12];
    ams_mel_ir_frame_v1 frame;
    memset(&config, 0, sizeof config);
    config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    for (size_t i = 0; i < 16; ++i) {
        config.channel_id.uuid[i] = (uint8_t)i;
        config.platform_id.uuid[i] = (uint8_t)(0xf0U + i);
    }
    config.channel_id.descriptive_label = (ams_mel_string_view_v1){"IR image channel", 16};
    config.platform_id.descriptive_label = (ams_mel_string_view_v1){"test platform", 13};
    config.sensor_location.offset_x_m = 1.25;
    config.sensor_location.offset_y_m = -2.5;
    config.sensor_location.offset_z_m = 3.75;
    config.sensor_location.key = (ams_mel_string_view_v1){"station-1", 9};
    config.sensor_location.system_name = (ams_mel_string_view_v1){"mock-aircraft", 13};
    config.buffer_count = 3;
    config.buffer_size = 64;
    config.queue_capacity = 4;
    memset(pattern, 0x5a, sizeof pattern);
    CHECK(ams_mel_ir_buffer_open(s, 64, INT64_MIN, &b, &result, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_buffer_write_storage(b, 0, pattern, sizeof pattern, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_open(s, &config, &stream, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_start(stream, D) == AMS_MEL_OK);
    memset(&frame, 0, sizeof frame);
    frame.pixels = pixels;
    frame.pixel_capacity = sizeof pixels;
    CHECK(ams_mel_ir_stream_receive(stream, 1000, &frame, D) == AMS_MEL_OK);
    CHECK(frame.width == 4 && frame.height == 3 && frame.pixel_required == sizeof pixels);
    for (size_t i = 0; i < sizeof pixels; ++i) CHECK(pixels[i] == (uint8_t)(16U + i));
    CHECK(ams_mel_ir_buffer_read_storage(b, 0, actual, sizeof actual, D) == AMS_MEL_OK);
    CHECK(memcmp(actual, pattern, sizeof actual) == 0);
    CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_stop(stream, D) == AMS_MEL_OK);
    CHECK(ams_mel_ir_stream_close(&stream, D) == AMS_MEL_OK);
    CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
}
int main(void)
{
    char path[] = "/tmp/irmel-buffer-XXXXXX";
    const int fd = mkstemp(path);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(setenv("AMS_MEL_TEST_LIFETIME_LOG", path, 1) == 0);
    output_slot_preservation(path);
    success("foundation-normal", 23, 0);
    success("foundation-normal", 0, 1);
    success("foundation-normal", INT64_MIN, 0);
    success("foundation-normal", INT64_MAX, 1);
    success("foundation-image-null", -8, 1);
    success("foundation-image-outside", -8, 1);
    success("foundation-address-mismatch", -8, 0);
    success("foundation-properties", -8, 0);
    refusal("foundation-factory-null", AMS_MEL_FACTORY_FAILED, UINT32_MAX);
    refusal("foundation-factory-throw", AMS_MEL_PROVIDER_EXCEPTION, UINT32_MAX);
    refusal("foundation-factory-allocation", AMS_MEL_INTERNAL_ERROR, UINT32_MAX);
    refusal("foundation-init-refuse", AMS_MEL_INITIALIZATION_FAILED, 3);
    operation_failure("foundation-get-refuse", 0, AMS_MEL_PROVIDER_FAILED, 4);
    operation_failure("foundation-get-throw", 0, AMS_MEL_PROVIDER_EXCEPTION, UINT32_MAX);
    operation_failure("foundation-unknown-flag", 0, AMS_MEL_PROVIDER_FAILED, 0);
    operation_failure("foundation-add-throw", 1, AMS_MEL_PROVIDER_EXCEPTION, 0);
    operation_failure("foundation-set-throw", 2, AMS_MEL_PROVIDER_EXCEPTION, 0);
    operation_failure("foundation-info-throw", 3, AMS_MEL_PROVIDER_EXCEPTION, 0);
    {
        ams_mel_session *s = session("foundation-normal");
        ams_mel_ir_buffer *b = NULL;
        uint32_t result;
        const uint64_t invalid[] = {0, UINT64_MAX, (uint64_t)INT64_MAX + 1};
        for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
            CHECK(ams_mel_ir_buffer_open(s, invalid[i], 0, &b, &result, D) == AMS_MEL_INVALID_ARGUMENT && !b);
        CHECK(ams_mel_ir_buffer_open(s, 16, 0, NULL, &result, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_buffer_open(s, 16, 0, &b, NULL, D) == AMS_MEL_INVALID_ARGUMENT && !b);
        CHECK(ams_mel_ir_buffer_close(NULL, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(ams_mel_ir_buffer_open(s, 16, 0, &b, &result, D) == AMS_MEL_OK);
        CHECK(ams_mel_ir_buffer_get_info(b, NULL, D) == AMS_MEL_INVALID_ARGUMENT);
        {
            size_t count;
            CHECK(ams_mel_ir_buffer_get_flags(b, NULL, 1, &count, &result, D) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(ams_mel_ir_buffer_get_flags(b, NULL, 0, NULL, &result, D) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(ams_mel_ir_buffer_get_flags(b, NULL, 0, &count, NULL, D) == AMS_MEL_INVALID_ARGUMENT);
        }
        CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
        CHECK(ams_mel_ir_buffer_open(s, 16, 0, &b, &result, D) == AMS_MEL_INVALID_ARGUMENT);
    }
    /* Check normal teardown before intentionally retained graphs pin the DSO. */
    {
        FILE *log = fopen(path, "r");
        char events[32768];
        CHECK(log);
        const size_t length = fread(events, 1, sizeof events - 1, log);
        events[length] = '\0';
        CHECK(fclose(log) == 0);
        const char *destroyed = strstr(events, "foundation_buffer_destroyed_with_storage");
        const char *control = strstr(events, "control_destroyed");
        const char *manager = strstr(events, "manager_destroyed");
        const char *unloaded = strstr(events, "library_unloaded");
        CHECK(destroyed && control && manager && unloaded);
        CHECK(destroyed < control && control < manager && manager < unloaded);
        CHECK(strstr(events, "foundation_instance_manager_verified"));
        CHECK(!strstr(events, "channel_attached") && !strstr(events, "buffer_registered") &&
              !strstr(events, "buffer_released") && !strstr(events, "image_delivered"));
    }
    acquisition_independence();
    refusal("foundation-init-throw", AMS_MEL_PROVIDER_EXCEPTION, UINT32_MAX);
    {
        ams_mel_session *s = session("foundation-retained");
        ams_mel_ir_buffer *b = NULL;
        uint32_t result;
        CHECK(ams_mel_ir_buffer_open(s, 16, 0, &b, &result, D) == AMS_MEL_OK);
        CHECK(ams_mel_session_close(&s, D) == AMS_MEL_OK);
        CHECK(ams_mel_ir_buffer_close(&b, D) == AMS_MEL_PROVIDER_FAILED && b);
    }
    CHECK(unlink(path) == 0);
    return 0;
}
