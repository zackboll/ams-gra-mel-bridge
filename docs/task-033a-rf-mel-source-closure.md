# Task 033A RF MEL DataMEL declaration closure and provider contract

Task 033A starts RF MEL support with the same evidence-first model as Task
029A. It adds **no** application-facing RF API, no production source, and no
public binding change. It pins the RF MEL upstream, measures the
compiler-observed declaration closure of the selected DataMEL/RFMFAInfo roots,
vendors exactly that closure, proves it compiles from repository-owned
dependencies with GCC and Clang, and inventories the pinned Squall RF provider
contract to bound Task 033B.

Starting repository SHA (reviewed `main`):
`cbd8f31fdb2c45686d9b1cbd3490a50ec6ecf811`.

## Upstream pins

| Source | Repository | Commit | Tree | Use in 033A |
|---|---|---|---|---|
| RF MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` (`Release 2026.06.01`) | `f5b9d4a8601cae5639785bad7d3d2f1f8b3d9ae8` | New pin; closure vendored |
| AMS VITA | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita` | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` (`Release 2026.06.01`) | `cc5287c816e82b8166923945e322a4915feaf10c` | New pin; header-only; closure **is** reached, so observed headers vendored |
| Common MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` | Reused pin; zero new files |
| AMS Math | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` | Reused pin; 3 previously unvendored headers added |
| Boost 1.83.0 | official `boost_1_83_0.tar.bz2` (SHA-256 `6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e`), source commit `564e2ac16907019696cdaba8a93e3588ec596062` | — | — | Reused pin; 1 previously unvendored header added |
| Squall (evidence only) | `open-arsenal/ams-gra-hello-world-sk-sensors-squall` | `b1015728f904c799fa0c07489fce48e78f67845f` | `588f4999916db86c0278e3785a16e4dce60ecbdf` | Not vendored; contract inventory only |

Every repository pin was cloned into `/tmp` (outside the repository), checked
with `git fsck --full`, checked out by commit, and `git log -1
--format='%H %T %s'` reported exactly the commit, tree, and message above.
(A first RF clone transiently could not read the tree object; a fresh full
clone reproduced commit and tree exactly.) The Boost archive was re-downloaded
and matched the recorded SHA-256 before extraction.

## Nominal dependency graph vs. measured declaration closure

**Upstream full-project dependency graph.** RF MEL's `CMakeLists.txt` defines
`rfmel` as an `INTERFACE` library that `add_subdirectory`s and links
`ams_vita` and `ams_math`; AMS Math's CMake in turn requires
`find_package(Boost)`. RF MEL's CMake does not name Common MEL, but its
published headers include `<mel/library/...>` directly, so Common MEL is an
unstated header dependency.

**033A compiler-observed declaration closure.** One translation unit
including

```text
rfmel/factory/RFCreateFunctions.h
rfmel/rfmeltypes/RFMEL.h
rfmel/mfa/RFMFAInfo.h
rfmel/data/DataMEL.h
```

reaches RF MEL, Common MEL, AMS Math, **AMS VITA**, and Boost. AMS VITA is
reached through the published declaration path `RFMEL.h ->
InvalidTxPowerModeException.h -> RFMELTypes.h -> JobDataFormat.h ->
ams/iface/vita/FixedDataPacket.h`, so its five observed headers are vendored
(not the whole library). Boost is reached through `RFMELTypes.h ->
math/geometry/Geometry.h -> boost/numeric/ublas/{blas,matrix}.hpp`.

## Measurement

Measured outside the repository against the full upstream trees (not the
repository vendor directories) with `-std=c++20 -M`, the project standard
(`cxx_std_20`, `CXX_EXTENSIONS OFF`):

| Compiler | Version | Project-owned upstream files observed |
|---|---|---|
| GCC | `g++ (Debian 14.2.0-19) 14.2.0` | **522** (RF 10, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 479) |
| Clang | `Debian clang version 19.1.7 (3+b1)` (`clang++-19`) | **523** (RF 10, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 480) |
| Union | — | **524** (RF 10, Common MEL 23, AMS Math 5, AMS VITA 5, Boost 481) |

- Unique to GCC: `boost/config/compiler/gcc.hpp`.
- Unique to Clang: `boost/config/compiler/clang.hpp`,
  `boost/config/compiler/clang_version.hpp`.
- Nothing else resolved outside the five upstream trees except the compiler's
  own standard-library headers. This host has no `/usr/include/boost`,
  `/usr/local/include/boost`, or installed `rfmel`, `mel`, `math`, or VITA
  headers.

The out-of-scope areas (Admin, COSITE, DEA, Monitor, VADB, VirtualAperture
jobs, CachedWaveform, WaveformTxEndpoint, product-receive callback
implementation, RDMA receive) do **not** enter the measured closure. They
appear only as forward declarations in `RFMELTypes.h` / `RFCreateFunctions.h`.
`ProductRxEndpoint`, `ExternalEndpoint`, `RDMAExternalEndpointParams`,
`PhysicalData`, `VirtualAperture`, and `CachedWaveform` are forward-declared
only, which is enough for `DataMEL.h`. So `ProductRxEndpoint.h` is not rooted.
The VADB headers that include `boost/dll.hpp` are never reached.

For boundary information only (these files were **not** vendored), the same
method showed that also rooting `rfmel/data/ProductRxEndpoint.h` would add 16
RF headers (the `jobs/*` family, `endpoints/BaseEndpoint.h`,
`rfmeltypes/ProductRxMetadata.h`) and zero Boost headers. Rooting
`rfmel/mfa/PhysicalData.h` would add exactly that one RF header.

## Newly vendored files

| Source | Location | New files | Detail |
|---|---|---|---|
| RF MEL | `native/vendor/rf-mel/` | 12 | 10 observed headers + `LICENSE` (Apache-2.0) + `INTENT.md` |
| AMS VITA | `native/vendor/ams-vita/` | 7 | 5 observed headers + `LICENSE` (Apache-2.0) + `INTENT.md` |
| AMS Math | `native/vendor/ams-math/` (existing tree) | 3 | `math/geometry/Geometry.h`, `math/geometry/LLAPoint.h`, `math/units/UTCTime.h` |
| Common MEL | `native/vendor/common-mel/` | 0 | All 23 observed headers were already vendored |
| Boost 1.83.0 | `native/vendor/boost-1.83.0/` (existing tree) | 1 | `boost/numeric/ublas/blas.hpp` |
| **Total** | | **23** | |

RF MEL headers vendored:

```text
include/rfmel/data/DataMEL.h
include/rfmel/factory/RFCreateFunctions.h
include/rfmel/mfa/RFMFAInfo.h
include/rfmel/mfa/TxPowerModeData.h
include/rfmel/rfmeltypes/InvalidTxPowerModeException.h
include/rfmel/rfmeltypes/JobDataFormat.h
include/rfmel/rfmeltypes/MELComplex.h
include/rfmel/rfmeltypes/RFMEL.h
include/rfmel/rfmeltypes/RFMELTypes.h
include/rfmel/rfmeltypes/ResourceUseAfterExpiredException.h
```

AMS VITA headers vendored:

```text
include/ams/iface/vita/DataPacket.h
include/ams/iface/vita/DataPacketBase.h
include/ams/iface/vita/FixedDataPacket.h
include/ams/iface/vita/GeneratedMasks.h
include/ams/iface/vita/Primitives.h
```

No docs, tests, Containerfile, Makefile, CMake, scripts, or spec sources were
copied. No second AMS Math, Common MEL, or Boost tree was created. The three
AMS Math additions come from the same pinned AMS Math commit as the existing
tree. `blas.hpp` comes from the same Boost 1.83.0 archive, and it is the only
header in the RF Boost union that was not already in the 483-header Track
union.

### Byte-identity audit

Each new RF MEL, AMS VITA, and AMS Math file was written with `git cat-file
blob <pinned-commit>:<path>` and then re-hashed with `git hash-object`. All 22
hashes equal the pinned tree's blob ids, including the `LICENSE` and
`INTENT.md` files. `blas.hpp` was compared with `cmp` against the verified
Boost 1.83.0 archive. The previously vendored files that belong to the RF union
(23 Common MEL, 2 AMS Math, 480 Boost) were also compared with `cmp` against
the pinned upstreams, with zero differences. No formatting, newline
normalization, generated comment, or local patch was applied.
`.gitattributes` now marks `native/vendor/rf-mel/**` and
`native/vendor/ams-vita/**` as `-whitespace`, as Boost already was, because the
pinned upstream `DataPacket.h`, `Primitives.h`, and `blas.hpp` contain trailing
whitespace that must be kept.

`docs/upstream-files.sha256.md` gains a sorted Task 033A block with the 23 new
files. No existing checksum line changed. All 611 recorded checksums verify.

## Declaration probe and closure checker

- `native/tests/rf_data_header_compile_probe.cpp` includes the four roots and
  contains only `static_assert`s about the published types (the `fnDataMEL`
  pointer shape, `DataMEL : RFMEL`, both classes abstract,
  `JobDataFormat::ComplexINT16 == 3`) and a trivial function. It instantiates
  no provider object and defines no RF type.
- CMake target `rf_data_header_compile_probe` (OBJECT, test builds only):
  `cxx_std_20`, `CXX_EXTENSIONS OFF`, `-Wall -Wextra -Wpedantic -Werror`. Its
  only include roots are `vendor/rf-mel/include`, `vendor/common-mel/include`,
  `vendor/ams-math/include`, `vendor/ams-vita/include`, and
  `vendor/boost-1.83.0`, all `SYSTEM`. They must be `SYSTEM` because, under
  Clang, the byte-identical pinned `TxPowerModeData.h` triggers
  `-Wignored-qualifiers` (`const auto` return) and Boost uBLAS, instantiated
  from AMS Math `Geometry.h`, triggers a `std::iterator` deprecation warning.
  Upstream stays unpatched. The first-party probe itself still compiles with
  the full warning set.
- `native/scripts/check_rf_data_closure.py` runs from the always-built
  `check_rf_data_header_closure` target and dependency-probes the probe with
  the configured `CMAKE_CXX_COMPILER`. It classifies every dependency as
  `rfmel/`, `mel/library/`, `math/`, `boost/`, or `ams/iface/vita/`. It fails
  when any of them resolves outside that family's approved vendor root, when a
  vendored file cannot be classified, or when an expected family is missing.
  Standard-library and compiler headers are allowed. Before probing, a
  built-in self-test confirms that the checker rejects `/usr/include/boost`,
  `/usr/local/include/boost`, `/usr/include/rfmel`, `/usr/local/include/rfmel`,
  system Common MEL / AMS Math / VITA paths, and headers placed under the
  wrong family inside the vendor tree.
- The target uses only `CMAKE_CURRENT_SOURCE_DIR` paths and the configured
  compiler. It assumes no fixed `native/build` path and works in the
  selector-driven `native/build-tests` tree, the CI `build/native` tree, and
  any out-of-tree build directory.

### Closure validation

| Check | GCC 14.2.0 | Clang 19.1.7 |
|---|---|---|
| Probe builds with `-Werror` through CMake (out-of-tree Release) | PASS | PASS |
| `check_rf_data_header_closure` | PASS: 522 headers (rfmel 10, mel 23, math 5, boost 479, vita 5) | PASS: 523 headers (rfmel 10, mel 23, math 5, boost 480, vita 5) |
| Existing `check_track_header_boost_closure` | PASS: 481 (unchanged) | PASS: 482 (unchanged) |
| Negative system-path test: rogue `boost/numeric/ublas/blas.hpp` and `rfmel/rfmeltypes/MELComplex.h` on an `-isystem` root searched before the vendor roots | Rejected as required, both leaks named | Rejected as required, both leaks named |

System-dependency leakage result: **none**. No RF, Common MEL, AMS Math, AMS
VITA, or Boost header resolves outside `native/vendor`. Nothing is fetched,
cloned, or downloaded at configure, build, or test time: no FetchContent and
no ExternalProject.

## Squall RF provider inventory (evidence only)

### Exports

Pinned `interfaces/squall-rf-mel-impl/exports.map` exports exactly:

```text
createAdminMEL
createC2MEL
createCOSITEMEL
createDataMEL
createDEAMEL
createMonitorMEL
gra_iface_mel_rf_vadb_GetVADB
```

The first-slice factory (`src/SquallRFFactory.cc`) is:

```cpp
extern "C"
std::shared_ptr<ams::iface::rfmel::DataMEL>
createDataMEL(std::string_view config);
```

This signature matches the published `ams::iface::rfmel::fnDataMEL` type from
`RFCreateFunctions.h` exactly, and the probe asserts that type. The Squall
contract therefore does not differ from the pinned public RF interface.

### Factory ABI hazard

`extern "C"` fixes only the **symbol name**. The function signature is still
C++: it returns `std::shared_ptr<DataMEL>` and takes `std::string_view`. The
factory can also throw. For example, Squall's `SquallContext::getInstance`
treats the config string as a JSON file path and throws
`std::invalid_argument` or `std::runtime_error` for an empty path, an
unreadable file, invalid JSON, or a missing required field. The future bridge
must therefore:

- resolve the C-linkage symbol with `dlsym` and cast it to `fnDataMEL`;
- call it only from private C++ inside `ams_mel_c`;
- catch every exception and keep all `shared_ptr` ownership inside the adapter;
- keep the DSO loaded until every provider-owned C++ object (the DataMEL, the
  `RFMFAInfo` it owns, endpoints, futures, callbacks, and deleters) has been
  destroyed;
- never expose this signature through `abi.h`.

`DataMEL.h` declares a static `DataMEL::create(const std::string&)`, but
neither RF MEL nor Squall defines it, so the bridge must not call it.

Squall keeps one `SquallContext` per config-path string in a process-global
weak-pointer map. All six `create*MEL` factories use that map, so DataMEL,
C2MEL, and MonitorMEL created from the same path share one context (and one
gRPC client).

### DataMEL contract (pinned RF MEL)

`class DataMEL : public RFMEL`. The class as a whole is annotated `@Required`.

| Operation | Declared in | Upstream annotation | Category |
|---|---|---|---|
| `mel::VersionInfo getVersionInfo() const` | `RFMEL` | `@Required` | Required |
| `void shutdown()` | `RFMEL` | `@Required` | Required |
| `RequestFor<ProductRxEndpoint> createProductRxEndpoint(JobDataFormat, size_t regionSizeBytes, char* regionAddress = nullptr)` | `DataMEL` | `@RequiredIfReceive` | Conditional: receive-capable MFA |
| `RequestFor<ExternalEndpoint> registerExternalRxEndpoint(JobDataFormat, const RDMAExternalEndpointParams&)` | `DataMEL` | `@RequiredIfReceive` **and** `@RequiredIfRDMA` | Conditional: receive-capable **and** RDMA-capable MFA |
| `const RFMFAInfo& getRFMFAInfo() const` | `DataMEL` | `@Required` | Required |

`RequestFor<T>` is Common MEL `std::future<ErrorOr<std::shared_ptr<T>>>`.

### Shutdown contract

Upstream `RFMEL::shutdown()` says: "Prepares an RFMEL to be shutdown.
Additional requests (for Endpoints, VAs, Jobs, etc.) result in undefined
behavior." The future C façade therefore needs a clear logical-close boundary.
Once shutdown begins, the façade must reject new provider operations itself
instead of forwarding them. Pinned Squall implements `shutdown()` as an empty
body on every RF MEL class, so Squall gives no evidence about post-shutdown
behavior or callback quiescence.

### Squall DataMEL positive evidence

- `createDataMEL(config)` constructs a `SquallDataMEL` that holds the shared
  `SquallContext` and its own `SquallRFMFAInfo`.
- `SquallDataMEL::getVersionInfo()` returns `VersionInfo(1, 1, "Squall",
  "Squall Simulator RF MEL")`: API version 1, library version 1, vendor
  `Squall`, description `Squall Simulator RF MEL`.
- `getRFMFAInfo()` is really implemented. It returns `*dummy_info_`, a
  `SquallRFMFAInfo` owned by the DataMEL, so the reference is valid only while
  the DataMEL lives. Despite the member name, `SquallRFMFAInfo` is a real
  implementation: face, format, and physical data come from the profile, and
  the frequency ranges come from live backend status. Pinned Squall's
  `tests/test_rf_mel.cc` exercises this same class through `C2MEL` and the
  VADB. Those tests assert one face with ID 0, receive-only operation,
  `ComplexINT16` as the only supported format, and the expected ranges. The
  DataMEL instance is a separate object of the same class.

DataMEL plus RFMFAInfo is therefore a real, positively implemented provider
surface and is suitable for the next production slice.

### Squall receive evidence boundary (future; not implemented here)

`SquallDataMEL::createProductRxEndpoint` positively supports only
`JobDataFormat::ComplexINT16`. Any other format resolves the future with
`ErrorCode::InvalidParameters`. It returns an asynchronous
`RequestFor<ProductRxEndpoint>` (already satisfied when returned), and a
failure while constructing the endpoint becomes `ErrorCode::InvalidState`. The
`SquallProductRxEndpoint`:

- binds a UDP socket to an ephemeral port and registers that destination with
  the Squall backend over gRPC (`AddDataDestination`), then unregisters it in
  its destructor;
- starts a receiver thread on the first `setDataReadyCallback` and receives raw
  UDP payloads of up to 65535 bytes;
- drops a payload when no job is active or when its size is not a multiple
  of 4;
- decodes little-endian signed 16-bit I/Q pairs into an internal
  `std::vector<MELComplex<int16_t>>` owned by the endpoint;
- calls the `ProductRxEndpoint` data callback with a new `ProductRxMetadata`, a
  `JobDataPointer` into that internal vector, and the sample count. The same
  vector is reused for the next datagram.

The real provider can therefore support a later IQ receive task through the
normal `ProductRxEndpoint` path.

### No zero-copy claim for RF

Squall's RF `ProductRxEndpoint` copies and decodes the incoming UDP bytes into
its internal IQ vector before calling the MEL callback. It ignores
`regionSizeBytes` and `regionAddress`. Future RF support must **not** use this
evidence to claim end-to-end zero copy, RDMA zero copy, GPU zero copy, or FPGA
zero copy. The pointer passed to the callback is valid only during the
callback.

### RDMA evidence boundary

In pinned Squall, `SquallDataMEL::registerExternalRxEndpoint(...)` is a stub.
It logs "Stub called" and returns a future holding `ErrorCode::Unsupported`,
`"Not implemented"`. Pinned Squall's tests assert that `Unsupported` result.
Initial positive RF receive work should target the normal `ProductRxEndpoint`
path, not RDMA.

### RF C2 future-slice inventory (`SquallC2MEL`)

- `requestVirtualAperture` is positively implemented for the configured VA. It
  returns a `SquallVirtualAperture` when `vaDefID` equals the profile's
  `va_definition_id` and `localFunctionInfo` is empty. Otherwise it returns
  `InvalidId` or `Unsupported`.
- The Squall profile accepts RX jobs. `SquallVirtualAperture::requestJob`
  rejects every non-RX element group with `Unsupported` ("the Squall RF
  profile accepts RX jobs only"). It also requires the configured RX
  element-group label, a duty factor in (0, 1], and expected center frequencies
  inside the live receive capabilities. Only one job can be pending or active
  at a time, and the job tunes the backend through gRPC `TuneRf`.
- `requestCachedWaveform` (cached waveform transmit) returns `Unsupported`,
  `"Not implemented"`, and `isCachedWaveformSupported()` returns `false`.
- `createWaveformTxEndpoint` (TX waveform endpoint) returns `Unsupported`,
  `"Not implemented"`.
- `registerExternalTxEndpoint` (external TX RDMA endpoint) returns
  `Unsupported`, `"Not implemented"`.
- `SquallC2MEL` owns its own separate `SquallRFMFAInfo` instance.

033A implements none of this.

## RFMFAInfo inventory

`RFMFAInfo` is annotated `@Required`. It cannot be copied or moved, and it
has 24 public pure-virtual operations under 23 names, because
`getTxPowerModeCharacteristics` is overloaded. "Per face" below means the
operation takes an `ams::iface::rfmel::FaceID` (`uint32_t`). `Femtoseconds` is
AMS Math `std::chrono::duration<int64_t, std::femto>`, which covers about
±2.56 hours.

| Operation | Upstream annotation | Return type | Category | DataMEL relevance | Squall implementation | Candidate C representation concerns |
|---|---|---|---|---|---|---|
| `getAGCProcessingTime(FaceID)` | `@RequiredIfAGC` | `Femtoseconds` | Conditional (AGC) | Timing for receive jobs | Constant `0` | Signed int64 femtosecond count, no unit conversion. A provider without AGC still returns a value, and nothing marks it absent |
| `getRxFrequencyRanges(FaceID)` | `@Required` | `std::vector<FrequencyRange>` | Required | Core receive capability | Implemented. Configured face only; ranges come from a **live** gRPC `GetStatus` and are empty if the backend is unavailable | Owned per-face array of `{double min_hz, double max_hz}` plus a count. The snapshot depends on when it is taken |
| `getTxFrequencyRanges(FaceID)` | `@Required` | `std::vector<FrequencyRange>` | Required | Capability disclosure | Empty vector | Same as Rx |
| `requiresEndpointAssociation(FaceID)` | `@Required` | `bool` | Required | Endpoint/pipe setup | `false` | Normalize to 0/1 |
| `containsOpenAdditions()` | `@Required` | `bool` | Required | Informational | `false` | Normalize to 0/1 |
| `supportsTransmit(FaceID)` | `@Required` | `bool` | Required | Capability | `false` | Per-face 0/1 |
| `supportsReceive(FaceID)` | `@Required` | `bool` | Required | Gates receive | `true` for the configured face only | Per-face 0/1 |
| `getNumFaces()` | `@Required` | `size_t` | Required | Face enumeration | `1` | `uint64_t`. Upstream does not require it to equal `getFaceIDs().size()`, so carry both unchanged |
| `schedulerResolution()` | `@Required` | `Femtoseconds` | Required | Job timing | `1` fs | int64 fs |
| `quantizeDuration(Femtoseconds)` | `@Required` | `Femtoseconds` | Required | Job-timing helper | Identity | A **function, not data**. It cannot be snapshotted, so it needs a live call or must be deferred |
| `minJobRequestLeadTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `maxJobRequestLeadTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `10^18` fs (1000 s) | Per-face int64 fs |
| `minJobDetailLeadTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `txRxSwitchingTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `rxTxSwitchingTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `txTxSwitchingTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `rxRxSwitchingTime(FaceID)` | `@Required` | `Femtoseconds` | Required | Job timing | `0` | Per-face int64 fs |
| `getSampleFrequencyRange(FaceID)` | `@Required` | `std::vector<FrequencyRange>` | Required | Receive sample rate | Implemented. Live gRPC; empty if the backend is unavailable | Owned per-face range array |
| `getMaxNumUserDefinedContextBytes()` | `@RequiredIfUserDefinedContextData` | `size_t` | Conditional (user-defined context data) | JobInterval context | `0` | `uint64_t` |
| `getSupportedDataFormats()` | `@Required` | `std::set<JobDataFormat>` | Required | Chooses the receive format | `{ComplexINT16}` | Ordered array of enum values. Keep unknown future values instead of dropping them |
| `getPhysicalData(FaceID)` | `@Required` | `const PhysicalData&` | Required | Face geometry | Implemented: height 0.25 m, width 0.25 m, lattice angle 0 rad, default `InstallationDetails` | Returns a **reference** into provider storage, so copy it immediately. The full type needs `PhysicalData.h` (+1 measured RF header) and a complete Common MEL `InstallationDetails` mapping. Only forward-declared in the 033A closure |
| `getTxPowerModeCharacteristics(FaceID)` | `@RequiredIfTransmit` | `const std::vector<TxPowerModeData>&` | Conditional (transmit) | Transmit only | Empty vector | Reference into provider storage. Each mode nests a range vector and a `std::chrono::nanoseconds` pulse width |
| `getTxPowerModeCharacteristics(TxPowerModeID, FaceID)` | `@RequiredIfTransmit` | `const TxPowerModeData&` | Conditional (transmit) | Transmit only | The same static default-constructed value for every ID | Reference. No "not found" result (upstream publishes `InvalidTxPowerModeException`, but the header does not say this getter throws it) |
| `getFaceIDs()` | `@Required` | `std::set<FaceID>` | Required | Key for face enumeration | `{profile.face_id}` | Ordered `uint32_t` array |

`ams_mel_c` implements none of these getters in 033A.

## Type-risk inventory

| Published type | Source | Shape | Classification |
|---|---|---|---|
| `FaceID`, `TxPowerModeID`, `TxPowerLevel`, `EndpointID`, `Frequency`, `DutyFactor`, and `bool`/`size_t` returns | `RFMELTypes.h`, RFMFAInfo | integer/double aliases | trivial scalar (widen `size_t` to `uint64_t`) |
| `FrequencyRange` | `RFMELTypes.h` | class holding `double min, max` in Hz. The default constructor uses `numeric_limits<double>::min()` (the smallest positive value, not the lowest) and `max()` | owned snapshot needed (`{min_hz, max_hz}`, copied unchanged) |
| `std::vector<FrequencyRange>` | RFMFAInfo | per-face vector | span/vector needed (owned array) |
| `JobDataFormat` | `JobDataFormat.h` | `enum class` with 14 values | trivial scalar (`uint32_t`); keep unknown values |
| `std::set<JobDataFormat>` (set of enum values) | RFMFAInfo | ordered set | span/vector needed |
| `std::set<FaceID>` and face-keyed getters | RFMFAInfo | ordered set plus per-face queries | span/vector needed; a snapshot iterates over faces |
| `Femtoseconds` | AMS Math `UTCTime.h` | `duration<int64_t, femto>` | trivial scalar (signed int64 femtosecond count, no conversion) |
| `std::chrono::nanoseconds` (in `TxPowerModeData`) | RF MEL | duration | trivial scalar (int64 ns); not in first slice |
| `UTCTime` | AMS Math | seconds + femtoseconds | owned snapshot needed; not in first slice |
| `PhysicalData` | `mfa/PhysicalData.h` (only forward-declared in the 033A closure) | 3 doubles + Common MEL `InstallationDetails` | owned snapshot needed; requires +1 measured header |
| `TxPowerModeData` | `mfa/TxPowerModeData.h` | scalars + range vector + ns duration | owned snapshot needed; transmit-only, not in first slice |
| Geometry types (`AnglePair`; AMS Math `Geometry.h` points and vectors) | RF MEL / AMS Math | doubles and Boost uBLAS containers | owned snapshot needed; not in first slice |
| `MELComplex<T>` and the `JobDataPointer` variant | `MELComplex.h`, `JobDataFormat.h` | interleaved complex integers; a variant of raw pointers that includes VITA packet buffers | span/vector needed (typed sample spans with an explicit format); not in first slice |
| AMS VITA `FixedDataPacket<N>` | AMS VITA | packet storage | span/vector needed (byte or packet view); not in first slice |
| `mel::VersionInfo` | Common MEL | 2 × `uint32_t` + 2 strings | owned snapshot needed (reuse the existing IR VersionInfo pattern) |
| `std::shared_ptr<DataMEL>` (returned by the factory) | `RFCreateFunctions.h` | provider-owned object | opaque owner needed; keeps the DSO loaded |
| `const RFMFAInfo&` | DataMEL | reference into the DataMEL | never exposed; snapshot it while the DataMEL is alive |
| `RequestFor<ProductRxEndpoint>`, `RequestFor<ExternalEndpoint>` | DataMEL | `std::future<ErrorOr<shared_ptr<T>>>` | async request owner needed; not in first slice |
| `std::shared_ptr<ProductRxEndpoint>` with a `std::function` data callback | `ProductRxEndpoint.h` | endpoint that delivers data by callback | opaque owner plus callback quiescence needed; not in first slice |
| `RDMAExternalEndpointParams` | endpoints | RDMA parameters | not in first slice (Squall returns Unsupported) |

## Future ownership hazards

1. The factory has C linkage but a C++ signature, and it may throw. Call it
   only from C++ inside the adapter and translate exceptions into status codes.
2. `getRFMFAInfo()`, `getPhysicalData()`, and `getTxPowerModeCharacteristics()`
   return references into provider-owned storage. Copy the data into
   façade-owned memory before returning to C, and never keep a reference past
   the DataMEL's lifetime.
3. The DSO must stay loaded until the `shared_ptr<DataMEL>` and everything it
   created (endpoints, futures, callbacks, `std::function` targets) have been
   destroyed. Provider-side `shared_ptr` control blocks and deleters live in
   the DSO.
4. Squall shares one `SquallContext` (gRPC client and job state) across every
   MEL family created with the same config string. Destruction order across
   those families is internal to the provider.
5. After `shutdown()` begins, further provider requests are undefined, so the
   façade must enforce its own closed state.
6. Some Squall getters perform live network I/O (gRPC `GetStatus`). A
   "snapshot" is a point-in-time value, may be empty when the backend is
   unavailable, and is not a constant property.
7. Future receive callbacks run on provider threads over provider-owned,
   reused buffers. Upstream does not document callback quiescence.

## Recommended next slice (033B)

The measured interface shows no serious obstacle to the expected slice. The
Squall factory matches the published `fnDataMEL` type. Squall positively
implements DataMEL's required base operations. RFMFAInfo is a real provider
object whose required getters return scalars, enums, and value vectors.

Recommended **033B: RF DataMEL foundation**:

- `dlopen` a caller-selected RF provider DSO, loaded locally as for IR;
- resolve `createDataMEL` and call it once from C++ with exceptions contained,
  producing one opaque DataMEL owner;
- implement `getVersionInfo` with the existing owned VersionInfo pattern;
- expose a fully owned RFMFAInfo snapshot of the **scalar/enum/range** subset:
  face IDs, `getNumFaces`, `containsOpenAdditions`,
  `getMaxNumUserDefinedContextBytes`, and supported data formats; and per face,
  receive/transmit support, the endpoint-association flag, Rx/Tx/sample
  frequency ranges, AGC time, lead times, and switching times; plus
  `schedulerResolution`;
- expose `quantizeDuration` as a separately justified live call, or defer it;
- defer `getPhysicalData`, which needs +1 measured header and the Common MEL
  `InstallationDetails` mapping, and the transmit-only
  `getTxPowerModeCharacteristics` overloads. Document these omissions rather
  than silently dropping fields;
- add an explicit logical close that calls `shutdown()` once, rejects later
  operations, releases the DataMEL, and only then unloads the DSO. A mock RF
  provider must prove DSO lifetime before any real-Squall validation.

Receive (`createProductRxEndpoint`, ComplexINT16, callbacks), RDMA, RF
C2/VA/jobs, and every other RF MEL family stay out of scope until each is
separately measured and justified.

## Scope and ABI

033A changes nothing under `native/src/`, `native/include/`, `ada/`, `rust/`,
or `python/`. It adds no runtime RF test. The production façade remains ABI
0.1 with exactly 100 exports, identical by symbol name to the starting SHA.
