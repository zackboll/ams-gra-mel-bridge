# Task 034F14 — copied RF CustomWaveform profiles and patterns

## Reconciliation and baseline

Starting main `c30ddc216bd9c597018d6763c7e7b4814b9951d8` includes merged
PR #88. Resumption fetched origin once: main remained unchanged, with no
matching remote F14 branch or open PR. Existing branch
`feature/034f14-rf-custom-waveform-values` and worktree
`/home/zboll/git/ams-mel-034f14` were reused. Earlier worktrees and unrelated
modifications were preserved; no reset, amend, rebase or force push was used.

Completed baseline evidence in `/home/zboll/git/ams-mel-034f14-evidence` was
reused: native 287/287; Python 133/133 and compile checks; Rust 86 unit/integration
plus four separate doctests, check/Clippy/fmt; Alire library/full Ada, formatting,
direct-GPR smoke, aggregate check and build-tree isolation. Production baseline
was 202 AMS_MEL_0.1 exports; vendor inventory 804/804. Main workflow 38198049679
passed on the starting main; it does not qualify F14.

The preserved actual-header probe initially passed GCC/Clang Release/NDEBUG
with explicit checks. Its only later change adds a required final newline.
No vendor header is modified. RF MEL is pinned to
`762ce84c5555dd0f3ea66f36b321fecf8839b89f`.

## Contract and execution boundary

`WaveformProfileParams` defaults to zero signed femtosecond duration, six false
update flags and six zero double scalars. The bridge records all thirteen
fields independently. Phase/delta phase are **degrees**, frequency offset/delta
frequency are Hz; frequency_slope_hz_sec maps verbatim to the pinned
get/setFrequencySlopeHzSec interface. False flags do not erase nonzero values.
Finite values, signed zero, infinities and NaN classification are forwarded;
NaN payload bits are not promised. No wrapping, conversion, normalization,
clamping or finite-only validation occurs.

`WaveformProfileIterations` has two native int fields: iterations default 1,
index default 0. Despite its introductory comment, it has no chip duration.
Duration belongs to the profile. Signed iteration values, including zero and
negative values, are copied without an invented repeat interpretation.
Every step index must fit native int and identify a profile in THIS definition;
there is no inner -1 sentinel. Reference checks apply even for nonpositive counts.

Event patterns and CustomWaveform own ordinary vectors. Constructors, setters
and ordinary copies copy their contents. Empty vectors and empty patterns are
preserved, as are order, repeated indices and duplicates. Empty profiles are
valid only without actual steps. Identical source addresses never merge entries.
Top-level TX references retain -1 as no-modulation and same-interval resolution;
multiple events may share CustomWaveform. OOK exclusivity remains OOK-only.

Pinned comments describe amplitude reduction in range 0–1, executable duration
quantization, combined chip duration matching an event, and cycling the outer
pattern list. These are application/provider execution obligations, not bridge
validation or execution promises. This bridge does not multiply duration by
iterations, sum timing, assign patterns to events, expand steps, require equal
pattern/event counts, generate samples or execute update flags.

Excluded: WaveformBuffer/IQ ownership, CachedWaveform, WaveformStream,
WaveformTxEndpoint, ModulationExtensionBase/pointer escape hatches, weights,
RDMA, registration, routing, transfer, VADB, waveform/timing algorithms,
hardware execution, capability auto-gating, vendor changes, unrelated lifecycle
refactoring, public Python/safe Rust expansion and applicable-group getter fixes.

## ABI, construction and ownership

ABI remains 0.1. The sole production addition is
`ams_mel_rf_job_add_intervals_v13`; bridge CustomWaveform tag is 5, not a native
variant index. Modulation-v3 nests frozen v2, which nests frozen v1. The nested
original kind is the sole discriminator. Tags 0–4 ignore custom spans entirely;
tag 5 ignores all older payload fields. V11 accepts only 0–2, v12 only 0–4,
v13 only 0–5. V13 nests frozen interval-v8, not v12, with one modulation-v3
span and unchanged TX-v3 span. Arrays are accessed with actual element strides.

Outer presence 0 ignores payload, 1 validates/copies including explicit empty,
other values reject. Active tables validate even when unreferenced or TX is
absent. Each active nested span is checked before traversal with its actual
record size. Each flag must be exactly 0/1. Complete-batch validation/preparation
precedes one unlocked provider Add using the existing typed builder. OOK
validation scratch remains inside the exception barrier. Existing terminal,
status, exception and once-per-active-field setter behavior is retained.

Native preparation creates actual profile/iteration/pattern/CustomWaveform
values and explicitly selects CommonModulations then Modulation by type.
All containers own their copies before provider Add. Observers use independent
mutable copies for non-const getters (not const-casts of provider state).
Borrowed C spans need remain valid throughout the synchronous call only.

Public Ada has private owned `Waveform_Profile`, `Waveform_Profile_Table`,
`Event_Waveform_Pattern`, `Waveform_Pattern_List`, a distinct nonnegative
`Waveform_Profile_Index`, paired Value/Update setters with explicit degrees,
append operations, Custom_Waveform and kind-guarded replacements. Final
relationships validate at submission, permitting independent assembly.
Shrinking/replacing profiles does not renumber or repair steps. Wrong-kind
replacement raises Constraint_Error before mutation. Both existing Add names
serialize v13 using final-sized separate profile/pattern/step arrays alongside
existing backing. Aggregate lengths, native-int/size_t conversions, multiplication
and one-past-end cursor capacity are checked before address publication.
Backing lives through the synchronous call and unwinds normally on exceptions.

## Measured LP64 layouts

Compiled C probe and Python/Rust comparisons cover sizes, alignments and every
new field offset; Ada uses existing representation-test infrastructure.
No 32-bit runtime claim is made.

| Record | bytes / alignment | offsets in declaration order |
|---|---|---|
| profile params | 80 / 8 | 0,8,12,16,20,24,28,32,40,48,56,64,72 |
| profile iterations | 8 / 4 | 0,4 |
| event pattern | 16 / 8 | 0 |
| custom config | 32 / 8 | 0,16 |
| modulation v3 | 104 / 8 | 0,72 |
| interval v13 | 288 / 8 | 0,240,248,264,272 |
| all new spans | 16 / 8 | 0,8 |

The C _Generic signature check qualifies by-value outer span; Rust assigns the
function to the exact extern-C signature and Python checks argtypes/restype.

## Requirement-to-test matrix

| Requirement | Concrete observation/assertion |
|---|---|
| pinned defaults/setters/copy chain | custom_waveform_header_probe.cpp main CHECKs through source vectors, patterns, CustomWaveform, both variants, table, JobInterval and retained independent interval copy |
| all 13 profile fields, both step fields, nested association | test_rf_job_custom_waveform.c main loops over two intervals/two definitions, profiles and every pattern/step using mock_rf_f14_value/double |
| source mutation independence | native fixture mutates source phase/iteration then observes retained 90/INT32_MIN |
| flags, false + nonzero, degrees, signed/IEEE values | native per-field flags/scalar classification assertions; Send_Custom_Waveforms public Ada numeric/flag assertions including all-false IEEE profile |
| public Ada ownership and expired scopes | Send_Custom_Waveforms Saved list submits after local profile/table/pattern/config sources expire, distinguishing retained and modified sources |
| append and replace results | public Ada observed 7/8 profiles, 3/4 patterns, appended step index 7, shorter/longer/empty/restored replacements |
| wrong-kind unchanged value | public Ada replacements reject then Submit_One observes original Tone kind |
| stale local references and recovery | native reject_custom and Ada shortened table assert unchanged Add count/observations, restored profiles submit successfully |
| invalid flags/references/nested spans/later definitions | native reject_custom checks INVALID_ARGUMENT, unchanged Add count and accepted phase/profile observations |
| partial preparation | later-custom-step hook after preceding pattern, INTERNAL_ERROR/no Add/accepted observation recovery; Ada provider-error mapping and same-Job recovery |
| old-kind v3 access paths | native kinds 0–4 with poisoned custom spans, retained kind/CPFSK duration observations; inherited full C/Ada suites retain prior alternatives/Stokes/radians/lifecycle |
| old profiles | native v12 rejects tag 5; unchanged v11/v12 focused suites check their limits |
| empty/presence | native empty profiles with three retained empty patterns; absent poisoned payload and explicit empty outer table |
| raw layouts/exports | custom_waveform_layout_probe.c; Python test_private_custom_waveform_abi; Rust custom_waveform_abi; Ada Check_Modulation_Representation; exact inventory tests |

F13's non-blocking debt remains disclosed: no exhaustive historical lifecycle
cross-product, all setter-trace presence combinations or every historical
wrong-kind mutation permutation is newly claimed. F14 focused tests are not
an assertion that every possible IEEE/ownership/presence permutation is tested.

## Development failures preserved

- custom-native-development.log: initial fixture omitted inherited VA capability
  initialization, producing VA capability mismatch before submission. Fixture
  initialization corrected; no validation was weakened.
- ada-custom-development.log: incorrect Add name spelling caused compile errors.
  Corrected to existing Add_Job_Intervals / Add_RX_Job_Intervals.
- ada-fixture-retry.log: relative GPR path failed because alr -C changes directory;
  corrected to absolute project path.
- python-layout-development.log: binding inventory order differed; corrected.
  python-layout-corrected.log ran during Alire test-library rebuilding and failed
  to load the transiently absent library; later stable layout run passed.
- native-matrix.log: Clang rejected missing final newline in new C fixture.
  Newline corrected; checks/warnings remain intact. Preserved header probe also
  received its missing final newline, with no behavioral rewrite.
- full-bindings.log: selected production library lacked test hooks and one exact
  inventory test still expected 202. Corrected runner selects test-enabled library,
  and inventory assertion expects the sole added export (203).

Final-source commands/results and publication identities follow below when
qualification is complete. Earlier development runs are not final-head evidence.
- full-ada-aggregate.log: direct-GPR warnings-as-errors rejected constant-range
  native-int comparisons on the current 32-bit C int target. The comparisons
  were moved into a widened Integer_64 helper, retaining portable range checks
  without suppressing warnings. Renewed final qualification uses that helper.
- strengthened-native.log: a newly added terminal-gate test omitted the Cancel
  output argument and failed C compilation. Corrected fixture passes 50/50;
  production Cancel contract was unchanged.

## Final-source measured qualification

Disk-backed evidence: `/home/zboll/git/ams-mel-034f14-evidence`.
Final fixture/implementation hashes are retained in final-fixture.sha256 and
final-implementation.sha256. No compiled source changed after these campaigns.

- final-native-matrix.log: CMake builds and CTest for GCC/Clang Debug/Release,
  **289/289 each**, followed by production Release build. Focused C checks
  remain active under NDEBUG and internally repeat 50 times.
- final-ada-squall.log: `make format-ada`, `alr -C ada build`,
  `alr -C ada/tests run`, Alire-environment `make check`, formatting check and
  direct-GPR smoke all passed. Aggregate validates isolation and native 289/289.
  Public Ada TX/RX focused executable selected with GCC Release native library
  passed **50/50**, including Send_Custom_Waveforms; no redundant outer repeat.
  Focused native CustomWaveform executable passed **50/50**. Native release
  trace records 50 later-custom-step reaches and 50 preparation-unwound events.
- final-bindings.log: Python warnings-as-errors **134/134**, compileall passed;
  Rust workspace unit/integration groups 8+2+14+16+8+8+0+31 = **87**, with
  **4 doctests separately**, workspace check, Clippy all-targets -D warnings,
  and fmt check passed. bindings-release.log independently uses stable Release
  native backing and records the same totals/checks.
- final-audits.log: **203 AMS_MEL_0.1** exports; sole v13 addition, vendor
  **804/804**, historical C declarations/constants byte-identical outside the
  additive block, test instrumentation absent from production. Git whitespace
  and first-party final-newline checks passed.
- Final-source pinned Squall Job, Ada VirtualAperture, C ProductRx and Ada
  ProductRx each passed one script iteration sequentially. Task-owned Squall
  backing is under evidence/squall; port bases 49000/50000/51000/52000.
  The Job fixture uses one 1,000,000 fs profile and one iteration, consistent
  with its 1,000,000 fs TX event. Only script-owned resources were cleaned up.
  Normal return is call-path acceptance only: receive-only Squall Add is a no-op,
  not profile fidelity, synthesis, hardware capability or RF emission evidence.

GCC 14.2, Clang 19, Alire 2.1.1 and existing pinned Ada tooling were used.
Imported source-free GPR project warnings persist; first-party compilation
retains warnings-as-errors. No 32-bit runtime qualification was available or
claimed. No physical transmission, waveform execution or NaN payload-bit claim.
Fresh feature/PR workflow identities are recorded in the review handoff, not
borrowed from earlier main/F13 green results. The PR must remain open,
non-draft, unmerged and without auto-merge for independent review.
