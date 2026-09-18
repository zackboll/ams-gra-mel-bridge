# Task 016 validation — safe Python IR C2 BIT no-op

## Scope and API

Task 016 exposes Task 014's unchanged asynchronous BIT no-op C ABI through the
dependency-free safe Python API. The private `ctypes` layer now declares all 19
current façade functions and retains explicit fixed signatures checked against
the C11 header by a compiled drift probe.

`ControlChannel.submit_bit_noop(command_id)` validates the exact uint32 command
ID and returns a uniquely owned `ReturnRequest`. `CommandReturn` represents all
five published values: `SUCCESS`, `BAD_POINTER`, `FAIL`, `NOT_SUPPORTED`, and
`NOT_IMPLEMENTED`. `ReturnCompleted` is distinct from `ReturnRejected`; in
particular, `ReturnCompleted(CommandReturn.FAIL)` is normal request completion,
not `MelError`. Unknown successful Return values are provider failures, while
unknown MEL rejection codes remain dynamically named `MelErrorCode` values with
their exact uint32 identity and complete description.

`ReturnRequest` retains no Python `Session` or `ControlChannel` reference. The
native graph retains provider/library state after those Python parents close and
are collected. Timeout does not consume or cancel; close/drop is idempotent,
nonblocking, and does not cancel pending provider work. Terminal results are
cached and repeatable. Diagnostics larger than the initial 512-byte buffer are
retrieved by one zero-time cached wait and accepted only when status, Return
value, error code, and exact required size remain identical. An oversized
timeout diagnostic never triggers a semantic second wait. Wait and close on one
request must be externally serialized; the GIL is not a substitute.

BIT remains limited to the empty initiate/cancel/clear-fault profile accepted by
the pinned provider. Payload-bearing BIT, BIT metadata/CommandStatus callbacks,
other C2 commands, and full IR MEL coverage remain unsupported.

## Mock-provider and ABI evidence

`make test-python` passed 52 Python unittests. Coverage includes explicit enable,
high-bit and boundary command IDs, Success and cached completion, normal
`Return::Fail`, timeout followed by completion after Python parent collection,
complete long UTF-8 rejection and cached rejection, unknown rejection-code
preservation, null results, future/send exceptions, unknown Return values,
malformed request publication cleanup, direct construction rejection, invalid
timeout/command inputs, terminal-retry consistency, and pending-request close
with completion before C2 destruction/provider unload.

The same gate compiled and ran the warnings-as-errors C11 ABI probe. It verifies
all 19 bound symbols, all five Return constants, exact Return-result size,
alignment and offsets, and the three BIT/Return-request signatures. Warning-
enabled Python compileall also passed.

## Real Squall evidence

The pinned Squall checkout and all six pinned dependency checkouts were exact and
clean. The Python-only accepted run used control/Couloir-metrics/optical-health/
optical-metrics ports `47095/58917/41799/57803`. It reported provider API 1,
library 1, vendor `Squall`, description `Squall Simulator IR MEL`, BIT Success,
TaskSched, both cached success lines, frame IDs 0..2 at 320x200 Mono8 and 64,000
bytes, counters 3/0/0, explicit request/control/stream teardown, and both the
Python-client and mode=python PASS lines. Exit status was zero.

The final accepted all-language run used ports
`51147/41003/35357/48879` and one iteration. C, Ada, Rust, and Python each
reported BIT Success, TaskSched, frame IDs 0..2 at 320x200 Mono8/64,000 bytes,
counters 3/0/0, and their language PASS line. Rust and Python repeated cached BIT
and mode waits after parent-first Session close. The combined result was
`PASS: Squall IR MEL integration (1 iteration(s), mode=all)` with exit status
zero. This establishes parity only for Session + Mono8 + Operate/TaskSched + BIT
no-op; it is not payload-bearing BIT or full MEL coverage.

Both runs used Squall `b1015728f904c799fa0c07489fce48e78f67845f`, Podman
5.4.2 with `podman-compose`, and provider compiler comment
`GCC: (GNU) 14.3.1 20251022 (Red Hat 14.3.1-4)`.

## Regression gates

- `make test-rust`: passed 31 safe tests, one raw C-header ABI drift test, and
  doc tests; native prerequisite tests were 6/6.
- `make test-native`: passed 6/6.
- Plain `make check`: native 6/6 passed, then the command stopped because
  GNAT/GPRbuild was not on ordinary shell `PATH`.
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check`: passed native 6/6,
  all Ada provider/image/C2 suites, newline audit, and whitespace checks.
- `git diff --check`: passed.

Native, Ada, Rust, and vendored sources were unchanged. The external Squall root
and pinned dependencies remained clean after validation.
