#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v12, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v12, signature: 1, default: 0), "v12 by-value span signature");
#define L(T) printf("%zu %zu ",sizeof(T),_Alignof(T))
#define F(T,f) printf("%zu ",offsetof(T,f))
int main(void) {
L(ams_mel_rf_modulation_config_v2);
F(ams_mel_rf_modulation_config_v2,modulation);
F(ams_mel_rf_modulation_config_v2,binary_data);
F(ams_mel_rf_modulation_config_v2,pulse_duration_femtoseconds);
F(ams_mel_rf_modulation_config_v2,frequency_offset_hz);
L(ams_mel_rf_modulation_config_span_v2);
F(ams_mel_rf_modulation_config_span_v2,data);
F(ams_mel_rf_modulation_config_span_v2,size);
L(ams_mel_rf_job_interval_config_v12);
F(ams_mel_rf_job_interval_config_v12,interval);
F(ams_mel_rf_job_interval_config_v12,has_transmit_events);
F(ams_mel_rf_job_interval_config_v12,transmit_events);
F(ams_mel_rf_job_interval_config_v12,has_modulations);
F(ams_mel_rf_job_interval_config_v12,modulations);
L(ams_mel_rf_job_interval_config_span_v12);
F(ams_mel_rf_job_interval_config_span_v12,data);
F(ams_mel_rf_job_interval_config_span_v12,size);
puts(""); return 0;
}
