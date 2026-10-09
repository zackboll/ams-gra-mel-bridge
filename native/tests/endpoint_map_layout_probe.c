#include <ams_mel/abi.h>
#include <stdio.h>
#include <stddef.h>
#define L(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define O(T,F) printf("%zu ", offsetof(T,F))
int main(void) {
    L(ams_mel_rf_interval_endpoint_connection_v1);
    O(ams_mel_rf_interval_endpoint_connection_v1,element_group_label);
    O(ams_mel_rf_interval_endpoint_connection_v1,data_pipe_label);
    O(ams_mel_rf_interval_endpoint_connection_v1,endpoint_id);
    L(ams_mel_rf_interval_endpoint_connection_span_v1);
    O(ams_mel_rf_interval_endpoint_connection_span_v1,data);
    O(ams_mel_rf_interval_endpoint_connection_span_v1,size);
    L(ams_mel_rf_job_interval_config_v8);
    O(ams_mel_rf_job_interval_config_v8,interval);
    O(ams_mel_rf_job_interval_config_v8,has_endpoint_connections);
    O(ams_mel_rf_job_interval_config_v8,endpoint_connections);
    L(ams_mel_rf_job_interval_config_span_v8);
    O(ams_mel_rf_job_interval_config_span_v8,data);
    O(ams_mel_rf_job_interval_config_span_v8,size);
    puts(""); return 0;
}
