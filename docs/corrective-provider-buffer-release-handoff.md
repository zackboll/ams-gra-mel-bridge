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
mutex held:     reconcile success, recycle uncertain slot
mutex unlocked: destroy the local exact-wrapper owner
mutex held:     decrement obligation after destruction/reentry, notify,
                promote deferred work or relinquish executor
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
release_executor_owned == false
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

## PR #43 final deferred-cleanup correction

This follow-up started at `3615a820e52b70727023086c48a06338aad72c48` with
intentional uncommitted work. The starting diff for the final regression pass
was preserved in `/tmp/ams43-observer-start.diff`. Intermediate checkpoint
notes have been consolidated here; their diffs remain in those local backups.

### Callback and external ownership

`Listener::onImage` moves its Buffer into by-value `CallbackState::image`, then
into its Releaser. Acceptance inserts metadata before transferring the wrapper
into the queue under the mutex, preserving allocation-failure safety. Rejection
moves that same wrapper into the release scheduler. There is no hidden listener
Buffer owner after dispatch. Healthy release drops its fallback while another
operation owner exists, destroys the final wrapper unlocked, then publishes
completion and drains destructor-triggered deferred work before surrendering
the executor. Failed wrappers remain in pre-reserved uncertain ownership forever.

Close performs Stop, discards unreachable queued frames while accumulating
failure, calls `finish_deferred_cleanup_from_external_owner`, then applies
handle/status policy. It has no second callback/release join or cleanup retry
algorithm. The helper takes a strong state by value and never runs from provider
callbacks, callback Releasers, or provider-wrapper destructors.

| State | External cleanup responsibility |
|---|---|
| Public owner open | Stop/Close; finishing children may assist after logical Stop |
| Public closed, snapshots remain | Final snapshot completion |
| Public closed, requests remain | Final Navigation adapter worker |
| Both child kinds remain | Completion removing the last requests/retained_frames obligation |
| Finite callback work only | Existing external joiner stays alive; callback never tears down its channel |
| Uncertain ownership | Permanent retention; no failed release retry |

Close's child handoff publishes `public_owner_closed` under the same mutex as
the shared `requests != 0 || retained_frames != 0` predicate. Snapshot and
Navigation completion decrement their own counts before calling the same helper
with strong owners. Thus either order in the mixed-child case mechanically
selects the actual last completion, without separate last-child algorithms.
Navigation completion runs on the adapter-created `std::thread` through
`run_worker -> complete -> finish_stream`, not the provider Image callback.

### Two snapshot join schedules and Navigation

The normal C suite runs five parameterized late-work cases:

- Public Close after Stop/discard: resumed Close must emit the helper wait marker.
- Snapshot A: S has completed its own release, then late B appears; S emits the
  external helper wait marker before B is released.
- Navigation: hold a genuine provider future, Close the public stream, pause
  late rejected B, then complete the future. The common helper wait marker and
  request `Wait(0) == TIMEOUT` prove the adapter worker joins B.
- Snapshot B: B is already paused in healthy release when S starts closing on
  an application thread. S emits the release-executor waiter marker, has not
  reached its completion barrier, and no buffer has yet been returned. After B
  finishes, S releases its own Buffer, reaches completion, and performs cleanup.
- Lost-cleanup mutation with safe observation and rescue, described below.

The two snapshot schedules intentionally observe DIFFERENT internal wait
points. Both prove that the last external child cannot relinquish the full graph
while finite callback provider work remains. Cases require no channel, Control,
manager or library destruction while work is held, maximum concurrent provider
release count one, exactly two wrapper destructions, exactly one channel
destruction, and ordered wrapper/channel/Control/manager/library teardown.
Healthy tests and the rescued mutation run before permanent-uncertainty tests.

### Direct negative controls

The destructor mutation changes only lifetime eligibility, retaining executor
ownership and normal provider release/result behavior. It must observe
`before-detach.reached` after cleanup claims ownership while B's final destructor
is held. Teardown then stays paused until destruction/reentry safely finishes.
A timeout or generic inner failure is never accepted as mutation evidence.

The lost-cleanup mutation skips ONLY final snapshot helper handoff after its
successful Buffer release. It places the state in a test-only strong observation
pin without changing counters, lifecycle, uncertainty, cleanup flags, or emergency
retention. B completes healthily; S returns; both test threads are joined. Direct
inspection under the lifecycle mutex requires:

- observer strong count exactly one; public owner closed;
- requests, retained frames, release obligations and callbacks all zero;
- executor unowned, uncertainty false, lifecycle not Failed;
- no emergency retention, cleanup not owned/complete/failed;
- channel still present and no provider teardown events.

This is a stranded healthy graph with no production obligation left, not a
manufactured permanent-retention result. The application test thread disarms
the mutation and rescues via the NORMAL external helper, requires success,
clears the observer, and verifies exactly-once ordered cleanup/unload. Inspection
and rescue exports exist only in the test facade, using a generated test export
map. The production export map, ABI and installed headers remain unchanged.

### Callback entry and helper terminal-state audit

No pre-detach callback-zero requirement is imposed. Release admission and
cleanup claim share the lifecycle mutex, but a callback not yet admitted may
race the claim. The cleanup owner's strong state retains CallbackState, listener,
registered Buffers and host bytes, Session and provider code through channel
destruction, the provider's no-new-callback boundary. It then drains callback
entries and rechecks retained/release/executor/uncertain ownership before freeing
registered storage. Late uncertainty instead retains the graph and reports
failure. Existing post-drain regression and mutation remain enabled.

The helper terminates by state: another child, no logical close, completed
cleanup, detached state, uncertainty or cleanup failure. Contended cleanup waits
and adopts publication. Lost cleanup admission (`NotRequired`) re-evaluates
while the strong external owner remains; there is no fixed retry count. A
synchronization exception poisons and permanently roots the graph instead of
escaping `noexcept`. Detach failure preserves public retry ownership or retains
the graph if public ownership is already gone. Callback code never becomes the
physical-cleanup executor.

### Discard failures and compatibility

Queued discard aggregates all release outcomes. Fail/throw cannot be overwritten
by Stop's earlier OK or later cleanup success: Close reports PROVIDER_FAILED.
Existing tests require one attempt, exact failed-wrapper survival, intact host
bytes, unchanged registered-buffer destruction, and no retry on repeated Close.
The scheduler publishes sticky uncertainty/lifecycle failure and permanently
roots the provider graph; failed underlying buffers are not returned to the pool.
Healthy discard returns OK/null. Safe Ada tests require Provider_Error for both
failure forms and retain the healthy Close, callback-only Close and lease tests.

ABI remains 0.1 with 90 production exports. Public C/Ada/Rust/Python APIs, vendor
bytes and provenance remain unchanged. Zero-copy alias and owned-copy contracts
are unchanged. Production must contain neither test environment strings nor
observer/rescue symbols. No real-Squall execution is claimed by this correction.

### Local validation

Native CTest passes 15/15, including both snapshot schedules, Navigation, direct
stranded-state inspection/rescue and destructor premature-claim mutation.
Build isolation passes. GNATformat and its check pass; Alire build and all Ada
contract suites pass, including Fail/throw `Provider_Error`. Rust passes; Python
passes 52 tests. GCC 14.2 Debug and Release each pass 15/15. Direct `make check`
passes its earlier gates but fails because bare GNAT/GPRbuild is unavailable on
PATH; the Alire results are separate. Clang is unavailable locally.

Production `nm` reports exactly 90 `ams_mel_` exports, with no observer/rescue
exports; production strings contain no `AMS_MEL_TEST_` or observer/rescue names.
The test facade exposes its two observation/rescue functions separately. Git
confirms export map, public APIs, vendor and provenance unchanged; whitespace
checks pass. Full commands/results are recorded in `/tmp/ams43-final-gates.log`
and `.results`; final native/GCC logs are `/tmp/ams43-last-{native,debug,release}.log`.
