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

`Stare_Request` and `Stare_Result` are distinct private types, privately derived
from the existing limited-controlled Return observer and owned result. Thin
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
submission/completion implementation. It now rejects an invalid future before
starting a worker (matching existing typed-command policy), and rejects a
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

The existing shared engine permanently retains an uncertain provider graph when
the post-send allocation/worker-launch failpoint prevents establishing a worker.
This task reuses that conservative behavior and tests that it does **not** unload
pending provider code. It does not pretend that later promise completion causes
eventual unload on that path. This is a concrete unresolved acceptance mismatch
with the task's post-send eventual-unload requirement. Resolving it requires a
reviewed shared-engine recovery/retirement contract, not weaker lifetime tests or
a new Stare-only worker architecture. Disposition remains **BLOCKED** while that
requirement is unresolved, regardless of ordinary-path successful teardown.

Independent review of the exact final HEAD remains required. Updating the
structured coverage evidence is not independent technical approval.
