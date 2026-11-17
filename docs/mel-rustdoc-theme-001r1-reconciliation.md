# MEL-RUSTDOC-THEME-001R1 — reconciliation qualification

## Scope and source identities

Reconcile existing PR #221 on `docs/mel-rustdoc-theme-001` using an ordinary
merge of main `3e462a6d5448918813a81b45653f9340a66b7c3d`, tree
`a8efcacf5a763867a149eb1a507caf75d4ff164c`. PR #223 is merged at that commit.
The preserved original theme HEAD is `c852dd2be429cd3f32420ca8ea26f0ca2e87a3c0`,
tree `d17f325c326add93d149d1cf555bbbc396657fb4`. The merge required no conflict
resolution. No theme palette, runtime source, production workflow policy, ABI,
API or coverage contract is changed by this reconciliation.

The existing `mel-rustdoc-theme-001.md` records the original visual research,
approved stylesheet identities and implementation boundaries. Those identities
and all existing comparison, security, link and browser assertions are retained.
Coverage delta is regression requalification only, not additional API coverage.

## Reproduction and evidence

Use the pinned GNAT 16.1.0 / GPRbuild 26.0.0 / GNATdoc 26.0.0 environment,
qualified enhancer 0.1.5 interpreter, and Rustdoc 1.98.1. Follow
`tests/README-qualification.md` for the independently authenticated historical
checkout and its separate pinned historical enhancer. The complete suites are:

```sh
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests -p source_epoch_regression_tests.py -v
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests -p test_live_docs_site.py -v
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests -p test_documentation_ci.py -v
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests -p rustdoc_theme_regression_tests.py -v
make test-policy
make test-docs-ci
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

Full current-source `docs_ci.py build`, `qualify`, sealed artifact unpack and
independent verification, independent themed/unthemed complete inventory
comparisons, both browser campaigns and exhaustive local HTTP entity comparison
are required in addition to these suites. The unthemed comparison uses the
integrated main source, not an obsolete crate inventory. Native, Ada, Rust,
Python and build-isolation repository qualification is also required.

Actual command logs, exit codes, source/native destination identities, complete
publication comparisons, browser screenshots and exact-HEAD hosted job results
are retained separately at
`/home/zboll/git/mel-rustdoc-theme-001r1-evidence/` and summarized in the existing
PR #221 description. Final commit/tree and actual results belong there rather
than a self-referential source identity. Earlier task and PR #223 evidence is
preserved separately and is not substituted for final-HEAD results.

## Limitations and review boundary

No missing, skipped or pending mandatory test counts as passing. Chromium
qualification is bounded usability/color/contrast evidence, not comprehensive
accessibility or cross-browser certification. Local HTTP uses the existing
explicit loopback fixture interface without relaxing production HTTPS policy.
No deployment, publication, approval, merge, replacement PR or auto-merge is
authorized. Readiness requires successful local qualification and inspected
required hosted jobs on the identical local/remote/PR HEAD, with deployment and
live deployment verification skipped.

## R2 — current-main and three-language reconciliation

Ordinary merge of `86eee19b252da563246d87072f4d71f69a377e30` into the
existing PR #221 branch. Preflight HEAD was `8948cac569514539c8a3ffdb5a40c0dc93abd44c`
(7 ahead / 67 behind), with a clean worktree. Textual conflicts were limited to
`docs/tools/docs_ci.py` and `docs/tools/rustdoc_publication.py`; the workflow and
Rust publication tests merged automatically and were reviewed semantically.
All native, ABI, provider and language-binding sources remain identical to main.

Generation retains the current Ada → themed Rust → Ada+Rust → Python → complete
assembly order, canonical records and complete artifact seal. Verification first
authenticates and reverses Python, verifies Rust against that Ada+Rust projection,
then independently regenerates every Rustdoc byte from the authenticated Git
export. Python reference files, source/run checks, payload hashes, main-only
deployment restrictions and HTTP security policies remain mandatory.

The current qualified enhancer is 0.1.6, revision
`8661855bfc245c3389e2ef30a423c100776ff525`. Its Ada theme still hashes to
`cf06637732a8b047a7191bfb991a5c3da7cd31a52076876880d5f8a3f63929bc`.
The approved Rust CSS, Git blob, palette, toolchain and invocation are unchanged.
Common, IR, RF, sys and compatibility façade documentation retain main's crate
inventory and re-exports. Browser checks now follow extracted IR documentation
and source ownership, retaining all previous functional/color assertions and
adding computed colors for each extracted crate.

The PR-triggered resize workflow previously assumed every candidate introduced
the original four-file resize delta. Its parity check now also handles a baseline
with resize already enabled: qualified Ada/Python bytes must be identical,
Rust bytes must be identical after removing only the hash-qualified theme and
its stylesheet links, and an independently rendered resize-disabled control
must still differ in exactly the original four files. Unrelated Rust CSS/JS,
forged themes and duplicate inclusions are covered by negative tests. The
original resize-introduction path and source/native identity checks remain.

### Actual local evidence and final-head gate

Logs, exit codes, sealed bundles, hashes and browser screenshots are retained at
`/home/zboll/git/mel-rustdoc-theme-001r2-evidence/`. Commands include the R1 suite
above, `make native`, `make test-native`, pinned Cargo workspace test/check/Clippy
(`--locked`, Clippy `-D warnings`), Rustfmt, warnings-denied offline Rustdoc,
`make check`, `make test-python`, and current authored/search/resize qualification.
Final results and exact commit/tree/hosted merge identities are recorded in PR
#221 after completion; an earlier successful source identity is not substituted
for final-head qualification.

At report preparation: production build and **821/821 native tests** passed;
Rust workspace **616 passed / 15 intentionally ignored / 0 failed**, including
the corrected CommsTest native-control parent from PR #229. No renewed lifetime
investigation or stress campaign was needed. Cargo check, Clippy, Rustfmt and
warnings-denied Rustdoc passed. Clippy/Rustfmt initially lacked pinned components;
those were installed before successful execution, not bypassed. Policy passed;
current CI tests **50**, pipeline tests **18**, Rust publication **17**, Python
publication **10**, HTTP **58 parent + 22 historical child**, search **3**, and
resize **4** passed. Aggregate/source-epoch/final-head checks were still running
when this report was prepared; only subsequent actual results count as success.

Complete build and sealed unpack/reverification passed. Same-current-revision
themed/unthemed Rustdoc has **2,320 → 2,321 files**: exactly `theme.css` added,
**1,549 HTML files** changed only by one approved stylesheet link, **771 existing
files byte-identical** (including native JS, search, fonts and other assets).
The **2,584-file** complete publication includes **63 Ada**, **2,321 Rust** and
**200 Python** files; Ada and Python are byte-identical to integrated main and
the same-source unthemed assembly. Deep repeated Ada/source reconstruction and
resize parity passed. Chromium 156.0.8078.4 passed the corrected theme campaign
at 1440×1000 and 390×844, light/dark, including computed colors, contrast,
preferences, native search/navigation, sidebar behavior and native Ayu isolation.
An initial browser failure identified obsolete pre-extraction test destinations;
the assertions were retained and the destinations corrected. An authored test
also correctly rejected evidence from a superseded local HEAD; final evidence
must be regenerated, not relabeled.

No deployment/live-site publication is claimed. Hosted Bootstrap (including all
four mandatory FULL_STRESS matrices), API Documentation, authored HTML, search
and resize must pass on the final HEAD, and independent technical review must
cover that same HEAD. PR #221 remains open, non-draft, unmerged, without
auto-merge. The repository currently exposes no required status contexts or
independent collaborators besides its author; this does not waive the requested
qualification or independent-review gates. No self-approval is permitted.
