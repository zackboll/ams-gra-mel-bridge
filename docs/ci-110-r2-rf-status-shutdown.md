# CI-110-R2 — RF interval-status shutdown-count investigation

## Scope and source identity

Starting main: `ddc63bf6883d16403db5fb23cd4a087ff5ebcf5c`.
PR #110 remains documentation-only at
`7852f48f9f045de88e79694c5e90d37514eba297`. Its native, Ada and vendor
trees are byte-identical to starting main. This correction is based on main,
not `docs/007a-developer-portal`. No production source, public header, export
map, Ada source, provider contract or vendor file changes.

Historical failure: [run 38858732245, job 116835959664](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/38858732245/job/116835959664),
Linux hosted Clang 18.1.3 Release. The first CTest execution passed 434/434;
the parallel four-process, 50-repeat execution failed test 55,
`rf-job-interval-status`, at original line 381, `shutdowns()==before+1`.
The preceding Job Close returned `AMS_MEL_PROVIDER_EXCEPTION` as expected.
The historical log contains no owner identities or actual counter values.

## Source-backed counterexample

`job_cleanup_shutdowns` counts **all** mock C2 shutdown calls in one process.
`mock_rf_job_wait_shutdown_after` waits only for that global value to exceed
its baseline, not for a particular owner or completion of graph destruction.
CTest's other processes do not share this counter; parallel execution can
change scheduling within the failing process.

The explicit delayed-Finalize scenario resolves its future and waits for an
increment. Later scenarios also Finalize without joining teardown: the first
status admission-hold case and the `cancelled == 0` `c2:job-ok` case.
The latter has an immediately ready future, but readiness is not worker exit.
`ams_mel_rf_job_close` can consume the public Job while its Finalize worker
still owns `JobState`. Its destructor subsequently drops JobDetail and VA,
then releases the C2 claim on the worker thread.

An isolated, reversible test-only experiment changed the earlier `c2:job-ok`
mock future from immediately ready to explicitly gated, using the existing
pending-Finalize mechanism. It resolved that future after the final scenario's
baseline read and waited for its shutdown. Production sources were unchanged.
This forces a legal late-completion ordering without sleeps or larger timeouts:

| Event | Owner | Thread | Global count | Owner shutdowns |
| --- | --- | --- | --- | --- |
| Final scenario baseline | 59, `c2:finalize-shutdown-throw` | caller | 57 | 0 |
| Earlier Finalize cleanup | 55, `c2:job-ok` | worker | 58 | 1 |
| Selected throwing shutdown | 59 | caller | 59 | 1 |

The original assertion failed (exit 1): actual 59, expected 58. Close returned
the expected exception and the Job pointer was null. Owner 55 was destroyed
after its one shutdown; owner 59 was not destroyed. The earlier explicit
delayed owner 51 had already shut down and been destroyed in this experiment.
This establishes a test counting/synchronization defect, **not** the identity
of the uninstrumented historical hosted interleaving.

## Ownership assessment and correction

`FinalizeInput` owns shared `JobState` through future consumption. The worker
releases its consumed future before publishing terminal state. `JobState`
owns JobDetail, VA and a sibling `RfC2ChildClaim`; observers own only bridge
queue/callback state and a DSO pin. Claim release moves away its parent
reference and selects shutdown under the parent mutex using `shutdown_started`.
An uncertain throwing shutdown retains `RfC2State`, C2 and the DSO permanently.
No source-backed production lifetime counterexample was identified. This is
not blanket qualification of an arbitrary provider or an ABI-incompatible DSO.

The correction gives each mock C2 an immutable creation index, with separate
shutdown/destruction counts protected by the existing cleanup mutex. Records
retain no provider objects. The C test captures that identity immediately after
serialized creation, before creating anything else. It checks zero initially,
then **exactly one** selected shutdown, expected exception status, consumed Job,
stopped observer, late callback safety, idempotent Job Close, no retry after
observer closure, and no selected C2 destruction or DSO unload. An unrelated
C2 shutdown explicitly leaves both selected-owner counts at zero.

The controlled background completion passes with owner-specific assertions.
A reversible negative control recursively calls the **selected** mock C2's
shutdown once more before throwing. The trace reports selected-owner counts
1 then 2, and the new exact-once assertion fails (exit 1). All instrumentation
and duplicate injection are removed from the correction; their patches, source
hashes, binary hashes and logs are retained in the external evidence directory.

## Documentation-gate policy conflict

The existing `scripts/check_doc_source_integrity.py` compares native test bytes
as well as production bytes against fixed historical baseline
`42648a92eb0cc84b2980d16db6028992c0494ba1`. Its workflow runs on every PR.
Consequently this intentional test-only correction fails that documentation
gate with `production bytes changed: native/tests/mock_rf_provider.cpp`.
The baseline, allowlist, workflow and protections are deliberately unchanged.
Resolving that policy conflict requires separate explicit review; passing
native tests is not a claim that this PR is merge-ready.

## Evidence and qualification

Evidence directory: `/home/zboll/git/ams-mel-ci-110-r2-evidence/r1`.
`status.tsv` records commands and actual exit statuses; each command log records
UTC start/end, shell-escaped arguments and complete output. Original, corrected
and instrumented build directories are separate. Mutable build-tree operations
are sequential. `instrumentation.patch`, `corrected-with-instrumentation.patch`
and `negative-control.patch` preserve the experiments.

The unchanged source passed 200 focused repetitions, all 434 tests under
`--parallel 4 --repeat until-fail:50` (21,700 completed executions), and a further
200 focused repetitions pinned to one CPU. There was **no spontaneous local
reproduction**. The controlled failure is reported separately, not relabeled
as an unchanged-source failure. Local Linux is Debian 13, GCC 14.2 and Clang
19.1.7; the hosted Clang 18.1.3 / Ubuntu 24.04 environment is not identical.
No passing rerun is evidence of permanent resolution of an intermittent issue.

Completed native qualification of the final test-only source:

| Compiler | Configuration | Focused repeats | Full suite | Parallel 4, repeats 50 |
| --- | --- | --- | --- | --- |
| GCC 14.2.0 | Debug | 200/200 | 434/434 | 21,700/21,700 |
| GCC 14.2.0 | Release | 200/200 | 434/434 | 21,700/21,700 |
| Clang 19.1.7 | Debug | 200/200 | 434/434 | 21,700/21,700 |
| Clang 19.1.7 | Release | 200/200 | 434/434 | 21,700/21,700 |

All configure/build/test/install commands exited zero; warnings are errors.
Separate strict `-std=c11 -Wall -Wextra -Wpedantic -Werror` syntax checks of
the C ABI test and corrected C test passed with GCC and Clang. All 804 vendor
manifest checksums match. Each native facade has the same 222 versioned
`AMS_MEL_0.1` public exports. The unchanged-source and corrected-source Clang
Release facade binaries have identical SHA-256:
`0bd87526941acada9489607f304754381a3b70508810125dc5fd71c782c914e0`.

Tested source SHA-256 values (also recorded before the matrix):

- `native/tests/mock_rf_provider.cpp`:
  `ef8c1dc59987ea2ff16cb00d6a60880ab560b910127105b6dd7698498be9d957`
- `native/tests/test_rf_job_interval_status.c`:
  `96f71931cbf3c03937d352997666ad459db9f517d514c7827579bbfa22a62930`

Exactly one hosted job retry was requested, using
`gh run rerun 38858732245 --job 116835959664`. Attempt 2's replacement
[Clang Release job 116846250357](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/38858732245/job/116846250357)
completed successfully on unchanged PR #110 head
`7852f48f9f045de88e79694c5e90d37514eba297`: initial 434/434 and parallel
50-repeat suite passed. `pr110-retry-job-log.log` preserves the raw output.
The original failed job remains historical evidence; this passing retry does
not supersede the demonstrated test defect or prove a permanent resolution.

Existing Ada contracts passed through `make test-ada`, `alr -n -C ada build`
and `alr -n -C ada/tests run`. `make check-ada-format` passed; no first-party
Ada source changed. GNATLS reports 16.1.0; the installed Alire
`gprbuild=26.0.1` package's executable reports GPRBUILD 26.0.0 (2026-04-15).
`make test-native` additionally passed 434/434 using the ordinary repository
build scripts. GPR emits its pre-existing source-less C-project warning; no
new compiled-source warning or warning suppression was introduced.

`make check` passed, including production/test build-tree isolation, native
434/434, Ada formatting/contracts, final newlines and Git whitespace checks.
Its complete output is in `make-check.log`. The script reconfigures the ordinary
test tree during its final Ada phase, so a subsequent copy of that tree's prior
`LastTest.log` was unavailable; the command output is preserved, as are all
matrix CTest logs and their separately copied `LastTest.log` files.

The duplicate-shutdown negative control was also repeated against the exact
final C test (including its unrelated-owner check). The selected mock owner's
second invocation caused CTest exit 8 at the new exact-once assertion. See
`final-negative-control.patch`, `final-negative-control-source.sha256`,
`final-negative-binaries.log` and `final-negative-duplicate.log`. The mock-only
injection was removed and the original qualified source hashes restored.
Another 200 focused repetitions and a full 434/434 suite then passed on the
restored final source; the final Ada formatting check also passed.
