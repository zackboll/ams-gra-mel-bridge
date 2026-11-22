# RUST-IR-023 — Safe Rust Scheduling AbortTask execution

This extends the [Scheduling foundation](rust-ir-022-scheduling-foundation.md)
with the actual upstream `SchedulingChannel::exec(AbortTask)` overload. The
starting `main` is `86e689864b24d330ab670ec8a211b1976d562338`, tree
`8270e2b70f6a53e0b9f89d80559e2b5ff255c7ce` (merge of PR #257). The change
does not overlap the paths in Ada Scheduling PR #258.

## Public API and mapping

`ams-mel-ir` and `ams-mel` export the same concrete `SchedulingTaskInfo` and
`TaskListAction` types. `SchedulingChannel::exec_abort(&mut self,
info: &SchedulingTaskInfo) -> Result<ReturnRequest, Error>` copies the owned
scalar values into the existing `AmsMelIrTaskInfoV1` and calls
`ams_mel_ir_scheduling_exec_abort`. The
[native implementation](irmel-ada-022-scheduling-abort-task.md) validates the
five public TaskInfo getters before constructing the C++ AbortTask.

| Rust field (same ABI field name) | Upstream setter | Exact representation |
| --- | --- | --- |
| `system_time_ns` | `setSystemTime` | Signed `i64` nanoseconds, including MIN, MAX, negative and zero; no epoch conversion |
| `request_id` | `setReqId` | Full `u32`; no inferred command ID or correlation |
| `action` | `setAction` | `Normal = 0`, `Next = 1`, `Immediate = 2`, `Abort = 3` |
| `precedence_within_priority` | `setPrecedenceWithinPriority` | Full `u32` |
| `priority` | `setPriority` | Full `u32`; no normalization or inversion |

The operation selects the AbortTask overload independently of the action. It
neither requires `Abort` nor rewrites any other action to `Abort`.

## Submission, completion and ownership

- Open returns Attached. Enable is explicit. Attached and Failed are refused by
  the native lifecycle, and a consumed Rust owner cannot enter the ABI. The
  unique mutable borrow serializes the operation with Close. The native adapter
  claims the Enabled channel and Session admission capacity before calling the
  provider without holding the lifecycle mutex. Rust adds no lock, thread,
  callback, retry or completion engine.
- `Ok(ReturnRequest)` means the bridge published an independent observer, not
  that the task completed successfully. `wait` returns `ReturnResult::Completed`
  with the actual `CommandReturn`: `Success`, `BadPointer`, `Fail`, `NotSupported`
  or `NotImplemented`. Those are payloads, not submission errors. A MEL
  rejection is instead `ReturnResult::Rejected { code, description }`.
  `Unsupported` and `InvalidState` are distinct; the shared Rust decoder
  preserves unrecognized numeric **ABI** codes as `MelErrorCode::Unknown(raw)`.
  The current native engine diagnoses an unknown **upstream** MEL code as
  `ProviderFailed` before it reaches that Rust decoder. No native policy changed.
- `wait(0)` polls. Finite timeout neither consumes the request nor cancels work.
  Terminal waits repeat the cached outcome; a longer diagnostic is retrieved
  with a checked cached poll, not by submitting again. Valid rejection text is
  returned in full. The native engine replaces invalid upstream rejection text
  with a diagnostic; Rust rejects invalid UTF-8, embedded NUL, unterminated text
  and inconsistent records when presented at its ABI boundary.
- The native completion claim retains the needed Scheduling, Session and
  provider DSO state after public channel, Session and weak-view closure.
  Channel Close can defer physical disable/detach. Request Close/Drop can
  abandon observation but cannot cancel admitted work. Native code keeps the
  provider loaded until cleanup is safe; work that never finishes or uncertain
  cleanup need not permit unload.
- There is one native submission call per invocation and at most one provider
  dispatch. A Rust `ReturnRequest` guard is in place before the call. Success
  needs both a valid success diagnostic record and a non-null owner. On failure
  or malformed success, a non-null published handle is offered once to the
  existing best-effort Close and never returned to the caller. Null-success is
  a protocol error. A post-send error can have no public handle while the native
  worker owns the future; do not treat it as proof of non-execution or replay it.
  Oversized submission failure diagnostics retain the native error category and
  required size without resubmission. Exceptions and null/unknown Return
  payloads use the existing `Error` paths.

No task or metadata registration is implicit. ExecuteTaskAck, TaskExecutingRep,
SchedCreatedRep and other Scheduling metadata are not exposed or consumed by
this operation. An actual acknowledgement cannot complete its Return future.

## Safe executable example

The [example](../rust/ams-mel-ir/examples/scheduling_abort.rs) discovers a
Scheduling capability, attaches and enables, submits once, closes its parents
and distinguishes a provider return from rejection or timeout. Run against
the existing mock from the repository root:

```sh
make test-native
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
export LD_LIBRARY_PATH="$AMS_MEL_NATIVE_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cargo run --locked --manifest-path rust/Cargo.toml -p ams-mel-ir \
  --example scheduling_abort -- "$AMS_MEL_TEST_PROVIDER_DIR/libmock_ir_provider.so" \
  sched-abort-success 4045620583 -9223372036854775808 4294967295 2147483649
```

## Test evidence and limits

Development runs of `cargo test --locked --manifest-path rust/Cargo.toml
-p ams-mel-ir --lib ir_scheduling` and `cargo test --locked --manifest-path
rust/Cargo.toml -p ams-mel --test ir_scheduling` passed 8 private tests and
33 compiled-mock scenarios (17 AbortTask cases; one ignored child entry run
by the parent scenarios). The example above printed `Provider returned Success`.
Exact committed-source identities, gate counts and hosted status are reported
in the delivery PR separately from these development runs.

Private tests check all five fields, malformed submission diagnostics and
contradictory ownership with real request handles, and the shared Wait boundary
for `BadPointer` and unrecognized ABI MEL error codes. A private compiled-mock
test explicitly opens a raw acknowledgement subscription solely to prove that
delivery on the same channel, with the same request ID, leaves the future
pending. The safe example and wrapper never make that subscription.

Subprocess cases use the unchanged mock and deterministic promise release to
check completion, capacity, reentry via a separate weak view of the same
channel, and cleanup with/without a public request. Dispatch and registration
counters and the lifetime log check exact-once execution and disable, detach,
destruction and eventual unload. Pending cases remove all test DSO pins before
checking retention, re-pin only to call the fixture release function, and wait
for the final-owner notification before asserting safe unload. The existing
historically named post-send allocation/worker-launch failpoints simulate failed
public handle publication, not a failed worker start in the current engine.

`AbortTask` has no public getter for its embedded TaskInfo. Rust checks the
conversion at the ABI boundary; the existing native tests verify native
TaskInfo through its published getters. Mock dispatch does not establish a
provider-side round trip of inaccessible fields. This is partial Rust Scheduling
coverage, not full parity or evidence of real-provider or physical task
execution. StareTask, ScanTask, TrackTask, PrioritizedTrackListCmd and Scheduling
metadata reception remain outside this safe Rust slice.
