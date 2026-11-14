# RUST-IR-010 — complete safe Rust C2 ConfigSetCommand

## Source preflight and coverage delta

- Starting freshly fetched `origin/main` commit:
  `76ade6cd6d4b0fa51c04947c040dfac3633d8886`.
- Starting tree: `89c9c5082c3483f4e60e0cfaf6dc8d14b8cd3c83`.
- Branch: `feature/rust-ir-010-config-set`, created directly from that commit in
  `/home/zboll/git/ams-rust-ir-010`. Existing worktrees, branches and history
  were preserved.
- PR #188 (RUST-IR-009) is merged; its merge commit is the starting commit above.
  General ModeCmd support is present. Open PRs #183, #184 and #189 and remote
  branch names were inspected; no overlapping Rust ConfigSet work was found.
- Reviewed repository/contribution policies, architecture/ABI policy, provider
  foundation task, IR coverage schema and ConfigSet inventory entries, upstream
  ConfigSetCommand, native implementation/C tests, raw Rust bindings, Ada and
  Python conversions/tests, and existing Rust Mode/Return completion ownership.
- The structured IR inventory classifies native/Ada coverage, not safe Rust.
  Those implementations and classifications are unchanged. This document owns
  the safe Rust delta: complete ConfigSet input and asynchronous submission.

Production changes are confined to safe Rust: a command module/public export,
submission using the existing ReturnRequest, and a private injectable wait-call
seam around the unchanged Return decoder/cache-retrieval algorithm. No new owner,
completion engine, Rust thread, callback, queue or future consumer is introduced.
Native ABI/exports, sys declarations, providers, vendor files, Ada, Python,
dependencies, CI policies and historical coverage are not changed by this task.
Final commit/tree identities belong in delivery/PR evidence, outside the commit's
own contents, avoiding self-referential hashes.

## Public API and complete mapping

```rust
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ConfigSetCommand {
    pub command_id: u32,
    pub system_time_ns: i64,
    pub config: String,
}
```

`ConfigSetCommand::new(command_id)` sets time to zero and text to empty, matching
the published C++ defaults. Fields remain public and owned. Validation occurs at
submission, including after callers mutate the String.

`ControlChannel::submit_config_set(&mut self, &ConfigSetCommand)` returns the
existing `Result<ReturnRequest, Error>`.

| Rust | C ABI | Published C++ |
|---|---|---|
| `command_id: u32` | `command_id` | `setCommandID(uint32_t)` |
| `system_time_ns: i64` | `system_time_ns` | `setSystemTime(chrono::nanoseconds)` |
| `config.as_bytes()` | `config.data` in `AmsMelStringViewV1` | `setConfig(string)` |
| `config.len()` (bytes) | `config.size` | exact string byte count |

All command-ID bits and the entire signed 64-bit nanosecond range are preserved.
No clock, epoch, UTC, normalization or conversion is inferred. Text is opaque
UTF-8; it need not be JSON/XML or any provider/hardware schema. Empty text is
allowed and uses the existing ABI-valid null-pointer/zero-size view. Embedded
NUL yields InvalidArgument before native submission. Rust String guarantees
UTF-8; original Unicode byte sequences, including composed/decomposed text and
line endings, are not parsed, normalized, encoded again or counted as characters.

The original String stays alive through the single synchronous native submit.
The native implementation validates the view and copies all fields into the
published C++ command before send. No borrowed text escapes the call. The caller
may drop or mutate its command immediately after submission returns.

## Results, admission and ownership

- Native submission failure is `Err(Error)`; no successful request is fabricated.
  Before Enable, after failed Enable, after Close, provider send exceptions and
  Session resource exhaustion remain distinct native errors.
- `ReturnRequest::wait(0)` polls; positive milliseconds wait finitely. Timeout
  is `Err` with Timeout and is not cancellation or a completed command.
- `Ok(ReturnResult::Completed { value })` means the future returned a published
  Return payload. Success, BadPointer, Fail, NotSupported and NotImplemented are
  all payloads, not Rust transport exceptions. Success is not evidence that a
  physical sensor applied configuration.
- `Ok(ReturnResult::Rejected { code, description })` means explicit MEL ErrorOr
  rejection, preserving the actual error and complete UTF-8 diagnostic.
- Future exceptions, null/malformed successful provider results and malformed
  ABI diagnostics are errors, never synthetic Return success. Terminal outcomes
  are cached natively. Long terminal diagnostics are fetched with a zero-timeout
  cached wait and checked for unchanged status/result/size, never a second send.
- CommandStatus metadata registration is independent and opt-in. Accepted
  metadata may coexist with explicit future rejection; Rejected metadata may
  accompany a completed Return::Fail. Neither path is reinterpreted as the other.

Submission establishes the existing RAII owner before the one native call. An
unexpected non-null error output is released once by non-raising Drop. Successful
status with a null request is ProtocolInconsistency. Diagnostic handling uses
the existing one-shot helper and never retries submission. The error path does
not explicitly release then allow Drop to retry a potentially freed pointer.

The existing request is not Send/Sync and serializes Wait versus consuming Close
through safe Rust ownership. Close/Drop release only its observer and never
cancel native work. Native admitted requests retain the C2/Session/provider graph
independently of public parents. Pending observers can be dropped, channel and
Session closed in either order, and remaining requests still waited on. Physical
channel destruction and library unload follow native completion/owner ordering.
Pending Close does not prematurely free shared Session admission capacity.
Never-completing provider work can retain that graph indefinitely; existing
fail-safe native retention on uncertain teardown is not weakened.

## Complete usage example

```rust
use ams_mel::{
    CommandReturn, ComponentLocation, ConfigSetCommand, ControlConfig, Error,
    ReturnResult, Session, UciId,
};
use std::path::Path;

pub fn configure(provider: &Path) -> Result<ReturnResult, Error> {
    let session = Session::open(provider, "application-instance", "")?;
    let config = ControlConfig::new(
        UciId::new([0; 16], "C2")?,
        UciId::new([1; 16], "platform")?,
        ComponentLocation::new(0.0, 0.0, 0.0, "station", "system")?,
    );
    let mut channel = session.open_control_channel(&config)?;
    channel.enable()?;
    let mut command = ConfigSetCommand::new(301);
    command.system_time_ns = -123; // raw nanoseconds, no inferred epoch
    command.config = "opaque configuration-€".into(); // empty is also valid
    let mut request = channel.submit_config_set(&command)?;
    channel.close()?;
    session.close()?;
    let result = request.wait(1_000)?;
    match &result {
        ReturnResult::Completed { value: CommandReturn::Success } => {
            // Future completion only; do not claim physical application.
        }
        ReturnResult::Completed { value: _ } => { /* other published Return */ }
        ReturnResult::Rejected { code: _, description: _ } => { /* MEL rejection */ }
    }
    request.close()?;
    Ok(result)
}
```

Provider path, channel identity and opaque text must come from the application's
published-provider contract. This example does not assert universal configuration
support. A timeout exits with Error here; RAII releases the observer, not work.
Applications that need to retry Wait should retain their request on Timeout.

## Qualification and implementation evidence

Evidence logs: `/tmp/rust-ir-010-evidence`. Gates run sequentially in this
worktree, with no overlapping native build-tree mutation. Cargo uses:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

For `make check` these overrides are unset; installed GNAT 16.1.0 and GPRbuild
26.0.1 are prepended to PATH, and Alire supplies pinned GNATformat 26.0.0.
Rust/cargo are 1.98.1. No first-party Ada sources change.

| Sequential gate on the initial base | Actual result |
|---|---|
| `make native` | PASS |
| `make test-native` | PASS, 555/555 native tests |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | PASS, 439 passed / 0 failed / 9 existing ignored subprocess helpers |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets` | PASS |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | PASS |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --lib ir_config_set` | PASS, 8 passed / 0 failed/ignored |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_config_set` | PASS, 9 passed / 0 failed/ignored |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files / 3 workflows |
| Standalone documentation example, `rustc --edition=2021 --crate-type=lib --emit=metadata -D warnings` | PASS against the built safe crate |

Development corrections: an initial compile check caught an attempted use after
the consuming Session::close in a test; the redundant Drop was removed. The new
document's missing final newline was corrected. The repository newline checker
also identifies the inherited `docs/pol002-event-fixture-cleanup.md` defect,
independently owned by corrective PR #190; this Rust task does not modify it.
One additional focused test attempt while the aggregate isolation gate was
rebuilding its test tree failed to link `-lams_mel_c` because the gate had removed
that tree. It was not counted as a test pass. Subsequent final qualification must
run only after the aggregate gate and complete fixture restoration, with no
overlap. No implementation/test contract is changed to work around it.

`main` advanced independently to Python ModeCmd merge
`5a5ad58b442b357eabc5f427bfd5b4312ad4c365` (tree
`00af4b3e979dad63a3d2fe336e7fe555b9696fdc`). It was incorporated through a
normal merge, without rebasing or rewriting history. Integration merge commit:
`6cd76a1b5dd0de3495421d6d7a6ddac05345c7ff`, tree
`3ae7725568b87cb3a3eab030e5010b7d388e18ff`. The feature diff against this
main still contains only four safe Rust implementation/test paths and this
task-owned document; Python changes are inherited through main, not task edits.

The first `make check` completed build-tree isolation, all 555 native tests,
pinned Ada formatting, 56 inventory regressions, Ada compilation and every
smoke/focused suite successfully, then exited **2** solely on the inherited
missing newline in `docs/pol002-event-fixture-cleanup.md`. It is not reported as
passed. No unrelated repair or gate weakening is included. The additional
`make test-policy` passed all 244 tests across eight suites; standalone IR
inventory checking and its 56 regressions passed as well. Aggregate GPR output
includes existing source-free imported C project discovery warnings, not a new
compiled-source warning.

On the integrated source, `make native`, `make test-native` (555/555), full
workspace tests (439 passed, 0 failed, 9 existing ignored helpers), check,
Clippy, format, warning-denied rustdoc, and focused release tests (8 unit plus
9 integration, none failed/ignored) all passed again. The raw ABI tests are part
of each workspace run. No new test is skipped. The ignored entries are existing
child-process entry points explicitly invoked by their parent scenario tests.

The integrated `make check` repeated the successful native/isolation/format/
inventory/Ada stages and again exited **2** only for that inherited newline
defect. Afterwards `make test-native` restored the complete fixture tree and
passed 555/555; the final workspace rerun passed 439 tests with 0 failed and
9 existing ignored helpers. Source integrity again passed (824 vendor files,
3 workflows). No incomplete aggregate result is presented as success. Delivery
remains blocked from a merge-ready claim by this independently owned defect
and pending exact-HEAD hosted checks/review.

Native header/export and sys declaration SHA-256 identities remain unchanged:

| Path | SHA-256 |
|---|---|
| `native/include/ams_mel/abi.h` | `314a204ebe89692b54c7cf5697210c872600c29d7525538ff3216547b2ffc5b3` |
| `native/src/exports.map` | `be9a6189511e33c0928afcaf3488f0dcc9f4ae0132885bfab708bf5570119616` |
| `rust/ams-mel-sys/src/lib.rs` | `922a0acef58f9fd359302d66704dd6c796919fc371d964fbe7c29ae9e158a971` |

Focused debug tests pass: eight unit tests and nine public integration tests,
zero failures/ignored. Release repeats the same eight/nine tests successfully.
Private tests inspect every field, exact original pointer/byte length, empty
view and signed extremes. Existing config-full/empty/positive compiled provider
scenarios independently validate complete ordinary native-to-C++ conversion.
Public tests submit all numeric extremes and Unicode, then drop input storage.
The fixture does not inspect extreme values; exact extreme-field evidence is
the private boundary test plus unchanged native direct assignments.

Lifetime/admission tests use existing promise and FinalOwner barriers, not
sleeps. They cover three outstanding ConfigSet requests, explicit pending Close
and Drop, both parent-close orders, repeated waits, shared capacity across two
C2 channels and legacy BIT/Operate, and ordered channel destruction/DSO unload.
The existing fixture's admitted held-future exception qualifies provider failure.
An intentional config-full mismatch qualifies synchronous send failure and
subsequent channel usability. The success/fail/reject fixtures qualify the
independence of CommandStatus versus request-future completion.

Malformed/null owner, all five Return values, long multibyte rejection retrieval,
invalid UTF-8/NUL/missing termination and changing cached results use private
Rust injection at the real submission/Return decoder seams. No unsafe fabricated
owner address is dereferenced: error-owner cleanup transfers a real unique native
request and disarms its original Rust owner. These are hostile-boundary tests,
not claims that ConfigSet's provider fixture generates malformed/long results.
The existing provider has no ConfigSet-specific malformed or long-rejection
scenario; it is intentionally not modified for this Rust-only task.

## Remaining boundaries and minimal-consumer parity

The minimal example was inspected at commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`: it sends ConfigSet with ID 301,
default zero timestamp and empty text because no configuration schema is
advertised. Safe Rust can now express that complete operation as well as
nonempty opaque UTF-8. Its existing general ModeCmd and NavigationReport remain
unchanged.

No real provider, physical sensor, Squall, credentials or containers are used
for qualification. Provider adherence to published lifetimes and C++ ABI
compatibility remains required behind the facade. No parsing, operational
validation, callback quiescence, cancellation or physical effect is inferred.
Remaining full-consumer gaps include broader Control discovery/typed channels,
general buffer registration and unconfigured layouts, and StackedImage. Full
payload-bearing BIT remains unimplemented; this task neither adds it nor claims
the entire minimal C++ consumer, DSP or private-transport support.

Delivery requires independent technical review. Leave the PR open/unmerged,
auto-merge disabled and no self-approval. Use `HOSTED_CI_PENDING` until required
Bootstrap and Documentation checks on the final HEAD complete.
