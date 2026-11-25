# RUST-IR-025 — Safe Rust Scheduling ScanTask execution

Safe Rust now exposes the upstream `SchedulingChannel::exec(IRSTScanTask)`
overload through the [existing native ScanTask ABI](irmel-ada-024-scheduling-scan-task.md).
This extends the Scheduling foundation, AbortTask and StareTask bindings without
changing native code, Ada or Python APIs, or the authoritative Ada coverage
inventory. Rust TrackTask, PrioritizedTrackListCmd, Scheduling metadata streams,
scheduling policy, and hardware integration are outside this change.

## API and ABI

`ams_mel_ir` and `ams_mel` export the same owned `SchedulingScanTask` value:

```rust
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SchedulingScanTask {
    pub command_id: u32,
    pub parameters: ScanParameters,
    pub task_info: SchedulingTaskInfo,
}

// On SchedulingChannel:
// pub fn exec_scan(&mut self, task: &SchedulingScanTask) -> Result<ReturnRequest, Error>
```

Scheduling and [C2 ModeCommand](rust-ir-009-general-mode-command.md) reuse the
same public `ScanParameters`, `ScanType`, `CoordinateFrameReference`, and
`DegradationMethod`, and the same private `AmsMelIrScanParamV1` conversion.
C2 defaults, mode validation, errors and public API are unchanged. Scheduling
uses the existing TaskInfo conversion; all actions and both IDs pass through
independently, with no inferred relation among command, request and scan IDs.

`ams_mel_ir_scheduling_exec_scan` synchronously consumes the input record.
The existing `AmsMelIrScanTaskV1` record is 144 bytes, aligned to 8 on LP64:

| Field | ABI representation | Offset |
| --- | --- | ---: |
| `scan_parameters` | Existing 112-byte `AmsMelIrScanParamV1` | 0 |
| `task_info` | Existing 24-byte `AmsMelIrTaskInfoV1` | 112 |
| `command_id` | Full `u32` | 136 |

All fields pass through unchanged, including inactive-but-published range and
altitude, signed nanoseconds, independent frames, and the three full-width
unsigned scan controls. Rust adds no unit or epoch conversion, clamping,
normalization, feasibility checks, or operational policy. Native validation
remains authoritative. Synthetic round trips compare every published getter,
including binary64 bits for signed zero, infinity, and a quiet NaN fixture.

## Submission and completion

Open attaches without enabling. Call `enable` explicitly before `exec_scan`.
Each submission on a live handle makes one native attempt; Attached, Failed
and Closed channels do not dispatch. Native admission can reject a request
before dispatch. The shared `submit_with` starts with a null observer slot.
It publishes a Rust observer only on well-formed success with a non-null
owner. A contradictory non-null output on error is dropped through the shared
best-effort cleanup path, not adopted or replayed. Submission errors can occur
after dispatch, notably if native request publication fails. Neither Rust nor
the observer retries, replays, or cancels the operation.

The existing `ReturnRequest` survives channel and Session closure. `wait(0)`
polls, a timeout does not cancel, and terminal results are cached.
`ReturnResult::Completed` preserves `Success`, `BadPointer`, `Fail`,
`NotSupported`, and `NotImplemented`. An explicit MEL rejection is instead
`ReturnResult::Rejected`, with its code and complete description. Native
validation and provider errors, invalid futures, malformed results or
diagnostics, and deferred cleanup failures are errors, not Return values.

Closing or dropping the observer does not retire admitted work. The native
completion graph holds its Session permit and retains channel and provider
code as needed. Public channel closure stops admission and can defer disable
and detach. After retirement the provider can unload if ownership is clear;
a deferred detach failure can report a terminal error and conservatively
retain the uncertain graph. No new completion worker, callback, or request
owner is introduced. No Scheduling metadata is registered or synthesized;
even a matching acknowledgement cannot complete a pending ScanTask request.

## Mock-only example

From the repository root, after `make test-native` completes:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
cargo run --locked --manifest-path rust/Cargo.toml -p ams-mel-ir --example scheduling_scan
```

The [example](../rust/ams-mel-ir/examples/scheduling_scan.rs) uses only safe
public Rust APIs and the compiled mock's `sched-scan-success` scenario. It
submits once and closes the channel and Session before waiting on the request.

**Successful provider completion does not establish physical scanning, image
acquisition, or sensor performance.** No real provider, hardware, or non-LP64
platform is qualified by these tests. The Scan mock emits four completed Return
values; the existing private shared Return boundary additionally covers
`BadPointer` and long multibyte UTF-8 rejection text. The Scan mock's long
rejection text is ASCII. Private tests inject invalid raw discriminators,
contradictory output, and a null success owner; these are not values a safe
caller can construct.

## Qualification evidence

Preflight and reconciled base: `e32457e1db65f92914bcee67bfbe37cb1282ce57`
(merged PR #262), tree `ddbc3f3900aca6b2ee2bdebecc231f94ff11e0c6`.
No open PR or overlapping unmerged remote branch was found. At baseline,
locked Rust `ir_scheduling` tests passed 12 private and 54 compiled-mock
scenarios; `ir_mode` passed 7 private and 9 compiled-mock tests. The
extracted field initializer matches the baseline, ignoring whitespace and a
trailing call-argument comma. Strict C11 probes compiled by GCC and Clang
both printed `144 8 0 112 136`. Independently built baseline and feature
production DSOs each expose exactly 356 `AMS_MEL_0.1` symbols, matching
`native/src/exports.map`; the sorted versioned-name inventories share SHA-256
`153e87083d51d3ce0d304d2627c4c757f3338360c797efc9d626e12d208b97ed`.
The feature-attributable production ABI delta is zero.

The delivery PR records the exact final Git identities, commands, tool
versions, counts, and local and hosted qualification outcomes. The focused
Rust tests use compiled mock controls and final-owner notifications instead
of sleeps; the regression suite includes the existing C11 layout probe,
native ScanTask lifetime tests, and AbortTask, StareTask, Scheduling foundation,
and C2 ModeCommand coverage.
