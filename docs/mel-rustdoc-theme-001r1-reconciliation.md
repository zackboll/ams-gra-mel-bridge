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