# IRMEL-ADA-005 — C2 Calibration Configuration qualification

## Scope and upstream mappings

Base: `ad8addcf96164f39ffd8e8cf93a45bf66bd87cb8` (fetched `origin/main`).
One optional IR vertical slice; qualification uses the separate mock provider only.

| Pinned upstream operation | Native C ABI | Safe Ada |
|---|---|---|
| `C2Channel::send(CalibrationConfigurationCmd)` | `ams_mel_ir_c2_submit_calibration_configuration`, request Wait/Close | `AMS.MEL.IR.C2.Submit_Calibration_Configuration`; `Calibration_Configuration.Configuration_Request`, `Wait`, `Close` |
| `C2Channel::registerMetadataCallback(CalibrationConfiguration)` | `ams_mel_ir_c2_calibration_configuration_open`, receive/counters/close, event view/close | `AMS.MEL.IR.C2.Metadata.Open_Calibration_Configuration`, `Receive_Calibration_Configuration`, `Calibration_Configuration_Counters` |

The canonical [Ada specification](../ada/src/ams-mel-ir-calibration_configuration.ads)
uses a private extension of a limited controlled owner, not an ordinary subtype.
The independent metadata owner is never registered by base `C2.Metadata.Open`.

## Complete representation and ownership

- Both command fields are independent uint32 values, including 0 and UINT32_MAX.
  No request identifier is invented in the response or used for correlation.
- `Calibration_Collection` owns the ordered outer vector; each
  `Subsystem_Calibration` owns its complete calibration UCI ID (all sixteen UUID
  bytes and label), accepted interface, ordered item names, component IDs and
  capability IDs. The nested lengths are independent, never zipped or truncated.
- Accepted interfaces are precisely NotSet=0, SubsystemCalibrationCommand=1,
  SubsystemStateCommand=2. Native static assertions bind these and MaxExclusive=3
  to the pinned header. Sentinel/future values are rejected, never remapped.
- Native `Snapshot` copies all provider values and builds immutable C views after
  final sizing. Request views live until request Close; event views live until
  event Close. Neither owns a provider result/deleter. Ada copies every nested
  value before returning and holds no native pointer. Empty success is valid.
- UTF-8 without embedded NUL is required. Counts/string sizes must fit INT32_MAX
  and checked pointer arithmetic. Private Ada conversion rejects null/nonzero
  spans, overflow and invalid enums; partial conversion never escapes.

## Request/callback lifecycle evidence

The existing request architecture is reused: Session `CompletionPermit`,
`claim_c2_submission(Enabled)` under the lifecycle lock,
`CommonRequestClaim`/`finish_c2_request`, unlocked send, exactly one worker
consuming the future, cached terminal result/diagnostic, deferred C2/Session/
Control/library cleanup. Timeout and observer Close do not cancel. Post-send
allocation/launch failure retains the accounted future graph without allocation;
cleanup failure never becomes success. Provider rejection, null success, future
exception and allocation failure have distinct native statuses.

Registration records one-shot attempts under the C2 lock while Attached/Enabled,
but calls the exact provider overload unlocked. Synchronous delivery is supported.
NotSupported, Fail and exceptions are distinct. Whole callback vectors are copied
before return into one bounded DROP-INCOMING queue, with received/dropped/malformed
counters. Queued events drain after shutdown. Captured state survives public Close
and late callbacks; physical C2 teardown waits for counted in-flight callbacks.
No unregistration or disable-based quiescence is assumed. Metadata delivery and
future completion do not complete each other.

`native/tests/test_ir_calibration_configuration.c` exercises complete nested
fidelity, boundary command fields, concurrent request isolation, cached Wait,
empty/one/multiple entries, success versus rejection, null/exception/invalid data,
timeout then completion, early public closure, deferred teardown, post-send
retention, bounded overflow, synchronous delivery, one-shot registration, callback
independence, late callback safety and close while a bridge callback is active.
Admission is observed through a token bound to the exact Session, including a
refused-submission negative control and exact release to zero, never a family-global
owner-destruction count. An executed implementation mutation deliberately omitted the capability-ID copy:
the C fidelity test failed, and the implementation was restored and revalidated.
Predicate mutation controls additionally remove a capability and alter
an interface and must fail the full fidelity check. Private Ada malformed-view
tests reject nested span/count/enum corruption without publishing partial values.

## ABI and coverage

Production inventory verified with `nm -D --defined-only` against the complete
version script: **257 AMS_MEL_0.1 exports**, baseline 248 + nine additions,
zero removals and **zero test-only production exports**. No existing v1 layout,
export signature or enum value changes. New C11 layouts (LP64): command 8 bytes,
entry 88 bytes (names/components/capabilities offsets 40/56/72), outer span 16,
result 24. Shared C layout probes qualify private Python ctypes and Rust sys mirrors;
no new safe Python or Rust API is introduced.

Exactly two operations and four data declarations become `safe_ada`; safe Ada
operations increase **39 → 41**. The pinned upstream catalog, catalog hash,
frozen Git evidence, all 75 historical source hashes and unrelated classifications
remain unchanged. Only reviewed binding evidence and its anchor change.

## Focused local commands/results

| Command / focused gate | Result |
|---|---|
| `sh native/scripts/build.sh` | PASS, first-party warnings treated as errors |
| `ctest --test-dir native/build-tests -R 'ir_c2|ir_camera|ir_calibration|c2_submission|c2_finish|public_common|common_channel_view' --output-on-failure` | PASS 27/27, including five new calibration tests |
| `alr -C ada/tests exec -- gprbuild -p -P /home/zboll/git/ams-mel-irmel-ada-005/ada/tests/ams_mel_tests.gpr ams_mel_ir_calibration_focused.adb` and focused executable with test facade/mock provider | PASS: calibration, C2, C2 common/metadata, CameraCommand and CameraProtection suites |
| Python `unittest discover -s python/tests -p test_abi.py -v` with test facade | PASS 49/49 |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel-sys --test abi` with test native library directory | PASS 43/43 |
| `make test-policy` | PASS |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 804 vendor files / 3 workflows |
| `make check-ir-api-coverage` | PASS, 32 regressions |
| `make format-ada`; `make check-ada-format` | PASS, pinned GNATformat |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`; `git diff --check` | PASS |
| Current-source request type-link tests using pinned enhancer | PASS 12/12 (five new calibration/nested-type tests, seven existing request tests) |

## Limitations and hosted validation

Mock-provider-only qualification: no real IR provider, Squall, credentials,
containers, hardware or calibration execution qualification. Provider C++ ABI
compatibility remains necessary. Upstream has no callback unregister; exceptional
post-send/teardown paths intentionally retain resources rather than unload live
provider code. Collection/string counts larger than INT32_MAX are rejected.
CalibrationStatus, SubsystemCompletedCalibration, CalibrationStatusCmd and all
other task exclusions remain unimplemented. No historical documentation campaign,
five-stage qualification or local FULL_STRESS was run. Hosted Bootstrap owns full
native stress; exact-head Bootstrap/Documentation status is reported at delivery.
