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

## Qualification record

Implementation qualification is in progress. No successful hosted CI, main
deployment, live deployment, or independent-review-ready disposition is claimed
until the final delivery evidence explicitly records it. Actual commands,
counts, timing, browser results and limitations will be recorded below.