# RUST-IR-013 — complete safe Rust Control capability discovery

## Source, scope and coverage delta

Freshly fetched base `origin/main`: `76631880ee236b9839b8b9668be36e423ef1b050`.
Base tree: `e21e61cf735ac37d56588e5798d997ebe67e5d8b`.
PR #197 (Image control-only) is merged at that commit; PR #195 (Python
discovery) is merged at `0a3c694ee6650e0641535edf7d3056432096b81a`.
Both were verified as ancestors. Preflight found no open PRs or overlapping
remote Rust discovery branch. Dedicated branch:
`feature/rust-ir-013-control-discovery`; worktree:
`/home/zboll/git/ams-rust-ir-013`. Other worktrees and published history remain
untouched. Exact final commit/tree and hosted qualification belong in the PR
handoff, avoiding a self-referential source hash.

Reviewed repository instructions, contribution and language policies,
architecture/C ABI contracts, provider-foundation task, structured IR inventory
and schema, native discovery implementation/C11/allocation tests, Ada/Python
discovery, raw Rust declarations/ABI tests, and existing Rust capability
conversion. The structured inventory already records native/Ada exposure of
`ir:operation:ams::iface::irmel::Control::getCapabilities() const` from pinned IR
commit `8d9224519f12b44e0b28815755c56a32a28d24a0`; this task changes only safe
Rust exposure, not those classifications or upstream evidence.

New API: `Session::capabilities(&self) -> Result<Vec<ChannelCapability>, Error>`.
It returns the existing complete owned representation, before attachment.
Provider order and duplicate entries/types are retained. Empty discovery
returns an empty vector. Zero Image/StackedImage layouts are valid and remain
zero. No filtering, sorting, attachment, enablement, callbacks, allocation of
acquisition buffers or registration occurs. Advertisement establishes neither
command execution, telemetry, successful attachment nor physical functionality.

Changed-file inventory (all paths relative to this worktree):

- `rust/ams-mel/src/lib.rs`: existing snapshot helper's private injected-call
  seam, aligned-record check and one-attempt entry release.
- `rust/ams-mel/src/ir_discovery.rs`: public method, private list owner and
  ordered discovery implementation.
- `rust/ams-mel/src/ir_discovery_tests.rs`: deterministic hostile ABI tests.
- `rust/ams-mel/tests/ir_discovery.rs`: public unchanged-provider scenarios.
- `docs/rust-ir-013-control-discovery.md`: task-owned evidence and boundaries.

No native implementation/ABI, sys declarations, Ada/Python, mock fixtures,
vendor source, dependencies, CI/policies, historical archive or existing
acquisition/command behavior is changed.

## Native mapping and complete field preservation

| Phase | Existing native function | Ownership |
| --- | --- | --- |
| Snapshot Control list | `ams_mel_ir_capability_list_create` | Initially-null guarded list output |
| Count | `ams_mel_ir_capability_list_count` | List remains live; zero valid |
| Ordered entry | `ams_mel_ir_capability_list_get` | Independent initially-null entry owner |
| Borrow and copy | `ams_mel_ir_channel_capability_view` | Existing `capability_snapshot` and `capability_from_raw` |
| Release entry | `ams_mel_ir_channel_capability_close` | Before next entry or on failure |
| Release list | `ams_mel_ir_capability_list_close` | Before success or on failure |

The existing decoder is unchanged. It copies all channel UUID bytes and exact
UTF-8 label; height, width, bit depth, row pitch, buffer size, image size, band
count, pixel format; ordered sensor types; complete platform identity;
component X/Y/Z metre offsets, key and system name; ordered channel types;
task schedule depth; ODC/NUC availability; ordered metadata capabilities;
ordered ImageBand indices with every nested BandInfo kind and both wavelength
values; and ordered navigation coordinate frames. Integer widths, double
values, strings and duplicate/list ordering are not narrowed or normalized.
Existing `Unknown(raw)` enum mappings and distinct reserved alternatives are
reused. No geometric/physical validation or inferred pixel layout is added.

All returned nested values are Rust-owned. Native entries and list have
already closed before public success. Results survive Session closure and
actual provider unload. Repeated discovery produces independent allocations.
Native snapshot data retains no provider objects or executable code.

## Cleanup and failure contract

The private list guard exists before Create, including a non-null error output.
The existing entry helper is reused for each Get; its private call seam enables
hostile-output tests without dereferencing fabricated handles. One narrowly
demonstrated correction was needed: the prior entry Close could be retried by
Drop if native returned an error without clearing the slot. Both temporary
owners now disarm **before** release, so a possibly consumed owner is never
released twice. Successful Close must clear its slot. Uncertain ownership is
never retried; an ABI violating release that refuses consumption can retain
memory rather than risk a double free. Ordinary native Close consumes/clears
the owner, and normal/error conversion paths do not leak owners.

Explicit release errors are returned when otherwise successful. On a primary
create/count/get/view/conversion failure, diagnostic-free Drop attempts entry
then list release once and preserves the primary error. A partial converted
prefix is dropped, never published. Count multiplication is checked against
overflow and `isize::MAX` before fallible exact vector reservation and retrieval.
All nested allocation uses the existing fallible converter. No native operation
is retried to obtain diagnostics. Malformed statuses/diagnostics do not panic.

Native ProviderFailed, ProviderException and InternalError categories and
diagnostics remain intact for valid native diagnostic outputs. Invalid owner,
view/alignment, count, span or boolean representation returns
ProtocolInconsistency; invalid UTF-8 returns InvalidUtf8; reservation failure
returns InternalError. Unknown status values retain `ErrorKind::Unknown(raw)`.
Malformed diagnostic output follows the existing diagnostic decoder rather
than claiming a complete native diagnostic. As with all opaque ABI handles,
arbitrary non-null/aligned dangling memory cannot be validated by the binding;
the native implementation must satisfy allocation/liveness contracts.

Session remains neither Send nor Sync. No thread, lock, cancellation or
concurrent-close protocol is introduced. Serialize discovery against closure
and conflicting provider operations.

## Compilable consumer selection example

The API rustdoc compiles the enumeration example during workspace doctests.
This complete value-only helper also selects advertised identities for existing
configurations without hardcoded UUIDs:

```rust
use ams_mel::{
    ChannelCapability, ChannelType, ControlConfig, Error, HealthConfig,
    ImageControlConfig, Session,
};

pub fn discover_required(session: &Session)
    -> Result<Vec<ChannelCapability>, Error>
{
    let capabilities = session.capabilities()?;
    for capability in &capabilities {
        if capability.channel_types.contains(&ChannelType::CommandAndControl) {
            println!("C2: {}", capability.channel_id.descriptive_label());
            let _config = ControlConfig::new(
                capability.channel_id.clone(), capability.platform_id.clone(),
                capability.sensor_location.clone(),
            );
        }
        if capability.channel_types.contains(&ChannelType::HealthAndStatus) {
            println!("Health/Status: {}", capability.channel_id.descriptive_label());
            let _config = HealthConfig::new(
                capability.channel_id.clone(), capability.platform_id.clone(),
                capability.sensor_location.clone(),
            );
        }
        if capability.channel_types.contains(&ChannelType::IrstImage) {
            println!("Image: {}", capability.channel_id.descriptive_label());
            let _config = ImageControlConfig::new(
                capability.channel_id.clone(), capability.platform_id.clone(),
                capability.sensor_location.clone(),
            );
        }
    }
    Ok(capabilities)
}
```

A capability can advertise multiple types, so membership is tested rather than
assuming the first type is exclusive. Configuration construction does not attach
anything. ImageControlConfig is not configured acquisition: zero dimensions do
not describe a usable sample geometry or a buffer size. Application policy for
missing/duplicate families, acquisition and StackedImage safe ownership remain
later tasks.

## Qualification evidence

Toolchain: Linux x86-64, rustc/cargo 1.98.1, GCC 14.2, pinned GNAT 16.1.0,
GPRbuild 26.0.1 and GNATformat 26.0.0. Native build mutations are serialized.
All ordinary Rust tests use:

```sh
cd /home/zboll/git/ams-rust-ir-013
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

Initial sequential gates passed: `make native`; `make test-native` (**587/587**);
`cargo test --manifest-path rust/Cargo.toml --workspace`; workspace all-target
check; warnings-denied Clippy; fmt check; and warnings-denied no-deps rustdoc.
The initial workspace (before two additional private controls) passed **500**,
failed **0**, ignored **9**. Ignored tests are existing subprocess-entry harnesses
explicitly invoked by their parents, not skipped discovery behavior.

Focused public discovery passes **9/9**, including the child entry; private
discovery passes **9/9**, with no ignored cases. Tests cover complete independent
rich expectations for all four entries, empty/zero layouts, full UUID/UTF-8,
post-unload use and dynamic C2/Health/Image configurations. Existing discovery
mock attachment aborts, plus lifetime logs excluding enablement/registration,
qualify absence of discovery side effects. Failure scenarios are malformed,
bad-label, throw, unknown-throw and bad-alloc, repeated twice with exact category,
diagnostic and required diagnostic size assertions, then actual provider unload.

Private injected handles are addresses of live independently owned tokens;
native never receives or dereferences them. Consumption and slot clearing are
independently controlled. Tests cover unexpected error owners/null successful
owners, count failures/overflow, fallible reservation, failures at entry 0/1/3,
null/misaligned/failed view, malformed nested data/UTF-8 at entry 0/1/3, nested
allocation failure, exact release order, complete duplicate entries/types,
unknown/reserved enums, malformed/oversized diagnostics, unknown statuses and
entry/list Close errors including consumed-but-uncleared and retained owners.
Secondary entry/list failures preserve the original malformed-data error.
Close diagnostic corruption cannot cause a second release. Test-controlled
retained tokens are rescued only by test storage after production returns.

`make test-policy` passed **244 tests across eight suites**. Source integrity
passed (825 vendor files, four workflows), structured API audit passed
(127 headers, 148 operations, 50 safe Ada), and final-newline/whitespace checks
passed. The Markdown example was extracted and compiled with:

```sh
rustc --edition=2021 --crate-type lib -D warnings --emit=metadata \
  /tmp/rust-ir-013-example.rs --extern ams_mel="$rlib" \
  -L dependency="$PWD/rust/target/debug/deps" \
  -o /tmp/rust-ir-013-example.rmeta
```

An early private-test linking attempt while aggregate qualification reset the
production tree failed with missing `-lams_mel_c`; rerunning against the
available test library passed. Early development also caught a removed helper's
stray delimiter and Clippy's objection to a vector of boxed test tokens; both
were corrected without suppressing warnings or changing contracts. The new
Markdown initially lacked an EOF newline; it was corrected before qualification.

`make check` passed, exit 0, using:

```sh
env -u AMS_MEL_NATIVE_LIB_DIR -u AMS_MEL_TEST_PROVIDER_DIR \
  -u CARGO_TARGET_DIR -u RUSTFLAGS -u RUSTDOCFLAGS \
  PATH="/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH" \
  make check
```

This includes build-tree isolation, **587/587** native tests, pinned Ada format
checks, **61/61** structured coverage regression controls, **49** Ada contract
PASS summaries, final-newline and whitespace checks. Existing import-only GPR
project warnings about no C sources were observed; no warning policy was
suppressed. No Ada source was edited. The earlier task documents' base-main
newline defects are not present in this freshly fetched base; the complete
repository newline gate passes here.

Separate Release native build and focused qualification passed:

```sh
cmake -S native -B /tmp/rust-ir-013-release \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rust-ir-013-release --parallel 2 --target \
  ams_mel_c mock_ir_provider test_ir_discovery test_ir_discovery_allocation
ctest --test-dir /tmp/rust-ir-013-release --output-on-failure \
  -R '^test_ir_discovery(_allocation)?$'
export AMS_MEL_NATIVE_LIB_DIR=/tmp/rust-ir-013-release/lib
export AMS_MEL_TEST_PROVIDER_DIR=/tmp/rust-ir-013-release/test-providers
export CARGO_TARGET_DIR=/tmp/rust-ir-013-release-rust
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_discovery
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --lib ir_discovery
```

Native Release discovery passed **2/2**, including the exhaustive bridge
allocation-failure test. An initial CTest regex `^ir_discovery` selected zero
tests; it is not counted as qualification and was corrected above. Both Rust
Release suites passed **9/9**, then each was repeated **20** more times:
**378** test executions total, zero failures, zero ignored. These runs used a
separate native and Cargo output tree, never reconfigured the ordinary tree.

After aggregate qualification, `make test-native` restored the full ordinary
mock-provider tree and passed **587/587** again. Final sequential commands:

```sh
unset CARGO_TARGET_DIR
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
cargo test --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_discovery
cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib ir_discovery
cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib capability
cargo test --manifest-path rust/Cargo.toml -p ams-mel-sys --test abi
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

All passed, exit 0. Final workspace: **502 passed, 0 failed, 9 existing ignored
subprocess harness entries**, including **158 unit tests**, **46 raw ABI tests**
and **34 doctests**. Focused final discovery public/private: **9/9 each**;
existing complete capability/unknown-enum conversion: **1/1**; explicit raw ABI
rerun: **46/46**, including C11 translation-unit layout checks. The complete
converter body is byte-identical to base. The standalone Markdown example was
compiled again against final restored-tree artifacts. All five changed files
were reviewed; scope, whitespace, formatting and final newlines pass.

Hosted status
is **HOSTED_CI_PENDING** until final-head and corresponding merge checks complete.
Independent technical review is required; no merge, auto-merge or self-approval.

## Limitations

Evidence uses only the existing separate deterministic native mock provider,
not Squall, minimal-mel-example, hardware or a working sensor. No general buffer
handling, safe StackedImage owner, automatic attachment, services/algorithms or
full minimal consumer is implemented. Existing upstream physical/unit semantics
are unchanged; no unresolved upstream semantics were guessed for this slice.
