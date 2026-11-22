# PY-MEL-024 — Safe Python C2 CalibrationStatus command submission

## Authority and scope

The dedicated `feature/py-mel-024-calibration-status-command` branch started
from freshly fetched `origin/main` at `5a9962d052445a3f2198199d94bf2235339b8425`
(tree `e0f1314f9d1323c6de293c86a140f7e24aa998a0`). This includes the merged
PY-MEL-023 and documentation CI trigger changes (PRs #250 and #251). Preflight
found no overlapping branch, worktree or open PR.

The source authorities are `native/include/ams_mel/abi.h`, the Enabled C2
submission adapter in `native/src/ir_c2.cpp`, and the completion engine in
`native/src/ir_calibration_status.cpp`. The pinned IR MEL revision
`8d9224519f12b44e0b28815755c56a32a28d24a0` supplies the vendored
`irmel/library/c2/{CalibrationStatusCmd,C2Channel}.h`. The pinned Common MEL
revision `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` supplies the vendored
`mel/library/{CalibrationStatus,SubsystemCompletedCalibration,SubsystemCompletedCalibrationItemType,Error}.h`.
The command encodes `setCommandID` and `setRequestID` independently. The
status and error alternatives follow the published native ABI, not guesses
based on provider-specific transports.
`native/tests/test_ir_calibration_status.c` and the `MockCalibrationStatus`
fixture in `native/tests/mock_provider.cpp` supply the compiled test contract.

This adds Python command coverage only. The structured IR catalog and reviewed
native/Ada inventories defined by `docs/reference/irmel-api-schema.md`, the
existing ctypes layouts and signatures, and the receiver introduced in
[PY-MEL-023](py-mel-023-calibration-status-reception.md) are unchanged.

## Public command and request

`CalibrationStatusCommand(command_id, request_id)` is frozen and requires two
exact Python `int` values in `0..4294967295`. This excludes `bool`, coercible
objects and `int` subclasses. Submission validates both fields again; forced
mutation does not bypass the checks. No relationship between the IDs is inferred.

`ControlChannel.submit_calibration_status(command)` requires an open channel.
Native admission requires Enabled C2 and an available Session completion slot.
Python prepares the command and unique `CalibrationStatusRequest` output slot
before a single call to `ams_mel_ir_c2_submit_calibration_status`. It does not
enable the channel or retry. `MelError` preserves native failure categories
such as `INVALID_ARGUMENT`, `RESOURCE_EXHAUSTED`, `PROVIDER_FAILED`,
`PROVIDER_EXCEPTION` and `INTERNAL_ERROR`. A post-send internal error may
mean the provider already accepted the command. A failed call that unexpectedly
publishes an owner gets at most one best-effort Close, without masking the
original error. Success without a non-null owner is a protocol inconsistency.

`request.wait(timeout_ms=0)` uses `ams_mel_ir_calibration_status_request_wait`.
Zero polls and a positive exact uint32 number of milliseconds is finite. A
structured `TIMEOUT` does not cancel work or consume the request. The native
observer caches terminal outcomes, so repeated Wait calls return equivalent
immutable values or raise equivalent structured failures. There is no Python
worker, polling service, or implicit Close. Externally serialize Wait and Close
on the same request and submission against C2 operations.

| Native Wait outcome | Python behavior |
| --- | --- |
| `OK` | Require `AMS_MEL_ERROR_NONE` and an empty, complete diagnostic, then deep-copy the entire status and return `CalibrationStatusCompleted(status)`. |
| `COMMAND_REJECTED` | Read only the active error code and diagnostic. Return `CalibrationStatusRejected(code, description)`, including the existing `MelErrorCode.UNKNOWN_0xXXXXXXXX` representation for unknown ABI rejection codes. |
| `PROVIDER_FAILED`, `PROVIDER_EXCEPTION`, `INTERNAL_ERROR` | Raise a structured `MelError`. Never read a result field. This includes the native bridge's classification of unknown upstream error codes, null successful data, invalid enums and malformed strings. |

If a terminal rejection/failure diagnostic exceeds the initial buffer, Python
reads the native cache once with zero timeout and the full reported capacity.
Status, required byte count (including NUL) and any active result field must
match the first read. The complete diagnostic must be NUL-terminated with no
embedded NUL and strictly UTF-8. Success must report exactly one NUL byte; an
oversized or malformed success diagnostic is rejected before nested access.
A timeout is not reread to retrieve diagnostic text.

## Deep copy and lifetime

The borrowed native status and nested pointers are valid until request Close.
Wait uses the PY-MEL-023 converter directly: both independent ordered tuples,
all UUID bytes and UTF-8 labels, signed 64-bit nanosecond counts, overall
results and failure text, and every nested item (including duplicates) are
copied. Supported results are `NOT_SET`, `COMPLETED`, `FAILED`, `INTERRUPTED`
and `NOT_CALIBRATED`; `MaxExclusive` and unknown results are invalid. There is
no epoch, timestamp conversion, result ranking or implied relationship between
collections. Malformed spans and strings fail before a value is returned.
The binding checks sizes, alignment and address arithmetic; it cannot establish
that an arbitrary aligned pointer from a broken C ABI is mapped. An allocation
or conversion failure does not consume the request and Wait can be retried.

The frozen command and terminal values are pickleable and canonically rooted
in `ams_mel`, with identical exports in `ams_mel.ir`. The request is not
copyable or pickleable. `is_open` reflects local ownership. `close()` calls
`ams_mel_ir_calibration_status_request_close` to release the observer without
cancelling admitted work. Repeated successful Close is a no-op; context exit
preserves a primary exception and finalization is best effort and non-raising.
A local allocation failure before calling native Close leaves it retryable.
After invocation with uncertain consumption, the request disarms the handle
and preserves the error for later explicit operations without calling native
again.

Requests retain no Python C2 or Session parent. The native C2 claim and Session
admission retain the provider graph until the native completion path releases
it. A completed Python snapshot retains no native state and can outlive all
owners and provider unload. Separate metadata registration can retain callback
state under its existing native contract. Submission never registers a callback,
uses metadata to complete a command, or emits a synthetic event. In the compiled
`calstatus-sync-held` case, the callback independently publishes a status seeded
by `77` during send while command Wait still times out. Its later command result
uses the submitted command-ID seed, not the callback data.

## Executable mock-provider example

Run against the test facade and mock provider from `make test-native`, with
`PYTHONPATH=python`, `AMS_MEL_NATIVE_LIB` set to the test facade and
`AMS_MEL_TEST_PROVIDER_DIR` set to the test-provider directory.

```python
import os
from pathlib import Path
from ams_mel import (
    CalibrationStatusCommand, CalibrationStatusCompleted,
    ComponentLocation, ControlConfig, Session, UciId,
)

provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ControlConfig.new(
    UciId(bytes(16), "calibration C2"),
    UciId(bytes(16), "platform"),
    ComponentLocation(0.0, 0.0, 0.0, "station", "mock"),
)
with Session.open(provider, "calstatus-rich", "") as session:
    with session.open_control_channel(config) as channel:
        channel.enable()
        with channel.submit_calibration_status(
            CalibrationStatusCommand(42, 7)
        ) as request:
            completed = request.wait(5000)
            assert isinstance(completed, CalibrationStatusCompleted)
            status = completed.status

# status remains valid after request, C2 and Session Close.
assert status.active_calibration_ids[0].uuid == bytes(range(42, 58))
```

This example expects the mock's success scenario. Actual callers must also
handle `CalibrationStatusRejected` and `MelError`. A returned status is data,
not evidence that physical sensor calibration has occurred. Qualification uses
the compiled mock and injected ABI outputs, not Squall, a real provider, or
hardware.

## Qualification ledger

Local command logs and exit statuses are retained at
`/home/zboll/py-mel-024-evidence`. Build-sensitive targets ran sequentially.

| Command | Actual local result |
| --- | --- |
| `make native` | Exit 0; production facade |
| `make test-native` | Exit 0; 978 CTest cases |
| `make test-python` | Exit 0; 838 tests with warnings treated as errors |
| `make test-rust` | Exit 0; 651 tests passed, 15 ignored |
| `make test-policy` | Exit 0; 333 tests |
| `make test-docs-ci` | Exit 0; 129 tests |
| `make check-ir-api-coverage` | Exit 0; inventory and 82 regression tests |
| `make check` | Exit 0; build isolation, native tests, pinned Ada formatting, Ada smoke, negative cases and scheduling examples |
| `python3 scripts/check_doc_source_integrity.py` | Exit 0; 825 vendor files and seven workflows checked |
| `python3 scripts/check_final_newlines.py` | Exit 0 after adding missing final newlines in the new files |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | Exit 0 |
| `git diff origin/main --check`; `git diff --check` | Exit 0 |

The focused run passed 258 tests, using the test-enabled facade and mock
provider paths from `make test-python`, with `python` and `python/tests` on
`PYTHONPATH`:

```sh
python3 -W error -m unittest -v test_ir_calibration_status_command \
  test_ir_calibration_status test_ir_calibration_configuration_command \
  test_ir_calibration_configuration test_ir_camera_protect_command \
  test_ir_camera_protect_response test_ir_navigation test_ir_c2_metadata \
  test_public_modules test_abi
```

`docs/tools/python_api_reference.py` generated the four new API pages and
the `ControlChannel.submit_calibration_status` section. The matching inventory
passed native-backed runtime reflection via `scripts/qualify_python_reference.py`
(193 canonical exports). The production facade and the separately built
fetched `main` have identical sorted `nm -D --defined-only --with-symbol-versions`
symbol names and types (338 `ams_mel_*` exports; the lists also include the
version node). SHA-256 of each list:
`a325a94fe7dfab6d5be4bd529a795d91420a3d0ee8159f6f0bf86b3a91da8973`.

For hosted qualification, the PR ledger records the observed final-HEAD
Bootstrap and API Documentation results separately from local qualification.
Under CI-DOC-SOURCE-FAST-001, exhaustive source browsing, search, resize and
authored-HTML campaigns are not ordinary PR gates; no workflow was changed or
extra browser campaign requested. Independent source review is against the
exact PR HEAD and is not an author approval or a request to merge.
