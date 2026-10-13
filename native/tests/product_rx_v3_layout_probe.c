#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*submit_signature)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_rf_product_rx_event *,
    const ams_mel_rf_product_rx_event_v3 **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_data_submit_product_rx_v3,
    submit_signature: 1, default: 0), "v3 submit signature");
_Static_assert(_Generic(&ams_mel_rf_product_rx_event_view_v3,
    view_signature: 1, default: 0), "v3 view signature");
#define LAYOUT(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define FIELD(T, F) printf("%zu ", offsetof(T, F))
int main(void)
{
    LAYOUT(ams_mel_rf_product_rx_event_v3);
    FIELD(ams_mel_rf_product_rx_event_v3, endpoint_id);
    FIELD(ams_mel_rf_product_rx_event_v3, data_format);
    FIELD(ams_mel_rf_product_rx_event_v3, direct_samples_i8);
    FIELD(ams_mel_rf_product_rx_event_v3, direct_samples_i16);
    FIELD(ams_mel_rf_product_rx_event_v3, complex_samples_i8);
    FIELD(ams_mel_rf_product_rx_event_v3, complex_samples_i16);

    FIELD(ams_mel_rf_product_rx_event_v3, metadata);
    LAYOUT(ams_mel_rf_direct_i8_span_v1);
    FIELD(ams_mel_rf_direct_i8_span_v1, data);
    FIELD(ams_mel_rf_direct_i8_span_v1, size);
    LAYOUT(ams_mel_rf_direct_i16_span_v1);
    FIELD(ams_mel_rf_direct_i16_span_v1, data);
    FIELD(ams_mel_rf_direct_i16_span_v1, size);
    LAYOUT(ams_mel_rf_product_rx_metadata_v1);
    FIELD(ams_mel_rf_product_rx_metadata_v1, mel_protocol_version_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, va_definition_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, va_instance_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, job_details_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, job_interval_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, lf_type_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, lf_instance_id);
    FIELD(ams_mel_rf_product_rx_metadata_v1, phase_coherence_with_prior);
    FIELD(ams_mel_rf_product_rx_metadata_v1, first_rx_event_start_s);
    FIELD(ams_mel_rf_product_rx_metadata_v1, first_rx_event_start_fs);
    FIELD(ams_mel_rf_product_rx_metadata_v1, rx_stream_ids);
    LAYOUT(ams_mel_rf_complex_i8_v1);
    FIELD(ams_mel_rf_complex_i8_v1, real);
    FIELD(ams_mel_rf_complex_i8_v1, imag);
    puts("");
    return 0;
}
