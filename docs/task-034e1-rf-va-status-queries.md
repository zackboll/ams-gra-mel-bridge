# Task 034E1 — Live RF VA queries and owned instance-status reports

## Baseline and scope

Started from fetched `origin/main` `82e1e008fca3befe6066f31d4a727d65e2de34e7`,
the merge commit of PR #68, on clean new branch
`feature/034e1-rf-va-status-queries`. Native baseline measured 257/257.
RF MEL pin `762ce84c5555dd0f3ea66f36b321fecf8839b89f`; Squall pin
`b1015728f904c799fa0c07489fce48e78f67845f`. Vendor declarations already suffice;
no vendor tree changes. ABI remains 0.1, starting production inventory 154.

This slice represents all six BaseVirtualAperture synchronous read/query methods,
not the whole BaseVirtualAperture/VirtualAperture interface. No callback, worker,
future, new sibling claim, VADB/unclaimed discovery, ElementGroupDescriptor/
DataPipe expansion, standalone LF query, weights/TX-power calculation, broader
JobRequest/conditional/TX, RDMA or additional ProductRx format is added.
No safe Rust or public Python query API is added.

## Exact newline checkpoint

Commit `400ff95` (`Restore final newlines in RF task evidence`) repairs only:

| Path under docs | Original bytes | New bytes | Original SHA-256 |
|---|---:|---:|---|
| task-034b2b1-native-rf-job-lifecycle.md | 3025 | 3026 | 5520b6e78a77645f4e60fc786d4f7d21feb8162a6ebb76aed4147b082230c923 |
| task-034b2b2-safe-ada-rf-job-lifecycle.md | 1948 | 1949 | 610483cec5f7d46f32484dde018025992ca4a0903c0cbad27e8737ee5343a736 |
| task-034c1-rf-duration-quantization.md | 2623 | 2624 | 94d6aab8c86718123c469b1703d4b587e14a5bb7233932ce84cb9fced23af8e6 |
| task-034c3-rf-tx-power-modes.md | 8475 | 8476 | 868e309a4078701b31ac8d6e7abc0f58a37e40f448360d3eee24e8bb511c33de |

For every file, Python asserted `new == original + b"\n"`. No other bytes,
whitespace or line endings changed. The existing final-newline checker passed.

## Ten additive C exports

1. `ams_mel_rf_virtual_aperture_get_id`
2. `ams_mel_rf_virtual_aperture_get_status`
3. `ams_mel_rf_virtual_aperture_get_instance_status`
4. `ams_mel_rf_virtual_aperture_get_all_instances`
5. `ams_mel_rf_virtual_aperture_get_instances`
6. `ams_mel_rf_va_instance_list_view`
7. `ams_mel_rf_va_instance_list_close`
8. `ams_mel_rf_virtual_aperture_get_instance_status_report`
9. `ams_mel_rf_va_instance_status_report_view`
10. `ams_mel_rf_va_instance_status_report_close`

Distinct `uint32_t` VirtualApertureStatus has None=0, Operational=1, Degraded=2,
Failed=3, each statically checked against upstream. Known statuses are OK data;
unknown direct/top-level/nested values fail closed with PROVIDER_FAILED. The
whole malformed report is rejected, never partially published.

Each explicit query calls only its corresponding provider method exactly once.
IDs/FaceID retain all uint32 bits; no membership precheck, cache, reconciliation,
sorting or deduplication. Claim still reads only getVAInstanceIDs (set),
getElementGroupLabels and isSingleGroup; frozen C info/Ada snapshot accessors
are unchanged. getAllInstances is the distinct vector method, not that set.

List snapshots own copied numeric vectors and preserve empty success {NULL,0}.
One report is the copy of one provider-returned value: its three getters are
each used once, with getLFStatus's BY-VALUE map stored locally before iteration.
Groups retain ascending map-key order, exact type IDs, empty entries, status
vector order/length/repetition. Positions are LF-instance ordering, not new IDs.
The returned instance ID is authoritative even when different from the request.
Independent direct status/list/report calls may disagree and are not an atomic
multi-call transaction. No getLocalFunctions/getLocalFunctionStatus enrichment.

Final-sized primitive backing storage is completed before publishing nested
spans. Partial allocation/decoding storage is destroyed by private unique owners.
Views allocate/call no provider; destruction uses only bridge primitive vectors.
Snapshots retain no provider, C2 claim or library pin and survive full teardown.
The public VA's existing claim keeps synchronous queries usable after C2 Close;
same-owner external serialization with VA Close is required. No C2/request mutex
is held during provider calls and rf_va_acquire_job_parent is not used.

Arguments are validated before provider calls. Scalars/views are unchanged on
failure; creation output must be non-null and initially null. Existing owners
are never overwritten. bad_alloc maps to INTERNAL_ERROR; other standard/unknown
exceptions to PROVIDER_EXCEPTION. Failures do not poison later explicit calls.

## Safe Ada

`AMS.MEL.RF.C2.Virtual_Aperture_Queries` reuses Virtual_Aperture'Class and exact
Interfaces.Unsigned_32 signatures. Explicit-representation Status_Kind is
separate from Job/request/MFA types. Query_ID/Query_Status/Query_Instance_Status
are fresh scalar calls. Snapshot_All_Instances/Snapshot_Instances and
Snapshot_Instance_Status_Report return ordinary private Ada values; list/report/
group values have no native handle, Close, Finalize or public System.Address.
Accessors distinguish LF TYPE count from individual status count, using Positive
indices and normal Constraint_Error bounds checks.

A private controlled temporary acquires/views/validates/copies/closes native
snapshots before returning, with nonraising exception cleanup. Copy checks
Natural, container Count_Type, Storage_Offset arithmetic, non-null nonempty
data, address overflow/alignment and every enum before dereferencing/copying.
Fixed 512-byte diagnostics are used without automatically repeating a live
query for a longer message. Native failures become Provider_Error; known
Failed/Degraded return normally. Closed VA queries fail while prior values remain.

## Evidence design

The existing MockVirtualAperture supplies six separate counters/exact input
observers. Claim-time set stays {0,3,9}; live vector is [UINT32_MAX,7,0,7], selected
face 0xFEDCBA98 is [42,2,42], unknown face is empty, and getID is 0x87654321,
not submitted 0xFEDCBA98. Explicit deterministic state changes yield all four
scalar statuses, later vectors/reports, and unchanged earlier snapshots.

Report request 0xDEADBEEF returns ID 42 and Degraded while direct status is
Failed. LF keys are inserted out of order and exposed as 0,0x80000001,UINT32_MAX.
Their vectors are [Failed,None,Degraded,Operational], [], and
[Operational,Operational,Failed]. A later malformed last group follows valid
groups and rejects the whole owner. Concrete getters are nonvirtual: source and
actual-vendored-header probes prove exact signatures/by-value getter use, not
fabricated mock getter interception.

C11 and safe Ada suites cover freshness, fidelity, independent values, standard/
unknown/allocation provider exceptions for every query, unchanged/null outputs,
no calls on invalid arguments, recovery, native construction failpoints including
after the first LF group, long fixed diagnostics with exactly one call, parent-
first queries and closed-owner rejection. The isolated no-registration C process
releases its own dlopen reference before VA Close and requires VA destruction ->
C2 shutdown -> C2 destruction -> library_unloaded while list/report views survive.
ProductRx's callback-pinned process is not used as unload evidence.

## Pinned Squall values and limits

Rechecked SquallC2MEL.h/.cc at the exact pin before integration. Zero fixtures
produce getID=0, getStatus=Operational, getAllInstances=[0], getInstances(0)=[0],
getInstances(UINT32_MAX)=[], getInstanceStatus(0)=Operational,
getInstanceStatus(UINT32_MAX)=Failed. Reports preserve requested 0/UINT32_MAX,
corresponding Operational/Failed and empty LF map. Failed is successful data.
The provider internally computes report status via its own direct implementation;
the bridge calls only getInstanceStatusReport for report creation.

The Ada VA client exercises all six while open and after public C2 Close, then
retains Ada values after VA Close. The helper-free C ProductRx client retains
native list/report across public VA/C2 Close. Existing ProductRx data/counters,
Job lifecycle, interval status timeout/stop and extension checks are preserved.
These are positive query-value results, not hardware health, real dynamic status
transitions or callback-delivery evidence.

## Local validation results

All build-driving targets using shared directories were run sequentially. Long
commands wrote evidence logs under `/tmp/ams-mel-034e1-*`; independent fresh
audit builds used separate `/tmp` trees. Actual baseline is native 257/257,
Python 114/114, production exports 154, ABI 0.1, vendor checksums 804/804.
The baseline Python check was rerun against an archived starting revision and
its separate test-enabled library/providers (production builds intentionally
lack test hooks).

| Command / audit | Result |
|---|---|
| `make test-native` | 258/258, GCC Debug, warnings as errors |
| Separate fresh Clang Release configure/build/CTest | 258/258, warnings as errors |
| `make test-build-isolation` | Pass, production/test trees and failpoints distinct |
| `make format-ada`; `make check-ada-format` | Pass, pinned GNATformat |
| `alr -C ada build` | Pass, GNAT 16.1.0 |
| `alr -C ada/tests run` | Pass, full smoke including isolated query suite |
| `make test-rust` | Pass: 71 ordinary tests plus 4 compile-fail doctests |
| `cargo check --manifest-path rust/Cargo.toml --workspace` | Pass |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | Pass |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Pass |
| `make test-python` | 115/115 plus compileall |
| `git diff --check` / final-newline checker | Pass |
| GCC/Clang actual-vendored-header probe + closure | Pass, 712/713 pinned dependency headers |
| Native `rf-va-live-queries` repeat until-fail:50 | 50/50 (also Clang Release 50/50) |
| Isolated safe Ada query suite, 50 process runs | 50/50 |
| `make check` | Pass, including direct-GPR Ada and repaired final-newline gate |
| `make test-squall-rf-ada-va` | Pass, all six before/after C2 Close, values after VA Close |
| `make test-squall-rf-ada-job` | Pass, preserved two-Job/parent-first/status/extension checks |
| `make test-squall-rf-rx` | Pass, query/list/report values and preserved 8-event receive checks |
| `make test-squall-rf-ada` | Pass, preserved safe Ada receive checks |

Rust tests used `AMS_MEL_NATIVE_LIB_DIR` pointing at `native/build-tests/lib`
and `AMS_MEL_TEST_PROVIDER_DIR` at `native/build-tests/test-providers` via the
repository target. Direct-GPR aggregate and Squall builds used the local GNAT
13.2.1 and GPRbuild 22.0.1 toolchain directories prepended to PATH. Ordinary
tests remain provider/container/credential/hardware/download-free.

The four Squall runs used distinct control/couloir-health/metrics/data port sets:
VA 24203/24318/24313/24314/24601; Job 25203/25318/25313/25314/25601;
C RX 26203/26318/26313/26314/26601; Ada RX
27203/27318/27313/27314/27601. Reusing the VA set initially found health port
24313 occupied; the existing fail-closed preflight rejected it, and retrying
with a separate isolated set passed. No unrelated container was stopped.
The C receive run observed eight 4096-element ComplexINT16 events, seven later
events differing from A, and counters received=8/queued=8 with zero drops,
malformed/allocation/after-close events. These are that run's observations,
not a new generic data-size/rate guarantee.

Fresh starting/final production Release trees measured dynamic symbols with
`nm -D --defined-only`: 154 -> 164, exactly the ten additions above, all earlier
symbols present, export-map parity, and no test-only symbols. Runtime ABI query
remains 0.1. Source comparison preserved all original C export declarations and
record definitions; the Claim function is byte-identical to starting main.
SHA-256 audit remains 804/804 and vendor/checksum-manifest diff is empty.
Public behavior/docs and raw ABI/layout/signature/inventory parity are updated;
safe Rust/public Python surface is unchanged.
