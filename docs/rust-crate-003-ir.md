# RUST-CRATE-003 — Extract safe IR MEL crate

## Scope and source identity

Starting verified Git main: `3e462a6d5448918813a81b45653f9340a66b7c3d`. This is the IR extraction phase
following merged RUST-CRATE-001 Common and RUST-CRATE-002 RF. The final commit
and tree are recorded in the pull request; no source file self-references its hash.

## Ownership and dependency graph

The new `ams-mel-ir` crate owns the original IR `Session`, `ImageStream`,
`ControlChannel`, `ChannelView`, `CommsRequest`, typed
`HealthStatusChannel` and `StackedChannel`, navigation requests, C2 modes,
BIT, NUC, image/health/C2/common communications metadata, discovery, and all
IR-only conversion and lifecycle helpers. All inherent `impl` blocks remain
with their defining owner. Unit tests and private allocation-failure fixtures
remain with that crate.

The old `ams-mel` crate becomes a strict compatibility façade, re-exporting
the exact IR and RF owners and values. No duplicate Rust struct, new wrapper,
implicit conversion, or additional native owner is introduced.
`ams-mel-ir` depends only on `ams-mel-common` and `ams-mel-sys`; it must
not depend on `ams-mel` or `ams-mel-rf`. The dependency graph remains
acyclic. `ams-mel-common` remains independent of the native ABI and forbids
unsafe Rust. `ams-mel-sys` remains the **sole** crate with
`links = "ams_mel_c"`; IR, RF and facade build scripts may emit the existing
local library search and RPATH metadata, never duplicate `rustc-link-lib`.

All existing source-owned IR files move together, preserving their non-Rustdoc
source bytes, nested modules and native error semantics. Standalone IR doctest
crate paths change from `ams_mel::...` to `ams_mel_ir::...`; the facade
continues to support the original imports and methods. Legacy integration tests
and examples remain in `rust/ams-mel` and exercise the compatibility façade.
New direct-IR consumer and cross-crate type-identity tests qualify the boundary.

## Provider and ABI behavior

No native C ABI, C++ bridge, provider implementation, Ada or Python binding
changes are authorized. Do not change the meaning of accepted/submitted
requests, callback draining, one-shot cleanup, pointer/span validation,
fallible allocation, NUL/UTF-8 handling, unknown numeric values,
diagnostic completeness, request cancellation, or owner lifetime.
IR owners remain non-`Send`/non-`Sync` where previously required; unit
and integration tests still validate their contracts. This refactor does not
establish real hardware interoperability or AMS GRA system compliance.

## Rustdoc

Add the independently qualified `/rust/ams_mel_ir/index.html` and IR source,
search and generated assets to the existing five-crate Rustdoc output.
Preserve `/rust/ams_mel/index.html` and former inline item URLs, especially
`/rust/ams_mel/struct.Session.html`. Common, RF and low-level FFI documentation
must remain intact. The Ada GNATdoc generation, verified Ada inventory,
reversible portal-home insertion, artifact digests and main-push-only
deployment gate remain unchanged. Every artifact file must be exhaustively
verified, not sampled. PR deployment and live checks must remain skipped.

## Qualification and review

- Independently compare public export inventory and the moved IR sources;
  do not mistake moving code for adding coverage.
- `cargo check --workspace --all-targets --locked`, complete
  `cargo test --workspace --locked`, Clippy `-D warnings`, Rustfmt check;
  repeat with the repository's production/test native trees where applicable.
- Original IR lifecycle, callback, error, resource, navigation and race
  tests; all RF/Common compatibility tests and ABI tests.
- Offline pinned Rustdoc for all five crates, safe IR/compatibility/RF doctests,
  zero broken intra-doc links, published item URL and site traversal checks.
- Policy/security, Ada/Python/native GCC/Clang Debug/Release matrix and
  exact final-HEAD hosted Bootstrap and Documentation.
- Independent technical review of moved owner graphs, type identity,
  unmodified behavior, artifact provenance and current-main mergeability.

Leave the PR **draft, unmerged, auto-merge disabled** until the exact final
HEAD and reviewer evidence are complete. No approval or merge is implied.
