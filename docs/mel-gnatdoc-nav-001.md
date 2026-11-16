# MEL-GNATDOC-NAV-001 — official resizable Ada navigation

## Scope

Refreshed main baseline: `680ee5c7308869059a9c395e286bd5437e897634`.
Enable only the official v0.1.6 `navigation.resizable_sidebar` option, retaining
documentation search, symbol bookmarks, native navigation and authored HTML.
The release wheel, distribution/package digest checks and all runtime APIs are
unchanged. No custom production JavaScript or CSS is introduced.

## Independent verification

`resize_audit.py` owns exact approved shell bytes, their placement, separator
attributes and independently pinned official CSS/JS digests. It verifies output
manifest identities without allowing a resealed manifest to authorize content.
`source_epoch.py` maintains a complete file allowlist and reverses resize, search,
then symbol/navigation additions before restoring the entire native index.
The live graph permits the resize script only in the index with exact attributes
and runs the same asset/shell authorization. Other HTML/JS restrictions remain.

`test_resize_publication.py` exercises missing, duplicated, altered, misplaced,
reordered and forged shell elements and assets, including resealed output hashes.
Existing search tampering and authored-HTML adversarial campaigns remain intact.

## Compatibility and qualification

`resize_parity.py` compares complete baseline/current assembled publications,
requires the exact four-file delta (two official assets, index, presentation
manifest), identical pristine files, native identities and coverage, and renders
a disposable resize-disabled control from the immutable current Git export.
It requires that control and its complete Rust/Python assembly to match the
baseline byte-for-byte. Counts/hashes are derived diagnostics, not historical gates.

With the pinned GNAT/GPRbuild/GNATdoc environment and release-wheel interpreter:

```sh
python docs/tools/docs_ci.py build --directory .doc-tools/resize-current --gnatdoc-prefix "$GNATDOC_PREFIX"
python docs/tools/docs_ci.py qualify --directory .doc-tools/resize-qualified --gnatdoc-prefix "$GNATDOC_PREFIX"
python docs/tools/resize_parity.py --baseline "$BASELINE" --candidate .doc-tools/resize-current --output .doc-tools/resize-parity.json
MEL_RESIZE_QUALIFICATION="$PWD/.doc-tools/resize-qualified" python -W error -m unittest discover -s docs/tests -p test_resize_publication.py -v
MEL_SEARCH_QUALIFICATION="$PWD/.doc-tools/resize-qualified" python -W error -m unittest discover -s docs/tests -p test_search_publication.py -v
python docs/tools/qualify_authored_html.py --candidate .doc-tools/resize-qualified --output .doc-tools/resize-authored
make test-policy
make test-documentation-source
```

With Playwright 1.64.0/Chromium in a separate browser environment:

```sh
python docs/tools/resize_browser.py --site .doc-tools/resize-current/published --output .doc-tools/resize-browser.json
python docs/tools/search_browser.py --site .doc-tools/resize-current/published --output .doc-tools/search-browser.json
```

The dedicated resize browser campaign uses the actual complete MEL site. It
checks iframe-crossing drag/capture/cleanup, persistence/history/reload/reset,
keyboard/ARIA/bounds, cancellation, malformed/denied storage, path scoping,
responsive light/dark/mobile layouts and JavaScript-disabled native fallback.
It requires Navigation Report search/native viewport arrival, resize, back/forward,
reload and repeated search; Submit_Navigation_Report, overloads, bookmarks and
direct native URLs are also checked. The existing exhaustive declaration-search
browser campaign is not reduced.

The new dedicated resize workflow is read-only, exact-PR-head and non-deploying.
Existing main-only Pages deployment and live-verification permissions are untouched.
Both requested post-merge runs were in progress at baseline inspection, not passed.
At the later review checkpoint, both completed successfully at the baseline SHA:
Documentation `43787451638`, Bootstrap `43787451632`.
Actual local and exact-head hosted results are reported separately in the PR;
pending/unrun checks are blockers, not implied successes. No merge or deployment
is authorized by this task.

## Local results before final-head replay

The immutable baseline/current complete builds passed. Their inventories contain
2,575 / 2,577 files respectively (61 / 63 isolated Ada/portal, 2,320 Rustdoc,
194 Python). Exactly the four authorized paths differ; all other 2,573 files
are byte-identical. All nine authored outputs, native declaration pages, complete
source/native ledgers and coverage, full-search index, search CSS and composed
router are unchanged. The search index SHA-256 is
`9751a7bad7295d099bc7d621fd6f74fdc8f68c769dd0c99f1f0e8253ff23358b`.
The resize-disabled control matches all 61 isolated baseline bytes and, through
the actual language assembly functions, all 2,575 baseline publication bytes.

Two independent native generations, official renderer verification/idempotence,
complete consumer reconstruction, four resize mutation tests, three search
mutation tests, five official release-identity tests and the policy suites
passed. The 32 authored-HTML adversarial controls still reject their mutations.
Chromium 156.0.8078.4 passed the dedicated resize campaign and the existing search
campaign (1,505 native arrival observations, including every current public
symbol/operation). These observations are not permanent count gates.

Local evidence, complete inventories, search/native identities, authored hashes
and raw logs are retained at
`/home/zboll/git/mel-gnatdoc-nav-001-evidence`. Final-head replay and hosted
conclusions must be reported separately; these initial results do not authorize
merge or deployment.
