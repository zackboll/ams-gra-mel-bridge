# RUSTDOC-005 — isolated Rustdoc portal assembly

## Scope and source

Base commit `99d7f11bc3e5b19c10c6d9daf0e7fe1c89b6fd41`, tree
`03dd5f736e05f422a8952778b18617842f3345a6`, freshly fetched `origin/main`.
Task branch `docs/rustdoc-005-portal-integration`; independent worktree
`/home/zboll/git/ams-rustdoc-005`. Final identity is recorded in the PR rather
than embedding a self-referential commit hash here.

Documentation generation/verification only. No binding source, comments, API,
ABI, provider, historical evidence or structured API coverage changes. Coverage
delta is publication of the existing safe and raw Rust references, not new
implementation or documentation-completeness claims. No missing-doc gate added.

## Reviewed verification boundaries

- `scripts/validate_docs.py:validate` accepts the exact pristine Ada portal;
  it rejects active content and reconciles packages, anchors and authored pages.
- `source_epoch.generate` exports Git blobs, checks Ada compilation and GNATdoc
  completion, derives the current ledger/coverage, renders and validates that
  **Ada-only** portal. No frozen warning or completeness threshold is substituted.
- `source_epoch.enhance` validates the pinned enhancer's isolated API staging
  output, then calls `verify_presentation`. The latter's exact inventory remains
  pristine Ada files plus the original three approved enhancer additions. Native
  restoration, type occurrences, declarations, catalog, anchors, pinned JS and
  stylesheet checks remain mandatory against **Ada-only** output.
- `build_enhanced_site.validate` is the historical qualified Ada publication
  checker; its exact inventory, historical projections and fingerprints are
  unchanged. It must not receive an assembled Rust publication.
- `check_live_docs_site.graph` remains the Ada-only active-content/link checker.
  `get_entity` remains the exhaustive transport/byte checker; extra MIME types
  require an explicit Rust-only call and `/rust/` URL boundary.
- `docs_ci.verify` validates the **whole sealed artifact** against an external
  manifest digest and exact checkout/run identity, then independently validates
  isolated pristine/qualified Ada and Rust, and reconstructs the complete
  assembly. `unpack` binds the external bundle digest, canonical manifest,
  complete archive inventory and safe regular members before extraction.
- `docs_ci.live` exhaustively fetches **every published file**, not a sample;
  local graphs are qualified before HTTP. Each returned decoded entity must
  equal the approved same-run artifact bytes. External hyperlinks are never
  fetched. Cross-origin or asset redirects remain forbidden.

## Assembly and authority

`pristine/` and `ada-qualified/` remain separate from `published/`. Rust generation
uses the same exported source and foundation Cargo-doc commands, a fresh target,
isolated Cargo home, controlled environment, `--locked --offline --no-deps`, and
`--deny rustdoc::broken_intra_doc_links`. It never builds the native facade or
runs doctests. Both crates are generated together and the **entire** doc subtree
is retained, including `.lock`, notices, source, implementation metadata, search
shards, fonts and shared assets.

The selected documentation toolchain is explicitly installed as **1.98.1**,
not mutable `stable`. Cargo/rustc/rustdoc full version strings and executable
SHA-256s are recorded in `rustdoc.json` with commit/tree/run identity, flags,
diagnostics, timing and ordered file/hash inventory. The generation record is
not self-authorizing: all its bytes and every output byte are covered by the
existing externally supplied manifest and bundle digests from the same build
job. Deployment does not execute Cargo or trust a newly provided Rust baseline.

Assembly copies Ada unchanged, adds exactly `rust/`, and inserts one constant,
consumer-owned section immediately before the single root `</main>`. Verification
reconstructs the complete expected file mapping and reverses that insertion to
recover the original qualified Ada root **exactly**. All `api/`, guide and asset
URLs remain unchanged. Root links use `rust/ams_mel/index.html` (primary safe API)
and `rust/ams_mel_sys/index.html` (explicitly low-level unsafe FFI, not recommended
application API). `rust/index.html` provides a separate labeled landing.

## Rust-only active-content policy

Rustdoc 1.98.1 intentionally generates inline JS, styles, forms, SVG UI, and
`javascript:void(0)` UI hyperlinks. Only exact generated/sealed bytes under
`rust/` receive this policy. External **hyperlinks**, never resource sources,
are limited to versioned `https://doc.rust-lang.org/1.98.1/` and Rust upstream
issue URLs. No arbitrary remote scripts/fonts/CSS, external verification fetches
or new redirect authority. CSS local fonts must resolve; exact generated SVG
data UI icons are retained. Local HTML paths/fragments resolve, including literal
percent-encoded Rustdoc IDs and JS-interpreted source ranges with real endpoints.
Rustdoc JS dynamically constructs search/implementation URLs: complete inventory
hashing protects all such assets; representative browser tests exercise runtime
loading. This is not a claim of static JavaScript semantic analysis.

Eight live HTTP workers bound concurrency while retaining deterministic ordered
evidence and all original response framing, MIME, encoding, origin and exact-byte
checks. Failure of any file fails the publication; no sampling is introduced.

## Actual qualification

Initial implementation `87e892f12da07306f6e05b5cead97f07415bd1ab` was built in
two independent worktrees with fresh exports/target directories. Both complete
builds passed (37.033s and 36.902s), Rust generation/validation 3.09s and 3.06s.
Implementation `5211887d7d69049c969efdcbc34b5fec94fbf33d`, tree
`c67385437d01ef048e51f7ce639c07b90189518b`, passed another full build in 37.294s
(Rust 3.14s). These are measured times, not gates.

Actual inventory: 54 pristine Ada files, 57 qualified Ada files, 2,068 Rustdoc
files (1,435 HTML, 633 other assets), 2,125 assembled published files. Rust
assets: 605 JS, 14 WOFF2, six TXT notices/licenses, three CSS, two SVG, one PNG,
one Markdown notice and `.lock`. No historical Rustdoc count is an acceptance
threshold. Both crates' broken-link diagnostics: **zero**. Safe missing-doc
diagnostics: zero; raw missing-doc warnings: 2,389, informational, not a new gate.
Ada source-derived coverage: 5,016/5,048 (99.3661%), GNATdoc warnings 124,
informational; this task does not claim to fix pre-existing source-doc gaps.

The two independent outputs had identical complete file/hash mappings:

| Projection | SHA-256 of canonical file/hash mapping |
| --- | --- |
| Pristine Ada | `8afb8d6f677b9fdbf9797ee78f5f89d601bf4a22ec94a1d589589c8d064fa727` |
| Qualified Ada | `5804ec93050ae065768142b56cce06a3f8b1ce1755575b7af15daa772a98c158` |
| Assembled | `58dd6dc12764a869cbfe24d1555107440198306bbde92f540dcded0a4e9fff05` |

All existing Ada API destinations and anchor-bearing page bytes are identical
between qualified and assembled output. The only non-Rust change is the exact
reversible root insertion. Original root Ada navigation remains; the added safe
and raw relative URLs resolve beneath the project prefix.

Commands run from `/home/zboll/git/ams-rustdoc-005` (with the pinned GNAT/GPRbuild
on PATH and the official enhancer interpreter where required):

- `python3 scripts/rustdoc_001_foundation.py`: passed independently, no native
  facade, no doctests; 1,435 HTML, same warnings noted above.
- `make test-policy`: passed all 262 tests before the final scoped-MIME test
  addition (18 source-integrity, 18 source policy,
  28 history policy, 65 epoch policy, 75 Bootstrap policy, plus 58 documentation
  tests). Intentional CTest failures inside Bootstrap negative fixtures are
  expected and their enclosing tests passed.
- `make test-docs-ci`: passed; current identity/archive/deployment tests,
  Rust assembly negatives, foundation tests, static subtype and prose tests;
  final fast suite **59/59**, including the added real-HTTP scoped-MIME negative.
- `make test-documentation-source`: **10/10 passed**, 226.396s. Repeated actual
  generation, documented and undocumented source additions, source/manifest
  mismatch, independently regenerated forgery rejection, original Ada metadata,
  subtype and JS mutations remain effective. Only fixture assembly was adapted.
- `python3 -W error -m unittest discover -s docs/tests -p test_live_docs_site.py -k Transport -v`:
  **26/26 passed**; the analogous `-k URL`: **24/24 passed**. Includes redirect,
  ambiguous response, gzip, MIME, encoded traversal and active-content negatives.
- Full archive extraction/reverification against external bundle/manifest SHA:
  passed. All 2,125 project-prefix HTTP entities compared to approved bytes:
  passed in 6.319s with eight workers, one complete attempt, no sampling.
- `docs/tools/rustdoc_portal_browser.py` under separately installed Playwright
  and Chromium 1248: **13 checks passed**. Safe/raw navigation, client-side Session
  search and result click, source navigation, search shards, CSS/fonts, Ada
  filter/API map, all requested responses 200, no JS/network errors. All requests
  confined to `/ams-gra-mel-bridge/` on loopback; external resources blocked.
- Original historical `test_docs_portal.py`: **13/13 passed** and
  `test_doc_foundation.py`: **24/24 passed** in detached `f5e5b89` with its exact
  40-file pristine baseline independently hash-checked by the original validators.
  Direct attempts without that historical baseline were rejected (missing site
  or source/package mismatch), not fixed by relaxing any test or fingerprint.
- `python3 scripts/check_final_newlines.py` and `git diff --check`: passed after
  correcting missing final newlines in new files.

### Hosted artifact evidence (implementation HEAD)

Documentation run [43662202000](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43662202000)
passed on exact `5211887d7d69049c969efdcbc34b5fec94fbf33d`, PR #212, attempt 1.
`deploy` and `verify-live` were **skipped**, as required. Source/run identity in
the downloaded artifact matches commit, tree, repository, PR ref, run and attempt.
Hosted Rust generation/validation took 6.490s and had the same complete inventory
and qualified executable hashes. Artifact ID `14140353973`:

- Bundle SHA-256: `c6468b15c485d832d19ef7f7608bf25cb200d6641fefdd8afd91ee1b8942d442`.
- Manifest SHA-256: `198210dd258816378c3ddedc1004184fb11b9c3af8f7bbc136376b7b0d567c3b`.

Final-HEAD local reruns, hosted status, exact final commit/tree and artifact
identities are recorded in PR #212 and the delivery report, not predicted here.

## Limitations and deployment distinction

Qualification is Linux x86_64 only: exact official 1.98.1 executable hashes are
reviewed in the generator, not accepted from the generation record. It fails
closed on other releases/platforms or substituted executables. Rust standard
library/dependency toolchain distribution trust remains Rustup's verified release
installation; this does not claim a reproducible compiler bootstrap. HTML is
repeatable with this exact toolchain, not across Rust releases.

Doctests remain separately qualified by RUSTDOC-001 through RUSTDOC-004; no native
or doctest rebuild is added to HTML publication. No real provider or hardware
qualification is implied. Local complete HTTP verification is not a claim of
GitHub Pages availability or production latency. Actual deployment and complete
live byte verification occur only after an authorized merge/main push. No
self-approval, merge or auto-merge is performed. No old Ada fingerprint, anchor,
asset allowlist, security-negative mutation or publication privilege is replaced
with an unchecked assembled baseline.
