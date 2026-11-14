# PY-MEL-011 — Complete safe Python general ModeCmd

## Coverage delta and source authority

Base main: `c4490a9ce509512e0de6a79d42a61a12c00a7b9a`, tree
`2c562cd9b52b27865806f32c6b6811469a2582d1`. PR #185 is incorporated
through merge `6746686719deed3dc39e8668c13eb1b756060d50`. Preflight open
PRs #187, #184 and #183 had no overlapping Python ModeCmd work.

Previously Python exposed only fixed `submit_operate(command_id)` for this
overload. It now also exposes `ControlChannel.submit_mode(ModeCommand)` using
the existing native `ams_mel_ir_c2_submit_mode` and `ModeRequest`. No second
asynchronous implementation exists. Native/Ada structured inventories already
classify the complete operation; this Python-only change does not change their
evidence or coverage ceiling. Historical coverage is untouched.

Public additions: frozen `ModeCommand`, `ScanParameters`, `ScanType`, canonical
`CoordinateFrameReference` and `DegradationMethod`, and `ControlChannel.submit_mode`.
Existing `MFAState` and `MfaMode` are reused by identity. All new types are exported
as their defining objects with public `ams_mel` module identity for pickle.

Authority: pinned `ModeCmd.h`, `ScanParam.h`, `ScanType.h`, `CommonIR_MEL.h` under
`native/vendor/ir-mel/include/irmel/library/`; `native/include/ams_mel/abi.h`,
`native/src/ir_c2.cpp::convert_scan` and `ams_mel_ir_c2_submit_mode`;
`native/tests/test_ir_c2.c::full_mode_command`; and the established defaults in
`ada/src/ams-mel-ir-c2.ads::Scan_Parameters`. No native, provider, ctypes, Ada or
Rust source is changed.

## Consumer example

`control` below is an already enabled `ControlChannel`. This exact example is
executed against the permissive compiled mock by the focused test suite.

```python
from ams_mel import (
    MFAState,
    MfaMode,
    ModeCommand,
    ModeSuccess,
    ScanParameters,
)

command = ModeCommand(
    command_id=302,
    state=MFAState.STANDBY,
    mode=MfaMode.UNUSED,
    scan_parameters=ScanParameters(),
)

request = control.submit_mode(command)
try:
    result = request.wait(timeout_ms=5000)
finally:
    request.close()

if isinstance(result, ModeSuccess):
    print("Provider returned mode:", result.mode)
else:
    print("MEL rejected:", result.code, result.description)
```

Submission and a completed Mode future are **not proof of a physical MFA state
transition or hardware qualification**. A returned mode is the provider's
completed command result. Physical eligibility and execution remain provider
responsibilities; no state/mode compatibility restrictions are inferred.

## Complete payload mapping

Each Python field maps explicitly to its identically named field in
`IrModeCommandV1`, `IrScanParamV1` or `IrScanTypeV1`, without layout changes:

| Fields | Representation and defaults |
| --- | --- |
| `command_id` | uint32, required |
| `state`, `mode` | canonical `MFAState` 0–14, `MfaMode` 0–3; required |
| `elevation_defined_with_range_and_altitude` | actual bool, False |
| `center_az_rad`, `center_el_rad` | double, 0.0 |
| `center_frame_ref_el`, `center_frame_ref_az` | `CoordinateFrameReference`: INERTIAL=0, AIRCRAFT=1; defaults INERTIAL and AIRCRAFT respectively |
| `scan_width_rad`, `scan_height_rad` | double, 0.0 |
| `scan_type` | independent `ScanType()` default factory |
| `continuous_scan`, `returning`, `agile_scan` | nested uint32, 0; **not Boolean flags**; (1,2,3) remains (1,2,3) |
| `scan_id` | uint32, 0 |
| `scan_rate_rad_per_second` | double, 0.0 |
| `preferred_revisit_interval_seconds`, `required_revisit_interval_seconds` | double, 0.0 |
| `max_range_of_interest_m`, `min_range_of_interest_m` | uint32, 0 |
| `elevation_scan_center_altitude_m`, `elevation_scan_center_range_m` | uint32, 0; no altitude datum inferred |
| `degradation_method` | `DegradationMethod`: CAPACITY=0, VOLUME=1, RANGE=2, REVISIT=3; default CAPACITY |

`ModeCommand.scan_parameters` has an independent `ScanParameters` default factory.
Validation runs on construction and recursively again before native submission,
including after deliberate frozen-object mutation. Enum inputs must be members
of the canonical enum type (not raw ints or unrelated IntEnums). Unsigned
integers reject bool, negatives and overflow. Numeric double inputs exclude bool,
complex and text; values that overflow C double are local INVALID_ARGUMENT.
Finite numeric inputs receive normal double rounding; exact arbitrary-precision
round trips are not promised. Signed zero, infinities, NaNs and subnormals are
passed without normalization. NaN payload-bit identity is not promised.
No angle/rate clamping, positivity, physical feasibility or state/mode scan
compatibility validation is added. Defaults do not fabricate nonzero scan data.

## Ownership and errors

The typed C2 owner must be open. The native Enabled-only rule remains authoritative;
submission never enables C2 or registers metadata. Invalid local input makes zero
submit calls. Valid input produces one complete native record and exactly one
native admission attempt, with no retry.

Only successful admission with a non-null request owner returns `ModeRequest`.
Failure or malformed diagnostics after owner publication triggers at most one
non-raising release attempt; the original error survives cleanup failure. The
temporary Python observer is disarmed even if native ownership is uncertain.
The bridge never retries an uncertain owner or forces provider unload.

`ModeRequest`, `ModeSuccess`, `ModeRejected` and `submit_operate` are unchanged.
Zero wait polls; timeout neither consumes nor cancels; terminal results are
cached and repeatable. MEL rejection preserves complete code and description;
unknown return modes are errors. Admitted native work retains its graph after
C2/Session closure. Request close releases observation without cancellation.
Externally serialize C2 operations and wait/close on each request; the GIL is not
a substitute. No worker, event loop, retry system or Python parent reference is added.

## Qualification and boundaries

The focused suite uses compiled `mode-full` and `mode-general` fixtures, the
existing deterministic completion gates, and test-only native-boundary injection.
Hardware, real-provider support for every state/mode, physical transitions,
qualification, and scan feasibility are not established. Upstream introductory
elevation wording does not establish an altitude datum; no semantics are guessed.

Local environment: Python 3.13.5, GCC 14.2.0, Linux; separate compiled mock
provider, no hardware, credentials or containers. Commands ran sequentially,
without overlapping native rebuilds/relinks and test execution:

| Command | Actual result |
| --- | --- |
| `make test-native` | PASS, 555/555 |
| `make test-python` | PASS, 515 tests, warnings-as-errors; native prerequisite also 555/555 |
| `make test-policy` | PASS, 243 unittest cases across eight groups plus policy/script checks |
| `make check-ir-api-coverage` | PASS, reviewed evidence and 56 checker tests |
| `make test-build-isolation` | PASS, both build directions, 555/555 replay, artifact hashes and failpoint separation |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files, three workflows |
| `python3 scripts/check_final_newlines.py` | PASS after adding final newlines to the two new files |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff --check` | PASS |

Focused/regression command, with `PYTHONPATH` containing this worktree's `python`
and `python/tests`, `AMS_MEL_NATIVE_LIB` pointing to its test facade, and
`AMS_MEL_TEST_PROVIDER_DIR` pointing to its test providers:

```sh
python3 -W error -m unittest -v test_ir_general_mode test_control_channel \
  test_ir_config_set test_ir_c2_metadata test_import_identity test_package_layout test_abi
```

PASS: 170 tests, including 14 new general-mode tests, existing ModeRequest,
ConfigSet, metadata, public import/pickle identity and C11 ABI/layout coverage.
The new suite exercises all 60 published state/mode pairs. Full-field conversion
uses the unchanged compiled `mode-full` fixture; default Standby/Unused uses
`mode-general`. Boundary injection covers all unsigned fields, IEEE values,
non-normalization, malformed diagnostics, null successful owners, unexpected
failure owners, unknown status and uncertain cleanup. Deterministic completion
gates cover polling, timeout, admission exhaustion, early request closure,
parent-first closure, cached waits, future exceptions and actual provider unload.

An initial focused run had 13 passes and one incorrect test expectation: the
unchanged ModeRequest maps unknown returned modes to PROVIDER_FAILED rather than
PROTOCOL_INCONSISTENCY. The assertion was corrected to the existing behavior;
no request implementation was changed.

Baseline-negative evidence: an isolated `git archive` of the exact base's Python
package, bound to the same compiled native facade, confirms neither `ModeCommand`
nor `ControlChannel.submit_mode` exists. Running the new public-model and compiled
full-field tests against that baseline produces two expected AttributeErrors for
missing `ModeCommand` and `ScanParameters`, not an unrelated environment failure.

All pre-existing ControlChannel methods are AST-identical to base. Native sources,
exports map, C headers, private ctypes, `_requests.py`, Ada and Rust are byte-identical
to base. Production dynamic symbols are checked against the unchanged export map;
test failpoint symbols must be absent. Hosted CI and exact delivery identities
are recorded in the PR handoff, not asserted by this local qualification document.

Changed files are limited to this document, `python/README.md`,
`python/ams_mel/{__init__,_types,_validation,_c2}.py` and
`python/tests/test_ir_general_mode.py`.
