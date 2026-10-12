# Task 034F12 — RF value-only modulations and TX references

## Reconciliation and scope

Fetched origin/main is `678bce6cad4d34dec5118039e9bb6f62feb38ad2`, exactly
reviewed main. PR #86 is merged, including corrective commit
`3b8a7cc0d26e4e4f95c5c2917327e4c70a1a875d`. No intervening main changes,
open PRs, or existing matching F12 branches were found. Isolated worktree
`/home/zboll/git/ams-mel-034f12` uses
`feature/034f12-rf-value-modulations`. Original/F10/F11 registrations and
evidence are preserved. No reset, clean, rebase, amend or force push was used.
Main hosted run 38188170822 completed successfully; it does not qualify F12.

This task constructs exactly Tone, PhaseCode and LinearFrequencyModulation
values. It does not synthesize waveforms, generate IQ, execute modulation,
transmit RF or establish hardware capability. OnOffKeying, CpfskPulse,
CustomWaveform, WaveformBuffer, CachedWaveform, WaveformStream,
WaveformTxEndpoint, ModulationExtensionBase, native pointer escape hatches,
Weights, RDMA, registration, routing, transfer, VADB, conditional triggers,
JIB, MFADrivenControls, std::any context and the known applicable-group getter
issue remain excluded. Vendor source is unchanged.

## Pinned contract and preparation

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f`: actual CommonModulations.h,
JobEvent.h, JobInterval.h and RFMELTypes.h are byte-identical to the pinned
Squall RF submodule. GCC 14 and Clang 19 actual-header probes compile/run
under `-O3 -DNDEBUG -Wall -Wextra -Werror`, with explicit checks active.
The probe is also in the normal native pinned-header runtime test.

Tone is an empty value class. PhaseCode defaults to 0 fs and empty phases;
its duration is `chrono::duration<int64_t, femto>`. Its constructor and
setPhaseChipData copy the ordered vector, while setChipDuration takes the
duration by value. LFM defaults to zero Hz; constructor/setter/getter use
double. CommonModulations is the inner variant and Modulation the outer
variant. Both layers are explicitly selected by type, never positional
indices. Copy independence is checked through phase vector, PhaseCode,
CommonModulations, Modulation, vector and JobInterval, then a copied interval,
with the earlier sources mutated and destroyed. The selected alternatives
never acquire resource-backed alternatives.

JobInterval defaults to an empty table, setModulations copies a vector, and
const/nonconst getters return references. TransmitEvent setModIndex copies
`vector<int>` and getModIndex returns it **by value**. Native int is signed;
the default list is {-1}. These upstream setters do not validate references.

The bridge intentionally validates the complete same-interval relationship:
-1 means no modulation, all other negative values reject, and nonnegative
values must fit native int and identify an entry in that interval's active
table. No reference can resolve through another interval, prior Add, registry
or provider query. Explicit Tone is distinct from -1. Ordered tables, phase
chips and index lists retain repetitions and unused definitions.

The complete batch is structurally validated before native preparation or
provider Add. Typed accessors select TX-v3's frozen TX-v2 controls/TX-v1 fields;
v11 reuses the frozen v8 common base. No array stride reinterpretation or
second Add pipeline is introduced. Native values are prepared locally; active
setModIndex, setModulations and setTxEvents are called once per respective
object, including explicitly empty vectors. One existing addJobIntervals call
follows full preparation, unlocked, preserving registration/terminal gates and
exception mapping. Test-only traces/failpoints are compiled out of production.
Exceptions within provider Add carry no rollback promise.

Signed fs values pass exactly. IEEE doubles retain finite values, signed zero,
infinity and NaN classification; NaN payload-bit preservation is not promised.
No wrapping, normalization, finite filter or physical arithmetic is added.
Empty chips and nonpositive durations are structurally representable, not
executable-configuration guarantees. Application/provider must enforce pinned
PhaseCode's minimum chip-count obligation (eventDuration/chipDuration), and
LFM's start/end frequencies (center minus/plus half extent) over event duration.
The bridge calculates neither those ratios/endpoints nor slopes/sample counts.

## ABI, presence and safe Ada

ABI remains AMS_MEL_0.1; the sole production addition is
`ams_mel_rf_job_add_intervals_v11` (200 to 201 exports). Bridge tags are
Tone=0, PhaseCode=1, LFM=2, independent of native variant indices. The fixed
modulation record has kind, int64 chip fs, borrowed double span, double Hz.
Tone ignores every payload; PhaseCode reads only duration/chips; LFM only Hz.
Unknown active kinds reject. Every active span receives pointer/count and
correct element-size multiplication checks.

TX-v3 nests frozen TX-v2, a uint32 presence and signed int32 span. V11 nests
**v8**, not v10: uint32 TX presence, TX-v3 span, uint32 table presence,
modulation span. V9/v10 and all previous records/functions/constants remain
unchanged. Presence 0 ignores payload entirely; 1 validates/copies even empty;
other encodings reject. Inactive TX does not inspect nested flags/spans. The
active table validates independently of TX. Absent indices retain {-1}; explicit
empty calls the setter with empty; append starts a fresh explicit list without
an implicit leading -1. Absent/empty tables permit no nonnegative reference.

Measured LP64 C size/alignment/offsets:

| Record | Size | Alignment | Offsets in declaration order |
| --- | ---: | ---: | --- |
| double/int32 spans | 16 | 8 | 0,8 |
| modulation v1 | 40 | 8 | 0,8,16,32 |
| modulation span v1 | 16 | 8 | 0,8 |
| TX v3 | 192 | 8 | 0,168,176 |
| TX span v3 | 16 | 8 | 0,8 |
| interval v11 | 288 | 8 | 0,240,248,264,272 |
| interval span v11 | 16 | 8 | 0,8 |

Compiled C signature/layout probes compare Rust and private ctypes; C++ public
header compatibility remains in the native suite. Raw Ada Convention C values
are exercised through public serialization and new-record representation
checks. No public Python or safe Rust modulation API is added.

Safe Ada offers private owned Modulation_Config and Modulation_Table,
Tone/Phase_Code/Linear_Frequency_Modulation constructors, kind-checked phase
append/replace, Append_Modulation with returned **zero-based** index,
No_Modulation, owned references, and absence/explicit-empty operations.
Set_Modulations copies the table. Final-sized phase, record, reference, TX and
inherited backing survives synchronous Add and unwinds on errors. Arithmetic,
byte counts and conversions precede allocation/pointer publication. References
are checked at submission against the final copied table, not during assembly;
clear/replace never renumbers or repairs references. Both Add names use v11.

## Requirement-to-test matrix

| Requirement | Normal-path evidence |
| --- | --- |
| Pinned types/defaults/copy chain/type-selected layers | value_modulations_header_probe, pinned interval runtime test |
| Both variant layers/selected values | mock_rf_f12_value inspects actual accepted native intervals |
| Two different tables, ordered repeated refs/chips, signed bounds/IEEE | C rf-job-modulations, internally 50 iterations |
| Safe ownership/source mutation/scoped destruction/list copy | public Ada Send_Modulations, existing focused executable, 50 iterations |
| Active-only poisoned payloads, absent/empty/default | C rf-job-modulations and Ada fixture |
| Unknown kinds/presence/null/overflow/local invalid refs/later definition | C rejection checks, unchanged Add count/snapshot and same-Job recovery |
| Partial definition/reference/interval preparation | deterministic hooks, trace reach/unwind, INTERNAL_ERROR/no Add/recovery |
| Once-only active setters | C initial fixture counts 3 index setters, 2 table setters, 2 TX setters from compiled-out traces; getters are not setter-count evidence |
| Frozen TX/RX fields, gates, exceptions/lifecycle | existing TX controls/events and RX suites; explicit v10-after-v11 defaults; v11 Flush/Finalize/Cancel/parent-first checks |
| Raw layout/signature parity | compiled C probe, Rust/ctypes comparisons, Ada representation checks |
| Frozen declarations/vendor | task audit: 225 records, 200 status declarations, 385 constants unchanged; 804/804 hashes |

The public Ada fixture checks table kinds, counts, signed durations, reference order/defaults and copied source scopes; detailed IEEE phase-value observation is in the C mock fixture.

The matrix identifies concrete scenarios, not an assertion that every historical
malformed input or every allocation site is exhaustively repeated. Narrower
32-bit runtime is unavailable and conditional/compile checks are not counted
as executed narrower-runtime evidence.

## Measured commands and development attempts

Task-owned disk-backed logs are under `evidence/task-034f12` (locally excluded,
not shipped). Baseline `make test-native`: 284/284. Python warnings-as-errors:
131/131 plus compileall. Rust workspace: 88 unit/integration + 4 doctests;
check, Clippy -D warnings and formatting passed. `alr -n -C ada build` passed.
Alire 2.1.1 selected GNAT 16.1.0/GPRbuild 26.0.1, GNATformat 26.0.0;
native GCC 14.2, Clang 19; Python 3.13.5; Cargo 1.98.1.

Baseline Ada full-test retry overlapped source edits and is not an untouched
baseline pass. Initial Alire run failed on an already-defined provider-directory
environment variable; retry unsets it. Development failures retained: incorrect
getModIndex reference-type assertion, missing signed operator visibility,
redundant use visibility warnings, nested C fixture macro/name collisions,
stale Python/Rust export-count assertions, missing TX observation helper, missing final newline under Clang, and uninitialized Ada representation-probe
variables. The attempted type-component attribute syntax was rejected by GNAT; corrected checks use initialized zero-record fixtures. Formatting failures
after edits are not counted as passes; GNATformat precedes renewed Ada builds.

Final qualification results and handoff identities are appended only after
the corresponding operations complete.

## Pinned Squall interpretation and review gate

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` is receive-only and
its interval Add is a no-op. The Job fixture now passes a small coherent table
with all three alternatives and explicit references. Normal return establishes
call-path acceptance only, not modulation fidelity, interpretation, waveform
execution, capability or RF emission. Positive payload evidence is mock and
pinned-header evidence. No physical transmission is performed.

The handoff must remain OPEN, NON-DRAFT, UNMERGED, AUTO-MERGE DISABLED.
Hosted pending checks are reported as PENDING_CI, never inferred from prior
green runs. Independent review is not self-certified.

### Completed local qualification

- `cmake -S native -B <task-tree> -DAMS_MEL_BUILD_TESTS=ON` with explicit
  GCC/Clang compilers and Debug/Release, build `--parallel 2`, then CTest:
  285/285 each. Focused C scenarios repeat internally 50 times; no outer
  50-repeat campaign was used. Final test-source rerun also passed all four at 285/285 (final-native-validation.log).
- Python `python3 -W error -m unittest discover -s python/tests -v`:
  132/132, plus compileall passed, also against final GCC Release (final-bindings-release.log).
- Rust workspace `cargo test`: 89 unit/integration tests and 4 doctests;
  check, Clippy `--all-targets -- -D warnings`, fmt check passed, also against final GCC Release (final-bindings-release.log).
- GNATformat ran before renewed Ada builds; focused public Ada passed 50/50,
  including new-record representation checks, source-copy scopes and rejected
  kind/reference operations. GCC Release facade/mock selection also passed
  50/50 (ada-focused-corrected.log, ada-release-final.log).
- Separate production Release: 201 exports exclusively AMS_MEL_0.1, sole
  addition v11; test failpoint/trace strings absent. Frozen C audit:
  225 records, 200 status declarations and 385 constants unchanged; vendor
  804/804. Final source newline and Git whitespace checks passed.

No 32-bit runtime qualification is available. Existing imported source-free
GPR project warnings remain; compiled first-party sources use warnings-as-errors.

- Aggregate `alr -n -C ada/tests exec -- make -C <worktree> check` passed:
  build-tree isolation in both directions, native 285/285, Ada formatting,
  direct-GPR full smoke, whitespace and final-newline checks. Subsequent
  `alr -n -C ada build` and `alr -n -C ada/tests run` passed on final source;
  FULL_ADA_AGGREGATE_COMPLETE is present in aggregate-squall.log.

An optional separate GNATformat check of the historical Squall Job integration
project reported that source as unformatted (squall-job-format.log). The required
first-party crate formatting gates passed. The integration file retains its
existing layout; it is compiled warnings-as-errors in the real-provider path.

### Final-source real-provider qualification

Pinned Squall Job, Ada VirtualAperture, C ProductRx and Ada ProductRx passed
one iteration each, sequentially, on final production source. Resources are
under evidence/task-034f12/squall; port bases 41000, 42000, 43000, 44000 use
control +3, Couloir metrics +18, health +13, RF metrics +14, data +601.
`alr -n -C ada/tests exec -- integration/squall/run-rf.sh <mode>` completed
all four, with FINAL_PATHS_COMPLETE in aggregate-squall.log. Job submitted the
new table/reference fixture through the actual synchronous Add call. No physical
transmission was run. Squall receive-only/no-op Add limits above apply fully.

### Outstanding qualification limitations

No 32-bit runtime is available. Detailed public-Ada IEEE chip-value observations
are not exhaustive (C mock/header evidence carries those values). The task does
not claim every historical invalid-input path after v11 is individually
repeated, every allocation site injected, or hardware execution fidelity.
There is no copying/lifetime blocker in the three selected value alternatives.
