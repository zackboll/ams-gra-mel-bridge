# AMS MEL — experimental C and Ada consumer binding

An independent, experimental **consumer-side language binding** for the
Agile Mission Suite Government Reference Architecture (AMS GRA) C++ MFA
Encapsulation Layer (MEL) interfaces.

**Status: provider foundation.** In addition to the façade ABI query, the C and
Ada APIs can load a compatible IR MEL provider, create/initialize its published
`Control`, copy complete provider version information, and close safely. Tests
use a separately loaded mock provider. No channels, images, RF, OMS/UCI, or
processing operations are implemented. This is not an official C MEL standard,
a complete Skill, or a claim of GRA compliance.

## Layout

```text
native/                 C11 headers, C++20 implementation, CMake, native Alire crate
ada/                    idiomatic Ada package and private C imports
ada/tests/              separate Ada test crate (no AUnit dependency yet)
docs/                   decisions, coverage, next task, original design review
.clinerules/            architectural and workflow rules for Cline
.github/workflows/      Linux native and direct-GPR Ada checks
scripts/                local validation helpers
```

There is deliberately **no root Alire crate**. The two crates are `ams_mel_c`
under `native/` and `ams_mel` under `ada/`. Names are provisional and have not
been reserved in either registry.

## Build the native library

Requirements: Linux, CMake 3.20+, Make or Ninja, and a C++20 compiler. CMake is
the **only owner of native compilation**; the GPR project imports the resulting
shared library rather than recompiling its C++ source.

```sh
make test-native
```

This builds `native/build/lib/libams_mel_c.so` and runs a C ABI test and a C++
header test. Tests do not depend on `assert`, so they remain active in release
builds. No upstream source or provider is downloaded.

To test another compiler, use a separate build directory:

```sh
CC=clang CXX=clang++ cmake -S native -B build/clang \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/clang --parallel 2
ctest --test-dir build/clang --output-on-failure
```

## Build Ada with Alire

Requirements: a hosted GNAT with Ada 2022 support, GPRbuild, Alire, and the
native prerequisites above. Use the Alire toolchain already installed on your
machine. The native crate's pre-build action invokes CMake.

```sh
alr -C ada build
alr -C ada/tests run
# Equivalent test action in the test crate:
alr -C ada/tests test
```

The workspace manifests have **relative development pins**. They are not
registry-ready release manifests. The Ada test crate pins both local packages
explicitly. See `docs/packaging.md` before removing any pins or publishing.

To test without Alire, using GNAT/GPRbuild already on PATH:

```sh
make test-ada
```

This sets `GPR_PROJECT_PATH` for this process only, builds the native library,
builds the Ada consumer, and runs it with a process-local library path. It does
not edit your shell startup files or global settings.

## Local gate

```sh
make check
```

The gate requires native tools and GNAT/GPRbuild. It runs native tests, Ada
smoke tests, and tracked-file whitespace checks when Git has been initialized.
It **fails** rather than silently skipping Ada when its toolchain is missing.
`make test-native` is the smaller native-only gate.

For an installed native CMake consumer, an exported `ams_mel::ams_mel_c` target
is provided:

```sh
cmake --install native/build --prefix "$PWD/build/install"
```

## Start Git

The downloadable source archive does not contain `.git`, build outputs,
credentials, remotes, or a fabricated author identity. After extraction:

```sh
git init -b main
git add .
git diff --cached --stat
git commit -m "Bootstrap AMS MEL C and Ada workspace"
```

Git uses your existing identity. If it asks for one, configure your own real
name/email locally; this project does not configure them for you.

Do not add `origin` until you have selected/created the remote repository.
No release tags, registry submissions, or remote pushes are part of bootstrap.

## Next implementation task

First run `docs/tasks/000-verify-bootstrap.md` on your local Ada toolchain.
The archive validation log distinguishes native tests from unexecuted Ada checks.
Then read `AGENTS.md`, `docs/architecture.md`, `docs/c-abi-policy.md`, and
`docs/tasks/001-provider-foundation.md`. The first milestone is to freeze the
relevant upstream header set and add a **separately loaded mock C++ provider**,
with matching C and Ada lifecycle tests. The task text is suitable for Cline.

Do not start by adding Squall-private gRPC bindings or porting the IR detector.

## Scope and compatibility

The C-facing ABI here is experimental version **0.1**, unrelated to upstream
MEL API versions, the architecture revision, or provider versions. Its only
public function is `ams_mel_get_abi_version`.

Only Linux x86-64 is an initial validation target. The headers include normal
Windows visibility declarations for later use; that does not claim a tested
Windows build or provider compatibility. No SPARK proof claim is made.

The candidate inventory remains historical. Task 001's selected immutable
revisions and vendored declaration closure are recorded in
`docs/upstream-provenance.md`.

## Licensing

New scaffold code is supplied under Apache-2.0; see `LICENSE`. Confirm the
project's intended license and complete maintainer metadata before publication.
The review documents refer to upstream material; upstream code is not bundled.
Future vendoring must preserve upstream license and attribution files.
