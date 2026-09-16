# AMS GRA MEL C and Ada Binding Design Review

**Review date:** September 15, 2026
**Status:** engineering proposal; no implementation or publication performed
**Primary recommendation:** one C ABI, implemented in C++20 against the existing MEL contracts, with an Ada API developed alongside each completed native feature. Add Rust later against that same ABI.

## 1. Scope and evidence

This review classifies the 22 public Open Arsenal repositories, examines selected Common/RF/IR MEL headers and supporting libraries in detail, and traces the reference Skills' usage and deployment requirements. It is not a line-by-line audit of every repository or a completed assessment of the normative architecture PDFs. No upstream build, actual provider loading, end-to-end test, or hardware execution was performed.

The companion `upstream_review_snapshot.json` records the umbrella repository's 14 submodule commit references and all 22 repository roles. Those references are a **candidate baseline**, not proof of a working combination. Source reads used the default branches; the first implementation milestone must freeze and recheck the selected revisions. The umbrella inventory and the GitLab release manifest are separate sources and must not be assumed equivalent. [S01–S03]

Statements describing upstream behavior below are observations from source. Package names, API shapes, lifecycle guarantees, milestones and build policies are recommendations, not existing Open Arsenal features.

## 2. What to build—and what not to build

The deliverable should be described as **a C language binding/adaptation library for the AMS GRA C++ MEL interfaces**. It is not an official government C MEL standard and is not a native-C rewrite of the sensor provider.

Proposed dependency boundary:

```text
C application         Ada application            Rust application (later)
      |                idiomatic Ada               safe Rust
      |                private C imports           unsafe sys crate
      +----------------------+--------------------------+
                             |
                    versioned C ABI façade
                             |
                  private C++20 translation
                             |
                  existing C++ MEL provider API
                             |
              Squall OR another compatible provider
```

OMS/UCI messaging is a parallel application concern:

```text
Ada Skill -- separate OMS/UCI client -- LA-CAL / OWP -- Sleet or another server
    |
    +-------- MEL binding ------------ vendor sensor provider
```

Squall's gRPC, Couloir and UDP mechanisms are provider internals. Using those directly would create a Squall client, not a general MEL binding. Sleet, OpenCV, JSBSim, Cesium, image processing and RF algorithms do not belong in the generic MEL runtime library. [S23, S25, S26, S28]

The initial binding is **consumer-side**: C/Ada programs can use existing C++ providers. Supporting a provider implemented entirely in C, Ada or Rust is a different, bidirectional adapter project and should remain out of scope for the first release.

## 3. Repository-by-repository coverage

The long names below are retained to avoid ambiguity. Detailed review depth and available candidate commit references are in the JSON companion.

| Repository | Role | Binding decision |
|---|---|---|
| `a-gra` | autonomy architecture | Adjacent architecture, not an AMS MEL binding dependency. |
| `a-gra-test-harness` | autonomy messaging compliance harness | Not a MEL conformance test suite. |
| `ams-gra` | normative architecture, IDDs and compliance artifacts | Normative requirements cross-check before making conformance claims. |
| `ams-gra-hello-world-sk` | umbrella repository | Inventory and candidate dependency snapshot; not a runtime package. |
| `ams-gra-hello-world-sk-getting-started` | integration/reference deployment | Not a runtime library dependency. |
| `ams-gra-hello-world-sk-infra-sleet` | OMS LA-CAL server | Separate messaging integration target; not part of MEL ABI. |
| `ams-gra-hello-world-sk-interfaces-common-mel` | shared C++ MEL definitions | Direct native source dependency. |
| `ams-gra-hello-world-sk-interfaces-ir-mel` | IR C++ MEL definitions | First sensor-domain binding target. |
| `ams-gra-hello-world-sk-interfaces-rf-mel` | RF C++ MEL definitions | Second sensor-domain binding target. |
| `ams-gra-hello-world-sk-libraries-ams-math` | C++ math and units support | Compiled static library with Boost dependency; keep internal initially. |
| `ams-gra-hello-world-sk-libraries-ams-vita` | AMS-tailored VITA packet library | Header-only generated C++; keep internal unless raw packet APIs are exposed. |
| `ams-gra-hello-world-sk-sensors-squall` | reference sensor provider | Integration-test provider; private gRPC/UDP not the public binding contract. |
| `ams-gra-hello-world-sk-sim-supercell` | scenario and flight-dynamics simulator | Optional full-stack test stimulus, not a binding dependency. |
| `ams-gra-hello-world-sk-skills-ir-search-and-track` | C++ IR example Skill | Reference usage/lifecycle and integration acceptance model. |
| `ams-gra-hello-world-sk-skills-rf-fm-demod` | C++ RF example Skill | Reference RF job/receive usage; self-assessment is not certification. |
| `ams-gra-hello-world-sk-test-la-cal-harness` | LA-CAL server protocol tests | Does not validate MEL ABI or entire Skill compliance. |
| `ams-gra-hello-world-sk-viz-graupel` | DIS/UCI to CZML bridge | Optional visualization acceptance target. |
| `ams-gra-hello-world-sk-viz-worldview` | browser visualization | Optional demonstration UI; do not require map assets in library CI. |
| `oms` | OMS standard | Separate requirements for a complete mission Service. |
| `oms-ghost-detector` | OMS training application | No dependency for the proposed MEL library. |
| `oms-sk-cal` | OMS training/reference CAL | Context and examples; do not assume this supplies MEL bindings. |
| `uci` | UCI schemas and standard | Schema and message semantics for the separate OMS/UCI client. |

### Dependency conclusions

The direct native source closure begins with `common-mel`, `ir-mel`, `rf-mel`, `ams-math` and `ams-vita`, plus their actual third-party requirements. The reference provider and reference applications are essential integration examples but should not become installed dependencies of the binding. [S02, S21–S26, S30–S32]

The getting-started README also refers to `interfaces/oms-cal`, but that component is not in the inspected umbrella's complete submodule list or release manifest. Treat this as a documentation/reference discrepancy to resolve, not evidence that an additional hidden package must be linked. [S01–S03]

## 4. Common MEL: shared types and asynchronous requests

### 4.1 The significant shared types

`common-mel` includes version information, identifiers, errors, built-in-test data, navigation, component/status information and security-audit-related types. It is not merely a header of integer aliases. The examined common header declares `CMN_MEL_API_VERSION` as `4.0`; `VersionInfo` separately contains integer API/library versions and vendor/description strings. [S04]

Keep distinct identifiers for:

- architecture/specification revision;
- upstream source revisions;
- provider API and implementation versions;
- the new C ABI version;
- Ada/Rust package semantic versions.

A crate version cannot establish binary compatibility with an arbitrary vendor library.

### 4.2 Requests are not synchronous status-returning calls

The actual template is:

```cpp
using RequestFor = std::future<ErrorOr<std::shared_ptr<T>>>;
```

`ErrorOr` holds either an error or a value; selecting the wrong alternative can throw. A request can be pending even when its creation succeeded. Successful results may own provider resources. [S04, S05]

**Proposed C mapping:** typed opaque request handles with poll/wait and one-shot result collection. Preserve the difference between submission failure, pending, wait timeout, completed provider error, and completed success. A convenience blocking function may be layered above that machinery.

A wait timeout must not be documented as cancellation. A dropped request cannot be assumed to cancel work on hardware. The adapter must retain resources needed by outstanding futures and must not unload the provider while completion may still execute provider code. A wrapper also cannot promise a finite deadline for an opaque provider function that itself blocks without cancellation support.

### 4.3 Identifier semantics need a decision

`UCI_ID` consists of a 16-byte UUID and a descriptive label; its equality compares both. Preserve both values in the binding. Separately document any UUID-only identity comparison introduced for application use. [S06]

There is a concrete source-level concern in its ordering operator: it combines UUID comparison and label comparison using logical AND, rather than lexicographic ordering. A counterexample to strict weak ordering is:

```text
A = (uuid 0, label "a")
B = (uuid 0, label "c")
C = (uuid 1, label "b")

Neither A < B nor B < A.
Neither B < C nor C < B.
But A < C.
```

This is an inspection finding, not a reported test failure. Avoid depending on that comparator in new maps/sets; seek upstream clarification rather than silently editing the published interface. [S06]

## 5. IR MEL: the best first vertical slice

### 5.1 Factories are C-linked, not C-compatible

The IR public factory declarations include C++ strings/string views and `std::shared_ptr` arguments/results. Their comments explicitly explain the Clang warning suppression: `extern "C"` supplies symbol linkage for dynamic lookup, but those functions are not intended to be called as ordinary C functions. [S07, S08]

The new façade must expose genuinely C-compatible functions. It must load and call the existing provider factories from C++, then retain the returned C++ owners internally.

This introduces two different boundaries:

```text
Ada/C/Rust ↔ façade: your controlled C ABI
façade ↔ provider: the upstream C++ ABI still exists
```

Compiler, standard-library, exception/RTTI and runtime compatibility still matter on the second boundary. Hiding exported symbols or statically linking a standard library does not turn C++ objects into portable C objects. A provider version getter is not a safe universal ABI probe: calling it already requires compatible C++ objects. Use known-compatible build profiles and trusted provider artifacts; the façade is not a sandbox, and exception translation cannot repair ABI-related undefined behavior. [S07, S24]

### 5.2 Control, channel and application responsibilities

`Control` initializes the library, queries capabilities/version, attaches channels and detaches them. A `Config` includes channel/platform UCI identifiers, sensor location, channel type and an image listener. `Channel` provides enable/disable, buffer registration, capabilities, keepalive/comms requests and callback registration. [S07, S10, S12]

The service must implement `ImageListener`. Image storage is created by the service and must be returned for reuse when processing finishes. Thus the binding must provide C++ implementations/adapters for service-side callbacks and buffers, not just wrappers for provider methods. [S08, S09]

The factory type list covers Image, C2, HealthStatus, Track, Instrumentation, Scheduling and StackedImage channels. A Mono8 image receive implementation is a valuable subset, not the whole IR API. Interpret each Required/RequiredIf annotation for the responsible component and selected capability profile; do not automatically treat every provider obligation as a requirement that every consumer binding expose the entire header set. [S13]

### 5.3 Buffer lifetime is an explicit protocol

The inspected contracts distinguish:

- the base buffer address from the image address;
- ownership of the C++ Buffer object from permission to reuse its contents;
- registering storage from enabling acquisition;
- disabling a channel from detaching it.

Buffers are registered while the channel is not enabled. The image callback hands a buffer to the service, and `Buffer::release()` makes it available for another image. Merely destroying a `shared_ptr` is not the documented release-for-reuse operation. [S08–S10]

**Recommended first implementation:** a bounded native buffer/queue implementation with an owned-copy receive path. The provider callback copies validated frame content and necessary metadata into adapter-owned storage, releases the provider buffer on all completed paths, and never invokes Ada code. C/Ada consumers poll or wait on the adapter queue.

An optional frame-handle API can preserve the adapter-owned copy without a second application copy. Direct borrowed provider-buffer views should be a later, explicitly leased API. Do not present all three modes as interchangeable.

### 5.4 Metadata must survive translation

`FrameHeader` contains much more than dimensions: nanosecond system/integration times, width/height, bits per pixel, band information, pixel format, field of view, contributing sensor, frame/subframe identifiers, image type and flip, flags, offsets/dither, inertial/navigation data and band index. FOV is documented in radians. The image is row-major, with a byte-alignment constraint on row bits. [S11]

For the first slice, support **host-accessible, single-band Mono8** explicitly. Reject other encodings/layouts as unsupported instead of interpreting them as Mono8. Preserve metadata in a documented subset and expose the limitations in a feature/coverage table. Validate multiplication overflow, image offset, accessible capacity and the actual image address before copying.

Do not assume a standard stride field exists where the examined header has none. An output stride can be part of the façade's own normalized copy format; its meaning must be documented as such.

### 5.5 The integration example needs more than an image callback

The existing IR example discovers both IRSTImage and CommandAndControl capabilities, supplies local buffer storage, and uses a C2 channel. Its smoke test transitions the provider into OPERATIONAL and supplies deterministic DIS traffic. It publishes observations only after receiving reference position data, and it emits service status. It does not perform frame-to-frame target association. [S25]

Therefore a useful demonstration should include the minimum explicit control path needed to run the selected provider. Do not make every image-stream start implicitly change the shared MFA's global mode; keep that operation deliberate and account for other clients.

## 6. RF MEL: model jobs and ownership, not just tuning

### 6.1 Interface families

The public factories create AdminMEL, C2MEL, DataMEL, MonitorMEL, COSITEMEL and DEAMEL. RF operation involves virtual apertures, element groups/datapipes, jobs, job intervals, endpoint objects, capabilities and rich metadata. [S14–S19]

A faithful binding should keep these concepts in the low-level C API. A later high-level continuous-receive helper can create and manage them on behalf of an application. Replacing them all with a provider-specific `tune/read` interface would unnecessarily couple the new library to one backend.

The example RF Skill continuously feeds job intervals rather than assuming one initial request grants indefinite receive operation. That behavior belongs in an explicitly documented stream helper, not in every low-level job call. [S26]

### 6.2 Callback counts and memory domains

The receive callback passes metadata, a `JobDataPointer` variant and a count of **objects/elements—not bytes**. Supported alternatives can include complex samples and structured packet objects. The metadata contains aperture/job/interval IDs, local function information, phase-coherence information, UTC time, receive events, pointing, optional event associations and stream IDs. [S17, S18]

The binding must switch on the actual alternative and map only supported data products. Never treat a C++ object buffer as an arbitrary serialized byte stream. For VITA-containing alternatives, serialized packet length and owning storage capacity also differ. [S17, S22]

`DataMEL` allows memory regions associated with host memory, GPUs and FPGAs when supported by the transport/provider. Consequently a non-null address is not enough to construct a safely dereferenceable Ada array or Rust slice. The initial binding must declare host-memory-only support. Device/RDMA memory needs a separate access contract. [S16]

### 6.3 Lifetime gaps to resolve before a safe wrapper

The inspected `setDataReadyCallback` takes a non-const reference to `std::function`; the declaration permits only one registered callback but does not itself establish a complete unregistration/drain protocol. Keep the callback object alive, and verify provider behavior during disable, endpoint destruction and concurrent completion. [S17]

Do not promise that destroying an Ada/Rust endpoint instantly prevents all callbacks unless that guarantee has been established. A callback can refer to an internal shared session state; shutdown must stop admission, quiesce producer activity and retain code/storage until no callback can still touch them.

### 6.4 Extension and time types are not trivial

`ProductRxMetadata` contains `std::any`. A universal, lossless translation of arbitrary `std::any` to C is impossible without knowledge of its concrete type. Define supported correlation/extension types deliberately, or return an explicit unsupported-extension result. Do not use RTTI names or raw object memory as a portable wire representation. [S18]

The math library's `UTCTime` stores integral seconds and fractional **femtoseconds**, whereas IR frame timing fields use nanoseconds. Preserve these distinctions. Converting every timestamp to a single nanosecond integer would lose precision for the RF type. Pin and document epoch/clock/leap-second interpretation separately; do not infer it solely from a C++ type name. [S11, S20]

RF `Frequency` is a double-valued Hertz alias. Preserve that semantic representation in the adapter rather than inheriting Squall's private integer-frequency protocol or imposing a new fixed-point resolution on every provider. [S19, S23]

### 6.5 VADB and deferred scope

Squall builds separate RF VADB libraries for simulated, RTL-SDR and HackRF profiles. These need a place in capability/definition-loading design even if not exercised by the first receive slice. [S24]

Mark TX waveforms, COSITE, DEA, device memory/RDMA and advanced local functions as deferred capability areas. An unimplemented binding feature is different from a feature a particular provider reports as unsupported.

## 7. Supporting libraries and build constraints

The Common and IR definitions are exposed through CMake INTERFACE targets. RF is similarly interface-oriented and expects local `ams-math` and `ams-vita` subdirectories. Common headers are nevertheless part of the include closure. These small CMake files are not a complete independently installable multilingual SDK. [S30–S32]

`ams-math` builds a static C++20 library and depends on Boost. When it is incorporated into a shared façade, configure position-independent code where required. Verify the actual transitive include/link closure with clean builds; do not treat the math code as just inline constants. [S21]

`ams-vita` is header-only C++20, generated from `spec/ams_vita_49-2_tailoring.yaml`. Its payload interface explicitly distinguishes network-order words and valid packet size from storage capacity. Keep it internal initially. A native C/Ada VITA generator can be a separate future project based on that specification, rather than an unplanned dependency of the first IR milestone. [S22]

The generic façade should not acquire Squall's gRPC, protobuf, logging and transport dependencies merely because Squall is the first test provider. Those are built into the provider implementation. [S24]

## 8. Proposed C ABI rules

These are design requirements for the new library, not assertions that upstream supplies them.

| Concern | Proposed policy |
|---|---|
| Language boundary | C11 public headers; C++20 implementation behind opaque typed handles |
| Ownership | Every returned resource has documented release/close behavior; child resources retain necessary provider owners |
| Public types | Fixed-width integers/discriminants, simple C records, counted arrays and strings; no STL or C++ object memory |
| Scalar semantics | Preserve source units/ranges/precision; explicit finite-value and checked conversion policy |
| Optional/variant data | Explicit presence/discriminant and typed accessors; reject unsupported alternatives |
| Diagnostics | Per-call status and caller-owned diagnostic storage or explicit error object; no sole process-global last-error string |
| Unknown values | Preserve raw code where meaningful; never cast arbitrary provider values into a closed Ada/Rust enum unchecked |
| Asynchrony | Request handles with pending/wait/collect states; timeout is not cancellation |
| Callback boundary | Provider callback terminates in C++; bounded queue and application-thread receive are the initial API |
| Unwinding | Catch all C++ exceptions at C entry points/callback trampolines; prevent Ada exceptions or Rust panics crossing ABI |
| ABI evolution | Versioned records/functions and explicit size negotiation; never change an existing public record layout casually |
| DSO lifetime | Unload only after all objects, deleters, pending requests and callbacks that need it have quiesced |
| Supported platforms | Advertise only tested target/toolchain/provider combinations |

### 8.1 Resource graph and closure

A provider session must remain alive while any channel/endpoint, pending request, callback state or provider-owned frame needs it. Destruction order must release provider objects **before** unloading their shared object.

Initially prefer explicit close returning a busy/outstanding-resources result when safe completion is not possible. Non-throwing language finalizers remain a fallback; they cannot manufacture provider cancellation or prove that foreign callbacks have stopped. Leaks or deferred cleanup should be diagnosable rather than hidden behind premature `dlclose`.

### 8.2 Error behavior

Specify output initialization on failure and exception translation at every exported entry point. `noexcept` alone is insufficient: an escaping exception from a noexcept function terminates rather than returning a C error. Include allocation-failure paths that do not themselves allocate diagnostic strings.

Do not claim arbitrary bogus opaque pointers are safely detectable. Either define a valid-handle caller contract and test supported misuse cases, or deliberately implement a validated token registry with generation counters. That choice affects ABI and overhead and belongs in an architecture decision record.

### 8.3 Data layout

For simple descriptors, specify field widths, array order, optional presence, string encoding/lifetime and size/alignment requirements. A C ABI is not a cross-platform serialized format: it is compatible within a supported platform ABI.

For owning containers, complex samples and time types, perform explicit value translation. Only use a zero-copy layout alias when layout, alignment, representation and aliasing assumptions have all been verified. In particular, C++ `MELComplex<T>`, C `_Complex`, Rust complex types and Ada records are not interchangeable merely because they all represent I/Q values.

## 9. Ada development in parallel

### 9.1 Package structure

Provisional public package arrangement:

```text
AMS.MEL
AMS.MEL.Types
AMS.MEL.IR
AMS.MEL.RF

private implementation hierarchy:
AMS.MEL.Internal
AMS.MEL.Internal.C_API
```

Prefer Ada-native identifiers, units, enumerations, arrays and limited resource owners in the public packages. Keep C representations, raw addresses, imported routines and foreign status codes in private interop packages. Public APIs should expose domain operations rather than a mechanical translation of every C++ setter.

Use limited private/limited controlled types for unique resource ownership and explicit Close/Release operations. Finalize must not raise. Copying value metadata should remain easy; copying a live provider/endpoint owner should be deliberate, not implicit.

### 9.2 How parallel implementation should work

For each vertical slice:

1. Identify the upstream declaration, responsibility, ownership and required/conditional context.
2. Sketch the Ada client usage before freezing the C entry point.
3. Implement the C++ adapter, actual C consumer test and Ada thin import.
4. Add the idiomatic Ada operation and negative/lifecycle tests.
5. Update the coverage matrix and ABI compatibility tests.

This discovers awkward ownership, array and error conventions early. It does not require implementing a whole Ada SDK before native loading works.

### 9.3 SPARK boundary

SPARK is appropriate for bounded value transformations, unit/range validation, metadata checks and selected state-machine logic. FFI, provider memory, OS services, native callback concurrency and much of the resource-management implementation form a trusted boundary unless separately modeled and justified.

A `SPARK_Mode => Off` interop body is not proof that the external provider respects its contract. Do not advertise a wholly verified MEL implementation merely because selected Ada packages prove. Keep the proof claims precise and test the foreign assumptions independently.

Start with the full hosted GNAT runtime, not an assumed Ravenscar/no-heap deployment. Add constrained-runtime profiles only after characterizing what the provider and adapter actually allocate, block on and call.

## 10. Packaging for Alire first, crates.io later

### 10.1 Proposed component split

Start with two independently packaged components, possibly in one development monorepo:

```text
ams-mel/
  native/                  proposed Alire crate: ams_mel_c
    include/               C-only installed headers
    src/                   C++ implementation
    vendor/                pinned upstream source closure / provenance
    CMakeLists.txt
    alire.toml
    gpr/                   native-library integration project
    tests/                 real C clients and fake C++ providers
  ada/                     proposed Alire crate: ams_mel
    src/
    alire.toml
    ams_mel.gpr
    tests/                 separate AUnit/proof/coverage crate
  docs/
  integration/             Squall and OMS test scenarios, not library deps
```

Names are provisional and registry availability is unverified. Both IR and RF can be separate internal modules without creating many independently versioned packages immediately. If public Ada components are split later, only one crate should own each root Ada package unit.

### 10.2 Build ownership

My default would be one authoritative native CMake build, with the Alire support crate invoking it through a declared, repeatable build action and exposing the result to GPR. Ada imports that native library; it must not compile a second copy of the native sources.

Pin the selected C++ compiler, flags, dependency paths and native library kind through the build configuration, not hard-coded host paths. Confirm how Boost and build tools are supplied on each supported platform before publication. If GPR is ultimately chosen as the native build driver instead, retain the same single-build-owner property.

Release archives must contain their source closure or resolve declared package dependencies. Avoid installation-time GitLab logins, network-fetching build hooks, missing submodules and undeclared sibling checkouts. Retain upstream licenses/notices and provenance, plus your patches if any. New glue can use a compatible license, but source redistribution review is still required.

### 10.3 Alire policy points

Alire supports local dependency pins for development and separate test crates. Publication needs an immutable public origin and required metadata; unsupported CI targets can be excluded with `available`. Treat development pins as overrides, not the published dependency solution. Test extracted release sources independently of the original workspace. [P01]

The initial platform claim should be Linux x86-64 plus specifically tested toolchains/providers, not “all platforms supported by Ada.” Add other targets after a clean build, loader test and lifecycle test have passed there.

### 10.4 Rust later

The normal public Rust destination is **crates.io**, published with Cargo, rather than a manual pull request to a Cargo index. `cargo publish --dry-run` and `cargo package --list` belong in release validation. [P02]

Use an unsafe `ams-mel-sys` crate to own native linkage and a separate safe `ams-mel` crate. The sys crate should declare the native `links` identity; Cargo allows one package for a given links value in a dependency graph. [P03]

The published sys crate must contain the native sources it builds, or have a deliberate documented installed-library mode. A local `../../native` path is not a complete crates.io release. Release automation can stage the same canonical native source snapshot into the sys crate package; that is packaging, not a second independently maintained bridge.

Only implement Rust Send/Sync and borrowed frame views after proving the corresponding native threading/lifetime guarantees. Neither `Arc` nor a shared native handle is automatic evidence of thread safety.

## 11. Suggested milestones and acceptance gates

The release gate M6 can be run immediately after M3 for an explicitly IR-only first release. M4 (complete example Skill) and M5 (RF support) are later expansions, not prerequisites for publishing the first useful MEL library. The numbers identify work packages, not a requirement to finish every domain before release.

### M0 — Inventory and compatibility contract

Freeze upstream references and create a declaration-level coverage matrix. Record full declaration path, responsibility (provider/service), Required/RequiredIf/Optional condition, binding exposure, supported provider behavior, units, ownership, thread assumptions and test evidence.

Acceptance: source archive rebuilds its native dependency baseline in a clean environment; no provider/hardware is needed. Known upstream ambiguities are tracked, and no native ABI version is promised stable prematurely.

### M1 — Common C ABI and Ada bring-up against a mock provider

Implement ABI/version query, diagnostics, genuine C exports, dynamic provider loading, provider-owned state, one capability/value query and explicit close. The mock must be a separate loadable C++ provider, not merely a stub behind the C functions.

Acceptance: a C-compiled client and an Ada client both use the library; tests cover success, missing symbol, incompatible declared profile, factory failure, clean unload and outstanding-owner closure. No Squall, WebSockets, OpenCV or hardware dependency.

### M2 — IR host-memory receive

Implement the service-side Buffer/ImageListener, attach/discover, registration before enable, bounded queue, Mono8 receive, metadata snapshot and release, disable/detach/shutdown. Include the minimal explicit mode/control operation needed by the chosen scenario.

Acceptance: C and Ada tests receive repeated frames and release them exactly once; malformed input and queue overflow are observable; shutdown cannot free memory while the fake provider can still access it. Document unsupported metadata/format capabilities.

### M3 — Real-provider integration

Load the chosen Squall IR provider through the same C++ MEL boundary. Use a small fixture rather than requiring the full visualization environment. Verify capability discovery and configuration correspondence. Keep the fake provider suite mandatory.

Acceptance: C and Ada consumers use the same library/provider path and produce consistent metadata/content for a deterministic fixture. Builds do not reach into Squall's private backend APIs.

### M4 — A complete example Skill via separate OMS/UCI support

Add explicit schema/service IDs, topic authorization, PositionReport subscription and ServiceStatus/observation publication. Keep protocol/client concerns out of the MEL library.

Acceptance: an independent observer or test fixture validates serialized UCI messages and their expected service/topic settings. This is integration evidence, not a substitute for the complete OMS/AMS conformance assessment. [S25–S29]

### M5 — RF receive subset

Add required factories, metadata and request handling, virtual-aperture/job operations, explicit interval management and ComplexINT16 delivery. Investigate VADB/definition loading rather than assuming one hard-coded aperture exists everywhere.

Acceptance: correct element counts, no unsupported raw-layout casts, bounded callback handling, finite documented wait behavior, clean teardown, continuous receive test and coherent metadata translation. TX/RDMA/COSITE/DEA remain declared exclusions until implemented.

### M6 — Alire release candidate

Exercise an extracted archive and clean dependency resolution, required metadata, supported target restrictions, a public provider-free build, separate test dependencies, release provenance, ABI tests and consumer examples.

Acceptance: a new user can retrieve/build the published library without your local checkouts, cached provider binaries, GitLab credentials or a running simulation. Real-provider integration is a separate opt-in job.

### M7 — Rust sys and safe API

Package the already-tested C ABI and add ownership-safe wrappers. No new provider transport implementation. Keep the C/Ada ABI tests as compatibility anchors.

## 12. Test matrix to establish before publication

| Category | Minimum coverage |
|---|---|
| C boundary | Compile with a C compiler; headers self-contained; exported symbols; no exceptions crossing |
| Representation | C/Ada sizes, alignment, offsets, enum code mapping, time precision and numeric boundaries |
| Requests | Pending, completion, provider error, timeout without cancellation, repeated collection, teardown with pending work |
| Loader | Missing library/symbol, factory failure, supported profile mismatch, live-child unload prevention |
| Buffers | Image offset vs base address, capacity checks, unsupported layout, repeated release, release after failure |
| Concurrency | Callback in flight during stop, queue overflow, consumer stalled, producer disconnect |
| Capabilities | Binding-not-implemented vs provider-unsupported vs rejected invalid request |
| Semantics | UUID and labels, timestamp parts, FOV units, metadata associations, element count vs byte count |
| Resource cleanup | All partially constructed states, error/exception paths, late callbacks and outstanding frame copies |
| Real provider | Squall example path plus a strict fake provider with deliberately different behavior |
| Distribution | Extracted archive build, no sibling paths/network build hooks, no test tools in runtime dependency graph |

The LA-CAL harness evaluates server protocol behavior. It does not test C ABI memory safety, the MEL provider or all requirements of a deployed Skill. [S29]

## 13. Outstanding questions and non-claims

Before calling the ABI stable, resolve provider callback quiescence, asynchronous cancellation/deferred cleanup, allocator/ABI compatibility, extension-type mapping and time/identity semantics. Before claiming standards compliance, review the relevant normative IDDs and architecture requirements, not just header comments and examples.

The reference Starter Kit explicitly is not yet a fully compliant deployment. Its RF Skill self-assessment records missing MBSE artifacts, DAST and measured structural coverage, among other qualifications. Treat those documents as useful input, not certification evidence or permission to assume every requirement has already been solved. [S03, S27, S33]

## 14. Recommended immediate start

**Begin with M0 and M1: source baseline, C ABI design rules, a real fake-provider shared library, and C plus Ada consumers exercising the same owner lifecycle.** Then add IR receive as the first complete vertical slice.

Do not begin by translating every header or porting an entire detector. The difficult, reusable work is the ownership/async/provider contract and repeatable packaging. Solving that once makes the Ada work productive immediately and leaves a viable foundation for Rust later.

## Sources

Source URLs below identify the inspected default-branch material. Use the JSON candidate revisions to create immutable source references before implementation; do not assume these moving URLs form a release lock.

- **S01 — Umbrella submodule inventory:** `https://github.com/open-arsenal/ams-gra-hello-world-sk/tree/main`
- **S02 — Release manifest:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-getting-started/blob/main/manifest.yaml`
- **S03 — Starter Kit architecture and limitations:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-getting-started/blob/main/README.md`
- **S04 — Common MEL requests and versions:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel/blob/main/include/mel/library/CommonMEL.h`
- **S05 — Common ErrorOr:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel/blob/main/include/mel/library/ErrorOr.h`
- **S06 — Common UCI_ID:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel/blob/main/include/mel/library/UCI_ID.h`
- **S07 — IR Control and factory:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/Control.h`
- **S08 — IR Buffer ownership:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/Buffer.h`
- **S09 — IR ImageListener:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/ImageListener.h`
- **S10 — IR Channel:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/Channel.h`
- **S11 — IR FrameHeader:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/FrameHeader.h`
- **S12 — IR Config:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/irmel-types/Config.h`
- **S13 — IR factory channel types:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/include/irmel/library/factory/IRCreateFunctions.h`
- **S14 — RF factories:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/factory/RFCreateFunctions.h`
- **S15 — RF C2MEL:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/c2/C2MEL.h`
- **S16 — RF DataMEL:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/data/DataMEL.h`
- **S17 — RF ProductRxEndpoint:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/data/ProductRxEndpoint.h`
- **S18 — RF ProductRxMetadata:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/rfmeltypes/ProductRxMetadata.h`
- **S19 — RF elementary and composite types:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/include/rfmel/rfmeltypes/RFMELTypes.h`
- **S20 — Math UTC time:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-math/blob/main/include/math/units/UTCTime.h`
- **S21 — Math build:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-math/blob/main/CMakeLists.txt`
- **S22 — VITA library and generation:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita/blob/main/README.md`
- **S23 — Squall public boundary:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-sensors-squall/blob/main/README.md`
- **S24 — Squall RF build and VADB:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-sensors-squall/blob/main/interfaces/squall-rf-mel-impl/CMakeLists.txt`
- **S25 — IR example architecture:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-skills-ir-search-and-track/blob/main/docs/architecture.md`
- **S26 — RF example architecture:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-skills-rf-fm-demod/blob/main/docs/architecture.md`
- **S27 — RF example self-assessment:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-skills-rf-fm-demod/blob/main/docs/compliance/ams-mpu-compliance.md`
- **S28 — Sleet server and authorization:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-infra-sleet/blob/main/README.md`
- **S29 — LA-CAL server harness:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-test-la-cal-harness/blob/main/README.md`
- **S30 — IR build:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel/blob/main/CMakeLists.txt`
- **S31 — RF build:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel/blob/main/CMakeLists.txt`
- **S32 — Common MEL build:** `https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel/blob/main/CMakeLists.txt`
- **S33 — AMS GRA normative document inventory:** `https://github.com/open-arsenal/ams-gra/tree/main`

### Package documentation

- **P01 — Alire documentation:** `https://alire.ada.dev/docs/`
- **P02 — Cargo publishing:** `https://doc.rust-lang.org/cargo/reference/publishing.html`
- **P03 — Cargo build scripts and links:** `https://doc.rust-lang.org/cargo/reference/build-scripts.html`

The package-policy summaries are intentionally limited; recheck the relevant version's tooling and package metadata during release preparation.
