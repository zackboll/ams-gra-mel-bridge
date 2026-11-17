#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_status_t (*signature)(ams_mel_ir_scheduling *,
    const ams_mel_ir_task_info_v1 *, ams_mel_ir_return_request **,
    char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_exec_abort, signature: 1, default: 0), "signature");
_Static_assert(sizeof(ams_mel_ir_task_info_v1) == 24, "size");
_Static_assert(_Alignof(ams_mel_ir_task_info_v1) == 8, "alignment");
_Static_assert(offsetof(ams_mel_ir_task_info_v1, system_time_ns) == 0, "time");
_Static_assert(offsetof(ams_mel_ir_task_info_v1, request_id) == 8, "request");
_Static_assert(offsetof(ams_mel_ir_task_info_v1, action) == 12, "action");
_Static_assert(offsetof(ams_mel_ir_task_info_v1, precedence_within_priority) == 16, "precedence");
_Static_assert(offsetof(ams_mel_ir_task_info_v1, priority) == 20, "priority");
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu\n", sizeof(ams_mel_ir_task_info_v1),
        _Alignof(ams_mel_ir_task_info_v1), offsetof(ams_mel_ir_task_info_v1, system_time_ns),
        offsetof(ams_mel_ir_task_info_v1, request_id), offsetof(ams_mel_ir_task_info_v1, action),
        offsetof(ams_mel_ir_task_info_v1, precedence_within_priority),
        offsetof(ams_mel_ir_task_info_v1, priority));
    return 0;
}
