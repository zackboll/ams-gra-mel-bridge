#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*signature)(ams_mel_ir_scheduling *,
    const ams_mel_ir_scan_task_v1 *, ams_mel_ir_return_request **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_exec_scan, signature: 1, default: 0), "signature");
_Static_assert(sizeof(ams_mel_ir_scan_param_v1) == 112, "existing scan size");
_Static_assert(sizeof(ams_mel_ir_task_info_v1) == 24, "existing task info size");
_Static_assert(sizeof(ams_mel_ir_scan_task_v1) == 144, "size");
_Static_assert(_Alignof(ams_mel_ir_scan_task_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_scan_task_v1, scan_parameters) == 0, "scan_parameters");
_Static_assert(offsetof(ams_mel_ir_scan_task_v1, task_info) == 112, "task_info");
_Static_assert(offsetof(ams_mel_ir_scan_task_v1, command_id) == 136, "command_id");
int main(void)
{
    printf("%zu %zu %zu %zu %zu\n", sizeof(ams_mel_ir_scan_task_v1),
        _Alignof(ams_mel_ir_scan_task_v1),
        offsetof(ams_mel_ir_scan_task_v1, scan_parameters),
        offsetof(ams_mel_ir_scan_task_v1, task_info),
        offsetof(ams_mel_ir_scan_task_v1, command_id));
    return 0;
}
