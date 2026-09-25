# Changelog

## Unreleased

- Characterize RequestFor completion scaling with test-only worker/get counters,
  deterministic held mock futures and a separate non-gating benchmark. C11
  coverage exercises 1/10/100 pending C2 Return operations and 100 mixed C2
  requests, including early request close and parent-first lifetime. No public
  ABI or production worker scheduling changes.
  Add seven-type N=100 controlled-wave success and stored-exception regressions:
  check consumed futures, weak result expiration before cleanup, empty Completion
  provider graphs, and final captured-owner reclamation. Preserve the original
  failed owner-before-unload assertion in the task history and distinguish
  provider safety from detached captured-storage lifetime.
  Separate seven-type retained-handle tests verify non-cancelling zero/positive
  timeouts, exact cached payloads, cached stored-exception statuses, and one get
  per future through request close. Separate non-gating five-run GCC Debug and
  three-run GCC Release C2-only and seven-type N=100 measurements characterize
  worker/thread growth, resident versus virtual memory, and distinct completion
  phases. Mixed measurements release the benchmark's own provider DSO handle
  after the last control call and observe actual physical teardown through an
  external test-only monotonic timeline, including library unload before exit.
  Bounded-resource designs remain Task 031B work.

- PR #43 follow-up: gate physical teardown through actual release-wrapper
  destruction; keep a strong graph during late callback execution and join
  callback-only work again at Close's final decision. Close-time queued-buffer
  release failure/exception now overrides Stop's earlier success; C and Ada
  exercise the public error without retrying uncertain hand-backs.

- Complete PR #43 release-obligation lifecycle integration. Release-executor
  ownership is explicit rather than inferred from a non-null active wrapper,
  survives failed/throwing operations and deferred draining, and covers unlocked
  final provider-wrapper destruction. Deferred promotion checks uncertain-slot
  availability and never indexes an empty free list or drops pending ownership.
  Stop, Close, initial cleanup, and post-drain cleanup consistently account for
  active and deferred obligations; public Close joins finite healthy
  callback-only work, returns `AMS_MEL_OK` only with a cleared handle, and then
  permits ordered channel, Control, manager, and library teardown. Deterministic
  C regressions cover failure, exception, actual waiter entry, A/B/C ownership,
  destructor reentry, callback-only Close, and separate executor/Close-gate
  mutations; safe Ada covers callback-only Close. ABI 0.1, all 90 exports,
  frozen layouts, zero-copy behavior, and vendor bytes remain unchanged.

- Correct the provider-buffer release/reuse handoff so a healthy provider that
  reuses a successfully returned physical buffer can no longer be misread as a
  non-conforming one.

  The bridge previously held the old callback's emergency-ownership retention
  slot for the whole duration of `Buffer::release()` and returned it only after
  the provider call completed. A conforming provider makes the physical buffer
  reusable as soon as the release succeeds -- pinned Squall republishes it
  **inside** `RequeueBuffer::release()`, with its pool mutex released, before
  the call returns. With every slot occupied, a new callback for that
  successfully returned buffer found an empty slot free list, and the bridge
  refused to release, counted a false malformed frame, published
  `uncertain_release`, poisoned the stream to `Failed`, and retained the
  provider graph permanently.

  This retracts the previous claim that the physical `buffer_count` alone
  bounds emergency ownership. A retention slot is keyed to a **per-callback
  `Buffer` wrapper generation**, not to a physical buffer.

  This further retracts the reviewed claim that two disjoint release sources
  bound unresolved wrapper generations by `2 * buffer_count`: repeated reuse
  lets either source create arbitrarily many generations while older provider
  calls remain unresolved. The bridge now enforces one active provider
  `release()` per stream. Ordinary explicit closes wait with their HOLD
  ownership intact, preserving synchronous result reporting. Callback-side
  reentry cannot wait for its enclosing release, so it transfers allocation-
  free into one of `buffer_count` preallocated deferred owners; the active
  executor drains those obligations iteratively. Before every provider call,
  the exact wrapper is also stored in one of `buffer_count` preallocated
  uncertain-owner slots. Success recycles that slot; failure consumes it
  permanently, is never retried, and does not block unrelated healthy releases.
  The legacy process-global 64-entry reserve was not increased and is not used
  for ordinary congestion.

  Uncertain-release safety is unweakened: emergency ownership is now allocated
  *before* the release rather than after one has failed, a failed or throwing
  release is still never retried, its exact wrapper is still never destroyed,
  teardown still requires
  `requests == 0 AND retained_frames == 0 AND release_obligations == 0 AND
  !uncertain_release`, and explicit
  close still reports `AMS_MEL_PROVIDER_FAILED` while Ada finalization stays
  non-raising.

  Counted deterministic regressions now drive eight repeated A generations,
  prove the one-call execution limit and recovery, and drive nine actual
  queue-full callbacks from release-side reentry while observing maximum
  provider release nesting of one. Distinct snapshot-owner closes prove
  intentional serialization, and actual Close-time queue discard is paused and
  accounted before stream-channel, Control, and library destruction. The Ada
  regression acquires A/B/C before closing A and requires frame 4 at A's exact
  address, with a log proving four callbacks were delivered.

  No C ABI change: ABI stays `0.1` with all 90 exports and `exports.map`
  unchanged, frozen record layouts untouched, zero bulk payload copies
  preserved, and no vendor delta. Mock-provider evidence only; the pinned
  Squall runtime was unavailable. See
  `docs/corrective-provider-buffer-release-handoff.md`.

- Complete the high-rate zero-copy data plane: the bridge now performs **zero**
  bulk payload copies from the MEL provider callback buffer into Ada. The
  native `frame.pixels.assign(...)` and its payload-sized
  `std::vector<std::uint8_t>` are removed. `QueuedFrame` instead owns the
  callback's `std::shared_ptr<irmel::Buffer>` plus the validated image address
  and byte count, and publishes `view.pixels` as a borrowed span directly over
  `irmel::Buffer::getImageAddress()`. The defining deterministic proof is the
  three-way pointer identity

      Buffer::getImageAddress() == snapshot pixels.data == Ada view address

  asserted natively and from Ada through a test-only address log, with no
  production ABI change and no reliance on elapsed-time benchmarks.

  This is a **bridge** claim. Pinned Squall still copies received UDP bytes
  into the registered MEL host buffer in `SquallImageChannel::udpCallback`, and
  nothing is claimed about NIC DMA, kernel socket buffers, or sensor transport.

  The public Ada API is unchanged from Task 030A: `Frame_Lease`,
  `Acquire_Frame`, `Is_Open`, `Close`, `Pixel_Count`, `With_Pixels`,
  `Copy_Pixels`, and the lease metadata accessors all keep their signatures.
  Only the meaning of the owner changed underneath, which is what Task 030A
  existed to make possible.

- Correct uncertain `Buffer::release()` ownership handling so it is complete
  and no longer depends on an arbitrary process-global limit.

  Two linked defects are fixed. First, `CallbackState::image()` releases frames
  the bridge **rejects before queue acceptance** -- malformed/unsupported,
  queue-full, and not-accepting. Those frames were never counted in
  `retained_frames` and the path never called `image_stream_retain_failed()`,
  so after an uncertain release a later Stop/Close could still observe
  `requests == 0 && retained_frames == 0` and physically tear the provider
  graph down, clear registered host storage, and unload the provider library.
  Second, the previous fail-safe could return `false` when a fixed 64-slot
  reserve was exhausted, and because callers passed the Buffer by `std::move`
  that return **destroyed the exact callback wrapper**, so the fixed reserve
  reduced risk without establishing the claimed invariant.

  Emergency ownership is now stream-owned and preallocated from the configured
  `buffer_count`: `CallbackState::retention_slots` is built in Start before
  `channel->enable()`, i.e. before any provider callback can run. Moving the
  exact callback `shared_ptr` into an existing empty slot allocates nothing and
  cannot throw. The enforced rule is that the bridge never calls `release()`
  unless it already owns a dedicated slot for that exact Buffer; if a
  non-conforming provider exhausts the pool the bridge refuses to release and
  keeps the buffer instead, which is safe because that branch runs *before* any
  release, so no wrapper can ever be dropped after a failed one. A
  `weak_ptr<ImageStreamState>` established at stream construction lets a
  callback-side uncertain release invoke the same graph-retention policy as
  snapshot/lease release.

  Retained-buffer accounting is documented exactly rather than fudged: an
  uncertain release of an *accepted* frame leaves the count unchanged (it is
  already counted and never decremented), while an uncertain release of a
  *rejected* frame adds one. Both reach the same post-condition -- exactly one
  never-removed unit -- so teardown is permanently blocked either way. A
  lock-free `uncertain_release` flag is an additional backstop, so physical
  teardown now requires
  `requests == 0 && retained_frames == 0 && !uncertain_release`.

  Deterministic regressions cover callback-side release failure for the
  malformed, queue-full, and not-accepting arms plus retention-slot exhaustion,
  each proving exactly one release attempt, the exact wrapper alive with
  destructor count 0, host bytes intact, provider channel/`Control` never
  destroyed, both public owners closable without freeing the graph, and no
  retry. A negative control deliberately drops the wrapper and asserts those
  invariants then fail, and a 96-buffer/200-cycle test proves capacity follows
  configured provider-buffer count rather than the old 64-slot constant. No ABI
  change: still ABI `0.1` with 90 exports and zero vendor delta.

- Close three remaining provider-Buffer ownership/lifetime defects found in the
  PR #42 third review.

  **Close teardown TOCTOU after an in-flight callback release failure.**
  `image_stream_cleanup()` checked `retained_frames`/`uncertain_release` once,
  before claiming physical cleanup, then destroyed and detached the provider
  channel, drained `callbacks_in_flight`, and cleared the registered ranges,
  registered Buffers, and backing host storage. A callback already in flight
  when that gate was passed could publish an uncertain `Buffer::release()`
  result afterwards, so cleanup could free registered host memory although
  `uncertain_release == true`, the failed wrapper was retained, and provider
  ownership hand-back was uncertain. Cleanup now re-evaluates that state after
  the drain reaches zero and before clearing anything. Visibility is
  established, not assumed: the callback publishes its uncertainty strictly
  before its `callbacks_in_flight` decrement (`acq_rel`), which the drain loop
  observes with acquire ordering. On late uncertainty cleanup keeps the host
  storage and retained Buffer ownership, retains the provider library and
  Session graph, reports failure truthfully, and never retries the release.
  Because the channel has already crossed its documented quiescence/destruction
  boundary by that point, the guarantee is documented exactly as retention of
  the storage/Buffer/library graph rather than a claim that the channel stayed
  attached. A barrier-driven regression proves the precise interleaving with no
  sleeps, and a negative mutation proves removing the recheck makes the
  host-storage assertion fail.

  **Callback `Releaser` disarmed before enqueue.** `std::deque::push_back` may
  allocate and throw, and the releaser was disarmed before it, so a throwing
  enqueue bypassed the release/uncertain-retention policy and dropped an owned
  provider buffer. The releaser now stays armed across the insertion and is
  disarmed only after it succeeds, all under the existing mutex. A failpoint at
  the enqueue-allocation boundary proves exactly one bridge-controlled release
  on failure, slot return on success, the uncertain path on a failing release,
  no wrapper destroyed after a failed release, and no leaked retention slot.

  **Null callback Buffer consumed a retention slot.** A slot was acquired
  before `!buffer` was tested, and the `Releaser` destructor returns
  immediately for a null value, so the slot was never recycled. A null Buffer
  is now rejected before any slot is acquired, never acquiring emergency
  ownership for an object that does not exist. A regression emits 40
  null-buffer callbacks against a `buffer_count` of 2 and proves later valid
  frames still acquire slots and complete normally.

  Also corrects `ams_mel_ir_stream_start` for `Lifecycle::Stopping`. Since Stop
  became logical-now/physical-later, a healthy stream with a queued frame or a
  live lease stays `Stopping` after a successful Stop, and Start reported
  `AMS_MEL_PROVIDER_FAILED` for it -- describing a healthy deferral as a
  provider failure, nondeterministically, depending only on whether the
  provider left a frame queued. Start now returns `AMS_MEL_STREAM_STOPPED`
  there, matching what `Receive` already reports; a poisoned stream is `Failed`
  and still reports `AMS_MEL_PROVIDER_FAILED`. Still ABI `0.1`, 90 exports,
  zero vendor delta.

- Make the pre-existing C2 lifetime assertions in `test_post_send_failure` and
  `test_bit_post_send_failure` deterministic. They slept a fixed 100 ms and then
  asserted `mode_completed`/`bit_completed` was already logged, but the retained
  worker completes the provider future asynchronously and the mock's delayed
  producer itself waits up to 40 ms, so under load -- exactly the CI condition
  of a 50x parallel `ctest` repeat -- the marker had not appeared yet. Both now
  poll the explicit completion condition under a generous bounded deadline. The
  assertion is not weakened: the retained worker must still genuinely reach the
  completion marker. This is unrelated C2 test infrastructure and is kept
  logically separate from the provider-buffer ownership work.

- Track provider buffers withheld from provider reuse in a new
  `retained_frames` count covering queued frames **and** live snapshots,
  guarded by the existing single teardown mutex and deliberately not derived
  from queue length. A buffer accepted by the bridge is released exactly once
  by bridge logic: the callback releases on malformed, not-accepting, and
  queue-full rejections and is explicitly disarmed on acceptance, with the
  later release performed by legacy `Receive`, snapshot/lease close, or
  Close-time queue discard. Destroying a C++ `shared_ptr` is never treated as
  a substitute for the published `release()` protocol.

- Extend the existing Image deferred-teardown state machine rather than adding
  a second one: physical provider teardown now requires
  `navigation requests == 0 AND retained_frames == 0`. `Stop` stays logical
  now / physical later and never blocks on an application-held lease; `Close`
  additionally discards still-queued, never-acquired frames and releases their
  provider buffers so none can be stranded in an unreachable queue, while
  preserving already-dequeued live leases. A live lease therefore survives
  public stream and Session close because the actual provider unload is
  **deferred**, not because bytes were copied; the Task 030A lease evidence was
  replaced with that stronger ordering property, and the owned-copy tests
  proving copied values survive real provider unload are retained.

- Call `irmel::Buffer::release()` outside the lifecycle mutex under a
  lock-to-claim / unlock-to-release / lock-to-publish discipline, and from the
  consumer/snapshot-close path rather than necessarily the provider callback
  thread. Pinned `Buffer.h` and `ImageListener.h` impose no callback-thread
  affinity on `release()`, and pinned Squall's `RequeueBuffer::release()` is an
  atomic one-shot guarded by a pool mutex; no stronger or universal claim is
  made.

- Treat provider-buffer release failure as a first-class safety case. A
  non-`Success` return or a thrown exception leaves ownership uncertain, so the
  `Buffer` is parked and never destroyed, its registered host byte range is
  never freed or resized, `retained_frames` is never decremented, `release()`
  is never retried, and allocation-free emergency retention keeps the graph
  alive. `ams_mel_ir_frame_snapshot_close` reports `AMS_MEL_PROVIDER_FAILED`
  and Ada `Frame_Lease.Close` now raises `Provider_Error` rather than falsely
  claiming the buffer was returned; automatic `Finalize` remains non-raising
  and preserves memory safety. This deliberate leak on uncertain ownership is
  documented.

- Document backpressure as intentional rather than hiding it: with
  `Buffer_Count = N` at most N provider buffers can be checked out at once
  unless the provider has an independent pool, so a slow consumer causes real
  provider-level drops. The bridge does not silently copy when the pool is
  exhausted. A provider-side "no reusable buffer available" event is not a
  bridge callback and is not counted as one; `frames_dropped_queue_full` keeps
  its existing meaning and no ABI counter field was added.

- Rework the mock provider's Image buffer handling to mirror pinned Squall
  `b1015728f904c799fa0c07489fce48e78f67845f`: a mutex-guarded available pool,
  checkout before the callback, requeue on successful `release()`,
  `accepting_releases = false` at channel destruction, and a one-shot guard
  that aborts on double release or double checkout. New deterministic
  native tests cover provider-buffer address identity, three-lease
  backpressure with explicit pool counters and condition variables rather than
  sleeps, deferred provider teardown ordering, Close-time queue discard,
  multiple concurrent leases across Stop/Close/Session close, and legacy
  `Receive` copy-then-release including the `BUFFER_TOO_SMALL` case that must
  consume and release nothing; the lease lifetime paths additionally run 20
  stress iterations. Ada adds a backpressure test and an owned `Full_Frame`
  independence test, and strengthens the alias proof to the full three-way
  identity.

- No C ABI change: ABI stays `0.1`, all 90 exports and `exports.map` are
  unchanged, no public type changed, no frozen record was grown, and the raw
  Rust and private Python declarations are untouched. Vendored upstream delta
  is exactly zero. RF MEL, RF header vendoring, RDMA, GPU/CUDA, FPGA mappings,
  Stacked Image, `cv::Mat` wrapping, zero-copy safe Rust, and zero-copy
  Python/NumPy remain unimplemented. See
  `docs/task-030b-provider-buffer-zero-copy.md`.

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
