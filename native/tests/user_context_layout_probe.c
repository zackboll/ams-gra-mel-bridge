#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_rf_job *, ams_mel_rf_job_interval_config_span_v16, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_job_add_intervals_v16, signature: 1, default: 0), "v16 by-value span");
int main(void)
{
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_v16), _Alignof(ams_mel_rf_job_interval_config_v16));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, interval));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, has_transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, transmit_events));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, has_modulations));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, modulations));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, has_user_context));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_v16, user_context));
    printf("%zu %zu ", sizeof(ams_mel_rf_job_interval_config_span_v16), _Alignof(ams_mel_rf_job_interval_config_span_v16));
    printf("%zu ", offsetof(ams_mel_rf_job_interval_config_span_v16, data));
    printf("%zu\n", offsetof(ams_mel_rf_job_interval_config_span_v16, size));
    return 0;
}
