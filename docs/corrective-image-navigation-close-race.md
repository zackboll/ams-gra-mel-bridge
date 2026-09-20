# Corrective: Image Navigation completion vs. stream Close teardown race

This document corrects a concurrency/ownership defect in the Task 027B
asynchronous Image `NavigationReport` slice, found by retrospective peer review
of merged PR #28. It supersedes the teardown-synchronization description in
`docs/task-027b-validation.md`; the deferred-teardown *design* from Task 027B
is retained, only its synchronization is corrected.

## The original race

`ImageStreamState` carried the teardown-participating fields `requests`,
`cleanup_started`, `public_owner_closed`, `enable_attempted`, `channel`, and
`image_channel`. Task 027B documented `requests`/`cleanup_started`/
`public_owner_closed` as guarded by the frame callback mutex, but
`image_stream_cleanup` read and reset `channel`/`image_channel` and read
`enable_attempted` *outside* that mutex, and both `ams_mel_ir_stream_stop` and
`ams_mel_ir_stream_close` inspected `state->channel` outside it too.

The adapter's own Navigation completion worker reaches that code through
`complete() -> finish_stream() -> image_stream_cleanup(deferred=true)`. So the
completion thread could reset the `std::shared_ptr` member `channel` while an
application thread inside Stop/Close was reading the same member. A
`std::shared_ptr` *object* may not be concurrently read and modified without
synchronization, so this was a C++ data race and undefined behavior.

External serialization between application threads did not excuse this: the
second party to the race was the library's own internal completion thread, not
another application thread.

## The original stale-success scenario

A bad interleaving could produce a successful `Close` that had not actually
detached:

    public Close
      -> Stop logically stops the stream while a Navigation request is pending
         physical cleanup is deferred; Stop returns AMS_MEL_OK
      -> [final Navigation completion runs here]
             request count -> 0
             deferred cleanup begins
             detachChannel FAILS; the channel remains attached
      -> Close re-reads the request count, now 0
      -> Close reads state->channel

`Close` then returned the earlier `AMS_MEL_OK` captured from its own logical
Stop. Depending on how the concurrent reset interleaved, this could yield
`AMS_MEL_OK` *and* a cleared stream owner even though the physical detach had
failed and provider ownership of the channel was still uncertain.

## Synchronization invariant after correction

`CallbackState::mutex` is the single Image teardown lock. All of the following
is read and written only while that mutex is held:

| State | Meaning |
| --- | --- |
| `requests` | outstanding Navigation request count |
| `channel` | generic provider `Channel` owner |
| `image_channel` | provider `ImageChannel` owner |
| `enable_attempted` | whether `disable()` is owed to the provider |
| `public_owner_closed` | public `ams_mel_ir_stream *` has been released |
| `cleanup_in_progress` | cleanup ownership is currently claimed |
| `cleanup_complete` / `cleanup_ok` | a terminal cleanup published its outcome |
| `cleanup_failed` | a cleanup attempt failed |
| `callback->lifecycle`, `accepting`, `queue`, `counters`, `registered_ranges` | pre-existing frame callback state |

`cleanup_done` is a `std::condition_variable` on that same mutex. Nothing
spins and nothing polls.

## Cleanup ownership rules

`image_stream_cleanup` follows lock / provider-work / lock:

    lock
        wait on cleanup_done until no other thread owns cleanup
        if requests != 0            -> NotRequired
        if cleanup_complete         -> replay the published outcome
        if deferred and the stream is still logically usable -> NotRequired
        if no channel               -> NotRequired
        claim cleanup_in_progress
        move channel/image_channel into local owners
    unlock

        channel->disable()
        control->detachChannel(channel)
        provider channel destruction

    lock
        publish cleanup_ok / cleanup_complete (or cleanup_failed)
        release cleanup_in_progress
        cleanup_done.notify_all()
    unlock

Exactly one thread can own physical teardown at a time, and the shared
`channel`/`image_channel` members are empty for the whole unlocked provider
phase, so no other thread can observe them mid-teardown.

### Why provider calls stay outside the callback lock

A pinned provider may invoke the registered `NavigationReportResp` metadata
callback synchronously, and `disable()` is explicitly *not* a quiescence
boundary. Holding `CallbackState::mutex` across `disable()`, `detachChannel()`,
or provider channel destruction could therefore deadlock against the frame or
metadata callback paths, which take that same mutex. The local-owner copy is
what lets the provider phase run unlocked while the graph stays alive.

Provider channel destruction remains the callback quiescence boundary: it
happens only after detach succeeds and before any callback-accessible host
storage (`buffers`, `storage`, `registered_ranges`) is released.

## How Close synchronizes with completion cleanup

`ams_mel_ir_stream_close` performs its logical Stop, then takes **one**
synchronized decision. It first waits on `cleanup_done` until no cleanup is in
progress, so a cleanup owned by the completion thread has already published its
result before the decision is taken. Close never inspects `channel`
concurrently and never returns based on a stale earlier Stop result.

Under that single lock:

- **`requests != 0`** — a Navigation request is still pending. The public owner
  is released and `public_owner_closed` is set *in the same critical section*,
  which is what makes the owner release atomic with respect to the
  deferred-cleanup decision. Final request completion performs the physical
  teardown.
- **`requests == 0` and `channel` still set** — detach ownership is not
  established (a synchronous detach failure, or a deferred cleanup that failed
  detach and restored the graph). The public owner is retained for a retry and
  the status becomes `AMS_MEL_PROVIDER_FAILED`.
- **`requests == 0` and no `channel`** — cleanup finished. Close adopts the
  published `cleanup_ok` outcome rather than the possibly stale Stop status,
  and releases the owner.

`ams_mel_ir_stream_stop` likewise waits on `cleanup_done` and inspects
`channel` under the lock.

## When public-owner release becomes committed

Release is committed inside the single Close critical section described above,
simultaneously with setting `public_owner_closed`. There is no window in which
the completion thread can observe "owner released" without also observing the
deferred-cleanup decision, or vice versa.

## Detach-failure retry semantics

A failed `detachChannel` restores `channel`/`image_channel` into the shared
state under the lock, sets `cleanup_failed`, poisons the lifecycle to `Failed`,
and leaves `cleanup_complete` false — precisely because the outcome is not
terminal. A later Close retries the detach.

A successful retry establishes ownership safety and clears the public owner.
**It does not erase the original provider failure**: the stream lifecycle is
already `Failed`, so the retry Close still returns `AMS_MEL_PROVIDER_FAILED`.
"Retry succeeds" means ownership cleanup/release succeeded, not that the
historical provider failure is removed from the reported status. This preserves
the existing contract proven by `test_cleanup_failure("detach-fail")`.

A `disable()` failure with a *successful* detach remains distinct: ownership
safety is proven, the public owner may be cleared, and the status is
`AMS_MEL_PROVIDER_FAILED`. Disable failure is never treated as equivalent to
detach ownership failure.

## Deferred permanent-retention semantics

If a deferred detach fails after the public owner is already gone, there is no
owner left to retry. `image_stream_retain_failed` then roots the whole state
graph permanently through the allocation-free `emergency_self`/`emergency_next`/
`emergency_retained` fields, mirroring `ChannelState::retain_failed` in
`ir_c2.cpp`. Provider code is never unloaded while detach ownership is
uncertain. The terminal Navigation result fails closed, as designed in
Task 027B.

## Deterministic regression coverage

`native/tests/test_ir_navigation.c` adds two barrier-driven regressions. The
barriers are compiled only under `AMS_MEL_ENABLE_TEST_FAILPOINTS`, and a stage
blocks only when the test arms it with a `<base>.<stage>.arm` marker, so every
other stage stays free-running. Production behavior never depends on an
environment variable or marker file.

`test_close_races_failed_deferred_cleanup` forces exactly the original
interleaving: Close parks at the armed `close-decision` barrier after its
logical Stop and before committing the owner-release decision; the final
Navigation request then completes, deferred cleanup starts, and detach fails
once. It proves the racing Close returns `AMS_MEL_PROVIDER_FAILED` with the
owner retained, that no channel destruction or library unload occurs while
detach ownership is uncertain, that request completion itself stays safe, and
that a second Close retries detach, clears the owner, and destroys the channel
before library unload.

`test_close_races_successful_deferred_cleanup` parks the deferred cleanup at
the armed `before-detach` barrier so it provably holds cleanup ownership, then
runs a public Close on a second thread. It proves Close blocks on that cleanup,
adopts its successful outcome, returns `AMS_MEL_OK`, clears the owner exactly
once, and that cleanup occurs exactly once.

Reverting only the Close outcome-adoption logic makes
`test_close_races_failed_deferred_cleanup` fail on the stale-success
assertion, confirming the regression detects the original defect.

## Scope

No public feature, ABI export, or ABI version change. The 88-export inventory
is unchanged. No vendored file and no `docs/upstream-files.sha256.md` entry was
modified. The correction is confined to Image teardown synchronization; C2,
Health, Instrumentation, and Track were not modified.
