#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v14, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v14, signature: 1, default: 0), "v14 by-value span");
int main(void) {
    printf("%zu %zu ", sizeof(ams_mel_rf_complex_i8_v1), _Alignof(ams_mel_rf_complex_i8_v1));
    printf("%zu ", offsetof(ams_mel_rf_complex_i8_v1, real));
    printf("%zu ", offsetof(ams_mel_rf_complex_i8_v1, imag));
    printf("%zu %zu ", sizeof(ams_mel_rf_complex_i8_span_v1), _Alignof(ams_mel_rf_complex_i8_span_v1));
    printf("%zu ", offsetof(ams_mel_rf_complex_i8_span_v1, data));
    printf("%zu ", offsetof(ams_mel_rf_complex_i8_span_v1, size));
    printf("%zu %zu ", sizeof(ams_mel_rf_waveform_buffer_config_v1), _Alignof(ams_mel_rf_waveform_buffer_config_v1));
    printf("%zu ", offsetof(ams_mel_rf_waveform_buffer_config_v1, data_format));
    printf("%zu ", offsetof(ams_mel_rf_waveform_buffer_config_v1, samples_i8));
    printf("%zu ", offsetof(ams_mel_rf_waveform_buffer_config_v1, samples_i16));
    printf("%zu %zu ", sizeof(ams_mel_rf_modulation_config_v4), _Alignof(ams_mel_rf_modulation_config_v4));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_v4, modulation));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_v4, waveform_buffer));
    printf("%zu %zu ", sizeof(ams_mel_rf_modulation_config_span_v4), _Alignof(ams_mel_rf_modulation_config_span_v4));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_span_v4, data));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_span_v4, size));
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_v14), _Alignof(ams_mel_rf_job_interval_config_v14));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v14, interval));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v14, has_transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v14, transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v14, has_modulations));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v14, modulations));
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_span_v14), _Alignof(ams_mel_rf_job_interval_config_span_v14));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v14, data));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v14, size));
    puts("");
    return 0;
}
