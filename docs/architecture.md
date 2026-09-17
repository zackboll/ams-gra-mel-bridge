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

```text
C++ provider -> MEL API -> ams_mel_c -> Ada
                                     |-> Rust
                                     `-> private ctypes -> Python API
```

## Separation

1. `native/include`: genuine C11 declarations, no STL or C++ object layouts.
2. `native/src`: C++20 adapter implementation; CMake owns compilation.
3. `ada/src`: idiomatic public Ada plus private imported C declarations.
4. `rust/ams-mel-sys`: unsafe declarations for the reviewed subset of the C ABI.
5. `rust/ams-mel`: safe Rust API over `ams-mel-sys`; no direct C++ path.
6. `python/ams_mel`: safe Python API over a private, sixteen-function `ctypes`
   layer for Session/version, IR image streams, and IR C2 Operate; no direct C++ or
   provider-factory path.
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
