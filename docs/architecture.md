# Architecture decisions

## Scope

Build a consumer-side binding, not a new official MEL standard and not a
provider rewrite. Preserve the published C++ provider boundary. The C facade
adapts it for C, Ada, Rust, and Python.

The bootstrap implemented an independent ABI-version query. Task 001 adds only
the first provider boundary: load a compatible IR MEL library, create and
initialize its `Control`, copy its complete `VersionInfo`, and close it.
Task 002 adds one vertical receive profile: attach an `IRSTImage` channel,
register service-owned host buffers, receive `ImageListener::onImage` callbacks,
copy validated Mono8 frames into a bounded queue, and poll them from C or Ada.
Task 003 adds one command profile: attach and explicitly enable a
`CommandAndControl` channel, send `Operate`/`TaskSched`, and preserve the
asynchronous `RequestFor<MFA_Mode>` result in C and Ada.
Task 004 adds no library API. Its isolated, opt-in applications exercise those
Task-002/003 slices against Squall's published IR MEL provider and hardware-free
simulated optical stack. Container deployment remains test orchestration, not a
generic-library dependency or an application-facing transport.
Task 006 adds the first Rust consumer without changing that C ABI: façade
version query plus provider Session open/version/close. Task 007 adds the Rust
consumer for the existing IR host-memory Mono8 stream ABI. Task 008 adds the
Rust consumer for exactly the existing IR C2 Operate/TaskSched profile. Task 009
adds no library behavior: its standalone, opt-in Rust application exercises the
safe API against the same pinned Squall provider/runtime used by C and Ada. RF
and additional C2 commands remain later vertical slices. Task 010 adds the
first Python consumer over the unchanged C ABI: façade version plus provider
Session open/version/close, structured diagnostics, and deterministic cleanup.
Task 011 extends that same dependency-free consumer through the existing IR
host-memory Mono8 stream ABI: open/start/receive/counters/stop/close, owned-copy
frames, distinct timeout/stopped errors, and parent-first Session lifetime. It
does not add RF, real-provider validation, zero-copy/NumPy views, or packaging.
Task 012 adds the Python consumer for exactly the existing C2
Operate/TaskSched profile: explicit enable, asynchronous finite waits, structured
success/rejection values, complete rejection text, cached terminal results,
retryable close, and independent Session/C2/request lifetime. It adds no native
feature or additional command.
Task 013 adds no library behavior: a standalone, opt-in Python application
exercises the same current image+C2 slice against the pinned real Squall
provider/runtime used by C, Ada, and Rust. It imports only the public `ams_mel`
API and the standard library; container orchestration remains outside the binding.
Task 014 adds the required IR `BIT_Command` only in the empty/no-op profile
accepted by pinned Squall: a caller command ID with empty initiate, cancel, and
clear-fault lists. C and Ada expose a reusable asynchronous `RequestFor<Return>`
owner. The raw Rust sys crate tracks the expanded ABI, while safe Rust and Python
do not yet expose BIT. Task 015 exposes that unchanged ABI through safe Rust with
typed `CommandReturn`, structured normal completion versus MEL rejection, and a
reusable `ReturnRequest`. A completed `Return::Fail` is a normal `ReturnResult`,
not a Rust error. Timeout and request close/drop do not cancel provider work, and
native request ownership remains valid after Rust Session/C2 closure. Python and
payload-bearing BIT remain unsupported.
Task 016 exposes the same unchanged 19-function ABI through dependency-free
Python. It adds typed `CommandReturn`, structured `ReturnCompleted` versus
`ReturnRejected`, and an independent reusable `ReturnRequest`. `Return::Fail`
is normal completion; timeout and close do not cancel, complete terminal
diagnostics are preserved, and unknown MEL rejection codes remain inspectable.
Payload-bearing BIT remains unsupported.
Task 017 begins an Ada-first feature-family cadence and completes the three
published required C2 sends. The additive 22-function C ABI carries complete
ModeCmd/ScanParam, every BIT vector through borrowed spans copied before send,
and complete ConfigSet data. Ada exposes strong state/frame/degradation types,
complete scan units, one-choice BIT operations, and opaque configuration text.
The proven ModeRequest and ReturnRequest workers are reused. Rust sys and private
Python ctypes track the raw ABI only; safe Rust and Python retain their existing
Operate/TaskSched and BIT-no-op subsets. Required C2 callbacks remain Task 018;
no callback or optional command is introduced by Task 017. Task 018's implemented
callback architecture is documented below.

```text
C++ provider -> MEL API -> ams_mel_c -> Ada
                                     |-> Rust
                                     `-> private ctypes -> Python API
```

## Separation

1. `native/include`: genuine C11 declarations, no STL or C++ object layouts.
2. `native/src`: C++20 adapter implementation; CMake owns compilation.
3. `ada/src`: idiomatic public Ada plus private imported C declarations.
4. `rust/ams-mel-sys`: unsafe declarations for the complete current C ABI.
5. `rust/ams-mel`: safe Rust API over `ams-mel-sys`; no direct C++ path.
6. `python/ams_mel`: safe Python Session/image/Operate/BIT-no-op API over a
   private complete 22-function `ctypes` façade; no direct C++ path.
7. Separate integration applications: OMS/UCI, image processing, RF processing.

`integration/squall` contains validation applications rather than production
library code. It may invoke Squall's supported container build/deployment
mechanisms, but the C, Ada, and Rust executables and interpreted Python client use
only this project's public language APIs/C ABI. They do not expose or call
Couloir, backend gRPC, UDP, REST, or Squall-private types. The standalone Rust
application remains outside the published workspace and depends only on the
repository's safe `ams-mel` crate. The standalone Python application imports only
the standard library and public `ams_mel` API.

The shared native library deliberately owns its C++ boundary. GPR imports it
as externally built, and Cargo links it from an explicitly selected external
build directory. CMake remains the sole owner of native C/C++ compilation.
Python loads that same CMake-built façade from an explicit
`AMS_MEL_NATIVE_LIB` path and passes the separate provider path through the C
ABI. The Python layer is not a native extension and makes no zero-copy claim.
Neither Rust crate models or links a provider directly. `Session`,
`ImageStream`, `ControlChannel`, and `ModeRequest` are deliberately not `Send`
or `Sync`; the safe receive API has
one conceptual receiver and does not expose cross-thread stream sharing. A
stream has no Rust borrow of its parent Session because the native stream
independently retains provider state. Closing Session first therefore leaves its
child stream valid, and provider unload waits for the stream owner.

The Rust C2 owner likewise has no borrow of Session, and each `ModeRequest` has
no borrow of either parent. Session and C2 may be closed while a request remains
pending. Request timeout and request drop are not cancellation; terminal waits
are cached and repeatable. A C2 close that cannot detach synchronously retains
its owner for explicit retry, which `ControlChannel::is_open` exposes. Drop makes
one best-effort close attempt and prioritizes native lifetime safety over leak
avoidance when detach still cannot be established.

Rust models command rejection as a terminal `ModeResult::Rejected`, preserving
the MEL error code and complete validated UTF-8 description. The wait wrapper
uses the C ABI's cached-terminal guarantee to repeat only an oversized terminal
wait with timeout zero and exact fallible storage; generic side-effecting calls
retain their non-retrying diagnostic behavior.

Rust receive follows the native non-consuming `BUFFER_TOO_SMALL` handshake: it
waits once with the requested timeout, fallibly allocates the exact required
pixel count, then polls the retained queued frame with timeout zero. Returned
`Frame` pixels are an owned `Vec<u8>` copy and never borrow provider storage.
Timeout and clean stream stop remain distinct errors; a zero timeout is a poll.

The safe Rust wrapper captures diagnostics in a fixed local buffer during each
native call. If an error reports a larger required capacity, Rust preserves the
native error kind and required byte count but exposes no diagnostic string: a
valid UTF-8 prefix is not the complete diagnostic. The wrapper does not retry
Session operations merely to recover text because open and close have provider
and ownership side effects. Consequently, the current C ABI cannot guarantee
recovery of an arbitrarily long provider diagnostic after one such call.

## Native dependency baseline

Do not add unpinned `FetchContent`, build-time network access, or recursive
submodule expectations to the normal build. Select, review, and record a source
closure before adding upstream headers. The earlier review inventory is only a
candidate baseline and belongs under `docs/reference`, not a release lock.

Task 001 freezes the exact source closure in `upstream-provenance.md`. Production
`ams_mel_c` compiles the private adapter against those declarations and has no
link dependency on a provider. At runtime it loads the caller-selected library
locally, resolves the published C-linkage/C++-signature factories, and retains
the loader owner until `Control` and `API_Manager` destruction completes. Mock
providers and failure fixtures are test-only targets and are not installed.

Sessions now hold a shared private provider state. An IR stream retains that
state, so closing the public parent owner while a stream exists does not unload
provider code. Stream close stops adapter acceptance, calls `disable`, detaches,
destroys the last façade channel owners, waits for explicitly counted adapter
callbacks to return, and only then destroys provider `Buffer` objects and host
storage. Provider/library ownership is released last. Failed detach retains the
whole callback-accessible graph for a later close attempt rather than risking a
use-after-free; an open-time detach failure is retained internally because no C
owner can safely be returned.

A C2 channel likewise retains shared provider state. Submission preallocates its
completion, worker input, public owner, and thread holder before provider `send`.
After `send` yields a valid future, moving it into the prepared worker input,
arming that input's self-retention, and incrementing the channel request count
form one nonthrowing accounting sequence under the channel lock. No untracked
provider future can cross that boundary. The blocking (non-polling) worker owns
the provider future and C2 graph until terminal completion, calls `future::get()`
once, and caches success, MEL rejection, provider exception, null success, or
unknown-value failure. Public request close only removes that owner; it neither
joins nor cancels. C2 close stops submissions immediately and defers
disable/detach while requests are in flight. The final worker performs cleanup
and releases provider/library ownership.

Worker creation/detach or later adapter failures are internal failures, not
provider exceptions. If a future cannot be safely handed to a detached worker,
or orphan cleanup cannot safely detach, an intrusive atomic root plus an
already-armed `shared_ptr` self-cycle retains the graph without allocating or
taking a mutex. That emergency graph is intentionally permanent because no safe
completion path remains. A never-completing provider future likewise retains the
graph indefinitely rather than risking unload of live code.

## First integration profile

Start with IR host-memory, single-band Mono8 reception only after provider
loading, ownership, and shutdown tests exist. Use bounded owned copies first;
leases/zero-copy require a separate reviewed API and explicit lifetime contract.
Keep metadata conversions loss-aware. Do not convert every timestamp to one
nanosecond value or copy Squall-private integer frequency conventions.

The implemented queue owns copied pixels and has caller-selected finite
capacity. Overflow drops the incoming frame. Provider callbacks never enter C or Ada and
release each non-null callback buffer exactly once after processing. Release
status/exceptions poison the stream and wake receivers. The pinned interface
does not state that `disable()` waits for callbacks. More concretely, pinned
Squall `disable()` only clears its enabled state and delegates control disable;
its destructor resets `UdpDataReceiver`. The façade therefore treats successful
detach plus destruction of all façade channel owners—not `disable()`—as the
boundary after which no new image callback can begin, then waits for its own
in-flight callback count to reach zero. The mock has both quiescing-disable and
destruction-quiescing scenarios. Generic unregister was removed because it
cannot establish this boundary and can race non-quiesced provider callbacks.

The stream lifecycle is `Attached`, `Starting`, `Running`, `Stopping`, `Stopped`,
or terminal `Failed`. Any provider operation/release/rollback failure stops frame
acceptance and wakes receivers. Frames copied before a clean stop or failure are
drained first; the next receive reports `STREAM_STOPPED` or `PROVIDER_FAILED`.
At most one consumer thread/task may execute receive on a stream at a time.
Python follows the same external-serialization contract and does not use the GIL
or an added Python lock as a thread-safety claim. Its Frame pixels are an owned
`bytes` copy, and its ImageStream does not retain the Python Session object;
native child ownership keeps provider/library state alive when the parent closes
first.

## Parallel Ada work

Task 018 completes the required C2-specific metadata surface for native C and
safe Ada: BIT_Configuration, CommandStatus, and BIT_Status. All three callbacks
share one bounded FIFO callback-to-owned-event queue with DROP-INCOMING overflow.
The provider callback stack performs validation/deep copy only and never invokes
Ada application code. A successful Ada `Receive` returns a wholly Ada-owned graph
including every nested BIT/fault vector and string.

Upstream publishes no callback unregister. Callback state therefore belongs to
the existing C2 `ChannelState`, not the public metadata owner. Partial registration
failure publishes no metadata owner but retains already registered closures.
Metadata close merely deactivates the public queue. Final C2 cleanup stops
acceptance, disables/detaches under existing rules, destroys the provider channel,
waits for adapter callbacks already in flight, marks metadata stopped, and wakes
receivers. Channel destruction—not `disable()`—is the quiescence boundary.
Immutable received native snapshots are independent of that graph and can outlive
metadata, C2, Session, and provider library teardown.

Task 019 adds the application-facing inherited services on the existing C2
channel. KeepAlive reuses the Return request owner; CommsTest has a parallel
typed asynchronous owner but shares `ChannelState.requests`; both work in
Attached or Enabled lifecycle states. The inherited CommsTest callback is an
explicit fourth registration into Task 018's existing queue and uses the same
callback guard, retained state, and channel-destruction quiescence boundary.

Task 020 adds the required IR HealthAndStatus channel and exactly six callback
overloads: MFA_Status, BIT_Status, SubsystemStatusResp, DiscreteStatus,
MFA_SecurityAuditRecord, and MFA_StatusDetailed. LFStatus and NUC_TempData remain
out of scope. One bounded DROP-INCOMING FIFO deep-copies callback values before
returning to provider code; Ada polls that queue and copies each complete graph
again before releasing the native event. No provider thread invokes Ada code.

The callback state belongs to the Health channel, not the public metadata owner.
There is no unregister assumption: partial registration retains earlier closures,
metadata close only deactivates public consumption, and provider channel
destruction is the callback-quiescence boundary. Immutable native snapshots and
Ada values can outlive metadata, Health channel, Session, and provider unload.
Reusable Common-MEL status values live in `AMS.MEL.Status`; Health does not depend
on C2 metadata types. Rust sys and private Python declarations track the 46-export
raw ABI, but no safe Rust or public Python Health API is introduced.

Task 021 preserves the legacy Mono8 frame record and receive operation, adding an
opaque immutable full-FrameHeader snapshot owner instead. Callback-time copying
builds one complete adapter-owned queued frame and one bounded FIFO is consumed by
both legacy and snapshot receive. Snapshot views retain complete contributing sensor,
ordered flags, inertial/nav data, and independently discriminated orientation variants
until close; safe Ada copies the graph before releasing the native owner.

ChannelCapability is validated and deep-copied completely into an opaque native
snapshot, then copied again into reusable Ada-native `AMS.MEL.IR.Channel` values.
Neither snapshot references provider STL storage. `Channel::registerBuffer` and
`unregisterBuffer` are deliberately not application-visible: existing image
buffer registration remains adapter-managed. This completes application-facing
common C2 Channel services, not every raw Channel virtual method. Scheduling is
not part of Task 019.

Task 024 reuses that native ChannelCapability snapshot for the existing sole
ImageChannel. One internal Ada converter deep-copies a native capability view for
both C2 and Image; each caller independently owns acquisition, view, diagnostics,
and exception-safe native-owner closure.

Task 026 registers BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp in that order on
one Image metadata FIFO. A stream-retained
callback state supports synchronous registration callbacks and outlives public metadata
closure. It deep-copies each valid event into a bounded DROP-INCOMING FIFO with
saturating counters; ImageChannel destruction is the callback-quiescence boundary.
Native event owners and safe Ada event values remain valid after metadata, stream,
Session, and provider teardown. NavigationReport send,
LineOfSightQuaternion, and other optional Image metadata are not implemented.

Task 027A is an internal Image ownership refactor only. The opaque public Image
stream is a thin owner of shared `ImageStreamState`, which owns its SessionState,
frame CallbackState and Listener, Channel and ImageChannel, stream-owned Image
metadata state, buffer factory/configuration, provider Buffer objects, host
storage, and enable state. This permits a future asynchronous Image request to
retain the private resource graph without retaining `ams_mel_ir_stream *`.
It changes neither ABI 0.1 nor application-visible behavior. No NavigationReport
API exists yet; request submission, waiting, logical close with pending work, and
deferred cleanup remain future work.

Task 027B adds `ImageChannel::send(NavigationReport)` as an asynchronous
request/future vertical slice built on that shared `ImageStreamState`: the
complete published `NavigationReport` (including all 18
`PositionVelocityCovariance` terms) crosses the C ABI as
`ams_mel_navigation_report_v1`; submission is valid while the stream is
logically Attached or Running and does not require a prior Start; provider
`send()` executes with neither the frame callback mutex nor the Image metadata
mutex held, because a pinned provider may invoke the registered
`NavigationReportResp` metadata callback synchronously from within `send()`.
`ImageStreamState` gains a request counter, `cleanup_started` guard, and
`public_owner_closed` flag so that Stop/Close with an outstanding request
defers physical provider teardown (disable/detach/destroy) to final request
completion, mirroring the C2 `ChannelState`/`retain_failed` fail-safe pattern
with its own allocation-free emergency retention root. ABI 0.1 grows from 56 to
59 exports. Safe Ada exposes `Navigation_Report`, `Navigation_Request`, and
`Navigation_Result` in `AMS.MEL.IR.Image`; the canonical safe
`Navigation_Response` also moves there, with
`AMS.MEL.IR.Image.Metadata.Navigation_Response` becoming a source-compatible
subtype. Raw Rust and private Python ABI declarations track all 59 exports; no
safe Rust or public Python Navigation API is added.

Task 028 adds the conditionally required Instrumentation family
(`@RequiredIfInstrumentation`) in `native/src/ir_instrumentation.cpp` and the
new `AMS.MEL.IR.Instrumentation` Ada package with its `Metadata` child. The
slice is deliberately scoped to the Instrumentation-specific conditional
surface -- `InstrumentationChannel::send(InstrumentationLevelCmd)` and
`registerMetadataCallback(InstrumentationReport)` -- plus `Enable` and
`ChannelCapability`. Instrumentation-specific copies of the inherited generic
`Channel` services (KeepAlive, CommsTest, the ChannelCommsTest callback, and
`registerBuffer`/`unregisterBuffer`) are intentionally not cloned; those should
be generalized across non-C2 channel families in a later task.

The private `ChannelState` follows the proven Health/C2 shape: SessionState,
generic `Channel`, `InstrumentationChannel`, metadata callback state, request
count, an explicit Attached/Enabled/Failed/Closed lifecycle, a
`cleanup_started` serialization guard, and an allocation-free emergency
retention root. Asynchronous work never retains the public
`ams_mel_ir_instrumentation *`. Provider `send()` runs with the lifecycle mutex
released, because a provider is permitted to invoke the InstrumentationReport
callback synchronously from inside both `send()` and
`registerMetadataCallback`; `metadata_open` publishes and retains the callback
state before releasing the lifecycle lock and calling provider registration, so
neither synchronous path can deadlock. Close with pending requests prevents new
submissions, deactivates public metadata consumption, releases the public
owner, and defers disable/detach/provider-channel destruction and callback
quiescence to final request completion. One canonical
`ams_mel_ir_instrumentation_report_v1` serves both the
`RequestFor<InstrumentationReport>` completion and the metadata event; upstream
`Priority` is exposed as exactly Normal=0/Debug=1 with no invented
MaxExclusive. Capability snapshots reuse the existing native
`snapshot_capability` and the existing Ada `Capability_Conversion`. ABI 0.1
grows from 59 to 72 exports and native CTest from 9 to 10 targets. Raw Rust and
private Python declarations track all 72 exports; no safe Rust or public Python
Instrumentation API is added. Positive Instrumentation behavior is mock-proven
only: pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` returns
`nullptr` from `attachChannel` for Instrumentation, so real-provider validation
covers clean unsupported-provider failure and continued Session health only.

For each added operation: sketch Ada usage, define C ownership, implement the
adapter, test from a C-compiled client, add Ada import/wrapper/tests, update the
coverage matrix. Test failures must not be hidden by reducing assertions.

The root package `AMS` is owned by `ams_mel` for now. Do not duplicate it in
future companion crates. No standalone-library `Interfaces` clause is needed
in this bootstrap. No SPARK proof is claimed for the provider or FFI boundary.

The validated private-import layout is:

```text
AMS.MEL                 public package and resource owner
AMS.MEL_C_API           private sibling containing C representations/imports
```

`AMS.MEL` may legally depend on this private sibling. The earlier proposed
`AMS.MEL.Internal` / `AMS.MEL.Internal.C_API` hierarchy caused a private-child
dependency failure and is not to be recreated. The corresponding proposal in
`reference/AMS_GRA_MEL_Design_Review.md` is retained as historical material but
is explicitly superseded by this decision.

## References

The full proposal and its source list are preserved in
`reference/AMS_GRA_MEL_Design_Review.md`. An implemented behavior may differ from
a proposal only through a documented design decision and matching tests.

## Task 029A Track declaration dependency

Task 029A adds no Track adapter or public ABI. It vendors a measured,
declaration-only Boost 1.83.0 header closure so the byte-identical pinned IR
MEL `TrackChannel.h` can compile even though its deferred `TrackDataUpdate.h`
declaration includes Boost uBLAS. Boost remains a private native include and
is not exported through the C ABI. A compiler dependency check rejects system
Boost leakage. The presence of TrackDataUpdate declarations and Boost headers
does not implement TrackDataUpdate; Task 029B is the earliest possible Track
adapter task.
