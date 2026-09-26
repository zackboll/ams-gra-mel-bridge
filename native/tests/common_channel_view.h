#ifndef AMS_MEL_TEST_COMMON_CHANNEL_VIEW_H
#define AMS_MEL_TEST_COMMON_CHANNEL_VIEW_H

#include <ams_mel/abi.h>

/* Test facade only; never installed or added to the production export map. */
typedef struct ams_mel_test_common_channel ams_mel_test_common_channel;
extern ams_mel_status_t ams_mel_test_common_from_c2(
    const ams_mel_ir_c2 *, ams_mel_test_common_channel **);
extern ams_mel_status_t ams_mel_test_common_from_stream(
    const ams_mel_ir_stream *, ams_mel_test_common_channel **);
extern ams_mel_status_t ams_mel_test_common_send_keepalive(
    const ams_mel_test_common_channel *, ams_mel_ir_return_request **,
    char *, size_t, size_t *);
extern ams_mel_status_t ams_mel_test_common_submit_comms_test(
    const ams_mel_test_common_channel *,
    const ams_mel_ir_channel_comms_test_request_v1 *,
    ams_mel_ir_channel_comms_request **, char *, size_t, size_t *);
extern void ams_mel_test_common_close(ams_mel_test_common_channel **);

#endif
