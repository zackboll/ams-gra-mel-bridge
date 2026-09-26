# Corrective C2 unlocked request submission

Starting `origin/main`: `ff62a09233a92661bcb575855e1f663660c98542`.
Branch: `corrective/c2-unlocked-request-submission`.
The parked Task 031B branch is unchanged; this corrective adds no admission.

## Ordering and ownership

Previously Mode, Return (BIT/ConfigSet/KeepAlive), and CommsTest submissions
locked `ChannelState::mutex`, validated lifecycle, called provider send, armed
the WorkerInput, incremented requests, then unlocked and launched the worker.
This was a source-level discrepancy with the other RequestFor families, not
a demonstrated user-visible deadlock. Simply unlocking before send would
leave requests zero and allow Close to physically detach an in-flight send.

All three engines now preallocate bridge owners and copy a strong ChannelState.
One shared `claim_c2_submission` helper locks the lifecycle mutex, validates,
copies a strong C2Channel, increments requests, and unlocks before provider code.
Mode and enabled Return operations still require Enabled. Inherited Return and
CommsTest still accept Attached or Enabled. No new lock replaces the old lock
across provider send or synchronous callbacks.

The claim is the linearization point: a successful claim wins over subsequent
Close, which logically closes but defers physical cleanup. If Close wins the
lifecycle lock first, the claim fails without provider send. This is an internal
lifecycle rule, not a promise that arbitrary concurrent access to a public
wrapper is safe. After claim, submission never dereferences that wrapper again.

On synchronous send exception, the temporary channel copy is dropped and
`finish_channel(state)` releases exactly the reserved request. A final request
on a Closed channel performs deferred cleanup; otherwise the channel remains
usable. The existing ProviderException status is preserved. On success, the
temporary channel copy is dropped before worker launch: cleanup must not wait
for a channel destructor still held by the submitting stack. The strong state
owner keeps Session/provider code alive through local unwinding.

After a future exists, allocation/launch failure still permanently retains the
exact WorkerInput and future. No submit-side decrement occurs on this path.
Healthy completion remains worker-per-future, exactly one get, with cached
results and existing parent-first ownership. No polling, queue, executor,
options, admission state, or new public status is introduced.

## Deterministic evidence

`native/tests/test_c2_submission.c` is compiled as C11 with assertions active in
Release. A mock-only per-engine condition-variable barrier parks inside send
before a future is returned. A test-facade-only probe uses `try_lock` and reads
requests under the lifecycle mutex. Each engine proves unlocked send and one
reserved request. The old lock-held implementation cannot satisfy that probe.

For Mode, Return/KeepAlive, and CommsTest separately:

- Close after barrier entry leaves one request and no physical destruction.
- Success returns a held future; explicit release produces exactly one get,
  final requests zero, and exactly one channel destruction.
- Synchronous throw after Close returns ProviderException and a null request,
  reaches zero accounting without launching a worker, and cleans up once.
- Throw while open returns to zero; a subsequent normal send succeeds.
- Success and throw after Close also run after dropping the observation token
  before releasing send, so the token cannot mask missing submission ownership.
- Channel, Control, manager, and library destruction occur once in that order
  after all appropriate owners (including the test's DSO handle) are released.

Six separate CTest processes cover allocation and worker-launch failure for
each engine. They require one retained request, no public request, no worker/get,
and no provider graph destruction even after observation and DSO owners close.
Those deliberately unconsumed futures are never released/retried by the test.
Existing C2 post-send failpoint tests are retained unchanged.

Existing synchronous Mode CommandStatus, BIT metadata, and Comms reply callback
coverage remains in `test_ir_c2.c`. KeepAlive has no mock synchronous metadata
emission. The unlocked call site also applies to all Return command senders;
no metadata redesign or new public callback contract is claimed.

Build-tree isolation now compares the test count captured before production
build with the count afterward, rather than freezing the previous 20 tests.

## Public-surface audit

ABI remains 0.1 and production exports remain 90. The production export map,
public C header, Ada, Rust, Python, vendor files, and provenance are unchanged.
Only the generated test export map includes the private observation hook.
All positive evidence uses the mock provider, not real Squall.

## Validation

GCC 14.2 Debug and Release full CTest each passed 27/27. The seven new CTest
cases also passed 50 repetitions with parallelism four. An out-of-tree mutation
restoring a lock across provider send failed at the direct try_lock assertion;
its blocked mock subsequently aborted on the bounded barrier watchdog. This
mutation is not present in the repository.

Sequential local gates passed: `make test-native`, `make test-build-isolation`,
`make check-ada-format`, `alr -C ada build`, `alr -C ada/tests run`,
`make test-rust`, `make test-python` (52 tests),
`python3 scripts/check_final_newlines.py`, and `git diff --check`.
`make check` **failed** because bare GNAT/GPRbuild is unavailable on PATH, after
its native/isolation/format checks. Passing Alire gates do not make direct GPR
pass. No Ada sources changed and the formatter was not run in write mode.
Local Clang is unavailable; hosted exact-head CI must provide both Clang
configurations and direct-GPR evidence before independent review.

The built production library has exactly 90 `ams_mel_` exports and no test hook.
Source comparison against the starting SHA shows zero changes to public headers,
production exports.map, Ada, Rust, Python, vendor, or upstream provenance files.
The parked Task 031B branch still points to the starting SHA.
