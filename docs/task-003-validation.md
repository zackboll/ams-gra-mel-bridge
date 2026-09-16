# Task 003 validation — 2026-09-16

Task 003 adds only the IR `ModeCmd` profile `Operate` / `TaskSched`. C exposes
opaque C2/request owners, explicit enable, asynchronous submit/wait, fixed-width
mode/error values, and `AMS_MEL_COMMAND_REJECTED`. Ada exposes controlled
`AMS.MEL.IR.C2` owners and `Mode_Result` success/rejection values.

The completion worker owns the upstream future and shared C2/session/provider
graph until terminal completion, calls `get()` once, and caches the result.
Timeout and public request close do not cancel. C2 close defers disable/detach
while requests are pending. A never-completing provider safely retains the graph.
Synchronous detach failure retains the public owner for retry; an orphaned
deferred failure is internally retained rather than unloading executable provider
state. Lifecycle logs prove completion and both C2/image destruction precede
provider unload.

The corrective lifetime review moved all façade allocation before provider
`send`. A valid future is moved into preallocated worker state, armed with a
pre-existing self-retention owner, and counted under the channel lock with no
intervening throwing operation. Worker launch/allocation failure is an internal
error and publishes no request; provider send exceptions remain provider
exceptions. Allocation-free intrusive atomic roots preserve failed worker or
orphan-detach graphs permanently when no safe completion path exists. Detached
worker helpers catch unexpected failures rather than crossing a `noexcept`
boundary or terminating.

The pinned `C2Channel.h` GCC dependency probe observes 61 upstream headers. The
existing closure supplied 47; 14 unmodified IR headers were added, taking the
vendored union from 71 to 85 headers. Existing repository license/intent files
cover the additions.

## Local results

Validation ran in `/home/zboll/git/ams-mel` on Linux x86-64 with GCC/G++ 14.2.0,
CMake 3.31.6, Alire 2.1.1, GNAT 16.1.0, and GPRbuild 26.0.0. All required
commands passed:

```sh
make test-native
alr -C ada build
alr -C ada/tests run
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
env -u LD_LIBRARY_PATH ./ada/tests/bin/ams_mel_smoke
```

Native CTest reports 6/6: C ABI, C++ header, Task-001 provider contract,
Task-002 image stream, Task-003 C2, and diagnostic-allocation behavior. Fresh
GCC Debug and Release configurations both passed serial CTest and 50 consecutive
parallel repetitions with `--parallel 4 --repeat until-fail:50`. The C2 test is
compiled as strict C11 and covers capability/type/null attach, exact config and
command ID/profile, explicit enable, immediate/delayed/repeated wait, timeout then
success, all requested rejection-description variants, null success, send/future
exceptions, cleanup failures/retry, pending public close, parent/C2-first close,
unload ordering, and image/C2 coexistence. Ada covers success, timeout then
success, normal and greater-than-512-byte UTF-8 rejection descriptions,
parent/C2-first close, and pending finalization. Deterministic private failpoints
also force post-send façade `bad_alloc` and worker-launch failure; both return
`INTERNAL_ERROR`, publish no request, observe provider completion, and prove the
provider/library graph remains safely retained. The failpoints are enabled only
in test builds and are absent from the production shared object.

A GCC AddressSanitizer plus UndefinedBehaviorSanitizer Debug build passed 6/6
with leak detection and halt-on-error enabled. The public header compiled with
`gcc -std=c11 -pedantic-errors -Wall -Wextra -Werror`. `nm` reports exactly the
16 intended versioned C exports. `readelf` reports no mock-provider dependency
from either façade or Ada executable. A clean Release install contains only the
library, C header/CMake package, and license/intent files; no test fixture is
installed.

All 91 vendored files (85 headers and six license/intent files) match the
checksum manifest and detached checkouts of the three recorded immutable commits
byte-for-byte. Normal builds remain offline. `git diff --check` passed and no
generated build artifact is staged. `scripts/check_final_newlines.py`, also run
by `make check`, passed over all first-party repository text files while
deliberately excluding `native/vendor`; the seven Task-003 files found by the
review now have final newlines.

Clang/Clang++ are not installed locally, so no local Clang result is claimed;
GitHub Actions supplies that independent compiler check after push. No real MEL
provider, Squall binary, credentials, container, sensor, or hardware validation
was performed. A provider future that never completes necessarily retains its
provider graph, and no cancellation API is claimed.
