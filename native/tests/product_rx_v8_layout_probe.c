#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*submit_signature)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_rf_product_rx_event *,
    const ams_mel_rf_product_rx_event_v8 **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_data_submit_product_rx_v8,
    submit_signature: 1, default: 0), "v8 submit signature");
_Static_assert(_Generic(&ams_mel_rf_product_rx_event_view_v8,
    view_signature: 1, default: 0), "v8 view signature");
_Static_assert(offsetof(ams_mel_rf_product_rx_event_v8, event) == 0, "frozen v7 offset");
_Static_assert(_Generic(((ams_mel_rf_receive_event_associations_v1 *)0)->present,
    uint32_t: 1, default: 0), "presence type");
_Static_assert(_Generic(((ams_mel_u32_span_v1 *)0)->data,
    const uint32_t *: 1, default: 0), "immutable exact uint32 IDs");
#define LAYOUT(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define FIELD(T, F) printf("%zu ", offsetof(T, F))
int main(void)
{
    LAYOUT(ams_mel_rf_receive_event_associations_v1);
    FIELD(ams_mel_rf_receive_event_associations_v1, present);
    FIELD(ams_mel_rf_receive_event_associations_v1, event_ids);
    LAYOUT(ams_mel_u32_span_v1);
    FIELD(ams_mel_u32_span_v1, data);
    FIELD(ams_mel_u32_span_v1, size);
    LAYOUT(ams_mel_rf_product_rx_event_v8);
    FIELD(ams_mel_rf_product_rx_event_v8, event);
    FIELD(ams_mel_rf_product_rx_event_v8, receive_event_associations);
    LAYOUT(uint32_t);
    putchar('\n');
    return 0;
}
