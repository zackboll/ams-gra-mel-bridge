# Task 034E4 — VA capability and Local Function queries

## Baseline and scope

Starting origin/main: `f0f2e4e558c9cc0fa14ef54d9db92a8108501914`.
PR #71 was verified merged; no open PR/equivalent branch existed and the
worktree was clean. Branch: `feature/034e4-rf-va-capability-local-functions`.
Measured before interface editing: native 270/270, Python 117/117, production
172 exports; vendor inventory 804. Baseline aggregate check passed, but its
background execution overlapped early edits; it is not claimed as an immutable
pristine-baseline aggregate run. Validation logs are under `/tmp/ams-034e4`.

Exactly four pinned methods are represented:

| Published method | Classification | C operation |
|---|---|---|
| isCachedWaveformSupported() const | @Required | ams_mel_rf_virtual_aperture_is_cached_waveform_supported |
| dynamicWeightsSupported() const | @Required | ams_mel_rf_virtual_aperture_dynamic_weights_supported |
| getLocalFunctions() const | @RequiredIfLFSupport | ams_mel_rf_virtual_aperture_get_local_functions |
| getLocalFunctionStatus(instance,type) const | @RequiredIfLFSupport | ams_mel_rf_virtual_aperture_get_local_function_status |

RF MEL stays pinned at `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
No vendor expansion or second LF-status enum. The existing E1 C status domain
and Ada `Virtual_Aperture_Queries.Status_Kind` remain canonical.

## ABI and fidelity

ABI 0.1 adds exactly eight exports. Besides the four live operations above:

- `ams_mel_rf_va_local_function_list_view`
- `ams_mel_rf_va_local_function_list_close`
- `ams_mel_rf_va_local_function_status_view`
- `ams_mel_rf_va_local_function_status_close`

New opaque owners: `ams_mel_rf_va_local_function_list` and
`ams_mel_rf_va_local_function_status`. New records:
`ams_mel_rf_va_local_function_info_v1` (uint32 type ID, uint64 instance count)
and `ams_mel_rf_va_local_function_info_span_v1` (const info pointer, size_t
length). Direct status View reuses `ams_mel_u32_span_v1`, every entry validated
in the existing `ams_mel_rf_virtual_aperture_status_t` domain 0..3.

Each explicit live query invokes exactly its matching provider method once.
Required Boolean False succeeds as 0; True succeeds as 1. Nothing is cached
in the public VA. LF catalog copies the provider's by-value std::map in
ascending unsigned type-ID order; zero counts remain. size_t counts widen
exactly to uint64 with a static digits check rejecting wider-size_t platforms.
Status queries forward both uint32 IDs exactly, preserve full vector length,
order and duplicates, and synthesize no IDs. Vector index is upstream LF
instance index 0..N-1. Empty maps/vectors succeed with {NULL,0}; they are not
translated to unsupported/failure or interpreted as an LF-support Boolean.

Independent live calls have no atomic consistency guarantee. Status length is
not reconciled with a catalog count; catalog/type and VA-instance membership
are not prerequisites. E1 getInstanceStatusReport remains a separate operation
using its own provider-returned nested map. Claim reads only its earlier info
getters. E2 callbacks still ignore their reference and call no provider getters;
E3 descriptor code is unchanged. Native mock counters distinguish these paths.

## Ownership and failure semantics

Creation uses only the existing live VA claim, with same-owner external
serialization including Close. No lifecycle/notification/Job/queue lock is held
across provider getters. No new child/sibling claim, future, worker, callback,
permanent shell or DSO pin. Provider temporaries die before return. Published
owners hold only bridge primitive vectors, never a provider object or E1 report.
Views/Close allocate/call no provider and remain usable after VA/C2/provider
destruction and actual unpinned DSO unload. E2's intentional permanent callable
pin remains separate and is exercised by its existing isolated regression.

Invalid public inputs are rejected before provider calls with INVALID_ARGUMENT,
outputs unchanged. Creation output must initially be null. Every exception
path leaves no partial owner. bad_alloc (provider or bridge copy) maps to
INTERNAL_ERROR; other std/unknown exceptions to PROVIDER_EXCEPTION. Any unknown
status, including one following valid entries, rejects the entire owner with
PROVIDER_FAILED. No substitution/drop/truncation. Prior snapshots survive and
a later explicit valid query succeeds; no query is repeated for diagnostics.
Close consumes only the bridge owner and already-null is successful.

## Safe Ada and raw parity

`Virtual_Aperture_Queries.Cached_Waveform_Supported` and
`Dynamic_Weights_Supported` return fresh Boolean values, False not an exception.
New `AMS.MEL.RF.C2.Local_Functions` exposes ordinary private
Local_Function_List/Local_Function_Info/Status_List values, snapshots, Count,
Info_At, Type_ID, Instance_Count and Status_At. Ada Index 1 maps to upstream
instance index 0 only as a container convention. No public Address/span/native
handle/Close/provider-aware finalizer. Controlled private temporaries protect
native owners across conversion errors. Pointer/count/alignment/address extent,
Natural, Count_Type, Storage_Offset and status-domain checks precede copying.
Native owners close before returning Ada-owned containers.

Private Ada FFI, Rust sys and private Python ctypes track all eight operations
and record layouts/signatures/inventory. No safe Rust or public Python RF LF
or capability API is added.

## Mock evidence

The existing MockVirtualAperture supplies independent four-method counters,
exact input observations and generation control. Generation A returns
True/False, catalog keys 0,7,0x80000001,UINT32_MAX with counts 2,3,1,0, and
Failed/None/Operational/Degraded/Failed for exact inputs
VA=0xDEADBEEF,type=0x80000001. Count 1 versus status length 5 intentionally
mismatches. Generation B returns False/True, only type 42 count 4, and
Degraded/None/Degraded for the now-absent requested type. Generation C is empty;
another generation verifies SIZE_MAX count widening. Earlier copied values
remain unchanged after newer snapshots close and after provider teardown.

Native tests cover null/malformed arguments and outputs, Views/Close,
all four methods' std/unknown/bad_alloc exceptions, unknown entry after valid
ones, four owner/copy failpoints (copy fails after one entry), later recovery,
E1/E3/Claim independence and parent-first queries. E2 tests assert all four new
counters remain zero during notification adaptation. No arbitrary sleeps.
The isolated native and Ada focused processes have no callback registration,
Job or ProductRx; test dlopen references release before VA teardown. Logs
require VA destruction -> C2 shutdown -> C2 destruction -> library unload,
then copied values are read (native snapshots also View/Close after unload).

## Pinned Squall evidence model

Squall revision `b1015728f904c799fa0c07489fce48e78f67845f` was rechecked.
`interfaces/squall-rf-mel-impl/src/SquallC2MEL.cc:488..498,530..537`
returns False, False, empty map, empty vector. Safe Ada VA integration requires
those results before and after public C2 Close and retains values through VA
Close. Production C ProductRx integration queries all four and Views/Closes
copied empty owners after public VA/C2 Close. Existing E1/E2/E3,
Job/JobInterval/status/extension/ProductRx assertions remain. These integration
processes have callback pins and are not DSO-unload evidence.

This is required-capability Boolean and empty LF-query evidence, **not positive
Squall LF support**. No Weights/TX operation is called as a consequence of False.

## Still deferred

VirtualAperture is not complete: VA-level DataPipes, association mutation,
descriptor-based command creation, static/dynamic Weights resources, TX
radiated/peak/gain/attenuation calculations, CachedWaveform allocation,
WaveformTxEndpoint/dynamic TX endpoints, external/RDMA, VADB and full TX
JobRequest/TransmitEvent support remain outside this task.

## Validation

Actual final command results and hosted/source-head details are recorded below
after validation; implementation or source inspection is not a runtime pass.

| Command/audit | Actual result |
|---|---|
| make test-native | 271/271 |
| make test-build-isolation | PASS |
| make format-ada; make check-ada-format | PASS |
| alr -C ada build | PASS |
| alr -C ada/tests run | PASS, including new isolated public LF test |
| make test-rust | PASS |
| cargo check --manifest-path rust/Cargo.toml --workspace | PASS |
| cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings | PASS |
| cargo fmt --manifest-path rust/Cargo.toml --all -- --check | PASS |
| make test-python | 118/118 |
| aggregate make check (Alire exec environment for direct GPR tools) | PASS |
| make test-squall-rf-ada-va | PASS, exact False/False + empty LF, parent-first repeats |
| make test-squall-rf-ada-job | PASS, unchanged two-Job lifecycle/extension assertions |
| make test-squall-rf-rx | PASS, exact False/False + empty LF; 8 ComplexINT16 events, zero drops/malformed/allocation failures |
| make test-squall-rf-ada | PASS, safe Ada ComplexINT16 receive |
| git diff --check | PASS |
| Native focused CTest --repeat until-fail:50 | 50/50, no Not Run |
| Safe Ada focused executable, 50 isolated invocations | 50/50 |
| GCC 14/Clang 19 exact vendored-header signature/type compilation | PASS, warnings-as-errors |
| GCC/Clang closure checks | 712/713 headers, unchanged pinned closure |
| C Squall ProductRx and Ada Squall VA integration compilation | PASS |
| Fresh production Release /tmp/ams-034e4/release | 180 exact function exports under AMS_MEL_0.1, no test-only exports |
| Baseline declaration/record comparison | All original 172 signatures and 160 record definitions unchanged |
| Vendor SHA-256 / inventory | 804/804 unchanged; no vendor diff |

Development failures were preserved: the first focused test used a fixture
that forbids descriptor queries; the test now uses the existing descriptor
fixture for E3 independence. Python initially retained two 172-entry inventory
expectations; these were synchronized to the exact eight additions and the
unchanged public safe Python surface. Neither was a product semantic workaround.
No legacy Job-abandonment timeout has been observed in completed local runs;
this is not a claim that the previously observed reliability issue is fixed.

The integration matrix used isolated control/metrics/health/data ports (control
24203/25203/26203/27203). All four targets passed at the exact Squall pin.
Actual VA assertions include
False/False and empty catalog/status before and after public C2 Close, with
copied Ada values still readable after VA Close. No unrelated user container
was replaced or stopped. C ProductRx additionally asserted exact False/False,
empty LF map/vector and copied native views after public VA/C2 Close. Its
ProductRx receive counters were 8 received/8 queued, zero drops/malformed/
allocation/after-close. Safe Ada ProductRx passed unchanged. No environmental
port failure or legacy Job-abandonment timeout occurred; no causal fix claimed.

Checkpoint commits: `76bdd2da71afffbbf2a855d73bf03ceb031e09b6`
(native/raw parity) and `297d0bd3b3dde3bdf90542715cde63a1ca72e63b`
(safe Ada). Final integration/evidence commit and hosted validation are recorded
in the PR and final task report; no amend or force push is used.
