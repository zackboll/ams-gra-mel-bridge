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

Final qualification, browser, example and exact-HEAD hosted results are recorded
below when available. This evidence is diagnostic, not deployment authority.

## Deployment boundaries and limitations

The existing main-push-only Pages job, exact same-run artifact selection,
bundle/manifest/source/run binding and post-publication byte verification remain
unchanged. A PR or manually dispatched qualification cannot deploy. This task
does not publish a site from its branch. A main deployment after an authorized
merge is therefore not a pre-merge claim. Browser qualification is bounded
Chromium behavior, not all-browser or full assistive-technology certification.