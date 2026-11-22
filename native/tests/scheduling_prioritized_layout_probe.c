#include <ams_mel/abi.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>

typedef ams_mel_status_t (*send_fn)(ams_mel_ir_scheduling *,
    const ams_mel_ir_prioritized_track_list_cmd_v1 *, ams_mel_ir_track_list_request **, char *, size_t, size_t *);
typedef ams_mel_status_t (*wait_fn)(const ams_mel_ir_track_list_request *, uint32_t,
    ams_mel_ir_track_list_result_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_fn)(ams_mel_ir_track_list_request **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_scheduling_send_prioritized_tracks, send_fn: 1, default: 0), "send signature");
_Static_assert(_Generic(&ams_mel_ir_track_list_request_wait, wait_fn: 1, default: 0), "wait signature");
_Static_assert(_Generic(&ams_mel_ir_track_list_request_close, close_fn: 1, default: 0), "close signature");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && sizeof(double) == 8, "binary64");
_Static_assert(AMS_MEL_IR_TRACK_LIST_RESPONSE_NONE == 0 && AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED == 1 &&
    AMS_MEL_IR_TRACK_LIST_RESPONSE_REJECTED_EXPIRED == 2 && AMS_MEL_IR_TRACK_LIST_RESPONSE_ACCEPTED == 3, "receipts");

#define LAYOUT(T, S, A) _Static_assert(sizeof(T) == S && _Alignof(T) == A, #T " layout");
#define MEMBER(T, M, K, O) _Static_assert(offsetof(T, M) == O && _Generic(((T *)0)->M, K: 1, default: 0), #M);
#define SHOW_LAYOUT(T, S, A) printf("%zu %zu ", sizeof(T), _Alignof(T));
#define SHOW_MEMBER(T, M, K, O) printf("%zu ", offsetof(T, M));
#define RECORDS(L, M) \
    L(rae, 32, 8) \
    M(rae, range_m, double, 0) M(rae, azimuth_rad, double, 8) \
    M(rae, elevation_rad, double, 16) M(rae, range_rate_mps, double, 24) \
    L(track, 112, 8) \
    M(track, timestamp_ns, int64_t, 0) M(track, track_id, int32_t, 8) \
    M(track, irst_priority, uint32_t, 12) M(track, reserved_priority, uint32_t, 16) \
    M(track, irst_precedence, uint32_t, 20) M(track, reserved_precedence, uint32_t, 24) \
    M(track, is_skippable, uint32_t, 28) M(track, maintain_outside_fos, uint32_t, 32) \
    M(track, irst_track_id, int32_t, 36) M(track, position, rae, 40) M(track, velocity, rae, 72) \
    M(track, task, uint32_t, 104) M(track, command_id, uint32_t, 108) \
    L(span, 16, 8) M(span, data, const track *, 0) M(span, size, size_t, 8) \
    L(command, 40, 8) M(command, schedule_time_ns, int64_t, 0) M(command, time_to_live_ns, int64_t, 8) \
    M(command, command_id, uint32_t, 16) M(command, track_count, uint32_t, 20) M(command, tracks, span, 24) \
    L(result, 8, 4) M(result, value, uint32_t, 0) M(result, error_code, uint32_t, 4)

typedef ams_mel_ir_range_az_el_v1 rae;
typedef ams_mel_ir_track_data_v1 track;
typedef ams_mel_ir_track_data_span_v1 span;
typedef ams_mel_ir_prioritized_track_list_cmd_v1 command;
typedef ams_mel_ir_track_list_result_v1 result;
RECORDS(LAYOUT, MEMBER)

int main(void)
{
    RECORDS(SHOW_LAYOUT, SHOW_MEMBER)
    putchar('\n');
    return 0;
}
