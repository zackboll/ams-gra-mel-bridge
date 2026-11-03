#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
/* IRMEL-ADA-008: exact C2 SystemTrackDataResponse signatures and layouts for the
 * private Python/Rust mirrors. The input reuses the frozen Track record. */
typedef ams_mel_status_t (*submit_signature)(ams_mel_ir_c2 *,
    const ams_mel_ir_system_track_data_response_v1 *, ams_mel_ir_c2_system_response_request **,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*wait_signature)(const ams_mel_ir_c2_system_response_request *, uint32_t,
    ams_mel_ir_c2_system_response_result_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_c2_system_response_request **, char *,
    size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_c2_submit_system_track_data_response, submit_signature: 1,
               default: 0), "submit signature");
_Static_assert(_Generic(&ams_mel_ir_c2_system_response_request_wait, wait_signature: 1,
               default: 0), "wait signature");
_Static_assert(_Generic(&ams_mel_ir_c2_system_response_request_close, close_signature: 1,
               default: 0), "close signature");
int main(void) {
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_system_track_data_response_v1),
        _Alignof(ams_mel_ir_system_track_data_response_v1),
        offsetof(ams_mel_ir_system_track_data_response_v1, system_time_ns),
        offsetof(ams_mel_ir_system_track_data_response_v1, command_id),
        offsetof(ams_mel_ir_system_track_data_response_v1, request_id),
        offsetof(ams_mel_ir_system_track_data_response_v1, track_id),
        offsetof(ams_mel_ir_system_track_data_response_v1, range_m),
        offsetof(ams_mel_ir_system_track_data_response_v1, range_rate_mps),
        offsetof(ams_mel_ir_system_track_data_response_v1, range_error_m),
        offsetof(ams_mel_ir_system_track_data_response_v1, range_rate_error_mps),
        offsetof(ams_mel_ir_system_track_data_response_v1, az_el_valid),
        offsetof(ams_mel_ir_system_track_data_response_v1, range_valid),
        offsetof(ams_mel_ir_system_track_data_response_v1, inertial_az_el),
        offsetof(ams_mel_ir_system_track_data_response_v1, az_el_error),
        sizeof(ams_mel_ir_c2_system_response_result_v1),
        _Alignof(ams_mel_ir_c2_system_response_result_v1),
        offsetof(ams_mel_ir_c2_system_response_result_v1, status),
        offsetof(ams_mel_ir_c2_system_response_result_v1, error_code),
        sizeof(ams_mel_ir_command_status_v1),
        offsetof(ams_mel_ir_command_status_v1, reason_description));
    return 0;
}
