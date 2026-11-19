# PY-MEL-021 — Safe Python C2 CalibrationConfiguration reception

## Authority, preflight and boundaries

Branch `feature/py-mel-021-calibration-configuration-reception` in
`/home/zboll/git/ams-py-mel-021` was created after fetching `origin/main` at
`4005df6c1a467e685a8a16f64e10e2d9fe947a9b`, tree
`be1868386d7fabc34c19ef81ee17c10613684519`. That commit merges PY-MEL-020,
PR #243. Existing branches/worktrees and open PRs were inspected: no overlapping
Python calibration implementation was found; the open #244 is Ada documentation.

Authority is `native/include/ams_mel/abi.h`, registration in
`native/src/ir_c2.cpp`, conversion/queue/lifecycle in
`native/src/ir_calibration_configuration.cpp`, C11 contract tests in
`native/tests/test_ir_calibration_configuration.c`, and the actual
`MockCalibrationConfiguration` implementation in `native/tests/mock_provider.cpp`.
Pinned `native/vendor/common-mel/include/mel/library/SubsystemCalibration.h`
defines `CalibrationConfiguration` as `std::vector<SubsystemCalibration>`.
The structured catalog and independently frozen source evidence in
`docs/reference/irmel-upstream-api.json` and `irmel-upstream-git-evidence.json`
remain authoritative. IR pin: `8d9224519f12b44e0b28815755c56a32a28d24a0`;
Common pin: `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.

Coverage delta: safe Python reception replaces private-ctypes-only access for
this callback, representing every nested field. The structured inventory governed
by `docs/reference/irmel-api-schema.md` classifies native/Ada evidence, not Python;
those classifications are unchanged. No native ABI, C++, provider, private ctypes,
Ada, Rust, vendor, dependency or workflow changes. Historical `docs/coverage.md`
is untouched. CalibrationConfigurationCmd submission and asynchronous Wait are
deliberately **not** exposed by this task.

## Exact data contract and public API

Both `ams_mel.ir` and root `ams_mel` export identical classes with canonical
`__module__ == "ams_mel"` and owned-value pickle paths:

```python
class CalibrationControlInterface(IntEnum):
    NOT_SET = 0
    CALIBRATION_COMMAND = 1
    CALIBRATION_STATE_COMMAND = 2

@dataclass(frozen=True)
class SubsystemCalibration:
    calibration_id: UciId
    accepted_interface: CalibrationControlInterface
    calibration_item_names: tuple[str, ...]
    subsystem_component_ids: tuple[UciId, ...]
    capability_ids: tuple[UciId, ...]

@dataclass(frozen=True)
class CalibrationConfiguration:
    entries: tuple[SubsystemCalibration, ...]
```

The upstream alternatives are respectively `NotSet`,
`SubsystemCalibrationCommand`, and `SubsystemStateCommand`. Value 3 is
`MaxExclusive`, not configuration data; 3 and every unknown value are rejected.
Each entry maps exactly to `ams_mel_ir_subsystem_calibration_v1`; the outer
`ams_mel_ir_calibration_configuration_v1` is an independent pointer/count vector.
There is no response command ID, timestamp, inferred association, uniqueness,
sorting, nonempty requirement or equal-length relationship. All four collections
retain order and duplicates independently. Empty strings and all empty collections
are valid. Every UciId copies all 16 UUID bytes and the exact descriptive label.

`ControlChannel.open_calibration_configuration(queue_capacity: int = 16)` returns
`CalibrationConfigurationStream`. Its public operations are `is_open: bool`,
`receive(timeout_ms: int = 0) -> CalibrationConfiguration`,
`counters() -> C2MetadataCounters`, `close() -> None`, and context management.
It reuses canonical `UciId`, `CommandReturn`, `C2MetadataCounters`, `MelError`
and `ErrorKind`; it adds no alternative counter or identifier representation.

## Borrowing, nested conversion and failure safety

Exactly six existing operations are used: `ams_mel_ir_c2_calibration_configuration_open`
and `ams_mel_ir_calibration_configuration_{receive,get_counters,close,event_view,event_close}`.
No record, handle or function declaration is duplicated in private ctypes.

Receive obtains an event owner, obtains its borrowed view, validates the view's
fixed-record pointer, converts the entire graph, closes the event, verifies that
Close cleared the owner, and **only then** returns the Python snapshot. Every
count is checked against native INT32_MAX before reading; each nested span is
checked independently for non-NULL data when nonempty, alignment, representable
byte size and pointer arithmetic, using the existing `_checked_span` and
`_checked_span_address`. String byte sizes have the same INT32_MAX limit before
the shared `_copy_string_view` and `_copy_uci_id` helpers read them. UTF-8 is strict;
embedded NUL is a protocol inconsistency. Invalid UTF-8 is `INVALID_UTF8`;
malformed spans or interface values are `PROTOCOL_INCONSISTENCY`; allocation
failures during reception are `INTERNAL_ERROR`. No partially converted result
escapes. Feature-local checks do not alter global FFI validation policy.

Structural validation cannot prove that an arbitrary aligned non-NULL address
is mapped or has the claimed allocation extent. Native ABI/provider correctness
still owns that obligation. Hostile tests read only allocated backing and use
invalid addresses only where validation rejects before dereference.

Cleanup always preserves the primary failure. If receive fails after publishing
an event, or event-view fails after publishing a pointer, the event gets one
best-effort Close. An attempted event Close is never retried, whether it consumed
the owner, retained it, returned failure, raised unexpectedly or corrupted its
diagnostic. Successful Close retaining ownership is a protocol inconsistency.

## Registration, ownership and native lifetime

Registration is opt-in while C2 is Attached or Enabled. It never Enables,
submits a command or registers ordinary C2 metadata. Capacity must be a positive
Python int representable in size_t (bool is rejected). The output owner and its
pointer storage exist before registration, permitting synchronous callbacks.
Local invalid arguments and pre-native allocation failures do not consume the
one-shot. The independently tracked one-shot is consumed immediately before the
native call, including native failures and unexpected invocation exceptions;
Close does not permit registration again. Ordinary C2 metadata remains independent
in either registration order. An unsuccessful Open never publishes a Python
owner; an unexpected native output receives at most one cleanup attempt.

`CalibrationConfigurationOpenError.registration: CommandReturn | None` retains
only independently published, known provider Returns. NOT_SUPPORTED and FAIL
remain distinct; native status/kind is a separate domain. A provider exception
has `PROVIDER_EXCEPTION`, not an invented FAIL Return. Unknown/unpublished Returns
remain None regardless of diagnostic wording. Unexpected Python exceptions are
preserved, not misclassified from their text.

The stream uniquely owns its native handle and retains no Python parents.
Copy, deepcopy and pickle of the owner are forbidden. Explicit successful Close
is idempotent; finalization is non-raising. A native Close attempt with uncertain
consumption is cached and re-raised on later operations, never retried by Close
or finalization. A failure before native invocation may be retried safely.

Zero timeout polls; positive values are finite uint32 milliseconds. Timeout does
not cancel registration/provider work. The native receiver drains queued events
before reporting stopped/failed terminal state. Counters are cumulative full-width
uint64 snapshots: received callbacks, DROP-INCOMING overflow drops, and malformed/
unsupported callbacks. Malformed callbacks do not publish partial configurations.

Externally serialize receive/counters/close on one receiver, and registration
against parent C2 operations; the GIL is not sufficient. Native Close deactivates
consumption but does not unregister the provider callback or prove immediate
quiescence. The provider-retained callback owns shared native receiver state;
C2 teardown deactivates/drains callbacks, and the native Session/channel graph
retains provider code until physical teardown. Returned Python values contain no
ctypes storage and survive event, receiver, C2, Session and provider-library unload.

## Executable mock-provider example

Build with `make test-native`, set `PYTHONPATH` to this worktree's `python`,
`AMS_MEL_NATIVE_LIB` to `native/build-tests/lib/libams_mel_c.so.0`, and
`AMS_MEL_TEST_PROVIDER_DIR` to `native/build-tests/test-providers` (absolute paths).

```python
import os
from pathlib import Path
from ams_mel import (
    CalibrationControlInterface, ComponentLocation, ControlConfig, Session, UciId,
)

provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
config = ControlConfig(
    UciId(bytes(range(16)), "c2"), UciId(bytes(16), "platform"),
    ComponentLocation(0.0, 0.0, 0.0, "sensor", "mock"),
)
with Session.open(provider, "calibration-callback-rich", "") as session:
    with session.open_control_channel(config) as c2:
        # Deliberately Attached: no c2.enable() call.
        with c2.open_calibration_configuration() as receiver:
            configuration = receiver.receive()
            entry = configuration.entries[1]
            assert entry.accepted_interface is CalibrationControlInterface.CALIBRATION_COMMAND
            assert entry.subsystem_component_ids[0].uuid == bytes(range(32, 48))
            assert entry.capability_ids[1].descriptive_label == ""
        # Event and receiver have closed; the snapshot remains Python-owned.
        assert entry.calibration_item_names == ("", "one", "z" * 700)
# Session and C2 closed too.
assert configuration.entries[2].subsystem_component_ids[1].uuid == bytes(range(160, 176))
```

## Qualification and limitations

Evidence logs are retained outside the worktree in
`/home/zboll/py-mel-021-evidence/logs`. The host `/tmp` has exhausted all inodes;
commands run in an unprivileged private mount namespace with task-owned storage
bound to `/tmp` (`unshare -Urm`), preserving host mounts and ordinary networking.

Compiled fixtures assert exact rich values, all three interfaces, empty outer/
nested collections, 700-byte name, UUID bytes and independent collection lengths.
Native malformed callback cases cover NULL payload, MaxExclusive/unknown enum,
invalid UTF-8 and embedded NUL, with exact received/malformed/drop counters.
Allocation, overflow, registration failure/exception, Attached/Enabled, independent
ordinary registration, parent-first/consumer-first/GC, late callbacks, in-flight
barriers and actual DSO unload execute in isolated subprocesses without sleeps.
The rich compiled fixture contains neither duplicate entries nor non-ASCII text;
allocated hostile-view fidelity tests cover those without modifying the provider.
`calibration-sync` and `calibration-sync-held` emit callbacks during **command
submission**, not registration; they were inspected but are not invoked by this
metadata-only task. Rich registration itself delivers synchronous callbacks.

Hostile view tests independently cover structural span/string errors at all
levels, late nested corruption, immutable owned values after backing mutation,
allocation at multiple levels, receive/view failure after output publication,
diagnostic corruption, primary-error preservation, event/stream uncertain Close,
full-width counters, facade identity, pickle and import order. Native C11 layout
and export inventories remain active regressions.

Actual final-source command results, export comparison and hosted evidence are
recorded below after execution. Exact final HEAD/tree and hosted CI URLs belong
in the PR ledger (not a recursively self-identifying source commit).

Qualification is mock-provider and hostile-ABI evidence, not Squall/hardware
qualification or a claim about calibration behavior. No calibration command or
future semantics are invented. The PR must remain open, unmerged, with auto-merge
disabled for independent ChatGPT review of the actual final-HEAD diff and native
contract. This implementation session is not an independent approval.

## Local results and reconciliation ledger

Stable Python/native feature source qualification:

| Command | Actual result |
| --- | --- |
| `make native` | PASS, production facade |
| `make test-native` | PASS, 878 CTest cases |
| `make test-python` | PASS on stable-source rerun, 775 tests |
| `make test-rust` | PASS, 640 passed and 15 intentionally ignored across 53 groups |
| `make test-policy` | PASS, 332 tests in 14 suites |
| `make test-docs-ci` | PASS, 128 tests in nine suites |
| `make check-ir-api-coverage` | PASS, 75 tests and inventory verification |
| `make check` | Initial run stopped at missing GNAT/GPRbuild on PATH after native/isolation/format/coverage stages; installed-toolchain rerun recorded in final ledger |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files and six workflows at initial base |
| `python3 scripts/check_final_newlines.py` | PASS after task-document newline repair |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff --check` | PASS |

Focused final-source regression command, with absolute Python/native/provider
environment variables as above and `python/tests` also on PYTHONPATH:

```sh
python3 -W error -m unittest test_ir_calibration_configuration \
  test_ir_c2_metadata test_ir_camera_protect_response test_ir_camera_protect_command \
  test_ir_image_optical_distortion_map test_ir_image_los_quaternion \
  test_control_channel test_abi test_public_modules -v
```

PASS: 243 tests. Source-only generation and explicit native-backed qualification
of `final-reference/python-reference.json` passed (179 public exports, all five
new public symbols included). The executable example passed.

Non-passing attempts are retained in evidence: initial full Python qualification
overlapped the last implementation write and one subprocess imported a partially
rewritten feature module; stable-source rerun passed. A reference generated before
that write correctly failed source-hash qualification; fresh generation and runtime
qualification passed. The first newline check found this new task document lacked
its final newline; repaired before final validation. No test was weakened.

During qualification freshly fetched main advanced to
`72492c53b0dd4a71d322bcb4b0f45a18b602ca9e`, tree
`cd3e358bc5b2be56afcc185ed9e6cc6b4c586a85`, merging #244. The incoming changes
are documentation tooling/workflows only, with no native/Python/Ada/Rust changes.
They add a PR-triggered source-browser workflow; this feature does not change
that or other workflow triggers. Ordinary main integration and combined-source
qualification are recorded in the final delivery ledger. Expensive existing
search/resize/authored-HTML campaigns remain unchanged (CI-DOC-FAST-001).

GitHub REST initially hit its authenticated rate limit; GraphQL remained available
and independently confirmed #243 merged, no overlapping open PR, repository
auto-merge disabled, and main conversation/review requirements. Final commit/tree,
hosted runs, mergeability and independent-review status are recorded on the PR.


### Completed combined-source local qualification

Ordinary conflict-free merge of main #244 produced implementation commit
`c65f70910b19c72caf0d0693425d011a69fbc88f`, tree
`b05d02686baabe487fdbf30f3ef902a1d4145741`. No Python implementation/test bytes
changed during or after the combined-source runs; recorded SHA-256s were checked.

All required commands have now passed on this combined source:

- `make native`; `make test-native`: PASS, 878 CTest cases.
- `make test-python`: PASS, 775 tests with warnings as errors.
- `make test-rust`: PASS, 640 passed / 15 intentionally ignored across 53 groups.
- `make test-policy`: PASS, 332 tests in 14 suites.
- `make test-docs-ci`: PASS, 128 tests in nine suites.
- `make check-ir-api-coverage`: PASS, inventory and 75 controls.
- `make check`: PASS, including production/test-tree isolation, native tests,
  pinned Ada formatting, coverage, GPR smoke suite, 14 negative controls and
  scheduling example. PATH supplies already installed GNAT 16.1.0 and GPRbuild
  26.0.1; no toolchain install or Ada source change. Existing GPR notices about
  the imported C project having no C sources are not hidden as source warnings.
- `python3 scripts/check_doc_source_integrity.py`: PASS, 825 vendor files and
  seven workflows after main reconciliation.
- `python3 scripts/check_final_newlines.py`: PASS.
- `python3 -W error -m compileall -q python/ams_mel python/tests`: PASS.
- `git diff --check`: PASS.
- Focused command above: PASS, 243 tests on combined source.
- Source-only generation to `combined-reference` and
  `python3 scripts/qualify_python_reference.py --inventory
  /home/zboll/py-mel-021-evidence/combined-reference/python-reference.json`: PASS.

Freshly fetched main `72492c53b0dd4a71d322bcb4b0f45a18b602ca9e` was archived
and built separately with `make native`; the sorted `nm -D --defined-only`
production exports match byte-for-byte: 334 entries (333 versioned functions and
one version node). Both inventories have SHA-256
`4ae158889e4369ad85e90295d797e3a3105193c938784a6c525b41bb23737abd`.
The feature delta against that main is exactly the eight intended Python/test/
task-document files; no native, ctypes, Ada, Rust, vendor or workflow changes.

### Hosted evidence and independent review gate

Open non-draft PR: <https://github.com/zackboll/ams-gra-mel-bridge/pull/245>.
Implementation-head API Documentation completed **success**:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44438413255>
(source `c65f70910b19c72caf0d0693425d011a69fbc88f`; deployment jobs skipped).
Implementation-head Bootstrap:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44438413208>.
At this evidence update eight jobs have passed; GCC Debug is still pending its
final conclusion. No pending job is claimed passed. Main-added source-browser
run <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44438413205>
is also in progress. Exact evidence-only final HEAD/tree and its new hosted
conclusions will be recorded in the PR ledger, avoiding a recursive self-hash.

Local/remote/PR implementation identities were verified identical; GitHub reports
MERGEABLE, OPEN, non-draft, `autoMergeRequest == null`, and repository auto-merge
is disabled. These are not review approval. Independent final-HEAD ChatGPT review
is outstanding and must examine nested coverage, validation, borrowing, event
cleanup/uncertain ownership, one-shot registration, provider lifetime, compiled
regressions, namespaces and any subsequent main changes. No self-approval, merge
or `READY_FOR_USER_MERGE` claim is made.


A final **base-relative** `git diff origin/main --check` additionally found one
trailing blank line at EOF in the new test module that clean-working-tree checks
do not expose. Removed without behavioral change; both base-relative and working-
tree diff checks pass. The final PR ledger supersedes the evidence-only commit
identity above and records renewed exact-HEAD CI. No whitespace failure is
retroactively reported as a pass.
