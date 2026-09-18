# Task 015 validation — safe Rust IR C2 BIT no-op

## Scope and API

Task 015 exposes Task 014's unchanged asynchronous BIT no-op C ABI through the
safe `ams-mel` crate. `ControlChannel::submit_bit_noop(u32)` sends only the
pinned-provider-compatible profile with empty initiate, cancel, and clear-fault
lists and returns a uniquely owned `ReturnRequest`. Payload-bearing BIT remains
unsupported, and Python still has no BIT API.

`CommandReturn` represents all five published IR `Return` values: Success,
BadPointer, Fail, NotSupported, and NotImplemented. `ReturnResult` separates a
normal completed Return value from an upstream MEL rejection with a preserved
`MelErrorCode` and complete UTF-8 description. In particular,
`ReturnResult::Completed { value: CommandReturn::Fail }` is a successful request
completion, not `Err(ErrorKind::ProviderFailed)`. Unknown Return values violate
the native contract and map to `ProviderFailed`; unknown rejection codes remain
`MelErrorCode::Unknown(u32)`.

`ReturnRequest` is neither `Send` nor `Sync`, retains no Rust Session or
ControlChannel, and relies on the native ownership graph to retain provider code.
Finite timeout does not consume or cancel a request. Terminal results are cached
and repeatable. Oversized terminal diagnostics are fetched exactly once more with
timeout zero and accepted only if status, raw result, and required size are
unchanged. Consuming close and RAII drop are nonblocking and do not cancel pending
provider work.

## Mock-provider evidence

The safe Rust tests cover explicit enable and high-bit command ID preservation,
Success and cached wait, ordinary Return::Fail completion, timeout followed by
completion after parent Session and ControlChannel closure, complete long UTF-8
rejection and cached rejection, null result, future exception, unknown Return,
submission exception with no request, and non-cancelling pending-request drop
with completion before C2 destruction and provider unload. A crate unit test maps
all five known Return values and rejects an unknown numeric value.

## Validation results

The following ordinary gates passed:

- `make test-rust`: native 6/6 plus 31 safe `ams-mel` tests and the raw sys ABI
  drift test; all doc tests passed;
- `make test-python`: 41 tests;
- `make test-native`: 6/6 CTest targets;
- workspace `cargo check`, `cargo test`, Clippy with `-D warnings`, and rustfmt
  check;
- standalone Squall Rust-client locked/offline check, Clippy, and rustfmt check;
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check`: native 6/6,
  all Ada provider/image/C2 suites, newline audit, and whitespace checks; and
- `git diff --check`.

The raw sys crate remains unchanged at the complete current 19-function ABI.
Native production/header/tests, Ada, Python, and vendored source are unchanged.

## Real Squall evidence

The safe Rust integration uses BIT command `0x00400402` and mode command
`0x00400403`. It requires BIT Success and TaskSched within 5000 ms, closes the
parent Session, repeats zero-time cached waits for both request types, receives
three real 320x200 Mono8 frames, validates counters, and explicitly closes both
requests, C2, and image resources.

The Rust-only bring-up passed with exit status zero using:

```sh
AMS_MEL_SQUALL_CONTROL_PORT=47105 \
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=35807 \
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=49195 \
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=41449 \
SQUALL_SOURCE_DIR=/home/zboll/git/squall \
  make test-squall-ir-rust
```

It printed BIT Success, TaskSched, both cached success lines, three frames with
IDs 0..2 at 320x200 Mono8/64,000 bytes, counters received=3/dropped=0/malformed=0,
and `PASS: real Squall IR Rust integration`.

The final accepted all-language run passed with exit status zero using:

```sh
AMS_MEL_SQUALL_REPEAT=1 \
AMS_MEL_SQUALL_CONTROL_PORT=58587 \
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=38697 \
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=59867 \
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=52189 \
SQUALL_SOURCE_DIR=/home/zboll/git/squall \
  make test-squall-ir
```

The generated profile used `control_address=127.0.0.1:58587`,
`data_host=127.0.0.1`, and client ID `ams-mel-task004-1533890`. The verified
Squall revision was `b1015728f904c799fa0c07489fce48e78f67845f`; the extracted
provider compiler comment was `GCC: (GNU) 14.3.1 20251022 (Red Hat 14.3.1-4)`.
The provider reported API 1, library 1, vendor `Squall`, and description
`Squall Simulator IR MEL`. Podman 5.4.2 with `podman-compose` provided the
runtime.

C, Ada, and Rust each reported BIT Success, TaskSched, three frames with IDs
0..2 at 320x200 Mono8/64,000 bytes, and counters 3/0/0. Python reported
TaskSched, its cached result, the same three frames, and counters 3/0/0. The
combined result was `PASS: Squall IR MEL integration (1 iteration(s), mode=all)`.
The harness removed its Task-owned containers and image tags. The root Squall
checkout and all six pinned dependency checkouts remained clean at their exact
documented revisions.

The intended final language coverage is C/Ada/Rust BIT no-op + TaskSched + Mono8,
and Python TaskSched + Mono8. This is not complete BIT language parity. Python
BIT and payload-bearing BIT remain unsupported.
