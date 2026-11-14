# MEL-GNATDOC-015-002 — source-backed authored-HTML pilot

## Disposition and adoption boundary

**COMPATIBILITY_QUALIFIED — small shared-enhancer extension required.** This
qualifies the compatibility decision, **not** a complete candidate publication
or a production switch. Official `ada-gnatdoc-enhancer` v0.1.5 unchanged cannot
render the current Examples and API Map. Production remains `guides.mode =
"none"`; its renderer, source-integrity checks and deployment workflows are
unchanged. No runtime API, Ada source, native ABI, Rust or Python binding changes.

Refreshed source: `0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab`, tree
`a6c579aa1f9d71f40971a7e556da8da210f1ba5f`, including merged PR #193
(`77e78fcec78b964799ea305b73e4c60cf88b33d1`). Clean isolated worktree:
`/home/zboll/git/mel-gnatdoc-015-002`. Candidate commit/tree and exact-HEAD
hosted run links are recorded in the PR handoff to avoid self-referential hashes.

Official wheel SHA-256:
`d712d330f213fc1909a66c47bc2c7e5e15d5ec687222bea3c4922f5243db4247`;
upstream revision `0120bee2f2455e4c832d1d5ddae0a4cb03cb9477`.
The pilot verifies installed origin, release version and complete package/assets
with the existing consumer verifier; it never imports a sibling upstream checkout.

## Compatibility matrix: authentic current inputs

| Guide (source order) | v0.1.5 authored HTML | Classification |
| --- | --- | --- |
| Getting Started | Byte-identical | Directly supported |
| Examples | `unknown/unresolved portal placeholder` | Shared extension required |
| Architecture | Byte-identical | Directly supported |
| IR Workflows | Byte-identical | Directly supported |
| RF Workflows | Byte-identical | Directly supported |
| Ownership and Errors | Byte-identical | Directly supported |
| API Map | `unknown/unresolved portal placeholder` | Shared extension required |

Landing and `assets/portal.css` are also byte-identical. All seven identities,
labels, order, paths and original source bytes are used, without removing tokens
or dropping guides. Navigation and complete Shared/Core, IR, RF ownership are
directly supported. Opaque destinations come from source/profile/native-anchor
reconciliation, not guessed hashes or guide-provided destinations.

| Feature | Result / smallest safe treatment |
| --- | --- |
| `{{example:ID}}` | Not expanded; requires a bounded source-owned example interface |
| `{{public-symbol-index}}` | Not expanded; requires source/native catalog generation |
| `{{example-usage-index}}` | Not expanded; requires canonical-source use catalog generation |
| `<details>` / `<summary>` | Both rejected by authored HTML allowlist, independently probed |
| `data-example-source` | Rejected; needs owner-checked generated annotation |
| `data-catalog-entry`, `data-kind`, `data-scope` | Rejected; need complete catalog validation |
| `aria-hidden` source-end marker | Rejected; needs narrowly permitted generated form |
| Native-output SHA-256 | Directly supported per source epoch, safely consumer-generated |
| Schema-2 contract digest | Supported integrity seal, **not** independent source approval |
| Arbitrary templates/callbacks/HTML injection | Intentionally incompatible; not requested upstream |

Changing the consumer to paste pre-rendered examples/catalogs or escaping/removing
their attributes is not a safe migration: it either loses functionality or keeps
the existing feature renderer behind a new wrapper. Neither workaround is used.
No full candidate output exists; missing files are explicitly `null` rather than
silently filled from production in the comparison report.

## Independent authority and evidence

The source-epoch baseline regenerates native GNATdoc and production presentation
twice from separate exact Git exports, verifies idempotence and compares complete
publication records. The pilot then independently measures the source/native
inventory, validates the pristine site, reconstructs **every** production API
byte/type occurrence/symbol destination and authored page, and verifies the full
publication before generating the disposable contract. The generated anchor
ledger is written only into that export, never over the repository's historical
ledger. Renderer exception remains exactly
`AMS.MEL.IR.Health_Status.Metadata.Status`.

Observed baseline: 41 public packages, 1,486 native declaration anchors, 313
stable symbols, 1,743 linked type occurrences; 403 catalog entries, 235 example
use occurrences, 14 distinct executable/support source files, nine programs and
13 run cases. These are observations, **not** future gates. Package inventories,
groups, declarations, anchors, input/output hashes and manifest digest are
re-derived on every run. Normal feature development does not need historical
count or output-size updates. Source content itself remains unchanged, including
historical explanatory counts in the existing API Map; those prose counts are
not used as inventory authority.

Evidence is retained at `/home/zboll/git/mel-gnatdoc-015-002-evidence`:

* `baseline/epoch.json`: source identity, complete native ledger, symbols,
  type-link occurrence audit and per-file pristine/publication SHA-256.
* `probe3/evidence.json`: exact source/tree, all 1,747 source file hashes,
  package-source ownership, native anchors, symbols, type links, complete symbol
  and example-use catalogs, canonical executable ownership and source hashes.
* `probe3/gnatdoc-authored.json` and `gnatdoc-authored-pilot.toml`: schema-2
  contract and disposable configuration derived from current sources/native.
* `probe3/comparison.json`: every one of the 55 production files with old/new
  SHA-256 and explicit unavailable candidate files; seven accepted files equal.
* `probe3/failed-full-pipeline`: actual rejected native-only pipeline input;
  atomic enhancer failure leaves native files unchanged, no candidate approval.
* `browser.json`, test logs and `examples.log`: actual execution, not inference.

Small reviewable comparison and contract snapshots accompany this document in
`/home/zboll/git/mel-gnatdoc-015-002/docs/mel-gnatdoc-015-002-evidence/`.
These snapshots are diagnostic only: subsequent source epochs regenerate their
own values. The non-deploying task workflow retains complete reproducible hosted
baseline/probe artifacts. Neither snapshot nor artifact digest authorizes source.

### Negative cases

All 18 cases reject: missing guide, symlink guide, substituted/resealed guide,
duplicate guide identity, altered/resealed package group, altered/resealed native
hash, altered/resealed native anchor destination, forged catalog entry, wrong
rendered example source, substituted executable bytes, unsafe script, event
attribute, URI, arbitrary executable template, unexpected example source, native
file, publication file, and manipulated contract digest.

Important experimental distinction: v0.1.5 accepts a source substitution if the
contract input hash and config digest are both recomputed. The independent
Git-byte verifier rejects it. `check_html` alone does not reject a JavaScript
URI; URI checks belong to the separate URI/full publication validation layers.
The pilot tests `safe_uri` for that negative case, not an invented guarantee
about the HTML element allowlist. No safety behavior is weakened.

## Actual local validation

Commands run from `/home/zboll/git/mel-gnatdoc-015-002`, using the hash-verified
interpreter `/home/zboll/git/mel-gnatdoc-015-002-evidence/enhancer/bin/python`
(`$PY`) and GNAT 16.1.0/GPRbuild 26.0.1 on `PATH`:

```sh
$PY docs/tools/docs_ci.py qualify \
  --directory /home/zboll/git/mel-gnatdoc-015-002-evidence/baseline \
  --gnatdoc-prefix /home/zboll/git/ams-mel-ada2/.doc-tools/gnatdoc-26.0.0
$PY docs/tools/authored_html_pilot.py \
  --commit 0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab \
  --baseline /home/zboll/git/mel-gnatdoc-015-002-evidence/baseline \
  --output /home/zboll/git/mel-gnatdoc-015-002-evidence/probe3
MEL_AUTHORED_PILOT_BASELINE=/home/zboll/git/mel-gnatdoc-015-002-evidence/baseline \
MEL_AUTHORED_PILOT_COMMIT=0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab \
  $PY -W error -m unittest discover -s docs/tests -p test_authored_html_pilot.py -v
make test-policy
DOC_EXAMPLES_ALIRE_SETTINGS=/home/zboll/git/ams-mel-ada2/.doc-tools/settings \
  sh docs/tools/qualify_examples.sh
```

Results: baseline repeat/deep audits pass; pilot five tests pass with all 18
negative cases; `make test-policy` passes; example format/build/loader inspection
and 13 semantic run cases across nine programs pass. No first-party Ada was
edited, so `make format-ada` is inapplicable. Full runtime test suites are not
substituted for documentation compatibility qualification.

Final pilot regression suite additionally checks the independently run API-only
control (six tests total). All 46 API/control files are byte-identical, including
`presentation.json`; they are not falsely attributed to the failed full candidate.
The comparison records these separate control hashes alongside explicit missing
full-candidate hashes. Final evidence uses `probe-final/` in place of the earlier
`probe3/` paths above; the schema-2 contract bytes are unchanged.

Further validation: official release tests **5 pass**; source-epoch regression
tests **10 pass**, including an actual documented source addition and independent
regeneration; live transport tests **50 pass**; source-integrity guard passes.
The complete legacy `test_live_docs_site.py` invocation reports two obsolete
workflow-layout assertions and a missing historical `_site` fixture; standalone
`test_docs_portal.py` reports 13 missing `_site` fixture errors. These failures
are reproduced against the unchanged baseline, not removed or weakened here.
Current-source reconstruction, the existing current `make test-policy` checks,
and fresh browser qualification remain the applicable passing controls. Historical
fixture suites are not represented as passing against current generated bytes.

Chromium command:

```sh
/home/zboll/q3-7b1f9c/v/bin/python docs/tools/authored_html_pilot_browser.py \
  --baseline /home/zboll/git/mel-gnatdoc-015-002-evidence/baseline \
  --probe /home/zboll/git/mel-gnatdoc-015-002-evidence/probe3 \
  --chromium /home/zboll/.cache/ms-playwright/chromium-1248/chrome-linux64/chrome \
  --output /home/zboll/git/mel-gnatdoc-015-002-evidence/browser.json
```

**640 checks / 16 contexts pass**: 1280/390 px, light/dark, JS on/off, production
and accepted-page navigation view. Retains all eight authored page visits,
responsive layout, native fallback, default package, Shared/Core/IR/RF links,
filter/clear, source-derived symbol destinations, history, examples and script
error checks. The accepted-page view overlays seven byte-identical outputs on
production to exercise navigation; API, Examples and API Map are explicitly
production fallbacks, **not** a qualified full candidate. This test-only view is
not an integration path. No all-browser/accessibility certification is claimed.

Initial diagnostic setup failures are retained: omitted `GNATDOC_PREFIX` for the
source-epoch regression runner; unused external attribution URL rejected by
host allowlist (replaced by local `index.html`, without adding host permissions);
and incorrect assumption that `check_html` itself validates URI schemes. These
were corrected without changing production contracts or enhancer implementation.

## Minimal upstream proposal and next implementation task

See `/home/zboll/git/mel-gnatdoc-015-002/docs/mel-gnatdoc-015-002-upstream-interface.md`.
Implement that bounded shared capability in its own upstream task, release it,
then run **MEL-GNATDOC-015-003: complete source-backed authored-HTML adoption**.
Require all nine portal outputs byte-identical, complete native reconstruction,
full candidate browser qualification, regenerated-manifest tamper cases and
exact-HEAD hosted evidence before changing production mode and removing the
superseded portal renderer. A new upstream release is necessary; switching to
v0.1.5 unchanged is not recommended. No merge, auto-merge or Pages deployment is
part of this pilot.
