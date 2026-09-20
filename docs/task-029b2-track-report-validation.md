# Task 029B2 Track IRSTTrackReport validation

Task 029B2 extends the merged Track channel foundation with exactly the
`@RequiredIfTrack` `TrackChannel::registerMetadataCallback(IRSTTrackReport)`
surface. Track ownership is not redesigned.

## Immutable upstream

- IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`
- Common MEL `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`

Authoritative declarations: `irmel/library/track/TrackChannel.h`,
`irmel/library/track/IRSTTrackReport.h`, and
`irmel/library/irmel-types/CommonIR_MEL.h`. The vendor delta is zero: no file
under `native/vendor/` and no entry in `docs/upstream-files.sha256.md` changed,
and the `check_track_header_boost_closure` target remains green over the same
481-header pinned Boost 1.83.0 closure.

## Enumeration mappings

Upstream `IrstTrackState` and `IrstTrackMode` declare no MaxExclusive value, so
the facade rejects anything above the last declared literal rather than
inventing a sentinel.

| Upstream | C constant | Value | Ada literal |
|---|---|---|---|
| `IrstTrackState::Idle` | `AMS_MEL_IR_TRACK_STATE_IDLE` | 0 | `State_Idle` |
| `IrstTrackState::Detected` | `AMS_MEL_IR_TRACK_STATE_DETECTED` | 1 | `Detected` |
| `IrstTrackState::Coast` | `AMS_MEL_IR_TRACK_STATE_COAST` | 2 | `Coast` |
| `IrstTrackState::Dropped` | `AMS_MEL_IR_TRACK_STATE_DROPPED` | 3 | `Dropped` |
| `IrstTrackMode::Idle` | `AMS_MEL_IR_TRACK_MODE_IDLE` | 0 | `Mode_Idle` |
| `IrstTrackMode::Scan` | `AMS_MEL_IR_TRACK_MODE_SCAN` | 1 | `Scan` |
| `IrstTrackMode::Stare` | `AMS_MEL_IR_TRACK_MODE_STARE` | 2 | `Stare` |

The Ada `State_`/`Mode_` prefixes exist only because both upstream enumerations
declare an `Idle`; the representation clauses pin the upstream values exactly.

## Complete report shape

`ams_mel_ir_track_report_v1` and the Ada `IRST_Track_Report` represent every
upstream getter exactly once:

| Upstream getter | C field | Ada field |
|---|---|---|
| `getSystemTime()` | `system_time_ns` | `System_Time_NS` |
| `getActivityId()` | `activity_id` | `Activity_ID` |
| `getMeasuredNed()` | `measured_ned` | `Measured_NED` |
| `getMeasuredIntensity()` | `measured_intensity` | `Measured_Intensity` |
| `getMeasuredSnr()` | `measured_snr` | `Measured_SNR` |
| `getFilteredNed()` | `filtered_ned` | `Filtered_NED` |
| `getFilteredIntensity()` | `filtered_intensity` | `Filtered_Intensity` |
| `getFilteredSnr()` | `filtered_snr` | `Filtered_SNR` |
| `getRange()` | `range_m` | `Range_M` |
| `getRangeError()` | `range_error_m` | `Range_Error_M` |
| `getSpatialExtent()` | `spatial_extent_rad` | `Spatial_Extent_Rad` |
| `getTrackQuality()` | `track_quality` | `Track_Quality` |
| `getClutter()` | `clutter` | `Clutter` |
| `getAge()` | `age_ns` | `Age_NS` |
| `getState()` | `state` | `State` |
| `getMode()` | `mode` | `Mode` |

Both NED vectors reuse the one canonical `ams_mel_north_east_down_v1`; no second
C NED representation was added. `getNorth`/`getEast`/`getDown` are copied
directly for each vector. `system_time_ns` and `age_ns` preserve signed
`std::chrono::nanoseconds` counts. No floating-point value is clamped,
normalized, or narrowed: the private Ada import keeps every C double as
`Interfaces.C.double` and converts only in the safe layer.

## Rich report fidelity

The mock emits one distinctive report whose every field is checked exactly in
both the native C test and the Ada test:

```
system_time_ns = -1_234_567_890_123    activity_id = 0xF1234567
measured_ned   = (-1.25, 2.5, -3.75)   measured_intensity = 4.125
measured_snr   = -5.25
filtered_ned   = (6.5, -7.75, 8.875)   filtered_intensity = -9.125
filtered_snr   = 10.25
range_m = 123456.75   range_error_m = 654.5   spatial_extent_rad = 0.0125
track_quality = 0.875 clutter = -0.5
age_ns = 9_876_543_210  state = Coast  mode = Stare
```

## Behavioral contract

- **Synchronous registration.** The mock invokes the `IRSTTrackReport` callback
  from inside `registerMetadataCallback`. `ams_mel_ir_track_metadata_open`
  therefore validates lifecycle, marks the attempt, publishes callback state and
  copies the shared `TrackChannel` under `TrackState::mutex`, releases that
  mutex, and only then registers. The lock is never held across provider
  registration, and the first report is neither lost nor deadlocked.
- **One-shot registration.** Upstream declares no unregister, so only one
  attempt per Track owner is permitted. Every later open returns
  `AMS_MEL_INVALID_ARGUMENT`, including after a `Return::Fail` registration,
  after a throwing registration, and after the public owner has been closed.
- **Registration failure.** Non-Success returns `AMS_MEL_PROVIDER_FAILED` and a
  throwing registration returns `AMS_MEL_PROVIDER_EXCEPTION`. No public owner
  escapes in either case, and the callback-accessible state remains retained by
  `TrackState`.
- **Malformed payloads.** A null `IRSTTrackReport` pointer, a state above
  `Dropped`, and a mode above `Stare` are each tested independently. Each
  increments `events_received` and `malformed_or_unsupported`, queues nothing,
  and leaves the subscription usable.
- **Allocation failure.** `AMS_MEL_TEST_TRACK_CALLBACK_FAILURE=allocation`
  proves no exception crosses the provider boundary, the lifecycle becomes
  Failed, receivers are woken, and Receive reports `AMS_MEL_PROVIDER_FAILED`.
- **Overflow.** Six reports into a capacity-2 queue give
  `events_received = 6`, `events_dropped_queue_full = 4`, and a queue holding
  the FIRST two in arrival order. The arrival index is carried in `activity_id`,
  so the test proves DROP-INCOMING rather than drop-oldest.
- **Receive.** A queued event is returned first whatever the lifecycle.
  Otherwise empty+Active is `AMS_MEL_TIMEOUT`, empty+Inactive/Stopped is
  `AMS_MEL_STREAM_STOPPED`, and empty+Failed is `AMS_MEL_PROVIDER_FAILED`. A
  zero timeout is a non-blocking poll.
- **Close.** `ams_mel_ir_track_metadata_close` is idempotent and nonblocking. It
  marks public consumption inactive, prevents future public enqueueing, wakes
  receivers, and deletes the wrapper. It does not unregister provider callbacks.

## Safe Ada ownership

`AMS.MEL.IR.Track.Metadata.Receive` obtains the native event owner, views it,
validates the event kind against `IRST_TRACK_REPORT` and both raw enumeration
values explicitly -- no unchecked enumeration conversion is used for provider
values -- copies every field into Ada-owned storage, closes the native owner,
and returns the Ada value. Any conversion exception still closes the native
owner before re-raising. No native pointer escapes the package.

## Lifetime and quiescence

Track cleanup captures the metadata state, marks it Inactive, and notifies
receivers before any provider teardown, then preserves the foundation ordering
of conditional disable followed by detach.

A failed detach leaves the public Track owner non-null, resets
`cleanup_started`, keeps the complete callback/provider graph alive, marks the
metadata Failed, and permits a Close retry. The native test proves an already
queued event still drains, that a subsequent Receive reports
`AMS_MEL_PROVIDER_FAILED`, and that a second Close succeeds.

After a successful detach the Track state clears `track` and `channel`, the
final local provider channel owner is destroyed, and only then does the adapter
wait for `callbacks_in_flight == 0` and move the metadata to Stopped unless it
was already Failed. Channel destruction is the quiescence boundary.

The callback-after-metadata-close case uses a deterministic ordered event log
rather than sleeps: the mock invokes the retained callback during `disable()`,
after the public metadata owner has been closed, and the log proves
`track_late_callback_entered` < `track_late_callback_returned` <
`track_channel_destroyed` < `control_destroyed` < `manager_destroyed` <
`library_unloaded`. The callback enters and returns safely and queues nothing.

## Mock provider scope

Only the `IRSTTrackReport` registration became a positive implemented callback.
`CandidateObjectMessage`, `RequestSystemTrackData`, and
`CandidateObjectPreProcMessage` registrations still return
`Return::NotSupported`, and both `TrackDataUpdate` and
`SystemTrackDataResponse` sends still return an `Unsupported` error. Each of
those remains instrumented, and legitimate `IRSTTrackReport` registration is no
longer counted as a deferred operation. Both the foundation and metadata native
tests assert that no deferred Track operation was invoked.

## Results

- Vendor delta: 0. `check_track_header_boost_closure`: green, 481 Boost headers.
- ABI version 0.1; `exports.map` 82; dynamic exports 82; Rust raw functions 82;
  Python bound functions 82.
- Native CTest: 12/12, including the new `ir_track_metadata_contract`.
- Stress: full suite `--repeat until-fail:50`, `ir_track_metadata_contract`
  `--repeat until-fail:100`, and `ir_track_foundation_contract`
  `--repeat until-fail:100` all green.
- Ada, Rust, and Python suites green; `make check-ada-format` clean.

## Explicitly not implemented

`TrackDataUpdate`, `SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
unimplemented. No safe Rust Track API and no public Python Track API exist. No
real Squall Track validation was added; `integration/squall/` is unchanged and
the pinned-Squall expected-negative Track probe is deferred to task 029B3.
