# PY-MEL-019 — Safe Python C2 CameraProtectCmdResp reception

## Source authority and preflight

The dedicated worktree is `/home/zboll/git/ams-py-mel-019`, branch
`feature/py-mel-019-camera-protect-response`. Freshly fetched main/base commit is
`f6c81cfff415566595ba177a50d41eb9f12a51d4`, tree
`824fa1bcbb5b9f7f466ac591f9b6561e6703174e`. GitHub confirms PY-MEL-018 / PR #233
merged at that commit. Preflight found no overlapping open Python C2 PR; the
only open PR was unrelated Ada Scheduling #241. Existing worktrees and their
branches were preserved. A second fetch confirmed the same main identity.

Authority is the existing `native/include/ams_mel/abi.h`, registration in
`native/src/ir_c2.cpp`, event/callback implementation in
`native/src/ir_camera_protect.cpp`, compiled C11 contract in
`native/tests/test_ir_camera_protect.c`, and actual scenarios in
`native/tests/mock_provider.cpp`. Reviewed the Ada camera-protection conversion
and tests, PY-MEL-018's once-only ownership handling, repository instructions,
architecture, ABI policy and structured IR coverage/schema. The immutable IR
catalog pin remains commit `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`; Common remains commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
`f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`.

Coverage delta: the previously private-ctypes-only C2 response callback gains
safe Python reception. All three published fields are represented, without a
native ABI, provider, Ada, Rust, vendor, workflow or historical coverage change.
The authoritative structured inventory classifies native/Ada coverage rather
than Python and does not need a status change for this Python-only wrapper.

## Public values and API

Both `ams_mel.ir` and compatible root `ams_mel` export the identical classes,
with canonical `__module__ == "ams_mel"` and compatible pickle paths:

```python
class CameraProtectStatus(IntEnum):
    OK = 0
    UNAVAILABLE = 1
    UNSUPPORTED = 2

@dataclass(frozen=True)
class CameraProtectResponse:
    command_id: int
    response_time_ns: int
    command_response: CameraProtectStatus
```

The returned snapshot owns only Python integers and an enum. Native unsigned
32-bit command IDs and signed 64-bit nanosecond counts are preserved exactly.
There is no inferred epoch, deadline, physical camera behavior or correlation
with a submitted request. Unknown response status values fail closed with
`PROTOCOL_INCONSISTENCY`; they are not mapped to an enum fallback.

`ControlChannel.open_camera_protect_response(queue_capacity: int = 16)` returns
a `CameraProtectResponseStream` with `is_open`, `receive(timeout_ms: int = 0)`,
`counters() -> C2MetadataCounters`, and `close()`. Counters reuse the canonical
existing class, preserving all three uint64 fields without reset or narrowing.

Three outcome domains remain distinct:

1. Native status, converted to `MelError.kind` and `native_status`.
2. Callback registration `CommandReturn`, available as
   `CameraProtectResponseOpenError.registration` only if a known value was
   independently published.
3. Received `CameraProtectStatus`, including normal provider data UNAVAILABLE
   and UNSUPPORTED, neither of which is a registration failure or MEL rejection.

## Registration and ownership

Registration is explicitly opt-in on an Attached or Enabled C2 channel. It
never enables the channel, submits a command or registers ordinary metadata.
Capacities must be positive integers representable in native size_t (bool is
not accepted). Invalid local arguments and allocations before invocation do
not consume eligibility. The Python output owner slot exists before invoking
native registration, so synchronous delivery is supported.

Each native attempt consumes a Python one-shot, including native failure,
invocation exceptions and diagnostic corruption. Closing does not reset it.
The ordinary C2 metadata one-shot is independent. No Python Session or
ControlChannel reference is retained in the stream; provider and callback
lifetime remain native responsibilities.

For an unsupported provider, Open raises `CameraProtectResponseOpenError` with
native `PROVIDER_FAILED` and known `CommandReturn.NOT_SUPPORTED`. FAIL remains
distinct. A provider exception is `PROVIDER_EXCEPTION` with registration None.
Unknown/absent Return stays None regardless of diagnostic wording or status.
Unexpected failed-call output is cleaned up once without replacing the primary
error, following PY-MEL-018.

Reception uses exactly the six existing native exports: C2 camera-protect Open
and camera-protect Receive, Get_Counters, Close, Event_View and Event_Close.
No native layout or ctypes signature is redeclared. Before dereferencing the
borrowed fixed-record view, pointer NULL/alignment/address-overflow metadata is
validated. The complete snapshot is converted, status validated, event closed,
and ownership consumption checked before publication. No borrowed memory escapes.

On receive/view/conversion failure, known remaining event ownership gets one
cleanup attempt. Close failure, malformed Close diagnostics and an invocation
exception all make consumption uncertain: no retry is made. Application errors
are not replaced by cleanup errors. Receive MemoryError/OverflowError becomes
INTERNAL_ERROR with the original cause; pre-registration allocation exceptions
retain the existing receiver convention and do not consume the one-shot.

Explicit Close is idempotent after success. Failed/uncertain Close is stored;
subsequent receive/counters/context-entry/Close rejects without another native
Close. Finalization is non-raising and once-only. Context exit preserves a body
exception. Streams cannot be copied, deep-copied or pickled. `is_open` reports
the local handle slot, not health or usable consumption after uncertain Close.
Uncertain ownership may retain resources rather than risk a double release.

## Queue and lifecycle contract

- Zero timeout is a poll; positive timeouts are finite uint32 milliseconds.
- Timeout does not cancel callback registration or provider work.
- Queued events drain before native stopped/failed terminal states.
- Overflow is native DROP-INCOMING; retained events stay FIFO.
- Close deactivates consumption, but does not unregister callbacks or promise
  immediate callback quiescence. C2 teardown retains the native responsibility.
- Operations on one stream, and registration versus parent C2 operations,
  require external serialization; the GIL is not a substitute.

This task does not expose `CameraProtectCmd` submission or its asynchronous
request owner. A metadata callback never proves that such a request completed.
Calibration, scheduling, other C2 response families, physical safety and real
hardware/provider behavior are outside scope.

## Executable mock-provider example

After building native test artifacts, set `PYTHONPATH` to this worktree's
`python` directory, `AMS_MEL_NATIVE_LIB` to its
`native/build-tests/lib/libams_mel_c.so.0`, and
`AMS_MEL_TEST_PROVIDER_DIR` to `native/build-tests/test-providers`:

```python
import os
from pathlib import Path
from ams_mel import common, ir

config = ir.ControlConfig(
    common.UciId(bytes(16), "control"),
    common.UciId(bytes(16), "platform"),
    common.ComponentLocation(0.0, 0.0, 0.0, "sensor", "example"),
)
provider = Path(os.environ["AMS_MEL_TEST_PROVIDER_DIR"]) / "libmock_ir_provider.so"
with ir.Session.open(provider, "protect-callback-rich", "") as session:
    with session.open_control_channel(config) as channel:
        # Attached is sufficient; there is deliberately no enable or submit.
        with channel.open_camera_protect_response(4) as responses:
            first = responses.receive()
            assert first == ir.CameraProtectResponse(
                1, -(1 << 63), ir.CameraProtectStatus.OK)
            assert responses.counters() == ir.C2MetadataCounters(3, 0, 0)
```

## Coverage and qualification

Direct tests exercise public signatures/identity/pickle/frozen values, uint32
and signed timestamp boundaries, canonical counters, local validation and
one-shot behavior, separately published registration Returns, corrupt
diagnostics, missing events, invalid pointer metadata, unknown statuses,
allocation/conversion errors, and at-most-once event/stream Close under uncertain
consumption. Provider status conversion is tested separately from response data.

Fresh child processes exercise `protect-callback-rich`, malformed, overflow,
allocation, register-fail, register-throw, late and in-flight scenarios. They
also cover Attached/Enabled registration, parent-first/consumer-first teardown,
parent garbage collection without retention, and ordinary metadata independence
in both registration orders. In-flight teardown uses the existing provider
file barrier with bounded inotify/select observation, never arbitrary sleeps.
The allocation scenario uses only the existing test-facade failpoint.

The compiled fixtures do not combine protection callbacks with a cleanup-failed
terminal state. Native PROVIDER_FAILED conversion is therefore injected at the
existing receive boundary after draining real events; no native mock expansion
or asynchronous protection command is introduced to manufacture this scenario.
Structural pointer checks cannot prove arbitrary aligned memory is mapped; that
remains the native ABI's responsibility. Qualification is mock-provider only.

### Actual commands and results

Commands run from `/home/zboll/git/ams-py-mel-019`. Logs and exit records are
retained outside the source tree in `/home/zboll/py019-validation`.

| Command | Actual result |
| --- | --- |
| `make native` | PASS, production library built |
| `make test-native` | PASS, 841 tests |
| `make test-python` | PASS, 733 tests; final-source rerun recorded in PR ledger |
| `make test-rust` | PASS, all workspace tests/doc-tests; existing ignored tests remain ignored |
| `make test-policy` | PASS, including its documentation-control dependency |
| `make test-docs-ci` | PASS |
| `make check-ir-api-coverage` | PASS |
| `make check` | Aggregate run and final result recorded in PR ledger |
| `python3 scripts/check_doc_source_integrity.py` | PASS |
| `python3 scripts/check_final_newlines.py` | PASS |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff --check` | PASS |

Focused regression command (with absolute `PYTHONPATH`, `AMS_MEL_NATIVE_LIB`
and `AMS_MEL_TEST_PROVIDER_DIR` set to this worktree's Python/test artifacts):

```sh
python3 -W error -m unittest test_ir_camera_protect_response \
  test_ir_c2_metadata test_ir_camera_response \
  test_ir_image_optical_distortion_map test_ir_image_los_quaternion \
  test_public_modules test_abi -v
```

The initial focused suite passed 204 tests. Final-source reruns include added
successful dual-registration cases; exact results are retained in the PR ledger.
The executable example above passed against the compiled provider. Source-only
reference generation passed with a fresh output directory, and its inventory
contains all four public symbols plus `open_camera_protect_response`. Native-
backed reference qualification and final-source results are in the PR ledger.

### Environment failures and recovery (not hidden passes)

The host `/tmp` exhausted all inodes. An initial build and native tests failed
on temporary file creation; changing TMPDIR alone could not redirect hardcoded
C fixture paths. Qualification was rerun using a task-owned directory mounted
as `/tmp` in an unprivileged private mount namespace. No unrelated files or host
mounts were changed. A first namespace also isolated networking, causing a local
HTTP MIME test to fail with `Network is unreachable` (also reproduced on main).
Policy/docs controls passed after keeping normal networking (`unshare -Urm`).

Two added independence test expectations initially assumed ordinary metadata
was unsupported in the protection fixture; actual provider behavior is Success.
The tests now assert both independent registrations succeed in either order.
An attempted full Python rerun overlapped `make check`'s destructive build-tree
isolation test, causing missing-provider failures; this is not claimed as a pass.
Final Python qualification is sequenced after that aggregate job completes.
A repeated reference generation correctly rejected a non-fresh output directory;
generation into a new directory passed. The first example execution during a
build-tree reset failed to load the temporarily absent library; a stable-artifact
execution passed. Original logs are preserved, not rewritten as successes.

### ABI and delivery

Separately built freshly fetched main and feature production libraries have
identical sorted defined dynamic exports: 332 versioned function exports and
one version-node entry, no additions/removals/version changes. Both complete
inventories have SHA-256
`156c9485e8a06af68932d66f917461d7d7536fa2816dc2df51009d924c2908d3`.
Native sources, headers, layouts, exports map and private ctypes are unchanged.

Exact final commit/tree, final qualification, hosted workflow URLs and review
request evidence belong in the PR ledger, avoiding a recursive self-hash here.
GitHub lists only the author as collaborator/assignable user and repository
auto-merge is disabled. No independent ChatGPT reviewer is available through
this implementation session's tools. A review request is not a completed AI
technical review or a separate-account GitHub approval. Readiness remains
withheld until independent review of the final exact HEAD has no blocking
findings. No approval, merge or auto-merge is performed.
