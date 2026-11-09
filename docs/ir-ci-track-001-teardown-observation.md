# IR-CI-TRACK-001 — asynchronous Track teardown observation

## Scope and preserved failures

Test-only correction based on main
`fa91028804f38d3b2bd15ec8e5e2f80cd4a74a7e`. No production source, mock provider,
public interface, dependency, workflow, or ownership contract changes.

* PR #173 remains at `7385e5bf45e56b09e3e9bc1869d5eff0bc9bd571`.
  [Bootstrap run 42537752110, Clang Release job 128905734538](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/42537752110/job/128905734538)
  built synthetic merge `46ad0c71156a80c533b021f6f70f7dc9f9176bc8`.
  The initial **472/472** suite passed. The parallel-4 `until-fail:50`
  campaign failed `ir_track_system_response_contract` at original line 635,
  the final `library_unloaded` assertion in `test_mixed_request_accounting`
  (propagated at line 888). The full campaign ended with 471/472 passing.
* [Bootstrap run 40446029045, GCC Debug job 121997407811](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/40446029045/job/121997407811)
  previously failed at original line 513, the immediate `library_unloaded`
  assertion in `test_pending_response_lifetime` (propagated at line 887),
  after an initial suite pass and 21 successful repetitions. This distinct
  failure is retained, not retroactively attributed to the stream-counter fix.

Complete authenticated job-log API downloads are retained outside the source
tree at `/home/zboll/git/ir-ci-track-001-evidence`:

| File | SHA-256 |
| --- | --- |
| `job128905734538-raw.log` | `3aa818d5307ab7e85272bb8b6e792717c3dcb85c19dd3d8d5adb01b0c59cde75` |
| `job121997407811.log` | `2918775dd3dc912e0604a4bebb15d6dc2dddfe8ee152f36e1fbf4b797b97c5fe` |

An empty earlier `gh run view --log-failed` response is not evidence; the
second failure was recovered with `gh api .../actions/jobs/121997407811/logs`.

## Diagnosis

`native/src/ir_track.cpp::complete()` calls `finish_request()` before publishing
its terminal result. Only the final accounted request of a logically closed
Track initiates cleanup. Cleanup disables, detaches, destroys the channel and
drains callbacks before successful publication; failed detach retains the graph
and publishes a failure. These behaviors are not changed.

However, publication is not a worker join. Under `Completion::mutex`, `complete`
sets `kind`, caches the result, and resets `Completion::channel`. It then unlocks
that mutex **before** resetting its local `channel` reference and notifying
waiters. A waiter arriving in this interval can see a terminal result without
waiting for notification. Closing the public request does not join the detached
worker or consume its local reference. `TrackState` retains `SessionState`, whose
provider objects are destroyed before its `SharedLibrary` calls `dlclose`.

The mock writes `library_unloaded\n` from the DSO's `UnloadRecorder` destructor;
`record()` appends through a local `ofstream` whose destructor flushes/closes the
file. Result publication therefore does not order the caller's immediate log
snapshot after this final record. This explains both observed failures without
requiring an accounting failure or changing cleanup semantics.

## Correction and invariants

Only `native/tests/test_ir_track_system_response.c` changes executable behavior.
The pending-response and mixed-request tests reopen the lifetime log until a
complete `library_unloaded\n` record appears, under a five-second absolute
`CLOCK_MONOTONIC` deadline. The equivalent fixed 500 ms sleep in
`test_request_close_is_not_cancel` is replaced by the same observer. Ten-ms
absolute monotonic polling sleeps pace observation; elapsed time is never proof
of completion, and interruption does not restart the deadline.

The reader checks for excess bytes and embedded NUL rather than accepting a
silently truncated C string. Final lifecycle checks use complete line records,
not substrings or unterminated final lines. Timeout prints the actual path and
latest freshly read log. Invalid snapshots fail closed with path and raw bytes.

All original ordering of API calls and release barriers, cached-result fidelity,
mixed-family request accounting, no-premature-unload checks and permanent
fail-safe-retention assertions remain. The final unload requirement is retained.
No positive observation is substituted for a negative ownership assertion.

## Controlled positive and negative experiments

The external `controls.py` harness includes either the exact original C test
from main or the corrected test, renames its `main`, and calls each of the three
lifetime tests individually. Both link the same unchanged facade and mock DSO.
All control C is compiled with C11 and `-Wall -Wextra -Wpedantic -Werror`.

An external `LD_PRELOAD` interposer intercepts `dlclose` **only** for the three
selected lifetime-log prefixes. The delay case waits 750 ms, then calls the real
`dlclose` (and hence the real mock unload logger). The missing case deliberately
does not call it. To deterministically expose the real publication window in the
two wait-based tests, the interposer implements their timed response wait as
repeated calls to the unchanged exported `wait(0)` API, with its own monotonic
deadline. Otherwise the normal condition-variable notification can conceal the
window. Neither the facade nor the provider is patched for these controls.

| Case | Original | Corrected |
| --- | --- | --- |
| Pending response, 750 ms delayed real unload | fails immediately | passes, 0.770 s |
| Mixed accounting, 750 ms delayed real unload | fails immediately | passes, 0.778 s |
| Request close, 750 ms delayed real unload | fails after 500 ms sleep | passes, 0.766 s |
| Pending response, unload suppressed | fails | fails, 5.006 s |
| Mixed accounting, unload suppressed | fails | fails, 5.009 s |
| Request close, unload suppressed | fails | fails, 5.009 s |

Each corrected missing-unload failure includes the path and latest log. Further
compiled-C observer controls accept a complete marker, reject prefixed/suffixed
markers and a missing newline at five seconds, reject NUL and overflow
immediately, and remain pending on `library_unloaded` until a separately appended
newline 750 ms later. This last control proves fresh reads and complete-record
observation. The evidence directory retains the harness, interposer, fixtures,
per-case diagnostics and complete command output; `controls.py` exits zero only
when expected failures actually fail and the elapsed-time bounds hold.

These results establish an observation-ordering defect in the tests, not a
production corrective requirement. They do not claim real-provider qualification.

## Qualification protocol

Four independent CMake trees use GCC 14.2.0 and Clang 19.1.7, each in Debug
and Release, with `AMS_MEL_BUILD_TESTS=ON`. For each tree the required commands
are a warning-as-error build, ordinary `ctest --test-dir BUILD
--output-on-failure`, and `ctest --test-dir BUILD --parallel 4 --repeat
until-fail:50 --output-on-failure`. The existing inventory is 472 tests; no new
CTest registrations, test exclusions or reduced campaigns are introduced.
Rebuild and repeat when the tested source changes; earlier passing runs are not
exact-head evidence for later source.

Additional qualification uses `make test-ada`, `alr -n -C ada build`,
`alr -n -C ada/tests run`, `make check`, `make check-ada-format`,
`make test-python`, `make test-rust`, workspace `cargo check`,
`cargo clippy --workspace --all-targets -- -D warnings`, `cargo fmt --all --
--check`, `make test-policy`, `python3 scripts/check_doc_source_integrity.py`,
`python3 scripts/check_final_newlines.py` and `git diff --check`. Cargo commands
use `--manifest-path rust/Cargo.toml`. The direct GPR invocation uses the
already-installed GNAT 16.1.0 and GPRbuild 26.0.1 binaries on `PATH`; no toolchain
installation, dependency update or Ada source change is needed.

The corrective PR records completed local exit statuses and hosted run/job
identities against its exact head and synthetic merge. This section specifies
the protocol, not a claim that a running or future job passed. Raw local logs,
exit-status files and controlled experiment outputs remain in the evidence
directory above.

## Source policy and handoff

Current main's `scripts/check_doc_source_integrity.py` expressly permits ordinary
first-party development without historical per-file byte authorization. Its
vendor checksums and deployment-security gates remain unchanged. Historical
IRMEL-POL-001 documents describe their own checkpoint, not current admission.
The corrective PR requires independent review; successful tests do not authorize
self-approval, merge or auto-merge.

PR #173 must remain unchanged until this correction is independently reviewed
and merged. Only then reconcile #173 with main using an ordinary merge and
requalify that exact new head. This task does not preempt that sequencing.
