# Task 034F13 — copied RF OOK and CPFSK modulation values

## Reconciliation and scope

Starting origin/main: `d3ca38512ef3b82ef43bc014508c66727efd1d4c`, exactly
reviewed main. PR #87 is merged, including its corrective Ada qualification.
No intervening changes, open PRs or matching F13 remote branch were found.
The isolated worktree `/home/zboll/git/ams-mel-034f13` uses
`feature/034f13-rf-binary-modulations`. Earlier worktrees and local work remain
intact. Main hosted run 38193486057 passed; it does not qualify this feature.
Disk-backed local evidence is under `/home/zboll/git/ams-mel-034f13-evidence`.
Native/Alire writers of the ordinary task build trees are serialized; the
compiler matrix uses separate task-owned trees.

This constructs copied configuration values, not waveform synthesis,
modulation/scheduling algorithms, IQ samples, hardware transmission or RF
emission. Squall is receive-only and interval Add is a no-op.

## Exact pinned contract

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` is unchanged. Actual
CommonModulations.h, JobEvent.h, JobInterval.h and RFMELTypes.h define:

- OnOffKeying defaults to empty `vector<bool>`. Its constructor takes a mutable
  vector reference and copies it; setPulseEnabled copies a const vector;
  addPulseEnabled appends a bool; getPulseEnabled is const-qualified.
- CpfskPulse defaults to empty chips, zero pulse/chip durations and zero Hz
  offset. Its constructor and setChipData copy chips. Duration setters/getters
  use `chrono::duration<int64_t, femto>` exactly; Frequency aliases double.
  Its chip, duration and frequency getters lack const qualification. Test
  observers make independent mutable copies, never cast away constness.
- CpfskPulse is a declared CommonModulations alternative despite the
  introductory CustomWaveform comment. No CustomWaveform dependency is added.
- CommonModulations is the inner variant, Modulation the outer. Both are
  selected explicitly by type. Bridge tags 3/4 are not native variant positions.
- JobInterval setModulations copies; getModulations has const/nonconst forms.
  setSequenceRepeatCount uses native size_t and getSequenceRepeatCount returns
  size_t. No infinite-repeat interpretation of zero is invented.

The actual-header GCC 14/Clang 19 probe uses `-O3 -DNDEBUG -Wall -Wextra
-Werror` with explicit checks. Source vector → selected value → inner variant
→ outer variant → interval table → JobInterval copy survives mutation and
destruction of the earlier objects. It is also a normal CTest runtime target.

## ABI and active fields

ABI remains 0.1. Exactly one production function is added:
`ams_mel_rf_job_add_intervals_v12`; production exports increase 201 → 202.
Modulation-v2 nests frozen v1, then the existing u32 span, signed int64 pulse
duration and double frequency offset. V12 nests **v8**, not v11, and contains
one optional TX-v3 collection and one optional modulation-v2 table. Arrays are
accessed by their declared type, not reinterpreted to a different stride.
All preceding declarations/constants/exports remain frozen, including v11's
three-alternative contract and rejection of tags 3/4.

| Kind | Active payload |
|---|---|
| Tone (0) | none |
| PhaseCode (1) | v1 chip duration, ordered phase radians |
| LFM (2) | v1 frequency extent Hz |
| OOK (3) | binary_data |
| CPFSK (4) | binary_data, v1 chip duration, pulse duration, offset Hz |

Inactive fields, including poisoned spans, are ignored. Unknown active tags
reject. Binary data is an **unpacked ordered uint32 sequence**: each element
is exactly 0 or 1, and size counts logical values, not bytes or packed words.
The bridge validates pointer/count and multiplication bounds before reading,
then constructs vector<bool> element by element. No Boolean proxy/storage
escapes the facade. Signed int64 femtoseconds and IEEE Hz are forwarded without
ratios, trajectories, sample counts or physical limits. Signed zero, finite
values, infinities and NaN classification are preserved; NaN payload bits are
not promised.

Presence 0 ignores the whole payload, 1 validates/copies including empty,
other encodings reject. Active unused table entries are validated. Absent
references retain {-1}; explicit empty remains empty; order, repetitions and
interior -1 values survive. Nonnegative references resolve only locally.

### Final OOK relationships

Every active OOK length equals the final interval's safely representable
sequence repeat count, including unused definitions. Empty/zero is structural
consistency only. One OOK table index may be referenced by at most one distinct
TX event **position**, not ID/label. Repeated references in that event remain
valid. Equal-valued separate entries, even with aliased raw source arrays, are
independent; intervals are independent. No other alternative is exclusive.
Validation scales with supplied entries, bits and references, not a computed
schedule. Owner scratch allocation is inside the existing exception barrier.
The complete batch is validated/prepared before one unlocked provider Add;
rejection never partially submits an earlier interval. Active setModIndex,
setModulations and setTxEvents retain their once-per-field behavior.
Existing terminal/status/lifetime gates and exception mapping are reused.
There is no rollback guarantee after entering a mutating provider operation.

## Safe Ada and raw layouts

Modulation_Boolean_Array, On_Off_Keying and CPFSK_Pulse own copied values.
Append_OOK_Pulse_Enable/Replace_OOK_Pulse_Enables and
Append_CPFSK_Chip/Replace_CPFSK_Chips check kind before mutation; replacement
uses temporary owned storage. Existing constructors/table/reference APIs,
zero-based indices and No_Modulation remain source-compatible. Neither
construction nor table assembly validates incomplete final relationships.
Both Add names flatten through v12; native INVALID_ARGUMENT becomes
Constraint_Error before provider Add. Boolean backing is a final-sized u32
array with 0/1 elements, independent of phase/reference/RX/TX Stokes offsets.
Checked counts/conversions and cursor bounds precede pointer publication;
all backing lives through the synchronous call and unwinds on exception.

Raw private Ada, Rust sys and Python ctypes mirror the additive records and
by-value outer-span signature; no safe Rust/public Python API is added.
Compiled LP64 C measurements (bytes) are:

| Record | size/alignment | field offsets in declaration order |
|---|---|---|
| modulation-v2 | 72/8 | 0, 40, 56, 64 |
| modulation span-v2 | 16/8 | 0, 8 |
| interval-v12 | 288/8 | 0, 240, 248, 264, 272 |
| interval span-v12 | 16/8 | 0, 8 |

C compilation qualifies the signature; Rust/ctypes compare every new field
against the compiled C probe. Ada's existing private child infrastructure
checks the new record sizes/alignments/positions with initialized records.
Conditional LP64 checks do not establish narrower-target runtime evidence.
32-bit runtime qualification is unavailable.

## Requirement-to-test evidence

| Requirement | Observation path |
|---|---|
| Pinned deep copies/defaults/setters/variant layers | binary_modulations_header_probe.cpp, GCC/Clang Release runtime |
| Actual retained ordered Boolean/scalar payloads | test_rf_job_binary_modulations.c → mock_rf_f13_value/double |
| Public Ada values and scope-exit ownership | Send_Binary_Modulations in ams_mel_rf_tx_events_focused.adb |
| Different interval counts/table/event associations | two-interval C and public-Ada asymmetric fixtures |
| Repeated/interior references, equal distinct OOK definitions | C and Ada actual retained reference observations |
| RX/TX Stokes retained beside binary values | C f5/f11/f10 observers and public-Ada f5/f11 observers |
| Exact count, duplicate-ID exclusivity, large mismatch | C pre-Add rejection/call counter; Ada Constraint_Error/recovery |
| Binary 2/UINT32_MAX, bad spans/tags/flags, inactive poison | focused C fixture |
| 63/64/65, empty/single, IEEE classifications, signed extrema | C and public-Ada fixtures; explicit checks active |
| Positive append/shorter/longer/empty replacement, wrong kind | public-Ada submission and retained-value observations |
| Later-definition/vector allocation failures | test-only failure hooks; INTERNAL_ERROR/no Add/recovery |
| Frozen v11 rejects binary tags | focused direct-C calls; unchanged F12 fixture remains active |
| Older behavior/lifecycle/provider exceptions | unchanged F11/F12 and full native/Ada suites |
| New sizes/alignments/all offsets, outer by-value span | compiled C probe, Rust/ctypes comparison, Ada representation child |
| Production inventory/frozen declarations/vendor | exact baseline comparison, nm audit, 804 SHA-256 hashes |

Focused C and Ada suites repeat internally 50 times; no redundant outer
50-repeat campaign wraps them. Added observations and hooks are confined to
test-only provider/failpoint builds and do not add production exports.

## Executed results and failed attempts

Baseline log: native 285/285; Python 132 plus compileall; Rust 85 unit/integration
plus **4 separate doctests**; Alire library/full Ada, formatting and direct-GPR
smoke passed. Historical numbers are not substituted for executed evidence.

Development: new/unchanged F12 C focused fixtures passed; public Ada focused
50/50 passed after correction. Python 133; Rust 86 unit/integration + 4 doctests,
workspace check, Clippy all-targets -D warnings and formatting passed.
Production Release audit: 202 AMS_MEL_0.1 exports, sole addition v12;
233 preceding records, 201 function declarations and 388 constants unchanged;
vendor 804/804. Final qualification results are recorded below before handoff.

Failed attempts are preserved, not silently discarded:
- Two standalone probe compilations used incorrect Boost include roots; the
  actual CMake vendored root fixed the command. Both compilers then passed.
- First Ada focused run exposed INVALID_ARGUMENT → Provider_Error at the
  generic Check boundary. The v12 submission boundary now raises Constraint_Error;
  the unchanged helper's behavior for other operations is not broadened.
- First Python run failed its stale exact function-name inventory; updated
  only for the sole additive export, then full suite passed.
- Clang rejected the new C fixture's missing final newline. Fixed the files;
  the failed native-matrix.log remains. GCC/Clang four-tree rerun passed
  287/287 each; subsequent fixture strengthening requires final-source rerun.
- The first foreground Ada build command exceeded the tool's 30-second
  capture window. Later builds run with disk-backed background logs.

Toolchains: GCC 14.2, Clang 19.1.7, GNAT 16.1.0/GPRbuild 26.0.1,
GNATformat 26.0.0, Alire 2.1.1, Python 3.13.5, Cargo 1.98.1.
Existing source-free imported GPR warnings remain; compiled first-party
sources build with warnings treated as errors.

## Provider semantics and explicit exclusions

The application/provider still owns capability, execution and physical
meaning. No hidden query or automatic capability gate is introduced.
Squall pinned `b1015728f904c799fa0c07489fce48e78f67845f` Job acceptance,
VirtualAperture and ProductRx paths are separate call-path/receive evidence,
never binary payload fidelity, modulation execution or emission evidence.
Positive payload fidelity comes from actual-header and mock-backed C/Ada tests.

Excluded: CustomWaveform/profile-pattern machinery; WaveformBuffer/IQ ownership;
CachedWaveform, WaveformStream, WaveformTxEndpoint; ModulationExtensionBase and
native pointer escape hatches; weights, RDMA, registration, routing, transfer,
VADB; modulation/scheduling algorithms and physical RF execution; capability
auto-gating/hidden queries; vendor changes; unrelated lifecycle or historical
test-reliability refactoring.

Publication must remain OPEN, NON-DRAFT, UNMERGED, AUTO-MERGE DISABLED.
Independent review is not self-certified. Pending fresh CI is PENDING_CI,
not failure and not qualified by earlier-task hosted green runs.

### Final-source measured qualification

`final-source-matrix.log`: GCC/Clang Debug/Release each **287/287**;
Release-selected public Ada focused **50/50**. `final-bindings.log`: Python
**133/133**, compileall; Rust **86 unit/integration + 4 doctests**, check,
Clippy all-targets -D warnings and fmt check. `audits.log`: exact 202 exports,
sole v12 addition, frozen 233/201/388, vendor 804/804, binary failpoint strings
absent from production. `f13-preparation-trace.log` confirms 50 reaches of each
new failure hook and preparation-unwound traces; no provider Add and recovery
are checked in the fixture. These traces also contain inherited fixture activity.

The first aggregate attempt stopped at the formatter gate because the final
Ada fixture additions followed the prior formatting run. That failure is
preserved in full-ada-aggregate-squall.log. make format-ada was rerun before
renewed Alire/build/test validation; final-ada-aggregate-squall.log records the
corrected full campaign. No formatting check was weakened.

Remaining evidence limits: no 32-bit runtime; no physical RF execution; no NaN
payload-bit guarantee. The focused scenarios are substantial but are not an
exhaustive cross-product of every historical lifecycle mode and new alternative.
Specific exhaustive v12 setter-trace presence combinations, final-table
replacement cases and every wrong-kind mutation permutation are not separately
claimed beyond the focused and inherited suites. Independent review must assess
these coverage limits against the task's full qualification requirements.

Final corrected Alire library/full suite, Release-selected public Ada 50/50,
direct-GPR smoke, GNATformat and aggregate make check all passed; the log has
FINAL_FULL_ADA_AGGREGATE_COMPLETE, including build-tree isolation and native
287/287. Final-source pinned Squall **Job, Ada VirtualAperture, C ProductRx and
Ada ProductRx** ran sequentially and passed (one script iteration per path).
Task-owned evidence/squall backing used port bases 45000/46000/47000/48000;
only script-owned resources were cleaned up. The Job fixture contains one
OOK enable matching default repeat count one, used only by its one TX event,
and a three-chip CPFSK value beside the original modulation/RX configuration.
Normal return establishes acceptance only; Squall Add is a no-op. No waveform,
modulation execution, hardware capability, payload fidelity or emission claim
is made from these runs. Check final-ada-aggregate-squall.log for actual paths.
