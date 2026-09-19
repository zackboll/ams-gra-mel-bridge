# Task 023 validation: local native test determinism

## Starting state

Investigation began on `task/023-native-test-determinism` at merged Task 022
revision `bedfca67b0e192b30f0e7eccee625165dfb5f9d1`, with a clean working tree.

The preserved `native/build` cache used `/usr/bin/cc` and `/usr/bin/c++`
(GCC 14.2.0), `CMAKE_BUILD_TYPE=Debug`, and `AMS_MEL_BUILD_TESTS=ON`.  No
`AMS_MEL`, `LD_`, sanitizer, compiler-flag, or `AMS_MEL_TEST_` environment
variables were set in the invoking shell.  The host was Linux 6.12.107 and
glibc 2.41.  Clang was not installed locally.

## Original failure

Both of these failed immediately, with no visible test output:

```sh
ctest --test-dir native/build -R '^ir_c2_contract$' --output-on-failure -V
native/build/test_ir_c2
```

CTest reported `SEGFAULT`; direct execution exited 139.  GDB reported the
sole crashing thread in executable `_init`, called from glibc `call_init`,
before `main`; therefore no C2 test scenario executed and there was no
project-owned C2 frame to diagnose.

Loader inspection showed that the executable loaded the intended
`native/build/lib/libams_mel_c.so.0`, but that facade and the mock provider had
a stale `RPATH` to Alire's GNAT 16 runtime:

```text
/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/lib64
```

Consequently `ldd` resolved `libstdc++.so.6` and `libgcc_s.so.1` from that
directory although the current build cache and current compiler were Debian
GCC 14.  The current generated linker command had no such RPATH, proving the
binary was stale from a prior environment and had not been relinked after that
environment changed.

## Discriminators

* Fresh GCC Debug build in `/tmp/ams-mel-native-clean`: passed
  `ir_c2_contract` 50 times.  It had no runtime RPATH and used the system GCC
  14 `libstdc++.so.6` and `libgcc_s.so.1`.
* Fresh GCC Debug ASan/UBSan build in `/tmp/ams-mel-asan`: passed
  `ir_c2_contract` with `detect_leaks=1`, `abort_on_error=1`, and UBSan halt on
  error; no sanitizer report occurred.
* Fresh `build.sh` (tests OFF) then `test.sh` (tests ON) transition: passed
  all seven native tests.  The shared build directory transition was not a
  reproducer.
* Clang Debug validation was unavailable because neither `clang` nor
  `clang++` was installed.

This is case A: the preserved `native/build` failed and the fresh build passed.

## Root cause and recovery

The failure was local stale native build/runtime-loader state, not a
deterministic native lifetime defect.  Alire injects `LD_RUN_PATH` and changes
the effective C++ toolchain while all native workflows reuse `native/build`.
`LD_RUN_PATH` was embedded by the linker, but was not an input CMake tracked
for rebuild decisions.  This permitted a GCC-14-configured cache to retain a
GNAT-16-runtime-linked library.

`native/scripts/configure-build.sh` now centralizes configuration for every
native script.  It unsets `LD_RUN_PATH` for configure and build, retains
toolchain-required library paths, and resets only a build tree whose resolved
C/C++ compiler differs or which predates the hardened cache marker.  This
prevents caller-specific runtime paths from persisting while retaining the
ordinary tests-OFF to tests-ON transition.

The immediate recovery for an already contaminated tree remains:

```sh
cmake -S native -B native/build \
  -DAMS_MEL_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build --parallel 2 --clean-first
```

`ldd native/build/test_ir_c2` then resolves the system GCC 14 runtime, and the
direct CTest invocation passes.  A sequential ordinary-native → Alire Ada
pre-build/test → ordinary-native transition was validated after the fix; all
native artifacts remained free of RPATH and `ir_c2_contract` passed afterward.

## ABI

The public ABI remains 0.1 (`AMS_MEL_ABI_VERSION_MAJOR=0`,
`AMS_MEL_ABI_VERSION_MINOR=1`).  The established `nm -D --defined-only` check
reports 49 `ams_mel_*` exports in `libams_mel_c.so.0.1.0`, and the version map
contains 49 symbols.  The literal requested command
`grep -c '^    ams_mel_' native/src/exports.map` reports zero because map
symbols are indented by eight spaces; the indentation-correct equivalent is
`grep -Ec '^[[:space:]]+ams_mel_.*;$' native/src/exports.map`.

## Final gates

After the final infrastructure fix, `ctest --test-dir native/build --repeat
until-fail:50 --output-on-failure` passed all seven tests 50 times in 63.83
seconds, and `ir_c2_contract` passed 100 repetitions in 91.30 seconds.  The
ordinary `make test-native`, `make test-rust`, `make test-python`, and
`make check-ada-format` gates passed.  `alr -C ada build` and
`alr -C ada/tests run` passed.  Finally,
`alr -C ada/tests exec -- make -C /home/zboll/git/ams-mel check` passed,
including native tests, Ada tests, formatting, whitespace, and final-newline
checks.
