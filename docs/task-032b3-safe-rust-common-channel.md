# Task 032B3 — safe Rust common Channel façade

Starting `origin/main`: `d442d2cad7ef15ee06a08b3448ae0f5cc7cd307e` (Task 032B2,
PR #52, merged). Branch: `feature/032b3-safe-rust-common-channel`.

032B3 exposes the 032B1 public common Channel C ABI through the safe `ams-mel`
Rust crate for the two typed families safe Rust already owns. It adds Rust
surface only. These are unchanged:

- `native/include/ams_mel/abi.h`, `native/src/exports.map`, and `native/src/**`
- the mock provider
- `rust/ams-mel-sys`
- Python and Ada

ABI stays **0.1** with exactly **100** production exports.

## Scope: C2 and Image only

```text
native C common Channel:                C2/Image/Health/Instrumentation/Track
safe Ada common Channel:                C2/Image/Health/Instrumentation/Track
safe Rust common Channel:               C2/Image
safe Rust Health/Instrumentation/Track: no typed owners yet
public Python common Channel:           not implemented
```

Before this task, safe Rust exposed `Session`, `ImageStream`, `ControlChannel`,
`ModeRequest`, and `ReturnRequest`. Raw sys declarations exist for Health,
Instrumentation, and Track, but safe Rust has no typed owners for them. A View
is created only from a safe typed owner, and inventing those owners just to
claim five-family parity is out of scope, so those families are deferred.

## Public API

```rust
impl ControlChannel { pub fn channel_view(&self) -> Result<ChannelView, Error>; }
impl ImageStream    { pub fn channel_view(&self) -> Result<ChannelView, Error>; }

pub struct ChannelView { /* private raw *mut AmsMelIrChannel + Rc<()> */ }
impl ChannelView {
    pub fn is_open(&self) -> bool;
    pub fn send_keepalive(&mut self) -> Result<ReturnRequest, Error>;
    pub fn submit_comms_test(&mut self, request: CommsTestRequest)
        -> Result<CommsRequest, Error>;
    pub fn capabilities(&mut self) -> Result<ChannelCapability, Error>;
    pub fn close(&mut self) -> Result<(), Error>;
}

pub struct CommsTestRequest { pub command_id: u32, pub channel_id: u32, pub request_id: u32 }
pub struct CommsTestReport  { pub command_id: u32, pub request_id: u32 }
pub enum CommsTestResult {
    Completed { report: CommsTestReport },
    Rejected { code: MelErrorCode, description: String },
}

pub struct CommsRequest { /* private raw *mut AmsMelIrChannelCommsRequest + Rc<()> */ }
impl CommsRequest {
    pub fn wait(&mut self, timeout_ms: u32) -> Result<CommsTestResult, Error>;
    pub fn close(self) -> Result<(), Error>;
}
```

The capability model consists of `ChannelCapability`, `PixelFormat`,
`SensorType`, `ChannelType`, `MetadataCapability`, `BandType`,
`CoordinateSystem`, `BandInfo`, and `ImageBand`.

This task adds these read-only getters, and the underlying fields stay
private:

- `UciId::uuid` and `UciId::descriptive_label`
- `ComponentLocation::offset_x_m`, `offset_y_m`, `offset_z_m`, `key`, and
  `system_name`

The existing public APIs (`Session`, `ImageStream`, `ControlChannel`,
`ModeRequest`, `ReturnRequest`, `CommandReturn`, `ReturnResult`, `ModeResult`,
and `MelErrorCode`) keep source compatibility and behavior. The existing
`c2.rs`, `image_stream.rs`, and `session.rs` tests are not modified.

No public `unsafe` function, `from_raw`, raw sys pointer, or sys struct is
exposed.

## Weak ownership, no borrow, `!Send`/`!Sync`

```text
ChannelView                          owns native ams_mel_ir_channel only
native ams_mel_ir_channel            owns only weak family state
Rust ReturnRequest / CommsRequest -> native request owner
                                  -> admitted native family graph
```

- **No retained owners.** A `ChannelView` retains no `ControlChannel`,
  `ImageStream`, `Session`, provider Channel, Control, or provider library. It
  has no lifetime parameter and does not borrow its source.
- **`is_open()`** reports only that the wrapper still owns a native weak view.
  After typed teardown it stays `true` while operations return
  `ErrorKind::ProviderFailed`.
- **Closed source.** A closed source returns `ProviderFailed`
  (`"C2 channel is closed"` or `"image stream is closed"`) without calling C.
- **Conversion errors.** Native success with a NULL view is
  `ProtocolInconsistency`. On native failure, any handle that was still
  published is best-effort closed.
- **Close and Drop.** `close()` calls `ams_mel_ir_channel_close`, requires
  native to clear the handle, and is idempotent. `Drop` makes one best-effort
  close with NULL diagnostics through the private
  `best_effort_close_channel_view`. Drop never panics and never touches the
  typed owner.
- **Thread safety.** `ChannelView` and `CommsRequest` carry the crate's
  `Rc<()>` marker, so neither is `Send` or `Sync`. Every View operation takes
  `&mut self`. Together these statically serialize operations and Close on one
  View without a Mutex. `compile_fail` doctests prove both types are
  `!Send`/`!Sync`.

## KeepAlive reuses `ReturnRequest`

`send_keepalive` calls `ams_mel_ir_channel_send_keepalive` and returns the
**existing** `ReturnRequest`. There is no `ChannelReturnRequest`. KeepAlive
inherits `ReturnRequest`'s behavior unchanged:

- cached Wait
- timeout is not cancellation
- `Return::Fail` is a normal completion
- MEL rejection is a normal terminal result
- complete long-diagnostic retry
- non-cancelling Close/Drop
- lifetime independent of the parent owners

The `ReturnRequest` docs now say it owns any public `RequestFor<Return>`:
C2 BIT and common Channel KeepAlive. Native success with a NULL request is
`ProtocolInconsistency`. On failure, an unexpectedly published request is
best-effort closed.

## CommsTest request/result

`CommsTestRequest` names its three IDs, so they cannot be passed in the wrong
order. `CommsTestReport` has no Channel ID because the upstream reply carries
none. `CommsRequest::wait` behaves like `wait_for_return`:

- **`AMS_MEL_OK`**: requires an empty diagnostic (`required == 1`) and
  `AMS_MEL_ERROR_NONE`. Returns `Completed { report }` with all 32 bits of
  both IDs.
- **`AMS_MEL_COMMAND_REJECTED`**: returns `Rejected { code, description }`, not
  an `Err`. `MelErrorCode::Unknown(raw)` is preserved.
- **Timeout**: returns `Err(Timeout)`. The request is not closed or consumed.
- **Long diagnostic**: when the diagnostic exceeds `WAIT_DIAGNOSTIC_CAPACITY`
  (512), the wait allocates exactly the required size and repeats a cached
  `wait(0)`. The status, command ID, request ID, error code, and required size
  must all be identical, otherwise the result is `ProtocolInconsistency`. A
  truncated diagnostic is never returned.
- **Close and Drop**: `close(self)` and `Drop` do not cancel. Pending provider
  work survives public request Drop.

## Complete owned ChannelCapability

`capabilities()` works in three steps:

1. Call `get_capabilities`, then `capability_view`.
2. Convert the whole record while the native owner is alive.
3. Explicitly close the native owner.

A private RAII `CapabilityOwner` closes the native snapshot on every early
return: `?`, UTF-8 failure, malformed span, or allocation failure. A NULL
owner or NULL view after success is `ProtocolInconsistency`.

All 19 fields are represented, and every vector and string is Rust-owned.

**Enums.** Every published constant maps to a named variant:

| Enum | Published constants |
| --- | --- |
| `PixelFormat` | 3 |
| `SensorType` | 5 |
| `ChannelType` | 9 |
| `MetadataCapability` | 32 |
| `BandType` | 15 |
| `CoordinateSystem` | 4 |

Any other value becomes `Unknown(u32)`, and each enum is `#[non_exhaustive]`.

**Booleans.** `odc_available` and `nuc_available` accept exactly 0 or 1.
Anything else is `ProtocolInconsistency`.

**Allocation.** Owned vectors use `try_reserve_exact`. A reservation failure
is `InternalError`.

### Checked span conversion

`checked_span` is the only `unsafe` helper that forms a slice from a native
span:

- `size == 0` is empty, whether `data` is NULL or not.
- `size > 0` requires a non-NULL, aligned pointer and
  `size <= isize::MAX / size_of::<T>()`.
- Any violation is `ProtocolInconsistency`.

String views go through the same helper. They must be valid UTF-8, otherwise
the result is `InvalidUtf8`. An embedded NUL is `ProtocolInconsistency`, which
keeps the existing `UciId`/`ComponentLocation` text invariant.

The pure `#[cfg(test)]` unit tests cover:

- complete conversion, including `Unknown` values for all six enums
- every published constant maps to a named variant
- empty spans, with both NULL and non-NULL data
- nonzero spans with a NULL pointer: u32 spans, image-band spans, nested
  band-info spans, string views, and a `usize::MAX` size
- a length beyond the slice bound, rejected before a slice is formed
- invalid UTF-8 and embedded NUL
- invalid booleans 2 and `u32::MAX`

No test dereferences invalid memory.

## Evidence: `rust/ams-mel/tests/common_channel.rs` (16 tests)

- **C2 lifecycle** (`comms-high`): while Attached, typed Operate and BIT return
  `ProviderFailed`, but KeepAlive, high-ID CommsTest, and Capabilities work.
  After Enable they work again, and typed Operate/TaskSched and BIT no-op are
  unchanged.
- **Image lifecycle**: the View is created before Start. All three operations
  work while Attached and again while Running.
- **High IDs**: `0x80000001 / 0xf0000002 / 0xe0000003` are sent for both
  families, and the mock throws on any mismatch. The reply IDs are exact, and
  `wait(0)` returns the identical cached result.
- **KeepAlive Return::Fail** (`keepalive-fail`): `Completed { value: Fail }`,
  not an `Err`.
- **Rejection**: `keepalive-reject` gives `Rejected(InvalidState)` and
  `comms-reject` gives `Rejected(InvalidParameters)`. Both carry the full
  613-byte UTF-8 diagnostic, and the cached repeat is identical.
- **Timeout** (`keepalive-delayed`, `comms-delayed`): `wait(0)` returns
  `Timeout`, the request stays usable, a later Wait returns the terminal
  result, and the cached `wait(0)` is identical.
- **Weak View** (C2 and Image): the typed owner and Session close while the
  View is alive.
  - With no earlier requests, the log already ends with
    `control_destroyed / manager_destroyed / library_unloaded` when Session
    Close returns. The log is read once, with no waiting, so the View did not
    delay teardown.
  - KeepAlive, CommsTest, and Capabilities then return `ProviderFailed`.
  - The only new log lines are the test build's
    `completion_owner_destroyed_1/_2`, one for each refused preallocation. No
    provider event appears.
  - Closing the View twice adds nothing.
- **Close first**: after the View closes, C2 Enable, Operate, BIT, and Close
  work, and Image Start, Receive, and Close work.
- **Multiple Views** (C2 and Image): closing the first View leaves the second
  usable. After typed teardown the second View is expired.
- **Pending request** (`keepalive-lifetime`): `wait(0)` returns `Timeout`. The
  View, C2, and Session then close while the request is pending. The existing
  `ReturnRequest` still completes and returns its cached result. The log orders
  `keepalive_completed < c2_channel_destroyed < library_unloaded`.
- **Capabilities**:
  - `capability-rich` checks the complete C2 model against the C/Ada values:
    labels, UUIDs (`seed + 7*i`), geometry, RGB, sensor types, location,
    channel types, schedule depth 17, ODC/NUC, metadata, both image bands and
    their nested band info, and nav frames.
  - `image-capability-rich` checks the exact Image profile: 320x200x8, Mono,
    `[IrstImage]`, and four metadata capabilities. All other fields hold the
    upstream defaults, including `sensorTypes{Unspecified}`. The Attached and
    Running snapshots are equal.
  - In both cases the snapshot is inspected after View, typed, and Session
    Close and provider unload.

### ResourceExhausted (`tests/admission.rs`)

`common_channel_view_shares_session_admission` uses the existing
`completion-scale` completion gate with a Session limit of 1. No new mock
mechanism was added.

- A held common C2 KeepAlive causes typed BIT, typed Mode, and common CommsTest
  to fail with `ResourceExhausted` ("async request limit reached").
- A held typed BIT causes common KeepAlive to fail with `ResourceExhausted`.
- After FinalOwner reclamation, common KeepAlive is admitted again.
- Optional cross-family case: a held common Image KeepAlive refuses typed C2
  BIT and the C2 View's KeepAlive on the same Session. After reclamation, typed
  BIT succeeds.

A static Mutex serializes the two admission tests because they share the
process-global gate.

### Repetition

The complete `common_channel` executable ran 30 consecutive times: 30/30, with
16/16 tests passing each time. No sleep is used as ownership proof. The
lifetime-log poll only observes teardown events from asynchronous worker
threads.

## Real Squall

Real Squall common-Channel validation was not run in this environment. It is
opt-in and not a gate.

## Deferred

- Safe Rust Health, Instrumentation, and Track typed owners, and therefore
  their common Channel views.
- A public Python common Channel API.
- Generic Enable/Disable, buffer registration, and common metadata callbacks.
