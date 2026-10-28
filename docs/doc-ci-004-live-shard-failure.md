# DOC-CI-004 — Historical live-shard failure diagnostics

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
