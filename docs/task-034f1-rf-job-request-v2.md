# Task 034F1 — RX JobRequest v2 and safe Ada

## Starting point and scope

PR #74 was verified MERGED, with merge commit and actual fetched origin/main
`db3ab3ab71597f5532d92138302204b4e76bfae6`. Starting worktree was clean;
no equivalent branch or open PR existed. Work is on
`feature/034f1-rf-job-request-v2`, created from that origin/main.
Measured pristine baseline: native 273/273, Python 120/120, production function
exports 189, documented vendor checksums 804/804. Aggregate `make check` passed
in the Alire toolchain environment. The initial plain-shell attempt lacked
GNAT/GPRbuild on PATH and is not counted as a pass. Logs are preserved under
`/tmp/ams-mel-034f1-logs`.

This is RX request configuration, not complete JobRequest coverage. Deferred:
estimatedStabPoint, RX expected pointing angles, TX commands/events/intervals,
MFADrivenControls/JIB machinery, rejection callbacks/context, CachedWaveform,
WaveformTxEndpoint, Weights, external/RDMA and VADB. No new provider owner or
vendor dependency is needed. The requested separate test_rf_job_lifecycle.c is
absent at this baseline; existing lifecycle coverage lives in test_rf_job.c,
whose source remains unchanged.

## Additive C contract

Exactly one new export: `ams_mel_rf_virtual_aperture_submit_job_v2`.
Same existing request/wait/claim/close owners and timeout/abandonment rules.
ABI remains 0.1. Frozen v1 records, export signature and caller requirements
remain unchanged; native v1 does not round-trip through the public v2 ABI.

New records:

- ams_mel_rf_utc_time_v1
- ams_mel_rf_rx_data_pipe_endpoint_config_v1
- ams_mel_rf_rx_data_pipe_endpoint_config_span_v1
- ams_mel_rf_rx_element_group_config_v2
- ams_mel_rf_rx_element_group_config_span_v2
- ams_mel_rf_job_request_config_v2

UTC is integral signed int64 seconds plus fractional signed int64 femtoseconds.
Input requires `0 <= fractional_femtoseconds < 1_000_000_000_000_000`, with the
full seconds domain. Invalid fractions return INVALID_ARGUMENT before command
creation, requestJob or publication; no silent normalization. Construction uses
exact `UTCTime{std::chrono::seconds{seconds}, Femtoseconds{fractional}}`.
No local min <= max relationship check. Duration/lookahead pass exact signed
int64 femtoseconds, including INT64_MIN, zero and INT64_MAX, without quantization,
clamping or scheduler comparisons.

At least one RX group is required. Caller group order and repeated labels remain.
Each group creates a command once by label, requires nonnull, calls getMode once
and requires RX, sets duty once, adds frequencies in caller order, adds pipe
endpoint sets in caller entry order, and adds the command to JobRequest once.
The fully constructed request reaches requestJob once. UTF-8/NUL-free labels,
finite duty in (0,1], finite ordered frequency bounds, well-formed spans and
nonempty duplicate-free per-entry endpoint sets are validated before provider
entry. Repeated pipe labels cause repeated calls, not bridge deduplication; the
provider decides how repeated calls affect its resulting connection container.
No E1 membership check, E3 descriptors, E5 pipe lookup or E6 TX query occurs.

Capability/activity IDs are arbitrary vectors: exact length, order, embedded
zeros, all high-bit bytes and empty values. No UCI UUID-length requirement or
string interpretation. Instance selection preserves order/duplicates/full uint32
values. TX power-mode IDs become a set before provider construction: duplicates
collapse, input order is not promised, empty is valid and all uint32 values pass.
This field does not activate TX execution.

## Shared hardened async path and cleanup

submit_prepared is the sole internal post-validation pipeline for both exports.
It owns parent acquisition, requestJob, future validity, preallocated completion/
input/public request/worker holder, worker launch, publication failpoints and
emergency self-retention. Existing Wait/Claim/Close/worker implementation is
unchanged. V1 retains the original input validation and five explicit setters.
V2 copies every group/string/frequency/endpoint set, vector and power-mode set
before acquiring the provider parent, then uses exactly the scoped setters.

Builder locals and JobRequest are nested inside the provider-call try scope.
On creation/null/TX/mode/setter failure at any group, the failing command and
all earlier request commands unwind before VA reset and C2 sibling-claim release.
No partial request reaches requestJob. Log evidence checks all command destruction
before VA destruction, then C2 shutdown; later valid submission succeeds.
Worker launch/post-provider allocation/publication failure retains the complete
future/parent graph permanently in isolated processes, as before.

The mock directly inspects all scoped request getters. Deferred defaults remain
null NextJIB/rejection callbacks, zero numJIBs, std::any holding nullptr context,
and the pinned default ECEF pointing variant, three-component vector dimensions
and zero UTC time. No numeric location/velocity default is promised: pinned
Boost c_vector's default constructor leaves component storage uninitialized.
Tests must not read those indeterminate doubles; production never sets pointing.
RX command pointing vectors remain empty and setters for pointing/TX are forbidden.
Direct v1 additionally checks zero min/max time, duration/lookahead and [0]
capability/activity/power modes, alongside original one-group fidelity.

## Safe Ada

Existing RX_Element_Group_Config/Create_RX_Element_Group/frequency append,
Append_Endpoint_ID(Group, ID), Job_Config/Create_Job_Config and Submit_Job remain
source-compatible. Create_Job_Config inserts its supplied group first. New public
Append_RX_Element_Group adds ordered groups; Append_Endpoint_ID's explicit String
pipe-label overload collects endpoints per label in insertion order. Duplicate
IDs within one collection raise Constraint_Error; same IDs in different pipes
are valid. Empty collections are omitted. Default append targets the original
Create_RX_Element_Group pipe label.

UTC_Time is private with canonical constructor and component accessors; invalid
fractions raise Constraint_Error. Byte_Array and Unsigned_32_Array are ordinary
Ada arrays. Added setters: Set_Min_Start_Time, Set_Max_Complete_Time,
Set_Duration_Femtoseconds, Set_Capability_ID, Set_Activity_ID,
Set_TX_Power_Mode_IDs and Set_Lookahead_Femtoseconds.

Safe Submit_Job now privately serializes v2 using final-sized backing and
controlled strings, without public C spans/addresses/imports. Historical defaults
remain 0/0 UTC, zero duration/lookahead and [0] capability/activity/power-mode IDs.
Existing safe one-group tests still exercise their original public source and
mock fidelity; public F1 fixtures exercise three groups, multiple associations,
canonical boundaries, binary/empty IDs, set/vector semantics, signed extrema,
timeout/claim and parent-first closure. Raw Rust/private Python record, layout,
signature and inventory parity is synchronized; no safe/public v2 API there.

## Pinned Squall evidence limit

Rechecked pin `b1015728f904c799fa0c07489fce48e78f67845f`,
SquallC2MEL.cc:395..453. It requires groups, iterates all, validates RX mode,
configured label/duty/frequency, tunes with the first available frequency and
copies requestId into JobDetail. It ignores added scheduling/identity fields.
Safe Ada Job integration constructs two `0` groups with duties 0.625/0.5,
the active 915 MHz point ranges and nondefault fields while preserving lifecycle/status/
extension assertions. Success proves full-request acceptance and group iteration,
not semantic use of ignored fields. C ProductRx activation remains native v1;
safe Ada ProductRx keeps its unchanged one-group source/defaults.

## Validation and reliability

GCC 14 and Clang 19 actual-vendored-header probes pass warnings-as-errors,
including exact setters/getters, IDs, seconds/Femtoseconds signed int64 and femto
denominator. Linked executable probes check canonical getters at seconds extrema.
Initial build errors (mock vector comparison, UCI fixture member name, Ada named
aggregate) were corrected normally. An expanded lifetime fixture initially closed
C2 before acquiring a new sibling Job claim; the existing contract rejected it.
Fixture sequencing was corrected, not the ownership contract. Python's initial
new binding used an incorrect library variable; corrected to the existing
_LIBRARY naming. First logs are preserved. No timeout increase/assertion weakening,
filesystem-exhaustion workaround or unrelated container cleanup was performed.

The first Squall Job fixture used 100/101 MHz and was correctly rejected. Inspection
of capabilitiesForStatus (SquallC2MEL.cc:80..88) showed rf_environment advertises
only its active center-frequency point, 915 MHz. Both groups were corrected to
that active range with distinct duties; assertions and provider policy were not
weakened. Original failed logs remain. Port preflight failures on 42313 were
confirmed TIME_WAIT without listener/task containers; unchanged targets waited
for actual bindability. The unrelated opencv-imgproc-publish-010 container was
untouched. No /tmp exhaustion occurred.

Starting-main hosted push 37206228820 had a pre-existing GCC Debug parallel-repeat
failure at test_rf_job.c:373 (mock_wait(shutdown)==1). The ordinary 273 suite and
other seven jobs passed. That log is preserved; it predates F1's shared helper.
No causal fix or timeout increase is claimed.

## Local validation results

| Command/audit | Result |
| --- | --- |
| make test-native | 274/274 |
| make test-build-isolation | PASS |
| make check-ada-format | PASS, after make format-ada |
| alr -C ada build | PASS |
| alr -C ada/tests run | PASS full public suite including legacy one-group and F1 |
| make test-rust | PASS all workspace suites |
| cargo check --workspace | PASS |
| cargo clippy --workspace --all-targets -- -D warnings | PASS |
| cargo fmt --all -- --check | PASS |
| make test-python | 121/121 |
| git diff --check | PASS |
| aggregate make check (Alire toolchain PATH) | PASS |
| GCC 14 / Clang 19 actual-header compiled and linked probes | PASS |
| native F1 repeat until-fail:50 | 50/50 |
| standalone public safe Ada F1 | 50/50 plus delayed timeout/claim |
| fresh production Release in /tmp/ams-mel-034f1-release | 190 functions, map/header parity, no test symbols |
| original C signatures/explicit records | 189 signatures and 165 records byte-identical, including v1 Job |
| Release runtime ABI version | 0.1 |
| full vendor checksum and baseline diff audit | 804/804, unchanged |
| native v1/C Squall ProductRx v1 source comparison | unchanged |

| make test-squall-rf-ada-va | PASS |
| make test-squall-rf-ada-job | PASS after correcting fixture to active 915 MHz range; two groups per Job, lifecycle/status/extension assertions preserved |
| make test-squall-rf-rx | PASS unchanged v1 activation, 8/8 events, 7 later events differ, counters received=queued=8 and all error/drop counters zero |
| make test-squall-rf-ada | PASS unchanged application using v2 defaults, 8/8 receive assertions/counters/parent-first teardown |

All four use the exact Squall pin. Ports 42303/42318/42313/42314/42601 were
isolated; the corrected Job retry used verified-bindable 43303/43318/43313/43314/
43601. No unrelated container was stopped/replaced. Squall's resulting JobDetail
reported zero scheduling fields even though nondefault values were submitted:
this matches the documented non-interpretation limit, not a fidelity claim.

Final publication and hosted results are recorded in the PR and final report.

## Hosted Release corrective

Initial head 650ed11's push/PR GCC Release jobs exposed an invalid new mock
assertion that read default ECEF location/velocity doubles. Actual pinned
Boost vector.hpp:2448..2449 initializes size only, not data_. The assertion and
documentation were corrected to defined default state (ECEF variant, dimensions,
UTC), retaining forbidden pointing setter checks. No provider default is locally
initialized, no vendor file is changed, and no pointing support is added. This
is a test undefined-read correction, not a weakening of any published numeric
contract. Initial hosted logs are preserved. A normal corrective commit follows
the original three commits and the new head is revalidated.

