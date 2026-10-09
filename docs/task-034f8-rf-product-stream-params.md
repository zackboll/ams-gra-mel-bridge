# Task 034F8 — RX JobInterval ProductStreamParams

## Scope and pinned contract

Starting origin/main: `4ac2cdb2fdc216f1dcf789f1c1853af67663ec19`, PR #82 merged.
Initial worktree clean on F7; no equivalent remote branch or open PR existed.
Branch: `feature/034f8-rf-product-stream-params`.
Measured baseline: native 280/280, Python 127/127, Release exports 196,
vendor SHA-256 804/804. Evidence is retained in
`/home/zboll/task-evidence/034f8` outside the repository.

RF MEL pin `762ce84c5555dd0f3ea66f36b321fecf8839b89f` declares
`EndpointID = uint64_t` in RFMELTypes.h. ProductStreamParams.h publishes:

- EndpointParameters setters taking EndpointID, uint64_t start address and
  uint64_t maximum bytes; const getters return these scalar values. Defaults
  are all zero.
- `setApplicableRxElementGroups(const std::vector<size_t>&)` and const getter
  returning `const std::vector<size_t>&`.
- `setEndpoints(const std::vector<EndpointParameters>&)` and const getter
  returning `const std::vector<EndpointParameters>&` (also a mutable overload).
- JobInterval.h: `setEndpointParameters(const ProductStreamParams&)` copies;
  const getter returns `const ProductStreamParams&`. Both vectors default empty.

Actual-header static signatures and runtime copy/move probes select the const
overloads explicitly and retain the F4 inconsistent applicable-group probe.

## Additive C ABI

Measured LP64 alignment is 8 for every new record:

| Record | Size | Offsets |
| --- | ---: | --- |
| product_stream_endpoint_v1 | 24 | endpoint_id 0, start_address 8, max_bytes 16 |
| product_stream_endpoint_span_v1 | 16 | data 0, size 8 |
| product_stream_params_v1 | 32 | applicable_rx_element_groups 0, endpoints 16 |
| job_interval_config_v7 | 216 | interval 0, has_product_stream_params 176, product_stream_params 184 |
| job_interval_config_span_v7 | 16 | data 0, size 8 |

Names above have the `ams_mel_rf_` prefix. V7 nests frozen 176-byte v6,
which nests frozen v5; no prior record is redefined. ReceiveEvent v1–v4 and
JobInterval v1–v6 declarations and signatures remain frozen. ABI remains 0.1.
Exactly one production function is added:

```c
ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v7(
    ams_mel_rf_job *job,
    ams_mel_rf_job_interval_config_span_v7 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required);
```

## Presence, validation and fidelity

Presence 0 ignores the complete payload, including poisoned pointers, overflowing
counts and unrepresentable inactive indices; it does not call the setter.
Presence 1 validates both spans and multiplication overflow, requires non-NULL
data for nonzero length and checks every uint64 group index with fits_count
before converting to size_t. Other presence encodings reject INVALID_ARGUMENT.
NULL/zero is valid. Empty, groups-only and endpoints-only values are structurally
accepted, without a claim of provider-semantic validity. Explicit empty values
call the setter once; absent values retain untouched upstream defaults.

Order and duplicates are preserved in both vectors. Group indices are not labels
and are not checked against a JobRequest group count. SIZE_MAX is accepted;
indices above SIZE_MAX reject on narrow hosts. Endpoint IDs, numeric start
addresses and maximum bytes forward the complete uint64 domain exactly, including
zero, one, high bit and UINT64_MAX, without clamp, normalization or narrowing.

The upstream endpoint vector MUST match the relevant JobRequest ElementGroup
endpoint-ID vector in size and each corresponding ID, relative to applicable RX
groups. The claimed Job owner is not an authoritative live representation of
these associations: the bridge does not query, sort, deduplicate, infer one
endpoint per group, compare or repair input. Caller/provider responsibility is
authoritative. Structural C acceptance is not a valid hardware destination proof.

StartAddress is a numeric starting virtual address, NOT a C pointer, Ada access
value, owned host buffer, registered region or DMA mapping. The bridge performs
no dereference, pointer arithmetic or `startAddress + maxBytes` calculation.
Application/provider must ensure valid registered writable memory, capacity and
execution lifetime where a real provider consumes these parameters. Safe Ada
value configuration does not guarantee hardware memory safety.

## Construction, errors and lifetime

The existing templated builder prepares all v6 fields, LF commands and locally
copied ProductStreamParams before one JobDetail::addJobIntervals mutation.
V7 status is read from the doubly nested v5 record. Invalid final intervals reject
the entire batch before Add. There are no MFA/capability/quantization/VA/data-pipe/
LF/endpoint/ExternalEndpoint/RDMA/VADB queries during preparation.

Borrowed C spans expire after synchronous Add; copied C++ values retain the
original values after caller mutation. No new owner, worker, thread, callback,
DSO pin or persistent caller pointer is introduced. Never needs no status stream;
Always/OnException still require a locally usable registration. Existing
Finalize/full-Cancel guards, Flush, CancelRemaining, ExtendEvent, parent-first
cleanup and externally serialized same-Job operations remain unchanged; no
implicit Cancel or automatic retry occurs.

Malformed inputs map INVALID_ARGUMENT. Bridge construction failures and bad_alloc
(including provider Add bad_alloc) map INTERNAL_ERROR; provider std/unknown
exceptions map PROVIDER_EXCEPTION. Test-only
`interval-product-stream-allocation` deterministically fails preparation before
Add. Later explicit same-Job recovery is supported.

Safe Ada owns ordinary copyable vectors, with independent parameter/interval
copies, replacing Set, absent Clear and re-Set semantics, and no manual Close.
Private serialization uses final-sized v7, LF, event, pulse, spatial, label,
activity and polarization backing, plus distinct event-group and stream-group
arrays/cursors, all live through synchronous Add. Raw Ada/Rust/private Python
mirror the five records and one export. No safe Rust or public Python F8 API.

## Squall boundary and deferred scope

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` Add is a no-op.
The safe Ada integration uses group 0 and endpoint ID/address/maxBytes 0/0/0.
Normal return establishes acceptance of bridge-constructed configuration only,
not matching, writable memory, registration, capacity, routing, DMA/RDMA,
delivery or hardware buffer lifetime. Mock getter observations establish positive
value fidelity, not operational destinations.

Deferred: JobInterval::setEndpoints mapping, inconsistent
JobInterval::setApplicableElementGroups, TX intervals/TransmitEvent, Weights,
Modulation, user context/std::any, standalone ExternalEndpoint, host memory
registration, allocation/mapping, DMA/RDMA setup and VADB. None is added here.

## Validation record

Initial complete native implementation run: 281/281, including the focused C
fixture's internal 50/50. A foreground build command exceeded the execution tool's
30-second limit; its incomplete log is preserved and is not counted as a pass.
The subsequent complete background run passed. LP64 C layout output matches the
table; `cc -m32 -fsyntax-only` passes (compile-only, no 32-bit runtime claim).
GCC and Clang 19 actual-header compile/runtime probes pass; closures are 712/713
pinned headers. An initial manual probe used the wrong Boost include root; its
failure log is preserved, and the corrected pinned-root run passed unchanged
source. Fresh Release audit: 197 exports, all original 196 retained, only v7 added,
export-map parity and no test-only production symbols; runtime ABI 0.1.
All 208 prior struct declarations and 196 function declarations remain byte-identical.
Vendor checksum audit remains 804/804.

Alire build and complete Ada suite passed, including safe Ada F8 50/50, F2–F7
and ProductRx compatibility. Rust workspace tests pass, raw sys ABI 25/25;
workspace cargo check, Clippy warnings-as-errors and rustfmt check pass.
Python passed 128/128. Build-tree isolation passed with complete 281/281 suites.
Further final validation results are recorded after completion of the remaining gates.

### Completed integration and aggregate gates

`make check` passed on the final implementation, including direct-GPR Ada,
F8 50/50 and all older suites. Native and safe Ada F8 focused final-source
executables both passed 50/50. All four pinned Squall targets passed:

| Target | Control / Couloir metrics / health / RF metrics / data ports |
| --- | --- |
| safe Ada VA | 31203 / 31318 / 31313 / 31314 / 31601 |
| safe Ada Job | 32203 / 32318 / 32313 / 32314 / 32601 |
| direct C ProductRx | 33203 / 33318 / 33313 / 33314 / 33601 |
| safe Ada ProductRx | 34203 / 34318 / 34313 / 34314 / 34601 |

Job accepted bridge-constructed ProductStreamParams; no data-plane claim.
Direct C ProductRx retained eight products, zero drops/malformed/allocation
failures; safe Ada retained its eight-product compatibility assertions. No
unrelated container was stopped and no TIME_WAIT workaround was needed.

An initial repeat-50 attempt after provider-only builds encountered a partial
native test tree and missing executables (Not Run). That log is preserved,
not counted as passing. A complete task-owned isolated Debug tree was rebuilt
before the replacement parallel repeat-50 run. No source or test contract was
weakened; vendor, IR implementation and PR #78 abandonment test are unchanged.
Disk-backed task-owned TMPDIR avoided interference with unrelated /tmp contents.
A manual link attempted during tree reconfiguration also failed for a temporarily
missing library; final isolated focused executables subsequently passed.

Normal implementation commits: `c5ae867f95e1b97b45f06ce4d6e74ce5a1bf9620`
(native/raw), `c84bc43dad0735c0bae5f26a0a4dc1652698d2db` (safe Ada),
`aabd130b5c5aa229a73e2bb2866c9ab60719a956` (stronger nested/batch tests),
`c7c4313294f8e243201e5cf9c4d574b6b1736228` (contract/integration docs).
PR: https://github.com/zackboll/ams-gra-mel-bridge/pull/83, open/non-draft,
unmerged and auto-merge disabled. Publication/hosted final results are recorded
on the PR; literal push source and PR synthetic merge checkouts are distinguished.
