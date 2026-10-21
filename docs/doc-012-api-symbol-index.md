# DOC-012 — source-verified Ada API symbol and example index

## Scope and authority

The API Map retains its complete 29-package directory and adds static indexes of
public types/subtypes and operations actually referenced by executable examples.
No JavaScript, new publication file, remote resource, runtime API or dependency
upgrade is introduced. Native declarations remain the destinations; a type's
presence does not imply provider or hardware support.

`docs/tools/symbol_audit.py` supplies the independently source-qualified stable
inventory: **238 symbols, 231 types and seven subtypes, across 24 owners**. It
reconciles public source spelling, visibility and declaration profiles with real
native package headings, opaque filenames, declaration anchors and the approved
`docs/gnatdoc-native-anchors.json` ledger. Abbreviated enumeration profiles require
the complete ordered native literal list. Private completions, nested members,
internal packages and synthetic generic-instance members cannot confer authority.

Stable-bookmark eligibility deliberately still excludes these owners because
their public scopes exceed the bounded resolver:

* `AMS.MEL.IR.C2`
* `AMS.MEL.IR.Channel`
* `AMS.MEL.IR.Health_Status.Metadata`
* `AMS.MEL.Status`

The catalog links directly to native declarations, not to query-string symbol
routes. **Nine additional types used by examples** have separately proven native
destinations in the first two excluded owners. They occupy a distinct section
and never receive stable-bookmark eligibility. The existing API browser and its
238 stable symbols remain byte-identical.

## Executable-example association rules

`docs/tools/example_sources.py` owns the approved manifest and DOC-011 occurrence
ledger; `docs/tools/ada_examples.py` resolves actual Ada tokens against public
source/native declarations. The renderer groups by exact ledger identity and
native page/anchor, never by displayed identifier. Each occurrence belongs to
the manifest entry whose main or support listing contains it. Repeated uses of
one declaration in one example produce one association. Shared support sources
contribute to each actual containing example, not to unrelated examples.

Only `type-mark` and `unique-operation` occurrences contribute. Context-package
links, prose, comments and string literals do not demonstrate declarations.
Types and subtypes retain their classification; functions and procedures retain
their source-qualified owner. Unique-operation eligibility requires exactly one
public declaration candidate: no overload inference, suffix matching or
unsupported alias resolution is added.

Reconstruction yields **57 demonstrated type/subtype declarations with 85
example associations**, and **81 operations with 111 associations**. The original
99 type/subtype and 136 operation *occurrences* remain unchanged in the 266-link
inline ledger. The 328 catalog entries (238 stable, nine additional, 81 operations)
have **196 deduplicated example associations**. Many stable types legitimately
have no verified example use. Source-level use and mock-backed execution within
the documented contract do not establish full real-provider/hardware coverage.

## Rendering and independent validation

`scripts/build_docs_portal.py` expands exactly one `{{public-symbol-index}}` and
one `{{example-usage-index}}` in `docs/guides/api-map.html`. Entries are grouped by
Shared/Core, IR and RF, then owning package, then public source declaration order.
Headings, lists, visible source-qualified names and ordinary relative links work
with keyboard navigation, browser Find and JavaScript disabled. Existing portal
CSS supplies responsive wrapping and visible focus; it is unchanged.

`docs/tools/api_catalog.py` renders from the occurrence ledger. Its independent
validation path instead walks every approved example main/support source and
re-resolves occurrences without trusting ledger listing numbers, candidate HTML,
output manifests or claimed counts. It verifies public declaration identity,
kind, owner, native section and approved destination, then compares parsed actual
entry/link tuples, exact associations, source ordering and canonical serialization.
All canonical example/support bytes and the manifest also have a separately
qualified SHA-256 identity. The main site validator invokes this reconstruction
before its complete authored-page comparison. Existing source preservation,
native restoration, coverage, warning and type-link checks remain active.

The existing DOC-009 live verifier reconstructs and verifies the local catalog
before any HTTP request. It verifies served catalog bytes again after comparing
every entity, and retains exact-byte comparison, graph/fragment checks, MIME,
gzip, restricted redirects, bounded retries, stale-content rejection, read-only
inputs and external diagnostics. No second publication verifier is introduced.

## Publication preservation and measured fingerprints

The complete publication remains **43 files and 38 HTML pages**. A byte comparison
against the authenticated main artifact changes only `guides/api-map.html`;
the other 42 files are identical, including every native API page, API index,
router, presentation manifest, other guide, examples HTML and portal stylesheet.
All 31 highlighted canonical listings and executable Ada sources are unchanged.
The 1,237 native sections, 1,428 native type links, 238 stable symbols and 266
inline example links are preserved.

The independently observed delta is 328 declaration links + 196 example links +
three local section-navigation links = **527 references**. The final graph has
**4,052 references**, versus 3,525 before this change. Counts do not authorize
destinations or replace exact reconstruction.

Fingerprints below are SHA-256 over the canonical sorted file-to-SHA-256 mapping
used by the existing publication verifier:

| Projection | Before | DOC-012 |
| --- | --- | --- |
| Complete pristine | `3a853e1a6c06e19f7f21d45622de3135054e7beb711245b1838555246c0d6c49` | `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291` |
| Complete final | `22e51b0a72744cf4a18991b54433ec802872d6a427d5cee1fe97a8ac33cd83d7` | `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb` |
| Pristine API only | `d2026657ddd02d2513514c545633c13fbaf8ce772e1bf35bf51223e16d811983` | unchanged |
| Final API only | `9790f7666b867250e8c78e998a32eb233c3cbe76f9e83bbda7f4d8978f0b6f37` | unchanged |
| Historical pre-bookmark API projection | `8f5090330038b70905211fd8ee32a2d662ee7902a27c9c2dc61028b19c48385f` | unchanged |

Complete pristine and final guards preserve their historical fingerprints via
an exact one-region API Map projection as well as checking new complete bytes.
No API-only fingerprint or historical qualification baseline is rewritten.

## Extension and requalification

1. Change public source/example behavior only in an independently authorized
   task; never expand bounded source resolution to manufacture coverage.
2. Reconcile source identities, visibility, native profiles, filenames and anchors
   against the approved ledger. Unsupported semantics are a blocker, not guessed.
3. Reconstruct stable and additional inventories and both occurrence/association
   ledgers. Review every changed identity and containing example, including shared
   support sources. Requalify example source hashes only after that review.
4. Run all existing documentation tests and additive catalog mutations. A mutation
   must actually change an isolated candidate, be rejected, and restore exact
   approved bytes. Preserve source and native checks and historical test identities.
5. Generate two fresh pristine and two enhanced trees; compare every byte. Review
   complete publication deltas and preserve independent API-only fingerprints.
   Measure reference counts from the qualified graph, never predict a new total.
6. Execute all seven repository gates in order, the strict local HTTP campaign,
   and real Chromium desktop/mobile, light/dark, scripts-on/off acceptance. Browser
   coverage is not cross-browser or full assistive-technology certification.
7. Retain external catalogs, reverse-reference proofs, source hashes, mutation
   results, browser observations and complete file comparisons; preserve historical
   DOC-008 through DOC-011 evidence. Commit/push/open a non-draft PR only after local
   qualification. Confirm exact local/remote/PR HEAD and hosted results; stop at
   independent review without merging, auto-merging or manual deployment.

External DOC-012 evidence is retained under
`/home/zboll/git/ams-mel-doc-012-evidence`. The final handoff records completed
command exits, corrective iterations, browser observations, exact Git identities
and hosted status; this document does not predeclare pending gates successful.

## Executed local qualification

The complete required seven-command sequence passed in order: `make test-native`,
`make test-ada`, `make check`, `make check-ada-format`, `alr -n -C ada build`,
`alr -n -C ada/tests run`, and `sh docs/tools/qualify_examples.sh`. Native CTest
retains 434 distinct tests, all passing. Nine canonical programs compiled and all
thirteen execution cases ran with their expected outcomes. The script-side suite
passed 133/133; the frozen original enhanced suite independently passed 330/330.
The candidate retains all inherited identities and passes **364/364 enhanced
tests**, adding 34 catalog tests: **497 documentation tests total**. The complete
original 463-test inventory is retained.

Two fresh pristine generations and two enhanced generations compare equal in
every byte. The strict local HTTP verifier passes all 43 entities and 4,052
references. Ordered native type-audit records, stable symbols and inline example
occurrence records equal the authenticated baseline exactly. Source integrity
still protects 1,162 inherited production files; no warning, coverage, vendor,
anchor-ledger, production source or executable-example changes were made.

Real Chromium 156.0.8078.4 passed 434 catalog/navigation checks in eight
desktop/mobile, light/dark, scripts-on/off contexts, including every declaration
destination, keyboard focus, source-spelled text Find, example fragments,
Back/Forward and the existing native package browser with and without scripts.
The unchanged DOC-011 highlighted-example campaign passed 794/794 checks. Both
campaigns observed zero console/page errors and unexpected remote requests.

Corrective runs remain in the external evidence: an early suite overlapped
regeneration and rejected changing inputs; initial comparison included
renderer-only listing metadata; the repository newline gate caught missing final
newlines in new files. These were corrected before the final complete sequence;
no assertion or native/source authority was weakened. The tool's configured old
worktree directory disappeared during qualification and had to be recreated as
a marker-only directory for command startup. Neighboring historical worktree and
evidence paths also disappeared outside DOC-012 edits; no DOC-012 deletion command
targeted them. Authenticated baseline artifacts, a frozen baseline source archive
and DOC-012 evidence remain separate from the implementation worktree.
