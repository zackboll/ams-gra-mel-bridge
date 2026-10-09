# Task 034F10 — RF JobInterval transmit-event construction

## Starting state

Isolated branch `feature/034f10-rf-tx-events` starts at origin/main
`153d9580cc913303d9d2da9e2c5880f6b86f6aeb`, merged PR #84. No open PR or
unmerged remote branch was found; the original worktree was preserved.
Baseline native 282/282, Python 129/129, Rust tests/check/Clippy/formatting,
Alire Ada library/full smoke and formatting passed. Production exports were
198 under ABI 0.1; vendor inventory 804/804 matched. An initial overlapping
Alire pre-build invalidated three native executables; the serial rerun passed.
Baseline logs: `/tmp/034f10-baseline-serial.log`, `/tmp/034f10-baseline-ada.log`.

## Exact pinned contract

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f`: JobEvent.h,
JobInterval.h and RFMELTypes.h match the pinned checkout byte-for-byte.
`TransmitEvent` derives from `JobEvent`, constructs Direction::Transmit,
and has value-copy construction/assignment. JobEvent's special members are
defaulted. Sequence also has defaulted copy/move special members.

- `JobEventID = uint32_t`; `ElementGroupLabel = std::string`;
  `Frequency = double`; Femtoseconds is chrono duration<int64_t, std::femto>.
- Inherited `setEventID(JobEventID)`, `setElementGroupLabel(const ElementGroupLabel&)`,
  `setStart(Femtoseconds)`, `setDuration(Femtoseconds)`,
  `setCenterFrequency(Frequency)`, `setStabPointIndex(size_t)`.
- `setTxAtten_dB(double)`, `setRiseDuration(Femtoseconds)`,
  `setFallDuration(Femtoseconds)`,
  `setApplicableTxElementGroups(const std::vector<size_t>&)`.
- `Sequence::setTxEvents(const std::vector<TransmitEvent>&)` and
  `setRxEvents(const std::vector<ReceiveEvent>&)` copy independently.
  `getTxEvents()` returns vector& on mutable Sequence and const vector& on const.
- Default modulation vector is `{-1}` (NO_MODULATION), attenuation/rise/fall
  are zero, applicable TX groups and inherited weights are empty. No setter
  for modulation, weights or waveform resources is called.

GCC/Clang actual-header compile/runtime probes verify signatures, defaults,
signed extrema, copied labels/groups/events/sequences and RX/TX independence.
The committed header probe uses explicit checks, active in Release.
Existence of these declarations does not establish hardware TX support.

## ABI and behavior

Exactly one export: `ams_mel_rf_job_add_intervals_v9`. ABI stays 0.1.
Frozen RX event v1–v4 and interval v1–v8 declarations/exports remain unchanged.
V9 nests v8, adds uint32 presence and a borrowed TX event span. The TX event
contains uint32 event ID, borrowed string view, signed start/duration,
double center frequency, uint64 stabilization index, ordered uint64 group
span, double attenuation in dB and signed rise/fall durations.

Measured LP64 layout (all alignments 8):

| Record (ams_mel_rf_ prefix) | Size | Field offsets in declaration order |
| --- | ---: | --- |
| tx_event_config_v1 | 96 | 0, 8, 24, 32, 40, 48, 56, 72, 80, 88 |
| tx_event_config_span_v1 | 16 | 0, 8 |
| job_interval_config_v9 | 264 | 0, 240, 248 |
| job_interval_config_span_v9 | 16 | 0, 8 |

Actual compiled C11 layout probes are compared with Rust and ctypes layouts
and exact foreign-function signatures. Raw Ada records remain private to the
thick binding. All backing strings and final-sized arrays survive the entire
synchronous call and controlled string owners clean up on exceptions.

Presence 0 never reads TX span contents. Presence 1 validates structure,
size multiplication overflow, UTF-8 policy (including rejecting embedded NUL)
and provider size_t representability, then copies the full ordered vector,
including empty. Other encodings reject before provider Add. Empty labels,
duplicates and empty group vectors are valid; no relation to stabilization
points/group counts is inferred. Signed values are not quantized, normalized,
or summed; doubles follow the existing IEEE policy, without NaN-payload-bit
promises. There is no physical capability validation.

All preparation precedes the one existing JobDetail Add. Active TX calls
Sequence::setTxEvents exactly once. RX construction and F4–F9 fields remain
unchanged. The existing status-registration requirement, unlocked provider
Add, exception mapping, Flush/Finalize/Cancel gates and parent ownership apply.
No new query, callback, worker, asynchronous owner or provider resource exists.
Preparation allocation failure maps INTERNAL_ERROR without provider Add;
provider exceptions retain same-Job recovery under the existing rules.

## Safe Ada

`Create_TX_Transmit_Event` owns its label and scalar values;
`Set_Event_Stabilization_Point_Index`, `Append_Applicable_TX_Element_Group`,
`Set_TX_Attenuation_DB` and `Set_TX_Edge_Durations` modify owned configurations.
`Append_TX_Event` copies a configuration to an existing RX_Job_Interval_Config
(the established type name is retained). `Set_Empty_TX_Events` replaces with
explicit empty; `Clear_TX_Events` restores absence. Copy assignment is independent.
`Add_Job_Intervals` aliases source-compatible `Add_RX_Job_Intervals`; both serialize
through v9. No raw C pointer or native lifetime-sensitive object is exposed.

## Evidence boundaries and exclusions

Positive payload fidelity comes from the existing RF mock, not Squall.
Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` is receive-only;
JobInterval Add is a no-op. The Job integration submits a mixed configuration
solely to test acceptance while retaining existing receive activation paths.
No RF emission, waveform generation, antenna steering, hardware capability,
registration, routing or data-transfer claim follows from Add acceptance.

Deferred: non-default modulation indices/resources, Weights pointer ownership,
CachedWaveform/WaveformTxEndpoint, RDMA/registration/VADB, hardware RF execution,
MFADrivenControls/JIB/rejection callbacks and capability auto-gating. Interval
applicable-element-group behavior remains excluded due to the pinned getter
returning the endpoint map rather than its label vector. Vendor code, JobRequest
and ProductRx production code are unchanged. Original async Job ownership stays.

## Validation

Final qualification results are recorded below after completed runs. Logs use
`/tmp/034f10-*` in the task environment; local results are not hosted CI results.

- GCC Debug (`make test-native`), GCC Release, Clang 19 Debug and Release:
  full native regression **283/283** each. Final separate-tree reruns are in
  `/tmp/034f10-matrix-final.log` (all three 283/283).
- The C11 TX suite executes 50 repetitions per invocation (including Release).
  It checks TX-only/RX-only/mixed, ordered IDs in each vector, duplicate labels,
  empty/absent/poison, malformed spans/UTF-8/NUL, scalar extrema, signed zero and
  infinity, modulation/weights defaults, allocation failpoint before Add,
  provider exceptions/recovery, status registration, Flush, Finalize/Cancel and
  parent-first lifetime. It reuses the F8 fixture and retains F4–F9 observations.
- Python warnings-as-errors unittest **130/130** and compileall passed.
- Rust workspace **115 tests** passed (including 27 sys ABI tests); cargo check,
  Clippy `--all-targets -- -D warnings`, and rustfmt check passed.
  Evidence: `/tmp/034f10-final2.log` up to the successful Ada library build.
- Alire library build and full public Ada smoke passed; the TX focused suite
  executes **50/50** with TX-only, mixed, explicit empty/absence, copied configs,
  NUL rejection, exact signed/double values, defaults, prior profiles, preparation
  and provider failures, status gating and parent-first lifecycle.
  Evidence: `/tmp/034f10-qualification.log`.
- Production Release (`build/production-release`): **199/199 exact exports**,
  all versioned `AMS_MEL_0.1`, no test exports. All 198 previous export declaration
  texts and 24 frozen RX/interval record declarations match starting main exactly.
- Compiled C11/C++20 layout outputs match; Python/Rust compare every new field
  offset, size, alignment and the Add signature with the compiled C11 probe.
- Vendor inventory **804/804** matched; no vendor diff. Whitespace and final
  newlines passed. No 32-bit runtime qualification was available; indices are
  checked against actual provider size_t at runtime, not assumed 64-bit.

Early development failures (header-probe alias collision, Ada literal/nested
selector compilation, stale inventory assertions) were corrected and rerun.
A validation environment collision and a wrongly rooted Alire `make` invocation
were corrected; they are not counted as passes. An initial integration build
was invalidated by shared-tree overlap and is likewise not acceptance evidence.

- Direct-GPR smoke (`alr -C ada/tests exec -- make -C <root> test-ada`)
  and aggregate `make check` in the same Alire environment passed. Aggregate
  includes production/test tree isolation, native 283/283, GNATformat checks,
  complete public Ada smoke (TX 50/50), final-newline and Git whitespace checks.
  Evidence: `/tmp/034f10-qualification-final.log`, aggregate exit marker 0.
  Existing GPR project warnings about an imported source-free C project remain;
  all first-party compiled sources build with warnings treated as errors.

- Pinned Squall Job integration passed with the added mixed TX configuration;
  its existing receive-only TX-group mismatch test also passed. This is no-op
  Add acceptance only, not positive TX payload fidelity or hardware evidence.
- Existing Squall safe Ada VirtualAperture, C ComplexINT16 ProductRx and Ada
  ComplexINT16 ProductRx paths all passed (one iteration each).
  Logs: `/tmp/034f10-qualification-final.log` (Job) and
  `/tmp/034f10-squall-remaining2.log` (VA/C RX/Ada RX), exit marker 0.
  Immediate default-port retries rejected the health port while its previous
  connection was in TCP TIME_WAIT; subsequent serial runs used distinct
  configured port sets (22xxx, 23xxx, 24xxx), without touching unrelated containers.

All evidence above is local. Hosted CI status is reported on the opened PR,
not inferred from local passes. Review gate: leave the PR open and unmerged,
with auto-merge disabled. Remaining risks are the unqualified 32-bit runtime,
provider-specific semantic/hardware validation, pinned C++ ABI compatibility,
and existing documented async/callback retention contracts; this task adds no
new provider resource ownership or transmission guarantee.
