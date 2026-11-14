# RUST-IR-009 — complete safe Rust C2 ModeCmd

## Source identity and scope

- Starting, freshly fetched `origin/main` commit:
  `e50079ad59ec4b37678f73a4f318d264f3a4639f`.
- Starting tree: `ad52cb86dfd17e36a6fa3e1af4c11a3127b47311`.
- Branch: `feature/rust-ir-009-general-mode`, created directly from that commit
  in `/home/zboll/git/ams-rust-ir-009`.
- Preflight inspected `AGENTS.md`, `CONTRIBUTING.md`, architecture/ABI policy,
  provider-foundation task, the IR inventory schema, reviewed ModeCmd/ScanParam
  evidence, safe Ada conversion, native conversion, raw Rust declarations and
  mock-provider/C/Rust tests. The structured inventory classifies native/Ada
  coverage, not safe Rust coverage; those implementations/classifications do
  not change here. This document records the Rust coverage delta.
- Open PR inspection found no overlapping Rust ModeCmd work. PR #186,
  `feature/rust-ir-008-navigation-report` at
  `6d91a5ea7c4a8f942c02b7ce97960ca4567f15e6`, is independent Image navigation
  work. Its branch and original worktree were left untouched. This branch is
  not stacked on it; no source dependency was found.
- During qualification, GitHub independently changed #186 to merged at
  `2026-11-14T02:51:52Z`, merge commit
  `c4490a9ce509512e0de6a79d42a61a12c00a7b9a`. This task did not perform that
  merge or modify #186. The original worktree remains at its inspected SHA;
  this feature retains the exact preflight base above.

Only safe Rust implementation/tests and this task-owned document change.
No native implementation, ABI, raw sys declarations, Ada, Python, provider,
vendor, dependency, workflow or historical coverage archive changes occur.
Final commit/tree identities are recorded in the PR delivery evidence, outside
this commit's own contents (avoiding a self-referential commit hash).

## Complete input mappings

`ModeCommand` owns `command_id: u32`, `state: MfaState`, the existing public
`mode: MfaMode`, and `scan_parameters: ScanParameters`. The command ID retains
all 32 bits, including the high bit and `u32::MAX`.

### Enum domains

| `MfaState` | Native/published value |
|---|---:|
| NotSet | 0 |
| Unknown | 1 |
| NotInstalled | 2 |
| Off | 3 |
| PreInitialization | 4 |
| Initialization | 5 |
| Standby | 6 |
| Operate | 7 |
| OperateRxOnly | 8 |
| OperateTxOnly | 9 |
| Maintenance | 10 |
| Calibration | 11 |
| InitiatedBit | 12 |
| Shutdown | 13 |
| Degraded | 14 |

`Unknown` is exactly the published state 1, never a fallback for an unknown
integer. Closed state/frame/degradation enums implement checked
`TryFrom<u32>` and expose their complete published `ALL` domain.

| Domain | Exact associations |
|---|---|
| Existing `MfaMode` | Unused=0, TaskSched=1, ScanVolumeSched=2, ScanBarSched=3 |
| `CoordinateFrameReference` | Inertial=0, Aircraft=1 |
| `DegradationMethod` | Capacity=0, Volume=1, Range=2, Revisit=3 |

`MfaMode::try_from(u32)` rejects all other integers. Existing `Unknown(u32)`
output representation and result decoding remain source-compatible.
Submission rejects **every** `MfaMode::Unknown(_)`, including `Unknown(0)`;
it never reinterprets the payload as a valid input mode.

### ScanParam mapping

Every row below maps directly to the identically named field of
`ams_mel_ir_scan_param_v1`. The three nested controls map to
`ams_mel_ir_scan_type_v1`. No field is inferred or omitted.

| Safe Rust field | Type | Published/native default |
|---|---|---|
| elevation_defined_with_range_and_altitude | bool → u32 0/1 | false |
| center_az_rad | f64 | +0.0 |
| center_el_rad | f64 | +0.0 |
| center_frame_ref_el | CoordinateFrameReference → u32 | Inertial (0) |
| center_frame_ref_az | CoordinateFrameReference → u32 | Aircraft (1) |
| scan_width_rad | f64 | +0.0 |
| scan_height_rad | f64 | +0.0 |
| scan_type.continuous_scan | u32 | 0 |
| scan_type.returning | u32 | 0 |
| scan_type.agile_scan | u32 | 0 |
| scan_id | u32 | 0 |
| scan_rate_rad_per_second | f64 | +0.0 |
| preferred_revisit_interval_seconds | f64 | +0.0 |
| required_revisit_interval_seconds | f64 | +0.0 |
| max_range_of_interest_m | u32 | 0 |
| min_range_of_interest_m | u32 | 0 |
| elevation_scan_center_altitude_m | u32 | 0 |
| elevation_scan_center_range_m | u32 | 0 |
| degradation_method | DegradationMethod → u32 | Capacity (0) |

`ScanParameters::default()` follows upstream `ScanParam`, including its
**different elevation/azimuth references**; a zero-initialized raw sys record
is not equivalent. `ScanType::default()` is all zero unsigned controls.
Angles are radians, scan rate radians/second and revisit intervals seconds.
Range fields preserve native meter-labelled scalars; no altitude datum is
invented. Zero range/altitude values retain the upstream unset representation.

All nested FFI records are constructed explicitly. Rust conversion preserves
every `f64` bit, including signed zero, infinities, subnormals and NaN payloads,
and all `u32` bits. No Boolean narrowing of ScanType, floating-point
normalization, physical validation, min/max sorting, state/mode compatibility
rules or coordinate transformations are performed. Native validation accepts
these floating-point representations; this does not guarantee provider-specific
NaN payload preservation or physical validity. Rust bit-fidelity tests cover
conversion itself; the existing provider fixture checks all ordinary nondefault
fields, and the native path is separately tested for acceptance of extreme data.

## Public usage

```rust
use ams_mel::{
    ComponentLocation, ControlConfig, CoordinateFrameReference, DegradationMethod,
    Error, MfaMode, MfaState, ModeCommand, ModeResult, ScanParameters, ScanType,
    Session, UciId,
};
use std::path::Path;

pub fn command_example(provider: &Path) -> Result<(), Error> {
    let session = Session::open(provider, "", "")?;
    let config = ControlConfig::new(
        UciId::new([1; 16], "C2")?,
        UciId::new([2; 16], "platform")?,
        ComponentLocation::new(0.0, 0.0, 0.0, "station", "aircraft")?,
    );
    let mut c2 = session.open_control_channel(&config)?;
    c2.enable()?;

    let mut command = ModeCommand {
        command_id: 302,
        state: MfaState::Standby,
        mode: MfaMode::Unused,
        scan_parameters: ScanParameters::default(),
    };
    let mut standby = c2.submit_mode(&command)?;
    match standby.wait(1_000)? {
        ModeResult::Success { mode } => println!("accepted: {mode:?}"),
        ModeResult::Rejected { code, description } => {
            println!("MEL rejection: {code:?}: {description}");
        }
    }
    standby.close()?;

    command.command_id = u32::MAX;
    command.state = MfaState::Operate;
    command.mode = MfaMode::ScanVolumeSched;
    command.scan_parameters = ScanParameters {
        elevation_defined_with_range_and_altitude: true,
        center_az_rad: 0.25,
        center_el_rad: -0.5,
        center_frame_ref_el: CoordinateFrameReference::Aircraft,
        center_frame_ref_az: CoordinateFrameReference::Inertial,
        scan_width_rad: 1.25,
        scan_height_rad: 0.75,
        scan_type: ScanType { continuous_scan: 1, returning: 2, agile_scan: 3 },
        scan_id: u32::MAX,
        scan_rate_rad_per_second: -0.125,
        preferred_revisit_interval_seconds: 2.5,
        required_revisit_interval_seconds: 3.5,
        max_range_of_interest_m: 123456,
        min_range_of_interest_m: 42,
        elevation_scan_center_altitude_m: 7000,
        elevation_scan_center_range_m: 9000,
        degradation_method: DegradationMethod::Revisit,
    };
    let mut scan = c2.submit_mode(&command)?;
    println!("provider outcome: {:?}", scan.wait(1_000)?);
    scan.close()?;
    c2.close()?;
    session.close()?;
    Ok(())
}
```

The provider path, initialization strings and channel/platform configuration
are deployment-specific, not a physical-scanning recipe. A Timeout returned
by the example's wait can instead be handled by polling/waiting again.

## Native reuse, errors and lifetimes

- `ControlChannel::submit_mode(&mut self, &ModeCommand)` calls the existing
  `ams_mel_ir_c2_submit_mode` once. Its complete value-only input remains alive
  until native copying returns. No borrowed Rust memory escapes the call.
- The same existing `ModeRequest` calls `ams_mel_ir_mode_request_wait` and
  `ams_mel_ir_mode_request_close`. No second owner, Rust worker, queue, callback
  or synchronization engine is introduced. The existing `submit_operate`
  implementation and native convenience entry point are unchanged.
- Native Enabled-only lifecycle checks, per-Session admission and categories
  remain authoritative: pre-enable/failed lifecycle is ProviderFailed,
  exhausted admission ResourceExhausted, malformed inputs InvalidArgument,
  send/future exceptions ProviderException, malformed/null result ProviderFailed.
  Calling the general method after successful channel Close reaches native's
  null-owner InvalidArgument check. No undocumented recovery is promised.
- Success and explicit MEL rejection are distinct normal terminal results.
  Rejections retain actual code and complete UTF-8 description, including
  empty and long descriptions. Existing native malformed-description policy is
  unchanged; malformed output is not presented as valid success.
- Zero-timeout polling and finite waits do not cancel. Terminal waits are
  cached by native code, including long-diagnostic retries. The wait decoder
  was factored behind a private callable seam solely to exercise malformed
  outputs and changed terminal-retry records; its checks are unchanged.
- Requests can outlive C2 and Session closure. Pending work retains the native
  provider graph and its admission permit, even after observer Close/Drop.
  FinalOwner reclamation governs deferred teardown and capacity release.
- Existing owners stay non-Clone/non-Send/non-Sync. Safe mutable/consuming
  methods serialize access. Drop is non-panicking and request release is once
  per unique native owner. The submission guard owns even an unexpected
  non-null error output, so it is released rather than leaked or published.

## Qualification

Local evidence directory: `/tmp/rust-ir-009-evidence`. All gate commands run
sequentially from `/home/zboll/git/ams-rust-ir-009`; none overlap native build
tree mutation. Cargo uses:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

For `make check` these two overrides are unset (Alire supplies its own provider
environment), and the installed GNAT 16.1.0/GPRbuild 26.0.1 directories are
prepended to PATH. Ada formatting is checked using the manifest-pinned
GNATformat 26.0.0. No Ada source changes or toolchain downloads are needed.
Rust/cargo: 1.98.1.

Focused development qualification passed: seven private tests and nine public
integration tests, no failures or skips. The public enum test submits all
60 state/mode pairs. Pending/multiple-request tests use the existing mock
promise gate and native FinalOwner observation barrier, not sleeps. Full-width
and IEEE bit fidelity are tested privately; complete ordinary ScanParam fidelity
is checked in the existing separate native mock provider. Existing raw ABI
tests remain unchanged.

| Sequential gate | Actual result |
|---|---|
| `make native` | PASS, production build, warnings treated as errors |
| `make test-native` | PASS, 555/555 native tests |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | PASS, 401 passed, 0 failed, 9 pre-existing ignored subprocess-entry tests |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets` | PASS |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | PASS |
| `make check` | PASS: build-tree isolation, 555 native tests, pinned Ada formatting, 56 inventory tests, Ada compile/smoke suites, final newlines and whitespace |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --lib ir_mode` | PASS, 7 passed, 0 failed/ignored, 116 unrelated tests filtered |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_mode` | PASS, 9 passed, 0 failed/ignored/filtered |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files / 3 workflows |

The Rust total includes seven new private tests, nine new public integration
tests and one executable doctest. The nine ignored entries are existing
subprocess helpers and are not counted as passes. No new test is skipped.
The complete documentation usage example was separately compiled as a Rust
2021 library with `rustc --emit metadata -D warnings` against the built crate.
Its initial private helper triggered the dead-code lint; making this example
function public corrected that documentation-only issue. The final example is
warning-clean. An initial newline check caught the new document's missing final
newline; it was corrected before completion of the aggregate gate.

The aggregate gate prints existing GPR project-discovery warnings that the
source-free imported C library has no C sources; no compiled-source warning
was introduced. After the aggregate gate, an extra Rust rerun exposed that
Ada's targeted `build-test-providers.sh` configuration leaves only Ada-required
fixtures in the test tree: an unrelated existing RF missing-symbol test saw
LibraryLoadFailed rather than SymbolNotFound because its fixture was absent.
The complete test tree is restored with `make test-native` before repeating the
final Rust static/test/doc and focused release qualification. This is an
environment sequencing correction, not a native/test-contract change.

## Consumer parity and remaining boundaries

Safe Rust is no longer restricted to Operate/TaskSched. It can express the
required C++ consumer's Standby/Unused command and the complete published
ModeCmd scan payload, then observe either success or explicit rejection.
The minimal C++ example was inspected at commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`: its consumer submits
Standby/Unused with ID 302, and its provider routes ModeCmd through a hardware
stub. This is not evidence of physical scanning support. Qualification here
uses the repository's separate deterministic mock, not that provider or hardware.

Remaining gaps include safe Rust ConfigSet, full payload-bearing BIT, complete
Control discovery/other typed channel owners, general buffer registration and
format coverage beyond existing Mono8 acquisition, and StackedImage. Independent
NavigationReport PR #186 remains separate and was not merged by this task. No full
minimal consumer, DSP, services, private transports, cancellation, hardware
transition verification or broad provider interoperability is claimed. C++ ABI
compatibility and provider adherence to memory/lifetime contracts remain required.
No additional unresolved upstream semantics are guessed; unspecified altitude
datum and provider operational capability remain unspecified.

Delivery status is `HOSTED_CI_PENDING` until the exact final HEAD's required
hosted gates pass. The PR must remain open, unmerged, with auto-merge disabled
and without self-approval.
