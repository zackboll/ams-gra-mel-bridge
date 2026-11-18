# IRMEL-ADA-023 — safe Scheduling IRSTStareTask execution

## Source and scope

Isolated worktree `/home/zboll/git/ams-mel-irmel-ada-023`, branch
`feature/irmel-ada-023-scheduling-stare-task`, created from freshly fetched main
`e41493dcef34bb62f45fecb53eceb989ce895500`. PR #236 was confirmed merged and
the open-PR preflight found no overlapping StareTask implementation.

The source contract is the published
`SchedulingChannel::exec(IRSTStareTask)` overload at IR MEL revision
`8d9224519f12b44e0b28815755c56a32a28d24a0`. Inspected SchedulingChannel,
IRSTStareTask, StareTaskParam, TaskInfo, CommonIR_MEL and AMS Math RangeAzEl.
The inventory and production implementation initially had no StareTask consumer
path; the concrete mock overload returned Unsupported. No vendor bytes change.

Exactly that overload is added. ScanTask, TrackTask, prioritized track lists,
the four remaining Scheduling metadata families, policies, queues, physical
sensor activity and image production remain excluded. Scheduling remains partial.

## Data conversion and ABI

One new production export under the existing `AMS_MEL_0.1` version:
`ams_mel_ir_scheduling_exec_stare` (332 → 333 exports). Existing Return request
Wait/Close exports are reused; no new asynchronous request engine is introduced.

The C11 `ams_mel_ir_stare_task_v1` contains:

| Field | Representation | LP64 offset |
|---|---|---:|
| center_azimuth_rad | double radians | 0 |
| center_elevation_rad | double radians | 8 |
| duration_ns | signed int64 nanoseconds | 16 |
| elevation_frame | uint32, Inertial=0 / Aircraft=1 | 24 |
| azimuth_frame | independent uint32 frame | 28 |
| command_id | complete uint32 | 32 |
| task_info | unchanged five-field TaskInfo v1 | 40 |

Size 64, alignment 8. A strict compiled C11 probe asserts and prints the actual
layout; private ctypes and raw Rust sys tests compare directly with that probe.
These mirrors do not expose safe Python or Rust StareTask execution.

The private converter pins both frame values, IEEE binary64 angle types and
CommandID type, and reuses the unchanged TaskInfo converter and its existing
TaskListAction/nanosecond assertions. It uses `setCenter`,
`setCentFrameRefEl`, `setCentFrameRefAz` and `setDurationNanoseconds`, then the
published `IRSTStareTask(id, params, info)` constructor. Before provider entry it
checks `getCommandID`, `getParams`, `getTaskInfo` and every nested published
getter. Angle comparison preserves signed-zero bits. No degrees, Ada Duration,
epoch, range, identifier correlation, priority or ordering constraints are added.

## Ada API and ownership

`AMS.MEL.IR.Scheduling.Commands` adds owned `Stare_Parameters`, `Stare_Task`,
`Coordinate_Frame_Reference` and `Exec_Stare`. `Task_Info` is exactly the existing
IRMEL-ADA-022 type. Public angles are binary64 `Long_Float` (compile-time checked),
duration is `Interfaces.Integer_64` and CommandID is `Interfaces.Unsigned_32`.

`Stare_Request` and `Stare_Result` are distinct private types containing
the existing limited-controlled Return observer and owned result. Thin
public forwarding operations reuse Wait, zero-timeout polling, cached terminal
results, actual Return values, complete MEL rejection descriptions and Close.
Non-raising finalization also uses the existing implementation rather than a copied
AbortTask future engine. Callers serialize observer Wait/Close and channel handle
access against Close until admission.

Submission uses Enabled-only Scheduling admission, its own pending-request
counter, the Session completion permit, graph retention and deferred physical
cleanup. The Scheduling mutex is released before the actual provider exec call;
reentry into Enabled Scheduling is tested during that call. Closing a request,
channel or Session does not cancel an admitted future.

Submission acceptance is distinct from a completed Return::Success; completed
Fail/NotSupported/NotImplemented are valid Return values, distinct from an
explicit ErrorOr Unsupported/InvalidState rejection. None proves physical
staring, acquisition, ExecuteTaskAck or TaskExecutingRep delivery.

Focused malformed-input controls exposed two defects in the shared Return
submission/completion implementation. It rejects an invalid future before
starting completion processing (the R1 prepared worker takes its abort path), and rejects a
non-null successful shared_ptr with no ownership control block before
dereferencing it. These are shared-engine correctness fixes, not a second engine.

## Metadata independence and conditional support

No callback is registered automatically. ExecuteTaskAck and TaskExecutingRep
streams coexist independently with pending StareTask work. Native tests deliver
both real metadata events with unrelated identifiers and verify the future still
polls pending. Submission/completion fabricate neither event. No identifier
correlation is inferred. Existing metadata contracts are unchanged.

Providers must advertise Scheduling before attachment. The Ada tests separately
qualify a provider lacking Scheduling: attachment fails non-fatally and the
Session remains usable. Presence of the binding is not provider support.

The safe-only executable example is
`ada/tests/src/ams_mel_ir_scheduling_stare_example.adb`. It opens a mock Session,
attaches/enables Scheduling, supplies complete parameters and TaskInfo, submits
one actual StareTask, inspects Return and explicitly closes all public owners.
Provider lifetime controls are separate test fixtures, not part of the safe API.

## Qualification record

Actual preliminary results (not final-source or hosted qualification claims):

- `make test-native`: 863/863 passed after the invalid-future and malformed
  ownership corrections. The earlier run caught those two defects.
- `ctest --test-dir native/build-tests -R '^ir_scheduling_stare_' --output-on-failure`:
  22/22 passed, including C11 layout, conversion, real overload dispatch,
  all Return/error mappings, provider/future exceptions, invalid future, null and
  unowned payloads, timeout, cached Wait, reentry, concurrent admitted Close,
  pending owner/Session closure and post-send conservative retention.
- `make format-ada`, `make check-ada-format`, `make check-ada-docs-changed`:
  passed; six new public declarations inspected, zero invalid/missing annotations.
- Strict Python private ABI suite: 63 tests passed after adding the exact new
  export/signature to the regression inventory.
- `alr -n -C ada build`: passed. Initial test-project compilation caught a missing
  signed-integer operator visibility clause in the example; corrected before retry.

Full sequential, matrix, stress, policy, Alire, coverage and hosted results must
be recorded below when complete. No skipped or running gate is a pass.

The first broader local campaign encountered host `/tmp` inode exhaustion
(1,048,576/1,048,576 used). This caused real compilation, fixture-creation and
stress failures and is not counted as passing qualification. Ordinary `TMPDIR`
alone is insufficient because native qualification deliberately uses literal
`/tmp` paths. Retried campaigns use a private `bwrap` mount namespace binding
`/home/zboll/irmel023-qualification/tmp` onto `/tmp`; this changes no production
source or test assertions and touches no unrelated worktree or temporary files.
Policy fixtures require scratch outside the source tree as well. The initial
scratch-inside-build retry correctly failed protected-source overlap controls.

Additional actual results:

- Safe Ada focused StareTask executable: passed with GNAT 16.1.0 and GPRbuild
  26.0.0, including both rejections, malformed/future/exec failures and pending
  owner/Session closure. One overlapping test-tree rebuild invalidated an
  intermediate observer run; its isolated rerun passed with a lifetime log
  ending in `library_unloaded`.
- Safe-only Ada example: passed; reports mock Return::Success without physical
  execution claims. It is also run by `scripts/test_ada.sh`.
- `make check-ir-api-coverage`: 75/75 regression controls passed. Exactly
  `exec(IRSTStareTask)` changes operation classification missing → safe_ada;
  IRSTStareTask and StareTaskParam change missing → safe_ada with every getter
  represented. Safe-Ada unique operations 58 → 59; Scheduling remains partial.
  Reviewed ceiling and digest updated; Markdown report regenerated, not edited.
- Rust raw Scheduling ABI suite: 3/3 passed, including the actual C11 layout.
- Rust workspace check, Clippy `--workspace --all-targets -- -D warnings` and
  formatting checks: passed.
- `make test-policy`: passed using scratch outside the source tree; all policy
  sub-suites ran, not merely the source-integrity smoke check.
- Source integrity passed: 825 vendor files, six workflows; vendor content,
  historical coverage archive and documentation workflow policy unchanged.
- Final-newline and Git whitespace checks passed.

The first hosted documentation run
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44240826896>
rejected the initial public subtype aliases to private Abort types with
`independent type audit: private ordinary subtype target`. The safe types now
have distinct private declarations and forwarding operations, preserving shared
completion/finalization implementation without modifying the documentation audit.
The subsequent hosted run rejected the private null-record extension in its
coverage parser; using ordinary private owner-containing records removes that
unsupported source shape without changing either audit or completion semantics.
The changed-public-API checker inspects 12 declarations with zero missing or
invalid annotations. That failed hosted run is not a passing gate.

Additional full local results on the initial implementation source: Python
694/694 passed; Rust 640 passed, zero failed and 15 existing ignored tests;
Ada smoke/negative/example passed with 55 PASS markers and the explicit mock
example output. GCC Debug full suite 863/863 and FULL_STRESS
`ctest --parallel 4 --repeat until-fail:50` passed (43,150 test executions).
The four focused StareTask lifetime/reentry controls repeated 100 times each
also passed. These results do not substitute for final-HEAD hosted qualification.

## Remaining limitations and unresolved acceptance gate

Qualification is mock-only; no hardware, real provider or physical stare tested.
Malformed shared ownership is detectable only where the public shared_ptr
contract provides evidence; arbitrary dangling pointers from a malicious provider
cannot be made safe by a consumer facade.

The initial implementation permanently retained a provider graph after post-send
allocation/worker-establishment failure. R1 below replaces that submission path;
the old conservative-retention qualification above is historical evidence, not
the corrected retirement contract. Qualification and independent review of R1
remain required before declaring readiness.

Independent review of the exact final HEAD remains required. Updating the
structured coverage evidence is not independent technical approval.

## IRMEL-ADA-023-R1 — prepared shared completion worker

### Reproduction and ownership obligations

Baseline `7e90d596fbd862a54c9d8eec7496a5b63d6aab4b` reproduced both
`ir_scheduling_stare_post-allocation` and
`ir_scheduling_stare_post-worker-launch`: 2/2 passed by demonstrating permanent
retention, even after completing the held provider promise. That was the blocker,
not evidence of eventual retirement.

The former `WorkerInput.emergency_self` cycle retained the future, completion and
Session admission permit. Completion's `CommonRequestClaim` retained the typed
family state. Scheduling state retained both provider channel views and Session;
Session retained Control, manager and `SharedLibrary`. Member destruction order
keeps the library alive while those provider objects and deleters run. A public
request owns only completion, not an independent cancellation capability. An idle
common Channel view is weak. The completion worker must consume/destroy the
provider outcome and future before finishing the family claim, and must release
the admission permit exactly once after the provider-dependent work is finished.

### Corrected submission and handoff

The existing Return/Comms template allocates completion, worker input and public
request before admission. It reserves the Session permit and family claim, then
constructs **and detaches** the completion thread before calling the provider.
Both thread establishment operations are fallible, but no provider task exists
yet. An injected detach failure aborts and joins the still-joinable prepared
thread; it does not abandon its thread object. A startup or after-claim failure
finishes the claim and releases the permit without dispatch.

The worker waits on the input's preconstructed atomic gate: 0 prepared, 1 valid
future published, 2 abort. An acquire read of 1 synchronizes with the submitter's
release store after future assignment. On 2 it returns without reading future,
completion or claim. Provider send exceptions and invalid futures always signal
abort; there is no indefinitely waiting prestarted worker on a refused request.
Invalid futures never enter `get()` or completion processing.

The actual provider call is outside lifecycle locks. `RequestFor<T>` is the
pinned `std::future<ErrorOr<shared_ptr<T>>>`; the empty destination's move
assignment and `valid()` are noexcept (compile-time checked). Shared-pointer
reset, atomic store/notification and unique-owner release do not allocate and are
non-throwing. No bridge resource establishment remains between successful send
and handoff. All failpoint selection is also prepared before provider entry.
Immediate-ready futures, pending futures and future exceptions use the same
worker and cached terminal-result implementation.

Typed C2 Return commands/KeepAlive and CommsTest formerly had duplicate launch
logic using these workers. They now delegate to the same submission template;
the C2 adapter preserves enabled-only command admission and attached-or-enabled
KeepAlive/Comms admission. C2 Mode and other typed completion engines are outside
this correction and unchanged.

### Guarantees and limits

- **Recoverable pre-send failure:** genuine error, zero dispatches, zero pending
  family claim and no remaining admission permit or asynchronous obligation.
- **Valid admitted future:** an established worker owns the completion path,
  independently of public request/channel/Session closure. On provider readiness
  and successful cleanup it consumes the future, retires the claim/permit and
  permits safe unload after the last legitimate provider owner releases.
- **Provider/ownership uncertainty:** a never-ready future remains pending; a
  failed detach retains the family graph conservatively. Timeout is not
  cancellation, and no unconditional eventual-unload promise is made. Worker
  retention remains available for unexpected completion/cleanup synchronization
  failure, not ordinary submission resource establishment.

The historical `COMMON_POST_SEND_FAILURE` (and typed-C2 counterpart) test knobs
retain their case names but now explicitly inject **public publication failure**
after worker handoff. They are not renamed pre-send failures: they still dispatch
once, return an error without a public request, and must retire after provider
completion. The eliminated allocation/thread-start window is separately tested
with `COMMON_PRE_SEND_FAILURE`: allocation, worker-launch, after-claim and detach
for Stare, common Return and Comms. Existing post-send tests for other engines
retain their original contracts.

### R1 qualification evidence

Artifacts live outside the worktree in `/home/zboll/irmel023-r1`. `/tmp` remains
inode-exhausted; qualification uses a private mount namespace mapping task-owned
disk scratch to `/tmp`, without deleting unrelated temporary files.

- Baseline reproduction: 2/2 tests passed with the old retention behavior.
- `cmake --build build/irmel023-gcc-Debug --parallel 3`: exited 0, warnings as
  errors; both public C tests and private C++ implementation compiled.
- Focused shared/Scheduling/C2/admission CTest selection: **171/171 passed**.
- Full GCC Debug CTest suite: **878/878 passed** (15 new deterministic cases).
- Earlier development runs failed on stale retention assertions and incorrect
  admission-observer use; they are retained as failed logs, not counted as passes.
- Focused deterministic stress (`ctest --parallel 4 --repeat until-fail:100`
  over those 171 cases): **17,100 executions passed**, exit 0.
- Sequential mandatory qualification is running; results must be recorded on
  completion, not inferred from this initial pass.

The post-publication Stare/Abort tests close all public owners, park the
post-`get`/pre-cleanup barrier, drop the test DSO pin while the future is pending,
verify no teardown, complete the provider promise, then release the barrier.
FinalOwner notification proves retirement before checking exactly one disable,
detach, channel destruction and actual DSO unload. Stare additionally checks the
Session permit changes 1 → 0. The explicit Scheduling detach-failure case proves
claim/permit retirement but conservative graph retention on real cleanup failure.
Common Image/C2 Return and Comms plus Health/Instrumentation/Track publication
failure controls now require retirement; cross-family tests require Session
reuse after it. Invalid-future Return and Comms controls independently prove
zero completion starts/gets and zero remaining Scheduling claims.

R1 changes no production exports, public C layout, Ada API, Stare conversion,
TaskInfo, metadata independence, coverage classification, safe Rust/Python API,
vendor header or CI/documentation policy. Export count remains 333 under
`AMS_MEL_0.1`, and safe-Ada coverage remains 59 operations.

### Main reconciliation and hosted stress correction

Main advanced to `f6c81cfff415566595ba177a50d41eb9f12a51d4` during qualification.
Ordinary merge `d604fa6f0882c2c069f98e6dc028e3a4f13b0da3` preserves its unrelated
safe Python LOS-quaternion implementation, tests and documentation fixture.
Native/Ada/Rust source bytes were unchanged by that merge. Synthetic merge
`9dc0e58bf978e495253b936aba3945f40feb4894` had tree
`088aebee456cbec36c39bfe70ef7b07d24a85c9b`, equal to the intended head tree and
`git merge-tree --write-tree` reconstruction.

On that integrated source, sequential local native 878/878, focused Stare 37/37,
common completion 114/114, focused stress 17,100 executions, full Ada smoke/
negative/example (55 PASS markers), and pinned Alire library build/tests passed.
Coverage/check/Rust/Python/policy qualification is still in progress at this
checkpoint. Pre-merge results are retained separately, not substituted for these.

Hosted API documentation passed for correction c414a61 and integrated d604fa6:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44418600730> and
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44419201063>.
Bootstrap runs
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44418600922> and
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44419201079> exposed a
Health **test observation race** during actual FULL_STRESS. Publication failure
and the admission-refused probe each created a Return input; waiting for just
one destroyed input could observe the refused input rather than the pending
worker. The test now requires the existing all-created-owner notification
barrier before asserting zero admission. This strengthens synchronization and
does not relax retirement. Those failed hosted attempts are not passing gates.
The library implementation is unchanged by this test correction.

All applicable checks must pass on the final correction HEAD, with actual
FULL_STRESS evidence in all four native jobs. Independent technical review has
been requested for pre-send preparation, handoff/abort, non-throwing post-send
operations, invalid futures, provider DSO lifetime, claim retirement, shared
Return/Comms regression, reentry/Close and ABI preservation. No review or approval
has been supplied. Until final qualification completes, disposition is BLOCKED;
after qualification alone it can be READY_FOR_INDEPENDENT_REVIEW, not approval
for automatic merge. PR #241 stays open, non-draft and unmerged with auto-merge
disabled.

### Completed sequential local qualification

The production/library implementation remains exactly c414a61 after main merge
and the Health observation correction. The following commands completed without
overlapping native/Ada build-tree writers, using GNAT 16.1.0, GPRbuild 26.0.1 and
Alire 2.1.1 with the existing pinned crate configuration. Each command's actual
exit is 0; logs and individual exit files are in
`/home/zboll/irmel023-r1/integrated` unless noted.

| Command | Actual result |
|---|---|
| `make test-native` | 878/878 passed |
| focused Stare CTest | 37/37 passed |
| focused common/C2/admission CTest | 114/114 passed |
| shared/Scheduling stress, `--parallel 4 --repeat until-fail:100` | 171 cases, 17,100 executions passed |
| `make test-ada` | full build, 55 smoke PASS markers, 14 minimal negative controls, Stare example passed |
| `alr -n -C ada build` | pinned library build passed |
| `alr -n -C ada/tests run` | pinned smoke/negative tests passed |
| `make check-ir-api-coverage` | 75/75 controls passed; safe_ada=59 unchanged |
| `make check` | passed; isolated build-tree regression, 878-test native suite, formatting, Ada and newline/whitespace checks |
| `make test-rust` | 640 passed, 0 failed, 15 existing ignored (53 suite summaries) |
| workspace `cargo check`, Clippy all-targets `-D warnings`, `cargo fmt --check` | all passed |
| `make test-python` | 718 passed; package compileall passed |
| `make test-policy` | 332 passed across 14 sub-suites |
| source integrity | 825 vendor files, six workflows; passed |
| final newline and `git diff --check` | passed |
| `make check-ada-format` | passed |
| `make check-ada-docs-changed` | 12 public declarations; zero missing/invalid annotations |

The former overlap-related `make check` failure is resolved by the isolated
successful execution (23:22:46–23:30:16 UTC, 2026-11-18). Existing GPR imported-
project warnings about no C sources remain unchanged; no first-party compiler
warning was introduced or suppressed.

The outer integrated qualification wrapper exited 127 after all individual
commands recorded success: editing that running shell script to pause for main
integration left a stale trailing shell token. That wrapper exit is **not** a
passing aggregate result. The affected final documentation command and all
final focused/ABI checks were independently rerun by the unmodified
`finish-final.sh`, exit 0. Its final-source stress again passed 17,100 executions;
Stare passed 37/37. The compiled production DSO was compared against
`exports.map`: exactly 333 `AMS_MEL_0.1` exports, no test exports.

On fb010f0, API documentation passed:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44420272832>.
Bootstrap <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44420272958>
had eight jobs passed at this checkpoint, with GCC Debug still running. Retrieved
GCC Release, Clang Release and Clang Debug logs prove actual FULL_STRESS execution
(`ctest --parallel 4 --repeat until-fail:50`) and two 878/878 pass summaries each:
ordinary suite plus 43,900 stress executions per configuration. Final evidence-
commit hosted qualification must still be checked; these superseded-source
results do not substitute for exact-final-HEAD CI or independent review.

The fb010f0 GCC Debug campaign subsequently failed Health `metadata-stop` on a
second test observation race: waiting for Worker `active` could return before
the following `peak` update in `start()`. The test now waits for the existing
`get_entries` barrier, which occurs after all startup accounting, before
asserting the unchanged peak value. Bootstrap 44420272958 is therefore a failed
attempt overall, not an eight-job substitute for final qualification. No
completion implementation or contract was weakened for either observation fix.
