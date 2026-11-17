# DOC-RESIZE-CI-001 — same-source resize qualification

## Baseline, root cause and scope

Base main commit: `51ede458587698cc1f8e4601d3471852d6f2a9e5`.
Base tree: `c2eada141d0b86de5acb39d6e58b275539d1d1ea`.
Dedicated branch: `fix/doc-resize-same-source-parity`.

No existing open corrective PR or matching remote branch was found at preflight.
PR #233 is unchanged by this task. No runtime, ABI, binding, vendored source,
public API baseline, branch protection or unrelated workflow is modified.

Failed hosted run: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44018141078>.
Downloaded diagnostics reproduce the exact original four-file assertion failure:
main `86eee19b252da563246d87072f4d71f69a377e30` generated 197 Python HTML
pages; feature `991c3a85d21688e3caf43052b440fa60681da37c` generated 202.
The 12 differing files are all legitimate Python reference files (five added
API pages plus indexes, module/source listings and search records). Resize was
already enabled in both revisions. Comparing these different sources cannot
measure the effect of a resize toggle. Current main's Rust-palette projection
does not correct this source-model error either.

## Corrected authority and invariants

The parity entry point requires the exact candidate manifest digest, performs
`docs_ci.verify`, and exports that verified commit, not a floating main ref.
Verification retains exact checkout/event identity, source/native reconciliation,
native anchors, coverage, Ada restoration, authored destinations, search checks,
complete assembly, source-derived Python reconstruction and independent Rust
regeneration. Only then may the already verified candidate language bytes be
reused in the disabled control.

Exactly one expected `resizable_sidebar = true` is changed to `false` in a
disposable export. The existing renderer/consumer verifier re-renders Ada from
the verified current native artifacts. Real Rust and Python assembly functions
produce the complete disabled site. Both Ada-only and complete-site comparisons
require precisely:

```text
api/gnatdoc-resize.css
api/gnatdoc-resize.js
api/index.html
api/presentation.json
```

All other file bytes are equal, including every Rust/Python byte, authored page,
native/search destination and asset. Only the CSS/JS files may be added, and no
file may be deleted. There are no Rust theme exceptions or ignored subtrees.
The control records `modified-export-not-deployable` and a `derived_from`
identity plus original/modified configuration hashes; it never fabricates an
immutable commit for the changed export. Source and candidate payload identities
are rechecked after rendering. Disabled files are temporary and cannot deploy.

## Regression coverage

The explicit pinned-toolchain suite uses actual complete publications and real
isolated fixture commits. It checks current-source parity; a Python-only public
class producing additional HTML (old cross-revision rejection, new parity pass);
and a subsequent documented Ada subtype and Rust type (old comparison rejection,
each revision independently qualified). Page counts are measured, not globally
hardcoded. Existing source-epoch regression tests remain unchanged.

Adversarial tests cover unauthorized non-resize files, missing delta files,
subset deltas, extra Rust/Python output, changed HTML/search/resize JS/CSS,
forged presentation data, wrong source identity, Python records, resealed Rust
output, invalid native anchors, altered separator markup, broken destinations
and incomplete language assembly. Resealing the outer manifest does not grant
authority. Existing resize markup/asset, search corruption and enhancer-release
negative tests remain required.

The hosted workflow builds exact PR HEAD, retains deep source reconstruction,
runs the same-source gate and new fixture suite, retains existing resize/release
tests and full Chromium resize/search/accessibility/security behavior. Pinned
tools, triggers, fail-closed exits and non-authorizing diagnostic retention remain.
No mainline publication is used as a resize baseline.

## Qualification ledger

Implementation qualified at commit `81b0c7892b8bd698ee07f6c7e63b166707d8dded`,
tree `269186e40259ec672b42ddb7d25e85423834510e`. Final source identity and exact-HEAD
hosted conclusions are recorded in the corrective PR ledger; this file cannot
contain its own final commit hash. Evidence and serial scripts/logs are retained
in `/tmp/doc-resize-ci-001-evidence/`.

Narrow additional test change: `docs/tests/test_rustdoc_publication.py` previously
called the removed palette projection. It now requires parity to reject all
Rust changes (including a qualified palette), rather than preserving an exception
that is invalid in a same-source comparison. Other Rust publication tests remain.

### Actual local commands/results

Pinned environment: Alire 2.1.1, GNAT 16.1.0, GPRbuild 26.0.0 (26.0.1 crate),
GNATdoc 26.0.0 (20260415), official enhancer 0.1.6 with verified wheel provenance,
Rustdoc 1.98.1, Playwright 1.64.0 and Chromium 156.0.8078.4. Build-sensitive
commands run serially under `alr -n -s "$PWD/.doc-tools/settings" -C docs/toolchain
exec -- ...`. `GNATDOC_PREFIX` selects `.doc-tools/gnatdoc-26.0.0`; the enhancer
venv interpreter runs documentation consumers. No native library is loaded by
the Python reference generator; Rust documentation is locked/offline.

- `test-docs-ci-rerun`: **112 tests passed**, exit 0.
- `test-policy`: **316 tests passed**, exit 0.
- `parity-tests`: **4 tests passed**, exit 0.
- `resize-tests`: **4 tests passed**, exit 0.
- `release-tests`: **5 tests passed**, exit 0.
- `search-tests`: **3 tests passed**, exit 0.
- `authored-tests`: **5 tests passed**, exit 0.
- `source-regressions-disk`: **11 tests passed**, exit 0.
- `docs_ci.py build`, explicit `docs_ci.py verify --manifest-sha <same-run digest>`,
  `docs_ci.py qualify`, `resize_parity.py --candidate ... --manifest-sha ...`,
  and `qualify_authored_html.py`: exit 0. Deep qualification reconstructs native
  docs twice; all 32 authored negative cases reject.
- Complete enabled publication: 2,584 files; disabled: 2,582; exactly four
  authorized differences and 2,580 byte-identical unchanged files. Rust records
  1,651 HTML + 670 assets; Python records 197 HTML + 3 assets. Counts are observations,
  not completeness thresholds. Current search has 1,538 source-derived records.
- Real isolated Python feature: 197 → 198 HTML pages; old cross-revision rejection
  and new same-source acceptance both observed. Subsequent Ada/Rust source change
  adds the measured subtype anchor and a Rust type page (1,652 Rust HTML pages).
  Both fixture commits pass complete verification and the exact four-file gate.
- `resize_browser.py` and `search_browser.py`: exit 0, no runtime errors or external
  requests. Resize includes desktop/mobile, light/dark, storage/JS-denied fallbacks,
  ARIA/keyboard/pointer/capture/iframe handling and mandatory search destinations.
- `make check`: exit 0, including build isolation,
  821 native contract tests, 70 structured IR coverage controls, pinned Ada
  format check, Ada execution and
  source/newline/whitespace checks.
- `python3 scripts/check_doc_source_integrity.py`,
  `python3 scripts/check_final_newlines.py`, `git diff --check`, actionlint 1.7.12,
  exact workflow shell syntax: exit 0. Optimized-Python Rust publication tests:
  17/17 passed; no disabled-assertion validation.

### Failures retained, limitations and disposition

Initial fast discovery failed because the retired palette projection's test was
still calling its removed function. The stricter test replacement passed the
full fast rerun. Missing final newlines were corrected before the implementation
commit. Initial source-epoch setup ran zero tests and failed with `Errno 28`:
`/tmp` exhausted inodes, not publication integrity. The identical suite passed
using workspace-backed `TMPDIR`; no assertions or source contracts were changed.
Downloaded historical artifacts omit `published/rust/.lock` in both sites due
to upload-artifact's hidden-file default. Every retained byte matches its seal;
those downloads are diagnostic reproduction, not full publication authority.
Fresh local publications include and verify the hidden-file inventory.

Native/provider/binding product sources have zero diff against base. Standalone
Rust/Python runtime suites, real providers/hardware, packaging and deployment
are intentionally outside this documentation-only correction. `make check`
still exercises native and Ada; hosted Bootstrap independently qualifies language
jobs. No provider/network execution is introduced into ordinary builds.

Independent technical review is explicitly requested in the corrective PR. Only
the owner account was listed as a collaborator, so an independent reviewer cannot
currently be assigned. No self-approval, merge or auto-merge is authorized.
Readiness requires exact-final-HEAD hosted qualification, not historical passes
or skipped deploy/live jobs. PR #233 remains unchanged. After independent review
and authorized mainline merge, it may incorporate corrected main with an ordinary
merge and requalify all required checks before independent review/user merge.
