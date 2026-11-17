# DOC-RESIZE-CI-001-R1 — main reconciliation for PR #239

## Source identities and scope

Prior PR head: `1a3c741abe3c83bb9752e7a892e69e30f2ed8232`, tree
`d43cf550dc1a3d582ccd89851a728121194dc049`.
Fetched main: `9614d4b9808ceae47daa67a3aa6bbe89fad62a06`, tree
`d6d25bbc66db19e4b2287817ff45b7ad87bf5a7d`.
Merge base: `51ede458587698cc1f8e4601d3471852d6f2a9e5`.
Before the ordinary merge: 2 branch-only and 8 main-only commits.

GitHub reported CONFLICTING/DIRTY despite all five original-head workflows
eventually succeeding. Those are historical results, not qualification of this
integration. The existing branch and PR are retained; no rebase, force-push,
new PR, approval or PR merge is performed.

Only two paths actually conflicted: `.github/workflows/documentation-resize.yml`
and `docs/tools/resize_parity.py`. Main's Scheduling AbortTask, ABI, inventories
and language bindings are integrated unchanged; this correction adds no product
functionality. Historical task prose is preserved; this document supersedes its
old PR-trigger description for current workflow behavior.

## Resolution

Keep main's CI-DOC-FAST-001 main-push/manual triggers, exact `github.sha`, strict
full-SHA ancestor baseline selection, source provenance, concurrency/cancellation,
permissions, pinned tools and independent complete candidate repetition.
Do not restore heavy PR triggers or change unrelated workflows. Baseline
generation remains a separate non-deployable source-local diagnostic gate.

Use the PR's manifest-digest-required verifier for each exact checked-out source.
It performs complete independent verification before language reuse and creates
a modified-export-not-deployable disabled control. Baseline verification is
explicitly local (`GITHUB_ACTIONS=false`); the current candidate and its repeated
publication retain hosted event identity. Compare all bytes with no Rust/theme
projection or cross-revision fallback. The exact authorized four-file set and
strict CSS/JS-only inventory additions remain unchanged.

The optional repeated publication must independently verify at its own supplied
manifest digest with the identical event/source identity; complete published,
pristine and isolated Ada bytes and native/coverage records must match. Language
records are verified separately rather than compared by nondeterministic timing.
The workflow supplies separate fresh current/repeated generation directories.

Reconcile main's verifier API tests to exercise the retained in-process complete
verification: wrong tree, stale artifact, wrong hosted event, exception
propagation, and no fallback or export on failed authorization. Update only the
workflow-wiring assertions that changed CLI; retain all event/ancestry/security
fixtures and all existing real Python/Ada/Rust source-feature/tamper controls.

## Qualification and disposition

Serial exact-commit qualification and actual exits are retained in
`/tmp/doc-resize-ci-001-r1-evidence/`. The final PR ledger records exact merged
commit/tree and hosted run URLs; no old artifact is relabeled as integrated
source evidence. Qualification is in progress; no readiness claim is made here.
Use external disk-backed temporary storage because `/tmp` inode exhaustion and
protected-checkout temporary outputs caused environmental failures previously.

Under main's new trigger policy, only Bootstrap and normal documentation run
automatically on PRs. The three heavy workflows must be dispatched manually on
the exact feature head for hosted qualification, with an explicit strict ancestor
for resize. Skipped deploy/live jobs are not qualification successes. Independent
technical review remains outstanding; no self-approval, auto-merge or PR merge.
PR #233 is not modified and can incorporate corrected main only after this fix
is independently reviewed and explicitly authorized for merge.

## Main advanced during qualification

The first integrated commit was `90c95185e4c6ea64c99b05bb56f81034b8c04caa`,
tree `f522311cb852cdbb643a7168442172717e9c164d`. Its full serial campaign passed:
128 fast documentation tests, 332 policy tests, separate main/current/repeated
complete publications, both strict parity epochs, four real-artifact feature/
tamper tests, deep reconstruction, four resize/five release/three search/five
authored tests, 32 authored negatives, 11 source-epoch tests, resize/search
Chromium, and `make check` (841 native tests, 72 IR coverage controls and Ada).
Formatting and final integrity checks passed without tracked-source changes.
No old-head passing record was used in place of this qualification.

The guarded pre-push fetch then found main had advanced to
`3b00a0a1b8019e6a5c2312a17659a6a7050f8e26`, tree
`696ea59e851bb53f59beaf5f014777d0c7833695`, incorporating RUST-IR-019.
Delivery stopped before pushing. That delta contains only the documented safe
Rust Buffer feature and its tests/task document; no workflow, documentation
helper, native, Ada or Python change. A second ordinary merge integrates it
without conflicts. Its source-derived Rust documentation must be freshly built
and independently verified, not hidden by a projection or a historical count.
Final qualification uses fresh `.doc-tools/r1-current-main-*` artifacts and
`/tmp/doc-resize-ci-001-r1-evidence/current-main/` logs. Product subtrees must
match this newly incorporated main, and all exact-source documentation gates
and final checks are re-run before pushing and manual hosted dispatch.
