#ifndef AMS_MEL_TEST_ADMISSION_OBSERVATION_H
#define AMS_MEL_TEST_ADMISSION_OBSERVATION_H

#include <ams_mel/abi.h>

/* Test facade only. No declarations are installed with the public API. */
typedef struct ams_mel_test_admission_token ams_mel_test_admission_token;
extern int ams_mel_test_admission(
    const ams_mel_session *, unsigned, ams_mel_test_admission_token **,
    uint32_t *, uint32_t *);
extern int ams_mel_test_c2_requests(const ams_mel_ir_c2 *, size_t *);
extern int ams_mel_test_navigation_requests(const ams_mel_ir_stream *, size_t *);
extern int ams_mel_test_instrumentation_requests(
    const ams_mel_ir_instrumentation *, size_t *);
extern int ams_mel_test_track_requests(const ams_mel_ir_track *, size_t *);

#endif
