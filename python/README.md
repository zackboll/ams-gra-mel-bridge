# Python MEL binding

This directory provides the dependency-free `ams_mel` Python API over the
existing native façade:

```text
Python safe API -> private ctypes -> ams_mel_c -> published C++ MEL provider
```

Set `AMS_MEL_NATIVE_LIB` to the absolute full path of `libams_mel_c.so.0` (or
`libams_mel_c.so`) before importing `ams_mel`. The provider library remains a
separate path passed to `Session.open` or `RFAdmin.open`; Python never loads
provider factories or models provider C++ interfaces directly.

## Internal package layout (PY-PKG-001)

`ams_mel/__init__.py` explicitly re-exports the existing public API. Public
classes and enums retain `__module__ == "ams_mel"` for introspection and pickle;
the exports are the implementation objects themselves, not wrappers or copies.
Consumers should continue importing public names from `ams_mel`.

The implementation dependency graph is acyclic. In this diagram arrows mean
"may import from" (higher layers depend on lower layers):

```text
_errors -> _native
_validation -> _errors
_types -> _native, _errors, _validation
_ffi -> _native, _errors, _validation, _types
_requests -> _native, _errors, _validation, _types
_common -> _native, _errors, _types, _ffi, _requests
_health_types -> _types
_health_convert -> _native, _errors, _types, _ffi, _health_types
_health_metadata -> _native, _errors, _validation, _common, _ffi, _health_types, _health_convert
_nuc -> _native, _errors, _validation, _common, _ffi, _types
_image_types -> _health_types (canonical Euler)
_image_convert -> _native, _errors, _ffi, _health_types, _image_types
_image_metadata -> _native, _errors, _validation, _common, _ffi, _image_types, _image_convert
_c2_types -> _types, _health_types (canonical BitStatus)
_c2_convert -> _native, _ffi, _health_convert, _c2_types, _types
_c2_metadata -> _native, _errors, _validation, _common, _ffi, _c2_types, _c2_convert
_health -> shared layers above, _health_metadata, _nuc
_image -> shared layers above, _nuc, _image_metadata
_c2 -> shared layers above, _c2_metadata
_instrumentation -> shared layers above
_rf_admin -> _native, _errors, _validation, _types
_session -> _native, _errors, _validation, _types, _ffi, family implementations
__init__ -> explicit public exports from implementation modules
```

Responsibilities:

- `_errors`: structured errors, native status mapping and diagnostic handling.
- `_validation`: numeric/timeout/size and input-string validation, independent of
  public value models.
- `_types`: frozen value models, enums, command/report values and model validation.
- `_ffi`: raw argument bundles retaining borrowed string storage, checked spans,
  capability snapshots/conversion, and best-effort close utilities.
- `_requests`: Mode/Return/Comms owners and their native-cached Wait handling.
- `_common`: weak `ChannelView`, common operations and shared `_NativeOwner`
  mechanics (the existing implementation, not a new ownership model).
- `_image`, `_c2`, `_instrumentation`, `_health`, `_rf_admin`: family owners and
  operations. Instrumentation's specialized Wait/metadata conversion stays with
  that family; image-frame conversion stays with Image.
- `_health_types`, `_health_convert`, `_health_metadata`: immutable Health
  metadata models, checked deep copies and independent native consumption.
- `_image_types`, `_image_convert`, `_image_metadata`: ordinary four-family
  Image metadata values, active-only conversion and independent consumption.
- `_c2_types`, `_c2_convert`, `_c2_metadata`: command/BIT configuration models,
  canonical Health BIT conversion reuse, and independent native C2 consumption.
- `_session`: Session lifetime, version queries and typed channel factories.

No implementation module imports public names back from `__init__`. `_native.py`
remains the single, unchanged private ctypes module, not a package. The original
PY-PKG-001 layout refactor added no API or lifecycle semantics; PY-MEL-005 adds
Health metadata vertically in separate modules without changing that baseline.
PY-MEL-007 adds Image metadata through the same modular layering.

`tests/public_api_baseline.json` records the pre-refactor API from main commit
`d1847d3e8043704352deedfc7f3f680d9752fce3` (tree
`5d6c8a6ad05b63156caf4941e0be7048e58cc379`). `test_package_layout.py` compares all
54 ordered exports, public identities/metadata, package-defined method
signatures, enum values/representations, dataclass fields/defaults/frozen status,
and representative errors. Interpreter-provided inherited methods are excluded
from signature snapshots so Python minor-version changes are not mistaken for
binding changes. Additional tests check defining-module object identity, the
import graph, fresh-process imports and representative pickle round-trips.
Existing native-backed lifecycle tests remain unchanged except for four private
failure-injection patch targets, now patched in their implementation modules.

## Supported behavior

The current API supports RF Admin state commands (see below), façade ABI/version
queries, provider `Session` open/version/close, host-memory Mono8 `ImageStream`
open/start/receive/counters/stop/close, and the existing IR CommandAndControl
Operate/TaskSched plus BIT no-op slices. `Session.open_control_channel()` returns
a `ControlChannel`; `enable()` is explicit. `submit_operate()` returns an
asynchronous `ModeRequest`, while `submit_bit_noop()` returns a `ReturnRequest`
without exposing unsupported BIT payloads. Return waits yield `ReturnCompleted`
with a typed `CommandReturn` or structured `ReturnRejected`; `CommandReturn.FAIL`
is a normal completion. Zero polls, timeout does not consume or cancel either
request, and terminal results may be read again.
Complete rejection descriptions are preserved, including text larger than the
initial diagnostic buffer. Known MEL error codes have named enum values; unknown
rejection codes remain structured mode/Return rejections and preserve their
exact uint32 value.

Frames preserve the supported metadata, and `Frame.pixels` is an owned `bytes`
copy: no view into ctypes storage, provider storage, or the native receive queue
escapes. Timeout and clean stream stop are distinct structured errors.

`ImageStream`, `ControlChannel`, `HealthStatusChannel`, `ModeRequest`, and `ReturnRequest` independently
own their native handles and do not retain Python parent objects. Session and
control owners may close first while a pending request remains valid. Request close only releases
the public owner and does not cancel provider work. Native C2 close decides
whether cleanup cleared the owner: a retryable detach failure leaves the Python
object open for another explicit close, while a cleared cleanup failure leaves
it closed.

Operations using one owner must follow the native external-serialization
contract. At most one receive operation may consume a stream at a time; request
wait must not race its request close; and control enable, both submissions, and
close must be serialized. The GIL is not a substitute for those contracts. Context managers
do not implicitly start streams or enable controls. If a `with` body raises,
cleanup is attempted without replacing that exception; close failures propagate
on normal context exit.

This is not a native extension or a zero-copy interface. The current Session +
IR Mono8 + C2 Operate/TaskSched + BIT no-op slice is also validated by the opt-in real Squall
client in `integration/squall`; that client adds no binding API and uses no
external Python dependency. Payload-bearing BIT, additional C2 commands and
callbacks beyond the four metadata families below, RF beyond the Admin slice,
NumPy/zero-copy image
views, wheels, and PyPI publication are not included.

## IR C2 metadata (PY-MEL-008)

`ControlChannel.open_metadata(queue_capacity=16)` returns an independent
`C2MetadataStream`. It registers exactly three base families, in native order:
**BIT_Configuration, CommandStatus, BIT_Status**. It works while C2 is Attached
or Enabled, never implicitly enables C2, and applies no capability-advertisement
gate. Callbacks can arrive synchronously during registration. Base registration
is one-shot per native C2 channel, including partial/failed attempts; closing the
consumer is not rollback and does not permit reopening.

CommsTest is **separately opt-in**, with its own one-shot registration, also
consumed on failure. Failure does not undo the base three registrations:

```python
with control.open_metadata() as metadata:
    metadata.enable_comms_test_events()
    event = metadata.receive(timeout_ms=1000)
    counts = metadata.counters()
```

The provider must actually emit an event; registration does not submit a command.
All four families share **one native bounded FIFO and the same counters**. Order
is successful enqueue order, not timestamp order. The native policy is
**DROP-INCOMING** when full; existing queued values are not replaced. Malformed
callbacks are counted separately. `C2MetadataCounters(events_received,
events_dropped_queue_full, malformed_or_unsupported)` preserves full uint64
values, and receiving never resets the counters. Zero timeout polls; an active
empty queue reports `TIMEOUT`. Queued values drain before `STREAM_STOPPED` or
`PROVIDER_FAILED`, according to the native lifecycle contract.

| Native family / kind | Frozen owned Python payload |
| --- | --- |
| `CommandStatus` / `COMMAND_STATUS = 1` | `C2CommandStatus(command_id, state, reason, reason_description)`; full uint32 ID, exact `C2CommandState` (0..4), exact 47-value `C2CannotComply` (0..46), complete UTF-8 text |
| `BIT_Configuration` / `BIT_CONFIGURATION = 2` | `C2BitConfiguration(bit_types)`; tuple of `C2BitType(bit_id, accepted_interface, bit_item_names, subsystem_component_ids, expected_duration_ns)` |
| `BIT_Status` / `BIT_STATUS = 3` | Existing canonical `BitStatus`, `ActiveBit`, `CompletedBit`, `CompletedBitItem`, `Fault`, `FaultData`, `FaultAmbiguityGroup`, `BitResult`, `FaultSeverity`, `FaultState` |
| `ChannelCommsTestRep` / `CHANNEL_COMMS_TEST = 4` | Existing `CommsTestReport(command_id, request_id)`; no channel ID is present |

`C2MetadataEvent(kind, payload)` enforces exact `C2MetadataKind`/payload agreement.
Enums reject unpublished raw values with `PROTOCOL_INCONSISTENCY`; the published
`C2CannotComply.UNKNOWN = 44` remains a valid, distinct category. BIT control
mapping is exactly `NOT_SET=0`, `SUBSYSTEM_BIT_COMMAND=1`,
`SUBSYSTEM_STATE_COMMAND=2`, `SUBSYSTEM_INITIATED=3`. The pinned prose for 2/3 is
contradictory; Python does not swap or reinterpret them. IDs include exact UUID
bytes and descriptive labels. Ordered/duplicate/empty values, signed int64
nanosecond durations/timestamps, out-of-range percentages and nonfinite doubles
are copied unchanged, without epoch inference or normalization. This metadata
does not synthesize supported tests from capabilities or add BIT execution APIs.

**ACCEPTED means command-message acceptance, not completion of physical activity.**
CommandStatus events are independent of `ModeRequest`/`ReturnRequest.wait()`;
CommsTest metadata and `CommsRequest.wait()` are separate native observations.
Metadata receipt neither completes nor replaces any request result.

| Python operation | Existing native C mapping (`ams_mel_ir_c2_` prefix) |
| --- | --- |
| `control.open_metadata(queue_capacity=16)` | `metadata_open` |
| `metadata.is_open` | Local owner presence, not C2 state or callback health |
| `metadata.enable_comms_test_events()` | `metadata_register_comms_test` |
| `metadata.receive(timeout_ms=0)` | `metadata_receive`, `metadata_event_view`, `metadata_event_close` |
| `metadata.counters()` | `metadata_get_counters` |
| `metadata.close()`, context management, finalizer | `metadata_close` |

Each event is deeply copied and its native event owner released **before return**.
No ctypes objects, native pointers or borrowed memory escape; copied values
survive actual provider unload. Pointer/alignment/span/text checks fail closed,
but cannot prove that arbitrary nonnull addresses supplied by a violated C ABI
refer to mapped memory. Native code remains responsible for valid memory.

The metadata owner retains no Python ControlChannel or Session. C2/Session may
close first; queued events remain drainable. Metadata Close does not close C2,
cancel pending Mode/Return/Comms requests, unregister provider callbacks, or
guarantee immediate callback quiescence. Native code alone retains provider
resources as needed. Successful Close is idempotent. A failed Close preserves
the native error; an uncertain retained handle is not blindly retried or reported
as successfully closed. Finalization is one non-raising best-effort attempt.
Context management preserves a body exception even if cleanup also fails.

**Externally serialize receive, counters, registration and Close on a metadata
owner, and metadata registration with parent C2 operations.** The GIL is not this
contract. No Python worker, callback, lock or queue is added.

For implementation coverage, compiled-mock qualification, source identities and
limitations, see the [PY-MEL-008 feature report](../docs/py-mel-008-ir-c2-metadata.md).

## RF Admin (PY-MEL-001)

```python
from pathlib import Path
from ams_mel import MFAState, RFAdmin

with RFAdmin.open(Path("/path/to/provider.so"), "provider-defined configuration") as admin:
    accepted = admin.command_state(MFAState.STANDBY)
    # True means command acceptance, NOT a completed hardware state transition.
    # False is normal provider rejection; native failures raise MelError.
```

`RFAdmin` is an independently owned AdminMEL, not an IR `Session`, DataMEL or
C2MEL. It has no Python parent dependency. `open(provider_library, configuration)`
accepts a string or `os.PathLike[str]` provider path and a string configuration;
both must encode as strict UTF-8 without embedded NUL. Configuration is forwarded
verbatim, including empty text. Byte paths, surrogate text and invalid argument
types raise `MelError` with `ErrorKind.INVALID_ARGUMENT` before any native call.

`is_open` inspects only the public native handle, not provider health. All
operations on one owner, including `command_state`, `close` and finalization,
require external serialization; the GIL is not a lock for this contract.
`command_state(state) -> bool` accepts `MFAState` or an integer in 0..14, never
Boolean, float, narrowed or wrapped input. Missing required interfaces and
provider exceptions raise `MelError`, not `False`; a successful call with a native
Boolean other than zero or one is a protocol error. No state-transition model,
polling, completion wait, hidden retries or cleanup thread is provided.

Explicit `close()` is idempotent. The native handle is consumed and shutdown is
called at most once, even when shutdown throws. Such a failure raises `MelError`
but leaves `is_open` false: repeated close does not retry. Native code permanently
retains the uncertain provider/library graph; Python must not try to destroy or
unload it. Successful shutdown destroys AdminMEL before releasing its library.
Finalization attempts non-raising closure. Context managers propagate cleanup
failure on normal exit but preserve an active body exception if cleanup also
fails. Closed-owner commands and context entry raise `INVALID_ARGUMENT`.

### Ada/API parity

| Ada authority | Python API | Shared semantics |
| --- | --- | --- |
| `AMS.MEL.RF.Admin.Open` / `Admin_MEL` | `RFAdmin.open` / `RFAdmin` | Independent createAdminMEL owner |
| `Is_Open` | `is_open` | Local handle only |
| `Command_State` | `command_state` | Provider acceptance Boolean, not transition completion |
| `Close` / `Finalize` | `close` / finalizer / context manager | Shutdown once; consume on failure; native retention |
| `AMS.MEL.Status.MFA_State` | `MFAState` | All fifteen representations below; no new RF state model |

| Value | Python | Ada |
| --- | --- | --- |
| 0 | `NOT_SET` | `Not_Set` |
| 1 | `UNKNOWN` | `Unknown` |
| 2 | `NOT_INSTALLED` | `Not_Installed` |
| 3 | `OFF` | `Off` |
| 4 | `PRE_INITIALIZATION` | `Pre_Initialization` |
| 5 | `INITIALIZATION` | `Initialization` |
| 6 | `STANDBY` | `Standby` |
| 7 | `OPERATE` | `Operate` |
| 8 | `OPERATE_RX_ONLY` | `Operate_Rx_Only` |
| 9 | `OPERATE_TX_ONLY` | `Operate_Tx_Only` |
| 10 | `MAINTENANCE` | `Maintenance` |
| 11 | `CALIBRATION` | `Calibration` |
| 12 | `INITIATED_BIT` | `Initiated_BIT` |
| 13 | `SHUTDOWN` | `Shutdown` |
| 14 | `DEGRADED` | `Degraded` |

Only the existing `ams_mel_rf_admin_open`, `ams_mel_rf_admin_command_state` and
`ams_mel_rf_admin_close` exports are used. No C ABI, private ctypes declarations,
Ada, Rust or provider mock changes are needed. RF status queries, callbacks,
erase, BIT payloads and all other Admin interfaces remain out of scope.

Diagnostics use the existing `MelError` handling: complete native text, native
status and required byte count (including NUL) are preserved when the text fits
the existing 4096-byte buffer. **Unresolved diagnostic boundary:** these three
exports have no post-call diagnostic retrieval operation. For a diagnostic larger
than that buffer, existing handling reports `diagnostic=None` plus the exact
required size and native status; it never passes off a truncated prefix as a
complete message or repeats a side-effecting operation to fetch text. Arbitrarily
long diagnostic preservation cannot be promised by this Python-only change.

The RF tests use the existing compiled mock provider for all lifecycle/error
scenarios and all fifteen state representations. Failed-shutdown retention is
isolated in subprocesses. Malformed native result/diagnostic boundaries are
injected around actual native calls without altering the provider mocks. This is
mock qualification, not real hardware transition evidence.

From the repository root, run:

```sh
make test-python
```

Binding features using existing C interfaces follow ordinary feature branches,
PRs, functional/ABI tests, hosted CI, and code review. No filename admission,
historical source freeze, or separate source-policy task is required. Tests
check that `ams_mel._native` is the intended private `_native.py` module, not
an accidentally shadowing package. Private bindings may evolve with ABI tests;
they are not byte-frozen.

With the exact pinned external Squall checkout and a supported container runtime,
the separate real-provider validation is:

```sh
SQUALL_SOURCE_DIR=/path/to/squall make test-squall-ir-python
```

## IR Instrumentation commands (PY-MEL-002)

This Python slice implements typed attachment, explicit Enable, complete owned
capabilities, a weak common Channel view, and asynchronous recording-level
commands using only the existing published C ABI. Metadata reception is provided
by PY-MEL-003 below; neither slice adds C2, Track, Health, RF, or Image commands.
The private ctypes layouts and all native exports are unchanged.

```python
from ams_mel import (
    ComponentLocation, InstrumentationCompleted, InstrumentationConfig,
    InstrumentationLevelCommand, InstrumentationPriority, Session, UciId,
)

config = InstrumentationConfig(
    UciId(bytes(range(16)), "recording channel"),
    UciId(bytes(range(16, 32)), "platform"),
    ComponentLocation(1.25, -2.5, 3.75, "sensor-1", "aircraft"),
)
with Session.open("/path/to/provider.so", "instance", "aperture") as session:
    with session.open_instrumentation_channel(config) as channel:
        attached_capabilities = channel.capabilities()  # valid before Enable
        channel.enable()  # Open and context entry never enable implicitly
        with channel.submit_level(InstrumentationLevelCommand(
            0xE1234567, InstrumentationPriority.DEBUG,
        )) as request:
            result = request.wait(1000)
            if isinstance(result, InstrumentationCompleted):
                report = result.report
                print(report.command_id, report.size, report.timestamp_ns, report.priority)
            else:
                print(result.code, result.description)  # explicit MEL rejection
```

### Public values and Ada mapping

All six new value models are immutable Python values; no ctypes object or
borrowed native storage escapes. UCI UUID bytes/labels and component location
offsets/identity strings reuse the existing models and UTF-8/NUL validation.
`InstrumentationConfig.new(...)` is also available, matching other Python configs.
The exact native config always selects `AMS_MEL_IR_CHANNEL_INSTRUMENTATION`.

| Ada `AMS.MEL.IR.Instrumentation` | Python | Semantics |
| --- | --- | --- |
| `Create_Config` / `Instrumentation_Config` | `InstrumentationConfig` | Channel ID, platform ID, component location |
| `Instrumentation_Priority` / `Normal`, `Debug` | `InstrumentationPriority.NORMAL=0`, `.DEBUG=1` | Only two published representations |
| `Instrumentation_Level_Command` | `InstrumentationLevelCommand` | Full uint32 command ID and checked priority |
| `Instrumentation_Report` | `InstrumentationReport` | Full uint32 command ID and size, signed int64 timestamp_ns, checked priority |
| `Open`, `Is_Open`, `Enable`, `Capabilities`, `Close` | `Session.open_instrumentation_channel`, `InstrumentationChannel.is_open/enable/capabilities/close` | Independent typed owner; Attached → Enabled; failed Enable blocks submission |
| Common `AMS.MEL.IR.Channel.From_Instrumentation` | `InstrumentationChannel.channel_view()` | Existing weak `ChannelView`, including capabilities/KeepAlive/CommsTest |
| `Submit`, `Instrumentation_Request`, `Wait` | `submit_level`, `InstrumentationRequest`, `wait` | Submission is not completion; finite Wait, zero polls |
| Success/Rejected `Instrumentation_Result` | `InstrumentationCompleted(report)` / `InstrumentationRejected(code, description)` | Complete owned report or explicit typed rejection |
| `Close` / `Finalize` | `close`, context manager, non-raising finalizer | Release public ownership, never cancel work |

Command IDs, timeout milliseconds and report size must be uint32; booleans,
floats, strings, negatives and oversized IDs/timeouts are rejected without
wrapping. Priority inputs accept the enum or exact integers 0/1, canonicalized
to the enum. Report timestamps preserve signed int64 exactly: Python neither
invents timestamps nor interprets size units. A report may contain a command ID
different from the submitted command; all four returned fields are preserved.

For this Instrumentation result **only**, published MEL rejection codes 0..8
are valid. Unknown codes, invalid report priorities, null/malformed outputs,
provider send/future exceptions and other native failures raise `MelError`.
This does not change the older C2 result models' unknown-code preservation.
Rejection descriptions larger than 512 bytes are recovered with one checked
native-cached `Wait(0)`; status, code, required size and all report fields must
remain identical. No provider command is resubmitted. Allocation or malformed
diagnostic failures leave the request available for explicit close/later Wait.

### Ownership and concurrency

- Open attaches without enabling. Successful Enable is natively idempotent;
  failed Enable does not admit commands. Capabilities are copied completely
  before the native snapshot closes and survive provider unload.
- Channel and request owners do not retain Python parents. A channel remains
  usable after Session close. A pending request remains valid after both public
  channel and Session close, because the native future retains the provider graph.
- A common `ChannelView` is weak: closing it never closes the typed owner, and
  keeping it alive does not retain that owner or the provider. After teardown,
  the view still owns its wrapper but operations fail with `PROVIDER_FAILED`.
- Timeout raises the existing structured `MelError` with `ErrorKind.TIMEOUT`,
  preserves the request and is not cancellation. Terminal values are cached by
  native code; repeated successful Wait returns the same value, not necessarily
  the same Python object. Python adds no worker, implicit retry, polling service,
  or independent result cache.
- Request close only drops its public owner, never cancels provider work. Channel
  close follows the returned native handle: a retryable detach failure may leave
  it open; a consumed handle stays closed even if cleanup reports failure.
  Repeated successful close is harmless. Python never forcibly destroys or
  unloads uncertain native ownership. A finalizer makes one non-raising cleanup
  attempt; uncertain provider graphs can consequently remain retained.
- Externally serialize operations and close on each owner. In particular, Wait
  must not race request close, and channel enable/submit/close must be serialized.
  The GIL is not a substitute. Context-manager cleanup does not replace an
  exception raised by the body; normal exit reports close failures.

### Existing native exports used

The feature uses `ams_mel_ir_instrumentation_open`, `_enable`,
`_get_capabilities`, `_submit_level`, `_request_wait`, `_request_close`, `_close`,
`ams_mel_ir_channel_from_instrumentation`, and existing common capability
`ams_mel_ir_channel_capability_view` / `_close`. Views reuse the existing common
Channel exports unchanged. There are no new C declarations, layouts or exports.

### Qualification and limitations

`python/tests/test_instrumentation.py` uses the existing compiled IR mock for
configuration fidelity, Normal/Debug commands, the rich four-field report,
terminal cache, typed/oversized rejection, provider failures, explicit enable,
complete capability ownership, weak views and parent-first lifetimes. Held
provider futures use explicit entered/closed/release markers and native owner
retirement observations, not guessed completion delays. Pending/uncertain
retention scenarios are subprocess-isolated. Controlled boundary injection
tests malformed responses the provider cannot publish, including retry identity,
allocation/diagnostic failures, snapshot release and consumed cleanup errors.

```sh
make test-native
make test-python  # all pre-existing tests, ABI/C11 layout probe, import identity
PYTHONPATH="$PWD/python" \
 AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
 AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
 python3 -W error -m unittest discover -s python/tests -p test_instrumentation.py -v
python3 -W error -m compileall -q python/ams_mel python/tests
python3 scripts/check_doc_source_integrity.py
make test-policy
python3 scripts/check_final_newlines.py
git diff --check
```

Positive Instrumentation behavior is mock-qualified, not hardware-qualified.
The pinned Squall provider does not attach Instrumentation channels, so this
feature cannot promise real recording behavior. Provider C++ ABI compatibility
and native retention of never-completing futures remain necessary. Diagnostics
from non-Wait side-effecting calls retain the existing 4096-byte limitation:
oversized text yields the native status and exact required size, not a truncated
message or a repeated operation. Ordinary feature tests and the streamlined
source/security validators apply; no new source-policy authorization or filename
allowlist is needed.

## IR Instrumentation metadata (PY-MEL-003)

`InstrumentationChannel.open_metadata(queue_capacity=16)` returns an independent
`InstrumentationMetadata` owner, matching `AMS.MEL.IR.Instrumentation.Metadata`.
Registration is allowed while Attached or Enabled and does not enable the channel.
The positive capacity must fit native `size_t`; bool, zero, negatives, non-integers,
and overflow are rejected before native registration. Registration is **one-shot
per native channel**, including failed attempts. Closing metadata does not permit
another registration.

```python
with session.open_instrumentation_channel(config) as channel:
    with channel.open_metadata(queue_capacity=16) as metadata:
        channel.enable()
        report = metadata.receive(timeout_ms=1000)
        print(report.command_id, report.size, report.timestamp_ns, report.priority)
        activity = metadata.counters()
        print(activity.events_received, activity.events_dropped_queue_full,
              activity.malformed_or_unsupported)
```

The provider may invoke callbacks synchronously during registration or command
send. Only the existing C++ façade registers callbacks, validates/copies payloads,
and queues reports. Python registers no callback and adds no callback thread or
background polling. Metadata reports and asynchronous command completion are
independent; neither receiving nor waiting consumes the other's result.

| Ada `AMS.MEL.IR.Instrumentation.Metadata` | Python | Existing C ABI suffix under `ams_mel_ir_instrumentation_` |
| --- | --- | --- |
| `Open` / `Metadata_Channel` | `channel.open_metadata(queue_capacity=16)` / `InstrumentationMetadata` | `metadata_open` |
| `Is_Open` | `metadata.is_open` | Local owner presence, not callback activity |
| `Receive` | `metadata.receive(timeout_ms=0)` → existing `InstrumentationReport` | `metadata_receive`, `metadata_event_view`, `metadata_event_close` |
| `Counters` / `Metadata_Counters` | `metadata.counters()` → frozen `InstrumentationMetadataCounters` | `metadata_get_counters` |
| `Close` / `Finalize` | `metadata.close()`, context manager, non-raising finalizer | `metadata_close` |

Receive validates a uint32 timeout in milliseconds (bool/non-integers/overflow
are rejected); zero polls. Native TIMEOUT, STREAM_STOPPED, and PROVIDER_FAILED
remain distinct `MelError.kind` values, with native status and the existing
structured diagnostic behavior. Successful receive must publish an event owner
and a nonnull view of exactly report kind 1 with priority NORMAL (0) or DEBUG (1).
Python copies full uint32 command ID and size, signed int64 nanosecond timestamp
without conversion, and typed priority before closing the temporary event.
No ctypes object, borrowed view, or provider pointer escapes. Reports survive
event release and closure of every owner. Events are also released on malformed
views, conversion exceptions, and unexpected owner publication on failed receive;
cleanup failure never replaces an existing exception.

The bounded FIFO uses **DROP-INCOMING**, not oldest-event eviction. Ordering is
successful enqueue order, not timestamp or concurrent callback entry order.
Counters preserve native uint64 values as Python integers and saturate natively:
`events_received`, `events_dropped_queue_full`, and `malformed_or_unsupported`.
They measure callback activity, not successful receives, and reads never reset
them. Null payloads and invalid priorities are counted malformed; callback
allocation failure instead fails the stream. Already queued reports drain before
the empty failed queue returns PROVIDER_FAILED.

### Metadata ownership and serialization

- Metadata retains no Python Session or typed-channel parent. The native owner
  provides the necessary lifetime; callback state remains channel-owned.
- Typed-channel close deactivates metadata, but an existing metadata owner can
  drain queued reports. An empty inactive queue returns STREAM_STOPPED; an empty
  failed queue returns PROVIDER_FAILED even after channel close.
- Metadata close is idempotent after success and invalidates further receive and
  counters calls. It closes consumption only: it does not unregister callbacks,
  close/destroy the typed/provider channel, cancel requests, or promise immediate
  callback quiescence.
- **Externally serialize receive, counters, and close on one metadata owner.**
  Also serialize metadata open with enable, submit, capabilities, view conversion,
  and close on its typed channel. The GIL is not serialization; no Python lock is
  added. A blocking receive must finish before closing that metadata owner.
- Cleanup follows consumed versus retryable native handles; Python never forces
  unload of an uncertain provider graph. Context cleanup preserves body exceptions;
  normal exit reports cleanup failure. Finalization makes non-raising best-effort
  closure, not a cancellation or quiescence guarantee.

### Metadata qualification and limits

`python/tests/test_instrumentation_metadata.py` uses existing compiled mock
scenarios for registration/send callbacks, queue behavior, malformed callbacks,
failed streams, command independence and parent-first lifetimes. Held-future and
in-flight-callback tests use existing deterministic provider barriers in
subprocesses; uncertain detach behavior is subprocess-isolated. Native-boundary
injection is limited to malformed C responses absent from the mock and conversion
failure cleanup. Existing command, ABI/layout and private import-identity tests
remain applicable.

```sh
PYTHONPATH="$PWD/python" \
 AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
 AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
 python3 -W error -m unittest discover -s python/tests -p test_instrumentation_metadata.py -v
```

This is Python parity over the published Ada/C metadata slice, not a native ABI
extension or new production export. Only the safe Python API, these tests and this
README change. No source-policy authorization is needed. Positive behavior remains
mock-qualified: no real Instrumentation hardware/provider qualification, new
transport, callback-unregister guarantee, or general provider C++ ABI compatibility
is claimed. The existing 4096-byte diagnostic limitation described above applies;
side-effecting registration is never repeated to retrieve diagnostic text.

## IR Health/Status channel (PY-MEL-004)

```python
from ams_mel import ComponentLocation, HealthConfig, Session, UciId

config = HealthConfig.new(
    UciId(bytes(range(16)), "health-μ"),
    UciId(bytes(range(240, 256)), "platform-€"),
    ComponentLocation(1.25, -2.5, 3.75, "station-1", "aircraft"),
)
with Session.open("/path/to/provider.so", "provider-instance", "") as session:
    with session.open_health_channel(config) as health:
        attached_capabilities = health.capabilities()
        # Open attaches only: Enable is explicit and repeated success is a no-op.
        health.enable()
        with health.channel_view() as channel:
            request = channel.send_keepalive()  # Existing ReturnRequest, not a new Health API.
            try:
                result = request.wait(1000)  # Timeout does not cancel or consume it.
            finally:
                request.close()
# attached_capabilities is independent of every native owner and the provider DSO.
```

### Public API and source mapping

`HealthConfig(channel_id, platform_id, sensor_location)` and its equivalent
`HealthConfig.new(...)` are frozen dataclasses using the existing `UciId` and
`ComponentLocation`. Construction and Open validate both 16-byte UUIDs, all UTF-8
labels/location strings without embedded NUL, the location type, and numeric
coordinates representable as C doubles. Empty strings are supported; UUID bytes,
Unicode spelling, meter offsets (including representable IEEE nonfinite values),
keys, and system names are neither truncated nor normalized. Open revalidates
before calling native code, and keeps all borrowed input buffers alive through
the call. The private C record follows `channel_id`, `channel_type = 4`,
`platform_id`, `sensor_location`, not the C2 field order.

| Python | Ada `AMS.MEL.IR.Health_Status` | Rust `ir_health` | Existing C ABI |
| --- | --- | --- | --- |
| `HealthConfig.new(...)` | `Create_Config` | `HealthConfig::new` | `ams_mel_ir_health_config_v1` |
| `Session.open_health_channel(config)` | `Open` | `Session::open_health_channel` | `ams_mel_ir_health_open` |
| `HealthStatusChannel.is_open` | `Is_Open` | `HealthStatusChannel::is_open` | Local nonnull owner check |
| `enable()` | `Enable` | `enable` | `ams_mel_ir_health_enable` |
| `capabilities()` | `Capabilities` | `capabilities` | `ams_mel_ir_health_get_capabilities`, common snapshot view/close |
| `channel_view()` | `As_Channel` | `channel_view` | `ams_mel_ir_channel_from_health` |
| `close()`, context exit, finalization | `Close`, `Finalize` | `close`, `Drop` | `ams_mel_ir_health_close` |

Direct `HealthStatusChannel()` construction is rejected. `is_open` describes only
ownership of the local native handle, **not** enablement or provider health.
Native status, diagnostic text and required diagnostic size remain structured
`MelError` data; the existing 4096-byte non-retrying diagnostic limit applies.
An unexpected null successful Open is a protocol error. An owner published before
an exception or malformed diagnostic receives one non-raising cleanup attempt.

### Lifecycle and inherited services

- Open leaves the channel Attached, without Enable or metadata registration.
  Capabilities and inherited common services work while Attached or Enabled.
- Repeated successful Enable reaches the provider once. Failed Enable leaves
  native state Failed: enable, capabilities, and provider-facing view operations
  reject rather than implying enabled service. Native weak-wrapper conversion can
  still succeed in Failed state; this does not restore provider access.
- The typed owner retains no Python Session. Closing or collecting the Session
  first is safe because native state retains its underlying provider graph.
- Close before Enable does not disable. Close after an enable attempt follows
  native disable/detach/channel destruction/Session destruction/DSO unload order.
  Common pending requests can defer physical cleanup independently of public owners.
- Successful Close is idempotent. A failed detach retaining the handle leaves
  `is_open` true for **explicit** retry. A consumed handle stays closed even if
  Close reports cleanup failure; it is never retried. Finalization makes one
  best-effort attempt, never forces destruction or unload of uncertain state, and
  never raises. Context exit reports close failure on normal exit but preserves
  an exception from the body.
- `channel_view()` reuses the existing weak `ChannelView`: `send_keepalive()`,
  `submit_comms_test(CommsTestRequest(...))`, `capabilities()`, and `close()`.
  There are no duplicate Health KeepAlive/CommsTest methods. Views retain no
  Python typed owner or Session, nor a strong native provider graph. Closing a
  view does not close Health. After typed-owner teardown a view remains an open
  wrapper, but provider-facing operations fail cleanly with `PROVIDER_FAILED`.
- Existing `ReturnRequest`/`CommsRequest` owners independently preserve admitted
  native requests after view, Health, and Session closure. Request close is not
  cancellation. CommsTest preserves every uint32 command/channel/request ID;
  replies carry command and request IDs only. `CommsRequest` uses explicit Close,
  not context-manager syntax.

**Externally serialize enable, capabilities, view conversion, and close on the
same Health owner.** Serialize operations/close on each view and wait/close on each
request under their existing contracts. The GIL does not satisfy native
serialization. No Python locks, worker threads, background cleanup, implicit
thread-safety promise, or hidden retries are added.

Capabilities reuse `_channel_capabilities` and `ChannelCapability`: every nested
array, ID, label, location string, image band, Boolean and enumeration is checked
and deep-copied before the native snapshot closes. No ctypes object or borrowed
span escapes. Copies survive child/Session Close and actual provider DSO unload.
Existing malformed-pointer checks validate NULL/size/alignment/representable span
boundaries; they cannot prove arbitrary nonnull ABI-violating pointers are mapped.

### Coverage and limits

`python/tests/test_ir_health.py` covers config validation/immutability and exact
conversion, Attached/Enabled capabilities, enable failure, independent ownership,
weak views, inherited services, high uint32 IDs, parent-first request completion,
teardown order, retained-handle detach retry, one-attempt uncertain finalization,
Open failure/publication cleanup, and malformed snapshot cleanup. It uses the
existing compiled `libmock_ir_provider.so` scenarios, deterministic completion
gates and blocking retirement observations, not sleeps. Uncertain graph retention
and extra test DSO loading run only in subprocesses. Boundary injection is limited
to malformed C output and conversion failure paths the mock cannot produce.
Existing Instrumentation, C2, ABI/layout, and private import-identity regressions
remain applicable.

```sh
PYTHONPATH="$PWD/python" \
 AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
 AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
 python3 -W error -m unittest discover -s python/tests -p test_ir_health.py -v
```

The lifecycle slice above is extended by PY-MEL-005 and the independently opt-in
Image/Health `NUC_TempData` reception in PY-MEL-006 below. Evidence
is mock-provider only, not real Health hardware or Squall qualification or a
general C++ ABI compatibility claim.

## IR Health/Status metadata (PY-MEL-005)

```python
from ams_mel import HealthMetadataKind, Session

# Use the HealthConfig from the preceding example.
with Session.open(provider_path, "", "") as session:
    with session.open_health_channel(config) as health:
        with health.open_metadata(queue_capacity=16) as metadata:
            health.enable()  # Explicit; metadata registration does not enable.
            event = metadata.receive(timeout_ms=1000)
            if event.kind is HealthMetadataKind.MFA_STATUS:
                print(event.payload.about.model)
                for component in event.payload.components:
                    print(component.component_id, component.temperature.temperature_c)
            counts = metadata.counters()
            print(counts.events_received, counts.events_dropped_queue_full)
```

`HealthStatusChannel.open_metadata(queue_capacity=16)` creates an independent
`HealthMetadataStream`. Registration is valid while Attached or Enabled, may
deliver callbacks synchronously, and makes **one native attempt per channel**.
Failed or partial registration consumes that attempt, as does successful
registration even after metadata Close. Invalid capacity (including zero) is
rejected before a native call and does not consume an attempt. It never
registers NUC. All six families share the existing native bounded
**DROP-INCOMING** queue; Python adds no callbacks, queue, workers, or threads.

| Native family / discriminant | `HealthMetadataKind` | Exact owned payload |
|---|---|---|
| `MFA_Status` / 1 | `MFA_STATUS` | `MfaStatus`: canonical `MFAState`, descriptions, transition, complete `About`, both installation/temperature-bearing components |
| `BIT_Status` / 2 | `BIT_STATUS` | `BitStatus`: active tests, completed tests and items, full faults/data/component IDs/ambiguity groups |
| `SubsystemStatusResp` / 3 | `SUBSYSTEM_STATUS` | `SubsystemStatus`: dependencies and CSCI records including all four version fields, BIT report, strict Boolean connection flag |
| `DiscreteStatus` / 4 | `DISCRETE_STATUS` | `DiscreteStatus.pairs`: ordered tuple of `NameValuePair` |
| `MFA_SecurityAuditRecord` / 5 | `SECURITY_AUDIT` | `SecurityAuditRecord`: identifiers, signed timestamp, artifacts, typed event alternative, outcome and severity |
| `MFA_StatusDetailed` / 6 | `MFA_STATUS_DETAILED` | `MfaStatusDetailed.pairs`: ordered tuple of `NameValuePair` |

`HealthMetadataEvent(kind, payload)` is frozen and validates the exact matching
payload type. All returned nested collections are tuples, not dictionaries or
lists: pair ordering, duplicate names, empty names/values, Unicode, complete
UUID bytes/labels, signed nanosecond timestamps, full uint32/uint64 values, and
reported subsystem/CSCI counts independent of copied lengths are preserved.
Floating-point values are copied without normalization. Failure/severity values
are categories, not a library-defined ordering. The existing `UciId`,
`ComponentLocation`, and `MFAState` domains are reused.

Security alternatives expose their `kind` and **only applicable subtype fields**:

| Variant class / kind | Category domain | Other subtype fields |
|---|---|---|
| `SecurityNone` / 0 | None | None |
| `SecurityAuthentication` / 1 | `SecurityAuthenticationCategory` | `details`, `subsystem_id`, `service_id`, `mdf_id` |
| `SecurityIntegrity` / 2 | `SecurityIntegrityCategory` | `details`, `mdf_id` |
| `SecurityFileManagement` / 3 | `SecurityFileManagementCategory` | `details`, `mdf_id` |
| `SecurityKeyManagement` / 4 | `SecurityKeyManagementCategory` | `details` |
| `SecuritySystem` / 5 | `SecuritySystemCategory` | `details` |
| `SecuritySanitization` / 6 | `SecuritySanitizationCategory` | `details` |

Category enums are six distinct exact upstream domains, not interchangeable.
Flattened inactive native identifiers must be unset (zero UUID and empty
label); `None` must also have zero category and empty details. Unknown enums,
invalid event kinds, non-0/1 Booleans, inconsistent variants, null nonempty or
misaligned/unrepresentable spans, and embedded NUL are rejected with
`PROTOCOL_INCONSISTENCY`. Invalid UTF-8 reports `INVALID_UTF8`. No partial event
escapes. Pointer metadata checks cannot prove arbitrary aligned addresses are
mapped: valid native memory provenance remains a C ABI precondition.

### Ownership, errors and counters

- `is_open` reports local public-owner presence, not provider health.
- `receive(timeout_ms=0)` polls. Native `TIMEOUT`, `STREAM_STOPPED` and
  `PROVIDER_FAILED` remain structured `MelError` statuses. Other native errors
  retain diagnostics and status. Every event is deeply copied and its native
  owner closed before returning, including cleanup on conversion failure.
  Cleanup failure never replaces an earlier conversion exception; consumed
  handles are not retried. Returned graphs survive all native owner/DSO closure.
- `counters()` returns frozen `HealthMetadataCounters` with full uint64
  `events_received`, `events_dropped_queue_full`, `malformed_or_unsupported`.
  These count native callbacks, not receives, and receiving never resets them.
  They remain accessible after Health closes while metadata remains open.
- `close()` is idempotent. It releases the public metadata wrapper and ends
  consumption, but does not close Health, unregister callbacks, or promise
  immediate callback quiescence. Context managers and a non-raising, one-attempt
  finalizer are supported. Explicit native close errors are reported according
  to native handle consumption; uncertain finalizer cleanup is not retried.
- Metadata holds no Python Session or Health reference. Native state implements
  provider/callback retention. Session-first, Health-first, and metadata-first
  closure are supported. Health Close stops enqueueing; queued events drain
  before `STREAM_STOPPED`, or `PROVIDER_FAILED` for an already failed queue.
- **Externally serialize receive, counters, and Close on one metadata owner.**
  Also serialize metadata Open with enable, capabilities, view conversion and
  Close on the Health owner. The GIL is not the synchronization contract.

The only ABI calls are the existing
`ams_mel_ir_health_metadata_{open,receive,get_counters,close,event_view,event_close}`.
`_health_types.py`, `_health_convert.py`, and `_health_metadata.py` keep models,
conversion and ownership separate; `_native.py` is unchanged. Public classes and
enums are explicitly re-exported as their defining objects with
`__module__ == "ams_mel"`, preserving pickle/import identity and the additive
PY-PKG-001 compatibility baseline.

Qualification in `python/tests/test_ir_health_metadata.py` uses the unmodified
compiled mock's rich, overflow, malformed, allocation, partial-registration,
nonquiescing-disable and NUC-rich scenarios. Malformed C boundary responses are
injected only where the provider cannot construct them. Uncertain retention and
callback ordering are subprocess-isolated and use native deterministic barriers,
not sleeps. The test-only public-wrapper observer verifies implicit finalization
without adding a production dependency. Run against the test-enabled facade:

```sh
PYTHONPATH="$PWD/python:$PWD/python/tests" \
 AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
 AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
 python3 -W error -m unittest test_ir_health_metadata -v
```

No native ABI/provider, Ada, Rust, workflow or policy changes. No
Image/Track/C2/RF metadata, processing, or new transport APIs are added by
PY-MEL-005. Optional NUC is covered separately below.

## IR Image/Health NUC temperature metadata (PY-MEL-006)

NUC is optional and explicitly opt-in on either channel family. Both factories
return the **same** public `NucStream` type; neither implicitly starts Image nor
enables Health. For example, using an existing `ImageConfig` or `HealthConfig`:

```python
from ams_mel import CommandReturn, NucOpenError, Session

with Session.open(provider_path, "", "") as session:
    with session.open_image_stream(image_config) as image:
        try:
            with image.open_nuc(queue_capacity=16) as nuc:
                image.start()
                data = nuc.receive(timeout_ms=1000)
                for point in data.points:
                    print(point.timestamp_ns, point.frame_band,
                          point.pixel.kind, point.pixel.value, point.temperature_k)
                print(nuc.counters())
        except NucOpenError as error:
            if error.registration is CommandReturn.NOT_SUPPORTED:
                print("Optional Image NUC is unavailable")
            else:
                raise  # FAIL and exceptions are not NOT_SUPPORTED.

with Session.open(provider_path, "", "") as session:
    with session.open_health_channel(health_config) as health:
        with health.open_nuc() as nuc:
            health.enable()
            data = nuc.receive(timeout_ms=1000)
# data is fully Python-owned and remains usable after every native owner closes.
```

Providers may deliver synchronously during Open. Registration is one attempt per
channel/family, independent of ordinary metadata. Actual failed attempts consume
the one-shot; invalid local capacities (including zero) do not call native or
consume it. Unsupported NUC leaves the parent and its ordinary metadata usable.
`NucOpenError(MelError)` preserves `kind`, `diagnostic`, `diagnostic_required`, and
`native_status`, plus `registration: CommandReturn | None`. The independent native
Return output is initialized to an unpublished sentinel: `NOT_SUPPORTED` and
`FAIL` retain their exact Return with `PROVIDER_FAILED`; a provider exception is
`PROVIDER_EXCEPTION` with `registration=None`. No diagnostic text is parsed.

### Owned values and units

All new classes/enums are explicitly exported from `ams_mel` with public module
identity. Models are frozen dataclasses, with tuples for collections:

| Model | Fields / published domain |
|---|---|
| `NucPointType` | `NUC_POINT=0`, `MFA_ESTIMATE=1`, `OTHER=2` |
| `NucPixelKind` | `UINT8=1`, `UINT16=2`, `UINT32=3` |
| `NucPixelValue` | `kind`, `value`; validated against 8/16/32-bit unsigned width |
| `NucTempPoint` | `timestamp_ns`, `frame_band: BandInfo`, `pixel: NucPixelValue`, `temperature_k`, `point_type` |
| `NucTempData` | `points: tuple[NucTempPoint, ...]` |
| `NucCounters` | `events_received`, `events_dropped_queue_full`, `malformed_or_unsupported` |

Pixel **width is the original provider variant**, never inferred from magnitude:
the value 1 may be UINT8, UINT16 or UINT32. Signed int64 timestamps are exact
nanoseconds, with **unknown upstream clock/epoch**; no epoch conversion occurs.
Bands reuse `BandInfo`/`BandType`, preserving wavelength limits in **meters**.
Unlike forward-compatible capability snapshots, NUC rejects unpublished bands.
Temperatures are native doubles in **Kelvin, not Celsius**: negative values,
nonfinite values and signed zero are preserved, not clamped or normalized. All
three classifications, point order, duplicates, and empty messages survive.
The upstream private optional `tempError` has **no public accessor**; it is not
exposed, inferred or fabricated. This is complete coverage of publicly readable
TempPoint fields, not a claim to expose that inaccessible field.

Receive validates a uint32 timeout, calls native exactly once, validates the outer
event and point spans (NULL/length, alignment, representable size), and copies
every point before closing the native event. Unknown bands/point/pixel kinds and
width mismatches are `PROTOCOL_INCONSISTENCY`. No ctypes storage escapes. As with
other FFI snapshots, pointer validation cannot prove an arbitrary aligned address
from an ABI-violating provider is mapped memory. Event cleanup is attempted once
on success or failure; an earlier exception is never masked by cleanup.

### Queue, ownership and concurrency

- `receive(timeout_ms=0)` polls; active-empty returns `TIMEOUT`. All native error
  categories remain structured `MelError` values. Counters preserve full uint64
  values and never reset when events are consumed.
- The existing native bounded FIFO uses **DROP-INCOMING**. Unlike ordinary Health
  metadata, a NUC callback allocation/copy failure increments
  `malformed_or_unsupported` and rejects **only that event**, not the stream.
- `NucStream` retains neither the Python parent nor Session. Native state controls
  provider/library retention. Session may close first; child owners may close
  independently. Owned data survives actual provider DSO unload.
- NUC Close is idempotent and deactivates public consumption. It does **not**
  unregister callbacks, stop Image, close Health, cancel requests, or promise
  immediate callback quiescence. Late callbacks remain native-owned and counted,
  but are not queued after deactivation.
- Image Stop ends its NUC producer/consumption lifecycle. Health teardown ends
  its NUC consumption independently. Queued messages remain drainable, followed
  by `STREAM_STOPPED`; uncertain Health detach may instead report `PROVIDER_FAILED`.
- Explicit closure and context managers are supported. Finalization makes one
  non-raising best-effort Close attempt. A Close that consumed or uncertainly
  retained a handle is never automatically retried; a context body exception is
  preserved. An uncertain retained graph may necessarily remain allocated.
- **Externally serialize operations and Close on one NucStream**, and serialize
  NUC Open with parent operations/Close. The GIL is not a native ownership lock.
  No Python callbacks, registration infrastructure, queues, polling threads or
  background workers are added.

### Existing C ABI mapping and qualification

Shared models, registration, ownership, conversion and counters live in `_nuc.py`;
`_image.py` and `_health.py` add only small factories. The private import graph
remains acyclic, `__init__.py` only re-exports, and `_native.py` is byte-identical.

| Python operation | Existing published C function(s) |
|---|---|
| `ImageStream.open_nuc` | `ams_mel_ir_image_nuc_open` |
| `HealthStatusChannel.open_nuc` | `ams_mel_ir_health_nuc_open` |
| `NucStream.receive` | `ams_mel_ir_nuc_receive`, `ams_mel_ir_nuc_event_view`, `ams_mel_ir_nuc_event_close` |
| `NucStream.counters` | `ams_mel_ir_nuc_get_counters` |
| `NucStream.close` | `ams_mel_ir_nuc_close` |

`python/tests/test_ir_nuc.py` independently runs Image and Health through the
existing compiled `nuc-rich`, `nuc-malformed`, `nuc-overflow`, `nuc-allocation`,
`nuc-register-fail`, `nuc-register-throw`, `nuc-late`, and `nuc-nonquiescing`
scenarios, plus unsupported `image-metadata-four-mixed`/`health-rich` cases.
Malformed C-boundary injection covers records the mock cannot produce. Failpoints,
provider unload and callback barriers are subprocess-isolated; barrier observations,
not arbitrary sleeps, establish ordering. Existing Health metadata and the additive
package/import compatibility baseline remain tested unchanged. No native ABI,
provider mock, Ada/Rust source, policy or workflow changes are needed.

Evidence is **mock-only**, not IR hardware or Squall NUC qualification. PR #158
(PY-MEL-005) merged into main at `664e86eb4bccf20fb6a6ded36dd31c89e313d505`.
The reconciled PY-MEL-006 tree preserves that final Health metadata API alongside
both optional NUC factories. The production ABI remains unchanged from current
main at **272 exported C symbols**; the seven mappings above add no C exports.

## Ordinary IR Image metadata (PY-MEL-007)

Image metadata reception is separate from image-frame reception and optional
Image/Health NUC. Given an attached `ImageStream` named `image`:

```python
with image.open_metadata() as metadata:
    image.start()
    event = metadata.receive(timeout_ms=1000)
    counters = metadata.counters()
```

`ImageStream.open_metadata(queue_capacity: int = 16)` returns an independent
`ImageMetadataStream` with `is_open`, `receive(timeout_ms: int = 0)`, `counters()`
and `close()`, context management, and non-raising finalization. Registration is
valid Attached or Running, but **prefer registration before Start** to avoid
missing early metadata. No implicit Start, frame Receive or NUC registration
occurs. Positive capacity must fit size_t; Boolean/invalid/zero capacity is
rejected locally without consuming registration. A real native attempt is
one-shot, including partial/failing registration. Closing metadata does not allow
another registration on that Image owner.

`ImageMetadataEvent` is frozen and enforces the exact `ImageMetadataKind`/owned
payload match. Its four alternatives map directly to the existing ABI:

| Kind / native event field | Python payload and complete fields |
| --- | --- |
| `BAD_PIXEL_LIST = 1` / `bad_pixel_list` (`ams_mel_ir_bad_pixel_list_v1`) | `BadPixelList(reported_size, reported_count, pixels)`; ordered tuple of `BadPixel(row, column, reason)`, strict `BadPixelReason.UNKNOWN = 0` |
| `LINE_OF_SIGHT_REPORT = 2` / `line_of_sight_report` (`ams_mel_ir_line_of_sight_report_v1`) | `LineOfSightReport(system_time_ns, pointing_angle, pointing_angle_rates, at_speed, in_tolerance, platform_attitude, validity_flag_bitfield, image_rotation_rad)` |
| `LINE_OF_SIGHT_EULER = 3` / `line_of_sight_euler` (`ams_mel_ir_line_of_sight_euler_v1`) | `LineOfSightEuler(system_time_ns, attitude, attitude_rates)` |
| `NAVIGATION_RESPONSE = 4` / `navigation_response` (`ams_mel_ir_navigation_response_v1`) | `NavigationResponse(system_time_ns, command_id, request_id)` from upstream `NavigationReportResp` |

`AzimuthElevation(azimuth_rad, elevation_rad)` distinguishes pointing angles from
pointing rates; no undocumented time denominator is invented for rates. All
attitudes reuse public `Euler(roll, pitch, yaw)`. Booleans require exact native
0/1. The full uint32 validity bitfield is preserved **without interpreting bits**.
Bad-pixel reported size/count are independent of tuple length. Coordinates retain
all uint32 bits, order and duplicates, without sorting, renumbering or inferring a
coordinate origin. Navigation IDs preserve all uint32 bits; asynchronous
navigation metadata is **not request completion**, and contains no invented status
or error code. Signed int64 nanosecond times have no inferred epoch. IEEE doubles,
including NaN, infinity, subnormal values and signed zero, are not transformed.

Returned data is **owned, not zero-copy**: every active payload is validated and
deep-copied before its native event is released. No ctypes storage, native pointer
or memoryview escapes. Copies survive all owners and actual provider library
unload. Inactive payloads are not inspected. Pointer/span validation checks NULL,
alignment, representable lengths and address overflow, but cannot prove that an
arbitrary ABI-violating aligned pointer refers to mapped memory. The native C
facade remains the trusted storage/provenance boundary.

Native code alone registers callbacks (bad pixels, LOS report, LOS Euler,
navigation, in that order), owns the bounded DROP-INCOMING FIFO and synchronizes
callback teardown. Python adds no callbacks, locks, workers, threads or queues.
`ImageMetadataCounters` is frozen and preserves full uint64 `events_received`,
`events_dropped_queue_full`, `malformed_or_unsupported`; receives do not reset
them. Callback allocation/malformed failures reject that event and increment the
last counter, not automatically fail the stream. The four-mixed mock with capacity
four reports six callbacks, retains kinds 1/2/3/4 in order, and drops two.

Zero timeout polls; valid timeouts are uint32, excluding Boolean. Receive calls
native exactly once and preserves TIMEOUT, STREAM_STOPPED, PROVIDER_FAILED,
PROVIDER_EXCEPTION, INTERNAL_ERROR and RESOURCE_EXHAUSTED. Active-empty is TIMEOUT;
after Image Stop/Close queued metadata drains before STREAM_STOPPED. Session,
Image and metadata are independently owned, with no retained Python parents.
Metadata Close does not stop Image, unregister callbacks or promise callback
quiescence. Close is idempotent after release. A Close attempt is never retried if
it could have consumed its handle; unresolved retained ownership remains visible
as `is_open` and subsequent operations reject (Close reports protocol
inconsistency). Finalization makes one best-effort attempt. Cleanup never replaces
the first application/conversion/native exception. Existing diagnostic capacity
limits apply; side-effecting operations are never repeated to retrieve text.

**External serialization is required** for receive/counters/close on the same
metadata owner and for registration versus parent Start/Stop/Close. The GIL is not
a substitute. Native ownership retains provider resources as required.

`python/tests/test_ir_image_metadata.py` qualifies compiled mock scenarios in
subprocesses, with deterministic provider barriers for callback lifetime/unload.
Boundary injection covers malformed outputs and special values absent from mock
fixtures. `_native.py`, native ABI/exports, mocks and Ada/Rust implementations are
unchanged. Qualification is **compiled mock-only**, not real-provider/Squall or
hardware qualification. CameraCommandResp, OpticalDistortionMap,
LineOfSightQuaternion, C2/Track metadata and new navigation commands are not added.
