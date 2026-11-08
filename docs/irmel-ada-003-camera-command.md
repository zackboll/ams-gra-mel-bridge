# IRMEL-ADA-003 — C2/Image CameraCommand and Image CameraCommandResp

Feature task. Branched from `origin/main` `e84ab3830034f4e2f09ae736901e6ea2526dd1c2`
(the supplied starting SHA). Before push, main advanced to `888553c` (PY-MEL-002
Python Instrumentation and RUST-IR-001 safe Rust HealthStatus, disjoint
Python/Rust files); it was merged normally (no rebase). After the merge, Python
204/204 and the full Rust workspace pass.

## R4 — final integration and focused local regression

Starting PR #150 head: `4e5cb535d3a4ba3bf2986050076ebb22ae2d783d`, tree
`7cea631c6f38ada70b9fb30d918379aeb3f70315`. Main
`1ab36aa488d9c12273efa5f338ef1f2dcbbe323d` is incorporated by a normal merge.
The only conflict was the independent additions at the top of `docs/coverage.md`;
both the CameraCommand/R3 and Rust NUC sections are preserved. CMake and the
Python common-channel test merged automatically, retaining the Health public-owner
test probe, all seven camera CTests, the modularized Python patch target and the
239-function ABI expectation.

CameraCommand production code, the session-specific admission correction, the
Ada private extension/non-raising finalizer and private ABI mirrors are unchanged
from the starting head. Main's Python modularization, Rust Health metadata and
NUC implementations are retained. The CI-002R3 source-authority verifier,
workflow and policy tests are byte-identical to main: an older event base must
be an ancestor of the actual base, the actual base must be reachable from trusted
main, the synthetic commit must have exactly two ordered parents with PR head
second, and its tree must match reconstructed merge output. Production changes
continue to require `FULL_STRESS`; no policy exceptions are introduced.

| Focused local check | Result |
|---|---|
| `make test-policy` | PASS, seven suites / 234 tests, including 75 Bootstrap policy tests |
| `python3 scripts/check_doc_source_integrity.py` | PASS |
| `make check-ir-api-coverage` | PASS, 26 tests; 37 safe Ada operations |
| `make check-ada-format`, Rust workspace format check, Git whitespace checks | PASS |
| Pinned enhancer Camera_Request type-link regression | PASS 3/3, including rejection of the old subtype link |
| Native camera, Health, NUC, Image metadata and navigation CTests | PASS 11/11, including all seven camera tests |
| Alire GPRbuild and Ada smoke against test facade/mock providers | PASS, including camera, navigation and NUC |
| Rust `ams-mel-sys` ABI tests | PASS 42/42 |
| Rust Health metadata / NUC integration tests | PASS 17 / 16; one subprocess helper ignored in each suite |
| Rust library unit tests | PASS 64/64 |
| Python tests with Makefile library/provider environment, plus package compile check | PASS 271/271 |
| Production build and dynamic symbol inspection | PASS, 239 `AMS_MEL_0.1` exports, zero `ams_mel_test_*` symbols |

Python's first full run lacked the unbuilt RF missing-symbol provider fixture;
building the existing target fixed the environment and the full rerun passed.
No local native 442 × 50 stress, GNATdoc regeneration, historical documentation
campaign or five-stage qualification was performed. Exact-head hosted Bootstrap
(including mandatory native stress) and Documentation remain the final gates;
their run identities and status are recorded in the PR handoff, not assumed here.
All runtime evidence is mock-provider only, not IR hardware or Squall evidence.

## R3 — documentation private-subtype link correction

Starting PR #150 head: `59d76ec842cdfe44696b7cdc4f9dbf21ca423096`, tree
`39c6b7e3894ce441262165826c3e8553533818b1`; its second parent is the R2 merged
main `1d0ab0caa745077e2a7e79f3490344b584105741`. During R3 inspection, live main
advanced to `c85aee316cc745168ece12d63eda434a3b0baac0` (Rust Health metadata;
no changes to the relevant Ada or documentation code).

Pinned current-source generation reproduces Documentation run `41345552072`:
`AMS.MEL.IR.Camera.Camera_Request` is rendered as an ordinary subtype whose base
links to `type Camera_Request_Owner is limited private;` in `AMS.MEL.IR`.
The containing native page/section is
`3d4e52a331f7fd13f06b72e4cf48e4adb29d8af65047a1134b2e2e456c2ae586.html#98eed098456b82b24600c4e3016b250504f9de135bf3a630f07dacea0cfeddb1`;
the rejected destination is
`fcc2aff5e67a49dbd4266c4d14c896260883e1cde02a978e467f1747f5236656.html#3b07f2aa98f84714b8c25aafee1ff515cfa800c40effd20db5b41ed401272988`.
Both declarations originate in this PR, not unmodified main.

The correction makes `Camera.Camera_Request` a private extension of the existing
limited-private controlled parent owner, using the audit's existing derived-type
context. The type remains limited, inherits the same non-raising finalizer and
hides its handle representation. The public request operations, submission
profiles are unchanged; private bodies use parent view conversions to access the
same handle without copies. No native lifecycle, audit/security checks or
policy exceptions change. The request is now a distinct type, not an alias of
the implementation owner; intended clients use `Camera.Camera_Request` as
documented.

The parent's partial view explicitly says `tagged limited private`, matching its
existing controlled full view and allowing visible derivation in the child.
Its handle and controlled completion remain private; no C representation leaks.

`docs/tests/test_camera_request_links.py` checks the real source's derived-type
link, the parent's limited-private declaration, the five public operation type
links, and continued rejection of the original ordinary subtype's private target.
It runs with the pinned enhancer interpreter as a targeted regression, alongside
the unchanged current-source Documentation build and its independent audit.

R3 local validation (provider-free, mock evidence only):

| Check | Result |
|---|---|
| Pinned `docs_ci.py build`, fresh current-source fixture | PASS; native restoration, six request-related links and all destinations verified |
| Pinned enhancer `unittest discover -s docs/tests -p test_camera_request_links.py -v` | PASS 3/3; original unsafe subtype link remains rejected |
| Alire `gprbuild -p -P ada/tests/ams_mel_tests.gpr ams_mel_smoke.adb`, GNAT 16.1.0, warnings as errors | PASS |
| `ams_mel_smoke` against test facade and mock providers | PASS, including CameraCommand ownership, repeated Wait, timeout, Close and finalization contracts |
| Native `ctest -R '^ir_camera_command_' --output-on-failure` | PASS 7/7, C11 contract and isolated failure processes |
| `make check-ir-api-coverage` | PASS, 26 regression tests |
| `make test-policy` | PASS, all seven suites |
| `check_doc_source_integrity.py`, `make format-ada`, `make check-ada-format`, final newlines, `git diff --check` | PASS |

No historical documentation campaign or FULL_STRESS was rerun. No provider,
hardware or Squall qualification is claimed. Earlier exploratory representation
forms were rejected by compilation or documentation checks and are not retained.

## Implemented upstream operations

1. `C2Channel::send(CameraCommand)` → `ams_mel_ir_c2_submit_camera_command`,
   Ada `AMS.MEL.IR.C2.Submit_Camera_Command` (Enabled only).
2. `ImageChannel::send(CameraCommand)` → `ams_mel_ir_stream_submit_camera_command`,
   Ada `AMS.MEL.IR.Image.Submit_Camera_Command` (Attached/Running).
3. `ImageChannel::registerMetadataCallback(CameraCommandResp)` →
   `ams_mel_ir_image_camera_response_open` + receive/counters/close/view/event-close,
   Ada `AMS.MEL.IR.Image.Metadata.Open_Camera_Response` (opt-in).

Both sends return the shared `ams_mel_ir_camera_command_request`
(`..._request_wait/close`), Ada `AMS.MEL.IR.Camera.Camera_Request`.

## Representation

| CameraCommand | C (`ams_mel_ir_camera_command_v1`) | Ada (`Camera.Camera_Command`) |
|---|---|---|
| commandID | `uint32_t command_id` | `ID : Command_ID` (mod 2**32) |
| systemTime | `int64_t system_time_ns` | `System_Time_NS : Nanoseconds` |
| imageType | `uint32_t image_type` (0/1 only) | `Kind : Image_Type (Staring, Scanning)` |
| width / height | `uint32_t` | `Unsigned_32` |
| frameRate | `uint32_t frame_rate_hz` | `Frame_Rate_Hz : Unsigned_32` |
| integrationTime | `int64_t integration_time_ns` | `Integration_Time_NS : Nanoseconds` |
| rowOffset / columnOffset | `uint32_t` | `Unsigned_32` |
| direction | `uint32_t direction` (`AMS_MEL_IR_SCAN_DIR_*`) | `Direction : Scan_Direction` |
| numberOfStages | `uint32_t` | `Unsigned_32` |

`Reserved13` (C value 2) and unknown image types/directions are rejected with
`AMS_MEL_INVALID_ARGUMENT` before any provider call; the Ada type cannot
represent them. No physical limits or timestamp normalization are applied.

CameraCommandResp (`ams_mel_ir_camera_command_response_v1`, Ada
`Camera_Command_Response`): `uint32 command_id`, `uint32 size`,
`int64 timestamp_ns`, `command_success` 0/1 → Boolean (any other native value
raises Provider_Error in Ada). `CommandSuccess = false` is `AMS_MEL_OK` /
`Status = Success`; a MEL error is `AMS_MEL_COMMAND_REJECTED` / `Rejected`.
Null successful response, unknown MEL code and deferred-cleanup failure are
`PROVIDER_FAILED`; a throwing future is `PROVIDER_EXCEPTION`.

## Lifecycle design

One engine (`native/src/ir_camera_command.cpp`) reuses the existing pieces
rather than a new framework: `CompletionPermit` Session admission, a family
claim adapter that validates lifecycle, copies the typed channel and accounts
one request under the family lock (C2: `claim_c2_submission(Enabled)` +
`finish_c2_request`; Image: `claim_navigation_submission` +
`finish_image_request`), an erased `CommonRequestClaim`, unlocked provider
send, one detached completion worker as the only `future::get()` caller,
allocation-free process-lifetime retention after post-send allocation or
worker-launch failure, and a new probe family `Camera` (6). The opt-in Image
owner gets its own one-shot slot on `ImageStreamState` and is stopped by both
logical stop and physical cleanup, like the IRMEL-ADA-002 NUC owner.

## ABI

Three new records, `ams_mel_ir_scan_dir_t`, three opaque handles, ten new
exports: 229 → **239** `AMS_MEL_0.1` exports (verified with `nm -D` on the
production library). No existing `*_v1` layout changed; `test_ir_camera_command.c`
static-asserts the image metadata, navigation/mode results, counters and
IRMEL-ADA-002 NUC layouts beside the new ones. Python (`_native.py`) and Rust
(`ams-mel-sys`) received private raw mirrors and header-probe layouts only.

## Focused qualification (local)

| Command | Result |
|---|---|
| `sh native/scripts/build.sh` (production, `-Werror`) | PASS; 239 exports, 0 test symbols |
| native test tree `ctest` (once) | PASS 442/442 (7 new camera tests) |
| `test_ir_camera_command` (C11) | PASS; plus isolated post-send allocation/worker-launch and finish-failure processes for C2 and Image |
| `gprbuild ams_mel_tests.gpr ams_mel_smoke.adb` (GNAT 16.1.0, warnings as errors) | PASS |
| `ada/tests/bin/ams_mel_smoke` | PASS (rc 0), incl. new camera contract and unchanged NUC/navigation suites |
| Python `test_abi` / `test_common_channel` | PASS 48/48, 45/45 |
| `cargo test -p ams-mel-sys --test abi` | PASS 42/42 |
| `make check-ir-api-coverage` | PASS, 26 tests; safe_ada operations 34 → 37 |
| `make test-policy`, `check_doc_source_integrity.py`, `make check-ada-format`, `git diff --check` | PASS |

Not run (DOC-CI-005 focused qualification): GNATdoc regeneration, historical
documentation suites, five-stage campaigns, FULL_STRESS. Hosted Bootstrap
remains responsible for its nine jobs. Positive evidence is mock-provider only;
no IR camera or Squall was used.

## Coverage audit

Three operations and `CameraCommand`, `CameraCommandResp` and `ScanDir` moved
to `safe_ada` with all 11 / 4 fields `represented`. Pinned commit/tree
identities, frozen Git evidence, the catalog and `CATALOG_SHA256` are
unchanged; `REVIEW_SHA256` is the SHA-256 of the sorted-key reviewed JSON
(`2c92c848…71d5`). Changed implementation files were rehashed in
`source_sha256` and six new files added (69 entries) as the reviewed baseline
record only, under the PY-POL-001 ordinary-development policy.

## Limitations and remaining gaps

- No C2 CameraCommandResp callback exists upstream; none is fabricated.
- No public Python or safe Rust CameraCommand API (raw mirrors only).
- Remaining IR gaps include CameraProtectCmd, calibration requests, EraseCommand,
  C2 SystemTrackDataResponse, OpticalDistortionMap/LineOfSightQuaternion,
  Scheduling, LOS3D and StackedImage (see `docs/irmel-ada-coverage.md`).
