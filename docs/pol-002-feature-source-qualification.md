# POL-002 — Feature-source qualification and hosted epochs

## R3 — Hosted Alire correction and explicit publication approval

R3 preserves PR #131's R2 commit `db91999832279c4baef66d6af7df0c311f3c4a7e`
and all earlier evidence. The exact failed Documentation run is `40456564783`,
attempt 1, at that head; its diagnostic artifact `12712779086` has verified ZIP
SHA-256 `de220dd9e93e0673d9f8cab6bdc0645399c90408aa506c23abbbf0b05054134d`.
The artifact shows 34 tests executed before representative class setup failed
at the actual `make format-ada`: nested Alire prompted “Press Enter to continue
or Ctrl-C to abort”, then raised `a-tigeli.adb:96`. Documentation preparation
failed, enhanced jobs were skipped, and aggregate build failed. Both exact-head
Bootstrap runs passed all eight jobs; that does not qualify Documentation.
Original artifacts/logs are preserved in `ams-mel-pol-002-build/r3`.

Fetched main advanced to `7d5f88164bacfdc40add4b92c74cfaefbefad0a6`, tree
`2ce582703250815109cc203c393df5460068e455`. It was incorporated by a normal
merge, preserving R2. The independently merged CI-002 tests/tooling remain
unchanged. Historical qualification still runs its exact 142+437 inventory;
the additional current-main CI-002 controls execute separately, without
rewriting or reducing either inherited set.

### Noninteractive toolchain boundary

The outer `alr -s SETTINGS exec` does not propagate its `-s` option to nested
Alire. The workflow now sets explicit task-owned `ALIRE_SETTINGS_DIR`.
`qualified_alire_environment` preselects GNAT 16.1.0 and GPRbuild 26.0.1 with
`alr -n`, disables the selection assistant in those settings, and verifies both
selected versions. All subprocess stdin is EOF. The representative fixture
uses a fresh settings namespace and actual unchanged `make format-ada`; its
default selection cannot come from the user's unrelated global configuration.
Only an explicit task settings cache/toolchain installation may be shared.
Formatter scripts, warnings, sources, dependency pins and build contracts are
unchanged. A regression checks fresh nested formatter execution, selection,
settings isolation and absence of interactive prompts.

### Mechanical qualification is not publication approval

R2's hosted main aggregate could set `deployable=True` while `REVIEWED_EPOCHS`
was empty. R3 closes that discrepancy: aggregation computes publication
authority independently from trusted Git, records it separately, and sets
deployable only for hosted main with that authority. Deployment recomputes the
same decision; a forged passing gate or manifest approval field cannot confer
permission. PRs remain read-only and never deploy. Local evidence remains
nondeployable. The `live` verifier also rejects an unapproved source epoch.

Two authority cases are enforced:

1. **Exact historical preservation.** Complete production/binding/vendor/test
   inputs must equal one of the literal reviewed historical source trees, and
   both complete pristine/publication fingerprints must equal DOC-012's fixed
   identities. Policy-only main merges can continue to publish that unchanged
   reviewed documentation. The case label alone is not authority.
2. **Reviewed feature epoch.** `REVIEWED_EPOCHS`, read as a literal tuple from
   the event's trusted target Git object, must name the actual post-merge
   feature commit and exact tree, full production-source fingerprint, and both
   complete pristine/publication fingerprints. That approved merge must be an
   ancestor of target and candidate; candidate production and complete outputs
   must match. A PR-head SHA—even with the same tree—is not interchangeable
   with the post-merge identity. The current registry remains empty; no future
   034F26 code or hash is approved.

Because main has no configured branch-protection/rulesets, a normal main push
is **not** treated as enforceable independent review. No settings were changed.
The literal policy registry requires separate independent policy review, not a
candidate manifest. Like all repository-executed policy, this assumes review
of policy/workflow changes and trusted Git; it is not an external signature
service or adversarial sandbox and does not claim to enforce its own immutability.

A new feature's first post-merge main push may pass mechanical qualification
but must not publish while unreviewed. After the actual merge exists and its
output is independently reviewed, a separate policy-only approval records its
exact commit/tree and output identities. That approval is itself reviewed and
merged; it cannot approve its own publication using newly supplied candidate
registry bytes. A subsequent qualified main event reads the already reviewed
target policy and can publish only matching approved source/output. This
two-stage procedure avoids circular/future hashes and requires no protection
settings change. PR evaluation never requires invented future feature hashes.

Positive/negative committed-Git fixtures cover historical policy preservation,
reviewed post-merge feature publication, unreviewed hosted-main publication,
PR-head/merge substitution, wrong merge trees, changed artifacts, candidate
registry self-approval, and manifest-based approval. Full replacement local
and exact-final-head hosted outcomes are recorded below/external handoff only
after execution; no queued run is described as successful.

R3 complete replacement local qualification passed on unpublished snapshot
`e422895bb50d9ea3fde6d861991ead0d0cf13149`, tree
`1d8068abcb2a576526829518654a666a3fca8d3e`, against current main
`7d5f88164bacfdc40add4b92c74cfaefbefad0a6`:

- Historical scripts **142/142** and enhanced **437/437**, all six isolated
  shards and unchanged exact identities; full byte-identical publication.
- Original policy **41/41** and expanded integration **51/51** (153 exact
  start/success/stop callbacks), including actual fresh nested formatting.
- Native **434/434**; Ada, aggregate/check, formatting and both Alire gates;
  Python **145**, Rust **98 + four doctests**, and **13 cases / nine examples**.
- Inherited IR-CI-004 **8** and current-main CI-002 **71** additional controls.
- Source/native candidate regeneration, independent reconstruction, sealed
  preparation/unpack, all six candidate-bound reports and aggregate: pass.
- Genuine passing local/PR aggregate offered for deployment: rejected; final
  aggregate records `deployable=false`, `publication_authority=null`.
- Vendor **804/804**, **1,145** production/binding/test/integration paths and
  the separate 034F26 report remain unchanged.

R3 seal bundle SHA-256 is
`d95e848ec49184310ba7cf31a35256e2400380a96ba70650de423560ab5b1eaa`;
manifest digest is
`6a695e9980725abf8e970f715e3c10150b23afa3d2aad3a9496a8385ddccb793`.
Evidence is retained under `ams-mel-pol-002-build/r3`; final delivery identities
and exact-head hosted results are recorded externally after normal commit/push.
One initial local snapshot launcher inherited a stale cloned `origin/main` ref;
the policy correctly rejected the extra CI-002 delta. The ref/event base were
corrected before the full replacement campaign. No failed evidence was deleted
or assertions weakened.

## R2 completed local qualification — independent review required

The complete local R2 chain passed on exact unpublished snapshot
`1d58b97383697be0db34dace2e92bfaa30c4b90c`, tree
`8c947402fed236cdf4700c947ab9abe7cda10884`, against target main
`33720d155d99f183229b6ed87037ecbd53a3875b`. The delivery commit/tree and
hosted results are recorded externally after commit/push; they cannot be
self-referentially embedded in their own source bytes. The sections below are
retained chronological development evidence, including all failures.

| Completed R2 gate | Actual result |
|---|---|
| Historical source authority and full deterministic publication | Original exact SHA/tree and fixed complete pristine/published fingerprints preserved |
| Historical scripts | 142/142 |
| Historical enhanced | 437/437, all six shards, no missing/duplicate/failed/skipped identities |
| Original POL-002 controls | 41/41, 123 start/success/stop callbacks |
| Additive epoch/event/artifact/representative controls | 39/39, 117 callbacks, 326.461 seconds |
| `make test-native` | 434/434 |
| `make test-ada`, `make check`, `make check-ada-format` | Passed sequentially |
| `alr -n -C ada build`, `alr -n -C ada/tests run` | Passed |
| `make test-python` | 145/145; warning-as-error compilation passed |
| `make test-rust` | 98 unit/integration tests plus four doctests passed |
| Canonical examples | 13 executed cases across nine compiled programs |
| Inherited IR-CI-004 source-policy successor controls | 8/8 |
| Candidate source/native/presentation regeneration | Two independent exports plus independent reconstruction pass |
| Actual outer seal/unpack, six consumers and aggregate | Passed; candidate/source/tree/run/attempt/artifact bound |
| Real passing PR/local gate offered for deployment | Correctly rejected |
| Source/vendor/historical evidence preservation | 1,145 production/binding/test/integration paths unchanged; vendor 804/804; all R1 evidence hashes retained |

Completed source-bound local seal:

- Bundle SHA-256: `122b15ba53b30eaa43c262408bc243c647b4cdc283a8144a5a2ed50bdcdd71b8`.
- Manifest digest: `406d38064b2d407b5878493d5ef41badfb3341473f7a72f491c5b68b735defd6`.
- Aggregate file SHA-256: `36a4e674b25371565cc85fbb03c93e0b939c2b935165f02015d0f216712925c3`.
- Exact epoch/inner and outer reports: `r2/corrected-outer-preparation`,
  `r2/corrected-unpacked`, `r2/corrected-reports`, `r2/corrected-gate.json`.
- Local run/attempt: `22005/1`, explicitly `local-not-deployable`.

The original failed 437-test run remains preserved. A clean historical
replacement plus a second complete six-shard outer campaign both pass, and
all 40 original failure/error traces are reconciled with unchanged passing
parent identities. Real-report negatives reject each shard's missing execution,
wrong artifact, wrong epoch, and failed/skipped/cancelled prerequisites.
Real authenticated seal negatives reject changed run/attempt and privileged
event contexts.

One R2 end-to-end attempt exposed an actual ownership bug: generation mutated
the caller-owned Git source classification before outer sealing. Authenticated
unpack rejected the bundle. The corrected implementation deep-copies that
record, tests nonmutation through actual full representative generation, and
has now passed fresh full preparation, seal, unpack and aggregate. Failed
bundles and logs are retained; none is retrospectively qualified. A separate
launcher attempt selected a nonfresh preparation directory and failed before
generation; the corrected launcher used a fresh task-owned destination.

### Subsequent feature and approval procedure

After independent review/merge of POL-002, a separately authorized 034F26 PR
must incorporate current main by normal merge. GitHub event base/head and
fetched refs establish source authority; no future file hashes are required
before implementation. The trusted target's literal scope and tooling evaluate
the complete actual Git delta. The candidate may modify only the reviewed
ProductRx surfaces and cannot modify its own authorizing tools. A policy-only
proposal is a separate review case and cannot mix production changes.

The unchanged POL-001 atomic original/approved pair stays recoverable. The
initial feature may evolve the mock only from the complete approved target
pair, leaving its partner unchanged. Actual resulting mock/shared-interface
bytes and complete deltas remain independent code-review obligations; passing
mechanical tests never approves unknown code. A later mock predecessor outside
that initial approved state requires a separately reviewed explicit successor
transition rather than arbitrary acceptance.

Historical 579-test authority remains in the immutable historical epoch. New
candidate declarations are source/native-derived, not admitted by changing old
fixed counts/ledgers. Target and candidate generate from exact Git trees,
retain unchanged profiles/anchors/sections, and reject new undocumented gaps
or warning expansions. Intentionally edited declaration prose must have actual
changed source blocks and pass the unchanged source/native coverage checks;
unchanged source blocks still require complete byte-exact native sections.
Two generations and source reconstruction qualify new candidate output.

Merged main is the publication-source transition: its exact push event/source
tree is regenerated and fully requalified, using reviewed target tools when
production changed. The same-run aggregate and main-only deployment gate must
pass before Pages upload. No candidate-supplied epoch registry, moving
historical fingerprint, branch label, PR description or editable hash manifest
can approve a transition. No exact future feature hash is preapproved here.
Human independent review and normal merge remain required; the repository has
no configured branch protection/rulesets, and this task does not claim workflow
code can enforce its own immutability against an adversarial policy edit.

GNATdoc and enhancer pins, Bootstrap stress configuration, inherited test
identities/assertions, warning/coverage baselines, and vendor content remain
unchanged. No 034F26 implementation, safe Rust/public Python expansion,
provider behavior, release, manual deployment or merge is part of POL-002.

## R2 continuation — current qualification record

R2 continues the same branch/worktree and preserves all R1 files and evidence.
This continuation section records development checkpoints; the completed local
qualification above supersedes its pending-state observations. Hosted results
and independent review remain separate requirements.

Fetched main advanced to `33720d155d99f183229b6ed87037ecbd53a3875b`, tree
`76d7988ea723a07ced054c0275e80763cbdcb772`, incorporating PR #128's exact
IR-CI-004 correction. The existing policy branch was fast-forwarded normally;
all eight prototype edits were retained. No reset/stash/rebase or new policy
branch was used. Native test changes in that merge are inherited main changes,
not policy implementation. The 034F26 checkout/report/evidence remain unchanged.

Before edits, R2 copied all eight prototype files, the prior handoff, original
failed log and complete evidence checksums to
`/home/zboll/git/ams-mel-pol-002-build/r2-preserved`. All 40 original traces were
classified individually: 38 show the broad pristine fingerprint rejection,
one shows a missing `_site-published` fixture, and one a missing native heading.
Classification is retained in `failure-classification-initial.json`; clean
replacement execution, not this categorization alone, determines remediation.

### Ordered, isolated historical qualification

The historical epoch is exactly `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`,
tree `0073b8627bab4f8409438fdf7102ba5e461fb439`. Its Git source is exported in
a private clone. Its original frozen checker, DOC-001/POL-001/IR authorities,
CI-001R2 inventory and publication verifier remain byte-identical.

Writers run in strict sequence: source verification; complete native pristine
generation; complete enhancement; independent publication verification;
second independent source/native/enhancement generation; complete byte
comparison; 142 script tests; immutable inner seal. Only then can any enhanced
shard consume those inputs. No regeneration runs in a consuming shard tree.
Each hosted shard gets its own historical clone and verified unpacked seal.
The six shard assignments and 437 inherited identities are unchanged.

R2 clean historical replacement completed: scripts **142/142**, Navigation
**34/34**, Publication **35/35**, Live **72/72**, Symbols/Catalog **57/57**,
Types **52/52**, Remaining **187/187**. Complete actual callback reconciliation
establishes **437/437** enhanced identities, exactly once, all six owners, no
failure/error/skip. `clean-historical-gate.json` is a successful explicitly
local/nondeployable aggregate. Every original failure/error trace maps to an
unchanged parent identity that now passes; per-trace evidence is retained in
`failure-classification-confirmed.json`. The original failed campaign remains
unchanged and is not retroactively marked passed.

### Event-derived candidate and policy authority

`epoch_ci.py` derives repository, target SHA, candidate SHA, source tree, run and
attempt from GitHub's read-only event payload and actual Git objects. PRs must
target this repository's main and the event head must equal the fetched
`refs/pull/NUMBER/head`. The base must be an ancestor on fetched main, and an
ancestor of candidate HEAD. Unsynchronized integration fails rather than
guessing a merge source. Push qualification binds `before/after`, GITHUB_SHA
and `refs/heads/main`. `pull_request_target` and arbitrary dispatch contexts
are rejected. Local context is explicitly nondeployable and forbidden inside
hosted Actions.

The source delta comes from Git trees. A policy-only delta may alter the exact
policy/tooling files but cannot contain production/vendor changes or edits to
historical verifier/test/baseline authority. A feature delta must leave the
policy tools unchanged and uses an exported trusted target implementation.
Policy proposals are mechanically evaluated in their separate review case;
this cannot approve source changes mixed into the same authorization.
All source/index/working-tree regularity and exact-byte checks remain active.

### Nested hosted artifact and epoch chain

Preparation qualifies both historical and exact candidate epochs. It runs the
41 inherited POL-002 controls, additive event/aggregate/representative controls,
native/Ada/format/Alire/Python/Rust gates, canonical examples and inherited
IR-CI-004 successor controls. Commands sharing build trees are sequential.
Candidate generation uses two independent exported source trees and a further
independent regeneration; it retains unchanged native declarations and anchors,
discovers new source/native declarations, checks complete prose/coverage and
warning identities, all type occurrences, symbols, catalogs and full manifests.
The dependency revision and historical fingerprints are unchanged.

The outer sealed bundle contains the immutable historical inner tar, candidate
pristine/publication maps and epoch record, complete actual policy callback
reports, language command exits/log hashes, and event-bound source/tree/run
identities. No self-consistent output manifest grants source authority.
Verification checks complete inner historical source/context, 142 script
callbacks, inventory and both fixed whole-publication fingerprints.

All six outer shard reports bind the same head/tree, epochs hash, input artifact
ID, bundle/manifest digests and run/attempt. Inner reports bind actual historical
tests, each executed exactly once with start/success/stop evidence. Every shard
also independently reconstructs the candidate publication from source/native
profiles. The aggregate rejects missing/duplicate/failed/skipped/cancelled jobs,
mixed artifacts/attempts/sources, omitted test identities and partial callbacks.
The `build` job remains always evaluated and cannot accept incomplete inputs.

Pages upload/deployment remains merged-main-only, after exact sealed aggregate
verification and independent candidate reconstruction; PRs never deploy.
Independent code/policy approval remains a human responsibility: event or Git
identity, deterministic generation and passing checks are not approval.
GitHub repository protections were not changed by this task.

### Representative positive/negative evidence

The synthetic representative changes a C ABI value declaration, native
ProductRx compile-time use, an executable Ada declaration/body, a mock fixture,
Rust/Python raw types and an independent C11 probe. It is not the ReceiveEvent
feature and does not manufacture provider success. It runs formatting, actual
C11 compilation/execution, all 434 native tests, Ada tests, Rust checking,
Python compilation and candidate source/native publication qualification.
An additional Ada consumer executes the synthetic function at zero and
Natural'Last; it must pass before final representative qualification.

The first fixture failed because a function was inserted after the Ada package
body elaboration `begin`; the fixture boundary was corrected without changing
production sources or formatter rules. The second representative campaign
passed 16 controls (467.992 seconds), before later event/Ada-consumer additions.
The expanded representative/event campaign passed **25 controls** (509.797
seconds), including actual execution of the Ada consumer. Twenty-eight focused
event/aggregate/archive/workflow controls also passed. Source-backed intentional
edits to existing Ada declaration prose passed three focused controls while
retaining complete historical profiles/anchors and unchanged gaps. Full final campaigns are
recorded separately, not inferred from these development results.

Required remaining R2 validation: complete clean 437-test historical replacement,
full final candidate matrix, end-to-end local outer preparation/unpack/six-shard
aggregate/deployment rejection, exact-head native/language gates, and final
scope/vendor/hygiene checks. Until those complete, there is no commit, push,
PR, hosted policy result or readiness claim.

The final full candidate/outer campaign is evaluated using unpublished committed
snapshots in task-owned clones, not commits on the policy branch. This permits
real exact Git-object/event checks before the requested delivery commit. These
local synthetic event contexts are labeled `local-not-deployable`; their SHAs
must not be reported as final branch or hosted identities. Earlier incomplete
outer-preparation attempts remain preserved separately.

## R1 historical record — incomplete policy proposal, not feature authorization

This policy-only work does **not** implement Task 034F26. It demonstrates
Git-derived scoped candidate evaluation and independently regenerated
documentation for a genuinely changed public Ada declaration. It does **not**
yet migrate the entire hosted qualification/publication chain to new source
epochs. It must not be described as a completed solution, independent approval,
merge-ready policy or authorization to resume 034F26.

The unresolved hosted-epoch integration is a blocker. No feature implementation,
future hashes, new publication epoch or source approval is fabricated. No PR is
to be delivered as successfully qualified while this blocker remains.

## Verified starting state

Fetched main is `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`. Separate worktree:
`/home/zboll/git/ams-mel-pol-002`, branch
`policy/002-feature-source-qualification`, directly from fetched main.
No duplicate policy branch/PR was found at preflight.

The existing 034F26 worktree remains at the same commit. Its untracked report
SHA-256 is `304767fc9ca07460959526d852850d32051318e77f1c8124991c78bb7b227d35`.
Neither that worktree nor `/home/zboll/git/ams-mel-034f26-build` was modified.
This task's evidence is `/home/zboll/git/ams-mel-pol-002-build`.

GitHub main runs `40104689494` (Documentation) and `40104689498` (Bootstrap)
are completed successes at the exact starting SHA. Documentation includes
preparation, all six shards, aggregate build, deployment and live verification.
Bootstrap includes Python, Rust, Ada, build-tree isolation and all four
GCC/Clang Debug/Release jobs. These are historical main results, not this
proposal's hosted qualification. GitHub reports no rulesets and main protection
returns “Branch not protected”; workflow success cannot enforce human review.
No repository protection setting was changed.

Reviewed contracts include DOC-001 foundation, POL-001 atomic RF pair,
IR-CI-001/002/003 corrections, CI-001 and CI-001R2, DOC-010 bookmarks,
DOC-011 examples and DOC-012 catalog/publication. Their immutable authorities,
test sources, declaration/anchor ledger, warnings and renderer exception are
not rewritten.

## Two qualification cases

### Documentation-only preservation

`feature_source.qualify(..., documentation_only=True)` requires the same exact
trusted target/candidate/index/working-tree agreement as feature evaluation and
rejects executable source changes. It does not infer permission from branch
names, labels, environment settings, PR descriptions or an editable manifest.
The existing historical `check_doc_source_integrity.py` remains unchanged and
continues protecting its F25 foundation, all reviewed RF/IR transitions and
1,162 inherited files. Current publication still uses that historical gate.

### Candidate feature-source evaluation

`docs/tools/feature_source.py` computes the complete NUL-delimited tree delta
from Git objects. The committed candidate must be checkout HEAD, its stage-0
index must exactly equal its tree, and each working file must be regular with
the exact committed bytes/mode. Missing files, removals, symlinks including
ancestor aliases, FIFOs, partial staging, unmerged indexes, mode transitions
and nonignored untracked inputs reject. Ignored build products are not source.
Concurrent mutation during checking is outside the guarantee.

The target must be an ancestor of the candidate; synchronize later work by
normal merge, not by resetting a newer main. The CLI accepts only fetched
`origin/main`, verifies historical ancestry/tree and requires its implementation
to equal the target Git blob. The separate trusted-target runner requires its
checkout to match that fetched target. A caller-supplied arbitrary SHA is not
CLI authority. Direct Python APIs accept fixture targets for committed tests;
they are not external attestations or independently approved source epochs.

Exact eligible RF ProductRx surfaces are literal constants, not directories:

- Native ProductRx implementation, existing C11 header/export map, native C
  ProductRx tests and RF mock, CMake, and two explicitly named v10 probes.
- Safe Ada ProductRx spec/body, private raw C bindings, existing ProductRx tests
  and its test-only child.
- Rust sys declarations and existing ABI probes; private Python ctypes and its
  existing ABI test module. No safe Rust/public Python expansion is authorized.
- The F26 qualification record and the explicitly requested coverage,
  architecture, C ABI policy, Ada README and changelog files.

Unrelated IR, RF Job, loader, vendor and unknown native/Ada additions reject.
Policy/checker/workflow changes cannot accompany a feature candidate.
Shared ABI/raw/build surfaces only admit additions retaining all historical
lines; export additions are limited to the two specified v10 names. Complete
shared-file deltas remain independent review input. A path allowlist cannot
prove that arbitrary CMake/mock/binding code belongs to ProductRx: returned
review obligations explicitly identify this limitation. Mechanical evaluation
does not approve unknown code on those surfaces.

The trusted-target runner invokes the complete required native/Ada/Alire,
Python and Rust commands sequentially, records actual exits, then regenerates
documentation. Those executable tests are candidate code and must run without
credentials/Pages permissions in an isolated ordinary runner; the tool is not
a hostile-code sandbox. Bootstrap stress settings and thresholds are unchanged.

## POL-001 compatibility and future mock transition

The original/approved RF pair and all four historical hashes remain unchanged
in the historical checker. A future candidate mock edit is evaluable only when
the target has **both complete approved POL-001 files**, and the partner
`native/tests/test_rf_job_interval_status.c` remains exactly unchanged.
Mixed predecessors, partially staged/dirty content and unknown predecessor
states reject. The result records a pending exact-content mock-review obligation.

Before publication approval, independent review must establish the future
candidate's actual mock blob/hash and complete source delta. This requires
implemented bytes that do not exist yet. This proposal cannot preapprove them.
The future reviewed epoch must retain the historical pair as predecessor
evidence, not replace original/approved acceptance with arbitrary mock bytes.

## Source/publication authority audit

| Consumer | Existing assumption | Proposal handling / remaining work |
|---|---|---|
| `check_doc_source_integrity.py` | Fixed F25 production bytes/tokens and exact RF/IR transitions | Unchanged historical verifier; separate candidate evaluator |
| `check_doc_baseline_delta.py` | No new warning/coverage baseline lines | Unchanged; feature evaluator forbids baseline edits |
| `check_doc_coverage.py` | Source/native profiles, complete members/prose, one renderer exception; fixed ledger at CLI | Reuse `measure` without candidate ledger, then unchanged gap ratchet |
| `build_enhanced_site.py` | Complete source, anchors, pristine, API and site fingerprints | Unchanged historical path; separate candidate generation/restoration |
| `local_type_audit.py` | Source plus pinned native ledger | Optional independently measured ledger; default unchanged |
| `symbol_audit.py` | Complete public source/native ownership, enum literals, ledger destinations | Unchanged algorithm applied to independently measured candidate ledger |
| `api_catalog.py` | Source/example/native identities and parsed HTML | Unchanged algorithm applied to candidate references |
| `documentation_ci.py` | 579 tests, fixed authorities, six shards, same-run artifact seal/aggregate | Unchanged; full new-epoch integration remains blocked |
| `documentation.yml` | Historical generation and source gate; main-only deploy | Adds proposed policy tests before sealing; does not enable feature publication |

## Demonstrated candidate generation

`source_epoch.py` exports fresh source trees from Git objects. The pinned
GNAT 16.1.0/GPRbuild 26.0.1 distribution/GNATdoc 26.0.0 semantically compiles
and generates native public documentation. Candidate-supplied ledgers,
manifests, warning baselines, exceptions and tooling do not establish authority.

The original coverage consumer reconciles all actual public declarations,
profiles/literals, parameters, members and attached prose. New gaps fail the
unchanged ratchet; warning identities use the actual exported source root and
must match the unchanged warning baseline. Every historical declaration keeps
its exact native destination/profile and complete rendered section. Additive
changes cannot replace old pages with incomplete documentation.

The immutable enhancer remains revision
`524b7610e21e9a7b87e8e0761580ed59a3b0295c`, archive SHA-256
`8b7c0a0d0957ef2ebb2cc27888173e653db6987c4279444fcffe095ccc099101`, installed
fingerprint `af5c5fec03ec3fbc41e3a55a55424a3ce65cf1bb5101d97b0c65ff5614e40632`.
No enhancer or GNATdoc upgrade/migration occurs.

Candidate enhancement runs API-only as required by the immutable dependency.
Independent consumer restoration must recover every regenerated native byte.
All eligible type-link occurrences are enumerated from plain native profiles,
not emitted links/counts. Symbols/catalog entries are source/native-derived;
every link/fragment resolves. The complete renderer manifest is reconstructed.
Two separate source exports/native generations/enhancements must reproduce all
pristine and publication bytes and audits exactly. The strong verifier
regenerates again; matching a self-edited HTML/manifest hash list is insufficient.

An actual committed synthetic public ProductRx subtype, documented with an
attached trailing block, qualifies: one new native declaration, 239 symbols,
4,295/4,296 documented elements (the one unchanged renderer omission), and
1,428 preserved type-link occurrences. Old destinations/profiles/complete
sections remain. An undocumented new declaration and forged HTML with an
equally forged manifest fail. This is mechanism evidence, not F26 approval.

## Remaining blocker: full hosted source-epoch transition

The new candidate tools are **not yet wired into** trusted target selection,
historical projection execution, candidate shard inventories, sealed artifacts,
aggregate reports and main publication as one reviewed end-to-end chain.
The present workflow still invokes the old frozen-source generator and gate;
a real F26 candidate will still fail that historical path.

`REVIEWED_EPOCHS` is deliberately empty. No candidate may populate it through
feature authorization. A future separately reviewed policy transition must
bind actual reviewed source/tree, complete delta, mock/shared-file hashes,
native/coverage/warning identities and deterministic publication evidence.
It must preserve independently verifiable historical DOC-012 generation and
the six-shard exact-once artifact/aggregate contracts. Approving unknown future
hashes or treating an output manifest as authority is prohibited.

The initial registry sketch and manual trusted-target runner are not a complete
sustainable publication migration. Further design and qualification are needed
for later documentation-only descendants of an approved feature epoch and for
main merge source/tree identities. Until then, no successful policy/feature
qualification or deployment readiness is claimed.

## Qualification evidence and limitations

Completed unchanged-source gates, all exit 0:

- `make test-native`: 434/434.
- `make test-ada`; `make check`; `make check-ada-format`.
- `alr -n -C ada build`; `alr -n -C ada/tests run`.
- Historical script suite: 142/142.
- Fresh pinned-tool pristine generation: historical complete fingerprint
  `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`.
- Fresh historical enhancement: complete fingerprint
  `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
- Policy campaign including shared-surface controls: 41/41, with 123 exact
  start/success/stop callbacks, including real changed-Ada generation,
  deterministic reconstruction and independent forgery rejection. The final
  recheck after authored HTML/CSS safety checks also passed 41/41 with 123 exact
  callbacks (`pol002-final-report-v2.json`, 126.615 seconds).

All 43 historical publication entities compare byte-for-byte equal across fresh
generations. All 1,145 native/Ada/Rust/Python/integration/Makefile paths compare
byte-for-byte equal to starting main; vendor checksums remain 804/804.

The inherited enhanced campaign completed **437 tests in 1,763.111 seconds,
with 38 failures and 2 errors**. It began before `_site-published` existed;
the catalog forgery test recorded a missing fixture. Later, regeneration of
the shared pristine tree overlapped Navigation tests: one observed a missing
native heading and 38 assertions observed the broad pristine-fingerprint
rejection instead of their intended mutation guard. This was an orchestration
error by this task, not permission to weaken the assertions. This run is failed,
not full historical qualification. The affected focused catalog forgery test
passed once its real publication fixture existed, but no complete clean
437-test replacement campaign was run. Historical inventory and
all inherited test sources/assertions remain unchanged. New policy tests use
separate explicit discovery and actual unittest callbacks; skips/expected
failures/incomplete setup cannot produce a passing policy report. They execute
in preparation before the existing immutable seal; six shard owners and the
aggregate gate remain unchanged.

Initial copied-settings Alire documentation generation failed with “No
index.toml file found in index”; direct generation with the exact installed
pinned tools passed. Earlier candidate attempts exposed an incorrect warning
source root, whole-site rather than API-only enhancer input, and tuple/list
serialization mismatch. These were corrected, with failed logs retained. No
baseline, assertion, warning level or dependency identity was weakened.

No final-head hosted run, PR, push, merge, release or deployment was performed.
No 034F26 source was modified. Source-qualification results are mechanical
evidence only and require independent review. With the hosted integration
blocker unresolved, this work is an incomplete proposal, not deliverable success.

The final fetched main and local branch HEAD remain the starting SHA/tree.
All policy edits remain unstaged/uncommitted. Complete changed-file inventory:

- `.github/workflows/documentation.yml`
- `docs/tools/ada_examples.py`
- `docs/tools/local_type_audit.py`
- `docs/tools/feature_source.py` (new)
- `docs/tools/source_epoch.py` (new)
- `docs/tools/qualify_feature_source.py` (new)
- `docs/tests/pol002_source_tests.py` (new)
- `docs/pol-002-feature-source-qualification.md` (new)

No standalone Python/Rust runtime suite was run in this policy worktree; their
source/ABI/runtime code is unchanged. No final sealed six-shard/aggregate
campaign or canonical-example execution was completed for this proposal.

## Recovery

Preserve the isolated policy worktree and evidence. Review the proposal and
negative controls independently before continuing. Complete trusted-target
hosted epoch selection, historical/candidate projections, identity-bound shard
and artifact integration, all required exact-head suites and deterministic
publication verification before committing/pushing a deliverable policy PR.
Do not resume 034F26 or modify its existing worktree as part of this task.
Normal reviewed reverts can recover the historical-only path; never reset main,
rewrite historical fingerprints or manually deploy to compensate for failure.
