# DOC-CI-004 — Historical live-shard failure diagnostics

## R2: preserve repeated hosted incident and exact integration blockers

The supplied identities were fetched and verified: main
`219b00c75210f5985d9889e8b541da2e7b658f04`, PR #136
`c41b6920d5c22365a51c9febfa1baa238dbeef55`, and PR #137
`e7eaba2cdf61a51f51f5a12d27e246c1041a1ce4`. The existing PR #136
worktree contained the prior stopped, staged normal merge with the expected
`MERGE_HEAD`. Its index, staged patch, merge message, tree and stop evidence
were preserved before continuation; no reset, rebase or replacement PR was used.

Run 40807408658, live job 123246118996, independently demonstrates three facts:
the authenticated historical `documentation_ci.py run --owner live` exited 1;
its nested log/report was not uploaded; and the separate canonical upload failed
because outer `results/live.json` was absent. The available preparation archive
does not contain that nested failure report. The precise failed historical test,
operation or exception remains **unknown**. Available logs/artifacts were freshly
downloaded and hashed, alongside the earlier run 40622165438 incident. No test
retry, timeout change, omission, historical source mutation or speculative live
test repair is proposed.

The prior explicit workflow conflict resolution keeps the exact Node 24 pins,
success-only canonical artifact and separate always-run diagnostic artifact.
The coverage conflict retains both entries. Auto-merged epoch/tests preserve
both source-policy routes: the original `ACTIONS_SOURCE` and exact diagnostic
successor. Feature allowlists, protected inputs and both false approval flags
are unchanged. Historical workflow pairs and original predecessor hashes remain
authority, never a candidate-owned approval registry. The diagnostic transition
now explicitly rejects a simultaneous publication-registry change.

Diagnostic hardening rejects unknown owners, input/log symlinks and existing
log outputs before child execution. Failure destinations cannot overlap protected
source, sealed input ancestors or canonical outputs; the documented private
`.doc-tools/failure-diagnostics` default remains supported. Real failing children
retain raw report bytes, full logs and original exit codes even when diagnostics
cannot be written. Bounded excerpts escape controls and cannot emit unprefixed
hosted workflow commands. New committed-Git controls exercise complete/incomplete
source transitions, modes, unknown bytes, action SHA/role changes, hidden-file
settings, canonical identity and diagnostic aggregate substitution. Complete
diagnostic capture never turns a failed required job into success.

Two independent qualification dependencies are retained, not waived:

1. This diagnostic-only branch retains main's three known obsolete workflow
   assertions. They still fail as two assertions and one lookup error. PR #137's
   exact inherited-test correction is not copied into this branch.
2. A disposable, unsubmitted combination with exact PR #137 merges textually,
   but all three corrected tests reject the added diagnostic upload because
   `ci001r3_workflow.jobs()` requires the old enhanced-job action-role sequence.
   Its exact shard command also lacks `--diagnostics`. The exact verifier hash
   is protected by PR #137's source transition; locally broadening it would not
   constitute reviewed integration. PR #137's branch/worktree remains untouched.

Evidence is external under `/home/zboll/git/ams-doc-ci-004-r2-evidence`.
Applicable source/epoch, historical shard, language, Bootstrap and publication
qualification must identify a fresh frozen tree. A failed current inventory or
failed exact compatibility snapshot is not green qualification. Delivery must
wait for independently reconciled source-policy/workflow authority; no merge,
approval or functional restoration is claimed here.

## R1 integration with merged Node 24 main

PR #136 remains the delivery PR on `corrective/doc-ci-004-live-shard-failure`.
Original head `c41b6920d5c22365a51c9febfa1baa238dbeef55`, tree
`851898cd1237bbf4c73346a96d2b0bbc9e9b8b0f`; verified fetched main
`219b00c75210f5985d9889e8b541da2e7b658f04`, tree
`4d5c6bb29cfbe6ac39041808e36af34cb84a8c0e`, incorporates PR #135.
A normal uncommitted merge in `/home/zboll/git/ams-mel-doc-ci-004r1` reported
conflicts only in `.github/workflows/documentation.yml` and `docs/coverage.md`.
The upload conflict keeps success-only canonical upload and the exact reviewed
Node 24 SHA for both canonical and diagnostic uploads. Both coverage entries
remain. The auto-merged epoch runner/tests were reconciled semantically too.

All five reviewed action references and Pages `include-hidden-files: true` remain.
Original `ACTIONS_SOURCE`, historical workflow pairs, predecessor checks and
negative-control identities remain. `DIAGNOSTIC_SOURCE` proposes a separate
complete-file transition from the PR #135 bytes; `DIAGNOSTIC_WORKFLOWS` adds only
the complete Node 24 plus diagnostic pair. This is **not independent approval**.
The predecessor/successor SHA-256 identities are:

| File | Predecessor | Proposed successor |
| --- | --- | --- |
| `.github/workflows/ci.yml` (unchanged) | `de38dd1e155bd1cf7d256995f6c8bded61e2fe000f1e733d3dad8a8f255fe235` | `de38dd1e155bd1cf7d256995f6c8bded61e2fe000f1e733d3dad8a8f255fe235` |
| `.github/workflows/documentation.yml` | `cb7ea4aa28d20bec2976af8d430fbff84d00bdc69f19b1dd13885f41022cc7d3` | `1b4be1e4cb526d58021a8aac25154faf8f8ee3e2172a98931f359d720983c711` |
| `scripts/check_doc_source_integrity.py` | `e60abd114b984ee3d1f1ce6905c74045e3f1d8ee75da0ba2ac1141f40c726ef9` | `3e8d60e64765321153b2e1d7683d0ee0e2c8cdbe3e9860835778884a7922f602` |
| `docs/tools/test_actions_dependencies.py` | `4312b37b2d54056e83b2970ed84ec1d50e96d4ca4c41a2c273d2fd66b3d5eabc` | `208caa1df12fbd197424145a87b2612e69c57e04338518ae43d9cf187fcee012` |

New controls cover exact successor content, partial transitions, unknown bytes,
mode changes, unexpected existing diagnostic directories, actual child class
setup/teardown errors and zero-exit truncated/incomplete callbacks. No original
test identity is deleted. Frozen historical 579 and current 650 inventories
remain separate from additive policy controls; observed results never redefine
the expected inventory. The three previously recorded current-650 workflow
expectation failures below remain failures if encountered in R1 qualification.

Current-main run [40791264968](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/40791264968)
also failed historical live (`123193787809`) and aggregate (`123200061874`),
with the other five shards passing and deploy/verify-live skipped. No retained
nested unittest error establishes a cause in either run. The original incident
predates the migration, so neither incident establishes a Node regression.

R1 evidence is separate at `/home/zboll/git/ams-doc-ci-004r1-evidence-20261029`.
Both `/home/zboll/git/ams-doc-ci-004-evidence-20261028/resume` and
`/home/zboll/doc-ci-004-evidence/original-run-40622165438` remain untouched.
Qualification must use a frozen candidate in a separate local-only clone;
its synthetic identity is not a delivered PR head. Stop before delivery commit
or push if any mandatory final-candidate gate fails. At authoring time local
qualification and independent review remain pending: **CORRECTIVE_WORK_REQUIRED**.
The independent incident disposition remains **BLOCKED_DIAGNOSTICS**.

## Incident and disposition

**ROOT_CAUSE_NOT_YET_ESTABLISHED.** Diagnostics repair is not a correction or
retrospective qualification of the original main failure.

Original main: `e4e756aafa4edaffdb11fe341bb016380bf19ca9`, tree
`41274f8df25f1f3cfd75879778ba760cb84a19e0`.
Documentation run: [40622165438](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/40622165438),
attempt 1; failed `enhanced-tests (live)` job:
[122613498184](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/40622165438/job/122613498184).
Preparation and the other five shards passed; aggregate correctly failed;
deployment and live verification were skipped. Bootstrap run `40622165188`
passed its eight jobs at the same revision, without qualifying Documentation.

The failed run was not rerun, cancelled, deleted, or replaced. Available complete
run logs, individual failed-job log/metadata, four run/job/artifact inventories,
all retained artifacts, archive member inventories, exact main source archive,
reachable Git history bundle, and SHA-256 inventory are preserved externally in
`/home/zboll/git/ams-doc-ci-004-evidence-20261028/original`.

The retained job traceback establishes that `epoch_ci.py::run_shard` invoked
historical `documentation_ci.py run --owner live` and received exit status 1.
Its checked subprocess raised before the outer `live.json` could be written.
The subsequent canonical artifact upload failed because the file was absent.
No failed-run live-result artifact exists in the retained artifact inventory.
The preparation diagnostics and sealed inputs were produced before shard
execution and do not contain that shard's later log or execution report.
Therefore the original individual failing test and its exception are unavailable
from these retained artifacts. Do not infer timeout, transport failure, fixture
interference, or runner contention from the outer exception.

## Historical comparison, not qualification

| Evidence | Result | Live execution duration |
| --- | --- | --- |
| Earlier main `7d5f88164bacfdc40add4b92c74cfaefbefad0a6`, run `40607063331` | 72 passed | 830.204 s (inner report) |
| PR #131 head `87d39c4197641d16f460b4bb5d839020409944ba`, run `40612684247` | 72 passed | 487.232 s (inner report) |
| Failed main run `40622165438` | inner exit 1 | approximately 792 s (outer execution step) |

All three retained qualified inputs have identical complete historical pristine
(40 files) and published (43 files) hash maps. Both POL-002 runs explicitly bind
the historical source commit `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`; this is not the later optimized
candidate source or DOC-PERF-001 fixtures. Failed-main historical artifact seal:

* Same-run outer input artifact ID: `12789881460`.
* Historical bundle SHA-256:
  `fb04b34ed8a8a244d307ac931a0df2b931a0f13d9eb3ae304a310f370448664b`.
* Historical manifest SHA-256:
  `85187543a42c337be909886c9f4bbaff98176ead8834e3ad60a30a6bfb130e1a`.

The failed live job ran on Ubuntu 24.04.5, runner image `20261025.353.1`,
runner `2.338.0`, in Azure eastus2. Documentation shards overlapped each other;
four Bootstrap native jobs overlapped part of live execution (ending between
03:31:55Z and 03:35:43Z). Cross-job host contention cannot be established from
these timestamps: they do not establish physical host sharing or CPU/memory
pressure. No scheduling changes are included; PR #133 owns CI-003 concurrency.

## Narrow fail-closed diagnostic correction (not a test-failure fix)

`epoch_ci.py run` now accepts `--diagnostics` (default
`.doc-tools/failure-diagnostics`, outside the sealed epoch directory). When the
historical unpack or run subprocess fails, it:

1. Prints the original command, shard, and exit status, plus at most the final
   80 log lines from a 12,000-byte tail. Control characters are escaped and
   lines prefixed so historical output cannot issue hosted workflow commands.
2. Copies the complete available combined stdout/stderr to `historical.log`,
   the unpack output to `unpack.log`, and any already-written inner report
   **byte-for-byte** to `execution.json`, even when malformed or unsuccessful.
3. Writes `failure.json` with source/head/tree, historical source/tree,
   run/attempt/repository, shard, failing command/status, outer artifact identity,
   outer and historical seal hashes, epoch digest, and hashes of copied evidence.
4. Exits with the original nonzero subprocess status. Diagnostic-writing errors
   are reported but do not substitute a successful or different exit status.

The canonical report is written only after successful inner execution, explicit
inner report validation, independent candidate reconstruction, and the unchanged
sealed-input revalidation. Existing inner/canonical result files are rejected
before execution. No retries or extra historical executions are introduced.

The workflow uploads canonical results only on success. A separate
`documentation-failure-diagnostics-SHARD-RUN-ATTEMPT-HEAD` artifact is uploaded
with `if: always()` from `$RUNNER_TEMP/failure-diagnostics/SHARD/`. No diagnostic
is required on successful jobs; absent evidence warns rather than changing
qualification. Diagnostic paths reject symlinks, preexisting destinations and
overlap with sealed inputs. Diagnostics never enter the sealed payload or the
`documentation-result-*` aggregate download pattern. Missing, truncated,
malformed, or success-looking diagnostics cannot replace a canonical shard
report, satisfy failed dependencies, or confer deployment/publication authority.

No historical protected test, publication fingerprint, integrity checker,
production API, binding, vendor source, scheduling policy, timeout, or assertion
was changed. POL-002 candidate-policy review and historical execution remain
distinct; post-merge independent publication approval remains mandatory.

## Qualification and delivery status

Additive POL-002 controls execute actual deliberately failing child unittest
processes through the orchestration path. They check original status 23,
failure identity/callback retention, complete logs and hashes, bounded excerpts,
missing/truncated/malformed reports, diagnostic-write errors, symlinks,
sealed-input separation, stale reports, zero-exit unsuccessful inner reports,
six-shard rejection, and independent workflow upload/deployment topology.
Workflow inspection is not a claim that a hosted negative-control artifact has
already uploaded successfully.

Local reproduction and full qualification results must be recorded before
delivery. A local run using the original source and qualified artifact but a
different OS/Python is comparison evidence, not an exact hosted-environment
reconstruction. An ordinary passing repetition does not prove an intermittent
failure fixed. No underlying test fix is justified without a demonstrated cause.

The original failed main remains unqualified. Exact-head Documentation and
Bootstrap checks are required on the corrective PR; leave it unmerged with
auto-merge disabled. After independent approval and merge, require a new complete
main Documentation qualification before declaring the incident resolved.

## Resumed sole-owner investigation

The duplicate session's complete handoff and proposal at
`/home/zboll/doc-ci-004-evidence/HANDOFF.md` and
`/home/zboll/doc-ci-004-evidence/uncommitted-diagnostic-proposal.patch` were read
and compared before integration. Both evidence checksum inventories and each
original artifact's authenticated GitHub SHA-256 digest verified. The abandoned
worktree and its failed/interrupted evidence remain untouched. Only complementary
current-workflow and setup/teardown callback controls were incorporated into the
existing additive POL-002 suite; the alternate diagnostics implementation was
not blindly applied. All original policy test methods remain present.

The duplicate session completed a local-not-deployable 142-script qualification,
72-test historical live replay and 187-test remaining shard with exact callback
sets. Its two fresh pristine/enhanced generations had complete identical bytes
and the original approved fingerprints. A separate original-input replay in this
investigation also passed all 72 historical live tests in 693.453 seconds. None
of these successes identifies the original failing historical test or corrects
that failure. The DOC-PERF-001 historical fixture-order issue remains a possible
failure class, not established incident causation; no historical helper or
assertion is changed here.

The current-main campaign retained independently at
`/home/zboll/git/ams-mel-pol-002a-evidence/current-650-execution-summary.json`
records 647 successes, two failures and one error. The three controls refer to
old `documentation_ci.py` workflow command strings and old live artifact paths.
Those assertions remain immutable historical contract authority. The immutable
historical workflow still contains those commands, and its workflow controls
passed in the local historical replay. Current POL-002 successor controls
separately inspect main-only publication, read-only preparation/verification,
same-run artifact/digest identity, all six shards, aggregate dependency rejection,
and aggregate-authorized deployment. Existing event/source and independently
reviewed epoch controls continue to test runtime authority. This distinction
does not turn the failed 650-test campaign into success or explain hosted live.

The disposition remains **BLOCKED_DIAGNOSTICS** unless original nested evidence
or a causally established reproduction identifies the incident. This proposal
repairs loss of failure evidence only. No restoration, merge-readiness,
publication approval or retrospective hosted qualification is claimed.
