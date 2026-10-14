#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*submit_signature)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_rf_product_rx_event *,
    const ams_mel_rf_product_rx_event_v4 **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_data_submit_product_rx_v4,
    submit_signature: 1, default: 0), "v4 submit signature");
_Static_assert(_Generic(&ams_mel_rf_product_rx_event_view_v4,
    view_signature: 1, default: 0), "v4 view signature");
_Static_assert(offsetof(ams_mel_rf_product_rx_event_v4, event) == 0, "nested frozen v3");
_Static_assert(_Generic(((ams_mel_rf_vita_small_packet_v1 *)0)->serialized_bytes,
    ams_mel_u8_span_v1: 1, default: 0), "byte span type");
_Static_assert(_Generic(((ams_mel_rf_vita_small_packet_span_v1 *)0)->data,
    const ams_mel_rf_vita_small_packet_v1 *: 1, default: 0), "descriptor pointer type");
#define LAYOUT(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define FIELD(T, F) printf("%zu ", offsetof(T, F))
int main(void)
{
    LAYOUT(ams_mel_rf_vita_small_packet_v1);
    FIELD(ams_mel_rf_vita_small_packet_v1, serialized_bytes);
    LAYOUT(ams_mel_rf_vita_small_packet_span_v1);
    FIELD(ams_mel_rf_vita_small_packet_span_v1, data);
    FIELD(ams_mel_rf_vita_small_packet_span_v1, size);
    LAYOUT(ams_mel_rf_product_rx_event_v4);
    FIELD(ams_mel_rf_product_rx_event_v4, event);
    FIELD(ams_mel_rf_product_rx_event_v4, vita_small_packets);
    putchar('\n');
    return 0;
}
