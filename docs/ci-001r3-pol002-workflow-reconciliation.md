# CI-001R3: inherited workflow tests follow the trusted POL-002 runner

## Starting authority and isolated scope

Fetched main is `e4e756aafa4edaffdb11fe341bb016380bf19ca9`, tree
`41274f8df25f1f3cfd75879778ba760cb84a19e0`. The independent worktree is
`/home/zboll/git/ams-mel-ci-001r3`, branch
`corrective/ci-001r3-pol002-workflow-tests`. This is a baseline correction,
not DOC-PERF-002 optimization/source admission, IRMEL audit approval or CI-003
runner-policy adoption. No workflow, production, binding, provider, example,
vendor, publication asset or approval-epoch change is included.

Preflight found PR #129 (IR coverage), PR #133 (CI-003) and PR #134
(IRMEL-POL-002A) open. Their worktrees and branches are untouched. The original
performance worktree/report, frozen policy tree
`e789666b604e0bf7d5b81e0510671067f4b67618`, indexes and external evidence were
fingerprinted before this task; no content there is modified by this correction.
Evidence for this task is external at
`/home/zboll/git/ams-mel-ci-001r3-evidence`.

## Unchanged-main reproduction and obsolete assertions

With the exact reviewed enhancer distribution installed, unchanged main's
official inventory discovers 142 script and 508 enhanced identities. Both live
workflow tests fail and the deployment workflow test errors before any edits:

1. `test_live_docs_site.WorkflowTests.test_main_only_read_only_dependency`
   expects `--artifact _site-published --pristine _site` and direct
   `documentation_ci.py deployment`. Main now unpacks sealed epochs and invokes
   the event-bound trusted runner's `deployment --root` and `live --root`.
2. `test_live_docs_site.WorkflowTests.test_pr_fixture_discovery_retained`
   expects direct workflow `documentation_ci.py inventory`/`aggregate`, the old
   canonical-example step label and an inline `DOC_EXAMPLES_ALIRE_SETTINGS`.
   POL-002 performs historical inventory/scripts, canonical execution and
   toolchain/settings isolation inside `prepare`, then issues the sealed input
   bundle used by six exact-once shard invocations and `aggregate`.
3. `test_documentation_ci.WorkflowTests.test_deploy_is_main_only_and_aggregate_authorized`
   searches for `documentation_ci.py deployment` before Pages upload. Its
   `.index()` errors because the deployment authorization is now performed by
   `${{ steps.authority.outputs.runner }}` selected from event-bound authority.

Complete baseline outputs are retained; historical-suite success is not
substituted for current-tree qualification.

## Corrected contracts, same three identities

Only those three inherited methods change. They delegate to the exact pinned
`docs/tools/ci001r3_workflow.py` structural verifier. No other method, import,
helper, class or existing `api_page()` correction changes. The verifier parses
the reviewed workflow's block structure, shell argument vectors and epoch Python
ASTs without adding a YAML dependency. Ambiguous required fields, duplicates,
unsupported permission syntax, extra/missing qualification commands, conditional
required steps and ignored failures reject.

Live verification checks main/non-PR gating, exact dependencies and read-only
permissions, deployed checkout/expected SHA, same-run artifact IDs, trusted
selection, unpack/deployment-before-live ordering, exact gate/artifact/digest
arguments, bounded HTTP verification and absence of privileged deployment there.
It verifies the epoch source's event/checkout/runner identity, actual deployed
source comparison and independent publication authorization.

Preparation checks trusted selection before pinned Alire epoch preparation,
source/run-bound sealed upload, the six unchanged shard IDs, no skipped shard
job, fail-fast false, trusted unpack-before-run with complete artifact identity,
actual historical source-integrity/inventory/script/seal calls, all nine language
commands including real canonical execution, source requalification, candidate
epoch generation and exact-once reconciliation. Assertions inspect the executable
statements, not obsolete display names or inert comment strings.

Deployment checks the always-evaluated aggregate build, its required dependencies
and actual aggregate command, main/non-PR deployment, minimal Pages permissions,
same-run inputs/gate IDs, trusted unpack/deployment before Pages upload, canonical
candidate publication path and matching run/attempt/source artifact names.
Epoch AST assertions retain gate SHA, hosted/main, complete shards, artifact
identity, mechanically qualified/deployable/source epoch and independently
reviewed publication checks. Pages actions outside the deployment job reject.

## Exact inherited-source transition

`docs/tools/ci001r3_source.py` records immutable predecessor commit/tree and exact
whole-file predecessor/successor SHA-256 pairs:

| Source | Predecessor SHA-256 | Successor SHA-256 |
| --- | --- | --- |
| `docs/tests/test_live_docs_site.py` | `4693fe458c1fae6dffc5d9d115e21dc9a5ff3a74380a0cd16ac22c4181444c74` | `76937ae947ecb2d3b7b873423abe4cceab93551b05dae281080af45b2f67d076` |
| `docs/tests/test_documentation_ci.py` | `5876442d34e4831c92f7df5b882cbbea886ffbec3bfae2731707583a0400973d` | `b4758385bb330fc17b925aae73592d6128e263d32db50026517dc6502732b236` |
| `docs/tools/documentation_ci.py` | `0df565993ce4858c7c417825f080062d3a21128911e4e45721f897237ab63b5b` | `b97d8ef68c6ed6788179615439cc91afd3e606200e036b775942f5e740934f0f` |

The verifier's exact SHA-256 is
`97817453acb10e22cf9090509b18dddf10c577bb22c2c8a64d0e7e6785e1c59b`.
These values describe this proposed correction only, never future unknown edits.

For the paired successor test sources, the qualifier verifies the exact three
replacement method ASTs, restores their immutable predecessor method byte
ranges, and requires byte equality for everything else. It then applies every
existing historical CI-001/CI-001R2/live-fixture check to the restored views.
Historical fixtures without the correction remain subject to the original
checks. The unchanged CI-001R2 controls also construct a minimal fixture with
immutable CI-001 live bytes and the current control-test module. Only that exact
combination outside the current source root, without the current qualifier or
CI-002 tests, restores the control method before the original checks. This is
not admission of an incomplete committed candidate: the policy route still
requires the complete paired successor. All twelve original CI-001R2 controls
pass unchanged. Test names, 650 identities and shard ownership are unchanged.

The `epoch_ci.classify` extension invokes this exact complete transition only
on the separate policy route. It verifies predecessor/successor blobs, regular
100644 modes, immutable predecessor tree, executing checker identity, complete
paired transition and narrow correction paths. It does not broadly add tests or
`documentation_ci.py` to `POLICY_PATHS`, alter `feature_source` eligibility, or
admit unrelated executable changes. Existing index/worktree/untracked-source and
ancestry verification remains active. Both independent approval flags remain
false; mechanical acceptance is not review approval.

## Independent controls

Ten additive `CI001R3Tests` methods in the explicitly invoked POL-002 epoch
suite use committed Git fixtures for exact policy-only classification,
predecessor restoration, historical compatibility, incomplete/stale transitions,
renamed/unknown/weakened methods, unrelated helpers/tests, source-integrity
assertion deletion, substituted qualifier/verifier/checker, unrelated production,
self-approval and publication-registry changes. Existing POL-002 test bodies
remain unchanged.

Workflow mutation controls remove trusted selection/deployment verification,
change gate/artifact/commit identities, omit/skip shards, enable fail-fast,
broaden permissions, permit PR Pages deployment and unbound retries. Source
mutation controls remove actual integrity/inventory/script/seal/canonical calls.
All must reject. Positive controls run the actual corrected three tests on main.

## Qualification protocol and publication invariants

Exploratory focused tests, twelve unchanged CI-001R2 controls and ten new controls passed; corrected inventory
discovery remains 650. These are not a claim that the final full campaign passed.
Before protected qualification, all task-owned source and this report are frozen,
staged and hashed externally. No tracked source/report edits may overlap the
campaign. Final execution outcomes and committed/frozen tree equality belong in
external evidence and the PR handoff rather than edits invalidating a running
source seal.

Required qualification retains complete POL-002 source/epoch suites, all existing
source-integrity controls, all seven repository gates, native 434/434, Ada and
formatting, Rust and Python, real canonical nine-program/13-case execution, the
ordered immutable historical/candidate epochs and independently regenerated
sites. The current 142 scripts and all six enhanced shards must reconcile exactly
once to 650 successful identities; no filtering/skips/errors can authorize a
local-not-deployable aggregate. All input/source/artifact seals remain required.

Independent pristine/enhanced outputs must have identical paths/bytes and match
the unchanged 40-file fingerprint
`2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`
and 43-file fingerprint
`2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
Warnings, coverage, native anchors, renderer exceptions, enhancer configuration,
publication restoration and `REVIEWED_EPOCHS` remain unchanged.

## Integration and review boundaries

PR #133 overlaps `epoch_ci.py` and `pol002_epoch_tests.py`; its workflow remains
untouched here. An isolated read-only compatibility control ran all three
corrected verifiers against its proposed workflow/epoch files successfully. This
does not approve CI-003, its `queue: max` proposal, actionlint blocker or concurrency
policy. After this correction merges, CI-003 must normally merge reviewed main,
retain the exact correction and independently requalify source/artifacts/tests.

PR #134 overlaps neither of the two corrected inherited test files nor the new
verifiers; it must carry the merged correction and requalify complete current and
historical inventories. Its `feature_source.py` and source-test changes are not
integrated or approved here. PR #129 and the DOC-PERF-002 frozen candidate likewise
gain no source/content/publication approval from this correction.

Only after all mandatory local gates pass may a normal commit/push and one
non-draft PR occur. No amend, rebase, force-push, approval, merge, deployment,
tag or release is authorized. Pending hosted jobs are pending, not successful.
Rollback requires a reviewed revert; never weaken security assertions or change
publication fingerprints to force qualification.

## CI-001R3-R1: restore original CI-002 rejection precedence

Continue the same branch, base and eight-file candidate; the previous staged
tree `6a38df914e0ee746e0caa13abe57544cebdcc53a` and qualification snapshot
`e237770463ae24594de2ae50949313962b98877a` remain historical evidence. R1 logs
are fresh under `/home/zboll/git/ams-mel-ci-001r3-evidence/R1`; no prior logs or
snapshots are overwritten. Fetched main remains the starting commit/tree; PRs
#133/#134 remain open and are not incorporated.

All four inherited CI-002 inventory negative controls independently reproduced
`FileNotFoundError`: the candidate called `inherited_sources()` before the
original CI-002 admission block. Those intentionally minimal fixtures have only
the control module, so unrelated protected-source reads preempted their intended
`ValueError` rejections. The immutable starting-main call graph first checks
CI-002 presence, complete source hash, 71 static identities, original 40-identity
preservation and exactly-once ownership, then historical reconciliation.

R1 moves only the new import/restoration call below that unchanged CI-002 block
and above the existing historical 579-test reconciliation. The qualifier's exact
successor SHA-256 is updated in the transition above. There is no catch, missing
source skip, root-based exemption, manifest fallback or reduced integrity check.
A complete valid CI-002 fixture with missing protected source still fails; the
committed source-policy route also rejects the missing-file delta.

The unchanged controls now pass with their original expected errors:

| CI-002 method | Expected observed rejection |
| --- | --- |
| `test_inventory_mutated_source` | `ValueError`, `control source changed` |
| `test_inventory_missing_identity` | `ValueError`, `missing or duplicated` |
| `test_inventory_duplicate_identity` | `ValueError`, `missing or duplicated` |
| `test_inventory_source_required` | `ValueError`, `control source missing` |

Five additive `CI001R3OrderingTests` in the existing standalone epoch module
exercise committed minimal/complete CI-002 fixtures, missing live/control source,
changed protected source, unknown/duplicated inventory, dirty/index mismatches
and original historical transition controls. The three corrected workflow-test
methods, verifier, inherited CI-002 bytes and original ten corrective controls
are unchanged.

Before the new official freeze, focused qualification passed: all four cases,
all 71 CI-002 tests, 12 existing CI-001R2 fixture controls, three workflow tests,
all ten existing plus five new correction controls, complete 41-test POL-002
source suite and complete 66-test epoch suite including real candidate execution.
The fresh staged tree/file hashes and official results are recorded externally.
Earlier partial qualification cannot qualify the new tree. All source/report
bytes freeze before the full current 650-identity, historical/candidate epoch,
seven-language-gate and Bootstrap campaigns. Submission remains conditional on
all mandatory gates and exact frozen/committed-tree parity.
