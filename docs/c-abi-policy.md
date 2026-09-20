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
Registration is BadPixelList, Report, Euler, then NavigationReportResp; partial registration
keeps all callback state stream-owned until ImageChannel destruction. NavigationReport send,
quaternion LOS, and optional Image metadata remain outside this contract.

## Task 027B NavigationReport request contract

ABI 0.1 grows to exactly 59 exports. Three new exports implement
`ImageChannel::send(NavigationReport)`:
`ams_mel_ir_stream_submit_navigation_report`,
`ams_mel_ir_navigation_request_wait`, and
`ams_mel_ir_navigation_request_close`. `ams_mel_navigation_report_v1` copies
the complete published `mel::NavigationReport`, including all 18 named
`ams_mel_position_velocity_covariance_v1` terms; only
`state >= AMS_MEL_POSITION_SOLUTION_MAX_EXCLUSIVE` is rejected, and no other
floating-point value is clamped or rejected for being negative, NaN, or
infinite. Submission requires the stream to be logically Attached or Running;
it does not require a prior Start, and is rejected during Starting, Stopping,
Stopped, or Failed.

Provider `send()` may synchronously invoke the registered
`NavigationReportResp` metadata callback before returning its future, so the
adapter calls `send()` with neither the frame callback mutex nor the Image
metadata mutex held; the request is accounted (incrementing a
per-stream request counter) before the call and released if `send()` throws
before a future exists. `ams_mel_ir_navigation_request_wait` follows the
established C2 model: a zero timeout polls, timeout is not cancellation and
never consumes the pending request, a detached worker calls `future.get()`
exactly once and caches the terminal result permanently, and repeated Waits —
including Wait(0) after completion — return the identical cached result even
with a differently sized diagnostic buffer.
`ams_mel_ir_navigation_request_close` is idempotent, nonblocking, and not
cancellation; it drops only the public request owner while a pending
worker/future continues to own its completion state and the Image stream
state.

When one or more Navigation requests are outstanding, logical Stop/Close stops
accepting new frames and submissions immediately but defers physical provider
teardown (disable/detach/channel destruction/buffer and host storage release)
until the final request reaches terminal completion, mirroring the C2
`ChannelState` deferred-cleanup pattern including its own allocation-free
emergency retention path for a deferred detach failure. A synchronous
detach failure with no pending request continues to retain the public stream
owner for retry, unchanged from Task 027A.

## Task 028 Instrumentation contract

ABI 0.1 grows to exactly 72 exports. Thirteen new exports implement the
conditionally required Instrumentation family (`@RequiredIfInstrumentation`):
`ams_mel_ir_instrumentation_open`, `_enable`, `_get_capabilities`,
`_submit_level`, `_request_wait`, `_request_close`, `_metadata_open`,
`_metadata_receive`, `_metadata_get_counters`, `_metadata_close`,
`_metadata_event_view`, `_metadata_event_close`, and `_close`.

This is the Instrumentation-specific conditional surface plus Enable and
ChannelCapability. Instrumentation-specific copies of the inherited generic
`Channel` services -- KeepAlive, CommsTest, the ChannelCommsTest callback, and
`registerBuffer`/`unregisterBuffer` -- are deliberately NOT part of this
contract. Those inherited operations should eventually be generalized across
non-C2 channel families rather than cloned into each family, so this task does
not mark them complete for Instrumentation.

`ams_mel_ir_priority_t` mirrors upstream `Priority` exactly: Normal is 0 and
Debug is 1. Upstream defines no MaxExclusive value, so the facade invents none
and instead rejects any value greater than Debug with
`AMS_MEL_INVALID_ARGUMENT` on input and treats any such value received from a
provider as `AMS_MEL_PROVIDER_FAILED`.
`ams_mel_ir_instrumentation_level_command_v1` carries the complete
`InstrumentationLevelCmd` (commandID and instrumentationPriority) with no
omitted field. `ams_mel_ir_instrumentation_report_v1` carries the complete
`InstrumentationReport` (commandID, size, timestamp, instrumentationPriority);
`timestamp_ns` preserves signed `std::chrono::nanoseconds` and both `uint32`
fields preserve their full range. That one canonical record is used for both
the `RequestFor<InstrumentationReport>` completion and the
`InstrumentationReport` metadata callback; no separate future-only or
metadata-only report type exists.

`ams_mel_ir_instrumentation_config_v1` follows the Health/C2 configuration
pattern and must carry `AMS_MEL_IR_CHANNEL_INSTRUMENTATION`. All string views
are validated as UTF-8 and copied before Open returns. No Image buffer or
listener field belongs to this configuration.

The facade lifecycle is Attached, Enabled, Failed, Closed. Open attaches the
upstream channel. `_get_capabilities` is valid while Attached or Enabled and
reuses the one existing native ChannelCapability snapshot. `_metadata_open` may
be called while Attached. `_enable` explicitly calls upstream
`Channel::enable()`. Instrumentation-specific submission requires Enabled.

Provider `send()` executes with the channel lifecycle mutex released, because a
provider is permitted to invoke the InstrumentationReport metadata callback
synchronously from inside `send()`. The request is accounted before the call
and released if `send()` throws before a future exists. `_request_wait` follows
the established C2/Image model: a zero timeout polls, timeout is not
cancellation and never consumes the pending request, a detached worker calls
`future.get()` exactly once and caches the terminal result permanently, and
repeated Waits -- including Wait(0) after completion -- return the identical
cached result even with a differently sized diagnostic buffer. `_request_close`
is idempotent, nonblocking, and not cancellation.

Semantically, `AMS_MEL_OK` means the report is valid; `AMS_MEL_COMMAND_REJECTED`
means `error_code` is valid and the per-call diagnostic carries the provider
rejection description; `AMS_MEL_TIMEOUT` leaves the request pending and
retryable.

Metadata uses a bounded FIFO with DROP-INCOMING and saturating
`ams_mel_ir_metadata_counters_v1` counters (`events_received`,
`events_dropped_queue_full`, `malformed_or_unsupported`). A null callback
payload is malformed. No callback exception may cross into provider code. The
callback state belongs to the Instrumentation channel state, not to the public
metadata owner: upstream has no unregister operation, `_metadata_close` only
deactivates public consumption, and provider channel destruction remains the
callback-quiescence boundary. `_metadata_open` publishes and retains the
callback state and releases the lifecycle lock before calling provider
registration, so a provider that invokes the callback synchronously from inside
`registerMetadataCallback` cannot deadlock.

`_close` with pending requests prevents new submissions, deactivates public
metadata consumption, releases the public channel owner, and defers
disable/detach/provider-channel destruction to final request completion. If
that final deferred cleanup cannot detach after the public owner is gone, the
complete channel/provider/callback graph is retained permanently through the
same allocation-free emergency-root pattern proven in C2 and Image; provider
code is never unloaded while provider objects may remain live. A synchronous
detach failure with no pending request retains the public owner for retry.

`AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE` (values `allocation` and
`worker-launch`) deterministically injects a post-send facade failure in
test-enabled builds. Submit then returns `AMS_MEL_INTERNAL_ERROR`, no public
request escapes, the provider future is retained safely, and the channel and
Session may still be closed publicly without unsafely unloading provider code.

## Task 029B1 Track channel foundation contract

ABI 0.1 grows to exactly 76 exports. Four new exports implement the
conditionally required Track family's (`@RequiredIfTrack`) channel
ownership/lifecycle foundation only: `ams_mel_ir_track_open`,
`ams_mel_ir_track_enable`, `ams_mel_ir_track_get_capabilities`, and
`ams_mel_ir_track_close`. `ams_mel_ir_track` is the single new opaque owner;
no Track metadata owner, Track metadata event, or Track request owner exists.

`ams_mel_ir_track_config_v1` mirrors the Health and Instrumentation
configurations, requires `channel_type == AMS_MEL_IR_CHANNEL_IRST_TRACK`, and
applies the same UTF-8 and copied-string rules. Open constructs the pinned
upstream `irmel::Config` with `ChannelType::IRSTTrack`, then requires all three
of: a non-null `attachChannel` result, a successful
`dynamic_pointer_cast<TrackChannel>`, and a reported `ChannelCapability` whose
channel types contain `ChannelType::IRSTTrack`.

Open failures are distinguished. A null `attachChannel` yields
`AMS_MEL_FACTORY_FAILED` without any detach attempt. A wrong concrete channel
type, an omitted IRSTTrack capability, and a throwing `getCapabilities` all
attempt rollback detach; a proven detach yields `AMS_MEL_INITIALIZATION_FAILED`.
When detach ownership cannot be proven, the complete provider graph is retained
permanently through the same allocation-free emergency-root pattern used by the
other families and `AMS_MEL_PROVIDER_FAILED` is returned. No provider object is
destructed while detach ownership remains uncertain.

Lifecycle is `Attached`, `Enabled`, `Failed`, `Closed`. Capability snapshots are
valid while attached or enabled and reuse the one shared
`ams_mel::internal::snapshot_capability`. Enable moves `Attached -> Enabled`; an
already enabled channel returns `AMS_MEL_OK`; a failed or closed channel returns
`AMS_MEL_PROVIDER_FAILED`. A non-Success provider enable sets `Failed` and
returns `AMS_MEL_PROVIDER_FAILED`; a throwing enable sets `Failed` and returns
`AMS_MEL_PROVIDER_EXCEPTION`.

Close rejects a null pointer-to-owner with `AMS_MEL_INVALID_ARGUMENT` and
accepts an already null owner with `AMS_MEL_OK`. It calls provider `disable()`
only when Enable was attempted. A failed disable does not prove ownership
safety, so detach is still attempted; if that detach succeeds the provider
channel is destroyed, the caller's owner is cleared, and
`AMS_MEL_PROVIDER_FAILED` reports the disable failure. A failed detach returns
`AMS_MEL_PROVIDER_FAILED`, leaves the caller's Track owner non-null, resets
`cleanup_started`, and retains the complete graph so a later Close retries.

`SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` are not
implemented in this ABI. `TrackDataUpdate` is implemented separately by Task
029C below under its own upstream condition, `@RequiredIfTrackUpdate`.

## Task 029B2 Track IRSTTrackReport metadata contract

Task 029B2 adds exactly six exports for the `@RequiredIfTrack`
`TrackChannel::registerMetadataCallback(IRSTTrackReport)` surface --
`ams_mel_ir_track_metadata_open`, `_receive`, `_get_counters`, `_close`,
`_event_view`, and `_event_close` -- taking ABI 0.1 from 76 to 82 exports. Two
new opaque owners, `ams_mel_ir_track_metadata` and
`ams_mel_ir_track_metadata_event`, join `ams_mel_ir_track`. The Track channel
foundation is extended, not redesigned: `TrackState` gains only a shared
metadata state pointer and a one-shot `metadata_attempted` flag, and still
carries no request accounting because no Track send exists.

`ams_mel_ir_track_report_v1` is the complete `IRSTTrackReport`. Every upstream
getter appears exactly once, both NED vectors reuse the one canonical
`ams_mel_north_east_down_v1`, `system_time_ns` and `age_ns` preserve signed
`std::chrono::nanoseconds` counts, and no floating-point value is clamped,
normalized, or narrowed. `ams_mel_ir_track_state_t` and
`ams_mel_ir_track_mode_t` expose exactly the upstream `IrstTrackState`
(Idle/Detected/Coast/Dropped) and `IrstTrackMode` (Idle/Scan/Stare) values; no
MaxExclusive value is invented, so anything above Dropped or Stare is malformed.
`ams_mel_ir_track_metadata_event_v1` carries a kind discriminator so future
Track callback families can be added without breaking the event format;
`AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT` is the only kind defined, and a
consumer must fail closed on any other.

Registration is one-shot because upstream declares no unregister operation. The
first `ams_mel_ir_track_metadata_open` marks the attempt under the Track mutex,
publishes the callback state into `TrackState`, copies the shared `TrackChannel`
locally, releases the Track mutex, and only then calls the provider; a provider
that invokes the callback synchronously from inside `registerMetadataCallback`
therefore cannot deadlock and cannot lose that first report. A non-Success
registration returns `AMS_MEL_PROVIDER_FAILED` and a throwing registration
returns `AMS_MEL_PROVIDER_EXCEPTION`; in both cases no owner escapes while the
callback-accessible state stays retained by the Track channel. Every later
`ams_mel_ir_track_metadata_open` returns `AMS_MEL_INVALID_ARGUMENT`.

The provider callback increments an explicit in-flight count before touching
callback state and decrements it on return, lets no exception cross the provider
boundary, and never calls Ada. It always increments `events_received`; a null
`IRSTTrackReport` pointer and an out-of-range state or mode each increment
`malformed_or_unsupported` and queue nothing without poisoning the subscription.
A full queue increments `events_dropped_queue_full` and drops the INCOMING
report, so the earliest reports survive in arrival order. Counters saturate. The
`AMS_MEL_TEST_TRACK_CALLBACK_FAILURE=allocation` failpoint proves an adapter
allocation failure moves the metadata to Failed, wakes receivers, and surfaces
as `AMS_MEL_PROVIDER_FAILED` from Receive.

`ams_mel_ir_track_metadata_close` is idempotent and nonblocking: it marks public
consumption inactive, prevents future public enqueueing, wakes receivers, and
deletes the wrapper. It does not unregister the provider callback, which may
still be invoked safely afterwards and simply queues nothing.

The public Track metadata wrapper owns only its `MetadataState`; it holds no
Track, session, or provider-library ownership. After a successful Track Close
and provider-channel destruction, an existing metadata wrapper may drain
already-owned queued events and read counters without retaining or invoking
provider code.

Track cleanup captures the metadata state, marks it Inactive, and notifies
receivers before any provider teardown, then preserves the foundation ordering
of conditional disable followed by detach. A failed detach leaves the caller's
Track owner non-null, resets `cleanup_started`, keeps the complete
callback/provider graph alive, marks the metadata Failed, and permits a Close
retry. After a successful detach the Track state releases the channel and the
final local provider channel owner is destroyed; that destruction is the
callback-quiescence boundary, and only afterwards does the adapter wait for the
in-flight callback count to reach zero and move the metadata to Stopped unless
it was already Failed.

## Task 029C Track TrackDataUpdate contract

Task 029C adds exactly three exports for the `@RequiredIfTrackUpdate`
`TrackChannel::send(TrackDataUpdate)` surface --
`ams_mel_ir_track_submit_update`, `ams_mel_ir_track_update_request_wait`, and
`ams_mel_ir_track_update_request_close` -- taking ABI 0.1 from 82 to 85 exports.
This is a distinct upstream condition from `@RequiredIfTrack` itself: the
`@RequiredIfTrack` core is complete and this task completes
`@RequiredIfTrackUpdate` only. `SystemTrackDataResponse`,
`CandidateObjectMessage`, `CandidateObjectPreProcMessage`, and
`RequestSystemTrackData` remain unimplemented.

One new opaque owner, `ams_mel_ir_track_update_request`, joins the Track family.
It owns a shared terminal completion state and never a raw `ams_mel_ir_track`
pointer, so it remains valid independently of the public Track and Session
owners. The Track channel foundation is extended, not redesigned: `TrackState`
gains only a `size_t requests` count, and the existing metadata ownership is
unchanged.

`ams_mel_ir_track_status_t` exposes exactly the upstream `TrackStatus` values
Create = 0, Update = 1, Predict = 2, and Delete = 3. Upstream declares no
MaxExclusive value, so any input above Delete is `AMS_MEL_INVALID_ARGUMENT`.

`ams_mel_ir_track_covariance_v1` carries exactly the 21 published covariance
terms and `ams_mel_ir_track_data_update_v1` is the complete `TrackDataUpdate`.
Every upstream setter is called exactly once with the corresponding C field
through the published setters only; no provider object layout is assumed. Both
times stay in upstream epoch seconds and are deliberately not converted to
nanoseconds. Both ECEF vectors reuse the one canonical
`ams_mel_ir_directional_v1`; no second XYZ representation exists.
`maneuver_probability`, `track_quality`, the covariance terms, the position and
velocity components, and the time values are copied verbatim, because the
upstream setters perform no validation, clamping, or normalization. Each of the
three `ams_mel_uci_id_v1` descriptive labels is validated with the existing
UTF-8/no-embedded-NUL rules and copied before Submit returns, so no borrowed
application string outlives the submit call.

Submission requires the Track lifecycle to be Enabled; attached, failed, and
closed all report `AMS_MEL_PROVIDER_FAILED`. Under the `TrackState` mutex the
adapter validates Enabled, copies the shared `TrackChannel` locally, and
increments `requests`; it then releases the mutex and only afterwards calls
`TrackChannel::send`. A provider is permitted to invoke the registered
`IRSTTrackReport` metadata callback synchronously from inside `send()`, and that
callback independently locks the metadata mutex, so the lifecycle mutex must not
be held across the provider send. Everything the request needs to own the
returned future -- the completion state, the worker input, the public request
wrapper, and the thread wrapper -- is allocated before the provider send, so no
ordinary allocation failure after the send can destroy the future or the
provider graph unsafely.

The terminal outcome reuses the existing generic `ams_mel_ir_command_status_v1`
inside `ams_mel_ir_track_update_result_v1`. On `AMS_MEL_OK` the status is valid,
`error_code` is `AMS_MEL_ERROR_NONE`, and `status.reason_description` points into
immutable request-owned cached storage that stays valid across repeated Wait
calls until `ams_mel_ir_track_update_request_close`; it never points into
provider-owned memory. On `AMS_MEL_COMMAND_REJECTED` the `error_code` is valid,
the status must be ignored, and the diagnostic carries the provider `Error`
description. On `AMS_MEL_TIMEOUT` the request is still pending and the caller's
result record is untouched.

A successful `CommandStatus` whose own state is `AMS_MEL_IR_COMMAND_REJECTED` is
still `AMS_MEL_OK`: `CommandStatus::Rejected` is not an `ErrorOr` rejection. On
successful future completion the adapter requires a non-null
`shared_ptr<CommandStatus>` and validates `CommandState <= Cancelled`,
`CannotComply <= Alignment_Maneuver`, and a `reasonDescription` that is valid
UTF-8 without an embedded NUL; unknown or malformed provider output is
`AMS_MEL_PROVIDER_FAILED`. An `ErrorOr` rejection maps all nine published MEL
`ErrorCode` values through the existing mapping, and an unknown provider error
code is `AMS_MEL_PROVIDER_FAILED`.

Exactly one completion worker calls `future.get()`; no other thread may. The
terminal outcome is cached permanently, so repeated Wait calls return the
identical terminal result and may use a differently sized diagnostic buffer. A
finite Wait timeout means only that the result is not ready yet: it is never
cancellation, request consumption, or provider interruption, and `Wait(0)` is a
poll. `ams_mel_ir_track_update_request_close` is idempotent, nonblocking, and not
cancellation; closing the public request owner while future work remains pending
destroys neither the future, the Track state, the provider channel, nor the
provider library.

Track Close moves the lifecycle to Closed and immediately deactivates public
Track metadata consumption and wakes metadata receivers. With `requests == 0` it
performs the existing synchronous cleanup unchanged, including the Task
029B1/029B2 synchronous detach-failure semantics: the caller's owner stays
non-null, the graph is retained, and a later Close may retry. With
`requests > 0` it clears and deletes the public Track owner, returns
`AMS_MEL_OK`, and defers physical provider teardown until final request
completion; the request owns the `TrackState` graph meanwhile. When a worker
completes it decrements `requests`, and if the count reaches zero while the
lifecycle is Closed it performs the physical cleanup: disable if attempted,
detach, destroy the provider `TrackChannel`, establish callback quiescence, and
release the provider/session graph. If the public Track owner is already gone and
that deferred detach fails, the complete graph is retained through the existing
allocation-free emergency root, the request's terminal result becomes
`AMS_MEL_PROVIDER_FAILED` with the diagnostic `deferred Track cleanup failed`,
and provider code is never unloaded with uncertain detach ownership.

The `AMS_MEL_TEST_TRACK_UPDATE_POST_SEND_FAILURE=allocation` and
`=worker-launch` failpoints prove that after the provider send has returned a
future, either failure returns `AMS_MEL_INTERNAL_ERROR`, exposes no public
request owner, and retains the future and provider graph safely. That retention
is deliberately permanent under the fail-safe policy; no recovery or cleanup is
claimed.
