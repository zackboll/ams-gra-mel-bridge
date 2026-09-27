# Task 032B4 — public Python common Channel façade

Starting `origin/main`: `c450f895d2c0e39f09bb4f8f67c1c90a1bb115e8` (Task 032B3,
PR #53, merged). Branch: `feature/032b4-python-common-channel`.

032B4 exposes the 032B1 public common Channel C ABI through the public
`python/ams_mel` API for the two typed families public Python already owns.
It adds Python surface only. These are unchanged, byte for byte:

- `native/include/ams_mel/abi.h`, `native/src/exports.map`, `native/src/**`
- `native/tests/mock_provider.cpp` (no new mock scenario)
- `python/ams_mel/_native.py` (blob `b4f505b21b4d1ac5232bb8019c8cad1bf3111c2b`)
- `rust/**` and `ada/**`
- `python/tests/test_session.py`, `test_control_channel.py`, `test_image_stream.py`

ABI stays **0.1** with exactly **100** production exports, and the private
ctypes inventory stays at **100** bound functions.

## Scope: C2 and Image only

```text
native C common Channel:      C2/Image/Health/Instrumentation/Track
safe Ada common Channel:      C2/Image/Health/Instrumentation/Track
safe Rust common Channel:     C2/Image
public Python common Channel: C2/Image
safe Rust/Python Health/Instrumentation/Track: typed owners not implemented
```

A View is created only from a public typed owner. Public Python has
`ControlChannel` (C2) and `ImageStream` (Image) but no Health,
Instrumentation, or Track owners. Inventing those owners only to claim
five-family parity is out of scope, so those families are deferred.

## Public API

```python
ControlChannel.channel_view() -> ChannelView
ImageStream.channel_view() -> ChannelView

class ChannelView:                   # __init__ raises TypeError
    is_open: bool                    # property
    def send_keepalive(self) -> ReturnRequest
    def submit_comms_test(self, request: CommsTestRequest) -> CommsRequest
    def capabilities(self) -> ChannelCapability
    def close(self) -> None
    # context manager; non-raising __del__

@dataclass(frozen=True) class CommsTestRequest: command_id, channel_id, request_id
@dataclass(frozen=True) class CommsTestReport:  command_id, request_id
@dataclass(frozen=True) class CommsCompleted:   report: CommsTestReport
@dataclass(frozen=True) class CommsRejected:    code: MelErrorCode, description: str

class CommsRequest:                  # __init__ raises TypeError
    is_open: bool
    def wait(self, timeout_ms: int) -> CommsCompleted | CommsRejected
    def close(self) -> None
```

The capability model is `ChannelCapability`, `BandInfo`, `ImageBand`, and the
IntEnums `PixelFormat` (3), `SensorType` (5), `ChannelType` (9),
`MetadataCapability` (32), `BandType` (15), and `CoordinateSystem` (4). All
new names are in `__all__`. `_native`, raw handles, `IrChannelCapabilityV1`,
and ctypes pointers are not public and are never returned.

## Weak ownership and no Python source retention

```text
Python ChannelView            -> native ams_mel_ir_channel only
native ams_mel_ir_channel     -> weak family state only
ReturnRequest / CommsRequest  -> native async request owner
                              -> admitted native family graph
```

- `ChannelView` follows the existing owner style. Its private state is exactly
  `_owner` (the `IrChannelHandle`), `_owner_pointer`, and `_close_function`.
  It stores no `ControlChannel`, `ImageStream`, `Session`, provider object, or
  closure capturing one. Conversion goes through a module-level helper that
  borrows the typed handle only for the duration of the native call.
- `is_open` means only "this object still owns its native weak-view wrapper".
  After typed teardown it stays `True` while every operation returns
  `PROVIDER_FAILED`.
- A closed source raises `PROVIDER_FAILED` (`"C2 channel is closed"` or
  `"image stream is closed"`) without calling C. An explicitly closed View
  fails locally with `PROVIDER_FAILED` (`"ChannelView is closed"`), not
  `INVALID_ARGUMENT`, and makes no native call.
- Conversion: native success with a NULL view is `PROTOCOL_INCONSISTENCY`. A
  handle published on failure is best-effort closed.
- `close()` is idempotent, calls `ams_mel_ir_channel_close`, and requires native
  to clear the handle (otherwise `PROTOCOL_INCONSISTENCY`). It never touches
  the typed owner. `__del__` makes one best-effort close with NULL/0/NULL and
  suppresses every exception. The context manager mirrors Session,
  ImageStream, and ControlChannel: it suppresses a Close failure while
  unwinding a body exception.

## External serialization

The module documentation already stated that the GIL is not a thread-safety
contract. It now also states that operations and Close on one `ChannelView`
require external serialization, and that Wait and Close on one `ReturnRequest`
or `CommsRequest` require external serialization. No Python lock was added.

## KeepAlive reuses `ReturnRequest`

`send_keepalive` calls `ams_mel_ir_channel_send_keepalive` and returns the
existing `ReturnRequest`; there is no new Return wrapper. The docstring and
constructor error now name both creators (`ControlChannel.submit_bit_noop` and
`ChannelView.send_keepalive`); behavior is unchanged. Native success with a
NULL request is `PROTOCOL_INCONSISTENCY`. A request published on failure is
best-effort closed with `ams_mel_ir_return_request_close`.

## CommsTest result design

Results follow the established Python style (`ModeSuccess`/`ModeRejected`,
`ReturnCompleted`/`ReturnRejected`), not Rust's enum API.

`CommsTestRequest.__post_init__` validates each ID with `_validate_uint32`,
rejecting negative values, values above `0xffffffff`, `bool`, `float`, and
`str`. `submit_comms_test` accepts only an actual `CommsTestRequest` and
re-validates it, so a deliberately mutated instance cannot reach C.
`CommsTestReport` has no channel ID because the upstream reply carries none.

The private `_wait_for_comms` mirrors `_wait_for_return`:

- `AMS_MEL_OK` requires `required == 1`, an empty diagnostic, and
  `AMS_MEL_ERROR_NONE`. It returns `CommsCompleted` with all 32 bits of both
  IDs.
- `AMS_MEL_COMMAND_REJECTED` returns `CommsRejected(MelErrorCode(raw), text)`.
  Unknown codes use the existing dynamic `UNKNOWN_0x...` member. Rejection is
  a result and is not raised.
- Timeout raises `MelError(TIMEOUT)`. The request stays open and usable.
- A diagnostic longer than 512 bytes is re-read with an exact-length buffer
  through a cached `wait(0)`. Status, both IDs, error code, and required
  length must be identical, or the result is `PROTOCOL_INCONSISTENCY`. A
  truncated description is never exposed.

## Complete owned ChannelCapability

`capabilities()` calls `ams_mel_ir_channel_get_capabilities`,
`ams_mel_ir_channel_capability_view`, and
`ams_mel_ir_channel_capability_close` through the private `_CapabilitySnapshot`
guard.

- A NULL snapshot or NULL record on native success is `PROTOCOL_INCONSISTENCY`.
- Every failure path, including a failed explicit Close, makes a best-effort
  Close. The normal path closes explicitly, but only after the complete copy.
- While the snapshot is open, every string, UUID, span, and nested band span is
  copied into `int`/`float`/`bool`/`str`/`bytes`, IntEnums, frozen
  dataclasses, and tuples. UUIDs are copied as 16-byte `bytes`.
- `odc_available` and `nuc_available` accept only 0 or 1. Other values are
  `PROTOCOL_INCONSISTENCY`; no Python truthiness is applied to the raw uint32.
- Unknown enum values are preserved as `UNKNOWN_0x...` members through one
  private base class that mirrors `MelErrorCode._missing_`.
- `MemoryError`, `OverflowError`, or `ValueError` while building owned storage
  is `INTERNAL_ERROR`. No partial capability is returned.

Strings: size 0 is `""`. Otherwise the pointer must be non-NULL and the size
representable; the exact bytes are copied. Strict UTF-8 failure is
`INVALID_UTF8`, and an embedded NUL is `PROTOCOL_INCONSISTENCY`.

### Span validation boundary

One private helper validates every native `{data, size}` span before any
dereference:

- Size 0 returns an empty tuple without inspecting the pointer.
- A non-empty span requires a non-NULL pointer, an address aligned to
  `ctypes.alignment(element_type)`, and `size * sizeof(element_type)` within
  `sys.maxsize`.
- Malformed metadata is `PROTOCOL_INCONSISTENCY`.

This validates pointer, size, and alignment metadata only. The wrapper cannot
prove that an arbitrary non-NULL, aligned address from a violated C ABI points
to mapped memory, and it makes no memory-safety claim against such a pointer.
The native ABI contract remains responsible for the validity of a non-NULL,
representable span address.

The malformed-span tests never hand ctypes an unmapped address. They use NULL
with a non-zero size, oversize lengths, and misaligned addresses inside live
ctypes storage, and each is rejected before dereference.

## Evidence (`python/tests/test_common_channel.py`, 45 tests)

- **C2 lifecycle** (`comms-high`): while Attached, typed Operate and BIT are
  `PROVIDER_FAILED`, but KeepAlive, high-ID CommsTest, and Capabilities work.
  They work again after Enable, and typed Operate and BIT then succeed.
- **Image lifecycle**: the View is created before Start. All three operations
  work while Attached and while Running, and Receive and Counters still work.
- **High IDs**: `0x80000001 / 0xf0000002 / 0xe0000003` on both families (the
  mock throws on any mismatch). The report IDs are exact, and the cached
  `wait(0)` is identical.
- **KeepAlive `keepalive-fail`** returns `ReturnCompleted(CommandReturn.FAIL)`.
- **Rejections**: `keepalive-reject` gives `ReturnRejected(INVALID_STATE)` and
  `comms-reject` gives `CommsRejected(INVALID_PARAMETERS)`. Each carries the
  full 613-byte UTF-8 description, and the cached repeat is identical.
- **Unknown code**: a mock-patched wait returning `0xfedcba98` gives
  `MelErrorCode.UNKNOWN_0xFEDCBA98` with the value preserved.
- **Timeout and cache** (`keepalive-delayed`, `comms-delayed`).
- **View Close first**: C2 Enable/Operate/BIT/Close and Image
  Start/Receive/Counters/Close all still work.
- **Weak View** (C2 and Image): with no prior requests, the lifetime log is
  read once, immediately after Session Close. It already contains the family
  channel destruction, `control_destroyed`, `manager_destroyed`, and
  `library_unloaded`. The View is still open, and all three operations are
  `PROVIDER_FAILED`. The only new log lines are the test build's
  `completion_owner_destroyed_1/_2` for the refused preallocations. View Close
  adds nothing.
- **No Python retention**: `weakref`s to the typed source and the Session are
  dead after Close, `del`, and `gc.collect()` while the View is alive. The
  View's instance dictionary holds only its three private fields.
- **Multiple Views**: closing one leaves the other usable. After teardown, the
  second is owned but expired.
- **Pending request** (`keepalive-lifetime`): the View, C2, and Session close
  while the request is pending. The existing `ReturnRequest` completes and
  caches, and the log orders
  `keepalive_completed < c2_channel_destroyed < library_unloaded`.
- **Capabilities**: the full `capability-rich` C2 profile and the exact
  `image-capability-rich` profile (Attached equals Running) are rechecked after
  View, typed, and Session Close and provider unload. A recursive audit finds
  no ctypes object, list, dict, bytearray, or memoryview in the graph.
- **Pure tests**: capability guard paths (success, NULL record, Close failure);
  NULL, oversize, misaligned, and zero-length spans; invalid UTF-8; embedded
  NUL; `odc_available = 2`; `nuc_available = 0xffffffff`; allocation failure;
  all six enums with `0xfedcba98`; submission and conversion ownership
  hardening; closed-source and closed-View no-native-call checks; finalizers;
  input validation; and exports.

### Admission (`python/tests/test_admission.py`)

`test_common_channel_view_shares_session_admission` reuses the existing
child-process isolation, `completion-scale`, and the completion gate with
`max_async_requests = 1`:

- A held common KeepAlive makes typed BIT, typed Mode, and common CommsTest
  `RESOURCE_EXHAUSTED`. Closing the public KeepAlive owner before provider
  release does not return capacity. After release and FinalOwner reclamation, a
  retry succeeds.
- A held typed BIT refuses common KeepAlive. After reclamation, it succeeds.
- A held common Image KeepAlive refuses typed C2 BIT and the C2 View's
  KeepAlive on the same Session. After reclamation, both succeed.

### Repetition

The complete `test_common_channel` module ran 30 consecutive times: 30/30, with
45/45 tests passing each time. No sleep is used as ownership proof. The
lifetime-log poll only observes teardown from asynchronous completion workers,
and the defining weak-teardown assertion reads the log once without waiting.

## Real Squall

Real Squall common-Channel validation was not run in this environment. It is
opt-in and not a gate.

## Deferred

- Public Python and safe Rust Health, Instrumentation, and Track typed owners,
  and therefore their common Channel views.
- Generic Enable/Disable, buffer registration, and common metadata callbacks.
