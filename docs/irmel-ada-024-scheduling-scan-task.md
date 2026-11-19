# IRMEL-ADA-024 — safe Scheduling IRSTScanTask execution

## Preflight and source

Task branch `feature/irmel-ada-024-scheduling-scan-task`, isolated worktree
`/home/zboll/git/ams-mel-irmel-ada-024`. Freshly fetched main:
`4005df6c1a467e685a8a16f64e10e2d9fe947a9b`, tree
`be1868386d7fabc34c19ef81ee17c10613684519`.
PR #241 was confirmed MERGED through the GitHub API, merge commit
`2857bc5c759dd790fc2ebba81a6c4ed15fbd4f9f`, and is in main's history.
Remote branch and all-state PR searches found no overlapping ScanTask work;
the only open PR at preflight was unrelated documentation PR #244.

Read the repository instructions, architecture, C ABI policy, coverage schema,
inventory, and the IRMEL-ADA-022/023 task evidence. Inspected the pinned
SchedulingChannel, IRSTScanTask, ScanParam, ScanType, TaskInfo and associated
CommonIR_MEL enum declarations. IR MEL revision remains
`8d9224519f12b44e0b28815755c56a32a28d24a0`; no vendored source is changed.

Verified baseline: 333 production exports, 59 unique safe Ada operations,
7 missing Scheduling-specific operations; SchedulingChannel partial.

Current-main reconciliation: unrelated PR #244 advanced main to
`72492c53b0dd4a71d322bcb4b0f45a18b602ca9e`, tree
`cd3e358bc5b2be56afcc185ed9e6cc6b4c586a85`. An ordinary fast-forward
preserves its documentation tooling changes; no overlapping implementation or
ABI changes were present. The initial commit/tree above remain the preflight
baseline.

## Implementation and representation

Exactly one production export is added under `AMS_MEL_0.1`:
`ams_mel_ir_scheduling_exec_scan`. The C11 `ams_mel_ir_scan_task_v1`
contains the **existing** `ams_mel_ir_scan_param_v1`, the **existing**
`ams_mel_ir_task_info_v1`, and a complete unsigned 32-bit command ID.
LP64 size/alignment: 144/8; offsets: scan parameters 0, TaskInfo 112,
command ID 136. No old record, signature or symbol changes.

C2's validation and ScanParam constructor conversion are extracted into
`native/src/internal/ir_scan_param.hpp` and reused by both consumers.
C2 retains its original validation domains and construction sequence.
Scheduling validates before provider entry, constructs the actual upstream
`IRSTScanTask(id, params, info)`, then verifies every published getter.
Verification includes every nested ScanType and TaskInfo getter, independent
frame references and exact binary64 bit comparison, including negative zero.
No private storage inspection, Boolean ScanType narrowing, angle limit,
positive-rate requirement, unit conversion, identifier relation, operational
feasibility or sensor limit is introduced. Published discriminators only:
elevation selector 0/1, each frame 0/1, degradation 0..3 and action 0..3.

Safe Ada `AMS.MEL.IR.Scheduling.Commands` adds `Scan_Task`, `Scan_Request`,
`Scan_Result`, `Exec_Scan` and the existing observer/result operation overloads.
`Scan_Task.Parameters` is exactly `AMS.MEL.IR.C2.Scan_Parameters`, not a
duplicate type. Its existing raw conversion moves into private
`AMS.MEL.IR.Scan_Conversion`, shared with C2 without changing C2's public API.
Input values are owned Ada records; native handles, addresses and imports stay
private. Thin forwarding operations contain the existing limited-controlled
Return observer and cached result; there is no second completion engine.

The established Enabled-only Scheduling adapter acquires admission and lifetime
claims before dispatch and invokes the provider outside its lifecycle mutex.
Existing provider graph retention, deferred Close, reentry, exception mapping,
observation-only timeout, cached Wait, failed Return versus MEL rejection,
and conservative retention on uncertain ownership remain unchanged.

## Tests and example

The separate mock implements the exact ScanTask overload with published-getter
capture of every value, an independent send barrier/future family, real promise
completion, Return/error/exception scenarios and metadata hooks. Strict-C11
tests cover conversion and dispatch, invalid domains before entry, all four
actions/degradation methods, all frame combinations, integer edges, signed zeros,
pending/timeout/cached completion, malformed/invalid futures, reentry, admitted
concurrent Close, post-send retention, cleanup and eventual unload.
ExecuteTaskAck and TaskExecutingRep remain independent and use unrelated IDs.
Existing Scheduling foundation tests and safe Ada test cover unsupported providers.

Safe Ada tests compare all input fields against the mock's upstream getters,
including signed-zero bits, and exercise the shared observer and lifecycle.
Private ctypes and raw Rust sys mirror layout/signature tests compare against
the compiled strict C11 probe. No safe Python or Rust ScanTask API is added.
The standalone `ada/tests/src/ams_mel_ir_scheduling_scan_example.adb` uses only
public safe Ada interfaces and runs against the mock provider in `make test-ada`.

## Coverage delta

Authoritative coverage and its reviewed ceiling/report classify only
`SchedulingChannel::exec(IRSTScanTask)` and `IRSTScanTask` from missing to
safe_ada. Existing ScanParam/ScanType/TaskInfo coverage gains no duplicate credit.
Unique safe Ada operations **59 → 60**, missing Scheduling operations **7 → 6**,
production exports **333 → 334**. SchedulingChannel remains partial.
TrackTask, prioritized track lists and the four other metadata registrations
remain missing. Coverage regression tests retain the negative control against
claiming TrackTask by borrowing foundation evidence.

## Qualification record

All required local qualification commands and final stress passed.
Local disposition: **READY_FOR_INDEPENDENT_REVIEW**. Hosted CI and independent
review remain separate gates.
Actual command logs and exit files are recorded outside the source tree under
`/var/tmp/irmel-ada-024-qualification`.

Initial environment failures are not passing qualification:

- `/tmp` exhausted all 1,048,576 inodes. Initial log-directory creation failed
  (exit 1); C compiler temporary-file creation aborted (exit 134).
- First `make test-native` compiled successfully but failed (make exit 2)
  because many existing tests hardcode `/tmp` for lifetime files.
- Initial coverage and Ada qualification exposed stale Scheduling count
  expectations (4 safe/7 missing); corrected to 5 safe/6 missing. Tests were
  not weakened or skipped.
- Initial Ada build also caught a missing visibility clause for signed-time
  unary minus in the standalone example; restored the required clause. An
  exploratory compile before the production native library existed exited 5.
- The first stress invocation exited 8 with tests **not run**: Alire changed
  compiler paths, causing the existing configure script to reset the test tree
  and build only providers. Rebuild the test executables after all Alire/native
  toolchain transitions before running final stress; no test is skipped.
- A first private-namespace attempt omitted a usable `/dev` and failed the
  formatting/documentation checks. It was stopped and replaced with a namespace
  providing `/dev` and a task-owned filesystem-backed `/tmp`.

Subsequent qualification uses:

```sh
bwrap --bind / / --dev /dev \
  --bind /var/tmp/irmel-ada-024-qualification/private-tmp /tmp -- COMMAND
```

This is a local mount namespace, not a container/provider dependency. No
unrelated worktree, temporary directory, global Git configuration or vendor
source is deleted or modified to obtain space. The first focused native
ScanTask campaign passed 37/37 (exit 0); strict C11 probe passed and printed
`144 8 0 112 136`. Additional completed results (all exit 0):

- Full native suite: 915/915.
- Safe Ada focused ScanTask suite and standalone example.
- Python: 753 tests and package compileall, including raw C11 layout mirror.
- Rust workspace: 641 tests/doctests across 53 summaries, including raw C11
  ScanTask mirror and exact export inventory.
- Coverage: 76 regression tests; final manifest gate regenerated and checked.
- GNATdoc changed declarations: 10 public declarations, zero invalid annotations.
- Formatting and final-newline checks.
- Source integrity after main reconciliation: 825 vendor files and 7 workflows.
- Production DSO compared directly to exports.map: exactly 334 versioned
  AMS_MEL_0.1 exports, no test exports; no existing symbol removed.
- Standalone warning-clean C++ conversion matrix; the shared C2 conversion
  extraction was compared textually against the original construction/validation.

Final explicit qualification commands (all observed exit 0):

| Command | Exit | Result |
|---|---:|---|
| `make format-ada` | 0 | GNATformat applied before build/test |
| `make check-ada-format` | 0 | Final first-party Ada source clean |
| `make check-ada-docs-changed` | 0 | 10 public declarations; zero missing/invalid |
| `make test-native` | 0 | 915/915, including final post-Alire rebuild |
| `make test-ada` | 0 | Full smoke suite, negative controls and both examples |
| `make test-python` | 0 | 753 tests; package compileall |
| `make test-rust` | 0 | 641 tests/doctests; raw ABI mirror included |
| `make check-ir-api-coverage` | 0 | 76 regression tests and generated report |
| `make check` | 0 | 01:38:16–01:46:29 UTC; isolation/native/Ada/format/newlines |
| `alr -n -C ada build` | 0 | Explicit Alire build |
| `alr -n -C ada/tests run` | 0 | Full smoke suite including ScanTask |

The imported native GPR project continues to emit its existing "no sources of
language C" project warning; first-party compiled code remains warning-clean.
No toolchain absence, skipped test, stale hosted result or failed attempt is
counted as passed.

Final deterministic stress command (exit 0):

```sh
ctest --test-dir native/build-tests \
  -R 'ir_scheduling_(scan|stare|abort)_' --parallel 4 \
  --repeat until-fail:100 --output-on-failure
```

94 cases × 100 repetitions = **9,400 passing executions**, covering ScanTask,
StareTask, AbortTask, conversion/layout, admission, reentry, concurrent Close,
future retirement and actual provider unload. Final source integrity, formatting,
newlines, strict C11 layout and the exact 334-export production DSO inventory
were verified. The final delivery commit/tree is recorded in the PR/delivery
report rather than self-referentially in this file.

## Boundaries and review gates

Mock-provider qualification only: no hardware, physical sensor scan, real
provider Scheduling support, scan feasibility, image acquisition or normative
compliance qualification is claimed. A successful Return reports provider
acceptance of scheduling, not physical execution. No acknowledgement or task
report is synthesized or correlated with future completion.

The shared Return engine establishes its worker before dispatch. Historical
post-send allocation/worker-launch failpoints now model failed public observer
publication: the established worker still retires the future and graph. Failed
detach conservatively retains uncertain provider ownership; eventual unload is
asserted only on paths whose ownership can safely retire. Callers serialize observer Wait/Close
and channel handle access against Close until successful admission.
Hosted CI and independent technical review are separate outstanding gates.
