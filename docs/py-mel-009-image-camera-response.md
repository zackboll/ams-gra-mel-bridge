# PY-MEL-009 — Safe Python Image CameraCommandResp reception

## Source identity and coverage delta

Repository: `https://github.com/zackboll/ams-gra-mel-bridge`.
Dedicated worktree: `/home/zboll/git/ams-py-mel-009`.
Branch: `feature/py-mel-009-image-camera-response`.
Fetched starting main commit: `648d08bfc5d3c8a92c52680f5b1c42bbd515a1e8`.
Starting main tree: `3ca2c4bf3ff74ea8e71c81a803e8e309d2a69b4c`.
PY-MEL-005/006/007/008 implementation commits `c014730`, `7838d4e`,
`aeef59e`, and `80acdb9` are all ancestors of that main. No equivalent safe
Python CameraCommandResp reception API existed there. The PR handoff records
the containing final commit/tree and exact-head hosted checks, avoiding a
self-referential commit identity in this document.

Coverage delta: existing native-only Python ctypes CameraCommandResp reception
becomes a complete safe Python opt-in Image reception slice. Four public
classes and one Image factory method are additive. No native implementation,
ABI, exports, provider, mock, Ada, Rust, compatibility baseline, workflow or
structured Ada coverage changes. Ordinary Image metadata, NUC, C2 and Health
tests remain unchanged. CameraCommand submission and request Wait are explicitly
**unsupported in the public Python API**. No DSP, service, transport, buffering,
correlation, retries or orchestration is added.

Authorities reviewed: `ada/src/ams-mel-ir-camera.ads`,
`ada/src/ams-mel-ir-image-metadata.ads`, `native/include/ams_mel/abi.h`,
`native/src/ir_camera_command.cpp`, `native/tests/test_ir_camera_command.c`,
`native/tests/mock_provider.cpp`, and the existing Python Image, ordinary
metadata, NUC, diagnostic, FFI and owner implementations. Documentation follows
CONTRIBUTING and DEV-PARALLEL-001; the shared historical coverage archive is not
modified.

## Existing native contract mapping

The task's “seven” reception contract elements are **six C functions plus the
independent `out_registration` output**, not seven distinct function symbols:

| Existing native element | Safe Python mapping |
| --- | --- |
| `ams_mel_ir_image_camera_response_open` | `ImageStream.open_camera_response(queue_capacity: int = 16) -> CameraResponseStream` |
| independent `out_registration` uint32 | `CameraResponseOpenError.registration: CommandReturn | None`; sentinel `0xFFFFFFFF` before call |
| `ams_mel_ir_camera_response_receive` | `CameraResponseStream.receive(timeout_ms: int = 0) -> CameraCommandResponse`, exactly one native Receive |
| `ams_mel_ir_camera_response_get_counters` | `CameraResponseStream.counters() -> CameraResponseCounters` |
| `ams_mel_ir_camera_response_close` | `CameraResponseStream.close() -> None`, context management and non-raising finalization |
| `ams_mel_ir_camera_response_event_view` | checked private immutable view, copied before release |
| `ams_mel_ir_camera_response_event_close` | exactly one release attempt for each published event |

All declarations already exist in the byte-unchanged `_native.py`. Implementation
lives in `_camera_response.py`, below `_image.py`, without importing public names
back from `__init__.py`. The latter contains exports/public identity wiring only.
The package dependency graph remains acyclic. Public class `__module__` is
`ams_mel`, preserving introspection and pickle identity.

## Complete public API and example

- `ImageStream.open_camera_response(queue_capacity: int = 16) -> CameraResponseStream`.
- `CameraResponseStream.is_open: bool` (local handle presence, not provider health).
- `CameraResponseStream.receive(timeout_ms: int = 0) -> CameraCommandResponse`.
- `CameraResponseStream.counters() -> CameraResponseCounters`.
- `CameraResponseStream.close() -> None`, `__enter__`, `__exit__`; no direct constructor.
- Frozen `CameraCommandResponse(command_id: int, size: int, timestamp_ns: int,
  command_success: bool)`.
- Frozen `CameraResponseCounters(events_received: int,
  events_dropped_queue_full: int, malformed_or_unsupported: int)`.
- `CameraResponseOpenError(MelError)`, with ordinary `kind`, `diagnostic`,
  `diagnostic_required`, `native_status` and independent `registration`.

With an already-open Image owner:

```python
from ams_mel import CameraResponseOpenError, CommandReturn, ErrorKind, MelError

try:
    with image.open_camera_response(queue_capacity=16) as responses:
        try:
            response = responses.receive(timeout_ms=100)
            print(response.command_id, response.size, response.timestamp_ns,
                  response.command_success)
        except MelError as error:
            if error.kind not in (ErrorKind.TIMEOUT, ErrorKind.STREAM_STOPPED):
                raise
        print(responses.counters())
except CameraResponseOpenError as error:
    if error.registration is not CommandReturn.NOT_SUPPORTED:
        raise
```

Registration is explicit and independent of `open_metadata`, `open_nuc`, Start
and frame Receive. It neither starts Image nor submits a command. Registration
may synchronously deliver responses before Open returns.

## Field and registration semantics

`ams_mel_ir_camera_command_response_v1` maps exactly: command ID and size retain
the full unsigned 32-bit domain; timestamp retains signed int64 nanoseconds,
including both extrema. No epoch, normalization, size meaning or unit conversion
is inferred. `command_success` must be exactly native 0 or 1. False is a normal
response event, **not** a native error or MEL rejection. The canonical owned
response model is reusable by future request work; this task does not expose it.

The outer native view is checked for NULL, alignment, fixed-record extent and
native address overflow before dereference. Conversion returns only Python ints
and bools, never ctypes values or native pointers. Diagnostics use the existing
strict helper, retaining structured categories and complete diagnostic metadata.

Open succeeds only with native OK, a nonnull owner, and published SUCCESS Return.
OK without both required outputs is PROTOCOL_INCONSISTENCY. NOT_SUPPORTED and
FAIL are independently preserved Returns with native PROVIDER_FAILED. A provider
registration throw yields PROVIDER_EXCEPTION and no published Return (`None`).
Unwritten/invalid Return is `None`, never inferred from text. Local invalid
capacity or closed Image yields INVALID_ARGUMENT, no native status, and
`registration=None`, before calling native. Capacity is a positive size_t;
bools/non-integers/overflow reject. Such local validation does not consume the
one-shot. A real registration attempt is one-shot even on failure or partial
registration failure; subsequent attempts reject locally. Unsupported registration
leaves ordinary Image metadata and the Image owner usable.

## Queue, counters and ownership

The native queue is bounded FIFO, DROP-INCOMING. Python adds no queue, lock,
callback, worker, polling service or reordering. Counters preserve full uint64
snapshots and count callbacks, not Python Receive calls. NULL callback payloads
and callback copy/allocation failure increment malformed counts, without failing
the stream or discarding other valid retained events. Active-empty Receive is
TIMEOUT; Image Stop drains queued responses before STREAM_STOPPED. Neither
timeout nor stop implies cancellation. Responses never establish future completion.

The response stream owns only its native public handle, retaining no Python
Image/Session to extend lifetime. Native callback state manages provider retention.
Close deactivates public consumption; it does not unregister callbacks, close
Image/Session, cancel requests or promise immediate callback quiescence. Close
is idempotent after successful release. Failed/uncertain Close is never retried;
later explicit calls re-raise the first failure, not silent success. A claimed
successful Close retaining a handle is PROTOCOL_INCONSISTENCY and blocks use.
Non-raising finalization makes at most one best-effort native Close attempt.
An event published even on Receive failure is released once; conversion failures
(including MemoryError and KeyboardInterrupt) preserve the first exception during
cleanup. Event Close failures/retained handles are never automatically retried.
Open failure with an unexpectedly published owner also attempts cleanup once,
without replacing the Open error.

Externally serialize operations and Close on one response owner, and registration
versus parent Start/Stop/Close. The GIL is not a native ownership lock. Values
survive response Close, Image Stop/Close, Session Close and actual provider unload.

## Baseline and qualification evidence

Starting-main native baseline: `make test-native`, **523/523 passed**.
Starting-main Python baseline: **456 tests passed**, warnings-as-errors, from a
complete `git archive 648d08b` source export using this worktree's unchanged
compiled facade/provider. The initial background Python attempt overlapped
implementation edits and is not baseline evidence; a Python-only archive lacked
native layout-probe files and was also discarded. The complete-source rerun
provides the uncontaminated baseline.

Feature tests: `python/tests/test_ir_camera_response.py`, **26 test methods**,
with parameterized defensive cases and six fresh-interpreter scenarios. Native
fixture expectations were taken from `native/tests/test_ir_camera_command.c`.

| Existing mock scenario | Evidence |
| --- | --- |
| `camera-callback-rich` | synchronous Open delivery; three ordered responses `(1,0,INT64_MIN,True)`, `(UINT32_MAX,UINT32_MAX,INT64_MAX,False)`, `(0,7,-1,True)`; active-empty timeout |
| `image-metadata-sync` | exact NOT_SUPPORTED; ordinary metadata remains receivable and Image Start/Stop works |
| `camera-register-fail` | exact FAIL; one-shot partial registration; late callback before unload in isolated process |
| `camera-register-throw` | PROVIDER_EXCEPTION; unpublished Return; isolated teardown/unload |
| `camera-callback-malformed` | received=2, malformed=1; valid `(9,9,9,True)` remains |
| `camera-callback-overflow` | capacity=2; received=5, dropped=3; FIFO retains IDs 0,1 |
| `camera-callback-allocation` | isolated global failpoint consumed; received=2, malformed=1; valid ID 2 remains |
| `camera-late` | owner-first/parent-first closure; ordered late callback invoke/return then actual DSO unload |
| `camera-sync` | test-only direct private native submission/Wait: callback `(id,77,-77,True)` available before Wait; request result `(id,123,456,True)` is distinct |
| `nuc-rich` | NUC remains usable despite unsupported CameraCommandResp; ordinary metadata independent |

Unload evidence uses native lifecycle logs and `/proc/self/maps`, in subprocesses,
not sleeps. Returned values and pickle round-trips are checked after unload.
Parent/Session weakrefs prove there is no Python parent retention. Tests also
cover public signatures, defining-module identity, frozen values, context use,
finalization call counts, no implicit registration/start/submission, and full
uint32 timeout forwarding.

Hostile injection covers command_success 2/UINT32_MAX, NULL/misaligned/overflow
outer views, missing successful event owner, failed Receive with a published
event, View failure, malformed diagnostics, consumed/retained failed event and
owner Close, claimed-success retained handles, Open failure with owner, OK with
NULL owner/invalid/unwritten/non-SUCCESS Return, conversion MemoryError/
KeyboardInterrupt, cleanup failure while another exception is active, full
uint64 counters, all existing status categories/unknown status and invalid timeout
before native Receive. Call counts and original exception identity are checked.
Retained test handles are manually reclaimed only by test harness teardown after
proving the production wrapper never retries. No arbitrary unmapped address is
dereferenced: invalid pointers are rejected structurally before access.

Sequential local qualification (mock-only):

| Command | Actual result |
| --- | --- |
| `make test-native` | PASS: 523/523 CTest cases; includes C11 translation units and warning-as-error native builds. |
| `make test-python` | PASS: 482 tests (456 existing + 26 feature); Python warnings-as-errors; prerequisite native 523/523 also passed. |
| `make test-policy` | PASS: 234 tests across seven suites (18 + 18 + 28 + 64 + 75 + 18 + 13). Intentional failing fixture output is negative-control evidence, not a failed gate. |
| `make check-ir-api-coverage` | PASS: offline audit plus 52 regression tests; existing Ada inventory unchanged. |
| `python3 scripts/check_doc_source_integrity.py` | PASS: 824 vendor files, three workflows. |
| `python3 scripts/check_final_newlines.py` | PASS after correcting the three new files' terminal newlines. |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS. |
| Explicit nine-module `python3 -W error -m unittest -v ...` run listed below | PASS: 279 tests. |
| `git diff --check` | PASS. |

The final-newline checker found missing terminal newlines in the three newly
created files on its first pass; these were corrected before final qualification.
The final committed source is requalified with the same sequential native/Python/
policy/coverage commands, source integrity, final-newline check,
`python3 -W error -m compileall -q python/ams_mel python/tests`, and
`git diff --check`. The explicit warnings-as-errors module run covers
`test_ir_camera_response`, `test_ir_image_metadata`, `test_ir_nuc`,
`test_ir_c2_metadata`, `test_ir_health_metadata`, `test_image_stream`, `test_abi`,
`test_import_identity`, and `test_package_layout`. The PR handoff records final
counts and commit/tree. Scope checks compare native code/exports, `_native.py`,
the public compatibility baseline and existing tests against starting main;
the only intended changes are the six Python feature/documentation files listed
in the PR. AST dependency inspection confirms all 24 implementation modules
remain acyclic and production reception references exactly the six existing
native functions above.

No real provider, Squall, credentials, hardware or provider code changes were
used. No Ada source was edited, so no Ada formatting/build claim is made.
Full local GNATdoc generation is not claimed; exact-head hosted Bootstrap and
Ada API Documentation checks are reported separately at delivery.

## Limitations

Qualification is compiled-mock-only, not real-provider/hardware interoperability
or formal proof. C++ provider ABI compatibility is still required. Structural
pointer checks cannot prove that an arbitrary aligned pointer refers to mapped
storage; the native ABI contract remains responsible. Uncertain cleanup can
require retaining a native allocation rather than risking double-free. There is
no unregister/quiescence guarantee, thread-safety claim, retry/replay/correlation
policy, or public CameraCommand submission/request Wait. No native, Ada or Rust
behavior or coverage classification changes are claimed.
