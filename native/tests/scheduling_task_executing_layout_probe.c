#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/* IRMEL-ADA-013: exact Scheduling TaskExecutingRep signatures, the Task and
 * TaskCompStatus constants and the 48-byte ams_mel_ir_task_executing_rep_v1
 * layout for the private Python/Rust mirrors, compiled as C11. Prints one
 * line of size/alignment/offset/constant values; the mirrors compare their own
 * layouts with it. As a CTest target it also fails closed on any drift. */
typedef ams_mel_status_t (*open_signature)(ams_mel_ir_scheduling *, size_t,
    ams_mel_ir_scheduling_task_executing_metadata **, ams_mel_ir_return_t *, char *, size_t,
    size_t *);
typedef ams_mel_status_t (*receive_signature)(ams_mel_ir_scheduling_task_executing_metadata *,
    uint32_t, ams_mel_ir_scheduling_task_executing_event **, char *, size_t, size_t *);
typedef ams_mel_status_t (*counters_signature)(
    const ams_mel_ir_scheduling_task_executing_metadata *, ams_mel_ir_metadata_counters_v1 *,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_scheduling_task_executing_metadata **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_ir_scheduling_task_executing_event *,
    const ams_mel_ir_task_executing_rep_v1 **, char *, size_t, size_t *);
typedef ams_mel_status_t (*event_close_signature)(ams_mel_ir_scheduling_task_executing_event **,
    char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_open, open_signature: 1,
                        default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_receive, receive_signature: 1,
                        default: 0), "receive");
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_get_counters,
                        counters_signature: 1, default: 0), "counters");
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_close, close_signature: 1,
                        default: 0), "close");
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_event_view, view_signature: 1,
                        default: 0), "view");
_Static_assert(_Generic(&ams_mel_ir_scheduling_task_executing_event_close,
                        event_close_signature: 1, default: 0), "event close");
#define REP ((ams_mel_ir_task_executing_rep_v1 *)0)
_Static_assert(_Generic(REP->command_id, uint32_t: 1, default: 0), "command");
_Static_assert(_Generic(REP->activity_id, uint32_t: 1, default: 0), "activity");
_Static_assert(_Generic(REP->request_id, uint32_t: 1, default: 0), "request");
_Static_assert(_Generic(REP->task, uint32_t: 1, default: 0), "task");
_Static_assert(_Generic(REP->estimated_duration_ns, int64_t: 1, default: 0), "duration");
_Static_assert(_Generic(REP->track_id, int32_t: 1, default: 0), "track");
_Static_assert(_Generic(REP->previous_activity_id, uint32_t: 1, default: 0), "prev activity");
_Static_assert(_Generic(REP->previous_request_id, uint32_t: 1, default: 0), "prev request");
_Static_assert(_Generic(REP->previous_task, uint32_t: 1, default: 0), "prev task");
_Static_assert(_Generic(REP->previous_task_status, uint32_t: 1, default: 0), "prev status");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, command_id) == 0, "0");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, activity_id) == 4, "4");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, request_id) == 8, "8");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, task) == 12, "12");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, estimated_duration_ns) == 16, "16");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, track_id) == 24, "24");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, previous_activity_id) == 28, "28");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, previous_request_id) == 32, "32");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, previous_task) == 36, "36");
_Static_assert(offsetof(ams_mel_ir_task_executing_rep_v1, previous_task_status) == 40, "40");
_Static_assert(sizeof(ams_mel_ir_task_executing_rep_v1) == 48, "48 bytes, 4 tail padding");
_Static_assert(_Alignof(ams_mel_ir_task_executing_rep_v1) == 8, "int64 alignment");
_Static_assert(AMS_MEL_IR_METADATA_TASK_EXECUTING_REP == 12U, "existing capability constant");
#define OFF(field) offsetof(ams_mel_ir_task_executing_rep_v1, field)
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu",
        sizeof(ams_mel_ir_task_executing_rep_v1), _Alignof(ams_mel_ir_task_executing_rep_v1),
        OFF(command_id), OFF(activity_id), OFF(request_id), OFF(task),
        OFF(estimated_duration_ns), OFF(track_id), OFF(previous_activity_id),
        OFF(previous_request_id), OFF(previous_task), OFF(previous_task_status));
    printf(" %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu",
        (size_t)AMS_MEL_IR_TASK_NO_TASK, (size_t)AMS_MEL_IR_TASK_ABORT_TASK,
        (size_t)AMS_MEL_IR_TASK_IRST_CAM_CAL, (size_t)AMS_MEL_IR_TASK_CAM_CAL,
        (size_t)AMS_MEL_IR_TASK_IRST_BORESIGHT, (size_t)AMS_MEL_IR_TASK_IDLE,
        (size_t)AMS_MEL_IR_TASK_IRST_SCAN, (size_t)AMS_MEL_IR_TASK_IRST_POSITION,
        (size_t)AMS_MEL_IR_TASK_IRST_STT, (size_t)AMS_MEL_IR_TASK_IRST_MTT,
        (size_t)AMS_MEL_IR_TASK_IRST_UPDATE, (size_t)AMS_MEL_IR_TASK_RESERVED_1,
        (size_t)AMS_MEL_IR_TASK_RESERVED_2, (size_t)AMS_MEL_IR_TASK_RESERVED_3,
        (size_t)AMS_MEL_IR_TASK_RESERVED_4, (size_t)AMS_MEL_IR_TASK_RESERVED_5,
        (size_t)AMS_MEL_IR_TASK_RESERVED_6);
    printf(" %zu %zu %zu %zu %zu %zu\n",
        (size_t)AMS_MEL_IR_TASK_COMP_STATUS_NO_TASK, (size_t)AMS_MEL_IR_TASK_COMP_STATUS_COMPLETE,
        (size_t)AMS_MEL_IR_TASK_COMP_STATUS_TIMEOUT, (size_t)AMS_MEL_IR_TASK_COMP_STATUS_ABORT,
        (size_t)AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED,
        (size_t)AMS_MEL_IR_METADATA_TASK_EXECUTING_REP);
    return 0;
}
