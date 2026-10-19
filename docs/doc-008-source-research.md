# DOC-008 source research and scope

## Baseline and coordination

Origin is `git@github.com:zackboll/ams-gra-mel-bridge.git`. Initial fetch selected
merged main `a5484514cfae3b7bccde078118b838fa40ce4d0e`, identical to the task's
preparation reference: there is no initial reference-to-main delta. The original
checkout was clean. No existing DOC-008/examples branch or PR was found.
Work is isolated in `/home/zboll/git/ams-mel-doc-008`, branch
`docs/008-compilable-ada-examples`; other worktrees remain untouched.

Read AGENTS, all `.clinerules`, architecture, ABI policy, coverage, GNATdoc
foundation, current guides and DOC-007A–E qualification before implementation.
PR #119 (DOC-007F) was open at initial inspection, head
`6fe6da0c65afd2dcba0d303f6ac89cc4acacf3f8`. It adopts a newer enhancer and
metadata/type-link behavior. DOC-008 does not adopt that work or change the
enhancer installation, assets, configuration, type resolver or navigation.
Potential overlapping files are README, `docs/tools/build_enhanced_site.py`,
`docs/tests/test_audit_output.py`, `docs/tests/test_enhanced_site.py` and
`docs/tests/test_navigation.py`. DOC-008 changes their authored-site metrics and
preservation checks, not enhancer semantics. If #119 merges, a normal main merge
and new inherited API/presentation comparison baseline are required.

## Public workflows and existing mock authority

All paths below are relative to the checkout root. The new canonical sources
are under `docs/examples/src`. No production/API/test/provider changes are needed.

| Example | Public contract | Existing source/fixture authority |
|---|---|---|
| Provider Session | `ams-mel.ads`: owned version survives Close | `ams_mel_smoke.adb`, `ada-explicit`; exact versions/vendor/description in `native/tests/mock_provider.cpp` |
| Owned Image | `ams-mel-ir.ads`, `ams-mel-ir-image.ads`: complete Receive, Mono8 | `ams_mel_ir_image_tests.adb`, `full-frame-rich`: 4×3, rich header, 160..171 |
| Frame lease | Image Acquire/With/Copy/Close | `ams_mel_ir_image_lease_tests.adb`, `ada-lease-success`: 16..27; no address probes/barriers copied |
| Common channel | C2 As_Channel; Channel capabilities/KeepAlive/Wait | `ams_mel_ir_channel_tests.adb`, `keepalive-delayed`; completion may precede poll |
| Navigation | Image complete Navigation_Report and outcome-guarded access | `ams_mel_ir_image_navigation_tests.adb`, `navigation-fidelity`, `navigation-reject`; all 18 covariance terms retained in named helper |
| Track metadata | Track.Metadata Receive_Event discriminant, capacity | `ams_mel_ir_track_metadata_tests.adb`, `track-metadata-mixed`; request/report/request delivered at registration |
| RF discovery/Admin | RF snapshot/index versus ID; independent Admin owner | `ams_mel_rf_tests.adb`, `ams_mel_rf_admin_tests.adb`; Data `success`, Admin `admin:ok`/`admin:reject` |
| RF RX Job | C2 Submit/Wait/Claim, finite intervals, Finalize/status | `ams_mel_rf_job_tests.adb`, `native/tests/mock_rf_provider.cpp`, `c2:job-ok`; exact required VA/Job constructors copied without private imports |
| RF ProductRx | Submit/Wait/Claim, Event metadata/borrow/copy/statistics | `ams_mel_rf_product_rx_tests.adb`, native mock `rx:ok:sync`: (77,-77),(-1,1); `rx:error-known`: Insufficient_Resources |

Ada reference filenames in the final column are in `ada/tests/src`; public
contracts are in `ada/src`. The clearly named `Mock_Fixtures` package exposes
complete navigation and RF constructors only. Resource acquisition, outcomes,
borrows and explicit Close remain visible in the mains. Standard controlled
owners provide non-raising cleanup fallback on unexpected exceptions.

## Semantics deliberately not invented

- Full_Frame header completeness does not add image format negotiation. The
  current image path here is validated single-band Mono8, with copied payload.
- A pixel lease avoids an additional Ada bulk copy; no end-to-end zero-copy claim.
- Common View is weak; local Is_Open does not prove its target lives. Timeout
  and request Close are not cancellation. Delayed polling accepts either race.
- Navigation is supplied by the application, not estimated or transformed.
  Explicit radian/metre/nanosecond names are used without assigning an epoch or
  units to upstream-unspecified fields.
- Mixed Track metadata is provider-originated; legacy report-only Receive can
  consume a non-report and raise. Queue bounds/drop-incoming are not losslessness.
- RF factories are independent. Admin acceptance and successful configuration or
  Finalize do not establish physical transitions/activity. A single Finalize
  future has a cached result, not a live progress stream; Close is not Finalize.
- ProductRx counts I/Q pairs. Callback data is already copied natively.
  Registration can synchronously deliver before Claim returns; With_Samples is
  synchronous and scoped; Copy_Samples is independent. Array bounds may differ.
- RX Job's claimed details ID `0x10203040` is not ProductRx's `0xDEADBEEF`.

## Documentation and execution authority

The manifest inventories nine mains and thirteen cases, required support,
families/configurations, expected observations, guide anchors and public
packages. A strict docs-owned validator reconciles it with the GPR main list,
actual source inventory, guide placeholders/anchors/package references, README
commands and executed mains. Static complete listings are HTML-escaped from
canonical sources and independently decoded for byte-equivalent text comparison.
No source-download assets or broad publication allowance are introduced.

The runner uses the existing production build script and separate mock-provider
build script, then GPR imports the real Ada library. It strips inherited loader
and bridge test settings, inspects `ldd`, checks production identity and absence
of test exports, and inspects each actual child's glibc `calling init` evidence.
It enforces bounded waits, outer process timeouts, actual exits and semantic
observations. GNATdoc generation remains semantic-only and does not execute it.
The existing documentation workflow adds an explicit example qualification step
before publication, retaining actual PR-head checkout and main-only Pages.

Before edits, complete pristine/enhanced baseline sites were freshly generated
and independently verified outside the repository. DOC-008 explicitly repins
the changed complete pristine site and authored-page counts while additionally
pinning both inherited complete `api/` inventories/bytes. Source/profile/anchor,
warning/coverage, renderer exception, enhancer installation/assets, independent
type-link audit and package navigation protections remain in force.
