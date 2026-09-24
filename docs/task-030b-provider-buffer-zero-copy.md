# Task 030B: MEL provider-buffer to Ada zero copy

Task 030A removed the native-snapshot-to-Ada payload copy. Task 030B removes
the remaining one: the MEL provider buffer to native queue copy. The bridge
high-rate path now performs **zero bulk payload copies from the MEL callback
buffer into Ada**.

## Exact claim

> **MEL provider-buffer -> Ada zero-copy through the bridge.**

This is deliberately narrower than "the whole sensor path has no copy".
Pinned Squall's `SquallImageChannel::udpCallback` still does
`std::memcpy(buffer->getImageAddress(), data, size)` to move received UDP
bytes into the registered MEL host buffer. That copy is upstream of this
bridge and is not removed, not claimed, and not modified. Nothing is asserted
about NIC DMA, kernel socket buffers, or physical sensor transport.

## Data path, old and new

```text
Task 030A                             Task 030B
--------------------------------      --------------------------------
MEL provider Buffer                   MEL provider Buffer
        |                                     |
        | COPY                                | retained ownership
        | frame.pixels.assign(...)            | NO PAYLOAD COPY
        v                                     v
native snapshot-owned vector          native frame snapshot / lease
        |                                     |
        | ZERO COPY                           | pointer + length
        v                                     | NO PAYLOAD COPY
Ada Frame_Lease / With_Pixels                 v
                                      Ada Frame_Lease / With_Pixels
```

Payload copies on the high-rate lease path:

```text
provider Buffer -> native            0   (removed here)
native          -> Ada               0   (removed in Task 030A)
payload-sized native allocation      0
payload-sized Ada allocation         0
per-pixel FFI calls                  0
per-pixel bridge transformation      0
```

`Copy_Pixels`, the legacy `Receive`, and `Full_Frame` are excluded because
copying is their documented contract.

## Evidence gate

The implementation was gated on pinned upstream and pinned provider evidence.
The gate passed: nothing in the pinned declarations or the pinned Squall
implementation contradicts delayed buffer release.

### Pinned `irmel::Buffer`

`native/vendor/ir-mel/include/irmel/library/irmel-types/Buffer.h`, IR MEL pin
`8d9224519f12b44e0b28815755c56a32a28d24a0`, states verbatim:

```
/**
 * @brief After a buffer handle is provided via the image produced callback it must be released via release() operation to allow the
 * library to reuse the buffer for a new image.
 */
/// @Required This function supports the callback release and must be provided by the implementer for all IR MEL implementations
virtual Return release() = 0;
```

Observations taken from that declaration, and only from it:

* `release()` is the published hand-back operation and its stated purpose is
  to allow the library **to reuse the buffer for a new image**. Until it is
  called, reuse is not permitted.
* Nothing states that `release()` must happen inside the callback, on the
  callback thread, or before the callback returns. The text says only that a
  buffer supplied through the callback must be released through `release()`.
* `virtual ~Buffer() = default;` is published, and there is no statement that
  destroying a `Buffer` releases it. Destroying a C++ `shared_ptr` is
  therefore **not** a substitute for the published protocol, and the bridge
  never relies on one.
* `getImageAddress()` and `getSize()` are published `const` accessors, so the
  image address can be read and retained.

### Pinned `ImageListener`

`ImageListener.h` declares:

```cpp
virtual void onImage(const Channel& channel, const FrameHeader& header, std::shared_ptr<Buffer> buffer) = 0;
```

and documents:

> The MEL uses a Buffer_obj registered with the associated Channel_obj to
> store the image data. The memory managed by the Buffer_obj is created by the
> Service and it is the Service's responsibility to indicate when it is
> finished with the Buffer_obj (i.e., the underlying memory) so that the MEL
> can reuse it.

Two facts matter. The buffer is delivered **by value as a `shared_ptr`**, so
the listener may legitimately retain it past the callback. And the service
decides **when** it is finished; the interface fixes no deadline.

### Pinned Squall `RequeueBuffer`

Squall pin `b1015728f904c799fa0c07489fce48e78f67845f`,
`interfaces/squall-ir-mel-impl/src/SquallImageChannel.cc`. Verified in source:

* `udpCallback` takes the pool mutex, and if `available_buffers` is empty it
  logs `"Dropped frame, no registered buffers are available"` and returns; it
  never generates a callback. Otherwise it pops the front buffer, so the
  buffer leaves the available pool **before** the callback.
* The listener receives `std::make_shared<RequeueBuffer>(buffer, buffer_state_)`,
  a wrapper that forwards every accessor to the registered buffer.
* `RequeueBuffer::release()` is one-shot via
  `released_.compare_exchange_strong`, returning `Fail` on a repeat; it then
  takes `state_->mutex`, returns `Fail` if `!state_->accepting_releases`,
  returns `Fail` if the buffer is not in `registered_buffers`, and otherwise
  pushes it back onto `available_buffers` and returns `Success`.
* `~SquallImageChannel()` calls `disable()`, resets the receiver, then sets
  `accepting_releases = false` under the mutex.

Consequences the bridge must honor, and does:

```text
Frame lease alive
      |
      v
provider Buffer still checked out
      |
      v
provider/channel MUST remain alive
      |
      v
Frame_Lease Close / Finalize
      |
      v
Buffer::release()
      |
      v
provider buffer reusable
      |
      v
only then may final teardown complete
```

### Release thread evidence

The published `Buffer` interface imposes **no** callback-thread requirement on
`release()`; no such statement exists in `Buffer.h` or `ImageListener.h`.
Pinned Squall's `RequeueBuffer::release()` uses an `std::atomic<bool>`
compare-exchange plus a `std::mutex` over the pool and touches no
thread-local or callback-thread state, so at that revision it is not
callback-thread-affine.

No universal claim is made. For providers whose implementation has not been
inspected, only the published interface applies, and the published interface
states no affinity either way.

This bridge calls `release()` from the **consumer / snapshot-close path**,
which is generally not the original provider callback thread.

### Existing mock-provider behavior

Before this task the mock round-robined a fixed `MockBuffer` array and
released inside the callback, so a buffer was never genuinely checked out.
That could not express backpressure. The mock now mirrors pinned Squall: a
mutex-guarded `available` pool, checkout before the callback, requeue on
successful `release()`, `accepting_releases = false` at channel destruction,
and a one-shot `outstanding_` guard that aborts on a double release or a
double checkout.

## Native frame representation

`frame.pixels.assign(pixels, pixels + pixel_count)` is gone, and with it the
`std::vector<std::uint8_t> pixels` member. The queued/snapshot representation
is now:

```text
QueuedFrame
    metadata ownership          small strings and small vectors
    provider Buffer owner       std::shared_ptr<irmel::Buffer> buffer
    validated payload address   const std::uint8_t *payload
    validated payload size      std::size_t payload_size
```

`bind()` publishes `view.pixels = {payload, payload_size}`, a borrowed span
directly over `Buffer::getImageAddress()`. The remaining vectors are metadata
whose length is bounded by the `FrameHeader`, not by the image size.

## Snapshot ownership and provider graph retention

```cpp
struct ams_mel_ir_frame_snapshot {
    QueuedFrame frame;
    std::shared_ptr<ImageStreamState> state;
};
```

The retained `ImageStreamState` is what keeps alive the four things a live
borrowed span needs: the registered host storage, the `irmel::Buffer` object,
the provider library containing the virtual `release()` implementation, and
enough of the channel graph that `release()` is still valid.

The strong reference is cycle-free by construction. A queued frame lives
*inside* the state and holds no back-pointer to it; a snapshot exists only
after its frame has been moved **out** of that queue. `ImageStreamState` is
therefore never reachable from anything the state itself owns.

## Checked-out buffer state machine

Every buffer the provider hands the bridge follows exactly one path:

```text
callback receives Buffer
        |
        +-- malformed / unsupported frame
        |      -> release() in the callback
        |
        +-- stream no longer accepting
        |      -> release() in the callback
        |
        +-- queue full
        |      -> count frames_dropped_queue_full
        |      -> release() in the callback
        |
        +-- accepted frame
               -> frame.buffer = buffer
               -> release.value.reset()   (callback releaser DISARMED)
               -> ++retained_frames
               -> released later by exactly one of:
                    legacy Receive copy
                    snapshot / Frame_Lease close
                    Close-time queue discard
```

The callback's RAII releaser owns the "release here" arm only. Accepting a
frame explicitly disarms it, so the callback releaser and the later lease
owner can never both release the same accepted buffer. Release is exactly
once by bridge logic, and never by a provider destructor.

## Outstanding-buffer accounting

`CallbackState::retained_frames` counts provider buffers currently withheld
from provider reuse: queued frames **plus** live snapshots. It is guarded by
the single teardown mutex and is deliberately **not** derived from queue
length, because a snapshot outlives its queue entry and several snapshots may
be outstanding at once.

```text
accepted into queue                    +1
queue-full rejection                    0
malformed rejection                     0
not-accepting rejection                 0
queue -> snapshot                 unchanged
legacy copy + release                  -1
owned Full_Frame + release             -1
snapshot / lease close                 -1
explicit queue discard                 -1
```

Uncertain release, i.e. `release()` returned non-`Success` or threw, is the
corrective (PR #42 second review) transition and is stated exactly:

```text
uncertain release of an ACCEPTED frame   unchanged   (stays counted forever)
uncertain release of a REJECTED frame          +1    (becomes counted forever)
no retention slot, release refused             +1    (becomes counted forever)
```

All three end in the same state: the buffer contributes exactly one unit to
`retained_frames` that is never removed, so physical teardown for that graph is
permanently blocked. The asymmetry is not counter fudging. A frame rejected in
the callback -- malformed, queue-full, or not-accepting -- was released
*before* queue acceptance and so was never counted; an uncertain result there
must therefore **add** the permanent obligation the accepted path already
holds. The post-condition

> an uncertain buffer contributes exactly one never-removed unit to
> `retained_frames`

holds uniformly across every path.

This is what the original PR #42 implementation got wrong. The callback
rejection paths neither counted the frame nor called
`image_stream_retain_failed()`, so after an uncertain release a later
Stop/Close could still observe `requests == 0 && retained_frames == 0` and
physically tear the provider graph down, clear the registered host storage, and
unload the provider library while Buffer ownership was uncertain.

`CallbackState::uncertain_release` is an additional lock-free backstop,
published with release ordering the instant any release for the stream is
uncertain. Physical teardown now requires

```text
requests == 0  AND  retained_frames == 0  AND  !uncertain_release
```

so teardown stays blocked even if the `retained_frames` publication under the
mutex could not be performed at all.

Decrements are saturating (`if (n != 0) --n`) and fail closed: if publishing
a decrement ever failed, the count stays high, which only over-defers
teardown.

Lease accounting begins at the **callback**, not at `receive_snapshot`,
because the provider buffer becomes unavailable for reuse as soon as the
callback hands it to the bridge.

## Host storage lifetime

The invariant, proved structurally rather than asserted:

```text
retained provider buffer exists
        =>
its registered host byte range exists unchanged
```

`stream.buffers.clear()` and `stream.storage.clear()` live inside
`image_stream_cleanup`, which is reachable only after the teardown lock has
confirmed `requests == 0 && retained_frames == 0`. A buffer whose release
failed is never counted down, so it permanently blocks that point and its
bytes are never freed or resized.

### Post-callback-drain recheck (PR #42 third review)

The initial gate alone is **not** sufficient, and the original implementation
relied on it alone. Cleanup checks `retained_frames`/`uncertain_release`, then
destroys and detaches the provider channel, waits for `callbacks_in_flight` to
reach zero, and only then clears the registered ranges, Buffers, and storage. A
callback that was **already in flight** when the gate was passed can fail a
`Buffer::release()` and publish its uncertainty after that gate but before the
drain finishes, so cleanup could free registered host memory although
`uncertain_release == true`, the failed wrapper was retained, and provider
ownership hand-back was uncertain.

`image_stream_cleanup` therefore re-evaluates the retained-buffer/uncertain
state **after** the drain reaches zero and **before** clearing any registered
range, registered Buffer, or backing storage.

Visibility is established rather than assumed. `retain_uncertain_buffer()`
publishes `uncertain_release` with release ordering, and `retained_frames`
under the mutex, strictly **before** the callback's `callbacks_in_flight`
decrement, which is an `acq_rel` RMW. The drain loop observes that count reach
zero with acquire ordering, so everything the callback published happens-before
the recheck; the mutex additionally orders the `retained_frames` read.

When late uncertainty is observed cleanup does not free registered host
storage, does not destroy retained Buffer ownership, retains the provider
library and Session graph via `image_stream_retain_failed()`, reports
`ImageCleanupOutcome::Failed` (so Stop/Close report `AMS_MEL_PROVIDER_FAILED`),
and never retries the uncertain release.

**Exact guarantee.** By this point the provider channel has already crossed its
documented quiescence/destruction boundary -- `disable()`, `detachChannel()`
and channel destruction all ran earlier in the same call -- so this path
deliberately does **not** claim the channel remained attached. What it
guarantees is what an uncertain hand-back actually requires: the registered
host storage, the registered `Buffer` objects, `CallbackState`,
`ImageStreamState`, the `Session` graph, and therefore the provider library
survive for process lifetime.

`test_teardown_race_late_uncertain` proves the interleaving deterministically
with a barrier/failpoint and never a sleep, and
`test_teardown_race_negative_mutation` proves that removing the recheck makes
the host-storage assertion fail.

## Callback handoff exception safety (PR #42 third review)

`std::deque::push_back` may allocate and therefore may throw. The original
accepted-frame code disarmed the callback `Releaser` **before** the insertion,
so a throwing enqueue bypassed the explicit release/uncertain-retention policy
entirely and silently dropped an owned provider buffer.

The handoff order is now:

```text
1. prepare frame.buffer and frame.retention
2. Releaser stays ARMED
3. queue.push_back(...)          <- the only throwing step
4. only now disarm the Releaser
5. ++retained_frames; notify the consumer
```

`frame.retention` is a copy of `release.retention` until step 4, so a throw
leaves exactly one owner of the slot -- the releaser -- and the discarded frame
is destroyed without ever having released anything. All five steps run under
`CallbackState::mutex`, so the queued frame cannot be consumed between
insertion and disarming and the two owners can never both release.

`test_enqueue_allocation_failure` drives
`AMS_MEL_TEST_ENQUEUE_ALLOCATION=fail` at exactly that boundary and proves a
failed enqueue performs exactly one bridge-controlled release, that success
returns the retention slot, that a failed release there follows the
uncertain-ownership path, that no wrapper is destroyed after a failed release,
and that no retention slot is leaked.

## Null callback Buffer (PR #42 third review)

The callback used to acquire a retention slot before testing `!buffer`. The
`Releaser` destructor returns immediately when `value` is null, so such a slot
was never recycled and every null-Buffer callback permanently consumed one of
the stream-sized slots.

A null `Buffer` is now rejected **before** any slot is acquired -- the simpler
design, which never acquires emergency ownership for an object that does not
exist. There is nothing to own and no `release()` is possible or owed; the arm
is a plain malformed/unsupported rejection.

`test_null_buffer_no_retention_slot` emits 40 null-Buffer callbacks against a
`buffer_count` of 2 and then proves later valid frames still acquire slots and
complete normally.

## Start during deferred teardown (PR #42 third review)

Because Stop is logical-now/physical-later, a healthy stream with a queued
frame or a live lease stays in `Lifecycle::Stopping` after a successful Stop.
`ams_mel_ir_stream_start` reported `AMS_MEL_PROVIDER_FAILED` for that state,
describing a healthy deferral as a provider failure -- and doing so
nondeterministically, since whether a Start after Stop observed `Stopping` or
`Stopped` depended purely on whether the provider's producer happened to leave
a frame queued.

Start now returns `AMS_MEL_STREAM_STOPPED` for `Stopping`, which is both
truthful and consistent with what `Receive` already reports for that state. A
genuinely poisoned stream is `Failed`, not `Stopping`, and still reports
`AMS_MEL_PROVIDER_FAILED`.

## Stop, Close, and Session close

```text
Stop
  -> stop accepting new callbacks
  -> queued frames remain consumable
  -> live snapshots remain valid
  -> physical teardown deferred while retained_frames != 0
```

Stop is **logical now, physical later**, exactly as it already was for
outstanding Navigation requests, and never blocks on an application-held
lease.

Close is different, because once the public stream owner is gone a queued but
never-acquired frame can no longer be consumed by that caller:

```text
Close
  -> logical stop
  -> discard still-queued, not-yet-acquired frames
  -> release those provider buffers safely
  -> preserve already-dequeued live Frame_Lease objects
  -> defer final physical teardown until
       navigation requests == 0
       AND retained provider-buffer leases == 0
```

`discard_queued_frames` swaps the whole queue out **under** the lock and then
performs every `release()` **outside** it, so provider code is never entered
while the teardown mutex is held and queued buffers are never stranded in an
unreachable queue.

Public Session close behaves the same way: the session owner is released, the
lease stays valid, and actual provider unload waits.

```text
public Session owner closed                           YES
provider library actually unloaded while lease alive   NO
```

## Deferred teardown state machine

No parallel teardown system was created. The existing Image state machine was
extended with one more term:

```text
May physically tear down the provider graph when:

    navigation_requests == 0
AND retained_provider_buffers == 0
AND no cleanup owner already active
```

The existing `cleanup_in_progress`, `cleanup_done`, `cleanup_complete`,
`cleanup_ok`, `cleanup_failed`, `public_owner_closed`, and the emergency
retention machinery are reused unchanged. When a lease releases the final
buffer it calls `image_stream_cleanup(state, true)`, the same deferred entry
point the final Navigation completion uses. That function claims cleanup
ownership exactly once; a caller that loses the claim blocks on
`cleanup_done` and adopts the published result. Exactly one owner performs
physical teardown, and the path is race-safe against public Stop, public
Close, Navigation completion, another lease closing concurrently, and
callback completion.

## Release failure semantics

A non-`Success` return or a thrown exception from `release()` means ownership
hand-back is **uncertain**. The repository's fail-safe rule applies:

* the exact callback `shared_ptr<irmel::Buffer>` is moved into a **preallocated
  intrusive retention node** and published on a process-lifetime lock-free
  list, so that Buffer object is never destroyed;
* its registered host byte range is never freed or resized;
* `retained_frames` is **not** decremented, so physical teardown is
  permanently deferred for that graph;
* the allocation-free `image_stream_retain_failed` emergency retention route
  is taken, so the surrounding graph survives even after the public owner is
  gone;
* `release()` is **never** retried, because the published interface
  guarantees no safe retry and pinned Squall makes release one-shot;
* the stream lifecycle is poisoned to `Failed`.

This is a deliberate leak on uncertain ownership, chosen over a possible
use-after-free or use-after-unload.

### Why the retention node is preallocated (PR #42 corrective)

The first implementation parked a failed Buffer with

```cpp
state->retained_failed_buffers.push_back(std::move(buffer));
```

That insertion can allocate and therefore throw. If it did, the local
`buffer` was destroyed. For pinned Squall the callback object is a
per-callback `RequeueBuffer` whose destructor requeues, so losing the last
reference after a failed release caused an unintended **second** `release()`,
violating the no-retry invariant. `image_stream_retain_failed(state)` retains
the `ImageStreamState` graph, but the graph does not own that callback
wrapper, so it is **not** by itself evidence that the wrapper survives.

The first correction moved the Buffer into a preallocated intrusive node, but
still fell back to a fixed 64-slot process-global reserve, and
`park_failed_buffer()` could return `false` when that reserve was exhausted.
Callers passed the Buffer by `std::move`, so a `false` return destroyed the
by-value `shared_ptr` while the caller's own had already been emptied, and the
`if (!parked && buffer)` fallback could not retain anything. The reserve
reduced risk without establishing the claimed invariant, and the callback
`Releaser` ignored the result entirely.

The second correction removes the arbitrary global limit from the correctness
argument. Emergency ownership is now **stream-owned and preallocated from the
configured `buffer_count`**:

```text
ams_mel_ir_stream_start(), BEFORE channel->enable()
    |
    | callback->retention_slots.resize(buffer_count)   <-- may fail harmlessly
    | callback->free_retention_slots.reserve(...)          here, before any
    v                                                      provider callback
provider callback: slot = acquire_retention_slot()    O(1), allocation-free
    |
    | slot index travels with the Buffer: queue -> snapshot -> lease
    |
    | Buffer::release()                               <-- may fail or throw
    v
retention_slots[slot] = std::move(buffer)   allocation-free, cannot throw,
                                            never retried
```

Moving a `shared_ptr` into an already-constructed empty vector element
allocates nothing and cannot throw. Successful release returns the slot to the
free list; uncertain release consumes it permanently. Ordinary `operator new`
is therefore never on the fail-safe path.

**The central rule** the design enforces is:

> the bridge never calls `Buffer::release()` unless it already owns a
> dedicated, preallocated retention slot for that exact Buffer.

Bounding argument, which replaces the 64-slot dependency. A slot is held
exactly while this stream has the corresponding provider buffer checked out,
plus permanently for each buffer lost to uncertain release. The bridge only
accepts a buffer whose base address is one of the `buffer_count` registered
host ranges it published, so a conforming provider cannot drive concurrent
checkouts above `buffer_count`. Capacity follows configured provider-buffer
capacity, not a process-global constant.

> **SUPERSEDED — see
> `docs/corrective-provider-buffer-release-handoff.md`.**
>
> The paragraph immediately above is **wrong**, and the defect it caused has
> been corrected. A retention slot is keyed to a **per-callback `Buffer`
> wrapper generation**, not to a physical buffer. A conforming provider makes
> the physical buffer reusable as soon as `release()` succeeds — pinned Squall
> republishes it *inside* `RequeueBuffer::release()`, with its pool mutex
> released, before that call returns — so one physical buffer legitimately has
> **two** live wrapper generations at once: the one whose release is still
> executing and the new callback the provider has already delivered for that
> same buffer. Physical `buffer_count` bounds simultaneous *checkouts*; it does
> **not** bound overlapping wrapper ownership.
>
> Because the old code held the hold slot across the provider call, a healthy
> reuse in that window found an empty free list and was misclassified as a
> non-conforming provider, producing a false malformed count and a permanently
> poisoned stream.
>
> Ownership is now split across two **disjoint** preallocated pools:
> `retention_slots` (HOLD, `buffer_count`) and `release_slots` (RELEASE,
> `2 * buffer_count`), with `CallbackState::begin_release()` handing the hold
> slot back atomically and strictly *before* the provider call. The
> `2 * buffer_count` bound is derived from the two disjoint release sources and
> is enforced by admission control, not assumed.

If a non-conforming provider *does* exhaust the pool, the bridge does **not**
release: it keeps the buffer, records the same permanent teardown obligation,
and parks it in the process-global reserve. That branch runs strictly *before*
any `release()` for the buffer, so even if the reserve is also exhausted and
the wrapper is dropped, the wrapper's own destructor performs its **first**
release. The forbidden case -- dropping a wrapper *after* a failed release,
making its destructor retry -- is unreachable by construction. The global list
therefore remains only a diagnostic record plus depth for that pre-release
branch, and no safety property depends on its size.

`CallbackState` also holds a `std::weak_ptr<ImageStreamState> owner`,
established during stream construction before any provider callback can exist,
so a callback-side uncertain release invokes exactly the same
`image_stream_retain_failed()` graph-retention policy as snapshot/lease
release.

`ImageStreamState::retained_failed_buffers` is retained only as an additional
diagnostic record and is written **after** the slot is published; safety never
depends on that `push_back` succeeding. The failpoint
`AMS_MEL_TEST_FAILED_BUFFER_PARK_ALLOCATION=fail` forces it to throw at
exactly that historical point, and
`test_release_failure_with_park_allocation_failure` proves the callback
wrapper still survives, is never destroyed, and is released exactly once.

### Callback-side release-failure evidence

The mock provider hands the listener a per-callback `RequeueBuffer`, modelling
pinned Squall: the bridge's `shared_ptr` is the wrapper's **only** owner, and
the wrapper marks itself requeued only when the hand-back actually succeeded,
so a dropped reference after a failed release performs the forbidden second
`release()`. The provider holds the wrapper only **weakly**, so a lost last
reference is observable immediately.

`test_callback_release_failure` drives each bridge rejection arm
deterministically -- `malformed`, `queue-full`, `not-accepting` -- by leasing
one healthy frame, putting the stream into the state that arm needs, and then
producing a frame that arm rejects whose in-callback release the provider
fails. Each proves, from provider state rather than from the bridge:

- release attempted exactly once;
- the exact callback wrapper still alive, destructor count 0;
- host backing bytes intact and unmodified;
- provider channel and `Control` never destroyed, so the provider library
  remains loaded and physical cleanup never runs;
- `Stop`/`Close` report `AMS_MEL_PROVIDER_FAILED`, and both public owners can
  be released without freeing the graph;
- release never retried.

`test_retention_slot_exhaustion` arms `AMS_MEL_TEST_RETENTION_SLOTS=exhaust`
and proves the bridge refuses to call `release()` at all, the wrapper stays
alive, and the graph is retained.

`test_retention_capacity_exceeds_legacy_reserve` opens `buffer_count = 96` and
sustains 200 checkout/lease/release cycles -- more than three times the old
global reserve -- entirely on this stream's own recycled slots.

`test_stop_status_healthy_versus_poisoned` asserts both halves of the Stop
contract explicitly: a healthy graph with an outstanding lease returns
`AMS_MEL_OK` with teardown deferred to the last lease close, while a poisoned
graph returns `AMS_MEL_PROVIDER_FAILED` and is never torn down.

`test_callback_release_failure_negative_control` arms
`AMS_MEL_TEST_DROP_FAILED_BUFFER=drop` so the fail-safe path deliberately drops
the wrapper, and asserts the invariants are then **violated** -- wrapper
destroyed, destructor-driven second release, attempt count above one -- which
is what makes the positive regressions non-vacuous. Those destructions are
counted in a separate `expected_failed_buffers_destroyed` bucket so the
deliberate defect can never mask a genuine one elsewhere.

Two independent source mutations were also confirmed to be caught: inverting
the retention-slot store so the wrapper is dropped, and removing both the
`uncertain_release` backstop and the rejected-frame count. The latter
reproduces the original defect exactly and fails on
`mock_failed_buffer_storage_intact()`, i.e. the registered host storage being
freed under an uncertain Buffer.

Explicit API operations report it truthfully:
`ams_mel_ir_frame_snapshot_close` returns `AMS_MEL_PROVIDER_FAILED` with the
diagnostic `provider buffer release failed; provider graph retained`, and Ada
`Frame_Lease.Close` raises `Provider_Error`. Ada `Finalize` may not propagate
an exception, so it preserves memory safety and retains the graph while
reporting nothing; call `Close` explicitly when the outcome matters.

## Memory ordering and locks

`CallbackState::mutex` remains the single teardown lock. Every racing read and
write of the pending Navigation request count, `retained_frames`, cleanup
ownership and result state, public owner state, channel owners, and logical
lifecycle happens under it.

Provider calls never run under it. The discipline is:

```text
lock   -> claim / move ownership of the Buffer
unlock -> Buffer::release()
lock   -> publish the count transition / fail-safe retention
```

## Backpressure is intentional

```text
Buffer_Count = N

at most N provider buffers can be simultaneously checked out
unless the provider has another independent pool
```

A lease held by the application keeps a provider buffer unavailable, so a slow
consumer causes genuine provider-level backpressure or provider-side frame
drops. This is not hidden by silently copying when the pool is exhausted;
exposing a real zero-copy ownership model is the point of the API. The
copying compatibility API remains available when independent ownership is
preferred.

A provider-side "no reusable buffer available" event is **not** a bridge
queue-full callback and is not counted as one. `frames_dropped_queue_full`
keeps its existing meaning: a callback that actually occurred and found the
bridge queue full. No public counter semantics changed and no ABI field was
added.

## Zero-copy identity proof

The defining proof, deterministic and pointer-based rather than timing-based:

```text
provider Buffer::getImageAddress()
        ==
native snapshot pixels.data
        ==
Ada With_Pixels first-element address
```

The test-only address log, already compiled out of the production facade by
`#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS`, was extended with a fourth field
holding the provider's own image address:

```text
<frame_id> <snapshot-address> <size> <provider-image-address>
```

`native/tests/test_ir_stream.c::test_provider_buffer_address_identity` asserts
the native half; `AMS_MEL_IR_Image_Lease_Tests.Test_Zero_Copy_Alias` asserts
the full three-way identity from Ada. No production ABI was altered.

## Proving the old copy is gone

`frame.pixels.assign(...)` no longer exists anywhere in `native/src`, and the
queued/snapshot representation contains no payload-sized byte vector. The
structural evidence is the backpressure test: if any bulk copy were
reintroduced, the bridge would no longer need to retain the provider buffer,
the pool would not stay empty with three live leases, and
`test_lease_backpressure` would fail at its `available == 0` assertions.

## Compatibility paths

Both copying APIs keep their ownership contracts.

```text
Receive / Full_Frame         owned copy
Acquire_Frame / With_Pixels  borrowed zero-copy path
Copy_Pixels                  explicit owned copy
```

Legacy `ams_mel_ir_stream_receive` and `AMS.MEL.IR.Receive`:

```text
provider Buffer
    |
    | one copy into caller / Ada-owned bytes (under the lock)
    v
owned Frame
    |
Buffer::release()  (outside the lock)
```

The buffer is reusable immediately after `Receive` returns, which
`test_legacy_receive_releases_buffer` asserts against the provider pool. A
`BUFFER_TOO_SMALL` result deliberately consumes and releases nothing: the
frame stays queued and the buffer stays retained.

`AMS.MEL.IR.Image.Receive` / `Full_Frame` / `Pixels (Full_Frame)`:

```text
provider Buffer
    |
native retained frame
    |
explicit copy into Full_Frame
    |
Buffer::release()
    |
fully Ada-owned Full_Frame
```

`Test_Owned_Full_Frame` tears the stream, Session, and provider down with no
lease outstanding and then re-reads the owned payload, proving the value is
genuinely independent of provider lifetime. No compatibility API became
borrowed.

## C ABI

No C ABI change. ABI version stays `0.1`, all 90 exports are unchanged,
`exports.map` is unchanged, and the raw Rust and private Python declarations
are unchanged. `ams_mel_ir_stream_receive_snapshot`,
`ams_mel_ir_frame_snapshot_view`, and `ams_mel_ir_frame_snapshot_close` keep
their signatures and their opaque-owner-plus-borrowed-span shape. Only what
the span points at changed, from snapshot-owned copied bytes to retained MEL
Buffer memory, and no frozen fixed-layout record was grown.

The externally observable snapshot guarantee is unchanged:

> A snapshot remains valid after public stream and Session close.

The implementation now achieves it by deferring the actual unload rather than
by copying bytes.

## Updating the Task 030A teardown evidence

Task 030A asserted that copied snapshot bytes survive actual stream, Session,
and provider teardown. On the lease path that implementation fact no longer
applies, so the lease evidence was replaced with the stronger correct
property, asserted by `test_lease_defers_provider_teardown`:

1. the public stream can close while a lease remains live;
2. the public Session can close while a lease remains live;
3. lease bytes remain valid at the same address;
4. provider channel and library teardown are demonstrably deferred, proved by
   the absence of `channel_destroyed` and `library_unloaded` in the lifetime
   log at that point;
5. lease close successfully calls `Buffer::release()`;
6. only afterward does channel destruction and library unload occur.

The owned-copy tests proving copied compatibility values survive actual
provider unload are retained unchanged.

## Remaining copies outside the bridge

Stated precisely, and limited to what the evidence supports:

* Pinned Squall copies received UDP payload bytes into the registered MEL host
  buffer in `SquallImageChannel::udpCallback`. Not removed, not claimed.
* Any copy performed by the kernel network stack, the NIC, or the sensor
  transport. Out of scope and unobserved.
* `Copy_Pixels`, `AMS.MEL.IR.Receive`, `AMS.MEL.IR.Image.Receive`, and
  `Pixels (Full_Frame)` copy by contract.
* Small per-frame metadata is copied; that is deliberate and bounded by the
  `FrameHeader`, not by the image size.

Inside the bridge, on the high-rate lease path, there are none.

## RF reuse implications

The reusable principle, recorded for the future RF MEL data plane:

```text
external / native backing storage
        |
explicit lifetime owner
        |
opaque bridge lease
        |
language-safe borrowed view when CPU-addressable
```

What transfers is the **ownership model**, not the IR array-view type. The IR
view is CPU-host-memory-specific. RF MEL permits receive endpoints over
application memory regions that may be ordinary host memory, RDMA-registered
memory, GPU memory, or FPGA/device memory, so a future RF region must not be
assumed representable as an Ada `Pixel_Array`; RF may instead need typed
spans, byte spans, packet views, device-memory descriptors, or explicit
non-CPU-addressable handles.

What does transfer directly: explicit checked-out/returned states, an exact
outstanding-lease count, deferred physical teardown gated on that count,
fail-safe retention on uncertain ownership, release outside the lifecycle
lock, and honest backpressure.

RF MEL, RF header vendoring, RDMA, GPU/CUDA buffers, FPGA mappings, Stacked
Image, `cv::Mat` wrapping, zero-copy safe Rust, zero-copy Python/NumPy, and
Scheduling are all explicitly **not** implemented by this task. No vendored
upstream file was modified: the vendor delta is exactly zero and
`docs/upstream-files.sha256.md` is unchanged.
