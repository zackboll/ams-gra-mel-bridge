# PY-MEL-025 — Safe Python C2 EraseCommand submission

## Authority and scope

The task branch `feature/py-mel-025-erase-command` started from freshly fetched
`origin/main` at `6cb2c459ae1c4ae201156b2a0b45190fb070ea16` (tree
`28c1b2600c6dd618cab609356035eb794cf7540b`), the merge of PY-MEL-024 (PR #254).
Preflight found no overlapping Python implementation, branch, worktree or PR.

The pinned IR MEL revision `8d9224519f12b44e0b28815755c56a32a28d24a0` supplies
`irmel/library/c2/{C2Channel,EraseCommand,EraseCommandType}.h`; its optional C2
send overload and four enum members are authoritative. The C records and three
entry points in `native/include/ams_mel/abi.h`, the Enabled admission adapter in
`native/src/ir_c2.cpp`, and the completion engine in `native/src/ir_erase.cpp`
define the Python FFI contract. The compiled fixtures in `native/tests/test_ir_erase.c`
and `native/tests/mock_provider.cpp` define the test behavior. Python reuses the
private ctypes bindings and the request conventions from
[PY-MEL-024](py-mel-024-calibration-status-command.md).

The coverage delta is one optional C2 send overload, its command and enum, two
terminal outcome values and an observer in Python. The structured upstream and
reviewed native/Ada inventories defined by `docs/reference/irmel-api-schema.md`
are unchanged, as are native ABI, provider, Ada, Rust and workflows. No
capability query, metadata registration, or response stream is introduced.

## Explicit submission

**EraseCommand can be irreversible.** Construction, opening, enabling, waiting,
closing and finalization do not submit anything. Only an explicit call to
`ControlChannel.submit_erase(command)` does. Qualification and the example below
use only the compiled test mock. Do not point them at a physical detector,
production provider, or deployed system.

`EraseCommand(command_id, erase_type)` is frozen and has no default values.
`command_id` must be an exact Python `int` in `0..4294967295`; booleans,
subclasses and coercible objects are rejected. `erase_type` must be an actual
member of `EraseCommandType`, not an integer, even if the integer is in range.
The checks run both on construction and before native submission so that an
invalid forced mutation fails with `MelError` / `INVALID_ARGUMENT`.

| Python alternative | ABI value | Pinned upstream alternative |
| --- | --- | --- |
| `ZEROIZE` | 0 | `Zeroize` |
| `CLASSIFIED_DATA_ERASE` | 1 | `Classified_data_erase` |
| `DISABLE` | 2 | `Disable` |
| `CDE_AND_ZEROIZE` | 3 | `CDE_AND_Zeroize` |

There is no `MaxExclusive` sentinel or fallback. Submission requires an open
`ControlChannel`; native admission requires Enabled C2 and an available Session
async slot. Python prepares the wire command and independent request slot before
calling `ams_mel_ir_c2_submit_erase` once. It neither Enables the channel nor
retries. A successful call must publish a non-null request. A failed submission
may already have started provider work: do not assume it is safe to resubmit.
If a failed submission unexpectedly publishes an owner, Python attempts one
best-effort Close without masking the original failure. Structured native errors
are preserved, including `INVALID_ARGUMENT`, `RESOURCE_EXHAUSTED`,
`PROVIDER_FAILED`, `PROVIDER_EXCEPTION` and `INTERNAL_ERROR`.

## Finite Wait and diagnostics

`EraseRequest.wait(timeout_ms=0)` polls; a positive exact uint32 value waits
for finite milliseconds. Timing out raises `MelError` / `TIMEOUT` without
consuming the observer, cancelling anything, or polling again to fetch text.
Repeated terminal waits read the native cache without issuing another send.

| Native Wait outcome | Python behavior |
| --- | --- |
| `OK` | Require `AMS_MEL_ERROR_NONE` and a complete, empty diagnostic. Validate the returned erase type and return `EraseCompleted(erase_type)`. Unknown ABI alternatives raise `PROTOCOL_INCONSISTENCY`. |
| `COMMAND_REJECTED` | Read only the active error code and diagnostic. Return `EraseRejected(code, description)`; an unknown uint32 ABI code uses the existing `MelErrorCode.UNKNOWN_0xXXXXXXXX` representation. An unsupported optional send is a MEL rejection. |
| `PROVIDER_FAILED`, `PROVIDER_EXCEPTION`, `INTERNAL_ERROR` | Raise structured `MelError` with the native status and diagnostic. Do not read either result field. Null or unknown provider returns, unknown upstream MEL codes, malformed provider descriptions and provider exceptions retain their native classifications. |

**Native success reports a provider-returned erase alternative. It does not
verify physical erasure, zeroization, or disablement.** The returned alternative
is independent of the input; no comparison, remapping, or inference of progress,
hardware state, security classification, or zeroization verification is made.

If a terminal diagnostic exceeds the initial buffer, Python allocates the full
required byte count and rereads the cache once at zero timeout. Status, required
size (including NUL), and active result fields must agree. The complete text must
be NUL-terminated, contain no embedded NUL, and decode as strict UTF-8. The mock
long rejection requires 614 bytes, including NUL (613 text bytes, 611 characters).
Malformed sizes/text or an inconsistent reread fail closed. Python allocation
failure while processing the result leaves the native request available for a
later Wait.

## Ownership and lifetime

The five public types have identical identities in `ams_mel` and `ams_mel.ir`,
with canonical `ams_mel` pickle paths for the immutable values. `EraseRequest`
is not copyable or pickleable. It supports explicit `close()`, `is_open`, and
context management. Successful Close is idempotent. Before actual invocation,
an allocation failure leaves Close retryable. After an uncertain native Close,
Python disarms the owner and preserves the original error without repeating
Close. Context exit does not mask an exception from the body, and finalization
is non-raising. Externally serialize Wait and Close on one request and
submission against other operations on its C2 parent.

An observer does not retain the Python C2 or Session. The native request graph
holds the C2 lifetime claim and Session admission permit. Request Close releases
the observer, **not** pending work or its admission slot. C2 and Session may
close before completion; native code retains the provider and defers physical
teardown until safe. A completed Python value contains no native state. Tests
use independent subprocesses, the mock future-release barrier (family 11), and
the native reclamation probe (family 10) rather than treating terminal-result
publication as proof of detached-worker reclamation. Native post-send failures
and deferred-cleanup failure deliberately retain ownership-uncertain graphs;
the binding neither recovers nor resends such work.

## Executable example — mock-provider only

From the repository root after `make test-native`, use `PYTHONPATH=python`,
`AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"`, and
`AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"`. The latter
must be the repository's compiled **test provider** directory.

```python
import os
from pathlib import Path

from ams_mel import (
    ComponentLocation, ControlConfig, EraseCommand, EraseCommandType,
    EraseCompleted, Session, UciId,
)

mock_provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ControlConfig.new(
    UciId(bytes(16), "mock C2"),
    UciId(bytes(16), "mock platform"),
    ComponentLocation(0.0, 0.0, 0.0, "station", "mock"),
)
with Session.open(mock_provider, "erase-echo", "") as session:
    with session.open_control_channel(config) as channel:
        channel.enable()
        with channel.submit_erase(
            EraseCommand(command_id=42, erase_type=EraseCommandType.DISABLE)
        ) as request:
            outcome = request.wait(5000)

assert outcome == EraseCompleted(EraseCommandType.DISABLE)
```

This assertion checks mock data fidelity only. Applications must also handle
`EraseRejected` and `MelError`; the returned value is not hardware evidence.

## Local qualification

Logs and exit statuses are retained under `/home/zboll/py-mel-025-evidence`.
Build targets that share a build tree were run in sequence for qualification.

| Command | Observed result |
| --- | --- |
| `make native` | Exit 0; production facade |
| `make test-native` | Exit 0; 978 CTest cases |
| `make test-python` | Exit 0; 860 Python tests with warnings treated as errors |
| `make test-rust` | Exit 0; 651 passed, 15 ignored |
| `make test-policy` | Exit 0; 333 tests |
| `make test-docs-ci` | Exit 0; 129 tests |
| `make check-ir-api-coverage` | Exit 0; structured inventory and 82 regression tests |
| `make check` | Exit 0 with GNAT 16.1 and GPRbuild 26 on `PATH`; build isolation, native suite, pinned Ada formatting, Ada smoke, negative cases and scheduling examples. Initial run exited 2 because `gprbuild` was not on `PATH`. |
| `python3 scripts/check_doc_source_integrity.py` | Exit 0; 825 vendor files and seven workflows checked |
| `python3 scripts/check_final_newlines.py` | Exit 0 after fixing missing final newlines in two new files |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | Exit 0 |
| `git diff origin/main --check`; `git diff --check` | Exit 0 |

The focused mock-backed `python3 -W error -m unittest -v` run passed 304
tests across `test_ir_erase_command`, `test_ir_calibration_status_command`,
`test_ir_calibration_status`, `test_ir_calibration_configuration_command`,
`test_ir_calibration_configuration`, `test_ir_camera_protect_command`,
`test_ir_camera_protect_response`, `test_control_channel`, `test_ir_c2_metadata`,
`test_ir_navigation`, `test_abi`, and `test_public_modules`. The focused
`test_ir_general_mode` and `test_ir_config_set` run passed 31 more tests. The
nine-case isolated lifetime/failpoint run passed 20 consecutive invocations.
Initial focused attempts exposed incorrect expectations about Attached refusal
and the native probe counting temporary owners for refused submissions. An early
retry overlapped a native relink and observed an incomplete library. The
corrected runs waited for the build prerequisites and used the compiled mock
exclusively. The native test suite also independently exercises EraseCommand.

`docs/tools/python_api_reference.py` generated pages for all five public types
and the `ControlChannel.submit_erase` section. The inventory passed
`scripts/qualify_python_reference.py` using the production facade for runtime
reflection (198 canonical exports; no provider opened during reflection).

`make native` also passed in a separate, clean worktree at the freshly fetched
base. The two production facades have identical sorted dynamic symbol types and
versioned names (`nm -D --defined-only --with-symbol-versions`): 338
`ams_mel_*` exports, plus the version node. Both lists have SHA-256
`c82ea697bb548c41c5e02d76974053b06e4a8b49ca9f137f6c2f7004b7a98fbf`.

The PR ledger records the exact commit/tree, incorporated main, and hosted
Bootstrap and API Documentation results separately from local qualification.
No exhaustive source-browser, search, resize or authored HTML run is required
as an ordinary PR gate under CI-DOC-SOURCE-FAST-001. The PR is for independent
source review, not author approval or a request to merge.
