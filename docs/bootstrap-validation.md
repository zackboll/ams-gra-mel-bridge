# Bootstrap validation — 2026-09-15

## Machine and tools

Validation was performed in `/home/zboll/git/ams-mel` on Debian GNU/Linux
13.6 (trixie), Linux 6.12.107+deb13-amd64, x86-64.

| Tool | Version / availability |
|---|---|
| Alire | 2.1.1 |
| Alire GNAT | 16.1.0 |
| Alire GPRbuild | 26.0.0 (2026-04-15) |
| System GNATmake | 14.2.0 |
| CMake / CTest | 3.31.6 |
| GCC / G++ | 14.2.0 |
| GNU Make | 4.4.1 |
| glibc loader (`ldd`) | 2.41 |
| GNU `readelf` | 2.44 |

`gprbuild` is not on the ordinary shell `PATH`. It is available in the
already-installed Alire toolchain at
`~/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin/gprbuild`.
No global toolchain or Git configuration was changed.

## Bootstrap issue fixed

The first required Ada build failed:

```text
$ alr -C ada build
ams-mel.adb:2:06: error: unit in with clause is private child unit
ams-mel.adb:2:06: error: current unit must also have parent "Internal"
```

`AMS.MEL` could not legally depend directly on the private grandchild
`AMS.MEL.Internal.C_API`. The import package was moved to the private sibling
`AMS.MEL_C_API`, and the unused intermediate package was removed. The raw C
record and imported declaration remain private. No public type or operation was
added; the ABI-version query remains the only operation.

## Required validation

### Native build and tests

```sh
make test-native
```

Result: pass. CMake built the C++20 shared library, a translation unit compiled
as C11, and the C++ header test. CTest reported 2/2 tests passed (`c_abi` and
`cpp_header`).

### Alire dependency resolution

```sh
alr -C ada show --solve
alr -C ada/tests show --solve
```

Result: pass. Alire resolved the exact local development pins as:

```text
ams_mel=0.1.0-dev       --> ams_mel_c=0.1.0-dev (=0.1.0-dev)
ams_mel_tests=0.1.0-dev --> ams_mel=0.1.0-dev (=0.1.0-dev)
```

Both dependencies were reported as pinned. This validates local development
resolution only; it is not evidence that either crate is registry-ready.

### Ada library build

```sh
alr -C ada build
```

Result after the fix: pass. Alire first ran the `ams_mel_c` pre-build action,
which invoked CMake and built `native/build/lib/libams_mel_c.so`. GPRbuild then
compiled `ams-mel.adb` and `ams-mel_c_api.ads` and archived
`ada/lib/libams_mel.a`. This confirms native pre-build ordering. The external
native GPR project has no source directories and did not recompile C++.

### Ada runtime test

```sh
alr -C ada/tests run
```

Result: pass. The transitive GPR imports linked and the executable printed:

```text
PASS: Ada -> private C import -> C++ implementation
```

### Direct-GPR helper gates

Task 000 makes these conditional on GPRbuild being on `PATH`; that condition is
false in the ordinary shell. To exercise the same scripts without changing the
global environment, they were run in Alire's existing toolchain environment:

```sh
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
```

Result: pass. `test-ada` built the native library, linked the Ada smoke test,
and ran it successfully. `check` additionally reran the two native tests and
Git whitespace checks; all passed.

Two earlier exploratory commands omitted `-C /home/zboll/git/ams-mel` from the
nested `make`. Because `alr -C ada exec` runs in the `ada` directory, those
invocations failed with `No rule to make target`; this was a command-directory
error, not a project build failure.

## Runtime shared-library loading

The linked executable was inspected and run with `LD_LIBRARY_PATH` explicitly
unset:

```sh
readelf -d ada/tests/bin/ams_mel_smoke | grep -E 'NEEDED|RPATH|RUNPATH'
env -u LD_LIBRARY_PATH ldd ada/tests/bin/ams_mel_smoke
env -u LD_LIBRARY_PATH ada/tests/bin/ams_mel_smoke
readelf -d native/build/lib/libams_mel_c.so | grep -E 'SONAME|NEEDED'
nm -D --defined-only native/build/lib/libams_mel_c.so
```

Result: pass. The executable has a `DT_NEEDED` entry for
`libams_mel_c.so.0`. Its RPATH resolved that SONAME to
`/home/zboll/git/ams-mel/native/build/lib/libams_mel_c.so.0`; `ldd` reported no
unresolved dependency, and direct execution printed the Ada PASS line. The
shared library SONAME is `libams_mel_c.so.0`, and its only defined dynamic code
symbol is `ams_mel_get_abi_version`.

## Boundary checks and scope

Repository shell scripts passed `sh -n`. Source inspection found C convention,
import, and external-name declarations only in the private
`ada/src/ams-mel_c_api.ads`; the public `AMS.MEL` specification contains only
Ada-native declarations. CMake remains the sole compiler of `native/src/abi.cpp`.

No provider implementation, upstream MEL integration, OMS/UCI feature, new C
ABI export, commit, push, publication, tag, or global configuration change was
performed. Task 001 was not started.
