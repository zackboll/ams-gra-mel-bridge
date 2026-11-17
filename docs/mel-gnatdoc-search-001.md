# MEL-GNATDOC-SEARCH-001 — official documentation search

## Scope and authority

Refreshed baseline: commit `43f8acc8843a907dd91150961bc94023ec23c414`, tree
`e0af603fb1243843aa174a61d49571530f725509`.

Enable only `[search] schema_version = 1, mode = "documentation"`. The existing
release-wheel pin remains GNATdoc Enhancer 0.1.6, upstream commit
`8661855bfc245c3389e2ef30a423c100776ff525`, wheel SHA-256
`adfec71188a868e3fb70a6da71a124c937f9e63e26a2f2e31af45364f49ffb27`.
There is no alternative search implementation, sidebar resizing, source API
change, authored-content rewrite, theme change or deployment permission change.

The consumer still reconciles current public source profiles with observed
native sections. `search_audit.py` independently requires exactly those package,
type/subtype and operation identities/destinations plus the approved guides.
The released renderer reconstructs searchable text. Its exact bytes, official
stylesheet/router composition, separately reconstructed shell allowlist and
digest are verified before removing the precisely placed search insertions and
restoring the entire native index. Neither `presentation.json` nor resealed
output hashes can authorize an additional, omitted or forged search record.

## Baseline and publication delta

The reconstructed baseline has 59 Ada/portal files and 2,573 assembled
Ada/Rust/Python files. Chromium 156.0.8078.4 confirms the label **Filter navigation
links**, 63 visible navigation links initially, zero visible links after
`Navigation Report`, and no documentation search index.

The search publication has 61 Ada/portal files. Exactly these files differ:

| File | Classification |
| --- | --- |
| `api/gnatdoc-search-index.json` | New source/native/rendered-guide-backed index |
| `api/gnatdoc-search.css` | New official release stylesheet |
| `api/gnatdoc-navigation.js` | Official search-enabled symbol-router composition |
| `api/index.html` | Search controls, results container and independent destination authority |
| `api/presentation.json` | Corresponding output inventory and SHA-256 identities |

All native declaration pages and all nine authored portal assets are
byte-identical to the baseline. The baseline source/native ledger contains
1,532 rendered declarations. Search includes 45 packages, 431 public symbols,
1,055 operations and seven guides: **1,538 records**. The distinction is
intentional: record components, constants and unsupported kinds are not search
declarations. No private or nested declaration gains enclosing-package identity.

Every current guide fits the release's 64,000-character visible-text bound:
API Map 43,763; Architecture 4,154; Examples 51,307; Getting Started 5,136;
IR Workflows 10,766; Ownership and Errors 7,352; RF Workflows 6,430.
No truncation or limit override is introduced.

## Required queries and native arrival

| Query | Actual match count | Required public records |
| --- | ---: | --- |
| `Navigation Report` | 23 | Navigation_Report (symbol), Submit_Navigation_Report (operation) |
| `Navigation_Report` | 7 | Both declarations |
| `Submit_Navigation_Report` | 4 | Submit_Navigation_Report (operation) |
| `AMS.MEL.IR.Image.Navigation_Report` | 2 | Navigation_Report (symbol) |

Additional matches are legitimate signature, descriptive-text and guide hits;
the official ranking/tokenization is unchanged. Required results must be
clicked and must arrive at the native heading in the iframe viewport, including
reload, a fresh direct link and back/forward history.

Both declarations are on native page
`1227ecb7e9dafa3f2bb5afc47ffd547c851f2fc8202385479135994de7b16763.html`:

* Navigation_Report: `8dd14b74126f84d582213f5a58833c155e983b70f86e61d544e786d176cfaf85`.
* Submit_Navigation_Report: `bd1fd74c6a68bbe3671ff467059ee9a679efa482b1f384c80ed3c623c28e2a30`.

## Reproducible gates

Use the repository's pinned GNAT/GPRbuild/GNATdoc environment and the interpreter
installed by `docs/tools/install_enhancer.sh`. All output directories must be
fresh, and the source checkout must remain at the exact committed HEAD for the
entire build; changing HEAD during verification correctly rejects its identity.

```sh
python docs/tools/docs_ci.py build --gnatdoc-prefix "$GNATDOC_PREFIX"
python docs/tools/docs_ci.py qualify --directory .doc-tools/search-qualified \
  --gnatdoc-prefix "$GNATDOC_PREFIX"
python docs/tools/qualify_authored_html.py --candidate .doc-tools/search-qualified \
  --output .doc-tools/search-authored
MEL_SEARCH_QUALIFICATION="$PWD/.doc-tools/search-qualified" \
  python -W error -m unittest discover -s docs/tests -p test_search_publication.py -v
make test-documentation-source
make test-policy
```

With separately installed Playwright 1.64.0 and Chromium:

```sh
python docs/tools/search_browser.py --site .doc-tools/search-qualified/published \
  --output .doc-tools/search-browser.json
```

The browser harness clicks every current public symbol and operation, including
all overload destinations across Shared/Core, IR and RF, plus all approved
top-level guides. It checks mandatory categories and native anchors, keyboard
movement and activation, clearing/no-match/multiple-result behavior, 256-character
input/50-result limits, malicious text, desktop/mobile and light/dark layouts,
SHA-256 cache rejection (including a forged index with a recomputed shell digest),
and native JavaScript-disabled navigation. The consumer tests additionally
reject omitted/duplicate/miscategorized/private/nested/forged records and changed
shell/style/router bytes even after generated manifest hashes are updated.

Dedicated PR search qualification is read-only and checks out the exact PR
head. The existing main-push-only Pages deployment and live verification guards
remain unchanged. Non-draft PR #235 must not be merged or deployed by this task;
auto-merge remains disabled. Readiness requires successful exact-HEAD
documentation, Bootstrap, authored HTML and dedicated search-browser results.

## Evidence status

Initial local reconstruction, official renderer verification/idempotence,
consumer search mutations and the exhaustive Chromium campaign passed. Full
current-source regeneration, schema-3 authored/security regressions and hosted
exact-HEAD jobs are separate required gates, not implied by those initial passes.
Retained local evidence is under
`/home/zboll/git/mel-gnatdoc-search-001-evidence`, including complete per-file
baseline/search inventories and SHA-256 hashes in `probe.json`, native ledgers,
build logs and browser per-destination arrival evidence. Hosted qualification
artifacts retain complete inventories and browser diagnostics but never grant
deployment authority. Final gate conclusions belong in the PR review report.
