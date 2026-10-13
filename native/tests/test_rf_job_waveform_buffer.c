#define _POSIX_C_SOURCE 200809L
#define main inherited_regression
#include "test_rf_job_product_stream_params.c"
#undef main
extern size_t ams_mel_test_sample_preparation(unsigned operation);

static int64_t sample_value(unsigned i, unsigned m, unsigned s, unsigned field)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int64_t (*fn)(unsigned, unsigned, unsigned, unsigned);
    CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f15_value");
    CHECK(fn);
    const int64_t result = fn(i, m, s, field);
    CHECK(dlclose(lib) == 0);
    return result;
}

static int64_t owner_operation(unsigned operation, unsigned m, unsigned s, int value)
{
    void *lib = dlopen(AMS_MEL_TEST_MOCK_RF_PROVIDER, RTLD_NOW | RTLD_LOCAL);
    int64_t (*fn)(unsigned, unsigned, unsigned, unsigned, int);
    CHECK(lib);
    *(void **)(&fn) = dlsym(lib, "mock_rf_f15_owner");
    CHECK(fn);
    const int64_t result = fn(operation, 0, m, s, value);
    CHECK(dlclose(lib) == 0);
    return result;
}
int main(void)
{
    capabilities[0].uuid[1] = 0x80;
    capabilities[0].uuid[2] = 0xff;
    capabilities[0].descriptive_label = (ams_mel_string_view_v1){"first", 5};
    capabilities[1].uuid[0] = 0xff;
    capabilities[2].uuid[15] = 0x80;
    capabilities[2].descriptive_label = (ams_mel_string_view_v1){"µ-third", 8};
    for (unsigned repeat = 0; repeat < 50; ++repeat) {
        ams_mel_rf_c2 *c2 = NULL;
        ams_mel_rf_virtual_aperture *va = NULL;
        ams_mel_rf_job *job = claimed("c2:job-ok", &c2, &va);
        ams_mel_rf_complex_i8_v1 i8[] = {{INT8_MIN, INT8_MAX}, {-7, 3}, {0, -9}};
        ams_mel_rf_complex_i16_v1 i16[] = {{INT16_MAX, INT16_MIN}, {123, -456}};
        ams_mel_rf_modulation_config_v4 table[5] = {0};
        ams_mel_rf_modulation_config_v4 second_table[2] = {0};
        ams_mel_rf_job_interval_config_v14 intervals[2] = {0};
        intervals[0].has_modulations = 1;
        intervals[0].modulations = (ams_mel_rf_modulation_config_span_v4){table, 5};
        for (unsigned m = 0; m < 3; ++m) {
            table[m].modulation.modulation.modulation.kind = 6;
            table[m].modulation.modulation.modulation.phase_chips_rad =
                (ams_mel_double_span_v1){(void *)1, SIZE_MAX};
            table[m].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
            table[m].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){i8, 3};
            table[m].waveform_buffer.samples_i16 = (ams_mel_rf_complex_i16_span_v1){(void *)1, SIZE_MAX};
        }
        table[1].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
        table[1].waveform_buffer.samples_i16 = (ams_mel_rf_complex_i16_span_v1){i16, 2};
        table[1].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){(void *)1, SIZE_MAX};
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){NULL, 0};
        table[3].modulation.modulation.modulation.kind = 5;
        table[4].modulation.modulation.modulation.kind = 1;
        double phase[] = {-0.25, 1.5};
        table[4].modulation.modulation.modulation.phase_chips_rad = (ams_mel_double_span_v1){phase, 2};
        table[4].modulation.modulation.modulation.chip_duration_femtoseconds = -37;
        for (unsigned m = 3; m < 5; ++m) {
            table[m].waveform_buffer.data_format = UINT32_MAX;
            table[m].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){(void *)1, SIZE_MAX};
            table[m].waveform_buffer.samples_i16 = (ams_mel_rf_complex_i16_span_v1){(void *)1, SIZE_MAX};
        }
        second_table[0] = table[1];
        second_table[1] = table[0];
        second_table[1].waveform_buffer.samples_i8.size = 1;
        intervals[1].has_modulations = 1;
        intervals[1].modulations = (ams_mel_rf_modulation_config_span_v4){second_table, 2};
        ams_mel_rf_stokes_vector_v1 stokes = {1, -2, 3, -4};
        ams_mel_rf_receive_event_config_v4 rx = {0};
        rx.event.polarization = (ams_mel_rf_stokes_vector_span_v1){&stokes, 1};
        rx.event.phase_offset_rad = 3.25;
        intervals[0].interval.interval.interval.interval.receive_events =
            (ams_mel_rf_receive_event_config_span_v4){&rx, 1};
        ams_mel_rf_tx_event_config_v3 events[2] = {0};
        int32_t refs0[] = {2, 0, -1, 4, 0, 3, 1};
        int32_t refs1[] = {1, 0, 1};
        for (unsigned i = 0; i < 2; ++i) {
            intervals[i].has_transmit_events = 1;
            intervals[i].transmit_events = (ams_mel_rf_tx_event_config_span_v3){events + i, 1};
            events[i].has_modulation_indices = 1;
            events[i].modulation_indices = (ams_mel_i32_span_v1){i ? refs1 : refs0, i ? 3 : 7};
            events[i].event.event.event_id = 91 + i;
            events[i].event.polarization = (ams_mel_rf_stokes_vector_span_v1){&stokes, 1};
            events[i].event.phase_offset_rad = -0.75;
        }
        ams_mel_rf_job_interval_config_span_v14 span = {intervals, 2};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(sample_value(0, 0, 0, 8) == 7);
        CHECK(sample_value(1, 0, 0, 8) == 3);
        for (unsigned s = 0; s < 7; ++s) CHECK(sample_value(0, 0, s, 9) == refs0[s]);
        for (unsigned s = 0; s < 3; ++s) CHECK(sample_value(1, 0, s, 9) == refs1[s]);
        CHECK(sample_value(0, 0, 0, 10) == 91);
        CHECK(sample_value(1, 0, 0, 10) == 92);
        CHECK(sample_value(0, 3, 0, 11) == 1);
        CHECK(sample_value(0, 4, 0, 12) == -37);
        CHECK(sample_value(0, 0, 0, 13) == 1);
        CHECK(sample_value(0, 0, 0, 14) == 1);
        for (unsigned m = 0; m < 3; ++m) {
            CHECK(sample_value(0, m, 0, 0) == 1);
            CHECK(sample_value(0, m, 0, 1) == 1);
            CHECK(sample_value(0, m, 0, 2) == 1);
            CHECK(sample_value(0, m, 0, 3) == (m == 1 ? 16 : 8));
            CHECK(sample_value(0, m, 0, 4) == 1);
            CHECK(sample_value(0, m, 0, 5) == (m == 0 ? 3 : m == 1 ? 2 : 0));
        }
        for (unsigned s = 0; s < 3; ++s) {
            CHECK(sample_value(0, 0, s, 6) == i8[s].real);
            CHECK(sample_value(0, 0, s, 7) == i8[s].imag);
        }
        for (unsigned s = 0; s < 2; ++s) {
            CHECK(sample_value(0, 1, s, 6) == i16[s].real);
            CHECK(sample_value(0, 1, s, 7) == i16[s].imag);
        }
        i8[0].real = 9;
        i16[0].imag = 8;
        CHECK(sample_value(0, 0, 0, 6) == INT8_MIN);
        CHECK(sample_value(0, 1, 0, 7) == INT16_MIN);
        CHECK(sample_value(1, 0, 0, 3) == 16);
        CHECK(sample_value(1, 1, 0, 5) == 1);
        /* Later malformed interval/event cannot partially submit the batch. */
        unsigned rejected_before = mock("mock_rf_job_add_calls");
        second_table[1].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){NULL, 1};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls") == rejected_before);
        CHECK(sample_value(0, 0, 0, 6) == INT8_MIN);
        second_table[1].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){i8, 1};
        refs1[2] = 2;
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls") == rejected_before);
        refs1[2] = -2;
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        refs1[2] = 1;
        intervals[1].has_modulations = 2;
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        intervals[1].has_modulations = 1;
        CHECK(ams_mel_rf_job_add_intervals_v14(job,
            (ams_mel_rf_job_interval_config_span_v14){(void *)1, SIZE_MAX}, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls") == rejected_before);
        const unsigned before = mock("mock_rf_job_add_calls");
        table[2].waveform_buffer.data_format = UINT32_MAX;
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        CHECK(mock("mock_rf_job_add_calls") == before);
        CHECK(sample_value(0, 0, 0, 6) == INT8_MIN);
        table[2].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        const uint32_t unsupported[] = {0, 1, 4, 5, 6, 7, UINT32_MAX};
        for (unsigned f = 0; f < sizeof unsupported / sizeof unsupported[0]; ++f) {
            table[2].waveform_buffer.data_format = unsupported[f];
            CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
            CHECK(mock("mock_rf_job_add_calls") == before);
        }
        table[2].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){NULL, 1};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){(void *)1, SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){NULL, 0};
        table[2].waveform_buffer.samples_i8 =
            (ams_mel_rf_complex_i8_span_v1){(void *)1, (size_t)PTRDIFF_MAX / 2 + 1};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INVALID_ARGUMENT);
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){NULL, 0};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(sample_value(0, 0, 0, 6) == 9);
        /* Two definitions aliasing the same caller array are independent. */
        table[2].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){i8, 3};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(owner_operation(0, 0, 0, 0) == 1);
        CHECK(owner_operation(0, 1, 0, 0) == 1);
        CHECK(owner_operation(1, 0, 0, -31) == 1);
        CHECK(sample_value(0, 0, 0, 6) == -31);
        CHECK(sample_value(0, 2, 0, 6) == 9);
        CHECK(sample_value(1, 1, 0, 6) == 9);
        /* A later Add cannot reuse the earlier allocation, even for identical input. */
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(sample_value(0, 0, 0, 6) == 9);
        CHECK(owner_operation(2, 0, 0, 0) == -31);
        CHECK(owner_operation(3, 0, 0, 0) == 1);
        CHECK(owner_operation(5, 0, 0, 0) == 0);
        CHECK(owner_operation(6, 0, 0, 0) == 0);
        CHECK(owner_operation(4, 0, 0, 0) == 1);
        CHECK(owner_operation(5, 0, 0, 0) == 1);
        CHECK(owner_operation(6, 0, 0, 0) == 1);
        const unsigned prepared_before = mock("mock_rf_job_add_calls");
        CHECK(ams_mel_test_sample_preparation(0) == 0);
        CHECK(setenv("AMS_MEL_TEST_RF_JOB_FAILURE", "later-sample-vector", 1) == 0);
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_INTERNAL_ERROR);
        CHECK(mock("mock_rf_job_add_calls") == prepared_before);
        CHECK(ams_mel_test_sample_preparation(1) == 2);
        CHECK(ams_mel_test_sample_preparation(2) == 2);
        CHECK(ams_mel_test_sample_preparation(0) == 0);
        CHECK(unsetenv("AMS_MEL_TEST_RF_JOB_FAILURE") == 0);
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(setenv("AMS_MEL_TEST_F4_ADD_FAILURE", "std", 1) == 0);
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_PROVIDER_EXCEPTION);
        CHECK(unsetenv("AMS_MEL_TEST_F4_ADD_FAILURE") == 0);
        CHECK(owner_operation(0, 0, 0, 0) == 1);
        i8[0].real = 19;
        CHECK(owner_operation(2, 0, 0, 0) == 9);
        CHECK(owner_operation(3, 0, 0, 0) == 1);
        CHECK(owner_operation(5, 0, 0, 0) == 0);
        CHECK(owner_operation(4, 0, 0, 0) == 1);
        CHECK(owner_operation(5, 0, 0, 0) == 1);
        /* Destroy caller storage, not just mutate an array still in scope. */
        ams_mel_rf_complex_i8_v1 *temporary = malloc(sizeof(*temporary));
        CHECK(temporary);
        *temporary = (ams_mel_rf_complex_i8_v1){-128, 127};
        const ams_mel_rf_complex_i8_span_v1 saved = table[0].waveform_buffer.samples_i8;
        table[0].waveform_buffer.samples_i8 = (ams_mel_rf_complex_i8_span_v1){temporary, 1};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        free(temporary);
        table[0].waveform_buffer.samples_i8 = saved;
        CHECK(sample_value(0, 0, 0, 6) == -128 && sample_value(0, 0, 0, 7) == 127);
        table[2].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT16;
        table[2].waveform_buffer.samples_i16 = (ams_mel_rf_complex_i16_span_v1){NULL, 0};
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(sample_value(0, 2, 0, 3) == 16 && sample_value(0, 2, 0, 5) == 0);
        CHECK(sample_value(0, 2, 0, 4) == 1);
        table[2].waveform_buffer.data_format = AMS_MEL_RF_JOB_DATA_FORMAT_COMPLEX_INT8;
        ams_mel_rf_job_interval_config_v13 old_interval = {0};
        ams_mel_rf_modulation_config_v3 old_modulation = {0};
        old_modulation.modulation.modulation.kind = 6;
        old_interval.has_modulations = 1;
        old_interval.modulations = (ams_mel_rf_modulation_config_span_v3){&old_modulation, 1};
        CHECK(ams_mel_rf_job_add_intervals_v13(job,
            (ams_mel_rf_job_interval_config_span_v13){&old_interval, 1}, D) == AMS_MEL_INVALID_ARGUMENT);
        old_modulation.modulation.modulation.kind = 0;
        CHECK(ams_mel_rf_job_add_intervals_v13(job,
            (ams_mel_rf_job_interval_config_span_v13){&old_interval, 1}, D) == AMS_MEL_OK);
        CHECK(sample_value(0, 0, 0, 1) == 0);
        /* False presence ignores poisoned payloads; explicit empty differs. */
        ams_mel_rf_job_interval_config_v14 transition = {0};
        transition.modulations = (ams_mel_rf_modulation_config_span_v4){(void *)1, SIZE_MAX};
        transition.transmit_events = (ams_mel_rf_tx_event_config_span_v3){(void *)1, SIZE_MAX};
        CHECK(ams_mel_rf_job_add_intervals_v14(job,
            (ams_mel_rf_job_interval_config_span_v14){&transition, 1}, D) == AMS_MEL_OK);
        transition.has_modulations = transition.has_transmit_events = 1;
        transition.modulations = (ams_mel_rf_modulation_config_span_v4){NULL, 0};
        transition.transmit_events = (ams_mel_rf_tx_event_config_span_v3){NULL, 0};
        CHECK(ams_mel_rf_job_add_intervals_v14(job,
            (ams_mel_rf_job_interval_config_span_v14){&transition, 1}, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        CHECK(owner_operation(0, 0, 0, 0) == 1);
        CHECK(ams_mel_rf_job_flush(job, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_virtual_aperture_close(&va, D) == AMS_MEL_OK);
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_OK);
        ams_mel_rf_job_cancel_result_v1 cancelled = {0};
        CHECK(ams_mel_rf_job_cancel(job, &cancelled, D) == AMS_MEL_OK);
        const unsigned terminal_before = mock("mock_rf_job_add_calls");
        CHECK(ams_mel_rf_job_add_intervals_v14(job, span, D) == AMS_MEL_PROVIDER_FAILED);
        CHECK(mock("mock_rf_job_add_calls") == terminal_before);
        CHECK(owner_operation(3, 0, 0, 0) == 1);
        CHECK(ams_mel_rf_job_close(&job, D) == AMS_MEL_OK);
        CHECK(owner_operation(2, 0, 0, 0) == 19);
        CHECK(owner_operation(5, 0, 0, 0) == 0);
        CHECK(owner_operation(4, 0, 0, 0) == 1);
        CHECK(owner_operation(5, 0, 0, 0) == 1);
        CHECK(ams_mel_rf_c2_close(&c2, D) == AMS_MEL_OK);
    }
    puts("PASS: C supplied Cartesian I8/I16 sample copy-in 50/50");
    return 0;
}
