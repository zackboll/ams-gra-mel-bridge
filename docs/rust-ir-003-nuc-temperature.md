# RUST-IR-003 — safe Rust Image/Health NUC temperature reception

## Scope and public API

Starting main: `c85aee316cc745168ece12d63eda434a3b0baac0` (merged #151).
Implementation and qualification used the dedicated
`/home/zboll/git/ams-rust-ir-003` worktree with separate native production/test
trees and worktree-local Cargo output. No other feature worktree was modified.

`ams_mel::ir_nuc` and the crate root expose:

```rust,ignore
ImageStream::open_nuc(&mut self, queue_capacity: usize) -> Result<NucStream, NucOpenError>
HealthStatusChannel::open_nuc(&mut self, queue_capacity: usize) -> Result<NucStream, NucOpenError>
NucStream::is_open(&self) -> bool
NucStream::receive(&mut self, timeout_ms: u32) -> Result<NucTempData, Error>
NucStream::counters(&mut self) -> Result<NucCounters, Error>
NucStream::close(&mut self) -> Result<(), Error>
NucOpenError::error(&self) -> &Error
NucOpenError::registration(&self) -> Option<CommandReturn>
```

The stream uniquely owns one native handle, is neither Send nor Sync, and has
non-panicking best-effort Drop. No dependency, callback, thread, trampoline,
provider mutex or Rust queue was added. Existing frame and ordinary Health
metadata representations and behavior are unchanged.

Owned data types:

- `NucPixelValue::{U8(u8), U16(u16), U32(u32)}` preserves the active alternative,
  including small values in wider alternatives; invalid width/value pairs reject.
- `NucPointType::{NucPoint, MfaEstimate, Other}` maps exactly 0, 1 and 2.
- `NucTempPoint { timestamp_ns: i64, frame_band: BandInfo, pixel: NucPixelValue,
  temperature_k: f64, point_type: NucPointType }` preserves every publicly readable
  field, including signed nanosecond extremes, strict published band kinds,
  meter limits, full uint32 values and IEEE double bits.
- `NucTempData { points: Vec<NucTempPoint> }` preserves order, duplicates and empty
  collections, independently of every native owner and provider library.
- `NucCounters { events_received, events_dropped_queue_full,
  malformed_or_unsupported }` uses three `u64` fields.

Upstream's private optional `tempError` has no public accessor. It is not accessed,
reconstructed or represented. Absence here does not assert absence in a provider;
TempPoint remains partial solely for that inaccessible upstream field.

## Registration, errors and lifetimes

Both Image and Health tests observe ProviderFailed + Some(NotSupported) for
unsupported optional registration, ProviderFailed + Some(Fail) for rejection,
and ProviderException + None for a throw. These are machine-readable outcomes,
not diagnostic parsing. Repeated attempts return InvalidArgument + None. Zero
capacity never invokes registration and leaves the one-shot available. Success
requires a non-null owner and a successful provider Return; any unexpectedly
published owner on failure is released. No actual registration is retried.

One shared receive path invokes Receive once, validates the outer view and
entire point span before slicing, fallibly reserves owned storage, copies while
the native event guard is alive, and releases the event before returning.
Conversion errors, native errors with unexpected events, and unwind release the
event too. Success from Close without clearing the handle is a protocol error;
the stale handle is forgotten rather than freed twice, including when the Close
diagnostic is also malformed. Native status categories remain unchanged.

Close deactivates consumption, not provider registration. Image stops with its
stream; Health stops with channel teardown. Queued events drain before terminal
status. Late callbacks and code retention are native responsibilities. Isolated
`nuc-nonquiescing` barriers test both explicit Close and Drop for both families:
callback entered, public owner closed, callback returned, library unloaded, in
that order without sleeps. Uncertain-detach ProviderFailed classification is
covered with an injected Receive result; the existing mock does not combine its
Health detach-failure scenario with successful NUC registration.

## Local qualification

All commands below passed on the delivered source. Native builds/tests use
`native/build` and `native/build-tests`, respectively. Cargo test runs used
`AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"` and
`AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"`.

| Command/check | Result |
| --- | --- |
| `make native` | production facade built |
| `make test-native` | 435/435 passed |
| `make test-build-isolation` | passed, both build orders and cross-tree checks |
| `ctest --test-dir native/build-tests -R '^ir_nuc_contract$' --output-on-failure` | 1/1 passed |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_nuc` | 15 passed; one child entry intentionally ignored by the parent harness |
| focused integration and `--lib ir_nuc` at `--test-threads=1`, `4`, `16` | 15 integration + 11 unit tests passed at each setting |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | 253 passed including doctests; five isolated child entry points intentionally ignored in outer runs |
| `cargo check --manifest-path rust/Cargo.toml --workspace` | passed |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | passed |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | passed |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | passed |
| `make test-policy` | passed |
| `git diff --check` | passed |

Integration tests exercise both families against the compiled mock: exact rich
empty/one/three-point synchronous messages and every numeric field from the C
test; 4/0/3 malformed counters; 5/3/0 overflow with first-two preservation;
2/0/1 callback allocation failure with continued consumption; late callback drain;
zero/repeated/failed registration; ordinary metadata independence; explicit and
implicit closure; closed operations; Image Stop; parent-first and Session-first
closure; independent Image/Health streams; actual provider unload. Unit tests
also cover every published band, all point/pixel domains and bounds, malformed
spans/outer views, swapped numeric negative controls, IEEE bits, errors/unwind,
unexpected Open/Receive outputs and malformed Close results.

## Compiled mutation evidence

Each mutation was applied independently to the implementation, compiled, executed
against an ordinary Rust test, and restored before final validation. All seven
compiled successfully and failed a runtime assertion: **7 applied, 7 killed,
0 surviving, 0 invalid**.

| Mutation | Killing test suffix |
| --- | --- |
| Remove NucStream Drop's native Close call | `stream_drop_invokes_close` (observed 0 closes, expected 1) |
| Remove native event cleanup on error/unwind | `event_lifetime_success_conversion_error_and_unwind` |
| Convert UINT16 into U32 | `pixel_widths_bounds_and_unknowns` |
| Rotate point order | `exact_numeric_mapping_extremes_bits_order_and_duplicates` |
| Copy max wavelength into min | `exact_numeric_mapping_extremes_bits_order_and_duplicates` |
| Collapse registration exceptions into ProviderFailed | `open_validates_both_outputs_and_cleans_unexpected_owners` |
| Collapse Receive statuses into ProviderFailed | `receive_once_preserves_status_and_guards_unexpected_event_on_failure` |

Private Rust-only operation injection observes cleanup deterministically, without
adding any production C ABI test hook. No source-text inspection substitutes for
the runtime assertions. Local raw logs are retained under the ignored worktree
output directory `/home/zboll/git/ams-rust-ir-003/rust/target/qualification`.

## Production linkage and export verification

A separate Cargo output linked against `native/build/lib`, not the test facade:
workspace build, all 11 NUC unit tests and 14 integration tests passed (only the
test-facade allocation failpoint case was excluded). This includes rich reception
and both callback barrier tests. ELF RUNPATH points to that production directory and NEEDED includes
`libams_mel_c.so.0`; the NUC executable has no undefined `ams_mel_test*` dependency.
The production dynamic export inventory exactly matches the baseline map: 229
`AMS_MEL_0.1` exports, no test exports. The unchanged export map SHA-256 is
`e0ecdebcdd18f384ea508350ec5017dd22d782324094c70b5070830a021caeb5`.

The seven reused exports are `ams_mel_ir_image_nuc_open`,
`ams_mel_ir_health_nuc_open`, `ams_mel_ir_nuc_receive`,
`ams_mel_ir_nuc_get_counters`, `ams_mel_ir_nuc_close`,
`ams_mel_ir_nuc_event_view`, and `ams_mel_ir_nuc_event_close`.

No native interface, raw binding, Ada, Python, vendor, workflow or source-policy
change is included. These are local source/mock results, **not real hardware or
Squall qualification**. Hosted exact-head Bootstrap status and final commit/tree
identities are delivery metadata reported separately in the PR/handoff; a local
pass does not imply a hosted CI pass.

## RUST-IR-003-R1 — closed Health owner preflight correction

Reviewed starting commit: `86e015aa295ea2eabddfadc2d6f89b91beb40390`;
reviewed tree: `4113be1de8228a463f3b7cef94004b154b48acdb`.
Local HEAD, remote feature HEAD and PR #156 HEAD matched, with a clean worktree.
Current main `80dcd76ee92b645cf41e8ddc6c6124e91cd7b91c` was incorporated by
ordinary merge commit `6439a96c97d370c7375dff5f2331c38c8fa6ea2b` (merge tree
`b3c93043117c5893148d1c2bc45be89377e30939`). The Python package changes came
only from that upstream merge; they are not corrective feature edits.

The corrective source changes are limited to `ir_health.rs`, `ir_nuc.rs` and
`tests/ir_nuc.rs`, plus this report. `HealthStatusChannel::open_nuc` now calls
the existing `live()` before capacity validation or native entry, matching
`open_metadata`. A crate-private `NucOpenError::preflight` retains that error
without fabricating a provider Return. Both null and successfully closed Health
owners, at capacities 4 and 0, return:

- `ErrorKind::ProviderFailed`;
- diagnostic exactly `IR Health channel is closed`;
- `diagnostic_required() == None` (no native diagnostic call);
- `registration() == None`.

The private null-owner unit test compares the error with `open_metadata`. The
isolated integration regression closes a real mock Health owner before Open,
checks the same contract and zero-capacity precedence, observes no
`nuc_registration_attempted` provider log entry, then verifies normal Session
teardown and provider unload. Existing live zero-capacity checks now additionally
assert that exactly one subsequent registration reaches the provider. Image NUC
behavior and all actual native registration-result handling remain unchanged.

### Original-defect negative control

A disposable Rust source copy was created at
`/home/zboll/git/ams-rust-ir-003/rust/target/qualification-r1/mutant/rust`.
Only in that copy, the new liveness guard was replaced with
`let health = self.raw;`, restoring direct native entry. With
`AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"`,
`AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"`, and
`CARGO_TARGET_DIR="$PWD/rust/target/qualification-r1/mutant-target"`, the actual
commands were:

```sh
cargo test --manifest-path rust/target/qualification-r1/mutant/rust/Cargo.toml \
  -p ams-mel --lib null_owner_nuc_preflight
cargo test --manifest-path rust/target/qualification-r1/mutant/rust/Cargo.toml \
  -p ams-mel --test ir_nuc closed_health_nuc_preflight
```

Both compiled and exited **101**, failing `assert_eq!` on error kind:
`left: InvalidArgument`, `right: ProviderFailed`. The integration failure was
observed inside the isolated child and propagated to its parent test. The
correct source was copied back into the disposable tree and both commands then
exited **0**. The working feature source was never mutated. Result: **1 applied,
1 killed, 0 surviving, 0 invalid**, observed by two regressions. The earlier
seven mutations were not rerun: their implementation targets are unchanged.

### Reconciled-tree local acceptance

The full acceptance campaign passed after incorporating main:

- `make native`; `make test-native` (**435/435**); `make test-build-isolation`.
- Focused integration: **16 passed**, one intentionally ignored child entry.
- Focused NUC tests at **1, 4 and 16 threads**: **16 integration + 12 unit tests**
  at each setting (the unit filter `nuc` includes the new Health preflight test).
- Rust workspace: **255 passed**, including doctests; five isolated child
  entries intentionally ignored in outer runs.
- Workspace `cargo check`, `cargo clippy --all-targets -- -D warnings`,
  `cargo fmt --all -- --check`, and
  `RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps`, all using
  `--manifest-path rust/Cargo.toml`.
- `make test-policy` and `git diff --check`.
- Separate production-facade Cargo build, **12 NUC unit tests** and **15
  integration tests** passed; only the test-facade allocation failpoint case was
  excluded. ELF RUNPATH names `native/build/lib`, NEEDED names
  `libams_mel_c.so.0`, and there is no undefined `ams_mel_test*` dependency.
- Production exports exactly match the reviewed unchanged map: **229** versioned
  exports, all seven original NUC exports, no test exports. Production C/C++,
  native providers, ABI headers/maps, raw Rust bindings and ImageStream source
  are unchanged from the reviewed commit.

An initial focused Rust attempt overlapped rebuilding the native library and
failed to link against the in-progress artifact. It is not counted as a pass;
the complete Rust acceptance campaign ran successfully after native/build
isolation checks finished. Raw R1 logs are retained under
`/home/zboll/git/ams-rust-ir-003/rust/target/qualification-r1`.

These results remain local source/mock qualification, not hardware or Squall
qualification. Final commit/tree and the one-time exact-head hosted Bootstrap
observation are reported separately in PR #156 and the handoff.
