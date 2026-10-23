# IR-CI-003 — bounded Navigation completion observation

## Review boundary and identities

**PROPOSED_PENDING_INDEPENDENT_REVIEW.** Mechanical source-policy acceptance,
computed digests and passing tests are not independent authorization or approval.
This proposal follows the existing exact-test transition policy; no broad native
test exemption or new foundation is introduced. Stop at independent review.

Baseline main: `9288f6acd4cbf807f576d387db390459f0b117c0`, tree
`78bc1f5b5087847ff385debc60340b14461c20b7`. Independent worktree:
`/home/zboll/git/ams-mel-ir-ci-003`; branch
`corrective/ir-ci-003-navigation-completion-observation`. Evidence is isolated at
`/home/zboll/git/ams-mel-ir-ci-003-evidence`. CI-001 PR #125, its worktrees,
documentation workflow, shards and evidence are not modified or reused.

Original [DOC-012 Bootstrap run 39546475170](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39546475170),
Clang Release job **119045674822**, remains **failed**. Its exact-main log was
downloaded afresh with SHA-256
`1c917a8e6111c0757363cae37a01a124ffca63ea41a0972230d9bcf12d206473`.
The ordinary 434-test campaign passed, then the parallel-4 until-fail:50 campaign
failed `ir_navigation_contract` at `test_ir_navigation.c:439`:
`strstr(log, "navigation_completed") != NULL`, called by
`test_request_close_while_pending()` at line 1192. Seven other Bootstrap jobs
succeeded. The separate main Documentation build/deploy/verify-live run passed.
Neither main run is rerun, cancelled or reinterpreted by this correction.

## What the evidence establishes, and what it does not

`native/tests/mock_provider.cpp` writes `navigation_sent` during send and starts
an independent provider thread. In `navigation-pending-close`, that thread waits
40 ms on a condition variable, then writes `navigation_completed` and publishes
its future. A timed wait does not guarantee the thread will run by any particular
wall-clock deadline after that wait expires. The event writer uses a local
`std::ofstream`; destruction closes/flushes the stream. It supplies diagnostic
evidence, not a production completion/cancellation contract.

`native/src/ir_navigation.cpp` retains the completion and stream graph while its
adapter-created worker waits on that future. Closing the public request deletes
the handle, **not** the worker or provider graph and **not** the request itself
as a cancellation. After completion, the worker decrements the stream request
obligation, drives deferred cleanup and releases graph ownership. Physical
cleanup is serialized under the lifecycle mutex; provider calls/destruction run
unlocked. The mock channel destructor joins its producer threads before writing
`channel_destroyed`; the unload recorder writes `library_unloaded`. See
`docs/task-027b-validation.md`, its corrective notice,
`docs/corrective-image-navigation-close-race.md`, and
`native/src/internal/ir_stream.hpp` for the actual ownership and synchronization
rules. The earlier production close-race correction is not undone.

The failed observation waited 200 ms, took one log snapshot, then assumed this
entire deferred chain had completed. That is not a completion barrier. The log
does not contain a retained provider-thread trace from the original failing
attempt, so the precise historical scheduling/interleaving cause is **unknown**.
We do not infer host scheduling merely from successful reruns or conceal a
known ownership defect. A delayed/missing log write also cannot be excluded from
that historical attempt. The corrected observer still fails boundedly for all
unresolved cases.

Controlled evidence independently distinguishes the observation problem from a
stalled completion: an **external diagnostic-only copy** of the mock inserts
an entered/release handshake before this scenario's timed provider completion.
The real original C test fails on the historical missing-completion assertion
while the provider is held for 350 ms. The corrected test remains pending,
then succeeds after release through the unchanged lifecycle assertions. Holding
completion indefinitely fails at the five-second monotonic deadline and retains
the path and latest log containing `navigation_sent`. This is not retrying a
failed assertion or adding delay until it passes; it establishes the specific
interleaving with a barrier. The diagnostic mock patch, binaries and complete
results are retained outside the repository; the committed mock is byte-identical
to main. No provider ownership or production implementation change is indicated
by this experiment. Any observed genuine lifecycle defect would instead require
`PRODUCTION_CORRECTIVE_REQUIRED` and separate implementation review.

## Narrow test-owned correction

Only `native/tests/test_ir_navigation.c` changes in native code:

1. Replace the fixed 200 ms sleep/read in `test_request_close_while_pending()`
   with the existing bounded `wait_for_log_marker(..., "library_unloaded", ...)`.
   It polls fresh snapshots every 5 ms with a monotonic five-second deadline;
   interruptions reobserve the marker/deadline. Timeout fails and retains the
   original log path and latest contents. No failed lifecycle assertion is retried.
2. Keep all four presence assertions and all three ordering assertions:
   submitted < completed < channel destroyed < library unloaded. Keep every
   original submission/request/stream/session Close status check and all other
   native test cases, including deferred cleanup races and fail-safe retention.
3. Make the test log observer fail closed on oversized/truncated snapshots and
   embedded NULs. Recognize only complete newline-delimited marker records, not
   matching substrings or partial writes. This is necessary for the deterministic
   malformed/truncated-log controls and strengthens, rather than bypasses,
   observation. `check_order` uses the same complete-record interpretation.

All producer/provider behavior and all production C/C++/Ada/Rust/Python source,
public ABI, examples, native CMake inventory, workflows and tool pins are unchanged.
The native test inventory stays **434**. No longer fixed sleep is introduced.

## Exact source-integrity proposal

Foundation remains `42648a92eb0cc84b2980d16db6028992c0494ba1` with the original CLI
restriction. Both foundation and actual starting main independently hash to the
same old identity for `native/tests/test_ir_navigation.c`:

| State | Whole-file SHA-256 |
| --- | --- |
| Original foundation / main | `56265cf8a44f1b054777b98457896dc338a79bb306b591b6ee8065c23b3ce224` |
| Proposed corrected test | `7f5aaa15d731f3b5a8b429d42c39565b999a308e1428c01e3ca7c029fe2a36c5` |

The singleton `PROPOSED_NAVIGATION_TRANSITION` independently accepts only these
two complete byte identities, with a required matching index/working-tree state.
The foundation must be the old bytes; an unknown intermediate, partial wait-only
correction, symlink, missing/nonregular/executable file, non-stage-0/unmerged
index entry, or crossed original/proposed state fails. Ordinary inventory rules
still reject introducing the path into an unrelated foundation. Existing RF,
IR-stream and Instrumentation tuples, digests and mechanical labels stay exactly
unchanged. No mock transition is needed because no committed mock changes.

Nine new policy tests reconstruct the proposed bytes through literal edits of
the immutable foundation, never reading candidate bytes as fixture authority.
All previous 51 source-policy tests remain: **60 policy tests**. A real proposed
working tree against an old staged test is deliberately rejected; stage the
exact proposal before qualification. This is a transparent **proposal**, not a
claim that the checker can approve its own policy transition.

## Deterministic controls

`docs/tests/test_navigation_completion_observation.py` compiles the actual C
translation-unit helper code with unused facade sections discarded; it does not
imitate the observer in Python or require a provider replacement. It extracts
and compiles the original four presence and three order assertions. Twelve
additive controls cover:

* ordinary delayed publication and completion beyond the former 200 ms window;
* no successful exit while final deferred cleanup evidence is outstanding;
* missing completion, missing channel destruction and missing final unload;
* every neighboring order inversion;
* final marker without newline (must remain pending until complete);
* substring-spoofed final marker (must time out);
* embedded NULs and a too-long log even when all marker names appear;
* an append immediately after a short EOF snapshot (must reobserve, not mistake
  the newly appended byte for evidence of a truncated input);
* bounded timeout with original file/path/latest log contents retained.

Controller handshakes establish the observer is running before delayed writes;
the delay is used only to exceed the old test window, not to infer completion.
Missing-marker tests fail rather than retry assertions. The external real-provider
barrier experiment separately proves the complete future/cleanup chain under
the controlled completion delay and stall.

Inherited documentation coverage remains **133 script + 364 enhanced = 497**.
Add nine policy and twelve observation controls: **142 script + 376 enhanced =
518 documentation tests**. CI-001's additive 49 controls/shards are not imported.

## Qualification protocol and evidence

Run the full native suite before the correction; it passed **434/434** locally.
For each independent `build/ir-ci-003/{gcc,clang}-{Debug,Release}` tree, run:

```sh
cmake -S native -B BUILD_DIRECTORY -DAMS_MEL_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=BUILD_TYPE -DCMAKE_C_COMPILER=CC -DCMAKE_CXX_COMPILER=CXX
cmake --build BUILD_DIRECTORY --parallel 2
ctest --test-dir BUILD_DIRECTORY --output-on-failure
ctest --test-dir BUILD_DIRECTORY --parallel 4 --repeat until-fail:50 --output-on-failure
cmake --install BUILD_DIRECTORY --prefix INSTALL_DIRECTORY
```

Require 434 ordinary identities and 21,700 repeated executions in **each**
configuration, keeping logs and actual counts, not a single successful rerun.
Separately stress focused Navigation repetitions under recorded parallel CPU load
without rebuilding mutable binaries. Local compiler versions are GCC 14.2.0 and
Clang 19.1.7 with libstdc++; the historical hosted failure used Clang 18.1.3.
Do not claim these local versions are an exact hosted-toolchain reproduction.

Then run all seven requested repository gates sequentially in the pinned isolated
Alire environment: `make test-native`, `make test-ada`, `make check`,
`make check-ada-format`, `alr -n -C ada build`, `alr -n -C ada/tests run`,
`sh docs/tools/qualify_examples.sh`. Retain commands, compiler/tool versions,
start/end times, failures and final statuses. Run staged/working source checks,
baseline-delta/format/newline gates and full documentation generation, all 518
tests and deterministic regeneration. Download main's authenticated publication
independently and compare every file, not only totals:

* 43 files, 38 HTML pages, 4,052 internal references;
* 238 stable symbols, 266 inline links and 31 canonical listings;
* complete pristine fingerprint:
  `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`;
* complete publication fingerprint:
  `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.

The new Markdown/test files do not enter the publication allowlist. No Pages
content is authorized to change or deploy. Final measured local/native/stress
and hosted results belong in external evidence and the PR handoff so recording
them does not invalidate the final qualified commit. No pending result is
predeclared successful here.

After complete local qualification, commit/push normally and open one non-draft
corrective PR. Verify local/fetched remote/PR SHA equality, exact-head Documentation
and all Bootstrap jobs, especially the full Clang Release repeated campaign.
Retain the original main failure forever; later successful workflows supersede
it for future approval, not history. Leave the PR open, auto-merge disabled,
unmerged; no dispatch/cancellation of original main runs, manual deployment,
tagging or release.

## CI-001 integration is a later independent task

Do not modify PR #125 here. Only after independent corrective approval/merge and
successful corrected-main post-merge Documentation and Bootstrap qualification
may CI-001 incorporate main through a normal merge (never rebase/force push),
then requalify its own exact head, 546-test exact-once gate and unchanged
publication. This proposal does not grant that future merge/deployment authority.
