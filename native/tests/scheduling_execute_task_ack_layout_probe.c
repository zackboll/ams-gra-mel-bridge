#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/* IRMEL-ADA-012: exact Scheduling ExecuteTaskAck signatures and the 8-byte
 * ams_mel_ir_execute_task_ack_v1 layout for the private Python/Rust mirrors,
 * compiled as C11. Prints one line of size/alignment/offset values; the
 * mirrors compare their own layouts with it. As a CTest target it also fails
 * closed on any layout drift. */
typedef ams_mel_status_t (*open_signature)(ams_mel_ir_scheduling *, size_t,
    ams_mel_ir_scheduling_execute_task_ack_metadata **, ams_mel_ir_return_t *, char *, size_t,
    size_t *);
typedef ams_mel_status_t (*receive_signature)(ams_mel_ir_scheduling_execute_task_ack_metadata *,
    uint32_t, ams_mel_ir_scheduling_execute_task_ack_event **, char *, size_t, size_t *);
typedef ams_mel_status_t (*counters_signature)(
    const ams_mel_ir_scheduling_execute_task_ack_metadata *, ams_mel_ir_metadata_counters_v1 *,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_scheduling_execute_task_ack_metadata **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_ir_scheduling_execute_task_ack_event *,
    const ams_mel_ir_execute_task_ack_v1 **, char *, size_t, size_t *);
typedef ams_mel_status_t (*event_close_signature)(ams_mel_ir_scheduling_execute_task_ack_event **,
    char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_open, open_signature: 1,
                        default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_receive, receive_signature: 1,
                        default: 0), "receive");
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_get_counters,
                        counters_signature: 1, default: 0), "counters");
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_close, close_signature: 1,
                        default: 0), "close");
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_event_view, view_signature: 1,
                        default: 0), "view");
_Static_assert(_Generic(&ams_mel_ir_scheduling_execute_task_ack_event_close,
                        event_close_signature: 1, default: 0), "event close");
/* Field types: two full-width unsigned 32-bit identifiers, in upstream order. */
#define ACK ((ams_mel_ir_execute_task_ack_v1 *)0)
_Static_assert(_Generic(ACK->command_id, uint32_t: 1, default: 0), "command identifier");
_Static_assert(_Generic(ACK->request_id, uint32_t: 1, default: 0), "request identifier");
_Static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, command_id) == 0, "command first");
_Static_assert(offsetof(ams_mel_ir_execute_task_ack_v1, request_id) == 4, "request second");
_Static_assert(sizeof(ams_mel_ir_execute_task_ack_v1) == 8, "8 bytes, no padding");
_Static_assert(_Alignof(ams_mel_ir_execute_task_ack_v1) == 4, "uint32 alignment");
_Static_assert(AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK == 14U, "existing capability constant");
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_execute_task_ack_v1),
        _Alignof(ams_mel_ir_execute_task_ack_v1),
        offsetof(ams_mel_ir_execute_task_ack_v1, command_id),
        offsetof(ams_mel_ir_execute_task_ack_v1, request_id),
        sizeof(ams_mel_ir_metadata_counters_v1),
        offsetof(ams_mel_ir_metadata_counters_v1, events_received),
        offsetof(ams_mel_ir_metadata_counters_v1, events_dropped_queue_full),
        offsetof(ams_mel_ir_metadata_counters_v1, malformed_or_unsupported),
        _Alignof(ams_mel_ir_metadata_counters_v1),
        (size_t)AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK);
    return 0;
}
