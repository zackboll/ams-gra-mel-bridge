#include <ams_mel/abi.h>
#include <stddef.h>
#include <stdio.h>
typedef ams_mel_status_t (*submit_signature)(ams_mel_ir_c2 *, const ams_mel_ir_erase_command_v1 *,
    ams_mel_ir_erase_request **, char *, size_t, size_t *);
typedef ams_mel_status_t (*wait_signature)(const ams_mel_ir_erase_request *, uint32_t,
    ams_mel_ir_erase_result_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_erase_request **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_c2_submit_erase, submit_signature: 1, default: 0), "submit signature");
_Static_assert(_Generic(&ams_mel_ir_erase_request_wait, wait_signature: 1, default: 0), "wait signature");
_Static_assert(_Generic(&ams_mel_ir_erase_request_close, close_signature: 1, default: 0), "close signature");
int main(void) {
    printf("%zu %zu %zu %zu %zu %zu %zu %zu\n",
        sizeof(ams_mel_ir_erase_command_v1), _Alignof(ams_mel_ir_erase_command_v1),
        offsetof(ams_mel_ir_erase_command_v1, command_id), offsetof(ams_mel_ir_erase_command_v1, erase_type),
        sizeof(ams_mel_ir_erase_result_v1), _Alignof(ams_mel_ir_erase_result_v1),
        offsetof(ams_mel_ir_erase_result_v1, erase_type), offsetof(ams_mel_ir_erase_result_v1, error_code));
    return 0;
}
