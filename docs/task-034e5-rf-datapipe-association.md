# Task 034E5 — RF VA DataPipe connections and endpoint association

## Baseline and exact scope

Starting fetched origin/main: `dfebb58a408c35c399d7b9011490c82726115fa2`.
PR #72 was verified merged at that SHA; no equivalent branch/PR or open PR
existed, and the worktree was clean. Created
`feature/034e5-rf-datapipe-association` directly from origin/main. Measured before
any edits: native 271/271, Python 118/118, production exports 180, vendor
SHA-256 804/804, pristine aggregate `make check` passing. Logs:
`/tmp/ams-mel-034e5` (development logs retained, not release artifacts).

RF MEL pin: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Squall pin: `b1015728f904c799fa0c07489fce48e78f67845f`.
No vendor changes/dependencies. This slice represents exactly VA::getDataPipes
and DataPipe getLabel/getAssociatedEndpoints/associateEndpoint/associateEndpoints.
E3's optional ElementGroupDescriptor::getDataPipes is DISTINCT and unchanged.

## ABI 0.1

Exactly five new production exports:

- `ams_mel_rf_virtual_aperture_get_data_pipes`
- `ams_mel_rf_va_data_pipe_connections_snapshot_view`
- `ams_mel_rf_va_data_pipe_connections_snapshot_close`
- `ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint`
- `ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints`

New opaque bridge-only owner:
`ams_mel_rf_va_data_pipe_connections_snapshot`.
New public records:
`ams_mel_rf_va_data_pipe_group_v1`,
`ams_mel_rf_va_data_pipe_group_span_v1`,
`ams_mel_rf_va_data_pipe_connections_snapshot_v1`.
Existing E3 data_pipe_info/span and u64 span reused without modification.
Frozen-baseline comparison: all original 180 declarations and 162 record
definitions unchanged. The final Release export count must be measured, not
inferred from the five planned additions.

## Returned values and deterministic fidelity

Each snapshot invokes VA::getDataPipes exactly once and each pipe's label and
endpoint getter once per represented occurrence. Outer unordered entries sort
lexicographically by unsigned UTF-8 key bytes, no locale/normalization/casefold.
Inner std::map traversal is preserved exactly; endpoint sets ascend unsigned
numerically, including 0, 0x8000000000000000 and UINT64_MAX. Lookup map keys and
returned labels remain distinct. Shared-object aliases preserve every map
occurrence, never object identity/deduplication. Empty outer map, inner map and
endpoint set succeed with {NULL,0}. Empty UTF-8 strings are valid.

Every outer/inner key and returned label must be complete UTF-8 without embedded
NUL. Malformed strings/null pipes reject the whole owner (PROVIDER_FAILED), no
partial publication. All bridge backing strings/vectors reach final size before
any public pointer publication; no subsequent growth/sort/move invalidates views.
Published owners retain only bridge strings, primitive vectors and C records,
not provider maps/shared_ptrs/sets, VA, C2 claims or library pins. View/Close
allocate/call no provider and survive actual DSO unload.

## Thick synchronous commands

Commands validate and copy all inputs before the provider query, then obtain
their own VA::getDataPipes value, select the exact outer/inner lookup keys and
invoke the exact mutation once. The upstream outer wrapper has no find method;
iterator exact-key search avoids operator[] insertion. Missing outer key, inner
key or null target is PROVIDER_FAILED with a diagnostic, not INVALID_ARGUMENT,
and invokes no mutation. All endpoint bits forward exactly. Multi-endpoint input
becomes std::set BEFORE the query: ordering insignificant, duplicates collapse,
empty input valid and associateEndpoints called once, never a single-ID loop.

Provider false/true maps to OK with accepted 0/1. This means ONLY that the exact
mutation returned false/true. No retry, readback, capability auto-gate, one-shot
cache, persistent pipe handle or association-persistence promise. All failures
leave association output untouched. Invalid public inputs call no provider;
bad_alloc maps INTERNAL_ERROR, other standard/unknown exceptions
PROVIDER_EXCEPTION, malformed provider snapshot/lookup state PROVIDER_FAILED.

Provider temporaries die before the synchronous command returns. Only existing
live VA ownership is used: no sibling claim, worker, callback or DSO pin. Provider
calls hold no lifecycle/notification/Job/queue mutex. Same-VA calls including Close
are externally serialized. Public C2 Close does not prevent E5 while VA is open;
after VA Close new calls fail, previously copied snapshots remain valid.

## Safe Ada and raw parity

`AMS.MEL.RF.C2.Data_Pipes` exposes ordinary Connection_Snapshot,
Element_Group_Connection and Data_Pipe_Connection values, Snapshot, Group_Count/
Group_At, Element_Group_Label, Pipe_Count/Pipe_At, Lookup_Label/Label and
Endpoint_Count/Endpoint_At, with Positive indexing and exact Unsigned_64 IDs.
Associate_Endpoint/Associate_Endpoints return provider Boolean data without
raising on false. Embedded NUL inputs raise Constraint_Error before native entry;
C validates UTF-8. Endpoint arrays preserve numeric values, including empty
arrays; native conversion provides upstream set semantics.

Checked native View copy-out validates Natural/Count_Type representability,
nonnull/alignment and Storage_Offset/address arithmetic for all levels. A private
controlled temporary closes the native owner on exceptions and explicit success
before returning Ada-owned containers. No public address/C pointer/owner or
provider-state finalizer. Private derived C_Pass_By_Copy string/span types serve
by-value imports without changing existing canonical raw records. Rust sys and
private Python track all types/functions and C-compiled signature/layout/inventory
probes; no safe Rust or public Python DataPipe API.

## Mock and lifecycle evidence

The existing MockVirtualAperture uses E5 counters distinct from E3 pipe/descriptor
counters. Claim does not query E5; E3 does not query E5; E5 does not query E3/E1/E4.
E2 notification tests require zero E5 getter/mutation calls during callback flow
and retain their existing intentional DSO-pin negative control.

The dedicated C test covers canonical ["", "rx/main", "µ-group"], inner order,
distinct/Unicode/empty/long labels, complete endpoint sets, aliased occurrences,
persistent-provider alias changes versus fresh-object state, exact single-ID
forwarding, duplicate-collapse/empty-set calls, repeatable false results, lookup
failures, UTF-8/NUL/null data, all five method exception kinds, four bridge
allocation failpoints and recovery, changing keys/labels/endpoints and independent
old snapshots. No arbitrary sleeps. C and public Ada tests release their own
dlopen reference and require VA destruction -> C2 shutdown -> C2 destruction ->
library unload, with copied snapshot values checked afterward, in isolated
processes with no E2 registration/Job/ProductRx/permanent callback shell.

## Exact Squall limit

Rechecked `SquallC2MEL.cc:125..142,467..471` at the exact pin. VA getDataPipes
constructs a fresh SquallDataPipe per invocation under configured RX group `0`,
inner/returned label `default`, initially empty associated-endpoint set. Single
and set mutations insert into that object's local set and return true. The safe
Ada VA and C ProductRx integration assert exact returned values and mutation
true, then explicitly obtain a NEW still-empty snapshot. This does not invalidate
mutation-call evidence: the bridge intentionally retains no pipe across calls.
Ada repeats after public C2 Close; retained C/Ada snapshots survive public VA/C2
Close. Existing E1/E2/E3/E4/Job/interval/status/extension/ProductRx assertions stay.
Callback-pinned Squall integration is NOT actual DSO-unload evidence.

Evidence is precisely **real VA getDataPipes returned-value evidence + real
DataPipe mutation method return-value evidence**. No persistent routing, RDMA
setup/Q-pair creation, external endpoint connectivity or hardware-route claim.

## Validation record

Baseline commands and checks above passed before edits. Final command results,
repeat counts, measured Release audit, commits and hosted source/merge checkout
details are recorded after execution; implementation is not a runtime pass.

Development failures retained: C fixture `pipe` collided with POSIX pipe(); its
mechanical rename also changed two fixture strings, corrected without weakening
tests. Ada unused use-clause warning fixed rather than suppressed. GDB diagnosed
the initial Ada by-value FFI convention mismatch; private derived by-copy types
correct the calling convention without changing frozen public records.

Deferred: equality operators, descriptor-based/general ElementGroupCommand
owners/builders, Weights, TX power, CachedWaveform, WaveformTxEndpoint,
ExternalEndpoint/RDMA, VADB, TX JobIntervals/TransmitEvent, extra ProductRx formats.
No such expansion started; no vendor closure or provider ownership expansion.

## Completed local validation

| Command/audit | Actual result |
|---|---|
| make test-native | 272/272 |
| make test-build-isolation | PASS |
| make format-ada; make check-ada-format | PASS; final format check repeated |
| alr -C ada build | PASS |
| alr -C ada/tests run | PASS, including public E5 isolated unload test |
| make test-rust | PASS |
| cargo check --manifest-path rust/Cargo.toml --workspace | PASS |
| cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings | PASS |
| cargo fmt --manifest-path rust/Cargo.toml --all -- --check | PASS |
| make test-python | 119/119, final corrected inventory run |
| aggregate make check (Alire exec toolchain environment) | PASS |
| Native E5 CTest --repeat until-fail:50 | 50/50, no Not Run |
| Public safe Ada E5, 50 separate invocations | 50/50 |
| GCC 14 / Clang 19 actual vendored header probes | PASS, warnings-as-errors |
| GCC / Clang header closure audits | 712 / 713 pinned headers, unchanged |
| Fresh production Release /tmp/ams-mel-034e5/release | 185 measured function exports, exact map/declaration parity, no test symbols |
| Frozen comparison / ABI | Original 180 signatures, 162 records unchanged; ABI 0.1 |
| Full vendor SHA-256 / diff audit | 804/804 unchanged; no vendor diff |
| make test-squall-rf-ada-va | PASS, E5 exact values/mutation true/fresh state/parent-first |
| make test-squall-rf-ada-job | PASS, preserved two-Job lifecycle |
| make test-squall-rf-rx | PASS, E5 exact values/both mutations true/copied view after Close |
| make test-squall-rf-ada | PASS, preserved safe Ada ComplexINT16 receive |
| git diff --check / final newline check | PASS |

Squall used isolated control ports 28203/29203/30203/31203 with distinct matching
metrics/health/data port sets. No port/TIME_WAIT failure occurred; no unrelated
container was replaced/stopped. The unrelated `opencv-imgproc-publish-010`
container remained. C ProductRx received/queued 8 events with zero drop,
malformed, allocation or after-close counts. No legacy Job-abandonment timeout
was observed in these completed local validations, and no fix to that unrelated
path is claimed.

The initial Python matrix retained one additional common-channel inventory
expectation at 180; corrected to 185, public exports unchanged. An attempted
immediate rerun while aggregate recreated the test tree failed to load the absent
library; final make test-python was run after tree writers finished and passed.
The first Release audit script incorrectly expected a literal ABI macro rather
than existing UINT32_C syntax; corrected audit checked the actual unchanged
macros and completed successfully. These first failures remain in the logs.

Normal commits, final source/remote/PR equality and both hosted workflow checkout
SHAs/results are reported in the PR/final report after execution. No amend,
force-push, merge or auto-merge is authorized/performed.
