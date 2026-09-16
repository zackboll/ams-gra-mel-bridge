# Task 002 validation — 2026-09-15

Validation runs in `/home/zboll/git/ams-mel` on Linux x86-64 with Alire 2.1.1,
Alire GNAT 16.1.0/GPRbuild 26.0.0, CMake 3.31.6, and GCC/G++ 14.2.0.
Clang is not installed locally; no local Clang result is claimed.

## Implemented profile

The C++ adapter uses the pinned IR MEL `Control`, `ImageChannel`, `ImageListener`,
and `Buffer` interfaces. It attaches `IRSTImage`, creates stable façade-owned
host buffers through `getBuffer`, registers/enables, validates and copies Mono8
callbacks into a bounded DROP-INCOMING queue, releases provider buffers, and
supports polling/timed receive, counters, stop, and close. The Ada
`AMS.MEL.IR` child provides controlled ownership and owned pixel arrays.

The independent-review hardening adds explicit attached/starting/running/
stopping/stopped/failed states, in-flight callback RAII, checked release status,
capability/range validation, and destruction-based callback quiescence. Pinned
Squall source shows `disable()` does not stop `UdpDataReceiver`; channel
destruction does. Teardown therefore retains listener/buffers/host storage until
detach and channel destruction complete and the adapter in-flight count is zero.

Native C coverage includes exact configuration conversion, deterministic pixels
and metadata, sequential receive, size discovery without dequeue, empty timeout,
stopped state, queue overflow/counters, null/out-of-range addresses, invalid and
overflowing dimensions, unsupported BPP/bands/format, buffer factory/register/
enable/attach failures, parent-first close, callback-active shutdown, repeated
cycles, and task-001 regressions. Ada coverage includes exact pixels/metadata,
sequential frames, counters, timeout/stopped distinction, explicit teardown,
parent-first close, and finalization-only cleanup.

Hardening coverage adds non-quiescing disable with condition-variable barriers,
disable/detach/release-return/release-exception failures, retry after failed
Start, Stop after failed Start, Close after cleanup failure, blocked Receive
wakeup on provider failure, queued-frame draining after Stop, incompatible
capabilities, `int64_t` range checks, safe malformed numeric addresses, and
deterministic `std::bad_alloc` from the Task-002 buffer factory path.

## Results

All required local commands passed:

```sh
make test-native
alr -C ada build
alr -C ada/tests run
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
env -u LD_LIBRARY_PATH ./ada/tests/bin/ams_mel_smoke
```

Native CTest reported 5/5: C ABI, C++ header, task-001 provider contract,
task-002 IR stream contract, and diagnostic-allocation behavior. The Ada output
reported both the IR Mono8 receive/timeout/lifetime contract and the existing
provider lifecycle contract as passing.

Fresh isolated GCC 14.2.0 Debug and Release configurations built with warnings
as errors. Each passed serial CTest 5/5 and:

```sh
ctest --test-dir <build-directory> --parallel 4 \
  --repeat until-fail:50 --output-on-failure
```

The public header compiled with GCC as strict C11 using `-pedantic-errors`.
`nm -D --defined-only` reports exactly the four task-001 exports and six new IR
stream exports, with no C++ implementation symbols. `readelf` reports no direct
mock/failure-provider dependency from the façade or Ada client. A clean Release
install contains only the library, public header, CMake package, and license/
intent files; no provider fixture or test binary is installed.

All 71 vendored headers match detached checkouts of the recorded immutable
commits byte-for-byte. The 27 task-002 checksum additions verify against
`docs/upstream-files.sha256.md`. `git diff --check` passed. Normal builds perform
no source fetch and use no provider download.

After the independent review fixes, fresh isolated GCC Debug and Release builds
again passed serial CTest 5/5. Both also passed:

```sh
ctest --test-dir <build-directory> --parallel 4 \
  --repeat until-fail:50 --output-on-failure
```

An isolated GCC AddressSanitizer plus UndefinedBehaviorSanitizer Debug build
passed CTest 5/5 with leak detection and halt-on-error enabled. This includes the
non-quiescing lifetime and malformed numeric-pointer fixtures. GCC `-fanalyzer`
was not advertised by the local compiler query, so no analyzer result is claimed.

The hardening run also repeated all six required native/Ada commands above,
strict C11 compilation, the exact ten-function export audit, clean install audit,
ELF dependency audit, unchanged-vendor check, and `git diff --check`.

## Limitations

No real provider, credentials, containers, hardware, or build-time downloads are
used. The pinned `disable()` contract does not promise callback quiescence. The
mock now covers both a normal join-in-disable provider and a Squall-like provider
whose channel destruction supplies quiescence. Providers that can invoke image
callbacks after their channel is destroyed remain incompatible.
Contributing-sensor and inertial/navigation vectors are not returned in the
task-002 frame record and remain explicit future metadata coverage.

Clang/Clang++ are unavailable locally, so no local Clang Debug or Release result
is claimed; the existing GitHub Actions matrix supplies independent Clang
coverage after push. Plain base-PATH `make test-ada` remains unavailable because
system `gprbuild` is absent; its Alire-wrapped invocation passed.
