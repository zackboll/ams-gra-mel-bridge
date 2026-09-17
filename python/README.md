# Python MEL binding

This directory provides the dependency-free `ams_mel` Python API over the
existing native façade:

```text
Python safe API -> private ctypes -> ams_mel_c -> published C++ MEL provider
```

Set `AMS_MEL_NATIVE_LIB` to the absolute full path of `libams_mel_c.so.0` (or
`libams_mel_c.so`) before importing `ams_mel`. The provider library remains a
separate path passed to `Session.open`; Python never loads provider factories or
models provider C++ interfaces directly.

The current API supports façade ABI/version queries, provider `Session`
open/version/close, host-memory Mono8 `ImageStream`
open/start/receive/counters/stop/close, and the existing IR
CommandAndControl Operate/TaskSched slice. `Session.open_control_channel()`
returns a `ControlChannel`; `enable()` is explicit, and `submit_operate()` returns
an asynchronous `ModeRequest`. `wait()` returns `ModeSuccess(MfaMode.TASK_SCHED)`
or a structured `ModeRejected(MelErrorCode, description)`. Zero polls, timeout
does not consume or cancel the request, and terminal results may be read again.
Complete rejection descriptions are preserved, including text larger than the
initial diagnostic buffer. Known MEL error codes have named enum values; unknown
rejection codes remain `ModeRejected` and preserve their exact uint32 value.

Frames preserve the supported metadata, and `Frame.pixels` is an owned `bytes`
copy: no view into ctypes storage, provider storage, or the native receive queue
escapes. Timeout and clean stream stop are distinct structured errors.

`ImageStream`, `ControlChannel`, and `ModeRequest` independently own their native
handles and do not retain Python parent objects. Session and control owners may
close first while a pending request remains valid. Request close only releases
the public owner and does not cancel provider work. Native C2 close decides
whether cleanup cleared the owner: a retryable detach failure leaves the Python
object open for another explicit close, while a cleared cleanup failure leaves
it closed.

Operations using one owner must follow the native external-serialization
contract. At most one receive operation may consume a stream at a time; request
wait must not race request close; and control enable, submit, and close must be
serialized. The GIL is not a substitute for those contracts. Context managers
do not implicitly start streams or enable controls. If a `with` body raises,
cleanup is attempted without replacing that exception; close failures propagate
on normal context exit.

This is not a native extension or a zero-copy interface. Additional C2 commands,
RF, real Squall Python validation, NumPy/zero-copy image views, wheels, and PyPI
publication are not included.

From the repository root, run:

```sh
make test-python
```
