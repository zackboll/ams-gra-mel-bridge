# Task 034F4 — RF RX JobInterval v3 spatial controls

## Starting point and scope

Fetched origin; PR #78 verified merged, no open/equivalent PR or branch.
Actual starting main: `6594b73f288c7b32f7dc03b5d236128ac061a9d4`.
Clean starting worktree; branch `feature/034f4-rf-interval-spatial-controls`.
Measured pristine baseline: native **276/276**, Python **123/123**, fresh
production Release exports **192**, vendor checksums **804/804**.
Task-owned logs/builds use disk-backed
`/home/zboll/git/ams-mel-task-034f4-evidence/`, not shared `/tmp` storage.

This slice configures RX spatial/selection values only. No TransmitEvent/TX
intervals, polarization, phase offset, execution/termination/delay/iteration
controls, channelization/pulse detection, interval LocalFunctionCommand/context/
ProductStreamParams/endpoints, Modulation/Weights/waveform resources, external/
RDMA endpoints or VADB were started. Existing Job owner/lifetime/async paths are
reused. RF pin is `762ce84c5555dd0f3ea66f36b321fecf8839b89f`; vendor unchanged.

## Exact additive C ABI

ABI remains **0.1**. Frozen v1/v2 interval/event records and exports remain.
The four new records have exactly this field order:

```c
typedef struct ams_mel_rf_receive_event_config_v2 {
    ams_mel_rf_receive_event_config_v1 event;
    uint64_t stab_point_index;
    ams_mel_u64_span_v1 applicable_rx_element_groups;
} ams_mel_rf_receive_event_config_v2;
typedef struct ams_mel_rf_receive_event_config_span_v2 {
    const ams_mel_rf_receive_event_config_v2 *data;
    size_t size;
} ams_mel_rf_receive_event_config_span_v2;
typedef struct ams_mel_rf_job_interval_config_v3 {
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
    ams_mel_rf_receive_event_config_span_v2 receive_events;
} ams_mel_rf_job_interval_config_v3;
typedef struct ams_mel_rf_job_interval_config_span_v3 {
    const ams_mel_rf_job_interval_config_v3 *data;
    size_t size;
} ams_mel_rf_job_interval_config_span_v3;
```

Exactly one new production function:

```c
ams_mel_status_t ams_mel_rf_job_add_rx_intervals_v3(
    ams_mel_rf_job *job,
    ams_mel_rf_job_interval_config_span_v3 intervals,
    char *diagnostic, size_t diagnostic_capacity,
    size_t *diagnostic_required);
```

All input spans/strings are borrowed only for synchronous Add. NULL/zero spans
are valid. Original 184 C record definitions compare byte-identically with the
starting header; all original 192 declarations/signatures compare unchanged.
Raw Rust and private Python C11 probes cover size/alignment/every field offset,
the new signature and exact export inventory. Ada raw records stay private.
On the validated LP64 host, receive-event v2 is 104 bytes/alignment 8 with
offsets event=0, stab_point_index=80, applicable_rx_element_groups=88.
JobInterval v3 is 128 bytes/alignment 8; offsets in the field order above are
0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 92, 96, 112.
Each new span is pointer followed by size_t (16 bytes/alignment 8 on LP64).
These measured offsets are platform evidence, not a universal packing promise.

## Validation, preparation and provider boundary

Portable uint64 indices/counts convert to provider size_t only after checking
`value <= SIZE_MAX`; compile-time size_t width must be at most 64 bits. Reject
unrepresentable values, malformed/overflowing spans, invalid UTF-8/NUL labels,
phase bool >1, unknown status or Pointing tags and noncanonical **active**
ECEF/LLA fractional UTC (`0 <= fs < 10^15`). No local relationship is checked
between an event index and stab vector length, or group indices and request
group count. Order, duplicates, zero and representable boundary indices survive.
Index 999 with only four points, and SIZE_MAX with no points, are accepted.

One shared private `valid_pointing`/`prepare_pointing` implementation and its
`canonical`/`pointing_time` helpers serve both JobRequest and JobInterval.
Only active payloads are inspected; relative-pointing inactive ECEF/LLA UTC is
ignored even when poisoned with -1/10^15. All active doubles pass without finite
checks, coordinate conversion, unit conversion or angle normalization; signed
zero, infinities and NaN classification survive (no NaN payload-bit promise).
Negative timing and existing nonfinite data/sample-rate fields remain accepted.

Every PointingType, size_t vector, ReceiveEvent, RX-only Sequence and JobInterval
is constructed before the single provider `JobDetail::addJobIntervals` call.
V3 construction failures map INTERNAL_ERROR; provider bad_alloc maps
INTERNAL_ERROR and std/unknown exceptions map PROVIDER_EXCEPTION. No retry.
No provider call occurs during validation/conversion; no hidden MFA, duration
quantization, E1–E6, group/DataPipe, Weights/LF or ProductRx query occurs.
Existing Add/Finalize/full-Cancel guards and status filtering are unchanged.
Always/OnException require this Job's registration to have returned normally
and remain locally usable; Never needs no stream. This is not delivery proof.

v1/v2 instantiate the same path without spatial setters and preserve upstream
empty stab points, event index zero and empty applicable RX group vector.
Their original direct tests and mock fidelity observations are not weakened.

## Safe Ada

Existing public configuration types/constructors remain source-compatible.
`Set_Stab_Point_Index`, `Append_Applicable_RX_Element_Group`, and
`Append_Stab_Point` modify Ada-owned vectors/values; no size_t/raw C type is
public. Defaults are index zero/empty group and point vectors. Private helper
full views are reordered only as needed, without duplicating Pointing.
All safe Add calls serialize v3 using the existing `Raw_Pointing`, final-sized
Ada-owned backing arrays and controlled strings through the borrowed call.
The public-only focused test runs separately because status callback shells
permanently pin the provider; legacy unload assertions remain isolated.
No safe Rust or public Python spatial API is added.

## Mock and pinned-header evidence

The existing provider DSO gains separate v3 observation; v1/v2 checks remain.
Interval A has ordered ECEF/platform/face/duplicate-face points. X/Y retain all
old fields, stab indices 1/999 and groups `[2,0,2]`/`[7,7,1]`. Interval B has
ordered LLA/baseline points and no events. Mock-side F2 inspection checks every
ECEF location/velocity and LLA lat/lon/alt/NED component plus exact UTC; relative
active doubles, signed zero/NaN/+infinity/-infinity and duplicates are checked.
Production never reads provider Pointing getters. Native tests also check
active UTC rejection before Add, both inactive poison boundaries, malformed
spans/labels/tags, size_t max, allocation-before-Add, registration gate, provider
std/unknown/bad_alloc failure and explicit same-Job recovery, Finalize/Cancel
guards and zero hidden-query counters. Safe Ada covers defaults, all five
constructors, multiple intervals/events, repeats, timing/rate/count fidelity,
status Never/Always/OnException, Provider_Error and same-Job recovery.

Actual-vendored-header probes retain F2 variant-order checks and verify:

- `JobInterval::setStabPoints(const std::vector<PointingType>&)` -> void;
- const `getStabPoints()` -> `const std::vector<PointingType>&` (overload-selected);
- inherited `JobEvent::setStabPointIndex(size_t)` -> void;
- inherited const `getStabPointIndex()` -> size_t;
- `ReceiveEvent::setApplicableRxElementGroups(const std::vector<size_t>&)` -> void;
- const `getApplicableRxElementGroups()` -> `const std::vector<size_t>&`.

## Explicit upstream deferral and Squall evidence boundary

Pinned `JobInterval.h:325–328` setter writes `applicableElementGroupLabels`;
`:140–143` getter returns **dataPaths**, a different member/type. The header
probe documents both exact signatures and observes that setting a nonempty
label vector leaves the returned dataPaths empty. Neither this setter nor
interval endpoints is bound. Vendor is not patched; upstream-contract review
is a separate task.

Squall pin `b1015728f904c799fa0c07489fce48e78f67845f` has empty
`addJobIntervals` in `interfaces/squall-rf-mel-impl/src/SquallC2MEL.h:107`.
The first existing safe Ada interval now supplies FaceRelative/ECEF/LLA, event
index 1 and groups `[0,0]`, retaining status/extension/flush/cancel/lifecycle.
Real-provider evidence can mean only: **real provider accepted addJobIntervals
containing bridge-constructed spatial values; payload fidelity and spatial
semantics are mock evidence only**. It cannot prove geometry use, index
validation, steering or scheduling. ProductRx integration sources are unchanged.

## Validation evidence

Measured results and final hosted checkout identities are recorded after the
corresponding commands complete; no unavailable or skipped checks count as passes.

### Completed local matrix

All shared writers ran sequentially. Exact commands/results:

| Command / observation | Result |
| --- | --- |
| `make test-native` | **277/277**, no Not Run |
| native focused C11 spatial test | **50/50** |
| safe Ada public focused spatial executable | **50/50** |
| direct rf-job-request/v2/pointing/tx-groups/interval-status | **5/5** |
| `make test-build-isolation` | pass; 277/277 suite, distinct trees/failpoints |
| `make check-ada-format` | pass |
| `alr -C ada build` | pass |
| `alr -C ada/tests run` | all suites pass, including isolated F4 50/50 |
| `make test-rust` | **81 tests**, no failed/ignored; raw ABI 21 |
| `cargo check --manifest-path rust/Cargo.toml --workspace` | pass |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | pass |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | pass |
| `make test-python` | **124/124**, compileall pass |
| `make check` | pass, includes direct GPR Ada/native/format/newline/diff gates |
| `git diff --check` | pass |
| GCC actual-vendored-header compile/runtime/closure probe | pass; 712 headers |
| Clang 19 actual-vendored-header compile/runtime/closure probe | pass; 713 headers |
| fresh production Release audit | **193**, map parity, no test-only symbols, ABI **0.1** |
| source ABI audit | original **184** records byte-identical; original **192** signatures unchanged |
| vendor SHA-256 audit | **804/804**, no vendor changes |

size_t runtime evidence is 64 bits, SIZE_MAX=18446744073709551615; index
and ordered `[SIZE_MAX,0,SIZE_MAX]` group vector survive with empty stab points.
Narrow-host rejection branches exist, but no 32-bit runtime is claimed.

### Pinned Squall matrix

All four targets passed sequentially. Exact isolated ports (control, Couloir
metrics, RF health, RF metrics, RF data):

| Target | Ports | Result |
| --- | --- | --- |
| `make test-squall-rf-ada-va` | 25203,25318,25313,25314,25601 | pass |
| `make test-squall-rf-ada-job` | 25303,25418,25413,25414,25701 | pass; representative spatial Add accepted |
| `make test-squall-rf-rx` | 25403,25518,25513,25514,25801 | pass; unchanged direct C v1 **8/8** events, no drops/malformed/allocation failures |
| `make test-squall-rf-ada` | 25503,25618,25613,25614,25901 | pass; unchanged safe Ada ProductRx **8/8** (A + seven later events) |

No listener/TIME_WAIT workaround or unrelated-container stop was needed. The
pinned checkouts were verified by the integration script; task-created stack
and images were cleaned by its normal cleanup. Real provider evidence is Add
acceptance only, as bounded above, not spatial use or index validation.

### Reliability and environment

The first full `ctest --parallel 4 --repeat until-fail:50` had one unchanged IR
malformed-frame observation failure at `test_ir_stream.c:813` (counters after an
existing 20 ms receive timeout). IR adapter/test/mock are byte-identical to
starting main. All RF tests, including corrected abandonment and F4, completed
50 repeats. Full log and extracted first failure are preserved; no sleeps,
retries, timeout increases or weakened assertions were added to tests.

An attempted post-Squall repeat found missing executables because the Alire
native pre-build had recreated only facade/provider targets in the shared test
tree. That **Not Run environment attempt is not a pass or final validation**;
its log is preserved and `make test-native` rebuilds the complete tree before
repeat. Disk-backed task TMPDIR avoids shared `/tmp` exhaustion; no unrelated
files were removed. Native/Alire compiler-path transitions legitimately rebuild
selected native trees under the existing isolation policy.

Final rebuilt unchanged-source full repeat:
`ctest --test-dir native/build-tests --parallel 4 --repeat until-fail:50 --output-on-failure`
passed **277/277**, each test 50 repetitions, **no failed/Not Run** (102.26 s).
Post-Squall `make test-native` passed **277/277** before that repeat. The first
IR malformed-observation failure is classified separately from F4 with unchanged
IR source evidence and successful full rerun, not erased or claimed fixed.

Normal commits:
- `22cefa2` — Add native RX JobInterval spatial controls;
- `9eaf214` — Add safe Ada RX interval spatial configuration;
- final evidence/integration documentation commit follows those two, without amend.

Hosted push/PR validation and exact source/merge checkout identities are recorded
in the PR and final report after completion. The PR must remain open, non-draft,
unmerged, auto-merge disabled; no publishing/tagging/merge is requested.
