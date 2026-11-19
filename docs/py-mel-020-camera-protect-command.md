# PY-MEL-020 — Safe Python C2 CameraProtectCmd submission

## Authority and scope

Prepared in `/home/zboll/git/ams-py-mel-020` on branch
`feature/py-mel-020-camera-protect-command` from freshly fetched main
`63008c88e8987842a795ad61a7c3c739f3549c47` (merged PY-MEL-019 / PR #242).
Preflight inspected open PRs, local/remote branches and worktrees. Only unrelated
Ada Scheduling PR #241 was open; no duplicate Python submission task existed.

Source authority is the existing C facade, not an invented command engine:

- `native/include/ams_mel/abi.h`: frozen command/response/result records and
  `ams_mel_ir_c2_submit_camera_protect`,
  `ams_mel_ir_camera_protect_request_wait`,
  `ams_mel_ir_camera_protect_request_close` contracts.
- `native/src/ir_c2.cpp`: Enabled-only admission, lifetime claim, provider send
  outside bridge locks and independent metadata registration.
- `native/src/ir_camera_protect.cpp`: exact field conversion, native future
  consumption, terminal cache, diagnostics and deferred graph release.
- `native/tests/test_ir_camera_protect.c` and `native/tests/mock_provider.cpp`:
  compiled-C contracts, exact getter probe, held-promise controls, reclamation
  barriers, synchronous callback and provider-library lifetime evidence.
- Pinned `native/vendor/ir-mel/include/irmel/library/c2/CameraProtectCmd.h`
  (which defines both command and response) and `C2Channel.h`; immutable source evidence remains
  in `docs/reference/irmel-upstream-git-evidence.json` and
  `docs/reference/irmel-upstream-api.json`.

Coverage delta: safe Python submission and uniquely owned asynchronous observer
over already implemented native functionality. No native ABI, provider, ctypes,
Ada, Rust, vendored SDK or workflow changes. The structured inventory described
by `docs/reference/irmel-api-schema.md` classifies native/Ada, not Python; those
classifications and the historical `docs/coverage.md` remain unchanged.

## API and validation

Both `ams_mel.ir` and legacy `ams_mel` export identical
`CameraProtectCommand`, `CameraProtectCompleted`, `CameraProtectRejected` and
`CameraProtectRequest` classes with canonical `ams_mel` pickle paths. Command
and outcome values are frozen, Python-owned dataclasses. The existing
`CameraProtectResponse`, `CameraProtectStatus`, `MelErrorCode`, `MelError` and
`ErrorKind` are reused, not duplicated.

`CameraProtectCommand(command_id, response_time_ns, protect_flag)` validates at
construction and again immediately before submission:

| Field | Contract | Native encoding |
| --- | --- | --- |
| command_id | Python int except bool, 0 through UINT32_MAX | uint32 unchanged |
| response_time_ns | Python int except bool, INT64_MIN through INT64_MAX | int64 unchanged |
| protect_flag | actual Python bool only | False = uint32 0; True = uint32 1 |

Unrelated duck-typed inputs, floats, strings and integer-like coercions are not
accepted. Frozen-dataclass bypasses do not bypass submission validation. There
is no truncation, clamp, clock epoch, deadline, expiration policy or retry.

`ControlChannel.submit_camera_protect(command)` requires an open C2 owner and
native Enabled admission. It never implicitly Enables or registers callbacks.
All output owner storage is allocated before the single submission call. A
successful call without a request is a protocol inconsistency. Native failures
retain their structured status, category and available diagnostic. An unexpected
failure output owner receives at most one best-effort Close, preserving the
primary exception. A post-send internal failure can mean work already started;
submission is never retried.

## Wait outcomes and diagnostics

`request.wait(timeout_ms=0)` calls the existing native observer. Zero polls;
positive uint32 milliseconds give one finite Wait. Timeout neither cancels nor
consumes the request; a later Wait is permitted. Python creates no worker,
future, callback-completion mechanism or indefinite polling loop.

- Native OK requires error code NONE and an empty complete diagnostic. It
  returns `CameraProtectCompleted(CameraProtectResponse(...))`, preserving the
  complete uint32 command ID and signed int64 response time. All three existing
  response alternatives (OK, UNAVAILABLE, UNSUPPORTED) are successful data.
- Native COMMAND_REJECTED returns `CameraProtectRejected(code, description)`.
  Inactive response bytes are never interpreted. Unknown returned uint32 codes
  use the existing `MelErrorCode.UNKNOWN_0xXXXXXXXX` contract.
- Other statuses raise structured `MelError`, never a successful response.
  Notably, the **existing native** `protect-unknown-error` fixture classifies an
  upstream unknown MEL code as PROVIDER_FAILED, before Python sees a rejection.
  Python preserves this behavior; hostile ABI tests separately verify unknown
  codes returned with COMMAND_REJECTED are preserved. Unknown response status,
  null successful provider data and invalid futures also remain native failures.
- Unsupported metadata registration is a separate `CommandReturn` domain,
  unchanged from PY-MEL-019; it is neither a command rejection nor response data.

Wait first uses the established 512-byte diagnostic capacity. Oversized terminal
diagnostics are allocated exactly and reread from the native cache with timeout
zero. Status, active result fields and required size must match. Error response
fields are inactive and ignored. Strict UTF-8 decoding rejects embedded NUL,
missing termination and inconsistent retry output. Nonterminal timeout is never
retried for text retrieval. The mock long rejection requires 614 bytes including
NUL; its Euro sign straddles the initial buffer boundary. Repeated completed
Waits preserve the same logical outcome. Allocation/conversion failures do not
consume the native cache and are reported as local internal errors.

## Ownership, callback independence and lifetime

Requests retain no Python ControlChannel or Session. Native admitted-work
retention keeps the provider graph alive across either parent-close order and
Python parent collection. Completed values own no native memory and survive
request Close and parent teardown.

Requests reject copy, deepcopy and instance pickling, support context management
and non-raising finalization, and must have Wait/Close externally serialized.
The GIL is not a thread-safety guarantee. Close drops the public observer,
**not** provider work. Repeated successful Close is a no-op. If Close actually
invokes native and fails, the object is disarmed, reports `is_open == False`,
and retains that original error for explicit subsequent operations; it never
releases an ownership-uncertain handle again. Local diagnostic allocation
failure before native invocation leaves Close retryable. Context cleanup never
masks a primary application exception.

`open_camera_protect_response()` remains unchanged and independent. Submission
never registers it; callbacks cannot complete or replace Wait. The `protect-sync`
fixture delivers metadata synchronously inside send (response time -77,
UNAVAILABLE), independently of the request result (original response time, OK).
Future completion does not synthesize an event. Related IDs imply neither Python
correlation nor physical protection state. No Python lock spans provider send.

Isolated subprocesses use provider `mock_completion_release_family(8, count)`
and native completion family-7 reclamation barriers, not sleeps, to prove:
pending timeout/re-Wait, multiple owners, parent teardown/collection, observer
Close/finalization without cancellation, retained admission, no early unload,
and exactly one ordered C2 destruction, Control destruction and library unload.
Test function lookup releases its temporary dlopen reference before these
assertions, so it cannot substitute for native library retention.

## Executable mock example

After `make test-native`, run with this worktree's Python path and test library:

```sh
export PYTHONPATH="$PWD/python"
export AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

```python
import os
from pathlib import Path
from ams_mel import common, ir

provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ir.ControlConfig.new(
    common.UciId(bytes(16), "camera C2"),
    common.UciId(bytes(16), "platform"),
    common.ComponentLocation(0.0, 0.0, 0.0, "station", "mock"),
)
with ir.Session.open(provider, "protect-echo", "") as session:
    with session.open_control_channel(config) as channel:
        channel.enable()
        with channel.submit_camera_protect(
            ir.CameraProtectCommand(0xFFFFFFFF, -(1 << 63), True)
        ) as request:
            outcome = request.wait(5000)
            assert outcome == ir.CameraProtectCompleted(ir.CameraProtectResponse(
                0xFFFFFFFF, -(1 << 63), ir.CameraProtectStatus.OK))
            assert request.wait() == outcome
            print("Mock completion:", outcome)
```

## Qualification evidence

Production `make native` passed and `make test-native` passed all 841 CTest
cases. The initial focused run found an incorrect test expectation for the
native unknown-error fixture; it was corrected to the authoritative native
PROVIDER_FAILED behavior, not by changing provider or binding semantics.

The host `/tmp` has exhausted its inodes. Commands run with a task-owned
temporary directory bind-mounted at `/tmp` in an unprivileged private mount
namespace (`unshare -Urm`), retaining normal networking for localhost tests.
No unrelated temporary files or host mounts are changed. Full final-source
commands/results and production export comparison are recorded below after
qualification; final HEAD/tree and hosted CI belong in the PR ledger to avoid
a recursive self-hash.

Stable implementation qualification (logs retained outside the worktree at
`/home/zboll/py-mel-020-evidence/logs`):

| Command | Actual result |
| --- | --- |
| `make native` | PASS, production build |
| `make test-native` | PASS, 841/841 CTest cases |
| `make test-python` | PASS, 751 tests with warnings as errors, plus native prerequisites |
| `make test-rust` | PASS, 639 tests across 53 test/doc-test groups, 15 intentionally ignored |
| `make test-policy` | PASS, 332 tests in 14 suites |
| `make test-docs-ci` | PASS, 128 tests in nine suites |
| `make check-ir-api-coverage` | PASS, inventory and 72 regression tests |
| `make check` | First run passed isolation, native, formatting and Ada smoke/negative tests; failed final-newline check on three new files; corrected and rerun for delivery |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files and six workflows |
| `python3 scripts/check_final_newlines.py` | PASS after final-newline repair |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff --check` | PASS |

The initial full Python run overlapped an implementation edit and one child
import read a partially rewritten new module. That run failed and is retained;
the subsequent stable-source `make test-python` passed all 751 tests. Neither
that failed run nor the initial unknown-error expectation is hidden as a pass.

Focused command (same explicit test-tree environment as the example, also
including `python/tests` on PYTHONPATH):

```sh
python3 -W error -m unittest test_ir_camera_protect_command \
  test_ir_camera_protect_response test_ir_general_mode test_ir_config_set \
  test_ir_navigation test_control_channel test_public_modules test_abi -v
```

The focused regression set includes the unchanged reception tests, C11 layout
probe, exact three native signatures, request Close/lifetime tests and the
executable example. Source-only Python reference generation also passed with
all public exports and operations documented; explicit native-backed reference
qualification and final-source repeats are recorded in the PR ledger.

Production ABI was compared to a **separately built archive of freshly fetched
main** at the base identity above: sorted defined dynamic-symbol inventories
match byte-for-byte, 333 entries (332 versioned functions plus version node).
Both SHA-256 digests are
`156c9485e8a06af68932d66f917461d7d7536fa2816dc2df51009d924c2908d3`.
No native/header/export-map/private-ctypes or provider bytes changed. Main was
refetched before delivery and remained the recorded base. CI-DOC-FAST-001
workflow triggers were not modified; expensive documentation campaigns remain
main-push/manual, not every PR.

## Limitations and review gate

Qualification uses compiled mock providers and hostile private ABI injection,
not Squall, hardware, physical safety or protection-state evidence. No upstream
semantics are invented and no native future engine is replaced. Other optional
C2 command families and broader Python IR parity remain separate tasks.
The native facade can intentionally retain uncertain post-send work for process
lifetime; Python neither overrides that policy nor retries the command.

Leave the PR open with auto-merge disabled for independent source-level ChatGPT
review of the exact final HEAD after hosted CI. This implementation session is
not an independent approval; no self-approval or merge is performed.
