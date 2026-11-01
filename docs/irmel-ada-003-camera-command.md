# IRMEL-ADA-003 — C2/Image CameraCommand and Image CameraCommandResp

Feature task. Base: `origin/main` `e84ab3830034f4e2f09ae736901e6ea2526dd1c2`
(unchanged from the supplied starting SHA at fetch time).

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
