# AMS MEL — Language Bridge for Ada/SPARK, Rust, and Python GRA Skills

An independent, experimental **consumer-side language binding** for the
Agile Mission Suite Government Reference Architecture (**AMS GRA**)
Multi-Function Aperture Encapsulation Layer (**MEL**) interfaces.

The project preserves the published **C++ MEL provider boundary**, isolates
non-C++ interoperability inside a native adapter, exposes a small **C ABI**, and
builds idiomatic Ada, Rust, and Python interfaces above that ABI. The Ada API
can also serve SPARK-oriented Skills at the reviewed FFI boundary, allowing
higher-assurance application logic to remain in Ada/SPARK. The Rust consumer
and Python's private `ctypes` layer reuse the same C ABI without separate C++
interoperability implementations.

Native C++ Skills do **not** need this bridge: they can consume the published
C++ MEL interface directly. `ams_mel_c` exists to make that same provider
ecosystem practical for languages that should not have to model the C++ ABI
themselves.

> **Current status:** C and Ada support provider Session lifecycle, IR host-memory
> Mono8 reception, and all three required C2 command sends: general ModeCmd with
> complete ScanParam, BIT, and ConfigSet, plus all three required C2-specific
> metadata callbacks. Ada uses safe one-choice BIT operations and fully owned
> `AMS.MEL.IR.C2.Metadata` values for CommandStatus, BIT_Configuration, and
> BIT_Status (including complete nested faults). Provider callbacks are copied
> into a bounded native queue; provider callback threads never invoke Ada
> application code. Ada additionally provides `AMS.MEL.IR.Image.Full_Frame`, an owned complete
> FrameHeader snapshot; legacy `AMS.MEL.IR.Receive` remains the Mono8 compatibility
> subset. Image capability access and `AMS.MEL.IR.Image.Metadata` implement owned
> BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp events through one bounded
> DROP-INCOMING queue. `AMS.MEL.IR.Image` additionally exposes
> `Submit_Navigation_Report`/`Wait`/`Close` for `ImageChannel::send(NavigationReport)`:
> the complete published NavigationReport (all 18 covariance terms), asynchronous
> completion independent of the public Image_Stream/Session owners, and deferred
> provider teardown while a request is outstanding. LineOfSightQuaternion
> and other optional Image metadata remain unimplemented.
> C and Ada also implement the conditionally required Instrumentation channel
> (`AMS.MEL.IR.Instrumentation` and its `Metadata` child): `Open`, `Enable`,
> `Capabilities`, `Submit`/`Wait`/`Close` for
> `send(InstrumentationLevelCmd)`, and a bounded DROP-INCOMING queue for the
> `InstrumentationReport` callback with a blocking receive/wait, where timeout
> zero is the nonblocking poll case. Upstream `Priority` is exactly
> Normal/Debug, and one canonical report type carries both the future result
> and metadata events. Positive Instrumentation behavior is mock-validated;
> pinned Squall does not support this channel and is validated only for clean
> unsupported-provider failure. The Instrumentation-specific copies of the
> inherited generic Channel services (KeepAlive, CommsTest, buffers) are
> deliberately not cloned.
> C and Ada further implement the conditionally required Track channel's
> `@RequiredIfTrack` core (`AMS.MEL.IR.Track` and its `Metadata` child):
> `Open`, `Enable`, `Capabilities`, `Close`, and a bounded DROP-INCOMING queue
> for the `IRSTTrackReport` callback with a blocking receive/wait (timeout zero
> is the nonblocking poll case), returning a complete owned
> `IRST_Track_Report`. Upstream `IrstTrackState` is exactly
> Idle/Detected/Coast/Dropped and `IrstTrackMode` exactly Idle/Scan/Stare, with
> no invented MaxExclusive value; registration is one-shot because upstream has
> no unregister. Positive Track behavior and complete `IRSTTrackReport` payload
> fidelity are mock-validated, including a report emitted synchronously from
> inside registration. Pinned Squall does not attach this channel at all, so it
> validates clean unsupported-provider behavior only and provides no positive
> Track execution or Track-report evidence. C and Ada additionally implement the
> separately conditional `@RequiredIfTrackUpdate`
> `TrackChannel::send(TrackDataUpdate)` (`AMS.MEL.IR.Track.Updates`): the complete
> update including all 21 covariance terms, both epoch-second times, and the
> canonical Directional ECEF position/velocity, with an asynchronous request whose
> timeout is not cancellation, whose terminal result is cached, and whose
> CommandStatus rejection is deliberately distinguished from an ErrorOr rejection.
> Positive `TrackDataUpdate` behavior and payload fidelity are mock-validated
> only; pinned Squall cannot attach Track and so provides no positive
> `TrackDataUpdate` evidence. C and Ada also implement the separately optional
> (`@Optional`) `TrackChannel::send(SystemTrackDataResponse)`
> (`AMS.MEL.IR.Track.System_Data`): the complete response including signed
> nanosecond system time, verbatim range/rate/error values, and both
> azimuth/elevation pairs through the one canonical AzEl representation, with the
> same asynchronous request semantics and the same single Track request-accounting
> domain, so a pending update and a pending response together keep the provider
> graph alive until both complete. Positive `SystemTrackDataResponse` behavior and
> payload fidelity are likewise mock-validated only; pinned Squall provides no
> positive `SystemTrackDataResponse` evidence. The optional
> `RequestSystemTrackData` is complete as an inbound metadata callback: upstream
> declares no `send()` overload for it, so it is delivered through
> `registerMetadataCallback` and shares the one bounded Track metadata queue
> with `IRSTTrackReport` rather than being a `RequestFor<T>` operation. A
> `Return::NotSupported` refusal of that optional registration is non-fatal,
> whereas `Return::Fail` means the datatype is already registered on the channel
> and fails the open closed. The current Track status is therefore:
>
> ```text
> @RequiredIfTrack core                     complete
> @RequiredIfTrackUpdate TrackDataUpdate    complete
> SystemTrackDataResponse                   complete
> RequestSystemTrackData                    complete
> CandidateObjectMessage                    unimplemented
> CandidateObjectPreProcMessage             unimplemented
> Track API overall                         incomplete
> ```
>
> The raw Rust sys crate
> and private Python ctypes layer track the complete current 88-function C ABI.
> Safe Rust and
> Python remain intentionally constrained to
> Session, Mono8, Operate/TaskSched, and the empty/no-op BIT profile. Their mode and Return
> requests include timeout, cached repeated waits, structured
> rejection descriptions, and independent parent/channel/request lifetime. An
> opt-in real Squall integration harness validates C, Ada, safe Rust, and safe Python
> against the same pinned provider/runtime stack; it is not part of ordinary
> builds or CI. Python remains a dependency-free, development-use-only binding:
> payload-bearing BIT and general Mode/ConfigSet are absent from their safe APIs;
> Ada additionally exposes inherited C2 KeepAlive, CommsTest request/callback,
> and complete ChannelCapability values. RF, explicit generic buffer management,
> Scheduling, optional C2 operations, and safe Rust/Python common-channel or
> metadata APIs are not implemented, and there is no wheel/PyPI publication
> or zero-copy/NumPy image API. Raw declarations alone are not safe-language parity.

This is not an official C MEL standard, a replacement for AMS GRA, a Squall
binding, or a claim of GRA compliance.

---

## Why This Project Exists

AMS GRA defines open boundaries between mission-system components so that
software and hardware can evolve independently. At the sensor edge, a
**Multi-Function Aperture (MFA)** exposes capabilities through the
**MFA Encapsulation Layer (MEL)**.

The published MEL interfaces are C++. That works naturally for C++ Skills, but
it creates a difficult interoperability boundary for languages such as Ada,
Rust, and Python because a direct binding would need to understand C++ object
lifetimes,
virtual interfaces, exceptions, smart pointers, callbacks, and ABI details.

This project puts that complexity in one place:

```text
                              PROVIDER SIDE

    C++ MFA       Ada MFA       Rust MFA       Hardware MFA
       \             |             |                /
        \            |             |               /
         +--------- GRA MEL (C++) ----------------+
                         |
                   standardized
                     boundary
                         |
          +--------------+-----------------------------+
          |                                            |
          v                                            v
      C++ Skill                                    ams_mel_c
      (direct)                                    C ABI bridge
                                                      |
                                  +-------------------+-------------------+
                                  |          |          |               |
                                  v          v          v               v
                                Ada        SPARK      Rust            Python
                               Skill       Skill      Skill            Skill

                              CONSUMER SIDE
```

The important idea is that the project is **not tied to one MFA
implementation**.

A C++ MFA, a Rust-based MFA, simulated sensor software such as Squall, or real
Native C++ Skills
can consume the published C++ MEL interface directly. Ada, Rust, and Python
Skills can consume the implemented portions of that same provider interface
through the shared `ams_mel_c` compatibility layer.

For an Ada MFA, Rust MFA, or hardware MFA, the provider-facing MEL library may
still contain a thin C++ layer while the actual implementation lives in Ada,
Rust, firmware, another process, another processor, or physical hardware.

That distinction is important: **"Ada MFA" describes the implementation behind
MEL; it does not mean replacing the published C++ MEL contract.** A typical Ada
provider architecture could look like:

```text
GRA Skill
   |
C++ MEL API
   |
thin C++ MEL provider
   |
C ABI / IPC
   |
Ada MFA backend
```

The same pattern can be used for Rust or other implementation languages.

---

## Two Consumer Paths: Native C++ and the Language Bridge

`ams_mel_c` is **not** intended to replace the native C++ MEL API.

There are two legitimate consumer paths:

```text
Native C++ path

C++ Skill
   |
   | published C++ MEL API
   v
MEL provider
```

and:

```text
Non-C++ language path

Ada / SPARK / Rust / Python Skill
             |
             | language wrapper
             v
         ams_mel_c
          |
          | C++ adapter
          v
      MEL provider
```

The C++ path is shorter because C++ can naturally consume the published MEL
types and object interfaces. There is no reason to force a C++ Skill through a
C ABI merely for architectural symmetry.

The C ABI exists for languages where directly importing the C++ MEL object
model would create unnecessary ABI, ownership, exception, callback, and
toolchain coupling.

This makes `ams_mel_c` an **interoperability bridge**, not a new mandatory GRA
layer.

### Consumer Language Paths

| Skill language | MEL path | Project status |
|---|---|---|
| **C++** | Directly consumes the published C++ MEL API | Native GRA path; does not require `ams_mel_c` |
| **Ada** | Ada API → private C imports → `ams_mel_c` → C++ MEL | Implemented for the current IR vertical slice |
| **SPARK** | SPARK/Ada code → Ada binding → `ams_mel_c` → C++ MEL | Architectural/high-assurance consumer path; FFI/native boundary itself is not SPARK-proved |
| **Rust** | Safe Rust wrapper → `-sys` crate → `ams_mel_c` → C++ MEL | Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op implemented and mock-tested; current safe-Rust slice also validated against real Squall; no payload-bearing BIT, additional C2 commands/callbacks, or RF |
| **Python** | Python API → private `ctypes` → `ams_mel_c` → C++ MEL | Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op implemented and mock-tested; current IR slice also validated against real Squall; no payload-bearing BIT, additional C2/RF, zero-copy/NumPy API, or wheel/PyPI publication |
| **C** | Calls the `ams_mel_c` C ABI directly | Low-level bridge API |

This split is intentional. C++ already speaks the native MEL interface, while
the other language paths benefit from a stable language-neutral ABI.

## MFA, MEL, Skills, and Squall in Plain English

### MFA — Multi-Function Aperture

A **Multi-Function Aperture** is the sensor-side component that interacts with
the physical/free-space environment.

Depending on the system, an MFA can include:

- antennas or optical apertures;
- analog RF or optical electronics;
- digitizers;
- cameras or RF front ends;
- FPGA or embedded processing;
- OEM firmware;
- local signal processing; and
- interfaces that deliver digital sensor data to mission processing.

An MFA converts physical phenomena such as electromagnetic energy or photons
into digital information that mission software can process.

### MEL — MFA Encapsulation Layer

The **MFA Encapsulation Layer** is the standardized interface between an MFA and
the mission software that consumes it.

MEL allows a Skill to request, configure, and receive sensor capabilities
without depending directly on the internal implementation of the aperture.

Examples include:

- **RF MEL** for radio-frequency capabilities and high-rate I/Q data; and
- **IR MEL** for electro-optical / infrared capabilities and image data.

The MEL boundary is the important compatibility point for this project.

### Skill

An **AMS GRA Skill** is mission-processing software that consumes aperture data,
performs domain-specific processing, and can publish higher-level mission data
products.

For example:

```text
IR MFA
  |
IR MEL
  |
IR Search-and-Track Skill
  |
UCI observation / track products
```

or:

```text
RF MFA
  |
RF MEL
  |
RF Processing Skill
  |
UCI signal products
```

"Skill" is a GRA term, not an acronym.

### Squall

**Squall** is the simulated MFA used by the public AMS GRA Hello World Starter
Kit.

At a high level, Squall:

1. consumes simulated world truth through DIS;
2. models RF and optical sensing;
3. synthesizes RF I/Q streams and IR image frames; and
4. presents those capabilities to mission software through MEL.

Squall is especially useful as a development and integration target because
real sensor hardware is not required.

However:

> **`ams-mel-ada` is not a Squall-specific binding.**

Squall is one MEL provider. The goal of this project is to allow Ada, Rust, and
Python software to consume the implemented portions of the **standard MEL
provider boundary**, whether the provider happens to be Squall or something
else. Native C++ Skills continue to use that MEL boundary directly.

---

## Why Not Just Rewrite Squall or the MFA in Ada, Rust, or Python?

Rewriting Squall solves a different problem.

An Ada, Rust, or Python reimplementation or replacement for Squall would
answer:

> How can an MFA or sensor backend be implemented in another language?

This project answers:

> How can an Ada, Rust, or Python **Skill** consume an existing GRA MEL provider
> without becoming a C++ application?

At the same time, an existing C++ Skill can continue using MEL directly; this
project does not insert itself into that native path.

Those are independent choices.

```text
                         PROVIDER IMPLEMENTATION

      C++ MFA        Ada MFA        Rust MFA        Hardware/Firmware
         \              |              |                  /
          \             |              |                 /
             +--------- GRA MEL (C++) ----------------+
                              |
                   standardized boundary
                              |
                  +-----------+---------------------------+
                  |                                       |
                  v                                       v
              C++ Skill                               ams_mel_c
              (direct)                               C ABI bridge
                                                        |
                              +-------------------------+----------------------+
                              |             |             |                  |
                              v             v             v                  v
                            Ada           SPARK         Rust               Python
                           Skill          Skill         Skill               Skill

                           SKILL IMPLEMENTATION
```

This provides several advantages:

1. **Provider independence**  
   An Ada Skill is not coupled to Squall. The same Skill can potentially use a
   simulated provider, lab equipment, or a deployed hardware provider that
   implements the same MEL contract.

2. **Incremental adoption**  
   A program does not need to rewrite an existing C++ GRA ecosystem before
   introducing Ada, Rust, or Python Skills. Existing C++ Skills remain on the
   native MEL path.

3. **One C++ interoperability implementation**  
   C++ exceptions, object ownership, callbacks, virtual interfaces, provider
   loading, and teardown rules are handled once in `ams_mel_c`.

4. **Language-appropriate application code**  
   Ada can expose strong types, deterministic ownership, contracts, and
   potentially SPARK-verifiable logic. Rust can expose ownership and
   memory-safe systems abstractions. Python can support rapid prototyping,
   mission-algorithm experimentation, analysis, test automation, and integration
   with scientific/ML tooling. All can reuse the same underlying native bridge.

5. **Separation of assurance boundaries**  
   Complex C++ provider interaction can remain behind a narrow C ABI while
   higher-assurance application logic is implemented in Ada/SPARK.

6. **SPARK as a Skill implementation option**  
   A Skill can keep the MEL/FFI boundary in ordinary Ada and place selected
   deterministic algorithms, state machines, scheduling/resource logic, and
   safety/security invariants in SPARK. The goal is not to claim that C++ MEL
   or the FFI itself is formally proved; it is to make the boundary narrow
   enough that the proof-oriented portion of the application remains tractable.

7. **Ada on both sides of MEL when useful**  
   The provider backend and the consuming Skill are independent choices. An
   Ada MFA backend can expose the standard MEL boundary through a thin provider
   adapter, while an Ada or SPARK Skill can independently consume MEL through
   `ams_mel_c`. They need not be part of the same process or product.

---

## High-Level Architecture

At the GRA boundary, C++ has a direct path while non-C++ languages can use the
bridge:

```text
                         MEL provider
                              |
                    published C++ MEL API
                              |
                 +------------+-------------------+
                 |                                |
                 v                                v
             C++ Skill                        ams_mel_c
             (direct)                         C ABI bridge
                                                 |
                              +------------------+-------------------+
                              |          |          |               |
                              v          v          v               v
                            Ada        SPARK      Rust            Python
                           Skill       Skill      Skill            Skill
```

The C++ Skill does not call `ams_mel_c`. It uses the same C++ MEL API that the
bridge's internal adapter uses.

The consumer-side path currently implemented by this repository is the Ada
branch:

```text
                   SAME PROCESS

+--------------------------------------------------+
| Ada Skill                                        |
|                                                  |
|   AMS.MEL                                        |
|      |                                           |
|      v                                           |
|   private C imports                              |
|      |                                           |
|      v                                           |
|   libams_mel_c.so                                |
|      |                                           |
|      | C++ adapter                               |
|      v                                           |
|   vendor / Squall MEL provider .so               |
+----------------------+---------------------------+
                       |
                       | provider-specific transport
                       | (may be IPC, network, PCIe, etc.)
                       v
                MFA implementation
```

The Ada program, `libams_mel_c.so`, and the loaded C++ MEL provider currently
share a process because MEL is an in-process C++ object API.

The **MFA implementation behind the provider does not have to share that
process**.

Squall is a good example.

A simplified Squall deployment is:

```text
PROCESS 1
Ada Skill
  |
AMS.MEL
  |
ams_mel_c
  |
Squall C++ MEL provider
  |
  | gRPC / TCP
  v

PROCESS 2
Couloir
  |
  | gRPC / Unix-domain socket
  v

PROCESS 3
Squall RF or Optical Backend
Rust
```

High-rate sensor data can use a separate data path. In Squall, control traffic
is handled through RPC while raw sensor payloads can be delivered over UDP to
the MEL-side data endpoint.

The practical result is:

- **Ada ↔ C/C++** uses an in-process FFI/ABI boundary.
- **C++ MEL provider ↔ Squall Rust backend** can use IPC/network protocols.
- Ada, C++, and Rust therefore do **not** all have to execute in one process.

---

## Why a C ABI?

C is used here as an interoperability boundary, not as the primary application
language.

Instead of exposing this to Ada or Rust:

```text
C++ virtual classes
std::shared_ptr
templates
exceptions
provider-specific object layouts
C++ callback lifetime rules
```

the public native facade can expose language-neutral concepts such as:

```text
opaque handles
fixed-width integers
plain C structures
explicit create / destroy operations
status codes
caller-owned buffers
bounded copies
blocking wait with timeout; nonblocking poll when the timeout is zero
```

That gives the project one controlled native boundary:

```text
                           C++ MEL
                              |
                        C++ adapter
                              |
                           C ABI
                /             |             \
              Ada            Rust          Python
             wrapper        wrapper        wrapper
             /   \
            /     \
         Ada     SPARK
        Skill    Skill

C callers can use the ABI directly.

Native C++ Skills bypass this entire bridge and use C++ MEL directly.
```

The C ABI belongs to this project. It is **not** presented as an official GRA
C interface, and it is **not** intended to become an extra layer for native C++
Skills.

---

## Current Implementation Status

The current implementation is intentionally narrow.

Implemented:

- native C ABI version query;
- runtime loading of a compatible IR MEL provider;
- creation and initialization of the provider `Control`;
- complete provider version-information copy;
- IR `IRSTImage` channel attachment;
- provider-owned host buffer management;
- single-band `Mono8` image reception;
- validation and bounded copying of incoming frames;
- finite DROP-INCOMING receive queue;
- C receive operations that block until the timeout expires and degrade to a
  nonblocking poll only when the timeout is zero;
- idiomatic Ada receive interface usable as the boundary for Ada/SPARK applications;
- native C ABI and safe Ada C2 general ModeCmd with complete ScanParam,
  intended one-choice payload-bearing BIT, BIT no-op, and ConfigSet interfaces;
- native queue and safe Ada support for complete C2-specific BIT_Configuration,
  CommandStatus, and BIT_Status metadata;
- native C and safe Ada application-facing inherited C2 Channel services:
  KeepAlive, ChannelCommsTest request/reply/callback, and complete owned
  ChannelCapability snapshots;
- explicit lifecycle and callback-quiescence handling;
- native and Ada tests using a separately loaded C++ mock provider;
- safe Rust Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op
  APIs over the existing C ABI, including reusable `ReturnRequest` and typed
  Return completion/rejection behavior (`Return::Fail` is a normal completed
  result); safe Rust does not yet expose payload-bearing BIT;
- dependency-free Python Session, IR host-memory Mono8, C2 Operate/TaskSched,
  and BIT no-op APIs with owned-bytes frames, asynchronous mode/Return requests,
  cached waits, retryable C2 close, and independent parent/child lifetimes; and
- opt-in real Squall validation of the current C, Ada, safe Rust, and safe Python
  IR slices.

Not yet implemented:

- a real hardware provider integration;
- SPARK proof of the native/FFI boundary;
- explicit generic Channel buffer management;
- optional/conditional C2 commands;
- safe Rust/Python catch-up for the expanded Task 017 command surface;
- safe Rust/Python C2 metadata APIs;
- safe Rust/Python common-channel APIs;
- RF MEL;
- stacked images;
- tracking interfaces;
- OMS/UCI integration;
- OpenCV processing;
- device-memory buffers;
- zero-copy/NumPy image views;
- Python wheel/PyPI publication; or
- full AMS GRA compliance.

The present work should be viewed as a validated **vertical slice** of the
consumer architecture rather than a complete MEL binding.

---

## Callback and Data Ownership Model

Provider callbacks do **not** call directly into Ada application code.

For the current IR receive profile:

```text
C++ MEL provider callback
          |
          v
native adapter
  validate metadata
  copy Mono8 pixels
          |
          v
bounded native queue
          |
          v
Ada Receive (timeout > 0)
  blocking wait; timeout 0 is a nonblocking poll
          |
          v
Ada-owned frame
```

`Receive` with a positive timeout blocks on a condition variable inside the
native adapter until an event arrives, the stream stops, or the timeout
expires. A zero timeout is a nonblocking poll. No Ada thread spins, and no Ada
application code runs on a provider callback thread. An application may of
course choose to write its own polling loop with a zero timeout, but that is an
application decision, not the design of this binding.

This isolates provider callback threads from Ada code and gives the binding an
explicit place to enforce ownership, validation, queue capacity, shutdown, and
error handling.

The current queue owns copied pixels. Overflow drops the incoming frame.
Zero-copy or leased-buffer APIs require a separate lifetime contract and are
intentionally deferred.

---

## Repository Layout

```text
native/
  include/               C11 public declarations
  src/                   C++20 MEL adapter implementation
  CMakeLists.txt

ada/
  src/                   idiomatic Ada API and private C imports
  tests/                 Ada integration tests

rust/
  ams-mel-sys/            unsafe declarations for the reviewed C ABI subset
  ams-mel/                safe Session, IR Mono8, and C2 API and tests

python/
  ams_mel/                 dependency-free Session, IR Mono8, and C2 API via ctypes
  tests/                   Python mock-provider and C ABI drift tests

docs/
  architecture.md        implemented architecture decisions
  c-abi-policy.md        ownership and ABI rules
  coverage.md            binding coverage
  packaging.md           build/package design
  upstream-provenance.md pinned upstream declaration provenance
  tasks/                 incremental implementation tasks
  reference/             retained design/reference material

.clinerules/              repository workflow and architecture rules
.github/workflows/        CI
scripts/                  local validation helpers
integration/squall/       opt-in real Squall IR MEL validation
```

There is deliberately no root Alire crate.

The current crates are:

- `ams_mel_c` under `native/`
- `ams_mel` under `ada/`
- Cargo workspace crates `ams-mel-sys` and `ams-mel` under `rust/`

CMake owns native C/C++ compilation. The Ada project consumes the resulting
native library rather than recompiling the C++ adapter through GPRbuild.
Cargo likewise links that externally built library; neither Rust crate compiles
native C++, vendored MEL headers, Squall, or a provider. The Rust crates have not
been published to crates.io. The Python binding is used directly through
`PYTHONPATH`; no wheel or PyPI package has been published.

---

## Build the Native Library

Requirements:

- Linux
- CMake 3.20+
- Make or Ninja
- C++20 compiler

Run:

```sh
make test-native
```

This builds the contract-test facade and mock providers:

```text
native/build-tests/lib/libams_mel_c.so
native/build-tests/test-providers/
```

and runs the native ABI/provider/IR-stream tests from that tree.

The production facade built by `make native` stays in a separate tree:

```text
native/build/lib/libams_mel_c.so
```

Production and contract-test builds never share a CMake build directory: the
test configuration compiles `AMS_MEL_ENABLE_TEST_FAILPOINTS` into the facade and
the production configuration does not, so a shared tree would let one workflow
silently rebuild the other's library. `make test-build-isolation` proves the two
trees stay separate. See
[`docs/corrective-native-build-tree-isolation.md`](docs/corrective-native-build-tree-isolation.md).

Real Squall validation is deliberately separate from ordinary builds and CI.
With an existing checkout at the pinned revision, run:

```sh
SQUALL_SOURCE_DIR=/path/to/ams-gra-hello-world-sk-sensors-squall \
  make test-squall-ir
```

This runs the C, Ada, Rust, and Python integration clients in one Squall startup.
All four validate BIT no-op plus TaskSched and Mono8. Use
`make test-squall-ir-c`, `make test-squall-ir-ada`, or
`make test-squall-ir-rust`, or `make test-squall-ir-python` to select one client. See
`integration/squall/README.md`, `docs/task-004-validation.md`, and
`docs/task-009-validation.md`, `docs/task-013-validation.md`, and
`docs/task-015-validation.md` and `docs/task-016-validation.md` for runtime,
revision, cleanup, and evidence.

To test another compiler, use a separate build directory:

```sh
CC=clang CXX=clang++ cmake -S native -B build/clang \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release

cmake --build build/clang --parallel 2
ctest --test-dir build/clang --output-on-failure
```

---

## Build Ada with Alire

Requirements:

- hosted GNAT with Ada 2022 support;
- GPRbuild;
- Alire; and
- the native prerequisites above.

Run:

```sh
alr -C ada build
alr -C ada/tests run
```

Equivalent test action:

```sh
alr -C ada/tests test
```

To test with GNAT/GPRbuild directly:

```sh
make test-ada
```

The Ada binding links the production facade in `native/build/lib`, and an
ordinary Ada build both links and runs it. The contract tests exercise the
test-only failpoints, so they link the production facade but run against the
contract-test facade in `native/build-tests/lib`: the test project emits
`DT_RUNPATH`, which `LD_LIBRARY_PATH` overrides, and both `make test-ada` and
`alr -C ada/tests run` set it along with `AMS_MEL_TEST_PROVIDER_DIR`.

Format and verify Ada sources with:

```sh
make format-ada
make check-ada-format
```

GNATformat is pinned through the Ada test/development Alire environment. The
canonical Ada line width is 100 characters, and CI rejects formatting drift.

The current development manifests contain relative development pins and are not
yet registry-ready release manifests.

---

## Build Rust

Requirements:

- stable Rust with Cargo and Clippy; and
- the native prerequisites above.

Build the externally owned native library and mock providers first, then run the
workspace checks:

```sh
make test-native
cargo check --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml --workspace
cargo clippy --manifest-path rust/Cargo.toml \
  --workspace --all-targets -- -D warnings
```

An ordinary or direct `cargo` build uses the build script default, which is the
production facade in `native/build/lib`, so downstream Rust builds never link a
test-enabled native library. Set `AMS_MEL_NATIVE_LIB_DIR` to select another
existing CMake build's library directory and `AMS_MEL_TEST_PROVIDER_DIR` to
select its `test-providers` directory. `make test-rust` and the Rust CI job set
both explicitly to the contract-test tree (`native/build-tests`). Separately,
the repository's provider-path helpers and the ABI probe fall back to that test
tree when those variables are unset; that fallback applies only to the
repository's own contract tests, not to the build script's library default.
Cargo never invokes CMake or compiles the native adapter. The safe
layer is `ams-mel -> ams-mel-sys -> ams_mel_c`; neither crate is published.
Rust `Frame` values own copied `Vec<u8>` pixels; this is not a zero-copy API.
Safe Rust BIT support is intentionally limited to `submit_bit_noop`; no
payload-bearing BIT data is exposed. `ReturnRequest` is reusable for cached
terminal waits, and timeout or close/drop does not cancel provider work.
The ordinary Rust tests use the mock provider only. Task 009's preserved,
opt-in evidence records successful real Squall validation.

---

## Use Python

Python 3.11 or newer is the development target. Build the native façade and
mock providers, set the explicit façade path, and place `python` on `PYTHONPATH`:

```sh
make test-python

PYTHONPATH=python \
AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
  python3 -W error -m unittest discover -s python/tests -v
```

The path passed to `Session.open` is the separate provider library. The safe
API accepts only `str` provider paths (including `os.PathLike` values whose
`os.fspath` result is `str`) and strictly encodes all inputs as UTF-8. Python
uses `ams_mel -> private ctypes -> ams_mel_c -> C++ MEL`; it neither models nor
loads C++ provider interfaces directly. This is not a native extension, wheel,
published package, or zero-copy API. The binding has no external Python
dependencies and is intended for development use. Session, IR host-memory Mono8,
and C2 Operate/TaskSched are implemented with owned-bytes frames, asynchronous
mode requests, and independent parent/child lifetimes. The current IR slice has
real Squall validation. RF and additional C2 operations are not implemented;
NumPy/zero-copy image views and packaging remain absent.

---

## Local Validation Gate

Run:

```sh
make check
```

The gate validates the native implementation, Ada integration, and repository
checks. It fails when a required Ada toolchain is unavailable rather than
silently skipping the Ada validation.

---

## Ada/SPARK, Rust, and Python Consumer Paths

### SPARK

SPARK is not a separate binary ABI from Ada. A SPARK-oriented Skill can use the
same Ada binding and keep the language boundary in a small ordinary-Ada package:

```text
C++ MEL provider
      |
  ams_mel_c
      |
ordinary Ada FFI / ownership wrapper
      |
   SPARK core
      |
proved application logic
```

This creates a natural **trusted-boundary pattern**: the native/C++ interaction
is isolated and reviewed, while selected logic above it can be written and
proved in SPARK.

Potential SPARK candidates include:

- deterministic state machines;
- sensor/resource arbitration;
- scheduling and bounded queues;
- validity and range invariants;
- track/state management;
- command validation; and
- other logic where explicit contracts and proof are valuable.

The current project does **not** claim SPARK proof across the C or C++ boundary.

### Rust and Python

The Rust and Python consumers reuse the same native boundary rather than binding
the C++ MEL API independently.

### Rust

The Rust implementation is structured as:

```text
rust/
  ams-mel-sys/       raw C ABI declarations
  ams-mel/           safe Rust ownership/API wrapper
```

Conceptually:

```text
Rust Skill
    |
safe ams-mel crate
    |
ams-mel-sys
    |
C ABI
    |
ams_mel_c
    |
C++ MEL provider
```

The safe Rust layer turns opaque handles and explicit C lifecycle operations into
Rust ownership types and RAII-managed Session, ImageStream, ControlChannel, and
ModeRequest and ReturnRequest resources for the current IR slice. Both request
types are neither `Send` nor `Sync`; their timeout and close/drop operations do
not cancel provider work. BIT is only the empty/no-op profile, not a generic or
payload-bearing API.

### Python

The dependency-free Python binding sits on the same C ABI:

```text
Python Skill
     |
Python package
     |
     private ctypes layer
     |
C ABI
     |
ams_mel_c
     |
C++ MEL provider
```

The current implementation uses `ctypes` for Session, owned-copy IR Mono8
reception, and C2 Operate/TaskSched. Frames own Python `bytes`; mode requests are
asynchronous and preserve cached waits, retryable C2 close, and independent
parent/child lifetimes. The current IR slice is validated against real Squall.
It does not provide RF, additional C2 operations, a native extension,
zero-copy/NumPy image views, wheels, or PyPI publication.

Python is particularly attractive for:

- rapid Skill prototyping;
- algorithm exploration;
- test and simulation tooling;
- data analysis and visualization;
- NumPy/SciPy/OpenCV integration;
- ML inference and experimentation; and
- orchestration around native high-performance processing.

A Python API should still preserve explicit ownership and bounded-buffer rules.
It should not assume that Python callback execution is appropriate on arbitrary
provider threads, and high-rate data paths need to account for copies, Python
object creation, and interpreter/GIL behavior.

### C++

C++ needs none of these wrappers:

```text
C++ Skill
    |
published C++ MEL API
    |
C++ MEL provider
```

This direct path remains the reference/native consumer path.

The overall result is:

```text
                              GRA MEL
                                 |
                 +---------------+----------------------+
                 |                                      |
                 v                                      v
             C++ Skill                              ams_mel_c
             direct                                     |
                                      +----------------+----------------+
                                      |        |         |              |
                                      v        v         v              v
                                    Ada      SPARK     Rust           Python
                                   Skill     Skill     Skill           Skill
```

This keeps provider ownership, callback handling, exception containment, and
C++ ABI compatibility in one bridge for the languages that need it, while
leaving native C++ consumers untouched.

---

## Project-Relevant Acronym Glossary

The GRA ecosystem contains many acronyms. These are the ones most relevant to
this repository and the surrounding Hello World architecture.

| Acronym | Meaning | Relevance |
|---|---|---|
| **AMS** | Agile Mission Suite | Mission-system architecture family. |
| **GRA** | Government Reference Architecture | Government-defined reference architecture; together, **AMS GRA**. |
| **AMS GRA** | Agile Mission Suite Government Reference Architecture | The open mission-system architecture this project targets. |
| **MFA** | Multi-Function Aperture | Sensor/aperture component that interacts with the free-space environment and produces digital sensor data. |
| **MEL** | MFA Encapsulation Layer | Standardized software boundary between an MFA and mission processing. |
| **RF** | Radio Frequency | Radio-frequency sensor domain. |
| **IR** | Infrared | Infrared / optical sensor domain. |
| **EO/IR** | Electro-Optical / Infrared | Optical and infrared sensing domain. |
| **IRST** | Infrared Search and Track | IR sensing/processing function; `IRSTImage` appears in the current receive profile. |
| **I/Q** | In-phase / Quadrature | Complex sample representation commonly used for digitized RF data. |
| **RX** | Receive | Receive-side RF operation. |
| **TX** | Transmit | Transmit-side RF operation. |
| **DIS** | Distributed Interactive Simulation | Simulation protocol used by the Hello World environment to distribute world truth. |
| **OMS** | Open Mission Systems | Mission-system standards used for service/data interoperability. |
| **UCI** | Universal Command and Control Interface | Standard message/data model used for higher-level mission products. |
| **CAL** | Critical Abstraction Layer | OMS software boundary that isolates services from underlying transport details. |
| **LA-CAL** | Language-Agnostic Critical Abstraction Layer | CAL pattern that allows services written in different languages to connect through a language-neutral protocol. |
| **OWP** | OMS WebSocket Protocol | WebSocket protocol used by the Hello World LA-CAL/Sleet implementation. |
| **ASB** | Abstract Service Bus | Logical OMS messaging network carrying UCI data among services. |
| **MASI** | Mission Agnostic Service Infrastructure | Shared mission-system infrastructure such as routing, management, health, and observability services. |
| **MPU** | Minimum Procurable Unit | AMS GRA unit intended to be independently procured/integrated. |
| **OEM** | Original Equipment Manufacturer | Manufacturer of sensor hardware/firmware behind an MFA. |
| **C2** | Command and Control | Mission command/control information and messaging. |
| **API** | Application Programming Interface | Source-level programming interface exposed to callers. |
| **ABI** | Application Binary Interface | Binary contract between compiled components; `ams_mel_c` exposes a C ABI. |
| **FFI** | Foreign Function Interface | Mechanism through which one programming language calls code written in another. |
| **IPC** | Inter-Process Communication | Communication between separate operating-system processes. |
| **RPC** | Remote Procedure Call | Request/response interaction across a process or network boundary. |
| **gRPC** | gRPC remote-procedure-call framework | RPC technology used in Squall's backend control path. |
| **TCP** | Transmission Control Protocol | Reliable byte-stream network transport. |
| **UDP** | User Datagram Protocol | Datagram transport useful for high-rate data paths such as Squall sensor payload delivery. |
| **UDS** | Unix-Domain Socket | Local IPC socket used by Squall/Couloir for backend gRPC communication. |
| **SPARK** | SPARK language/toolset for high-assurance Ada | Ada-based language/toolset used for contract-based analysis and formal proof; a SPARK Skill can use the same Ada MEL binding while keeping native FFI code outside the proved core. |

---

## Terminology at a Glance

A useful mental model for the larger ecosystem is:

```text
Physical / simulated world
          |
          v
         MFA
   sensor / aperture
          |
          | MEL
          v
        Skill
   raw-data processing
          |
          | OMS / UCI
          v
Mission-system services
```

For the public Hello World system:

```text
DIS truth
   |
   v
Squall simulated MFA
   |
   +---- RF MEL ----> RF Skill ----+
   |                               |
   +---- IR MEL ----> IR Skill ----+----> OMS / UCI
```

For this project:

```text
      C++ MFA      Ada MFA      Rust MFA      Hardware MFA
         \           |            |              /
          +--------- GRA MEL (C++) -------------+
                         |
             standardized boundary
                         |
              +----------+------------------------------+
              |                                         |
              v                                         v
          C++ Skill                                 ams_mel_c
          direct                                        |
                                  +--------------------+------------------+
                                  |         |          |                 |
                                  v         v          v                 v
                                Ada       SPARK      Rust              Python
                               Skill      Skill      Skill              Skill
```

---

## Provider and Skill Languages Are Independent

One of the architectural goals is to make these choices independent:

```text
Provider / MFA implementation:
    C++ | Ada | Rust | hardware / firmware | other

                    ↓

              standard GRA MEL

                    ↓

Skill implementation:
    C++ | Ada | SPARK | Rust | Python | other bridgeable language
```

An **Ada MFA** and a **SPARK Skill** therefore solve different problems and can
exist independently:

```text
Ada MFA backend
      |
thin C++ MEL provider
      |
==== standardized MEL boundary ====
      |
ams_mel_c
      |
Ada binding
      |
SPARK Skill
```

They also do not have to be in the same process. MEL-provider internals may use
FFI, IPC, network transport, device drivers, or other implementation-specific
mechanisms behind the standardized boundary.

## Scope and Compatibility

The C-facing ABI remains experimental version **0.1** and is unrelated to:

- an upstream MEL API version;
- an AMS GRA architecture revision; or
- a provider's own version number.

Linux x86-64 is the initial validation target.

The headers contain normal Windows visibility declarations for future work, but
that is not a claim of tested Windows compatibility.

No SPARK proof claim is made for the provider or FFI boundary.

Provider declarations used by the native adapter are pinned and documented in
`docs/upstream-provenance.md`.

---

## Design Principles

1. **Preserve the published C++ MEL provider boundary.**
2. **Do not invent a competing provider protocol.**
3. **Keep C++ implementation details out of Ada/SPARK, Rust, and Python APIs.**
4. **Use a small, explicit, ownership-aware C ABI for languages that need a bridge.**
5. **Do not force native C++ Skills through the C ABI; let them consume MEL directly.**
6. **Contain exceptions at the native bridge boundary.**
7. **Do not call arbitrary language-runtime application code from provider callback threads.**
8. **Prefer bounded ownership and explicit lifetimes before zero-copy designs.**
9. **Keep provider loading and unloading safe across live child resources.**
10. **Make metadata conversions loss-aware.**
11. **Allow MFA implementation language and Skill implementation language to vary independently.**
12. **Preserve a narrow ordinary-Ada boundary so SPARK logic can sit above it without pretending the native FFI is proved.**
13. **Add capability vertically: provider boundary, C tests, language wrapper, language tests, documentation.**

---

## Background References

Useful public background material:

- AMS GRA Hello World — Overview and Terminology  
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/0-overview.html

- AMS GRA Hello World — How to Build a Multi-Function Aperture  
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/3-build-mfa.html

- Open Arsenal — Common MEL  
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel

- Open Arsenal — RF MEL  
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel

These references describe the surrounding GRA concepts. The behavior and
supported surface of this repository are defined by this repository's own
source, tests, architecture decisions, and pinned upstream provenance.

---

## Licensing

New scaffold and binding code is supplied under Apache-2.0; see `LICENSE`.

Vendored upstream declaration material retains its upstream licensing and
provenance. See:

```text
docs/upstream-provenance.md
docs/upstream-files.sha256.md
```

before changing, replacing, or publishing the pinned declaration closure.
