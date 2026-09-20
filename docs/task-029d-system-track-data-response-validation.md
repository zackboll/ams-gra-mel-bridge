# Task 029D SystemTrackDataResponse validation

Task 029D implements exactly `TrackChannel::send(SystemTrackDataResponse)`
(`@Optional`) over the existing Track channel/report/update foundation, in
native C, safe Ada, raw Rust ABI, and private Python ABI.

Correct coverage language:

- `@RequiredIfTrack` core complete.
- `@RequiredIfTrackUpdate` `TrackDataUpdate` send complete.
- Optional `SystemTrackDataResponse` send complete.

This operation is `@Optional`. It is deliberately NOT described as
`@RequiredIfTrack` or `@RequiredIfTrackUpdate`; all three are distinct upstream
conditions. The entire Track API is NOT marked complete.

## Upstream

Pinned exactly as reviewed in Task 029A:

- IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`
- AMS Math `00be45190f0e47d268cece8b8c2f8fb58b5418d2`

`SystemTrackDataResponse` and `RangeAzEl` were already vendored, because
`TrackChannel.h` already included `SystemTrackDataResponse.h`, which includes
`math/geometry/RangeAzEl.h`. The vendor delta for this task is zero, nothing
under `native/vendor/` or `docs/upstream-files.sha256.md` changed, and the
pinned Boost closure probe (`track_header_compile_probe`,
`check_track_header_boost_closure`) is preserved at 481 headers with no system
Boost.

## Complete SystemTrackDataResponse mapping

Every upstream setter is called exactly once with the corresponding C field,
using the published setters only; no provider object layout is assumed.

| Upstream | C field | Semantics |
|---|---|---|
| `systemTime` | `system_time_ns` | signed `chrono::nanoseconds` count |
| `commandID` | `command_id` | `uint32` |
| `requestId` | `request_id` | `uint32` |
| `trackId` | `track_id` | `uint32` |
| `range` | `range_m` | verbatim `double`, meters |
| `rangeRate` | `range_rate_mps` | verbatim `double`, meters/sec |
| `rangeError` | `range_error_m` | verbatim `double`, meters |
| `rangeRateError` | `range_rate_error_mps` | verbatim `double`, meters/sec |
| `AzElValid` | `az_el_valid` | `uint8_t`, only 0 or 1 |
| `inertialAzEl` | `inertial_az_el` | canonical `ams_mel_ir_az_el_v1`, radians |
| `AzElError` | `az_el_error` | canonical `ams_mel_ir_az_el_v1`, radians |
| `rangeValid` | `range_valid` | `uint8_t`, only 0 or 1 |

Nothing is clamped or normalized -- not the range, not the range rate, not
either error term, and not any azimuth or elevation -- because the upstream
setters perform no such validation. The system time stays signed nanoseconds
and is deliberately not converted to seconds.

### AzEl fidelity

Pinned upstream `AzEl` is exactly `double az; double el;`, both radians. The
existing canonical `ams_mel_ir_az_el_v1` (`azimuth_rad`, `elevation_rad`) is
reused for both pairs; no second azimuth/elevation representation was created.
The mock verifies `inertialAzEl` and `AzElError` independently, so a swapped
`az`/`el` or a swapped pair is detectable.

### Boolean validation

`az_el_valid` and `range_valid` use the repository's established `uint8_t`
representation for published bool values and accept only `0` or `1`. Each is
rejected independently with `AMS_MEL_INVALID_ARGUMENT`, for both `2` and `255`,
so "non-zero is true" is provably not the rule and a shared check cannot hide
one of them. The `false`/`false` combination is exercised positively with
otherwise identical distinctive numbers.

## Async ownership

Three new exports take ABI 0.1 from 85 to exactly 88:

```text
ams_mel_ir_track_submit_system_track_data_response
ams_mel_ir_track_system_response_request_wait
ams_mel_ir_track_system_response_request_close
```

The ABI version itself remains `0.1`.

`ams_mel_ir_track_system_response_request` is a deliberately distinct public
opaque owner: the `TrackDataUpdate` request type is never exposed under a
System-response name. `ams_mel_ir_track_system_response_result_v1` likewise
matches the Track update result shape -- both upstream operations return
`RequestFor<CommandStatus>` -- while remaining a semantically distinct public
type rather than an alias.

Internally both families share exactly one async lifecycle model: the Task 029C
`CompletionKind`, `Completion`, `WorkerInput`, `complete(...)`,
`finish_request(...)`, `retain_worker(...)`, and `TrackState::requests`. No
second async model was created. `Completion` now stores the terminal value as
the neutral `ams_mel_ir_command_status_v1`/`ams_mel_error_code_t` pair rather
than as either public result record, so neither public layout is assumed to
equal the other.

Submission requires the Track lifecycle to be `Enabled`, exactly as
`TrackDataUpdate` does: attached-only, failed, and closed all report
`AMS_MEL_PROVIDER_FAILED`. The Track lifecycle mutex is never held across the
provider send.

## CommandStatus semantics

Reused exactly from Task 029C. A successful future requires a non-null
`CommandStatus`; `CommandState` above `Cancelled` and `CannotComply` above
`Alignment_Maneuver` are malformed, because upstream declares no `MaxExclusive`
value for either; the reason description must be valid UTF-8 without an embedded
NUL and is copied into request-owned cached storage.

A successful `CommandStatus` whose own state is `Rejected` is still
`AMS_MEL_OK`: `CommandStatus::Rejected` is not an `ErrorOr` rejection. Only an
`ErrorOr` failure becomes `AMS_MEL_COMMAND_REJECTED`. All nine published MEL
`ErrorCode` values map through the existing mapping; an unknown code is
`AMS_MEL_PROVIDER_FAILED`.

## Wait and Close

`Wait` is finite and a zero timeout polls. `AMS_MEL_TIMEOUT` means only "not
ready yet": it is never cancellation, the request stays pending, and the
caller's result record is left untouched (verified by pre-filling it with
`0xAB` and checking it afterwards). Exactly one adapter worker calls
`future::get()`. A terminal value is cached permanently, so repeated `Wait(0)`
calls return the identical terminal result and may use a differently sized
diagnostic buffer. `reason_description` is request-owned and valid until request
close.

Request `Close` drops only the public owner. It is idempotent, nonblocking, and
not cancellation: pending provider work, the future, the `TrackState` graph, and
the provider library all survive. No raw `ams_mel_ir_track *` is ever retained.

## One shared request-accounting domain

`SystemTrackDataResponse` requests participate in the SAME `TrackState::requests`
count as `TrackDataUpdate` requests. No second counter was added.

Track `Close` with either request family pending moves the lifecycle to Closed,
deactivates public Track metadata and wakes receivers, releases the public Track
owner, returns `AMS_MEL_OK`, and defers physical teardown. Final completion
performs teardown only when the total shared request count reaches zero.

The decisive regression is `test_mixed_request_accounting`: one pending
`TrackDataUpdate` and one pending `SystemTrackDataResponse` on one Track, each
with its own barrier. After closing Session and Track the provider graph is
still alive. Releasing only the first future and observing its terminal result
still leaves `track_disabled`, `channel_detached`, `track_channel_destroyed`,
and `library_unloaded` all absent, because one request remains. Only after the
second future is released does teardown occur. Two independent counters would
fail this test.

## Native contract test

`native/tests/test_ir_track_system_response.c`, CTest name
`ir_track_system_response_contract`, taking the native CTest count from 13 to
14. It covers exact complete input fidelity, the `false`/`false` boolean
combination, independent rejection of `az_el_valid` and `range_valid`,
submission before Enable, the rich successful `CommandStatus`, a successful
`CommandStatus` with `state = Rejected` remaining `AMS_MEL_OK`, `ErrorOr`
rejection with full diagnostic retry, null successful status, unknown command
state, unknown cannot-comply, invalid UTF-8 reason, unknown MEL error code, send
exception, future exception, timeout then later success, repeated cached
`Wait(0)`, pending-response lifetime across Session/Track close, the
mixed-request regression, a synchronous `IRSTTrackReport` callback from inside
`send(SystemTrackDataResponse)`, request close not being cancellation, deferred
detach failure, and both post-send failpoints.

Every pending future is deterministic: the mock background completion waits on a
test-controlled barrier file rather than a sleep. Finite watchdogs exist only to
prevent hanging tests.

## Metadata reentrancy

`track-response-reentrant` invokes the already-registered `IRSTTrackReport`
callback synchronously from inside `send(SystemTrackDataResponse)` and before
the future is returned. This is why the provider send occurs outside
`TrackState::mutex`. The test proves no deadlock, that the Track report is
received correctly through the normal polling queue, and that the response
request still completes correctly.

## Post-send ownership failure

The submit uses the same fail-safe launch pattern as `TrackDataUpdate` through a
shared internal failpoint mechanism, under its own
`AMS_MEL_TEST_TRACK_RESPONSE_POST_SEND_FAILURE` variable so the existing
`TrackDataUpdate` post-send tests are untouched and unweakened. Both the
allocation and worker-launch failpoints prove that the provider send already
occurred, the facade returns `AMS_MEL_INTERNAL_ERROR`, no public request owner
escapes, and the future/provider graph is retained permanently rather than
destroyed with uncertain ownership -- the library is NOT unloaded.

## Deferred detach failure

Because the same request-accounting/cleanup engine is reused, one response case
proves the shared behavior: the public Track owner is already gone, the final
response completion attempts the deferred detach, the detach fails, the complete
graph is retained permanently through the existing allocation-free emergency
root, and the request's terminal result becomes `AMS_MEL_PROVIDER_FAILED` with
the diagnostic `deferred Track cleanup failed`. Synchronous detach-retry
behavior is not modified by this task and remains owned by the `TrackDataUpdate`
contract test.

## Safe Ada

`AMS.MEL.IR.Track.System_Data` exposes `Azimuth_Elevation`,
`System_Track_Data_Response`, an Ada-owned `Command_Status` with
`Command_ID`/`State`/`Reason`/`Reason_Description`, `Response_Outcome`,
`Response_Error_Code`, `Response_Result`, and a controlled `Response_Request`
with `Submit`, `Is_Open`, `Wait`, and `Close`. The reason description is copied
into Ada-owned storage during `Wait`, so no C pointer escapes that call.

This package is the natural future home for the `RequestSystemTrackData`
callback, but that callback is deliberately NOT implemented here.

> **Superseded by Task 029E:** inspection of the pinned `TrackChannel` showed
> `RequestSystemTrackData` is an inbound metadata callback -- it is declared
> only as a `registerMetadataCallback` overload, with no `send()` overload --
> so its implemented Ada home is `AMS.MEL.IR.Track.Metadata` rather than
> `System_Data`. The prediction above records what Task 029D believed at the
> time and is left unmodified; see
> `docs/task-029e-request-system-track-data-validation.md` for the
> authoritative explanation.

The private FFI uses `Interfaces.Integer_64`, `Interfaces.Unsigned_32`,
`Interfaces.Unsigned_8`, and `Interfaces.C.double`, and reuses `C.Az_El_V1` and
`C.IR_Command_Status_V1`. No C `double` is modeled directly as `Long_Float`
inside an imported record.

`Command_State` and `Cannot_Comply` are package-local with explicit
representation clauses identical to Task 029C and `Size => 32`; the package
deliberately does not depend on `AMS.MEL.IR.C2.Metadata` merely to reuse enums.
From the first commit the Ada test proves `'Enum_Rep = 'Pos` for every literal
of both types and that both are 32 bits, so no later representation correction
is needed.

Outcome model: native `AMS_MEL_OK` becomes `Success` plus `Command_Status`,
native `AMS_MEL_COMMAND_REJECTED` becomes `Rejected` plus the error code and the
complete diagnostic (recovered with exact storage from the cached terminal
request), native `AMS_MEL_TIMEOUT` raises `Timeout_Error`, and anything else
raises `Provider_Error`.

## Raw Rust and private Python

Raw Rust adds `AmsMelIrSystemTrackDataResponseV1`,
`AmsMelIrTrackSystemResponseResultV1`, the opaque
`AmsMelIrTrackSystemResponseRequest`, and the three functions, taking raw Rust
functions to exactly 88. Private Python adds the ctypes equivalents and the
three bindings, taking `BOUND_FUNCTION_NAMES` to exactly 88. Both layout and
signature probes compile against the authoritative C header.

No safe Rust Track API and no public Python Track API were added.

## Mock scope

Only `send(SystemTrackDataResponse)` became newly positive. The `IRSTTrackReport`
callback and `send(TrackDataUpdate)` stay positive. The `RequestSystemTrackData`,
`CandidateObjectMessage`, and `CandidateObjectPreProcMessage` callbacks stay
`Return::NotSupported` and remain instrumented, so the tests still prove none of
them were exercised. Because both published `TrackChannel` sends are now
positively implemented, the deferred-send helper was removed and only deferred
registrations remain counted.

The successful response `CommandStatus` is deliberately distinct from the Track
update one, so the two request families can never be confused in a test.

## Evidence boundary

- `SystemTrackDataResponse` positive behavior/payload: mock provider only.
- Pinned Squall: unsupported Track Open only.

The Track expected-negative integration probe is unchanged, and this task
introduces no new positive real-Squall Track claim. Pinned Squall cannot attach
a Track channel, so it provides NO positive `SystemTrackDataResponse` evidence
whatsoever.

## Still not implemented

The `RequestSystemTrackData`, `CandidateObjectMessage`, and
`CandidateObjectPreProcMessage` callbacks. No safe Rust Track API and no public
Python Track API were added. No Scheduling, StackedImage, or RF work was done.
The entire Track API is NOT marked complete.
