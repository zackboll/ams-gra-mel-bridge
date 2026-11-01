# CI-001R3-R5 — Historical transition scope (proposal)

This is a narrowly scoped source-policy correction. It is not an independent
approval, a publication epoch, or a change to any historical successor.

## Defect

`epoch_ci.classify` called two one-time historical validators for every
candidate:

- `ci001r4_replay.source_transition()` (CI-001R3-R4 replay successor), and
- `doc_ci004_source.diagnostic_transition()` (DOC-CI-004 integration successor).

Each validator is entered whenever any member of its `SUCCESSOR` set differs
from the target. Both sets contain `docs/coverage.md`. Each then requires the
complete target tree to equal one of its fixed historical predecessors
(`219b00c…`/`e85d99b…` and `219b00c…`/`e7eaba2…`). Every later reviewed main
fails that test, so any ordinary later `docs/coverage.md` edit was rejected:

```
CI-001R3-R4 historical replay: not a complete verified predecessor checkout
```

The defect was reproduced on unchanged main `10990c39…` (tree `1a7732f3…`)
with an isolated committed documentation-only fixture changing only
`docs/coverage.md`. The trusted `feature_source.qualify` route classified it
`documentation-only`. Both validators, called directly, and
`epoch_ci.classify` rejected it. The retained evidence is external (see below).

## Correction

`epoch_ci.HISTORICAL_TRANSITION_TREES` names, for each validator, exactly the
complete target trees from which its transition is defined. These are the
validators' own `MAIN_TREE` and `PREDECESSOR_TREE` identities.
`historical_transition_trees()` requires that literal registry to equal both
validators' constants on every classification. A widened, narrowed or
inconsistent scope rejects. `classify` invokes a validator only when the
target tree is in its scope. Otherwise the candidate uses the unchanged
ordinary routes.

It does not change:

- either validator, its `SUCCESSOR` SHA-256 identities, the replay binding or
  any historical predecessor/successor control;
- `POLICY_PATHS`, `PROTECTED_INPUTS`, `feature_source` eligibility, the
  CI-ACTIONS-001 or CI-001R3 routes, source identities, approval flags, the
  `REVIEWED_EPOCHS` registry or publication-epoch requirements; or
- any workflow, production, ABI, vendor, RF or Ada source.

The correction makes the policy-only route stricter in one respect. Outside
the exact historical transitions, a later proposal that changes
`.github/workflows/documentation.yml` or `docs/tools/epoch_ci.py` must still
satisfy the trusted CI-001R3 live, preparation and deployment workflow
contracts. These cover pinned action roles, the success-only canonical upload,
always-run non-authorizing diagnostics and main-only deployment. Previously,
only the historical validators checked them.

The existing `HistoricalReplayTransitionTests` fixture now takes its R4
successor bytes from immutable commit `2be40f9…` rather than the working tree.
The test assertions are unchanged. This keeps the control on the exact
historical transition rather than today's R5 runner.

## Adversarial regression controls

`HistoricalTransitionScopeTests` commits complete Git trees descended from the
real later main `10990c39…` and exercises the trusted classifier:

- the reproduced documentation-only edit classifies `documentation-only`,
  while both unchanged validators still reject it directly;
- an ordinary documentation edit on the real unmodified later main classifies;
- the scope registry must equal each validator's predecessors, including
  under widened, narrowed and validator-constant substitution;
- unrelated executable/production/vendor/script/test/workflow additions and
  candidate approval files reject when combined with documentation;
- the RF_PRODUCT_RX surface stays `feature-source` with its review obligation;
- every protected control, inherited test, qualifier, verifier, checker,
  baseline and Bootstrap workflow change rejects; mode and removal changes
  reject;
- predecessor bytes of individual historical members (workflow, checker,
  tests, qualifier, verifier) mixed with documentation reject, as does a
  reapplied historical R4 successor with a hitchhiking validator edit;
- candidate self-authorization rejects: scope widening with production code,
  validator predecessor rewrites and approval JSON. A candidate
  `REVIEWED_EPOCHS` entry is only policy-only and grants no publication
  authority;
- later policy proposals remain `policy-only`, are never approved and cannot
  carry production changes. Unpinned upload, PR deployment and canonical
  upload on failure reject through the trusted workflow contracts; and
- the exact historical routes remain selected only from their verified
  predecessors, and both validator files remain byte-identical to history.

All controls report `independent_approval=false` and
`publication_epoch_approved=false`.

## Qualification and limits

Qualification of this proposal requires the complete POL-002 source and epoch
suites, every historical control, the mandatory repository gates and
exact-head hosted Documentation and Bootstrap runs. Command logs and results
are external at `/home/zboll/git/ams-mel-ci-001r3-r5-evidence`. They are local
and not deployable. Mechanical acceptance is not independent approval.
DOC-CI-004 (PR #136) remains blocked until this correction is independently
reviewed and merged. It does not establish the root cause of historical
live-shard run `40622165438`.
