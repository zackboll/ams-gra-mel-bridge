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