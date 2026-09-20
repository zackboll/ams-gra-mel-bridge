# Task 027B validation

Task 027B implements `ImageChannel::send(NavigationReport)` as an asynchronous
request/future vertical slice through the shared `ImageStreamState` graph
established by Task 027A. ABI 0.1 grows from 56 to exactly 59 exports.

> **Correction notice.** Retrospective peer review of the merged PR found a
> defect in the deferred-teardown synchronization described below: the
> asynchronous Navigation completion worker could read and reset
> `ImageStreamState::channel` concurrently with a public Stop/Close (a
> `std::shared_ptr` data race), and a bad interleaving could make `Close`
> return a stale `AMS_MEL_OK` after a deferred `detachChannel` had actually
> failed. The deferred-teardown *design* recorded here is retained; its
> synchronization is corrected. See
> `docs/corrective-image-navigation-close-race.md`, which supersedes the
> teardown-synchronization statements in this document.

## Native C

`ams_mel_navigation_report_v1` carries the complete published
`mel::NavigationReport`, including all 18 named
`ams_mel_position_velocity_covariance_v1` terms and the complete
`ams_mel_attitude_rate_v1`/`ams_mel_north_east_down_v1` substructures. Only
`state >= AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE` is rejected; every other
floating-point input, including negative, NaN, or infinite values, is copied
as-is. `ams_mel_ir_stream_submit_navigation_report`,
`ams_mel_ir_navigation_request_wait`, and
`ams_mel_ir_navigation_request_close` follow the C2
mode-request pattern exactly: preallocate every normal-path object before
calling provider `send()`, account the request under the frame callback mutex
before calling `send()` (never while holding it during the call), a detached
worker calls `future.get()` exactly once and caches a terminal result
permanently, and `Close` is nonblocking, idempotent, and not cancellation.

`ImageStreamState` gains `requests`, `cleanup_started`, and
`public_owner_closed`, all guarded by `CallbackState::mutex`, plus an
allocation-free `emergency_self`/`emergency_next`/`emergency_retained`
retention root mirroring C2's `ChannelState::retain_failed`. Logical Stop/Close
always executes immediately: it stops accepting new frames, transitions frame
lifecycle to Stopping/Failed, and rejects new Navigation submissions. Physical
provider teardown (disable/detach/channel destruction/host storage release) is
deferred to final request completion whenever `requests != 0`; the final
completed request performs it exactly once via the shared `cleanup_started`
guard. `AMS_MEL_TEST_IMAGE_NAVIGATION_POST_SEND_FAILURE` injects an allocation
or worker-launch failure after a provider future exists; both leave the
provider future/channel/ImageStreamState permanently retained (never unloading
provider code) and `out_request` NULL.

## Native tests

`native/tests/test_ir_navigation.c` is registered as CTest target
`ir_navigation_contract`, bringing the native suite to 9/9. It covers: rich
input fidelity round-tripped through the mock provider's getters; rich
response system_time/command_id/request_id; submission before Start (valid
while Attached) and after Stop (rejected); invalid `PositionSolutionState`
values (`MAX_EXCLUSIVE` and beyond); null-argument and pre-populated
`out_request` rejection; provider `send()` throwing; a null successful
`shared_ptr<NavigationReportResp>`; provider rejection with an exact
description; a long (>510-byte) UTF-8 rejection requiring the cached-retry
diagnostic path; a future `std::exception`; both post-send failpoints with
lifetime-log evidence that the provider channel/library are never torn down
while the request remains pending; Session-close-first lifetime with cached
Wait(0) after completion; request-Close-while-pending followed by
Image_Stream/Session close, deferring physical teardown until the delayed
future completes (`navigation_sent` -> `navigation_completed` ->
`channel_destroyed` -> `library_unloaded`); a synchronous
`NavigationReportResp` metadata-callback proof with identical
callback/future response fields; and two simultaneous requests on one stream,
requiring both to complete before Close's deferred cleanup runs.

All 9 native CTest targets pass. `ir_navigation_contract` passed 50/50 under
`--repeat until-fail:50`; `ir_stream_contract` and
`ir_image_metadata_contract` (whose shared Image teardown logic changed again)
each passed 50/50 under the same repeat count.

## Safe Ada

`AMS.MEL.IR.Image` gains `Position_Solution_State` (representation `0..4`,
`MaxExclusive` intentionally not a valid safe value),
`North_East_Down`, `Attitude_Rate` (reusing `AMS.MEL.Status.Euler`), an
explicitly-named `Position_Velocity_Covariance` (no anonymous array),
`Navigation_Report`, `Navigation_Outcome`/`Navigation_Error_Code`, a private
`Navigation_Result`, and a limited-controlled `Navigation_Request` with
`Submit_Navigation_Report`/`Is_Open`/`Wait`/`Close`. Private C imports live in
`AMS.MEL_C_API` using `Interfaces.C.double` for every C `double`,
`Interfaces.Integer_64` for signed nanoseconds, and `Interfaces.Unsigned_32`
for `uint32_t` fields, matching the existing repository convention.

The canonical `Navigation_Response` moved from
`AMS.MEL.IR.Image.Metadata` to `AMS.MEL.IR.Image`;
`AMS.MEL.IR.Image.Metadata.Navigation_Response` is now a source-compatible
`subtype`, and `Navigation_Response_Value` is unchanged at its existing
location.

`ada/tests/src/ams_mel_ir_image_navigation_tests.adb` proves: complete rich
NavigationReport fidelity (including all 18 covariance terms) round-tripped
through the mock provider and exact response fields; provider rejection with
an exact `Invalid_Parameters`/description match; `Wait(0)` timing out on a
delayed request, then a later finite `Wait` succeeding, then `Wait(0)` again
returning an identical cached response; and Session-close-first followed by
Image_Stream-close, with the request surviving both and completing
successfully afterward. It is wired into `ams_mel_smoke.adb` alongside the
other Ada contract suites; all 9 Ada smoke suites pass, and `alr -C ada build`
and `alr -C ada/tests run` both succeed cleanly.

## Rust and Python

`rust/ams-mel-sys` gains raw declarations for the `PositionSolutionState`
constants, `AmsMelNorthEastDownV1`, `AmsMelAttitudeRateV1`,
`AmsMelPositionVelocityCovarianceV1`, `AmsMelNavigationReportV1`,
`AmsMelIrNavigationResultV1`, the opaque `AmsMelIrNavigationRequest`, and the
three new functions. `tests/abi_probe.c` and `tests/abi.rs` were extended with
matching constant values, sizes/alignments/every-field-offset checks, and
function-signature checks; `cargo test --workspace` passes, including
`declarations_match_the_c_header`, confirming raw Rust exactly tracks the
59-function native ABI. No safe Rust Navigation API was added.

`python/ams_mel/_native.py` gains the equivalent private ctypes
records/constants and the opaque `IrNavigationRequestHandle`, appended to
`BOUND_FUNCTION_NAMES` (now 59 entries).
`python/tests/abi_probe.c`/`test_abi.py` were extended identically to Rust;
`make test-python` passes, including
`test_ctypes_declarations_match_authoritative_c_header` and
`test_private_layer_binds_complete_current_facade`. No public Python
Navigation API was added.

## Real Squall

Pinned Squall implements `send(NavigationReport)` by copying
`report.systemTime` into the response `systemTime` and zeroing both
`commandID` and `reqId`; when a `NavigationReportResp` callback is registered
it invokes it synchronously inside `send()` before the future completes. Real
Squall therefore proves: the provider send path, the synchronous
`NavigationReportResp` metadata callback, future completion, timestamp
propagation, and callback/future correlation. It does not exercise
`NavigationReport` fields Squall ignores (state, position, attitude, rates,
speed, acceleration, wander angle, magnetic heading, altitude MSL, or the 18
covariance terms) -- those remain proven only by the mock provider's exact
getter-level fidelity check.

The Ada real-Squall integration submits a complete `Navigation_Report` while
the Image stream is logically Attached (intentionally before `Start`), drains
Image metadata for the `Navigation_Response_Event` (registration-time LOS
events may precede it), and waits for the request. It requires: metadata
callback `System_Time_NS`/`Command_ID`/`Request_ID` equal to the submitted
timestamp and zero/zero; future `Response` equal to the metadata callback
response; and a cached `Wait(request, 0)` after completion returning an
identical response. Existing frame/FrameHeader/C2/Health Squall coverage
continues to pass unchanged.

## Scope

No optional Image functionality (CameraCommand/CameraCommandResp,
LineOfSightQuaternion, LOS3D_Kinematics, OpticalDistortionMap,
CandidateObject*, NUC_TempData) was added. No Scheduling, Track, or RF feature
was added. No safe Rust or public Python Navigation API was added. No vendored
upstream source was modified.
