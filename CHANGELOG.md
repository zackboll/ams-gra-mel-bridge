# Changelog

## Unreleased

- Correct an Image `NavigationReport` completion-vs-`Close` teardown race
  introduced with Task 027B. The asynchronous Navigation completion worker
  could perform deferred physical teardown — reading and resetting the
  `std::shared_ptr` members `ImageStreamState::channel`/`image_channel` — while
  application code was concurrently inside `ams_mel_ir_stream_stop` or
  `ams_mel_ir_stream_close`, which inspected the same members outside the frame
  callback mutex. That was a C++ data race and undefined behavior. A bad
  interleaving could also make `Close` return the stale `AMS_MEL_OK` captured
  from its own logical Stop even though the deferred `detachChannel` had just
  failed and left the channel attached, potentially clearing the stream owner
  while provider ownership was still uncertain.

  `CallbackState::mutex` is now the single Image teardown lock: `requests`,
  `channel`, `image_channel`, `enable_attempted`, `public_owner_closed`, and
  the new `cleanup_in_progress`/`cleanup_complete`/`cleanup_ok`/`cleanup_failed`
  fields are observed and modified only under it. `image_stream_cleanup` claims
  cleanup ownership under the lock, moves the provider owners into locals, runs
  `disable()`/`detachChannel()`/channel destruction unlocked (a pinned provider
  may invoke the `NavigationReportResp` callback synchronously, and `disable()`
  is not a quiescence boundary), then publishes the outcome and signals a new
  `cleanup_done` condition variable. `Stop`/`Close` block on `cleanup_done`
  instead of inspecting `channel` concurrently or polling, and `Close` adopts
  the published cleanup outcome rather than a stale Stop status. `Close`
  commits the public-owner release and `public_owner_closed` in the same
  critical section as the deferred-cleanup decision.

  A failed deferred detach restores the graph under the lock and retains the
  public owner so a later `Close` retries the detach; a successful retry
  establishes ownership safety and clears the owner but still reports
  `AMS_MEL_PROVIDER_FAILED`, because the lifecycle is already poisoned. Disable
  failure with a successful detach remains distinct and still permits owner
  release. Permanent allocation-free retention when no owner is left to retry,
  provider callback quiescence at channel destruction, and the rule that
  provider code is never unloaded while detach ownership is uncertain are all
  preserved. Two deterministic barrier-driven regressions in
  `native/tests/test_ir_navigation.c` force both interleavings; the barriers
  compile only under `AMS_MEL_ENABLE_TEST_FAILPOINTS` and production behavior
  never depends on an environment variable or marker file. No public feature,
  no ABI export, and no ABI version change; the 88-export inventory and all
  vendored files are unchanged. See
  `docs/corrective-image-navigation-close-race.md`.

- Implement exactly the optional IR Track `RequestSystemTrackData` (`@Optional`)
  surface in native C, safe Ada, raw Rust ABI, and private Python ABI. The
  pinned upstream `TrackChannel` declares `RequestSystemTrackData` **only** as a
  `registerMetadataCallback` overload and declares no
  `send(RequestSystemTrackData)`; it is an inbound request the provider delivers
  to the application, so it is implemented on the existing bounded
  DROP-INCOMING Track metadata queue rather than as a request/wait handle. Both
  implemented Track metadata kinds share one queue, capacity, and counter set
  and preserve FIFO order across kinds. All four published fields
  (`systemTime`, `commandID`, `requestId`, `trackId`) are copied verbatim;
  `systemTime` is `std::chrono::nanoseconds`, whose representation is signed, so
  it is carried as `int64_t` with no unit conversion. Upstream declares no enum
  and no constrained field, so only a null payload is malformed. Because the
  callback is `@Optional`, a provider that answers `Return::NotSupported` to this
  registration does not fail the metadata open and does not disturb the
  `@RequiredIfTrack` `IRSTTrackReport` callback. `Return::NotSupported` and
  `Return::Fail` are **not** treated as equivalent: upstream defines `Fail` as
  "a callback is already registered for this datatype on this channel", which is
  a genuine conflict rather than an optional refusal, so it -- and any other
  unrecognized non-`Success` value -- fails the open closed with
  `AMS_MEL_PROVIDER_FAILED` and lets no public metadata owner escape. The export count
  is unchanged at exactly 88 because no new C function was required; the
  `ams_mel_ir_track_metadata_event_v1` layout gains a discriminated
  `request_system_track_data` member and a second kind constant. Native CTest
  grows from 14 to 15. The vendor delta is zero: `RequestSystemTrackData.h` was
  already present in the reviewed Task 029A closure and no vendored file or
  `docs/upstream-files.sha256.md` entry changed. `CandidateObjectMessage` and
  `CandidateObjectPreProcMessage` remain unimplemented, no safe Rust or public
  Python Track API was added, and the entire Track API is NOT marked complete.
- Correct asynchronous/threading documentation so it distinguishes
  application-visible blocking waits, zero-timeout nonblocking polls, provider
  callback queueing, and the current native one-completion-thread-per-request
  implementation. Ada does not busy-poll: a positive timeout is a
  condition-variable-backed blocking wait and only a zero timeout is a
  nonblocking poll. Bounded-queue DROP-INCOMING overflow policy is documented as
  independent of any application polling strategy.
- Add an explicitly unnumbered future roadmap item for asynchronous
  completion-thread scalability, so 029F/029G sequencing is not disturbed. The
  current design is documented as correct but potentially inefficient at high
  concurrency, and the roadmap requires measuring the existing implementation
  before optimizing. No async worker-pool or thread-performance redesign is
  implemented.

- Implement exactly the optional IR Track
  `TrackChannel::send(SystemTrackDataResponse)` (`@Optional`) over the existing
  Track channel/report/update foundation, in native C, safe Ada, raw Rust ABI,
  and private Python ABI. This is `@Optional`, a distinct upstream condition
  from both `@RequiredIfTrack` and `@RequiredIfTrackUpdate`: the
  `@RequiredIfTrack` core remains complete, `@RequiredIfTrackUpdate`
  `TrackDataUpdate` remains complete, and the optional `SystemTrackDataResponse`
  send is now complete. The `RequestSystemTrackData`, `CandidateObjectMessage`,
  and `CandidateObjectPreProcMessage` callbacks remain unimplemented, no safe
  Rust or public Python Track API was added, and the entire Track API is NOT
  marked complete. ABI 0.1 grows from 85 to exactly 88 exports
  (`ams_mel_ir_track_submit_system_track_data_response`,
  `ams_mel_ir_track_system_response_request_wait`,
  `ams_mel_ir_track_system_response_request_close`) and native CTest grows from
  13 to 14. The vendor delta is zero: `SystemTrackDataResponse` and `RangeAzEl`
  were already present in the reviewed Task 029A closure (IR MEL
  `8d9224519f12b44e0b28815755c56a32a28d24a0`, AMS Math
  `00be45190f0e47d268cece8b8c2f8fb58b5418d2`).
- The complete response carries systemTime as signed nanoseconds, commandID,
  requestId, trackId, range/rangeRate/rangeError/rangeRateError verbatim in
  upstream meters and meters per second, AzElValid, inertialAzEl, AzElError, and
  rangeValid. Both angle pairs reuse the one canonical `ams_mel_ir_az_el_v1`
  (radians); no second azimuth/elevation representation was created. Nothing is
  clamped or normalized, because the upstream setters perform no such
  validation, and every published setter is called exactly once without assuming
  any C++ object layout. `az_el_valid` and `range_valid` use the established
  `uint8_t` bool representation and accept only 0 or 1; each is rejected
  independently with `AMS_MEL_INVALID_ARGUMENT`.
- `ams_mel_ir_track_system_response_request` and
  `ams_mel_ir_track_system_response_result_v1` are deliberately distinct public
  types rather than the `TrackDataUpdate` request/result reused under a
  misleading semantic name, while internally both families share exactly one
  async lifecycle model and one `TrackState::requests` accounting domain. A
  deterministic mixed-request regression proves that one pending
  `TrackDataUpdate` plus one pending `SystemTrackDataResponse` keep the provider
  graph alive until BOTH complete, so a second request counter would fail. Also
  covered: `Wait` timeout leaving the caller's record untouched and never
  cancelling, permanently cached terminal results, a successful `CommandStatus`
  whose state is Rejected remaining `AMS_MEL_OK` while only `ErrorOr` failure
  becomes `AMS_MEL_COMMAND_REJECTED`, request close not being cancellation, a
  synchronous `IRSTTrackReport` callback from inside the provider send,
  deferred-detach failure retention, and a System-response-specific post-send
  failpoint that leaves the existing `TrackDataUpdate` post-send tests
  unweakened.
- Safe Ada adds `AMS.MEL.IR.Track.System_Data` with `Azimuth_Elevation`,
  `System_Track_Data_Response`, an Ada-owned `Command_Status`, and a controlled
  `Response_Request`/`Response_Result`. Its package-local `Command_State` and
  `Cannot_Comply` carry explicit representation clauses with `Size => 32`, and
  the Ada test proves `'Enum_Rep = 'Pos` for every literal of both types from
  the first commit. `RequestSystemTrackData` is deliberately not implemented in
  this entry; Task 029E later established that it is an inbound metadata
  callback whose implemented Ada home is `AMS.MEL.IR.Track.Metadata`, not this
  package. Positive `SystemTrackDataResponse` behavior and payload fidelity are
  mock-validated only; pinned Squall still cannot attach a Track channel and
  therefore provides no positive `SystemTrackDataResponse` evidence.
- Implement exactly the conditionally required IR Track
  `TrackChannel::send(TrackDataUpdate)` (`@RequiredIfTrackUpdate`) over the
  existing Track channel/report foundation, in native C, safe Ada, raw Rust ABI,
  and private Python ABI. This is a distinct upstream condition from
  `@RequiredIfTrack` itself: the `@RequiredIfTrack` core remains complete and
  `@RequiredIfTrackUpdate` `TrackDataUpdate` send is now complete.
  `SystemTrackDataResponse`, `CandidateObjectMessage`,
  `CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
  unimplemented, and no safe Rust or public Python Track API was added.
  ABI 0.1 grows from 82 to exactly 85 exports (`ams_mel_ir_track_submit_update`,
  `ams_mel_ir_track_update_request_wait`, `ams_mel_ir_track_update_request_close`)
  and native CTest grows from 12 to 13. The vendor delta is zero:
  `TrackDataUpdate` was already present in the reviewed Task 029A closure
  (IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`, common MEL
  `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`).
- Upstream `TrackStatus` is exactly Create=0, Update=1, Predict=2, Delete=3 with
  no MaxExclusive value, so any input above Delete is `AMS_MEL_INVALID_ARGUMENT`.
  The complete update carries platformId, capabilityUUID, activityUUID, trackId,
  entityUUID, trackStatus, both epoch-second times (deliberately not converted to
  nanoseconds), ECEF position/velocity through the one canonical
  `ams_mel_ir_directional_v1`, all 21 published covariance terms in a new
  `ams_mel_ir_track_covariance_v1`, maneuverProbability, and trackQuality. No
  value is clamped or normalized, because the upstream setters perform no such
  validation, and every borrowed UCI label is validated as UTF-8 without an
  embedded NUL and copied before Submit returns.
- The asynchronous outcome reuses the proven Instrumentation request pattern and
  the existing generic `ams_mel_ir_command_status_v1`. Submission requires the
  Track lifecycle to be Enabled; the TrackState mutex is released before the
  provider send so a provider may invoke the registered `IRSTTrackReport`
  callback synchronously from inside `send()` without deadlocking. Exactly one
  completion worker calls `future.get()`, a terminal result is cached
  permanently, and `reason_description` points into immutable request-owned
  storage rather than provider memory. A successful `CommandStatus` whose own
  state is Rejected is still `AMS_MEL_OK`; only an `ErrorOr` rejection is
  `AMS_MEL_COMMAND_REJECTED`. Timeout means only "not ready yet" and request
  close is neither cancellation nor consumption. Track Close with pending
  requests clears the public owner and defers physical provider teardown to
  final request completion; a deferred detach failure retains the complete graph
  through the existing allocation-free emergency root and fails the request
  closed with `deferred Track cleanup failed`. Synchronous detach-failure
  semantics from Tasks 029B1/B2 are unchanged.

- Validate the IR Track `@RequiredIfTrack` core against pinned real Squall
  `b1015728f904c799fa0c07489fce48e78f67845f` as an expected unsupported
  provider. No Track functionality is implemented and no C ABI entry point is
  added: ABI 0.1 stays at 82 exports, native CTest stays at 12, and the vendor
  delta is zero. That provider's `SquallControl::attachChannel` supports only
  `IRSTImage`, `CommandAndControl`, and `HealthAndStatus`; `IRSTTrack` reaches
  the `default:` branch and returns `nullptr`, and its exported
  `createTrackChannel` is a hard `return nullptr;` that the bridge deliberately
  does not call — the bridge remains `Control::attachChannel` based. The C and
  Ada real-Squall integration clients now probe Track explicitly alongside the
  retained Instrumentation probe and require an exact clean negative:
  `AMS_MEL_FACTORY_FAILED` with a `NULL` owner in C and `Provider_Error` in
  Ada, both carrying exactly `attachChannel returned null`. Neither client
  reopens the Session; each continues on the same Session and still requires
  its established positive evidence (C Image/C2/BIT/frames/counters; Ada
  Image capability and BadPixelList, NavigationReport callback/future/cached
  wait, C2 and common-channel services, Health/Status, and all metadata
  counters). Because the channel cannot be attached, `Enable`,
  `Track.Metadata.Open`, and `IRSTTrackReport` reception are deliberately not
  attempted against real Squall. Rust and Python remain unchanged
  regression-only clients with no safe Track API. The evidence boundary is
  exact: the mock provider validates positive `@RequiredIfTrack` behavior and
  complete `IRSTTrackReport` payload fidelity, pinned Squall validates clean
  unsupported-provider behavior only, and pinned Squall does NOT provide
  positive Track execution or Track-report evidence. `TrackDataUpdate`,
  `SystemTrackDataResponse`, `CandidateObjectMessage`,
  `CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
  unimplemented.

- Add the conditionally required IR Track `IRSTTrackReport` metadata callback
  (`@RequiredIfTrack`) in native C, safe Ada, raw Rust, and private Python. The
  slice is exactly `TrackChannel::registerMetadataCallback(IRSTTrackReport)`
  over the merged Track channel foundation; Track ownership is not redesigned.
  `ams_mel_ir_track_report_v1` copies every upstream getter exactly once --
  `getSystemTime`, `getActivityId`, both `NorthEastDown` vectors through the one
  canonical `ams_mel_north_east_down_v1`, both intensity/SNR pairs, range, range
  error, spatial extent, track quality, clutter, age, state, and mode -- with no
  clamping, normalization, or narrowing. Upstream `IrstTrackState` and
  `IrstTrackMode` are exposed as exactly Idle/Detected/Coast/Dropped and
  Idle/Scan/Stare with no invented MaxExclusive value; anything above Dropped or
  Stare, and a null payload, are malformed, counted, and never queued. The
  callback state belongs to the Track channel rather than to the public metadata
  owner because upstream declares no unregister, so registration is one-shot and
  every later attempt returns `AMS_MEL_INVALID_ARGUMENT` even after a failed
  attempt. Registration publishes callback state and releases the Track
  lifecycle lock before calling the provider, so a provider that emits an
  `IRSTTrackReport` synchronously from inside `registerMetadataCallback` neither
  deadlocks nor loses that report; the mock does exactly that. The bounded queue
  is FIFO with DROP-INCOMING (six reports into a capacity-2 queue retain the
  first two and drop four) and counters saturate. Track cleanup marks metadata
  inactive before provider teardown, destroys the provider channel as the
  callback-quiescence boundary, waits for in-flight callbacks only afterwards,
  and then reports stopped; a failed detach instead marks the metadata failed
  and keeps the complete callback graph alive for a Close retry. ABI 0.1 grows
  from 76 to 82 exports; native CTest grows from 11 to 12. No safe Rust or
  public Python Track API is added, and no real Squall Track validation is added
  (deferred to task 029B3). `TrackDataUpdate`, `SystemTrackDataResponse`,
  `CandidateObjectMessage`, `CandidateObjectPreProcMessage`, and
  `RequestSystemTrackData` remain unimplemented; Track is not complete.

- Add the IR Track channel ownership/lifecycle foundation
  (`@RequiredIfTrack`) in native C, safe Ada, raw Rust, and private Python.
  The slice is exactly `Open`, `Enable`, `ChannelCapability`, and `Close` over
  a shared private `TrackState` that follows the Health/Instrumentation channel
  architecture without metadata state or request accounting. Open constructs
  the pinned upstream `irmel::Config` with `ChannelType::IRSTTrack` and requires
  all three of a non-null `attachChannel`, a successful concrete
  `TrackChannel` cast, and a reported capability containing `IRSTTrack`; each
  failure rolls back through detach, and a graph whose detach ownership cannot
  be proven is retained permanently by an allocation-free emergency root rather
  than destroyed. Close disables only when Enable was attempted, always attempts
  detach, and a failed detach leaves the caller's owner non-null for a later
  retry. The mock provider derives from the abstract upstream `TrackChannel`,
  implements every pure virtual Track operation as unsupported/not-supported,
  and records any invocation so the tests prove the deferred surface was never
  exercised; it uses only the vendored pinned Boost include root and the
  `check_track_header_boost_closure` target remains green with a vendor delta of
  zero. ABI 0.1 grows from 72 to 76 exports; native CTest grows from 10 to 11.
  No safe Rust or public Python Track API is added, and no real Squall Track
  validation is added. `IRSTTrackReport`, `TrackDataUpdate`,
  `SystemTrackDataResponse`, `CandidateObjectMessage`,
  `CandidateObjectPreProcMessage`, and `RequestSystemTrackData` are not
  implemented; Track is not complete.

- Add the conditionally required IR Instrumentation channel
  (`@RequiredIfInstrumentation`) in native C, safe Ada, raw Rust, and private
  Python. The slice is exactly the Instrumentation-specific conditional surface
  -- `send(InstrumentationLevelCmd)` and the `InstrumentationReport` metadata
  callback -- plus `Enable` and `ChannelCapability`. One canonical
  `ams_mel_ir_instrumentation_report_v1` carries both future completions and
  metadata events, preserving full `uint32` command ID/size and signed
  nanosecond timestamps; upstream `Priority` is exposed as exactly Normal/Debug
  with no invented MaxExclusive value. The mock provider validates positive
  behavior, full payload fidelity, and synchronous callbacks emitted from both
  `registerMetadataCallback` and `send`. Pinned Squall does not support this
  channel and is validated only for clean unsupported-provider behavior. ABI 0.1
  grows from 59 to 72 exports; native CTest grows from 9 to 10. No safe Rust or
  public Python Instrumentation API is added, and Instrumentation-specific
  copies of the inherited generic Channel services are deliberately not cloned.

- Add the `ImageChannel::send(NavigationReport)` request/future vertical slice
  in native C, safe Ada, raw Rust, and private Python: the complete published
  `NavigationReport` (all 18 covariance terms), asynchronous submit/wait/close,
  deferred provider teardown while a request is outstanding, allocation-free
  emergency retention for post-send facade failures, and a dedicated native
  contract test. ABI 0.1 grows from 56 to 59 exports.
- Refactor private Image stream ownership into shared `ImageStreamState`. The
  public stream remains an opaque thin owner; ABI 0.1, its 56 exports, and all
  application-visible behavior are unchanged. NavigationReport remains unimplemented.
- Add owned Image `NavigationReportResp` metadata (kind 4) in C and safe Ada.
  ABI 0.1 remains exactly 56 exports; NavigationReport send is not implemented.
- Extend the existing Image metadata FIFO with complete owned LineOfSightReport and
  LineOfSightEuler values in C and Ada. Raw Rust and private Python ABI declarations
  track the added records and constants; ABI 0.1 remains exactly 56 exports.
- Add ImageChannel capability access and the first Image metadata vertical slice:
  bounded DROP-INCOMING BadPixelList callbacks with complete owned C and Ada values,
  synchronous-registration safety, malformed/allocation recovery, and teardown-safe
  event lifetime. The raw Rust/private Python ABI now tracks all 56 exports; no safe
  Rust or public Python metadata API is added.
- Add an additive owned full `FrameHeader` snapshot (`receive_snapshot`/view/close)
  while preserving legacy `ams_mel_ir_frame_v1` and `ams_mel_ir_stream_receive`.
  One bounded FIFO serves both receives.  Native C and safe Ada preserve complete
  contributing sensor, ordered duplicate flags, inertial/nav states, independent
  Euler/quaternion variants, and owned pixels; raw Rust/private Python track 49 exports.

- Add application-facing inherited C2 Channel services in native C and safe Ada:
  pre-enable KeepAlive, asynchronous ChannelCommsTest request/reply, its callback
  through the existing metadata queue, and complete owned ChannelCapability
  snapshots. Synchronize the 36-function raw Rust/Python ABI only; explicit
  generic buffer management, Scheduling, and safe Rust/Python APIs remain out of scope.
- Add all three required C2-specific metadata callbacks through a bounded native
  FIFO queue with DROP-INCOMING overflow, complete deep-copied CommandStatus,
  BIT_Configuration, BIT_Status, and nested Fault values, immutable event owners,
  and safe Ada-owned events. Expand only the raw Rust sys/private Python ABI to
  the complete 28-function façade; their safe metadata APIs remain unchanged.
- Complete all three required IR C2 command sends in the native façade and Ada:
  general ModeCmd with complete ScanParam, full raw BIT payload transport with a
  safe one-choice Ada API, and opaque ConfigSet. Synchronize only the raw Rust
  sys and private Python ctypes layers, expanding the façade to 22 functions.
- Expose the existing IR BIT empty/no-op profile through dependency-free Python
  with typed `CommandReturn`, structured completion/rejection values, reusable
  `ReturnRequest`, complete cached diagnostics, and parent-independent ownership.
- Expose the existing IR BIT empty/no-op profile through safe Rust with typed
  `CommandReturn`, structured `ReturnResult`, and reusable asynchronous
  `ReturnRequest` ownership; `Return::Fail` remains a normal completion.
- Add the required IR BIT command in the pinned-provider-compatible empty/no-op
  profile for C and Ada, with reusable asynchronous `RequestFor<Return>` ownership.
- Synchronize the complete 19-function raw Rust sys ABI while leaving safe Rust
  and Python BIT APIs for later tasks.
- Add the safe Rust Session foundation over the existing C ABI.
- Add safe Rust IR host-memory Mono8 and C2 Operate/TaskSched bindings.
- Validate the current safe Rust IR slice against the pinned real Squall stack.
- Add the dependency-free Python Session foundation and IR host-memory Mono8
  binding with owned-copy frames and mock-provider tests.
- Add the dependency-free Python IR C2 Operate/TaskSched binding with explicit
  enable, asynchronous requests, complete structured rejection results, cached
  waits, retryable close, and independent/coexisting native lifetimes.
- Validate the current safe Python IR Mono8 plus C2 slice against the pinned real
  Squall stack and extend the opt-in harness to run C, Ada, Rust, and Python.
- Add an opt-in real Squall IR MEL integration harness with strict pinned-source
  validation, hardware-free simulated optical/Couloir orchestration, and public
  C11/Ada C2-plus-Mono8 smoke clients; ordinary builds and CI remain provider-free.
- Add IR CommandAndControl attach/enable and asynchronous
  `ModeCmd(Operate, TaskSched)` requests for C and `AMS.MEL.IR.C2`.
- Distinguish timeout, normal MEL `ErrorOr` rejection, provider exception,
  provider failure, and null successful mode results.
- Retain C2/provider/library ownership through pending requests, including after
  public request, C2, or Session close; add C2/image coexistence coverage.
- Expand the immutable upstream declaration closure from 71 to 85 headers.
- Add a host-memory, single-band Mono8 `IRSTImage` receive profile for C and Ada.
- Add bounded DROP-INCOMING frame queues, exact-size caller copies, saturating
  counters, and distinct timeout/stopped results.
- Extend the separately loaded C++ mock with asynchronous image production,
  malformed/failure scenarios, and lifecycle instrumentation.
- Expand the immutable upstream declaration closure from 44 to 71 headers.
- Harden IR callback teardown around channel destruction and explicit in-flight
  tracking; propagate release/provider cleanup failures through C and Ada.
- Validate Mono8 channel capabilities and configuration integer ranges, and
  document the per-stream single-consumer receive contract.
- Add the task 001 IR MEL provider load/init/version/close slice in C and Ada.
- Vendor the exact reviewed Common MEL, IR MEL, and AMS Math declaration closure.
- Add separately loaded mock/failure providers and native/Ada lifecycle tests.

## Unreleased — 0.1.0-dev

- Create separate native and Ada development crates.
- Implement one C-callable ABI version query with C and C++ tests.
- Add an idiomatic Ada version query and a separate smoke-test crate.
- Add native installation support, local gates, and Linux CI definitions.
- Record review provenance, scope, ABI policy, and the next implementation task.

At that bootstrap snapshot, no provider, IR, RF, OMS client, or Rust binding was
implemented yet.
