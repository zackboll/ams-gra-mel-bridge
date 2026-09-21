# Changelog

## Unreleased

- Add the first high-rate zero-copy data-plane slice: `AMS.MEL.IR.Image` gains
  a limited `Frame_Lease` with `Acquire_Frame`, `Is_Open`, `Close`,
  `Pixel_Count`, `With_Pixels`, `Copy_Pixels`, and the full set of lease
  metadata accessors. The lease is an `Ada.Finalization.Limited_Controlled`
  owner of exactly one `ams_mel_ir_frame_snapshot`, and `With_Pixels` binds a
  constrained `AMS.MEL.IR.Pixel_Array` directly to the snapshot's own
  `pixels.data` with `Import` plus an address clause. This removes the
  native-snapshot-to-Ada pixel copy: no payload-sized Ada allocation, no
  `memcpy` from the snapshot, no per-pixel FFI call, no per-pixel `Append`
  loop, and no `Ada.Containers` pixel Vector on the lease path.
  `Acquire_Frame` and `With_Pixels` setup are both O(1) with respect to pixel
  count.

  This is deliberately **not** end-to-end zero-copy. The native frame callback
  still copies the MEL provider buffer once into snapshot-owned storage before
  `irmel::Buffer::release`; retaining the upstream `irmel::Buffer` until the Ada
  lease is released is the separate Task 030B investigation. Payload copies on
  the high-rate path are therefore `provider -> native storage: 1` and
  `native storage -> Ada: 0`.

  The naming makes ownership explicit: `Receive`/`Pixels` are owned and may
  copy, `Acquire_Frame`/`With_Pixels` borrow without a bulk copy, and `Copy_*`
  is an explicit owned copy. No function named simply `Pixels` was added to the
  lease. `AMS.MEL.IR.Receive`, the owned `Frame` record,
  `AMS.MEL.IR.Image.Receive`, `Full_Frame`, and `Pixels (Full_Frame)` are all
  unchanged compatibility/copying APIs; `Receive` was refactored to share one
  metadata conversion with the lease path and now performs its payload copy
  explicitly, with identical observable behavior. Existing applications acquire
  no new native lifetime dependency.

  Lease semantics: one live lease owns one snapshot, the owner cannot be
  copied, finalization closes exactly once, `Close` is idempotent, finalization
  after an explicit close is harmless, failed acquisition and metadata
  conversion failures leak nothing, later frames do not invalidate an earlier
  lease, multiple outstanding leases each own distinct storage, and the lease
  stays valid through queue advance, stream Stop, stream Close, Session close,
  and provider teardown. The existing one-task-per-stream receive restriction is
  preserved, and no lock is held across the borrow callback. Null-pointer-with-
  nonzero-size and out-of-Ada-index-range spans fail closed with
  `Provider_Error`; zero-length payloads are valid.

  Zero-copy is proven structurally, not by timing. A test-only facade hook under
  `AMS_MEL_ENABLE_TEST_FAILPOINTS` logs each snapshot's pixel-data address, and
  the Ada regression requires the borrowed view's first element address to equal
  it exactly; a new C test additionally proves the snapshot's payload address is
  stable, distinct per snapshot, and survives stream/Session/provider teardown.

  **No C ABI change.** No export was added or removed, no public type changed,
  `exports.map` is untouched, and the ABI version stays `0.1`; the existing
  snapshot receive/view/close trio already provided the required
  opaque-handle-plus-borrowed-span shape. `docs/architecture.md` gains a
  high-rate data ownership section describing the reusable
  native-owner/opaque-handle/limited-owner/borrowed-view chain, its intended
  reuse by RF Receive products, RF waveform streaming, and Stacked Image, and
  the explicit constraint that future bulk buffers -- heap, RDMA-registered,
  GPU, or FPGA/device memory -- must not be assumed to be ordinary CPU-copyable
  Ada memory. No RF MEL, RDMA, GPU, CUDA, FPGA, Stacked Image, or
  provider-buffer retention work is implemented here.

- Add the `@Optional` IR Track `CandidateObjectPreProcMessage` as inbound
  metadata in native C, safe Ada, raw Rust ABI, and private Python ABI,
  completing every published `TrackChannel`-specific surface. The pinned
  `TrackChannel` declares it **only** as a `registerMetadataCallback` overload
  and declares no `send(CandidateObjectPreProcMessage)` and no
  `RequestFor<CandidateObjectPreProcMessage>`, so it is implemented on the
  existing bounded DROP-INCOMING Track metadata queue as metadata kind `4`
  rather than as an asynchronous request. No request handle, completion, worker
  thread, or `CommandStatus` mapping was added, and `TrackState::requests` is
  untouched.

  Registration order becomes `IRSTTrackReport`, `CandidateObjectMessage` (only
  when advertised), `RequestSystemTrackData`, `CandidateObjectPreProcMessage`,
  which makes the four-kind FIFO deterministic. It runs with the Track
  lifecycle lock released, so a provider may deliver synchronously from inside
  `registerMetadataCallback`. Because the callback's own annotation is
  `@Optional`, `Return::NotSupported` is **non-fatal** and Metadata Open still
  succeeds, exactly as for `RequestSystemTrackData`; `Return::Fail` --
  documented upstream as a conflicting existing registration -- plus
  `BadPointer`, `NotImplemented`, and any future value fail closed with
  `AMS_MEL_PROVIDER_FAILED`, and a throwing registration maps to
  `AMS_MEL_PROVIDER_EXCEPTION`.

  All 17 published `CandidateObjectPreProc` getters and all 3 message getters
  are mapped exactly once. The message getters return by value, including the
  vector, so each is called once and its result retained. The upstream
  `std::array<std::array<std::int16_t, 3>, 3>` background patch becomes one
  explicit fixed `int16_t samples[9]` record with published side/sample
  constants and an exact row-major mapping; the `std::array` object is never
  `memcpy`'d and all nine values are copied individually. Both the
  message-level and each entry's own `SensorInertialState` are copied, `edge`
  is normalized to exactly 0 or 1, and nothing is clamped, renormalized, or
  decoded -- `candidateObjectQuality` is documented "0 to 1" upstream but its
  setter enforces nothing, so it is deliberately NOT clamped.

  Unlike `CandidateObjectMessage`, whose fixed 900-entry array needs
  `numberOfCOs` to select a prefix, the PreProc container is a `std::vector`
  with its own size, and the pinned headers publish no invariant requiring
  `numberOfCOs == candidateObjectPreProcs.size()`. The adapter therefore copies
  `numberOfCOs` verbatim, copies the COMPLETE vector at its actual size, and
  neither truncates nor rejects a mismatch; a mock fixture with
  `numberOfCOs = 3` and a 2-element vector asserts both values survive.

  `ams_mel_ir_track_metadata_event_v1` and `ams_mel_ir_track_metadata_event_v2`
  are both frozen and their view operations are unchanged. The payload lives in
  the new `ams_mel_ir_track_metadata_event_v3`, whose first member is the
  complete v2 record, reachable only through the one new export
  `ams_mel_ir_track_metadata_event_view_v3`. Export count 89 -> 90; ABI version
  stays `0.1`. The v2 freeze was mutation-checked: appending the PreProc
  payload to v2 was observed to fail the native contract and both the Rust and
  Python authoritative ABI probes, and was then reverted.

  The mock provider's deferred-Track-surface instrumentation
  (`track_deferred_calls`, `deferred_registration`) is removed; after this
  change there is no deferred `TrackChannel` surface. Positive Track behavior
  remains mock-only because pinned Squall cannot attach a Track channel through
  `Control::attachChannel`, and the safe Rust and public Python Track APIs
  remain intentionally absent.

- Add the `@RequiredIfDetectCandidateObjects` IR Track `CandidateObjectMessage`
  as inbound metadata in native C, safe Ada, raw Rust ABI, and private Python
  ABI. The pinned `TrackChannel` declares `CandidateObjectMessage` **only** as a
  `registerMetadataCallback` overload and declares no
  `send(CandidateObjectMessage)` and no `RequestFor<CandidateObjectMessage>`, so
  it is implemented on the existing bounded DROP-INCOMING Track metadata queue
  rather than as an asynchronous request. No request handle, completion, worker
  thread, or `CommandStatus` mapping was added, and `TrackState::requests` is
  untouched.

  Three distinct upstream annotations are preserved rather than collapsed: the
  `CandidateObjectMessage` class is `@RequiredIfBuiltInTracker`, the callback is
  `@RequiredIfDetectCandidateObjects`, and the `CandidateObject` class is
  `@RequiredIfTrack`. The callback annotation governs registration.

  Registration is conditional on the channel advertising
  `ChannelMetadataCapabilityType::CandidateObjectMessage`, recorded during Track
  `Open` from the one capability query that already ran. Unadvertised channels
  skip it entirely and keep their exact previous behavior. When it **is**
  advertised, any non-`Success` return -- including `Return::NotSupported` --
  fails Metadata Open closed with `AMS_MEL_PROVIDER_FAILED`, because refusing a
  callback the channel itself advertised would promise an event path the adapter
  cannot receive; an exception maps to `AMS_MEL_PROVIDER_EXCEPTION`. This
  deliberately differs from the `@Optional` `RequestSystemTrackData`, whose
  `NotSupported` stays non-fatal. Registration order is `IRSTTrackReport`,
  `CandidateObjectMessage`, `RequestSystemTrackData`, with `TrackState::mutex`
  released across every registration call.

  The complete payload is deep-copied: the header (including binary32 `CFAR`,
  which is deliberately not widened to `double`, and the verbatim undecoded
  validity bitfield), the complete `HotRegion` vector in published order, the
  canonical `SensorInertialState`, and exactly `numberOfCOs` candidate objects.
  `numberOfCOs` is the meaningful prefix length of the upstream fixed 900-entry
  array, so trailing storage slots are neither exposed nor read; a `numberOfCOs`
  above `MAX_CANDIDATE_OBJECTS` (900), a null payload, and a `HotRegion` enum
  outside the upstream `0..3` range are each malformed and enqueue nothing.

  `EventData` now owns two `std::vector` members so the native event owns every
  byte its spans reference; the spans remain valid until `event_close`, proven
  by re-verifying a complete message field by field after provider channel
  destruction and provider library unload.

  New ABI value types `ams_mel_ir_row_col_v1`, `ams_mel_ir_hot_region_v1`,
  `ams_mel_ir_hot_region_span_v1`, `ams_mel_ir_candidate_object_header_v1`,
  `ams_mel_ir_candidate_object_v1`, `ams_mel_ir_candidate_object_span_v1`, and
  `ams_mel_ir_candidate_object_message_v1`, plus
  `AMS_MEL_IR_MAX_CANDIDATE_OBJECTS` and the four `AMS_MEL_IR_HOT_REGION_*`
  constants and Track metadata kind 3. The canonical `SensorInertialState`,
  quaternion, directional, and uncertainty declarations were relocated earlier
  in `abi.h` so they could be reused; their layouts are unchanged and the ABI
  probes verify that. Native CTest stays at 15 and the vendor delta is zero.

  **Track metadata event ABI versioning.** `docs/c-abi-policy.md` prohibits
  appending fields to an existing fixed-layout record without a compatible
  size/version scheme or a new type and operation. Task 029E had historically
  appended `request_system_track_data` to
  `ams_mel_ir_track_metadata_event_v1`, which was itself inconsistent with that
  rule. Rather than break the record a second time, the layout currently on
  `main` is **grandfathered and permanently frozen** at exactly `kind`,
  `track_report`, and `request_system_track_data`, and `CandidateObjectMessage`
  is **not** appended to it.

  A new versioned record is added instead:

  ```c
  typedef struct ams_mel_ir_track_metadata_event_v2 {
      ams_mel_ir_track_metadata_event_v1 base;
      ams_mel_ir_candidate_object_message_v1 candidate_object_message;
  } ams_mel_ir_track_metadata_event_v2;
  ```

  The complete frozen v1 is the first member, so `offsetof(v2, base) == 0`, no
  report or `RequestSystemTrackData` layout is duplicated, `base.kind` remains
  the one discriminator, and candidate storage remains event-owned.

  `ams_mel_ir_track_metadata_event_view` is unchanged in signature and
  semantics and still returns `const ams_mel_ir_track_metadata_event_v1 *`, so
  existing consumers need no recompilation merely because
  `CandidateObjectMessage` was added; it does not gain a larger output
  contract. A Candidate event seen through it reports `kind == 3` with no
  candidate payload present. One new export,
  `ams_mel_ir_track_metadata_event_view_v2`, returns the v2 record and is the
  only way to reach the candidate payload. Exports therefore go **88 -> 89**
  across `exports.map`, the production and test dynamic symbol tables, the raw
  Rust declarations, and the private Python `BOUND_FUNCTION_NAMES`. The facade
  ABI version remains **0.1**.

  Future Track metadata additions must not append fields to v1 or v2; they must
  introduce a further version record with the earlier version as its first
  member plus a matching view operation. The C, Rust, and Python ABI probes
  assert the exact v1 member set so an accidental v1 append fails those
  compatibility tests.

  `CandidateObjectPreProcMessage` remains the only unimplemented Track metadata
  callback and the only remaining deferred mock surface, so the Track API as a
  whole is still not complete. Positive evidence is mock-provider only: pinned
  Squall still cannot attach Track through `Control::attachChannel`. There is no
  safe Rust and no public Python Track API.

- Isolate the native production and contract-test CMake build trees. Every
  native script previously shared `native/build`, and
  `native/scripts/configure-build.sh` silently reconfigured that one tree
  between `AMS_MEL_BUILD_TESTS=ON` and `OFF`. Those modes are not
  interchangeable: the test configuration compiles
  `AMS_MEL_ENABLE_TEST_FAILPOINTS` into `ams_mel_c` and adds the mock providers
  and CTest targets. Running `native/scripts/build.sh` during or after test work
  therefore rebuilt the facade underneath CTest and stress runs and removed the
  deterministic test barriers, which is how an infrastructure change was
  mistaken for a product failure during the Image navigation teardown
  corrective task. The inverse was also possible: a production build could
  silently become a failpoint-enabled facade.

  The production facade now builds in `native/build` and the contract-test
  facade, providers, and tests build in `native/build-tests`.
  `configure-build.sh` takes `production` or `tests` instead of `ON`/`OFF`,
  derives the directory from the mode through the new shared
  `native/scripts/build-tree.sh` helper, and resets only the selected tree, so
  neither workflow can delete or reconfigure the other's tree. The Task 023
  `LD_RUN_PATH`, compiler-change, and pre-hardening-cache hardening is unchanged
  and now applies independently to both trees.

  The Ada binding keeps linking the production facade, and an ordinary Ada build
  both links and runs it. The Ada contract suite, however, exercises the
  test-only failpoints, so it links the production facade but runs against the
  contract-test facade: `ada/tests/ams_mel_tests.gpr` now links with
  `-Wl,--enable-new-dtags` so the smoke executable carries `DT_RUNPATH`, which
  `LD_LIBRARY_PATH` overrides, and `scripts/test_ada.sh` and
  `ada/tests/alire.toml` point it at `native/build-tests/lib`. Mock providers
  resolve from `AMS_MEL_TEST_PROVIDER_DIR` with a repository-relative fallback
  to `native/build-tests/test-providers`. `make test-rust`, `make test-python`,
  and the matching CI jobs point explicitly at the test tree, while the Rust
  build-script native-library default stays on the production facade so
  downstream builds never implicitly depend on a test-enabled library.
  Real-Squall integration deliberately remains a production-tree build.

  `native/scripts/test-build-tree-isolation.sh` (`make test-build-isolation`,
  also run by `make check` and a dedicated CI job) is a new regression guard
  proving the test tree survives `build.sh` byte-identically and still passes
  15/15 without rebuilding, the production tree survives `test.sh`, the
  failpoint definition is present only in the test tree, and both trees coexist
  in opposite modes. No C ABI, export set, ABI version, provider behavior, or
  vendored file changed.

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

  That synchronization also covers the window between the final request-count
  decrement and the cleanup ownership claim. `release_navigation_submission`
  unlocks before `image_stream_cleanup` re-locks and claims ownership, so a
  racing Close could observe `requests == 0`, `cleanup_in_progress == false`,
  and a still-attached `channel` while the completion thread was already
  committed to cleaning up. Deciding from that transient state could return the
  stale `AMS_MEL_OK` with the public owner still non-null. Close now treats it
  as cleanup owed: it runs or joins the cleanup outside the lock and re-decides
  from the published result. Cleanup ownership is single-claim, so teardown
  still happens exactly once and the losing thread adopts the published
  outcome. No Close return can satisfy `AMS_MEL_OK` with a retained owner.

  A failed deferred detach restores the graph under the lock and retains the
  public owner so a later `Close` retries the detach; a successful retry
  establishes ownership safety and clears the owner but still reports
  `AMS_MEL_PROVIDER_FAILED`, because the lifecycle is already poisoned. Disable
  failure with a successful detach remains distinct and still permits owner
  release. Permanent allocation-free retention when no owner is left to retry,
  provider callback quiescence at channel destruction, and the rule that
  provider code is never unloaded while detach ownership is uncertain are all
  preserved. Three deterministic barrier-driven regressions in
  `native/tests/test_ir_navigation.c` force the raced detach failure, the
  in-progress successful cleanup, and the decrement-to-claim window; the
  success race proves contention through a nonblocking observation handshake
  rather than a timing sleep, and each regression is mutation-checked. The
  barriers
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
