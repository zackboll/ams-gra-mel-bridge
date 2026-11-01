# CI-002: Bootstrap event deduplication and conservative native stress

## Scope and review status

Starting main is `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`. This changes CI qualification,
not native behavior, bindings, examples, or publication bytes. The exact
Bootstrap workflow transition is **proposed pending independent review**.
Mechanical acceptance by the source-integrity checker is not policy approval.
No additional protected production file is changed. The checker itself was
already an explicitly authorized DOC-001 documentation addition.

### CI-002R2 reconciliation

The clean primary checkout was on the original local main, not staged with the
seven R1 files claimed by the earlier handoff. The preserved candidate tree
`f91f515884ef4436ad676dc055bb2712e311c9f1` was inspected against the original
PR head and recovered selectively in an isolated worktree. Its external workload
commit is `775a01954a4a989396cb9ae614a748df5d01e7ec`; it is evidence, not PR
history. Original evidence directories are unchanged.

Fetched main at reconciliation is `33720d155d99f183229b6ed87037ecbd53a3875b`,
tree `76d7988ea723a07ced054c0275e80763cbdcb772`, incorporating PR #128. A normal
merge retains PR history. The coverage conflict preserves both CI-002 and
IR-CI-004 scope. The source-integrity checker retains main's complete
`PROPOSED_IR_COUNTER_TRANSITION` and all older exact-content transitions; only
the Bootstrap successor hash is recovered from R1. The native IR test is
byte-identical to incorporated main. Independent source-policy approval remains
required for the exact Bootstrap transition; mechanical tests cannot grant it.

## Source authority and execution

Bootstrap listens to main pushes, pull requests and manual dispatches. Required
job names and the four compiler/build configurations are unchanged. No job-level
skip authorizes a native result, no prerequisite job is introduced, and no new
token or deployment permission is requested. Documentation workflow is unchanged.

CI-002R1 corrects the original proposal's HEAD-only native checkout. That checkout
qualified the feature branch, not the integration of feature and main. Native
checkout now leaves `ref` unset, preserving GitHub's synthetic PR merge commit.
The old successful hosted runs remain evidence for the code they actually tested,
not proof of merge-result qualification. Original proposal commit/tree:
`cec2a98ee90cbf532c66ddf8c1ff13e86b6b6ae0` /
`71dff7ad2af48c5cd6c0712e3ba75f5bb534a735`.

Compilation authority is the exact `BOOTSTRAP_BUILD_SHA` (`github.sha`). Before
CMake configuration, the policy checks clean tracked checkout parity, non-shallow
history and actual commit objects. PR builds require exactly two ordered parents
`[BOOTSTRAP_BASE_SHA, BOOTSTRAP_HEAD_SHA]`. The actual commit tree must match an
independent `git merge-tree --write-tree --no-messages base head` reconstruction.
Git replace objects are disabled; a reachable-object traversal also rejects
missing historical commits, trees and blobs in non-shallow repositories.
Missing support, conflicts, missing objects,
wrong topology, stale event identities or forged trees fail the native job;
there is no fallback to PR-head builds. Push/manual/unknown events require head
and build to match and retain FULL_STRESS.

Classification authority remains the immutable PR head and event base, distinct
from the build commit. The policy fetches the exact event base SHA, rejects
ambiguous merge bases, and compares
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
job summary identify mode, exact head/base/merge-base, build SHA/tree/parents,
reconstructed merge tree and all PR-specific changed paths. An exclusively
created pre-configuration audit must equal fresh verification before execution.
Every build/test/install command rechecks the same authority before and after
running, rejecting changed checkout, source, identities or classification. The
audit never authorizes a skipped job or replaces actual source verification.
This PR necessarily selects FULL_STRESS because it changes the workflow.

## Exact workflow transition

Foundation remains `42648a92eb0cc84b2980d16db6028992c0494ba1`. Foundation and
starting-main workflow Git blob: `b30f2e67ac26e15b623c3c2e61e2f7dad5dd36d3`.
Complete-file SHA-256 identities (not Git blob identities):

* Original: `47ec6d285239ce84ca89c3ec303561c539158c2fa491abfa1adc0769605df28e`.
* Corrected proposal: `c89a50fab57f9bbb7653cf26a8b0260f72d9436c1ebde188dde4b29f257514f2`.
* Superseded HEAD-only proposal (historical, **not admitted**):
  `e42d0031368885935b5157babe9f9b2063f157842a306a25995130789fb0fe8c`.

Only original or exact proposed bytes are admitted, with identical index and
working-tree states. Unknown intermediates, wrong foundation, missing files,
symlinks, executable bits and unmerged index entries fail. Existing RF, IR,
instrumentation and navigation exceptions and CLI base restriction are retained.

## Additive qualification and publication invariants

The original 497 inherited DOC-012 tests, 49 original CI-001 controls, reviewed
IR-CI-003 additions and 12 CI-001R2 controls remain intact. All original forty
CI-002 identities are retained and 31 CI-002R1 controls are additive:
650 total = 142 scripts + 508 enhanced.
Shard counts are 34 navigation, 35 publication, 72 live, 57 symbols/catalog,
52 types and 258 remaining. Historical CI-001R2 fixtures explicitly isolate
their original 579 identities; they do not filter actual required execution.
The new test module is whole-file SHA-256 pinned and its static identity set
must match discovery exactly. Inventory/artifact/aggregate/deployment gates
are unchanged. Real CTest failure controls supplement actual native workloads.

Real divergent Git fixtures independently advance native base code and either
static feature documentation or separate native feature code, then perform a
genuine Git merge. Warning-clean C11 compilation/execution distinguishes stale
feature native code (`1 2` / `1 9`) from the combined merge (`7 2` / `7 9`). The
static PR still selects COMPLETE_SMOKE; the native PR selects FULL_STRESS.
Real malformed commits cover swapped/missing/extra/unrelated parents and forged
trees, plus stale merges, tag objects, incomplete history and source changes.

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

CI-001 main qualification was freshly confirmed successful during CI-002R1:
Documentation run
40104689494 (preparation, six shards, build, deploy and verify-live) and
Bootstrap 40104689498 (all eight jobs). Do not cancel or rerun them. After
merge, verify main push runs full stress and feature pushes no longer launch
Bootstrap. Pre-merge absence of a branch-push run depends on GitHub trigger
timing and is not evidence of main-only behavior by itself. No additional
manual dispatch should manufacture performance data. Queue time and runner
execution time must be measured separately; no percentage saving is promised.
