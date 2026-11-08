#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
/* IRMEL-ADA-010: exact Image LineOfSightQuaternion signatures and layouts for
 * the private Python/Rust mirrors, compiled as C11. Prints one line of
 * size/alignment/offset values; the mirrors compare their own layouts with it.
 * As a CTest target it also fails closed on any LP64 layout drift. */
typedef ams_mel_status_t (*open_signature)(ams_mel_ir_stream *, size_t,
    ams_mel_ir_image_los_quaternion_metadata **, ams_mel_ir_return_t *, char *, size_t, size_t *);
typedef ams_mel_status_t (*receive_signature)(ams_mel_ir_image_los_quaternion_metadata *, uint32_t,
    ams_mel_ir_image_los_quaternion_event **, char *, size_t, size_t *);
typedef ams_mel_status_t (*counters_signature)(const ams_mel_ir_image_los_quaternion_metadata *,
    ams_mel_ir_metadata_counters_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_image_los_quaternion_metadata **, char *,
    size_t, size_t *);
typedef ams_mel_status_t (*view_signature)(const ams_mel_ir_image_los_quaternion_event *,
    const ams_mel_ir_line_of_sight_quaternion_v1 **, char *, size_t, size_t *);
typedef ams_mel_status_t (*event_close_signature)(ams_mel_ir_image_los_quaternion_event **, char *,
    size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_open, open_signature: 1, default: 0),
               "open");
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_receive, receive_signature: 1,
                        default: 0), "receive");
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_get_counters, counters_signature: 1,
                        default: 0), "counters");
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_close, close_signature: 1, default: 0),
               "close");
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_event_view, view_signature: 1,
                        default: 0), "view");
_Static_assert(_Generic(&ams_mel_ir_image_los_quaternion_event_close, event_close_signature: 1,
                        default: 0), "event close");
/* Field types: signed 64-bit time and the two canonical records, reused. */
#define LOSQ ((ams_mel_ir_line_of_sight_quaternion_v1 *)0)
_Static_assert(_Generic(LOSQ->system_time_ns, int64_t: 1, default: 0), "signed nanoseconds");
_Static_assert(_Generic(LOSQ->q_xyzw, ams_mel_ir_quaternion_v1: 1, default: 0),
               "canonical quaternion");
_Static_assert(_Generic(LOSQ->pqr_rps, ams_mel_ir_forward_right_down_v1: 1, default: 0),
               "canonical FRD");
_Static_assert(_Generic(LOSQ->q_xyzw.w, double: 1, default: 0), "quaternion component");
_Static_assert(_Generic(LOSQ->pqr_rps.down, double: 1, default: 0), "rate component");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(ams_mel_ir_quaternion_v1) == 32, "quaternion unchanged");
_Static_assert(offsetof(ams_mel_ir_quaternion_v1, w) == 24, "quaternion w unchanged");
_Static_assert(sizeof(ams_mel_ir_forward_right_down_v1) == 24, "FRD unchanged");
_Static_assert(offsetof(ams_mel_ir_line_of_sight_quaternion_v1, system_time_ns) == 0, "time");
_Static_assert(offsetof(ams_mel_ir_line_of_sight_quaternion_v1, q_xyzw) == 8, "quaternion");
_Static_assert(offsetof(ams_mel_ir_line_of_sight_quaternion_v1, pqr_rps) == 40, "rates");
_Static_assert(sizeof(ams_mel_ir_line_of_sight_quaternion_v1) == 64, "LOS quaternion size");
#endif
int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_line_of_sight_quaternion_v1),
        _Alignof(ams_mel_ir_line_of_sight_quaternion_v1),
        offsetof(ams_mel_ir_line_of_sight_quaternion_v1, system_time_ns),
        offsetof(ams_mel_ir_line_of_sight_quaternion_v1, q_xyzw),
        offsetof(ams_mel_ir_line_of_sight_quaternion_v1, pqr_rps),
        sizeof(ams_mel_ir_quaternion_v1),
        offsetof(ams_mel_ir_quaternion_v1, x), offsetof(ams_mel_ir_quaternion_v1, y),
        offsetof(ams_mel_ir_quaternion_v1, z), offsetof(ams_mel_ir_quaternion_v1, w),
        sizeof(ams_mel_ir_forward_right_down_v1),
        offsetof(ams_mel_ir_forward_right_down_v1, forward),
        offsetof(ams_mel_ir_forward_right_down_v1, right),
        offsetof(ams_mel_ir_forward_right_down_v1, down),
        sizeof(ams_mel_ir_metadata_counters_v1),
        offsetof(ams_mel_ir_metadata_counters_v1, events_received),
        offsetof(ams_mel_ir_metadata_counters_v1, events_dropped_queue_full),
        offsetof(ams_mel_ir_metadata_counters_v1, malformed_or_unsupported),
        _Alignof(ams_mel_ir_metadata_counters_v1));
    return 0;
}
