# Task 033B RF DataMEL foundation

Task 033B is the first production RF MEL slice. It uses exactly the source
closure pinned by Task 033A (PR #55). It adds a native C foundation for one RF
`DataMEL`:

* provider DSO load and `createDataMEL(config)`;
* `getVersionInfo`;
* a fully owned `RFMFAInfo` snapshot;
* logical Close: `shutdown()` exactly once, then DataMEL destruction, then DSO
  unload.

Starting repository SHA (reviewed `main`, after 033A / PR #55 merged):
`0a335b47aa20be8ca7e23146df70a8f4e6800ab1`.

## Public ABI delta

The ABI stays **0.1**. Exactly six production exports are added (100 -> **106**):

```text
+ ams_mel_rf_data_open
+ ams_mel_rf_data_get_provider_version
+ ams_mel_rf_data_get_mfa_info
+ ams_mel_rf_mfa_info_view
+ ams_mel_rf_mfa_info_close
+ ams_mel_rf_data_close
```

No other export is added, removed, or changed. New public types:

* the opaque owners `ams_mel_rf_data` and `ams_mel_rf_mfa_info`;
* `ams_mel_rf_job_data_format_t` with all 14 published `JobDataFormat` values
  (`DIRECT_INT8` = 0 ... `LF_TYPE3` = 13). No MaxExclusive value is invented;
* `ams_mel_rf_frequency_range_v1` and its span;
* `ams_mel_rf_face_info_v1` and its span;
* `ams_mel_rf_mfa_info_v1`.

The existing `ams_mel_provider_version_v1` and `ams_mel_u32_span_v1` are
reused. No existing record or status changed.

## RF owner graph and DSO lifetime

RF is a separate provider family and is never an IR `ams_mel_session`:

```text
IR:  DSO -> getAPI_Manager / getControl -> Control -> Channels
RF:  DSO -> createDataMEL(config)        -> DataMEL

ams_mel_rf_data --shared_ptr--> RfDataState
                                  library : unique_ptr<SharedLibrary>   (existing wrapper)
                                  data    : shared_ptr<rfmel::DataMEL>
```

`SharedLibrary` is reused unchanged. It was moved verbatim to the private
`native/src/internal/shared_library.hpp`, which `internal.hpp` still includes.
No second `dlopen` wrapper exists. RF open never calls `getAPI_Manager` or
`getControl`, creates no `SessionState`, shares no IR completion admission, and
takes no aperture config ID. All RF lifecycle code is in
`native/src/rf_data.cpp`.

The DSO is unloaded only by destroying `library`. Every path destroys the
`shared_ptr<DataMEL>` first, including its provider control block and deleter:

* **Open:** `data` is a local declared after `library`, so any early return
  destroys the DataMEL before the DSO. A factory exception object is destroyed
  when its handler exits, which is also before `library` unloads.
* **Normal Close:** `shutdown()` -> `data.reset()` -> `library.reset()`.
* **`~RfDataState`:** resets `data` and then `library` explicitly, independent
  of member order. It never calls `shutdown()`.

There is no manual `dlclose`. If a C++ destructor threw, the `noexcept` Close
would terminate the process rather than unload code under a live graph.

## Factory handling

`createDataMEL` is resolved through `SharedLibrary::symbol` with the exact pinned
type `std::shared_ptr<rfmel::DataMEL> (*)(std::string_view)`. A `static_assert`
checks it against `rfmel::fnDataMEL`. It is called exactly once, only from
private C++, and is never exposed through C. All façade allocations (the state
and the public owner) happen **before** the factory call. No failure can
therefore occur between a successful factory call and publication.

| Condition | Status | Notes |
|---|---|---|
| `dlopen` failure | `AMS_MEL_LIBRARY_LOAD_FAILED` | dlerror text |
| no `createDataMEL` | `AMS_MEL_SYMBOL_NOT_FOUND` | DSO unloaded |
| factory returns null | `AMS_MEL_FACTORY_FAILED` | `createDataMEL returned null` |
| `std::bad_alloc` (bridge or factory) | `AMS_MEL_INTERNAL_ERROR` | `allocation failed` |
| factory `std::exception` | `AMS_MEL_PROVIDER_EXCEPTION` | valid UTF-8 `what()` |
| factory unknown exception | `AMS_MEL_PROVIDER_EXCEPTION` | `unknown provider factory exception` |

On every failure `*out_data` stays NULL and the DSO is unloaded. The mock
lifetime log shows `rf_factory_called`, `library_unloaded`, and `RTLD_NOLOAD`
confirms the DSO is unmapped.

## Version

`ams_mel_rf_data_get_provider_version` and
`ams_mel_session_get_provider_version` now share one internal helper. That
helper is `publish_provider_version`, plus `valid_provider_version_output` and
`translate_provider_exception`, declared in `internal/provider_common.hpp` and
defined in `provider.cpp`. The IR helper bodies were moved, not rewritten. The
IR `provider_contract` test is unchanged and passes.

Semantics:

* Buffers are caller-owned.
* `*_required` includes the NUL.
* `BUFFER_TOO_SMALL` writes only `*_required`.
* Invalid UTF-8 or an embedded NUL is `AMS_MEL_PROVIDER_EXCEPTION`.
* Exceptions are contained.

## RFMFAInfo snapshot

`get_mfa_info` calls exactly these getters on the `const RFMFAInfo&` returned
by `getRFMFAInfo()`. The reference never escapes the call.

```text
global:   getNumFaces, containsOpenAdditions, schedulerResolution,
          getMaxNumUserDefinedContextBytes, getSupportedDataFormats, getFaceIDs
per face: supportsReceive, supportsTransmit, requiresEndpointAssociation,
          getAGCProcessingTime, minJobRequestLeadTime, maxJobRequestLeadTime,
          minJobDetailLeadTime, txRxSwitchingTime, rxTxSwitchingTime,
          txTxSwitchingTime, rxRxSwitchingTime, getRxFrequencyRanges,
          getTxFrequencyRanges, getSampleFrequencyRange
```

Per-face getters run only for IDs returned by `getFaceIDs()`. Faces are never
assumed to be `0..getNumFaces()-1`. `reported_num_faces` (`getNumFaces()`) and
`faces.size` (`getFaceIDs().size()`) are both preserved and never forced equal.

These are deferred and never called:

* `quantizeDuration`: a live operation, not snapshot data.
* `getPhysicalData`: needs a new measured header and the InstallationDetails
  mapping.
* Both `getTxPowerModeCharacteristics` overloads: transmit-only and they return
  provider-owned references.
* `createProductRxEndpoint` and `registerExternalRxEndpoint`.

Representation:

* `Femtoseconds` -> `int64_t count()`, with no unit conversion.
* `size_t` -> `uint64_t` through a checked conversion. A `size_t` value wider
  than 64 bits fails closed as `AMS_MEL_PROVIDER_FAILED` with a precise
  diagnostic. This is unreachable on LP64/ILP32.
* Booleans -> `uint32_t` 0/1.
* `FrequencyRange` min/max are copied verbatim in Hz. They are never
  normalized, merged, sorted, clamped, or unit-converted.
* `JobDataFormat` -> raw `uint32_t`, in provider `std::set` order.

Ownership: `ams_mel_rf_mfa_info` owns the record, the format vector, the face
vector, and every per-face range vector. Construction runs in three steps:

1. Every vector is built completely (reserved, with no later insertion).
2. The spans are wired once.
3. The owner is published.

Nothing is mutated afterwards. Empty spans are `{NULL, 0}`. No pointer refers
to provider memory, so the snapshot survives RF Close and provider unload.
View and snapshot Close make no provider call.

This is a **point-in-time** snapshot. Pinned Squall answers the Rx and sample
range getters with a live gRPC `GetStatus`, so two snapshots may legitimately
differ. If any getter throws:

* the partial snapshot is discarded and `*out_info` stays NULL;
* `AMS_MEL_PROVIDER_EXCEPTION` is returned (`AMS_MEL_INTERNAL_ERROR` for
  `bad_alloc`);
* the DataMEL stays open and usable.

## Close, shutdown once, and shutdown-exception retention

Close consumes the public owner **before** any provider call, and `*data` is
NULL on every return. No further provider operation can therefore be initiated
through it. Close then calls `shutdown()` exactly once. Closing a NULL owner
returns `AMS_MEL_OK` and never calls `shutdown()` again. This differs
deliberately from the retryable IR channel detach Close: upstream makes
requests after shutdown begins undefined, so a retryable contract would be
unsound.

If `shutdown()` throws, Close returns `AMS_MEL_PROVIDER_EXCEPTION`. It does not
retry, does not destroy the DataMEL, and does not unload the DSO. The
`RfDataState` is kept permanently through the same allocation-free intrusive
root the IR families use:

* `emergency_self` reuses the existing control block;
* `emergency_next` is a raw link;
* an atomic head anchors the list;
* `emergency_retained` is a once-flag.

Process exit is the cleanup boundary.

Threading contract:

* Open shares nothing.
* Version, snapshot, and Close on one RF owner need external serialization.
* View and Close on one snapshot need external serialization.
* Independent snapshots may be read concurrently.

No mutex was added.

## Mock evidence (`native/tests/mock_rf_provider.cpp`)

The mock is a dedicated shared library. It exports only `createDataMEL` plus
TEST-only counters: `mock_rf_shutdown_calls`, `mock_rf_forbidden_calls`, and
`mock_rf_getter_calls`.

* `MockDataMEL` implements `getVersionInfo`, `shutdown`, and `getRFMFAInfo`.
* The endpoint methods and every out-of-scope `RFMFAInfo` method log
  `rf_forbidden_call` and throw.
* Every getter logs `rf_call_after_shutdown` if it is reached after shutdown,
  and `rf_unknown_face` for an ID not in `getFaceIDs()`.

Every test requires all of these counts to be zero.

Deterministic global values:

* Faces `{42, 7}` (set order 7, 42).
* `getNumFaces() = 2` (5 in the `inconsistent-faces` scenario).
* Open additions true, scheduler resolution 12345 fs, 9876 context bytes.
* Formats `{DirectINT8, ComplexINT16, AMSVitaLarge, LFType3}`.
* Version values differ from Squall's (`0x0000A5A5`/`0x5A5A0000`,
  `Mock RF µVendor`).

The two faces are deliberately different:

| Field | Face 7 | Face 42 |
|---|---|---|
| Receive / transmit / endpoint association | Rx yes, Tx no, association yes | Rx no, Tx yes, association no |
| Rx ranges | two non-integer ranges | empty |
| Tx ranges | empty | one range |
| Sample ranges | one range | two unordered, inverted ranges |
| Durations (fs) | 1001..1008 | include 0, `INT64_MAX`, and a negative value |

Future enum values: `JobDataFormat` is a scoped enum with the fixed underlying
type `int`, so any `int` is a valid value. The `future-format` scenario inserts
`static_cast<JobDataFormat>(static_cast<int>(0xF0000001u))`, and the C snapshot
preserves the raw value `0xF0000001`. It sorts first because its underlying
`int` is negative.

`native/tests/test_rf_data.c` (C11) runs one process per CTest case:

```text
rf-data-open  version  version-long  snapshot  two-snapshots
snapshot-independent  snapshot-throw  inconsistent-faces  future-format
missing-symbol  null-factory  factory-throw  close  shutdown-throw
shutdown-throw-unknown  invalid  allocation
```

What the cases prove:

* Every snapshot field for both faces is checked exactly.
* 100 re-inspections return identical addresses and contents, and the provider
  getter count does not change.
* The snapshot is read after RF Close, with the provider verified unmapped.
* Closing one of two snapshots leaves the other intact.
* The normal Close log is exactly `rf_factory_called`, `rf_shutdown`,
  `rf_data_destroyed`, `rf_mfa_info_destroyed`, `library_unloaded`.
* The shutdown count stays 1 across repeated Close.
* After a throwing shutdown, and after the test pin is dropped,
  `rf_data_destroyed`, `rf_mfa_info_destroyed`, and `library_unloaded` are all
  0, and the DSO stays mapped.
* All mock `dlsym` calls happen before the test pin is dropped (Task 032A rule).

Test-build-only failpoints cover allocation failure:
`AMS_MEL_TEST_RF_ALLOCATION_FAILURE=open|snapshot-owner|snapshot-range`. They
are compiled out of production builds.

* Public-owner OOM: the factory is never called and the DSO is unloaded.
* Snapshot OOM, including nested OOM after face 7's ranges and face 42's Rx
  ranges were already copied: no partial snapshot is published.
* In both cases the owner stays closable (strong guarantee).

Mutation checks (temporary source edits, reverted) confirm the tests catch
real defects:

| Mutation | Cases that fail |
|---|---|
| Unload the library before destroying the DataMEL | 10 (segfault) |
| Skip `shutdown()` | 12 |
| Make the retention a no-op | both shutdown-throw cases |
| Iterate face IDs `0..N-1` | 10 |
| Swallow a range exception | `snapshot-throw` |

## Real Squall evidence (opt-in, run)

`make test-squall-rf-c` runs `integration/squall/run-rf.sh` with
`squall_rf_c_smoke.c`. The runner:

1. Verifies the pinned checkout and its submodules (`verify-checkout.sh`).
2. Builds `libsquall_rf_mel.so` from that exact checkout through the E2E
   `squall-rf-data-consumer` target, never a registry `latest` image.
3. Starts only `squall-rf` (`config/rf-simulated.toml`, `rf_environment`) and
   Couloir, on host networking.
4. Generates a profile with the RF shape.
5. Waits a bounded time for `/ready` and the control port.
6. Runs the C client against the production Release façade.

It creates no ProductRxEndpoint, receives no UDP IQ, requests no jobs, and
touches no VADB.

Result: **PASS** against Squall `b1015728f904c799fa0c07489fce48e78f67845f`.
The extracted provider's SHA-256 is
`4d5b27ca36d480aacecd7de084b5bde88a0893836111a8927049cef85a740536`.

```text
version    api=1 library=1 vendor="Squall" description="Squall Simulator RF MEL"
reported_num_faces=1  faces={0}  contains_open_additions=0
scheduler_resolution_fs=1  max_user_defined_context_bytes=0  formats={3 ComplexINT16}
face 0: supports_receive=1 supports_transmit=0 requires_endpoint_association=0
        agc=0 min_req=0 max_req=1000000000000000 min_detail=0 switching=0 (fs)
        rx  = [915000000 .. 915000000] Hz   (live; asserted only nonempty, min<=max)
        tx  = []
        sample = [2048000 .. 2048000] Hz    (live; asserted only nonempty, min<=max)
RF Close OK; snapshot re-read after Close OK; snapshot Close OK
```

In that run, Squall's provider library stayed mapped after RF Close. Direct
C++ probes with no bridge involved traced this to the provider:

| Probe | DSO after close |
|---|---|
| Bare `dlopen`/`dlclose` | unmapped |
| Factory + `getNumFaces` + shutdown + destruction | unmapped |
| Factory + `getRxFrequencyRanges` (live gRPC `GetStatus`) + shutdown + destruction | **still mapped** |
| Through the bridge: open + Close, no snapshot | unmapped |

The provider imports `__cxa_thread_atexit_impl`. The bridge releases its own
reference in the documented order, and the mock proves that unload actually
happens. The smoke therefore reports the mapping state as information only.

## Raw language synchronization (no safe RF API)

* **Ada:** changes are limited to the private `AMS.MEL_C_API`:
  `RF_Data_Handle`, `RF_MFA_Info_Handle`, `RF_Frequency_Range_V1`,
  `RF_Face_Info_V1`, `RF_MFA_Info_V1`, the 14 format constants, and six
  imports. There is no `AMS.MEL.RF`. `make format-ada` produced no changes.
* **Rust:** changes are limited to `ams-mel-sys`: `AmsMelRfData`,
  `AmsMelRfMfaInfo`, the value structs, the `AmsMelRfJobDataFormat` constants,
  and six functions. Tests cover a C-probe layout check, an exact signature
  test, and a raw inventory of 106 matching `exports.map`.
* **Python:** changes are limited to the private `_native.py`:
  `RfDataHandle`, `RfMfaInfoHandle`, the ctypes structures, the constants, and
  six bindings. Tests cover a C-probe layout check, and `BOUND_FUNCTION_NAMES`
  (106) matches `exports.map`. The public `ams_mel.__all__` is unchanged.

Safe Ada, Rust, and Python RF APIs are deferred until after native review.

## Vendor and provenance

No vendored file changed. `native/vendor/**` and
`docs/upstream-files.sha256.md` are identical to the starting SHA.

* The façade adds `vendor/rf-mel/include` and `vendor/ams-vita/include` as
  SYSTEM includes; these are the 033A roots.
* `rf_data.cpp` includes only `DataMEL.h`, `RFCreateFunctions.h`, and
  `RFMFAInfo.h`, all of which are 033A roots.
* The `check_rf_data_header_closure` gate is unchanged and still reports 522
  headers with GCC.
* The RF MEL and AMS VITA licenses are now installed alongside the other
  upstream licenses, because their headers are compiled into the façade.
