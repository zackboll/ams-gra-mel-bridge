# IR-CI-004 — bounded malformed-frame accounting observation

## Scope and authority

This is a proposed native C test-only correction, not a production callback,
counter, provider, ABI, Ada, RF, Navigation or Instrumentation change. Source
policy acceptance is **proposed pending independent review**, not authorization
conferred by passing a local checker. No generated publication change is allowed.

The investigation starts at reviewed main
`5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`. PR #127 remains at
`cec2a98ee90cbf532c66ddf8c1ff13e86b6b6ae0` and is not modified. The expected
CI-002R1 tree `f91f515884ef4436ad676dc055bb2712e311c9f1` survives as a Git
object and as the retained external workload tree. However, the supplied primary
worktree was actually clean on main, with index tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`; it did not contain seven staged
files when inspected. This discrepancy is recorded, not repaired by applying,
resetting or reconstructing the candidate. The actual primary tree/index and
the entire pre-existing diagnostic directory are preserved independently.

The seven retained candidate paths are `.github/workflows/ci.yml`,
`docs/ci-002-bootstrap-policy.md`, `docs/coverage.md`,
`docs/tests/test_bootstrap_ci_policy.py`, `docs/tools/bootstrap_ci_policy.py`,
`docs/tools/documentation_ci.py`, and `scripts/check_doc_source_integrity.py`.

## Original failure evidence

The original evidence is read-only at
`/home/zboll/git/ams-mel-ci-002r1-evidence`. Its `gcc-Debug.log` SHA-256 is
`02b3a2fc3a510b27443a3594ad290e20054deef950c72d1b820d676560e5d949`;
`blocking-native-failure.json` SHA-256 is
`7b61827de80e052c849e353c384fec8e03c15c562327933584939abaac5abb97`.
The actual retained build source HEAD is
`775a01954a4a989396cb9ae614a748df5d01e7ec`, tree
`f91f515884ef4436ad676dc055bb2712e311c9f1`. Native source bytes equal reviewed
main. The configured C/C++ compilers are `/usr/bin/gcc` and `/usr/bin/g++`,
GNU 14.2.0; `AMS_MEL_BUILD_TESTS=ON`, `CMAKE_BUILD_TYPE=Debug`.
Configure, build and the ordinary 434-test run each exited zero. The command
`ctest --test-dir /home/zboll/git/ams-mel-ci-002r1-evidence/native/gcc-Debug/FULL_STRESS --parallel 4 --repeat until-fail:50 --output-on-failure`
exited 8 after 740.05 seconds (CTest reports 740.03).

The failure is test #419, `ir_stream_contract`, repeat iteration **7**: the log
contains six successful stress executions before failure, in addition to its
ordinary-run success and the earlier smoke success. Its assertions are
`test_ir_stream.c:813` (both counters at least one) and `:3734`
(`test_malformed(malformed[i]) == EXIT_SUCCESS`). The failing test lasted
0.99 seconds. The driver launched all four compiler/configuration campaigns
concurrently, each repeated invocation using `--parallel 4`. No original
scenario identity, callback timestamps, exact callback interleaving or host CPU
utilization was recorded; none is inferred from the summary or from later tests.

## Callback ordering

Start initializes registered buffers/ranges and retention state, sets accepting,
then calls provider enable unlocked. The mock enable launches `producer_` and
returns without waiting for a malformed callback. Start publishes Running;
the producer may run before or after that return. The producer checks out a
buffer, builds metadata and invokes `Listener::onImage` synchronously on its
own thread. Callback entry retains the graph and increments in-flight state.

Null-buffer rejection increments both counters under one callback mutex lock.
Other malformed arms increment frames-received before validation, then the
malformed counter under a later lock. Rejection never queues a frame. Provider
buffer release runs via the callback releaser; listener return decrements
in-flight state. A mutex-protected counter snapshot can precede callback entry
or fall between the two increments. It is coherent, not a completion join.

Receive waits only for a nonempty queue or terminal lifecycle. Its 20 ms
`AMS_MEL_TIMEOUT` is not cancellation, delivery or malformed-callback completion.
Close logically stops, disables the mock producer (which joins its thread),
discards queued frames and completes or safely defers physical cleanup. The
existing close/idempotence and session-close assertions remain in `close_all`.
The corrected helper runs that cleanup even after an observation deadline.

`test_nested_malformed_recovery` receives frame ID 2 from the same serial
producer after the first malformed callback returned, so it has an ordering
witness. `test_overflow` instead sleeps 50 ms before observing exact counts.
That is a similar timing assumption but no independent failure is established
here; its source is unchanged. No unrelated test is broadened.

## Independent controlled reproduction

External source/builds, diagnostic scripts and complete traces live at
`/home/zboll/git/ams-mel-ir-ci-004-evidence`. They are never compiled into the
proposed provider or production library. The test-only provider barrier holds
the first delivery before listener entry using a mutex/condition variable;
actual callback return signals completion. All nine malformed scenarios are
tested: null-buffer, null-image, image-before, image-after, invalid-dimensions,
overflow-dimensions, unsupported-bpp, unsupported-bands and unsupported-format.

For every scenario the unchanged original test fails when delivery is held past
20 ms. A separate trace records monotonic start, callback entry/exit, receive
status/elapsed time, immediate counters (0/0), explicitly released callback
completion and counters (at least 1/1), and successful stream/session teardown.
The held negative control reaches a bounded deadline with 0/0, cancels delivery
without invoking a callback, closes successfully and exits failure. No completed
malformed callback in these controls lacks either expected counter. This proves
an insufficient test observation condition; it cannot retrospectively identify
the uninstrumented original failure's exact scheduling history.

The actual corrected helper is additionally tested in that external barrier
build: delayed/released delivery must pass, permanently held delivery must fail
with last counters retained. No timeout or failed API result is waived.

## Exact proposed native edit and source-policy successor

Foundation `42648a92eb0cc84b2980d16db6028992c0494ba1` has IR C-test SHA-256
`6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b`.
The unchanged historical IR-CI-001 transition ends at reviewed main's
`fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400`.
The proposed complete successor is
`bad90aacb6e19ea19f965c56e07d7856ae1a04696e1a900c90cf91641ba71e32`.

Only `native/tests/test_ir_stream.c` changes:

1. Insert `wait_for_malformed_counters` immediately before `test_malformed`.
   It checks each counter API operation once, uses a two-second monotonic
   deadline and one-millisecond polling intervals, preserves the last snapshot,
   and prints scenario/counter diagnostics on deadline. Clock/sleep/API failure
   remains failure. Sleep is pacing only, never evidence of callback completion.
2. After the original unchanged 20 ms receive-timeout assertion, call the bounded
   observer instead of the one immediate counter query; retain the exact
   counter assertion. Run `close_all` before asserting observation success so
   observation failure cannot skip cleanup, and still require cleanup success.

The counter condition establishes accounting, not full callback quiescence;
the existing lifecycle/close path remains responsible for lifetime safety.
A provider hook in committed mock source would also require changing the
atomic Instrumentation authority pair; the public counter observation avoids
that unrelated expansion. No provider behavior is changed to make counters pass.

`PROPOSED_IR_COUNTER_TRANSITION` adds only that exact predecessor/successor pair;
the foundation and historical IR transition stay intact. Index/worktree hashes
must agree. Unknown/partial content, working-tree executables, symlinks,
nonregular or unmerged index entries are rejected. RF, Instrumentation and
Navigation transitions remain unchanged. Independent controls run explicitly:

```sh
python3 docs/tools/test_ir_counter_source_policy.py
python3 -m unittest discover -s scripts -p test_doc_source_integrity.py -v
python3 scripts/check_doc_source_integrity.py
```

The eight additive successor controls live outside the pinned documentation
inventory. All historical policy test bytes and documentation inventories remain
unchanged; this is not an exemption for the native test directory. Source-policy
acceptance remains pending independent approval even when mechanical checks pass.

## Qualification and handoff

Require ordinary 434/434 and repeated 21,700/21,700 executions in each GCC/Clang
Debug/Release configuration, with separate CPU-loaded focused repetitions.
Record failures without retrying them into a pass. Then run the seven requested
repository gates sequentially, all historical documentation tests, both fresh
documentation generations twice and full-byte comparisons against the retained
authenticated baseline. Preserve original failure evidence independently.
Measured results and disposition belong in external evidence, not predeclared
successes here. Do not push an unqualified correction.

Only after independent approval/merge should corrected-main Documentation,
deployment, verify-live and Bootstrap be qualified, followed by a normal merge
into the preserved CI-002R1 candidate and complete requalification. This task
does not reconcile, replace, push or discard CI-002R1 history or evidence.
