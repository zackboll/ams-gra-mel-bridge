# CI-002: Bootstrap event deduplication and conservative native stress

## Scope and review status

Starting main is `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`. This changes CI qualification,
not native behavior, bindings, examples, or publication bytes. The exact
Bootstrap workflow transition is **proposed pending independent review**.
Mechanical acceptance by the source-integrity checker is not policy approval.
No additional protected production file is changed. The checker itself was
already an explicitly authorized DOC-001 documentation addition.

## Source authority and execution

Bootstrap listens to main pushes, pull requests and manual dispatches. Required
job names and the four compiler/build configurations are unchanged. No job-level
skip authorizes a native result, no prerequisite job is introduced, and no new
token or deployment permission is requested. Documentation workflow is unchanged.

Each native checkout uses the exact event head with complete history; the policy
fetches the exact event base SHA, validates both immutable commit identities and
checkout parity, rejects shallow history and ambiguous merge bases, and compares
complete merge-base/head trees with an independently parsed NUL-delimited raw
diff. Rename detection is disabled deliberately: both old and new paths are
qualified. Raw records must agree exactly with the full tree delta, preventing
partial or forged diff output from authorizing smoke. Spaces, tabs, newlines and
non-UTF-8 path bytes are not split or interpreted as shell source.

Only `README.md` and `.md` files directly in `docs`, `docs/guides`,
`docs/reference`, `docs/history`, or `docs/tasks` can authorize COMPLETE_SMOKE.
Unknown nested directories are not exempt. Both sides of every change must be
regular non-executable Git mode 100644. Symlinks, submodules, mode transitions,
empty deltas and every unapproved path select FULL_STRESS. Authority failures
fail the required native job. Main pushes, dispatch and unknown events default
to FULL_STRESS. No PR title, label or human classification is consulted.

Both modes build, execute the entire unfiltered ordinary CTest inventory once,
and perform the existing CMake installation qualification. FULL_STRESS adds the
unchanged `ctest --test-dir build/native --parallel 4 --repeat until-fail:50
--output-on-failure`. Commands use checked subprocesses and stop on failure;
there is no fallback from a failed stress campaign to smoke. The JSON audit and
job summary identify mode, exact head/base/merge-base and all changed paths.
This PR necessarily selects FULL_STRESS because it changes the workflow.

## Exact workflow transition

Foundation remains `42648a92eb0cc84b2980d16db6028992c0494ba1`. Foundation and
starting-main workflow Git blob: `b30f2e67ac26e15b623c3c2e61e2f7dad5dd36d3`.
Complete-file SHA-256 identities (not Git blob identities):

* Original: `47ec6d285239ce84ca89c3ec303561c539158c2fa491abfa1adc0769605df28e`.
* Proposed: `e42d0031368885935b5157babe9f9b2063f157842a306a25995130789fb0fe8c`.

Only original or exact proposed bytes are admitted, with identical index and
working-tree states. Unknown intermediates, wrong foundation, missing files,
symlinks, executable bits and unmerged index entries fail. Existing RF, IR,
instrumentation and navigation exceptions and CLI base restriction are retained.

## Additive qualification and publication invariants

The original 497 inherited DOC-012 tests, 49 original CI-001 controls, reviewed
IR-CI-003 additions and 12 CI-001R2 controls remain intact. Forty new CI-002
controls belong to `remaining`: 619 total = 142 scripts + 477 enhanced.
Shard counts are 34 navigation, 35 publication, 72 live, 57 symbols/catalog,
52 types and 227 remaining. Historical CI-001R2 fixtures explicitly isolate
their original 579 identities; they do not filter actual required execution.
The new test module is whole-file SHA-256 pinned and its static identity set
must match discovery exactly. Inventory/artifact/aggregate/deployment gates
are unchanged. Real CTest failure controls supplement actual native workloads.

Publication remains 43 files, 38 HTML pages, 4,052 internal references, 238
symbol bookmarks, 266 example API links and 31 canonical Ada listings.
Pristine fingerprint: `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`.
Enhanced fingerprint: `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
Completed command results, independent event inventories, byte comparisons,
hosted timings and exact source identities are attached to the PR rather than
committing external diagnostics or invalidating qualified heads.

## Maintainer configuration and post-merge obligations

Read-only GitHub inspection reported main unprotected (HTTP 404, "Branch not
protected"). A maintainer must enable required pull-request checks for `build`,
`native (gcc, Debug)`, `native (gcc, Release)`, `native (clang, Debug)`,
`native (clang, Release)`, `ada`, `rust`, `python`, and `build-tree-isolation`.
Select the matching GitHub Actions checks, not similarly named external checks.
Do not accept skipped required matrix jobs. No repository setting is changed.

Before merge readiness, independently require CI-001 main Documentation run
40104689494 (preparation, six shards, build, deploy and verify-live) and
Bootstrap 40104689498 (all eight jobs). Do not cancel or rerun them. After
merge, verify main push runs full stress and feature pushes no longer launch
Bootstrap. Pre-merge absence of a branch-push run depends on GitHub trigger
timing and is not evidence of main-only behavior by itself. No additional
manual dispatch should manufacture performance data. Queue time and runner
execution time must be measured separately; no percentage saving is promised.
