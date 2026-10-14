# Task 034F24 — ProductRx VITA receive-event associations

## Reconciliation and source identities

Starting main: `cf13713c0e62f15f01d7b9a249978e49c7f6b76a`, PR #98 merged.
The existing `/home/zboll/git/ams-mel-034f24` worktree and
`feature/034f24-rf-product-rx-vita-associations` branch were resumed, with clean
tracked sources, no implementation commit/PR and no outstanding F24 writers.
Earlier worktrees and evidence were preserved. Main workflow 38504581159 was
inspected once as in progress; it is not feature qualification. Evidence and
failed attempts are retained in `/home/zboll/git/ams-mel-034f24-build`; TMPDIR is
its disk-backed `tmp` directory, not pressured `/tmp`.

Retained pristine baseline: GCC/Clang 19 Debug/Release 428/428 each; Python 143;
Rust 96 unit/integration plus four separate doctests; Alire library/full Ada,
formatting/direct-GPR, aggregate check/build isolation, whitespace/newlines pass.
Production Release: 218 AMS_MEL_0.1 exports, no test hooks; vendor SHA-256 804/804.
The continuation did not repeat this completed baseline.

RF MEL pin: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Squall pin: `b1015728f904c799fa0c07489fce48e78f67845f`.

## Pinned semantics and profile

`ProductRxMetadata::getReceiveEventAssociations()` returns by value an
`optional<vector<JobEventID>>`; static assertions establish the exact getter
result and `JobEventID = uint32_t`. Its documented positions correspond to
VITA callback packet-object indices. A single observed ReceiveEvent can generate
multiple packets: `[17,17,23]` associates packets 1/2 with event 17, packet 3 with
23. Ordering and duplicate IDs are not sorted, deduplicated or normalized.
These identify observations, not commanded JobIntervals. IDs are opaque numeric
identifiers: no actual ReceiveEvent referent validity is asserted or checked.

Submit-v8 is an explicit opt-in metadata profile, not JobDataFormat 8. Its domain
is 0..7; 8 (PDWType1), 9..13 and UINT32_MAX reject before provider creation.
Frozen domains are v1={3}, v2={2,3}, v3={0..3}, v4={0..4}, v5={0..5},
v6={0..6}, v7={0..7}. The maximum-format calculation explicitly caps v8 at 7.

| Profile / data | Absent | Present-empty | Present-nonempty |
|---|---|---|---|
| Legacy v1..v7 | accepted | accepted, including nonzero count | rejected |
| v8 Direct/Complex 0..3 | accepted | accepted | rejected |
| v8 VITA 4..7 | accepted, including nonzero packets | accepted only with zero packets | accepted iff association count equals packet count |

The existing completion carries the immutable opt-in choice through Submit,
worker input's completion, Claim and callback state. State is fully initialized
before registration, including synchronous provider callbacks. There is no new
worker or provider call. Closed admission returns before any metadata/payload
access. The getter is read once in event construction; the returned optional
is retained locally through validation/copy, never exposed as provider storage.

## ABI, copy ownership and failure handling

Exactly two exports are added, submit-product-rx-v8 and event-view-v8, under
AMS_MEL_0.1. Association-v1 contains uint32 `present` and frozen u32-span-v1.
Event-v8 nests the complete frozen v7 at offset zero. Absent is 0 + NULL/0;
present-empty is 1 + NULL/0; present-nonempty is 1 + immutable event-owned IDs.
All association backing finishes before final span publication. The new view
is provider-free, allocation-free and payload-copy-free. The existing owner and
Event Close are reused; borrowed pointers cease to be valid at Event Close.
Historical views v1..v7 reject nonempty associations with INVALID_ARGUMENT and
leave the output pointer unchanged. Absent/empty events retain historical
format restrictions; v8 views preserve observed presence even on legacy events.

Malformed cardinality rejects the entire callback. Later association allocation
failure after at least one copied ID increments allocation_failures and publishes
no samples, bytes or partial metadata. Other exceptions retain existing malformed
handling. Recovery uses the same endpoint. Queue policy remains bounded
DROP-INCOMING and counters remain saturating. Existing eight pointer alternatives,
VITA header-derived lengths, network/packet byte order and full-byte tests are
unchanged. Permanent exact registered std::function, callback state and provider
DSO retention, request/unique Claim, abandonment, parent-first Close, blocked
Receive wakeup and admitted-callback drain remain unchanged. Drain is not
provider-wide quiescence and never permits unloading retained provider code.

Safe Ada VITA Submit/Receive use v8; integer Submit and payload-view routing
remain frozen. Receive additionally reads v8 metadata for integer events so
historically accepted present-empty association presence is not lost. Existing
nested-v7 and earlier validation is reused via offset-zero nesting, without
calling an exported older view that rejects richer metadata. Presence, NULL/0,
representable counts, pointer alignment/extent and cardinality are checked
before importing IDs. Descriptor count multiplication is guarded against the
address-width maximum before computing the extent. IDs are copied to a
private Ada vector in Product_Metadata. Its ordinary owned copies remain usable
after native Event, endpoint and parent Close. Invalid Positive indices raise
Constraint_Error. The limited-controlled extended-return Event still finalizes
its native owner if copying raises.

## Qualification

The new C11 probe checks both signatures, nested offset zero, association/event
sizes and alignments, field offsets and immutable exact uint32 span. Rust and
ctypes compile that probe and compare measurements; private Ada representation
checks retain v1..v7 and add the measured 64-bit v8 layout (232 bytes, alignment
8; association record 24 bytes, presence offset 0 and span offset 8).

Mock scenarios use actual pinned optional setters, not synthetic C views.
Absent/empty/single/duplicate/distinct/shorter/longer/zero-packet cases cover
all formats, historical submits and every view generation. All four VITA widths
preserve `[17,17,23]`, `0`, UINT32_MAX and `0x81234567`. Provider metadata is
overwritten after callback return. The C campaign executes 50 iterations with
explicit checks active under NDEBUG. Public Ada executes 50 iterations per VITA
width, covering independent metadata, indices, malformed counts, allocation
failure/recovery, packet copy alignment and parent-first lifetime. Dedicated
association-bearing held-callback/Close and synchronous-registration tests
augment inherited process-isolated ProductRx lifecycle regressions.

Implementation failures and corrections remain in evidence: initial Ada accessor
placement after the package elaboration begin; a redundant extent comparison
that GNAT correctly diagnosed as always false for bounded Natural counts.
The final language script also encountered Alire rejecting an already-exported
provider directory with an equivalent differently spelled path during its last
format check; unsetting the variable corrected the invocation. The failed log
is retained, not reported as a wholly successful script.
No warning suppression, vendor edit or inherited test weakening was used.

### Measured final-source results

GCC Debug/Release and Clang 19 Debug/Release each pass **431/431** one-pass
native tests and every test repeated exactly **50** times at parallelism four.
Log counts establish **21,550** repeat executions per configuration and
**87,924** total one-pass plus repeat executions. The three additive cases
are v8 C11 layout, association-close/synchronous registration and associations;
all 428 inherited cases remain. Focused C campaigns internally run 50 iterations
including Release/NDEBUG. Public Ada association cases run 50 iterations per
VITA width in full Debug-backed and GCC Release-backed smoke tests.

Python **144** full tests and compile checks pass. Rust **97** unit/integration
and **four separately executed doctests** pass; Cargo check, Clippy -D warnings
and rustfmt pass. Alire library/full Ada, direct-GPR full smoke, aggregate make
check/build-tree isolation, GNATformat, whitespace/final-newline checks pass.
The follow-up runs qualify the final Ada extent and optional-presence changes.
Both end-of-script formatter environment conflicts were corrected by unsetting
the provider variable; successful corrected logs are retained separately.

Production Release measures exactly **220 AMS_MEL_0.1 exports**, no test hooks.
Removing only the F24 additive block reproduces the historical header/constants
byte-for-byte. Vendor SHA-256 remains **804/804**. The F22 navigation completion
correction is byte-identical. Evidence: final-native.log, four repeat logs,
final-languages.log, final-followup-corrected.log, final-format-corrected.log,
final-audit.log and final-summary.json. Disk availability was checked before
stress; about 741 GiB remained at completion.

Sequential final-source pinned Squall Job, Ada VirtualAperture, C ProductRx and
Ada ProductRx each pass **one iteration**, using task-owned build resources
and separate 55000/56000/57000/58000 port bases. ComplexINT16 receives remain
the positive controls. No nonempty real-provider association vector is claimed.
Fresh push/PR workflow identities and their single inspection are recorded in
the PR/handoff, not inferred from main CI.

## Provider and scope limits

ComplexINT16 remains the positive real-provider receive control. Squall need not
emit nonempty VITA associations: positive association fidelity is mock-backed,
not claimed for Squall without observed and independently checked values.
Complete 32-bit cross-language runtime qualification is unavailable and not
claimed; Ada's measured parity is the supported 64-bit host.

Excluded: full ReceiveEvent object mapping, stabilization-point vectors,
arbitrary std::any user data, new JobDataPointer alternatives, PDW/LF receive,
packet decoding/IQ interpretation, VITA generation/transmission, RDMA/external
memory registration, DSP/waveform execution, RF emission or hardware claims,
vendor modifications, public Python/safe Rust receive APIs and unrelated
lifecycle refactoring. Mock IDs do not establish semantic referent validity.
Publication stops at an OPEN, NON-DRAFT, UNMERGED PR with auto-merge disabled;
local qualification is not independent approval.

## PR #99 inherited IR stream lifetime correction

Reviewed feature: `fef815b035329deb5a415f67b8f2c537374571c1`.
Push workflow 38508809524 / Clang Release job 115672342002 checked out that
exact SHA (git log in retained full job log). Serial 431/431 passed; parallel-4
until-fail:50 failed `ir_stream_contract`, at the child-2 late-callback check.
Complete log SHA-256:
`d799f4eed6999ef2d01aab8ecba01cd1ec823e19a0e6fcdc2879ec3e74927e80`.
Evidence is retained under the task evidence directory's `ir-correction`.
The starting-main and feature test file hashes are both
`eb7dc538c46f12416595ecba1157c115283636f007d721e2e45d89bbac9d7247`;
that identity alone was not treated as evidence of safety.

### Established counterexample, not the initial hypothesis

Test-only diagnostics preserve synchronization order and print failed line,
marker state, last navigation status, destruction/executor counters and lifetime
log before cleanup releases any barriers. Producer status is read only after
its thread has joined (otherwise printed as unavailable), avoiding a diagnostic
race. Failed lifetime logs are retained. A narrowly isolated child-2 invocation
supplements, and never replaces, the full default lifetime contract suite.

The early-navigation-completion hypothesis was disproved in reproduced failures:
the zero-timeout observation returns TIMEOUT (numeric status 9), while the
external join marker is present and the callback release remains held. Once
released, the callback returns, two wrappers are destroyed before the channel,
and registered buffers and channel destruction counters are correct.

Affinity-constrained two-CPU/eight-process pre-correction runs reproduce the
**final log snapshot** failure in seven lanes, including lane 4 iteration 1010
and lane 6 iteration 518. At the failed snapshot, logs contain the expected
release/drain/channel teardown and `completion_graph_released_3`, but no
`control_destroyed`, `manager_destroyed`, or `library_unloaded` yet. Native
navigation `complete()` publishes terminal state under its mutex, then resets
its local stream reference, then notifies; a zero/bounded wait may acquire the
mutex and observe terminal state before that local reference is dropped.
Request Close is not a worker join. Session Close may therefore return while
that last independent graph reference is still unwinding. This is permitted
ownership, not demonstrated premature destruction or lost cleanup.
Unconstrained isolated 2000 attempts and 80,000 multi-process attempts did not
reproduce; constrained counterexamples, not those passes, establish the cause.
The additional pre-correction serial diagnostic repetition was stopped once
the counterexample was obtained and is not reported as qualification.

### Narrow test correction

Production sources, provider implementation, ProductRx associations and ABI are
unchanged. The test now observes the actual `library_unloaded` lifetime condition
with a five-second CLOCK_MONOTONIC deadline before taking the final ordered
snapshot for child 2. Short sleeps only pace condition re-observation; there is
no blind scheduling delay, retries of failed assertions, or assumption that
terminal navigation state is a worker join. The existing zero-timeout assertion,
held callback, external join, exact buffer-wrapper counts, release executor
checks and wrapper/channel/control/manager/unload order remain active. A missed
unload still fails with diagnostics rather than passing or retrying.

Resource-failed full campaigns are also retained: host `/tmp` reached 100%
capacity despite task TMPDIR because inherited fixtures hard-code `/tmp`.
Several unrelated lifetime-marker assertions then failed. Only task-owned
campaign processes were stopped; no host files or earlier evidence were removed.
Complete replacement campaigns run in a private user/mount namespace binding
hard-coded `/tmp` to task-owned disk storage. No inherited tests are excluded.

The corrected full Clang Release IR stream suite passed 100 consecutive runs,
including deterministic positive and destructive negative controls. The same
two-CPU/eight-process isolated child-2 setup passed 16,000 executions (2,000 per
lane) with no failure. Final-source full qualification is recorded below when
completed. No Squall rerun is
needed for this test-only change; the original pinned final-source evidence is
retained without additional provider claims.

### Measured corrective final-source qualification

GCC Debug/Release and Clang 19 Debug/Release each pass **431/431** serial native
checks and every test exactly **50 times**, parallelism four: **21,550** repeat
executions per configuration, **87,924** serial-plus-repeat executions total.
Final-source focused Clang Release IR stream passes **100 consecutive** full
executions (final-focused100.log), retaining all deterministic positive and
negative lifetime controls. All F24 association optional/cardinality/failure
atomicity and four-width campaigns remain active and unchanged.

Python **144**, Rust **97 unit/integration plus four separate doctests**, Cargo
check/Clippy -D warnings/rustfmt, Alire library/full Ada, direct-GPR full smoke,
GNATformat, aggregate make check/build-tree isolation and whitespace/newlines
pass. Production Release remains **220 AMS_MEL_0.1 exports**, zero hooks;
vendor SHA-256 **804/804**. Production/provider/ProductRx/header/constants/
exports are byte-identical to reviewed fef815b. No Squall rerun or new provider
claim is made for the test-only correction.

Successful replacement qualification is in ir-correction/final-native.log,
four repeat logs, final-languages.log, final-focused100.log and
namespace-validation.log (CORRECTIVE_FINAL_COMPLETE). Earlier resource-failed
and reproduction logs remain separately retained, not erased or counted as
passes. The isolated namespace leaves host /tmp and other worktrees untouched.
