#include <ams_mel/abi.h>
#include <stdio.h>
#include <stddef.h>
#define L(T) printf("%zu %zu ",sizeof(T),_Alignof(T))
#define O(T,F) printf("%zu ",offsetof(T,F))
int main(void) {
L(ams_mel_rf_lf_address_value_v1);O(ams_mel_rf_lf_address_value_v1,address);O(ams_mel_rf_lf_address_value_v1,value);
L(ams_mel_rf_lf_address_value_span_v1);O(ams_mel_rf_lf_address_value_span_v1,data);O(ams_mel_rf_lf_address_value_span_v1,size);
L(ams_mel_rf_lf_command_v1);O(ams_mel_rf_lf_command_v1,local_function_type_id);O(ams_mel_rf_lf_command_v1,local_function_instance);O(ams_mel_rf_lf_command_v1,address_values);
L(ams_mel_rf_lf_command_span_v1);O(ams_mel_rf_lf_command_span_v1,data);O(ams_mel_rf_lf_command_span_v1,size);
L(ams_mel_rf_job_interval_config_v6);O(ams_mel_rf_job_interval_config_v6,interval);O(ams_mel_rf_job_interval_config_v6,local_function_commands);
L(ams_mel_rf_job_interval_config_span_v6);O(ams_mel_rf_job_interval_config_span_v6,data);O(ams_mel_rf_job_interval_config_span_v6,size);
return 0;}
