# Corrective: provider-buffer release and reuse handoff

## Starting state

PR #43 review-correction session:

- Actual local HEAD, remote branch head, and PR `headRefOid` at start:
  `6b71d6abf81413511166a2854777cd3e9a5f9c5d`.
- The working tree was clean, the branch had not advanced, PR #43 was open and
  unmerged, and auto-merge was disabled. No reset, force-push, or replacement
  PR was used.

Original corrective branch creation:

- Actual starting SHA: `b8df5aee274fee31faf3cece54be2a872e27cc53`
  (`origin/main`, merge of PR #42, task 030B provider-buffer zero copy).
- Branch: `corrective/provider-buffer-release-handoff`, created from that
  exact `origin/main`.
- `origin/main` had not advanced past the reviewed SHA, so no intervening
  change had to be assessed and nothing was reset backwards.
- Working tree was clean at branch creation; no unrelated local change was
  modified, stashed, or discarded.

## The source-level finding

`release_retained_buffer()` and the callback-side `Releaser` destructor both
held the frame's emergency-ownership **retention slot** for the entire duration
of `Buffer::release()`, and returned that slot to the free list only *after*
the provider call completed:

```
acquire hold slot  ->  ...  ->  buffer->release()  ->  return hold slot
                                ^^^^^^^^^^^^^^^^^
                                provider republishes the physical
                                buffer somewhere inside here
```

A conforming provider makes the physical buffer reusable as soon as the release
succeeds. Pinned Squall
(`b1015728f904c799fa0c07489fce48e78f67845f`,
`interfaces/squall-ir-mel-impl/src/SquallImageChannel.cc`) pushes the registered
buffer back onto `available_buffers` **inside** `RequeueBuffer::release()`, and
releases its pool mutex before that call returns. The bridge therefore cannot
have finished any bookkeeping for the old wrapper at the moment the buffer
becomes reusable.

`CallbackState::retention_slots` was sized at exactly `buffer_count`. With every
slot occupied — three live snapshots over three registered buffers — a brand new
`onImage` callback for that successfully returned physical buffer found
`free_retention_slots` **empty**. The bridge classified that healthy reuse as a
non-conforming provider:

- refused to call `release()` at all,
- incremented `malformed_or_unsupported_frames`,
- published `uncertain_release`,
- poisoned the lifecycle to `Failed`,
- and retained the provider graph permanently.

A healthy provider doing exactly what its contract permits therefore produced a
permanently poisoned stream and a false malformed count.

## Why physical `buffer_count` did not bound wrapper overlap

This is the assertion the previous task documented and this correction
retracts. A retention slot is keyed to a **per-callback `Buffer` wrapper
generation**, not to a physical buffer. During the window above, one physical
buffer legitimately has **two** live wrapper generations at once:

| generation | state |
| --- | --- |
| N | `release()` still executing; provider has already republished the buffer |
| N+1 | brand new `onImage` callback for that same physical buffer |

`buffer_count` bounds simultaneous physical **checkouts**. It never bounded
overlapping wrapper ownership, because the provider's republication point and
the bridge's reconciliation point are not the same instant.

## Deterministic baseline reproduction

### PR #43 repeated-generation correction

The reviewed head `6b71d6abf81413511166a2854777cd3e9a5f9c5d` still used six
(`2 * buffer_count`) RELEASE slots for a three-buffer stream. A new counted
mock barrier reproduced the remaining defect without either exhaustion
failpoint: A generation 0 was released on its own thread and paused after
republication; A1 through A5 repeated that sequence while B and C remained
live and byte-identical. All six slots became genuinely occupied. Closing A6
returned `AMS_MEL_PROVIDER_FAILED`; no seventh provider call occurred, A stayed
withheld, and teardown could not complete. The focused baseline command exited
zero only because the regression expected and asserted that reviewed failure:

```text
PASS: reviewed repeated-generation release pool exhausted
baseline_exit=0
```

This executed result, not a source prediction, disproves the former argument:
disjoint release sources do not bound the number of overlapping generations
created by either source.

Two mock scenarios were added, both pooled and driven on demand from the test
thread. Neither sleeps to create the interleaving, and both record that the
forced window was actually reached so the test asserts it rather than assuming
it.

- `handoff-pause-after` -- `release()` republishes the physical buffer into the
  provider pool, then blocks inside the provider call on a condition variable
  until the test releases it. The forced window is exactly "successfully
  published, bridge bookkeeping not yet reconciled".
- `handoff-reuse-inside` -- `release()` republishes the buffer and then, still
  inside the release call and **with the pool mutex released**, drives one
  complete new callback generation for that same physical buffer before
  returning. This is why the fix cannot depend on an unsupported
  return-before-reuse assumption.

`AMS_MEL_TEST_RETENTION_SLOTS` is deliberately **not** used by these
regressions: manufacturing exhaustion that way would bypass the real handoff
defect.

Scenario (`test_release_reuse_handoff`, `native/tests/test_ir_stream.c`):

1. three provider buffers configured; three frames produced and acquired as
   three live snapshots;
2. `ams_mel_mock_pool_available() == 0` -- every physical buffer checked out, so
   every retention slot is occupied;
3. one snapshot closed on its own dedicated thread (distinct owner; the same
   public handle is never touched by two threads);
4. `mock_handoff_wait_window() == 1` and `ams_mel_mock_pool_available() == 1`
   prove the window was genuinely reached;
5. a new callback for that same physical buffer is forced inside the window;
6. the paused release is let go and its thread joined;
7. the other two snapshots stay live throughout.

### Observed baseline failure

Running the regression against the defective protocol -- reinstated exactly by
the `AMS_MEL_TEST_SKIP_RELEASE_HANDOFF=skip` negative control, which keeps the
release slot but removes the hold-slot handoff:

```
$ AMS_MEL_TEST_SKIP_RELEASE_HANDOFF=skip ./native/build-tests/test_ir_stream
FAIL at native/tests/test_ir_stream.c:2221: ams_mel_ir_stream_receive_snapshot(...) == AMS_MEL_OK
FAIL at native/tests/test_ir_stream.c:2543: test_release_reuse_handoff("handoff-pause-after") == EXIT_SUCCESS
exit=1
```

The failing assertion is the acceptance of the reusing callback. The stream had
been poisoned to `Failed`, so `receive_snapshot` returned
`AMS_MEL_PROVIDER_FAILED`, and `malformed_or_unsupported_frames` had been
incremented for a perfectly healthy frame.

## Exact interleaving

```
  bridge thread (snapshot close)        provider thread (new callback)
  ------------------------------        ------------------------------
  take hold slot H for wrapper N
  buffer->release()  ---------------->
                                        physical buffer pushed back
                                        onto available_buffers
                                        pool mutex released
                                        onImage(wrapper N+1) for the
                                        SAME physical buffer
                                          acquire_retention_slot()
                                          -> free list EMPTY   <-- DEFECT
                                          -> refuse release
                                          -> malformed++, poison stream
  release() returns
  return hold slot H                     (too late)
```

## Ownership state transitions

### Corrected bounded protocol

The final implementation bounds quantities it actually controls:

| quantity | enforced bound | ownership while saturated |
| --- | ---: | --- |
| physical checkouts in HOLD or callback-deferred state | `buffer_count` | HOLD/deferred preallocated slots |
| provider `Buffer::release()` calls executing | **1 per stream** | `active_release` plus a reserved uncertain slot |
| callback-side deferred releases | `buffer_count` | preallocated deferred slots; drained iteratively |
| permanently uncertain wrappers | `buffer_count` | preallocated uncertain slots, one per physical checkout removed from reuse |

An ordinary explicit close encountering the active limit waits while retaining
its HOLD slot. It therefore reports the outcome of its own release and never
turns temporary congestion into provider failure. A callback reentered from the
active provider call cannot wait for itself; it transfers its exact wrapper to
a preallocated deferred slot, returns from the callback, and the active executor
drains deferred work without recursive provider-release nesting. Every active
or deferred obligation increments teardown accounting.

Before each provider call the exact wrapper is already present in a reserved
uncertain slot, so failure/throw requires no allocation. Success clears and
recycles it. Failure leaves it there permanently, is never retried, publishes
uncertainty, and retains the provider graph; the execution permit itself is
released so unrelated healthy physical buffers can still be returned.

### Before

One pool, `retention_slots`, sized `buffer_count`, covering both obligations:

| state | slot |
| --- | --- |
| queued frame / live snapshot | held |
| release in progress | still held (the defect) |
| release succeeded | returned, after the provider call |
Preallocated ownership is built in `ams_mel_ir_stream_start()` before
`channel->enable()` and therefore before any callback can run:
### After

Two **disjoint** preallocated pools, both built in `ams_mel_ir_stream_start()`
before `channel->enable()` and therefore before any callback can run:
| `active_release` | 1 | one provider release executes |
| `deferred_releases` | `buffer_count` | callback-side releases cannot wait for their enclosing provider call |
| `uncertain_slots` | `buffer_count` | allocation-free fallback reserved before each attempt and retained permanently only on uncertainty |
| pool | size | occupied while |
The distinguished states are explicit:
| `retention_slots` (HOLD) | `buffer_count` | the bridge holds a buffer it has not yet begun releasing -- a queued frame or a live snapshot |
| `release_slots` (RELEASE) | `2 * buffer_count` | one bridge-controlled `release()` is unresolved; permanently if that release was uncertain |

The distinguished states are now explicit:
| explicit close waiting for execution | hold slot; no provider call started |
| callback-side release waiting for execution | deferred slot; hold slot already returned |
| release executing | `active_release` plus reserved uncertain slot |
| permanently uncertain release | uncertain slot never returned; `uncertain_release` set; counted forever |
| registered physical buffer | `registered_ranges` / `ImageStreamState::buffers` |
`CallbackState::release_buffer()` is the single entry point. Admission and
ownership transitions happen under `CallbackState::mutex`; provider calls do
not:
| permanently retained uncertain release | release slot never returned; `uncertain_release` set; counted in `retained_frames` forever |

`CallbackState::begin_release()` is the single entry point and performs the
    wait for active permit OR defer if callback-side reentry
    reserve uncertain_slots[u]
    active_release = wrapper
    return_retention_slot(H)
```
    buffer->release()
on success  -> recycle u; admit/drain next obligation
on failure  -> retain u permanently; never retry; admit/drain other obligation
    return_retention_slot(H)       <-- the corrective step
unlock
    buffer->release()              (provider call, never under the mutex)
on success  -> return_release_slot(r)
on failure  -> retain_uncertain_buffer(...); slot r never returned
```

## Invariants

- **A.** A healthy provider reusing a successfully returned physical buffer
  cannot cause false slot exhaustion: the hold slot is free before the provider
  can republish, because it is returned in the same critical section that
  precedes the release call.
- **B.** Before any bridge-controlled release attempt the exact wrapper already
  owns a dedicated, preallocated uncertain slot. Emergency ownership is
  allocated *before* the release, never after one
  has failed.
- **C.** A failed or throwing release is never retried and its exact callback
  wrapper is never destroyed: the uncertain slot keeps owning it and is never
  returned to the free list.
- **D.** Provider calls remain outside the lifecycle/callback mutex.
  Admission performs no provider call; `release()` runs unlocked.
- **E.** Temporary execution saturation waits or defers with safe ownership; it
  is not provider failure and does not create permanent uncertainty.
- **F.** Physical teardown still requires
  `requests == 0 && retained_frames == 0 && release_obligations == 0 &&
  !uncertain_release`, and
  the post-callback-drain recheck is unchanged.
- **G.** No callback deadlock. Callback-side reentry transfers into deferred
  ownership and returns; the active executor drains a finite chain iteratively.
  Production logic, not a one-shot mock, limits provider release nesting to one.
- **H.** Explicit-close behavior is unchanged: it waits through temporary
  congestion and reports its own release outcome; a failed release returns
  `AMS_MEL_PROVIDER_FAILED` from
  `ams_mel_ir_frame_snapshot_close`, and Ada finalization stays non-raising.

## Capacity and boundedness argument

The implementation directly enforces one active provider call. Every explicit
waiter keeps one physical checkout in HOLD; every callback-deferred obligation
keeps one distinct physical checkout in a deferred slot. Their combined count
cannot exceed `buffer_count`. An uncertain slot is consumed only after an actual
failed/throwing provider call, and that physical checkout is never republished,
so at most `buffer_count` uncertain slots can be consumed. Wrapper generations
are not used as a bound.

Crucially, this bound is **enforced**, not merely argued: `begin_release()`
performs admission control and refuses to release when no release slot is
available, falling back to the already-proven pre-release branch that parks the
buffer, blocks teardown permanently, and never retries a failed release. A
permanently consumed release slot also permanently removes its physical buffer
from provider reuse and poisons the stream, so such consumption cannot recur on
a healthy stream.

The legacy process-global 64-entry reserve was **not** increased and remains a
publication/diagnostic record used only by the pre-release refusal branch.

## Locking and callback reentry

`CallbackState::mutex` remains the single teardown lock. `begin_release()`,
`return_release_slot()`, and `return_retention_slot()` all run under it and all
are allocation-free and `noexcept`. No provider call is made while it is held.
`handoff-reuse-inside` exercises a provider that calls back into the bridge
from inside a still-executing `release()`; that callback takes the same mutex,
finds it free, and completes normally.

## Preservation of uncertain-release handling

Every existing protection is retained and rerun: failed and throwing release,
exact failed-wrapper survival, allocation failure while recording failed-buffer
diagnostics, malformed/queue-full/not-accepting callbacks, retention-slot
exhaustion safety, null-buffer callbacks, enqueue allocation failure,
post-callback-drain teardown recheck, live leases across stream and Session
close, navigation-request/stream-close interactions, provider-buffer/snapshot/
Ada-view address identity, and the existing large-buffer-count coverage. No
existing assertion was weakened.

## Regression coverage added

Native (`native/tests/test_ir_stream.c`):

- `test_release_reuse_handoff("handoff-pause-after")` and
  `("handoff-reuse-inside")` -- both forced windows. Assert: the new callback is
  accepted normally; the new snapshot uses the returned physical buffer's
  address; the two still-live snapshots keep their addresses **and** bytes; no
  false malformed or queue-full count; no permanent uncertain-ownership state;
  exactly one bridge-controlled release attempt per wrapper
  (`pool_releases` delta == `pool_callbacks` delta); provider capacity fully
  restored after all owners close; channel destroyed before Control destruction
  and provider unload, with no emergency-retention leak.
- `test_repeated_generation_release_backpressure` -- eight A generations,
  exceeding the reviewed six-slot guess, prove one active call, waiting closes,
  recovery after each counted release, exact A address reuse, unchanged B/C
  bytes, exactly-once release, restored capacity, and normal teardown.
- `test_callback_rejection_reentry_chain` -- nine actual queue-full callbacks
  are driven from release-side reuse. All ten wrappers release exactly once and
  observed maximum provider release nesting is one.
- `test_concurrent_release_distinct_owners` -- two snapshots closed from two
  threads with a third live; counted entry proves intentional serialization and
  recoverable backpressure rather than merely creating two threads.
- `test_close_discard_release_backpressure` -- pauses each release during actual
  queue discard and proves no channel teardown occurs until all three resolve;
  a test-specific log then proves channel destruction, Control destruction, and
  library unload in order.
- `test_release_reuse_handoff_common_paths` -- the same window through legacy
  owned-copy `Receive` and through Close-time queue discard, i.e. the other two
  entries into the common release helper.
- `test_release_slot_admission_control` -- `AMS_MEL_TEST_RELEASE_SLOTS=exhaust`
  drives the refusal branch: `AMS_MEL_PROVIDER_FAILED`, no `release()`
  attempted, wrapper alive, no destructor-driven second release, capacity not
  restored, teardown blocked.
- `test_release_reuse_handoff_negative_mutation` -- the mutation check.

The callback-side release entry is covered by the existing
`test_callback_release_failure(...)` regressions, which use actual rejection
scenarios (malformed header, queue-full, not-accepting) rather than artificial
slot exhaustion, and which now exercise the corrected `Releaser` handoff.

Ada (`ada/tests/src/ams_mel_ir_image_lease_tests.adb`):

- `Test_Release_Reuse_Handoff` -- acquires all A/B/C leases before closing A,
  records A's address and frame ID 1, and requires replacement frame ID 4 at
  A's exact address. B/C bytes stay unchanged and a test-only lifecycle log
  records exactly four delivered callbacks. No production C export was added.

## Mutation evidence

`AMS_MEL_TEST_SKIP_RELEASE_HANDOFF=skip` removes exactly the essential
corrective step -- `begin_release()` still takes a release slot but no longer
returns the hold slot before the provider call -- reinstating the defective
protocol.

```
$ ./native/build-tests/test_ir_stream
PASS: C IR Mono8 stream receive/queue/lifetime contract
exit=0

$ AMS_MEL_TEST_SKIP_RELEASE_HANDOFF=skip ./native/build-tests/test_ir_stream
FAIL at native/tests/test_ir_stream.c:2221: ... receive_snapshot(...) == AMS_MEL_OK
FAIL at native/tests/test_ir_stream.c:2543: test_release_reuse_handoff("handoff-pause-after") == EXIT_SUCCESS
exit=1
```

The failpoint is compiled out of production builds
(`AMS_MEL_ENABLE_TEST_FAILPOINTS`); no temporary source mutation remains. This
was verified directly:

```
$ strings native/build/lib/libams_mel_c.so.0 | grep -E 'AMS_MEL_TEST_(RELEASE_SLOTS|SKIP_RELEASE_HANDOFF)'
(no output)
$ strings native/build-tests/lib/libams_mel_c.so.0 | grep -E 'AMS_MEL_TEST_(RELEASE_SLOTS|SKIP_RELEASE_HANDOFF)'
AMS_MEL_TEST_RELEASE_SLOTS
AMS_MEL_TEST_SKIP_RELEASE_HANDOFF
```

## Scope and compatibility

Unchanged: C ABI version 0.1; the existing **90** production exports and
`native/src/exports.map`; all frozen public C record layouts; the public Ada
`Frame_Lease`/`Acquire_Frame`/`With_Pixels`/`Copy_Pixels` APIs; owned-copy
`Receive`/`Full_Frame` behavior; zero bulk payload copies on the lease path;
the production/test build-tree separation; vendored upstream bytes and
provenance hashes.

Verified:

```
$ nm -D --defined-only native/build/lib/libams_mel_c.so.0 | grep -c ' T ams_mel_'
90
$ git diff --stat origin/main -- native/vendor/ native/src/exports.map native/include/
(no output)
```

No Scheduling, RF, StackedImage, GPU/CUDA, RDMA, FPGA, OpenCV, provider-side
binding, Rust/Python API expansion, or OMS/UCI feature is included. No
channel-framework or `RequestFor`-worker redesign was performed.

## Test commands and actual results

| Command | Result |
| --- | --- |
| `make test-native` | **PASS** -- 15/15 CTest tests |
| `make test-build-isolation` | **PASS** |
| `alr -C ada build` | **PASS** |
| `alr -C ada/tests run` | **PASS** -- 15/15 suites, including the new `Test_Release_Reuse_Handoff` |
| `make test-rust` | **PASS** |
| `make test-python` | **PASS** |
| `make check-ada-format` | **PASS** (`make format-ada` was run first, as AGENTS.md requires) |
| `git diff --check` | **PASS** -- clean |
| `make check` | **FAILS** -- see limitation below |
| GCC Debug CMake + CTest | **PASS** -- 15/15 |
| GCC Release CMake + CTest | **PASS** -- 15/15 |

### `make check` limitation

`make check` invokes `scripts/test_ada.sh`, which requires bare GPRbuild on
`PATH`:

```
GNAT/GPRbuild is required on PATH. Alternatively use: alr -C ada/tests run
make: *** [Makefile:58: check] Error 1
```

Only `gnatmake` is present on this machine; `gprbuild` is not. This was
verified to be **pre-existing at the base**: a clean worktree of
`b8df5aee274fee31faf3cece54be2a872e27cc53` fails the same script with the same
message. It is a toolchain-availability limitation, not a regression from this
change. The native portion of `make check` that did run passed 15/15, and the
Ada gates were exercised through the supported Alire path, which passes. This
command is reported as failed, not relabelled as passed.

### Validation not performed

- **Real Squall provider.** The pinned Squall checkout/runtime is not available
  on this machine, so `make test-squall-ir*` was not run. All evidence in this
  document is **mock-provider evidence**: the interleaving is forced by the
  test-only mock, which is modelled on pinned Squall's `RequeueBuffer` but is
  not the real provider. No real-provider claim is made.
- **Clang** was unavailable locally (`clang: command not found`). GCC Debug and
  Release were both exercised locally. CI coverage for the pushed exact head is
  reported in the pull request.
