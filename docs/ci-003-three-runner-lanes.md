# CI-003: three shared main-branch runner lanes

## Scope and disposition

Baseline: `e4e756aafa4edaffdb11fe341bb016380bf19ca9` (current main when
implementation began). Branch: `ci/003-three-runner-lanes`. No production,
binding, provider, native-test, Ada, publication-fingerprint, branch-protection,
or required-check-name changes are authorized by this task.

Disposition: **NOT READY — local and hosted qualification incomplete**.
Mechanical exact-content acceptance is not independent review approval.
Never merge until the exact final head passes every local and hosted gate and
receives independent review. Keep one non-draft PR with auto-merge disabled.

## Lane assignment

Both workflows use repository-wide `main-runner-lane-1`,
`main-runner-lane-2`, and `main-runner-lane-3` groups only when
`github.ref == 'refs/heads/main'` and the event is not `pull_request`.
Every runner-consuming matrix instance has exactly one group:

| Lane | Bootstrap | Documentation |
| --- | --- | --- |
| 1 | native (gcc, Debug), native (gcc, Release), rust | enhanced-tests (navigation), enhanced-tests (symbols-catalog), build |
| 2 | native (clang, Debug), native (clang, Release), python | enhanced-tests (publication), enhanced-tests (types), deploy |
| 3 | build-tree-isolation, ada | prepare-documentation, enhanced-tests (live), enhanced-tests (remaining), verify-live |

One running job per shared group implies at most three simultaneously running
jobs across these two workflows, including overlapping main runs and attempts
that contain this policy. There is no runner-side waiting step or external lock.
Jobs from older revisions without these lanes are outside this invariant; drain
old main runs before measuring the rollout. This does not cap unrelated workflows.

PR and non-main jobs instead use `unlimited-<run_id>-<run_attempt>-<job>` plus
every matrix axis. They do not share groups with each other or with main.
No job IDs, display names, steps, dependencies, conditions, permissions,
environments, timeouts, matrix axes, or event filters change. All eight Bootstrap
instances and all ten Documentation instances remain present. The four native
configurations still select FULL_STRESS on main and retain `until-fail:50`.

Job-level policy is always `queue: max`, `cancel-in-progress: false`.
Documentation's existing workflow-level group and `cancel-in-progress: true`
remain byte-identical. Superseding a Documentation workflow can therefore still
cancel it; the limiter itself does not cancel running work or replace pending
jobs. Existing main-only deployment/live conditions and all fail-closed gates
remain unchanged; PR deployment/live skips are intentional preexisting behavior.

GitHub documents **100 pending jobs per group** for `queue: max`; overflow is
canceled. This is not an unbounded lossless queue. Hosted qualification must
show no overflow/cancellation/skip attributable to this limiter. A requirement
for no cancellation under unlimited backlog cannot be met with these GitHub
primitives. Do not claim otherwise or disable existing workflow cancellation.

## Source-integrity authorization

`CI003_WORKFLOWS` pins predecessor and successor SHA-256 for both complete
workflow files. Both index and working bytes, regular nonexecutable mode,
stage zero, parity, and atomic application are checked. Bootstrap's immutable
DOC-001 foundation and CI-002 predecessor remain admitted; the superseded
HEAD-only CI-002 proposal remains rejected. No directory exemption is added.

POL-002 requires one narrow exception because its policy-only path previously
forbade Bootstrap and integrity-checker changes. `CI003_SOURCE` binds exact old
and new identities for both workflows, the checker and the new independent
control module. Arbitrary mixed production changes remain forbidden. Historical
inventory, assertion sources, publication authority and epoch fingerprints are
unchanged. The new controls join the sealed integration inventory, not the frozen
historical shards. Unknown bytes, partial transitions, mismatched views, missing
files, executable/symlink modes and unmerged entries have negative controls.

## Local qualification

Use the reviewed enhancer interpreter for tests requiring its installed package:

```sh
cd /home/zboll/git/ams-mel
/tmp/ci-runner001-tools/actionlint .github/workflows/ci.yml .github/workflows/documentation.yml
.doc-tools/enhancer/bin/python -m unittest discover -s scripts -p 'test_*doc*.py' -v
.doc-tools/enhancer/bin/python -m unittest discover -s docs/tests -p test_bootstrap_ci_policy.py -v
.doc-tools/enhancer/bin/python -m unittest discover -s docs/tools -p 'test_*.py' -v
GNATDOC_PREFIX="$PWD/.doc-tools/gnatdoc-26.0.0" alr -n -s "$PWD/.doc-tools/settings" -C docs/toolchain exec -- sh -c 'cd ../..; .doc-tools/enhancer/bin/python docs/tests/pol002_source_tests.py -v'
GNATDOC_PREFIX="$PWD/.doc-tools/gnatdoc-26.0.0" alr -n -s "$PWD/.doc-tools/settings" -C docs/toolchain exec -- sh -c 'cd ../..; .doc-tools/enhancer/bin/python docs/tests/pol002_epoch_tests.py -v'
python3 scripts/check_doc_source_integrity.py
git diff --check
```

Evidence directory for this local attempt: `/tmp/ci003-evidence` (external,
not an archive or hosted authority). Initial checks passed 71 Bootstrap controls,
142 script controls and eight IR counter controls. A first system-Python attempt
failed because the reviewed enhancer package was not installed there; rerunning
with the documented interpreter passed. Do not count the failed attempt as green.
The final targeted CI-003 integration run passed all 13 controls; the complete
POL-002 source suite passed all 41 controls. The staged source-integrity gate
passed with 1,162 protected files and pending-independent-review workflow state.
Initial POL-002 attempts used the system GNAT and were rejected; use the pinned
Alire environment above. Its historical source-gate fixture also exposed a
missing historical workflow pair, corrected by pinning the exact frozen pair
instead of bypassing its validation. The standalone IRMEL transition tests needed
their existing immutable fixture commit `47630e61d2631effbbbe3e222e998e33daa90f60`
fetched from origin. No fixture/test assertion was replaced.
An additional epoch retry hit inherited `GNAT_NATIVE_ALIRE_PREFIX` from the outer
Alire process when a fixture selected its own isolated settings. The subsequent
retry uses pinned GNAT/GPRbuild binary directories on `PATH`, without importing
outer Alire environment variables. A retry is not passing evidence until complete.

Running `test_documentation_ci.py` directly against current POL-002 workflows
reports the preexisting obsolete workflow-shape assertion expecting
`documentation_ci.py deployment` (current workflow uses `epoch_ci.py deployment`).
The production qualifier runs these frozen controls in its separate historical
checkout; do not weaken their assertions to make a current-checkout run green.

**Lint blocker:** latest published actionlint 1.7.12 rejects all ten `queue: max`
keys as unknown. The GitHub syntax documentation explicitly supports this key.
No lint suppression, patched linter, or removal of the required key is used to
manufacture a passing gate. Full actionlint qualification remains unresolved.

## Hosted qualification and timing protocol

Retain the exact run IDs, attempts, SHAs, event payloads, complete job/step lists,
logs, stress audit outputs, artifact IDs and aggregate/publication gates. Verify
each original check name and success conclusion; count four native instances,
eight Bootstrap instances, six shards and ten main Documentation instances.
Any missing, skipped, canceled, neutral, or unsuccessful required job fails
qualification. PR deploy/live skips must match the unchanged baseline conditions.

Collect paired runs with the existing measurement tool:

```sh
python3 docs/tools/measure_documentation_ci.py RUN_ID --output /tmp/ci003-evidence/hosted-RUN_ID.json
```

The last completed successful main pair collected at implementation start was
commit `7d5f88164bacfdc40add4b92c74cfaefbefad0a6`:

| Workflow / run | Elapsed seconds | Summed runner seconds | Summed job queue seconds |
| --- | ---: | ---: | ---: |
| Bootstrap / 40607063244 | 726 | 2653 | 18 |
| Documentation / 40607063331 | 2659 | 4544 | 827 |

These use GitHub job timestamps, not an assertion of isolated scheduler delays.
This pair predates subsequent performance/policy merges and is provisional:
prefer a completed successful pair on the exact immediate pre-CI-003 baseline
when available. Do not attribute unrelated workload/runtime changes to lanes.
Candidate timings and a measured delta are **not yet available**.

To measure the ceiling, collect **all** overlapping main runs of both workflows,
including every attempt, not merely two selected successful runs. Confirm their
workflow revisions contain the lanes. Sweep job execution intervals
`[started_at, completed_at)`, processing completions before starts at equal
timestamps. Reject incomplete timestamps or unsuccessful required jobs rather
than treating absent intervals as zero. Record maximum combined active count and
per-lane count; require at most three combined and at most one per lane.
Include overlapping runs to demonstrate groups are shared across runs/workflows.
Preserve raw API responses for independent replay; account for timestamp precision.

PR qualification exercises only the deliberately unthrottled branch. It cannot
demonstrate main group contention, Pages deployment or live verification.
Do not change refs/conditions or deploy unmerged PR content to claim that proof.
Resolve how to obtain approved pre-merge main-semantic hosted evidence with the
independent reviewer; actual main push/deployment qualification remains mandatory.
If this conflicts with the pre-merge requirement, report the blocker rather than
merging to create evidence or labeling the candidate READY prematurely.
