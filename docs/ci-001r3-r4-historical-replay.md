# CI-001R3-R4: explicit corrected historical live-fixture replay

This is a source-policy proposal for existing PR #137. Mechanical acceptance is
not independent approval and grants no publication epoch.

## Starting identity

- Main `219b00c75210f5985d9889e8b541da2e7b658f04`, tree
  `4d5c6bb29cfbe6ac39041808e36af34cb84a8c0e`.
- PR #137 predecessor `e85d99bbec15452b4edf5ef554784a18986db709`, tree
  `64430578a6e458396561f7888c478634aaed0a8f`.
- PR #136 is not an input and is not modified.

## Authenticated failure

Documentation run `40989263985` on the predecessor, live job
`123862363728`, failed. The DOC-CI-004 diagnostic artifact `12950348192`
(ZIP SHA-256 `918a478cbe732b039949e759d9c115d679ec835837be4abb202d53ae2075acf7`)
matches GitHub's recorded digest. Its `failure.json` hashes for
`execution.json`, `historical.log` and `unpack.log` match the retained files.
The inner report records all 72 historical live identities started and stopped
exactly once: 70 successes and two failures,
`test_live_docs_site.QualifiedArtifactTests.test_missing_required_type_link`
and `...test_wrong_native_anchor`, each `AssertionError: 0 != 1` at the
replacement-count assertion. Bootstrap run `40989264030` passed. Original
evidence is retained read-only outside the repository, separate from all later
qualification results.

## Root cause

Historical epoch `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf` (tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`) contains the CI-001
`docs/tests/test_live_docs_site.py` (SHA-256
`0658a2e40b8e815e71a9de61e077b7b0a122f74f23ba385d363f80ebfc0b6164`). Its
`QualifiedArtifactTests.api_page()` returns `next()` of a filesystem `glob`, with
no suitability check. The authenticated hosted historical publication has 29
non-index API pages. The AMS root namespace page
`5f9de8789b28ed02de7bf3d4f6aa6447445b50a3c927e5e3bb132a2c8efb1779.html`
contains no native `<h4 id=...>` declaration anchor and no type-reference link.
The other 28 pages contain both.

Running the unchanged historical test methods from a clean historical checkout,
with only `Path.glob` ordering controlled, reproduces both exact `0 != 1`
failures when the root page enumerates first. Selecting
`09bd8292c9fb6f9b8644595432b39db4e5b39aba955eceb6cbca789ebcb5efc0.html`
(AMS.MEL.IR.Instrumentation) or another suitable declaration page lets both
mutations occur and both tests pass. No historical source or artifact was
modified. The hosted log does not name the selected path; the actual hosted
enumeration order is not invented, only shown to be unconstrained.

This is the defect DOC-PERF-001-R2 (`d425b29e0570c8c970b318e6c25e0f761a0571e1`)
already corrected on main. That correction is not an ancestor of the immutable
POL-002 historical epoch, so the historical replay still executed the old helper.

## Explicit replay transition

`docs/tools/ci001r4_replay.py` constructs one child commit of the original
epoch from immutable reviewed Git objects only:

| | Commit | Tree | `test_live_docs_site.py` | `documentation_ci.py` |
|---|---|---|---|---|
| Original historical epoch | `5c136a04…` | `0073b862…` | `0658a2e4…` | `87cf2034…` |
| Corrected replay | `f1a512fe0b7de08fff99ed379d6bb2145fe978ef` | `cf865c6d00d43636641daa84f8ff980fa132df93` | `4693fe45…` | `93c68f09…` |

The replay's live test file is exactly DOC-PERF-001-R2's, which differs from the
original only in `api_page()`. That helper derives its page from the trusted
source/native `Authority.required_additions()`, sorted by `occurrence_key`, and
verifies the containing declaration section, the native anchor, the required
occurrence in the semantic link audit and complete native restoration. It does
not use a hardcoded filename or filename sort. The historical qualifier gains only
DOC-PERF-001-R2's exact `live_fixture_source()` admission (SHA-256
`1e5f8b2b…`) and its single call; that admission re-derives the exact successor
from immutable CI-001 bytes. Every test body and identity is unchanged.

Construction uses a fixed author/committer identity and timestamp, so the
commit is reproducible; the policy rejects any other tree, commit, parent, delta
or source byte. The original commit is never rewritten or relabelled.

`epoch_ci.prepare` still runs the original historical source-integrity check,
generation, enhancement, verification and second deterministic regeneration from
`5c136a0`. It records the original inventory, switches the same clean,
publication-bearing clone to the replay commit, reruns source integrity, then
produces the inventory, scripts report and historical seal. The inner seal and
every historical execution report therefore identify the replay. The outer
`epochs.json` binds both identities, the original and replay inventories, all
source hashes and the reviewed fixture commit. Shard runners construct the same
replay before unpacking. Aggregation requires the replay execution identity and
publishes a `historical_execution` record stating
`corrected-replay-not-original-historical-execution`.

Both inventories contain the same 142 script and 437 enhanced identities and the
same six shard assignments. Only the live test-source hash differs.

## Preserved boundaries

Unchanged: historical publication generation, fingerprints and approval epochs;
all production, published HTML, anchors, GNATdoc, renderer configuration and
warning baselines; CI-ACTIONS-001 Node 24 pins; CI-001R3's three corrected
methods; DOC-CI-004 diagnostics; the six shards; exact-once accounting; sealed
input identities; main-only Pages; read-only live verification; the publication
approval registry; workflow files; current tests and the current qualifier.

The complete successor tree is bound by exact hashes in the replay policy from
either verified predecessor. No `docs/tests`, `docs/tools`, `scripts` or workflow
exemption is created. The new policy cannot approve itself:
`independent_approval` and `publication_epoch_approved` remain false.

## Controls

`pol002_epoch_tests.py` adds committed-Git and real-execution controls. The
fixture probe (`docs/tests/ci001r4_replay_probe.py`) runs in a separate
`python -P` interpreter inside the replay checkout, importing only replay
modules, against freshly regenerated historical native output. It proves root
unsuitability, exact original failures, order-independent source-derived
selection, exactly one mutation of each kind, rejection by the publication
validator, and rejection of missing/fabricated occurrences. Further controls
reject missing/altered replay sources, helper/test/admission edits, dirty or
overlaid checkouts, identity substitution, inventory additions/removals and
reassignment, unrelated production/workflow/registry changes, failed historical
tests and diagnostics as aggregate inputs.

Exact local and exact-head hosted results live in external evidence, not here.
Rollback is a normal reviewed revert.
