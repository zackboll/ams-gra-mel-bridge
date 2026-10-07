# Task 034F5 — RF RX JobEvent controls

## Starting point

Fetched origin; PR #79 verified merged, no equivalent branch/open PR. Clean
starting main `4f27031a2e5638c2fffd7186cd7f6ba26f8d93b2`; branch
`feature/034f5-rf-rx-event-controls`. Pristine measurements: native 277/277,
Python 124/124, fresh production Release exports 193, vendor checksums 804/804.
Task-owned evidence: `/home/zboll/task-034f5-evidence/`; disk-backed TMPDIR is
`/home/zboll/task-034f5-evidence/tmp` (shared /tmp was 97% occupied).

## Contract and scope

Task 034F5 adds value-only RX receive-event v3 / JobInterval v4 controls through
one new export, `ams_mel_rf_job_add_rx_intervals_v4`. Shared execution values are
Normal=0 and Conditional=1; event termination is Inhibit=0 / Cancel=1. Ordered
polarization consists of zero, one or two four-double Stokes values, without
normalization or finite/range checks. Phase radians and Stokes doubles retain
signed zero, infinities and NaN classification. Boolean encodings are exactly
0/1; uint64 iteration counts must fit provider size_t. No execution/termination,
count/repeat, event/interval, activity or TX mode relationships are inferred.
Activity IDs are arbitrary copied binary bytes; uint32 TX Power Mode ID is not
TxPowerLevel. All values are prepared before the single provider Add, with no
hidden queries; existing status-registration/lifetime/lock/error paths remain.
Older event v1/v2 and interval v1/v2/v3 records/signatures remain frozen, ABI 0.1.
Safe Ada owns backing and privately serializes v4; raw Ada/Rust/private Python
match, with no safe Rust or public Python F5 API. Pinned Squall Add is a no-op:
real-provider acceptance is not execution/polarization/channelization evidence.
Weights (provider pointer ownership), PulseDetectionSettings (cohesive conditional
settings), Modulation (resource variants), endpoints and the inconsistent pinned
interval applicable-group setter/getter remain deferred. No TX events/intervals,
LF/context/ProductStream/RDMA/VADB expansion. See `docs/task-034f5-rf-rx-event-controls.md`.

## Exact additive records

```c
typedef uint32_t ams_mel_rf_execution_type_t;
#define AMS_MEL_RF_EXECUTION_NORMAL UINT32_C(0)
#define AMS_MEL_RF_EXECUTION_CONDITIONAL UINT32_C(1)
typedef uint32_t ams_mel_rf_event_termination_type_t;
#define AMS_MEL_RF_EVENT_TERMINATION_INHIBIT UINT32_C(0)
#define AMS_MEL_RF_EVENT_TERMINATION_CANCEL UINT32_C(1)
typedef struct ams_mel_rf_stokes_vector_v1 {
    double s0;
    double s1;
    double s2;
    double s3;
} ams_mel_rf_stokes_vector_v1;
typedef struct ams_mel_rf_stokes_vector_span_v1 {
    const ams_mel_rf_stokes_vector_v1 *data;
    size_t size;
} ams_mel_rf_stokes_vector_span_v1;
typedef struct ams_mel_rf_receive_event_config_v3 {
    ams_mel_rf_receive_event_config_v2 event;
    ams_mel_rf_stokes_vector_span_v1 polarization;
    uint32_t polarization_beam_steer_correction;
    double phase_offset_rad;
    ams_mel_rf_execution_type_t execution_type;
    ams_mel_rf_event_termination_type_t termination_type;
    uint32_t allow_delay_start;
    uint64_t iteration_hold_count;
    uint64_t iteration_termination_count;
    uint32_t channelization_enabled;
} ams_mel_rf_receive_event_config_v3;
typedef struct ams_mel_rf_receive_event_config_span_v3 {
    const ams_mel_rf_receive_event_config_v3 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v3;
typedef struct ams_mel_rf_job_interval_config_v4 {
    int64_t interval_start_femtoseconds;
    uint32_t interval_id;
    int64_t interval_starting_gap_femtoseconds;
    int64_t sequence_duration_femtoseconds;
    uint64_t sequence_repeat_count;
    int64_t calibration_duration_femtoseconds;
    int64_t interval_ending_gap_femtoseconds;
    uint32_t phase_coherence_with_prior;
    uint64_t iterations_per_signal;
    double max_data_rate_bps;
    double max_sample_rate_hz;
    uint32_t job_details_id;
    ams_mel_rf_job_interval_status_enable_t status_enable;
    ams_mel_rf_pointing_span_v1 stab_points;
    ams_mel_rf_receive_event_config_span_v3 receive_events;
    uint32_t tx_power_mode_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_rf_execution_type_t execution_type;
} ams_mel_rf_job_interval_config_v4;
typedef struct ams_mel_rf_job_interval_config_span_v4 {
    const ams_mel_rf_job_interval_config_v4 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v4;

```

All spans are borrowed only for synchronous Add; NULL/zero is valid. Unknown
stable uint32 enums, malformed spans, polarization size above two, non-0/1 flags
and unrepresentable size_t counts fail before provider mutation. Preparation
exceptions map INTERNAL_ERROR; Add bad_alloc maps INTERNAL_ERROR, other/unknown
provider exceptions map PROVIDER_EXCEPTION. No automatic retry. Never reporting
needs no stream; Always/OnException need this Job's normally-returned, usable
registration. Finalize/full Cancel prohibit subsequent Add.

## Evidence status

Measured validation results follow below. Hosted validation is recorded separately
in the PR/final report after pushing the tested source.

## Measured LP64 layout (not a cross-platform offset promise)

```text
RfStokesVectorV1: size=32, alignment=8; s0=0, s1=8, s2=16, s3=24
RfStokesVectorSpanV1: size=16, alignment=8; data=0, size=8
RfReceiveEventConfigV3: size=176, alignment=8; event=0, polarization=104, polarization_beam_steer_correction=120, phase_offset_rad=128, execution_type=136, termination_type=140, allow_delay_start=144, iteration_hold_count=152, iteration_termination_count=160, channelization_enabled=168
RfReceiveEventConfigSpanV3: size=16, alignment=8; data=0, size=8
RfJobIntervalConfigV4: size=160, alignment=8; interval_start_femtoseconds=0, interval_id=8, interval_starting_gap_femtoseconds=16, sequence_duration_femtoseconds=24, sequence_repeat_count=32, calibration_duration_femtoseconds=40, interval_ending_gap_femtoseconds=48, phase_coherence_with_prior=56, iterations_per_signal=64, max_data_rate_bps=72, max_sample_rate_hz=80, job_details_id=88, status_enable=92, stab_points=96, receive_events=112, tx_power_mode_id=128, activity_id=136, execution_type=152
RfJobIntervalConfigSpanV4: size=16, alignment=8; data=0, size=8
```

Source ABI comparison against starting main: all 188 earlier record definitions
and 193 earlier signatures remain byte-identical. GCC and Clang 19 actual-header
syntax probes both pass, including inherited JobEvent declaring types.

Fresh production Release audit: **194** exports, exactly v4 added, original 193
retained, export-map parity and no test-only exports. ABI version stays 0.1.

Reliability: early Ada generation/compilation failures were corrected without
weakening warnings/contracts. Two mistakenly concurrent task-owned Alire builds
were terminated; their concurrent Python failure due to missing/recreated DSOs
is not validation. Subsequent shared-tree writers run sequentially. Original
logs are preserved. No unrelated processes/containers/files were stopped/deleted.

## Focused payload evidence

Native C focused test runs 50 independent Jobs, inspecting copied values in the
existing mock DSO. It covers empty/one/two/duplicate/ordered Stokes vectors,
all components, signed zero, +/-infinity and NaN classification; phase zero,
signed zero, infinities, NaN and nonzero; every boolean and enum 0/1 accepted,
2/MAX rejected before Add; hold/termination 0/1/high-bit/SIZE_MAX; TX mode
0/1/high-bit/MAX and DEADBEEF; five binary bytes and an 8192-byte activity;
retained Pointing/index/group fields, status gating/closure, provider
std/unknown/bad_alloc failures and explicit same-Job recovery, Finalize/Cancel
rejection, and zero hidden query counters. Normal+Cancel+allow-delay and counts
above repeat, Conditional+Inhibit and TX mode absent from JobRequest all pass.
Actual runtime count evidence is 64-bit; no actual 32-bit runtime is claimed.

Safe Ada focused test runs 50 independent Jobs using public constructors/setters
and test-only provider observations. It verifies legacy defaults, one/two ordered
Stokes values, third append Constraint_Error, beam correction, signed-zero phase,
Conditional/Cancel/delay/high counts/channelization, binary activity/mode/interval
execution, F4 fields, status Never/Always, provider failure and same-Job recovery.
Existing safe Ada F4/default and older native tests remain source-unchanged.

GCC `-m32 -fsyntax-only` compiles the focused C test, including its narrow-size_t
rejection branch. This is compile-only, not 32-bit provider/runtime evidence.

## Recorded validation so far

- `make test-native`: 278/278, including focused native 50/50.
- `make test-build-isolation`: pass, both complete suites 278/278.
- `make check-ada-format`: pass.
- `alr -C ada build`: pass.
- `alr -C ada/tests run`: pass; safe Ada F5 focused 50/50 and earlier suites.
- `make test-rust`: pass; sys ABI suite 22/22 and workspace/doc tests.
- workspace `cargo check`, Clippy `-D warnings`, fmt check: pass.
- `make test-python`: 125/125 after correcting two export inventories; initial
  failed inventory run is preserved, not reported as a pass.
- vendor checksum audit: 804/804, all vendor/prior focused tests unchanged.
- `git diff --check`: pass.
- GCC/Clang 19 actual-header probes: pass; C narrow-host compile-only: pass.
- fresh Release: 194/map parity/one new export/no test exports, ABI 0.1.

Aggregate first attempt completed native/format checks but direct GPRbuild was
absent from shell PATH. Installed Alire toolchain paths were supplied explicitly
for the rerun; no global settings changed. Full native tree is rebuilt before
stress because Alire's prebuild intentionally leaves a partial contract tree.

Normal commits (no amend): `dadaaab` native/raw controls, `4e30f34` safe Ada controls.

Pinned Squall source confirmation: `interfaces/squall-rf-mel-impl/src/
SquallC2MEL.h:107` defines `addJobIntervals(const std::vector<JobInterval>&)
override {}`. Mock inspection, not this real no-op, is payload-fidelity authority.

Final aggregate `make check` with explicit installed toolchain PATH passed.
Complete native pre-stress rebuild passed 278/278; full `ctest --parallel 4
--repeat until-fail:50 --output-on-failure` passed **278/278**, every test 50
repetitions, no failed/Not Run (136.67 seconds). No corrected Job abandonment
or unrelated IR counter observation failure occurred in this run.

## Pinned Squall acceptance matrix

All four targets passed sequentially against pinned
`b1015728f904c799fa0c07489fce48e78f67845f`:

| Target | Control / Couloir metrics / health / RF metrics / data ports | Result |
| --- | --- | --- |
| safe Ada VA | 27003 / 27018 / 27013 / 27014 / 27601 | pass |
| safe Ada Job | 27103 / 27118 / 27113 / 27114 / 27701 | representative F5 Add accepted |
| C ProductRx | 27203 / 27218 / 27213 / 27214 / 27801 | direct v1 8/8, zero drops/malformed/allocation failures |
| safe Ada ProductRx | 27303 / 27318 / 27313 / 27314 / 27901 | 8/8 |

No TIME_WAIT workaround was needed. Normal task-created stack cleanup preserved
unrelated `opencv-imgproc-publish-010`. Squall accepted constructed values only;
no polarization, execution, termination, delay/count/channelization, activity or
TX-power interpretation/behavior is demonstrated by its no-op Add.
