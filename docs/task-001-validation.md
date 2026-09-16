# Task 001 validation — 2026-09-15

Validation ran in `/home/zboll/git/ams-mel` on Linux x86-64 with Alire 2.1.1,
Alire GNAT 16.1.0/GPRbuild 26.0.0, CMake 3.31.6, GCC/G++ 14.2.0, GNU Make
4.4.1, GNU binutils 2.44, and glibc 2.41. Clang was not installed, so no Clang
result is claimed. No global PATH, shell, Git, or toolchain setting was changed.

## Required commands

All of these passed:

```sh
make test-native
alr -C ada show --solve
alr -C ada/tests show --solve
alr -C ada build
alr -C ada/tests run
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
```

Native CTest reported 3/3: bootstrap C ABI, C++ header, and provider contract.
The provider test is compiled as C11 and covers success, exact numeric/string
conversion, size discovery, insufficient capacity without partial writes,
missing library, missing symbol, null manager/control, failed initialization,
ordinary exceptions from factory/init/version, invalid documented arguments,
20 open/close cycles, and repeated close. Instrumentation recorded
`control_destroyed`, `manager_destroyed`, `library_unloaded` in that order for
both success and failed initialization.

The Ada test printed:

```text
PASS: Ada provider load/init/version/close/finalization contract
```

It exercises exact values, explicit close twice followed by finalization,
finalization without explicit close, and missing-library failure. CMake alone
compiled C/C++; GPR imported the external native library.

## Additional checks

Fresh out-of-tree GCC Debug and Release configurations each passed 3/3 CTest
tests. The in-tree build was removed before `make test-native`, establishing a
clean rebuild. The Ada executable ran directly with `LD_LIBRARY_PATH` unset.

`readelf` found no direct mock-provider dependency in the façade or Ada client.
`nm -D --defined-only` showed exactly:

```text
ams_mel_get_abi_version
ams_mel_session_open
ams_mel_session_get_provider_version
ams_mel_session_close
```

An install to a temporary prefix contained the façade/header/CMake package and
all three upstream license/intent pairs, but no mock provider or failure
fixture. Shell syntax, `git diff --check`, and vendored-file equality against
detached exact-commit checkouts passed. Normal project builds performed no
network fetches and required no credentials, containers, hardware, or external
provider downloads.

The first direct-GPR run exposed a stale preprocessed relative fixture path;
the harness was corrected to derive the absolute test provider path from its
executable location. A first instrumentation run also showed GNU-unique test
provider symbols delaying unload until process exit; GCC test fixtures now use
`-fno-gnu-unique`, and the asserted unload event occurs at `dlclose`. These were
test-harness defects, not reported as passing attempts.

## Review corrections — 2026-09-15

Review findings against `ac6352179b360f1892a327c18c55fbcd1fa5ee5a` were
corrected without changing the four C exports or the vendored snapshot.
First-party task files now end with a newline; vendored files were not edited.

Diagnostics now use non-owning views, never allocate while reporting an error,
replace invalid provider exception text with a fixed valid UTF-8 fallback, and
truncate only at UTF-8 boundaries while reporting the full required byte count.
A test-only executable makes every process allocation fail after a mock factory
throws a long static diagnostic, then calls the real exported boundary with
diagnostics enabled and disabled. It passes without a public fault-injection
export or memory exhaustion. Mock `bad_alloc` modes also cover manager factory,
control factory, initialization, and version-query translation.

Native lifecycle instrumentation now asserts exact factory/init/destruction and
unload events for null, failed, throwing, and allocation-failure paths. Invalid
UTF-8 and embedded-NUL provider version fixtures leave numeric values, required
lengths, and caller buffers unchanged. A multibyte diagnostic fixture verifies
valid-prefix truncation. Tests remain ordinary comparisons rather than C
`assert`, so they remain effective in Release builds.

Ada `Open` validates all three input strings before allocation or native entry,
raises `Constraint_Error` for embedded NUL, and retains the empty aperture ID.
Limited controlled owners initialized to null release each temporary C string
if any later conversion, native call, or provider operation raises. Ada tests
reject NUL in each parameter and assert no factory/init event occurred. The
finalization-only test now asserts the exact `Control` → manager → library event
sequence instead of relying on successful scope exit.

The corrected tree passed:

```sh
make test-native                         # CTest 4/4
alr -C ada build
alr -C ada/tests run
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
env -u LD_LIBRARY_PATH ./ada/tests/bin/ams_mel_smoke
```

Fresh isolated GCC 14.2.0 Debug and Release builds each passed CTest 4/4 with
warnings as errors. `nm` still reports exactly the four intended façade exports;
`readelf` reports no direct mock/failure-provider dependency from the façade or
Ada executable. `git diff --check`, the first-party final-newline audit, and an
empty vendor diff passed.

Clang/Clang++ remain unavailable, so no Clang result is claimed. Plain
`make test-ada` cannot run from the base PATH because system `gprbuild` is not
installed; the same command passed inside the available Alire environment.

## Test-log isolation correction — 2026-09-16

Review of `17506573fc55398dc717af954c661948e7f2ce65` found that
`provider_contract`, `diagnostic_allocation`, and the Ada smoke test could all
append to `native/build/provider-lifetime.log`. Parallel or overlapping runs
could therefore interleave lifecycle events and make one process delete or
inspect another process's log.

The test-only mock now writes only to the path supplied in its process's
`AMS_MEL_TEST_LIFETIME_LOG`; the compile-time shared fallback was removed. Each
native test invocation creates its own `mkdtemp` directory, points the mock at a
log within that directory, and removes only that log and directory at process
exit. The Ada smoke executable independently reserves a unique name with
`GNAT.OS_Lib.Create_Temp_File`, converts it to a private directory, configures a
log within that directory before loading the mock, and cleans up on success or
exception. Its executable-owned setup keeps direct execution independent of a
launcher and still works with `LD_LIBRARY_PATH` unset. CMake remains the only
native build owner, and the mock remains a separately loaded test provider.

The C and Ada lifecycle checks still compare complete logs to exact event
sequences. Allocation-test log setup finishes before the mock enables global
allocation failure, and the mock clears that injection before unload logging;
no setup or cleanup allocation was added to the exported failure path. Native
CI now runs both ordinary CTest and parallel CTest with 50 until-fail
repetitions for every existing GCC/Clang Debug/Release matrix entry.

Local validation passed:

```sh
make test-native                         # CTest 4/4
alr -C ada build
alr -C ada/tests run
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
env -u LD_LIBRARY_PATH ./ada/tests/bin/ams_mel_smoke
```

Fresh isolated GCC 14.2.0 Debug and Release builds each passed ordinary CTest
4/4 and:

```sh
ctest --test-dir <build-directory> --parallel 4 \
  --repeat until-fail:50 --output-on-failure
```

Twenty already-built instances each of `test_provider`,
`test_diagnostic_allocation`, and the Ada smoke executable also passed while
all 60 processes ran concurrently. No test-log temporary directories remained.

Local Clang/Clang++ remain unavailable, so no local Clang Debug or Release
result is claimed. System `gprbuild` also remains absent from the base PATH;
plain `make test-ada` is unavailable there, while its Alire-wrapped equivalent
passed. Dedicated regressions for failure of the Ada `Open` function's second
and third `New_String` allocations remain a follow-up coverage item; this
test-harness correction does not add an Ada allocation-injection framework.
