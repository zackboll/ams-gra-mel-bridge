# RUST-IR-012 — safe Rust Image control-only attachment

## Source identity, prerequisites and scope

Freshly fetched base `origin/main` is commit
`0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab`, tree
`a6c579aa1f9d71f40971a7e556da8da210f1ba5f`.
PR #184 is merged at `4df03db1b0abd25734b827d86ecaa4acbd56da05`;
PR #194 is merged at the base above. Both were verified as ancestors, not
merely inferred from branch names. No open PRs or overlapping remote Rust
Image control-only branch were found. The dedicated branch is
`feature/rust-ir-012-image-control-only`, worktree
`/home/zboll/git/ams-rust-ir-012`; unrelated worktrees/history are unchanged.
Final feature commit/tree identities and exact hosted run identities belong
in the delivery PR evidence, avoiding a self-referential enclosing tree hash.

Reviewed repository instructions, contribution rules, architecture/C ABI
policies, provider-foundation task, structured IR inventory/schema, native
Image implementation/C11 control-only test, Ada control-only conversion and
lifecycle, reconciliation report, and existing Rust Image, metadata,
NavigationReport and common Channel owners. The authoritative structured
inventory classifies native/Ada operations and getters, not safe Rust. Its
existing Image attachment evidence/classification remains correct and is not
modified. This document owns the safe Rust coverage delta.

Only four files change:

- `rust/ams-mel/src/lib.rs`: owned configuration, safe attachment, private
  conversion/open helper and minimal public documentation/compiled example.
- `rust/ams-mel/src/ir_image_control_tests.rs`: private call-injection tests.
- `rust/ams-mel/tests/ir_image_control.rs`: public integration qualification.
- `docs/rust-ir-012-image-control-only.md`: this task's evidence and boundaries.

No native implementation, C ABI, sys declarations, Ada/Python binding, mock
fixture, upstream header, dependency, workflow, policy or historical archive
changes. The existing `ams_mel_ir_image_control_open` and
`AmsMelIrImageControlConfigV1` are reused without extending either.

## New safe API and exact field mapping

```rust
pub struct ImageControlConfig { /* private owned fields */ }

impl ImageControlConfig {
    pub fn new(channel_id: UciId, platform_id: UciId,
               sensor_location: ComponentLocation) -> Self;
}

impl Session {
    pub fn open_image_control(&self, config: &ImageControlConfig)
        -> Result<ImageStream, Error>;
}
```

The configuration is owned, Clone, Debug and PartialEq, following existing
ImageConfig/ControlConfig conventions. UUIDs retain all 16 bytes. Original
UTF-8 labels, key and system name are retained byte-for-byte, without Unicode
normalization. Existing UciId/ComponentLocation construction rejects embedded
NUL as InvalidArgument; empty text remains valid. Offsets retain `f64` metre
values, including signed zero; no physical validation or coordinate conversion
is added.

| Native field | Safe input |
|---|---|
| channel_type | Always `AMS_MEL_IR_CHANNEL_IRST_IMAGE` |
| channel_id | Original channel UUID and borrowed original label bytes |
| platform_id | Original platform UUID and borrowed original label bytes |
| sensor_location | Original X/Y/Z metre offsets, key and system-name bytes |

There are **no** buffer-count, buffer-size or frame-queue fields in this record.
The native implementation expands it to its internal attachment configuration
with all three acquisition settings zero. The Rust helper borrows the owned
configuration for the entire single Open call; no temporary CString/span
allocation can expire before native consumption. Empty string views use
null/zero. The native call copies attachment identities before returning.

## Control-only versus acquisition

The returned owner is the existing unique `ImageStream`, intentionally neither
Send nor Sync. It is not a second ownership hierarchy or a detector-readiness
signal. Zero width, height, depth, row pitch, buffer size, image size and band
count remain actual reported capability values. The common snapshot path
copies these without treating them as malformed acquisition settings.

Raw pixel-format value **0 already maps to `PixelFormat::Mono`** in the existing
published enum. Copying that provider-reported enum is not inferring Mono8,
choosing a sample format or establishing an acquisition layout. It does not
override the independent zero dimensions/depth/bands or select any buffer.

Start invokes native channel enablement only. No Buffer factory lookup/call,
sample allocation, Buffer initialization or registration is performed. Both
zero and nonzero receive timeouts return native `InitializationFailed` with
`image receive unavailable on control-only attachment`, never Timeout, an empty
frame or fabricated 1×1 pixels. Unsolicited provider image callbacks are rejected
and released by native code without querying sample storage. Native counters
count those rejected callbacks too; `frames_received` is not proof of delivered
images. No raw sample pointer or unsafe storage API is added.

`open_image_stream(&ImageConfig)` retains its separate nonzero limits and
strict supported Mono8 single-band capability requirement. An acquisition Open
against the zero-layout mock fails; ordinary 4×3 Mono8 pixels still arrive
unchanged through the acquisition method. No new acquisition formats exist.

## Existing Image services

- `channel_view().capabilities()` returns an owned snapshot independent of
  Image/Session/provider lifetime. Provider identities in that snapshot are
  not overwritten with the configuration's requested identities.
- `open_metadata(8)` registers exactly BadPixelList, LineOfSightReport,
  LineOfSightEuler and NavigationReportResp, using the existing independent
  bounded FIFO/owned event conversion. No optional registrations are implicit.
  The mock delivers four synchronous registration events and a navigation
  response on submission. Real providers may deliver none; polling can time out.
- `submit_navigation_report` accepts the complete existing NavigationReport
  before Start. `PositionSolutionState::NotSet` is a representation of unset
  solution state, **not a real INS position solution**. The configured mock
  explicitly rejects with `NavigationResult::Rejected`,
  `MelErrorCode::InvalidState` and exact description `no navigation hardware`.
  A separate held mock future supplies a successful response after parent Close.
  Neither metadata delivery nor successful submission fabricates future success.
- The weak common Channel view supports KeepAlive, high-bit CommsTest IDs,
  correlated cached completion and idempotent safe View Close. Closing the View
  neither closes the Image owner nor cancels admitted requests.

## Errors, ownership and teardown

Open executes once and propagates the existing ErrorKind/native status mapping
and diagnostic without replacing attachment failure with an owner. Null native
output on apparent success is ProtocolInconsistency. Unexpected non-null output
on failure or malformed diagnostics receives exactly one best-effort native
Close; it is never wrapped as a valid ImageStream. Private tests inject these
otherwise nonconforming outputs and test retained versus cleared cleanup output.
Opaque test tokens never reach native code or get dereferenced.

The existing bounded Open diagnostic helper retains complete fitting diagnostics;
an oversized failure retains its original status and required byte count with
no invented/truncated diagnostic and no repeated side-effecting Open. Invalid
diagnostic shape/encoding uses the established protocol/UTF-8 error handling.
Navigation waits reuse the cached terminal diagnostic-copy path and retain the
provider's rejection code/description separately from transport errors.

Native statuses observed on failed attachment: FactoryFailed for null attachment,
ProviderException for throwing attachment, InitializationFailed for an incompatible
provider channel, and ProviderFailed for failed rollback detach. Safe callers
cannot configure another channel type; C11 tests cover invalid native type input.
The native wrong-type diagnostic still mentions Mono8 capability; Rust preserves
it literally rather than rewriting this existing generic diagnostic.

The existing consuming ImageStream Close is unchanged: if native Close retains
the owner after failure, Drop gives its existing single cleanup fallback. The
detach-fail fixture fails the first attempt and permits that fallback retry;
its original reported failure is not turned into success. Normal native release
and destruction occur once. Drop alone is best-effort and non-raising.

Timeout, request Close/Drop and Session Close do not cancel provider work.
Pending navigation retains the provider graph through both Image-first and
Session-first close orders. Physical detach/destruction waits for admitted work.
Metadata observers close independently; callback state remains safe through
provider destruction-time callbacks. Disable/Close is not a blanket promise of
callback quiescence. Uncertain release, failed detach and exceptions after
listener exposure keep native state and provider code alive when safe destruction
cannot be established, potentially until process exit. Tests requiring permanent
retention run in separate child processes, with isolated process-global logs.
Deterministic held promises and existing completion-boundary/FinalOwner probes
synchronize work and reclamation; bounded polling observes lifetime-log events.

## Compilable public usage

The following uses only the public safe API. The complete caller-supplied report
must describe the application's actual state; using NotSet does not establish
navigation hardware. The `Session::open_image_control` rustdoc has a compiled
`no_run` service example as well.

```rust
use ams_mel::{ComponentLocation, Error, ImageControlConfig, NavigationReport,
              NavigationResult, Session, UciId};
use std::path::Path;

pub fn attach(provider: &Path, instance: &str, channel: UciId, platform: UciId,
              location: ComponentLocation, report: &NavigationReport)
              -> Result<NavigationResult, Error> {
    let session = Session::open(provider, instance, "")?;
    let config = ImageControlConfig::new(channel, platform, location);
    let mut image = session.open_image_control(&config)?;
    let mut view = image.channel_view()?;
    let _capabilities = view.capabilities()?;
    let mut metadata = image.open_metadata(8)?;
    let mut navigation = image.submit_navigation_report(report)?;
    let outcome = navigation.wait(1_000)?;
    navigation.close()?;
    image.start()?;
    assert!(image.receive(0).is_err());
    metadata.close()?;
    view.close()?;
    image.stop()?;
    image.close()?;
    session.close()?;
    Ok(outcome)
}
```

This short example propagates Timeout and drops its observer; a real polling
consumer should retain pending requests to retry Wait, and should handle
explicit Close failures. It need not receive metadata if the provider supplies
none. Applications must inspect actual capabilities and results rather than
interpreting successful attachment/enablement as detector or INS availability.

## Qualification evidence

Toolchain: Linux x86-64, rustc/cargo 1.98.1, GCC 14, pinned local GNAT 16.1.0,
GPRbuild crate 26.0.1 (binary reports 26.0.0), GNATformat 26.0.0.

Commands run from the dedicated worktree, with native build mutations serialized:

```sh
make native
make test-native
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
cargo test --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_image_control
cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib image_control
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps
```

`make native` passed; initial `make test-native` passed **558/558**. Focused
Rust integration passes **18/18** (17 isolated scenarios plus the child-entry
harness, which is a no-op in its parent invocation). Private Rust tests pass
**4/4**. No focused tests are ignored. The standalone public usage example was
extracted and compiled with `rustc --edition=2021 --crate-type lib -D warnings
--emit=metadata`, using the built ams-mel rlib/dependency directory. The API's
rustdoc example is compiled by workspace doctests.

Separate native Release configuration and qualification:

```sh
cmake -S native -B /tmp/rust-ir-012-release \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rust-ir-012-release --parallel 2 --target \
  ams_mel_c mock_ir_provider test_ir_image_control test_ir_stream \
  test_ir_image_metadata test_ir_navigation
ctest --test-dir /tmp/rust-ir-012-release --output-on-failure \
  -R '^(ir_image_control_contract|ir_stream_contract|ir_image_metadata_contract|ir_navigation_contract)$'
export AMS_MEL_NATIVE_LIB_DIR=/tmp/rust-ir-012-release/lib
export AMS_MEL_TEST_PROVIDER_DIR=/tmp/rust-ir-012-release/test-providers
export CARGO_TARGET_DIR=/tmp/rust-ir-012-release-rust
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_image_control
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --lib image_control
for i in $(seq 1 50); do
  cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_image_control || exit
done
```

Native Release **4/4**, Rust Release integration **18/18**, private **4/4**;
the final focused Release integration repeated **50/50** successful invocations
(900 harness-reported passes, 850 actual isolated scenario executions). Native
Release retains `-DNDEBUG`; C11 contract checks remain active.

Aggregate command:

```sh
env -u AMS_MEL_NATIVE_LIB_DIR -u AMS_MEL_TEST_PROVIDER_DIR \
  -u LD_LIBRARY_PATH -u LIBRARY_PATH -u LD_RUN_PATH \
  PATH=/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH \
  make check
```

`make check` **failed**, exit 2, solely at the final-newline gate for two
unchanged base-main files: `docs/mel-gnatdoc-015-001-official-release.md` and
`docs/tests/test_enhancer_release.py`. Their missing EOF newlines were independently
confirmed with `git show origin/main:PATH`. They remain untouched under the task's
scope restrictions. Before that failure, build-tree isolation passed, native
CTest passed **558/558**, Ada formatting passed, the structured coverage audit
passed (127 headers, 148 operations, 50 safe Ada) with **60/60** regression tests,
and the Ada smoke driver passed all **48** contract PASS summaries, including
Image control-only (49 PASS lines including native build isolation). Existing
GPR project warnings about no C sources in the imported native project were
observed; no Ada source changes or warning suppression is included.

Earlier failures were corrected without weakening contracts: initial test
expectations for pixel enum zero, unsolicited callback counters and the manager
lifetime event were wrong; Clippy caught an approximate fixture angle and a
manual C string literal. A workspace attempt against the deliberately partial
Release provider tree failed because its RF mock had not been built (148 unit
passes, one RF load failure). That run is **not** workspace qualification. Full
workspace qualification uses the restored complete ordinary mock-provider tree.

After aggregate qualification, `make test-native` restored the **full** ordinary
mock-provider tree and passed **558/558** again. Final Debug workspace tests
passed **482**, failed **0**, ignored **9**, across 31 result groups (including
doctests). The nine ignored cases are existing subprocess-entry harnesses
explicitly invoked by their parent scenarios, not skipped feature behavior.
The final focused Debug suite passed **18/18** and private tests **4/4**. Final
workspace all-target check, warnings-denied Clippy, fmt check and
warnings-denied no-deps rustdoc all passed against the restored tree. Git
whitespace checks passed. The independent final-newline check still reports
exactly the two unchanged upstream files above; all four task files end in LF.

Delivery status is **HOSTED_CI_PENDING** until the required hosted final-head
checks actually pass. Exact final commit/tree identities, hosted run URLs and
head-versus-synthetic-merge provenance are recorded on the PR. Independent
technical review remains required; no merge, auto-merge or self-approval is
authorized. Only the repository owner is listed as a collaborator, so an
independent reviewer cannot be assigned by inventing a GitHub account; the PR
explicitly requests independent technical review.

## Limitations and remaining minimal-consumer work

The existing separate native mock is the sole provider evidence here. No actual
interoperability with minimal-mel-example, Squall, a sensor or hardware has been
tested. Passing mock metadata delivery does not establish real acquisition or
sensor data. No unresolved native blocker was found for this safe attachment
slice. General Buffer access, StackedImage, new pixel formats, Control capability
list discovery, processing algorithms, services and a full consumer are excluded.

A future minimal Rust consumer still needs provider-specific public configuration
and loading, capability/channel selection, a real event/polling loop with timeout
policy, truthful navigation input, explicit asynchronous outcome handling and
orderly application shutdown. Optional metadata must remain explicitly selected.
That consumer and any real-provider build/run qualification are separate work.
