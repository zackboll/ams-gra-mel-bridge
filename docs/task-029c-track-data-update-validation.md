# Task 029C TrackDataUpdate validation

Task 029C implements exactly `TrackChannel::send(TrackDataUpdate)`
(`@RequiredIfTrackUpdate`) over the existing Track channel/report foundation, in
native C, safe Ada, raw Rust ABI, and private Python ABI.

Correct coverage language:

- `@RequiredIfTrack` core complete.
- `@RequiredIfTrackUpdate` `TrackDataUpdate` send complete.

This task does not describe `TrackDataUpdate` as part of `@RequiredIfTrack`
itself; they are distinct upstream conditions.

## Upstream

Pinned exactly as reviewed in Task 029A:

- IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`
- Common MEL `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`

`TrackDataUpdate` was already vendored, because `TrackChannel.h` already
included `TrackDataUpdate.h`. The vendor delta for this task is zero, and the
pinned Boost closure probe (`track_header_compile_probe`,
`check_track_header_boost_closure`) is preserved with no system Boost.

## TrackStatus

Pinned upstream values, with no MaxExclusive value:

| Upstream | C constant | Value | Ada literal |
|---|---|---|---|
| `Create` | `AMS_MEL_IR_TRACK_STATUS_CREATE` | 0 | `Create` |
| `Update` | `AMS_MEL_IR_TRACK_STATUS_UPDATE` | 1 | `Update` |
| `Predict` | `AMS_MEL_IR_TRACK_STATUS_PREDICT` | 2 | `Predict` |
| `Delete` | `AMS_MEL_IR_TRACK_STATUS_DELETE` | 3 | `Delete` |

Input values greater than `Delete` are `AMS_MEL_INVALID_ARGUMENT`.

## Complete TrackDataUpdate mapping

Every upstream setter is called exactly once with the corresponding C field,
using the published setters only; no provider object layout is assumed.

| Upstream | C field | Semantics |
|---|---|---|
| `platformId` | `platform_id` | `uint32` |
| `capabilityUUID` | `capability_uuid` | `ams_mel_uci_id_v1` |
| `activityUUID` | `activity_uuid` | `ams_mel_uci_id_v1` |
| `trackId` | `track_id` | `uint32` |
| `entityUUID` | `entity_uuid` | `ams_mel_uci_id_v1` |
| `trackStatus` | `track_status` | `ams_mel_ir_track_status_t` |
| `timeOfValidity` | `time_of_validity_seconds` | epoch seconds, NOT nanoseconds |
| `timeOfLastUpdate` | `time_of_last_update_seconds` | epoch seconds, NOT nanoseconds |
| `trackPosition` | `track_position_ecef` | canonical `ams_mel_ir_directional_v1`, ECEF |
| `trackVelocity` | `track_velocity_ecef` | canonical `ams_mel_ir_directional_v1`, ECEF |
| 21 covariance setters | `covariance` | `ams_mel_ir_track_covariance_v1` |
| `maneuverProbability` | `maneuver_probability` | verbatim `double` |
| `trackQuality` | `track_quality` | verbatim `double` |

The 21 covariance terms map one-to-one and are individually verified, so a
swapped pair is detectable: `XX`/`xx`, `XY`/`xy`, `XZ`/`xz`, `XVx`/`x_vx`,
`XVy`/`x_vy`, `XVz`/`x_vz`, `YY`/`yy`, `YZ`/`yz`, `YVx`/`y_vx`, `YVy`/`y_vy`,
`YVz`/`y_vz`, `ZZ`/`zz`, `ZVx`/`z_vx`, `ZVy`/`z_vy`, `ZVz`/`z_vz`,
`VxVx`/`vx_vx`, `VxVy`/`vx_vy`, `VxVz`/`vx_vz`, `VyVy`/`vy_vy`,
`VyVz`/`vy_vz`, `VzVz`/`vz_vz`.

Nothing is clamped or normalized -- not `maneuverProbability`, not
`trackQuality`, not the covariance terms, not the position/velocity components,
and not the time values -- because the upstream setters perform no such
validation. Every borrowed UCI label is validated as UTF-8 without an embedded
NUL and copied before Submit returns, so no borrowed application string
outlives the submit call.

## Async ownership

Three new exports take ABI 0.1 from 82 to exactly 85:

```text
ams_mel_ir_track_submit_update
ams_mel_ir_track_update_request_wait
ams_mel_ir_track_update_request_close
```

The opaque `ams_mel_ir_track_update_request` owns a shared terminal completion
state and never a raw `ams_mel_ir_track` pointer. Exactly one completion worker
calls `future.get()`; the terminal outcome is cached permanently, so repeated
Wait calls return the identical result. A finite Wait timeout means only "not
ready yet": it is not cancellation, not request consumption, and not provider
interruption, and it leaves the caller's result record untouched. `Wait(0)` is a
poll. Request close is idempotent, nonblocking, and not cancellation.

Submission requires Track lifecycle `Enabled`; attached-but-not-Enabled, failed,
and closed all report `AMS_MEL_PROVIDER_FAILED`. The `TrackState` mutex is
released before the provider send, because a provider may invoke the registered
`IRSTTrackReport` callback synchronously from inside `send()`. Everything needed
to own the returned future is allocated before the send.

## Result semantics

`ams_mel_ir_track_update_result_v1` reuses the existing generic
`ams_mel_ir_command_status_v1`. A successful `CommandStatus` whose own state is
`Rejected` is STILL `AMS_MEL_OK`; only an `ErrorOr` rejection is
`AMS_MEL_COMMAND_REJECTED`. On `AMS_MEL_OK`, `status.reason_description` points
into immutable request-owned cached storage valid through repeated Waits until
`request_close`, never into provider-owned memory; safe Ada copies it into
Ada-owned storage immediately.

Provider output is validated completely: non-null `shared_ptr<CommandStatus>`,
`CommandState <= Cancelled`, `CannotComply <= Alignment_Maneuver`, and a valid
UTF-8/no-embedded-NUL `reasonDescription`. Anything else is
`AMS_MEL_PROVIDER_FAILED`. All nine published MEL `ErrorCode` values are mapped
through the existing mapping; an unknown code is `AMS_MEL_PROVIDER_FAILED`.

## Track lifetime

Track Close moves the lifecycle to Closed and immediately deactivates public
Track metadata consumption and wakes metadata receivers. With no pending
requests it performs the existing synchronous cleanup, and the Task 029B1/B2
synchronous detach-failure semantics are unchanged: `AMS_MEL_PROVIDER_FAILED`,
the public owner stays non-null, and retry is allowed. With pending requests it
clears the public Track owner, returns `AMS_MEL_OK`, and defers physical
provider teardown to final request completion; the request owns the
`TrackState` graph meanwhile.

On final completion the worker decrements `requests`, and at zero with a Closed
lifecycle performs the physical cleanup: disable if attempted, detach, destroy
the provider `TrackChannel`, establish callback quiescence, and release the
provider/session graph. If the public owner is already gone and that deferred
detach fails, the complete graph is retained through the existing
allocation-free emergency root, the request's terminal result becomes
`AMS_MEL_PROVIDER_FAILED` with the diagnostic `deferred Track cleanup failed`,
and provider code is never unloaded with uncertain detach ownership.

## Native contract test

`native/tests/test_ir_track_update.c`, CTest name `ir_track_update_contract`,
taking the native CTest count from 12 to 13. It covers exact complete input
fidelity including all 21 covariance terms, invalid `TrackStatus`, invalid UCI
labels, submission before Enable, the rich successful `CommandStatus`, a
successful `CommandStatus` with `state = Rejected` remaining `AMS_MEL_OK`,
`ErrorOr` rejection with full diagnostic retry, timeout then later success,
repeated cached `Wait(0)`, null successful status, unknown command state,
unknown cannot-comply, invalid UTF-8 status reason, unknown MEL error code, send
exception, future exception, Track Close while pending, Session parent-first,
request survival of its parent owners, request close not being cancellation, a
synchronous `IRSTTrackReport` callback from inside `send(TrackDataUpdate)`, both
post-send failpoints, synchronous detach failure, deferred detach failure
emergency retention, and provider unload ordering.

The pending future is deterministic: the mock background completion waits on a
test-controlled barrier file rather than a sleep, the test observes that the
send occurred, `Wait(0)` returns `AMS_MEL_TIMEOUT`, the test releases the
barrier, and a later Wait returns the cached terminal result. Finite watchdogs
exist only to prevent hanging tests.

## Mock scope

Only `send(TrackDataUpdate)` became newly positive. The `IRSTTrackReport`
callback stays positive, `send(SystemTrackDataResponse)` stays Unsupported, and
`CandidateObjectMessage`, `RequestSystemTrackData`, and
`CandidateObjectPreProcMessage` stay `Return::NotSupported`. `TrackDataUpdate`
send is no longer counted as a deferred-operation violation; the remaining
deferred Track surfaces are still provably unused.

## Evidence boundary

- `TrackDataUpdate` positive behavior/payload: mock provider only.
- Pinned Squall: unsupported Track Open only.

The Track expected-negative integration probe is unchanged, and this task
introduces no new positive real-Squall Track claim.

## Still not implemented

`SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData`. No safe Rust
Track API and no public Python Track API were added. The entire Track API is
NOT marked complete.
