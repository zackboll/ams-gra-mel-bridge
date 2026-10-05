# Task 034F3 — ordered mixed RX/TX JobRequest v4

## Baseline and scope

Fetched origin and verified PR #76 merged. Actual starting main:
`6c701875445b3367652a0d729f2f39797c20de63`. Clean worktree; no existing F3
branch/open PR. Branch: `feature/034f3-rf-job-tx-groups`.

Measured before editing: native **275/275**, Python **122/122**, production
exports **191**, vendor checksums **804/804**. Aggregate `make check` passed
with installed Alire GNAT 16.1/GPRbuild 26 tools explicitly added to PATH.
The initial plain-PATH attempt could not find GPRbuild; this was an environment
failure, not a source fix. Logs are in the ignored `build/task-034f3/` tree.

This task constructs JobRequest requirements only. It does not execute TX
events or create TransmitEvent, TX JobInterval sequences, modulation, Weights,
CachedWaveform/WaveformTxEndpoint, MFADrivenControls/JIB/rejection callbacks,
external/RDMA endpoints, VADB or additional ProductRx formats. No vendor change.

## Additive ABI

Exactly one new production export:

```c
ams_mel_status_t ams_mel_rf_virtual_aperture_submit_job_v4(
    ams_mel_rf_virtual_aperture *va,
    const ams_mel_rf_job_request_config_v4 *config,
    ams_mel_rf_job_request **out_request,
    char *diagnostic, size_t diagnostic_capacity, size_t *diagnostic_required);
```

The existing request owner/Wait/Claim/Close and sole hardened `submit_prepared`
worker/future/abandonment/emergency-retention path are reused. No new async owner.

```c
typedef struct ams_mel_rf_tx_element_group_config_v1 {
    ams_mel_string_view_v1 label;
    uint32_t tx_power_level;
    double desired_duty_factor;
    ams_mel_rf_frequency_range_span_v1 expected_center_frequencies;
} ams_mel_rf_tx_element_group_config_v1;

typedef struct ams_mel_rf_job_element_group_config_v4 {
    ams_mel_rf_element_group_mode_t mode;
    ams_mel_rf_rx_element_group_config_v3 rx;
    ams_mel_rf_tx_element_group_config_v1 tx;
} ams_mel_rf_job_element_group_config_v4;

typedef struct ams_mel_rf_job_element_group_config_span_v4 {
    const ams_mel_rf_job_element_group_config_v4 *data;
    size_t size;
} ams_mel_rf_job_element_group_config_span_v4;

typedef struct ams_mel_rf_job_request_config_v4 {
    uint32_t request_id;
    uint32_t priority;
    uint32_t precedence_within_priority;
    uint32_t is_interruptable;
    ams_mel_u32_span_v1 instance_selection;
    ams_mel_rf_job_element_group_config_span_v4 element_groups;
    ams_mel_rf_utc_time_v1 min_start_time;
    ams_mel_rf_utc_time_v1 max_complete_time;
    int64_t duration_femtoseconds;
    ams_mel_u8_span_v1 capability_id;
    ams_mel_u8_span_v1 activity_id;
    ams_mel_u32_span_v1 tx_power_mode_ids;
    int64_t lookahead_femtoseconds;
    uint32_t has_estimated_stab_point;
    ams_mel_rf_pointing_v1 estimated_stab_point;
} ams_mel_rf_job_request_config_v4;
```

Existing E3 mode type/constants are relocated verbatim, not duplicated: RX=0,
TX=1. Unknown modes reject before provider command construction. Only the
selected payload is read/validated. RX ignores the entire TX payload; TX ignores
the entire RX payload, including labels, pipes and pointings. Nonempty total
groups are required; RX-only/TX-only/mixed are all valid bridge input. Caller
interleaving and repeated labels/modes are preserved without sorting/deduplication.

TX labels are complete UTF-8, no embedded NUL, empty allowed. Duty is finite
`0 < duty <= 1`; ranges have finite min/max and min<=max, copied in caller order.
TxPowerLevel forwards exact uint32 including 0, 1, high bit and MAX. It is not
TxPowerModeID, not Weights, and no dB/dBW conversion/clamping/default is inferred.
There is no descriptor/RFMFAInfo/TX-band/power capability lookup or auto-gate.
Provider requestJob remains authoritative for actual supported requirements.

All F1/F2 common setters remain: priority, precedence, min/max canonical UTC,
signed duration/lookahead, request ID, instance vector (duplicates retained),
interruptable, binary capability/activity IDs, TX-power-mode set, and conditional
estimated stab point. All five pointing alternatives and active-field rules
remain. MFADrivenControls defaults remain null callback, zero numJIBs, nullptr
context and null rejection callback; default ECEF numeric storage is never read.

RX per occurrence: create once, getMode once/require RX, duty once, ordered
frequencies, ordered endpoint-entry sets, ordered expected pointings, add once.
TX per occurrence: create once, getMode once/require TX, duty once, power once,
ordered frequencies, add once. **No TX endpoint or expected-pointing calls; no
RX setTxPower.** Production never uses filtered getRx/getTx getters.

All active strings/vectors/sets/pointing values and common data are prepared
before parent acquisition; completion/input/public owner/worker holder allocate
before provider mutation. Preparation allocation failure is INTERNAL_ERROR with
no commands/requestJob. Unknown mode/invalid active data are INVALID_ARGUMENT
without commands/requestJob/output. Both mode mismatches are PROVIDER_FAILED
without requestJob/fallback/retry. Null commands also fail PROVIDER_FAILED;
bad_alloc maps INTERNAL_ERROR and ordinary/unknown exceptions PROVIDER_EXCEPTION.
Already-created commands and JobRequest unwind before parent VA/C2 claim release.

Fresh Release measured **192 exports**, exactly the v4 addition, no removal,
exports.map parity, no test symbols; runtime ABI **0.1**. Source audit compares
all **180 existing records** and **191 existing declarations** byte-for-byte to
main: unchanged, including frozen Job/RX v1/v2/v3. Direct v2/v3 tests and real
C ProductRx v1 application are byte-identical. Vendor checksum audit **804/804**.

Raw Ada/Rust/private Python records, spans, signature/layout/inventory probes
are synchronized. No safe Rust or public Python Job v4 API is introduced.

## Actual pinned headers

RF MEL pin: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
GCC/Clang compile actual-vendored-header static assertions for TxPowerLevel=
uint32_t, Mode underlying uint8_t, RX=0/TX=1, exact set/getTxPower, set/getDuty,
addExpectedCenterFrequencies signatures. JobRequest getElementGroups returns
`const ElementGroupCommandList&`; filtered getRx/getTx return const list values.
List indexing preserves `const shared_ptr<ElementGroupCommand>&` semantics.
Unchanged closure: GCC **712**, Clang **713** pinned dependency headers.

## Positive mock evidence

The existing mock DSO is extended, not replaced. Primary order is RX `rx/a`,
TX `tx/a`, RX `rx/b`, TX `tx/b`. Both RX occurrences have nontrivial ranges,
two endpoint sets and ECEF/platform points. TX `tx/a` uses power 0xDEADBEEF,
duty .375 and ordered ranges [100000000.25,100000001.5], [915000000,915000000].
TX `tx/b` uses MAX, duty 1, range [-1.25,3.5]. Provider-side observations verify
full global order and filtered RX/TX relative order, exact setter counts and
mode/duty/power/frequency operation markers. TX `same`, RX `same`, TX `same`
arrive as three distinct occurrences.

Focused native tests include RX-only v4/TX-only, all power boundaries, poisoned
inactive UTF-8/duty/spans/point tags/active ECEF time, active validation, both
mode mismatches and 51 TX command-failure combinations across first/middle/final
positions. They check cleanup before parent destruction and successful recovery.
No hidden E1-E6/MFA/Weights calls: query counters remain zero and forbidden
mock methods fail. V4 also tests requestJob/future exceptions/allocations,
invalid future, rejection, null detail, delayed future, zero-timeout, claim,
abandonment, parent-first lifetime and worker/post-allocation/publication retention.
Native focused repeat: **50/50** on the tested initial implementation.

## Safe Ada

Public private-value `TX_Element_Group_Config`, `Create_TX_Element_Group
(Label, TX_Power_Level : Unsigned_32, Desired_Duty_Factor := 1.0)` and overloaded
`Append_Expected_Center_Frequency` expose no raw C types/addresses. No TX endpoint
or expected-pointing mutators. One private discriminated RX/TX vector preserves
global append order. Existing RX Create_Job_Config inserts RX first unchanged;
the TX overload permits a TX-only config, and Append_TX_Element_Group appends
to the same sequence. Submit_Job privately serializes v4 with final-sized backing
and controlled strings; inactive raw fields are deterministically initialized.
Long_Float/C-double policy and exact Unsigned_32 fidelity are retained.

Focused tests drive public Ada only (test-only DSO release controls are used for
the delayed completion barrier). Initial full Ada smoke passed, including
**50/50** mixed/RX-only/TX-only/repeated scenarios, power boundaries, retained RX
and common/estimated fields, mode/setter Provider_Error recovery, timeout/claim
and parent-first ownership. Legacy RX source tests remain green.

## Pinned Squall qualification

Squall pin: `b1015728f904c799fa0c07489fce48e78f67845f`. Rechecked source:
supportsTransmit returns false; descriptor/command mode is RX; label `0` creates
RX; requestJob rejects non-RX. It is **not positive TX construction evidence**.
The safe Ada Job application first attempts valid TX-only label `0`, power
0xDEADBEEF, duty .375, 915 MHz range, expecting Provider_Error at command mode
check before requestJob. Its existing positive two-group F2 RX flow follows
immediately, testing lifecycle recovery and v4 RX preservation. C ProductRx
stays frozen v1; safe Ada ProductRx application source remains unchanged.
Actual integration and final validation results are recorded below when run.

## Environment/reliability disposition

The first final `alr -C ada/tests run` passed the F3 50/50 scenarios, then failed
the unrelated isolated Local Function suite with ADA.IO_EXCEPTIONS.DEVICE_ERROR:
No space left on device. The first failing log is preserved as
`build/task-034f3/first-enospc-alire-tests.log`. `df` verified host `/tmp` 43 GiB
full (100%, inodes not exhausted), while the repository filesystem had 854 GiB
free. No timeout/assertion or source change was made in response. No unrelated
temporary tree/container was removed. Validation reran unchanged source inside
a task-only user/mount namespace binding the task-owned disk-backed temporary
directory to `/tmp`; this avoids hardcoded legacy test paths exhausting host
tmpfs. This is an environment workaround, **not a causal source fix**.

After aggregate make check passed, its Ada pre-build had recreated the shared
native test tree with only facade/mock targets. The following focused CTest was
**Not Run** because its executable was absent (first log preserved as
`first-focused-after-alire.log`); this is not counted as a pass. The complete
native tree was rebuilt before focused/full repetitions, with unchanged source.

## Measured final local validation

| Command/check | Actual result |
| --- | --- |
| make test-native (final complete rebuild) | **276/276** |
| make test-build-isolation | pass |
| make check-ada-format | pass |
| alr -C ada build | pass |
| alr -C ada/tests run | pass in task temporary namespace |
| make test-rust | pass: 76 tests + 4 compile-fail doctests |
| cargo check --manifest-path rust/Cargo.toml --workspace | pass |
| cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings | pass |
| cargo fmt --manifest-path rust/Cargo.toml --all -- --check | pass |
| make test-python | **123/123**, compileall pass |
| git diff --check / final-newline check | pass |
| aggregate make check | pass, native and full direct-GPR Ada suites |
| GCC / Clang actual-header signature + closure probes | pass, 712 / 713 headers |
| focused native v4 until-fail:50 | **50/50** after complete rebuild |
| focused public Ada TX/RX suite | **50/50**, plus error/recovery and delayed claim |
| full native parallel-4 until-fail:50 | **276 tests x 50**, no failures (102.77 s) |
| fresh production Release audit | **192** exports, exact one addition, ABI **0.1** |
| frozen records/declarations audit | **180 / 191** byte-identical |
| vendor checksum audit | **804/804**, no vendor diff |

No legacy Job abandonment timeout or IR late-callback failure recurred in this
full native repeat. No claim is made that F3 fixes those previously unproven
reliability observations. Shared writers ran sequentially; the independent fresh
Release tree/header probes did not mutate shared build trees. Hosted CI and
Squall evidence are separate from these provider-free results.

## Measured pinned-provider integration

All four requested targets passed at the exact pinned Squall checkout, one
iteration each, with distinct control/metrics/health/data port sets:

| Target | Ports (control + four isolated service ports) | Result |
| --- | --- | --- |
| make test-squall-rf-ada-va | 32401..32405 | pass |
| make test-squall-rf-ada-job | 32421..32425 | pass |
| make test-squall-rf-rx | 32441..32445 | pass, C frozen-v1 **8/8** |
| make test-squall-rf-ada | 32461..32465 | pass, unchanged safe Ada **8/8** |

Job log explicitly reports the receive-only TX/RX mode mismatch, then both
existing RX Job snapshots and successful parent-first lifecycle. Production
builder control flow returns false at mode mismatch before requestJob. RX two
groups and F2 pointings remain accepted via v4. This is real-provider negative
TX/mode evidence, **not positive Squall TX submission/scheduling/RF emission**.

C ProductRx measured received=8, queued=8, dropped=0, malformed=0, allocation=0,
after-close=0, with unchanged copy/lifecycle assertions. Safe Ada ProductRx
retains its eight receive and zero-error/drop checks through private v4 without
application-source edits. No assertion weakening, TIME_WAIT workaround or
replacement/stopping of unrelated containers was needed. Task-created runtime
containers/images were cleaned by the existing integration runner.




