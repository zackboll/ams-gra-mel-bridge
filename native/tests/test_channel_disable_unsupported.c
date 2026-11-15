#include <ams_mel/abi.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define ARGS NULL, 0, NULL
#define CALL(fn, ...) CHECK(fn(__VA_ARGS__, ARGS) == AMS_MEL_OK)
int main(void)
{
    ams_mel_session *session = NULL;
    ams_mel_ir_instrumentation *instrumentation = NULL;
    ams_mel_ir_track *track = NULL;
    ams_mel_ir_scheduling *scheduling = NULL;
    ams_mel_ir_stream *image = NULL;
    ams_mel_ir_instrumentation_config_v1 ic = {0};
    ams_mel_ir_track_config_v1 tc = {0};
    ams_mel_ir_scheduling_config_v1 sc = {0};
    ams_mel_ir_stream_config_v1 ac = {0};
    ams_mel_ir_channel *views[4] = {NULL, NULL, NULL, NULL};
    ams_mel_ir_return_t result;
    ams_mel_ir_buffer *buffer = NULL;
    ic.channel_type = AMS_MEL_IR_CHANNEL_INSTRUMENTATION;
    tc.channel_type = AMS_MEL_IR_CHANNEL_IRST_TRACK;
    sc.channel_type = AMS_MEL_IR_CHANNEL_SCHEDULING;
    ac.channel_type = AMS_MEL_IR_CHANNEL_IRST_IMAGE;
    ac.buffer_count = 3; ac.buffer_size = 1024; ac.queue_capacity = 4;
    CALL(ams_mel_session_open, AMS_MEL_TEST_MOCK_PROVIDER, "sched-ada-common", "", &session);
    CALL(ams_mel_ir_instrumentation_open, session, &ic, &instrumentation);
    CALL(ams_mel_ir_track_open, session, &tc, &track);
    CALL(ams_mel_ir_scheduling_open, session, &sc, &scheduling);
    CALL(ams_mel_ir_stream_open, session, &ac, &image);
    CALL(ams_mel_ir_channel_from_instrumentation, instrumentation, &views[0]);
    CALL(ams_mel_ir_channel_from_track, track, &views[1]);
    CALL(ams_mel_ir_channel_from_scheduling, scheduling, &views[2]);
    CALL(ams_mel_ir_channel_from_stream, image, &views[3]);
    CALL(ams_mel_ir_buffer_open, session, 16, -20, &buffer, &result);
    for (unsigned pass = 0; pass < 2; ++pass) {
        for (unsigned i = 0; i < 4; ++i) {
            CHECK(ams_mel_ir_channel_disable(views[i], &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(result == UINT32_MAX);
            CHECK(ams_mel_ir_channel_register_buffer(views[i], buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(result == UINT32_MAX);
            CHECK(ams_mel_ir_channel_unregister_buffer(views[i], buffer, &result, ARGS) == AMS_MEL_PROVIDER_FAILED);
            CHECK(result == UINT32_MAX);
        }
        if (!pass) {
            CALL(ams_mel_ir_instrumentation_enable, instrumentation);
            CALL(ams_mel_ir_track_enable, track);
            CALL(ams_mel_ir_scheduling_enable, scheduling);
            CALL(ams_mel_ir_stream_start, image);
        }
    }
    CALL(ams_mel_ir_instrumentation_close, &instrumentation);
    CALL(ams_mel_ir_track_close, &track);
    CALL(ams_mel_ir_scheduling_close, &scheduling);
    CALL(ams_mel_ir_stream_stop, image);
    CHECK(ams_mel_ir_stream_start(image, ARGS) == AMS_MEL_STREAM_STOPPED);
    CALL(ams_mel_ir_stream_close, &image);
    CALL(ams_mel_ir_buffer_close, &buffer);
    CALL(ams_mel_session_close, &session);
    for (unsigned i = 0; i < 4; ++i) CALL(ams_mel_ir_channel_close, &views[i]);
    return 0;
}
