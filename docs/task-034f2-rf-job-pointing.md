# Task 034F2 — RF PointingType and RX JobRequest v3

## Starting point

Fetched origin and verified PR #75 MERGED, with actual origin/main
`6e5c869d29ef07fbf41ae3677259d59f97363ddb`. Starting worktree was clean;
no equivalent branch or open PR existed. Branch:
`feature/034f2-rf-job-pointing`, created directly from updated origin/main.
Measured pristine baseline: native 274/274, Python 121/121, fresh production
Release 190 functions, documented vendor checksums 804/804. Logs are under
`/tmp/ams-mel-034f2`. Pristine aggregate `make check` also passes. The archive was given isolated local
Git metadata for the newline/diff gates; source bytes match the starting commit.
Starting-main hosted push run 37239455962 is also successful in all eight jobs.

## Exact value construction and ABI

RF MEL pin `762ce84c5555dd0f3ea66f36b321fecf8839b89f` defines PointingType
as exactly `std::variant<ECEFPointing, LLAPointing, PlatformRelativePointing,
FaceRelativePointing, BaselineRelativePointing>`, with alternatives 0..4.
PointingType is @Required; ECEF/LLA/platform/baseline are conditional on their
respective pointing support, FaceRelative is @Required, and expected-pointing
methods are @RequiredIfReceive. No bridge capability query is inferred from those
classifications. It is not an upstream enum. Bridge `ams_mel_rf_pointing_kind_t` is uint32, with
exact constants AMS_MEL_RF_POINTING_ECEF=0, LLA=1, PLATFORM_RELATIVE=2,
FACE_RELATIVE=3, BASELINE_RELATIVE=4 and no MaxExclusive pseudo-value.

New fixed C records (no public union):

- `ams_mel_rf_vector3_v1`: x/y/z doubles.
- `ams_mel_rf_az_el_v1`: azimuth_rad/elevation_rad doubles.
- `ams_mel_rf_ecef_pointing_v1`: location_m, velocity_mps, time_of_validity.
- `ams_mel_rf_lla_pointing_v1`: latitude_rad/longitude_rad/altitude_m,
  velocity_north_mps/east_mps/down_mps, time_of_validity.
- `ams_mel_rf_pointing_v1`: kind, ecef, lla, platform_relative, face_relative,
  baseline_relative_conic_rad.
- `ams_mel_rf_pointing_span_v1`: const pointing pointer and size.
- `ams_mel_rf_rx_element_group_config_v3`: nested frozen v2 group and ordered
  expected_pointing_angles span.
- `ams_mel_rf_rx_element_group_config_span_v3`: const group pointer and size.
- `ams_mel_rf_job_request_config_v3`: all F1 request fields with v3 groups,
  followed by has_estimated_stab_point and estimated_stab_point.

The one additive export is `ams_mel_rf_virtual_aperture_submit_job_v3`, using
the existing request Wait/Claim/Close and Job owners. ABI remains 0.1. V1/v2
records and signatures are unchanged; direct native v2 does not call public v3.
Raw Ada, Rust and private Python bindings/layout/signature inventories match.
There is no safe Rust or public Python pointing/Job v3 API.

All six ECEF location/velocity components and all three NED velocity components
are explicitly assigned before setter/copy construction. LLA uses the exact
LLAPoint triple. Relative alternatives use their exact AzEl or conic constructors.
No provider object layout/memcpy, geodesy helper or coordinate conversion is used.
Meters, meters/second and radians pass as doubles, including out-of-conventional
range values, negative altitude/conic, signed zero, infinity and NaN. No finite
checks, clamping, wrapping, normalization or NaN payload-bit promise.

Only active ECEF/LLA UTC fractions must satisfy `0 <= fs < 10^15`; full signed
int64 seconds remain. Inactive payloads, including both inactive UTC records,
are ignored. has_estimated_stab_point is exactly 0/1; 0 does not validate or set
the point, 1 calls setEstimatedStabPoint once. Default pinned ECEF numeric vector
storage is never read or claimed zero. Defined variant/dimension/UTC observation
in default regression tests preserves F1's corrective rule.

## Preparation, ordering and lifetime

F1 strings/vectors/sets and all active PointingType values are prepared before
provider-parent acquisition. Preparation allocation failure returns INTERNAL_ERROR
without commands/requestJob/public owner. V2/v3 share a private extended builder;
v1/v2/v3 all reuse the unchanged submit_prepared future/worker/publication,
abandonment and emergency-retention path. No third worker or request owner.

Per group: create by label, getMode once/require RX, set duty, add frequencies
in caller order, add endpoint sets in pipe-entry order, add pointings in caller
order, then addElementGroup. One requestJob follows all groups. Empty pointing
spans make no calls; order, repeated values and duplicates survive. Estimated
stab point is independent: never injected into or inferred from group points.
There is no E1/E3/E4/E5/E6/RFMFAInfo/Weights cross-query or capability auto-gate;
provider requestJob remains authoritative.

## Safe Ada and mock evidence

Public `Pointing` is private and holds ordinary Ada-owned numbers/time. Five
unit-bearing Create_*_Pointing functions preserve exact coordinates. Public
Append_Expected_Pointing and Set/Clear_Estimated_Stab_Point extend the single
source-compatible Job API. Submit_Job privately uses final-sized v3 backing;
default configurations have no pointings and no estimated point. Numeric
conversion follows narrowly scoped IEEE handling, not global validity changes.

The existing F1 provider scenario checks all F1 fields and callback defaults.
Primary group order is [ECEF, platform], [LLA, face, face duplicate], [baseline].
Primary estimated LLA is distinct in every active component. Native checks also
exercise all five estimated alternatives. Mock getter observations are test-only;
production never validates by reading provider pointing getters. Setter operation
traces prove duty/frequency/endpoint/pointing order. Malformed kind/time/span/flag
tests require no commands/requestJob/owner. Relative inactive UTC garbage passes.
Face NaN/-0 and ECEF +/-infinity are classified/sign-checked, not locally rejected.
First/middle/final pointing setters throw standard/unknown/bad_alloc with exact
status mapping, all commands destroyed before VA/C2, no requestJob/owner, and
later success. A test-build-only preparation allocation failpoint proves no entry.

V3 repeats synchronous submit errors, invalid/stored-error futures, rejection,
null JobDetail, delayed timeout/claim, abandonment, parent-first teardown and
isolated worker-launch/post-provider-allocation/publication emergency retention.
Direct v1/v2 tests remain unchanged and active.

## Pinned Squall interpretation limit

Rechecked Squall `b1015728f904c799fa0c07489fce48e78f67845f`,
interfaces/squall-rf-mel-impl/src/SquallC2MEL.cc:183..185 and 395..453.
addExpectedPointingAngle stores PointingType in a vector. requestJob inspects
neither expected pointings nor estimatedStabPoint. The two-group safe Ada Job
integration supplies face/ECEF and platform/baseline plus independent LLA,
retaining valid 915 MHz frequencies/duties and all lifecycle assertions.

Successful real-provider evidence means: real Squall ElementGroupCommand
accepted bridge-issued addExpectedPointingAngle calls and requestJob accepted
the completed request. It does NOT prove geometry validation, scheduling use,
estimated-stab use or hardware pointing. Positive payload fidelity is mock evidence.
C ProductRx remains frozen v1; safe Ada ProductRx application source is unchanged
and defaults to no points through private v3 serialization.

## Scope and validation status

No TX commands/events/intervals/execution, MFADrivenControls/NextJIB/rejection
callbacks/context, Weights resources, CachedWaveform/WaveformTxEndpoint,
external/RDMA endpoints, VADB or conversion utilities are added. Vendor unchanged.
RX JobRequest now covers F1 fields plus all five alternatives for both pointing
inputs, but JobRequest functionality is not complete.

Measured fresh production Release exports: **191**, exactly the v3 addition,
no removal, exports.map parity and no test-only symbols. Runtime ABI **0.1**.
All **190 original signatures and 171 original explicit C records** are
byte-identical, including frozen v1/v2. Vendor audit: **804/804**, unchanged.
GCC 14 and Clang 19 actual-header linked probes pass, including exact variant
order/size/types, vector/double/UTC representations and pointing member signatures.

Local native suite: **275/275**. Full parallel native repeat: **275 x 50 =
13,750/13,750**, no failed/Not Run tests. Focused native F2: **50/50** after the final
native changes. Alire library build/full safe Ada suite pass, including five
constructors, all five estimated kinds, ordered duplicate pointings, IEEE values,
clear, canonical/invalid UTC, timeout/claim and parent-first ownership. Standalone
safe Ada F2: **50/50** plus delayed timeout/claim. Rust workspace tests/check,
Clippy all-targets with -D warnings and rustfmt check pass. Python corrected
inventory run: **122/122**. Final aggregate/integration/publication results follow
when completed; no incomplete check is counted as passing.

Normal commits so far: `ba7f67b` (native/raw parity) and `c342818` (safe Ada).
The submit_prepared implementation, run_worker implementation and emergency
retain implementation are byte-identical to F1; only preparation/builders differ.
Source inspection proves one guarded estimated setter and one ordered pointing
setter loop, with no provider pointing getters or geodesy calls in production.

Environment/reliability: no source timeout or assertion was weakened. The initial
aggregate baseline inadvertently overlapped editing and is not counted pristine;
the isolated exact baseline subsequently passed. Archive Alire-prefix/Git metadata
issues and a new-report missing newline are recorded in first logs and corrected
without compiled-source changes. Two Python inventory expectations were corrected
after the first 122-test run, then 122/122 passed. A prematurely concurrent aggregate
rerun was stopped (task-owned processes only); the ensuing test-tree run was not
counted and was rebuilt/rerun sequentially. `/tmp` did not fill; no unrelated work
or user container was reset/stashed/discarded/stopped. The legacy abandonment
timeout remains pre-existing/unproven; no causal fix is claimed.

## Local validation matrix

| Command/audit | Result |
| --- | --- |
| pristine starting-main make check | PASS in isolated exact-source checkout |
| make test-native | 275/275 |
| native F2 repeat until-fail:50 | 50/50 |
| full native parallel-4 repeat until-fail:50 | 13,750/13,750 (275 x 50), no Not Run |
| make test-build-isolation | PASS |
| make check-ada-format | PASS after make format-ada |
| alr -C ada build | PASS |
| alr -C ada/tests run | PASS full safe-language suite |
| standalone public safe Ada F2 | 50/50 plus delayed timeout/claim |
| make test-rust / workspace cargo check | PASS |
| cargo clippy --workspace --all-targets -- -D warnings | PASS |
| cargo fmt --all -- --check | PASS |
| make test-python | 122/122 and compileall |
| GCC 14 / Clang 19 actual-header linked probes | PASS |
| fresh production Release / runtime ABI | 191 functions, exact one addition, ABI 0.1 |
| frozen record/signature comparison | 171/171 and 190/190 byte-identical |
| vendor checksum/source comparison | 804/804 unchanged |
| git diff --check / final newline check | PASS |
| make test-squall-rf-ada-va | PASS |
| make test-squall-rf-ada-job | PASS two groups/all lifecycle assertions/pointing acceptance |
| make test-squall-rf-rx | PASS unchanged C v1 activation; 8/8 events, received=queued=8, zero drops/errors |
| make test-squall-rf-ada | PASS unchanged safe Ada application/no points; existing 8/8 assertions |

Squall targets use isolated ports 44303/44318/44313/44314/44601. Between targets,
no active listener/task runtime remained; TIME_WAIT was allowed to expire and
actual bindability was checked without changing source/targets/timeouts.
The unrelated opencv-imgproc-publish-010 container is untouched.
