# Task 034F17 — copied RF JobInterval user context

## Reconciliation

Fetched starting main is `d90a191d5d2f8ccd763148a82fe190270b027445`, exactly
the reviewed main and PR #91 merge commit. No intervening commits, open PRs,
or matching F17 work were found. Earlier worktrees remain untouched. Work is
isolated in `/home/zboll/git/ams-mel-034f17` on
`feature/034f17-rf-interval-user-context`; disk-backed evidence is under
`/home/zboll/git/ams-mel-034f17-evidence`. Main workflow 38337475016 was
inspected once and was pending; no baseline polling or restart was performed.

Baseline measured native 293/293, Python 136/136, Rust 89 unit/integration
tests and four doctests, and successful Alire library/full Ada suite.
The baseline aggregate invocation failed because `alr exec` changed directory
to the Ada crate and `make check` lacked `-C`; that attempt remains recorded
in baseline.log. Subsequent invocations explicitly select the repository.

## Pinned concrete contract

RF MEL revision is `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
JobInterval accepts `const std::any&` and copies it into a member; its default
copy constructor copies that member. The bridge selects exactly
`std::vector<std::uint8_t>` because it is an owned, ordered binary value with
ordinary value-copy semantics. No pointer, span, custom deleter, aliasing
shared_ptr or caller-storage reference is stored.

The upstream interface is type-erased and application-defined, **not a
universal RF MEL context schema**. A provider must know and support the exact
concrete type selected here. The pinned getter dereferences pointer-form
`std::any_cast` without a null check. Its comment promising nullopt on an
incompatible type is not a safe executable contract: absent/wrong-type access
can invoke undefined behavior. Tests call the typed getter only after a fixture
definitively supplies the selected vector. Absent/present observations use
test-only setter traces, never speculative getter calls. Vendor files are
unchanged.

The actual-header probe passed GCC and Clang in Debug and `-O2 -DNDEBUG`
with active explicit checks and warnings-as-errors. It demonstrates any
construction, setter copies, JobInterval copies, original-vector mutation and
scope expiration, independent mutation of a copied interval, and present-empty
vectors. The first compile omitted the pinned Boost include path and failed;
contract-probes.log preserves it, while contract-probes-corrected.log records
the corrected include closure and four successes.

## ABI and preparation

ABI remains 0.1. Exactly one production operation is added:
`ams_mel_rf_job_add_intervals_v16` (205 → 206 exports). Interval v16 nests
frozen v8, with one TX-v3 collection and one modulation-v5 table. Previous
declarations, strides, enum values and tag limits remain unchanged; v16
supports modulation kinds 0–7. Private Ada, raw Rust sys, and private ctypes
mirror the C11 record and by-value outer span.

`has_user_context` is exactly 0/1. Zero ignores the complete span, including
poisoned pointer/count, and does not call the setter. One validates and copies
every ordered byte, including explicit empty. Other flags reject before Add.
Null/nonempty, span extent overflow and vector max_size are checked before
copying. Counts are bytes. No UTF-8 decoding, NUL termination, normalization,
endian swapping, sorting or deduplication occurs.

Each interval receives a fresh vector and concrete any, then exactly one
setter call. All intervals are structurally validated and prepared before the
existing single unlocked provider Add. Existing registration, terminal gates,
diagnostics, exception barriers and same-Job recovery are retained. Local
preparation failure leaves provider accepted state unchanged. Once provider
Add is entered, rollback is not promised: a provider may retain copies and
then throw.

The pinned `RFMFAInfo::getMaxNumUserDefinedContextBytes()` is a capability
limit, not a bridge allocation limit. Add performs no hidden information
query or cache. Caller/provider must apply the relevant limit and decide
whether the context is interpretable or executable. Representable copied
length alone proves neither.

## Safe Ada ownership

`RX_Job_Interval_Config` privately owns bytes and an independent presence
Boolean. New intervals start absent. Set prepares temporary storage before
moving it into the value; append sets presence after successful insertion;
Set_Empty replaces content with present empty; Clear clears content and
presence. Interval/list copies own independent values. Both existing public
Add names serialize privately through v16.

Final-sized task-local context backing is separate from Activity ID storage.
Checked totals, size_t byte extents and one-past-final cursors precede address
publication. Inactive context contributes no backing counts and is not
traversed. Empty active context publishes a zero span with flag one. Backing
stays live through synchronous Add. No native any, raw address or C type is
exposed publicly. Activity ID remains the existing identity field; user
context is a distinct application-defined value.

## Qualification and remaining obligations

Normal regression paths include active-check C and public Ada 50-iteration
fixtures. Mock observations use actual retained JobIntervals, not reconstructed
inputs. Fixtures cover binary `[00,80,FF,01,00]`, present empty, a 257-byte
ordered repeated pattern, asymmetric absent intervals, same-source borrowing,
caller mutation/free, source scope expiration, independent interval/list
copies, replacement/append/clear and unchanged resubmission. The mixed fixture
keeps Activity ID, WaveformStream, WaveformBuffer, CustomWaveform/PhaseCode,
TX references and RX/TX controls. Existing lifecycle regressions remain active.

Deterministic later-context preparation and partial-copy hooks prove hook
reach and exception unwinding; null/nonempty, invalid flags, oversized spans,
inactive poison and late legacy validation reject before provider Add.
Failed qualification attempts are retained, including the misplaced additive
header block, missing test include, and stale Rust inventory count; these are
implementation defects corrected without weakening inherited tests. Additional
attempts exposed Ada warning/operator visibility errors and a non-const pinned
WaveformProfileParams getter in the new mock observer; the observer now copies
the profile before reading it, consistent with existing observer patterns.
A 30-second shell wait timed out; validation processes and their logs continued
uninterrupted. No test failure was erased or turned into a passing result.

### Final-source measured evidence

Toolchains: GCC 14.2.0, Clang 19.1.7, GNAT 16.1.0 through Alire 2.1.1,
Rust/cargo 1.98.1, Python 3.13.5, CMake 3.31.6. All first-party compiled
sources keep warnings-as-errors; the inherited source-free native GPR import
emits its existing project warning.

- final-native-matrix.log: GCC Debug/Release and Clang Debug/Release each
  pass **295/295**, including the C context fixture's internal **50/50**
  ownership/recovery loop and **50/50** mixed-fixture loop. Release checks
  remain active; all configurations use independent disk-backed build trees.
- final-context-release.log repeats final-source GCC Release C qualification:
  mixed fixture **50/50**, owned context/recovery fixture **50/50**.
- production-audit.log: production Release has exactly **206** exports,
  **AMS_MEL_0.1 only**, and no test instrumentation.
- final-validation.log: Python **137/137** and compile checks; Rust **90**
  unit/integration tests plus **four separately run doctests**, cargo check,
  Clippy `-D warnings` and cargo formatting all pass. Alire library/full Ada
  suite passes with the final focused source.
- qualification-sixth.log: native **295/295** and full public Ada suite pass
  after stronger mixed-payload checks. The public TX executable repeats the
  context scenarios internally **50/50**, with checks active.
- final-public-ada-release.log: the final-source public Ada TX executable
  passes **50/50** against GCC Release native facade and mock backing.
- source-audits.log: **804/804** vendor hashes and every frozen C declaration
  and constant byte-identical outside the sole additive v16 block. Exactly
  one export-map entry is added; old modulation strides are untouched.
- final-validation.log: aggregate `alr -C ada exec -- make -C` repository
  `check` passes, including native **295/295**, formatter verification,
  direct-GPR build/run, newline/whitespace and production/test build-tree
  isolation. Explicit subsequent direct `gprbuild` and smoke run also pass.
- layout-final.log: actual C11 v16 size/alignment **312/8**, offsets
  **0,240,248,264,272,288,296**; outer span **16/8**, offsets **0,8**.
  The C compiler checks the by-value signature. Rust and ctypes compare every
  measurement directly with this executable; private Ada checks the same
  actual platform representations alongside unchanged v15 checks.
- final-source.sha256 identifies qualified implementation and fixture content.
  No safe Rust or public Python operation is introduced.

The C borrowed-source observer additionally verifies distinct vector backing
for two active intervals borrowing one array, mutates one retained vector and
observes the other unchanged, then restores from unchanged caller configuration
through another Add. The retained-then-throws case proves that a provider may
accept copies and throw without rollback. Ada preparation failures directly
check unchanged Add count, retained bytes, deterministic hook reach/unwind and
same-Job recovery. Mixed observations read actual WaveformBuffer samples,
CustomWaveform profile values, PhaseCode duration, TX references and RX/TX
polarization/phase controls, as well as context and Activity ID.

Coverage limits: complete 32-bit bridge qualification is unavailable. Active
byte-span element width is one, so multiplication by its width cannot itself
overflow; oversized SIZE_MAX and above-vector-capacity counts are rejected.
Older-context non-setter behavior is explicitly traced for v15 and follows the
compile-time inactive branch for earlier profiles, whose existing regressions
remain unchanged. The fixtures reuse historical lifecycle regressions rather
than multiplying the complete lifecycle cross-product by context values.

- final-validation.log concludes **FINAL_SQUALL_COMPLETE** and
  **FINAL_VALIDATION_COMPLETE**. Final-source pinned Squall Job, Ada
  VirtualAperture, C ProductRx and Ada ProductRx each pass **one iteration**,
  sequentially with task-owned evidence backing and port bases
  **53000/54000/55000/56000**. Only script-owned resources are cleaned up.
  The Job fixture includes `[00,80,FF,01,00]`; acceptance is not fidelity.

Pinned Squall revision is `b1015728f904c799fa0c07489fce48e78f67845f`.
Its RF provider is receive-only and interval Add is a no-op. A normal return
is only call-path acceptance, never evidence of context preservation,
ProductRx metadata delivery, processing, transmission or RF emission.
Positive fidelity comes from actual-header and mock-backed tests. Complete
32-bit bridge/mock and cross-language qualification is unavailable and is
not claimed.

## Explicit exclusions

No context parsing/schema interpretation, ProductRx metadata insertion or
callback-format expansion, RDMA, buffer registration/endpoint ownership,
CachedWaveform, WaveformTxEndpoint, Weights, ModulationExtensionBase,
hardware transfer/waveform execution/RF emission, hidden capability queries,
vendor edits, unrelated lifecycle refactoring, public Python or safe Rust API
expansion. Publication stops at an open, non-draft, unmerged independent PR
review gate with auto-merge disabled.
