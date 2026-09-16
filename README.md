# AMS MEL — experimental C and Ada consumer binding

An independent, experimental **consumer-side language binding** for the
Agile Mission Suite Government Reference Architecture (AMS GRA) C++ MFA
Encapsulation Layer (MEL) interfaces.

**Status: IR image reception foundation.** In addition to the façade ABI query, the C and
Ada APIs can load a compatible IR MEL provider, create/initialize its published
`Control`, copy complete provider version information, and receive host-memory
single-band Mono8 frames from an `IRSTImage` channel. The adapter owns provider
buffers, copies validated frames into a bounded DROP-INCOMING queue, and offers
poll/wait receive APIs in C and `AMS.MEL.IR`. Tests use a separately loaded C++
mock provider. No real provider, RF, stacked images, tracking, OMS/UCI, OpenCV,
device memory, zero-copy, or processing operations are implemented. This is not an official C MEL standard,
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

This builds `native/build/lib/libams_mel_c.so` and runs C ABI/provider/IR stream
tests plus a C++ header test. Tests do not depend on `assert`, so they remain active in release
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

## Implemented receive profile

The task-002 profile is intentionally host-only and Mono8-only. C callers supply
output storage; Ada callers receive owned arrays. Queue capacity is configured
at stream creation, and incoming frames are dropped when full. Provider callback
threads never call application code. See `docs/c-abi-policy.md` for ownership,
metadata, timeout, teardown, and callback-quiescence limitations.

## Scope and compatibility

The C-facing ABI here remains experimental version **0.1**, unrelated to upstream
MEL API versions, the architecture revision, or provider versions.

Only Linux x86-64 is an initial validation target. The headers include normal
Windows visibility declarations for later use; that does not claim a tested
Windows build or provider compatibility. No SPARK proof claim is made.

The candidate inventory remains historical. Task 001's selected immutable
revisions and vendored declaration closure are recorded in
`docs/upstream-provenance.md`.

## Licensing

New scaffold code is supplied under Apache-2.0; see `LICENSE`. Confirm the
project's intended license and complete maintainer metadata before publication.
The vendored declaration closure retains upstream license and intent files; see
`docs/upstream-provenance.md` and `docs/upstream-files.sha256.md`.
