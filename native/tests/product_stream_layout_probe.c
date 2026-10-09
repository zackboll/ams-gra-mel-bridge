#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
#define L(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define O(T,F) printf("%zu ", offsetof(T,F))
int main(void) {
    L(ams_mel_rf_product_stream_endpoint_v1);
    O(ams_mel_rf_product_stream_endpoint_v1,endpoint_id);
    O(ams_mel_rf_product_stream_endpoint_v1,start_address);
    O(ams_mel_rf_product_stream_endpoint_v1,max_bytes);
    L(ams_mel_rf_product_stream_endpoint_span_v1);
    O(ams_mel_rf_product_stream_endpoint_span_v1,data);
    O(ams_mel_rf_product_stream_endpoint_span_v1,size);
    L(ams_mel_rf_product_stream_params_v1);
    O(ams_mel_rf_product_stream_params_v1,applicable_rx_element_groups);
    O(ams_mel_rf_product_stream_params_v1,endpoints);
    L(ams_mel_rf_job_interval_config_v7);
    O(ams_mel_rf_job_interval_config_v7,interval);
    O(ams_mel_rf_job_interval_config_v7,has_product_stream_params);
    O(ams_mel_rf_job_interval_config_v7,product_stream_params);
    L(ams_mel_rf_job_interval_config_span_v7);
    O(ams_mel_rf_job_interval_config_span_v7,data);
    O(ams_mel_rf_job_interval_config_span_v7,size);
    puts("");
    return 0;
}
