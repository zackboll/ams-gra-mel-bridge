#ifndef AMS_MEL_TEST_ADMISSION_OBSERVATION_H
#define AMS_MEL_TEST_ADMISSION_OBSERVATION_H

#include <ams_mel/abi.h>

/* Test facade only. No declarations are installed with the public API.
 * ams_mel_test_admission operations: 0 bind token to a Session's admission,
 * 1 snapshot active/maximum, 2 release token, 3 wait (bounded, 15 s) until
 * that Session's active count is <= *active on entry; on return *active is
 * the observed count and the result is 0 on timeout. */
typedef struct ams_mel_test_admission_token ams_mel_test_admission_token;
extern int ams_mel_test_admission(
    const ams_mel_session *, unsigned, ams_mel_test_admission_token **,
    uint32_t *, uint32_t *);
extern int ams_mel_test_c2_requests(const ams_mel_ir_c2 *, size_t *);
extern int ams_mel_test_navigation_requests(const ams_mel_ir_stream *, size_t *);
extern int ams_mel_test_instrumentation_requests(
    const ams_mel_ir_instrumentation *, size_t *);
extern int ams_mel_test_track_requests(const ams_mel_ir_track *, size_t *);
extern int ams_mel_test_instrumentation_callbacks(
    const ams_mel_ir_instrumentation_metadata *, size_t *);
extern int ams_mel_test_track_callbacks(const ams_mel_ir_track_metadata *, size_t *);
/* Test-only weak observers. A zero result from requests means expired. */
typedef struct ams_mel_test_instrumentation_observer ams_mel_test_instrumentation_observer;
typedef struct ams_mel_test_track_observer ams_mel_test_track_observer;
extern int ams_mel_test_instrumentation_observer_from(
    const ams_mel_ir_instrumentation *, ams_mel_test_instrumentation_observer **);
extern int ams_mel_test_instrumentation_observer_requests(
    const ams_mel_test_instrumentation_observer *, size_t *);
extern void ams_mel_test_instrumentation_observer_close(
    ams_mel_test_instrumentation_observer **);
extern int ams_mel_test_track_observer_from(
    const ams_mel_ir_track *, ams_mel_test_track_observer **);
extern int ams_mel_test_track_observer_requests(
    const ams_mel_test_track_observer *, size_t *);
extern void ams_mel_test_track_observer_close(ams_mel_test_track_observer **);

#endif
