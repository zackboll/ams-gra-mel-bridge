# RUSTDOC-002 — core safe Rust API documentation

## Identity and boundaries

Freshly fetched `origin/main` and exact starting commit:
`084a8906f3cd729b7a20a921f39bd513d8ac02c2`, tree
`12f97090b9e12a3a9b43773cb0c4390bc154e520` (includes RUSTDOC-001, PR #201).
Branch: `docs/rustdoc-002-core-api`. Independent worktree:
`/home/zboll/git/ams-rustdoc-002`. Final commit/tree, hosted run identities and
review disposition are recorded in the PR/delivery evidence, avoiding an
impossible self-referential enclosing commit/tree hash in this document.

Only `rust/ams-mel/src/lib.rs` and this task-owned document change. All Rust
source changes are `///`/`//!` comments. No behavior, declaration, exported symbol,
native ABI, dependency, workflow, publication/security policy, sys crate,
IR/RF module, Ada or Python changes. Existing accurate architecture and feature
comments are retained. Two overbroad weak-view statements are clarified: local
`is_open` and view-only `close` do not behave like expired work operations.

Reviewed repository guidance, architecture/C ABI policies, provider-foundation
task, RUSTDOC-001 tooling/evidence, common-channel and Image control-only task
contracts, native ABI and lifecycle implementation, pinned capability/frame
headers, Rust conversion/cleanup code and existing tests. The structured IR
inventory/schema classifies native/Ada implementation evidence, not Rustdoc;
no classification/evidence changes are appropriate. Its read-only audit passes
unchanged: 127 headers, 148 operations, 50 safe Ada. Historical coverage remains
untouched; this document owns the documentation coverage delta.

## Public declarations covered

- `Session`, `SessionOptions`, `max_async_requests`, `open`, `open_with_options`,
  `provider_version`, Image acquisition/control and C2 opening, consuming `close`.
- `ImageConfig::{new,with_limits}`, `ImageControlConfig::new`,
  `ControlConfig::{new}`, `ImageStream` lifecycle, receive, counters, weak view,
  and the error contract on its existing core metadata opening method.
- `ControlChannel`: explicit/idempotent enable, Operate and BIT-NoOp submission,
  weak view, retryable close and handle-only `is_open`.
- `ChannelView`: weak ownership, expiration, local handle state, KeepAlive,
  CommsTest, complete capabilities and view-only close.
- `ModeRequest`, `ReturnRequest`, `CommsRequest`: independent native graph,
  polling/finite waits, cached terminal outcome, rejection versus Rust failure,
  non-cancelling timeout/Close/Drop and best-effort cleanup.
- `AbiVersion`, `ProviderVersion`, `abi_version`, `Error`, `ErrorKind`, `UciId`,
  `ComponentLocation`, their fields/variants/accessors and construction errors.
- All core variants/fields of `ImageType`, `ImageFlip`, `MfaMode`, `CommandReturn`,
  `MelErrorCode`, `ModeResult`, `ReturnResult`, `CommsTestRequest`,
  `CommsTestReport`, `CommsTestResult`, `PixelFormat`, `SensorType`, `ChannelType`,
  `MetadataCapability`, `BandType`, `CoordinateSystem`, `BandInfo`, `ImageBand`,
  `ChannelCapability`, `Frame`, and `StreamCounters`.

Ownership precedes mechanics: copied Rust values versus native owners versus
weak views; acquisition versus control-only attachment; logical closure versus
physical teardown/retention; consuming versus retryable close. Submission,
provider acceptance, future completion and physical execution remain distinct.
No new thread-safety, cancellation, callback-quiescence or immediate-unload
guarantees are asserted. No dedicated `# Panics` sections are invented: these
APIs establish no intentional argument-triggered panic contract; ordinary Rust
allocation/runtime behavior and caller-provided conversion implementations are
not promoted to a new no-panic guarantee.

## Measurement and examples

Same installed toolchain for baseline and final-source runs: Linux x86_64,
Cargo 1.98.1 (`797e8a9bc 2026-08-05`), rustc/rustdoc 1.98.1
(`48a229cea 2026-09-01`), CMake 3.31.6, GCC/G++ 14.2.0 (Debian 14.2.0-19).
No toolchain/dependency downloads or new repository-wide threshold.

| Diagnostic | Starting `ams-mel` | After `ams-mel` | Starting `ams-mel-sys` | After `ams-mel-sys` |
| --- | ---: | ---: | ---: | ---: |
| `missing_docs` | 572 | 333 | 2,384 | 2,384 |
| broken intra-doc links | 0 | 0 | 0 | 0 |
| other Rustdoc diagnostics | 0 | 0 | 0 | 0 |

Reduction: **239** safe-crate warnings. No remaining missing-doc diagnostics
originate in `lib.rs`. Remaining 333 warnings originate in the separately owned
IR/RF modules; they are not suppressed or documented indirectly here.

Four new runnable, provider-free examples cover owned UCI identity construction,
Image configuration construction, completed-failure value handling and unknown
pixel-format handling. Two explicitly `no_run` compile-only examples cover
provider session opening/version/closure and polling an already submitted mode
request. Existing compile-only/control-only and compile-fail lifetime/trait
examples remain. All **40** safe-crate doctests pass, with no ignored doctests;
all four new runnable examples execute without a provider/hardware. The two
new provider/request examples are compiled, not executed.

## Actual local qualification

Commands run from the absolute worktree above. The foundation script always
creates its own fresh Cargo target under this worktree. Other Rust checks use
the independent target
`/home/zboll/git/ams-rustdoc-002/rust/target/rustdoc-002-checks` with
`CARGO_NET_OFFLINE=true`. Native production/test builds are this worktree's own
separate `native/build` and `native/build-tests`, never another worktree's trees.

```sh
python3 scripts/rustdoc_001_foundation.py
python3 scripts/rustdoc_001_foundation.py --doctests
python3 -W error -m unittest discover -s scripts -p test_rustdoc_001_foundation.py -v
python3 -W error -m unittest discover -s scripts -p test_doc_source_integrity.py -v
python3 -W error -m unittest discover -s docs/tests -p test_docs_ci.py -v
make test-native
CARGO_TARGET_DIR="$PWD/rust/target/rustdoc-002-checks" CARGO_NET_OFFLINE=true \
  AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
  AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
  cargo test --manifest-path rust/Cargo.toml --workspace --locked
CARGO_TARGET_DIR="$PWD/rust/target/rustdoc-002-checks" CARGO_NET_OFFLINE=true \
  cargo check --manifest-path rust/Cargo.toml --workspace --all-targets --locked
CARGO_TARGET_DIR="$PWD/rust/target/rustdoc-002-checks" CARGO_NET_OFFLINE=true \
  cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets --locked -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
python3 scripts/check_ir_api_coverage.py
python3 scripts/check_final_newlines.py
git diff --check
```

Results: foundation regression **8/8**; source-integrity regression **18/18**;
documentation CI regression **18/18**; native CTest **587/587** (including C11
translation units); workspace **508 passed, 0 failed, 9 ignored** across 33
result groups, including the 40 doctests. The nine existing ignored cases are
subprocess-entry harnesses exercised by their parent tests, not new omissions.
All-target check, warnings-denied Clippy, Rust formatting, inventory audit,
final-newline and whitespace checks pass. No warning suppressions added.

Baseline report: `rust/target/rustdoc-001/run-2u0he5ny/qualification.json`, recorded
on exact starting HEAD with empty Git status before any tracked edits. Final
source iteration reports: `run-nfccnf_b` (HTML), `run-4v1d4gz8` (HTML/doctests),
in the same parent directory. These iteration reports truthfully identify dirty
source, not a committed artifact. Clean committed-source reruns and identities
are recorded on delivery. Full logs stay in ignored local target directories.

Both references build with `--deny rustdoc::broken_intra_doc_links --warn missing_docs`;
all **1,430 HTML files** pass the foundation local-file-link verifier. Additional
HTML checks find all **38** core type pages with docblocks and expected method
anchors on Session, ImageStream, ControlChannel, ChannelView and all three core
request owners. Intra-doc diagnostics are zero, not merely an HTML file check.
The verifier's existing limitation remains: it is not a browser/search or
exhaustive HTML-fragment/external-link validator.

All **1,172** generated `ams_mel_sys/` and `src/ams_mel_sys/` files have identical
SHA-256 hashes before and after. Raw source is unchanged; unchanged warning
counts alone are not the only evidence. Safe-crate source equality is verified
against the exact base after removing only lines starting with `///` or `//!`
(allowing indentation): all remaining text is exactly equal. Restricted source,
ABI and workflow paths are absent from the diff.

## Limitations and unresolved semantics

- No real-provider interoperability, detector activity or hardware execution is
  qualified; native/Rust integration uses existing separate mock providers.
- The pinned capability image-size getter does not independently specify its
  unit; docs preserve its value rather than inventing a byte/pixel conversion.
- Pinned frame `getNumBands()` naming and its spectral-band comment disagree;
  docs explicitly preserve this ambiguity rather than choose a new meaning.
- Provider version-number encoding, time epoch, reference text conventions,
  unsupported formats and physical ranges are not guessed.
- IR/RF module documentation remains with RUSTDOC-003/RUSTDOC-004. No mandatory
  repository-wide missing-doc threshold or implementation coverage claim is added.
- Ada/Alire, Pages publication and real-provider suites are not claimed as local
  qualification for this documentation-only slice. Hosted checks are reported
  separately on the exact final PR HEAD, without self-approval or merge.
