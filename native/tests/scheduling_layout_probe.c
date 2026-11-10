#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
/* IRMEL-ADA-011: exact SchedulingChannel foundation signatures and the
 * Scheduling configuration layout for the private Python/Rust mirrors,
 * compiled as C11. Prints one line of size/alignment/offset values; the
 * mirrors compare their own layouts with it. As a CTest target it also fails
 * closed on drift from the Track/Health/Instrumentation configuration layout. */
typedef ams_mel_status_t (*open_signature)(const ams_mel_session *,
    const ams_mel_ir_scheduling_config_v1 *, ams_mel_ir_scheduling **, char *, size_t, size_t *);
typedef ams_mel_status_t (*enable_signature)(ams_mel_ir_scheduling *, char *, size_t, size_t *);
typedef ams_mel_status_t (*caps_signature)(ams_mel_ir_scheduling *,
    ams_mel_ir_channel_capability **, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_scheduling **, char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_ir_scheduling *, ams_mel_ir_channel **,
    char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_open, open_signature: 1, default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_scheduling_enable, enable_signature: 1, default: 0),
               "enable");
_Static_assert(_Generic(&ams_mel_ir_scheduling_get_capabilities, caps_signature: 1, default: 0),
               "capabilities");
_Static_assert(_Generic(&ams_mel_ir_scheduling_close, close_signature: 1, default: 0), "close");
_Static_assert(_Generic(&ams_mel_ir_channel_from_scheduling, view_signature: 1, default: 0),
               "view");
#define SCFG ((ams_mel_ir_scheduling_config_v1 *)0)
_Static_assert(_Generic(SCFG->channel_id, ams_mel_uci_id_v1: 1, default: 0), "channel id");
_Static_assert(_Generic(SCFG->channel_type, ams_mel_ir_channel_type_t: 1, default: 0), "type");
_Static_assert(_Generic(SCFG->platform_id, ams_mel_uci_id_v1: 1, default: 0), "platform id");
_Static_assert(_Generic(SCFG->sensor_location, ams_mel_component_location_v1: 1, default: 0),
               "canonical component location");
_Static_assert(AMS_MEL_IR_CHANNEL_SCHEDULING == 3U, "Scheduling is upstream value 3");
_Static_assert(sizeof(ams_mel_ir_scheduling_config_v1) == sizeof(ams_mel_ir_track_config_v1),
               "same configuration convention as Track");
_Static_assert(offsetof(ams_mel_ir_scheduling_config_v1, sensor_location) ==
               offsetof(ams_mel_ir_health_config_v1, sensor_location), "same as Health");
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_scheduling_config_v1),
        _Alignof(ams_mel_ir_scheduling_config_v1),
        offsetof(ams_mel_ir_scheduling_config_v1, channel_id),
        offsetof(ams_mel_ir_scheduling_config_v1, channel_type),
        offsetof(ams_mel_ir_scheduling_config_v1, platform_id),
        offsetof(ams_mel_ir_scheduling_config_v1, sensor_location),
        sizeof(ams_mel_ir_scheduling *),
        (size_t)AMS_MEL_IR_CHANNEL_SCHEDULING);
    return 0;
}
