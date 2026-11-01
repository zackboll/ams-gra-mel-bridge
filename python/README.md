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

Source-policy admission is separate from functionality:
`PYTHON_SOURCE_POLICY_ADMISSION_REQUIRED`. The current source-integrity/POL-002
routes do not generally admit safe Python API/test changes. This feature does not
change or self-authorize policy; a separately reviewed Python-only transition is
needed, preserving native, Ada, Rust, ABI, vendor and documentation authorities.

From the repository root, run:

```sh
make test-python
```

With the exact pinned external Squall checkout and a supported container runtime,
the separate real-provider validation is:

```sh
SQUALL_SOURCE_DIR=/path/to/squall make test-squall-ir-python
```
