#define _POSIX_C_SOURCE 200809L
#define main inherited_regression
#include "test_rf_job_product_stream_params.c"
#undef main
#include <unistd.h>

static uint64_t stream_value(unsigned i, unsigned m, unsigned field)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    uint64_t (*fn)(unsigned, unsigned, unsigned);
    CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f16_value");
    CHECK(fn);
    uint64_t result = fn(i, m, field);
    CHECK(dlclose(lib) == 0);
    return result;
}

static int64_t context_value(unsigned i, unsigned operation, unsigned n)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int64_t (*fn)(unsigned, unsigned, unsigned);
    CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f17_value");
    CHECK(fn);
    int64_t result = fn(i, operation, n);
    CHECK(dlclose(lib) == 0);
    return result;
}
static void observe_context(unsigned i, const uint8_t *bytes, size_t size)
{
    CHECK(context_value(i, 0, 0) == (int64_t)size);
    for (size_t n = 0; n < size; ++n) CHECK(context_value(i, 1, (unsigned)n) == bytes[n]);
}

static int mixed_fixture(void)
{
    capabilities[0].uuid[1] = 0x80;
    capabilities[0].uuid[2] = 0xff;
    capabilities[0].descriptive_label = (ams_mel_string_view_v1){"first", 5};
    capabilities[1].uuid[0] = 0xff;
    capabilities[2].uuid[15] = 0x80;
    capabilities[2].descriptive_label = (ams_mel_string_view_v1){"µ-third", 8};
    for (unsigned iteration = 0; iteration < 50; ++iteration) {
        ams_mel_rf_c2 *c2 = NULL;
        ams_mel_rf_virtual_aperture *va = NULL;
        ams_mel_rf_job *job = claimed("c2:job-ok", &c2, &va);
        ams_mel_rf_modulation_config_v5 tables[2][10] = {0};
        ams_mel_rf_job_interval_config_v16 intervals[2] = {0};
        ams_mel_rf_tx_event_config_v3 events[2] = {0};
        ams_mel_rf_complex_i8_v1 samples[] = {{1, -2}};
        ams_mel_rf_waveform_profile_params_v1 profile = {0};
        profile.chip_duration_femtoseconds = 37;
        profile.phase_deg = -23.5;
        profile.update_phase = 1;
        ams_mel_rf_stokes_vector_v1 stokes = {1, -2, 3, -4};
        ams_mel_rf_receive_event_config_v4 rx = {0};
        rx.event.polarization = (ams_mel_rf_stokes_vector_span_v1){&stokes, 1};
        rx.event.phase_offset_rad = 3.25;
        intervals[0].interval.interval.interval.interval.receive_events =
            (ams_mel_rf_receive_event_config_span_v4){&rx, 1};
        int32_t references[2][5] = {{0, 1, 0, -1, 2}, {2, 1, 2, 0, -1}};
        for (unsigned i = 0; i < 2; ++i) {
            intervals[i].has_modulations = 1;
            intervals[i].modulations = (ams_mel_rf_modulation_config_span_v5){tables[i], 10};
            intervals[i].has_transmit_events = 1;
            intervals[i].transmit_events = (ams_mel_rf_tx_event_config_span_v3){&events[i], 1};
            events[i].has_modulation_indices = 1;
            events[i].event.polarization = (ams_mel_rf_stokes_vector_span_v1){&stokes, 1};
            events[i].event.phase_offset_rad = -0.75;
            events[i].modulation_indices = (ams_mel_i32_span_v1){references[i], 5};
            for (unsigned m = 0; m < 3; ++m) {
                tables[i][m].modulation.modulation.modulation.modulation.kind = 7;
                tables[i][m].waveform_stream = (ams_mel_rf_waveform_stream_config_v1){
                    UINT64_MAX - i * 100 - m, UINT64_MAX - i * 300 - m * 7,
                    m == 0 ? 0 : m == 1 ? 1 : SIZE_MAX, m, i * 3 + m, i};
                tables[i][m].modulation.waveform_buffer.data_format = UINT32_MAX;
                tables[i][m].modulation.waveform_buffer.samples_i8 =
                    (ams_mel_rf_complex_i8_span_v1){(void *)1, SIZE_MAX};
                tables[i][m].modulation.modulation.modulation.modulation.phase_chips_rad =
                    (ams_mel_double_span_v1){(void *)1, SIZE_MAX};
                tables[i][m].modulation.modulation.modulation.binary_data =
                    (ams_mel_u32_span_v1){(void *)1, SIZE_MAX};
            }
            for (unsigned m = 3; m < 10; ++m) {
                tables[i][m].modulation.modulation.modulation.modulation.kind = m - 3;
                tables[i][m].waveform_stream.sample_format = UINT32_MAX;
                tables[i][m].waveform_stream.item_format = UINT32_MAX;
                tables[i][m].waveform_stream.repeat_mode = UINT32_MAX;
            }
            tables[i][9].modulation.waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
            tables[i][9].modulation.waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){samples, 1};
            tables[i][8].modulation.modulation.custom_waveform.profiles =
                (ams_mel_rf_waveform_profile_params_span_v1){&profile, 1};
        }
        uint8_t context[] = {0, 0x80, 0xff, 1, 0};
        uint8_t activity[] = {19, 0, 31, 255};
        intervals[0].has_user_context = 1;
        intervals[0].user_context = (ams_mel_u8_span_v1){context, sizeof context};
        intervals[0].interval.interval.interval.interval.activity_id = (ams_mel_u8_span_v1){activity, sizeof activity};
        intervals[1].user_context = (ams_mel_u8_span_v1){(void *)1, SIZE_MAX};
        ams_mel_rf_job_interval_config_span_v16 span = {intervals, 2};
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        observe_context(0, context, sizeof context);
        CHECK(context_value(0, 2, 0) == sizeof activity);
        for (unsigned n = 0; n < sizeof activity; ++n) CHECK(context_value(0, 3, n) == activity[n]);
        CHECK(context_value(0, 6, 0) == 1 && context_value(1, 6, 0) == 0);
        CHECK(context_value(0, 7, 0) == 1 && context_value(1, 7, 0) == 1);
        CHECK(context_value(0, 20, 9) == 1 && context_value(1, 20, 9) == 1);
        CHECK(context_value(0, 21, 8) == 1 && context_value(1, 21, 8) == 1);
        CHECK(context_value(0, 23, 0) == 1 && context_value(0, 24, 0) == 1);
        for (unsigned i = 0; i < 2; ++i) {
            CHECK(stream_value(i, 0, 7) == 5);
            for (unsigned k = 0; k < 5; ++k)
                CHECK(stream_value(i, 0, 8 + k) == (uint64_t)(int64_t)references[i][k]);
        }
        for (unsigned i = 0; i < 2; ++i) for (unsigned m = 0; m < 3; ++m) {
            const ams_mel_rf_waveform_stream_config_v1 v = tables[i][m].waveform_stream;
            CHECK(stream_value(i, m, 0) == 1);
            CHECK(stream_value(i, m, 1) == v.endpoint_id);
            CHECK(stream_value(i, m, 2) == v.start_address);
            CHECK(stream_value(i, m, 3) == v.num_bytes);
            CHECK(stream_value(i, m, 4) == v.sample_format);
            CHECK(stream_value(i, m, 5) == v.item_format);
            CHECK(stream_value(i, m, 6) == v.repeat_mode);
        }
        for (unsigned item = 0; item < 9; ++item) {
            tables[0][0].waveform_stream.item_format = item;
            CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
            CHECK(stream_value(0, 0, 5) == item);
        }
        tables[0][1].waveform_stream.endpoint_id = 0;
        tables[0][1].waveform_stream.start_address = 0;
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        CHECK(stream_value(0, 1, 1) == 0 && stream_value(0, 1, 2) == 0);
        uint32_t *fields[] = {&tables[0][0].waveform_stream.sample_format,
            &tables[0][0].waveform_stream.item_format, &tables[0][0].waveform_stream.repeat_mode};
        const uint32_t invalid[] = {3, 9, 2};
        const unsigned before = mock("mock_rf_job_add_calls");
        for (unsigned f = 0; f < 3; ++f) {
            uint32_t saved = *fields[f];
            for (unsigned k = 0; k < 2; ++k) {
                *fields[f] = k ? UINT32_MAX : invalid[f];
                CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
                CHECK(mock("mock_rf_job_add_calls") == before);
                CHECK(stream_value(0, 0, 5) == 8);
            }
            *fields[f] = saved;
        }
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE", "later-stream-definition", 1) == 0);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls") == before);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE") == 0);
        intervals[1].has_modulations = 2;
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        intervals[1].has_modulations = 1;
        intervals[1].has_transmit_events = 2;
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        intervals[1].has_transmit_events = 1;
        references[1][0] = 10;
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        references[1][0] = 2;
        tables[1][2].modulation.modulation.modulation.modulation.kind = 8;
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        tables[1][2].modulation.modulation.modulation.modulation.kind = 7;
        const ams_mel_rf_modulation_config_span_v5 saved = intervals[1].modulations;
        intervals[1].modulations = (ams_mel_rf_modulation_config_span_v5){NULL, 1};
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        intervals[1].modulations = saved;
        CHECK(mock("mock_rf_job_add_calls") == before);
        CHECK(stream_value(0, 0, 5) == 8);
        observe_context(0, context, sizeof context);
        ams_mel_rf_modulation_config_v4 old = tables[0][0].modulation;
        ams_mel_rf_job_interval_config_v14 historical = {0};
        historical.has_modulations = 1;
        historical.modulations = (ams_mel_rf_modulation_config_span_v4){&old, 1};
        CHECK(ams_mel_rf_job_add_intervals_v14(job,
            (ams_mel_rf_job_interval_config_span_v14){&historical, 1}, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls") == before);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_job_close(&job, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2, D) == AMS_MEL_OK);
    }
    puts("PASS: C mixed v16 legacy descriptors 50/50");
    return 0;
}

static unsigned trace_count(const char *path, const char *text)
{
    FILE *file = fopen(path, "r");
    char line[256];
    unsigned count = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) if (strstr(line, text)) ++count;
    CHECK(fclose(file) == 0);
    return count;
}

int main(void)
{
    CHECK(mixed_fixture() == 0);
    for (unsigned repeat = 0; repeat < 50; ++repeat) {
        ams_mel_rf_c2 *c2 = NULL;
        ams_mel_rf_virtual_aperture *va = NULL;
        ams_mel_rf_job *job = claimed("c2:job-ok", &c2, &va);
        uint8_t original[] = {0, 0x80, 0xff, 1, 0};
        uint8_t longer[257];
        for (unsigned n = 0; n < sizeof longer; ++n) longer[n] = (uint8_t)((n * 37 + n / 3) % 256);
        uint8_t *borrowed = malloc(sizeof original);
        CHECK(borrowed);
        memcpy(borrowed, original, sizeof original);
        ams_mel_rf_job_interval_config_v16 intervals[5] = {0};
        for (unsigned i = 0; i < 5; ++i) intervals[i].interval.interval.interval.interval.interval_id = 100 + i;
        intervals[0].has_user_context = 1;
        intervals[0].user_context = (ams_mel_u8_span_v1){borrowed, sizeof original};
        intervals[1].has_user_context = 1;
        intervals[1].user_context = intervals[0].user_context;
        intervals[2].has_user_context = 1; /* explicitly empty */
        intervals[3].has_user_context = 1;
        intervals[3].user_context = (ams_mel_u8_span_v1){longer, sizeof longer};
        intervals[4].user_context = (ams_mel_u8_span_v1){(void *)1, SIZE_MAX};
        ams_mel_rf_job_interval_config_span_v16 span = {intervals, 5};
        char trace[4096];
        CHECK(snprintf(trace, sizeof trace, "%s/context-trace-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") > 0);
        int fd = mkstemp(trace);
        CHECK(fd >= 0 && close(fd) == 0);
        CHECK(setenv("AMS_MEL_TEST_RF_TX_TRACE", trace, 1) == 0);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        CHECK(trace_count(trace, "setUserDefinedContextData") == 4);
        memset(borrowed, 23, sizeof original);
        free(borrowed);
        intervals[0].user_context.data = original;
        intervals[1].user_context.data = original;
        observe_context(0, original, sizeof original);
        observe_context(1, original, sizeof original);
        observe_context(2, NULL, 0);
        observe_context(3, longer, sizeof longer);
        CHECK(context_value(0, 4, 0) == 1);
        observe_context(0, original, sizeof original);
        CHECK(context_value(0, 9, 1) == 1);
        CHECK(context_value(0, 8, 0) == 1);
        observe_context(1, original, sizeof original);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        observe_context(0, original, sizeof original);
        const unsigned before = mock("mock_rf_job_add_calls");
        const uint32_t invalid[] = {2, UINT32_MAX};
        for (unsigned n = 0; n < 2; ++n) {
            intervals[3].has_user_context = invalid[n];
            CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        }
        intervals[3].has_user_context = 1;
        const ams_mel_u8_span_v1 malformed[] = {{NULL, 1}, {(void *)1, SIZE_MAX}, {(void *)1, SIZE_MAX / 2 + 1}};
        for (unsigned n = 0; n < 3; ++n) {
            intervals[3].user_context = malformed[n];
            CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        }
        intervals[3].user_context = (ams_mel_u8_span_v1){longer, sizeof longer};
        const char *failures[] = {"later-context-prepared", "later-context-copy"};
        for (unsigned n = 0; n < 2; ++n) {
            CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE", failures[n], 1) == 0);
            CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_INTERNAL_ERROR);
            CHECK(trace_count(trace, n ? "partial copy reached" : "preceding context complete") == 1);
            CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE") == 0);
        }
        CHECK(trace_count(trace, "preparation-unwound") == 2);
        CHECK(mock("mock_rf_job_add_calls") == before);
        for (unsigned i = 0; i < 5; ++i) CHECK(context_value(i, 5, 0) == 100 + i);
        observe_context(0, original, sizeof original);
        CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE", "std", 1) == 0);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(mock("mock_rf_job_add_calls") == before + 1);
        observe_context(0, original, sizeof original);
        CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE") == 0);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        observe_context(3, longer, sizeof longer);
        CHECK(ams_mel_rf_job_add_intervals_v16(job, span, D) == AMS_MEL_OK);
        observe_context(0, original, sizeof original);
        /* Older profiles never enter the context setter. */
        unsigned setters = trace_count(trace, "setUserDefinedContextData");
        ams_mel_rf_job_interval_config_v15 old = {0};
        CHECK(ams_mel_rf_job_add_intervals_v15(job,
            (ams_mel_rf_job_interval_config_span_v15){&old, 1}, D) == AMS_MEL_OK);
        CHECK(trace_count(trace, "setUserDefinedContextData") == setters);
        intervals[0].has_user_context = 0;
        intervals[0].user_context = (ams_mel_u8_span_v1){(void *)1, SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v16(job,
            (ams_mel_rf_job_interval_config_span_v16){intervals, 1}, D) == AMS_MEL_OK);
        CHECK(trace_count(trace, "setUserDefinedContextData") == setters);
        CHECK(unsetenv("AMS_MEL_TEST_RF_TX_TRACE") == 0);
        CHECK(remove(trace) == 0);
        no_queries();
        CHECK(ams_mel_rf_job_close(&job, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_c2_close(&c2, D) == AMS_MEL_OK);
    }
    puts("PASS: C copied binary context and preparation recovery 50/50");
    return 0;
}
