# RUST-IR-017-R1 — saturation child lifetime-log cleanup ordering

## Scope and source isolation

Corrective investigation for [issue #225](https://github.com/zackboll/ams-gra-mel-bridge/issues/225).
[PR #224](https://github.com/zackboll/ams-gra-mel-bridge/pull/224) remains a separate,
blocked consumer-only change; its branch, four changed files and qualification
reports are untouched. No consumer integration or merge is part of this task.

Freshly fetched starting main is `3e462a6d5448918813a81b45653f9340a66b7c3d`,
tree `a8efcacf5a763867a149eb1a507caf75d4ff164c`, not the task-preparation main.
Dedicated branch: `fix/rust-ir-017-r1-saturation-process-exit`, worktree
`/home/zboll/git/ams-rust-ir-017-r1`. Preflight inspected all issue comments,
open PRs and matching remote branches; no competing corrective PR was found.

Historical negative control: clean detached worktree
`/home/zboll/git/ams-rust-ir-017-r1-baseline`, commit
`604ef38f8f81a9c71add60c5ca2bac4db218ea7b`, tree
`c2bf94b76fa1aa841345484ba2e8efa89e783cce`. Its private test, current starting
main and PR #224 HEAD `55a5d19673a2a1044cf9c54a364c6af3cd631638` all have Git
blob `06ad87dc8b514dd5a0bf1939a68979ded3ab3b0d`. Native implementation and test
sources also compare byte-identical between the historical baseline and starting
main. The baseline checkout is not edited. Its native DSOs are freshly built
from that checkout, not borrowed from another qualification.

Changed-file inventory:

- `/home/zboll/git/ams-rust-ir-017-r1/rust/ams-mel/src/ir_comms_metadata/tests.rs`:
  private native probes, cleanup wait and deterministic parked-completion control.
- `/home/zboll/git/ams-rust-ir-017-r1/docs/rust-ir-017-r1-saturation-exit-correction.md`:
  task-owned evidence and limitations.

Coverage delta: one additional private lifetime-ordering regression; no public
API, native implementation, ABI/layout/export, provider fixture, Ada, Python,
structured native/Ada coverage classification, CI policy or historical report
changes. Final commit/tree identities must be recorded outside their own hashed
source in delivery evidence.

Corrective implementation commit **`7879d533b23b0f684799475d7977d24cbeb8042f`**,
tree **`6680ee62f0b0c32f518c41610138695e8978a34c`**. Qualified Rust test blob:
`263caa38ad26d76b391c3dce47004be8a66c5e62`, SHA-256
`84accb5741051da65b3794344668df72bc6006fb756005df6be83e4fb1e993c6`.
The subsequent evidence commit adds this document only; final-head source and
synthetic-merge identities belong in the PR evidence to avoid self-reference.

## Original hosted failure

Bootstrap run **43677846726**, Rust job **132701805032**, failed at the
unmodified private `isolated_native_controls` saturation child:

```text
PRIVATE_CONTROL_COMPLETE saturation
test ir_comms_metadata::tests::native::child ... ok
timeout: the monitored command dumped core
test result: FAILED. 171 passed; 1 failed; 1 ignored
Process completed with exit code 101
```

Feature tree `067c281c09eb3bf671a3765b04fb80e5beaa9724`; synthetic merge
`028c8250e5f85ebde2f5c62358d900411753d0f5`. The original hosted log is preserved
as `/tmp/rust-ir-017-r1-evidence/hosted-original-rust.log`. Hosted output alone
did not establish a signal or cause; the following core evidence does.

## Historical negative control and actual abort

Environment: Debian GCC **14.2.0**, Rust/Cargo **1.98.1**, CMake **3.31.6**,
x86-64 Linux. Affinity mask `0-31`; CPU **0** is selected from the allowed mask,
not assumed portable. The campaign bound was 20 instrumented / 10 uninstrumented
attempts, stopping at the first failure. **Both reproduce on attempt 1**, child
return code **-6 / SIGABRT**, empty stderr and completion marker on stdout.
The process is terminated, not merely a failed test-body assertion. Core dumping
was enabled only for the reproduction process and descendants with `setrlimit`;
system configuration was not changed. `core_pattern=core`, `core_uses_pid=1`.

Fresh baseline build:

```sh
cmake -S /home/zboll/git/ams-rust-ir-017-r1-baseline/native \
  -B /tmp/rust-ir-017-r1-baseline-debug-native \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/rust-ir-017-r1-baseline-debug-native \
  --target ams_mel_c mock_ir_provider --parallel 2
AMS_MEL_NATIVE_LIB_DIR=/tmp/rust-ir-017-r1-baseline-debug-native/lib \
CARGO_TARGET_DIR=/tmp/rust-ir-017-r1-baseline-debug-cargo \
cargo test --manifest-path /home/zboll/git/ams-rust-ir-017-r1-baseline/rust/Cargo.toml \
  -p ams-mel --lib --no-run
```

Reproduction (Python harness retains each stdout/stderr, PID, status and core):

```sh
RUST_COMMON_METADATA_PRIVATE_CASE=saturation \
AMS_MEL_TEST_PROVIDER_DIR=/tmp/rust-ir-017-r1-baseline-debug-native/test-providers \
taskset -c 0 /tmp/rust-ir-017-r1-baseline-debug-cargo/debug/deps/ams_mel-7e7ed044b87cfb24 \
  --exact ir_comms_metadata::tests::native::child --ignored --nocapture
```

First timeline-enabled PID **4187384**, core
`/tmp/rust-ir-017-r1-evidence/baseline-debug/1/core.4187384`;
second, no additional timeline instrumentation, PID **76854**, core
`/tmp/rust-ir-017-r1-evidence/baseline-debug-uninstrumented/1/core.76854`.
Postmortem GDB all-thread backtraces establish the same explicit abort:

```text
__GI_abort
ams_mel_test_completion::event(Comms, "worker_return_boundary")
  completion_probe.hpp:72: file == nullptr
  errno == 2 (ENOENT)
  path == "/tmp/rust-common-private-4187384/events"
ams_mel_common::run_comms_worker
  common_channel_requests.cpp:708
```

Another worker remains in `complete_comms`'s `state->ready.notify_all()` at
line 701. The Rust harness/main thread is waiting for the test result. Thus this
local post-marker abort can occur **before actual process termination begins**;
the hosted appearance of a termination defect must not be mistaken for proof of
an exit destructor failure. This is an explicit test-probe `abort()`, not a
demonstrated `std::terminate`, assertion failure or memory-corruption termination.
Live single-CPU GDB execution exits normally: scheduling perturbation, not a
negative-control pass. Core inspection does not perturb the failing execution.

Original timeline (first core's append-only native timeline):

1. Four future results consumed; four claims/graphs released and results published.
2. Only two completion-owner destruction events have occurred.
3. Typed channel detached/destroyed, Control/Manager destroyed, provider unloaded.
4. Rust removes the lifetime-log directory and prints its completion marker.
5. Another worker records `worker_return_boundary` in the independent timeline,
   cannot open the now-deleted lifetime log, and explicitly aborts.

The independent timeline survives directory deletion. The original per-child
lifetime log itself does not: deletion is exactly the bug. Binary/DSO SHA-256 and
ELF build IDs, all-thread stacks, loaded-library mappings, toolchain/affinity,
per-attempt stdout/stderr/status and timeline are retained under
`/tmp/rust-ir-017-r1-evidence`. Baseline build identities are in
`baseline-build-identities.txt`. Core mappings identify the dedicated baseline
facade, not a different existing native build. Provider unload precedes the core,
so its identity is also established by the freshly built absolute provider path
and the native unload timeline, not an assertion that it remains mapped.

ELF build IDs (full SHA-256 identities accompany each in external evidence):

| Artifact | Historical Debug | Corrected Debug | Corrected Release |
|---|---|---|---|
| Rust test binary | `230c5e90a4776812ca05e75da9986f4f055ce606` | `2f4baa2750bbe7a509ebe8b5ca2652d250d29dba` | `616639ac0461e77f0229e1958885bc69cdf023a0` |
| Facade | `d27386465ac7a06c1bb3dcb0258473d4b3edb454` | `c9d2e179311a806a156aba8546839599ca999400` | `44f4ec3617600a4c74eda0ab2fed5d6b8dadad69` |
| Mock provider | `b52cb8a1ab52640a73baa0926026a033d02d2840` | `af274d513964fe109672100816226326f53c91f3` | `98366a3b71a8cc587fd10564671c39b167a57012` |

Original hosted log SHA-256:
`7c46b615619f1442447dcc8e2e6280860d90d6d85b2d470c2433489ac1f8b993`.
Timeline-enabled baseline full backtrace SHA-256:
`e88dc133ec330ef5fe05455872d569f5347196bc2b9387ce6d99273eabcb966f`.
Uninstrumented baseline backtrace SHA-256:
`897e2041043f153ee0b5bdc3c76d66701dc0a245a34fc79301ffa1b8a88ba613`.

## Causal boundary and correction

`complete_comms` consumes/destroys the provider result before finishing the claim.
The empty-claim probe precedes result publication. `wait()` can return after the
result mutex is released, even before the worker's result notification returns.
Request Close consumes the public request only; it does not cancel or join.
Typed Close/detach and callback counters likewise do not join completion workers.

After `run_comms_worker`'s return-boundary event, `Worker` records worker finish;
the captured `CommsWorkerInput` then destroys future, completion and admission
permit before its first-declared `FinalOwner`. `FinalOwner` records both its final
lifetime events **before** incrementing `owners_destroyed`. Its dedicated owner
notification returns before `owner_notifications` advances under the observation
mutex. An owner count alone is not a full notification-return or worker-join
observation; the test now waits for both established observations.

The corrected child checks the exact expected owner creation/destruction count:
four for saturation, one for C2 and the deterministic cleanup control, zero for
the other private cases. Only then does it release its manual DSO pin, require
the actual provider unload event, delete the log directory and print completion.
Every completion lifetime-log access has finished before that deletion.

Corrected timeline:

1. Original futures, saturation counters and two exact FIFO reports are checked.
2. Public request/receiver/typed/View/Session closure preserves existing semantics.
3. All expected Comms owners destroy their provider-independent completion state,
   release admission and finish their last log writes; owner notifications return.
4. Manual provider pin released; actual unload recorded while log remains live.
5. Log directory removed, completion marker printed, whole child exits **0**.

This is **not** a new worker-join contract. Shared probe notifications/native
thread epilogues can still return after the owner handshake. They no longer use
the lifetime log or provider graph; probe synchronization has process-lifetime
storage as already qualified by IR-CAMERA-CI-002. The facade remains linked for
the process lifetime. Nothing authorizes explicit facade unloading with live
workers. Production code is unchanged: its provider result/claim ownership is
not replaced with test counters. The core and ordering do not indicate provider
code executing after DSO unload in this scenario, or establish general freedom
from all possible unrelated native defects.

## Deterministic and stress regression

New private `completion-cleanup` case parks a real Comms completion after
`future.get()` but before claim release. A zero-time public wait returns Timeout;
request Close remains non-cancelling; typed Close defers physical destruction.
A strict native owner-notification waiter is armed and observed under the native
mutex, with generation still zero. It cannot finish while the completion is
parked. Only then is the existing boundary released. The waiter joins, the typed
channel destruction is verified and common cleanup checks the final owner and
actual unload. No sleep, retry or new native barrier is introduced.

Separate candidate native/Cargo trees are used for Debug and Release; build
commands mirror the baseline with `candidate-debug-native` /
`candidate-release-native` and `CMAKE_BUILD_TYPE=Debug` / `Release`. The Release
Cargo build uses `--release`. `stress.py` in the evidence directory records each
subprocess individually, including exit status/signal, stdout/stderr, timeline,
affinity and binary/DSO SHA-256/build IDs. The child still requires original
counter values `u64::MAX`, exact two queued reports and all future/Close checks.

| Configuration | Single-CPU saturation | Other private single-CPU controls | Ordinary multi-CPU |
|---|---:|---:|---:|
| Debug | 100/100 exit 0 | 7 cases × 10/10 exit 0 | 8/8 exit 0 |
| Release | 100/100 exit 0 | 7 cases × 10/10 exit 0 | 8/8 exit 0 |

Other controls: allocation, registration reentry, blocked Receive, async callback,
async typed Close, C2 rejection, deterministic completion cleanup. No terminating
signal, core or post-marker failure in this campaign. Each saturation timeline
contains four final owners before provider unload. Separate Debug/Release private
metadata suites and common-metadata integration suites passed; the latter each
report **18 passed / 1 ignored** (the process-isolated child entry).
The complete private parent also passes under single-CPU affinity in both
configurations, retaining its normal `timeout` subprocess/exit-status checks.

To avoid treating instrumentation as proof, an additional **100 single-CPU
saturation children per configuration without `AMS_MEL_TEST_TIMELINE`** also
passed, every process exit 0. Thus the saturation campaign totals **200 Debug +
200 Release**, half with independently retained timeline evidence. A separate
`LD_DEBUG=libs` control exits 0 and identifies initialization/finalization of the
absolute candidate facade and provider paths; it is identity evidence, not the
schedule stress control.

Focused ASan/UBSan build uses its own `/tmp/rust-ir-017-r1-asan-native`, GCC
`-fsanitize=address,undefined -fno-omit-frame-pointer`, Debug, with both executable
and shared-library sanitizer linking. **34/34** compiled-C controls pass:
cross-family completion/retention and allocation/worker-launch failure injection,
plus Health/Image/Stacked saturation, held requests, asynchronous callbacks,
typed-close overlap and reentry. The corrected Rust child also passes **10/10
single-CPU runs each** for saturation, completion cleanup, C2, async and
async-typed against those instrumented DSOs (`LD_PRELOAD` selects GCC libasan,
`LD_LIBRARY_PATH` selects the dedicated facade). No ASan/UBSan diagnostic occurs.
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` deliberately excludes leak checking:
the retention controls intentionally keep uncertain graphs. This is not leak
qualification or instrumentation of Rust itself. UBSan uses `halt_on_error=1`.

Dedicated Release compiled-C controls passed **118/118** metadata, completion,
retention and C2 registration cases, plus **69/69** common-channel ownership
cases. An initial build command used nonexistent target `test_ir_channel` and
stopped with exit 2 before running tests; the corrected target inventory and both
subsequent successful CTest campaigns are preserved, not counted as an initial
pass. The source-integrity gate passes (825 vendor files, four workflows), and
the IR inventory checker passes (127 headers, 148 operations, 57 safe-Ada
operations unchanged).

Preserved invariants: saturating counters, DROP-INCOMING FIFO, maximum/high-bit
IDs, null accounting, independent callback/future completion, one-shot registration,
at-most-once Close, non-cancelling Close/Drop, weak View, pending provider retention,
callback reentry, uncertain-detach retention, and unload only after ownership
release. No new engine, public ownership model or cross-language contract change.

## Qualification and delivery status

The required sequence was executed sequentially from the corrective worktree,
clearing conflicting native/Cargo overrides and selecting the contract facade
explicitly for workspace execution only. All commands below passed:

| Command | Result |
|---|---|
| `make native` | Production build passed. |
| `make test-native` | **799/799** passed. |
| `cargo test --manifest-path /home/zboll/git/ams-rust-ir-017-r1/rust/Cargo.toml --workspace` | **607 passed / 0 failed / 15 ignored**, 46 result groups including doc-tests; native/provider test-tree paths supplied explicitly. |
| `cargo check --manifest-path /home/zboll/git/ams-rust-ir-017-r1/rust/Cargo.toml --workspace --all-targets` | Passed. |
| `cargo clippy --manifest-path /home/zboll/git/ams-rust-ir-017-r1/rust/Cargo.toml --workspace --all-targets -- -D warnings` | Passed without suppression. |
| `cargo fmt --manifest-path /home/zboll/git/ams-rust-ir-017-r1/rust/Cargo.toml --all -- --check` | Passed. |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path /home/zboll/git/ams-rust-ir-017-r1/rust/Cargo.toml --workspace --no-deps` | Passed. |
| `make check` | Passed build-tree isolation, native **799/799**, GNATformat, IR inventory/regressions, Ada suites, newline and whitespace gates with installed GNAT 13.2.1 / GPRbuild 22.0.1. |
| `make test-python` | **663/663** passed; package compilation passed. |
| `make test-policy` | Passed, including deliberate failing CTest negative controls in policy tests. |
| `python3 /home/zboll/git/ams-rust-ir-017-r1/scripts/check_doc_source_integrity.py` | Passed. |
| `python3 /home/zboll/git/ams-rust-ir-017-r1/scripts/check_final_newlines.py` and `git diff --check` | Passed. |

The full workspace includes raw Rust ABI/drift/layout and common/C2 ownership
regressions; Ada smoke invokes common Comms metadata and C2 common tests. Existing
ignored subprocess entries and opt-in providers are not represented as executed
interoperability checks. An initial document-newline check caught a missing final
newline; corrected before aggregate validation, without weakening the checker.

Additional aggregate validation using the repository's current pinned installed
GNAT **16.1.0** and GPRbuild **26.0.1 package** (reports GPRBUILD 26.0.0), followed
by full provider-tree restoration and workspace rerun, is recorded separately:
the pinned-toolchain **`make check` passed** (including native **799/799**, all
formatting and Ada tests) on the committed corrective implementation. Full
provider restoration passed `make native` and `make test-native` (**799/799**),
and post-restoration workspace validation passed again (**607 passed / 15 ignored**).
Existing imported-native-GPR metadata warnings about no C sources remain; no
compiled first-party warning is suppressed. No provider tree has been hidden or
replaced. Evidence roots are
`/tmp/rust-ir-017-r1-evidence` and the durable external copy
`/home/zboll/git/ams-rust-ir-017-r1-evidence`; diagnostic binaries/cores are not
committed as source.

**CORRECTION_UNQUALIFIED** until all local gates and exact-head/synthetic-merge
hosted checks pass. Independent technical review is required. PR #224 remains
blocked; no self-approval, auto-merge or automatic merge is authorized.
