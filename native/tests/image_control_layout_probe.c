#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

/* Compile-time checks include constness, output indirection and diagnostics. */
typedef ams_mel_status_t (*open_signature)(
    const ams_mel_session *, const ams_mel_ir_image_control_config_v1 *,
    ams_mel_ir_stream **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_image_control_open, open_signature: 1, default: 0),
               "Image control open signature");
#define FIELD_TYPE(member, type) \
    _Static_assert(_Generic(((ams_mel_ir_image_control_config_v1 *)0)->member, \
                            type: 1, default: 0), "Image control field type: " #member)
FIELD_TYPE(channel_type, uint32_t);
FIELD_TYPE(channel_id, ams_mel_uci_id_v1);
FIELD_TYPE(platform_id, ams_mel_uci_id_v1);
FIELD_TYPE(sensor_location, ams_mel_component_location_v1);

int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu\n",
           sizeof(ams_mel_ir_image_control_config_v1),
           _Alignof(ams_mel_ir_image_control_config_v1),
           offsetof(ams_mel_ir_image_control_config_v1, channel_type),
           offsetof(ams_mel_ir_image_control_config_v1, channel_id),
           offsetof(ams_mel_ir_image_control_config_v1, platform_id),
           offsetof(ams_mel_ir_image_control_config_v1, sensor_location));
    return 0;
}
