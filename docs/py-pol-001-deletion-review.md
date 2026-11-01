# PR #142 — source-policy deletion review

## Review boundary and disposition

PR #142 is the **single source-policy redesign effort**, including the proposed
POL-003 overlap. Do not create a parallel redesign PR. Leave merging and approval
to independent review; no auto-merge, force push or deployment is authorized.
PR #140's RF Admin implementation remains separate and unchanged. Its exact
blobs appear only in disposable qualification fixtures, not this branch's API.

This audit examines main `52285ce7f47be030b80812e2efa45b2d69c3508f` against
redesign head `6568bf1fe6db0569b3137856eababb2f30d8b0cb` (tree
`dad3168a779927da1aa6f4b24779dc06b0bd22fa`): **29 files, 839 inserted and 6,944
deleted lines**. This follow-up restores coverage, so later diff totals differ.
The original comparison is fixed for audit reproducibility, not development
authorization. No historical revision or prior result is rewritten.

AST comparison found **312 removed named test methods**, two modified methods
and 43 added methods in the original redesign. These are static definitions,
not executed counts: inherited test classes and generated cases differ. A test
rename counts as remove/add. Each removed method's body/assertions was examined;
the machine-readable identity list and assertion extract are retained outside
the source tree. The table below accounts for every affected test file, including
mixed classes where an authorization assertion surrounded a real security test.

## Test deletion accounting

Paths below are relative to the repository root. Replacements mean actual named
tests of current behavior, not a claim that retaining a function proves coverage.

| Original test source | Removed methods | Disposition / retained protection |
| --- | ---: | --- |
| `docs/tests/pol002_epoch_tests.py` | 118 | Exact migration/predecessor/policy-only/registry tests retire. `AggregateTests`, `FailureDiagnosticsTests`, `ArtifactBoundaryTests` and seven event-identity tests remain. This follow-up restores all six `DiagnosticR2FailureControls`, all three `DiagnosticIntegrationFailureTests`, two replay-identity tests, and adds teardown/gate-substitution tests. Action/Pages mutation coverage moves to `scripts/test_doc_source_integrity.py`. Real generation mutations move to `source_epoch_regression_tests.py`. See mixed-class detail below. |
| `docs/tests/pol002_source_tests.py` | 55 | 29 source-scope tests become ordinary multi-language delta and exact checkout tests. Fourteen audit-target allowlist/approval tests retire. Ten generated-epoch tests are not all obsolete: current generation/artifact/coverage replacements are explicit below. Two historical tests are replaced by an immutable checkout executing original tests. Four deterministic replay/source-tampering tests are restored here. |
| `docs/tests/test_bootstrap_ci_policy.py` | 12 | Eight exact Bootstrap migration/foundation tests retire; four frozen-current inventory tests become dynamic/duplicate/empty inventory tests. All 30 synthetic-merge tests remain, including real C execution, parent order/tree reconstruction, wrong event base/head, dirty/shallow/missing-object and execution recheck rejection. |
| `docs/tests/test_documentation_ci.py` | 14 | Twelve reviewed-transition methods include one dependency-failure test, retained as `PrerequisiteTests`; the source-byte/method-count/inherited-inventory gates retire. Two inventory methods become current discovery/unique assignment/new-module tests. Existing 24 reconciliation, eight artifact, four runner, two workflow and three measurement methods remain. Teardown-error coverage is added to the epoch suite. |
| `docs/tools/test_actions_dependencies.py` | 11 | Six exact migration-authorization tests retire. Five workflow tests mixed exact-whole-file comparison with real pin/role/hidden-file checks. `ci001r3_workflow.py` retains those structural checks; restored mutation tests exercise wrong owners/pins/tags, role substitution, Pages hidden files, prerequisites, diagnostics and gate verification. Bootstrap matrix/smoke/stress tests remain in its own suite. |
| `docs/tools/test_ir_counter_source_policy.py` | 8 | Exact three-file successor/original-body restoration tests retire as current admission gates. Native IR counter/observer behavior and existing native/Ada tests are untouched. Historical Git sources/evidence remain; these eight campaign tests have not been replayed as a separate full campaign in this audit. |
| `docs/tools/test_irmel_source_transition.py` | 24 | Exact fixed audit additions/transition blobs, atomic pairs, source freezes and policy flags retire. Vendor/checksum/upstream-object integrity, coverage evidence/symbol validation and 24 IR API audit tests remain. Current implementation-byte drift now passes; upstream/vendor tampering still fails. This is not a whole-provider ABI proof. |
| `docs/tools/test_live_fixture_transition.py` | 9 | Current-to-historical helper/body admission freezes retire. Their substantive fixture selection/type-link/anchor mutations remain in immutable historical tests and the existing explicitly identified corrected replay. Replay construction/tampering regressions are restored; full ordering probes are not claimed equivalent solely from a positive live-fixture run. |
| `scripts/test_doc_source_integrity.py` | 60 | 54 navigation/native/IR/instrumentation transition tests and six blanket first-party-freeze tests retire for current development. Full clean index/worktree/mode identity moves to `feature_source.exact_checkout`; vendor mutations and privileged workflow mutations have positive/negative replacements. The original 60 tests still execute unmodified inside the original immutable historical checkout. |
| `scripts/test_ir_api_coverage.py` | 1 | `test_reviewed_source_change` becomes `test_current_implementation_may_differ_from_historical_baseline`. It no longer demands a historical hash update for implementation edits; other 23 upstream/vendor/evidence/classification tests remain. Original hash evidence is unchanged. |

The counts sum to 312. Two original modifications were the epoch generation
copy-boundary test's stop point (now `toolchain`) and the Bootstrap inventory-owner
test (current explicit shard assignment rather than enhancer-dependent discovery).

### Mixed epoch classes: security versus obsolete authorization

- `ActionDependencyEpochTests` (3), `CI001R3OrderingTests` (5),
  `CI001R3Tests` (10), `DiagnosticSuccessorTests` (3),
  `HistoricalReplayTransitionTests` (5), `HistoricalTransitionScopeTests` (12)
  and most of `DiagnosticIntegrationTests` (8) enforce historical exact-content
  transitions. They are intentionally not requirements on future source edits.
  Their clean-source, workflow and report checks instead map to the current
  source/merge, workflow mutation, callback and artifact suites.
- `CI001R3ActionsIntegrationTests` (7), `DiagnosticR2SourceControls` (4),
  `MainOnlyDocumentationTests` (5) and `CurrentWorkflowSuccessorTests` (4)
  contain real action-role/permission/deployment/teardown assertions alongside
  frozen transitions. Current workflow contracts and restored negative mutations
  cover these properties without authorizing files by historical digest.
  The audit also found that write permissions newly added specifically to
  preparation/aggregate jobs were not explicitly rejected by the replacement
  workflow checker. It now checks every non-deploy documentation job for write
  permissions, with a mutation regression for both jobs. This is a concrete
  security-coverage correction, not a historical-policy exception.
- All nine diagnostics tests in `DiagnosticR2FailureControls` and
  `DiagnosticIntegrationFailureTests` are restored rather than dismissed:
  wrong owners; symlink logs; protected-source/canonical/input-ancestor overlap;
  diagnostic traversal; original child statuses 1/7/63 despite collection
  failures; escaped workflow-command output; success-shaped reports from failed
  children; diagnostics accidentally downloaded into aggregate inputs; and
  changed source/run/artifact identity. Their predecessor-authorization context
  is not restored.
- `HistoricalReplayAggregateTests` (3): the two identity/non-authorizing replay
  tests return. Failed/skipped/incomplete execution is already rejected by
  `AggregateTests`/`FailureDiagnosticsTests` and their restored diagnostics tests.
  Synthetic reports test reconciliation; they do not prove historical execution.
- `HistoricalReplaySourceTests` (8): four replay construction/source-tampering
  tests return. The fixed original/replay source bytes remain immutable objects,
  not today's test-file byte requirements. Original/replay inventory and
  filesystem-order fixture probes require separate historical execution; they
  are not silently counted as covered by current source admission.
- `HistoricalReplayFixtureTests` (9): the underlying original live tests execute
  in the historical run and both problematic tests passed unchanged on the
  observed enumeration. Earlier order-dependent failures/evidence remain valid.
  This positive run is not a replacement for every root-first/fabricated-link
  ordering adversary; replay authenticity tests and source-derived link/anchor
  restoration remain, with full historical completion reported separately.
- `PublicationApprovalTests` (11): reviewed-content registry and an extra approval
  commit are deliberately removed. That **changes the trust model** to successful
  qualified main CI plus independent code review. PR/local publication rejection,
  event SHA matching, gate/source/artifact substitutions and candidate
  reconstruction retain explicit tests (`MainPublicationTests`,
  `PublicationGateTests`, `ArtifactBoundaryTests`). Green synthetic gate tests
  are not real GitHub Pages deployment qualification.
- `RepresentativeFeatureTests` (6) and `AdditiveDocumentationTests` (4): old
  exact feature scopes/shared-interface approvals retire. Current functional and
  ABI tests remain; source changes are not ABI approval. Historical unchanged
  anchor/profile byte preservation was removed, not proven equivalent. Current
  source-derived native/profile/catalog restoration, coverage/warning checks and
  regenerated artifact rejection are retained. Their actual generated positive
  and negative tests are in `source_epoch_regression_tests.py`.
- `EventAuthorityTests` (2 removed): separating policy-only source and rejecting
  mixed policy/vendor deltas is obsolete. Event-bound identity and clean source
  still apply; vendor changes must agree with dependency evidence. Neither a
  candidate checksum file nor candidate test code is external review authority.

## Production code deletion accounting

| Removed/changed code | Meaning and replacement |
| --- | --- |
| `ci001r3_source.py`, `doc_ci004_source.py`, `ci001r4_replay.source_transition`, epoch transition tables/helpers | One-time historical successor admission is removed from current classification. The original commits are unmodified. Replay construction, exact replay source/commit/tree checks and distinct original/replay identities remain. |
| `feature_source.audit_transition`, `shared_surface_delta`, hardcoded RF/safe-Python scope | No file-level permission or historical-line comparison for first-party development. Complete Git/index/worktree identities remain; ABI/behavior validation comes from compilation/tests/review, not filename classification. |
| `documentation_ci.reviewed_authorities`, `source_methods`, `live_fixture_source`, inherited byte/count reconciliation | Today's inventory is discovered from today's tests and sealed to current hashes. Duplicate/discovery/assignment/execution mismatches still fail. Historical comparisons remain only inside immutable historical tooling. |
| `source_epoch.trusted_publication_epochs`, `readonly_qualification_inputs`, `inherited_declarations` | Empty approval ledger and development input/old-section freezes retire. Actual current GNATdoc generation, coverage/warnings, native restoration, theme/router pinning, catalog/type links, deterministic second generation and independent reconstruction remain. Historical destination equality is no longer a source permission rule. |
| Source-integrity fixed transition predicates and blanket `check` loop | First-party files are not frozen. Existing vendor SHA-256 inventory and dependency closure remain, with security-critical workflow structure checked. The Ada tokenizer/profile helper remains because symbol documentation verification uses it. |
| `ci001r3_workflow.authority_contract` and deployment identity wording | Runs event-bound main source after merge rather than exporting the prior main checker. Its AST checks still require source identity, main-only deploy, hosted evidence and exact gate/artifact binding. No current source/test byte authorization is reintroduced. |
| IR coverage implementation-hash loop | Baseline source hashes stay historical evidence. Current evidence/schema/upstream/vendor checks and actual functional tests replace blanket byte equality; arbitrary current semantic correctness cannot be inferred from symbol presence. |

## Protections and qualification

Unchanged native/ABI/Ada/Rust runtime code and PR #140 implementation are
separate from this diff. Native C11/C++20 warning-as-error compilation and
434 tests, Ada ownership tests, Python layout/import/runtime tests and Rust
layout/signature/runtime tests remain. Removing a header-line freeze does not
prove ABI compatibility: there is no newly added general historical binary
`abidiff` gate, nor a demonstrated 32-bit/provider ABI qualification here.

At reviewed head `6568bf1`, hosted Bootstrap
[41270315153](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/41270315153)
completed **success in all nine jobs**, including GCC/Clang Debug/Release stress,
policy, Ada, Rust, Python and build-tree isolation. This does not qualify later
audit follow-up commits. Local evidence for that head: native 434, Python 147,
Rust 122 plus one intentionally ignored child entry, policy 141, documentation
controls 50, IR audit 24, `make check`, Alire build/run, cargo check/clippy/fmt and
duplicate deterministic current-source generation all passed. Exact PR #140
overlay passed 177 functional/ABI/import tests and both normal source CLIs.

The standalone original historical run at immutable `5c136a0` has passed its
own generator/build/verify/inventory, 142 script tests, 34 navigation, 35
publication and 72 live tests. Later shards are pending at this audit snapshot;
**do not call the full historical campaign green until it finishes**. Original
logs, identities and statuses are retained even if a later shard fails. Tests
from later policy campaigns still exist in Git but have not all been executed
in isolated checkouts by this review. Preservation is not a claim of execution.

The audit follow-up restores the security tests above and adds an explicit real
current-source documentation mutation suite: documented/undocumented Ada
additions, source/tree substitution, pristine/publication tampering and forged
self-consistent manifests rejected by independent regeneration. It is run by
the main Documentation preparation and explicitly via
`make test-documentation-source` in the pinned documentation environment.
Its results and the follow-up hosted run are recorded in the PR, not guessed
from the preceding green head.

## Outstanding review risks — not hidden by green tests

1. **Trust-model change:** merged, reviewed main CI replaces per-feature content
   approval. Candidate tests/locks can be changed in the same PR. Repository
   branch protection, required reviews and environment approval settings are
   external controls; this code does not prove they are enabled. Self-tests
   cannot make malicious workflow changes safe without independent review.
2. **No end-to-end new Pages deployment:** structural/negative tests and artifact
   reconstruction retain important protections, but full new orchestration,
   artifact upload/download, aggregate, deployment and live publication have not
   been qualified as one hosted post-merge campaign. No deployment is attempted.
3. **Documentation PR coverage:** Documentation remains main-only; pinned real
   generation/mutation tests are explicit locally and in main preparation, not
   in provider-free Bootstrap. Bootstrap success is not full docs qualification.
4. **Historical runtime/cost:** historical shards are long-running and retain
   original order-sensitive behavior. Their status must stay separate from
   current functional correctness; original failures are diagnostics rather
   than fabricated passing history. The already defined corrected replay must
   never be labeled original execution.
5. **ABI limits:** current multi-language compatibility tests are meaningful but
   are not exhaustive binary compatibility with every old consumer/provider.
   Removing exact content gates increases reliance on review and actual ABI
   contract tests when interfaces evolve.
6. **Import protection is detection:** an added `_native/` package makes the
   runtime import identity test fail; Python's importer is not modified to
   prevent malicious imports. The test must remain enabled in binding CI.
7. **Workflow/dependency assumptions:** existing structural parser intentionally
   supports a restricted workflow form, not arbitrary YAML. Existing unpinned
   Alire/Rust setup actions and documentation tool installer network/dependency
   behavior are not comprehensively hardened by this redesign. Documentation
   action pins, main-only guards and vendor integrity remain checked.
8. **Local fixture setup:** Ada validation rebuilt only three provider targets,
   causing initial missing-factory tests to see a missing RF fixture. A complete
   native build restored it; unchanged Rust/PR140 tests passed. This is recorded
   separately from functional/security defects and obsolete policy failures.

## Evidence and reproduction

Evidence is preserved outside the source tree at
`/home/zboll/git/ams-py-pol-001-r1-evidence/`, including `deletion-audit/`:
the original changed test sources, `test-delta.json`, `removed-assertions.txt`,
restored-control logs and exact command statuses. Prior narrow-policy evidence
remains at `/home/zboll/git/ams-py-pol-001-evidence/` and immutable commit
`32d375a`; it is not replaced or claimed passing by this redesign.

```sh
git diff --numstat 52285ce 6568bf1
git show 52285ce:docs/tests/pol002_epoch_tests.py
make test-policy
python3 scripts/check_doc_source_integrity.py
# Pinned enhancer + GNAT 16.1/GPRbuild 26/GNATdoc 26 environment:
make test-documentation-source DOC_PYTHON=/absolute/pinned/enhancer/bin/python
```

**Disposition: independent review required; leave #142 and #140 unmerged.**
No new policy registry, task campaign or overlapping PR is introduced.
