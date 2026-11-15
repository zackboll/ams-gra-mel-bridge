# IRMEL-ADA-022 — safe Scheduling AbortTask execution

## Base and scope

Fetched `origin/main`: `43f8acc8843a907dd91150961bc94023ec23c414`, tree
`e0af603fb1243843aa174a61d49571530f725509`. The working tree was clean. Created
`feature/irmel-ada-022-scheduling-abort-task` directly from that base after
checking worktrees and open PRs (Python, Rust and documentation work, no
overlapping AbortTask implementation). The preparation SHA was older than fresh
main. The reference consumer parity task/harness and historical coverage archive
are unchanged.

This implements exactly `SchedulingChannel::exec(AbortTask)` at IR MEL revision
`8d9224519f12b44e0b28815755c56a32a28d24a0`. All vendor sources remain unchanged.
There is no scheduler, task fabrication, automatic retry, CommandID inference,
request correlation, physical abortion claim or synthetic metadata delivery.

## Conversion and ABI

Exactly one production export is added under `AMS_MEL_0.1`:
`ams_mel_ir_scheduling_exec_abort`. The production inventory changes 331 → 332.
The versioned C11 `ams_mel_ir_task_info_v1` contains signed 64-bit nanoseconds
and four unsigned 32-bit fields. On the qualified LP64 targets its size is 24,
alignment 8, and field offsets are 0, 8, 12, 16 and 20.

| Public Ada field | C field | Published TaskInfo interface |
|---|---|---|
| `System_Time_NS` | `system_time_ns` | `setSystemTime` / `getSystemTime().count()` |
| `Request_ID` | `request_id` | `setReqId` / `getReqId` |
| `Action` | `action` | `setAction` / `getAction` |
| `Precedence_Within_Priority` | `precedence_within_priority` | `setPrecedenceWithinPriority` / `getPrecedenceWithinPriority` |
| `Priority` | `priority` | `setPriority` / `getPriority` |

The single private converter `convert_task_info` validates the action domain,
calls all five setters, and verifies all five public getters **before** calling
the actual upstream `AbortTask(TaskInfo)` constructor. Static assertions pin
Normal=0, Next=1, Immediate=2 and Abort=3, and the signed nanosecond representation.
`Normal` is not coerced to `Abort`. `INT64_MIN`, `INT64_MAX`, zero and high-bit
unsigned values remain exact; no epoch, normalization or priority transformation
is applied. Unknown actions are rejected before provider entry.

`AbortTask` has no published TaskInfo getter. Neither the mock nor any test reads
its private member or object bytes. Converter tests inspect the real TaskInfo
through its published getters; provider tests separately establish exact dispatch
to the AbortTask overload. This is intentionally not described as a provider-side
round trip of inaccessible fields.

Private Python ctypes and raw Rust sys declarations mirror the record/signature.
Their layout tests compare against a compiled strict C11 probe. No safe Python
or Rust command API is introduced.

## Ownership, execution and asynchronous completion

`AMS.MEL.IR.Scheduling.Commands` owns a limited-controlled `Abort_Request` and
copyable `Abort_Result`. The submission parameter is `Info`, because `task` is
an Ada reserved word. Result inspection reuses public Channel `Outcome`,
`Command_Return` and `Error_Code` types. Misusing a result accessor raises
`Provider_Error` even with assertion checks disabled. Rejection text is owned,
including a checked second cached Wait when the first diagnostic buffer is short.
Finalization is non-raising.

A private Enabled-only specialization of the existing Scheduling adapter claims
the Scheduling-specific request counter and full provider graph under the
lifecycle mutex, then releases that mutex before calling the actual
`SchedulingChannel::exec(abort_task)`. Attached, Failed and logically Closed
owners cannot submit. Closing the public owner after admission defers physical
disable/detach until its last request retires. Common requests use their original
Attached-or-Enabled policy and the same Scheduling-only accounting domain.

The private `submit_return` hook uses the existing common Return submission
template and completion worker: Session admission permit, pre-send allocations,
post-send failure retention, future consumption, cached terminal result,
completion claim retirement and provider DSO protection are not duplicated.
Existing Return Wait/Close exports are reused. No additional worker engine,
Wait export or Close export is added.

Success means a future completed with a valid Return payload. Fail,
NotSupported and NotImplemented are successful future payloads, not bridge
failures or MEL rejections. Explicit Unsupported and InvalidState errors are
Rejected with the actual code and complete description. Null payloads and
unknown Return values are provider failures; exec/future exceptions are provider
exceptions. Timeout does not consume/cancel. Observer Close does not cancel.
Terminal Wait is cached and repeatable after channel and Session closure.

The pre-existing Attached→Enabled provider call and capability query still run
under the Scheduling lifecycle mutex. This task does not broaden or repair those
unrelated paths. Reentrant AbortTask dispatch on an already Enabled owner is
tested without changing Enable semantics. Callers must externally serialize
public handle access against Close until submission has been admitted; this is
not an arbitrary concurrent raw-handle deletion protocol.

## Metadata independence

ExecuteTaskAck and TaskExecutingRep public contracts are unchanged. Both opt-in
owners can coexist with a pending AbortTask. The native test delivers an actual
ExecuteTaskAck with unrelated identifiers and verifies that the AbortTask future
remains pending. Empty metadata queues stay empty merely because an AbortTask is
submitted or completes. No CommandID/RequestID relationship is inferred.

## Regression evidence and qualification status

Actual focused results obtained during implementation:

- Native GCC Debug build with `-Wall -Wextra -Wpedantic -Werror`: passed.
- `ctest --test-dir native/build-tests -R '^ir_scheduling_abort_' --output-on-failure`:
  initially 18/18 passed; final matrix 20/20 passed after adding the two
  conservative post-send failure-retention controls. Includes compiled-C ABI/layout,
  real conversion, Return/error
  mapping, malformed payloads, exceptions, finite timeout, cached Wait, observer
  closure, owner/Session/view closure, concurrent Close after admission and reentry.
- Focused delayed/reentry/observer-close/concurrent-close tests repeated 100 times
  each: passed (400 executions, no sleeps).
- Safe Ada focused executable: passed using only public binding operations, with
  a separate fixture for mock promise release and worker-notification observation.
- `make test-native`: initially 839/839 passed; the final 841-test inventory is
  exercised by `make test-rust` and the matrix below.
- `make test-python`: 687 tests passed, including 62 private ABI tests.
- `make test-rust`: 613 Rust tests passed, zero failures; its native prerequisite
  passed 841/841.
- `make test-ada`: passed (54 PASS markers, including AbortTask and the separate
  14-control minimal-consumer negative qualification).
- `make check-ir-api-coverage`: 72/72 controls passed, 58 safe-Ada operations.
- Rust Scheduling ABI tests: 2 passed, including strict C11 comparison.
- `make check-ada-docs-changed`: passed, 11 public declarations inspected, zero
  missing/invalid annotations.

The sequential runner is `/tmp/irmel022-qual/sequential.sh`; its status and
individual command logs are in that directory. Matrix runner:
`/tmp/irmel022-matrix/run.sh` (separate output directories, no overlap with native
or Ada managed build directories). Both use the repository's warnings-as-errors
policy. `make check` and the final Alire/lint gates are still in progress; no
independent-review readiness is claimed here yet.

| Native configuration | Full suite | Focused AbortTask | Deterministic stress |
|---|---:|---:|---:|
| GCC 14.2 Debug | 841/841 through Rust prerequisite | 20 cases in full suite | pending final rerun after `make check` |
| GCC 14.2 Release | 841/841 | 20/20 | 400 initial + 2,000 additional executions |
| Clang 19.1 Debug | 841/841 | 20/20 | 400 executions |
| Clang 19.1 Release | 841/841 | 20/20 | 400 executions |

Each matrix configuration runs `cmake -S native -B <isolated-directory>
-DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=<Debug|Release>` with explicit GCC or
Clang C/C++ compiler paths, `cmake --build ... --parallel 4`, full `ctest`, then
`ctest -R '^ir_scheduling_abort_(delayed|reentry|observer-close|concurrent-close)$'
--repeat until-fail:100 --output-on-failure`. The additional GCC Release stress
uses repeat count 500. All final matrix build logs contain zero compiler warnings.

Pinned GNATdoc 26.0.0 (20260415) generation passed with public/trailing style:
`GPR_PROJECT_PATH=/home/zboll/git/ams-gra-mel-bridge/native:/home/zboll/git/ams-gra-mel-bridge/ada
alr -n -C docs/toolchain exec -- /home/zboll/.alire/gnatdoc_bin_26.0.0_6e6e0320/bin/gnatdoc
-P /home/zboll/git/ams-gra-mel-bridge/ada/ams_mel.gpr --backend html --generate public
--style trailing --warnings -O /tmp/irmel022-qual/gnatdoc-pinned`. The generated
Commands page contains every new public field and operation. No public Commands
warnings; two informational warnings concern its private Finalize declaration
and parameter. Existing raw-import documentation warnings remain visible.

`make test-policy` passed, including source integrity (825 vendored files/four
workflows) and documentation/security controls. Exact production `nm -D
--defined-only` name-set equality with the export map passed (332 names), and
comparison with base main found exactly the one addition above, no removals.

Preserved failed intermediate evidence: Ada initially rejected the reserved
parameter name `Task`; GNAT warnings required constant observers in two tests;
Clang rejected missing new-file trailing newlines; the first coverage run found
an old Scheduling count assertion (2 instead of 3). All were corrected, never
suppressed. An ad-hoc pinned GNATdoc invocation omitted GPR_PROJECT_PATH and was
rerun successfully with the native project import path. One standalone focused
Ada invocation overlapped the managed `make check` rebuild and could not load
the temporarily absent mock DSO; it is not counted as qualification. Full Ada
qualification itself passed. No hosted final-HEAD result is claimed yet.

Native tests use deterministic provider promise and send-entry barriers, never
sleep-based release. Before removing a lifetime log, they observe the established
final-owner notification-return boundary for Return and Comms workers. The
pending-close cases drop the test's provider DSO pin while work is still pending,
verify no unload, then re-pin before invoking test-only promise-release code
(the bridge protects pending work, not harness execution after release). They
release the real future and require exactly one disable, detach,
channel destruction and eventual library unload. Ada tests separately drain the
same notification boundary before their fixture pin drops and before the smoke
harness can delete its temporary lifetime-log directory.

## Coverage delta

The authoritative coverage JSON, reviewed-binding ceiling and generated report
record `SchedulingChannel::exec(AbortTask)`, `AbortTask`, `TaskInfo` (all five
getters represented) and `TaskListAction` as safe Ada. Unique safe-Ada operations
increase **57 → 58**; data types do not add operation counts. SchedulingChannel
remains partial. The three other exec overloads, PrioritizedTrackListCmd and four
other metadata families remain missing. Audit tests retain negative controls
against borrowing foundation evidence for an unimplemented Scan command.

## Limitations

- Positive qualification is mock-provider-only. No hardware, Squall or real
  sensor Scheduling task execution was tested.
- The pinned minimal reference provider does not advertise Scheduling, so it
  cannot positively qualify this conditional operation. Unsupported capability
  is tested separately from a missing binding.
- A successful AbortTask future proves neither physical abortion nor subsequent
  ExecuteTaskAck delivery. Receiving that metadata does not complete the future.
- No private AbortTask accessor exists; no upstream declaration was modified.
- Existing conservative post-send retention may deliberately retain an uncertain
  provider graph rather than unload code unsafely.
- No implementation of excluded task commands, other Scheduling callback
  families, local prioritization, sensor-side execution or automatic retries.
