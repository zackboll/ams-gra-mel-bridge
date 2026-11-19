# PY-MEL-022 — Safe Python CalibrationConfiguration command submission

## Authority, base and scope

Dedicated branch `feature/py-mel-022-calibration-configuration-command` and
worktree `/home/zboll/git/ams-py-mel-022` were created from freshly fetched
`origin/main` at `f76b827bb2d3fa967c8f3624cb1b9deee13f54b6`, tree
`d7b4125a25a92a676147071004f20542d73e4328`. PR #245 is merged. Preflight
inspected local changes, active worktrees, branches, commit subjects and open
and historical PRs; no duplicate implementation or open PR was found.

Authority is the existing C11 facade in `native/include/ams_mel/abi.h`,
Enabled admission in `native/src/ir_c2.cpp`, snapshot/completion ownership in
`native/src/ir_calibration_configuration.cpp`, compiled C contract tests in
`native/tests/test_ir_calibration_configuration.c`, and
`MockCalibrationConfiguration` in `native/tests/mock_provider.cpp`. Pinned
`CalibrationConfigurationCmd.h` defines two independent uint32 identifiers;
`SubsystemCalibration.h` defines the nested configuration vector. Structured
source evidence remains in `docs/reference/irmel-upstream-api.json` and
`docs/reference/irmel-upstream-git-evidence.json`, governed by
`docs/reference/irmel-api-schema.md`.

Coverage delta: safe Python command submission, finite asynchronous Wait and
request Close over three **existing** exports:

- `ams_mel_ir_c2_submit_calibration_configuration`
- `ams_mel_ir_calibration_configuration_request_wait`
- `ams_mel_ir_calibration_configuration_request_close`

No new native ABI, private ctypes declaration, C++, provider, Ada, Rust, vendor,
dependency or workflow change. The authoritative inventory classifies native/Ada
coverage, not Python; its evidence and classifications are unchanged. Historical
`docs/coverage.md` is untouched. Reception, callback registration and native
completion handling are unchanged. No DSP, transport, hardware calibration,
identifier correlation, background thread or implicit polling is introduced.

## Public values and validation

Root `ams_mel` and `ams_mel.ir` export identical objects, canonically located
at `ams_mel` for introspection and value pickling:

- `CalibrationConfigurationCommand(command_id: int, request_id: int)`
- `CalibrationConfigurationCompleted(configuration: CalibrationConfiguration)`
- `CalibrationConfigurationRejected(code: MelErrorCode, description: str)`
- `CalibrationConfigurationRequest`, obtained only through
  `ControlChannel.submit_calibration_configuration(command)`.

The three values are frozen dataclasses. Both command identifiers must be
Python integers, excluding bool, in `0..0xFFFFFFFF`. Strings, floats and
integer-like coercions are refused. There is no wrapping, narrowing or inferred
relationship between identifiers. Construction validates, and submission fully
revalidates even forcibly mutated frozen values. Unrelated inputs are refused.

Submission requires an open Python C2 owner and native Enabled admission. It
does not Enable or register any metadata callback. Native output owner storage
is allocated before the single native invocation. A successful call without an
owner is a protocol inconsistency. Failed or uncertain submissions are never
retried: even INTERNAL_ERROR after send can mean provider work started. An
unexpected failure output receives at most one best-effort Close, preserving
the original failure even if cleanup also fails. INVALID_ARGUMENT,
RESOURCE_EXHAUSTED, PROVIDER_FAILED, PROVIDER_EXCEPTION and INTERNAL_ERROR
retain their structured native status and available diagnostic.

## Terminal outcomes and nested ownership

`request.wait(timeout_ms: int = 0)` performs a finite native Wait; zero polls.
A timeout raises `MelError(TIMEOUT)` without consuming or cancelling work and
is never retried just to obtain a longer diagnostic.

On OK, the MEL error code must be NONE and the diagnostic must be exactly a
valid empty NUL-terminated string. Nonempty or oversized success diagnostics
are rejected **before** copying or rereading a nested response. The result's
configuration remains borrowed from the native request's terminal snapshot
until request Close. Python directly reuses PY-MEL-021
`_configuration_from_raw`, validating the outer span and each nested span,
count, interface and string while the request is open. Only a complete,
independently owned Python snapshot is published. No ctypes structure or native
pointer is retained in the completed value.

The exact existing `CalibrationConfiguration`, `SubsystemCalibration`,
`CalibrationControlInterface` and `UciId` classes are reused. All UUID bytes,
UTF-8 labels and names, empty/long strings, independent collection sizes,
ordering and duplicates remain intact. Empty configurations and all three
valid interfaces remain valid. NULL/nonempty, misaligned, overflowing and
over-limit spans, embedded NULs, invalid UTF-8 and unknown interfaces are
refused by the existing safe converter. Address validation cannot prove that
an otherwise plausible provider address is mapped; the trusted native facade
must honor its borrowing contract. Hostile tests dereference allocated backing
only. Success describes returned configuration data, not physical calibration
or command execution success.

COMMAND_REJECTED produces a rejected value, not an empty configuration. Only
the active error code is read, never the inactive configuration. Unknown uint32
codes preserve `MelErrorCode.UNKNOWN_0xXXXXXXXX`. Provider diagnostics are
strict UTF-8, retrieved completely, including the fixture's 700-byte rejection.
For a terminal rejection or failure requiring a larger buffer, Python records
status and only active fields, allocates a full buffer, rereads the cached
terminal result with timeout zero, and requires matching status, required size
and active fields. Inconsistent rereads publish nothing. Native failures never
read inactive result fields and raise structured `MelError` instead of
fabricating a configuration.

Repeated Waits return equivalent immutable values and do not consume the
request. Copy/allocation failure leaves the cached result available for another
Wait; partially copied values are never returned.

## Request, parent and provider lifetime

The request is unique, non-copyable, non-pickleable and independent of Python
C2/Session parents. `is_open` describes local ownership. Explicit Close is
idempotent after success; context exit preserves a primary body exception and
finalization never raises. Allocation failure before Close invocation leaves
the owner retryable. After invocation, uncertain ownership is fail-closed,
attempted at most once and the original error is retained for later explicit
operations. Finalization does not retry uncertain Close.

Externally serialize Wait and Close: the GIL is not a lock. Timeout and Close
do not cancel provider work. The native graph retains admitted work, channel,
Session resources and provider code until its own completion/reclamation rules
permit release. Test-only native barriers, not arbitrary sleeps, distinguish
terminal publication from worker reclamation. An owned returned configuration
remains accessible, comparable and pickleable after request Close, parent Close
and final provider unload.

`open_calibration_configuration()` is independent. Submission never registers
it, synthesizes an event, correlates IDs or treats callback delivery as future
completion. In `calibration-sync-held`, synchronous metadata has seed 77 while
the command future is still pending; a later explicit fixture release produces
its separately owned command configuration.

## Executable mock-provider example

After `make test-native`, set `PYTHONPATH` to this worktree's `python` directory,
`AMS_MEL_NATIVE_LIB` to `native/build-tests/lib/libams_mel_c.so.0`, and
`AMS_MEL_TEST_PROVIDER_DIR` to `native/build-tests/test-providers` (absolute
paths in each case). The test suite executes this example verbatim:

```python
import os
from pathlib import Path
from ams_mel import (
    CalibrationConfigurationCommand, CalibrationConfigurationCompleted,
    ComponentLocation, ControlConfig, Session, UciId,
)

provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ControlConfig.new(
    UciId(bytes(16), "calibration C2"),
    UciId(bytes(16), "platform"),
    ComponentLocation(0.0, 0.0, 0.0, "station", "mock"),
)
with Session.open(provider, "calibration-rich", "") as session:
    with session.open_control_channel(config) as channel:
        channel.enable()
        with channel.submit_calibration_configuration(
            CalibrationConfigurationCommand(42, 7)
        ) as request:
            completed = request.wait(5000)
            assert isinstance(completed, CalibrationConfigurationCompleted)
            configuration = completed.configuration
            assert request.wait() == completed

# Configuration remains usable after request, C2 and Session Close.
assert len(configuration.entries) == 3
assert configuration.entries[1].calibration_item_names[2] == "z" * 700
print("Mock completion:", configuration)
```

## Qualification ledger and limitations

Logs and command exit statuses are retained outside the worktree at
`/home/zboll/py-mel-022-evidence/logs`. Initial native compiler probes failed
because `/tmp` exhausted its inode quota. Using task-owned TMPDIR allowed
compilation and the initial focused Python run: implementation, namespace and
ABI checks passed; the not-yet-created example document caused its sole error.
The first compiled native suite still failed fixture creation where C tests
hard-code `/tmp`; those failures are environmental, not reported as passes.
Final-source qualification and delivery identities are recorded below and on
the PR, avoiding a recursive final-commit hash in this file.

All provider qualification uses mocks, not Squall, credentials, hardware or
physical calibration. Python cannot override native permanent retention of
uncertain post-send work. No blanket thread-safety or callback-quiescence claim
is made. Independent final exact-HEAD technical review remains a separate gate;
this implementation session does not self-approve, enable auto-merge or merge.

### Final-source local results

All commands below returned exit status 0. Build-sensitive targets were run
sequentially in an isolated user/mount namespace with a private `/tmp`; no host
files were removed and no test source was changed to accommodate the host's
inode exhaustion. PATH supplies the already installed GNAT 16.1.0 and GPRbuild
26.2, with the repository's pinned GNATformat 26.0.0. Existing imported native
GPR notices about having no C sources remain visible, not hidden as warnings.

| Command | Actual result |
| --- | --- |
| `make native` | PASS, production facade |
| `make test-native` | PASS, 915 CTest cases |
| `make test-python` | PASS, 794 tests, warnings treated as errors |
| `make test-rust` | PASS, 641 tests across 53 groups, 15 intentionally ignored |
| `make test-policy` | PASS, 332 tests across 14 suites |
| `make test-docs-ci` | PASS, 128 tests across nine suites |
| `make check-ir-api-coverage` | PASS, inventory and 76 regression tests |
| `make check` | PASS, build isolation, native tests, pinned formatting, coverage, GPR/Ada smoke/negative tests and scheduling examples |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files and seven workflows |
| `python3 scripts/check_final_newlines.py` | PASS after repairing the task document's initial missing final newline |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff origin/main --check`; `git diff --check` | PASS |

Focused final regression command, with the same explicit test-tree native and
provider paths and both `python` and `python/tests` on PYTHONPATH:

```sh
python3 -W error -m unittest test_ir_calibration_configuration_command \
  test_ir_calibration_configuration test_ir_camera_protect_command \
  test_ir_camera_protect_response test_ir_general_mode test_ir_config_set \
  test_ir_navigation test_control_channel test_ir_image_optical_distortion_map \
  test_public_modules test_abi -v
```

PASS: 242 tests, including 18 task-owned methods with parameterized hostile
cases and six isolated lifetime subprocesses. Coverage includes exact independent
uint32 boundaries, no implicit registration/Enable, all named provider scenarios,
full long diagnostics, inactive-field poisoning, cached inconsistency, malformed
nested pointers/counts/enums/strings, nested allocation failure/retry, retained
admission after observer Close, uncertain Close and cleanup error preservation.
The snapshot-first subprocess copies the real returned result, closes requests
before C2/Session, proves final DSO unload, then accesses and pickles that result.
Other subprocesses prove pending parent-first and parent-GC lifetimes.

Source-only generation with
`python3 docs/tools/python_api_reference.py --output /home/zboll/py-mel-022-evidence/python-reference`
and explicit native-backed
`python3 scripts/qualify_python_reference.py --inventory /home/zboll/py-mel-022-evidence/python-reference/python-reference.json`
both PASS. All four new public symbols and the new submission operation appear
in generated API/source pages. Runtime qualification reports 183 canonical
exports and no missing public operation docstrings.

Fresh main was archived and built separately at the recorded base; production
`nm -D --defined-only` inventories (symbol type/name, sorted) match byte-for-byte:
335 entries with SHA-256
`1239f9b315b9e89357cd9f03ab0429fafb7a43888f840d074bc56bcb0a9c5fc8`.
Repeated pre-delivery fetch confirms main remains the recorded base and is an
ancestor of this branch. There is no native, private ctypes, Ada, Rust, vendor,
dependency or workflow delta. Exact final HEAD/tree, hosted workflow URLs and
conclusions belong in the PR ledger; pending hosted results are not claimed
passed. Independent technical review is outstanding.
