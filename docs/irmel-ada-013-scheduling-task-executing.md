# IRMEL-ADA-013 — complete Scheduling TaskExecutingRep metadata

## Base and scope

PR #179 was merged before this task. Fresh remote refs selected `origin/main` at
`1dc48caf1811051738cba472c09c6515b60645d4`, tree
`6e05ef035a593a38588c3b45a120d77a0e4f51fc` (the last verified main). The feature worktree is
`/home/zboll/git/ams-mel-irmel-ada-013`, branch
`feature/irmel-ada-013-scheduling-task-executing`, created directly from that commit; target
is `main`, not stacked. No rebase, amend, force push, self-approval, auto-merge or PR merge is
performed by this feature.

This slice adds only owned reception of
`SchedulingChannel::registerMetadataCallback(TaskExecutingRep)` through one explicit opt-in
owner, reusing the IRMEL-ADA-011 Scheduling foundation and following the IRMEL-ADA-012
ExecuteTaskAck contract. It never implements or invokes any Scheduling `exec`,
`send(PrioritizedTrackListCmd)` or other Scheduling metadata registration. A report says what
the provider reports as executing; it is not a task request, execution, completion or success
proof, and the bridge correlates nothing. The Scheduling channel itself remains partial.

Per the PR #178 policy, `docs/coverage.md` (the historical archive) is not modified; this
task-owned document carries the feature narrative, coverage delta, tests and limitations.

## Pinned upstream and the ten-field mapping

IR MEL revision `8d9224519f12b44e0b28815755c56a32a28d24a0` is unchanged; no new upstream
header was needed (`TaskExecutingRep.h`, `SchedulingChannel.h` and `CommonIR_MEL.h` were
already vendored by IRMEL-ADA-011). `SchedulingChannel.h` declares exactly
`std::function<void(Channel&, const TaskExecutingRep* const)>`. Compile-time checks bind all
ten getter return types, both enums' underlying `std::uint32_t` type, the signed 64-bit
nanosecond representation and every enumerator value to upstream. No provider layout is used.

| Upstream getter | Pinned type | C field (offset) | Ada field |
|---|---|---|---|
| `getCommandID()` | `std::uint32_t` | `uint32_t command_id` (0) | `Command_ID : Unsigned_32` |
| `getActivityId()` | `std::uint32_t` | `uint32_t activity_id` (4) | `Activity_ID : Unsigned_32` |
| `getReqId()` | `std::uint32_t` | `uint32_t request_id` (8) | `Request_ID : Unsigned_32` |
| `getTask()` | `const Task&` | `ams_mel_ir_task_t task` (12) | `Current_Task : Task_Kind` |
| `getEstimatedDuration()` | `std::chrono::nanoseconds` | `int64_t estimated_duration_ns` (16) | `Estimated_Duration_NS : Integer_64` |
| `getTrackId()` | `std::int32_t` | `int32_t track_id` (24) | `Track_ID : Integer_32` |
| `getPrevActivityId()` | `std::uint32_t` | `uint32_t previous_activity_id` (28) | `Previous_Activity_ID : Unsigned_32` |
| `getPrevReqId()` | `std::uint32_t` | `uint32_t previous_request_id` (32) | `Previous_Request_ID : Unsigned_32` |
| `getPrevTask()` | `const Task&` | `ams_mel_ir_task_t previous_task` (36) | `Previous_Task : Task_Kind` |
| `getPrevTaskStatus()` | `TaskCompStatus` | `ams_mel_ir_task_comp_status_t previous_task_status` (40) | `Previous_Task_Status : Task_Completion_Status` |

`ams_mel_ir_task_executing_rep_v1` is 48 bytes with 8-byte alignment (four tail padding bytes),
verified by C11 `_Static_assert`s, C++ `static_assert`s, the compiled C11 layout probe, the
Ada representation check and the Python/Rust mirrors. The duration is the exact signed
`count()`; it is never converted to seconds, Ada `Duration`, a time point or an epoch. Negative
durations and track identifiers, zero and high-bit identifiers are valid. Current and previous
identifiers and tasks are independent; no relationship is inferred. The existing
`AMS_MEL_IR_METADATA_TASK_EXECUTING_REP = 12` is unchanged and checked against upstream.

## Task and TaskCompStatus

`Task` and `TaskCompStatus` are separate `std::uint32_t` enums with separate C typedefs and
Ada types. An unknown value in any one of the current task, previous task or previous-task
status rejects the whole report (malformed counter, never queued, nothing partially published).

| Value | Upstream `Task` | C constant | Ada `Task_Kind` | Classification |
|---:|---|---|---|---|
| 0 | NoTask | `AMS_MEL_IR_TASK_NO_TASK` | `No_Task` | meaningful |
| 1 | AbortTask | `AMS_MEL_IR_TASK_ABORT_TASK` | `Abort_Task` | meaningful |
| 2 | IRSTCamCal | `AMS_MEL_IR_TASK_IRST_CAM_CAL` | `IRST_Camera_Calibration` | meaningful |
| 3 | CamCal | `AMS_MEL_IR_TASK_CAM_CAL` | `Camera_Calibration` | meaningful |
| 4 | IRSTBoresight | `AMS_MEL_IR_TASK_IRST_BORESIGHT` | `IRST_Boresight` | meaningful |
| 5 | Idle | `AMS_MEL_IR_TASK_IDLE` | `Idle` | meaningful |
| 6 | IRSTScan | `AMS_MEL_IR_TASK_IRST_SCAN` | `IRST_Scan` | meaningful |
| 7 | IRSTPosition | `AMS_MEL_IR_TASK_IRST_POSITION` | `IRST_Position` | meaningful |
| 8 | IRSTSTT | `AMS_MEL_IR_TASK_IRST_STT` | `IRST_Single_Target_Track` | meaningful |
| 9 | IRSTMTT | `AMS_MEL_IR_TASK_IRST_MTT` | `IRST_Multiple_Target_Track` | meaningful |
| 10 | IRSTUpdate | `AMS_MEL_IR_TASK_IRST_UPDATE` | `IRST_Update` | meaningful |
| 11–16 | Reserved1–Reserved6 | `AMS_MEL_IR_TASK_RESERVED_1..6` | `Reserved_1..Reserved_6` | preserved, non-operational |
| 17 | NumTaskTypes | — | not representable | count sentinel: malformed |
| > 17 | — | — | not representable | unknown: malformed |

Reserved values follow the existing bridge policy for reserved published alternatives
(`ChannelType` Reserved_1/2, reserved metadata capabilities): preserved as labelled literals
with no operational meaning. The Ada subtype `Operational_Task_Kind` covers exactly 0..10 and
`Is_Operational` is False exactly for the reserved literals. Preserving a reserved value does
not make it supported or executable. The `NumTaskTypes` count sentinel follows the bridge's
MaxExclusive policy and is rejected, never remapped.

| Value | Upstream `TaskCompStatus` | C constant | Ada `Task_Completion_Status` |
|---:|---|---|---|
| 0 | NoTask | `AMS_MEL_IR_TASK_COMP_STATUS_NO_TASK` | `Status_No_Task` |
| 1 | Complete | `AMS_MEL_IR_TASK_COMP_STATUS_COMPLETE` | `Complete` |
| 2 | Timeout | `AMS_MEL_IR_TASK_COMP_STATUS_TIMEOUT` | `Timeout` |
| 3 | Abort | `AMS_MEL_IR_TASK_COMP_STATUS_ABORT` | `Abort_Status` (`abort` is an Ada reserved word) |
| 4 | Skipped | `AMS_MEL_IR_TASK_COMP_STATUS_SKIPPED` | `Skipped` |

Any other status is malformed; upstream declares no sentinel. In a report the status belongs
to the previous task only, never to the current task. Ada conversion is checked a second time
in `To_Report`; an out-of-domain native value raises `Provider_Error` and the native event is
still closed.

## Additive ABI and safe Ada ownership

Six new exports, all under the existing `AMS_MEL_0.1` version:

- `ams_mel_ir_scheduling_task_executing_open`
- `ams_mel_ir_scheduling_task_executing_receive`
- `ams_mel_ir_scheduling_task_executing_get_counters`
- `ams_mel_ir_scheduling_task_executing_close`
- `ams_mel_ir_scheduling_task_executing_event_view`
- `ams_mel_ir_scheduling_task_executing_event_close`

Production inventory increases 295 → 301 (verified by `nm -D --defined-only` against an
independently built baseline: exactly these six additions, zero removals, every symbol
`@@AMS_MEL_0.1`, no test-only symbol). No existing record, enum, export or signature changes.
C11 and C++ signature checks cover all six exports. Python ctypes and Rust sys gain private raw
mirrors and layout/constant tests against the compiled C11 probe; neither gains safe reception.

`AMS.MEL.IR.Scheduling.Metadata` gains `Task_Kind`, `Operational_Task_Kind`, `Is_Operational`,
`Task_Completion_Status`, the Ada-owned `Task_Executing_Report`, the limited-controlled
`Task_Executing_Stream`, `Open_Task_Executing`, `Receive_Task_Executing`,
`Task_Executing_Counters` and a `Close` overload. It reuses `Not_Supported`, `Metadata_Counters`
and the existing `Timeout_Error` / `Stream_Stopped` / `Provider_Error` conventions. The report
is a separate type from `Execute_Task_Ack`. Receive copies all ten fields into Ada ownership
and closes the native event before returning and on every conversion exception. Finalization
is non-raising.

## Independent registration and callback lifetime

The Scheduling owner carries two separate one-shot flags (`execute_task_ack_attempted`,
`task_executing_attempted`) and two separate callback states (`ExecuteTaskAckState`,
`TaskExecutingState`), each with its own leaf mutex, bounded queue, counters, condition
variables and in-flight callback count, and each family has its own public owner. Neither
open registers or requires the other; both may be open on one owner in either order. The
TaskExecutingRep claim runs under the Scheduling lifecycle mutex on an Attached or Enabled
owner; invalid local arguments and pre-registration allocation failure consume no attempt;
every attempt reaching the provider consumes it, including refusal, unknown Return and provider
exceptions. The exact upstream overload is called with no bridge lock held, so synchronous
delivery from inside registration is safe. Success, Fail, NotSupported, NotImplemented and
BadPointer are mapped separately; unknown values leave the registration output unchanged and
fail; provider exceptions have a distinct status. No exception crosses the callback boundary.

The registered closure captures only its shared `TaskExecutingState`, never the public wrapper
or provider payload. Each callback reads and validates all ten getters, then copies them,
before it returns. The bounded FIFO is DROP-INCOMING: domain validation precedes the
queue-full decision, so null payloads and invalid enumerations are malformed even while full;
a valid report arriving while full is counted queue-full without allocating a snapshot.
Received, queue-full and malformed counters saturate. Allocation failure rejects one whole
report, preserves earlier queued reports, and later reception recovers.

Scheduling Close stops both families and wakes blocked receivers of each. Already queued
reports drain first, even after channel/Session closure and before a terminal cleanup failure.
Public metadata Close only deactivates that family; upstream has no unregister. Pending
inherited common requests still defer physical teardown with unchanged admission and
non-cancellation semantics. Physical cleanup destroys the provider channel while the Session
and provider library graph are retained, then drains in-flight bridge callbacks of the
ExecuteTaskAck family and then the TaskExecutingRep family before releasing that graph. No
callback-state lock is nested with another or held across provider registration, disable or
detach. Late callbacks during channel destruction are memory-safe through retained callback
state and never enqueue. A failed direct detach marks both owners failed and retains the
Scheduling owner for retry; an uncertain deferred detach retains the complete graph
permanently, preserving the foundation policy. The ExecuteTaskAck teardown tests are unchanged
except for the negative control described below.

## Negative controls and deterministic mock scenarios

`MockSchedulingChannel` now implements the TaskExecutingRep registration
(`MockTaskExecuting`) next to the existing ExecuteTaskAck one. The mock keeps three
independent counters: `mock_scheduling_ack_registrations`,
`mock_scheduling_rep_registrations` and `mock_scheduling_specific_calls`, the last covering the
nine remaining Scheduling-specific methods (four `exec` overloads,
`send(PrioritizedTrackListCmd)`, and the SchedCreatedRep, UpdateTrackListResponse, TaskEvents
and ScanPerformanceReport registrations). Every TaskExecutingRep C scenario asserts zero calls
and zero lifetime records for all nine plus exact independent registration counts of both
families; the ExecuteTaskAck and foundation C/Ada tests additionally assert zero
TaskExecutingRep registration attempts, so neither path registers it implicitly.

Reports are built only through the pinned setters, including deliberately out-of-domain enum
values. Scenarios `sched-rep-*` offer TaskExecutingRep only; `sched-both-*` offer both families.
The mock supports synchronous delivery during registration, manual delivery on the caller's
thread (`mock_scheduling_rep_deliver`), null payloads, every registration outcome, failed
detach, late callbacks during channel destruction and a deliberately non-quiescing in-flight
producer joined only at library unload. Each delivered payload is overwritten with unrelated
values and destroyed after callback return. All concurrency uses the independent test-facade
probe `ams_mel_test_scheduling_task_executing_probe` (park, wait-entered, release,
wait-drain, allocation count, armed failure, wait-receiver) and the existing ExecuteTaskAck
probe; there are no sleeps or polling loops.

## C11 and Ada contract coverage

`native/tests/test_ir_scheduling_task_executing.c` (compiled as C11, one process per CTest case,
31 scenario cases plus the `ir_scheduling_task_executing_layout` probe) covers: all ten getters
with independent values; every meaningful and reserved Task value in both current and
previous task (opposite orders) and every TaskCompStatus; invalid current-task, previous-task
and status values (17, 18, high bits, `UINT32_MAX`; 5, 99) each alone, with neighbours
preserved and no snapshot allocated; signed duration `INT64_MIN`/`INT64_MAX`/negative/zero and
track ID `INT32_MIN`/`INT32_MAX`/negative/zero; high-bit identifiers; repeated reports and exact
FIFO; payload overwrite after callback return and ownership after every owner and the library
close; one-shot after success and every failure outcome; failed-owner ineligibility; invalid
arguments; synchronous delivery during registration; capacity-one queue-full vs. malformed
classification (null and each invalid enum while full) and allocation failure and recovery;
blocked Receive woken by delivery and by Scheduling Close; drain before stopped and before
provider failure; parent Session closed first; pending common requests during channel Close
(including uncertain deferred detach); late callbacks during provider destruction; in-flight
callback teardown drain; and common-request independence.

Two-family cases: `both-report-first` and `both-ack-first` (either registration order, separate
one-shots, counters and failpoints, independent owner closure), `both-sync` (both queues
active from synchronous registration delivery, drained independently after Scheduling and
Session Close), `both-close-wakes` (one blocked receiver per family, both woken by one Close),
`both-late` (late callbacks of both families during destruction), `both-in-flight` (one parked
callback per family; teardown waits for each drain in turn and releases the Control only after
both) and `both-detach-fail` (both owners fail, drain, then report PROVIDER_FAILED).

`ada/tests/src/ams_mel_ir_scheduling_task_executing_tests.adb` (smoke and focused main)
covers the representation check, the checked private conversion (reserved values verbatim,
sentinel and unknown values raise `Provider_Error`), synchronous delivery and fidelity for
every Task value 0..16 in both positions with signed extremes, named literals, payload
overwrite, post-closure ownership, all registration outcomes including distinct
`Not_Supported`, malformed/queue-full/allocation recovery, both families in either order with
independent closure, and detach-failure drain. It also asserts the nine-method negative control.

## Mutation evidence

`scripts/test_scheduling_task_executing_mutations.py` copies the native sources into an
isolated temporary tree, runs every focused scenario unmutated, then applies each of 22
mutants. Each must compile and fail a named C11 assertion (a compile failure never counts):
activity/previous-activity, previous-request/request, current/previous-task and
command/request swaps; duration truncation to 32 bits and conversion to microseconds; track
identifier sign/width loss; previous-activity truncation; omission of each of the three enum
checks; accepting the NumTaskTypes sentinel; rejecting reserved tasks; counting an invalid
enum as queue-full; allocating a full-queue snapshot; dropping the final queued event; allowing
a second registration; sharing the ExecuteTaskAck one-shot; omitting the TaskExecutingRep
callback drain; omitting the Scheduling Close stop; omitting the failed-detach notification;
and implicit foundation registration. Each mutant is restored; the restored tree is rebuilt and
all focused TaskExecutingRep, ExecuteTaskAck and foundation scenarios rerun. Checkout source
digests are verified unchanged.

## Coverage delta

Exactly `SchedulingChannel::registerMetadataCallback(TaskExecutingRep)`, `TaskExecutingRep`
(all ten getter mappings `represented`), `Task` and `TaskCompStatus` move `missing → safe_ada`
in `docs/irmel-ada-coverage.json` and the reviewed-binding record, with their limitations
(mock-provider only; reserved Task values non-operational; NumTaskTypes and unknown values
rejected; previous-task status describes only the previous task). Unique safe Ada operations
increase 48 → 49 (missing 18 → 17). `SchedulingChannel` remains `partial`; the nine other
Scheduling-specific operations remain `missing`. The reviewed-binding digest and the generated
`docs/irmel-ada-coverage.md` follow the established audit workflow. The upstream catalog, Git
evidence, vendored headers and their identities are unchanged (824 vendored files).

## Limitations

- Positive evidence is from the separate mock provider only; no real provider, hardware,
  credentials or Scheduling task execution was used.
- The callback is conditional (`@RequiredIfIRSTTrackTasking`); NotSupported raises
  `AMS.MEL.IR.Scheduling.Metadata.Not_Supported`.
- Upstream has no unregister: a closed owner leaves the provider holding an inactive callback.
- Reserved Task values are preserved but not supported, executable or interpreted.
- No Scheduling task execution, queue scheduling, priority calculation, request correlation,
  task cancellation, prioritized track list or other Scheduling metadata family is provided.
- No safe Python or Rust TaskExecutingRep reception; only private raw mirrors.
