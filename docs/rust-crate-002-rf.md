# RUST-CRATE-002 — Extract RF Admin/DataMEL

## Exact source base
`ad35ec4d5fc2faa9296b530defad108bf1dbac31` (Git tree `e9a72310c6453c3edce599f44c6472b6da40f46d`).

## Ownership and dependency contract

- New independent crate `ams-mel-rf` owns the existing `RfAdmin`, `RfData`,
  `RfMfaState`, `RfJobDataFormat`, `RfFaceInfo`, `RfFrequencyRange` and `RfMfaInfo`
  declarations and all RF-only implementation, error, conversion and snapshot behavior.
- `ams-mel` remains the compatibility entry point; it re-exports the same concrete
  RF types without wrappers or conversions. Legacy RF integration tests remain
  exercised through `ams_mel`; the new crate adds RF-only consumer tests.
- `ams-mel-common` continues to own `Error`, `ErrorKind`, `ProviderVersion` and
  other shared values. It has no dependency on the FFI or RF/IR owner crates.
- `ams-mel-sys` remains the sole `links = "ams_mel_c"` native ABI/link crate.
- Dependency DAG: `ams-mel` → `ams-mel-rf` → (`ams-mel-common`, `ams-mel-sys`);
  `ams-mel` also retains its direct IR dependencies. No cycles or new native ABI.
- `ams-mel-rf::transport` is a **private**, scoped copy of the existing ABI
  validation helpers until a later Common/IR support extraction reconciles the
  architecture. It preserves exact current diagnostic buffers/status mapping,
  NUL/UTF-8 handling, checked spans, fallible copies, two-stage provider versions
  and error metadata. Do not alter native error semantics merely to remove duplication.
- RF Admin and RF DataMEL remain independent owner graphs, intentionally
  neither `Send` nor `Sync`. Native Close, Drop and snapshot retention preserve
  their existing contracts; no provider/hardware qualification is implied.

## Source and documentation boundaries

Only Rust workspace metadata, the two RF implementations, a private transport
module, tests, docs, and existing Rustdoc declaration/inventory controls change.
No Ada, Python, native C/C++ or C ABI, provider, GitHub workflow permissions,
or shared GNATdoc controls are modified.

The published legacy Rustdoc destinations, including `ams_mel/index.html`
and RF item pages, must continue to resolve. Add independent
`rust/ams_mel_rf/index.html` and complete RF source/search assets while
retaining Common and raw ABI references. Reject any broken links or stale
published file inventory; preserve main-only deployment authorization.

## Required qualification

- Check the original `ams_mel::RfAdmin`/`RfData` and value imports still
  compile and have identical types to `ams_mel_rf` definitions.
- Verify `ams-mel-rf` depends only on Common and Sys and no other crate links
  `ams_mel_c` independently.
- Run `cargo test --workspace --locked`, `cargo check --workspace --all-targets --locked`,
  warnings-denied Clippy and `cargo fmt --all -- --check`.
- Run native CTest and original RF lifecycle/provider/negative tests,
  including snapshot teardown and uncertain-shutdown child-process scenarios.
- Check offline Rustdoc for all four crates, safe+RF doctests, publication
  integrity, browser/search paths, and Ada/GNATdoc security regression suites.
- Independently verify the resulting source and generated site from a clean
  checkout and pass hosted Bootstrap/Documentation on the exact PR HEAD.

No changes to the actual provider / hardware behavior or RF feature coverage
are authorized. Leave the PR open and unmerged for independent review.
