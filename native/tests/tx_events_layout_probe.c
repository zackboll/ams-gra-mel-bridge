#include <ams_mel/abi.h>
#include <stdio.h>
#include <stddef.h>
int main(void) {
printf("%zu %zu ", sizeof(ams_mel_rf_tx_event_config_v1), _Alignof(ams_mel_rf_tx_event_config_v1));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, event_id));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, element_group_label));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, start_femtoseconds));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, duration_femtoseconds));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, center_frequency_hz));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, stab_point_index));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, applicable_tx_element_groups));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, tx_attenuation_db));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, rise_duration_femtoseconds));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_v1, fall_duration_femtoseconds));
printf("%zu %zu ", sizeof(ams_mel_rf_tx_event_config_span_v1), _Alignof(ams_mel_rf_tx_event_config_span_v1));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_span_v1, data));
printf("%zu ", offsetof(ams_mel_rf_tx_event_config_span_v1, size));
printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_v9), _Alignof(ams_mel_rf_job_interval_config_v9));
printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v9, interval));
printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v9, has_transmit_events));
printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v9, transmit_events));
printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_span_v9), _Alignof(ams_mel_rf_job_interval_config_span_v9));
printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v9, data));
printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v9, size));
puts(""); return 0;
}
