# Task 014 validation — IR C2 BIT no-op foundation

## Scope

Task 014 exposes required upstream `BIT_Command` only in the profile implemented
by pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f`: an exact `uint32_t`
command ID with empty `initiateBIT_ID`, `cancelBIT_ID`, and `clearFaultCode`
vectors. Non-empty payloads are rejected by that provider, so the C operation is
named `ams_mel_ir_c2_submit_bit_noop` and accepts no ignored payload arguments.

C and Ada support this profile. `ams-mel-sys` declares the complete expanded raw
C ABI, but the safe Rust crate has no BIT API. Python's intentional 16-function
private subset and safe API also have no BIT support. Payload-bearing BIT,
metadata/CommandStatus callbacks, ConfigSet, scan/camera/RF commands, and Task 015
are outside this task.

## ABI and asynchronous contract

The experimental façade remains ABI 0.1. Three exports increase the C ABI from
16 to 19 functions: BIT no-op submit plus generic Return-request wait and close.
`ams_mel_ir_return_result_v1` contains fixed-width upstream Return and MEL error
values. Success, BadPointer, Fail, NotSupported, and NotImplemented map to 0..4.
Unknown upstream values are provider failures.

A successful `ErrorOr` is `AMS_MEL_OK` for every known Return value. In
particular, upstream `Return::Fail` is the normal inspectable pair
`AMS_MEL_OK`/`AMS_MEL_IR_RETURN_FAIL`; it is not `AMS_MEL_PROVIDER_FAILED`.
`ErrorOr(Error)` is `AMS_MEL_COMMAND_REJECTED` with complete cached diagnostics.
Provider exceptions, null successful pointers, unknown values, and façade
failures remain distinct.

The generic request owner matches ModeRequest: all façade ownership is prepared
before send, submission is asynchronous, timeout does not cancel or consume,
terminal results are cached, only one future `get()` occurs, and public close is
idempotent/nonblocking. Pending work retains C2, Session/provider, and library;
Session or C2 may close first. Deferred teardown follows final completion.
Deterministic post-send allocation/worker failures test allocation-free emergency
retention rather than unsafe provider unload.

## Validation

Mock-provider native tests cover explicit enable, high-bit command ID, empty BIT
payloads, Success, ordinary Return::Fail, timeout/repeated wait, Session/C2-first
close, pending public request close, long rejection diagnostics, null result,
future/send exceptions, unknown Return, idempotent close, unload order, and both
post-send failpoints. Ada covers success/high-bit ID, Return::Fail, timeout after
parent/channel close, complete long rejection, close, and finalization. Rust's C
probe checks new constants, layout, and signatures; no safe API was added.

## Executed results

- `make test-native`: 6/6 CTest targets passed (Debug); a separate Release build
  also passed 6/6.
- `make test-rust`: 25 safe Rust tests plus one raw ABI drift test passed; doc
  tests passed. No safe BIT API exists.
- `make test-python`: 41 tests passed; the private subset remains 16 functions.
- `alr -C ada build` and `alr -C ada/tests run`: passed, including Mono8,
  Operate/TaskSched, and BIT no-op contracts.
- Plain `make check` could not find GNAT/GPRbuild on ordinary `PATH`, exactly as
  documented by the repository. `alr -C ada exec -- make -C
  /home/zboll/git/ams-mel check` passed all native/Ada/newline/whitespace gates.
- `nm` and the version script report exactly 19 versioned `ams_mel_*` exports.
- The Squall checkout remained clean at
  `b1015728f904c799fa0c07489fce48e78f67845f`.

The final accepted command used explicit free Task-owned ports:

```sh
AMS_MEL_SQUALL_REPEAT=1 \
AMS_MEL_SQUALL_CONTROL_PORT=46473 \
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=57239 \
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=56307 \
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=43219 \
SQUALL_SOURCE_DIR=/home/zboll/git/squall \
  make test-squall-ir
```

It passed one complete iteration. C and Ada each printed `BIT result: SUCCESS`,
`C2 result: TASK_SCHED`, and three real 320x200 Mono8 frames (64,000 bytes,
zero malformed). Rust and Python each retained their existing TaskSched plus
three-real-frame success. This is not BIT language parity.
