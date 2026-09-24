# Corrective: provider-buffer release executor and lifecycle

## Scope and starting state

This document records PR #43's final provider-buffer release protocol. It is a
consumer-side correction only; it adds no MEL feature or public operation.

The review-correction session started from
`a10321e069051adfbb4ce735d9ed0cd904603bf8`. Local HEAD, the remote
`corrective/provider-buffer-release-handoff` head, and PR #43 `headRefOid` all
matched that SHA. The tree was clean, the PR was open and unmerged, and
auto-merge was disabled. No reset, force-push, replacement branch, or replacement
PR was used.

ABI 0.1, all 90 production exports, `exports.map`, frozen C layouts, and the
existing Ada APIs remain unchanged. The lease path still performs zero bulk
payload copies; owned `Receive`/`Full_Frame` paths still copy by contract.

## Historical defects

### Hold-slot handoff

The earlier PR #43 correction fixed a healthy reuse defect. A provider may
republish a physical buffer inside `Buffer::release()`, before that call returns.
The bridge had retained the old wrapper generation's HOLD slot until after the
call returned, so a callback for the new wrapper generation could see a false
HOLD-pool exhaustion. The final protocol returns HOLD ownership at admission and
uses separate per-operation uncertain ownership.

Physical `buffer_count` bounds simultaneous physical checkouts. It does **not**
bound all overlapping wrapper generations, and no current-state section claims
a `2 * buffer_count` wrapper-generation pool.

### Executor handoff after failure

At the reviewed head, executor identity was inferred from whether
`active_release` was nonempty. A failed operation cleared `active_release` and
`active_uncertain_slot`, notified waiters, and unlocked before the same thread
resumed deferred draining. A distinct explicit closer could claim that apparent
idle slot, after which the original thread could overwrite shared active state
while continuing deferred work.

The executed negative control
`AMS_MEL_TEST_SURRENDER_RELEASE_EXECUTOR=surrender` recreates that exact
post-failure surrender. With A failing, malformed callback B deferred, and C's
distinct closer admitted in the forced interval, the mock observes two
simultaneous provider release calls. This is executed mutation evidence, not a
source prediction. The corrected path for both returned failure and thrown
exception observes a maximum of one.

### Final wrapper destruction under the mutex

Success previously reset shared active and uncertain owners under
`CallbackState::mutex`. For a deferred wrapper these could be the final strong
references, allowing a provider-defined destructor or shared-pointer deleter to
run under the lifecycle/callback mutex.

### Release obligation omitted from public Close

Physical cleanup checked `release_obligations`, but Stop and public Close's
owner-release decision did not do so consistently. An isolated malformed
callback could therefore have a healthy release in progress while
`retained_frames == 0`, `requests == 0`, and `uncertain_release == false`.

The executed negative control
`AMS_MEL_TEST_OMIT_RELEASE_OBLIGATION_GATE=omit` reproduces the reviewed public
outcome: Close returns `AMS_MEL_OK` while leaving the stream handle non-null.
The corrected path waits for finite healthy release work, returns
`AMS_MEL_OK`, and clears the handle.

## Final release protocol

### Independent executor ownership

`release_executor_owned` is the stream's provider-execution permit. It is
independent of whether a current wrapper pointer is nonempty. It stays true
across provider release, result publication, unlocked final wrapper destruction,
every deferred handoff, and failure reconciliation.

An explicit closer that finds the permit owned waits on `release_ready`. A
callback cannot wait for an enclosing provider call, so it transfers its exact
wrapper into a preallocated deferred owner. The current executor drains those
owners iteratively. Waiters are notified only when the executor is genuinely
relinquished.

Therefore at most one thread per stream can invoke provider release or wrapper
destruction code controlled by this scheduler at a time. The provider mock
records the maximum active count directly.

### Stable per-operation ownership

Each admitted operation has one local exact-wrapper owner used for the provider
call and final unlocked destruction, plus one reserved uncertain-slot identity
containing that same exact wrapper before provider code is entered. The pair
remains stable for the entire reconciliation.

Success clears and recycles the reserved uncertain slot under the mutex,
unlocks, then destroys the local owner while retaining the executor permit.
Failure leaves the exact wrapper in its reserved slot permanently, publishes
`uncertain_release`, never retries that wrapper, and retains the complete
provider graph.

Deferred promotion checks `free_uncertain_slots` before `back()`/`pop_back()`.
If a failed operation consumed the final free uncertain slot, a pending wrapper
remains in its deferred owner unless it can be moved to the pre-release
fail-safe reserve. It is never silently dropped and no sentinel slot is indexed.

### Unlocked provider-defined destruction

The success transition is:

```text
mutex held:     publish success, recycle slot, decrement obligation
mutex unlocked: destroy the local exact-wrapper owner
mutex held:     promote deferred work or relinquish executor and notify
```

The executor permit remains owned during unlocked destruction. A callback
reentered by a provider destructor therefore takes callback-side deferral and
cannot recursively execute provider release code.

The deterministic `destructor-reentry` mock makes deferred wrapper B's release
succeed, then makes B's final destructor synchronously invoke callback-side work
that needs the bridge mutex. The callback returns, B is destroyed exactly once,
provider release nesting remains one, and the pool regains both buffers. Failed
wrappers are covered separately and remain never destroyed.

### Release obligations and teardown

Physical teardown requires:

```text
requests == 0
retained_frames == 0
release_obligations == 0
uncertain_release == false
```

Stop accounts for `release_obligations` and is the safe external joiner for
callback-only healthy release work: it waits on `release_ready` and stops
waiting if uncertainty is published. Close uses that same Stop path before its
owner-release decision. Neither asks a provider callback to destroy or join its
own channel.

After the obligation finishes, Close re-runs the ordinary cleanup decision,
destroys the channel outside the mutex, drains callbacks, destroys Control, and
unloads the library in order. `AMS_MEL_OK` is returned only with the public
stream owner cleared. Existing lease and Navigation-request deferral semantics
are unchanged; provider failures remain explicit.

## Deterministic evidence

Synchronization evidence is state/barrier driven. Sleeps are used only in
bounded watchdog polling for marker files, never as proof of bridge entry.

| Regression | Evidence |
|---|---|
| Distinct explicit owners | A bridge-side nonblocking marker records the second closer at executor backpressure. |
| Failed A / deferred B / waiting C | Malformed B takes the real rejection/deferred path; C is a distinct lease closer; a post-failure barrier forces the reviewed interval; returned failure and thrown exception both keep maximum provider activity at one. |
| Executor mutation | Surrendering only the executor permit in that interval produces maximum provider activity of two. |
| Exact failed ownership | A's failed exact wrapper remains alive, is attempted once, and is never destructor-retried. |
| Deferred destruction reentry | B succeeds after deferred promotion, is destroyed once unlocked, and its mutex-taking callback returns without recursive release execution. |
| Callback-only Close | No frame is accepted and no lease or Navigation request exists; Close is observed waiting on the sole healthy release obligation, then returns OK with a null handle. |
| Close-gate mutation | Omitting only the obligation gates reproduces OK with a non-null stream handle. |
| Healthy teardown order | The log proves release before channel, Control, manager, and library destruction. |
| Ada-facing behavior | Safe Ada `Close` waits for the callback-only obligation and `Is_Open` is false afterward. |

The existing repeated-generation, finite queue-full reentry, provider nesting,
queue-discard blocking, Ada A/B/C frame-4 identity, failure/throw retention,
allocation-failure, enqueue exception, callback-drain, Navigation/Close,
zero-copy identity, and large-buffer-count regressions remain active.

## Compatibility and limitations

No public header, exported symbol, public Ada declaration, Rust API, or Python
API changed. Production and test build trees remain separate. Test barriers and
mutations compile only with `AMS_MEL_ENABLE_TEST_FAILPOINTS`; mock controls are
not production exports. Vendored source is unchanged.

This is mock-provider evidence. The mock models the relevant pinned Squall
release/requeue behavior, but no real-Squall runtime claim is made here.