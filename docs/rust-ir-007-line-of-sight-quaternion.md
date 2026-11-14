# RUST-IR-007 — Complete safe Rust LineOfSightQuaternion reception

## Source preflight and scope

Freshly fetched `origin/main`: commit
`648d08bfc5d3c8a92c52680f5b1c42bbd515a1e8`, tree
`3ca2c4bf3ff74ea8e71c81a803e8e309d2a69b4c`. GitHub confirms PR #175
merged at that commit on 2026-11-10; its safe ODM implementation,
`ForwardRightDown` and single shared test allocator are present. No overlapping
open PR or Rust quaternion branch was found. The existing merged native
quaternion branch/PR #170 was identified, not modified. Worktree:
`/home/zboll/git/ams-rust-ir-007`; branch `feature/rust-ir-007-los-quaternion`.
All unrelated worktrees and branches are preserved.

Authoritative sources inspected: `native/src/ir_image_los_quaternion.cpp`,
`native/include/ams_mel/abi.h`, `rust/ams-mel-sys/src/lib.rs`,
`rust/ams-mel/src/ir_optical_distortion_map.rs`,
`native/tests/test_ir_image_los_quaternion.c` and
`docs/irmel-ada-010-line-of-sight-quaternion.md`. The structured Ada coverage
inventory remains unchanged: its existing native/Ada implementation is complete;
this task adds the previously raw-only safe Rust reception surface. The pinned
IR upstream commit is `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`, as recorded in
`docs/reference/irmel-upstream-git-evidence.json`.

This is a Rust-only vertical slice. Native implementations, exports, raw
bindings, providers, Ada, Python, CI policies and historical `docs/coverage.md`
are not changed. No new C functionality, production test hooks, dependency,
allocator, processing algorithm, provider-private protocol or callback thread is
introduced. The task-owned mutation runner is an offline developer test script.

## Complete field mapping

| Published upstream getter | Owned Rust field | Representation |
|---|---|---|
| `getSystemTime().count()` | `system_time_ns` | signed `i64` nanoseconds |
| `getQ_xyzw().getQuaternionX()` | `q_xyzw.x` | `f64` |
| `getQ_xyzw().getQuaternionY()` | `q_xyzw.y` | `f64` |
| `getQ_xyzw().getQuaternionZ()` | `q_xyzw.z` | `f64` |
| `getQ_xyzw().getQuaternionW()` | `q_xyzw.w` | `f64` |
| `getPqr_rps().getForward()` | `pqr_rps.forward` | `f64` |
| `getPqr_rps().getRight()` | `pqr_rps.right` | `f64` |
| `getPqr_rps().getDown()` | `pqr_rps.down` | `f64` |

`Quaternion` and `LineOfSightQuaternion` are `Clone + Copy + Debug + PartialEq`
owned values. `pqr_rps` reuses the existing public ODM `ForwardRightDown` type;
there is no second representation of that upstream type. Every field is assigned
directly from the existing fixed C record. The complete signed nanosecond range,
including `i64::MIN` and `i64::MAX`, is preserved. There is no inferred epoch,
normalization, sign canonicalization, rounding, clamping, frame transformation,
reordering or inferred unit conversion. In particular q and −q remain distinct,
and norm²=348 remains 348, not 1. Zero, signed zero, subnormals, finite extrema,
infinities and NaNs are retained; NaN payload-bit preservation is not promised
beyond the native contract. `PartialEq` has ordinary IEEE/Rust behavior (NaN is
not equal to itself; signed zeros compare equal); fidelity tests use bit
comparisons for non-NaNs. Reception establishes neither attitude accuracy nor
suitability for estimation or independent pointing calculations.

## Registration and ownership contract

`ImageStream::open_line_of_sight_quaternion` returns a unique
`LineOfSightQuaternionStream` with `is_open`, `receive`, `counters` and `close`.
It implements neither Clone, Send nor Sync; three compile-fail doctests enforce
these exclusions. No parent borrow is retained, and the consumer does not keep
Image or Session open.

Closed-owner preflight returns ProviderFailed before capacity validation. Zero
capacity returns InvalidArgument locally and does not consume the native
registration one-shot. Attached and Running streams are eligible; Stopped is
rejected. Actual registration is one-shot even on failure. Ordinary metadata,
NUC, ODM and CameraCommandResp have independent native registration one-shots.
The wrapper never retries registration or parses diagnostic text.
`LineOfSightQuaternionOpenError::error()` retains the checked native error;
`registration()` retains a known provider Return, including Success,
NotSupported, Fail, BadPointer and NotImplemented. Local failures, unknown
Returns and exceptions do not fabricate a known Return. Success requires both
successful registration and a non-null owner. Unexpected failure owners remain
guarded for cleanup.

The six existing native exports are the only quaternion operations used:
`ams_mel_ir_image_los_quaternion_open`, `_receive`, `_get_counters`, `_close`,
`_event_view` and `_event_close`. Existing sys types are reused without mirrors.
The native FIFO is bounded and DROP-INCOMING. `ImageMetadataCounters` retains
all three saturating `u64` fields without narrowing.

Receive performs exactly one native operation. Zero polls; every other `u32`
timeout passes through unchanged, never meaning cancellation. An event RAII
guard owns any published event before diagnostics are decoded, including native
failures that unexpectedly publish an event. Checked FFI helpers reject null or
misaligned record views and malformed diagnostics before dereference/copy. The
trusted C facade remains the memory-provenance boundary: structural checks do
not establish that arbitrary hostile addresses are mapped. Every field is
copied while the native event is alive; successful event Close precedes return.
Failed receive/view/copy, diagnostic errors and unwinds also release the event.
No collection allocation is needed for this fixed record.

Close forgets local ownership before native release and attempts it once, even
if release status or diagnostics are uncertain. Subsequent Close is idempotent.
Drop is non-panicking, best-effort, and uses allocation-free null diagnostics.
Fail-safe handling of an actually incomplete but uncertain release can leak
native state rather than retry a potentially freed pointer. Close does not
unregister callbacks, stop Image, close Session or imply callback quiescence.
Queued values drain before StreamStopped/ProviderFailed on parent teardown;
consumer Close discards its queue. Native physical teardown still bears the
in-flight callback barrier and provider-unload responsibility.

## Coverage delta

- Ten private unit tests exercise hostile FFI: null successful event, null and
  misaligned view, failed view, malformed diagnostic size/terminator/content,
  invalid UTF-8 during receive/view, status failures publishing an event,
  event-close error/uncleared handle/malformed diagnostic, conversion rejection,
  receive/conversion unwind, unexpected failed-open ownership, full-width
  counters, local preflights and once-only uncertain consumer release.
- Twenty-six process-isolated integration scenarios exercise all eight native
  C11 fixture families, synchronous Open delivery, signed timestamps, XYZW/FRD
  order, opposite signs, non-unit and IEEE edge values, FIFO/drop/counters,
  malformed callbacks, snapshot allocation failure and recovery, one-shots and
  provider registration outcomes, Attached/Running/Stopped eligibility,
  independent base/NUC/ODM registration, stop/drain, failed-detach/drain,
  Image/Session-first teardown, consumer Close/Drop, late callbacks and actual
  provider unload. Values are rechecked after unload.
- Blocking Receive with `u32::MAX` and in-flight callback teardown use existing
  bounded native probes, not arbitrary sleeps. Safe owners never cross threads.
  The ignored subprocess entry is executed by each parent scenario.
- Existing C11 tests continue to qualify native CameraCommandResp independence
  and native physical teardown. No new public camera-response Rust API is
  introduced to test this unrelated feature.

## Qualification evidence

Commands use `AMS_MEL_NATIVE_LIB_DIR=$PWD/native/build-tests/lib` and
`AMS_MEL_TEST_PROVIDER_DIR=$PWD/native/build-tests/test-providers` for repository
Rust tests. Independent downstream qualification uses the production
`native/build/lib` instead.

Initial native tests passed 523/523; Alire build and smoke passed with 46 PASS
reports. Alire compiler selection resets the native build trees, so native
tests were restored (523/523) afterward. Initial mutation/Rust attempts raced
that artifact reset and are not acceptance evidence: nine mutation link failures
were invalid, not kills. An early reproducible-run classifier also rejected a
runtime failure at an unwrap rather than the required named assertion; the
uncertain-release test now retains safe backing and checks the release count
explicitly. Final qualification is sequenced after all compiler-changing and
build-isolation gates. No tests or CI policies were weakened.

| Gate | Command / qualification | Result |
|---|---|---|
| Production native | `make native` | PASS |
| Full native | `make test-native` | PASS, 523/523 |
| Focused C11 | `ctest --test-dir native/build-tests -R '^ir_image_los_quaternion' --output-on-failure` | PASS, 3/3 |
| Rust workspace | `cargo test --manifest-path rust/Cargo.toml --workspace` | PASS, 383 tests including 25 doctests; nine ignored subprocess entries exercised by parents |
| Focused Rust | Unit filter `ir_los_quaternion --lib` and `--test ir_los_quaternion`, each at 1, 4 and 16 threads | PASS, 10 unit + 26 integration tests at each setting |
| Optimized Rust | Same focused selections with `--release` | PASS, 10 unit + 26 integration tests |
| Rust analysis | `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets`; `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS |
| Rust format/docs | `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`; `RUSTDOCFLAGS='-D warnings' cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | PASS |
| Python | `make test-python` | PASS, 456 tests; compileall PASS |
| Policy | `make test-policy` | PASS, 234 tests across seven suites |
| Build isolation | `make test-build-isolation` | PASS, both trees coexist and failpoints remain test-only |
| Alire | `alr -n -C ada build`; `alr -n -C ada/tests run` | PASS, 46 smoke PASS reports |
| Final repository gate | `PATH=<installed GPRbuild bin>:$PATH make check` | PASS, native 523/523, Ada format/smoke, 52 IR coverage audit tests, final newlines and Git whitespace |
| Source integrity | `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files / 3 workflows |
| Downstream | Independent Cargo package, production facade, separate mock provider | PASS, all six symbols resolve; no test-hook imports; values valid after provider unload |
| Production ABI | Sorted `nm -D --defined-only` symbol names, starting main vs feature | PASS, identical 295 versioned function exports; zero additions/removals |

The identical sorted production symbol inventory SHA-256 is
`8340a0e4d88ed66e26baa5437c144a130de6022ab686d30cedc151bf7f602489`.
The independent downstream package supplies `LD_LIBRARY_PATH` explicitly because
Cargo's downstream executable does not inherit the dependency's executable rpath.
Its first run without that runtime search path failed to locate the production
DSO; the corrected production-path run passed. No production consumer imports
`ams_mel_test_*`. Repository native/raw/provider/Ada/Python sources and existing
ODM/C2/NUC/ordinary Image Rust modules are byte-identical to starting main.

## Compiled mutation qualification

Reproduce with `python3 scripts/test_rust_los_quaternion_mutations.py` after
`make test-native`, with no concurrent rebuild of the native libraries. The
runner copies the Rust workspace privately, uses an isolated Cargo target,
requires an unmodified baseline, compiles each mutant before execution, checks
an actual runtime assertion failure, restores original source after each
experiment, and reruns the restored unit and integration baselines. The
checkout source hash before/after is identical:
`ca6c176c6487a81adf0137e6055ee124d22a40555d421d094470d5d416eb883d`.

Final campaign: **9 applied, 9 compiled, 9 executed, 9 runtime-killed,
0 invalid, 0 surviving**. Compile failures never count as kills.

| Mutation | Runtime detection |
|---|---|
| Swap quaternion X/W | `fidelity`, exact component comparison |
| Zero quaternion Z | `fidelity`, exact component comparison |
| Swap forward/right rates | `fidelity`, exact rate comparison |
| Clear signed time sign bit | `fidelity`, signed timestamp assertion |
| Narrow time through `i32` | `fidelity`, `i64::MIN` timestamp assertion |
| Normalize the norm²=348 quaternion | `fidelity`, non-unit component assertion |
| Fabricate Fail for an unknown registration return | Failed-open registration preservation assertion |
| Omit native event cleanup | Event guard exactly-one-release counter assertion |
| Retain/retry an uncertain native release | Consumer exactly-one-release counter assertion (safe retained fake backing) |

Earlier exploratory attempts included build-artifact races and an overly
strict failure classifier; they were discarded, not aggregated into this final
campaign's counts. The final complete campaign has no invalid or surviving
mutants, and the restored baseline passes. Local raw logs and the independent
downstream consumer are retained in `/tmp/rust-ir-007-evidence` on the
qualification host; the committed runner makes the mutation campaign repeatable.

## Limitations and delivery evidence

All positive payload/lifecycle evidence uses the separate deterministic mock
provider. No physical IR camera, Squall, credentials, network service or hardware
qualification is claimed. Provider C++ ABI compatibility and compliance with the
published native memory/lifetime contract remain prerequisites. The PR records
the immutable feature commit/tree and exact-head hosted Bootstrap/Documentation
results; incomplete hosted checks are pending, never passes. No self-approval,
auto-merge or merge is performed.
