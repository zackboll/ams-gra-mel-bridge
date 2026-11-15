#include <ams_mel/abi.h>
#include <stdio.h>

typedef ams_mel_status_t (*open_signature)(ams_mel_session *, uint64_t, int64_t,
    ams_mel_ir_buffer **, uint32_t *, char *, size_t, size_t *);
typedef ams_mel_status_t (*info_signature)(ams_mel_ir_buffer *,
    ams_mel_ir_buffer_info_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*get_signature)(ams_mel_ir_buffer *, uint32_t *, size_t,
    size_t *, uint32_t *, char *, size_t, size_t *);
typedef ams_mel_status_t (*set_signature)(ams_mel_ir_buffer *, const uint32_t *,
    size_t, char *, size_t, size_t *);
typedef ams_mel_status_t (*add_signature)(ams_mel_ir_buffer *, uint32_t,
    char *, size_t, size_t *);
typedef ams_mel_status_t (*read_signature)(ams_mel_ir_buffer *, uint64_t, uint8_t *,
    size_t, char *, size_t, size_t *);
typedef ams_mel_status_t (*write_signature)(ams_mel_ir_buffer *, uint64_t, const uint8_t *,
    size_t, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_signature)(ams_mel_ir_buffer **, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_buffer_open, open_signature: 1, default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_buffer_get_info, info_signature: 1, default: 0), "info");
_Static_assert(_Generic(&ams_mel_ir_buffer_get_flags, get_signature: 1, default: 0), "get flags");
_Static_assert(_Generic(&ams_mel_ir_buffer_set_flags, set_signature: 1, default: 0), "set flags");
_Static_assert(_Generic(&ams_mel_ir_buffer_add_flag, add_signature: 1, default: 0), "add flag");
_Static_assert(_Generic(&ams_mel_ir_buffer_read_storage, read_signature: 1, default: 0), "read");
_Static_assert(_Generic(&ams_mel_ir_buffer_write_storage, write_signature: 1, default: 0), "write");
_Static_assert(_Generic(&ams_mel_ir_buffer_close, close_signature: 1, default: 0), "close");

int main(void)
{
    typedef ams_mel_ir_buffer_info_v1 S;
    printf("%zu %zu %zu %zu %zu %zu %zu %zu %u %u %u\n",
        sizeof(S), _Alignof(S), offsetof(S, storage_size),
        offsetof(S, initialized_context), offsetof(S, provider_size),
        offsetof(S, provider_context), offsetof(S, buffer_address_matches),
        offsetof(S, image_location), AMS_MEL_IR_BUFFER_HOST,
        AMS_MEL_IR_BUFFER_GPU, AMS_MEL_IR_BUFFER_RDMA);
    return 0;
}
