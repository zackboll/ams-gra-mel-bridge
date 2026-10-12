#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v11, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v11, signature: 1, default: 0), "v11 signature");
int main(void) {
printf("%zu %zu ",sizeof(ams_mel_double_span_v1),_Alignof(ams_mel_double_span_v1));
printf("%zu ",offsetof(ams_mel_double_span_v1,data));
printf("%zu ",offsetof(ams_mel_double_span_v1,size));
printf("%zu %zu ",sizeof(ams_mel_i32_span_v1),_Alignof(ams_mel_i32_span_v1));
printf("%zu ",offsetof(ams_mel_i32_span_v1,data));
printf("%zu ",offsetof(ams_mel_i32_span_v1,size));
printf("%zu %zu ",sizeof(ams_mel_rf_modulation_config_v1),_Alignof(ams_mel_rf_modulation_config_v1));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_v1,kind));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_v1,chip_duration_femtoseconds));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_v1,phase_chips_rad));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_v1,frequency_extent_hz));
printf("%zu %zu ",sizeof(ams_mel_rf_modulation_config_span_v1),_Alignof(ams_mel_rf_modulation_config_span_v1));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_span_v1,data));
printf("%zu ",offsetof(ams_mel_rf_modulation_config_span_v1,size));
printf("%zu %zu ",sizeof(ams_mel_rf_tx_event_config_v3),_Alignof(ams_mel_rf_tx_event_config_v3));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v3,event));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v3,has_modulation_indices));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v3,modulation_indices));
printf("%zu %zu ",sizeof(ams_mel_rf_tx_event_config_span_v3),_Alignof(ams_mel_rf_tx_event_config_span_v3));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_span_v3,data));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_span_v3,size));
printf("%zu %zu ",sizeof(ams_mel_rf_job_interval_config_v11),_Alignof(ams_mel_rf_job_interval_config_v11));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v11,interval));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v11,has_transmit_events));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v11,transmit_events));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v11,has_modulations));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v11,modulations));
printf("%zu %zu ",sizeof(ams_mel_rf_job_interval_config_span_v11),_Alignof(ams_mel_rf_job_interval_config_span_v11));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_span_v11,data));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_span_v11,size));
puts(""); return 0;
}
