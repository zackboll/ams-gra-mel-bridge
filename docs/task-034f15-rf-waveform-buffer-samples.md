# Task 034F15 — copy-owned RF WaveformBuffer samples

## Reconciliation

Starting main: `0edcdbdcf5ad441ff11094a05d2302ea90238c2f`, exactly the
reviewed SHA. PR #89 was merged into that SHA; no intervening changes, open
PRs or matching F15 branches were found. The isolated worktree is
`/home/zboll/git/ams-mel-034f15`, branch
`feature/034f15-rf-waveform-buffer-samples`. Earlier worktrees were preserved.
Evidence is disk-backed under `/home/zboll/git/ams-mel-034f15-evidence`.
Main workflow 38326370923 passed on starting main, not on the new feature.

Baseline commands: `make test-native`, `make test-python`, `make test-rust`,
`alr -C ada build`, `alr -C ada/tests run`, and Alire-environment `make check`.
Measured native 289/289, Python 134/134, Rust 87 unit/integration plus four
doctests, and library/full Ada tests. Aggregate reached its final newline
check but failed on the newly added probe's missing newline; the failure is
preserved in baseline.log rather than represented as a clean aggregate pass.

## Pinned contract and implementation

RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
The actual-header probe compiles/runs with GCC and Clang, `-O2 -DNDEBUG`,
warnings-as-errors, and checks independent of assert. Signed I8/I16 accessors
and constructors preserve values; native size/alignment are 2/2 and 4/4.
C pairs have independent layouts (I8 2/1, I16 4/2); they are never
reinterpreted or memcpy'd as native objects.

JobTransmitVector has exactly ComplexI8VecPtr and ComplexI16VecPtr.
WaveformBuffer defaults to ComplexCartesian and a null I8 pointer.
Constructors/setters/getters and explicit WaveformBuffer selection through
CommonModulations and Modulation are qualified. Copies/moves through vectors
and JobInterval preserve shared ownership, **not deep copies**. Sequential
mutation of one native shared copy is visible to another copy of that same
allocation. Weak ownership proves survival with a retained strong owner and
expiration after all strong owners are released. A weak observer retaining
control-block storage does not retain the sample-vector object.

The additive C block follows the frozen I16 record/span. Bridge tag 6 is not
a native variant index. Modulation-v4 nests frozen v3/v2/v1; interval-v14
nests frozen v8 and reuses TX-v3. Exactly one production export is added:
`ams_mel_rf_job_add_intervals_v14`, with the existing diagnostic convention.
ABI remains 0.1. V11/V12/V13 limits remain 0–2/0–4/0–5.

Presence 0 ignores payload, 1 validates/copies even empty or unused, and
other values reject. ComplexINT8/ComplexINT16 select only the respective
span; native format is fixed ComplexCartesian. Inactive width and inherited
payloads are ignored for kind 6; kinds 0–5 ignore the new payload entirely.
Span counts are sample pairs. Null/nonempty, multiplication overflow and
selected native vector max_size are checked before sample reads/copy.
All allocations and potentially throwing copy operations are inside the
preparation exception barrier. Full-batch preparation precedes one unlocked
provider Add; local owners unwind on preparation failure.

Each active definition in each Add receives a fresh ordinary shared vector
filled component by component. No normalization, scaling, clipping, wire
encoding, cache, pointer aliasing or caller-storage deleter is used.
Empty active buffers receive non-null empty vectors of the selected width.
Repeated references to one native entry may share its allocation; separate
definitions, intervals and Adds do not share allocations.

Safe Ada exposes Interfaces.Integer_8/Integer_16 sample records/arrays in C2,
distinct constructors and kind/width-guarded append/replace operations.
Configurations and their container copies own values; replacement prepares
temporary backing before assignment. Both Add names serialize through v14,
with separate final-sized private C arrays, aggregate/byte-size and
one-past-end cursor checks. Ada never shares mutable native sample storage.
Rust sys and private ctypes track raw ABI only; ProductRx remains unchanged.

Sample lifetime is not public Job lifetime: a legitimate retained native
owner keeps its vector alive. Existing facade/provider code-residency rules
still apply; safe facade dlclose with surviving code-dependent owners is not
promised. Native getters' const references do not make pointees immutable.
Caller isolation comes from fresh copy-in.

## Requirement-to-test matrix

| Requirement | Concrete evidence |
|---|---|
| Exact pinned sample/variant/ownership semantics | waveform_buffer_header_probe.cpp: signed extremes, sizes/alignment, default null state, both variant layers, JobInterval copies, shared mutation, weak survival/expiration |
| C11/by-value raw ABI and all new layouts | waveform_buffer_layout_probe.c, ctypes and Rust compiled-C comparisons; private Ada LP64 representation checks retain historical assertions |
| Supplied signed values/empty selected ownership | C focused test and Send_Waveform_Buffers observe real/imag, width/count, format, outer/inner alternatives, non-null empty |
| Interval/table/event association | Asymmetric two-interval C/Ada fixtures use different ordering/lengths/events; observe every reference, repeats/interior indices/-1 and event IDs |
| Mixed legacy behavior | Buffers plus CustomWaveform and PhaseCode; observe CustomWaveform selection, PhaseCode duration, RX/TX Stokes and phase controls |
| Caller/Ada isolation | C mutation after Add retains earlier values; Ada source scope expires before Add, source/config replacements do not change retained table/list copies |
| Fresh native allocation | Aliased C definitions remain independent under mutation; unchanged later C/Ada Add restores caller values while independently retained earlier allocation remains mutated |
| Native owner retention/release | Test-only native WaveformBuffer copies, not deep-copy caches; latest snapshot replacement/clear, sequential shared mutation, weak expiration after explicit release |
| Public Job versus sample lifetime | C fixture closes VA/Job while a legitimate C2 owner maintains provider residency; retained native sample survives until explicit test-owner release |
| Ada mutation guards and replacement | Positive append both widths, wrong-width append/replace and wrong-kind replace rejection, shorter/longer/empty replacements retain width and values |
| Preparation failure/unwind | later-sample-vector failpoint reaches a later partially filled vector after first completed buffer; INTERNAL_ERROR/no Add, test-only weak observers prove both vectors expire; trace records reach/unwind |
| Provider retains then throws | Mock assigns actual intervals before std exception; PROVIDER_EXCEPTION, caller mutation cannot alter retained samples; explicit release expires weak owner |
| Validation/recovery | Unknown/direct/VITA selector values, poisoned inactive spans/legacy fields, null/nonempty, overflow/excessive counts, malformed later buffer/event/presence, final-table indices; accepted state/Add count unchanged and same-Job recovery |
| Compatibility/presence/lifecycle | V13 rejects 6, then old-profile Tone replaces buffer state; absent poisoned tables/events and explicit empty transitions, Flush/full Cancel and VA-first closure; F11–F14 remain in normal suites |

## Failed attempts and corrections

- contract.log: probe lacked the existing vendored Boost include root;
  contract-corrected.log uses all actual build include roots and passes both compilers.
- baseline.log: new probe lacked final newline; corrected without suppressing check.
- vertical-development.log: C close fixture omitted established diagnostic arguments.
- vertical-corrected.log: reused setup omitted capability initialization; corrected
  to the established asymmetric fixture, without changing provider validation.
- vertical-fixture-corrected.log: Ada signed operators needed use-type clauses;
  source mutation needed an actual observation to avoid unused-assignment warning.
- bindings-development.log: exact Python inventory tuple omitted v14.
- bindings-corrected.log: concurrent native relinking caused invalid ELF header
  in Python subprocess checks; stable-backed binding qualification follows it.
- expanded-vertical.log / expanded-focused.log: fixture used wrong Cancel result
  type and an insufficient RX envelope path; corrected to frozen declarations.
- expanded-focused-corrected.log / expanded-crash-gdb.log: test observation after
  final provider unload found a newly loaded empty retention container. Corrected
  by retaining a legitimate C2 owner until retained test copies are released,
  not by adding a dlopen pin or weakening sample lifetime assertions.

## Validation and execution limits

Final commands/results are recorded below after qualification. Development
passes do not qualify a later edited source or a future feature-head workflow.

Pinned Squall: `b1015728f904c799fa0c07489fce48e78f67845f`.
The Job fixture supplies small Cartesian buffers of both widths and valid
references without generating samples. Job, Ada VA, C ProductRx and Ada
ProductRx run sequentially with task-owned resources/nonconflicting ports.
Receive-only Squall interval Add is a no-op: successful return is call-path
acceptance, not sample fidelity, waveform execution, hardware capability or
RF transmission. Positive fidelity/ownership evidence comes from mocks and
actual-header probes. No 32-bit runtime qualification is available or claimed.

Excluded: Real/Polar/floating samples, VITA transmit packets, CachedWaveform,
WaveformStream/TxEndpoint, public shared-owner handles, zero-copy, RDMA,
weights/registration/routing/transfer/VADB, waveform generation/resampling,
timing calculations, capability gating, vendor or unrelated lifecycle changes.

The review handoff must remain OPEN, NON-DRAFT, UNMERGED and without auto-merge.

## Final-source measured qualification

- final-native-matrix-complete.log: GCC/Clang Debug and Release each **291/291**;
  production Release builds successfully. New C checks remain active under NDEBUG.
- final-bindings.log: Release-backed Python **135/135**, compileall; Rust
  unit/integration **88** (8+2+14+16+8+8+0+32), **4 separate doctests**,
  workspace check, Clippy all-targets `-D warnings`, and fmt all pass.
- final-public-ada-release.log: public Ada focused TX executable, including
  sample fidelity, retained configuration/table/list copies, guarded mutations,
  asymmetric/mixed fixtures and native-owner retention, **50/50** with GCC
  Release native backing. final-native-release.log: C sample fixture **50/50**.
  Both executables repeat internally; no redundant outer repetition campaign.
- final-native-release-trace.log: 50 later-sample-vector reaches and 50
  preparation-unwound traces. Test-only weak observations additionally require
  both the completed first vector and partially constructed second vector to
  expire after rejected preparation, with no provider Add.
- final-audits.log: **204 AMS_MEL_0.1** production exports, sole v14 addition;
  historical declarations/constants byte-identical outside the additive block;
  **804/804** vendor hashes; new test instrumentation absent from production.

Failed final-newline checks on this new task document were corrected without
weakening the repository check. The additional final C caller-free/empty-I16
fixture was requalified in all four native configurations rather than borrowing
an earlier matrix result. final-source.sha256 records the implementation and
focused fixtures used for qualification. No 32-bit runtime result is claimed.

- final-ada-aggregate.log: `alr -C ada build`, `alr -C ada/tests run`,
  Alire-environment `make check`, and `make check-ada-format` all pass.
  Aggregate includes native 291/291, direct-GPR test build/run, preserved
  production/test build-tree isolation, formatting and newline/whitespace checks.
- final-format-check.log: formatter verification passes. Imported source-free
  GPR project warnings persist; first-party code retains warnings-as-errors.
- final-public-ada-release.log and final-native-release.log use final focused
  source; final-source.sha256 remains verified after these campaigns.

### Coverage boundaries for independent review

The new sample fixture directly tests Flush/full Cancel and VA-first closure;
Finalize/status-registration behavior and fully parent-first C2 closure continue
through the existing generic Job/F11–F14 regressions rather than every new sample
combination. The C retains-then-throws case directly qualifies sample ownership;
the public-Ada new fixture does not independently duplicate that failure case.
Ada guarded mutation coverage directly rejects wrong-width I16 append/replace
on I8 and wrong-kind I8 replacement; the symmetric guards are implemented but
not each separately exercised on every constructor. The shared-owner observer
is sequential and test-only, not a production concurrent-observation facility.
These are recorded coverage boundaries, not execution or lifetime guarantees.

- final-direct-gpr-squall.log: explicit Alire-environment direct `gprbuild`
  smoke passes, then final-source Squall Job, Ada VirtualAperture, C ProductRx
  and Ada ProductRx each pass one iteration sequentially; script concludes
  FINAL_SQUALL_COMPLETE. Ports are 53000/54000/55000/56000 bases, backing
  under task evidence/squall. Only script-owned resources are cleaned up.
  final-squall-fixture.sha256 records the Job fixture; acceptance remains
  no-op call-path evidence, not samples or transmission fidelity.
- final-audit-confirmation.log repeats vendor/frozen-header/production-export
  checks after final-source native qualification; all pass.

Publication identities and fresh push/PR checkout identities are recorded in
PR description and the final handoff. Pending hosted checks do not invalidate
local results, but neither earlier main CI nor local qualification constitutes
independent approval. The PR must not be merged by this task.
