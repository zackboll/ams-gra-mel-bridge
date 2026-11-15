#include <ams_mel/abi.h>
#include <stdio.h>
typedef ams_mel_status_t (*open_sig)(ams_mel_ir_stream *, size_t, size_t,
    ams_mel_ir_image_checkout **, char *, size_t, size_t *);
typedef ams_mel_status_t (*receive_sig)(ams_mel_ir_image_checkout *, uint32_t,
    ams_mel_ir_image_checkout_lease **, char *, size_t, size_t *);
typedef ams_mel_status_t (*counters_sig)(const ams_mel_ir_image_checkout *,
    ams_mel_ir_image_checkout_counters_v1 *, char *, size_t, size_t *);
typedef ams_mel_status_t (*close_sig)(ams_mel_ir_image_checkout **, char *, size_t, size_t *);
typedef ams_mel_status_t (*lease_sig)(ams_mel_ir_image_checkout_lease **, uint32_t *, char *, size_t, size_t *);
_Static_assert(_Generic(&ams_mel_ir_image_checkout_open, open_sig: 1, default: 0), "open");
_Static_assert(_Generic(&ams_mel_ir_image_checkout_receive, receive_sig: 1, default: 0), "receive");
_Static_assert(_Generic(&ams_mel_ir_image_checkout_get_counters, counters_sig: 1, default: 0), "counters");
_Static_assert(_Generic(&ams_mel_ir_image_checkout_close, close_sig: 1, default: 0), "close");
_Static_assert(_Generic(&ams_mel_ir_image_checkout_lease_close, lease_sig: 1, default: 0), "lease");
int main(void)
{
    typedef ams_mel_ir_image_checkout_counters_v1 S;
    printf("%zu %zu %zu %zu %zu %zu %zu %zu\n", sizeof(S), _Alignof(S),
        offsetof(S, callbacks_received), offsetof(S, checkouts_accepted), offsetof(S, queue_overflow),
        offsetof(S, callbacks_rejected), offsetof(S, releases_succeeded), offsetof(S, releases_uncertain));
    return 0;
}
