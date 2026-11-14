# PY-MEL-012 — complete safe Python NavigationReport submission

## Scope and source identity

Prepared from freshly fetched main `b22ddb64f5deaf9994fcd9478e56dbdbf7e06148`,
tree `fa7ea1a8daf60a9c518a43e78f60f99d2614da7a`, in a dedicated clean worktree.
PR #190's final-newline repair is an ancestor. Python ModeCmd and ConfigSet
are present. At preflight the only open PR was unrelated Ada #184; there was
no overlapping Python NavigationReport PR. Merged Rust #186 is a comparison
reference, not implementation scope.

Coverage delta: Python now represents and submits every field of published
`ImageChannel::send(NavigationReport)` and owns its cached asynchronous result.
Existing C, Ada and Rust coverage classifications are unchanged. The IR audit
inventories in `docs/reference/irmel-api-schema.md` classify native/safe Ada,
not Python. No upstream catalog or reviewed binding ceiling changes are needed.
No native sources, C ABI, ctypes declarations, mock provider, Ada/Rust sources,
production exports, CI/policy or historical coverage archive are changed.

Authoritative mapping:

- `native/vendor/common-mel/include/mel/library/NavigationReport.h`:
  PositionSolutionState, AttitudeRate, PositionVelocityCovariance, NavigationReport.
- `native/vendor/common-mel/include/mel/library/Directional.h`: Euler, NorthEastDown.
- `native/vendor/ir-mel/include/irmel/library/image/ImageChannel.h`: send overload.
- `native/vendor/ir-mel/include/irmel/library/image/NavigationReportResp.h`: response.
- Exact upstream commit/tree/blob evidence is preserved in
  `docs/reference/irmel-upstream-git-evidence.json` and `irmel-upstream-api.json`.
  Common MEL commit `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
  `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`; IR MEL commit
  `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
  `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`.
- `native/include/ams_mel/abi.h`: NavigationReport v1 input/result/owner contracts.
- `native/src/ir_navigation.cpp`: `convert_report`, provider send, native-only
  worker, cached terminal outcomes, deferred cleanup and lifetime accounting.
- `native/tests/test_ir_navigation.c` and `native/tests/mock_provider.cpp`:
  existing compiled field-getter fidelity fixtures and deterministic barriers.
- `python/ams_mel/_native.py`: existing ctypes layouts/signatures reused unchanged.

## Public API and complete ABI field map

All added value classes are frozen Python-owned dataclasses. Public class
identity and value pickling use `ams_mel`; `Euler` and `NavigationResponse`
are the existing canonical classes, not aliases to newly defined substitutes.

`PositionSolutionState(IntEnum)` is exactly NOT_SET=0, ALIGNING=1,
FREE_INERTIAL=2, GPS=3, BLENDED=4. MaxExclusive=5 is excluded. A report requires
a canonical enum member, not a bare integer, Boolean or another enum. States
are not an ordered quality scale. NOT_SET is accepted, without assigning
physical meaning to it or to zero-valued placeholders. No default report factory
is added: every report field must be explicitly supplied.

`NavigationReport` maps one-to-one into existing `NavigationReportV1`:

| Python / ABI field | Pinned upstream getter / representation |
| --- | --- |
| system_time_ns | getSystemTime().count(), signed int64 nanoseconds |
| state | getState(), published uint32 enum domain 0..4 |
| latitude_rad | getLatitude(), double radians |
| longitude_rad | getLongitude(), double radians |
| altitude_m | getAltitude(), double meters |
| attitude | getAttitude(), canonical Euler roll/pitch/yaw → EulerV1 |
| attitude_rate | getAttitudeRate() → AttitudeRateV1 |
| speed | getSpeed(), NorthEastDown → NorthEastDownV1, meters/sec |
| acceleration | getAcceleration(), same NorthEastDown, meters/sec squared |
| wander_angle_rad | getWanderAngle(), double radians |
| magnetic_heading | getMagneticHeading(), double, no inferred units |
| altitude_msl | getAltitudeMSL(), double, upstream comment “barometric altitude”; no datum conversion |
| position_velocity_covariance_uncertainty | getPositionVelocityCovarianceUncertainty() → PositionVelocityCovarianceV1 |

`NorthEastDown(north, east, down)` preserves the three doubles individually.
`AttitudeRate(attitude_rate: Euler, attitude_rate_time_ns: int)` maps to
getAttitudeRate() and getAttitudeRateTime().count(). The signed nanosecond
timestamp is independent of system_time_ns; neither has an inferred epoch.
Angular-rate units are not specified by the pinned declaration.

`PositionVelocityCovariance` maps all 18 independent doubles in published order:

| Python / ABI field | Upstream getter |
| --- | --- |
| position_position_pn_pn | getPositionPositionPnPn |
| position_position_pn_pe | getPositionPositionPnPe |
| position_position_pn_pd | getPositionPositionPnPd |
| position_position_pe_pe | getPositionPositionPePe |
| position_position_pe_pd | getPositionPositionPePd |
| position_position_pd_pd | getPositionPositionPdPd |
| position_velocity_pn_vn | getPositionVelocityPnVn |
| position_velocity_pn_ve | getPositionVelocityPnVe |
| position_velocity_pn_vd | getPositionVelocityPnVd |
| position_velocity_pe_ve | getPositionVelocityPeVe |
| position_velocity_pe_vd | getPositionVelocityPeVd |
| position_velocity_pd_vd | getPositionVelocityPdVd |
| velocity_velocity_vn_vn | getVelocityVelocityVnVn |
| velocity_velocity_vn_ve | getVelocityVelocityVnVe |
| velocity_velocity_vn_vd | getVelocityVelocityVnVd |
| velocity_velocity_ve_ve | getVelocityVelocityVeVe |
| velocity_velocity_ve_vd | getVelocityVelocityVeVd |
| velocity_velocity_vd_vd | getVelocityVelocityVdVd |

No covariance units, missing symmetric terms, conditioning or positive
definiteness requirement are inferred. No rotation, normalization, clamping,
altitude interpretation, coordinate transformation or quality improvement occurs.

Inputs are validated at construction and again throughout the complete nested
graph before native submission, including deliberately mutated frozen objects.
Timestamps support the entire signed int64 range. Numeric Booleans are rejected;
existing C-double validation rejects overflow/unrepresentable inputs while
preserving signed zero, subnormals, nonfinite values and supported finite extrema.
NaN payload-bit preservation across provider conversions is not promised.

## Submission, result and ownership contracts

`ImageStream.submit_navigation_report(report) -> NavigationRequest` validates
locally, requires a live owner, then submits exactly once using the existing
`ams_mel_ir_stream_submit_navigation_report`. Attached and Running are eligible;
Stopped, Failed and Closed are not. It neither starts acquisition nor registers
metadata. Native admission/resource limits and synchronous send failures remain
distinct structured errors. Success without a request owner is a protocol error.
An unexpectedly published owner on failure receives one non-raising release
attempt; cleanup never replaces the original error or retries uncertain ownership.

`NavigationRequest` is a unique noncopyable/nonpickleable owner with `is_open`,
`wait(timeout_ms)`, `close()` and context management. Wait timeout is a uint32
millisecond interval; zero polls. Timeout raises structured `MelError(TIMEOUT)`
without cancellation or consumption. Terminal Wait is repeatable indefinitely.

- `NavigationCompleted(response: NavigationResponse)` contains signed int64
  system_time_ns and full uint32 command_id/request_id. Success requires NONE
  error code and an empty diagnostic.
- Explicit COMMAND_REJECTED returns
  `NavigationRejected(code: MelErrorCode, description: str)`. Unknown uint32
  MEL codes preserve the existing `UNKNOWN_0xXXXXXXXX` convention. The existing
  native bridge itself maps unknown upstream codes to PROVIDER_FAILED.
- Provider exceptions, null successful responses, admission exhaustion and timeout
  remain errors, not MEL rejections. Inactive response bytes are never interpreted.

Long terminal text uses the native cached result: one further Wait(0) on the
same request, never a resubmission. Status and required length must match;
only success response/error fields or rejection error code are compared.
Failure response bytes are inactive. Invalid size, changed active result,
malformed NUL termination and invalid UTF-8 are explicit errors. A nonterminal
timeout is never retried merely to retrieve text.

Close releases the public request, not pending provider work, and is idempotent
after release. An uncertain failed release is disarmed rather than retried;
this can leak if the native boundary is broken, but prevents double release.
Finalization is non-raising and attempts release no more than once. Context-body
exceptions take precedence over cleanup exceptions.

The request retains neither Python ImageStream nor Python Session. Native
completion may retain the provider graph independently after either parent
closes. Logical Image Close/Stop defers physical teardown until admitted work
finishes. Native cleanup destroys provider channels/callbacks before unload;
ownership-uncertain teardown is retained rather than unsafely unloaded. Close
does not promise callback quiescence or completion. Operations on each request,
including Wait versus Close, require external serialization. Image submission
must likewise be externally serialized with other operations on that Image.
The GIL is not a native ownership lock. No Python thread, worker, retry queue,
new transport, second completion mechanism or cancellation API is introduced.

## Metadata is independent

Ordinary `ImageMetadataKind.NAVIGATION_RESPONSE` receives NavigationReportResp
independently of request completion. A provider may invoke this callback
synchronously inside send. The native bridge already avoids holding metadata
and frame mutexes across send. Metadata receipt does not complete a request;
a terminal future does not synthesize metadata. Both carry the same canonical
response payload, and correlation identifiers must be checked independently.

## Executable mock-only usage

The caller supplies an already constructed, fully specified `mock_test_report`
and an open `image: ImageStream`. The regression executes this exact example
with the compiled `navigation-fidelity` fixture. It is test data, not a real INS
solution. Real applications must supply their own externally sourced report.

```python
from ams_mel import NavigationCompleted, NavigationRejected

with image.submit_navigation_report(mock_test_report) as request:
    outcome = request.wait(5000)
    if isinstance(outcome, NavigationCompleted):
        print("Mock completion:", outcome.response)
    elif isinstance(outcome, NavigationRejected):
        print("Mock rejection:", outcome.code, outcome.description)
```

Forwarding these values through MEL does not implement sensor-side INS transfer
alignment. Provider acceptance establishes neither navigation accuracy nor that
hardware consumed the data. No physical INS accuracy, transfer-alignment
performance, real-provider compatibility or real IR interoperability is claimed.

## Qualification

Implementation commit `1763995e880df70f933115724bc8e586d6258e48`, tree
`f0351d62140a81072da2fb710ae1ef12d4536651`, was qualified from a clean worktree.
All following commands ran sequentially, with no native rebuild overlapping
tests using those artifacts:

| Command | Actual local result |
| --- | --- |
| `make test-native` | PASS, 557/557 native CTest tests |
| `make test-python` | PASS, 537 Python tests, warnings-as-errors; prerequisite 557/557 native tests |
| `make test-policy` | PASS, 244 tests across eight suites (18+18+28+65+75+18+9+13) |
| `make check-ir-api-coverage` | PASS, offline inventory check and 59 regression tests |
| `make test-build-isolation` | PASS, both build orders, 557/557 retained tests, production/test artifacts and failpoints isolated |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files and 3 workflows |
| `python3 scripts/check_final_newlines.py` | PASS, all first-party text files |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS |
| `git diff --check` | PASS |

Additional focused command, using the test-tree environment below:

```sh
export PYTHONPATH="$PWD/python:$PWD/python/tests"
export AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
python3 -W error -m unittest test_ir_navigation test_image_stream \
  test_ir_image_metadata test_ir_general_mode test_ir_config_set \
  test_control_channel test_import_identity test_package_layout test_abi -v
```

PASS: 208 tests, including 21 new NavigationReport tests, existing Image,
metadata, C2 ModeCmd/ConfigSet, import/pickle compatibility and C11 ABI/layout
regressions. The focused pending-lifetime subprocess covers both parent-close
orders, explicit request Close, request finalization, completion after parent
collection, metadata arrival before completion, exact correlation IDs, deferred
teardown and eventual unload, using provider barriers/native reclamation probes.
Resource exhaustion and failed deferred detach are also deterministic controls.
The test provider verifies all rich input fields via actual C++ getters,
including distinct covariance values 1.5 through 18.5. Numeric/hostile-boundary
tests inspect all 36 input doubles and both timestamps before forwarding sends.

Base negative controls imported the archived base Python package against the
same native library: all eight new exports and the Image submission method
were absent, while submit_mode and submit_config_set were present.
`nm -D --defined-only native/build/lib/libams_mel_c.so.0.1.0` contained exactly
the same 305 production exports as the base export map. The ABI header, export
map and ctypes file were byte-compared against base Git blobs; all unchanged.
The entire native, Ada, Rust, workflow/policy and historical coverage paths
have no diff from base. Main was re-fetched and remained at the recorded base.

This evidence update changes documentation only. The delivery process reruns
the complete sequence on the final committed documentation-inclusive HEAD;
that exact HEAD/tree, repeated results and hosted CI state are reported in the
PR without a self-referential commit hash in this file.

Qualification is provider-free compiled mock testing plus private hostile ABI
injection, not hardware evidence. No real provider was tested. The local PATH
has no gprbuild (`command -v gprbuild` returned no path); full local `make check`
and Ada tests are not claimed. Exact-HEAD hosted Ada qualification is separate
evidence, not a substitute local result. Use HOSTED_CI_PENDING until required
hosted checks finish; independent technical review is required before merge.
