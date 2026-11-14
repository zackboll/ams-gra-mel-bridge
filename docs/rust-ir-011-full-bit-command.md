# RUST-IR-011 — complete safe Rust C2 BIT_Command

## Source preflight and coverage delta

- Freshly fetched starting `origin/main` commit:
  `b22ddb64f5deaf9994fcd9478e56dbdbf7e06148`.
- Starting tree: `fa7ea1a8daf60a9c518a43e78f60f99d2614da7a`.
- Dedicated branch: `feature/rust-ir-011-full-bit-command`, worktree
  `/home/zboll/git/ams-rust-ir-011`. Unrelated worktrees/history were preserved.
- RUST-IR-009 PR #188 is merged at
  `76ade6cd6d4b0fa51c04947c040dfac3633d8886`; RUST-IR-010 PR #191 is merged
  at the starting commit above. Their implementations are present and ancestors.
- Open PR #184 and fetched remote branches were inspected before editing. No
  overlapping Rust BIT work was found. The older Task 015 BIT-no-op branch was
  inspected as existing functionality, not used as the starting branch.
- Reviewed AGENTS/CONTRIBUTING and repository rules, architecture/ABI policy,
  provider foundation, Task 017 validation, pinned BIT_Command, native C tests
  and conversion, safe Ada one-choice operations, sys declarations and existing
  safe Rust Mode/ConfigSet/no-op/Return completion and deterministic probes.
- The structured IR inventory schema and BIT operation/type entries classify
  native/Ada, not safe Rust. Neither implementation/classification changes here;
  that inventory and its reports remain unchanged. This document owns the Rust
  delta from no-op-only BIT to all three payload-bearing operations.

Production changes are confined to safe Rust. No native implementation, ABI,
export, raw sys declaration, provider fixture, vendor, Ada, Python, dependency,
CI policy or historical coverage change is included. Existing
`ams_mel_ir_c2_submit_bit`, `ams_mel_ir_return_request_wait` and
`ams_mel_ir_return_request_close` match the sys declarations and are reused.
No discrepancy or unresolved conversion semantics was found.

## Safe API and one-choice semantics

```rust
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BitAction {
    NoOp,
    Initiate(Vec<u32>),
    Cancel(Vec<u32>),
    ClearFaultCodes(Vec<String>),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BitCommand {
    pub command_id: u32,
    pub action: BitAction,
}
```

`ControlChannel::submit_bit(&mut self, &BitCommand)` returns the existing
`Result<ReturnRequest, Error>`. Public collections are owned and validation
occurs on every submission, including after mutation. Initiate, Cancel and
ClearFaultCodes must contain at least one element; empty input is
`InvalidArgument` before native submission. Use NoOp deliberately instead.
`submit_bit_noop(command_id)` retains its original signature and implementation.

The pinned IR MEL revision is `8d9224519f12b44e0b28815755c56a32a28d24a0`,
tree `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`.
`native/vendor/ir-mel/include/irmel/library/c2/BIT_Command.h` explicitly assigns
the application responsibility to populate only one vector. Its documented
mixed-vector precedence is initiate, then cancel, then clear faults. The safe
enum prevents ambiguous mixed input rather than silently invoking precedence.
The existing raw ABI intentionally still transports all three vectors.

| Rust action | `AmsMelIrBitCommandV1` mapping |
|---|---|
| NoOp | All three spans null/zero |
| Initiate(ids) | `initiate_bit_ids = { ids.as_ptr(), ids.len() }`; other spans null/zero |
| Cancel(ids) | `cancel_bit_ids = { ids.as_ptr(), ids.len() }`; other spans null/zero |
| ClearFaultCodes(codes) | `clear_fault_codes` spans call-local string views; both numeric spans null/zero |

`command_id` and every numeric BIT ID retain all `u32` bits, including zero,
high-bit values and MAX. IDs are numeric selectors, not UCI UUIDs. The binding
does not infer a conversion from the UUID identities published in BIT metadata,
validate against a catalog, or impose physical-device restrictions. Ordering
and duplicate numeric IDs/fault strings are unchanged.

Fault codes are opaque owned UTF-8 Strings. Rust guarantees valid UTF-8; the
wrapper rejects embedded NUL before calling native code. String byte lengths,
not character counts, populate `AmsMelStringViewV1.size`. Unicode composition,
decomposition, whitespace and line endings remain byte-exact: no normalization,
sorting, trimming, parsing or deduplication. A nonempty collection may contain
an empty string, represented by the existing native null/zero view contract.

## FFI storage and completion ownership

The original command's numeric vectors and String buffers are immutably borrowed
through submission. Only ClearFaultCodes allocates additional input storage: a
temporary owned `Vec<AmsMelStringViewV1>`, reserved with the existing fallible
allocation helper (including allocation-size overflow checks). Descriptors are
built without unchecked length arithmetic, and stay alive through the one
native call. All inactive spans are explicitly constructed as null/zero.

Native `ams_mel_ir_c2_submit_bit` synchronously copies both numeric spans and
every string before constructing the upstream command and invoking provider
send. No borrowed input pointer escapes into ReturnRequest. The application
can mutate or drop the command immediately upon return. The private helper
accepts FnOnce, and diagnostic decoding never retries submission.

The existing ReturnRequest RAII owner is established before native submission,
so unexpected non-null output on an error or malformed successful diagnostic
also has exactly one cleanup path. Null successful request output fails closed
as ProtocolInconsistency. Existing non-panicking Drop invokes best-effort close
once; the native close exchanges the handle to null before deleting it. No
additional cleanup retry or ownership path is introduced.
No new BIT owner, Rust worker thread, callback, queue or completion engine exists.

- Submission requires Enabled C2. Attached, failed Enable and closed channel
  states retain the native error behavior; failed Enable is not retried.
- Session-wide admission applies across BIT, legacy no-op, Mode and other
  asynchronous operations/channels. Pending request Close does not free its
  permit while provider work remains outstanding; exhaustion is ResourceExhausted.
- `wait(0)` polls. Timeout neither consumes nor cancels work; retain the request
  if the application wants to wait again. Terminal results/errors are cached.
- Completed `CommandReturn::{Success, BadPointer, Fail, NotSupported,
  NotImplemented}` are ordinary payloads. Fail is not a transport exception.
- Explicit MEL rejection is `ReturnResult::Rejected { code, description }`,
  preserving the actual code and full UTF-8 description. Provider exceptions
  remain errors. Native null/unknown provider result validation remains intact.
- CommandStatus and BIT metadata are separate registered streams, not implicit
  subscriptions or substitutes for request completion. Even Accepted
  CommandStatus can accompany completed Fail or an explicitly rejected future.
- Requests may outlive C2 and Session in either closure order. Public Close/Drop
  does not cancel pending work. Native retained ownership defers physical
  detach/destruction/unload until work and callback-accessible objects are safe.
  Close alone is not a callback-quiescence promise.

Successful submission, provider acceptance or Return::Success does **not** prove
that a BIT physically executed, a cancellation happened, or a fault was cleared.

## Examples

With an attached and enabled `ControlChannel`, each action uses the same owner:

```rust
use ams_mel::{BitAction, BitCommand, ControlChannel, Error, ReturnResult};

pub fn bit_examples(channel: &mut ControlChannel) -> Result<Vec<ReturnResult>, Error> {
    let mut results = Vec::new();
    for action in [
        BitAction::NoOp,
        BitAction::Initiate(vec![1, 0x8000_0001, u32::MAX, 1]),
        BitAction::Cancel(vec![7, 9]),
        BitAction::ClearFaultCodes(vec!["fault-alpha".into(), "fault-€".into()]),
    ] {
        let command = BitCommand { command_id: 301, action };
        let mut request = channel.submit_bit(&command)?;
        let result = request.wait(1_000)?;
        request.close()?;
        results.push(result);
    }
    Ok(results)
}
```

Provider path and channel configuration must come from the published provider
contract. Session/open/attach/Enable and metadata APIs are unchanged. A Timeout
returned by this example drops the observer, not work; a real polling consumer
should retain its request to retry Wait. The source API includes a compiled
rustdoc example for all four variants.

## Test evidence and fixture boundaries

- `rust/ams-mel/src/ir_bit/tests.rs`: exact individual field mapping, live pointer
  identity and synchronous byte-copy checks for all choices, null/zero inactive
  vectors, single/multiple/duplicate values, full-width IDs, Unicode composition
  and byte lengths, empty fault string, pre-send validation, exactly one call,
  null successful owner, all five Return values and hostile output diagnostics.
- Real unique native owners are transferred/disarmed for unexpected-error and
  malformed-success cleanup tests. Fabricated native addresses are never used.
  Long/malformed/changing cached decoder outputs use the existing private wait
  seam; these do not claim compiled-provider generation of every hostile output.
- `rust/ams-mel/tests/ir_bit.rs`: public payload submission and immediate input
  drop; independent CommandStatus; no-op equivalence and full command ID;
  pre-send validation with provider send counts; Success/Fail/rejection; exact
  long UTF-8 rejection; send/future exceptions; null/unknown provider returns;
  Enable/Close boundaries; multiple requests; shared admission; pending Close
  and Drop; both parent-close orders; cached polling and ordered provider unload.
- Existing `completion-scale` promise gates and FinalOwner probes deterministically
  synchronize pending work/admission/teardown. No sleep-based ordering is used.
- `bit-ada` permits payloads but does **not** compare their fields. Exact one-choice
  payload fidelity is private FFI boundary evidence plus the unchanged native
  direct-copy code, not independent compiled-provider payload comparison.
- `bit-full` checks all three populated vectors. It remains an unchanged C raw-ABI
  transport test, and is intentionally not used to justify a mixed safe command.
- Response/lifetime scenarios other than `bit-ada`/`bit-full` reject nonempty
  payloads. Therefore compiled-provider error/pending tests use general NoOp;
  they qualify the shared submission/completion path, not each nonempty operation
  under every error. Existing C2 metadata tests independently exercise BIT
  configuration/status and nonquiescing/retained callback teardown regressions.

The initial debug run exposed an incorrect test expectation for an unknown
provider Return: native rejects it as ProviderFailed before Rust result decoding.
That expectation was corrected without changing production contracts. The run
had six integration passes, one real failure and two mutex-poison fallout
failures; the corrected focused run passed all nine. No gate was weakened.

## Qualification

Local evidence directory: `/tmp/rust-ir-011-evidence` (not a release artifact).
Gates run sequentially with no concurrent native build-tree mutation. Rust uses:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

Tool versions: rustc/cargo 1.98.1, system GCC 14.2.0, CMake 3.31.6.
No first-party Ada sources change. For aggregate Ada qualification, Rust's
library/provider overrides are unset and installed GNAT 16.1.0/GPRbuild 26.0.1
are prepended to PATH; Alire provides pinned GNATformat 26.0.0. The complete
provider tree is restored by `make test-native` after aggregate qualification,
before final Rust tests. Commit/tree hashes for this feature belong in delivery
and PR evidence outside the commit's own contents to avoid self-referential hashes.

The first `make check` attempt passed native isolation/557 CTests but stopped
at Alire environment setup: the inherited `AMS_MEL_TEST_PROVIDER_DIR` absolute
path conflicted with the equivalent crate-relative setting. This exit 2 is not
reported as a passed gate. The rerun unsets Rust's overrides as described above.
The new task document's missing final newline was also caught and corrected
before final qualification; no unrelated files or checks were changed.

### Unchanged interface identities

Final local results (same Rust implementation/tests as delivered):

| Command | Actual result |
|---|---|
| `make native` | PASS |
| `make test-native` | PASS, 557/557 |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | PASS, 458 passed, 0 failed, 9 existing ignored subprocess entry points |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets` | PASS |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | PASS |
| `make check` with clean environment/toolchain above | PASS, exit 0; isolation, 557 native tests, pinned formatting, 59 inventory regressions, Ada build/all contract suites, final newlines/whitespace |
| Post-Ada `make test-native` | PASS, complete provider tree restored, 557/557 |
| Final workspace test/check/Clippy/fmt/rustdoc after restoration | All PASS; workspace 458 passed, 0 failed, 9 existing ignored helpers |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib ir_bit::tests` | PASS, 8 passed, 0 failed/ignored |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_bit` | PASS, 9 passed, 0 failed/ignored |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --lib ir_bit::tests` | PASS, 8 passed, 0 failed/ignored |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --release --test ir_bit` | PASS, 9 passed, 0 failed/ignored |
| `make test-policy` | PASS, 244 tests across 8 suites |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files, 3 workflows |
| `python3 scripts/check_ir_api_coverage.py` | PASS, 127 headers, 148 operations |
| Standalone Markdown example, `rustc --edition=2021 --crate-type=lib --emit=metadata -D warnings` | PASS against built safe crate |

The workspace count includes 145 safe unit tests, 32 doctests and raw ABI
compatibility tests. Existing ConfigSet (9), ModeCmd (9), NavigationReport (10),
C2 (14, including BIT no-op) and C2 metadata (6 parent tests plus an ignored
child entry invoked by parents) pass unchanged. No new test is ignored.
The aggregate output contains existing source-free imported C-project GPR
discovery warnings, not compiled-source warning suppressions or task changes.
The independently compiled Markdown example initially failed `-D warnings`
because its unused private function was dead code; making the example function
public corrected that check without suppressing warnings.

Verified byte-identical to the starting main (SHA-256):

| File | SHA-256 |
|---|---|
| `native/include/ams_mel/abi.h` | `66b18d88696d95b984d25dc1c61a7e4a216e866d0e963d2dfc5fe82b08e43ab1` |
| `native/src/exports.map` | `0e56830eccda55d5df8efb6c9a1000a9b952dd34a7db577657a78ea0f7135ce4` |
| `rust/ams-mel-sys/src/lib.rs` | `f08fe5940c2c283471a1969ab3b5eb30426a1fe7dd54eb5658cff3b0f6ae50af` |
| `native/tests/mock_provider.cpp` | `f06f5a77b095713dc4a01a4729e77e05390bbde19ee15230224965d24c5c057d` |
| `native/vendor/ir-mel/include/irmel/library/c2/BIT_Command.h` | `9c86609cf1dced975b7f4c20b939c6df5276f0a057fdf2a6eb8bf1348d90923d` |

## Limitations and remaining minimal-consumer parity

Qualification is mock-only: no real provider, Squall, hardware, credentials or
containers. Provider conformance and C++ ABI compatibility remain required
behind the facade. Allocation-failure handling reuses the existing helper;
actual allocator exhaustion is not forced. Unsafe malformed arbitrary non-null
native addresses cannot be validated by a safe wrapper and are not fabricated
as test owners. No operational/physical effect guarantee is inferred.

The minimal example was inspected at commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`. Its BIT call uses ID 301 and empty
vectors because it advertises no operational BIT identifiers. Rust now supports
that operation and every one-choice payload-bearing BIT action; ModeCmd,
ConfigSet and NavigationReport remain covered. This does not implement the
complete consumer: safe Rust Control discovery, image control-only operation,
general buffer registration/unconfigured layouts and StackedImage remain gaps.
No DSP, private transports or orchestration are added.

Delivery requires independent technical review, open/unmerged PR, no
self-approval and auto-merge disabled. `HOSTED_CI_PENDING` applies until final-HEAD
Bootstrap and Documentation checks complete.
