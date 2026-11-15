# RUST-CRATE-001 — Extract provider-independent Common MEL types

Base: `1a3f3efbd5ff2520697fed0c924f9e4eb1c76ebd`, tree: `8b2e87fe4492f76fd32b857dac87a7b4434b5c07`. No native, API or ABI behavior changes.

## Layering and ownership

`ams-mel-common` now owns `AbiVersion`, `ProviderVersion`, `Error`, `ErrorKind`,
`UciId` and `ComponentLocation`. It has no provider or native C ABI dependency
and forbids unsafe code. `ams-mel` re-exports the **actual same types** via `pub use`,
not duplicates or wrappers, and retains all existing IR/RF owners and their inherent
methods. `ams-mel-sys` remains the sole crate with `links = "ams_mel_c"`.

Current graph: `ams-mel` -> (`ams-mel-common`, `ams-mel-sys`).
There are no cycles. IR `Session`, `ImageStream`, `ControlChannel`,
`ChannelView`, metadata and `NavigationReport` implementations remain together
in `ams-mel` until their subsequent owner extraction. RF Admin and DataMEL
also remain there until RF extraction.

The original `ams_mel::...` imports continue to resolve for existing consumers.
Provider-to-Rust conversion reconstructs Common values through validated
constructors rather than exposing private representation. NUL, diagnostic
completeness, unknown native error status, timestamps and physical data are
not normalized or reinterpreted. Native transport retains dedicated hidden
constructors; these do not add cancellation/cleanup or provider guarantees.

## Rustdoc and CI

The original Ada-only qualification boundary is unchanged. The source-bound,
offline Rustdoc generator adds the Common crate's own reference under
`rust/ams_mel_common/` and verifies its entire generated file inventory in the
existing same-run publication. The safe `rust/ams_mel/` and raw
`rust/ams_mel_sys/` entry points remain in place. Tests for the foundation and
Rustdoc publication fixture include the third crate. No new completion gates
or changes to main-push deployment authorization are introduced.

## Required qualification

Run exact-base comparison and clean-final-head `cargo test --workspace --locked`,
`cargo check --workspace --all-targets --locked`, warnings-denied Clippy, and
`cargo fmt --all -- --check`. Run focused Common type-identity tests,
existing native CTest, Rustdoc foundation and publication tests, complete Docs CI,
the local project-prefix browser checks, and final hosted Bootstrap/Documentation.
Check no old Rust public import breaks. Verify only `ams-mel-sys` links
`ams_mel_c`, no new unsafe code, and no source/ABI changes outside Rust.

## Next steps

RUST-CRATE-002: extract RF Admin/DataMEL owners into `ams-mel-rf`.
RUST-CRATE-003: extract the IR `Session` owner and all its inherent methods into
`ams-mel-ir`; avoid cross-crate inherent impls. RUST-CRATE-004: reduce
`ams-mel` to its compatibility facade and qualify documentation migrations.
Each step must preserve shared type identity, legacy consumer imports and ABI.
