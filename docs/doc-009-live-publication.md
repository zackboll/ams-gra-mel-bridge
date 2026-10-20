# DOC-009 — live GitHub Pages acceptance

## Boundary and independent authority

The read-only checker is `docs/tools/check_live_docs_site.py`; offline controls
are discovered from `docs/tests/test_live_docs_site.py`. They deliberately live
under the already-authorized documentation tree rather than widening the fixed
source-integrity script allowlist. No production, example, native/vendor,
enhancer, authored HTML, navigation asset or authority fingerprint changes.

The exact committed DOC-007F consumer qualifies the artifact against independently
pinned pristine bytes, complete source, native declaration ledger, immutable
installed enhancer `3867c8879a3c832fb6e096d7680a8b739a2cffe1`, source-owned
metadata and full output/manifest reconstruction **before HTTP**. A downloaded
presentation manifest cannot authorize expected content. Both qualified input
trees must be stable, trusted, nonsymlink filesystems throughout verification.

The caller supplies the full deployed Git SHA established by successful deployment
workflow/metadata. HTTP proves entity identity, not commit provenance; arbitrary
HTML does not establish a Git identity. Production accepts an explicit HTTPS
`*.github.io/<repository>/` origin/prefix without credentials, query or fragment.
Loopback HTTP is available only through the Python fixture API, not the CLI.

## Invocation

From the repository root, with the immutable enhancer installed:

```sh
.doc-tools/enhancer/bin/python docs/tools/check_live_docs_site.py \
  --artifact /absolute/qualified-publication \
  --pristine /absolute/qualified-pristine \
  --base-url https://zackboll.github.io/ams-gra-mel-bridge/ \
  --expected-commit FULL_DEPLOYED_40_CHARACTER_GIT_SHA \
  --attempts 10 --interval 30 \
  --evidence /absolute/external-existing-parent/live-publication.json
```

The evidence filename must be fresh and outside repository/artifact/pristine
trees. Exclusive creation prevents overwriting an existing file or alias. No
source, artifact, repository setting or website is modified. The report includes
every attempt, file status/final URL/MIME/encoding/entity length/SHA-256/redirect
chain/comparison, complete publication inventory and fingerprint, all internal
references, complete 1,428 source/native-qualified type occurrence records, ten
separately derived additions, original ordered 1,418-record fingerprint, metadata
authority and canonical example identities. Failure returns nonzero.

Every attempt GETs all 43 approved files; it cannot combine partial successes
across attempts or accept a stale 200. Gzip response bodies are decoded before
comparison; transfer representation is not publication content. Strict MIME and
UTF-8 parameter behavior, duplicate header/framing guards and bounded entity reads
apply. Redirects are disabled except an observed 301/308 from the repository root
without slash to its exact canonical slash root. Even same-origin redirects from
individual files, authorization queries and repository escapes fail.

The independently inspected served href/src graph must remain entirely inside
the approved inventory, with actual fragment IDs and no remote/active dependencies
beyond the existing qualified index-only local script. Complete served snippets
are audited again against source/native authority, not `data-type` claims.
All 31 decoded complete listings must equal canonical Ada sources; the nine-main,
thirteen-case inventory is retained, without compiling HTML or adding hardware
execution. Existing hosted compile/execute qualification remains mandatory.

## Permanent workflow

The existing PR/main build and main-only Pages upload/deploy boundaries remain.
One additional **main-only pristine artifact** preserves the already-qualified
native authority for the later checker. The read-only `verify-live` job depends
on successful `deploy`, checks out the exact run SHA and downloads the two exact
same-run named artifacts. It installs the same immutable enhancer and runs the
checker with at most ten complete attempts, thirty seconds apart. It has only
`contents: read` and `actions: read`, no Pages/OIDC write permission or script
deployment credentials. Diagnostics are uploaded even on failure. PRs never run
the job; their normal documentation discovery exercises isolated HTTP fixtures.

This task qualifies existing merged-main deployment separately from local checker
behavior and hosted PR fixtures. The new job's real deployment operation can only
be observed after independent approval and a future normal main merge; it is not
claimed to have run on this feature branch.

## Baseline and evidence

Fetched starting main is actual PR #119 merge
`d420dc765156fafdae447be14aa7802b6cbbba17`, tree
`eecf26dcc38f364b3cefa24703513410f5279b96`, parents
`626319064585f0cdd59c385b72da9bcc08b3007a` and
`f45d96c9270f7fbda6249abe8db60c842b5ded34`. No intervening main changes or
equivalent DOC-009 work existed. No existing worktrees or running tasks changed.

Documentation run `39340608573`, attempt 1, completed build and Pages deployment
successfully on that exact commit. Deployment `7187482114` is successful in
`github-pages` at `https://zackboll.github.io/ams-gra-mel-bridge/`.
Review ZIP artifact `12229631044` SHA-256:
`d44c335c5c8e26e0497209dea81808d772b552b8199ea68daafc52671ba4ac2a`.
Pages ZIP artifact `12229690857` SHA-256:
`4f082f70aad9a84743ce4c8c58b8e1f11928ede7468a2ac694d650c1dcc01c4b`.
Both archive digests and workflow associations were independently checked;
all 43 Pages TAR members equal the review ZIP files. Duplicate, symlink,
nonregular and traversal archive members were rejected before extraction.

Fresh A/B pristine and enhanced generations equal completely. Independent
downloaded-artifact validation accepts 43 files / 38 HTML / 29 packages / 1,237
native sections / 3,021 references / 1,428 local type links / zero external.
The first explicit live check found all 43 entities byte-identical, including
gzip decoding, and recorded the exact canonical 301 root normalization.

The fresh public-site Chromium 156.0.8078.4 campaign passed **10,057 assertions**
across 1440×1000 and 390×844, light/dark and JavaScript enabled/disabled. It
recorded 2,804 resource responses (200 plus canonical 301), no console/page errors,
native final anchors, search/tagged navigation, history/bookmarks, all six report
links, ten additions, canonical examples, keyboard activation and code scrolling.
Sixteen screenshots and the complete matching publication inventory are retained.
The initial external harness lacked its ten-addition ledger; a second run's
recorder misclassified the explicitly tested slashless canonical root as remote.
Both failures remain preserved; only the harness setup/exact-root recorder was
corrected, with no publication changes, before the passing fresh campaign.

All six requested sequential repository commands passed: native **434/434**,
Ada, aggregate check, Ada-format check, Alire build and Alire test run. The
merged-main Bootstrap run `39340608614` also passed: each of four native matrix
jobs records all 434 tests with exactly 51 successes (ordinary plus 50-repeat).
The documentation suite retains the original 328 tests and adds 72 offline
acceptance/security controls, **400 distinct tests**; focused subsets are not
additional tests. Final complete-suite and example execution conclusions are
recorded only after their actual completion in the handoff.

External evidence, actual final-head results, browser observations, screenshots,
preservation, failures and immutable evidence manifest are recorded in
`/home/zboll/git/ams-mel-doc-009-evidence/r1/FINAL-HANDOFF.md` after qualification.
No pending workflow or future deployment gate is counted as passing. Chromium
acceptance is bounded, not comprehensive accessibility/cross-browser certification.
No merge, auto-merge, force-push, rebase, tag, release or manual deployment.
