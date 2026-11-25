# RUST-IR-024 — Safe Rust Scheduling StareTask execution

This adds `SchedulingChannel::exec(IRSTStareTask)` to the partial safe Rust
Scheduling binding, following [RUST-IR-022](rust-ir-022-scheduling-foundation.md)
and [RUST-IR-023](rust-ir-023-scheduling-abort.md). It uses the existing
[native StareTask operation](irmel-ada-023-scheduling-stare-task.md). There is
no change to the C ABI, provider, native completion engine, Ada or Python API,
or authoritative Ada coverage inventory. ScanTask, TrackTask,
PrioritizedTrackListCmd, Scheduling metadata reception, and scheduling policy
are not implemented by this change.

## API and representation

`ams-mel-ir` and `ams-mel` export the same two concrete owned values:

```rust
pub struct SchedulingStareParameters {
    pub center_azimuth_rad: f64,
    pub center_elevation_rad: f64,
    pub elevation_frame: CoordinateFrameReference,
    pub azimuth_frame: CoordinateFrameReference,
    pub duration_ns: i64,
}

pub struct SchedulingStareTask {
    pub command_id: u32,
    pub parameters: SchedulingStareParameters,
    pub task_info: SchedulingTaskInfo,
}

// Method on SchedulingChannel:
// pub fn exec_stare(&mut self, task: &SchedulingStareTask) -> Result<ReturnRequest, Error>
```

The call passes one `AmsMelIrStareTaskV1` to
`ams_mel_ir_scheduling_exec_stare`. The native LP64 record is 64 bytes,
aligned to 8:

| Field | Representation | Offset |
| --- | --- | ---: |
| `center_azimuth_rad` | Binary64 radians | 0 |
| `center_elevation_rad` | Binary64 radians | 8 |
| `duration_ns` | Signed 64-bit nanoseconds | 16 |
| `elevation_frame` | Independent `u32`: Inertial=0, Aircraft=1 | 24 |
| `azimuth_frame` | Independent `u32`: Inertial=0, Aircraft=1 | 28 |
| `command_id` | Full `u32` | 32 |
| `task_info` | Unchanged `AmsMelIrTaskInfoV1` | 40 |

Angles pass unchanged, including signed zero. Duration passes unchanged,
including negative, zero, minimum and maximum values. Rust does not convert
units or frames, infer a system-time epoch, correlate command and request IDs,
or change priority, precedence, or any action (Normal, Next, Immediate,
Abort). Native validation remains authoritative. There are no extra Rust
pointing or scheduling restrictions. The input is borrowed only for submission.

## Submission, completion and ownership

Open leaves the channel Attached; callers must explicitly Enable. Attached,
Failed and Closed channels do not dispatch. Each live-owner submission makes
one native call, with no automatic retry. The shared AbortTask/StareTask guard
initializes the request slot to null and publishes a Rust request only after
well-formed success with a non-null owner. Contradictory output yields an error;
an unexpected published owner gets at most one best-effort cleanup, not adoption
or replay. An error after dispatch is not evidence that the provider did no work.

On successful submission, the existing `ReturnRequest` survives public channel,
Session and weak-view closure. `wait(0)` polls, timeout does not cancel, and
subsequent terminal waits read the cached result. `ReturnResult::Completed`
preserves `Success`, `BadPointer`, `Fail`, `NotSupported` and `NotImplemented`.
`ReturnResult::Rejected` is an explicit MEL error, not a completed Return value;
it carries the MEL code and complete UTF-8 description. Provider exceptions,
invalid futures, malformed results and cleanup failures are errors, not these
outcomes.

Request Close/Drop drops only the observer. The admitted native completion
graph retains its Session permit and provider code until it can retire safely.
Public Scheduling Close stops admission and may defer disable/detach. A
deferred detach failure can report a completion error and retain the uncertain
graph and provider DSO. Rust adds no callback, thread, queue, or lock around
provider dispatch.

No Scheduling metadata (including ExecuteTaskAck, TaskExecutingRep,
SchedCreatedRep and TaskEvents) is registered, delivered or fabricated by this
operation. Even an actual event with matching command and request IDs cannot
complete the Return future.

## Safe example

The [example](../rust/ams-mel-ir/examples/scheduling_stare.rs) discovers and
attaches Scheduling, enables it, submits a complete mock task once, closes
the channel and Session, and inspects the result before closing the request.
Run from the repository root:

```sh
make test-native
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
cargo run --locked --manifest-path rust/Cargo.toml -p ams-mel-ir \
  --example scheduling_stare -- \
  "$AMS_MEL_TEST_PROVIDER_DIR/libmock_ir_provider.so" sched-stare-success
```

**Command completion does not verify physical staring, pointing accuracy or
image acquisition.** No real provider, hardware, or non-LP64 targets were
qualified by the mock tests.

## Qualification

Preflight started at `6ef2cfcb9d1e01bf3100dcca90c4d9c588009551` (merged
PR #260); the branch fast-forwarded to
`eab6a8004069933678357adc340a2066eb7f3518` (disjoint Python PR #259)
before final qualification.

The private tests inject contradictory output, invalid discriminators, null
successful output and Return results not emitted by the Stare mock. The
existing Stare scenarios provide a compiled provider round trip, including
long rejections (ASCII); the common Return boundary tests separately verify
`BadPointer` and multibyte UTF-8 without truncation. The same-channel matching
ACK is deliberately registered and delivered only in a private boundary test.
Subprocess lifetime tests use the provider promise release and the native
final-owner notification, not sleeps. Both historical post-send failpoints
now represent failed public request publication after a valid admission.

The initial focused development runs of
`cargo test --locked -p ams-mel-ir --lib ir_scheduling` and
`cargo test --locked -p ams-mel --test ir_scheduling` (from `rust/` with the
test-library and mock paths set above) passed 12 private tests and 54
subprocess scenarios, respectively. `make test-native` passed 1009/1009.
These development runs are not final-commit qualification.

The delivery PR records exact commit/tree identities, tool versions, commands,
exit statuses, matrix and repetition counts, and hosted status. An intermediate
workspace test attempt exited 101 with undefined native symbols when run
alongside `make check`, whose build-isolation regression rebuilds the same
native test tree. That run does not count as qualification; Rust final runs
use stable, separate build trees.

The standalone final-newline gate also rejected a pre-delivery draft of this
document (exit 1); the draft was corrected before rerunning the final gates.
