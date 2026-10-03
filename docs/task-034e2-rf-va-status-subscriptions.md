# Task 034E2 — RF VirtualAperture status-change subscriptions

## Baseline and scope

Fetched origin; PR #69 verified merged. Actual updated main:
`c13a44a0269289df19db1da329799e5aa538c2ff`. Clean worktree, no equivalent
branch/PR; new branch `feature/034e2-rf-va-status-subscriptions`.
Measured starting native 258/258, Python 115/115, production exports 164.
ABI 0.1. No dependency/vendor expansion, provider ownership graph refactor,
safe Rust subscription or public Python RF subscription.

RF MEL pin `762ce84c5555dd0f3ea66f36b321fecf8839b89f` declares:

```cpp
size_t addStatusCallback(const std::function<void(BaseVirtualAperture&)>& callback);
void removeStatusCallback(size_t key);
```

This callback's argument is a **provider-object reference**, not D2's by-value
JobIntervalStatus payload. Upstream promises neither unlocked callback entry nor
drain/quiescence or destruction of saved callable copies after removal.

## Five exports and one record

One opaque owner `ams_mel_rf_va_status_subscription`, exactly:

1. `ams_mel_rf_va_status_subscription_open`
2. `ams_mel_rf_va_status_subscription_wait`
3. `ams_mel_rf_va_status_subscription_get_statistics`
4. `ams_mel_rf_va_status_subscription_unsubscribe`
5. `ams_mel_rf_va_status_subscription_close`

New `ams_mel_rf_va_status_subscription_statistics_v1`: four uint64 counters
followed by uint32 `pending`, `stopped`, each exactly 0/1. All old declarations
and layouts, ABI 0.1, E1 snapshots and Claim-time getter set stay unchanged.

* `callback_entries`: every callable invocation, including stopped entries.
* `callbacks_coalesced`: active invocation with pending already set.
* `notifications_delivered`: successful Wait consumes pending.
* `callbacks_after_stop`: stopped invocation counted and discarded.

Counters saturate at UINT64_MAX; totals beyond saturation are not exact.
Wakeup uses Boolean pending, never sequence equality. Statistics locks once for
a consistent nonconsuming snapshot, allocates nothing and calls no provider.

## Signal-only, coalescing and serialization

The callback leaves BaseVirtualAperture& unnamed and unused: no dereference,
getID/getStatus/other getter, address copy, pointer comparison/retention, deferred
worker, Ada callback or application code. It only locks bridge signal state,
updates counters/pending, and wakes waiters. Notification means **this
registration's callable was invoked**. Association is with the VA used for
registration, not a verified identity of the argument. No changed status, report,
reference identity or exact transition history crosses the callback boundary.

Fixed state is a mutex, condition variable, stopped/pending flags and saturating
counters: callback frequency cannot grow storage. No per-callback thread, payload,
event owner or variable queue. Not lock-free, wait-free or hard-real-time.
Three callbacks before Wait produce entries=3, coalesced=2 and one pending
notification. Wait atomically clears pending; a later callback sets it anew.
Timeout zero polls; finite absence returns TIMEOUT. Stopped clears pending and
wakes receivers as STREAM_STOPPED, with precedence over pending. Invalid public
arguments consume nothing. Open synthesizes no initial notification, but permits
a provider's synchronous registration invocation.

Externally serialize Open/Unsubscribe with all same-VA calls/Close. Subscription
Wait/Statistics may overlap VA Close because the subscription owns only signal
state. Serialize every same-subscription use with destruction of its wrapper:
raw-handle/own-Close races are not safe. Callback publication and stop use the
same mutex, never held across add/remove. No provider getter reentrancy requirement.

## Registration transaction and permanent shell

Allocate the subscription, signal state, VA registration control, heap-stable
exact std::function and DSO pin before provider exposure. Prepare an intrusive
permanent-retention link: publication afterwards allocates nothing. Only then
consume the attempt, retain the shell and call addStatusCallback exactly once.
Preparation failures clean temporaries, publish nothing and allow a later first
attempt. A stored-then-throw registration consumes the attempt, stops/clears
signal state and publishes no owner, retaining the exact callable/state/DSO.
No returned key means no removal on VA Close; no guessed zero/sentinel.

One registration attempt per public VA, including after local Close or successful
Unsubscribe. This is an explicit bounded bridge profile, **not** an upstream
multiple-subscriber limitation. Provider users' other callbacks stay untouched.
Native size_t keys include **0 and SIZE_MAX**, stored unchanged with a separate
key-known flag; no C/Ada conversion or truthiness test.

Each exposed attempt intentionally retains one small callable/signal/DSO shell
for process lifetime. It uses RfC2ChildClaim::library_pin() but retains **no child
claim, VA/C2/provider owner, registration-control record, subscription wrapper,
application value or VA-wrapper back-pointer**. Successful removal still does
not authorize reclaiming the exact callable passed by const reference.

## Three distinct teardown operations

* **Explicit Unsubscribe:** live matching VA/subscription required, validated by
  bridge signal-state identity before mutation, not provider IDs/keys/reference.
  Stop/clear/wake, consume removal attempt **before** provider entry, exact-key
  remove outside locks, cache status and bounded allocation-free diagnostic.
  Wrapper remains open for statistics; Wait is stopped. Repeats return the cached
  outcome without retry, even after bad_alloc. Does not cancel/finalize Jobs or
  close VA, local observer, or other registrations.
* **Local Close/finalization:** stop/clear/wake, consume/null only the wrapper;
  null-idempotent, **no provider call**. Does not mean removal occurred.
* **VA Close:** consume owner, stop immediately, remove once if key known and not
  already attempted; destroy provider VA while existing claim protects C2/DSO,
  release claim and preserve deferred C2 shutdown. A surviving subscription is
  a stopped observer. Public C2 Close alone leaves live VA registration usable.

Removal exceptions map bad_alloc to INTERNAL_ERROR, others to PROVIDER_EXCEPTION.
Reception stays stopped and removal never retries. Failed removal does not retain
the graph to protect a callback that never touches it. C2 shutdown failure retains
the uncertain graph under the existing policy. VA Close deterministically returns
deferred C2 shutdown failure first, otherwise removal failure, otherwise OK.
When both fail, fixed combined diagnostics preserve both where possible **after
cleanup**. No diagnostic allocation can skip cleanup or cause retry.

VA Close includes synchronous provider removal and **may block**; no whole-Close
nonblocking/time-bound promise. Bridge does not additionally drain callbacks.
Permanent callable safety cannot repair provider-internal lifetime bugs or
legalize invocation with an invalid C++ reference.

## Safe Ada and subscribe-first application flow

`AMS.MEL.RF.C2.Virtual_Aperture_Notifications` provides limited controlled
Subscription, Open/Is_Open/Wait_For_Change/Statistics/Unsubscribe/Close. Is_Open
means wrapper ownership, so it stays True after Unsubscribe while Stopped=True.
TIMEOUT maps to C2.Timeout_Error, STREAM_STOPPED to Subscription_Stopped, other
errors to Provider_Error. Mutations use fixed diagnostics, never automatic retry.
Finalization is nonraising local-only closure with no saved VA borrow/pointer.
Ordinary Ada statistics expose typed Boolean flags, no raw public addresses.

The tested Ada client demonstrates:

```ada
declare
   Subscription : Notifications.Subscription := Notifications.Open (VA);
   Current : Queries.Status_Kind := Queries.Query_Status (VA);
begin
   -- Application-thread query AFTER registration; still not atomic.
   Notifications.Wait_For_Change (Subscription, Timeout);
   Current := Queries.Query_Status (VA);
   Notifications.Unsubscribe (VA, Subscription);
   Notifications.Close (Subscription);
end;
```

Queries run on the application thread, never inside the callback. They may see
a later state than the triggering change; multiple transitions coalesce. Queries
do not clear pending, and successful Wait does not prove Query_Status must differ.
Already-copied E1 reports remain immutable. No background refresh/report cache.

## Mock versus real pinned provider evidence

The existing mock implements per-VA callbacks and exact keys, copies or exact
references, synchronous entry, stored-then-throw, counters and other provider-owned
registration. A nonrecursive status mutex encloses callbacks; getters acquire
that mutex. Deterministic gates and bounded watchdogs prove callback completion
without getters or sleeps. All six E1 query counters stay unchanged on callbacks.
Race hooks pause **before** taking the production signal mutex; publication remains
one locked transaction. Saturation fixtures show continued pending/wakeup behavior.
Late calls after original VA destruction use a **different, still-live** mock
BaseVirtualAperture; no dangling-reference undefined behavior is used as evidence.
No-registration DSO-unload controls remain in separate legacy test processes.

Squall pin `b1015728f904c799fa0c07489fce48e78f67845f`,
`interfaces/squall-rf-mel-impl/src/SquallC2MEL.cc:539..552`, stores callback copies
in status_callbacks_ keyed by returned ID, erases only that key. Reviewed usages
show no status-change emission path. This is distinct from no-op JobDetail
interval-status registration. Ada VA and C ProductRx integration require timeout,
registration/removal and stopped lifecycle, not positive real transitions or
hardware health. No test hook manufactures real-provider emission.
