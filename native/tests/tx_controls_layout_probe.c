#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*add_signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v10, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v10, add_signature: 1, default: 0), "v10 signature");
int main(void) {
printf("%zu %zu ",sizeof(ams_mel_rf_tx_event_config_v2),_Alignof(ams_mel_rf_tx_event_config_v2));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,event));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,polarization));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,polarization_beam_steer_correction));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,phase_offset_rad));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,execution_type));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,termination_type));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,allow_delay_start));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,iteration_hold_count));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,iteration_termination_count));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_v2,channelization_enabled));
printf("%zu %zu ",sizeof(ams_mel_rf_tx_event_config_span_v2),_Alignof(ams_mel_rf_tx_event_config_span_v2));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_span_v2,data));
printf("%zu ",offsetof(ams_mel_rf_tx_event_config_span_v2,size));
printf("%zu %zu ",sizeof(ams_mel_rf_job_interval_config_v10),_Alignof(ams_mel_rf_job_interval_config_v10));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v10,interval));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v10,has_transmit_events));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_v10,transmit_events));
printf("%zu %zu ",sizeof(ams_mel_rf_job_interval_config_span_v10),_Alignof(ams_mel_rf_job_interval_config_span_v10));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_span_v10,data));
printf("%zu ",offsetof(ams_mel_rf_job_interval_config_span_v10,size));
puts(""); return 0;
}
