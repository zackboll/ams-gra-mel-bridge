# IRMEL-POL-001 — exact IR MEL audit source transition

## Scope, status and immutable sources

This corrective policy proposal admits **only** the five-file IRMEL-ADA-001 audit
unit below. It does not implement that feature on this branch, authorize future
production edits, or establish independent approval through successful tests.
Independent source-policy review and protected-branch requirements remain
authoritative. No merge, self-approval or auto-merge is authorized.

Verified starting main (including merged PR #128):

- Commit: `33720d155d99f183229b6ed87037ecbd53a3875b`.
- Tree: `76d7988ea723a07ced054c0275e80763cbdcb772`.
- Candidate PR #129 commit: `47630e61d2631effbbbe3e222e998e33daa90f60`.
- Candidate tree: `6b16f462a5072c22b52f13f7b59fa13280f3e252`.
- Immutable DOC-001 foundation: `42648a92eb0cc84b2980d16db6028992c0494ba1`.
- PR #127 remains at `cec2a98ee90cbf532c66ddf8c1ff13e86b6b6ae0`.

Work is isolated in `/home/zboll/git/ams-mel-irmel-pol-001`; original checkouts,
published feature branches and retained earlier evidence are not changed.
New raw evidence is at `/tmp/irmel-pol-001-evidence`. Git objects, not that
directory or a mutable documentation manifest, supply the source identities.

DOC-001 froze source inventory, executable Ada tokens and production bytes to
prove that documentation work did not change library behavior or weaken its
tests. That remains necessary: passing a renderer, a coverage inventory or a
native test cannot itself authorize changed source. Legitimate later IR MEL
implementation nevertheless needs controlled successors; silently broadening
the documentation allowlist or replacing the historical foundation would erase
the very evidence this gate protects.

## Reproduced blocker

On unchanged main, `python3 scripts/check_doc_source_integrity.py` exits zero:
1,162 protected foundation files; approved POL-001 pair; IR-CI-004 counter
successor; existing Instrumentation and Navigation states.

On a detached exact PR #129 fixture the unchanged candidate checker exits one:

```text
ValueError: unauthorized tracked files added: ['scripts/check_ir_api_coverage.py', 'scripts/freeze_ir_api_evidence.py', 'scripts/test_ir_api_coverage.py']
```

The checker fails fast. In that disposable fixture only, removing those three
index entries exposes the next diagnostic, also exit one:

```text
ValueError: production bytes changed: Makefile
```

Restoring only Makefile to main's original bytes exposes the final diagnostic:

```text
ValueError: production bytes changed: scripts/test_ada.sh
```

All candidate files/index entries were restored afterward. Complete tracebacks
are preserved in `control-b.log`, `control-b-makefile.log` and
`control-b-test-ada.log`; `control-a.log` preserves the main control. These
diagnostic reductions do not redefine the exact candidate or waive its failure.

## Five-file authorization unit

Every blob and mode was checked with `git ls-tree` on both immutable commits;
each file was read directly with `git show COMMIT:PATH`. SHA-256 and the Git blob
hash (including its `blob SIZE` header) were independently computed from those
complete bytes. These tables are explanatory evidence, **not** an input to the
checker and not a candidate-editable authority.

| Exact path | Mode | Original main blob | PR #129 candidate blob |
| --- | --- | --- | --- |
| `Makefile` | `100644` | `e4c3441a32169c6474dedef00a2db1f798375970` | `7b608a960241c87f88819dbe2cd6bbe42cdbdebd` |
| `scripts/test_ada.sh` | `100755` | `d32e162093a94ba77ab482cb53e98d91e31ee29a` | `9d35abb1345aa8b6a71a9d7b1229f19c16d33239` |
| `scripts/check_ir_api_coverage.py` | `100644` | absent | `6775d2e9986d3b59233e7c464745a1482d047f81` |
| `scripts/freeze_ir_api_evidence.py` | `100644` | absent | `48af395041dd3babb1a1df378e61643064ff91b0` |
| `scripts/test_ir_api_coverage.py` | `100644` | absent | `fa96e0ceb5c16b911fdb0acdfec15b1b53ca981a` |

| Exact path | Whole original file SHA-256 | Whole candidate file SHA-256 |
| --- | --- | --- |
| `Makefile` | `7cf0c004e277af70766685f91663c1acde5980fcd1037b42c1092e1c310a5236` | `446ed30875726cc7dd45e2ca59a48254fed65e5036c0de1542dd5d3c38803ba8` |
| `scripts/test_ada.sh` | `b3d19397440873dbf427fe77663083dc99ab5f13e14cd937e0a250fafb09af66` | `21ec1d4545f0ef2e6dbb0af110bfa9083b1e102e9b2f32ee6b1e9dedd5ee9acf` |
| `scripts/check_ir_api_coverage.py` | absent | `6a23700ea3eaa7de8ed7f57d5dcd43d38fc7c5cdd64f67bbf9ad7494202e9e31` |
| `scripts/freeze_ir_api_evidence.py` | absent | `a8f371013211ffaba2f0723388b50acb79f24b8c4abe5e1748b42a6dc38abc9e` |
| `scripts/test_ir_api_coverage.py` | absent | `96ff71c641673f36ea90a9c1a7707e2eae367000c0e9a16dd3bd4bc1d621b333` |

Rationale per member:

- Makefile exposes the offline audit command and its regressions.
- The Ada test driver invokes the same audit before compiling/running Ada.
- The checker validates pinned upstream declaration and implementation evidence,
  including truthful missing/partial classifications; it adds no bindings.
- The freeze helper is an explicit offline maintainer operation, never an
  ordinary build action, and cannot approve replacement anchors.
- The regression module tests the audit's evidence and classification failures.

The drivers, audit, evidence-freezing helper and regressions are one indivisible
unit: admitting only entrypoints could invoke absent/unverified tools; admitting
only scripts would omit their intended qualification path. Source-integrity
admission is deliberately distinct from review of the inventory's claims.

## Mechanical state machine and preserved boundaries

`IRMEL_POL_001_TRANSITION` in the existing checker has two complete states:

- **ORIGINAL:** both original entrypoints, no new audit script in the index or
  worktree. The main/foundation identities must remain original.
- **CANDIDATE:** all five exact complete PR #129 files with the recorded modes.

No intermediate state exists: all 30 other original/candidate combinations
fail. Both states require exact paths, stage-0 regular index entries, regular
worktree files, no symlink file/parent, correct executable modes, complete
SHA-256/blob identities and identical index/worktree bytes. Missing scripts,
unknown content, unmerged entries and gitlinks fail before admission. Additional
tracked scripts still fail the original inventory rule. A documentation JSON
file cannot replace hashes. CLI foundation choices remain fixed; the new unit
also rejects a substituted foundation argument when called programmatically.

Historical minimal fixtures without either entrypoint retain their original
inventory behavior; this is not a way to admit the five paths to an unrelated
foundation. The original test module is not edited.

The existing POL-001 atomic RF pair, IR-CI-001 original/corrected singleton,
IR-CI-002 Instrumentation pair, IR-CI-003 Navigation singleton and IR-CI-004 exact
successor remain intact. DOC-001 documentation exceptions and Ada token policy
are unchanged. No `scripts/`, `ada/`, native, provider, RF, Python or Rust
directory permission is added. No production C ABI exports, public Ada sources,
mock providers, vendor files or historical assertions change on this branch.

The independent controls live in `docs/tools/test_irmel_source_transition.py`,
outside the frozen discovery patterns. Actual isolated Git fixtures exercise
the two complete states, every partial combination, content/index/path/mode
failures, unrelated source changes, forged documentation claims and immutable
authority replacement. Documentation assertions under `docs/tests/` are owned
by the unchanged documentation identity/AST qualifier, not by an invented
blanket source-byte rule for docs. That real qualifier is used to reject their
mutation. Normal discovery and execution must still reconcile exactly **579**
identities (142 script-side, 437 enhanced, six existing shards).

Explicit standalone execution is required. Adding it to hosted execution needs
a separately reviewed CI transition; do not insert it into the frozen 579-test
inventory or change the workflow as a shortcut.

## Repeatable future IR MEL transition procedure

1. Identify a small task and its actual coverage gap. Fetch/verify main's commit,
   tree and ancestry, preserve prior merged transitions, and use clean isolated
   policy/task fixtures. Record original failures before changing policy.
2. Read the entire immutable task diff. Record each exact path, old/new complete
   blob, SHA-256, mode, ownership/ABI effects and rationale. Verify original
   identities against historical authorities, not a manifest supplied by that
   same unreviewed task. Stop on unresolved upstream semantics or extra scope.
3. Propose a separately named narrow successor in a **separate policy PR**.
   Preserve original versus successor identities and every older authority;
   never reset DOC-001 to current main, a feature commit or a synthetic merge.
   Specify atomic groups and reject duplicate, overlapping, contradictory and
   mixed transitions. Exact-content review is required again for new bytes.
4. For Ada declarations, explicitly review executable token changes, safe public
   API design and private FFI ownership; do not relabel declarations as comments.
   For native C ABI additions, follow the C11 ABI policy with C translation-unit
   and matching Ada tests, export reconciliation, ownership/error/timeout and
   metadata fidelity checks. For mock-provider changes, distinguish test-only
   behavior from production, preserve assertions, and qualify the whole dependent
   group before considering a real provider. No blanket directory permissions.
5. Add independent negative controls outside frozen inventories. Demonstrate
   original/exact successor admission, all partial-state rejection, mutation,
   mode/index/symlink rejection, historical regressions and inability to replace
   source authority. Run unchanged documentation tests and inventory checks.
   A renderer profile or published-source fingerprint may require its **own**
   independent documentation qualification transition for new declarations;
   source admission alone never overrides publication validators or inventories.
6. Qualify the policy and an isolated three-way integration with the feature.
   Run native/Ada/Alire and normal documentation gates without inventing missing
   functionality to make an inventory pass. Preserve failures/unavailable gates;
   report exact commits/trees and exact-head hosted results separately.
7. Independent reviewers assess source policy and task semantics. Branch
   protection remains binding. After reviewed policy merge, reconcile the task
   through ordinary merges preserving both histories, reverify all exact bytes,
   and run mandatory hosted CI on the final task HEAD. Do not amend, force-push,
   self-approve or automatically merge another PR to manufacture authorization.

Status vocabulary:

| State | Meaning |
| --- | --- |
| Proposed | Exact successor and rationale submitted; not independently authorized. |
| Mechanically admissible | Exact bytes/index satisfy a proposed checker; no review or merge authority implied. |
| Reviewed | Independent source-policy review accepted the proposal; hosted/branch gates still apply. |
| Merged | Protected integration completed; subsequent feature reconciliation still needs exact-head qualification. |

A generalized manifest-based approval service would change the historical trust
model. It is **not implemented here**. Any future design needs separate policy
review of independent immutable authority provenance, ancestry, conflicts,
revocation, discovery and hosted execution. A feature-owned JSON manifest is
never sufficient authority.

## PR #129 reconciliation and qualification protocol

PR #129 predates PR #128. A raw tip-to-tip diff shows missing PR #128 files and
old native test/checker bytes; applying that snapshot would be wrong. A real
three-way synthetic integration of main and PR #129 has tree
`ad76baea1557ebfcb13afd0f4004aac254052326`, preserves PR #128, and adds only
the five protected files plus the feature's documentation changes. It does not
modify either PR. The new checker is exercised on clean main, exact PR #129
source and that integration in isolated fixtures. It must not authorize any
additional production change.

The preliminary synthetic audit passes 24/24 regressions and
`make check-ir-api-coverage`: 127 headers, 148 operations, **34 missing**, 13
partial, 69 provider-only, 32 safe Ada, zero native-only. These truthful gaps do
not prevent the audit from passing and do not authorize implementations.

Mandatory local sequence:

```sh
python3 scripts/check_doc_source_integrity.py
python3 -m unittest discover -s scripts -p 'test_doc_source_integrity.py' -v
python3 docs/tools/test_irmel_source_transition.py
make test-native
make test-ada
make check
alr -C ada build
alr -C ada/tests run
```

Also execute the historical standalone IR-CI-004 controls, reconcile discovery
and identity, run supported pinned documentation generation/examples/enhancement
and all documentation shards explicitly, verify production ELF/version-map
exports, unchanged protected sources and whitespace/final newlines. Retain
complete logs, commands, exit statuses and rejection evidence outside the source
tree. Initial development failures are evidence, not passed gates; fixes require
rerunning qualification without weakening assertions.

Final measured qualification, feature commit/tree, PR URL and hosted run status
belong in the corrective PR handoff, avoiding an evidence-only commit that
invalidates exact-head CI. Local success is not hosted success or independent
authorization. Leave auto-merge disabled and request independent source-policy
review. PR #127/#129 remain unchanged by this task. Only after this policy is
independently reviewed and merged should PR #129 receive an ordinary merge of
updated main and fresh exact-head source/audit/native/Ada/documentation CI.

## IRMEL-POL-001-R1 — integration with merged CI-002

The historical measurements above describe the original proposal, not the
current inventory. R1 fetch verified main
`7d5f88164bacfdc40add4b92c74cfaefbefad0a6`, tree
`2ce582703250815109cc203c393df5460068e455`, and unchanged PR #130/#129 heads
`e2ca271b6a0f0716f09157349e1ca6954faa4f5d` /
`47630e61d2631effbbbe3e222e998e33daa90f60`. Main incorporates both #127 and
#128. A normal merge into the existing policy branch preserves both histories.
The checker conflict consists of adjacent independently added constants,
validators and result fields: resolution retains both implementations in full.
Bootstrap's exact original/successor workflow states and IRMEL's exact
ORIGINAL/CANDIDATE five-file states are independent; neither is an exemption
for any other source. All previously reviewed blob, SHA-256, mode and foundation
authorities remain unchanged.

The 24 standalone IRMEL controls remain outside frozen documentation discovery.
Their isolated fixtures now start at the verified CI-002 main and reconcile its
71 additive control identities with the original 579: **650 = 142 script-side
+ 508 enhanced**. The original main/tree remains a separate immutable source
authority for the five audit files. Original and candidate controls exercise
both Bootstrap workflow states. No historical test, inventory authority,
Documentation workflow, shard assignment, sealing or deployment gate changes.

Complete original hosted logs were retrieved through the authenticated job-log
API and preserved at `/tmp/irmel-pol-001-r1-20261027`, alongside the successful
push run's complete log archive. The CLI `gh run view --log` initially returned
empty output; those attempts are retained, not treated as evidence of success.

* Documentation run `40446029063`, job `121997408052`, exits 1 on
  `unauthorized tracked files added: ['scripts/check_ir_api_coverage.py',
  'scripts/freeze_ir_api_evidence.py', 'scripts/test_ir_api_coverage.py']`.
* Bootstrap run `40446029045`, GCC Debug job `121997407811`, fails CTest #428,
  `ir_track_system_response_contract`, on repetition **22 of 50** after the
  initial full-suite pass and 21 successful stress repetitions. The diagnostic
  is `test_ir_track_system_response.c:513: strstr(log, "library_unloaded") != NULL`,
  propagated at line 887 by `test_pending_response_lifetime() == EXIT_SUCCESS`.
  It is not the malformed-frame counter observation. PR #128 changes the IR
  stream counter test, not this Track lifetime test, and cannot be represented
  as fixing this separate observed failure. The root cause is not established
  by the log; no race claim or source/assertion change is justified here.
* Successful push run `40445977646` tested the same PR #129 head. Its GCC Debug
  job passed the suite and all 50 Track repetitions. That success does not
  explain the PR failure or qualify a future integration head.

R1 local execution, negative controls, synthetic integration identities and
exact-head hosted status are recorded in the existing PR #130 handoff. Local
documentation evidence is explicitly non-deployable. PR #129 is not modified
while policy integration remains unmerged. Any Track correction outside the
approved source scope needs independent policy authorization; IRMEL-ADA-002
does not begin as part of this reconciliation.
