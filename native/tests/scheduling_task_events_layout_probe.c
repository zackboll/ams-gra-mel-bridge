#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_ir_scheduling_task_events_metadata stream_t;
typedef ams_mel_ir_scheduling_task_events_event event_t;
typedef ams_mel_ir_task_events_v1 view_t;
#define DIAG char *, size_t, size_t *
typedef ams_mel_status_t (*open_t)(ams_mel_ir_scheduling *, size_t, stream_t **,
                                  ams_mel_ir_return_t *, DIAG);
typedef ams_mel_status_t (*receive_t)(stream_t *, uint32_t, event_t **, DIAG);
typedef ams_mel_status_t (*get_counters_t)(const stream_t *, ams_mel_ir_metadata_counters_v1 *, DIAG);
typedef ams_mel_status_t (*close_t)(stream_t **, DIAG);
typedef ams_mel_status_t (*event_view_t)(const event_t *, const view_t **, DIAG);
typedef ams_mel_status_t (*event_close_t)(event_t **, DIAG);
#define SIGNATURE(name) _Static_assert(_Generic(&ams_mel_ir_scheduling_task_events_##name, \
                                               name##_t: 1, default: 0), #name)
SIGNATURE(open);
SIGNATURE(receive);
SIGNATURE(get_counters);
SIGNATURE(close);
SIGNATURE(event_view);
SIGNATURE(event_close);

/* Incomplete, distinct owner tags (never interchangeable with Scheduling). */
_Static_assert(_Generic((stream_t *)0, struct ams_mel_ir_scheduling_task_events_metadata *: 1, default: 0),
               "stream owner tag");
_Static_assert(_Generic((event_t *)0, struct ams_mel_ir_scheduling_task_events_event *: 1, default: 0),
               "event owner tag");
_Static_assert(_Generic((stream_t *)0, event_t *: 0, ams_mel_ir_scheduling *: 0, default: 1),
               "independent owner types");
#define MEMBER(name, type, offset) \
    _Static_assert(_Generic(((view_t *)0)->name, type: 1, default: 0), #name " type"); \
    _Static_assert(offsetof(view_t, name) == offset, #name " offset")
MEMBER(event_type, uint32_t, 0);
MEMBER(scan_bar, uint32_t, 4);
MEMBER(stop_number, uint32_t, 8);
MEMBER(system_time_ns, int64_t, 16);
MEMBER(track_id, uint32_t, 24);
_Static_assert(sizeof(view_t) == 32 && _Alignof(view_t) == 8, "record layout");
_Static_assert(AMS_MEL_IR_TASK_EVENT_NOT_SET == 0, "NOT_SET");
_Static_assert(AMS_MEL_IR_TASK_EVENT_SLEW_START == 1, "SLEW_START");
_Static_assert(AMS_MEL_IR_TASK_EVENT_SCAN_START == 2, "SCAN_START");
_Static_assert(AMS_MEL_IR_TASK_EVENT_BREAK_SCAN == 3, "BREAK_SCAN");
_Static_assert(AMS_MEL_IR_TASK_EVENT_RESERVED_11 == 4, "RESERVED_11");
_Static_assert(AMS_MEL_IR_TASK_EVENT_RESERVED_12 == 5, "RESERVED_12");
_Static_assert(AMS_MEL_IR_TASK_EVENT_ACCEL_START == 6, "ACCEL_START");
_Static_assert(AMS_MEL_IR_TASK_EVENT_ACCEL_COMPLETE == 7, "ACCEL_COMPLETE");
_Static_assert(AMS_MEL_IR_METADATA_TASK_EVENTS == 26, "TaskEvents capability");

int main(void)
{
    const size_t layout[] = {
        sizeof(view_t), _Alignof(view_t),
        offsetof(view_t, event_type),
        offsetof(view_t, scan_bar),
        offsetof(view_t, stop_number),
        offsetof(view_t, system_time_ns),
        offsetof(view_t, track_id),
        sizeof(stream_t *), _Alignof(stream_t *), sizeof(event_t *), _Alignof(event_t *),
        AMS_MEL_IR_TASK_EVENT_NOT_SET,
        AMS_MEL_IR_TASK_EVENT_SLEW_START,
        AMS_MEL_IR_TASK_EVENT_SCAN_START,
        AMS_MEL_IR_TASK_EVENT_BREAK_SCAN,
        AMS_MEL_IR_TASK_EVENT_RESERVED_11,
        AMS_MEL_IR_TASK_EVENT_RESERVED_12,
        AMS_MEL_IR_TASK_EVENT_ACCEL_START,
        AMS_MEL_IR_TASK_EVENT_ACCEL_COMPLETE,
        AMS_MEL_IR_METADATA_TASK_EVENTS
    };
    for (size_t i = 0; i < sizeof layout / sizeof *layout; ++i) printf("%zu ", layout[i]);
    puts("");
    return 0;
}
