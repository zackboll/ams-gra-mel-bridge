#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_ir_scheduling_schedule_created_metadata stream_t;
typedef ams_mel_ir_scheduling_schedule_created_event event_t;
typedef ams_mel_ir_activity_record_v1 activity_t;
typedef ams_mel_ir_activity_span_v1 span_t;
typedef ams_mel_ir_sched_created_rep_v1 report_t;
typedef ams_mel_ir_range_az_el_v1 position_t;
#define DIAG char *, size_t, size_t *
typedef ams_mel_status_t (*open_t)(ams_mel_ir_scheduling *, size_t, stream_t **,
                                  ams_mel_ir_return_t *, DIAG);
typedef ams_mel_status_t (*receive_t)(stream_t *, uint32_t, event_t **, DIAG);
typedef ams_mel_status_t (*get_counters_t)(const stream_t *, ams_mel_ir_metadata_counters_v1 *, DIAG);
typedef ams_mel_status_t (*close_t)(stream_t **, DIAG);
typedef ams_mel_status_t (*event_view_t)(const event_t *, const report_t **, DIAG);
typedef ams_mel_status_t (*event_close_t)(event_t **, DIAG);
#define SIGNATURE(name) _Static_assert(_Generic(&ams_mel_ir_scheduling_schedule_created_##name, \
                                          name##_t: 1, default: 0), #name)
SIGNATURE(open);
SIGNATURE(receive);
SIGNATURE(get_counters);
SIGNATURE(close);
SIGNATURE(event_view);
SIGNATURE(event_close);

#define FIELD(record, name, type) \
    _Static_assert(_Generic(((record *)0)->name, type: 1, default: 0), #name)
FIELD(position_t, range_m, double);
FIELD(position_t, azimuth_rad, double);
FIELD(position_t, elevation_rad, double);
FIELD(position_t, range_rate_mps, double);
FIELD(activity_t, activity_id, uint32_t);
FIELD(activity_t, task_id, uint32_t);
FIELD(activity_t, track_id, int32_t);
FIELD(activity_t, position, position_t);
FIELD(activity_t, duration_ns, int64_t);
FIELD(span_t, data, const activity_t *);
FIELD(span_t, size, size_t);
FIELD(report_t, command_id, uint32_t);
FIELD(report_t, scan_id, uint32_t);
FIELD(report_t, estimated_duration_ns, int64_t);
FIELD(report_t, activity_count, uint32_t);
FIELD(report_t, activities, span_t);

#define LAYOUT(t) sizeof(t), _Alignof(t)
#define OFFSET(t, field) offsetof(t, field)
int main(void)
{
    const size_t layout[] = {
        LAYOUT(position_t), OFFSET(position_t, range_m), OFFSET(position_t, azimuth_rad),
        OFFSET(position_t, elevation_rad), OFFSET(position_t, range_rate_mps),
        LAYOUT(activity_t), OFFSET(activity_t, activity_id), OFFSET(activity_t, task_id),
        OFFSET(activity_t, track_id), OFFSET(activity_t, position), OFFSET(activity_t, duration_ns),
        LAYOUT(span_t), OFFSET(span_t, data), OFFSET(span_t, size),
        LAYOUT(report_t), OFFSET(report_t, command_id), OFFSET(report_t, scan_id),
        OFFSET(report_t, estimated_duration_ns), OFFSET(report_t, activity_count), OFFSET(report_t, activities)
    };
    for (size_t i = 0; i < sizeof layout / sizeof *layout; ++i) printf("%zu ", layout[i]);
    puts("");
    return 0;
}
