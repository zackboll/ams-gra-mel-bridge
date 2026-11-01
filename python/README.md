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

`ImageStream`, `ControlChannel`, `ModeRequest`, and `ReturnRequest` independently
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
callbacks, RF beyond the Admin slice below, NumPy/zero-copy image
views, wheels, and PyPI publication are not included.

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
commands using only the existing published C ABI. It does **not** expose
Instrumentation metadata receive/callback APIs or add C2, Track, Health, RF, or
Image commands. The private ctypes layouts and all native exports are unchanged.

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
message or a repeated operation. Metadata receive/callback support remains out
of scope. Ordinary feature tests and the streamlined source/security validators
apply; no new source-policy authorization or filename allowlist is needed.
