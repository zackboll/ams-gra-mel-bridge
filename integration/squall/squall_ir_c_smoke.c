#include <ams_mel/abi.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAGNOSTIC_CAPACITY 1024U
#define MAX_FRAME_BYTES (1024U * 1024U)

static char diagnostic[DIAGNOSTIC_CAPACITY];

static ams_mel_string_view_v1 view(const char *text)
{
    ams_mel_string_view_v1 result = {text, strlen(text)};
    return result;
}

static int failed(const char *stage, ams_mel_status_t status)
{
    fprintf(stderr, "FAIL: %s: facade status=%" PRId32 ": %s\n",
            stage, status, diagnostic[0] == '\0' ? "no diagnostic" : diagnostic);
    return EXIT_FAILURE;
}

static ams_mel_status_t open_session(const char *library, const char *profile,
                                     ams_mel_session **session)
{
    size_t required = 0;
    diagnostic[0] = '\0';
    return ams_mel_session_open(library, profile, "", session, diagnostic,
                                sizeof diagnostic, &required);
}

static void fill_ids(ams_mel_uci_id_v1 *channel, ams_mel_uci_id_v1 *platform,
                     const char *label)
{
    static const uint8_t channel_uuid[16] = {
        0x00, 0x40, 0x04, 0x00, 0x11, 0x22, 0x43, 0x44,
        0x85, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc
    };
    static const uint8_t platform_uuid[16] = {
        0x00, 0x40, 0x04, 0x01, 0x21, 0x32, 0x43, 0x54,
        0x86, 0x67, 0x78, 0x89, 0x9a, 0xab, 0xbc, 0xcd
    };
    memcpy(channel->uuid, channel_uuid, sizeof channel->uuid);
    memcpy(platform->uuid, platform_uuid, sizeof platform->uuid);
    channel->descriptive_label = view(label);
    platform->descriptive_label = view("Task 004 integration platform");
}

static void fill_location(ams_mel_component_location_v1 *location)
{
    location->offset_x_m = 0.0;
    location->offset_y_m = 0.0;
    location->offset_z_m = 0.0;
    location->key = view("task-004-station");
    location->system_name = view("ams-mel-squall-integration");
}

static uint64_t checksum(const uint8_t *pixels, size_t count)
{
    uint64_t value = UINT64_C(1469598103934665603);
    for (size_t index = 0; index < count; ++index) {
        value ^= pixels[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static void print_counters(ams_mel_ir_stream *stream)
{
    ams_mel_ir_stream_counters_v1 counters;
    if (stream == NULL) return;
    diagnostic[0] = '\0';
    if (ams_mel_ir_stream_get_counters(stream, &counters, diagnostic,
                                       sizeof diagnostic, NULL) == AMS_MEL_OK) {
        fprintf(stderr, "IR stream counters: received=%" PRIu64
                        " dropped=%" PRIu64 " malformed=%" PRIu64 "\n",
                counters.frames_received, counters.frames_dropped_queue_full,
                counters.malformed_or_unsupported_frames);
    } else {
        fprintf(stderr, "IR stream counters unavailable: %s\n",
                diagnostic[0] == '\0' ? "no diagnostic" : diagnostic);
    }
}

int main(int argc, char **argv)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_stream *stream = NULL;
    ams_mel_ir_c2 *c2 = NULL;
    ams_mel_ir_mode_request *request = NULL;
    ams_mel_status_t status;
    uint32_t timeout_ms;
    unsigned long requested_frames;
    uint8_t *pixels = NULL;
    int result = EXIT_FAILURE;

    if (argc < 3 || argc > 5) {
        fprintf(stderr, "usage: %s PROVIDER_SO PROFILE_JSON [FRAME_COUNT] [TIMEOUT_MS]\n", argv[0]);
        return EXIT_FAILURE;
    }
    requested_frames = argc >= 4 ? strtoul(argv[3], NULL, 10) : 3UL;
    timeout_ms = argc >= 5 ? (uint32_t)strtoul(argv[4], NULL, 10) : UINT32_C(10000);
    if (requested_frames < 3UL || requested_frames > 1000UL || timeout_ms == 0U) {
        fputs("FAIL: frame count must be 3..1000 and timeout must be finite/nonzero\n", stderr);
        return EXIT_FAILURE;
    }

    status = open_session(argv[1], argv[2], &session);
    if (status != AMS_MEL_OK) return failed("open session", status);

    {
        ams_mel_provider_version_v1 version;
        size_t required = 0;
        char vendor[256];
        char description[512];
        memset(&version, 0, sizeof version);
        version.vendor = vendor;
        version.vendor_capacity = sizeof vendor;
        version.description = description;
        version.description_capacity = sizeof description;
        diagnostic[0] = '\0';
        status = ams_mel_session_get_provider_version(session, &version, diagnostic,
                                                       sizeof diagnostic, &required);
        if (status != AMS_MEL_OK) {
            (void)failed("query provider version", status);
            goto cleanup;
        }
        printf("provider version: api=%" PRIu32 " library=%" PRIu32
               " vendor=%s description=%s\n", version.api_version,
               version.library_version, vendor, description);
    }

    {
        ams_mel_ir_stream_config_v1 config;
        memset(&config, 0, sizeof config);
        config.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
        fill_ids(&config.channel_id, &config.platform_id, "Task 004 IRSTImage");
        fill_location(&config.sensor_location);
        config.buffer_count = 4U;
        config.buffer_size = MAX_FRAME_BYTES;
        config.queue_capacity = 8U;
        diagnostic[0] = '\0';
        status = ams_mel_ir_stream_open(session, &config, &stream, diagnostic,
                                        sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("open image stream", status); goto cleanup; }
        status = ams_mel_ir_stream_start(stream, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("start image stream", status); goto cleanup; }
    }

    {
        ams_mel_ir_c2_config_v1 config;
        ams_mel_ir_mode_result_v1 mode;
        memset(&config, 0, sizeof config);
        config.channel_type = AMS_MEL_IR_CHANNEL_COMMAND_AND_CONTROL;
        fill_ids(&config.channel_id, &config.platform_id, "Task 004 IR C2");
        fill_location(&config.sensor_location);
        status = ams_mel_ir_c2_open(session, &config, &c2, diagnostic,
                                    sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("open C2", status); goto cleanup; }
        status = ams_mel_ir_c2_enable(c2, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("enable C2", status); goto cleanup; }
        status = ams_mel_ir_c2_submit_operate(c2, UINT32_C(0x00400401), &request,
                                               diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("submit Operate/TaskSched", status); goto cleanup; }
        memset(&mode, 0, sizeof mode);
        status = ams_mel_ir_mode_request_wait(request, 5000U, &mode, diagnostic,
                                               sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("wait for Operate", status); goto cleanup; }
        if (mode.mode != AMS_MEL_IR_MFA_MODE_TASK_SCHED) {
            fprintf(stderr, "FAIL: C2 returned unexpected mode=%" PRIu32 "\n", mode.mode);
            goto cleanup;
        }
        puts("C2 result: TASK_SCHED");

        /* Children and the completed request must retain provider state. */
        status = ams_mel_session_close(&session, diagnostic, sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("close parent session", status); goto cleanup; }
        memset(&mode, 0, sizeof mode);
        status = ams_mel_ir_mode_request_wait(request, 0U, &mode, diagnostic,
                                               sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK || mode.mode != AMS_MEL_IR_MFA_MODE_TASK_SCHED) {
            (void)failed("inspect completed request after parent close", status);
            goto cleanup;
        }
    }

    pixels = malloc(MAX_FRAME_BYTES);
    if (pixels == NULL) { fputs("FAIL: allocate frame buffer\n", stderr); goto cleanup; }
    {
        uint32_t previous_id = 0;
        for (unsigned long index = 0; index < requested_frames; ++index) {
            ams_mel_ir_frame_v1 frame;
            size_t expected;
            memset(&frame, 0, sizeof frame);
            frame.pixels = pixels;
            frame.pixel_capacity = MAX_FRAME_BYTES;
            diagnostic[0] = '\0';
            status = ams_mel_ir_stream_receive(stream, timeout_ms, &frame, diagnostic,
                                                sizeof diagnostic, NULL);
            if (status != AMS_MEL_OK) {
                (void)failed("receive frame", status);
                print_counters(stream);
                goto cleanup;
            }
            if (frame.width == 0U || frame.height == 0U ||
                frame.bits_per_pixel != 8U || frame.number_of_bands != 1U ||
                frame.pixel_format != AMS_MEL_IR_PIXEL_MONO ||
                (size_t)frame.width > SIZE_MAX / (size_t)frame.height) {
                fputs("FAIL: malformed or unsupported frame profile\n", stderr);
                goto cleanup;
            }
            expected = (size_t)frame.width * (size_t)frame.height;
            if (expected != frame.pixel_required || expected > frame.pixel_capacity ||
                (index > 0UL && frame.frame_id <= previous_id)) {
                fputs("FAIL: incomplete frame or non-monotonic frame ID\n", stderr);
                goto cleanup;
            }
            previous_id = frame.frame_id;
            printf("frame %lu: id=%" PRIu32 " geometry=%" PRIu32 "x%" PRIu32
                   " bytes=%zu checksum=%016" PRIx64 "\n", index + 1UL,
                   frame.frame_id, frame.width, frame.height, expected,
                   checksum(pixels, expected));
        }
    }

    {
        ams_mel_ir_stream_counters_v1 counters;
        status = ams_mel_ir_stream_get_counters(stream, &counters, diagnostic,
                                                 sizeof diagnostic, NULL);
        if (status != AMS_MEL_OK) { (void)failed("query stream counters", status); goto cleanup; }
        printf("counters: received=%" PRIu64 " dropped=%" PRIu64 " malformed=%" PRIu64 "\n",
               counters.frames_received, counters.frames_dropped_queue_full,
               counters.malformed_or_unsupported_frames);
        if (counters.frames_received < requested_frames ||
            counters.malformed_or_unsupported_frames != 0U) {
            fputs("FAIL: stream counters violate integration contract\n", stderr);
            goto cleanup;
        }
    }
    result = EXIT_SUCCESS;

cleanup:
    free(pixels);
    if (request != NULL && ams_mel_ir_mode_request_close(&request, diagnostic,
                                                          sizeof diagnostic, NULL) != AMS_MEL_OK)
        result = EXIT_FAILURE;
    if (c2 != NULL && ams_mel_ir_c2_close(&c2, diagnostic, sizeof diagnostic, NULL) != AMS_MEL_OK)
        result = EXIT_FAILURE;
    if (stream != NULL && ams_mel_ir_stream_close(&stream, diagnostic,
                                                   sizeof diagnostic, NULL) != AMS_MEL_OK)
        result = EXIT_FAILURE;
    if (session != NULL && ams_mel_session_close(&session, diagnostic,
                                                  sizeof diagnostic, NULL) != AMS_MEL_OK)
        result = EXIT_FAILURE;
    if (result == EXIT_SUCCESS) puts("PASS: real Squall IR C integration");
    return result;
}
