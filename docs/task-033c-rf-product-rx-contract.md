# Task 033C RF ProductRxEndpoint receive declaration closure and callback/lifetime contract

Task 033C is an evidence task. It pins the declaration closure of the published
RF MEL receive interface (`ProductRxEndpoint` + `ProductRxMetadata`), proves it
compiles with GCC and Clang from repository-owned upstream sources only, and
records the data model and callback/buffer/lifetime assumptions that Task 033D
must design around. It adds **no** production receive code.

Starting repository SHA (reviewed `main`, after 033B / PR #56 merged):
`12cfef4407df06b5e1b69afb44969d53a319ea8d`.

## Scope and ABI

* No change to `native/include/ams_mel/abi.h`, `native/src/exports.map`,
  `native/src/*.cpp`, `ada/`, `rust/`, or `python/`.
* ABI stays **0.1** with exactly **106** production exports, identical by symbol
  name to the starting SHA.
* No ProductRxEndpoint C API, no mock endpoint, no runtime test.
* Added: 16 vendored RF MEL headers, one declaration-only compile probe, one
  distinct closure checker and its build target, documentation.

## Upstream pins (reused exactly from Task 033A)

| Source | Commit | Tree | Use in 033C |
|---|---|---|---|
| RF MEL `open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` | `f5b9d4a8601cae5639785bad7d3d2f1f8b3d9ae8` | 16 additional headers vendored |
| AMS VITA | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` | `cc5287c816e82b8166923945e322a4915feaf10c` | reused; 0 new files |
| Common MEL | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` | reused; 0 new files |
| AMS Math | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` | reused; 0 new files |
| Boost 1.83.0 (archive SHA-256 `6478edfe…3b8e`, source commit `564e2ac16907019696cdaba8a93e3588ec596062`) | — | — | reused; 0 new files |
| Squall (evidence only) | `b1015728f904c799fa0c07489fce48e78f67845f` | `588f4999916db86c0278e3785a16e4dce60ecbdf` | not vendored |

All five git checkouts outside the repository reported exactly these commit and
tree ids with a clean worktree; the Boost archive re-hashed to the recorded
SHA-256. No new version was selected: the measured closure needs no other
immutable dependency.

## Receive root and measurement

One translation unit, measured outside the repository against the **full**
upstream trees (not the vendor directories), `-std=c++20 -M`:

```cpp
#include <rfmel/data/DataMEL.h>
#include <rfmel/data/ProductRxEndpoint.h>   // includes ProductRxMetadata.h
```

No RF C2/job header is rooted artificially; `jobs/*` enter only transitively
(`ProductRxEndpoint.h -> jobs/JobInterval.h`, `ProductRxMetadata.h ->
jobs/JobEvent.h, jobs/Pointing.h`).

| Compiler | Version | Project-owned upstream files |
|---|---|---|
| GCC | `g++ (Debian 14.2.0-19) 14.2.0` | **537** (RF 25, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 479) |
| Clang | `Debian clang version 19.1.7 (3+b1)` (`clang++-19`) | **538** (RF 25, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 480) |
| Union | — | **539** (RF 25, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 481) |

GCC-only: `boost/config/compiler/gcc.hpp`. Clang-only:
`boost/config/compiler/clang.hpp`, `boost/config/compiler/clang_version.hpp`
(unchanged from 033A). Nothing else resolved outside the five upstream trees
except the compiler's standard library.

### Delta from the Task 033A closure (524 union)

| Family | 033A union | 033C union | Added | Removed |
|---|---|---|---|---|
| RF MEL | 10 | 25 | **16** | 1 (`rfmel/factory/RFCreateFunctions.h`, not rooted here) |
| Common MEL | 23 | 23 | 0 | 0 |
| AMS Math | 5 | 5 | 0 | 0 |
| AMS VITA | 5 | 5 | 0 | 0 |
| Boost | 481 | 481 | 0 | 0 |
| **Total** | 524 | 539 | **+16** | −1 |

The same per-family delta holds separately for GCC (522 → 537) and Clang
(523 → 538). Re-measurement confirms the 033A boundary note exactly: 16 RF,
0 Boost, and also 0 Common MEL, 0 AMS Math, 0 AMS VITA. `RFCreateFunctions.h`
drops out only because 033C does not root it; it stays vendored and in the 033A
check.

`rfmel/endpoints/RDMAMemoryRegionParams.h` is **not** in the closure.
`ProductRxEndpoint.h` only forward-declares `RDMAMemoryRegionParams` (via
`RFMELTypes.h`), which suffices for the pure-virtual by-value return. 033D must
not call `getRDMAMemoryRegionParams()`, so the header is not needed.

## Newly vendored files (16, all RF MEL)

```text
native/vendor/rf-mel/include/rfmel/data/ProductRxEndpoint.h
native/vendor/rf-mel/include/rfmel/endpoints/BaseEndpoint.h
native/vendor/rf-mel/include/rfmel/jobs/CachedWaveform.h
native/vendor/rf-mel/include/rfmel/jobs/CommonModulations.h
native/vendor/rf-mel/include/rfmel/jobs/DataPipe.h
native/vendor/rf-mel/include/rfmel/jobs/ElementGroupToEndpointConnections.h
native/vendor/rf-mel/include/rfmel/jobs/JobEvent.h
native/vendor/rf-mel/include/rfmel/jobs/JobInterval.h
native/vendor/rf-mel/include/rfmel/jobs/LocalFunctionCommand.h
native/vendor/rf-mel/include/rfmel/jobs/ModulationExtensionBase.h
native/vendor/rf-mel/include/rfmel/jobs/Pointing.h
native/vendor/rf-mel/include/rfmel/jobs/ProductStreamParams.h
native/vendor/rf-mel/include/rfmel/jobs/PulseDetectionSettings.h
native/vendor/rf-mel/include/rfmel/jobs/WaveformStream.h
native/vendor/rf-mel/include/rfmel/jobs/Weights.h
native/vendor/rf-mel/include/rfmel/rfmeltypes/ProductRxMetadata.h
```

Each file was written with `git cat-file blob 762ce84…:<path>` and re-hashed
with `git hash-object`. All 16 equal the pinned tree's blob ids, and `cmp`
against the checkout reports no difference. No file already present from 033A
was duplicated. The full 539-file union (vendored copies) was also compared
with `cmp` against the pinned upstream trees and the verified Boost archive
extraction: **539 compared, 0 differences**. `.gitattributes` already marks
`native/vendor/rf-mel/**` as `-whitespace`, so no attribute change is needed.

`docs/upstream-files.sha256.md` gains a Task 033C block with exactly these 16
entries. No existing line changed: the previous file is a byte-identical prefix.
All **627** recorded checksums verify (611 + 16).

## Declaration probe and closure checker

* `native/tests/rf_product_rx_header_compile_probe.cpp` includes only the two
  roots. It contains `static_assert`s/concepts over the published declarations
  and one trivial function. It instantiates no provider object, derives no
  endpoint, and registers no callback. It asserts, among others:
  `ProductRxEndpoint : BaseEndpoint`, abstract, non-copyable/non-movable;
  `EndpointID == uint64_t`; the exact member-pointer types of
  `getEndpointID`, `getAssignedDataFormat`, `getRDMAMemoryRegionParams`,
  `setDataReadyCallback`, and `DataMEL::createProductRxEndpoint`;
  `RequestFor<ProductRxEndpoint> == std::future<ErrorOr<shared_ptr<…>>>`; the
  callback accepts only a non-const lvalue `std::function`; the **absence** of
  `removeDataReadyCallback`, `clearDataReadyCallback`,
  `unregisterDataReadyCallback`, `stopCallbacks`, `stop`, `close`, `shutdown`
  on the endpoint; all eight `JobDataPointer` alternatives and their
  index-to-`JobDataFormat` mapping; the `MELComplex<int16_t>` layout traits; the
  return type of every `ProductRxMetadata` getter; UTCTime representation;
  `PointingType` alternatives and their field types; and the `ReceiveEvent`
  getters.
* CMake target `rf_product_rx_header_compile_probe` (OBJECT, test builds only):
  `cxx_std_20`, `CXX_EXTENSIONS OFF`, `-Wall -Wextra -Wpedantic -Werror`, the
  same five `SYSTEM` vendor roots as 033A (reusing
  `ams_mel_rf_data_vendor_roots`).
* `native/scripts/check_rf_product_rx_closure.py`, run by the always-built
  `check_rf_product_rx_header_closure` target, imports the **unchanged** 033A
  path rule (`check_rf_data_closure.classify/dependencies/self_test`) and adds
  stricter receive checks. The observed RF header set must **equal** the
  measured 25-file set, and the Common MEL/AMS Math/AMS VITA counts must equal
  23/5/5. Boost is checked by the path rule only, because its count is
  compiler-dependent. A built-in self-test rejects receive-specific rogue
  locations (`/usr/include/rfmel/data/ProductRxEndpoint.h`,
  `/usr/local/include/rfmel/rfmeltypes/ProductRxMetadata.h`, `/opt/rfmel/...`,
  RF headers placed under `ir-mel/` or `ams-math/` inside the vendor tree).
* The 033A `rf_data_header_compile_probe` and `check_rf_data_header_closure`
  targets, and `check_rf_data_closure.py`, are unchanged.
* Only `CMAKE_CURRENT_SOURCE_DIR` paths and the configured compiler are used,
  so the target works in `native/build-tests`, the CI `build/native` tree, and
  any out-of-tree directory.

### Closure validation

| Check | GCC 14.2.0 | Clang 19.1.7 |
|---|---|---|
| Probe builds with `-Werror` through CMake (out-of-tree Release) | PASS | PASS |
| `check_rf_product_rx_header_closure` | PASS: 537 (rfmel 25, mel 23, math 5, boost 479, vita 5) | PASS: 538 (rfmel 25, mel 23, math 5, boost 480, vita 5) |
| Existing `check_rf_data_header_closure` (033A) | PASS: 522 (unchanged) | PASS: 523 (unchanged) |
| Existing `check_track_header_boost_closure` | PASS: 481 (unchanged) | PASS: 482 (unchanged) |
| Rogue `-isystem` root searched **before** the vendor roots, containing `rfmel/jobs/Pointing.h`, `rfmel/rfmeltypes/ProductRxMetadata.h`, `boost/numeric/ublas/blas.hpp` | Rejected, all 3 leaks named | Rejected, all 3 leaks named |
| Closure growth: an extra `RDMAMemoryRegionParams.h` in a scratch vendor copy, rooted by a scratch probe | Rejected: `unexpected ['rfmel/endpoints/RDMAMemoryRegionParams.h']` | — |
| Closure shrinkage: scratch probe rooting only `DataMEL.h` | Rejected: 16 receive headers named missing | — |

The in-repository counts equal the full-upstream-tree measurement exactly.
System-dependency leakage: **none**. No RF, Common MEL, AMS Math, AMS VITA, or
Boost header resolves outside `native/vendor`. Nothing is fetched at configure,
build, or test time.

## ProductRxEndpoint method inventory (pinned)

`class ProductRxEndpoint : public BaseEndpoint` (abstract; copy/move deleted;
virtual destructor defaulted; protected default constructor; a public
`explicit ProductRxEndpoint(JobDataFormat)` is declared but has no definition in
the published headers).

| Method | Signature | Tag | 033D |
|---|---|---|---|
| `getEndpointID` | `[[nodiscard]] EndpointID getEndpointID() const` (`EndpointID = uint64_t`, from `BaseEndpoint`) | Required | expose |
| `getAssignedDataFormat` | `[[nodiscard]] virtual JobDataFormat getAssignedDataFormat() const` | RequiredIfReceive | validate == requested |
| `getRDMAMemoryRegionParams` | `[[nodiscard]] virtual RDMAMemoryRegionParams getRDMAMemoryRegionParams() const` | RequiredIfRDMA | do **not** call |
| `setDataReadyCallback` | `virtual void setDataReadyCallback(std::function<void(std::shared_ptr<ProductRxMetadata>, JobDataPointer, size_t)>& cb)` | RequiredIfReceive | call exactly once |

### Exact callback type

```cpp
std::function<void(
    std::shared_ptr<ams::iface::rfmel::ProductRxMetadata>,
    ams::iface::rfmel::JobDataPointer,
    size_t)>
```

It is registered through `ProductRxEndpoint::setDataReadyCallback(...)`, taking
a **non-const lvalue reference** (the probe proves that rvalues and const lvalues
are rejected). The bridge must keep a named `std::function` lvalue and must not
assume whether the provider copies, moves from, or references it. Squall copies
it (`user_callback_ = cb`). The published comment says:

* "Only one callback may be registered to an Endpoint." Re-registration
  semantics are unspecified, so 033D registers exactly once.
* The third argument is "the number of actual objects/elements in the buffer.
  NOT THE NUMBER OF BYTES!"
* `JobDataPointer` "refers to the starting address of a contiguous buffer of
  these objects/elements in memory". It is a tagged `std::variant` of raw,
  non-owning pointers.

### NO UNREGISTER OPERATION

> **The pinned `ProductRxEndpoint` exposes `setDataReadyCallback` and NO
> `removeDataReadyCallback`, `clearDataReadyCallback`,
> `unregisterDataReadyCallback`, `stopCallbacks`, `stop`, or `close`.**
> `BaseEndpoint` adds only `getEndpointID`. `shutdown()` exists only on the
> parent `RFMEL`/`DataMEL`. The compile probe asserts every one of these
> absences.

This is a first-class lifecycle constraint. Once registered, the callback stays
installed for the provider endpoint's whole lifetime, and the only way to stop
delivery through the published interface is to destroy the endpoint (or shut
down the parent). 033D must not infer an unregister mechanism, and it must not
re-register an empty `std::function` as a pseudo-unregister, because
re-registration is not specified.

Destroying the endpoint ends the bridge's **ownership** of it. It does not
prove that the provider can never start another invocation of a callback it
has copied. There is no unregister and no quiescence primitive, so generic RF
MEL gives no proof that provider **code** is finished with a registered
callback. That is why 033D must permanently pin the provider DSO after callback
registration (see "Recommended 033D receive design" → "DSO lifetime").

## JobDataPointer inventory

`JobDataFormat` has 14 enumerators. `JobDataPointer` is
`std::variant<…8 raw pointers…>`, and its alternative index equals the first
eight enumerator values. The probe asserts this correspondence. `PDWType1..3`
(8–10) and `LFType1..3` (11–13) have **no** alternative, so a provider cannot
deliver them through this callback at this pin.

| `JobDataFormat` (value) | Variant alternative (index) | Element size | Count means | Candidate C representation | 033D |
|---|---|---|---|---|---|
| `DirectINT8` (0) | `int8_t*` (0) | 1 | int8 samples | `int8_t[]` | unsupported (fail closed) |
| `DirectINT16` (1) | `int16_t*` (1) | 2 | int16 samples | `int16_t[]` | unsupported |
| `ComplexINT8` (2) | `MELComplex<int8_t>*` (2) | 2 | I/Q pairs | `{int8_t real, imag}[]` | unsupported |
| **`ComplexINT16` (3)** | **`MELComplex<int16_t>*` (3)** | **4** | **I/Q pairs** | **`ams_mel_rf_complex_i16_v1[]`** | **supported** |
| `AMSVitaSmall` (4) | `DataPacketSmall*` (4) | 512 | packet storage slots | byte packets + VITA view | unsupported |
| `AMSVitaMedium` (5) | `DataPacketMedium*` (5) | 1024 | packet storage slots | byte packets + VITA view | unsupported |
| `AMSVitaLarge` (6) | `DataPacketLarge*` (6) | 8192 | packet storage slots | byte packets + VITA view | unsupported |
| `AMSVitaExtraLarge` (7) | `DataPacketExtraLarge*` (7) | 32768 | packet storage slots | byte packets + VITA view | unsupported |
| `PDWType1..3`, `LFType1..3` (8–13) | none | — | — | — | not deliverable at this pin |

For VITA, the element is fixed-capacity packet **storage**. The valid serialized
length is header-derived (`DataPacketView::getPacketSize()`), not the element
size. That, plus `rxEventIDAssociations`, is why VITA is a separate future
design.

**Initial receive target: `JobDataFormat::ComplexINT16`.** The evidence
supports it: it is the only format Squall accepts (`createProductRxEndpoint`
returns `InvalidParameters` otherwise), the only format in Squall's
`getSupportedDataFormats()` (033B smoke: `formats={3}`), and a fixed-size,
header-free element. Direct/ComplexINT8/VITA receive is not designed here.

## ComplexINT16 layout and copy model

Compile-time evidence, identical under GCC 14.2.0 and Clang 19.1.7, asserted
in the probe:

| Property of `MELComplex<int16_t>` | Result |
|---|---|
| `sizeof` / `alignof` | 4 / 4 (`alignas(sizeof(T)+sizeof(U))`) |
| `real()` / `imag()` return type | `int16_t` / `int16_t` (by value) |
| `is_standard_layout` | true |
| `is_trivially_destructible` | true |
| `is_nothrow_copy_constructible` | true |
| `is_trivially_copyable` | **false** (user-provided move ctor / move assignment) |
| `is_trivial` | false |

Members are private (`T mReal; U mImag;`). The upstream comment requires them
to be "fully interleaved … like the C-compatible std::complex". However,
because the type is **not trivially copyable**, byte-wise `memcpy` of
`MELComplex` objects, or reinterpreting the provider buffer as a C
`{int16_t, int16_t}` array, is **not** sanctioned by the language. Squall's
4-byte UDP decoding does not change that. Standard layout alone does not make
the bytes a public contract.

**Conclusion:** copying is safe **element-by-element through the public
accessors**. 033D must fill each bridge-owned C element as `out[i].real =
p[i].real(); out[i].imag = p[i].imag();` for `i < count`. It must not expose
`MELComplex<T>` or `std::complex`, and must not claim binary layout
equivalence. This relies on nothing undocumented, so it is not a stop condition.

## Provider buffer lifetime

The callback supplies only a raw pointer (inside `JobDataPointer`) plus an
element count. **The interface does not promise that the storage survives
callback return**, and it declares no ownership, release, or lease operation. The
future bridge therefore treats the `JobDataPointer` buffer as
**CALLBACK-SCOPED**. It copies before returning and never retains the pointer.
No borrowed or zero-copy public lifetime may be claimed for RF receive.

### Squall evidence: provider-owned, reused buffer

Pinned `SquallProductRxEndpoint::UdpCallback(const uint8_t* data, size_t size)`
(`interfaces/squall-rf-mel-impl/src/SquallDataMEL.cc:61-95`):

```text
UDP datagram bytes (UdpDataReceiver's 65535-byte recv vector, also reused)
  -> drop if !context_->isJobActive()                 (no job => no callback)
  -> copy user_callback_ under callback_mutex_; return if empty
  -> drop (spdlog) unless data_type_ == ComplexINT16
  -> drop (spdlog) if size == 0 or size % sizeof(MELComplex<int16_t>) != 0
  -> num_samples = size / 4
  -> iq_buffer_.resize(num_samples)                   (endpoint MEMBER vector)
  -> for each i: iq_buffer_[i].real(LE int16 @ 4i); iq_buffer_[i].imag(LE int16 @ 4i+2)
  -> metadata = std::make_shared<ProductRxMetadata>() (all defaults)
  -> cb(metadata, JobDataPointer{iq_buffer_.data()}, num_samples)
```

`iq_buffer_` is a `std::vector<MELComplex<int16_t>>` member of the endpoint. It
is **reused for every later datagram** and may be reallocated by `resize`. A
pointer retained after callback return would alias the next datagram's samples
or dangle. So **033D must copy ComplexINT16 samples into bridge-owned storage
before the callback returns.**

### Squall sample decoding (provider evidence, not a universal contract)

Each element is decoded from little-endian `int16` real followed by
little-endian `int16` imaginary (`decodeLittleEndianI16`, two's complement).
The pinned Squall test (`tests/test_rf_mel.cc`, `test_datamel_reception_success`)
sends `0a 00 14 00 1e 00 28 00`, which yields `10+20i`, `30+40i`. It then sends
`fb ff 06 00`, which yields `-5+6i`. A 3-byte payload produces no callback. This
is Squall's UDP transport format, **not** an RF MEL wire-format contract for
other providers. 033D consumes the `MELComplex` objects the provider delivers
and never re-decodes provider transport bytes. The same test values make
convenient mock/real-Squall expectations.

## ProductRxMetadata complete inventory

`ProductRxMetadata` is a concrete, copyable value class. Every field is private
and reachable only through a getter that returns **by value**, so each call
copies. For vectors, a future bridge calls each getter once and walks the local
copy. The header documents that "these are fields for what already happened …
the absolute truth of the collection". Every field has a default initializer,
and the class has no "set/unset" flags. So apart from the explicit `std::optional`,
no field distinguishes "provider did not populate" from "provider populated with
the default value".

| # | Getter / field | C++ type | Shape | Units / meaning | Optional semantics | Candidate owned C representation | First-runtime (033D) |
|---|---|---|---|---|---|---|---|
| 1 | `getMelProtocolVersionID` / `melProtocolVersionID` | `uint32_t` (`auto`) | scalar | API version of the constructing MEL (should equal `VersionInfo::getAPIVersion`) | always present; default 0 | `uint32_t` | **copy** |
| 2 | `getVaDefinitionID` / `vaDefinitionID` | `VirtualApertureDefinitionID = uint32_t` | scalar | VA identity | present; default 0 | `uint32_t` | **copy** |
| 3 | `getVaInstanceID` / `vaInstanceID` | `VirtualApertureInstanceID = uint32_t` | scalar | VAI identity | present; default 0 | `uint32_t` | **copy** |
| 4 | `getJobDetailsID` / `jobDetailsID` | `uint32_t` | scalar | JobDetail containing the interval | present; default 0 | `uint32_t` | **copy** |
| 5 | `getJobIntervalID` / `jobIntervalID` | `uint32_t` | scalar | interval that produced the data | present; default 0 | `uint32_t` | **copy** |
| 6 | `getLfTypeID` / `lfTypeID` | `LocalFunctionTypeID = uint32_t` | scalar | LF type (with instance) | present; default 0 | `uint32_t` | **copy** |
| 7 | `getLfInstanceID` / `lfInstanceID` | `uint32_t` | scalar | LF instance index | present; default 0 | `uint32_t` | **copy** |
| 8 | `getUserDefinedData` / `userDefinedData` | `std::any` | type-erased | implementation-defined | empty `std::any` by default | **none generic** (see policy) | **require empty; else malformed** |
| 9 | `getPhaseCoherenceWithPrior` / `phaseCoherenceWithPrior` | `bool` | scalar | phase coherence with previous interval of same JobDetail | present; default false | `uint8_t` 0/1 | **copy** |
| 10 | `getFirstReceiveEventStart` / `firstEventStart` | `ams::util::math::UTCTime` | {`std::chrono::seconds` integral, `Femtoseconds` fractional} | absolute UTC of the first sample | present; default 0 s / 0 fs (epoch) | `int64_t first_rx_event_start_s; int64_t first_rx_event_start_fs;` | **copy** |
| 11 | `getStabPoints` / `stabPoints` | `std::vector<PointingType>` | vector of 5-way variant | beam pointing per stab point | empty by default | tagged-union array (deferred) | **require empty; else malformed** |
| 12 | `getReceiveEvents` / `rxEvents` | `std::vector<ReceiveEvent>` | vector of polymorphic events | actual Rx events incl. repeats | empty by default ("something strange" if empty) | large nested record (deferred) | **require empty; else malformed** |
| 13 | `getReceiveEventAssociations` / `rxEventIDAssociations` | `std::optional<std::vector<JobEventID>>` (`JobEventID = uint32_t`) | optional vector | per-packet event ID (VITA only) | **explicit optional**; should be `nullopt` for non-VITA | presence flag + `uint32_t` span | **require nullopt or empty; else malformed** |
| 14 | `getRxStreamIDs` / `rxStreamIDs` | `std::vector<StreamID>` (`StreamID = uint32_t`) | vector | VITA-49.2 stream IDs these products came from | empty by default; RequiredIfReceive | `ams_mel_u32_span_v1` (owned copy) | **copy** |

The probe asserts the exact return type of all fourteen getters. There are no
other getters: the class has exactly these 14 fields. There is an upstream quirk:
the rxStreamIDs setter is misnamed `setReceiveEvents(const std::vector<StreamID>&)`,
an overload of the `ReceiveEvent` setter. It matters only to a mock that
populates the field.

### `std::any userDefinedData` — policy

`std::any` has **no published generic serialization or type contract**:

* `ProductRxMetadata.h` says only "Returns the User Defined Data".
* The related `JobInterval::userDefinedContextData` (also `std::any`) says "The
  details of how this data is packaged are **implementation defined**", bounded by
  `RFMFAInfo::getMaxNumUserDefinedContextBytes()`. It offers only a
  `template <T> getUserDefinedContextData()` that uses `any_cast<T>`, so the reader
  must already know `T`. The upstream README, `include/README.md`, and the VITA
  tailoring doc define no type for either field.
* Squall never calls `setUserDefinedData`. It constructs
  `std::make_shared<ProductRxMetadata>()` and passes it unmodified, so
  `userDefinedData` is always the empty `std::any`. Squall also reports
  `getMaxNumUserDefinedContextBytes() == 0`. This is sparse, provider-specific
  behavior and does not generalize to all providers.

A generic C ABI therefore cannot represent non-empty content faithfully.
Options:

| Option | Assessment |
|---|---|
| A. Require empty; non-empty → malformed/unsupported (counted, not queued) | faithful, fail-closed, loses nothing silently |
| B. Presence flag only; content unavailable | would publish a product while knowingly discarding provider data |
| C. Defer metadata entirely; samples only | silently drops *all* metadata, including faithfully representable fields |
| D. `type().name()` / byte dump | `type_info::name` is implementation-specific; `std::any` has no byte view; not faithful |

**Decision: Option A.** 033D checks `getUserDefinedData().has_value()`. If it is
non-empty, the whole callback is rejected as malformed/unsupported: no sample
or metadata is published, and `malformed_or_unsupported` is incremented. This
never silently drops a non-empty `std::any`. A future task may add a
provider-profile-specific decoder once a provider documents the contained type.

### UTCTime (`getFirstReceiveEventStart`)

Pinned AMS Math `ams::util::math::UTCTime` stores two signed 64-bit counts:
`std::chrono::seconds integral` (`getIntegralSeconds()`) and
`Femtoseconds fractional` (`getFractionalFemtoseconds()`, where
`Femtoseconds = duration<int64_t, std::femto>`). The probe asserts both types.
Resolution is **femtoseconds**, finer than nanoseconds, and the range is
±2^63 s. Only the `(seconds, femtoseconds)` constructor normalizes. The duration
constructor can yield a negative fractional part for negative inputs. So a copy
must not assume normalization.

**Candidate C form:** `int64_t first_rx_event_start_s; int64_t
first_rx_event_start_fs;`, both copied verbatim from the two getters. There is
no conversion to nanoseconds or `double`, and no renormalization. This matches
033B's verbatim `*_fs` femtosecond fields. The AMS Math pin is reused. The
default (0 s, 0 fs) is indistinguishable from "not populated"; the record
documents that and does not invent a validity flag.

### PointingType inventory (`getStabPoints`)

`PointingType = std::variant<ECEFPointing, LLAPointing, PlatformRelativePointing,
FaceRelativePointing, BaselineRelativePointing>` (index order as listed;
asserted). `RFMELTypes.h` states "All units in (meters, seconds, radians, hertz)
unless otherwise specified".

| Alt | Class | Fields (getter → type) | Units |
|---|---|---|---|
| 0 | `ECEFPointing` (RequiredIfECEFPointing) | `getLocation()` → `const EcefPoint&`; `getVelocity()` → `const EcefVelocity&`; `getTimeOfValidity()` → `const UTCTime&` | `EcefPoint = EcefVelocity = boost::numeric::ublas::c_vector<double,3>` (global aliases in AMS Math `Geometry.h`); ECEF metres, m/s; UTC s+fs |
| 1 | `LLAPointing` (RequiredIfLLAPointing) | `getLocation()` → `const LLAPoint&` (`double lla[3]`: latitude, longitude, altitude); `getVelocity()` → `const NedVelocity&` (`c_vector<double,3>`); `getTimeOfValidity()` → `UTCTime` (by value) | WGS-84 radians, radians, metres; NED m/s; UTC s+fs |
| 2 | `PlatformRelativePointing` (RequiredIfPlatformRelativePointing) | `getLocation()` → `const AzEl&` (`double az, el`) | radians |
| 3 | `FaceRelativePointing` (Required) | `getLocation()` → `const AzEl&` | radians |
| 4 | `BaselineRelativePointing` (RequiredIfBaselineRelativePointing) | `getLocation()` → `Conic` (`double`) | radians |

The mapping to a tagged C union (`uint32_t kind` + five fixed structs of
doubles and s/fs pairs) is mechanically unambiguous. The fields are read
through `c_vector::operator()`/`LLAPoint` getters, never by layout. It is still
**deferred**: it only has meaning together with `ReceiveEvent::stabPointIndex`,
which 033D also defers.

### ReceiveEvent inventory (`getReceiveEvents`)

`ReceiveEvent : public JobEvent` (polymorphic; `vector<ReceiveEvent>` holds
concrete values, so no slicing on copy).

| Source | Field (getter) | C++ type | Notes |
|---|---|---|---|
| JobEvent | `start` (`getStart`) | `Femtoseconds` | relative to sequence iteration start |
| JobEvent | `duration` (`getDuration`) | `Femtoseconds` | |
| JobEvent | `centerFrequency` (`getCenterFrequency`) | `Frequency = double` | Hz |
| JobEvent | `iterationHoldCount`, `iterationTerminationCount` | `size_t` | |
| JobEvent | `polarization` (`getPolarization`) | `const std::vector<StokesVector>&`, `StokesVector = std::array<double,4>` | 0/1/2 entries |
| JobEvent | `enablePolarizationBeamSteerCorrection` (`getPolarizationBeemSteerCorrection` [sic]) | `bool` | |
| JobEvent | `phaseOffset` (`getPhaseOffset`) | `Angle = double` | radians |
| JobEvent | `stabPointIndex` (`getStabPointIndex`) | `size_t` | index into `stabPoints` |
| JobEvent | `weights` (`getWeights`) | `const std::map<WeightType(size_t), Weights*>&` | **raw non-owning pointers**; `Weights` is **non-polymorphic** (`TwoDimWeights` derives from it), so the dynamic type cannot be recovered safely and pointee lifetime across a `ProductRxMetadata` copy is unspecified |
| JobEvent | `direction` (`getDirection`) | `JobEvent::Direction {None, Transmit, Receive}` | published `const auto`; the const is discarded |
| JobEvent | `eventID` (`getEventID`) | `JobEventID = uint32_t` | |
| JobEvent | `executionType` | `ExecutionType {Normal, Conditional}` | |
| JobEvent | `eventTerminationType` | `JobEvent::EventTerminationType {InhibitEvent, CancelEvent}` | |
| JobEvent | `allowDelayStart`, `channelizationEnabled` | `bool` | |
| JobEvent | `pulseDetectionSettings` | `PulseDetectionSettings` | enum(uint8) reference, two `{uint8 m,n}`, `Femtoseconds minPulseWidth`, enum(uint8) timetag threshold, `vector<{double leadingEdgeDb, trailingEdgeDb}>` |
| JobEvent | `elementGroupLabel` | `ElementGroupLabel = std::string` | encoding unspecified |
| ReceiveEvent | `sampleFrequency` | `Frequency = double` | Hz |
| ReceiveEvent | `numIterationProcessingAGC`, `numIterationIgnoredPostAGC` | `size_t` | |
| ReceiveEvent | `applicableRxElementGroups` | `const std::vector<size_t>&` | |
| ReceiveEvent | `maxExtensionDuration` | `Femtoseconds` | |

Safely copyable by value: every scalar, enum, `Femtoseconds`, `double`,
`size_t` (as `uint64_t` on the 64-bit targets), polarization arrays, rx element
groups, and pulse-detection settings. What pushes complete mapping beyond the
first sample-receive slice: the per-event nested vectors (a ragged
array-of-records), the string label (needs an encoding policy), and above all
the `Weights*` map, which has no ownership, no dynamic-type recovery, and no
serialization. So a faithful complete `ReceiveEvent` mapping is its own task.
Even then a non-empty `weights` map must fail closed.

### Event associations (`getReceiveEventAssociations`)

`std::optional<std::vector<JobEventID>>`. Upstream says it is relevant only to
AMS VITA packet delivery: "Otherwise, this vector should be empty, and optional
should have no value". When present, its size "must ALWAYS equal the number of
elements in the returned data buffer", one-for-one by index.

For ComplexINT16 in 033D: **nullopt or empty is required; anything else is
validated as malformed** (whole callback rejected and counted, never silently
discarded). Tolerating `has_value() && empty()` follows the "should be empty"
wording and loses no information. The field is not represented in the first
public event because, under this validation, it never carries data there.

### RX stream IDs (`getRxStreamIDs`)

`std::vector<StreamID>`, `StreamID = uint32_t` (exact width asserted). It is
RequiredIfReceive, and a flat vector of fixed-width integers. **Copy verbatim**
into an owned `uint32_t` array, published as the existing `ams_mel_u32_span_v1`.
Squall leaves it empty.

### Squall metadata sparsity

Pinned Squall calls `std::make_shared<ProductRxMetadata>()` and passes it
**unmodified**. It never calls any setter, so every field is its default: IDs 0,
`userDefinedData` empty, `phaseCoherenceWithPrior` false, time 0/0, all vectors
empty, associations nullopt. `test_rf_mel.cc` checks only `meta != nullptr`.

```text
Squall proves the callback/sample path.
A future rich mock must prove complete ProductRxMetadata fidelity.
```

033D's mock therefore populates every copied field with distinct non-default
values, including a non-empty `rxStreamIDs` set through the misnamed setter. It
also drives each fail-closed field (`userDefinedData`, `stabPoints`,
`rxEvents`, associations) non-empty to prove rejection.

## Endpoint creation (asynchronous)

```cpp
[[nodiscard]] virtual ams::iface::mel::RequestFor<ProductRxEndpoint>
DataMEL::createProductRxEndpoint(JobDataFormat dataType,
                                 size_t regionSizeBytes,
                                 char* regionAddress = nullptr) = 0;
// RequestFor<T> = std::future<ErrorOr<std::shared_ptr<T>>>
```

The result is a **future**. It is not an ordinary synchronous factory: nothing
in the contract bounds when it becomes ready. Pinned Squall happens to complete
it before returning (a `std::promise` set inline). Its constructor even
performs a synchronous gRPC `AddDataDestination` and fails with `InvalidState`
when the backend rejects. That behavior is provider-specific.

| Design | Unbounded blocking | Timeout | Cancellation | Parent lifetime | Admission | Consistency |
|---|---|---|---|---|---|---|
| A. `endpoint_open` blocking in `future.get()` | **yes**: a caller thread may hang forever | none without a hidden thread | none | simple | n/a | **inconsistent**; the bridge never exposes unbounded provider waits |
| B. create-request owner + `wait(timeout)` → endpoint owner (worker is the only `get()` caller) | no for the caller; a worker may block | `wait(timeout_ms)`, 0 = poll; timeout ≠ cancel | none upstream; request close is not cancellation | request/worker retain the RF data state; DSO pinned until the worker finishes | per-request worker thread | **matches** the existing RequestFor pattern (navigation, C2, track requests) |
| C. background worker publishing into the DataMEL owner | no | via a separate wait | none | same as B | same | adds a second completion model and an unowned result |

**Recommendation: B.** 033D uses an `ams_mel_rf_product_rx_request` owner with
`…_wait(request, timeout_ms, &out_endpoint, …)` and an idempotent, nonblocking
`…_close`. It reuses the repository's completion engine: one worker per future,
and the worker is the only `future::get()` caller. Terminal results are cached.
A successful terminal result transfers the `shared_ptr<ProductRxEndpoint>` into
exactly one endpoint owner; the first successful wait claims it. If the request
closes before a claim, the worker destroys the unclaimed endpoint (no callback
registered yet). Timeout is never cancellation. A pending create keeps the RF
data state and DSO alive, like IR requests.

### RF admission

RF DataMEL is not an IR `Session` and has no `CompletionAdmission`. 031B's pool
is Session-scoped and does not apply. **Recommendation: no RF admission bound in
033D.** Endpoint creation is a low-rate setup operation (Squall completes it
inline). Per-DataMEL live requests/endpoints are still counted for lifetime
purposes. A bound, if later needed, should be a **future common
provider-admission design** shared by IR and RF, not an RF-specific copy. This
is recorded so that it is not mistaken for an oversight.

### Region size / external buffer

Upstream says a null `regionAddress` "instructs the MEL to manage the memory
buffer internally". Squall ignores both arguments, uses its own `iq_buffer_`,
and returns a default (0-size) `RDMAMemoryRegionParams`. Its tests pass `1024`.

**Recommendation for 033D:** always pass `regionAddress = nullptr`. Take
`region_size_bytes` as an explicit caller argument (`uint64_t`, rejected if it
exceeds `SIZE_MAX`) and pass it through verbatim. Upstream does not define its
meaning for MEL-managed memory, so the bridge invents no default and assigns
no bridge meaning to any value, including `0`. The pinned Squall tests' `1024`
is a reasonable example value, not a requirement. No host buffer, RDMA registration, or
`getRDMAMemoryRegionParams()` call in the first slice.

### Endpoint ID and assigned format

* `getEndpointID()` → `uint64_t`. Squall uses a per-context atomic counter
  starting at 1. **Expose it** as `uint64_t endpoint_id` on the endpoint owner
  and in each received event. It is read once at attach and cached.
* `getAssignedDataFormat()` is read once at attach. **Requested ComplexINT16 ⇒
  returned must be ComplexINT16.** A mismatch fails closed: the endpoint is
  destroyed without a callback registration, and the request completes with a
  distinct failure. Squall returns the requested value.

## Callback lifetime

### Published-contract gap (central 033C finding)

Upstream does **not** state any of:

```text
destroy ProductRxEndpoint  =>  all data-ready callbacks are quiescent
DataMEL::shutdown()        =>  endpoint callbacks are quiescent
setDataReadyCallback(...)  =>  replaces / waits for the previous callback
```

There is no unregister operation, and no documented threading model (which
thread invokes the callback, whether invocations may overlap, or whether a
callback can start before `setDataReadyCallback` returns). **Callback
quiescence is not proven by the interface**, and the bridge must not claim it.

### C++ object-lifetime expectation (separate, weaker statement)

A conforming provider cannot soundly keep invoking callback state *owned by a
destroyed `ProductRxEndpoint` object*. Doing so would be undefined behavior in
the provider itself. That is a **language/implementation expectation**, not an
RF MEL synchronization guarantee. It says nothing about callbacks held by other
provider objects (for example a shared context or DSO-global thread), about a
copy of the `std::function` already on another thread's stack, or about when
destruction completes relative to an in-flight invocation. Nothing stops a
provider from legally copying the `std::function` into state that outlives the
endpoint object and invoking that copy later. 033D therefore makes its own
callback state safe **independently** of provider quiescence (see callback
state ownership). It **never** treats endpoint destruction as a generic
callback-quiescence point, including for providers that have their own
evidence (Squall, below).

### Three distinct lifetimes

```text
A. Bridge callback-state lifetime
   The registered lambda captures shared_ptr<RfRxCallbackState>. Any
   provider-held copy keeps that state alive, so a callback can never
   use freed bridge memory (no bridge-state UAF).

B. Provider endpoint object lifetime
   Dropping the bridge's shared_ptr<ProductRxEndpoint> ends the bridge's
   ownership of the endpoint object. That is all it proves.

C. Provider DSO code lifetime
   Neither A nor B proves that provider code will never start another
   callback invocation. RF MEL publishes no provider-independent
   callback-quiescence primitive. This third boundary is the missing
   proof, and 033D closes it by retention, not by inference.
```

The problem is not only bridge-state UAF. The lambda **target** is compiled
into `ams_mel_c`, but a delayed invocation is still started and driven by
provider code. It may run on a provider-owned thread, from a provider call
site, or through provider callback storage and dispatch machinery (for
example, a `std::function` copy whose manager/invoker path and enclosing
frames live in the provider DSO). If that code is unmapped while such a path
can still run, the process crashes even when every bridge object is alive.

### What `in_flight == 0` does and does not prove

```text
in_flight == 0   proves:          no bridge callback invocation is executing
                                  over RfRxCallbackState NOW
in_flight == 0   does NOT prove:  no provider-held copy of the registered
                                  std::function can begin a NEW invocation later
```

So this sequence is **not** safe and must never be allowed by 033D:

```text
endpoint destruction returns
in_flight observed == 0
provider DSO unloaded
provider-held callback copy invokes later      -> executes unmapped code
```

`in_flight == 0` is **not** a DSO-unload condition.

### Squall endpoint destruction (positive, provider-specific evidence)

`SquallProductRxEndpoint` members, in declaration order:
`data_type_`, `context_` (`shared_ptr<SquallContext>`), `endpoint_id_`,
`callback_mutex_`, `user_callback_`, `iq_buffer_`, `receiver_started_`,
`receiver_` (`unique_ptr<UdpDataReceiver>`).

Exact destruction order when the last `shared_ptr<ProductRxEndpoint>` drops:

1. `~SquallProductRxEndpoint()` body runs: it calls
   `context_->getClient()->RemoveDataDestination(client_id)` (synchronous gRPC).
   **The receiver thread is still running during this step.**
2. Members are destroyed in reverse declaration order. **`receiver_` is first**:
   `~UdpDataReceiver()` sets `stop_flag_ = true`, calls `shutdown(socket_fd_,
   SHUT_RDWR)` to unblock `recvfrom`, **joins `receiver_thread_`**, then
   `close(socket_fd_)`.
3. Then `iq_buffer_`, `user_callback_`, `callback_mutex_`, …, `context_`.
4. `~ProductRxEndpoint()` / `~BaseEndpoint()` (defaulted).

Because the join completes in step 2, **before** `iq_buffer_`,
`user_callback_`, and `callback_mutex_` are destroyed and before destruction
returns, pinned Squall's receive thread is quiescent when endpoint destruction
completes. Any callback it was executing has returned. (The receiver's
`receiveLoop` catches callback exceptions and logs them.) In summary:

```text
SquallProductRxEndpoint destruction
    -> UdpDataReceiver destruction
    -> stop flag, socket shutdown, receiver-thread join
    -> receiver thread quiescent before endpoint destruction completes
```

This is positive **provider-specific** evidence about one pinned provider
revision. It is **not** the generic RF MEL contract, and 033D does not use it to
relax any production rule. The bridge must not branch on provider name,
vendor, or version (for example "Squall ⇒ unload the DSO, others ⇒ retain").
The generic rule is always "callback registered ⇒ provider DSO retained". Only
a future provider-independent quiescence mechanism may relax it.

### Squall callback storage

`setDataReadyCallback` locks `callback_mutex_`, copies `cb` into
`user_callback_`, and, **only on the first call**, starts the receiver thread
with a lambda capturing raw `this`. `UdpCallback` locks `callback_mutex_`, copies
`user_callback_` into a local, **releases the mutex**, and then invokes the copy.
Implications:

* An already-copied callback may be executing while endpoint destruction
  begins. The destructor takes no callback lock, and step 1 runs concurrently.
* Squall's quiescence point is the receiver-thread **join** in step 2.
* Callback code **must never destroy the endpoint** (drop the last
  `shared_ptr`) from inside the callback. On the receiver thread,
  `~UdpDataReceiver` would `join()` itself (`std::system_error`/deadlock)
  inside a destructor. So bridge callbacks must never hold the last endpoint
  reference and must never call endpoint Close.
* Re-registration replaces `user_callback_`, but an invocation already copied
  may still run the old callback afterwards. This is further reason to register
  exactly once.

### Callback reentrancy rules for the bridge

The registered bridge callback does **only**:

```text
validate variant / pointer / count / metadata policy
copy metadata subset + samples into bridge-owned storage
enqueue one bridge-owned event (or count a drop)
update counters
notify the receiver condition variable
```

It must **not** destroy the endpoint, close or shut down the RF DataMEL, wait
on any provider future, call any provider API, or run application code. The
copy and allocation happen **before** the queue mutex is taken. The mutex is
held only for the state check, push, and counters. No provider operation ever
runs while a bridge queue mutex is held. The callback catches every exception,
so none escape into provider threads.

## Recommended 033D receive design (not implemented)

### Endpoint lifecycle

```text
          wait() claims endpoint; format validated; callback registered
(request) ---------------------------------------------------------> Receiving
                                                                        |
                              Close (logical: stop public delivery)     v
                                                                     Closed
Receiving --(provider teardown failure / emergency retention)--> Failed (retained)
```

* No separate Attached state is exposed. Squall can invoke the callback as soon
  as a job is active after registration, so the owner is **Receiving
  immediately** after `setDataReadyCallback` returns. Registration happens
  inside the wait that claims the endpoint, before the owner is published.
* **Close is logical first.** Under the callback-state mutex it sets
  `lifecycle = Closed`, discards queued events, and wakes waiters. Only then
  does it drop the bridge's `shared_ptr<ProductRxEndpoint>` outside every
  bridge mutex. Because there is no unregister, logical Close is what stops
  public delivery. A callback that arrives later sees `Closed`, counts
  `callbacks_after_close`, and returns without copying.
* The logical Close step is, exactly:

  ```text
  lock callback state
  lifecycle = Closed
  discard queued public events
  wake receivers
  unlock
  -- then, outside every bridge mutex --
  drop the bridge's shared_ptr<ProductRxEndpoint>
  ```

  A callback that begins after logical Close increments `callbacks_received`,
  observes `Closed`, increments `callbacks_after_close`, performs **no** sample
  or metadata copy and **no** queue publication, and returns safely.
* After the provider endpoint is destroyed, Close may wait for the callback
  state's `in_flight == 0`. That proves only that no bridge callback body is
  executing over `RfRxCallbackState` right now. It lets bridge-side resources
  (queue storage, counters, the endpoint owner) become reclaimable. It is **not**
  callback quiescence and does **not** authorize provider DSO unload (see
  "DSO lifetime" below).
* `RFMEL::shutdown()` runs only once all child endpoints and create requests
  are gone. RF Data Close with live children is rejected, or it defers physical
  teardown, following the IR child pattern. See "DataMEL parent/child
  lifecycle" below for how this interacts with the DSO pin.
* Close must not be called from inside the bridge callback (it cannot be, since
  application code never runs there) and never holds the last reference on a
  provider thread.

### Callback state ownership

The `std::function` registered with the provider captures **only** a
`std::shared_ptr<RfRxCallbackState>`. It captures no raw `this`, no endpoint
owner, no provider pointer, and no DataMEL. Squall copies the `std::function`,
so the state lives as long as any provider-held copy. It stays safe even if the
public endpoint wrapper and the bridge endpoint state disappear first.

```text
RfRxCallbackState
  std::mutex                 mutex
  std::condition_variable    ready
  lifecycle                  {Receiving, Closed}
  bounded FIFO               deque<shared_ptr<const RfRxEvent>> (capacity = config)
  uint32_t                   in_flight           (callbacks currently executing)
  counters                   see queue policy
  uint64_t                   endpoint_id         (immutable after attach)
```

`in_flight` is incremented at callback entry and decremented at exit, under the
mutex, and the decrement notifies. 033D keeps it: it proves that no **bridge**
callback body is executing over this state at the observed instant,
independently of the provider. It is useful for reclaiming bridge-side state
safely once all provider-held callback copies have disappeared (the last
`shared_ptr<RfRxCallbackState>` drops). It proves nothing about whether a
provider-held copy can start a **new** invocation later. The provider's own
quiescence stays provider-specific (Squall: the receiver join).

```text
in_flight == 0 IS NOT a DSO-unload condition
```

Although the `std::function` target lambda is compiled into `ams_mel_c`, any
later invocation is started by provider code: a provider thread, call site, or
callback storage/dispatch machinery. Unloading provider code without a
quiescence guarantee is therefore unsafe, independently of bridge-state
lifetime.

### DSO lifetime (generic 033D policy)

> Once a ProductRxEndpoint callback has been successfully registered, generic
> RF MEL does not provide enough evidence to prove that no provider-held
> callback copy can invoke after endpoint destruction. Therefore the bridge
> MUST keep that provider DSO mapped for the remainder of the process unless a
> future provider-independent quiescence mechanism is established.

Invariant for the generic 033D contract:

```text
successful setDataReadyCallback
    =>
provider DSO cannot subsequently become unmapped during process lifetime
```

This is deliberate fail-safe lifetime retention, **not** a leak bug. It is
also deliberately narrow. The safety requirement is only that the **DSO stays
mapped**. It is not a requirement to keep any of these alive forever:

```text
DataMEL object
ProductRxEndpoint object
public endpoint owner
queued products
callback bridge state
```

033D should be designed so that, after successful endpoint teardown, the
`ProductRxEndpoint` may be destroyed, the DataMEL may reach its normal shutdown
boundary, and bridge queues/callback state may become reclaimable when safe
(callback state lives exactly as long as any provider-held copy). A
**dedicated provider-library pin** is what stays process-lifetime.

Acceptable implementation shapes (033D chooses and justifies one; it must not
pick one only because it is easiest):

```text
A. a dedicated shared provider-library pin (for example a
   shared_ptr<SharedLibrary> split out of RfDataState) moved into a
   process-lifetime emergency root;
B. another allocation-free permanent SharedLibrary retention root;
C. an equivalent mechanism that retains the DSO without incorrectly keeping
   public endpoint ownership, the DataMEL, or queued products alive.
```

The pin must be established **before** registration can be observed as
successful by any path that could later drop the last library reference, and
establishing it must not fail after registration succeeds. For example, reserve
any pin storage before calling `setDataReadyCallback`, or use an
allocation-free intrusive root as 033B does.

#### Registration failure

Permanent pinning is needed only once callback registration has **actually
succeeded**. For example:

```text
endpoint future resolves
assigned format validated
setDataReadyCallback throws BEFORE registration succeeds
```

does not by itself establish the permanent callback-code hazard. But upstream
does not say whether a throwing `setDataReadyCallback` may already have
copied or installed the callback before throwing. **033D design question:**
can registration state ever be proven after a throw? Strong recommendation:

```text
if setDataReadyCallback throws and registration state cannot be proven,
retain the provider DSO rather than assume no callback copy escaped.
```

Under the generic contract registration state after a throw is never provable,
so a throwing `setDataReadyCallback` pins the DSO exactly as a successful one
does. It still reports failure and publishes no endpoint owner. Only the
paths that never called `setDataReadyCallback` (for example a format mismatch or
an unclaimed endpoint destroyed by the worker) avoid the pin.

#### Shutdown exception remains stronger

033B's rule is unchanged: if `DataMEL::shutdown()` throws, the **complete**
unproven provider graph (`RfDataState`: DataMEL + library) is retained through
the emergency root. It is not weakened to "library pin only". The two retention
cases are distinct:

```text
successful endpoint teardown after callback registration:
    retain the DSO, because future callback dispatch is not disproven
shutdown() exception:
    retain the entire DataMEL/provider graph, because the shutdown
    boundary itself is unproven
```

### DataMEL parent/child lifecycle

```text
all endpoint/request children logically and physically gone
    =>
DataMEL may reach its normal shutdown()/destroy boundary

BUT

if any ProductRxEndpoint callback was ever successfully registered
(or setDataReadyCallback threw with unprovable registration state):
    provider DSO remains process-lifetime pinned
```

Child endpoints and create requests retain `RfDataState` while they exist, so
the DataMEL is not shut down under them. DataMEL shutdown and destruction are
therefore **not** gated on `in_flight == 0` or on any claimed callback
quiescence, and DSO unload is never gated on them either. Unload happens only
through the ordinary 033B path when no callback was ever registered on that
provider instance's endpoints; otherwise the separate library pin prevents it.

### Buffer copy model (ComplexINT16)

```text
provider callback(meta, JobDataPointer p, size_t n)
  -> in_flight++ ; if lifecycle != Receiving: count, in_flight--, return
  -> validate: meta != nullptr; p.index() == 3; ptr != nullptr || n == 0;
               n <= max_samples; metadata policy (below)
  -> allocate owned event (outside mutex; n * 4 bytes, overflow-checked)
  -> for i < n: out[i] = { ptr[i].real(), ptr[i].imag() }
  -> copy scalar metadata + rxStreamIDs
  -> lock: if Receiving and queue not full: push; else count drop
  -> notify ; in_flight-- ; return          (provider pointer never retained)
```

### Malformed / edge callback cases

| Case | Behavior | Counter |
|---|---|---|
| `metadata == nullptr` | reject whole callback, nothing queued | `malformed_or_unsupported` |
| wrong `JobDataPointer` alternative (index ≠ 3) | reject | `malformed_or_unsupported` |
| pointer alternative holds `nullptr` with `count > 0` | reject | `malformed_or_unsupported` |
| `count == 0` (non-null or null pointer) | accept: an empty sample product with metadata; there is no upstream rule against it, and the event is lossless | `products_queued` |
| `count` overflow when computing `count * sizeof(element)`, or `count` above the configured max-samples bound | reject before allocation | `malformed_or_unsupported` |
| non-empty `userDefinedData`, `stabPoints`, `rxEvents`, or associations | reject (fail closed, never truncated) | `malformed_or_unsupported` |
| assigned endpoint format ≠ ComplexINT16 | detected at attach; the endpoint is never registered and the create request fails | (request status, not a counter) |
| callback after logical Close | ignore without copying | `callbacks_after_close` |
| allocation/copy failure (`bad_alloc` etc.) | catch; nothing queued | `allocation_failures` |
| bounded queue full | drop the incoming product (DROP-INCOMING) | `products_dropped_queue_full` |

Every accepted or rejected invocation also increments `callbacks_received`. No
exception escapes into provider code.

### Queue policy

Use a bounded FIFO with the repository's established **DROP-INCOMING**
overflow policy (IR frames, C2/Health/Instrumentation/Track metadata). Capacity
and the per-product max-samples bound are explicit caller configuration, both
nonzero. Candidate counters (`uint64_t`, saturating), modeled on
`ams_mel_ir_stream_counters_v1`:

```text
callbacks_received
products_queued
products_dropped_queue_full
malformed_or_unsupported
allocation_failures
callbacks_after_close
```

Receive semantics match the IR metadata queues: `receive(timeout_ms)` blocks on
the condition variable, `0` polls, `AMS_MEL_TIMEOUT` if empty, and a
distinct closed status after Close. This is not a production ABI yet.

### Complex sample C type

```c
typedef struct ams_mel_rf_complex_i16_v1 {
    int16_t real;
    int16_t imag;
} ams_mel_rf_complex_i16_v1;
```

**Recommended as a public value type in 033D.** It is fixed-width, contains no
padding on any supported ABI (two `int16_t`), and is populated element-by-element
from `real()`/`imag()`. Its layout is defined by the C header, **not** by
`MELComplex`. It does not expose `std::complex` or `MELComplex<T>`, and it
makes no layout-equivalence claim.

### Receive snapshot vs caller buffer

| Option | Pros | Cons |
|---|---|---|
| A. caller-owned sample buffer (`receive(buf, capacity, &count)`) | simple C; no extra owner | a second copy (callback copy → caller); needs a two-call size protocol or `BUFFER_TOO_SMALL` handling that must not lose the product; metadata still needs a record |
| B. immutable owned event snapshot (`receive → event owner`; `view` → `const ams_mel_rf_product_rx_event_v1*`; `close`) | one copy total, since the callback copy *is* the snapshot; strong lifetime independent of endpoint, DataMEL, and DSO; carries metadata and samples atomically; same pattern as `ams_mel_ir_image_metadata_event` / `ams_mel_rf_mfa_info` | one more owner type |
| C. both | flexibility | double the API and tests in the first slice |

**Recommendation: B only.** The callback must copy anyway, so an immutable
owned snapshot adds no copy. It keeps samples and metadata together without a
`BUFFER_TOO_SMALL` path, and it reuses the view/close pattern already proven for
RF MFA info. A caller-buffer convenience can be added later without changing B.

### Initial public event shape (recommended, not final)

```c
typedef struct ams_mel_rf_product_rx_metadata_v1 {
    uint32_t mel_protocol_version_id;
    uint32_t va_definition_id;
    uint32_t va_instance_id;
    uint32_t job_details_id;
    uint32_t job_interval_id;
    uint32_t lf_type_id;
    uint32_t lf_instance_id;
    uint8_t  phase_coherence_with_prior;       /* 0/1 */
    int64_t  first_rx_event_start_s;           /* UTCTime integral seconds, verbatim */
    int64_t  first_rx_event_start_fs;          /* UTCTime fractional femtoseconds, verbatim */
    ams_mel_u32_span_v1 rx_stream_ids;         /* owned copy */
} ams_mel_rf_product_rx_metadata_v1;

typedef struct ams_mel_rf_product_rx_event_v1 {
    uint64_t endpoint_id;
    ams_mel_rf_job_data_format_t data_format;  /* always COMPLEX_INT16 (3) in 033D */
    uint64_t sample_count;                     /* elements, not bytes */
    const ams_mel_rf_complex_i16_v1 *samples;  /* owned by the event */
    ams_mel_rf_product_rx_metadata_v1 metadata;
} ams_mel_rf_product_rx_event_v1;
```

The record carries no field for `userDefinedData`, `stabPoints`, `rxEvents`, or
`rxEventIDAssociations`. That is correct, and loses nothing, **only because**
every published event is validated to have those fields empty/nullopt. The
header must state this invariant explicitly.

## ProductRxMetadata policy decision for 033D

> **033D copies the complete representable ProductRxMetadata subset: the 7
> `uint32_t` IDs, `phaseCoherenceWithPrior`, `firstReceiveEventStart` (verbatim
> s + fs), and `rxStreamIDs`. It publishes them with the owned ComplexINT16
> samples. Any callback whose `userDefinedData` is non-empty, whose
> `stabPoints` or `rxEvents` is non-empty, or whose `rxEventIDAssociations`
> holds a non-empty vector is rejected as malformed/unsupported (counted, not
> queued, not truncated).**

This loses no metadata silently. Squall (all defaults) passes the policy
unchanged. A richer provider fails closed and visibly until the deferred
PointingType/ReceiveEvent mapping exists.

## Adversarial callback evidence required by 033D

033D needs two distinct adversarial callback tests with the separate RF mock
provider. Both are mandatory.

### 1. Blocked mid-callback across Close

A provider thread enters the bridge callback and is held **inside** it while
Close runs. `ProductRxMetadata` getters and `MELComplex` accessors are
non-virtual, so the mock cannot block inside them. 033D must define a
deterministic hold point without sleeps, for example a private test-only hook
in the bridge callback that is compiled only into test objects and never
exported. The test proves:

* logical Close takes effect;
* there is no bridge UAF;
* `in_flight` drains to 0 only after the held callback returns;
* nothing the callback produced is published after Close.

### 2. Late start after zero in-flight (REQUIRED, distinct from 1)

This covers a callback that has **not started at all** when every bridge-side
drain condition is already satisfied.

```text
1. Endpoint registers the bridge callback.
2. Mock provider copies the std::function into independently retained
   provider-controlled state (a DSO-global slot, not an endpoint member).
3. Mock provider does NOT invoke the retained copy yet.
4. Public endpoint Close begins: lifecycle -> Closed; bridge drops/destroys
   the provider endpoint.
5. Endpoint destruction returns.
6. callback_state.in_flight is observed == 0.
7. Public endpoint owner is gone (in the full variant, RF Data Close has also
   run shutdown() and the DataMEL is destroyed).
8. Only NOW does another provider-controlled thread invoke the retained copy.
```

Required results:

```text
no bridge UAF (also run under an ASan/UBSan build)
callback observes Closed
callbacks_received increments
callbacks_after_close increments
no sample/metadata copy and no allocation of sample payload
no event queued
no provider API call from the callback
provider DSO is still mapped when the late callback runs and after it returns
```

#### Non-vacuity (deterministic ordering, no sleeps)

* The mock creates the late thread during the "arm" call (step 2). The thread
  records `rf_late_armed` in the lifetime log and then blocks in `read()` on a
  pipe whose write end belongs to the test. It cannot reach the invocation
  until the test writes a byte, and the test writes it only after it has
  **observed** steps 4–7. Causality, not timing, proves "late start after
  destruction and after an observed `in_flight == 0`".
* Step 5 is observed through the `rf_endpoint_destroyed` lifetime-log record,
  which the mock endpoint destructor writes, together with Close having
  returned.
* Step 6 is observed through Close's own contract (it returns only after the
  drain) **and** through a non-exported, test-only internal observation of the
  `RfRxCallbackState`. The native test links the private implementation
  objects, so there is no production export and no ABI change. The public owner
  no longer exists, so the same seam reads `callbacks_received`,
  `callbacks_after_close`, the queue length, and `in_flight` after the late
  callback. 033D must define this seam explicitly and must not add a production
  export for it.
* The late thread records `rf_late_invoke_begin` before it calls the retained
  copy and `rf_late_invoke_returned` afterwards. It then writes one byte to a
  second, test-owned pipe. The test blocks on that pipe. A watchdog bounds the
  wait; if it fires, the test fails and it never counts as a pass. There are no
  sleeps.
* "No copy" is observable. The late invocation passes `count > 0` with a
  ComplexINT16 pointer into an anonymous `PROT_NONE` mapping, so any sample
  read faults. It also passes a **null** metadata `shared_ptr`. The
  `ProductRxMetadata` getters are non-virtual, so metadata reads cannot be
  logged. A bridge that checked `Closed` first counts `callbacks_after_close`
  and leaves `malformed_or_unsupported`, `allocation_failures`, and the queue
  unchanged. A bridge that validated or copied before the lifecycle check would
  change those counters or fault.
* "No provider call from callback" reuses the 033B `rf_forbidden_call` /
  `rf_call_after_shutdown` records.
* "DSO still mapped" is checked reference-neutrally: the mock's path is still
  present in `/proc/self/maps` (or `dl_iterate_phdr`), **and** there is no
  `library_unloaded` record from the mock's `UnloadRecorder`. Do not use
  `dlopen(RTLD_NOLOAD)`, which is not reference-neutral (Task 031A).

#### Test-owned DSO pin discipline

The test's own `dlopen` of the mock may be used only while it calls TEST-only
gate functions. It must never be what keeps the DSO mapped during the late
callback:

```text
a. create the two pipes (provider-independent synchronization objects)
b. open RF data, create + claim the endpoint (callback registered)
c. dlsym + call mock_rf_arm_late_callback(release_read_fd, done_write_fd):
   the LAST provider function the test calls. The mock copies the registered
   std::function into its global slot and starts the blocked thread.
d. clear every dlsym'd pointer, dlclose the test handle (must succeed), and
   clear the handle. From here on the test holds no provider reference.
e. Close the endpoint; observe rf_endpoint_destroyed and the drain (steps 4-6)
f. Close RF data: shutdown() once, DataMEL destroyed (full variant)
g. assert: no library_unloaded record, and the mock is still in /proc/self/maps
h. write one byte to the release pipe (provider-independent release)
i. read the done pipe, then inspect the lifetime log, the mapping, and the
   internal callback-state observation. Never call a stale dlsym pointer
   after (d).
```

After (d), only the bridge's own pin can keep the mock mapped. The test
therefore proves the production bridge's DSO pin, not a test-owned one. This
follows the Task 031A discipline: clear the gate pointer, call `dlclose`
successfully, and make no further provider calls.

#### Negative control (mandatory mutation)

Temporarily remove the permanent DSO retention, for example by letting the
library pin drop with `RfDataState`. The test handle is already closed at (d),
so RF Data Close at (f) unloads the mock. Step (g) must then fail explicitly,
because `library_unloaded` is recorded and the mapping is gone. Without that
check, releasing the thread would execute unmapped code.

Run the scenario in a forked child and let the parent assert a clean exit. The
mutation's failure mode (explicit assertion or `SIGSEGV`) is then reported as a
test failure and never takes down the runner. A run where the mock stayed
mapped only because of a test-owned handle does not count. The 033D report
lists this mutation next to the existing ones (033B style) and shows that the
late-start test fails under it.

## Stop-condition review

| Stop condition | Finding |
|---|---|
| ProductRxEndpoint requires an unpinned external dependency | No. The closure is 100 % from the existing five pins |
| Closure cannot be satisfied from immutable sources | No. 539/539 byte-identical to pinned sources |
| Callback signature differs materially from the pinned evidence | No. Exactly `std::function<void(shared_ptr<ProductRxMetadata>, JobDataPointer, size_t)>`. The only nuance is the by-non-const-reference parameter, which the probe pins |
| ComplexINT16 cannot be copied without relying on undocumented layout | No. Element-wise copy through the public `real()`/`imag()`; no layout reliance |
| ProductRxMetadata cannot be represented without silent loss and no fail-closed policy exists | No. A complete representable subset plus a fail-closed policy for the four non-representable fields |
| Endpoint creation cannot be reconciled with a safe async ownership model | No. It fits the existing RequestFor request-owner/worker model |
| Teardown requires assuming a nonexistent unregister | **Not for logical Close; yes, for generic DSO unload, which is resolved by retention.** Logical endpoint Close assumes no unregister: `lifecycle = Closed` under the callback-state mutex, then shared-state capture and the `in_flight` bridge drain. However, the absence of any unregister/quiescence primitive **does** prevent a generic proof that the provider DSO can be unloaded after callback registration. Endpoint destruction plus `in_flight == 0` does not prove that no provider-held callback copy can start later. Resolution: 033D permanently pins the provider DSO after callback registration (and after a `setDataReadyCallback` throw with unprovable registration state) |
| Production API/runtime changes needed to finish the evidence task | No |
| Final exports ≠ 106 | No (see validation) |

## Recommended 033D (paste-ready boundary)

```text
033D — ComplexINT16 ProductRxEndpoint receive

Scope
- Asynchronous endpoint-creation request owner (ams_mel_rf_product_rx_request):
  submit from an open ams_mel_rf_data; one completion worker per future (the
  only future::get() caller); wait(timeout_ms) with 0 = poll; timeout is not
  cancellation; idempotent nonblocking request close; an unclaimed endpoint is
  destroyed by the worker without callback registration. No RF admission bound.
- DataMEL parent/child lifecycle: request and endpoint owners retain
  RfDataState; RF Data Close with live children follows the IR child pattern
  (reject or defer); shutdown() still exactly once. Once all endpoint/request
  children are logically and physically gone, DataMEL may reach its normal
  shutdown/destroy boundary. The library pin is retained separately.
- Callback registration permanently pins the provider DSO for the process
  lifetime unless future provider-independent quiescence evidence exists.
  Invariant: successful setDataReadyCallback => the provider DSO cannot
  subsequently become unmapped during process lifetime. A throwing
  setDataReadyCallback whose registration state cannot be proven also pins.
  The pin is a dedicated provider-library retention (process-lifetime
  emergency root or equivalent allocation-free permanent SharedLibrary
  root). It does not keep the DataMEL, ProductRxEndpoint, public owner,
  queued products, or callback state alive. Deliberate fail-safe retention,
  not a leak. No provider-name/vendor/version-based unload optimization
  (not even for Squall).
- Destroying ProductRxEndpoint and observing bridge in_flight == 0 do NOT
  authorize provider DSO unload. in_flight is kept: it proves only that no
  bridge callback body is executing now, for bridge-state reclamation.
- shutdown() failure retains the full provider graph (RfDataState), as in
  033B. That is not weakened to a library pin only.
- Requested format = ComplexINT16 only (other formats: INVALID_ARGUMENT before
  any provider call).
- createProductRxEndpoint(ComplexINT16, region_size_bytes, nullptr): provider-
  managed region; region_size_bytes is a caller-supplied pass-through.
- Endpoint ID exposed (uint64_t); getAssignedDataFormat() must equal
  ComplexINT16, otherwise fail closed with no callback registered.
- setDataReadyCallback called exactly once with a named std::function lvalue.
  The callback captures only shared_ptr<RfRxCallbackState> (mutex, cv,
  lifecycle, bounded FIFO, in_flight, counters), never the endpoint, the
  endpoint owner, the DataMEL, or a provider pointer.
- Callback: validate (metadata non-null, variant index 3, null-with-count>0,
  overflow and max-samples bound, metadata policy), copy each MELComplex<int16_t>
  element-by-element through real()/imag() into ams_mel_rf_complex_i16_v1, copy
  the metadata subset, enqueue, notify; never retain the provider pointer; no
  provider API, no application code, no endpoint destruction; catch all.
- Bounded DROP-INCOMING queue; counters callbacks_received, products_queued,
  products_dropped_queue_full, malformed_or_unsupported, allocation_failures,
  callbacks_after_close.
- Receive = immutable owned event snapshot (receive(timeout) -> event owner,
  view -> ams_mel_rf_product_rx_event_v1, close).
- Endpoint Close remains logical-first: lock callback state; lifecycle =
  Closed; discard queued events; wake receivers; unlock. Then drop the provider
  endpoint outside every bridge mutex, and then drain in_flight to 0 for
  bridge-state reclamation only. A callback that starts after Close increments
  callbacks_received and callbacks_after_close, copies nothing, queues nothing,
  and returns.
- ProductRxMetadata policy (033C): copy the 7 uint32 IDs,
  phaseCoherenceWithPrior, firstReceiveEventStart (int64 s + int64 fs verbatim)
  and rxStreamIDs; reject as malformed any callback with non-empty
  userDefinedData, stabPoints, rxEvents, or a non-empty rxEventIDAssociations.

Evidence
- Separate RF mock provider endpoint: rich non-default metadata for every copied
  field; each fail-closed field driven non-empty; wrong variant; null pointer with
  count>0; count 0; assigned-format mismatch; delayed/never-ready create future;
  provider-owned buffer overwritten immediately after callback return (proves the
  copy); queue-full drops.
- Adversarial callback, blocked mid-callback across Close: prove no bridge UAF,
  correct callbacks_after_close, and that in_flight drains only after the held
  callback returns.
- MANDATORY late-start-after-zero-in-flight adversarial mock: the mock copies
  the std::function into DSO-global state; Close, endpoint destruction,
  observed in_flight == 0, public owner gone (and DataMEL shutdown/destroy)
  all happen first; only then does a provider thread invoke the retained copy.
  Require no bridge UAF, Closed observed, callbacks_received and
  callbacks_after_close incremented, no payload copy/allocation, no event
  queued, no provider call from the callback, and the provider DSO still mapped.
  Ordering uses pipes/barriers (no sleeps). The test's own dlopen handle is
  dlclosed right after the last TEST-only gate call, before Close, and before
  the late release; no stale dlsym pointer is used afterwards. Mapping is
  checked reference-neutrally (/proc/self/maps + no library_unloaded record).
  Mandatory negative control: removing the permanent DSO pin must make the
  test fail (forked child; explicit unload observation or crash).
- Real Squall positive UDP/IQ evidence (opt-in): C2 job to activate reception,
  exact 10+20i, 30+40i, -5+6i values; endpoint destruction removes the data
  destination. Squall's receiver-thread join is provider-specific evidence only
  and does not relax the generic DSO pin.
- C translation-unit tests; ABI 0.1 export delta listed explicitly.

Out of scope
- No provider DSO unload after callback registration, and no provider-specific
  relaxation of that rule. Only a future provider-independent quiescence
  mechanism, evidenced by its own task, may relax it.
- No RDMA / getRDMAMemoryRegionParams / external endpoints / host buffers.
- No jobs / C2 / VADB in the production API (the Squall runner may use C++ to
  activate a job, as test harness only).
- No Direct, ComplexINT8, VITA, PDW or LF formats.
- No PointingType / ReceiveEvent / std::any mapping (fail closed instead).
- No safe Ada/Rust/Python RF receive API (raw FFI inventory sync only).
```

The 033C evidence disproves none of the expected direction. There are two
refinements. First, metadata is not deferred: 033D publishes the complete
representable subset and fails closed on the rest. Second, because there is no
unregister and no quiescence primitive, callback registration permanently pins
the provider DSO. Endpoint destruction plus a bridge `in_flight` drain is never
treated as permission to unload provider code.

## Validation

Local host: Debian, GCC 14.2.0, Clang 19.1.7, CMake 3.31.6, Python 3.13.5,
cargo 1.98.1, alr 2.1.1.

| Gate | Result |
|---|---|
| `make test-native` (native/build-tests, GCC) | PASS: 214/214 tests; both RF closure checks built (033A: 522; 033C: 537) |
| `make test-build-isolation` | PASS |
| Out-of-tree GCC + Clang Release builds, closure targets | PASS (table above) |
| `make check-ada-format` | PASS |
| `alr -C ada build` | PASS |
| `alr -C ada/tests run` | PASS (17 Ada contract groups) |
| `make test-rust` | PASS; `raw_inventory_matches_production_exports` (106) ok |
| `cargo fmt --check`, `cargo check --all-targets`, `cargo clippy --all-targets -D warnings` | PASS |
| `make test-python` | PASS: 104 tests; bound/exported inventory 106 |
| `python3 scripts/check_final_newlines.py`, `git diff --check`, `git diff --cached --check` | PASS |
| `make check` | **FAIL**: GNAT/GPRbuild unavailable on PATH. Every earlier step (isolation, native 214/214, Ada format) passed before the bare-GPRbuild Ada step; that step was covered by the Alire runs above |
| Final ABI audit (fresh production Release builds of the starting SHA and this branch) | ABI 0.1; **106** `ams_mel_*` exports each, all versioned `AMS_MEL_0.1`; symbol-name lists **identical** |
| Upstream checksums | all 627 verify; 16 new; 0 existing changed |

No production runtime source, mock provider, or runtime build flag changed.
The only CMake change adds a test-only OBJECT probe and a custom check target.
So the fresh repeat-50 Debug/Release regression is not required for 033C, and
it was not run. Hosted native CI is the remaining mandatory gate.
