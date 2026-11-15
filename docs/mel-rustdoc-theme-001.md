# MEL-RUSTDOC-THEME-001 — Rustdoc palette qualification

## Scope and identities

Starting main commit `cb33c5e5434fa27b9d9660f6a9d361ed0993e37b`, tree
`0f6851c7e87ed002491fd61c2c43bd2683243ca1`, fetched before creating isolated
branch `docs/mel-rustdoc-theme-001` in `/home/zboll/git/mel-rustdoc-theme-001`.
The existing dirty checkout and other worktrees are untouched. PR #215 changes
the Rustdoc generator's crate inventory and tests: this task does not change
that inventory, any workspace source, public API, ABI or runtime dependency.
Final commit/tree and exact-head hosted results belong in the PR, avoiding a
self-referential source identity. Coverage delta: documentation appearance and
qualification only; no API completeness claim.

## Visual authority

The current qualified publication configuration enables the bundled theme from
the official ada-gnatdoc-enhancer **0.1.5** wheel, revision
`0120bee2f2455e4c832d1d5ddae0a4cb03cb9477`. The existing consumer validates that
release's package bytes and distribution identity. Its `gnatdoc-theme.css`
SHA-256 is `cf06637732a8b047a7191bfb991a5c3da7cd31a52076876880d5f8a3f63929bc`.
Neither the enhancer nor any Ada input/stylesheet is modified.

| Role | Light | Dark |
| --- | --- | --- |
| Page | `#ffffff` | `#141b26` |
| Navigation | `#f5f7fb` | `#1c2634` |
| Text/headings | `#243247` | `#e3eaf5` |
| Muted | `#51627a` | `#b0c0d5` |
| Border | `#c8d4e3` | `#485b74` |
| Code | `#f3f6fb` | `#202d40` |
| Link | `#15559f` | `#8bc4ff` |
| Active/hover selection | `#e1ecfa` | `#293f5c` |
| Focus/attribute | `#925100` | `#ffd184` |
| Keyword | `#7442a7` | `#cfa8ff` |
| String | `#14663e` | `#8ddfb7` |
| Number | `#95520c` | `#ffc48e` |
| Comment | `#53657b` | `#b4c2d4` |
| Operator | `#415572` | `#c0cfe4` |

Rustdoc's semantic syntax classes retain differentiated colors from these Ada
roles. All navigational links use the Ada link color; Rustdoc still supplies
their kind labels and syntax classes. Warning borders use Ada's focus/attribute
color. Native icons, layout, sizing, scrollbars, fonts, search and navigation
JavaScript remain unchanged. Ayu and unknown themes are not targeted. System
preferences and persistence remain owned by native Rustdoc theme selection.

## Qualified inputs and reconstruction

The MEL-owned input is `docs/assets/rustdoc-mel-theme.css`, SHA-256
`1c71d3a839a3ed6697083f1ad65a9917f3bd4c2201e7dbd3741dab9ce50d879e`, Git blob
`5a763d03b62ba2d938e8582c5698825600a17ebd`.

Rustdoc 1.98.1's help and a real generation probe confirm supported
`--extend-css`: it emits the supplied bytes as **`theme.css`**, irrespective of
the input basename, and links it after built-in styles. No generated HTML or
CSS is patched. The generator checks both approved blob and SHA-256 identities
before invoking tools, rejects missing/symlink/changed inputs, and uses encoded
Rustdoc arguments to handle source-export paths without shell splitting. Host
Rustdoc flags remain excluded by the controlled environment. Existing locked,
offline, no-dependency generation, warning and broken-link flags are preserved.

The record binds the normalized complete invocation, source/run identity,
approved theme input, existing pinned executable bytes and full output hashes.
The log includes the concrete export path and encoded arguments. Independent
verification re-exports the approved Git source and regenerates the entire Rust
tree with the pinned toolchain, comparing every filename and byte, plus stable
record fields. It does not trust a self-consistent output manifest as authority.
The existing deploy/live verifier jobs therefore install that same pinned Rust
release; the bounded workflow policy explicitly authorizes only that exact
command. No job, required check, permission or deployment condition is added.
Existing external bundle/manifest and same-run checks remain mandatory.

## Qualification commands and evidence

Explicit Chromium qualification is in `docs/tools/rustdoc_theme_browser.py`;
it requires the separately installed Playwright environment, not a production or
ordinary build dependency. It derives expectations from the qualified Ada CSS,
uses local project-prefix HTTP, records computed colors and contrast, and
captures Ada/Rust screenshots at 1440×1000 and 390×844 in both modes.

Real Rustdoc negative/reproducibility tests are in
`docs/tests/rustdoc_theme_regression_tests.py`; ordinary fixture/security tests
remain in `docs/tests/test_rustdoc_publication.py` under the existing CI target.
Exact commands/results, complete baseline-to-themed inventory differences,
Ada preservation and screenshots are recorded in external task evidence and
the PR after execution. Pending work is not qualification success.

## Limitations

Only the already-qualified Linux x86_64 Rust 1.98.1 toolchain is supported.
Reproducibility is for that exact release, not future Rustdoc versions. Chromium
tests are bounded usability/contrast checks, not full accessibility or
cross-browser certification. No deployment, publication, hardware qualification,
merge, approval or auto-merge is authorized by this task's evidence.
