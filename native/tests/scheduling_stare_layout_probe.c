#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_ir_scheduling *,
    const ams_mel_ir_stare_task_v1 *, ams_mel_ir_return_request **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_exec_stare, signature: 1, default: 0), "signature");
_Static_assert(sizeof(ams_mel_ir_stare_task_v1) == 64, "size");
_Static_assert(_Alignof(ams_mel_ir_stare_task_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, center_azimuth_rad) == 0, "center_azimuth_rad");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, center_elevation_rad) == 8, "center_elevation_rad");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, duration_ns) == 16, "duration_ns");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, elevation_frame) == 24, "elevation_frame");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, azimuth_frame) == 28, "azimuth_frame");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, command_id) == 32, "command_id");
_Static_assert(offsetof(ams_mel_ir_stare_task_v1, task_info) == 40, "task_info");
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu\n", sizeof(ams_mel_ir_stare_task_v1),
        _Alignof(ams_mel_ir_stare_task_v1),
        offsetof(ams_mel_ir_stare_task_v1, center_azimuth_rad),
        offsetof(ams_mel_ir_stare_task_v1, center_elevation_rad),
        offsetof(ams_mel_ir_stare_task_v1, duration_ns),
        offsetof(ams_mel_ir_stare_task_v1, elevation_frame),
        offsetof(ams_mel_ir_stare_task_v1, azimuth_frame),
        offsetof(ams_mel_ir_stare_task_v1, command_id),
        offsetof(ams_mel_ir_stare_task_v1, task_info));
    return 0;
}
