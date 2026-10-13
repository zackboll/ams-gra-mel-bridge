# Task 034F16 — copied RF WaveformStream descriptors

## Reconciliation and scope

Starting main is `db57443415db8b6748b0a1e1bd1b944245a31515`, exactly the
reviewed main. PR #90 is merged; no intervening main commits, open PRs or
matching F16 work were found. Main run 38333053696 completed successfully.
The isolated worktree is `/home/zboll/git/ams-mel-034f16`, branch
`feature/034f16-rf-waveform-stream-values`. Earlier worktrees and evidence
are preserved. Evidence lives in `/home/zboll/git/ams-mel-034f16-evidence`.

This slice copies six scalar descriptor values. It does not acquire an
endpoint, register/own memory, transfer bytes or execute a waveform.

## Exact pinned contract

RF MEL revision: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
`WaveformStream.h` declares these setters, returning void, and corresponding
const getters returning values of the same types:

| Setter / getter suffix | Native type | Default |
|---|---|---|
| EndpointID | EndpointID = uint64_t | 0 |
| StartAddress | uint64_t | 0 |
| NumBytes | size_t | 0 |
| SampleFormat | DataSampleFormat | ComplexCartesian |
| ItemFormat | DataItemFormat | FixedPointSigned |
| RepeatMode | BufferRepeatMode | RepeatFromTop |

DataSampleFormat is Real=0, ComplexCartesian=1, ComplexPolar=2.
DataItemFormat is FixedPointUnsigned=0, FixedPointSigned=1,
VRT_unsignedFloat=2, VRT_signedFloat=3, Float32=4, Float64=5, Float16=6,
FixedPointUnsignedNonNormalized=7, FixedPointSignedNonNormalized=8.
BufferRepeatMode is RepeatFromTop=0, ContinueFromPrevious=1.
Production compile-time assertions check every bridge mapping. C enums are
independent uint32 domains, not claims about native enum layout.

`Modulation` directly contains WaveformStream, alongside CommonModulations,
extension and cached-waveform alternatives. The seven previous bridge kinds
select alternatives inside CommonModulations. Construction explicitly uses
`std::in_place_type<rfmel::WaveformStream>`; no positional variant index.
The actual-header runtime probe checks signatures/defaults, all enum
combinations, holds_alternative/get, vector copies and copied JobIntervals
after earlier values are mutated/cleared. Checks remain active under NDEBUG.

## ABI and ownership

ABI remains 0.1. Exactly one production export is added:
`ams_mel_rf_job_add_intervals_v15`. Modulation-v5 nests frozen v4 and the
new stream record. Interval-v15 uses frozen v8, presence flags, frozen
TX-v3 span and modulation-v5 span. Older array strides are never reused as
new strides. V11/V12/V13/V14 retain limits 0–2/0–4/0–5/0–6; V15 adds tag 7.

Endpoint ID and address preserve the full uint64 range. NumBytes must be
at most native size_t maximum; larger counts reject before provider Add.
Safe Ada performs a checked conversion/round trip before backing addresses
are published. No owned buffer is needed for a stream descriptor. Safe Ada
has explicitly represented Stream sample/item/repeat enums, a constructor
and kind-guarded atomic replacement. Wrong-kind replacement raises
Constraint_Error without changing the value. Both Add operations use v15.

Presence 0 ignores the entire collection; 1 validates/copies even empty;
other encodings reject. Kind 7 ignores inherited inactive payloads. Kinds
0–6 ignore stream payloads and retain their active validation. Interval-local
references preserve -1, ordering and duplicates. OOK exclusivity is unchanged.
Full-batch preparation precedes one unlocked provider Add within existing
exception barriers, registration and terminal-state gates.

StartAddress is an opaque **numeric address**, not a process pointer. The
bridge never casts it to a pointer, dereferences, maps, inspects or registers
it. Zero ID/address/count are structurally valid copied values, not evidence
of executable streams. No endpoint existence/capability/permission queries
or auto-gating are performed. No address-plus-count, repeated extent, sample
duration, transfer rate, data-item size or packet-length arithmetic occurs.
The pinned header requires adequate backing for RepeatFromTop and
ContinueFromPrevious; the application/provider must enforce it. Exact repeat
mode values are retained without calculating memory extents. Float format
choices do not establish hardware support or convert data. Stream formats
are distinct from JobDataFormat and WaveformBuffer sample widths.

## Requirement-to-test matrix

| Requirement | Qualification |
|---|---|
| Exact signatures/defaults/direct variant/copies | actual-header probe |
| All mappings | production static assertions; header enum loops |
| C11 records/spans/all offsets/by-value signature | compiled stream layout probe |
| Raw Rust/ctypes parity | respective ABI tests compare compiled C output |
| Ada representation | existing private representation qualification extended |
| Retained native six fields/direct variant | mock f16 observer; C and public Ada |
| Repeated/reordered local references | asymmetric C tables/TX lists; Ada copies |
| All enums/high numeric boundaries/zero values | C enum/boundary fixture; header probe |
| Mixed old/new kinds | C v15 all-kind table; public Ada stream/buffer/custom/phase |
| Independent safe values | public Ada source/table/interval/list retained copies |
| Invalid enums/late definitions/presence/spans/local refs | C pre-Add rejection/recovery |
| Preparation unwinding/no Add/recovery | test-only later-stream-definition failpoint |
| Historical profiles/lifecycle | existing regressions; explicit v14 kind-7 rejection |

## Failed attempts and corrections

Baseline.log records native 291 tests, Python/Rust and Alire baseline stages.
Source edits began before the aggregate baseline completed: its final Ada
format check therefore failed on newly edited files. This is not an
untouched-baseline aggregate pass. The workflow mistake is recorded, not
erased by later qualification.

Native attempt 1 rejected fixture field/span names; attempt 2 rejected a
missing inherited mock capability setup. Both were corrected to existing
conventions. Ada attempt 3 rejected an always-false LP64 size comparison
under warnings-as-errors; checked conversion/round-trip validation replaced
it. Attempt 4 rejected a redundant use-type clause, which was removed.
No warning suppression or weakening of checks was used.
Attempt 5 found that the representation helper's unchecked conversion used
object size where the new padded records have a different value size;
explicit scalar/nested aggregates replaced that conversion. Attempt 6 then
rejected a mixed-type others aggregate; six explicit zero fields fixed it.
Final-newline failures in the new C fixture/task document were corrected.

## Exclusions and remaining limits

No WaveformTxEndpoint creation, CachedWaveform ownership,
ModulationExtensionBase, Weights, RDMA registration/memory-region ownership,
ExternalEndpoint allocation/association, memory mapping, DMA, upload/data
transfer, IQ generation/conversion/resampling, RF transmission, hidden
provider query, vendor changes, unrelated lifecycle refactoring or public
Python/safe Rust API expansion is included.

The C11 layout probe and actual-header C++ Release/NDEBUG probe also
compiled and ran as i386 executables (`layout32`, `header32`). This is
limited layout/header runtime evidence, not a complete 32-bit bridge/mock
or Ada/Rust/Python qualification. NumBytes > size_t rejection cannot be
exercised on LP64; the complete bridge runtime remains qualified on LP64.
Lifecycle cases continue through existing regression paths rather than a
fictional stream memory-release/transfer-completion contract.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` is receive-only;
JobInterval Add is a no-op. Its Job fixture includes a zero-valued stream
and valid local reference. A successful real-provider call qualifies only
call-path acceptance, not descriptor fidelity, address validity, RDMA,
transfer, waveform generation or RF emission. Positive fidelity comes from
actual-header and mock-backed tests.

Final validation and publication results are recorded below as completed.

## Final-source results recorded so far

- final-validation.log: native GCC Debug **293/293**, Python **136/136**
  with warning-as-error compileall, Rust **89** unit/integration tests and
  **4** separately counted doctests, workspace check, Clippy `-D warnings`
  and rustfmt check pass. Alire library and full Ada suite pass; the focused
  public Ada TX test includes stream cases within its internal **50/50** loop.
- audits-source.log: **804/804** vendor SHA-256 values unchanged; all frozen
  C declarations/constants byte-identical outside the additive v15 block.
- final-validation.log ended at an Alire environment-variable conflict after
  the Rust test environment was exported. The clean-environment remainder
  preserves that failed driver attempt rather than erasing it.

Coverage boundaries: the stream fixtures observe stream scalar fidelity and
TX references directly. Mixed old-kind payload/control fidelity also remains
qualified by the existing F12–F15 fixtures, now using v15 from public Ada,
not by duplicating every observer in the new stream function. C rejection
cases explicitly count provider Adds and retain observations; late-definition,
span and presence cases share an unchanged-count/observation check after the
sequence. Only limited i386 layout/header runtime evidence is claimed; no complete
32-bit bridge/mock or cross-language runtime suite was run.

- Limited i386 qualification: the actual C11 layout executable compiled and
  ran, reporting stream record size/alignment 36/4, modulation-v5 124/4,
  interval-v15 184/4 and spans 8/4. The actual-header C++ probe compiled and
  ran with `g++ -m32 -O2 -DNDEBUG -Wall -Wextra -Werror`. This does not replace
  unavailable complete 32-bit bridge/mock and cross-language suites.

- final-remainder.log: GCC Debug/Release and Clang Debug/Release each pass
  **293/293** native tests. Release checks remain active; the C stream fixture
  repeats internally **50/50** in each configuration, without an additional
  outer campaign. Test-only trace files record **50** later-stream-definition
  reaches per configuration and preparation-unwound events. The public Ada
  focused executable passes **50/50** against GCC Release native backing.
- production-audit.log: production Release has exactly **205** symbols under
  `AMS_MEL_0.1`; the sole addition is v15, with no test instrumentation.

- Aggregate `alr -C ada exec -- make check` passed, including native 293/293,
  direct-GPR Ada smoke, formatting, newline/whitespace and production/test
  build-tree isolation. Explicit direct-GPR build/run also passed before the
  sequential pinned Squall campaign. Imported source-free GPR warnings remain;
  first-party compiled sources retain warnings-as-errors.

- final-remainder.log concludes FINAL_SQUALL_COMPLETE and
  FINAL_VALIDATION_COMPLETE. Final-source pinned Squall Job, Ada
  VirtualAperture, C ProductRx and Ada ProductRx each pass one iteration,
  sequentially on port bases 53000/54000/55000/56000 with task-owned backing.
  No unrelated containers/processes were removed. Job acceptance remains
  no-op call-path evidence only, not descriptor fidelity or RF execution.

Additional review limits: the new fixtures do not enumerate every invalid
legacy payload, every presence-zero poison combination, or every lifecycle
case independently for tag 7; unchanged generic validation/lifecycle tests
remain in the full suite. The preparation hook demonstrates no provider Add,
unchanged retained observations and same-Job recovery; unwinding traces prove
exception unwinding, not a separate resource-release contract. NumBytes
exceeding size_t is not tested through a complete 32-bit bridge runtime.
These are explicit qualification limits, not additional API guarantees.

Publication is left at the independent review gate: open, non-draft,
unmerged, auto-merge disabled. Hosted checks qualify their own checkout
identities, not earlier main runs. Pending CI is not represented as green.
