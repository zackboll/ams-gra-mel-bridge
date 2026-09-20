# Corrective: isolate the native production and contract-test build trees

## The hazard

Before this change every native script shared one CMake build tree,
`native/build`, for two incompatible configurations:

| Script | `AMS_MEL_BUILD_TESTS` |
| --- | --- |
| `native/scripts/build.sh` | `OFF` |
| `native/scripts/test.sh` | `ON` |
| `native/scripts/build-test-providers.sh` | `ON` |

`native/scripts/configure-build.sh` reconfigured that single tree between `ON`
and `OFF` silently. The two modes are not interchangeable: the test
configuration compiles `AMS_MEL_ENABLE_TEST_FAILPOINTS` into `ams_mel_c` and
adds the mock providers and CTest targets, while the production configuration
compiles none of them.

Running `build.sh` during or after test work therefore mutated the native test
tree underneath CTest and stress runs. This was observed during the Image
navigation teardown corrective task:

- the tree was configured with `AMS_MEL_BUILD_TESTS=ON`;
- `native/scripts/build.sh` reconfigured the same tree with
  `AMS_MEL_BUILD_TESTS=OFF`;
- the test-only deterministic barriers disappeared from the rebuilt facade;
- subsequent test behavior looked like a product failure even though the test
  infrastructure had been replaced underneath it.

The inverse transition is equally undesirable: an ordinary production build must
never silently become a failpoint-enabled facade.

Baseline evidence reproduced on `main` at `6d3c90a` before the fix, with no
concurrent test or stress run:

```
sh native/scripts/test.sh
grep '^AMS_MEL_BUILD_TESTS:' native/build/CMakeCache.txt
    AMS_MEL_BUILD_TESTS:BOOL=ON
    md5 native/build/lib/libams_mel_c.so.0.1.0 = ddaaa284864c77cb0cc46146f568690d

sh native/scripts/build.sh
grep '^AMS_MEL_BUILD_TESTS:' native/build/CMakeCache.txt
    AMS_MEL_BUILD_TESTS:BOOL=OFF
    md5 native/build/lib/libams_mel_c.so.0.1.0 = 02ad6bae78d89fb94403fba31b79d671
```

The same directory flipped mode and the facade binary was rewritten.

## Required invariant

The production native build and the native contract-test build must never share
a mutable CMake build directory.

## Build trees

| Directory | Purpose | `AMS_MEL_BUILD_TESTS` | `AMS_MEL_ENABLE_TEST_FAILPOINTS` |
| --- | --- | --- | --- |
| `native/build` | production facade | `OFF` | absent |
| `native/build-tests` | contract-test facade, mock providers, CTest targets | `ON` | present |

`native/build` remains the production and default location because
`native/ams_mel_c.gpr` names `build/lib`, Alire's native package represents the
production facade, real-Squall integration intentionally builds and links the
production facade, and the default Rust native-library discovery in
`rust/*/build.rs` points at `native/build/lib`.

`native/build-tests` is ignored by Git through an explicit `build-tests/` rule;
the generic `build/` rule does not cover it.

## Script contract

`native/scripts/build-tree.sh` is the single shared helper. Sourcing it and
calling `ams_mel_select_tree {production|tests}` sets `ams_mel_build_dir` and
`ams_mel_build_tests`, so no script duplicates directory or mode knowledge.

`native/scripts/configure-build.sh` now takes `production` or `tests` instead of
`ON`/`OFF`, derives the directory from the mode, and resets **only** the selected
tree. A test configure never removes or reconfigures the production tree, and a
production configure never touches the test tree.

- `build.sh` -> `native/build`, `AMS_MEL_BUILD_TESTS=OFF`.
- `test.sh` -> `native/build-tests`, `AMS_MEL_BUILD_TESTS=ON`, CTest from that
  tree.
- `build-test-providers.sh` -> `native/build-tests`, `AMS_MEL_BUILD_TESTS=ON`.

## Task 023 hardening remains active

The Task 023 environment hardening is unchanged and now applies independently to
both trees: caller `LD_RUN_PATH` is unset before every CMake configure and build,
a cached compiler change resets the tree, and a pre-hardening cache without
`AMS_MEL_SANITIZED_BUILD_ENVIRONMENT=ON` resets the tree. A mode mismatch in a
cache is now also a reset reason, which is a safety net only: each caller already
selects the tree matching its mode.

## Which facade each consumer uses

| Consumer | Link-time facade | Runtime facade | Providers |
| --- | --- | --- | --- |
| Ada binding / ordinary Ada build (`ams_mel_c.gpr`, `alr -C ada build`) | production `native/build/lib` | production `native/build/lib` | n/a |
| Ada contract tests | production `native/build/lib` | contract-test `native/build-tests/lib` via `LD_LIBRARY_PATH` | `native/build-tests/test-providers` |
| Rust ordinary/direct build (`rust/*/build.rs` default) | production `native/build/lib` | production `native/build/lib` | n/a |
| Rust contract tests (`make test-rust`, CI) | `native/build-tests/lib` | `native/build-tests/lib` | `native/build-tests/test-providers` |
| Python contract tests (`make test-python`, CI) | n/a (`ctypes`) | `native/build-tests/lib/libams_mel_c.so.0` | `native/build-tests/test-providers` |
| Real Squall integration | production `native/build/lib` | production `native/build/lib` | real provider image |

### Ada link-time versus runtime

The Ada contract suite exercises `AMS_MEL_ENABLE_TEST_FAILPOINTS` directly, for
example by setting `AMS_MEL_TEST_IMAGE_CALLBACK_FAILURE` and asserting the
resulting recovery counters. Those failpoints exist only in the contract-test
facade, so the smoke executable must run against `native/build-tests/lib` even
though the binding itself is linked against the production facade named by
`ams_mel_c.gpr`.

`ada/tests/ams_mel_tests.gpr` therefore links with `-Wl,--enable-new-dtags`, so
the executable carries `DT_RUNPATH` rather than `DT_RPATH`. `DT_RUNPATH` is
searched *after* `LD_LIBRARY_PATH`, which lets `scripts/test_ada.sh` and the
`[environment]` section of `ada/tests/alire.toml` select the contract-test
facade at run time. `DT_RPATH` would take precedence over `LD_LIBRARY_PATH` and
would silently pin the tests to the production facade.

To be explicit: the Ada contract tests do **not** execute the production facade.
They link against it and then run against the contract-test facade. The ordinary
Ada build and any downstream Ada consumer both link and run the production
facade.

Mock providers are resolved from `AMS_MEL_TEST_PROVIDER_DIR`, with a
deterministic repository-relative fallback to
`native/build-tests/test-providers`, so both `make test-ada` and
`alr -C ada/tests run` find them.

### Rust and Python

`rust/ams-mel-sys/build.rs` and `rust/ams-mel/build.rs` keep defaulting to the
production facade in `native/build/lib`, so an ordinary or direct `cargo build`
never links a test-enabled native library. `make test-rust` and the Rust CI job
override that by setting `AMS_MEL_NATIVE_LIB_DIR` to `native/build-tests/lib`
explicitly. Separately, the repository's provider-path helpers and the ABI probe
in `rust/ams-mel-sys/tests/abi.rs` fall back to the test tree when
`AMS_MEL_TEST_PROVIDER_DIR`/`AMS_MEL_NATIVE_LIB_DIR` are unset, because those
fallbacks only ever apply to repository contract tests.

Python contract tests bind the test facade explicitly through
`AMS_MEL_NATIVE_LIB`. Downstream Rust and Python builds keep the production
defaults and therefore never implicitly depend on a test-enabled native library.

## Why production must not contain test failpoints

`AMS_MEL_ENABLE_TEST_FAILPOINTS` exists only to make contract tests
deterministic; it inserts barriers into the shipped code paths. A production
facade carrying those barriers would not be the artifact under test elsewhere,
and consumers linking `native/build/lib` must get the real behavior.

## Why real Squall uses the production tree

`integration/squall/run.sh` configures `native/build` with
`AMS_MEL_BUILD_TESTS=OFF` deliberately. Real-Squall integration exists to
exercise the production facade against a real provider. Moving it to the
failpoint-enabled test facade merely for uniformity would weaken what that gate
proves, so it stays on the production tree and is unaffected by this change.

## Regression guard

`native/scripts/test-build-tree-isolation.sh` (also `make test-build-isolation`,
and run first by `make check` and by a dedicated CI job) proves:

- **A. Test then production.** Build the test tree, confirm it is `ON`, record
  SHA-256 hashes of the test facade, the `test_ir_navigation` contract
  executable, and the test `CMakeCache.txt`; run `build.sh`; confirm production
  is `OFF` while the test tree is still `ON`; confirm none of the recorded test
  artifacts were rewritten; then run CTest from the test tree **without**
  rebuilding it and require 15/15.
- **B. Production then test.** Build production, record its facade and cache
  hashes, run `test.sh`, and confirm production stayed `OFF` and unmodified while
  the test tree is `ON`.
- **C. Failpoint separation.** Inspect the generated `flags.make` metadata for
  the `ams_mel_c` target in each tree and require
  `AMS_MEL_ENABLE_TEST_FAILPOINTS` to be present in the test tree and absent in
  the production tree. This reads generated compile flags, not timestamps.
- **D. Cross-tree acceptance.** Both `CMakeCache.txt` files exist simultaneously
  in opposite modes and neither script deleted the other's tree.

Check A is the direct regression for the original defect: production build
activity can no longer compile the deterministic Navigation barriers out from
under an existing test tree.

## Scope

Build isolation only. No C ABI, export set, ABI version, provider behavior, MEL
functionality, or vendored upstream file was changed.
