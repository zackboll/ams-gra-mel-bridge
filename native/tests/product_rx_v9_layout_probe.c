#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*submit_signature)(ams_mel_rf_data *,
    const ams_mel_rf_product_rx_config_v1 *, ams_mel_rf_product_rx_request **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_rf_product_rx_event *,
    const ams_mel_rf_product_rx_event_v9 **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_rf_data_submit_product_rx_v9,
    submit_signature: 1, default: 0), "v9 submit signature");
_Static_assert(_Generic(&ams_mel_rf_product_rx_event_view_v9,
    view_signature: 1, default: 0), "v9 view signature");
_Static_assert(offsetof(ams_mel_rf_product_rx_event_v9, event) == 0, "frozen v8 offset");
_Static_assert(_Generic(((ams_mel_rf_pointing_span_v1 *)0)->data,
    const ams_mel_rf_pointing_v1 *: 1, default: 0), "immutable reused F2 record");
_Static_assert(_Generic(((ams_mel_rf_pointing_v1 *)0)->kind,
    uint32_t: 1, default: 0), "kind type");
#define LAYOUT(T) printf("%zu %zu ", sizeof(T), _Alignof(T))
#define FIELD(T, F) printf("%zu ", offsetof(T, F))
int main(void)
{
    LAYOUT(ams_mel_rf_product_rx_event_v9);
    FIELD(ams_mel_rf_product_rx_event_v9, event);
    FIELD(ams_mel_rf_product_rx_event_v9, stabilization_points);
    LAYOUT(ams_mel_rf_pointing_span_v1);
    FIELD(ams_mel_rf_pointing_span_v1, data);
    FIELD(ams_mel_rf_pointing_span_v1, size);
    LAYOUT(ams_mel_rf_pointing_v1);
    FIELD(ams_mel_rf_pointing_v1, kind);
    FIELD(ams_mel_rf_pointing_v1, ecef);
    FIELD(ams_mel_rf_pointing_v1, lla);
    FIELD(ams_mel_rf_pointing_v1, platform_relative);
    FIELD(ams_mel_rf_pointing_v1, face_relative);
    FIELD(ams_mel_rf_pointing_v1, baseline_relative_conic_rad);
    putchar('\n');
    return 0;
}
