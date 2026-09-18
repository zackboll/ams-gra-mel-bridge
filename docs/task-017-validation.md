# Task 017 validation — required IR C2 commands for Ada

## Scope

Task 017 completes the three published required C2 sends in the native façade
and safe Ada binding: general ModeCmd with complete ScanParam, full raw BIT
transport with safe Ada one-choice operations, and ConfigSet. Existing Operate,
BIT no-op, ModeRequest, and ReturnRequest contracts remain supported. The ABI
remains 0.1 and contains 22 exported functions.

Rust sys and private Python ctypes declarations are mechanically synchronized.
Safe Rust and public Python intentionally gain no general Mode, BIT-payload, or
ConfigSet API. No callbacks, optional C2 commands, or other MEL families are in
scope.

## Provider limitations

Pinned Squall returns normal `Return::Fail` for nonempty BIT and ConfigSet
payloads. It rejects Operate scan modes because its implementation requires
TaskSched. Mock-provider tests establish complete payload and ScanParam fidelity;
these Squall outcomes demonstrate provider reachability, not adapter limitations.

## Validation

- `make test-native`: 6/6 CTest targets passed.
- `alr -C ada build` and `alr -C ada/tests run`: passed. The Ada executable
  reports three contract groups: image, required C2 commands, and Session.
- `make test-rust`: 31 safe tests plus one raw ABI drift test passed; doc tests
  passed. No safe Rust command API was added.
- `make test-python`: 52 tests passed, including the 22-function ctypes drift
  probe. No public Python command API was added.
- GCC 14.2 Debug and Release each passed 6/6 targets, 50 repeated runs per
  target, and installation. Clang was unavailable locally and is covered by CI.
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check` passed native,
  Ada, whitespace, and newline gates. Plain `make check` passed native 6/6 and
  then stopped because GNAT/GPRbuild is absent from ordinary `PATH`, as expected.
- `nm` and the version script report exactly 22 `AMS_MEL_0.1` exports.

The final all-language Squall run used repeat count one and explicit ports
37649/42701/36475/37707. C, Rust, and Python retained BIT-no-op + TaskSched +
three Mono8 frames. Ada additionally observed Standby/Unused success,
Operate/ScanVolumeSched rejection with Invalid Parameters, empty ConfigSet
Success, nonempty ConfigSet Fail, and payload-bearing BIT Fail before its BIT
no-op, TaskSched, three 320x200 Mono8 frames, and zero malformed frames. The
Squall checkout and all pinned dependency checkouts remained clean.
