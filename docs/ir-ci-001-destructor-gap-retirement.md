# IR-CI-001 — Destructor-close mutation and release-executor retirement

## Scope and independent-review boundary

**Policy disposition: PROPOSED_PENDING_INDEPENDENT_REVIEW.** This PR proposes
one exact-content IR singleton authorization alongside its native test fix.
Computing a digest, passing this proposed checker, or passing CI does not
constitute independent approval. Both commits require independent review.
This correction is a prerequisite to requalifying documentation PR #114, not
authorization to reconcile, merge or otherwise modify that PR.

Only these four files change:

- `native/tests/test_ir_stream.c`: mutation-only synchronization and local markers;
- `scripts/check_doc_source_integrity.py`: independent proposed IR transition;
- `scripts/test_doc_source_integrity.py`: literal reconstruction and regressions;
- `docs/ir-ci-001-destructor-gap-retirement.md`: this qualification record.

Production implementation, cleanup predicates, release-executor semantics,
public headers/exports/ABI, Ada, vendor files, mock providers, RF tests,
workflows, documentation generators and published baselines are unchanged.
The healthy destructor-gap ordering, negative control, native registration and
all existing counter/handle/lifetime assertions remain intact.

## Starting identities and tools

Fetched `origin/main` was `4b2ccbda6a1ad393b436f0e8f1caac9defb283c2`, tree
`96f765beac229b7d64246ddf85059326d65e7c27`, exactly the supplied independently
verified main. Existing branches and open PRs were inspected before creating
`corrective/ir-destructor-gap-retirement` in fresh isolated worktree
`/home/zboll/git/ams-mel-ir-ci-001`. No duplicate correction existed. PR #114
remained open at reviewed R3 head
`b7b3d324f5f9ca2f5929d08609be728ee7cddf55`; its worktree and all R1/R2/R3
evidence were preserved. No DOC-007C implementation was imported.

Local diagnostic and explicit Release builds use Debian **GCC/G++ 14.2.0-19**,
CMake 3.31.6 and Python 3.13.5. This is **not** the original hosted GCC 13.3.0
toolchain. Sequential repository regression selects GCC/G++ explicitly and
places the existing GNAT 16.1.0 / GPRbuild 26.0.1 distribution on PATH
(GPRbuild binary reports 26.0.0, 2026-04-15). Alire is 2.1.1;
GNATformat is the manifest-pinned 26.0.0. Documentation uses an isolated local
settings directory with GNAT 16.1.0, that GPRbuild distribution, and qualified
GNATdoc 26.0.0 (20260415), following starting-main's workflow, not PR #114's
enhancer workflow. These are development local-pin checks, not registry or
arbitrary-provider qualification.

Starting production `native/src/ir_stream.cpp` SHA-256 is
`4011a04b76251e9c6e8237506f95dedee1367d3c7fe4629453098e465baa2566`;
`native/tests/mock_provider.cpp` is
`8c41c152f18eb9e96c1219d7c4073324719b5364d3a8fa458a59f396dda51666`.
Both remain byte-identical in the correction.

## Original failure versus diagnosis

Original Bootstrap **push** run:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39046321478>.
GCC Release job:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39046321478/job/117435657074>.
The hosted log establishes this failure during the existing parallel repeat gate:

```text
destructor close gap failed stage=7 mutation=1 release=11 close=11
```

Both statuses mean `AMS_MEL_PROVIDER_FAILED`. The assertion short-circuits on
the status failure; its printed lifetime log is the earlier stage-5 snapshot.
That hosted log does **not** establish final wrapper/callback/release counters
or a final lifetime sequence. Those must not be inferred from it.

The preserved R3 investigation is
<https://github.com/zackboll/ams-gra-mel-bridge/blob/b7b3d324f5f9ca2f5929d08609be728ee7cddf55/docs/doc-007c-qualification.md>.
Its external original diagnostic checkout is at
`2645c856fc08adb32e8f3cd3bd61d8e563079b6c`, with the exact preserved
`diagnostic-only.patch`. Its forced run establishes the mechanism, independently
of its passing ordinary diagnostic repetitions. Ordinary passing repetitions do
not disprove the race and are not the new correction's qualification.

### Source-backed causal edge

`RequeueBuffer::~RequeueBuffer()` increments
`destruction_callbacks_returned` **inside** the wrapper destructor, before the
remaining destructor body, member destruction and return to the bridge.
`CallbackState::release_buffer()` calls `operation.reset()` unlocked, then
relocks and retires `release_executor_owned`. The mutation intentionally clears
the ordinary obligation counts early and permits cleanup to claim ownership
while that destructor is held. It must continue to prove that forbidden claim.

The old test treated callback count 1 as permission to resume before-detach
cleanup. Cleanup could therefore reach `image_stream_cleanup()`'s post-drain
recheck while the executor remained owned. The guard correctly selected
`late_uncertain`, retained the graph and published failed terminal cleanup.
Both public closes then reported that failure. This correction changes the test
ordering, **not** that production safety guard.

`ams_mel_ir_frame_snapshot_close()` calls the existing production-compiled-out
`image_cleanup_barrier("snapshot-completion")` after
`release_retained_buffer()` returns and before
`finish_deferred_cleanup_from_external_owner()`. Reaching this hook establishes
completion of the release operation's executor bookkeeping for this schedule.

## Corrected handshake and failure cleanup

Only `mutation == 1` constructs and arms the three snapshot-completion marker
paths using the existing unique `mkstemp` base, before starting the release
thread. New path buffers are initialized empty for safe early cleanup.

1. Hold the wrapper destructor; start stream close and observe before-detach.
2. Preserve the stage-5 zero-destruction, unchanged-release, release-depth and
   lifetime assertions while cleanup already owns the channel.
3. Release the destructor using its existing marker.
4. Wait with the existing bounded marker wait for snapshot-completion reached,
   **not** the counter published within the destructor.
5. Assert callback count remains exactly 1.
6. Release **both** before-detach and snapshot-completion before joining either
   thread. Snapshot close may join the cleanup owner: joining it while cleanup
   remains paused would deadlock.
7. Preserve both `AMS_MEL_OK` requirements, consumed stream, exact wrapper and
   callback counts, unchanged destructor-driven release baseline, session close
   and applicable lifetime assertions.

On every failure path all potentially blocking gates, including the new
snapshot release, are opened before joining started threads. Environment and
markers survive until joins and residual owner cleanup finish; all three new
markers are then removed. Normal marker creation and bounded wait failures
remain failures. There are no new sleeps, increased timeouts, accepted failure
statuses, removed mutations or changed production predicates. The healthy branch
never arms snapshot completion and retains its original ordering.

## Controlled BEFORE / AFTER evidence

New task evidence lives outside the repository at
`/home/zboll/git/ams-mel-ir-ci-001-evidence`; original R3 material remains at
`/home/zboll/git/ams-mel-doc-007c-evidence/r3`. The actual R3 patch, forced and
ordinary logs, reproducer and original run/job API records were inspected and
copied without modification. Its checkout diff matches its preserved patch.
Both new diagnostic clones start at exact starting main above; identities,
whole-file source hashes, complete diffs, build logs and controllers are retained
there, distinct from qualified uninstrumented sources.

### BEFORE

`before-source` applies **only** the preserved R3 diagnostic patch to unchanged
starting main. `R3_FORCE_POST_DESTRUCTOR_GAP=1` holds the executor immediately
after `operation.reset()` until the post-drain observation event. This is a
controlled failure, not a timeout, crash or deadlock. CTest exits 8 as expected:

```text
healthy: release=0 close=0 stream=(nil) wrappers=1 callbacks=1
         destructor_releases=0 baseline=0
post-drain: retained=0 obligations=0 executor=1 uncertain=0 failed=0 late_uncertain=1
mutation: release=11 close=11 stream=(nil) wrappers=1 callbacks=1
          destructor_releases=1 baseline=1
```

These final counter/handle observations come from explicit diagnostic reads,
not from the original hosted assertion or its earlier lifetime snapshot.

### AFTER

`after-source` contains the frozen corrected test plus external observation and
scheduling instrumentation only. It does **not** reuse the old controller rule:
holding the executor until post-drain would deadlock the corrected handshake,
which intentionally keeps cleanup paused until executor retirement.

`after-controller.py` observes explicit `gap` and `test-wait` events, then checks
before-detach reached exists, its release does not, snapshot reached does not,
and post-drain has not run. Only then does it create `executor-release`. It
observes the retirement event, snapshot publication event and post-drain event.
The committed test, not the controller, opens the two normal cleanup gates.
No ordering claim is based merely on elapsed time. The observed result is:

```text
test waiting; cleanup still paused; no post-drain
explicit executor release -> retired -> snapshot publication -> post-drain
post-drain: retained=0 obligations=0 executor=0 uncertain=0 failed=0 late_uncertain=0
healthy:  release=0 close=0 stream=(nil) wrappers=1 callbacks=1 releases=0 baseline=0
mutation: release=0 close=0 stream=(nil) wrappers=1 callbacks=1 releases=1 baseline=1
PASS: C IR Mono8 stream receive/queue/lifetime contract
```

All unchanged assertions, including healthy lifetime order and stage-5 mutation
evidence, execute successfully. Instrumentation only logs state and controls
scheduling; it changes no assertions, counters, cleanup predicates, return values
or mutation semantics. Polling is bounded diagnostics, not a timing inference.
Diagnostic execution finished before ordinary native qualification began; no
mutable native build-tree writers overlap.

### Exact external evidence identities

All names below are relative to the new external evidence directory.

| Artifact | SHA-256 |
| --- | --- |
| `diagnostic-only.patch` / `before-complete.patch` | `38be5c1619f57263d35c195ea34d0d4c642a2fd4cead4f5f60bc4a00da15a667` |
| `after-complete.patch` | `dca9230a6111295d28d8b0d637dd773a314b0a75beb742588c6ba65b3d598bbc` |
| `before.log` | `93e61e31871fa10bb7bff6b31f9e3ceed91832a4581d6995081f03fdce6f18a0` |
| `after.log` | `5872949877e6e5f3e26e8b8396916055d004f21573db06b30b4c0b567cef7455` |
| `after-controller.py` | `a2dfbf0d7b66c7b05e85b301645952c9b8e1e1b28be63d538b483e18d4458f87` |
| `after-controller.log` | `4d8670ce9fa5785f6450b13aaba75db55b1675d204c1069f258214052694040e` |

`preserved-initial.sha256`, `diagnostic.sha256` and the final external evidence
manifest cover the preserved artifacts and completed logs. Instrumented source
and complete patches are never part of the committed four-file correction.

## Proposed independent exact-content policy

The DOC-001 foundation remains fixed at
`42648a92eb0cc84b2980d16db6028992c0494ba1`, including the CLI restriction.
The original IR bytes were independently extracted from that immutable commit,
hashed, and compared with starting main, not trusted from the R3 recorded value.

| Path | State | Whole-file SHA-256 |
| --- | --- | --- |
| `native/tests/test_ir_stream.c` | ORIGINAL, foundation and starting main | `6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b` |
| `native/tests/test_ir_stream.c` | PROPOSED corrected singleton | `fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400` |

`PROPOSED_IR_TEST_TRANSITION` is independent of, and is **not appended to**,
`REVIEWED_NATIVE_TEST_TRANSITION`. POL-001's RF hashes, atomic pairing,
original/approved states and reporting remain unchanged. Either exact IR state
can coexist with either valid complete RF state; mixed RF pairs still fail.

The proposed checker verifies the exact original foundation identity first,
then accepts only exact original or corrected IR bytes in **both** the working
tree and regular `100644` stage-0 index blob, requiring matching states. Missing
files, working-tree symlinks/nonfiles, nonregular index entries, unmerged stages,
unlisted content and staged mutations hidden by restored working-tree bytes
fail closed. Corrected state reports `proposed_pending_independent_review`, not
approved. Ordinary inventory, protected-byte and executable Ada-token checks
continue unchanged. There is no directory exemption, mutable manifest,
automatic hash update or new foundation.

Committed fixtures use literal original/corrected expected identities and a
narrow frozen edit applied to immutable original bytes. They never read the
current IR file as their authorization oracle. Eleven new tests (with subcases)
cover both IR states crossed with both RF states, both mixed RF pairs, one-byte
and unlisted mutations in both views, hidden staged mutations, both mismatch
directions, missing/symlink/directory/index-mode/unmerged states, altered
foundations, unrelated native changes and unauthorized additions. Existing
23 policy tests, including CLI/Ada protections and all RF tests, are preserved.

## Qualification and limitations

The uninstrumented candidate was frozen before policy implementation. The
explicit fresh Release commands were:

```sh
CC=gcc CXX=g++ cmake -S native -B build/ir-ci-gcc-release \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/ir-ci-gcc-release --parallel 2
ctest --test-dir build/ir-ci-gcc-release --output-on-failure
ctest --test-dir build/ir-ci-gcc-release --parallel 4 \
  --repeat until-fail:50 --output-on-failure
```

Both CTest gates pass, actual inventory **434/434**, unchanged from starting
main. The ordinary suite takes 10.44 seconds; parallel repeat takes 140.58
seconds. This is the existing gate, not an additional hundreds-of-repetitions
campaign. Builds include a real C11 translation unit and retain Release-mode
checks. Complete logs and exit statuses are external.

The policy suite passes **34/34** (23 preserved + 11 new tests); the complete
starting-main documentation discovery suite passes **116/116**, **including**
those 34, not 116 plus 34. Two complete site generations are byte-identical to
each other and to a separately generated starting-main site. Coverage remains
4,294/4,295; native declarations remain 1,238 source / 1,237 rendered / one exact
renderer exception. Site equality preserves anchors/profiles. Warnings remain
73 raw / 0 public / 73 private-part with no identity delta. Portal validation
remains 37 HTML pages / 1,516 links. Baseline-delta against starting main passes.

The staged/working-tree checker passes with 1,162 protected files, RF state
`approved` and independent IR state `proposed_pending_independent_review`.
The frozen fixture reconstruction also matches the candidate byte for byte.
All 1,330 pre-existing paths outside the authorized edit set were independently
compared with starting main; all match, including 1,094 native/Ada paths other
than the corrected test. Existing RF identities and all vendor bytes remain
unchanged. Exact four-file scope is checked separately from the policy gate.

The repository-required regression sequence completed **sequentially**, all
exit 0, after the environment correction described below:

| Command | Result |
| --- | --- |
| `make test-native` | 434/434 passed |
| `make test-ada` | Complete existing contracts and focused repeats passed |
| `make check` | Build-tree isolation, native, Ada format/contracts, hygiene passed |
| `make check-ada-format` | Passed; only existing source-less C-project GPR warnings |
| `alr -n -C ada build` | Passed |
| `alr -n -C ada/tests run` | Complete existing contract execution passed |
| Source-integrity checker with matching staged and working tree | Passed, proposed IR / approved RF |
| `git diff --check`, staged whitespace, final-newline check | Passed |

No first-party Ada sources changed, so no formatting rewrite was needed.
Both native caches record absolute `/usr/bin/gcc` and `/usr/bin/g++` after the
successful sequential gates. The explicit Release tree remains separate and
uninstrumented. Diagnostic, Release and ordinary regression writers were never
run concurrently against a shared native tree.

### Preserved attempts and environment correction

The first repository regression attempt stopped **at linking**, before test
execution: prepending GNAT's toolchain and using relative `CC=gcc CXX=g++`
selected GCC/G++ 16.1.0, whose facade required `GLIBCXX_3.4.35` unavailable to
the selected link runtime. `attempt1-make-test-native.log`, its exit 2 and
`attempt1-regression-toolchain.txt` preserve this environment failure. The
required full sequence was restarted with absolute `CC=/usr/bin/gcc` and
`CXX=/usr/bin/g++`, retaining qualified GNAT/GPRbuild on PATH for Ada. No source,
assertions or runtime policy was changed to pass it. The fresh explicit GCC 14
Release qualification above had already passed and was unaffected.

An initial documentation driver stopped while recording GNATdoc's version:
that binary prints its version but returns nonzero. The driver's diagnostic
version probe was corrected to tolerate that documented local observation;
the generator's actual pinned-version gate remained unchanged. An external
tool-call time limit interrupted a redundant documentation test invocation;
its partial log is retained separately and the complete invocation passes.
The newline gate detected this newly created report's missing final newline;
it was added and restaged before validation. No unexplained native test failure
was retried to obtain a green result.

Final-head hosted gate results, local/remote/PR equality, current base and
synthetic merge identities are preserved in external handoff evidence after
publication. No queued/running check is considered passed. No merge,
auto-merge, force push, tag or release is authorized or performed. PR #114
reconciliation is a later independent action.

This exact-content policy is a review gate, not an adversarial sandbox or signed
attestation. Branch code can change its checker; trusted Git/Python, no concurrent
writers and SHA-256 collision resistance are assumed. Hash equality proves
content identity, not safety or independent approval. Controlled diagnostics
establish this schedule with the mock, not arbitrary-provider quiescence or
hosted-internal scheduling. Real hardware/provider qualification is outside
scope. Existing source-less external C-project GPR warnings are not new
first-party compiled warnings.
