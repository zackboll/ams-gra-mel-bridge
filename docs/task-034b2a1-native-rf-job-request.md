# Task 034B2A1 — native RX Job request and JobDetail ownership

Starting branch SHA: `f364154a5da9496e53cd36ccb1a779166d2729cb`.
The JobDetail declaration closure was already committed: 803/803 vendored
files; this checkpoint does not change any vendor blob. ABI stays 0.1.

Exactly one RX `ElementGroupCommand` is obtained from the provider VA via
`createElementGroupCommand(label)`; null and non-RX modes are rejected. No
bridge-owned subclass or provider command pointer crosses C. The bridge sets
the finite duty factor in (0, 1], adds the caller's ordered finite frequency
ranges (min <= max), and, only for nonempty endpoint IDs, associates their
set with the unchanged UTF-8 data-pipe label. Duplicate endpoint IDs are
rejected *before* calling the provider; the set must not silently collapse
caller input. No Squall-specific frequency restriction is imposed.

The represented JobRequest fields are exact uint32 request ID, priority,
precedence within priority, Boolean interruptable (C input exactly 0/1),
ordered VA instance selections, and the single provider-created RX group.
Unrepresented fields retain their *pinned upstream defaults*: capability ID
and activity ID are each `{0}`; minimum start, maximum completion and
estimated stab point are default-constructed; requested duration and
lookahead are zero femtoseconds; Tx power mode IDs remain `{0}` (not cleared);
sendNextJIBatchCallback and requestRejectedCallback are null, numJIBs is zero,
and callbackContext is nullptr. Multiple groups, TX, pointing inputs, UUIDs,
timing inputs, TX modes and MFA-driven/JIB controls are not exposed.

A private VA helper copies a strong provider VA reference and acquires a NEW
`RfC2ChildClaim` sibling from the public VA's parent. The public VA retains
its original claim. The strong provider VA and the new child claim follow the
pending future into the cached unclaimed JobDetail and then into the claimed
Job owner; C2 shutdown waits for the last child. Only `settle` in `rf_job.cpp`
calls this request family's `future.get()`. The `launch_state` handshake and
intrusive emergency root retain a valid future, provider VA and C2 claim for
the process lifetime if worker launch or publication fails. No cancellation
is inferred. A finite Wait bounds waiting, zero polls, and neither cancels;
terminal results and diagnostics are cached. Request Close is nonblocking:
an abandoned success is destroyed in the worker, then its retained VA is
released, then the C2 claim. Failed deferred shutdown retains the C2/DSO
without retry. Exactly one Claim succeeds. Getter failure after getter start
is cached so no provider getter is repeated.

Claim copies the JobDetail's actual-start integral seconds and fractional
femtoseconds, total duration femtoseconds, VA instance and definition IDs,
Job details ID (independent of Job request ID), Job request ID, lookahead
femtoseconds and ordered `getRxStreamIDs(0)` into an immutable bridge-owned
view. Job Close destroys JobDetail first, provider VA second, then releases
the C2 claim. Public VA and C2 may be closed before the pending Job completes
or before the claimed Job is closed. `finalize()`, `cancelJob()`, intervals,
status callbacks, ProductRx activation and the Squall helper are deferred.
Safe Ada Job APIs and real Squall validation are deferred to 034B2A2.

ABI additions: canonical `ams_mel_u64_span_v1`,
`ams_mel_rf_rx_element_group_config_v1`,
`ams_mel_rf_job_request_config_v1`, `ams_mel_rf_job_result_v1`,
`ams_mel_rf_job_info_v1`, opaque `ams_mel_rf_job_request` and
`ams_mel_rf_job`; six operations are `ams_mel_rf_virtual_aperture_submit_job`,
`ams_mel_rf_job_request_wait`, `ams_mel_rf_job_request_claim`,
`ams_mel_rf_job_request_close`, `ams_mel_rf_job_view`, and
`ams_mel_rf_job_close`. Raw Ada, Rust sys and private Python declarations
mirror these shapes; no safe Rust or public Python Job wrapper is added.