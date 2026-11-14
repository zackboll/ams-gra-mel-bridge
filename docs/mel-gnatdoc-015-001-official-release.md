# MEL-GNATDOC-015-001 — official Enhancer v0.1.5

## Scope and acquisition

Documentation-only migration; no runtime API, Ada source, build ownership,
workflow permissions, triggers, branch protection or deployment policy changes.
The consumer retains its authored HTML landing page, seven guides, API Map,
example rendering and Shared/Core, IR and RF organization. No `authored-html`,
search, generic-instance or other new enhancer mode is enabled.

The refreshed baseline is main commit
`b22ddb64f5deaf9994fcd9478e56dbdbf7e06148`. An isolated, detached clean worktree
and the old `0.1.0.dev0` environment remain available outside the migration tree.
The previous archive revision and historical qualification remain recorded in
`doc-010-symbol-bookmarks.md`; reverting this migration restores that dependency.

The replacement is the official GitHub release wheel
`ada_gnatdoc_enhancer-0.1.5-py3-none-any.whl`, SHA-256
`d712d330f213fc1909a66c47bc2c7e5e15d5ec687222bea3c4922f5243db4247`.
The annotated v0.1.5 tag resolves to upstream commit
`0120bee2f2455e4c832d1d5ddae0a4cb03cb9477`.
Installation uses pip's required hash verification against the exact official
URL, without source builds, index dependencies, setuptools or wheel bootstrap.
Consumer verification checks distribution/CLI version, import origin, PEP 610
URL/digest and every installed non-bytecode implementation/asset file. The latter
fingerprint, independently calculated from wheel members, is
`bccaa00c7441cbc879eb635c63d86dc44fa5c87513300e2b9824be479cfa07c8`.

## Compatibility and independent comparison

Both environments render newly generated native output; no development
presentation manifest is reused. GNATdoc 26.0.0, public scope, trailing comments,
default `AMS.MEL` and the sole
`AMS.MEL.IR.Health_Status.Metadata.Status` renderer exception are unchanged.

Only two publication files change:

* `api/index.html`: the exact filter label changes from `Search documentation`
  to `Filter navigation links`. This is approved release presentation wording,
  not a new search feature. The consumer still removes only one exact,
  correctly placed control before comparing the entire pristine index.
* `api/presentation.json`: version `0.1.5` and the index output hash change.
  This is expected release serialization. Complete independent reconstruction
  remains mandatory; the manifest does not authorize its own contents.

All other publication bytes, native declarations/anchors, type destinations,
symbol metadata, stylesheets, selected symbol router, portal pages and example
listings are identical. The selected router retains SHA-256
`233cf43211a711d0d64dfba2815bc2d9cc887720faed0ca38f464bc6d29e145f`.
The unused default-router asset has upstream history/fragment visibility fixes;
its separately pinned historical projection changes accordingly, but MEL never
selects that router. No unexplained rendering difference has been accepted.

The fixed-byte legacy test utility is qualified separately against a fresh
GNATdoc export of DOC-012 commit `f5e5b89`, which exactly recovers its existing
pristine hash before enhancement. Its complete expected publication/projection
hashes are re-derived only after the same two-file comparison. These historical
counts are not requirements on today's Ada declarations. Normal builds and deep
qualification retain current-source inventories and complete occurrence audits.
Deep qualification now also explicitly checks enhancement idempotence.

## Evidence and validation

External evidence is retained at
`/home/zboll/git/mel-gnatdoc-015-evidence`; the detached baseline is
`/home/zboll/git/mel-gnatdoc-015-baseline` and the migration worktree is
`/home/zboll/git/mel-gnatdoc-015`.

Baseline commands run with the existing isolated enhancer interpreter and the
pinned GNAT 16.1.0 / GPRbuild 26.0.1 / GNATdoc 26.0.0 environment:

* `docs/tools/docs_ci.py build --directory .../baseline --gnatdoc-prefix ...`:
  passes; 4,967/4,991 documented elements, 113 diagnostic warnings.
* `docs/tools/docs_ci.py qualify --directory .../baseline-qualified --gnatdoc-prefix ...`:
  passes; two independent native generations and complete deep audits, 55 files.
* `make test-docs-ci`: passes on the unmodified baseline and migration tooling.
* Official wheel probe: enhancement, verification and second enhancement pass;
  independent consumer restoration and deep audit pass for 312 bookmarks and
  1,736 type-link occurrences derived from current source.

Additional completed validation:

* `make docs DOC_PYTHON=.../official-env/bin/python`: passes from clean committed
  source; the existing native/portal generation and sealed artifact verification
  succeed with the official release. No source-count baseline is introduced.
* `docs/tools/docs_ci.py qualify --directory .../release-qualified --gnatdoc-prefix ...`:
  passes, including separate deterministic generations, idempotence, 1,484 source
  declarations / 1,483 native sections and the single documented exception.
  The baseline and release deep records are equal for every native anchor,
  all 312 source-derived bookmarks and all 1,736 type occurrences.
* `make test-documentation-source DOC_PYTHON=.../official-env/bin/python`:
  all 10 tests pass, including actual source addition, independent regeneration,
  resealed corruption, removed/duplicated/wrong type links, old filter wording
  and forged release version. No rejection assertion is removed or weakened.
* Explicit `test_enhancer_release.py`: 5 tests pass, covering installed contents,
  CLI/distribution version, wrong digest/URL, missing origin and shadowed imports.
  These run in the existing non-deploying scheduled/manual qualification job;
  no new workflow, approval gate or required status is added.
* `make test-policy`: passes, including `make test-docs-ci` and existing artifact,
  source, privilege and workflow mutation controls.
* `test_compare_gnatdoc_warnings.py`: all 42 tests pass.
* `DOC_EXAMPLES_ALIRE_SETTINGS=... sh docs/tools/qualify_examples.sh`: passes
  formatting, compilation, production-library loader checks and all 13 semantic
  run cases across nine example programs with separate mock providers.
* Fresh Chromium matrix on both old and release publications: **640 checks in
  16 contexts** pass (1280/390 pixels × light/dark × JavaScript on/off × old/new).
  It checks every authored page, responsive layout, native fallback, default
  package, Shared/Core/IR/RF navigation, filter/clear, symbol destinations,
  history, example links and absence of browser script errors. Screenshots and
  the executable harness are retained with the external evidence.

Complete current-source SHA-256 fingerprints use sorted relative-path → file
SHA-256 JSON with compact separators (the existing consumer algorithm):

| Identity | SHA-256 |
| --- | --- |
| Pristine, both environments | `8cd94e2672392bf17273d811b0a34bc48bfe4fa006c9e779d8b154adaac9b304` |
| Old publication | `7451a80681b941c768cff2880213870629448dc1a81638611a9d0196b00962a1` |
| Release publication | `41617de5cc22c4e8b857bb1e24eaa1c08f5e655bf2a01fc7f2450d8a61cdeb01` |

Of 55 published files, 53 are byte-identical. `coverage.json`, `warnings.json`,
`native-anchors.json` and raw `gnatdoc.log` are also byte-identical. Coverage
remains 4,967/4,991 and warnings remain 113; these are observations, not frozen
requirements for evolving APIs. Exact final commit/tree identities, independent
hosted-artifact comparison and exact-HEAD hosted conclusions belong in the PR
handoff (embedding the final commit's own identity here is circular).

Qualification setup failures were corrected without changing production guards:
the first provenance mock needed an explicit string `version` property because
`importlib.metadata.version()` reads the mocked distribution; an initial browser
harness selected a nonexistent RF symbol and was corrected to a source-inventoried
one; a foreground historical verification exceeded the command-runner timeout
and was rerun in the background. The fresh historical generation recovered its
exact original pristine fingerprint before any hash changes were accepted.
External historical checks use their original source and tests, with the current
presentation verifier and its already-qualified performance-only symbol scanner.
This diagnostic replay is not a new CI governance requirement.

## Deployment boundaries and limitations

The existing main-push-only Pages job, exact same-run artifact selection,
bundle/manifest/source/run binding and post-publication byte verification remain
unchanged. A PR or manually dispatched qualification cannot deploy. This task
does not publish a site from its branch. A main deployment after an authorized
merge is therefore not a pre-merge claim. Browser qualification is bounded
Chromium behavior, not all-browser or full assistive-technology certification.