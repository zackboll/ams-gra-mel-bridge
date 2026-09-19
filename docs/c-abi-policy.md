# Experimental C ABI policy

The current ABI is 0.1, not yet stable. Header version macros describe this
facade only. Provider API, upstream source, architecture, and package versions
must remain distinct.

## Required for subsequent features

- Fixed-width numeric status and enum representations, not compiler-dependent
  enum storage. Validate unknown values and conversions.
- Typed opaque resource handles; ownership, borrowing, and close behavior are
  part of the contract. Do not promise detection of arbitrary dangling pointers.
- Explicit string length, encoding, output capacity, and lifetime rules.
- No C++ exception propagation through exported operations or foreign callbacks.
  `noexcept` alone is not an exception translator; fallible operations must catch
  and map ordinary C++ exceptions without losing useful error information.
- Keep metadata and optional/variant discriminants explicit. Do not memcpy C++
  objects into C records or treat their in-memory bytes as serialized messages.
- Per-call diagnostics or explicit error objects, not only mutable global state.
- Versioned value records. Do not append fields to an existing fixed-layout
  record without a compatible size/version scheme or a new type and operation.
- Asynchronous submission, pending, completion, wait timeout, and cancellation
  must be distinct concepts. Do not imply that a timeout stops provider work.
- Poll/wait/receive from application threads first. Queue provider callbacks in
  native code with bounded capacity and a documented overflow policy.
- Never unload provider code until all children, requests, deleters, and in-flight
  callbacks are finished. A C ABI does not make a mismatched C++ provider safe.
- Host-memory-only is an explicit initial constraint. Do not dereference device
  addresses as Ada arrays or Rust slices.

Current `ams_mel_get_abi_version` has no allocations or fallible C++ operations;
its null-pointer rejection is the only failure path. Future provider-related
status values should be designed with the corresponding feature, not prefilled
with fake implementations returning success.

## Task 001 provider session contract

The façade ABI remains 0.1. `ams_mel_session_open` creates one uniquely owned,
opaque session only after the provider library is loaded, both published
factories return non-null owners, and `Control::init` returns `Success`. Failure
leaves the caller's initially-null output unchanged. `ams_mel_session_close`
accepts a pointer to that owner, clears it, destroys `Control`, then
`API_Manager`, and only then unloads the library. Closing an already-cleared
owner succeeds. A null owner pointer is invalid; arbitrary non-null or dangling
handles are outside the contract.

`ams_mel_session_get_provider_version` preserves all four fields of upstream
`mel::VersionInfo`: two `uint32_t` values and the vendor/description strings.
Strings must be valid UTF-8 without embedded NUL, are validated and copied to
caller-owned storage, and have no lifetime after the call. Capacities and
required lengths count bytes including the NUL.
Callers may first supply null/zero buffers to discover both lengths. Insufficient
capacity returns `AMS_MEL_BUFFER_TOO_SMALL`, writes both required lengths, and
does not modify numeric fields or either buffer; strings are never truncated on
success.

Each fallible call optionally writes a per-call UTF-8 diagnostic and its full
required capacity including NUL. A short buffer receives a NUL-terminated,
valid UTF-8 prefix rather than a partial code unit. Diagnostic reporting does
not allocate; invalid provider exception text is replaced with a fixed valid
UTF-8 fallback. Diagnostics are informational; output validity is determined by
status. All ordinary C++ exceptions are translated. Provider destructors and
deleters are required not to throw, consistent with ordinary C++ destruction
rules.

The initial threading contract permits concurrent use of independent sessions.
Calls using the same session, including close, must be externally serialized.
No callback, asynchronous work, cancellation, timeout, or child-resource
guarantee is introduced by this slice.

## Task 002 IR receive contract

`ams_mel_ir_stream` is a uniquely owned opaque child. It retains shared
library/manager/control ownership independently of the C session owner.
`open` attaches only `ChannelType::IRSTImage`; `start` creates provider `Buffer`
objects through `getBuffer`, initializes stable façade-owned host memory,
registers every buffer, then enables. Stop rejects new adapter work, calls
disable, detaches, destroys channel owners, drains explicitly counted callbacks,
then destroys buffers/storage. Close is idempotent after its owner is cleared.

Configuration records preserve both UCI IDs as 16 UUID bytes plus UTF-8 labels.
Component location preserves meter-valued X/Y/Z offsets and both `ForeignKey`
strings. Unknown channel values and invalid UTF-8/NUL are rejected. No C++
layout crosses the ABI.

Callbacks are provider-controlled and may be concurrent. They are caught at the
C++ boundary, validate non-null addresses, dimensions, Mono/8-bit/one-band
format, known image enums/flags, checked byte arithmetic, and image containment
inside the registered buffer. Valid pixels and metadata are copied into a
bounded queue; overflow drops the incoming frame. Every non-null callback buffer
is released exactly once, including malformed and overflow paths. A release
failure or exception terminally poisons the stream and wakes receivers.

Receive uses caller-owned pixel storage. At most one thread may execute receive
for a stream at a time; this also makes Ada's size-discovery/copy pair one logical
single-consumer operation. `BUFFER_TOO_SMALL` reports the exact
required size and does not dequeue or partially return a frame. `TIMEOUT` means
only that no frame arrived during the interval; `STREAM_STOPPED` is distinct.
Counters are cumulative saturating `uint64_t` values. Times remain signed
nanoseconds, FOV remains radians, and frame fields needed by the Mono8 profile
are explicit fixed-width values.

The returned task-002 record deliberately omits contributing-sensor identity
and inertial/navigation vectors. These do not change the constrained raster
byte interpretation, but full metadata consumers require a later versioned API.
The pinned interface gives no generic callback-quiescence guarantee for
`disable()`. Pinned Squall stops its `UdpDataReceiver` in channel destruction,
not in `disable()`. The façade therefore retains listener, provider buffers, and
host storage through detach and channel destruction, then waits for adapter
callbacks already in flight. Generic unregister is not used during teardown.
Successful channel destruction is the provider-side no-new-callback boundary;
providers that retain callback work beyond destruction are incompatible.

Lifecycle states are attached, starting, running, stopping, stopped, and
terminal failed. Failed enable/disable/detach/release/rollback never permits a
later successful Start. Already queued frames are drained deterministically;
then receive returns `PROVIDER_FAILED` or `STREAM_STOPPED`. Close clears the C
owner after safe cleanup even when returning `PROVIDER_FAILED`; if detach cannot
establish safe cleanup, it retains the owner for retry. An open-time detach
failure retains the internal graph as the only memory-safe fallback.

## Task 003 IR C2 request contract

`ams_mel_ir_c2` is a uniquely owned opaque child retaining SessionState. Open
accepts a separate versioned C2 configuration, validates all UTF-8 string views,
requires a published `CommandAndControl` capability, attaches that type with no
image listener, and dynamic-casts the returned channel to `C2Channel`. Enable is
explicit; submission before enable fails and never enables implicitly.

`ams_mel_ir_c2_submit_operate` constructs only `MFA_State::Operate` and
`MFA_Mode::TaskSched`, leaving `ScanParam` defaulted. It returns a unique public
request without calling `future::get()`. A private completion worker retains the
future, C2 channel, provider, and library; closing the request is idempotent,
nonblocking, and is not cancellation. Timeout preserves the pending request.
Terminal results are cached and repeatable, with exactly one future `get()`.
Wait may be repeated, but request close must not race wait on the same handle.
Enable, submit, and close on one C2 owner require external serialization; parent
Session close rules are unchanged.

All façade owners and worker storage are allocated before provider `send`. Once
`send` returns a valid future, the future move, allocation-free self-retention,
and request-count increment establish lifetime accounting without an intervening
throwing operation. Provider `send` exceptions map to `PROVIDER_EXCEPTION`;
façade allocation and worker launch/detach failures map to `INTERNAL_ERROR`.
Post-send façade failure publishes no request and permanently retains the
already-accounted graph through an intrusive atomic root and pre-existing
`shared_ptr` cycle. Deferred detach failure uses the same allocation-free safe
retention. These emergency paths intentionally leak rather than unload code used
by a future or provider object with no safe completion path.

`AMS_MEL_COMMAND_REJECTED` represents a normal upstream `ErrorOr(Error)` and
maps every known `ErrorCode` explicitly to fixed-width C values. Its validated
UTF-8 description is the per-call diagnostic, including exact required bytes;
invalid text receives a fixed fallback. Provider exceptions, unknown values, and
null successful shared pointers remain distinct provider failures. C2 close
defers disable/detach until in-flight requests complete. Synchronous detach
failure retains the public owner for retry; orphaned deferred failure is retained
internally. A future that never completes safely retains provider/library state.
Task 014 adds the separate generic `ams_mel_ir_return_request` owner with the same
accounting, timeout, cache, close, and deferred-cleanup guarantees. Its only
producer is `ams_mel_ir_c2_submit_bit_noop`, which sets the command ID and leaves
all BIT payload lists empty. A completed upstream `Return::Fail` is represented
as `AMS_MEL_OK` plus `AMS_MEL_IR_RETURN_FAIL`; provider/facade failures remain
distinct statuses. Payload-bearing BIT, config, camera, scan, and callbacks are
not exposed by that task.

Task 017 retains ABI 0.1 and adds exactly three submit functions. Versioned C
records represent all published ModeCmd/ScanParam, BIT_Command, and
ConfigSetCommand fields with fixed-width values and explicit borrowed spans.
Inputs are validated and synchronously copied before provider `send`; no caller
storage is retained. Unknown states/modes/frame references/degradation methods,
the MFA state sentinel, invalid Boolean values, malformed spans, and invalid
UTF-8 are rejected before provider activity. Multiple populated raw BIT vectors
are preserved rather than normalized so provider precedence remains authoritative.
The existing Operate and BIT-no-op operations retain their exact semantics.

The Ada wrapper initially waits with a bounded diagnostic buffer. For a normal
rejection whose required byte count is larger, it allocates exactly that count,
repeats the cached wait with timeout zero, verifies the same rejection status and
error code/size, and preserves the complete validated UTF-8 description.

## Task 018 C2 metadata contract

Task 018 retains ABI 0.1 and adds exactly six operations, producing a
28-function façade. One opaque metadata owner registers, in order,
`BIT_Configuration`, `CommandStatus`, and `BIT_Status`. Its bounded FIFO queue is
ready before the first registration and drops incoming valid events when full.
Provider callbacks validate and atomically deep-copy every nested string, ID,
vector, enum, signed nanosecond value, and floating-point value. No provider or
callback-stack pointer enters a published event view.

There is no published callback unregister operation. Registration is attempted
only once per C2 channel. The callback state is retained by the existing C2
`ChannelState` even if a later registration fails or the public metadata owner
closes. Public close is idempotent/nonblocking and deactivates consumption but
does not close C2 or cancel requests. The provider C2 channel is destroyed before
the adapter waits for its atomic in-flight callback count; `disable()` is not a
quiescence claim. Queued events drain before stopped or provider-failed status.

Each successful receive transfers an immutable event snapshot containing only
adapter-owned storage. Its borrowed root/nested C views remain valid until event
close and are independent of metadata, C2, Session, and provider unload. Safe Ada
copies that complete graph again and closes the native event before returning.


## Task 019 common C2 Channel contract

ABI 0.1 grows from 28 to 36 functions. KeepAlive produces the existing Return
request owner. CommsTest preserves all three uint32 request fields, returns a
typed request owner, caches command/request IDs, and applies the established
timeout, rejection, exception, null-result, non-cancelling close, failpoint, and
parent-independent lifetime rules through `ChannelState.requests`. These common
services are accepted while Attached or Enabled; required C2 commands still
require Enabled.

CommsTest callback registration is explicit and one-attempt-only on an existing
metadata owner. It adds event kind 4 without renumbering kinds 1–3. Failed
registration cannot discard callback-accessible state because upstream has no
unregister; channel destruction remains quiescence.

The capability owner validates every pinned enum, UTF-8 string, ID/location,
band map entry, and nav frame before publication and deep-copies the complete
ChannelCapability graph in provider iteration order. Its immutable view remains
valid after C2/Session/provider teardown. Boolean outputs are normalized to 0/1.
No provider-owned STL pointer crosses the ABI. Generic buffer registration is
not exported by this task.

## Task 020 IR Health/Status contract

ABI 0.1 grows from 36 to exactly 46 functions: four Health channel operations
and six Health metadata owner/event operations. The channel attaches only
HealthAndStatus, enables explicitly, reuses the complete Task 019 capability
snapshot, and retains Session/provider state independently of the public Session.

Metadata registration attempts exactly MFA_Status, BIT_Status,
SubsystemStatusResp, DiscreteStatus, MFA_SecurityAuditRecord, and
MFA_StatusDetailed in that order. LFStatus and NUC_TempData are not registered.
The bounded queue is ready before registration, uses DROP-INCOMING, and has
saturating received/dropped/malformed counters. Every callback validates and
deep-copies complete nested values, including all SecurityAudit variant fields;
no provider pointer or STL storage enters a published C view.

Upstream publishes no unregister. Partial registration failure publishes no
metadata owner but retains callback-accessible state. Metadata close is
idempotent and nonblocking. Health close destroys the provider channel before
waiting for in-flight callbacks; disable is not a quiescence claim. A detach
failure retains the owner for retry. Received event snapshots remain valid until
event close and are independent of metadata/channel/session/provider lifetime.

## Task 021 full FrameHeader contract

ABI 0.1 grows from 46 to exactly 49 functions with an additive opaque frame snapshot
owner, immutable view, and close. The fixed legacy `frame_v1` layout and legacy receive
signature remain unchanged. Both receives consume one FIFO; a legacy `BUFFER_TOO_SMALL`
probe does not consume the head. Snapshot nested pointers borrow only adapter-owned
storage and remain valid until snapshot close, including after stream, Session, and
provider unload. Full flags retain provider vector order and duplicates; each nav
orientation has an independent discriminator.

## Task 024 Image capability and BadPixel metadata contract

ABI 0.1 grows from 49 to exactly 56 functions: one Image stream capability getter
and six Image metadata owner/event operations. The existing fixed
`ams_mel_ir_frame_v1`, legacy receive, and full-frame snapshot signatures are unchanged.

The Image capability operation reuses the complete immutable ChannelCapability
snapshot and remains valid after stream/Session/provider teardown. BadPixelList is
the sole Image metadata kind: reported size, reported count, actual ordered pixel
count, uint32 row/column values, and validated reason are preserved independently.
The callback-ready bounded FIFO uses DROP-INCOMING and saturating received/dropped/
malformed counters. Registration is one-shot and may invoke synchronously.

Metadata close is idempotent and nonblocking and does not assume unregister or callback
quiescence. Callback state remains attached to the Image stream until provider channel
destruction. Each received immutable event owns its complete graph and remains valid
after metadata, stream, Session, and provider teardown. Line-of-sight and navigation
metadata and requests are outside this contract.

## Task 025 Image line-of-sight metadata contract

ABI 0.1 remains exactly 56 exports. The existing six Image metadata operations carry
BadPixelList (kind 1), LineOfSightReport (kind 2), and LineOfSightEuler (kind 3) in one
bounded DROP-INCOMING FIFO. The fixed LOS C values preserve signed nanoseconds, radians,
Euler values, normalized fixed-width Booleans, and validity flags without conversion.
Registration is BadPixelList, Report, then Euler; partial registration keeps all callback
state stream-owned until ImageChannel destruction. NavigationReportResp, NavigationReport
send, quaternion LOS, and optional Image metadata remain outside this contract.
