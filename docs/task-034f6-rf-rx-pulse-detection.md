# Task 034F6 — RF RX pulse detection settings

Task 034F6 adds optional value-only OEM pulse settings to RX events via
ReceiveEvent v4 / JobInterval v5 and exactly `ams_mel_rf_job_add_rx_intervals_v5`.
Presence 0 ignores the entire inactive payload without calling the pulse setter;
presence 1 validates enum domains and threshold span structure. Defaults remain
DBQ, M/N 0/0, width 0 fs, 50-percent time tag, empty thresholds. Reference values
are DBQ=0, DB_ABOVE_NOISE=1, DB_BELOW_SATURATION=2; time-tag values are 50%=0,
90%=1. Ordered duplicate thresholds preserve all IEEE doubles; M/N accepts all
uint8 combinations and signed int64 femtoseconds pass exactly without physical
validation, quantization or capability queries. All values precede one provider
Add, with existing status/lifetime gates and exception mapping. Safe Ada owns
copyable vectors and final-sized synchronous backing; raw Ada/Rust/private
Python match, without a safe Rust or public Python API. Older profiles are frozen,
ABI remains 0.1. Pinned Squall Add is a no-op: acceptance is not pulse detection,
PDW generation, threshold interpretation or hardware support. No TX, modulation,
Weights, ProductStreamParams, LF/context, endpoints, RDMA or VADB implementation.
See `docs/task-034f6-rf-rx-pulse-detection.md` for layouts and validation evidence.

## Starting point

Clean branch `feature/034f6-rf-rx-pulse-detection` from merged PR #80 main
`d253f0153ad9bbcd5200da99da078ce30a9d3dff`. Reused measured baseline:
native 278/278, Python 125/125, fresh Release 194 exports, vendor 804/804.
Evidence: `/home/zboll/git/ams-mel-task-034f6-evidence/`, with disk-backed TMPDIR.

## Exact additive declarations

```c
typedef uint32_t ams_mel_rf_pulse_threshold_reference_t;
#define AMS_MEL_RF_PD_REFERENCE_DBQ UINT32_C(0)
#define AMS_MEL_RF_PD_REFERENCE_DB_ABOVE_NOISE UINT32_C(1)
#define AMS_MEL_RF_PD_REFERENCE_DB_BELOW_SATURATION UINT32_C(2)
typedef uint32_t ams_mel_rf_pulse_timetag_threshold_t;
#define AMS_MEL_RF_PD_TIMETAG_50_PERCENT UINT32_C(0)
#define AMS_MEL_RF_PD_TIMETAG_90_PERCENT UINT32_C(1)
typedef struct ams_mel_rf_pulse_m_of_n_v1 {
    uint8_t m;
    uint8_t n;
} ams_mel_rf_pulse_m_of_n_v1;
typedef struct ams_mel_rf_pulse_threshold_v1 {
    double leading_edge_db;
    double trailing_edge_db;
} ams_mel_rf_pulse_threshold_v1;
typedef struct ams_mel_rf_pulse_threshold_span_v1 {
    const ams_mel_rf_pulse_threshold_v1 *data;
    size_t size;
} ams_mel_rf_pulse_threshold_span_v1;
typedef struct ams_mel_rf_pulse_detection_settings_v1 {
    ams_mel_rf_pulse_threshold_reference_t reference;
    ams_mel_rf_pulse_m_of_n_v1 leading_edge_m_of_n;
    ams_mel_rf_pulse_m_of_n_v1 trailing_edge_m_of_n;
    int64_t min_pulse_width_femtoseconds;
    ams_mel_rf_pulse_timetag_threshold_t timetag_amplitude_threshold;
    ams_mel_rf_pulse_threshold_span_v1 thresholds;
} ams_mel_rf_pulse_detection_settings_v1;
typedef struct ams_mel_rf_receive_event_config_v4 {
    ams_mel_rf_receive_event_config_v3 event;
    uint32_t has_pulse_detection_settings;
    ams_mel_rf_pulse_detection_settings_v1 pulse_detection_settings;
} ams_mel_rf_receive_event_config_v4;
typedef struct ams_mel_rf_receive_event_config_span_v4 {
    const ams_mel_rf_receive_event_config_v4 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v4;
typedef struct ams_mel_rf_job_interval_config_v5 {
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
    ams_mel_rf_receive_event_config_span_v4 receive_events;
    uint32_t tx_power_mode_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_rf_execution_type_t execution_type;
} ams_mel_rf_job_interval_config_v5;
typedef struct ams_mel_rf_job_interval_config_span_v5 {
    const ams_mel_rf_job_interval_config_v5 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v5;

```

## Error and ownership policy

Malformed active structure or presence returns INVALID_ARGUMENT before Add.
Preparation exceptions and bad_alloc (including provider Add) return INTERNAL_ERROR;
other provider Add exceptions return PROVIDER_EXCEPTION. No retry; later explicit
Add uses the same Job and registration. No new owner, callback, worker or DSO pin.
Input thresholds are borrowed only synchronously and copied into provider values.
Ada Set copies owned data, replaces rather than accumulates; Clear serializes
absence with deterministic harmless backing. No manual Close for settings.

## Validation record

Local validation results are recorded below; hosted CI is recorded after publication.

## Measured LP64 layout (bytes)

| Record | Size / alignment | Field offsets in declaration order |
| --- | --- | --- |
| M/N | 2 / 1 | 0, 1 |
| Threshold | 16 / 8 | 0, 8 |
| Threshold span | 16 / 8 | 0, 8 |
| Settings | 40 / 8 | 0, 4, 6, 8, 16, 24 |
| ReceiveEvent v4 | 224 / 8 | 0, 176, 184 |
| ReceiveEvent v4 span | 16 / 8 | 0, 8 |
| JobInterval v5 | 160 / 8 | 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 92, 96, 112, 128, 136, 152 |
| JobInterval v5 span | 16 / 8 | 0, 8 |

These are measured host layouts, not a promise of LP64 offsets on other targets.
C11 compiler probes compare every field with raw Rust and private Python.
All 194 original C record declarations and 194 function declarations remain
text-identical to starting main. Fresh production Release audit measured exactly
195 exports: one added, none removed, exports.map parity, no test-only symbols,
runtime ABI 0.1. Actual tracked GCC 14.2 / Clang 19.1.7 probes pass exact signatures,
defaults and copy construction/assignment; closure remains 712 / 713 headers.

Initial native full suite passed 279/279 and dedicated focused test passed 50/50.
Final-source gates and pinned-provider acceptance are recorded below.

## Local gates completed

- Native full rebuild: 279/279, dedicated C11 F6 50/50; previous F4/F5 assertions
  retained, pulse-default checks added to old-profile observations.
- Build-tree isolation: pass, complete 279/279 suites.
- Ada format check and Alire library build: pass.
- Alire tests: pass; safe F6 50/50; aggregate direct-GPR `make check` also passes
  the strengthened clear/default and parent-first F6 fixture.
- Rust workspace tests including sys ABI 23/23, check, Clippy `-D warnings`, fmt: pass.
- Python: 126/126, authoritative C layout/signature/inventory probes included.
- Aggregate `make check`, whitespace and final-newline checks: pass.
- Native parallel 4 repeat-until-fail 50: 279/279, every test 50 repetitions,
  zero Failed/Not Run, 133.38 seconds. Squall matrix: all four targets pass.

Normal commits: `5f8d18d` native/raw/test changes; `ba0814b` safe Ada configuration.
First failure logs are retained: interrupted initial native build left a zero-byte
RF Data test object; only that generated object was removed and rebuilt. In-scope
missing header-probe return, missing Ada threshold serialization, test syntax and
unused test helpers were corrected without suppressions, weakened assertions or
timeout changes. No unrelated IR source was changed or causal-fix claim made.
Large threshold observation resolves symbols once per event and caches provider
getter copies per Add revision; every ordered threshold remains checked.
Task-owned disk-backed TMPDIR remains in use; unrelated files/containers untouched.

## Pinned Squall matrix

All four targets passed against `b1015728f904c799fa0c07489fce48e78f67845f`:

| Target | Control / Couloir metrics / health / RF metrics / data ports | Result |
| --- | --- | --- |
| safe Ada VA | 28003 / 28018 / 28013 / 28014 / 28601 | pass |
| safe Ada Job | 28103 / 28118 / 28113 / 28114 / 28701 | representative pulse settings Add returned normally |
| direct C ProductRx v1 | 28203 / 28218 / 28213 / 28214 / 28801 | 8/8, zero drops/malformed/allocation failures |
| safe Ada ProductRx | 28303 / 28318 / 28313 / 28314 / 28901 | unchanged eight-product assertions pass |

`interfaces/squall-rf-mel-impl/src/SquallC2MEL.h:107` defines
`addJobIntervals(const std::vector<JobInterval>&) override {}`. Thus actual payload
fidelity comes from the independent mock getter observations, not Squall. No OEM
hardware support, detector execution, PDW generation, trigger or threshold semantics
are established. No TIME_WAIT workaround was needed; normal task-stack cleanup
preserved the unrelated `opencv-imgproc-publish-010` container.

## Focused evidence

Native 50-Job fixture covers ordered intervals/events; poisoned inactive payload;
all active enum values and invalid values; flags 0/1/2/MAX; NULL/empty/one/repeated/
8192 ordered thresholds and overflow; full M/N endpoints including M>N; widths
MIN/-1/0/1/MAX; signed zero, infinities, NaN classification and finite values.
Invalid last-interval events prevent the entire Add. Preparation failpoint prevents
provider entry. std/unknown/bad_alloc Add failures map correctly with exactly one
attempt and later explicit same-Job recovery. Never/Always/OnException status
registration and post-Finalize/full-Cancel guards remain; parent-first close works.
Query counters remain zero, including capability getters, quantization and forbidden
operations. v1/v2 mock fidelity checks and v3/v4 tests verify default pulse settings.

Safe Ada public-only construction covers all reference variants/both time tags;
MIN/zero/MAX widths, M/N 255/0 and 0/255, ordered duplicate finite/IEEE thresholds;
Set replacement, Set/Clear and Clear/Set; independent owned settings copies;
multiple events/intervals, retained spatial/execution/polarization controls;
status gate, three provider exception classes and recovery, parent-first close.
Existing focused F4/F5 tests and source-compatible constructors remain passing.

No safe Rust or public Python pulse API, OEM engine, TX interval/event, Modulation,
Weights, ProductStream, LocalFunctionCommand/context, endpoint mapping, RDMA or
VADB work was started. Existing earlier TX request tests are regression only.
