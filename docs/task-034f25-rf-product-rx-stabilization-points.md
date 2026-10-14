# Task 034F25 — ProductRx stabilization-point receive metadata

## Source gate and baseline

Fetched origin and independently verified PR #99 MERGED and actual starting
`origin/main` **ffd3c547f60924fa14075c3579a58b4625b3551d**, including F24 and corrective
commit **7c427d986035b4aa2bc63a091ce5a7fe05144ce7** in its ancestry. The supplied
repository path did not exist on this host; `/home/zboll/git/ams-mel` has the
expected `zackboll/ams-gra-mel-bridge` remote. A new isolated worktree was created
at `/home/zboll/git/ams-mel-034f25` on
`feature/034f25-rf-product-rx-stabilization-points`, directly from that main.
Other worktrees and their state were not changed.

Verified RF MEL pin: **762ce84c5555dd0f3ea66f36b321fecf8839b89f**.
Verified Squall pin: **b1015728f904c799fa0c07489fce48e78f67845f**.
Vendor SHA-256 baseline: **804/804**. Pristine native baseline: **431/431** in
each GCC Debug/Release and Clang 19 Debug/Release configuration. Python **144**;
Rust **97** unit/integration plus **four** separately run doctests; Alire library,
full Ada suite, aggregate `make check`, direct-GPR and build isolation pass.
Production Release baseline: **220 AMS_MEL_0.1 exports**, no test hooks.

Evidence lives under `/home/zboll/git/ams-mel-034f25-build`. Host `/tmp` was
already full at task entry. From the baseline onward, inherited hard-coded
`/tmp` fixtures run in a private user/mount namespace bound to task-owned
disk-backed storage. TMPDIR also points there. No unrelated host files were
deleted. Failed development logs are retained separately from qualification.

## Exact pinned contract

`ProductRxMetadata.h` defines the by-value getter `getStabPoints()` for a
nonoptional `std::vector<PointingType>`. `RFMELTypes.h` defines exactly
`std::variant<ECEFPointing, LLAPointing, PlatformRelativePointing,
FaceRelativePointing, BaselineRelativePointing>` in that order. Production
`rf_product_rx.cpp` has `is_same_v` static assertions for both complete types;
the variant assertion establishes the five alternatives and their order.

These are facts about executed collection, not new Job commands. They are not
the separately commanded estimated stabilization point or expected RX group
pointings. They do not imply any correspondence to packet count or to the
unmapped full ReceiveEvent vector.

## Native ABI and conversion

Exactly two production exports are added: `ams_mel_rf_data_submit_product_rx_v9`
and `ams_mel_rf_product_rx_event_view_v9`, under unchanged **AMS_MEL_0.1**.
Event-v9 nests complete frozen event-v8 at offset zero and appends the existing
F2 `ams_mel_rf_pointing_span_v1`; no duplicate pointing/coordinate model or owner
is introduced. The C11 probe checks exact function signatures, const point
pointer, uint32 kind, reused record size/alignment and every field offset.
Measured 64-bit layout: v9 **248 bytes/alignment 8**, event offset **0**, span
offset **232**; span **16/alignment 8**, pointer/size offsets **0/8**; F2 pointing
**176/alignment 8**, kind/ECEF/LLA/platform/face/conic offsets
**0/8/72/136/152/168**. Historical probes remain active.

The reverse conversion uses only the active pinned alternative:

| Kind | Native getters / copied C fields | Units |
|---|---|---|
| ECEF (0) | location and velocity indices 0/1/2; time getters | meters, meters/second, UTC seconds/femtoseconds |
| LLA (1) | latitude/longitude/altitude getters; NED velocity indices 0/1/2; time getters | radians, meters, meters/second, UTC seconds/femtoseconds |
| Platform-relative (2) | location az/el members | radians |
| Face-relative (3) | location az/el members | radians |
| Baseline-relative (4) | conic location getter | radians |

Destinations are value-initialized and explicitly assigned, never memcpy'd from
provider classes. The pinned vector getter is read once and the returned vector
remains alive through conversion. Counts, allocation limits and element sizes
are checked; ECEF/NED native vector sizes must be three. Completed C points are
owned in the existing event's vector. Only after all copies finish is its
immutable span published. Empty is **NULL/0**, with no presence flag. No provider
STL storage or metadata pointer escapes.

Order, repeats, active kind and normal numeric values are preserved without
physical-range checks, wrapping, clamping or coordinate conversion. Signed zero,
NaN classification and infinity survive; **NaN payload-bit fidelity is not
claimed**. Active UTC components are read verbatim with exact signed int64
representation and no unit conversion. Canonical fractions are **0 <= fs <
10^15**. The pinned duration constructor can produce a noncanonical negative
fraction without normalization; such an active value is unrepresentable in safe
Ada UTC and rejects the entire callback, never silently normalized. Relative
alternatives neither read nor invent validity time.

Frozen RF Job production input paths were not refactored. The F2 representation,
constructors and qualification oracles are reused, not replaced.

## Compatibility matrix

Submit versions are metadata profiles, not JobDataFormat numbers.

| Version | Accepted format domain | Nonempty associations | Nonempty stabilization |
|---|---|---|---|
| v1 | {3} | reject | reject |
| v2 | {2,3} | reject | reject |
| v3 | 0..3 | reject | reject |
| v4 | 0..4 | reject | reject |
| v5 | 0..5 | reject | reject |
| v6 | 0..6 | reject | reject |
| v7 | 0..7 | reject | reject |
| v8 | 0..7 | VITA only, exact packet count | reject |
| v9 | 0..7 | same as v8 | accept complete vector |

Historical absent/present-empty association behavior is retained. v8/v9 present
VITA associations must match packet count, including zero packets; nonempty
integer associations reject. Formats 8..13 and invalid numeric formats reject
before provider creation in v9. Views v1..v8 reject nonempty stabilization,
leaving output pointers unchanged. Views v1..v7 additionally retain nonempty
association rejection and historical format domains. v8 retains all-format
association viewing for empty-stabilization events; v9 also views historical
empty-stabilization events. Production views remain allocation-, provider- and
payload-copy-free.

A const completion/callback-state stabilization capability is initialized before
registration, including synchronous callbacks. v9 enables associations too.
Existing worker, queue, DROP-INCOMING, ownership, drain and permanent exact
callback/DSO-retention mechanisms are unchanged. Unsupported std::any and full
ReceiveEvents still reject atomically. No new callback registration or worker.

## Safe Ada

Product_Rx now submits all eight formats through v9 and receives through exactly
one v9 export. Offset-zero nesting reaches v8/v7/earlier width-specific records;
historical view exports are no longer called for integer payloads. Existing
format, inactive-span, packet length, association cardinality and ownership
checks remain, with complete inactive integer/VITA checks.

`Stabilization_Point_Count` and Positive 1-based `Stabilization_Point_At` return
owned metadata and `AMS.MEL.RF.C2.Pointing` values. There is no C2-to-Product_Rx
dependency or elaboration cycle; reusing this value type does not open a C2
provider. Its private kind is now a defaulted discriminant; constructors and
client-visible definiteness remain source-compatible. Minimal read-only
Kind/ECEF/LLA/relative/conic/time accessors raise **Constraint_Error** on wrong
kind. Bad Positive indices also raise Constraint_Error.

Before importing points, Ada checks Natural and container count limits,
canonical empty NULL/0, nonnull nonempty pointer, alignment, size multiplication
and address extent. Every tag and active UTC fraction is validated. The import
renames each individual raw element rather than copying inactive raw payloads;
only active numeric fields are read. All accepted points are copied to a private
Ada vector. Returned metadata remains usable after Event, endpoint and parent
Close. Supported normal C double representation is checked at elaboration.
Measured Ada parity is **64-bit only**; no 32-bit runtime qualification is claimed.
As with other checked ABI spans, validation cannot prove an arbitrary address is
mapped; the native ABI owns the storage-validity contract.

## Failure and deterministic controls

Test-only failpoint 6 throws bad_alloc after at least one point is copied.
Tests require allocation_failures to increase, Receive to return TIMEOUT without
a partial event or stale output, and a later callback to recover on the same
endpoint. Separate malformed counters and association cardinality remain checked.
Legacy EMIT_STAB_POINTS remains a negative control: publishing an event while
discarding points fails. A positive exact-vector control requires all five kinds
in order plus duplicate face/ECEF elements.

Mock fixtures use pinned setStabPoints/addStabPoints. Every active ECEF component
is initialized. All integer/VITA formats cover empty, single-kind, ordered mixed,
duplicates, signed UTC endpoints, out-of-conventional values, IEEE, point counts
different from packet counts, zero-packet nonempty points, simultaneous VITA
associations and immediate provider metadata mutation. Late failure, malformed
associations, unsupported ReceiveEvents/user data, queue-full, synchronous
registration and held-callback parent-first Close controls augment inherited
lifecycle and IR tests. Release assertions use explicit CHECK, not NDEBUG-disabled
asserts.

Ada raw-boundary controls cover count/nullness/alignment/extent, unknown tags,
noncanonical UTC and ignored inactive UTC. Test-only native Close observations
prove extended-return finalization after malformed first and later elements,
including a failure after an Ada point was already copied. Wrong-kind access,
owned-copy inspection after Close and classification-aware IEEE comparisons are
also tested. Test seams are not in production headers or production exports.

## Qualification and review gate

Final-source qualification completed successfully. Baseline and failed
development logs remain preserved separately. No toolchain/provider result is
inferred from mock tests.

| Native configuration | Serial | Full parallel-4 repetition | Focused v9 repetition |
|---|---|---|---|
| GCC 14.2 Debug | 434/434 | 434 x 50 = 21,700/21,700 | 3 x 50 = 150/150 |
| GCC 14.2 Release | 434/434 | 434 x 50 = 21,700/21,700 | 3 x 50 = 150/150 |
| Clang 19.1.7 Debug | 434/434 | 434 x 50 = 21,700/21,700 | 3 x 50 = 150/150 |
| Clang 19.1.7 Release | 434/434 | 434 x 50 = 21,700/21,700 | 3 x 50 = 150/150 |

All **86,800** full-suite repeat executions pass, every test exactly 50 times,
with no failed/Not Run cases. The four serial campaigns add **1,736** executions;
focused v9 campaigns add **600**, total **89,136** final matrix executions.
All inherited F24 association and IR lifetime tests remain active. C11 probes
also compile warning-clean under both GCC and Clang. Final fresh production
Release measures exactly **222 AMS_MEL_0.1 functions**, zero test hooks, with
exactly the two specified additions and no removals. Vendor SHA-256 remains
**804/804**, all 57 RF vendor headers also byte-match the verified RF checkout.
Removing only the additive F25 header block reproduces the frozen v1..v8
header/constants byte-for-byte. Native qualification source hashes were checked
unchanged throughout the campaign.

Evidence: `final-native.log`, four `repeat-*.log`, four `focused-*.log`,
`final-production-symbols.txt`, `final-audit.log`, `final-summary.json`,
`evidence/source-abi-audit.txt` and `evidence/contract-hashes.txt`.

Completed final language qualification (`final-languages.log`): Alire library
build and full safe Ada suite, direct-GPR full smoke, GNATformat/format check,
aggregate `make check` including all native tests and physical build-tree/
failpoint separation, Python **145/145** plus warning-as-error compilation,
Rust **98** unit/integration plus **four** separate doctests, workspace check,
Clippy all-targets **-D warnings** and rustfmt all pass. The Ada stabilization
campaign performs **50 iterations per format**, all eight formats (400 endpoint
campaigns), including eight finalization-failure cases per endpoint. GNAT
16.1.0 is the Alire compiler; GCC 14.2.0 and Clang 19.1.7 compile first-party
native code with warnings as errors. Existing imported-project GNATformat
warnings about a source-free native C GPR project are unchanged; no compiled
source warnings were suppressed.

Development failures are retained: omitted generated Ada declarations and an
operator visibility clause; test dlsym string type mismatch; duplicate/unused
use clauses and mutable test values flagged by warnings-as-errors; Python
inventory ordering and one second inventory count. All were corrected, with
successful replacement evidence. No inherited assertion, timeout or repetition
count was weakened. Two newly added files initially lacked a final newline;
the newline gate exposed and corrected them before final qualification ended.

Sequential production-source Squall **Job, VA, C ProductRx and safe Ada
ProductRx** each pass **one iteration**, using task-owned build resources and
distinct port bases **54000/55000/56000/57000**. C ComplexINT16 received **8/8**
events, received=queued=8, zero drops/malformed/allocation failures; copied event
survives parent/endpoint Close. Safe Ada ComplexINT16 passes through the new v9
Submit/Receive path. Logs: `squall-{job,va,rx,ada}.log`, `squall.log`. No nonempty
real-provider stabilization or association observation is claimed. All
task-owned Squall/Couloir runtimes were removed by their existing cleanup; user
containers and pinned source checkouts remain untouched.

## Scope limits and next task

Excluded: full ReceiveEvent mapping and stabilization references therein,
JobEventID referent semantics, arbitrary std::any, PDW/LF receive, VITA
decoding/generation, DSP/hardware execution, frame conversion, RDMA/DMA changes,
vendor edits, public Python/safe Rust receive extensions and unrelated lifecycle
or IR refactoring. Next vertical slice: pinned full ReceiveEvent mapping with
explicit stabilization reference semantics, only after upstream inspection.

Real Squall ComplexINT16 receive remains the provider-positive control. Mock
pointing fidelity does not establish that Squall emits or semantically validates
nonempty stabilization vectors; no such claim is made without observed callback
evidence. Publication stops at an open non-draft PR with auto-merge disabled;
local qualification is not independent approval.
