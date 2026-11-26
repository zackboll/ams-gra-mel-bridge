#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_ir_scheduling_scan_performance_metadata stream_t;
typedef ams_mel_ir_scheduling_scan_performance_event event_t;
typedef ams_mel_ir_scan_performance_report_v1 view_t;
#define DIAG char *, size_t, size_t *
typedef ams_mel_status_t (*open_t)(ams_mel_ir_scheduling *, size_t, stream_t **,
                                  ams_mel_ir_return_t *, DIAG);
typedef ams_mel_status_t (*receive_t)(stream_t *, uint32_t, event_t **, DIAG);
typedef ams_mel_status_t (*get_counters_t)(const stream_t *, ams_mel_ir_metadata_counters_v1 *, DIAG);
typedef ams_mel_status_t (*close_t)(stream_t **, DIAG);
typedef ams_mel_status_t (*event_view_t)(const event_t *, const view_t **, DIAG);
typedef ams_mel_status_t (*event_close_t)(event_t **, DIAG);
#define SIGNATURE(name) _Static_assert(_Generic(&ams_mel_ir_scheduling_scan_performance_##name, \
                                               name##_t: 1, default: 0), #name)
SIGNATURE(open);
SIGNATURE(receive);
SIGNATURE(get_counters);
SIGNATURE(close);
SIGNATURE(event_view);
SIGNATURE(event_close);

/* Incomplete, distinct owner tags (never interchangeable with Scheduling). */
_Static_assert(_Generic((stream_t *)0, struct ams_mel_ir_scheduling_scan_performance_metadata *: 1, default: 0),
               "stream owner tag");
_Static_assert(_Generic((event_t *)0, struct ams_mel_ir_scheduling_scan_performance_event *: 1, default: 0),
               "event owner tag");
_Static_assert(_Generic((stream_t *)0, event_t *: 0, ams_mel_ir_scheduling *: 0, default: 1),
               "independent owner types");
#define MEMBER(record, name, type) \
    _Static_assert(_Generic(((record *)0)->name, type: 1, default: 0), #name " type")
MEMBER(view_t, commanded, ams_mel_ir_scan_param_v1);
MEMBER(view_t, achieved, ams_mel_ir_scan_param_v1);
MEMBER(ams_mel_ir_scan_param_v1, elevation_defined_with_range_and_altitude, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, center_az_rad, double);
MEMBER(ams_mel_ir_scan_param_v1, center_el_rad, double);
MEMBER(ams_mel_ir_scan_param_v1, center_frame_ref_el, ams_mel_ir_coord_frame_ref_t);
MEMBER(ams_mel_ir_scan_param_v1, center_frame_ref_az, ams_mel_ir_coord_frame_ref_t);
MEMBER(ams_mel_ir_scan_param_v1, scan_width_rad, double);
MEMBER(ams_mel_ir_scan_param_v1, scan_height_rad, double);
MEMBER(ams_mel_ir_scan_param_v1, scan_type, ams_mel_ir_scan_type_v1);
MEMBER(ams_mel_ir_scan_param_v1, scan_id, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, scan_rate_rad_per_second, double);
MEMBER(ams_mel_ir_scan_param_v1, preferred_revisit_interval_seconds, double);
MEMBER(ams_mel_ir_scan_param_v1, required_revisit_interval_seconds, double);
MEMBER(ams_mel_ir_scan_param_v1, max_range_of_interest_m, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, min_range_of_interest_m, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, elevation_scan_center_altitude_m, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, elevation_scan_center_range_m, uint32_t);
MEMBER(ams_mel_ir_scan_param_v1, degradation_method, ams_mel_ir_degradation_method_t);
MEMBER(ams_mel_ir_scan_type_v1, continuous_scan, uint32_t);
MEMBER(ams_mel_ir_scan_type_v1, returning, uint32_t);
MEMBER(ams_mel_ir_scan_type_v1, agile_scan, uint32_t);
_Static_assert(sizeof(view_t) == 224 && _Alignof(view_t) == 8, "report layout");
_Static_assert(offsetof(view_t, commanded) == 0 && offsetof(view_t, achieved) == 112, "report offsets");
_Static_assert(sizeof(ams_mel_ir_scan_param_v1) == 112 && _Alignof(ams_mel_ir_scan_param_v1) == 8, "scan param layout");
_Static_assert(sizeof(ams_mel_ir_scan_type_v1) == 12 && _Alignof(ams_mel_ir_scan_type_v1) == 4, "scan type layout");
_Static_assert(sizeof(stream_t *) == sizeof(void *) && _Alignof(stream_t *) == _Alignof(void *), "stream pointer");
_Static_assert(sizeof(event_t *) == sizeof(void *) && _Alignof(event_t *) == _Alignof(void *), "event pointer");
_Static_assert(AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT == 27, "capability");

int main(void)
{
    const size_t layout[] = {
        sizeof(view_t), _Alignof(view_t),
        offsetof(view_t, commanded),
        offsetof(view_t, achieved),
        sizeof(ams_mel_ir_scan_param_v1), _Alignof(ams_mel_ir_scan_param_v1),
        offsetof(ams_mel_ir_scan_param_v1, elevation_defined_with_range_and_altitude),
        offsetof(ams_mel_ir_scan_param_v1, center_az_rad),
        offsetof(ams_mel_ir_scan_param_v1, center_el_rad),
        offsetof(ams_mel_ir_scan_param_v1, center_frame_ref_el),
        offsetof(ams_mel_ir_scan_param_v1, center_frame_ref_az),
        offsetof(ams_mel_ir_scan_param_v1, scan_width_rad),
        offsetof(ams_mel_ir_scan_param_v1, scan_height_rad),
        offsetof(ams_mel_ir_scan_param_v1, scan_type),
        offsetof(ams_mel_ir_scan_param_v1, scan_id),
        offsetof(ams_mel_ir_scan_param_v1, scan_rate_rad_per_second),
        offsetof(ams_mel_ir_scan_param_v1, preferred_revisit_interval_seconds),
        offsetof(ams_mel_ir_scan_param_v1, required_revisit_interval_seconds),
        offsetof(ams_mel_ir_scan_param_v1, max_range_of_interest_m),
        offsetof(ams_mel_ir_scan_param_v1, min_range_of_interest_m),
        offsetof(ams_mel_ir_scan_param_v1, elevation_scan_center_altitude_m),
        offsetof(ams_mel_ir_scan_param_v1, elevation_scan_center_range_m),
        offsetof(ams_mel_ir_scan_param_v1, degradation_method),
        sizeof(ams_mel_ir_scan_type_v1), _Alignof(ams_mel_ir_scan_type_v1),
        offsetof(ams_mel_ir_scan_type_v1, continuous_scan),
        offsetof(ams_mel_ir_scan_type_v1, returning),
        offsetof(ams_mel_ir_scan_type_v1, agile_scan),
        sizeof(stream_t *), _Alignof(stream_t *), sizeof(event_t *), _Alignof(event_t *),
        AMS_MEL_IR_METADATA_SCAN_PERFORMANCE_REPORT
    };
    for (size_t i = 0; i < sizeof layout / sizeof *layout; ++i) printf("%zu ", layout[i]);
    puts("");
    return 0;
}
