#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v15, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v15, signature: 1, default: 0), "v15 by-value span");
int main(void) {
    printf("%zu %zu ", sizeof(ams_mel_rf_waveform_stream_config_v1), _Alignof(ams_mel_rf_waveform_stream_config_v1));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, endpoint_id));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, start_address));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, num_bytes));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, sample_format));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, item_format));
    printf("%zu ", offsetof(ams_mel_rf_waveform_stream_config_v1, repeat_mode));
    printf("%zu %zu ", sizeof(ams_mel_rf_modulation_config_v5), _Alignof(ams_mel_rf_modulation_config_v5));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_v5, modulation));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_v5, waveform_stream));
    printf("%zu %zu ", sizeof(ams_mel_rf_modulation_config_span_v5), _Alignof(ams_mel_rf_modulation_config_span_v5));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_span_v5, data));
    printf("%zu ", offsetof(ams_mel_rf_modulation_config_span_v5, size));
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_v15), _Alignof(ams_mel_rf_job_interval_config_v15));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v15, interval));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v15, has_transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v15, transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v15, has_modulations));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v15, modulations));
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_span_v15), _Alignof(ams_mel_rf_job_interval_config_span_v15));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v15, data));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v15, size));
    puts("");
    return 0;
}
