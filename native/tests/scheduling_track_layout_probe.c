#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_ir_scheduling *,
    const ams_mel_ir_track_task_v1 *, ams_mel_ir_return_request **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_exec_track, signature: 1, default: 0), "signature");
_Static_assert(AMS_MEL_IR_TRACK_TASK_CAPACITY == 255, "capacity");
_Static_assert(sizeof(ams_mel_ir_track_task_data_v1) == 40, "ams_mel_ir_track_task_data_v1 size");
_Static_assert(_Alignof(ams_mel_ir_track_task_data_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_track_task_data_v1, target_id) == 0, "target_id");
_Static_assert(offsetof(ams_mel_ir_track_task_data_v1, location_azimuth_rad) == 8, "location_azimuth_rad");
_Static_assert(offsetof(ams_mel_ir_track_task_data_v1, location_elevation_rad) == 16, "location_elevation_rad");
_Static_assert(offsetof(ams_mel_ir_track_task_data_v1, rate_azimuth_rad_per_second) == 24, "rate_azimuth_rad_per_second");
_Static_assert(offsetof(ams_mel_ir_track_task_data_v1, rate_elevation_rad_per_second) == 32, "rate_elevation_rad_per_second");
_Static_assert(sizeof(ams_mel_ir_track_task_param_v1) == 10216, "ams_mel_ir_track_task_param_v1 size");
_Static_assert(_Alignof(ams_mel_ir_track_task_param_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_track_task_param_v1, duration_ns) == 0, "duration_ns");
_Static_assert(offsetof(ams_mel_ir_track_task_param_v1, count) == 8, "count");
_Static_assert(offsetof(ams_mel_ir_track_task_param_v1, targets) == 16, "targets");
_Static_assert(sizeof(ams_mel_ir_track_task_v1) == 10248, "ams_mel_ir_track_task_v1 size");
_Static_assert(_Alignof(ams_mel_ir_track_task_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_track_task_v1, track_parameters) == 0, "track_parameters");
_Static_assert(offsetof(ams_mel_ir_track_task_v1, task_info) == 10216, "task_info");
_Static_assert(offsetof(ams_mel_ir_track_task_v1, command_id) == 10240, "command_id");
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_track_task_data_v1), _Alignof(ams_mel_ir_track_task_data_v1), offsetof(ams_mel_ir_track_task_data_v1, target_id), offsetof(ams_mel_ir_track_task_data_v1, location_azimuth_rad), offsetof(ams_mel_ir_track_task_data_v1, location_elevation_rad), offsetof(ams_mel_ir_track_task_data_v1, rate_azimuth_rad_per_second), offsetof(ams_mel_ir_track_task_data_v1, rate_elevation_rad_per_second));
    printf("%zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_track_task_param_v1), _Alignof(ams_mel_ir_track_task_param_v1), offsetof(ams_mel_ir_track_task_param_v1, duration_ns), offsetof(ams_mel_ir_track_task_param_v1, count), offsetof(ams_mel_ir_track_task_param_v1, targets));
    printf("%zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_track_task_v1), _Alignof(ams_mel_ir_track_task_v1), offsetof(ams_mel_ir_track_task_v1, track_parameters), offsetof(ams_mel_ir_track_task_v1, task_info), offsetof(ams_mel_ir_track_task_v1, command_id));
    return 0;
}
