# RUST-IR-015 — safe Rust StackedImage channel foundation

## Source preflight and boundaries

Freshly fetched `origin/main`: commit
`8a2d409b53b90161bee01dcdd2f3641480d178d6`, tree
`05fe78b738f2b297c4d10dffeee352fad174d141`. Dedicated worktree
`/home/zboll/git/ams-rust-ir-015`, branch
`feature/rust-ir-015-stacked-foundation`, based directly on that commit.
PR #205 (RUST-IR-014) is merged as
`95404ddb5be293b2335be8aeb0d760f49cad24be`; PR #198 (IRMEL-ADA-016) as
`f75adc6e43dd69011ce14ac5c0bda62b0b07f6f6`. Both are verified ancestors.
PR #207 is already merged into the selected baseline. Open PRs #204 and #208
were inspected along with remote branches; neither branch was adopted or changed.
They overlap native common callback implementation, not this safe wrapper.

Reviewed AGENTS, CONTRIBUTING, architecture, C ABI policy, provider-foundation
task, IR inventory schema and Stacked/common entries, native implementation,
C11 contract/layout tests, raw Rust mirror/drift test, Ada owner, and existing
safe Health/Image/discovery ownership and diagnostic/conversion machinery.

Coverage delta: the fourth required reference-provider channel now has a public
safe Rust typed foundation. Only safe Rust implementation, its minimal exports
and accurate crate API documentation, task-specific tests and this document
change. No native ABI, raw declarations, provider fixtures, Ada/Python, vendor,
CI, callback engine, historical archive or three-channel example changes.
The structured inventory describes native/Ada coverage, not safe Rust; those
classifications and evidence remain unchanged. This document does not substitute
for any native/Ada implementation inventory change.

## Public API and configuration-to-ABI mapping

```rust
use ams_mel::{ChannelType, CommandReturn, CommsTestRequest, CommsTestResult,
              Error, ReturnResult, Session, StackedConfig};

fn fourth_channel(session: Session) -> Result<(), Error> {
    let discovered = session.capabilities()?;
    let capability = discovered.iter()
        .find(|c| c.channel_types.contains(&ChannelType::StackedImage))
        .expect("application requires an advertised StackedImage channel");
    let config = StackedConfig::new(
        capability.channel_id.clone(), capability.platform_id.clone(),
        capability.sensor_location.clone());
    let mut stacked = session.open_stacked_channel(&config)?; // Attached
    let snapshot = stacked.capabilities()?;
    let mut view = stacked.channel_view()?; // independent weak view
    let mut keepalive = view.send_keepalive()?;
    assert_eq!(keepalive.wait(15_000)?,
               ReturnResult::Completed { value: CommandReturn::Success });
    keepalive.close()?;
    let ids = CommsTestRequest { command_id: u32::MAX, channel_id: 6,
                                request_id: 0xf000_0002 };
    // channel_id=6 is the reference stub's documented routing convention,
    // not a universal provider rule. All three IDs remain full-width u32.
    let mut comms = view.submit_comms_test(ids)?;
    match comms.wait(15_000)? {
        CommsTestResult::Completed { report } => {
            assert_eq!(report.command_id, ids.command_id);
            assert_eq!(report.request_id, ids.request_id);
        }
        other => panic!("application requires completed CommsTest: {other:?}"),
    }
    comms.close()?;
    stacked.enable()?; // Enabled, not acquisition
    view.close()?;
    stacked.close()?;
    session.close()?;
    assert!(snapshot.channel_types.contains(&ChannelType::StackedImage));
    Ok(())
}
```

`StackedConfig` derives Clone, Debug and PartialEq and privately owns two `UciId`s
and a `ComponentLocation`. Construction contacts no provider. Existing identity
constructors reject embedded NUL and allow empty labels. Open maps channel UUID
and label to `channel_id`, constant **6** to `channel_type`, platform UUID and
label to `platform_id`, and all three f64 offsets plus location key/system to
`sensor_location` in `AmsMelIrStackedConfigV1`. All 16 UUID bytes, UTF-8 bytes and
byte lengths, empty values and f64 values (including negative zero) are preserved.
The borrowed strings remain alive throughout the synchronous call; native Open
copies them. No dimensions, pixel format, detector resolution or stack geometry
are inferred; no buffer, priority, task or metadata queue fields exist.

`Session::open_stacked_channel(&StackedConfig)` makes exactly one
`ams_mel_ir_stacked_open` call. Native discovery and attachment validation remain
the only engine: advertisement first, one attachment, concrete dynamic type and
attached advertisement validation, then publication. Missing advertisement is
InitializationFailed without poisoning the Session; null attachment is
FactoryFailed. Exceptions and rollback preserve native categories/diagnostics
(including InitializationFailed after proven rollback). Uncertain rollback can
permanently retain the provider graph. Success without an owner is a protocol
error. Failure or malformed diagnostics with a non-null output receive one
best-effort cleanup attempt, never another side-effecting Open.

## Capabilities and lifecycle

`StackedChannel` is Debug, uniquely owned, neither Clone nor Send nor Sync.
It exposes `is_open`, `enable`, `capabilities`, `channel_view` and `close`.
Compile-fail doctests verify both thread-affinity restrictions.

- Attached: capabilities and inherited common operations are available.
- Enabled: explicit Enable succeeded; subsequent Enable is idempotent.
- Failed: Enable refusal/exception is not retried; subsequent Enable and
  capability operations fail. Close still performs cleanup.
- Closed: new typed/common operations are refused. A retained detach-retry
  handle still makes `is_open()` true; that is ownership, not operational state.

Capabilities reuse **the existing `capability_snapshot` converter**, not another
field converter. Every identity, scalar, zero-layout/depth value, sensor/channel
collection, metadata capability, image-band nested record, navigation frame and
existing unknown/reserved representation is copied into owned Rust values.
TaskScheduleDepth 0 and u32::MAX are valid values, not absent or clamped values.
The snapshot survives typed owner, Session and library teardown. Malformed
owners/views/spans, invalid text and conversion errors have no partial result;
the shared guard gives each temporary owner one release attempt.

Close calls `ams_mel_ir_stacked_close`: logical closure first, disable only if
Enable was attempted and at most once during native cleanup, then detach.
Disable failure remains an error even when detach succeeds and consumes the
handle. Failed detach can retain the same handle for explicit retry. Native
pointer output is authoritative, including null on error; consumed handles
are never released again. Drop makes one diagnostic-free best-effort Close
attempt on a remaining owner, without panicking or inventing safe teardown.
An uncertain retained graph is not manually freed. Rust disarms the owner before
Close and restores it only for the documented ProviderFailed result retaining
the exact same pointer with valid error conversion. Malformed diagnostics,
inconsistent success or other uncertain release results cannot trigger a second
release through Drop. This can conservatively leak a malformed native owner;
it cannot manufacture certainty that the owner remains safe to release.

## Weak common views, requests and deferred teardown

`ams_mel_ir_channel_from_stacked` produces the existing safe `ChannelView`:
capabilities, KeepAlive, correlated direct CommsTest and view Close. Multiple
views are independent. Neither view ownership nor view closure retains/closes
the typed channel or provider. A view can remain allocated after provider unload;
operations then fail cleanly. This task adds no callback registration behavior.

Admitted requests independently own the native completion claim, future and
Session admission permit. Stacked accounting is family-local; an outstanding
Health request does not delay Stacked teardown. Session limits are shared across
families and views, not separate Rust queues. With a full Session, admission
returns ResourceExhausted before family expiry checks. Timeout (including a zero
poll) is not cancellation. Return::Fail is a completed KeepAlive payload, not a
fabricated native error. Request Close/Drop and all parent closure leave admitted
work running. No new executor, thread or queue is introduced.

Deferred disable/detach happens only when native claims can safely release.
Two gated futures prove that completing CommsTest cannot detach while KeepAlive
is pending. Deferred detach failure can return ProviderFailed at completion and
intentionally retain the provider graph without a public retry handle. Clearing
the typed owner does not prove physical cleanup or callback quiescence. Tests
wait on deterministic mock gates and final-owner condition variables, never
sleep-based synchronization. Test-only dlopen pins are released before asserting
provider unload. Intentionally retained graphs and environment changes are
isolated in subprocesses. Injected opaque tokens never reach actual native Close.

## Qualification record

Local evidence directory: `/tmp/rust-ir-015-evidence` (not a release artifact).
Toolchain: GCC/G++ 14.2.0, rustc 1.98.1, Cargo 1.98.1. Exact commands, final
counts and aggregate/hosted completion are recorded below when finished.

Initial native gates: `make native`, `make test-native`: exit 0, **656/656**.
Focused Debug: 7 unit tests, 30 mock scenarios pass; one ignored subprocess
entry is exercised by all parent scenarios, not skipped behavioral coverage.
Initial integration compilation exposed consuming Session/request Close APIs;
tests were corrected to respect ownership. Initial mock execution exposed
admission-before-expiry and empty/throwing discovery fixture assumptions; tests
now assert those existing contracts. No native behavior or test was weakened.

## Actual reference-provider interoperability

Verified clean checkout `/home/zboll/git/ams-rust-ir-014-reference` at commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`. Built unchanged sources separately:

```sh
cmake -S /home/zboll/git/ams-rust-ir-014-reference \
  -B /tmp/rust-ir-015-reference-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build /tmp/rust-ir-015-reference-build \
  --target minimal_ir_mel_provider --parallel 2
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib" \
AMS_MEL_REFERENCE_PROVIDER=/tmp/rust-ir-015-reference-build/libminimal_ir_mel_provider.so \
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test ir_stacked_reference -- --ignored --nocapture
```

Actual Debug execution: **1 passed**, using only public safe Rust (test forbids
unsafe code). Provider reports vendor `minimal-mel-example`, API 4, library 1,
description `IR MEL 4.0 required-interface provider stub; no sensor connected`.
Channel UUID bytes are `42 71 6d 65 6c 73 40 00 80 00 00 00 00 00 00 07`,
label `Stub stacked-image interface`; platform UUID/label and location strings
are zero/empty. All seven acquisition-layout scalars and TaskScheduleDepth are
zero, pixel format Mono, sensor Unspecified, metadata `[ChannelCommsTestRep]`,
no bands/nav frames, ODC/NUC false. Attached and discovered snapshots compare
fully equal. Weak capabilities, KeepAlive, correlated CommsTest, Enable and
normal View/typed/Session closure pass without acquisition.

The first interoperability attempt intentionally used a full-width numeric
channel routing ID and received the stub's explicit InvalidId rejection. Source
inspection confirms the stub requires its ChannelType value (6) as routing ID;
the successful run uses that convention, with command ID u32::MAX and request ID
0xf0000002. Mock tests separately verify full-width preservation of all three IDs.
This is not a facade restriction and no provider source was modified.

Provider DSO SHA-256:
`7a4f39131ea72840d2142d004ddcfc35f8505d856271da91a973adf4c2fcf4cd`;
GNU build ID `b07cb50c9fbb08ff3fbd2c4f433a4308b5a63a03`.
Initial system-toolchain production facade SHA-256:
`02a7401e0ceea34a13f2c0d3c3bdbb067bf50bb19b5d8f931c58a5a08a2a2c63`;
initial Debug test facade SHA-256:
`32c8445d9220d1922b901e9c7a6ede1d11a295bb542f0c7f55ce94b8c499e03a`.
DSO hashes describe this local executable build, not universal binary identities.

## Unsupported scope and delivery

No General Buffer factory/registration, stacked acquisition, frame delivery,
StackedImageListener/StackFrameHeader, optional LOS3D, general ChannelCommsTestRep
callback receiver, new metadata queues or completion engine. Advertisement,
attachment and Enable do not demonstrate configured detectors, stack acquisition,
sensor hardware or physical operation. The RUST-IR-014 consumer remains unchanged
and three-channel only; its four-channel expansion and remaining required common
callback/Buffer consumer parity belong to subsequent tasks.

Final commit/tree identities belong in the delivery PR to avoid self-reference.
Independent technical review is required; leave the PR open, non-draft, unmerged,
without auto-merge or self-approval. **HOSTED_CI_PENDING** until final-HEAD
Bootstrap/Documentation and matching synthetic-merge qualification finish.

### Additional qualification details

Focused Debug and Release each pass **7 unit tests + 30 mock subprocess
scenarios**; 20 repeated Release runs pass **600 scenario executions**. The
final stress runs also use a separately compiled Release native facade/mock
tree at `/tmp/rust-ir-015-native-release`. Native C11 Stacked foundation/layout:
`ctest --test-dir /tmp/rust-ir-015-native-release -R '^ir_stacked_' --output-on-failure`
passes **29/29**. An initial selector `ir-stacked` matched no tests; a subsequent
overbroad `stacked` selector included unbuilt unrelated callback targets. These
are not counted as qualification; the exact built foundation/layout selection
above is the passing evidence. No missing test executable was treated as a pass.

Early exploratory Rust links made while aggregate validation was rebuilding the
test facade failed with `unable to find library -lams_mel_c`. No source or build
contract was changed to bypass this; focused tests were rerun against the
complete separate Release tree and aggregate is followed by full test-tree
restoration before final workspace validation. Only aggregate mutates the
standard native build trees; separate Release compilation uses a distinct tree.

New PRs #209 (RF Rustdoc) and #210 (native/Ada Buffer) opened during local
qualification. Their changed-file inventories were inspected: neither changes
the task-owned safe Stacked paths, and neither branch is adopted or modified.

Final Debug/Release reference executions also pass against the separate Release
facade (SHA-256
`34675924ff13c68b0b7dad0e9bf7b46a1110d152f321fa6c329638ff6a8f3f59`).
These reruns include the conservative malformed-Close disarming behavior.

Static final-source gates pass: workspace all-target `cargo check`, Clippy with
`-D warnings`, rustfmt check and Rustdoc with `-D warnings`. Doctests pass
**51/51**, including the Stacked discovery example and non-Send/non-Sync checks.
The task-document public API example was separately type-checked by rustc with
`-D warnings`. Policy suite passes **244/244**; source integrity verifies **825
vendored files / four workflows**. No first-party Ada source was changed; pinned
GNATformat checks run through aggregate validation without rewriting Ada sources.

Native Release foundation/layout stress also passes 20 repetitions, **580 test
executions**, using `ctest --test-dir /tmp/rust-ir-015-native-release
-R '^ir_stacked_' --repeat until-fail:20 --output-on-failure`.

Standard Rust gates (test-enabled facade/provider paths explicitly exported):

```sh
cargo test --manifest-path rust/Cargo.toml --workspace
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps
cargo test --manifest-path rust/Cargo.toml -p ams-mel ir_stacked --lib
cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_stacked
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release ir_stacked --lib
cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_stacked
# Repeat the last command 20 times, preserving each exit status.
```

Aggregate command removes conflicting overrides and uses pinned GNAT 16.1.0,
GPRbuild package 26.0.1 (banner 26.0.0), GNATformat 26.0.0:

```sh
env -u AMS_MEL_NATIVE_LIB_DIR -u AMS_MEL_TEST_PROVIDER_DIR \
  -u CARGO_TARGET_DIR -u LD_LIBRARY_PATH \
  PATH="/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH" \
  make check
# Restore the entire test-provider tree before final Rust tests:
make test-native
# Re-export native/build-tests/lib and native/build-tests/test-providers, then:
cargo test --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test common_channel --test ir_discovery --test ir_image_control
cargo test --manifest-path rust/Cargo.toml -p ams-mel-sys
ctest --test-dir native/build-tests -R stacked --output-on-failure
make test-policy
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

Aggregate `make check`: **PASS, exit 0**, including build-tree isolation,
**656/656 native tests**, **62/62 structured-inventory regression tests**, pinned
format checks, and all **49 Ada contract PASS groups**, then newline/whitespace
checks. The pre-existing imported native GPR project emits three metadata
warnings about no C sources; compiled first-party source remains warning-clean.
The full test tree is rebuilt afterwards rather than leaving the providers-only
configuration used by the Ada stage.

Restored-tree final workspace: **567 passed, 0 failed, 12 ignored** (including
51 doctests). Ten ignored entries are subprocess harness entry points exercised
by parent tests; two are opt-in reference-provider tests. This task's new actual
reference test was run explicitly in Debug and Release; the pre-existing
RUST-IR-014 executable opt-in is unchanged and was not rerun for this task.
Explicit Common Channel/discovery/Image control regressions: **43 passed**.
Raw sys ABI/drift suite: **55 passed**, including the C11 Stacked layout/signature
comparison. Restored Debug native `-R stacked`: **46/46** (foundation/layout plus
existing common metadata regressions). All-target check, Clippy, formatting and
Rustdoc gates were rerun successfully after full native test-tree restoration.
Initial aggregate restores rebuild the facade with toolchain-dependent binary
hashes; final restored test facade still hashes to the initial test hash above;
post-Ada production facade SHA-256 is
`fe5f21702f4ab9a92513a01b90b32e3121f999940b8e43bb2abd451727ac7de4`.

## Mainline integration during delivery

PR #211 was opened non-draft after initial local qualification. Main advanced
through RF documentation PR #209 to
`99d7f11bc3e5b19c10c6d9daf0e7fe1c89b6fd41`. It was incorporated normally with
merge `ef3ff6cfe844b22189a18a8cc4a0617618e0c9c3`, integrated tree
`8475d57df5f91ad6cfaa691da6987ef8c5407545`, without rewriting published history.
That tree exactly equals the initial hosted synthetic-merge tree for
`f597a4ab22a53b1cde9fce97210a721d2f76e5b0` (parents main and initial feature
HEAD `fda4cb2f269d14c5b9f3da0ec95878cc9a6a8d8c`). Only mainline RF documentation
was added; the task diff against current main remains the six listed task paths.

All standard gates, aggregate/restoration, workspace/static checks, focused
Release stress, and actual reference-provider Debug/Release runs are rerun for
this integrated source. Final integrated counts, final commit/tree and hosted
run/synthetic-merge evidence are recorded on PR #211; earlier numbers above are
not silently relabelled as later-source evidence. **HOSTED_CI_PENDING** until
those final-source runs finish. Repository collaborators/teams expose only the
author and no review team; an independent technical review is requested in the
PR rather than self-assigned or self-approved. No access settings are changed.
