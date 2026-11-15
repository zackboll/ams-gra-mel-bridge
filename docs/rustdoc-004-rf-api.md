# RUSTDOC-004 — complete safe Rust RF API documentation

## Identity and exclusive scope

Freshly fetched starting `origin/main`: commit
`8a2d409b53b90161bee01dcdd2f3641480d178d6`, tree
`05fe78b738f2b297c4d10dffeee352fad174d141`.
Branch: `docs/rustdoc-004-rf-api`. Independent implementation worktree:
`/home/zboll/git/ams-rustdoc-004`; final clean detached checkout:
`/home/zboll/git/ams-rustdoc-004-clean`. Each checkout has its own Cargo outputs
and production/test native build trees. Final commit/tree and hosted run
identities are delivery/PR evidence, avoiding a self-referential commit hash.

Only `rust/ams-mel/src/rf_admin.rs`, `rust/ams-mel/src/rf_data.rs` and this
task-owned document change. Every Rust change is a `//!` or `///` line. Removing
those lines yields byte-identical source against the base; inline test modules
are independently byte-identical. No formatting-only changes, declarations,
signatures, discriminants, implementations, tests, native ABI, dependencies,
other bindings, tooling, workflows, or publication/security policy change.
There is no warning suppression or repository-wide coverage gate.

## Coverage delta and contract review

All seven public RF types and all their public fields, variants, associated
items and methods have Rustdoc. The **53** previously missing descriptions are:

- `RfMfaState`: all 15 variants, exact Common MEL state values `0..=14`,
  distinct from IR MFA mode. Existing `ALL`, `as_raw` and checked `TryFrom`
  documentation is retained and augmented with a runnable conversion example.
- `RfJobDataFormat`: 14 published formats, raw values `0..=13`, plus `Unknown`.
  Existing `from_raw`/`as_raw` semantics retain unknown bits, with a runnable
  round-trip example. Variant descriptions do not claim payload processing.
- `RfFrequencyRange`: both Hz fields; `RfFaceInfo`: all 15 fields;
  `RfMfaInfo`: all six fields. Counts/IDs, RX/TX flags, endpoint association,
  AGC timing, request/detail lead times, four switching gaps, scheduler quantum,
  context byte limits, formats and three frequency-range collections are explicit.

Existing `RfAdmin` and `RfData` owner documentation is preserved and clarified;
all fallible public operations have `# Errors`. `open`, `is_open`,
`command_state`, `provider_version`, `quantize_duration`, `mfa_info` and `close`
describe actual behavior rather than inferred hardware effects. Module/type
documentation links actual public items. Existing provider-dependent `no_run`
examples and non-Send/non-Sync compile-fail examples remain intact.

Reviewed repository instructions, architecture/C ABI policies, provider
foundation task, RF foundation/Admin task evidence, upstream provenance and
the structured IR inventory schema. That inventory measures native/Ada
implementation, not RF Rustdoc; its implementation evidence is unchanged.
Read-only audit still reports 127 headers, 148 operations, 50 safe Ada.

Semantic authority is current Rust conversion/lifecycle code, native
`rf_admin.cpp`, `rf_data.cpp`, `abi.h`, and pinned published headers:

- RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f`:
  `StatusControl.h`, `RFMFAInfo.h`, `RFMELTypes.h`, `JobDataFormat.h`.
- Common MEL `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`:
  `MFA_Component.h` state annotations and representations.

No semantics were guessed to eliminate warnings. The pinned ComplexINT16
annotation says “8 bit” but its actual `int16_t` pair and `MELComplex<int16_t>`
type establish 16-bit components. Documentation follows the types without
modifying vendored source. VITA capacities describe storage, not valid wire
packet lengths. PDW/Local Function names do not establish payload layouts.

## Ownership and capability boundaries

- RF Admin, RF DataMEL and IR Session are independent owners. Factory calls
  execute trusted provider code behind the C facade and require compatible C++
  MEL ABIs; this is not sandboxing or a transport implementation.
- Native command-call success, provider acceptance and physical state transition
  are distinct. Rejection is `Ok(false)`; binding/native failures are `Err`.
  No hardware transition completion, telemetry or cancellation is promised.
- Native resource ownership remains private. Version strings and snapshots are
  Rust-owned and usable after closure. Snapshot conversion releases its native
  copy on every path; there are no borrowed provider pointers in returned data.
- Explicit close consumes the native owner even on failure, reports errors,
  and is idempotent. Drop is non-raising best-effort cleanup and cannot report
  shutdown failure. Uncertain provider graphs are intentionally retained for
  the process lifetime; no retry or guaranteed provider unload is promised.
- The native ABI supports deferred DataMEL shutdown with live native children.
  Safe Rust creates none of those children; copied snapshots do not defer it.
  Other owners/retained resources may keep a provider library loaded even after
  successful closure. Owner operations are statically serialized by mutable
  borrowing and non-Send/non-Sync restrictions; copied value types are independent.
- Frequencies retain Hz doubles; times retain signed i64 femtoseconds. Face IDs
  are provider identifiers, not indices. Reported counts are not reconciled.
  Collections retain order/duplicates and unknown formats; ranges are not sorted,
  merged, clamped or normalized. Capability reports are not hardware qualification.
- There is no new safe Rust RF C2, ProductRx, Virtual Aperture, RF Jobs, transmit,
  waveform, or data-processing API. Native support does not change that boundary.

## Qualification evidence

Same toolchain throughout: Rustdoc/Rustc/Cargo **1.98.1** (Rustdoc
`48a229cea 2026-09-01`). Foundation builds are offline, use fresh target
directories, warn on missing documentation and deny broken intra-doc links.

| Measurement | Exact base | Documented source |
|---|---:|---:|
| Safe `ams-mel` missing_docs | 53 (15 Admin, 38 Data) | **0** |
| Raw `ams-mel-sys` missing_docs | 2,389 | **2,389** |
| Broken intra-doc links | 0 | **0** |
| Safe doctests | inherited 48 | **50 passed** |

The two new provider-free examples execute; provider-dependent examples compile
as `no_run`, not hardware tests. All **1,435** generated HTML files pass local
file-link verification. Additional task-local checks verify generated pages for
all seven RF types, the ten owner methods, enum conversion/associated items,
all 23 public fields and all 30 enum variants, with documentation blocks.
This is not exhaustive browser/search/external-link or HTML-fragment validation.

All **1,177** raw crate/source reference files have matching SHA-256 hashes and
byte-identical generated content against the exact-base reference using the
same toolchain/flags. Raw source is untouched. Initial reports:
`/home/zboll/git/ams-rustdoc-004/rust/target/rustdoc-001/run-im903bif/qualification.json`
(base), and `run-n4le4_ga/qualification.json` in the same parent (iteration with
doctests). Dirty iteration output is not final clean-checkout evidence.

Commands (from each independent checkout; Cargo checks/tests use checkout-local
`CARGO_TARGET_DIR` and `CARGO_NET_OFFLINE=true`):

```sh
python3 scripts/rustdoc_001_foundation.py
python3 scripts/rustdoc_001_foundation.py --doctests
python3 -W error -m unittest discover -s scripts -p test_rustdoc_001_foundation.py -v
sh native/scripts/test.sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib" \
  cargo check --manifest-path rust/Cargo.toml --workspace --all-targets --locked
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib" \
  cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets --locked -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
  cargo test --manifest-path rust/Cargo.toml --workspace --locked
python3 scripts/check_ir_api_coverage.py
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

Iteration results: native CTest **656/656** (including compiled-C contracts),
production build passed via foundation `--doctests`, Rust **479** unit/integration
tests passed, **10** pre-existing provider/hardware tests ignored, **50** safe
doctests passed, no failures. All-target checking, warnings-denied Clippy,
formatting, **8/8** foundation regressions, inventory/source-integrity/newline
and whitespace checks passed. No test was weakened or enabled with real hardware.

Final clean committed-checkout results, commit/tree, report identities and hosted
Bootstrap/Ada documentation checks must be recorded in delivery/PR evidence
against final HEAD. No Ada source changed, so no Ada formatter run is required.
Local Ada/provider/hardware qualification is not claimed for this Rustdoc slice.

## Remaining limits and disposition

No unresolved source meaning blocks documentation. Provider-specific open
additions, quantization rounding/ties, actual capability validity and physical
effects remain provider-defined or unqualified; this task does not invent them.
Resource reclamation and provider callback quiescence are not implied by close.
The raw crate's 2,389 warnings intentionally remain outside exclusive ownership.

`READY_FOR_INDEPENDENT_REVIEW` is conditional on the final clean-checkout
qualification and hosted Bootstrap/Ada documentation checks passing on the final
PR HEAD. Delivery leaves the non-draft PR open, unmerged, without auto-merge or
self-approval; this document does not substitute for those final results.
