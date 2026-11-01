# IRMEL-ADA-002 — Image/Health NUC_TempData reception

Feature task. Specification: the merged IRMEL-ADA-001 audit
(`docs/irmel-ada-coverage.json`). Branch base `b4a550a`; synchronized by a
normal merge of `origin/main` `52285ce` (disjoint safe-Rust RF Admin change).

## Design

- **Opt-in owner, not a new mode.** `ams_mel_ir_image_nuc_open(stream, …)` and
  `ams_mel_ir_health_nuc_open(health, …)` each register exactly the pinned
  `registerMetadataCallback(std::function<void(Channel&,const NUC_TempData*const)>)`
  overload on their own channel family, with their own one-shot. Existing
  `ams_mel_ir_image_metadata_open` / `ams_mel_ir_health_metadata_open` never
  register NUC and are byte-for-byte unchanged in behavior.
- **Shared private implementation** `native/src/ir_nuc.cpp` /
  `internal/ir_nuc.hpp`: copy, bounded DROP-INCOMING queue, counters,
  receive/close/view. Callbacks capture only a `shared_ptr<NucState>`.
- **ABI:** new records `ams_mel_ir_pixel_value_v1` (`kind` 1/2/3 + `value`),
  `ams_mel_ir_temp_point_v1`, `ams_mel_ir_temp_point_span_v1`,
  `ams_mel_ir_nuc_temp_data_event_v1`; seven new exports (229 total). No
  existing `*_v1` record changes (static-asserted in `test_ir_nuc.c`).
- **Ada:** public `AMS.MEL.IR.NUC` (owned values, `Pixel_Width`-discriminated
  `Pixel_Value`, `Not_Supported` exception); limited `NUC_Stream` owners with
  `Open_NUC`/`Receive_NUC`/`NUC_Counters`/`Close` in both existing metadata
  packages; private `AMS.MEL.IR.NUC_Conversion`. Array stride uses
  `'Object_Size` (C tail padding), checked by `AMS.MEL.IR.NUC.Testing`.
- **Lifecycle:** Image NUC owner stops with logical stop and physical cleanup of
  the stream; Health NUC owner is deactivated on Close and stopped after channel
  destruction once in-flight NUC callbacks drain (`drain_and_stop`), or failed on
  uncertain detach.

## tempError limitation

Pinned `TempPoint` has private `std::optional<double> tempError`, settable only
via the constructor, with **no public getter**. It is not represented, not read
from private storage, not inferred and not reported as present or absent.
`TempPoint` is therefore classified `partial`; `NUC_TempData`, `PixelValue`,
`TempPointType` and both registrations are `safe_ada`.

## Focused qualification (local, 2026-10-31)

| Command | Result |
| --- | --- |
| `sh native/scripts/build.sh` (production, `-Werror`) | PASS |
| `sh native/scripts/test.sh` (ordinary native suite, once) | PASS 435/435, incl. `ir_nuc_contract`, `ir_image_metadata_contract`, `ir_health_contract` |
| `gprbuild -P ada/tests/ams_mel_tests.gpr ams_mel_smoke.adb` (GNAT 16.1.0) | PASS, warnings as errors |
| `ada/tests/bin/ams_mel_smoke` | PASS (rc 0), incl. Image/Health NUC tests |
| `make format-ada`, `make check-ada-format` | PASS |
| `make check-ir-api-coverage` | PASS, 24/24 controls; safe_ada 32→34, missing 34→32 |
| Python `test_abi.py` (raw ctypes mirror, header probe) | PASS 48/48 |
| `cargo test -p ams-mel-sys --test abi` | PASS 42/42 |
| `nm -D` production `libams_mel_c.so.0` | 229 `ams_mel_` exports, 7 NUC |

Not run (per DOC-CI-005 focused qualification): historical/current
documentation suites, GNATdoc regeneration, five-stage campaign, 50-repeat
FULL_STRESS. Hosted Bootstrap remains responsible for GCC/Clang Debug/Release,
Ada, Rust, Python and isolation. No physical IR camera or Squall was used;
all positive evidence is deterministic mock-provider evidence.

## Reviewed source-hash mapping

All 58 previous `source_sha256` entries were first verified equal to their
`origin/main` blobs. Eleven changed files were rehashed from their final bytes
and five new implementation files added (63 entries). Only six classification
entries changed (two operations, four types); `inherited`/`image_capabilities`
are unchanged. `REVIEW_SHA256` in `scripts/check_ir_api_coverage.py` is the
SHA-256 of the sorted-key reviewed JSON; `CATALOG_SHA256`, upstream pins, the
catalog and frozen Git evidence are unchanged.

## POL-002 / POL-001 source-integrity result (not bypassed)

Executed unchanged trusted-main tools against merged candidate `4cceeaf`:

```text
docs/tools/feature_source.py --root .   -> exit 1
ValueError: source qualification: incomplete/modified audit member: Makefile
scripts/check_doc_source_integrity.py  -> exit 1
ValueError: IRMEL-POL-001 unapproved complete bytes: scripts/check_ir_api_coverage.py
```

Control on unmodified `origin/main` `52285ce`: `feature_source.py` exit 0
(documentation-only); `check_doc_source_integrity.py` **already exits 1**
(`unauthorized tracked files added: ['rust/ams-mel/src/rf_admin.rs',
'rust/ams-mel/tests/rf_admin.rs']`), i.e. it fails on main independently of
this PR.

Cause: both policies freeze the nine-member IRMEL audit unit at its PR #129
bytes and admit only documentation or RF ProductRx surfaces, so any legitimate
IR binding PR that updates the audit records is rejected. Neither policy file
is modified here. Documentation CI runs only on `main` (DOC-CI-005), so this
blocks only post-merge documentation qualification, not PR Bootstrap.

Narrow post-merge authorization needed (separate policy task, independent
review): admit this exact merged commit's delta as a reviewed IR binding
transition — the 39 implementation/audit paths (plus this record) listed by `git diff --name-status
52285ce 4cceeaf`, including ten new source files, and new exact identities for
`scripts/check_ir_api_coverage.py`, `scripts/test_ir_api_coverage.py`,
`docs/irmel-ada-coverage.json` and `docs/reference/irmel-reviewed-bindings.json`
(other five audit members unchanged) — keyed to the immutable merge commit and
tree, plus the pre-existing RUST-MEL-001 additions on main.
