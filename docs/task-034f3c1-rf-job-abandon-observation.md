# Task 034F3C1: RF Job abandonment cleanup observation

Corrected test observation lifetime across asynchronous provider cleanup.
**No production Job/C2 cleanup behavior was changed.** Only the C11 test, mock
provider unload recorder, and this report change. Production sources, public ABI,
exports map, bindings, vendor, and CI workflow remain byte-identical to main.

## Starting state and historical hosted failures

Fetched origin, verified PR #77 merged, and fast-forwarded clean local main to
`037b97aeb8915893001e2d5941b7a87b3e0c23eb`. Created the existing branch
`corrective/rf-job-abandon-observation-lifetime` from that SHA. Subsequent
continuations preserved its uncommitted changes and all investigation evidence;
no reset, stash, discard, branch recreation, amend, or force-push occurred.

| Failure | Main SHA | Workflow | Job |
| --- | --- | --- | --- |
| A | `db3ab3ab71597f5532d92138302204b4e76bfae6` | [37206228820](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/37206228820) | GCC Debug, `111447866752` |
| B | `6c701875445b3367652a0d729f2f39797c20de63` | [37249537340](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/37249537340) | GCC Release, `111574159145` |

Both ordinary `rf-job-request` executions passed first. Both failed in the
parallel repeated phase at `native/tests/test_rf_job.c:373`,
`mock_wait(shutdown)==1`, under the unchanged command:

```sh
ctest --test-dir build/native --parallel 4 --repeat until-fail:50 --output-on-failure
```

This exact failure predates F2/F3; it is not attributed to PR #76 or PR #77.
Post-F3 main workflow
[37254250389](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/37254250389)
completed successfully, **8/8 jobs**, at the starting main SHA. One passing run
does not disprove the historical observation race.

Before editing, pristine `make test-native` passed **276/276** (7.89 seconds),
and focused `rf-job-request` passed **1/1** (0.16 seconds). The Makefile's native
test tree is `native/build-tests`, distinct from CI's `build/native`.

## Three layers of diagnosis

### 1. Original counter-lifetime race

Originally, both `mock_call()` and `mock_wait()` independently opened the provider,
resolved a symbol, invoked it, and closed their temporary reference. The mock's
shutdown counter, mutex, and condition variable are DSO-static. The async worker
can release the last production reference between the test's completion call and
next observation call. A new `dlopen` then observes reset statics in a new DSO
lifetime: cleanup may already have succeeded, but the new counter cannot report
the previous lifetime's shutdown. Baseline acquisition could likewise precede an
unload/reload before scenario creation. The v2/v3/v4 focused tests already retain
an observation pin across baseline/release/wait; that architecture is reused.

### 2. Stable observation does not prove final library release

The retained observer prevents disappearing counters. Both shutdown and C2
destruction barriers then succeed, with correct Job/VA/C2 ordering. However,
`MockC2MEL::~MockC2MEL()` notifies from inside its destructor. Production runs
`state->c2.reset()` before `state->library.reset()` in `finish_c2_shutdown`.
The C2-destructor barrier does not prove the later SharedLibrary destruction.
The old bounded external-log/yield loop is not a completion synchronization
primitive for that library release. It is replaced by a reference-neutral pipe
notification from the existing mock unload recorder.

### 3. Earlier intentional retention contaminated the full process

The first stable-observer implementation passed the finalize-abandon scenario
but failed the later delayed-request case's newly strengthened final-unload gate.
Investigation stopped and preserved the failure rather than changing production.
Valid isolated delayed-request runs passed **50/50** with the old yield loop;
the full process still failed. GDB and loader tracing identified the extra
reference precisely: earlier `c2:finalize-shutdown-throw` invokes
`retain_c2_forever` after its deliberate provider shutdown exception. That
retained RfC2State intentionally owns the same SharedLibrary indefinitely for
safety. A later unload assertion in the same process was invalid, not evidence
of a delayed-request production leak.

The continuation explicitly authorized isolating that intentional-retention
case, without modifying `retain_c2_forever` or production ownership behavior.

## GDB, loader, and reference-neutral evidence

All original evidence remains under
`/home/zboll/git/ams-mel/build/corrective-evidence/`, including the
`unload-investigation/` and `finalization/` directories. Pre-investigation and
pre-finalization source patches, statuses, and reports were preserved.

Valid isolated `dl_iterate_phdr` exact-path probes added no handle:

| Milestone | Mapped |
| --- | --- |
| A: before observer dlopen | no |
| B: after observer dlopen | yes |
| C: after public request/VA/C2 closure | yes |
| D: after delayed result release | yes |
| E: after shutdown barrier | yes |
| F: after C2-destructor barrier | yes |
| G: before observer dlclose | yes |
| H: immediately after observer dlclose | no |
| I: after final unload observation | no |

`LD_DEBUG=files` shows two direct opens (observer and production), count 2,
matching closes, provider `calling fini`, and `destroying link map` before process
exit. No provider NODELETE indication appears. There is no temporary mock_call
in the isolated sequence; its baseline/completion/waits use the retained handle.
The two direct provider-open sites in the complete test are balanced mock_call
and the retained observer. Assertion failures terminate rather than continuing
with an unmatched handle. Mapping probes acquire no loader reference.

GDB captures both isolated closes for handle `0x555555580a60`:

```text
observer, thread 1:
  main -> delayed_abandonment -> mock_job_observer_finish -> dlclose

production, thread 3:
  run_worker -> RfC2ChildClaim::release -> finish_c2_shutdown
    -> c2.reset (rf_c2.cpp:61) -> MockC2MEL destructor
    -> library.reset (rf_c2.cpp:62) -> SharedLibrary::~SharedLibrary -> dlclose
```

`UnloadRecorder::~UnloadRecorder` is reached through the final production close
and glibc `_dl_close_worker`. Under GDB the destructor event can precede final
unmapping; the new pipe deliberately observes recorder completion, not a portable
claim that every loader mapping has already disappeared at that instruction.

The full-process retention trace proves:

```text
lifecycle_tests (c2:finalize-shutdown-throw)
  -> ams_mel_rf_job_close -> RfC2ChildClaim::release
  -> finish_c2_shutdown (shutdown throws, rf_c2.cpp:48)
  -> retain_c2_forever

retained RfC2State:      0x555555591b60
retained SharedLibrary: 0x5555555a4a10
retained loader handle: 0x555555580c60
```

Later delayed cleanup (GDB thread 386) and observer (thread 1) both close their
references to `0x555555580c60`; the earlier retained reference remains. Full
loader direct count ends at 1 and provider fini occurs only at process exit.
No normal library reset is executed on the earlier shutdown-exception path.

An initial diagnostic selector mistakenly preceded capability initialization and
failed at VA submission; those 50 runs are excluded from lifetime evidence. The
corrected selector produced the valid 50/50. A broad full GDB run interrupted by
the command time limit is also excluded; the focused background debugger run
completed and established the retention path. All temporary mapping/selector/
debugger instrumentation is removed from the final source.

## Final test-only correction

### Intentional-retention process isolation

Extract the original shutdown-throw block into
`finalize_shutdown_throw_retention_case()`. The dedicated
`finalize-shutdown-retained` argv path runs only this case after creating its own
lifetime log and initializing the unchanged fixture, then unlinks the log.
The parent invokes the existing fork/exec/waitpid helper immediately after
`lifecycle_tests()`, requiring normal child exit 0. Exec gives a clean loader
state rather than inheriting parent handles into a fork-only scenario.

Every original assertion remains: claimed Job; successful Finalize; VA/public C2
closure; successful Cancel; terminal Complete within the unchanged wait; Job
close returns PROVIDER_EXCEPTION; exactly one C2 shutdown; no unload before
termination. C2 destruction is not required for intentionally retained ownership.
Existing worker-launch/post-allocation/publication/finalize-worker-launch child
cases are unchanged. The final `c2:job-shutdown-throw` remains in the parent:
no later unload assertion depends on unloading after that deliberate retention.

### Retained observation and deterministic unload completion

Both clean abandonment cases use a private C11 `mock_job_observer`:

1. Create a close-on-exec pipe before opening the provider; make its write end
   nonblocking, and configure the decimal FD with `AMS_MEL_TEST_UNLOAD_FD`.
2. Open one provider handle with `RTLD_NOW | RTLD_LOCAL`. Check every required
   dlsym; resolve only the relevant completion operation and existing shutdown/
   C2-destruction count/wait controls.
3. Acquire both baselines from that handle. Keep it across scenario creation,
   public closure, provider completion, shutdown wait, and C2-destruction wait.
4. Require exact once-per-scenario events and ordering through C2 destruction,
   no cancellation or forbidden call, no unload log record, and poll(timeout=0)
   with no notification while intentionally pinned.
5. Checked dlclose releases the observer. Clear all saved provider function
   pointers; never invoke them afterward.
6. Poll the pipe for at most **3000 ms**, a distinct test-only unload barrier.
   Require readable, consume exactly one `U` marker, and require no second byte.
   Re-read the external log and require exactly one unload after C2 destruction.
7. Unset the FD environment variable and close both descriptors before the next
   independent scenario. Neither pipe nor poll creates a DSO reference.

The mock's `UnloadRecorder` retains `record("library_unloaded")`, then parses only
a nonempty, nonnegative decimal FD with overflow checks. Missing/malformed values
disable notification. Its noexcept destructor performs a best-effort one-byte
write (retrying EINTR), without intentional allocation or new file/DSO opens.
The test holds both pipe ends until receiving the marker; the nonblocking write
end avoids blocking provider finalization. Other tests leave the variable unset.

Both `c2:finalize-abandon` and abandoned `c2:job-delayed` request require:

```text
rf_job_destroyed < rf_va_destroyed < rf_c2_shutdown
  < rf_c2_destroyed < library_unloaded
```

The observer exists only for the observable cleanup window and is released
before accepting unload. It cannot hide a production leak as a passing test.
No `rf_job_cancel` occurs in either abandonment case. The unsafe `mock_wait()`
is deleted. No sched_yield/logfile spin remains as unload synchronization.
Provider's existing three-second shutdown/destruction waits, production worker
retention, parent claims, shutdown ordering, and public Close semantics are
unchanged. CI still uses parallel 4 and repeat-until-fail 50.

## Measured final local validation

Fresh separate native trees use GCC 14.2.0 and Clang 19.1.7, with first-party
warnings treated as errors and a translation unit compiled as C.

| Configuration | Targeted rf-job-request | Ordinary suite | Parallel 4, repeat 50 |
| --- | --- | --- | --- |
| GCC Debug | **1000/1000** | **276/276** | **276 x 50**, 108.40 s |
| GCC Release | **1000/1000** | **276/276** | **276 x 50**, 99.10 s |
| Clang Debug | **1000/1000** | **276/276** | **276 x 50**, 104.65 s |
| Clang Release | **1000/1000** | **276/276** | **276 x 50**, 99.13 s |

First fresh GCC Debug focused gate passed **1/1**, followed by **50/50** complete
focused invocations. Counts above were independently checked from actual CTest
pass lines: every contention test has exactly 50 successful invocations, no
Failed or Not Run. Each focused invocation exercises both unload proofs and the
isolated retention child. Trees are `build/corrective-{gcc,clang}-{debug,release}`;
no shared Alire tree is used for reliability counts.

Fresh production-only GCC Release in `build/corrective-production-release` has
**192** exports, exact exports.map parity, no test symbols, runtime ABI **0.1**.
Vendor hashes match **804/804** against `docs/upstream-files.sha256.md`.
No public record/signature/export or binding inventory changed.

Additional native descriptor probes with the facade loaded, matching the real
test runtime, passed missing/empty/signed/whitespace/trailing-text/overflow FD
rejection and valid/leading-zero one-byte notification. Standalone probes without
the already-loaded C++ runtime did not observe finalization at their immediate
dlclose; preserved separately, not counted as product failures or passing unload
evidence. No source change followed that diagnostic probe.

| Aggregate gate | Result |
| --- | --- |
| make test-native | **276/276** |
| make test-build-isolation | pass |
| make check-ada-format | pass |
| alr -C ada build | pass |
| alr -C ada/tests run | pass, full Ada smoke |
| make test-rust | **76 tests + 4 compile-fail doctests**, pass |
| cargo check --manifest-path rust/Cargo.toml --workspace | pass |
| cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings | pass |
| cargo fmt --manifest-path rust/Cargo.toml --all -- --check | pass |
| make test-python | **123/123**, compileall pass |
| make check | pass, isolation/native/format/full direct-GPR Ada/newlines/whitespace |
| git diff --check | pass |

Shared build-tree writers ran sequentially after all matrix stress completed.
Aggregate Alire/direct-GPR gates used the installed GNAT 16.1.0/GPRbuild 26.0.1
toolchain. GPR's existing imported-project warnings about having no C sources
are not first-party source compiler warnings. No first-party Ada source changed.
Full corrected executable under LD_DEBUG and direct `finalize-shutdown-retained`
selector both exited successfully. Final source SHA-256 fingerprints are saved
with the matrix evidence, ensuring stress-tested source remains unchanged.

## Publication and hosted evidence

Final push and PR evidence is recorded after their actual workflows complete.
Push tests the literal corrective source commit; pull_request tests GitHub's
synthetic merge commit. The PR must remain open, non-draft, unmerged, with
auto-merge disabled. No PR merge-ref result is described as literal-source-head
testing. This report's hosted completion record may use a separate normal
documentation-only commit; no amend or force-push is used.

## Environment and evidence preservation

Original task `/tmp` was 99% full; no unrelated files or containers were removed.
Finalization started with about 12 GiB free in `/tmp` and 852 GiB on the repository
filesystem. Task logs and compiler temporary storage use disk-backed
`build/corrective-evidence/tmp`; unchanged legacy test lifetime-log templates
still use `/tmp`. All earlier failed experiments and stop evidence remain
preserved. No timeout increase, sleep, retry, CI weakening, or production change
is used to obtain passing results.
