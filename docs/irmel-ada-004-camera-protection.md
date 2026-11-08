# IRMEL-ADA-004 — C2 camera self-protection (CameraProtectCmd / CameraProtectCmdResp)

Feature task. Branched from `origin/main` `5f7ec45dbe4709ee18171047b766a61edb133404`
(the supplied starting SHA). Before push, main advanced to
`0bd87efabda62d0afb085722be179a1d1ad03c62` (IR-CAMERA-CI-001, a test-only CameraCommand
owner-quiescence fix); it was merged normally (no rebase), and the same quiescence
was applied to the analogous camera-protection owner-count assertions.

## Upstream mapping

| Pinned upstream (`@RequiredIfCameraProtect`) | Native C ABI | Safe Ada |
|---|---|---|
| `C2Channel::send(CameraProtectCmd)` | `ams_mel_ir_c2_submit_camera_protect`, `ams_mel_ir_camera_protect_request_wait/close` | `C2.Submit_Camera_Protection`, `Camera_Protection.Protection_Request` (`Is_Open`, `Wait`, `Close`) |
| `C2Channel::registerMetadataCallback(CameraProtectCmdResp)` | `ams_mel_ir_c2_camera_protect_open`, `ams_mel_ir_camera_protect_receive/get_counters/close/event_view/event_close` | `C2.Metadata.Open_Camera_Protection`, `Receive_Camera_Protection`, `Camera_Protection_Counters`, `Close` |
| `CameraProtectCmd` (commandID, responseTime, protectFlag) | `ams_mel_ir_camera_protect_command_v1` (`uint32`, `int64` ns, `uint32` 0/1) | `Protection_Command` (`Command_ID`, `Nanoseconds`, `Boolean`) |
| `CameraProtectCmdResp` (commandID, responseTime, commandResponse) | `ams_mel_ir_camera_protect_response_v1` | `Protection_Response` |
| `CmdResp { OK, Unavailable, Unsupported }` | `AMS_MEL_IR_CAMERA_PROTECT_OK/UNAVAILABLE/UNSUPPORTED` (0/1/2) | `Protection_Status` (`Protection_OK`, `Protection_Unavailable`, `Protection_Unsupported`) |

## Distinct outcomes

| Outcome | C ABI | Ada |
|---|---|---|
| `CmdResp::OK` / `Unavailable` / `Unsupported` response | `AMS_MEL_OK`, exact `command_response` | `Status = Success`, exact `Protection_Status` |
| Asynchronous MEL rejection (including MEL `Unsupported`) | `AMS_MEL_COMMAND_REJECTED` + `error_code` + description | `Status = Rejected`, `Rejection_Code`, `Description` |
| Unknown `CmdResp` / null response / unknown MEL code / deferred-cleanup failure | `AMS_MEL_PROVIDER_FAILED` | `Provider_Error` |
| Future or send exception | `AMS_MEL_PROVIDER_EXCEPTION` | `Provider_Error` |
| Registration `Return::NotSupported` | `PROVIDER_FAILED` + `out_registration = NOT_SUPPORTED` | `Camera_Protection.Not_Supported` |
| Registration `Return::Fail` | `PROVIDER_FAILED` + `out_registration = FAIL` | `Provider_Error` |
| Registration exception | `PROVIDER_EXCEPTION`, no registration value | `Provider_Error` |

`protect_flag` values other than 0 and 1 are rejected with `AMS_MEL_INVALID_ARGUMENT`
before any provider call. Unknown `CmdResp` values are never remapped: the request
reports `PROVIDER_FAILED` and the callback counts them as malformed. No deadline,
camera behaviour or automatic retry is introduced.

## Lifecycle design

The request path reuses the established architecture without a new framework:
`CompletionPermit` Session admission; a C2 family claim that runs
`claim_c2_submission(Enabled)` and publishes a `CommonRequestClaim` with
`finish_c2_request` under the C2 lock; an unlocked provider send; a single detached
completion worker, which is the only `future::get()` caller; deferred physical C2
teardown when Close precedes completion; and allocation-free process-lifetime
retention after a post-send allocation or worker-launch failure. A pending request
retains the channel, Session and provider library after request, channel and
Session closure. Probe family `CameraProtect` (7) is appended; existing indices are
unchanged.

The opt-in C2 CameraProtectCmdResp owner follows the IRMEL-ADA-002 NUC pattern. Its
one-shot attempt flag and state are published under the C2 lock, and registration
runs with no bridge lock held, so synchronous delivery cannot deadlock. Callbacks
count themselves in flight under the state mutex. C2 physical cleanup deactivates
the owner before disable/detach, marks it Failed if detach fails, and, after the
provider channel is released, waits for in-flight callbacks before marking it
Stopped. No upstream unregister is assumed. The base C2 `Metadata.Open` is
unchanged and never registers this overload.

The Ada request is `type Protection_Request is new
AMS.MEL.IR.Camera_Protection_Request_Owner with private`. It derives from an opaque
tagged limited controlled owner with non-raising finalization, avoiding the
IRMEL-ADA-003 ordinary-subtype documentation defect.

## ABI

Three new records, `ams_mel_ir_camera_protect_status_t`, three opaque handles and
nine new exports take the production inventory from 239 to **248** `AMS_MEL_0.1`
exports, with zero `ams_mel_test_*` symbols (verified with `nm -D` against
`exports.map`). No existing `*_v1` layout, export, enum value or calling convention
changed. `test_ir_camera_protect.c` static-asserts the new layouts (24/24/32 bytes)
and the pre-existing CameraCommand, NUC, counters, mode and navigation layouts.
Python (`_native.py`) and Rust (`ams-mel-sys`) received private raw mirrors and
header-probe layouts only; public safe Python/Rust APIs are reserved for later tasks.

## Focused qualification (local)

| Command | Result |
|---|---|
| `sh native/scripts/build.sh` (production, `-Werror`) | PASS; 248 exports, 0 test symbols |
| native test tree `ctest` (once, before and after merging main `0bd87ef`) | PASS 447/447 (5 new camera-protection tests) |
| Camera-protection CTests `--repeat until-fail:100 -j 8`; camera command + protection `until-fail:20` | PASS |
| `test_ir_camera_protect` (C11) + isolated post-send allocation/worker-launch, finish-failure and callback-in-flight processes | PASS |
| Mutation negative control (`Unavailable` collapsed into `OK`) | Detected (FAIL), then restored |
| GPRbuild `ams_mel_tests.gpr` (GNAT 16.1.0, warnings as errors) and `ams_mel_smoke` | PASS, including the new camera-protection suite and unchanged CameraCommand, C2, C2 metadata and C2 common suites |
| `make check-ir-api-coverage` | PASS, 29 tests; safe_ada operations 37 → 39 |
| Camera / CameraProtection type-link regressions (pinned enhancer) | PASS 7/7, including rejection of a private-subtype link |
| Python `test_abi` + `test_common_channel` / full Python suite (test library + providers) | PASS 93/93 / 271/271 |
| Rust `cargo test -p ams-mel-sys --test abi` | PASS 42/42 (248-export inventory) |
| `make test-policy`, `check_doc_source_integrity.py`, `make check-ada-format`, `cargo fmt --check`, `git diff --check` | PASS |

Not run (as directed): GNATdoc regeneration, historical documentation
suites, five-stage campaigns and local FULL_STRESS. Hosted Bootstrap remains
responsible for full native stress. All positive evidence is mock-provider only;
no IR camera or Squall was used.

## Coverage audit

Two operations (`C2Channel::send(CameraProtectCmd)` and the C2
`CameraProtectCmdResp` registration) and three types (`CameraProtectCmd`,
`CameraProtectCmdResp`, `CmdResp`) moved from `missing` to `safe_ada`, with all three
fields of each record `represented`. Upstream pins, the declaration catalog,
`CATALOG_SHA256` and the frozen Git evidence are unchanged. `REVIEW_SHA256` is the
SHA-256 of the sorted-key reviewed JSON. Changed implementation files were rehashed
in `source_sha256` and six new files added (75 entries).

## Limitations

- Mock-qualified only; no physical camera self-protection or Squall evidence.
- `@RequiredIfCameraProtect` is an upstream declaration, not a provider promise.
  Providers without it reject the send with MEL Unsupported and answer the
  registration with NotSupported.
- Public safe Python and Rust APIs are not part of this task.
