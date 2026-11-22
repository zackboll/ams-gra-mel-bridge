# PY-MEL-023 — Safe Python C2 CalibrationStatus reception

## Authority and scope

The task branch `feature/py-mel-023-calibration-status-reception` and worktree
`/home/zboll/git/ams-py-mel-023` were created from freshly fetched `origin/main`
`3b28def52d98aa1db33794d63fbd560ad77cf220`, tree
`3789f74e34a2894d83cdd40318c2b4c8b5e64787`. This includes PY-MEL-022 (merged
PR #247). No overlapping open PR, task branch or worktree existed at preflight.

The source authorities are `native/include/ams_mel/abi.h`, registration and
teardown in `native/src/ir_c2.cpp`, snapshot/queue/event operations in
`native/src/ir_calibration_status.cpp`, and `native/tests/test_ir_calibration_status.c`
with `MockCalibrationStatus` in `native/tests/mock_provider.cpp`. The pinned
IR MEL revision is `8d9224519f12b44e0b28815755c56a32a28d24a0`: its vendored
`irmel/library/c2/C2Channel.h` defines the optional registration overload. The
pinned Common MEL revision is `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`: its
vendored `mel/library/{CalibrationStatus,SubsystemCompletedCalibration,SubsystemCompletedCalibrationItemType}.h`
define the full data contract. The structured IR catalog, pinned Git evidence and
reviewed native/Ada coverage governed by `docs/reference/irmel-api-schema.md`
are unchanged: this task adds safe **Python reception**, not native or Ada coverage.

## Public API and field mapping

`ControlChannel.open_calibration_status(queue_capacity: int = 16)` returns a
`CalibrationStatusStream`. Use `is_open` to inspect local ownership,
`receive(timeout_ms: int = 0)` to obtain a `CalibrationStatus`, `counters()`
for a `C2MetadataCounters` snapshot, and `close()` or context management to
release the consumer. `CalibrationStatusOpenError` is a `MelError` with the
independently published `registration: CommandReturn | None`.

| Python value / field | Pinned getter / C field |
| --- | --- |
| `CalibrationStatus.active_calibration_ids: tuple[UciId, ...]` | `getActiveCalibrationIDs()` / `active_calibration_ids` |
| `CalibrationStatus.completed_calibrations: tuple[CompletedCalibration, ...]` | `getCompletedCalibrations()` / `completed_calibrations` |
| `CompletedCalibration.calibration_id: UciId` | `getCalibrationID()` / `calibration_id` |
| `CompletedCalibration.time_tag_ns: int` | `getTimeTag().count()` / `time_tag_ns` |
| `CompletedCalibration.result: SubsystemCalibrationResult` | `getResult()` / `result` |
| `CompletedCalibration.fail_reason: str` | `getFailReason()` / `fail_reason` |
| `CompletedCalibration.calibration_items: tuple[CompletedCalibrationItem, ...]` | `getCalibrationItems()` / `calibration_items` |
| `CompletedCalibrationItem.item_name: str` | `getItemName()` / `item_name` |
| `CompletedCalibrationItem.result: SubsystemCalibrationResult` | `getResult()` / `result` |
| `CompletedCalibrationItem.fail_reason: str` | `getFailReason()` / `fail_reason` |

`SubsystemCalibrationResult` is a separate `IntEnum`: `NOT_SET=0`,
`COMPLETED=1`, `FAILED=2`, `INTERRUPTED=3`, `NOT_CALIBRATED=4`. The upstream
`MaxExclusive` sentinel (5) and all unknown values are rejected. The two outer
tuples and every item tuple are independently sized and may be empty. Order,
duplicates, all 16 UUID bytes, descriptive labels, empty and long text, and
non-ASCII text are preserved. No equality, correspondence, inferred result
relationship, quality ranking, sorting or deduplication is applied. Every
`int64_t` bit pattern is a valid signed count of nanoseconds; there is no epoch,
unit conversion, narrowing, or inference about physical calibration validity.

The three records are frozen dataclasses. `ams_mel.ir` and `ams_mel` expose the
same six new classes with canonical `__module__ == "ams_mel"`; owned values
support pickling. `UciId`, `C2MetadataCounters`, `CommandReturn`, `MelError`
and `ErrorKind` are the existing canonical classes.

## Validation and ownership

Only six existing ABI operations are used in production:
`ams_mel_ir_c2_calibration_status_open` and
`ams_mel_ir_calibration_status_{receive,get_counters,close,event_view,event_close}`.
Receive validates the timeout (uint32 milliseconds, excluding bool), obtains a
native event owner, validates the fixed borrowed record address, copies every
field, then closes the event and verifies consumption before returning the value.
All span counts and string byte lengths must fit `INT32_MAX`. Every nonempty
span independently requires a non-NULL, aligned pointer and representable
extent/address arithmetic. The bridge copies exact bytes, rejects embedded NUL
as `PROTOCOL_INCONSISTENCY` and invalid UTF-8 as `INVALID_UTF8`. Late nested
failure or allocation failure returns no partial snapshot. Structural checks
cannot prove a plausible address is mapped or has the claimed extent: that
remains part of the trusted native ABI contract.

On failed Receive or Event View with a held event, Python makes one best-effort
cleanup attempt and preserves the primary exception. An attempted Event Close
is never repeated, including after malformed diagnostics and unexpected
exceptions. A successful status that leaves the event owner set is rejected.
The unique stream owner cannot be copied, deep-copied or pickled. Successful
Close is idempotent; finalization is non-raising. A local failure before Close
invocation leaves the owner usable. Uncertain native Close consumption is
cached as a failure and the handle is disarmed; subsequent operations do not
invoke native Close again.

## Registration, queue, and lifetimes

Registration is independent and opt-in while Attached or Enabled. Capacity is
a positive Python integer (not bool) representable in `size_t`. Local
validation and owner/output allocation precede the one-shot, which is consumed
immediately before native invocation. Synchronous callbacks are allowed; even
failed native attempts and consumer Close do not permit retry. `NOT_SUPPORTED`
and `FAIL` are distinct known provider Returns, not inferred from diagnostics
or native status. An exception reported by the provider is a separate native
error; unknown or unpublished Returns remain `None`. Received results do not
share either status domain.

The native bounded FIFO uses DROP-INCOMING. **Even when full**, malformed
callbacks increment `malformed_or_unsupported`; only valid full-queue callbacks
increment `events_dropped_queue_full`. The native full-queue path validates
without building another snapshot. `events_received` counts callbacks, and all
three cumulative `uint64` values are reported as-is without resetting. The
compiled C `calstatus-callback-capacity` contract is the primary evidence;
Python also checks its native counters through the private test ABI.

Zero timeout polls; timeout is not cancellation. Native Receive drains queued
events before `STREAM_STOPPED` or terminal `PROVIDER_FAILED`. Close deactivates
public consumption but does not unregister the callback or prove quiescence.
Externally serialize operations on one receiver and registration versus C2
operations; the GIL is not a native ownership lock. The receiver retains no
Python C2 or Session reference. The native C2/provider graph retains callback
state and manages teardown; event backing and Python values survive closure of
the stream and parents and actual provider-library unloading.

This API neither Enables C2 nor registers ordinary C2 metadata or
CalibrationConfiguration. It does not change either existing calibration
configuration operation. No `CalibrationStatusCmd` submission, asynchronous
request Wait, inferred UCI/timestamp correlation, or callback-to-future
completion is exposed. Safe Python status command/request support and other
unimplemented operations remain separate parity work. Qualification is against
the mock provider, not a real provider, Squall or physical hardware.

## Executable mock-provider example

After `make test-native`, set `PYTHONPATH` to this worktree's `python`,
`AMS_MEL_NATIVE_LIB` to `native/build-tests/lib/libams_mel_c.so.0`, and
`AMS_MEL_TEST_PROVIDER_DIR` to `native/build-tests/test-providers` (absolute paths).
The Python tests execute this example verbatim:

```python
import os
from pathlib import Path
from ams_mel import ComponentLocation, ControlConfig, Session, SubsystemCalibrationResult, UciId

provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ControlConfig.new(
    UciId(bytes(range(16)), "c2"), UciId(bytes(16), "platform"),
    ComponentLocation(0.0, 0.0, 0.0, "sensor", "mock"),
)
with Session.open(provider, "calstatus-callback-rich", "") as session:
    with session.open_control_channel(config) as c2:
        # Register while Attached; do not call enable().
        with c2.open_calibration_status() as stream:
            status = stream.receive()
            completed = status.completed_calibrations[2]
            item = completed.calibration_items[1]
            assert completed.result is SubsystemCalibrationResult.FAILED
            assert item.result is SubsystemCalibrationResult.INTERRUPTED
            assert item.fail_reason == "q" * 600

# Event, stream, C2 and Session have all closed.
assert completed.time_tag_ns == -(1 << 63)
assert completed.fail_reason == "f" * 700
assert status.active_calibration_ids[0] == status.active_calibration_ids[2]
assert item.item_name == "beta" and len(item.fail_reason) == 600
```

## Local qualification ledger

Build-sensitive checks ran sequentially. Native and Python suites use the
test-enabled facade and deterministic mock fixtures; the production export
comparison uses two separate production builds. Command outputs and exit
statuses are retained in `/home/zboll/py-mel-023-evidence/qualified`, with
earlier attempts in `/home/zboll/py-mel-023-evidence/logs`.

| Command | Observed result |
| --- | --- |
| `make native` | PASS, production facade |
| `make test-native` | PASS, 953 CTest cases |
| `make test-python` | PASS, 816 tests with warnings as errors |
| `make test-rust` | PASS, 642 passed and 15 intentionally ignored across 53 groups |
| `make test-policy` | PASS, 332 tests in 14 suites |
| `make test-docs-ci` | PASS, 128 tests in nine suites |
| `make check-ir-api-coverage` | PASS, structured inventory and 79 regressions |
| `make check` | PASS with Python provider variables unset; build isolation, native tests, pinned Ada formatting, smoke and negative controls, and scheduling examples |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files and seven workflows |
| `python3 scripts/check_final_newlines.py` | PASS |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff origin/main --check`; `git diff --check` | PASS |

With `python/tests` also on `PYTHONPATH`, the focused command passed 242 tests:

```sh
python3 -W error -m unittest test_ir_calibration_status \
  test_ir_calibration_configuration test_ir_calibration_configuration_command \
  test_ir_camera_protect_response test_ir_c2_metadata \
  test_ir_image_optical_distortion_map test_ir_image_los_quaternion \
  test_public_modules test_abi -v
```

The feature tests exercise all rich fields, independent collection shapes,
malformed callbacks at capacity, allocation recovery, an isolated held native
request that does not complete on callback delivery, and the executable example.
The native callback barrier proves consumer Close and parent deactivation
while a callback is in flight; subprocesses verify post-closure and post-unload
event and value access. Hostile FFI tests use allocated backing for all reads.
No real fixture combines rich callbacks with failed detach, so that terminal
error translation is also tested separately at the FFI boundary after draining
the real queue.

Source-only `python3 docs/tools/python_api_reference.py --output <fresh-dir>`
and native-backed `python3 scripts/qualify_python_reference.py --inventory
<fresh-dir>/python-reference.json` both passed. The reference has 189 canonical
exports, including all six new classes and `ControlChannel.open_calibration_status`.
Freshly fetched main was built separately with `make native`; sorted
`nm -D --defined-only` symbol type/name inventories are byte-identical, 336
entries with SHA-256
`4ddbb8571553a17fe84b7279149c36c1f4af8ad2637e3c1cbd42d73d257c909f`.

An initial newline check identified missing terminal newlines in new files,
which were repaired. Reusing a reference output directory was refused; the
stale inventory correctly failed source-hash qualification, and fresh generation
and runtime reflection passed. Supplemental interpreter probes that overlapped
native relinking failed to load the transient library and are not counted as
passes. `make check` with the Python provider path exported stopped at Alire's
environment conflict after the native checks; it passed without those exports
using the installed GNAT 16.1.0, GPRbuild 26.2 and pinned GNATformat 26.0.0.
No tests or source policies were changed to avoid these setup failures.

Exact final HEAD, tree, incorporated-main identity, hosted workflow conclusions,
and independent-review state are recorded in the PR ledger. This implementation
session does not constitute the mandatory independent technical review or a
separate-reviewer GitHub approval.
