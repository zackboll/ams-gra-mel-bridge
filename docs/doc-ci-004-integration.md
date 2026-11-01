# DOC-CI-004-INTEGRATION / CI-001R3-R3

This is a diagnostic-only successor proposal for existing PR #137, not a
historical live-shard functional correction or independent approval.

## Verified source and preservation

- Main: `219b00c75210f5985d9889e8b541da2e7b658f04`, tree
  `4d5c6bb29cfbe6ac39041808e36af34cb84a8c0e`.
- PR #137 predecessor: `e7eaba2cdf61a51f51f5a12d27e246c1041a1ce4`, tree
  `d90c9b33ed40ada5e363276f01ba09fd044e61d2`.
- PR #136 original: `c41b6920d5c22365a51c9febfa1baa238dbeef55`, tree
  `851898cd1237bbf4c73346a96d2b0bbc9e9b8b0f`.
- External R2 snapshot: `aea6e3dd6a38f44243a2c7458923cdac54443f4e`, tree
  `fc02a1800500284e862f903cf9d633471792a68d`. Its parents are original #136
  and main. It was verified in the preserved external clone, then fetched into
  the target object store. PR #136's index, worktree and pending merge are not
  inputs to any state-changing command.

Every recovered diagnostic file was compared byte-for-byte between the staged
Git blob and external snapshot. Original/main/#137/recovered SHA-256 identities
are retained in the external `diagnostic-provenance.json` evidence. The old
workflow was not copied over a newer version: its entire main-to-R2 diff was
inspected and consists only of the diagnostic argument, success-only canonical
upload, and separate always-run diagnostic upload. Bootstrap remains unchanged.

## Diagnostic and authorization boundaries

`epoch_ci.run_shard` retains complete execution/unpack logs, raw inner report
bytes when present, original child exit status, source/tree/run/attempt/shard and
sealed-input identities, retained-file hashes, and bounded escaped log excerpts.
Malformed/missing reports do not conceal child failure. Diagnostic collection
errors cannot replace the child exit status. Output and diagnostic destinations
reject traversal, symlinks, stale files, protected source and sealed-input
overlap. No retry, test skipping or historical source mutation is introduced.

The strict workflow reader requires the exact reviewed action owner/SHA/release
comment in the diagnostic role, after execution and canonical upload, with
`if: always()` and missing-file warning semantics. Aggregation downloads only
source/run/attempt-bound `documentation-result-*` artifacts and rejects required
failed/missing/skipped/cancelled jobs. Failure evidence cannot authorize a gate.
Permissions, main-only Pages and read-only live verification are unchanged.

The new executing `doc_ci004_source.py` policy binds complete verified
predecessor trees and complete successor hashes/modes. It permits only named
files in the exact integration, including the necessary checker transition.
Candidate manifests and the publication registry cannot authorize anything.
The old four-file diagnostic helper is retained for its historical controls but
does not authorize classification. CI-ACTIONS-001's original atomic migration
and CI-001R3's original three-method transition retain their exact historical
Git fixtures. New integration controls use complete committed source trees.

The recovered current-workflow assertion that compared the protected tests to
pre-CI-001R3 bytes now compares their independently restored historical view.
The restoration proves all bytes outside the three authorized methods remain
unchanged. The three corrected methods themselves are not edited.

## Hosted incidents, not a root-cause claim

Runs `40807408658` (failed live job `123246118996`) and `40622165438`
retain independently hashed job-log archives and all fourteen available
artifacts. Every artifact ZIP digest matches GitHub's authenticated recorded
SHA-256. Preparation and qualified downloads succeeded; historical live child
execution exited 1; no canonical outer live report existed; canonical upload
failed independently. Nested historical reports/logs were not uploaded and are
unavailable. The precise inner failing test/setup/teardown operation and root
cause remain unknown. Bootstrap `40807408569` passed for the original head,
not for this successor. A green replay is not evidence of a functional fix.

## Qualification and disposition

Development checks are not protected frozen-source qualification. Complete
focused controls, mandatory local gates, current 650/historical 579 exact-once
campaigns, ordered epochs and independent regeneration must pass on one frozen
candidate before delivery. Exact-head hosted Documentation and Bootstrap are
then required. Detailed command logs and actual handoff status are external at
`/home/zboll/git/ams-ci-001r3-r3-integration-evidence`; no earlier campaign is
reused as acceptance for this successor. Preserve pristine/enhanced fingerprints
and do not grant independent or publication approval. Keep #137 open, unmerged,
non-draft and auto-merge disabled; keep #136 untouched.
