# Task 031B — bounded completion admission

## Design and scope

Starting main: `54cbc283375aee2540503b619b8cf9bb2a56d977`.
The parked branch fast-forwarded to that commit without rewriting history.

The experimental ABI 0.1 adds one session-open function and an independent
`ams_mel_session_options_v1`. Zero (including legacy open) means unlimited;
nonzero is a per-Session limit shared across the seven asynchronous RequestFor
operations. A full limit refuses before lifecycle accounting and provider send
with bridge status 13 and `async request limit reached`. No work is queued or
retried by the bridge. Each admitted future still uses its own completion
worker: admission is not multiplexing.

The provider-independent admission counter uses a CAS and a move-only permit.
The six worker inputs declare their TEST FinalOwner first, then permit, then
Completion, then future and bookkeeping. Reverse member destruction order is
bookkeeping, future, Completion, permit, TEST FinalOwner. Public request,
channel, or Session close does not release a slot. Permanent emergency retention
after provider send permanently occupies that slot by design.

## Native source and lifetime evidence

The inspected integration functions are:

| Engine | Submission function | Claim after admission |
|---|---|---|
| C2 Mode | `submit_mode_command` | `claim_c2_submission` |
| C2 Return | `submit_return_operation` | `claim_c2_submission` |
| C2 Comms | `submit_comms_operation` | `claim_c2_submission` |
| Navigation | `ams_mel_ir_stream_submit_navigation_report` | `claim_navigation_submission` |
| Instrumentation | `ams_mel_ir_instrumentation_submit_level` | mutex-protected `requests` increment |
| Track update | `ams_mel_ir_track_submit_update` | mutex-protected `requests` increment |
| Track response | `ams_mel_ir_track_submit_system_track_data_response` | mutex-protected shared `requests` increment |

Each path preallocates its owner, Completion, WorkerInput and thread storage;
borrowed payload conversion precedes admission. Each submission acquires once;
the two Track paths independently use the same Session counter. Refusal returns
before the claim and send, without publishing an output owner. C2 claim returns
with its lifecycle mutex unlocked before the send, preserving PR #45. Synchronous
throw handlers unwind the request claim; unarmed input destruction releases the
permit. After send, the armed self-cycle and intrusive retention preserve the
whole input, including its permit, on post-send failure. The native failure
suite below now supplies bounded failure-injection evidence for these paths.

All six inputs have the documented member order. “Completion destroyed” means
the input's shared Completion **owner** is destroyed; a retained public request
can keep the graph-free cached Completion object alive. The FinalOwner comment
now explicitly records that its observation follows permit destruction.
Null-options rejection leaves the output unchanged, consistent with legacy
open's requirement that the caller supply a null output owner; it must not clear
a pre-existing live owner on invalid input.

## Deterministic tests added during continuation

The options-zero, bounded-one and bounded-eight C2 scenarios now run in separate
CTest processes. Peak assertions use the absolute high-water value from a fresh
process, never subtraction of cumulative peaks. Wave transitions wait for every
created WorkerInput's FinalOwner, including preallocated refused inputs, rather
than merely waiting for worker-return counters.

`completion_cross_family_admission` uses one limit-1 Session with C2, Image,
Instrumentation and Track owners. It holds C2 Return, closes its request owner,
and rejects every request type with null output and unchanged total mock send
count after each refusal. It repeats with Navigation as the occupying owner.
For both owners it parks at the existing post-get/pre-cleanup boundary and proves
cross-family refusal persists. FinalOwner reclamation permits subsequent
submission, ending with a successful Track retry. Four typed test-only hooks
now inspect the C2, Image Navigation, Instrumentation and shared Track request
counts under their respective lifecycle mutexes. Every one of the seven refusals
in both occupancy passes leaves all four counts unchanged, as well as every
output null and the aggregate provider send count unchanged.

## Direct admission and lifetime observations

The test facade alone exports an admission observation token. Its sole member
is `shared_ptr<CompletionAdmission>`; it cannot retain SessionState, Control,
manager, library, channels, Completion or WorkerInput. Acquisition requires a
live bounded Session. Read and release need no Session owner. Unlimited Sessions
have no admission object and are deliberately not observable through this hook.
The token and four request-count hooks are absent from public headers, installed
API and the production export map.

`completion_parent_admission` holds one KeepAlive at limit one, closes the
request, all children and Session, and directly observes active=1 without C2,
Control, manager or library teardown. It also observes active=1 at the post-get
boundary. After FinalOwner reclamation active=0. Ordered channel/Control/manager/
library destruction occurs **while the token remains alive**, proving that the
observer does not mask provider teardown. The test drops its own DSO handle
before submission and gates cleanup until the provider release function returns.

`completion_isolated_admission` opens two independent limit-one Sessions, uses
C2 Return in A and Navigation in B, and directly checks both counters through
A-only occupancy, both occupied, independent refusals, A-only release, successful
A retry while B remains saturated, and final reclamation of both. Family-specific
mock release selects A without completing B; global send counts are not used as
a substitute for observing each admission object.

`completion_concurrent_admission` starts eight pthread submitters at a barrier,
each using a distinct Attached C2 wrapper on one limit-four Session. Exactly four
succeed and four refuse with null outputs; sends, active permits and workers are
all four. No future is released during the race, so the occupied count is
monotonic and the final count is also its maximum. The test does not assume which
callers win and uses no sleep or polling as synchronization evidence.

Latest incremental validation: `make test-native` passed **35/35**; the four
direct-observation scenarios each passed 30 repetitions (**120 executions**).
Logs: `/tmp/ams031b-observation-native.log` and
`/tmp/ams031b-observation-repeat.log`. A rebuilt production library still has
91 `ams_mel_` exports and no test hooks, recorded in
`/tmp/ams031b-observation-production.log` and
`/tmp/ams031b-observation-exports.txt`. These are incremental checks, not the
final requested multi-toolchain validation matrix.

`completion_seven_type_bounded` explicitly retries 100 logical operations at
limit 8: 15 Return, 15 Mode, and 14 each Comms, Navigation, Instrumentation,
Track update and Track response. Observed: **100 sends, 100 gets, 12 refusals,
peak active workers 8, final active workers 0**. Every wave waits for FinalOwner
reclamation. Held futures ensure the synchronized pre-release active snapshot
contains every accepted worker; waves cannot overlap after reclamation. No
particular refusal count is a contract. This test closes request handles early;
it does not substitute for retained-handle bounded result/exception tests.

## Failure-path evidence

The C11 cross-family fixture adds 21 isolated CTest cases:

- `admission_send_throw_{0,3,4,5}`: C2 Return, Navigation, Instrumentation and
  Track update. The PR #45 one-shot send barrier is reused (extended to non-C2
  families); a separate entry count distinguishes a throwing send invocation
  from a successfully returned held future. With limit one, at the barrier both
  accounting and admission equal one. Throw returns ProviderException/null,
  FinalOwner is reclaimed, both counts return to zero, and worker/get counts
  stay zero. Two subsequent held sends on the same Session complete and reclaim.
  FinalOwner reclamation also rules out emergency WorkerInput retention.
- `admission_exception_{0,3}`: Return and Navigation stored exceptions. One held
  future, one worker, one get and one get-threw event. At the post-get boundary
  admission/accounting remain one. Wait and zero-timeout cached Wait return
  ProviderException with the public handle retained through final reclamation.
  Both counts become zero; a new request on the same Session is accepted. The
  mock exception switch is sticky, so the retry also completes exceptionally;
  each test is a separate process.
- `admission_retention_{0..6}_{allocation,worker-launch}`: both failure classes
  for Return, Mode, Comms, Navigation, Instrumentation, Track update and Track
  response. Each has one returned provider future, no public request, no worker
  or get, and one retained accounting claim and permit. After clearing only the
  failpoint, a second submission returns ResourceExhausted/null without another
  provider future/send. The future is never released, and no cleanup/rescue is
  attempted. Process exit is the cleanup boundary.
- `admission_provider_rejection`: held Mode future resolves to
  `mel::ErrorCode::InsufficientResources` with `provider resource rejection`.
  Available admission yields OK and a handle; Wait and cached Wait yield
  CommandRejected, `AMS_MEL_ERROR_INSUFFICIENT_RESOURCES` and that diagnostic,
  **not** bridge ResourceExhausted. While the first future is held, a second
  request instead gets bridge ResourceExhausted/null with no additional send.
  This is the normative native evidence for the next binding continuation.

All four typed accounting observers are checked in every failure scenario:
only the affected domain may be one, all other domains remain zero. Normal
throw/completion paths end at zero; uncertain post-send paths remain one.
Instrumentation and Track synchronous catches call `finish_channel` before
returning and destroying the unarmed input. Navigation calls
`release_navigation_submission`; C2 calls `finish_channel`. All six inputs retain
the same future → Completion shared owner → permit → TEST FinalOwner destruction
order. Both Track submissions feed the same `WorkerInput`, `arm_worker`,
`retain_worker` and `run_worker`; both entry points are nevertheless tested.

The admission token still owns only `shared_ptr<CompletionAdmission>`, with no
Session/provider graph. The existing parent-admission regression proves ordered
provider unload while that token remains alive; no duplicate test was added.

### Failure matrix (bounded Session)

“Counted” describes the outstanding channel claim, not cumulative submissions.
Healthy completion frees accounting before the final WorkerInput releases its
permit; a cached public handle may outlive both.

| Path | Provider send? | Channel request counted? | Permit held? | Request published? | Result | Permit outcome |
|---|---|---|---|---|---|---|
| Admission full | No | No new claim | No new permit | No | ResourceExhausted (13) | Existing permits unchanged |
| Channel lifecycle claim rejected | No | No | Briefly, before claim | No | ProviderFailed | Released on unarmed input destruction (source audit) |
| Synchronous provider send throw | Invoked once; no future returned | One, then unwind to zero | One, then zero | No | ProviderException | Released on unarmed input destruction |
| Provider future success | Yes | One until finish | Through post-get/cleanup | Yes | OK | Released on final WorkerInput destruction |
| Provider future rejection | Yes | One until finish | Through post-get/cleanup | Yes | CommandRejected; MEL error preserved | Released on final WorkerInput destruction |
| Stored future exception | Yes | One until finish | Through post-get/cleanup | Yes | ProviderException, cached | Released on final WorkerInput destruction |
| Post-send allocation failure | Yes; future exists | One permanently | One permanently | No | InternalError; next submit ResourceExhausted | **permit retained for process lifetime** |
| Post-send worker-launch failure | Yes; future exists | One permanently | One permanently | No | InternalError; next submit ResourceExhausted | **permit retained for process lifetime** |
| Public request close while pending | Already sent | One until finish | Yes | Published then closed | Close OK; not cancellation | Released only on final WorkerInput destruction |
| Parent close while pending | Already sent | One until finish | Yes | Independent child lifetime | Close OK; deferred teardown | Released only on final WorkerInput destruction |

Permanent retention is intentional fail-safe retention of future, graph,
channel claim and permit, not an accidental leak or a recoverable admission
queue. No rescue mechanism was added.

## Safe bindings

Ada `Open` is unchanged; `Open_With_Options` accepts `Session_Options` with
`Max_Async_Requests`, default zero. Private submission checks raise
`Resource_Exhausted` with the native diagnostic. The C2 contract tests exercise
legacy open, explicit zero with two pending requests, limit-one cross-type
refusal, public request Close retaining capacity, final-owner recovery, and
provider `Insufficient_Resources` as a rejected Mode result.

Rust `Session::open` is unchanged; `Session::open_with_options` accepts
`SessionOptions`, whose default is unlimited. The separate `admission` test
executable exercises the same cases with `ErrorKind::ResourceExhausted` and
`ModeResult::Rejected { code: MelErrorCode::InsufficientResources, .. }`.

Python preserves its existing surface and uses `max_async_requests=0` by
default. Its subprocess-isolated admission test covers the same lifetime and
provider distinction, plus invalid limits. No safe API exposes test controls.
All three tests await FinalOwner reclamation, never sleeps or Wait alone, before
retrying. Ada/Rust use test-local FFI only for mock gates and observer access.

## Bounded benchmark

`request_completion_scalability bounded` uses one Session, limit eight, and
15 Return, 15 Mode, and 14 each CommsTest, Navigation, Instrumentation,
TrackDataUpdate, and SystemTrackDataResponse. The client retries a refused
logical request only after completing its held wave and awaiting all created
FinalOwners (including refused preallocated inputs). The bridge never retries.
Existing Task 031A unlimited modes remain available.

Each invocation verifies 100 logical requests, 100 provider sends, 100 gets,
zero final active workers, and at most eight active workers. Held-wave OS
samples are sampled maxima, not continuously measured process peaks. Duration
covers Session/channel setup through final WorkerInput reclamation, before
public owner teardown. OS memory, latency, and refusal count are non-gating.

Full GCC Debug and Release CTest each passed 56/56 before these samples.
All samples had 12 refusals, eight peak active workers, one baseline thread,
and nine maximum held-wave threads. No assertion requires exactly 12 refusals.

### GCC Debug raw samples

```text
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=5372 rss_peak_held_kb=6096 vm_base_kb=9084 vm_peak_held_kb=599052 duration_ms=4.5256
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=5168 rss_peak_held_kb=6044 vm_base_kb=9084 vm_peak_held_kb=599052 duration_ms=3.7501
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=5248 rss_peak_held_kb=6060 vm_base_kb=9084 vm_peak_held_kb=599052 duration_ms=3.57245
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=5436 rss_peak_held_kb=6096 vm_base_kb=9084 vm_peak_held_kb=599052 duration_ms=3.41771
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=5436 rss_peak_held_kb=6160 vm_base_kb=9084 vm_peak_held_kb=599052 duration_ms=3.33589
```

| Metric | Minimum | Median | Maximum |
|---|---:|---:|---:|
| resource_exhausted | 12 | 12 | 12 |
| peak_active_workers | 8 | 8 | 8 |
| threads_peak_held | 9 | 9 | 9 |
| rss_peak_held_kb | 6044 | 6096 | 6160 |
| vm_peak_held_kb | 599052 | 599052 | 599052 |
| duration_ms | 3.33589 | 3.57245 | 4.5256 |

### GCC Release raw samples

```text
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=4024 rss_peak_held_kb=4880 vm_base_kb=7144 vm_peak_held_kb=597112 duration_ms=3.04667
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=4024 rss_peak_held_kb=4896 vm_base_kb=7144 vm_peak_held_kb=597112 duration_ms=2.59736
bounded logical_requests=100 provider_sends=100 gets=100 final_active_workers=0 peak_active_workers=8 resource_exhausted=12 threads_base=1 threads_peak_held=9 rss_base_kb=4024 rss_peak_held_kb=4832 vm_base_kb=7144 vm_peak_held_kb=597112 duration_ms=2.73834
```

| Metric | Minimum | Median | Maximum |
|---|---:|---:|---:|
| resource_exhausted | 12 | 12 | 12 |
| peak_active_workers | 8 | 8 | 8 |
| threads_peak_held | 9 | 9 | 9 |
| rss_peak_held_kb | 4832 | 4880 | 4896 |
| vm_peak_held_kb | 597112 | 597112 | 597112 |
| duration_ms | 2.59736 | 2.73834 | 3.04667 |

Task 031A's unlimited mock case can produce approximately 100 completion
workers for 100 pending logical requests. Task 031B limits admission to eight
and exposes explicit caller-managed backpressure. This is not multiplexing,
thread pooling, or evidence of improved throughput or real Squall performance.
A fixed pool of blocked future-get workers would introduce starvation concerns;
admission instead refuses before send without queueing accepted provider work.

## Validation and audit evidence

Local results from the final implementation:

- Native GCC Debug: **56/56**; build-tree isolation **PASS**.
- Separate GCC Release tree `/tmp/ams031b-failure-release`: **56/56**.
- Ada format/check-format **PASS**; foreground `alr -C ada build` **PASS**;
  foreground `alr -C ada/tests run` **PASS**, including the new admission test.
  Earlier 30-second tool timeouts are not counted as passes.
- Rust `make test-rust`, workspace check/test, all-target Clippy with warnings
  denied, and formatting **PASS**, rerun after adding explicit options size,
  alignment, status and function-signature assertions. Raw ABI tests: **2 passed**.
- Python `make test-python`: **54 passed**, warnings treated as errors;
  compileall **PASS**.
- Newline, unstaged and staged whitespace checks **PASS**.
- Production rebuild and symbol inventory: ABI **0.1**, **91** exports,
  original 90 preserved, sole addition `ams_mel_session_open_with_options`.
  No test/mock/probe export. Vendor/provenance/manifest diff is empty.
- `make check`: **FAIL — GNAT/GPRbuild is required on PATH** (exit 2).
  After foreground tool timeouts, the complete command was observed through a
  log and exit-status file; it finished and left no validation process running.
  Its isolation/native/format gates passed before direct Ada failed.
  Alire-provided GPR is available; the final Ada build and tests above completed
  in the foreground, not as background substitutes.

Cross-language declarations agree on status 13, a uint32 limit and all eight
open-with-options parameters. C compiler probes match Rust/Python layouts.
GNAT `-gnatR3` reports the private Ada options record as Size 32, Alignment 4,
with `MAX_ASYNC_REQUESTS at 0 range 0 .. 31`. All old C exports and Session call
signatures are preserved. The Navigation Ada wrapper now explicitly sequences
submission before reading diagnostics, as do the other async wrappers.

The final source audit confirms admission before request accounting/send in
all seven paths and future/Completion ownership destruction before permit
release in all six WorkerInputs. C2 sends remain outside the lifecycle lock.
There is no admission polling, hidden queue or automatic bridge retry.

The production ABI audit initially used a faulty string filter that treated
`comms_test` as a test hook; the actual inventory is 91 with only the intended
addition. No production-library defect was demonstrated.

## Limitations and hosted acceptance

Exact-head hosted CI is reported on the PR rather than predicted here. Hosted
direct-GPR remains required to close the bare-PATH limitation. All admission
and benchmark evidence is mock-provider-only; no
vendor changes, real-Squall measurements, or cancellation guarantees are added.
Permanent future retention permanently consumes capacity by design.
