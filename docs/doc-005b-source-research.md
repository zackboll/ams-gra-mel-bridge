# DOC-005B source research

## Starting identity and provenance

Fetched main is exactly `8f4f0b50b0bc0b9db0ab46b99f4dcf238c4d6c08`,
the DOC-005A merge. No ancestry reconciliation was necessary. No equivalent
DOC-005B local/remote branch, worktree or open PR existed. The unrelated
`docs/005-rf-admin-c2` branch and all prior worktrees/evidence remain intact.
Work is isolated at `/home/zboll/git/ams-mel-doc-005b`, branch
`docs/005b-rf-c2-va-job`; external evidence:
`/home/zboll/git/ams-mel-doc-005b-evidence/r1`.

The pins in `docs/upstream-provenance.md` agree with the actual upstream
checkout commits. All 85 committed RF/Common headers were compared against
immutable Git blobs; per-file hashes are in external
`verified-upstream-headers.json`:

- RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
- Common MEL: `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.
- Historical Squall evidence only: `b1015728f904c799fa0c07489fce48e78f67845f`.

No upstream fetch, provider build, container or hardware is needed for ordinary
documentation qualification. Historical provider reports are not new executions.

## Source evidence matrix

RF declarations below are relative to
`native/vendor/rf-mel/include/rfmel`; Common declarations are relative to
`native/vendor/common-mel/include/mel`, at the revisions above. Implementation
citations name stable operations rather than comment-sensitive line numbers.

| Family | Pinned declaration | Current native implementation | Safe Ada implementation | Existing tests |
| --- | --- | --- | --- | --- |
| Independent C2 owner | `factory/RFCreateFunctions.h`, `rfmeltypes/RFMEL.h`, `c2/C2MEL.h` | `native/src/rf_c2.cpp`: open, close, finish_c2_shutdown; `native/src/internal/rf_c2.hpp`: counted claims/destruction order | `ada/src/ams-mel-rf-c2.adb`: Open, Close, controlled Finalize | `native/tests/test_rf_c2.c`; `ada/tests/src/ams_mel_rf_c2_tests.adb` |
| VA inputs and asynchronous completion | `c2/C2MEL.h`: requestVirtualAperture; Common `library/CommonMEL.h`: RequestFor; `library/Error.h`: ErrorCode | `native/src/rf_virtual_aperture.cpp`: submit, settle, run_worker, wait, request close | VA config builders, Submit, Wait and request Finalize in the same Ada body | `native/tests/test_rf_va.c`; safe Ada C2 tests |
| Unique VA Claim/snapshots | `c2/VirtualAperture.h`: getVAInstanceIDs, getElementGroupLabels, isSingleGroup | VA request claim and immutable view; sorted set iteration, ordered label copy, cached getter-started failures | Claim copies IDs/labels/flag to Ada vectors; getters do not require a live native view | C VA cases and safe Ada parent-first snapshot checks |
| RX/TX requirements | `c2/ElementGroupCommand.h`, `c2/JobRequest.h`, `c2/VirtualAperture.h`: createElementGroupCommand/requestJob | `native/src/rf_job.cpp`: submit_extended_job, PreparedGroup, submit_prepared; mode check before setters | RX/TX constructors/appends, Submit_Job serializes v4 | `native/tests/test_rf_job_v2.c`, `native/tests/test_rf_job_tx_groups.c`; Ada job_v2/job_tx_groups suites |
| UTC and exact pointing | `jobs/Pointing.h`, `rfmeltypes/RFMELTypes.h`: PointingType; pinned Math UTCTime | valid_pointing, prepare_pointing, exact active-field construction in rf_job.cpp | Create_UTC_Time, five pointing constructors, active-field accessors, Raw_Pointing | `native/tests/test_rf_job_pointing.c`; `ada/tests/src/ams_mel_rf_job_pointing.adb` |
| Job request and Claim | `c2/VirtualAperture.h`: requestJob; `c2/JobDetail.h`: snapshot getters | request settle/worker/cache/Claim; `native/src/internal/rf_va_job_parent.hpp`: strong VA plus sibling claim | Job Wait, Claim and copied snapshot getters | `native/tests/test_rf_job.c`; `ada/tests/src/ams_mel_rf_job_tests.adb` |
| Finalize/status/cancel/Close | `c2/JobDetail.h`: finalize/cancelJob; `c2/VirtualAperture.h`: JobStatus; `c2/CancelStatus.h`: Boolean independent of error | JobState, finalize_worker, one-shot finalize/cancel, wait_status, close | Finalize_Job, Wait_Job_Status, Cancel_Job, explicit Close and non-raising Finalize | C lifecycle_case/lifecycle_tests/retention cases; safe Ada lifecycle tests |

## Reconciled semantics and limitations

### 005B1 — C2 and Virtual Aperture

C2 open rejects embedded NUL before temporary string allocation but does not
validate path/configuration as UTF-8. VA configuration builders reject NUL;
native submission additionally validates UTF-8 and copies all arguments before
the provider call, preserving all UCI UUID bytes and labels. Empty text survives.

One move-only child claim follows each VA future through cached success into
the uniquely claimed VA. C2 Close consumes the public handle, stops admission
and defers shutdown while children remain. A worker alone consumes the future.
Wait timeout is not cancellation; request Close is nonblocking abandonment, not
destruction of a pending provider resource. An abandoned successful resource is
destroyed before its claim releases. Post-provider publication/worker failure
retains the graph rather than guessing it is safe to unload code.

Claim snapshots a sorted provider ID set, ordered validated labels and Boolean
flag before native publication; Ada copies them again. Getter-started failure is
cached, not retried. Ada snapshot copying can still raise; controlled ownership
cleans the newly acquired native owner. Public parent-first Close is supported;
after C2 closure, however, new sibling child admission is prohibited.

Synchronous explicit cleanup can report deferred shutdown exceptions; background
cleanup cannot. Shutdown failure permanently retains the uncertain C2/DSO graph
without retry. Finalizers do not raise. Dropping one DSO reference is not a
guarantee that all references are gone or that the DSO unloads immediately.

### 005B2 — Job configuration, pointing and request

UTC uses full signed integral seconds and a fraction in
`0 .. 999_999_999_999_999` femtoseconds, rejected rather than normalized outside
that range. Duration/lookahead preserve the full signed 64-bit domain. There is
no local minimum-start/maximum-completion ordering or scheduling comparison.

Pointing is the pinned five-alternative variant, not an upstream enum. ECEF
position is meters, velocity meters/second; LLA is WGS-84 radians/meters with
NED meters/second. Relative angles are radians. Constructors perform no frame
conversion, clamping, normalization or finite-value check; IEEE classification
is preserved, not NaN payload bits. Wrong-kind accessors raise Constraint_Error.
Only ECEF/LLA have a validity-time accessor. The absent estimated point omits
the provider setter; no uninitialized default ECEF numeric storage is read.

Duty must be finite in `(0,1]`; frequency bounds finite and ordered, but need
not be positive. Labels are copied and NUL-rejected in Ada, UTF-8-checked at
native submission. RX endpoint append groups IDs by pipe label; duplicates
within that collection reject. Native endpoint IDs become sets. Group order,
repeated labels and expected-pointing order/duplicates survive. TX power level
is not a power-mode ID. The current bridge accepts RX-only/TX-only/mixed
requirements, but requires the provider command's mode to match. No TX expected
pointing or endpoint-association API is exposed by this configuration family.
The pinned interruptable flag selects resume (True) versus terminate (False)
after higher-priority interruption; the bridge preserves it without enforcing
a scheduler policy. New Job submission requires C2 child admission still open,
even though already submitted work survives public C2 Close.

Capability/activity byte vectors allow arbitrary lengths, empty input, zero and
high bits; no UUID-length rule or text interpretation. Power-mode IDs become a
set; instance selections preserve order/duplicates. Replacement byte/ID fields
are built in temporary Ada storage before assignment. Provider setter mutations
are not rolled back after a later provider failure; no global failure-atomicity
claim is made. Bridge preparation/validation failure publishes no request.

Job submission retains a strong provider VA and a new sibling C2 claim; the
public VA keeps its original claim. Parent owners may close after submission
without invalidating pending work or the Job. Request success, provider rejection,
future exception and native failure are distinct. Exactly one successful Claim
transfers ownership. Claim snapshots the exposed JobDetail scalar fields and only the
ordered `getRxStreamIDs(0)` list, not streams for all element groups. These are
reported values, not evidence of scheduled/physical activity.

### 005B3 — Job lifecycle

Current source and C lifecycle tests distinguish synchronous Finalize invocation
from the single future completion. Once provider invocation is attempted, both
normal return and synchronous failure are cached; repeated calls do not resend.
Pre-invocation allocation failure does not consume the provider attempt. Cancel
before Finalize prevents a new Finalize, even after semantic False or exception;
an already attempted Finalize remains repeatable from cache after Cancel.

The completion worker publishes a cached result, even if the returned JobStatus
is None or In_Progress. Thus "terminal cache" means the future was consumed,
not that RF execution is complete; Wait does not poll a live status stream.
All six known statuses are data, including failures. Unknown status, invalid
future, synchronous exception and future exception raise Provider_Error in Ada.
Timeout raises Timeout_Error without cancellation or ownership consumption.

CancelStatus Boolean and error are independent: False/None is valid rejection,
not success inferred from None. Cancel invokes the provider synchronously once
and caches its result or failure; it does not itself destroy the Job. Close does
not implicitly Finalize or Cancel. A pending finalize worker retains JobDetail,
VA, C2 claim and DSO after public Close, potentially indefinitely. Without pending
work, cleanup releases JobDetail, VA, then the claim. Ada-owned snapshots remain
readable after closure; one-based vector indices outside count raise
Constraint_Error. Snapshot fields are not refreshed after lifecycle commands.

## Historical reports and evidence boundaries

Read 034B1B1, 034B1B2, 034B2A1, 034B2A2, 034B2B1, 034B2B2, 034F1,
034F2, 034F3 and corrective 034F3C1. Early reports' single-RX-group limitations
are historical, superseded by current v4 configuration; the lifecycle retention
and cache paths remain current. The corrective report changes test observation
lifetime, not production cleanup semantics.

Historical pinned Squall observations include VA snapshots, RX Job requests,
parent-first retention, Finalize-driven receive, immediate timeout, received
ProductRx events, successful Cancel and Complete status. F3 explicitly observes
TX/RX mode mismatch on the receive-only provider before requestJob. Positive
mixed/TX requirements and complete value fidelity are mock evidence, not positive
Squall TX submission, scheduling, emission or general provider guarantees. No
new real-provider or hardware run is claimed by DOC-005B.

RX/TX events, modulation/waveforms and interval configuration/operations remain
DOC-005C. References to their existence do not document their gap identities.

## Final local qualification — PASS

The final corrected source passed all required gates, with full logs and actual
zero exit statuses retained under external `r1/final`:

| Gate | Result |
| --- | --- |
| Format and format check before semantic compilation | PASS |
| Strict semantic compilation and GNATdoc twice | PASS |
| Exact gap/warning ratchets, profiles and anchors | PASS |
| Two fresh sites, all 32 files byte-identical | PASS |
| HTML structure and links | PASS: 31 pages / 1,307 links |
| Documentation unit/mutation tests | PASS: 75 |
| Dedicated integrity tests | PASS: six |
| Literal-aware executable tokens and protected/vendor bytes | PASS |
| make test-native | PASS: 434/434 |
| make test-ada | PASS: full safe Ada contracts and focused cases |
| make check | PASS: isolation, native, formatting, Ada and text gates |
| make check-ada-format after make check | PASS |
| Final source hash against frozen tested source | PASS |
| Whitespace and final newlines | PASS |

Production gates ran sequentially, without overlapping mutable native trees.
Final specification SHA-256:
`22e05187ab1cce06e2ffb9f90eb0dd143bc1ab2751d4525921e6f1789890fbdd`.
Independent complete-diff review and the seven-file allowlist passed. No tests,
contracts, warnings or policy were weakened. No real-provider/hardware run is
claimed. Main was refetched and remained at the exact starting merge. Hosted
checks and local/remote/PR-head equality are separate publication observations,
recorded externally and on the PR; the PR must remain open for independent review.
