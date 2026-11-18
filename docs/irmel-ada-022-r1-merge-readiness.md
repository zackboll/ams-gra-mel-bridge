# IRMEL-ADA-022-R1 — PR 236 merge-readiness correction

## Integration and scope

Normal merge `1ddcc1be913f91e43f4802fa497ea7c79a47fe49` incorporates main
`86eee19b252da563246d87072f4d71f69a377e30` without conflicts, preserving both
original AbortTask commits. No native/Ada implementation or structured coverage
bytes changed during integration. No other Scheduling command, safe Python/Rust
command API, vendor source, or workflow policy is added by this correction.

Coverage delta: none beyond the original task's reviewed 57 → 58 safe Ada
operations. The five-field converter, four actions, Enabled-only admission,
unlocked actual upstream dispatch, common Return engine, ownership/DSO retention,
timeout/cache, complete diagnostics and limited Ada owners remain intact.

## Original authored-HTML failure

Inspected run 43764778608: the fixture omitted `python-reference.json`, so the
normal verifier failed before reaching its corruption assertions. Verified main's
fix before merging. The merged fixture generates real Python files/record,
assembles those files and writes the record. The sealed manifest includes it.
No redundant fixture patch or integrity/source-epoch relaxation was necessary.

## Additional inherited resize-gate defect

Exact merge-head run 44015110919 failed in `resize_parity.py` with
`source/native or authored baseline differs`. This gate assumed every later PR
was the original resize-only feature: identical pristine API documentation and
exactly four changed files relative to main. AbortTask legitimately adds public
Ada and raw Rust API documentation; main also already enables resize. Even a
source-identical later PR cannot have the original four-file introduction delta.

The narrow correction keeps that strict original introduction comparison when
the baseline Git configuration has resize disabled. When main already enables
resize, it compares enabled/disabled publications of the **same candidate
source**. Both original baseline and candidate artifacts first pass the unchanged
normal verifier in detached exact-source checkouts. The candidate retains hosted
event identity; only the explicitly non-deployable baseline uses local identity.
Verifier errors propagate. Candidate native/coverage bytes are copied unchanged
into a disposable disabled control, and its complete Rust/Python assembly must
match byte-for-byte. The existing exact four-file delta, independent second
disabled render and full assembly equality remain mandatory. No exception is
caught to fall back to a weaker comparison. Production rendering is unchanged.

## Actual local results

On the integrated implementation head, all requested commands exited zero:

- `make format-ada`, `make check-ada-format`, `make check-ada-docs-changed`
  (11 public declarations, zero missing/invalid annotations).
- `make test-native test-python test-rust`: native 841/841; Python 693 tests;
  Rust 617 passed, 15 existing ignored, zero failures. Combined Make invocation
  shares the native prerequisite rather than rebuilding it three times.
- `make test-ada`, `make check-ir-api-coverage` (72/72 controls), `make check`.
- `alr -n -C ada build`, `alr -n -C ada/tests run`.
- `make test-policy`, source integrity (825 vendored files/six workflows),
  workspace rustfmt, all-target Clippy `-D warnings`, Rustdoc `-Dwarnings`,
  final Ada format check, final newline/whitespace checks.
- Pinned `docs_ci.py build` and `qualify`, enhancer 0.1.6/GNATdoc 26.0.0:
  5181/5182 informational coverage; 103 private-part warnings, zero public,
  private-unit, tool or unclassified warnings and zero mapping failures.
  Rustdoc 1.98.1: 1653 HTML/669 assets; Python: 197 HTML/three assets.
- `qualify_authored_html.py`: 32 negative controls rejected; five evidence
  tests passed. Chromium: 2968 checks/16 contexts passed.
- Pinned `make test-documentation-source`: 11/11 passed in 570.740 seconds,
  including valid-artifact acceptance, recomputed-payload-hash corruption
  rejection, Python-page/record mutations and independent forged-hash rejection.
- Documentation executable examples, final formatter/Clippy/Rustdoc campaign:
  passed (`FINAL_SUPPLEMENT_PASS`).
- Strict C11 layout/signature probe: size 24, alignment 8, offsets 0/8/12/16/20.
  Python/Rust compiled-C layout comparisons passed with their language suites.
- Final rebuilt production `nm -D --defined-only` inventory exactly equals the
  332-symbol map, with no test exports. Current-main delta is exactly
  `ams_mel_ir_scheduling_exec_abort`; no removals.

Correction-specific validation so far: `python3 -W error -m unittest discover
-s docs/tests -p test_docs_ci.py -v`: 21/21 passed, including new exact-source,
candidate-event preservation, verifier-error propagation, wrong-tree/old-head/
deployable-baseline rejection and original-introduction-path controls.
Pinned `test_resize_publication.py`: 4/4 passed, retaining resealed shell/asset
mutations and complete independent reconstruction. Live corrected parity and
final-commit qualification are recorded in the existing PR as they complete;
pending results are not claimed here. Corrected live parity passed against the
sealed main baseline and integrated candidate: exactly the four resize files
differ, all 2582 other assembled files are identical, and the independently
regenerated disabled assembly matches. No API additions were excluded.

The downloaded diagnostic artifact omitted Cargo's hidden empty `.lock` file.
For local diagnostic replay only, its empty bytes were restored after confirming
their SHA-256 exactly matches the original sealed manifest; no manifest was
resealed or check bypassed. Fresh local main generation also passed.

## Review and limitations

### Subsequent main integration

While final hosted checks completed successfully on `ae49fdc`, main advanced to
`51ede458587698cc1f8e4601d3471852d6f2a9e5` (PR 221). Incorporated that revision
with another normal merge. Its Rustdoc palette and independent Git-source Rust
regeneration are retained. The sole textual conflict was `resize_parity.py`:
combined the new source-bound same-candidate wrapper with main's qualified
palette projection and comparison controls in `qualify_delta`, rather than
discarding either side. The existing main projection negative tests remain.
All 1472 compiled/language/coverage inputs are still identical to the qualified
implementation merge. Both unit suites passed after reconciliation: pipeline
21/21 and Rustdoc publication 17/17. Source-bound documentation and hosted checks
must qualify the new merge HEAD; earlier green checks are not substituted.

The final diff is self-audited for ownership, concurrency, ABI and lifecycle;
this is not independent approval. A separate technical review is requested at
<https://github.com/zackboll/ams-gra-mel-bridge/pull/236#issuecomment-6937986324>.
Per user direction no formal reviewer assignment is required to finish technical
qualification. `REVIEW_PENDING` remains explicit. No merge, self-approval or
auto-merge. Exact final SHA/tree and hosted workflow conclusions belong in the PR
description to avoid self-referential commit identities in tracked source.

Qualification remains mock-provider-only. Physical task abortion and metadata
delivery are not inferred from future completion. No upstream private layout is
read, no raw-handle concurrent-deletion protocol is promised, and conservative
retention of uncertain provider graphs is unchanged.
