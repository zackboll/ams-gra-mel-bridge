# Task 034D1 — bounded receive JobInterval commands

Starting main: `2fde3aaea8212e582348f7157f843f2820c7d2d7` (merged PR #65).
Branch: `feature/034d1-rf-rx-job-intervals`. Recorded baseline: native 254/254,
Python 112, production export inventory 143, vendor checksums 804/804.

## Pinned continuation representation and limitation

RF MEL pin: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
AMS Math pin: `00be45190f0e47d268cece8b8c2f8fb58b5418d2`.
`native/vendor/ams-math/include/math/units/UTCTime.h:9` defines Femtoseconds as
`std::chrono::duration<int64_t, std::femto>`.
`native/vendor/rf-mel/include/rfmel/jobs/JobInterval.h:130` defines:

```cpp
static constexpr ams::util::math::Femtoseconds ContinueFromPrevious =
    std::numeric_limits<ams::util::math::Femtoseconds>::max();
```

The original task incorrectly assumed this count was INT64_MAX. The original
production equality assertion failed: `(0 == 9223372036854775807)`.
`numeric_limits<Femtoseconds>` is unspecialized and its max count is **0**;
`Femtoseconds::max().count()` is **INT64_MAX**. Production and the authoritative
vendored-header probe explicitly assert both the pinned zero contract and the
distinction. A runtime header regression checks the default JobInterval start.

The corrected C constant is
`AMS_MEL_RF_JOB_INTERVAL_CONTINUE_FROM_PREVIOUS_FS = INT64_C(0)`; safe Ada exposes
`Continue_From_Previous_Femtoseconds : constant Interfaces.Integer_64 := 0`,
and its interval constructor default uses that symbolic constant.

Upstream documents the named constant as continuation after the preceding
interval. Its actual zero representation also aliases an ordinary zero relative
start. The unchanged scalar provider interface cannot distinguish those two
intentions. This is pinned-interface compatibility, not an upstream scheduling
fix or a universal RF MEL sentinel. Matching the constant does not establish
real continuation scheduling. Future adoption of a corrected sentinel requires
explicit dependency/provider and published-C-constant compatibility review.

Candidate upstream correction, **PROPOSED, not applied or verified against all
providers**:

```cpp
static constexpr Femtoseconds ContinueFromPrevious = Femtoseconds::max();
```

No vendor patch, replacement header, numeric_limits specialization, include-order
workaround, provider modification or pin change is introduced. Vendored bytes
remain authoritative. Local compiler identities: GCC 14.2.0 (Debian 14.2.0-19),
Clang 19.1.7 (Debian 3+b1), libstdc++ release 14 / `__GLIBCXX__ = 20250315`.

## Profile and exact mapping

New borrowed C records: `ams_mel_rf_receive_event_config_v1`,
`ams_mel_rf_receive_event_config_span_v1`, `ams_mel_rf_job_interval_config_v1`,
`ams_mel_rf_job_interval_config_span_v1`. Empty spans may have NULL data;
nonempty spans require data. Labels allow empty UTF-8, reject invalid UTF-8 and
embedded NUL, and are copied before provider invocation.

JobInterval sets only interval start, ID, starting gap, Sequence, repeat count,
calibration duration, ending gap, phase coherence with prior, iterations per
signal, max data rate bps, max sample rate Hz and Job Details ID.
Sequence sets only duration and the ordered ReceiveEvent vector; TX stays empty.
ReceiveEvent sets only event ID, element-group label, start, duration, center
frequency, sample frequency, AGC-processing iterations, ignored post-AGC
iterations and maximum extension duration.

All eight signed int64 femtosecond fields are forwarded verbatim; no automatic
quantizeDuration, rounding, normalization or timing relationship validation.
In particular starts 0, 1, -1 and INT64_MAX stay unchanged: there is no sentinel
translation. Negative durations, NaN, infinities and signed zero remain provider
data. All four uint64 counts are checked against size_t max before any provider
method, then cast without truncation. On 64-bit hosted platforms every uint64
count fits; narrower-host rejection tests are conditional on SIZE_MAX.

## Omitted pinned defaults

JobInterval: applicable groups/endpoint maps, stabilization points, local
function commands, modulations and activity ID remain empty; TxPowerModeID is
zero; JobIntervalStatusEnable is Never; execution type is Normal. ProductStreamParams
and user-defined context remain default constructed and are not exposed.

ReceiveEvent: direction Receive (constructor); polarization and weights empty;
beam-steer correction false; phase offset and stab-point index zero; execution
Normal; termination InhibitEvent; allow-delay false; hold and termination counts
zero; channelization false; applicable RX groups empty. Pulse-detection settings
remain default constructed. Only public pinned getters are used for inspection.

## Lifecycle, errors and ownership

Exactly three synchronous exports add to ABI 0.1 (143 → 146):

- `ams_mel_rf_job_add_rx_intervals` → addJobIntervals once, including empty input.
- `ams_mel_rf_job_flush` → flush once for each accepted invocation, not cached.
- `ams_mel_rf_job_cancel_remaining_intervals` → cancelRemainingJobIntervals once
  for each accepted invocation, not cached.

Add/Flush reject with PROVIDER_FAILED and a clear diagnostic after Finalize or
full Cancel has been attempted, even when that attempt threw. Cancel_Remaining
allows before Finalize, pending future and terminal status, but rejects after
full Cancel has been attempted. Public same-owner calls/Close remain externally
serialized. Complete validation and temporary construction precede provider Add.
A brief JobState mutex inspection ends before all three provider calls; the
finalize worker can wait/publish independently. No new worker, Job owner or
ownership graph is introduced. JobDetail, retained VA, sibling C2 claim,
immutable snapshot, cached cancellation and deferred shutdown remain unchanged.

bad_alloc maps INTERNAL_ERROR; other standard/unknown exceptions map
PROVIDER_EXCEPTION. No exception crosses C and no mutating command is retried
to obtain a longer diagnostic. Safe Ada uses the fixed diagnostic path and
raises Provider_Error. Private values own labels and nested vectors. Marshalling
uses final-sized interval/event arrays and controlled C-string owners; nested
span addresses are assigned only after backing arrays are stable, and all
temporary storage is destroyed after the single synchronous call.

## Evidence boundary

The existing mock records complete most-recent intervals and exact counters.
Two-interval/two-ordered-event fixtures prove Unicode labels, all represented
fields, signed timings, >32-bit counts, signed zero/infinity/NaN, empty nested
spans, omitted defaults and the Ada symbolic default. Separate start-boundary
fixtures prove INT64_MAX is not translated. Invalid inputs invoke no provider;
exception fixtures count one call and preserve closable Jobs. Repeat-50 paths
cover parent-first Close, deferred C2 shutdown, pending timeout, Cancel_Remaining,
full Cancel, Complete and snapshot access without sleeps.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` was rechecked in
`interfaces/squall-rf-mel-impl/src/SquallC2MEL.h`: addJobIntervals, flush and
cancelRemainingJobIntervals have empty bodies. The helper-free C and safe Ada
ProductRx clients and second-Job test exercise the new calls, but provide only
production call-path/lifecycle integration evidence—not interval payload
fidelity, scheduler ordering, executed timing or continuation scheduling.

JobIntervalStatus callbacks/payloads, JobEventLogInfo, extendJobEvent,
TransmitEvent, modulation, pointing, weights, LocalFunctionCommand, context,
ProductStreamParams, endpoint routing, conditional semantics and broader
multi-group/TX JobRequests remain deferred. Rust/Python changes are raw parity
only; Ada is the sole new safe API.

## Validation results

Local results:

| Check | Result |
|---|---|
| `make test-native` | 255/255; baseline 254/254 |
| Native RX JobInterval path | 50/50 in the existing C11 Job contract |
| GCC pinned setters/getters/sentinel probe | Pass, GCC 14.2.0 |
| Clang Release full native suite + pinned probe | 255/255, Clang 19.1.7 |
| `make test-build-isolation` | Pass |
| `make format-ada`, `make check-ada-format` | Pass |
| `alr -C ada build`, `alr -C ada/tests run` | Pass |
| Direct GPR (`alr -C ada/tests exec -- sh scripts/test_ada.sh`) | Pass |
| Safe Ada RX JobInterval path | 50/50, including symbolic default and start boundaries |
| `make test-rust` | Pass (73 tests across workspace targets) |
| Workspace cargo check / clippy all-targets `-D warnings` / fmt check | Pass |
| `make test-python` | 112/112 |
| Fresh GCC production Release dynamic export audit | 146, exactly three additions, zero removals |
| ABI version | 0.1; original 143 declarations unchanged |
| Vendor SHA-256 audit | 804/804, zero vendor blob/pin changes |
| Squall safe Ada two-Job integration | Pass, parent-first preserved |
| Squall C ProductRx / safe Ada ProductRx | Pass, existing data/counter/ownership evidence preserved |
| `AMS_MEL_SQUALL_REPEAT=3` C / Ada ProductRx | Both 3/3 |
| `git diff --check` | Pass |

Real-provider commands used `SQUALL_SOURCE_DIR=/home/zboll/git/squall`.
Immediate successive runtime launches encountered occupied health ports (21313,
24313); these were environmental collisions, not product failures. Successful
single/repeat runs used isolated port sets 24203/24318/24313/24314/24601,
25203/25318/25313/25314/25601, 26203/26318/26313/26314/26601 and
27203/27318/27313/27314/27601. No unrelated stack/container was replaced.

Initial validation found and corrected Ada by-value span convention
(`C_Pass_By_Copy`) and GNAT float-validity rejection of intentional IEEE special
values. Only optional floating validity checks are disabled in this command
body/test fixture (`Validity_Checks ("F")`); other validity/range checks remain.
The initial incorrect convention also caused an Ada integration crash; final
mock and real runs passed after correction. Mutating operations were not retried
for diagnostics. All existing RFMFAInfo, PhysicalData and TxPowerMode regression
tests remain in the passing native/Ada/real receive suites.

The additional final-newline utility reports four pre-existing baseline task
documents without final newlines (034B2B1, 034B2B2, 034C1 and 034C3); those
unrelated files are preserved. The new task document has its final newline.
Hosted exact-head GCC/Clang/Ada and other workflow results are reported in the
PR/final report, separately from local evidence.
