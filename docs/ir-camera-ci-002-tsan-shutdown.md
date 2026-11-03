# IR-CAMERA-CI-002 — completion-probe shutdown lifetime

## Scope and source identity

Starting main: `0bd87efabda62d0afb085722be179a1d1ad03c62`, tree
`b07b4c8e08bec77e08fe449b53d762637fa6e6fd` (merged PR #161).
Dedicated worktree: `/home/zboll/git/ams-ir-camera-ci-002`, branch
`corrective/ir-camera-ci-002-tsan-shutdown`. No existing branch was changed.
Preflight found only unrelated Python/Rust PRs #158–160 open, and no other
TSan correction owner. Original CI-001 evidence was left untouched: all 218
files were SHA-256 inventoried and subsequently verified.

Only test-probe storage/notification synchronization, a compiled-C shutdown
regression and its test-only export/CTest registration change. No production
functionality, installed header, export map, Ada source or CI policy changes.
PR #161's existing test bodies and session-specific admission observations
remain intact. All seven original families use the same corrected storage:
Mode, Return, Comms, Navigation, Instrumentation, Track and Camera. C2 and
Image CameraCommand share Camera. This is mock-provider evidence, not camera
hardware or Squall qualification.

Main subsequently advanced to `ad8addcf96164f39ffd8e8cf93a45bf66bd87cb8`
(PR #162, CameraProtect). This branch retains its requested starting identity;
the generic Count-sized storage also covers appended families on integration.
Local results below identify the starting-main inventory, not that newer main.

## Original finding reproduced before the fix

Environment: Debian 13 x86-64, Linux `6.12.113+deb13-amd64`, Clang 19.1.7,
CMake 3.31.6, glibc 2.41, libstdc++ 14. Native CMake Debug configuration:

```sh
cmake -S native -B build/tsan -DAMS_MEL_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/clang-19 \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  '-DCMAKE_C_FLAGS=-fsanitize=thread -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer'
cmake --build build/tsan --parallel 8 --target test_ir_camera_command
```

On **unmodified starting main**, 100 independent CameraCommand contract
processes yielded 76 exits **66** and 24 exits **0**. All 76 reports identified
`ams_mel_test_completion::observation_ready`: main-thread `pthread_cond_destroy`
at process exit versus detached `pthread_cond_broadcast` from
`FinalOwner::~FinalOwner` (`completion_probe.hpp:111`), through
`CameraWorkerInput::~CameraWorkerInput` (`ir_camera_command.hpp:44`), launched
by `submit` (`ir_camera_command.cpp:347`). Example creation stack:
`test_admission` (`test_ir_camera_command.c:644`) -> Image submit -> detached
worker. Actual symbolized reports, source identities, compiler flags, binaries
and every exit code are preserved, not inferred from CI-001's handoff.

An initial shutdown-only regression linked against the preserved unmodified
instrumented facade reproduced the same finding **50/50**, exit 66. Replacing
only the final corrected probe storage with exit-destroyed `Observation`
reproduces the race in both shutdown and full contract modes, exit 66.

## Lifetime and synchronization proof

`FinalOwner` publishes `owners_destroyed` after its graph and admission permit
are released but **before** its final notification. Neither that count nor
session admission zero proves notification return or worker quiescence.

The observation mutex and condition variables are now placement-constructed
once in aligned static byte storage. References do not own the object; no
destructor is registered for the placement-constructed `Observation`. It owns
no provider graph or heap allocation. There is no exit-time synchronization
teardown racing detached workers; the process image reclaims the storage.
Disassembly confirms no `__cxa_atexit` in the completion-probe initializers and
no emitted `Observation` destructor. This is not a worker-join claim and does
not license explicitly unloading the facade while a worker executes it.

The user-authorized deterministic regression adds one test-only export,
`ams_mel_test_completion_owner_notify`, only to CMake's generated
`test-exports.map`. Each family has a dedicated owner condition variable;
only its `FinalOwner` notifies it. Under the observation mutex, FinalOwner
notifies, **then** advances the generation. A waiter must reacquire that mutex
and see its target generation: a spurious wake before the intended notification
cannot satisfy it. Shared worker/boundary notifications and other families
cannot wake the dedicated condition variable.

The C test starts a pthread waiter, then observes its armed flag under the same
mutex before releasing either held future. The waiter sets armed and enters
`wait_until` without releasing the mutex in between; the controller cannot
observe armed until the waiter has atomically released the mutex to wait.
Success requires a non-timeout wake and target generation before the absolute
15-second deadline. A predicate becoming true at timeout is explicitly rejected.
The waiter is joined on the test path. Both C2 and Image are exercised, including
public request Close before completion, result validation, final owner counts,
and resource closure. Process exit may still overlap the **shared** final
notification; process-lifetime storage, not the handshake, makes that safe.

The first shared-condition-variable handshake was rejected during development:
its notification-disabled negative control could pass from unrelated worker
notifications. Those failed attempts and the superseded results remain in the
evidence directory. The final design isolates the intended notification.


## Qualification and negative controls

Final native sources were unchanged throughout these runs. Commands use the
CMake binary tree explicitly; all output and actual exits are under
`/home/zboll/evidence/IR-CAMERA-CI-002`.

| Qualification | Actual result |
| --- | --- |
| Clang TSan CameraCommand contract and shutdown, `ctest -R '^ir_camera_command_(contract\|shutdown)$' --repeat until-fail:100` | 100 each, 200 executions, exit 0, no reports |
| Additional TSan shutdown processes | 500/500 exit 0, no reports |
| Clang ASan with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, same two tests × 500 | 1,000 executions, exit 0, no ASan/LSan reports |
| ASan all eight CameraCommand scenarios once (including retained-failure paths) | 8/8, exit 0 |
| GCC Debug ordinary CameraCommand contract and shutdown × 500 | 1,000 executions, exit 0 |
| Isolated GCC Debug native inventory once | 443/443, exit 0 |
| Isolated native full stress, `ctest --parallel 4 --repeat until-fail:50 --output-on-failure` | 443 × 50 = 22,150 passes, exit 0; every test verified exactly 50 times |
| `make test-policy` | exit 0 |
| `make check-ir-api-coverage` | exit 0 (including 26 coverage tests) |
| `make test-ada` with installed GNAT 15.3.1 / GPRbuild 26 on PATH | exit 0 |
| Production baseline/fixed Release builds | both exit 0; 239 ABI functions plus version symbol identical; whole library SHA-256 identical |
| `git diff --check` | exit 0 |

Inventory is 443 because this correction adds one explicit shutdown test to the
starting main's 442; the original inventory and repetitions were not reduced.
No hosted runner count/workload, retry, sleep, suppression or ignorelist changed.
A first stress attempt was invalidated when an Ada helper reconfigured the
shared `native/build-tests` tree underneath CTest; its misleading exit 0 was
**not** accepted. The complete qualified stress run used dedicated
`build/native-qualification`, verified from the JSON inventory and per-test log.

### Negative controls

Disposable external source copies, compiled with the CMake flags and linked
against the unchanged final C test, reject:

- Exit-destroyed observation storage: both contract and shutdown report TSan
  race, exit **66**. The process-lifetime fix is necessary.
- Dedicated FinalOwner notification disabled, generation accounting and all
  shared/worker notifications left enabled: **10/10 exit 1**, failing the
  strict waiter assertion; no PASS lines. Missing generation also exits 1.
- Missing, duplicate or premature owner destruction counts: exit **1**.
- Missing/duplicate/early channel destruction, control-before-channel ordering,
  and library unload before completion: exit **1**.
- Lost/duplicate request accounting, premature admission release, Close
  incorrectly cancelling/releasing a pending request: exit **1**.
- Unsafe synchronous callback re-entry with the callback mutex held: fails to
  finish within the external 60-second negative-control bound, killed **-9**.

The existing C2/Image, Image Stop-first/Close-only, timeout/failure, admission,
worker safety masks, completion graph/order, provider unload, earlier-worker
completion and repeated-owner checks are retained, not replaced by the new test.

### Broader TSan limitations — not a globally TSan-clean claim

TSan executes reliably here and reproduces the original race. The focused
CameraCommand contract/regression is clean; the broader suite is **not**:

- 174 relevant completion/common/public-channel/admission tests requested for
  20 repetitions: exit **8**; 172 tests completed all 20 repetitions. The two
  exception cases (`completion_cross_family_exception`,
  `completion_retained_exception`) stopped at their first failure, with four
  unsuppressed exception-storage `free`/worker-read reports. The same
  Navigation exception-storage reports reproduce against the preserved
  unmodified starting-main TSan facade (`run-10.log` in the baseline exception
  repetition evidence). No observation-state report occurs.
- A broad final TSan pass (442 executable tests, excluding the unrelated
  `diagnostic_allocation` target which cannot link its custom new/delete with
  Clang's TSan runtime) exits **8**, 426 passed / 16 failed. Findings include
  mock-provider unload/global-state races, C2 metadata-close mutex lifetime,
  retained joinable producer thread reports, guarded-memory subprocess TSan
  crashes, and the RF interval timeout. Baseline broad execution already
  exhibited these categories. There are **zero** observation synchronization
  reports after the fix. No unrelated production repair is attempted here.

Full-tree final-newline validation also rejects the pre-existing
`docs/rust-ir-003-nuc-temperature.md`, which lacks its final newline in starting
main. It is not modified by this task. `make check` status is recorded in the
external delivery report; no broader gate is represented as passing without
an actual successful exit.

Production facade SHA-256 (both baseline and fixed Release artifacts):
`ab043e81c911cc5c8564d6e25fc450afa4c5066b9a37f22c6b74e2eb34576035`.
