# IRMEL-ADA-006 — C2 Calibration Status qualification

## Scope and upstream mappings

Base: `4c5d9c29b30694afb19a3d5a75701779b21fe439` (fetched `origin/main`, includes PR #163).
One optional IR vertical slice; qualification uses the separate mock provider only.

| Pinned upstream operation | Native C ABI | Safe Ada |
|---|---|---|
| `C2Channel::send(CalibrationStatusCmd)` | `ams_mel_ir_c2_submit_calibration_status`, request Wait/Close | `AMS.MEL.IR.C2.Submit_Calibration_Status`; `Calibration_Status.Status_Request`, `Wait`, `Close` |
| `C2Channel::registerMetadataCallback(CalibrationStatus)` | `ams_mel_ir_c2_calibration_status_open`, receive/counters/close, event view/close | `AMS.MEL.IR.C2.Metadata.Open_Calibration_Status`, `Receive_Calibration_Status`, `Calibration_Status_Counters` |

The canonical [Ada specification](../ada/src/ams-mel-ir-calibration_status.ads)
derives the request from a limited controlled owner (never an ordinary subtype).
The metadata owner is independent of base `C2.Metadata.Open`, of the
CalibrationConfiguration owner and of every request. CalibrationConfiguration
behaviour is unchanged.

## Complete representation and ownership

- `CalibrationStatusCmd`: both independent uint32 fields, including 0 and UINT32_MAX,
  forwarded unchanged. No response correlation is inferred.
- `CalibrationStatus`: two independent ordered collections, `active_calibration_ids`
  (complete UCI IDs) and `completed_calibrations`. No equal lengths, one-to-one
  correspondence, uniqueness or ordering relationship is assumed; duplicates are kept.
- `SubsystemCompletedCalibration`: complete ID (16 UUID bytes and label), `time_tag_ns`
  as the exact signed `std::chrono::nanoseconds::count()` in int64 (no epoch, no unit
  conversion; INT64_MIN/MAX preserved), result, complete fail reason and an
  independently sized ordered item vector.
- `SubsystemCompletedCalibrationItemType`: item name, result, fail reason; each item is
  independently owned.
- `SubsystemCalibrationResult`: NotSet=0, Completed=1, Failed=2, Interrupted=3,
  NotCalibrated=4, bound by native static assertions, C/Ada layout checks and the
  Ada representation clause. MaxExclusive=5 and unknown values are rejected, never
  remapped (request: `PROVIDER_FAILED`; callback: malformed counter).
- Native `Snapshot` deep-copies the provider value, then builds every view vector at
  final size before publishing any pointer. Request views live until request Close;
  event views until event Close; neither owns a provider result/deleter. Ada copies
  every nested value into private owned types and holds no native pointer.
- UTF-8 without embedded NUL is required for labels, names and fail reasons. Counts and
  string sizes must fit INT32_MAX and checked pointer arithmetic. Private Ada
  conversion rejects null/nonzero spans, overflow, NUL and invalid results; partial
  conversion never escapes.

## Request/callback lifecycle evidence

The IRMEL-ADA-005 request architecture is reused unchanged: Session `CompletionPermit`,
`claim_c2_submission(Enabled)` under the lifecycle lock,
`CommonRequestClaim`/`finish_c2_request`, unlocked send, exactly one worker consuming
the future, cached terminal result/diagnostic, deferred C2/Session/library cleanup and
allocation-free emergency retention. Timeout and observer Close do not cancel. Provider
rejection, null success, malformed data, future exception, unknown MEL code and
allocation failure have distinct native statuses; cleanup failure never becomes success.
Completion-probe family `CalibrationStatus` (9) is appended to the PR #164
process-lifetime observation; existing indices are unchanged. The mock appends send
family 10 for its promise gate.

Registration records one-shot attempts under the C2 lock while Attached/Enabled and
calls the exact provider overload unlocked; synchronous delivery is supported.
NotSupported, Fail and exceptions are distinct. Whole statuses are copied before
callback return into one bounded DROP-INCOMING queue with saturating
received/dropped/malformed counters. Queued events drain after shutdown. Captured
state survives public Close and late callbacks; physical C2 teardown waits for counted
in-flight callbacks. Metadata Close and callback delivery never complete or cancel a
request, and neither path requires the other.

## Test and mutation evidence

`native/tests/test_ir_calibration_status.c` (C11) covers zero/one/multiple and
different-length collections, ordered duplicates, all UUID bytes and labels, all five
results at both levels, 0/-1/INT64_MIN/INT64_MAX/positive time tags, empty, 600- and
700-byte and multi-byte UTF-8 strings, sentinel/unknown results at both levels, invalid
UTF-8 (including overlong and surrogate encodings) and NUL in every string category,
null success, rejection (including a 700-byte description), future/send exceptions,
invalid future, timeout then success, cached Wait, request Close before completion,
C2 and Session Close while pending, Session-specific admission exhaustion with a
refused-submission negative control, post-send allocation/worker-launch retention,
deferred cleanup failure, registration NotSupported/Fail/exception one-shot,
synchronous delivery, overflow, malformed callbacks, callback copy allocation failure,
in-flight callback during closure, late callbacks and request/callback independence.
The Ada suite covers the same behaviours through the safe API plus private malformed
native-view rejection and C/Ada layout comparison.

Executed mutation controls (each restored and revalidated byte-identically):

| Mutation | Result |
|---|---|
| Native: drop last nested completed item from each item span | C contract FAIL at fidelity |
| Native: zero every nested item result | C contract FAIL at fidelity |
| Native: publish empty active collection | C contract FAIL at fidelity |
| Ada: drop last nested item during conversion | Ada focused suite FAIL (`complete rich status seed 0`) |

Predicate controls additionally remove an item, an item result, truncate an item
reason, remap an overall result, perturb a time tag, and drop either collection or the
duplicate active ID; each must fail the full fidelity predicate (C and Ada).

## ABI and coverage

Production inventory: **266 AMS_MEL_0.1 exports**, baseline 257 + nine additions, zero
removals and zero test-only production exports. ABI version unchanged; no existing v1
layout, export signature or enum value changes. New C11 layouts (LP64): command 8 bytes;
item 40 (result 16, fail reason 24); completed 80 (time tag 32, result 40, fail reason
48, items 64); status 32 (completed 16); result 40. A shared C layout probe qualifies
private Python ctypes and Rust sys mirrors; no safe Python or Rust API is added.

Exactly two operations and five data declarations change `missing -> safe_ada`, with
field-level representation evidence; safe Ada operations increase **41 -> 43**. The
pinned upstream catalog, catalog hash, vendor headers, historical source hashes and
unrelated classifications are unchanged; only reviewed binding evidence and its anchor
change.

## Limitations

Mock-provider-only qualification: no real IR provider, Squall, credentials, containers,
hardware or calibration-execution qualification. Provider C++ ABI compatibility remains
necessary. Upstream has no callback unregister; exceptional post-send/teardown paths
retain resources rather than unload live provider code. Counts/string sizes above
INT32_MAX are rejected. Calibration execution commands, EraseCommand, Scheduling and RF
remain out of scope. No historical documentation campaign or local FULL_STRESS was run.
Local command results are recorded in the pull request description.
