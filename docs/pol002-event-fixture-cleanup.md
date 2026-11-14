# POL002 corrective task — temporary event repository cleanup

## Scope and evidence

Reported failing main: `e50079ad59ec4b37678f73a4f318d264f3a4639f`,
[Bootstrap run 43546000335](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43546000335),
policy job `132297346510`. Both named negative-event tests reached registered
`TemporaryDirectory.cleanup()` and failed with `Directory not empty: 'source'`.
They did not fail their `assertRaises(ValueError)` authorization assertions.

Previous main `de529f92f36623db8cdd3ccc6f3cdf7da31a1be8` passed
[Bootstrap run 43542909322](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43542909322).
The fixture, `epoch_ci.py`, and Bootstrap workflow are byte-identical between
these commits. Both hosted runs used Git 2.56.1. The corrective branch was
fast-forwarded to newer main `c4490a9ce509512e0de6a79d42a61a12c00a7b9a`
before final qualification; those intervening changes do not touch these files.

## Established mechanism

All explicit clone, checkout, commit, and ref-update subprocess calls are
synchronous. Waiting for `git commit`, however, does not wait for detached
automatic maintenance. Git 2.56.1 process and syscall traces establish:

1. Fixture commit starts `git maintenance run --auto --quiet --detach`.
2. Maintenance forks, its parent exits, and the commit exits successfully.
3. The surviving maintenance process starts
   `git commit-graph write --split --reachable --no-progress`.
4. Python removes `.git` during recursive teardown. The graph writer then
   successfully recreates `.git`, `.git/objects`, `.git/objects/info`, and
   `.git/objects/info/commit-graphs` in the same fixture worktree.
5. Python cannot remove `source`, which again contains `.git`.

An unmodified 2.56.1 run without syscall tracing reproduced **17 cleanup errors
in 20 tests** (ten repetitions of each reported negative event). A separate
traced run reproduced **100/100 errors**. The previous passing main reproduced
**20/20 errors** under the same 2.56.1 experiment. Thus a previous green run
does not disprove this timing-dependent defect. On local Git 2.47.3, the
unmodified focused tests passed 100 traced and 1,000 untraced executions;
an unmodified `make test-policy` also passed. Reproduction required matching
the runner Git version, built outside the repository from the explicit
`git-2.56.1.tar.xz` release. No project build dependency was added.

The original hosted failure has no process trace: its precise scheduling is
not recoverable. The matching locally observed cleanup error and writer are
direct evidence of this mechanism, not a claim to have traced the old runner.

## Minimal correction and regression

The fixture's `git clone --config maintenance.auto=false` writes only the
temporary clone's local configuration before checkout or commit. It prevents
automatic maintenance instead of attempting to outwait a daemon at teardown.
No source-repository/global Git setting, production policy, workflow,
authorization assertion, historical source, replay identity, artifact gate,
or test inventory authority is changed. There are no sleeps, cleanup retries,
ignored errors, skips, or weakened security assertions.

The additive regression checks the local configuration and traces a real
empty commit. It requires a commit trace and rejects any maintenance child
start. Existing real committed-event qualification and all negative-event
tests remain active. The current epoch suite gains one test (64 → 65);
immutable historical inventories and sources are unchanged.

Initial corrected qualification with Git 2.56.1 passed **300/300 tests** in
76.939 seconds: 100 repetitions each of both reported negative-event tests and
the new maintenance regression. An outer Git Trace2 log recorded **zero
maintenance child starts** across these fixtures. Direct execution confirmed
the unchanged rejection reasons:

- `pull_request_target`: `source qualification: unsupported source event`;
- wrong base repository: `source qualification: PR must target trusted main repository`.

Both direct-probe repositories were removed by the unchanged cleanup. Test
runs use dedicated `TMPDIR` directories so leftovers are checked independently
of unittest success. Failed reproductions intentionally left graph-bearing
repositories; their paths and trace evidence were recorded before explicit
investigation-only removal after their writer processes exited. This is not
a fixture retry or a suppressed cleanup error.

## Final-head qualification and review boundary

Exact commands include:

```sh
python3 scripts/check_doc_source_integrity.py
python3 -W error -m unittest discover -s docs/tests -p pol002_epoch_tests.py -v
make test-policy
git diff --check
```

Repeated focused runs construct a `unittest.TestSuite` of
`EventAuthorityTests.test_pull_request_target_rejected`,
`EventAuthorityTests.test_wrong_target_repository_rejected`, and
`EventAuthorityTests.test_fixture_commit_does_not_start_automatic_maintenance`.
Final policy campaigns run against committed HEAD in separate fresh shared
clones, each with a fresh HOME and TMPDIR, `GIT_CONFIG_NOSYSTEM=1`, and
`PYTHONNOUSERSITE=1`; TMPDIR must be empty after each campaign. Qualification
clones themselves are explicitly removed and their absence verified.

The exact final corrective SHA, completed final-head repeated policy results,
and hosted Bootstrap run/check results are recorded in the corrective PR,
not by creating another unqualified documentation-only successor commit.
This document makes no advance claim about those results. The PR must remain
open, non-draft, unmerged, and without auto-merge. Mechanical qualification
does not establish merge readiness or replace independent review.