# RUST-IR-005 — safe Rust IR C2 metadata reception

## Source and scope

Dedicated worktree: `/home/zboll/git/ams-rust-ir-005`, branch
`feature/rust-ir-005-c2-metadata`. Freshly fetched starting main:
`e746c49fd3ffe57b37064878cdf8225a3745a35e`, tree
`7b022169714075949ff0604ad79d94238cf532cd`. The task's earlier observed
`8df77cd1b8263ab2328fc2e7ec8b4db5a4673583` is an ancestor. PR #159 is merged
at `90ddf07267d98b50bce93bbaec7f42a5b43ae799` and is also an ancestor.
Preflight found no competing Rust C2 metadata branch or PR; the source worktree
and newly created dedicated worktree were clean.

This is an additive safe Rust consumer of the existing C facade, not a new
provider implementation. Native C/C++, headers, exports, mocks, raw bindings,
Ada, Python, build scripts, CI and policy are unchanged. Existing Health and
Image metadata APIs and tests remain intact. The only Health source change is
`pub(crate)` visibility for the complete existing BIT status converter.

ABI remains **0.1**, with exactly **284 `AMS_MEL_0.1` production exports**.
The implementation invokes only these seven existing C2 metadata exports:

```text
ams_mel_ir_c2_metadata_open
ams_mel_ir_c2_metadata_register_comms_test
ams_mel_ir_c2_metadata_receive
ams_mel_ir_c2_metadata_get_counters
ams_mel_ir_c2_metadata_close
ams_mel_ir_c2_metadata_event_view
ams_mel_ir_c2_metadata_event_close
```

## Public API and complete mappings

`ControlChannel::open_metadata(&mut self, queue_capacity: usize)` returns
`Result<C2MetadataStream, Error>`. The uniquely owned stream provides:

- `is_open(&self) -> bool` (local handle presence, not provider liveness);
- `enable_comms_test_events(&mut self) -> Result<(), Error>`;
- `receive(&mut self, timeout_ms: u32) -> Result<C2MetadataEvent, Error>`;
- `counters(&mut self) -> Result<C2MetadataCounters, Error>`;
- `close(&mut self) -> Result<(), Error>` and best-effort non-panicking Drop.

All new types are re-exported from the crate root:
`C2MetadataStream`, `C2MetadataCounters`, `C2MetadataEvent`, `CommandStatus`,
`CommandState`, `CannotComply`, `BitConfiguration`, `BitTypeDefinition`, and
`BitControlInterface`. The three fixed-domain enums expose `ALL`, `as_raw()`
and `From<Enum> for u32`, matching existing metadata conventions.

| Native kind / Rust variant | Complete owned data |
| --- | --- |
| 1 / `CommandStatus` | Full `u32 command_id`, exact `CommandState` 0..4, exact `CannotComply reason_id` 0..46, owned `reason_description` |
| 2 / `BitConfiguration` | Ordered `bit_types: Vec<BitTypeDefinition>`, each with complete `UciId bit_id`, exact `accepted_interface` 0..3, ordered `bit_item_names`, ordered `subsystem_component_ids`, signed `i64 expected_duration_ns` |
| 3 / `BitStatus` | Canonical existing Health `BitStatus`: all active BIT identities/times/fractions, completed BIT identities/timestamps/outcomes/descriptions/items, and complete faults/data/components/ambiguity groups/diagnostic IDs |
| 4 / `CommsTest` | Canonical existing `CommsTestReport`: independent full `u32 command_id` and `u32 request_id` |

All five command states are represented: NotSet, Received, Accepted, Rejected,
Cancelled. Every published CannotComply value has its own named variant; raw
47 and above are errors, never silently Other, Unknown or NotSet. Accepted is
not proof of physical completion. No request/result correlation is inferred.

BIT interfaces retain the pinned associations: NotSet=0,
SubsystemBitCommand=1, SubsystemStateCommand=2, SubsystemInitiated=3. Upstream
names and adjacent descriptions for 2/3 contradict each other; this binding
does not resolve that inconsistency by swapping names or inventing semantics.
UCI BIT identities are not inferred to be numeric command selectors.

Empty collections/text, non-ASCII strings, duplicates and provider ordering are
preserved. Signed nanoseconds are not epoch-converted or normalized. Completion
fractions and IEEE values are copied without clamping. No competing BIT/fault
representation is introduced; the whole Health converter is reused unchanged.
Communications metadata is not `CommsRequest::wait()` completion.

## Ownership, lifetime and registration

Open is valid while C2 is Attached or Enabled and does not enable C2. Zero
capacity is rejected locally without consuming registration. An actual native
Open attempt is one-shot, even on partial failure. Native code initializes its
queue before registering BIT_Configuration, CommandStatus and BIT_Status in
that order; synchronous callback delivery is permitted. Failed Open returns no
partial Rust owner. Native callback-accessible state remains retained according
to the existing channel contract, not freed as registration rollback.

Communications registration is a separate one-shot native operation, including
unsuccessful attempts. It does not send a test, retry, or re-register the three
base callbacks. A repeated attempt preserves native InvalidArgument.

The stream has no Rust borrow or retained Rust parent/Session. The native weak
channel link and retained queue/callback state remain authoritative. Access to
native owners must remain externally serialized; the stream is neither Send
nor Sync, verified by two compile-fail doctests. There are no added production
threads, callback trampolines, mutexes or queues.

Receive performs one native call. Zero timeout polls. The bounded FIFO retains
DROP-INCOMING semantics and drains queued events before stopped/failed status.
Counters preserve all three native saturating `u64` fields independently:
`events_received`, `events_dropped_queue_full`, `malformed_or_unsupported`.
Callback allocation exceptions fail this native stream rather than being
reclassified as malformed events.

Close is idempotent; it does not close C2, unregister callbacks or cancel
requests. Close consumes the local release attempt, even if native status or
diagnostic/output is uncertain; Drop never retries a possibly released handle.
Drop uses null diagnostics and performs no allocating diagnostic conversion.
The native channel destructor, not Disable, is the callback-quiescence boundary.
Owned returned graphs remain valid after stream, C2, Session and provider unload.

## Defensive conversion and errors

A private RAII event owner guards even an unexpected Receive output on failure.
Only the active payload is converted. Successful Receive without an event,
successful View without a view, unknown discriminants/enums, null nonempty
spans, misaligned/impossible spans and embedded NUL are
`ErrorKind::ProtocolInconsistency`. Invalid UTF-8 is `InvalidUtf8`; fallible
nested vector/string reservations return `InternalError`. Native Timeout,
StreamStopped, ProviderFailed and ProviderException remain distinct. Closed
Rust owner operations return ProviderFailed without entering native code.

Each nested span/string/ID uses existing checked helpers. Native storage is the
trusted provenance boundary: structural checks cannot prove that an arbitrary
otherwise plausible C address is mapped, initialized or live. No pointer or
slice into native storage escapes. Any conversion error or unwind drops the
guard; explicit Event Close failure returns an error instead of the copied
event. No partial event escapes. Unknown data is rejected, never normalized.

## Tests and qualification

The new integration executable uses the existing compiled mock and child-process
isolation for process-global failpoints/barriers/logs. It has no private native
symbol dependency. Six parent tests execute 18 child cases; the ignored child
entry is explicitly executed by the parents, not an untested skipped scenario.
Fixtures spell out expected public values independently of the converter.

Scenarios cover metadata-rich, metadata-command-states, metadata-overflow,
metadata-malformed, metadata-register-fail, metadata-nonquiescing-disable,
comms-high, comms-register-retain-fail, late callbacks after consumer close,
callback allocation failure, Attached/Enabled registration, zero capacity,
parent/Session/stream-first teardown and implicit Drop. The existing deterministic
callback barrier establishes entry, nonquiescing Disable, callback return,
channel destruction and provider unload order without Rust sleeps.

Hostile C-boundary unit fixtures cover active-only dispatch, all 5 × 47 command
state/reason combinations, all four BIT interfaces, signed duration extremes,
duplicates/empty/non-ASCII text, strict nested enums/IDs/spans, floating-point
specials, native View/Close failures, malformed diagnostics, null successes,
panic cleanup and exact-once event/stream cleanup. A test-only thread-local
System allocator wrapper injects each of six nested configuration reservation
failures, verifies InternalError and one event release, and is absent from
production builds. Existing Health hostile-graph tests also run unchanged.

Qualification logs and the mutation driver are local artifacts under
`/tmp/rust-ir-005-evidence`; final source identities and hosted results belong
in the PR and final handoff rather than recursively in this document.

### Local command evidence

Commands run from the dedicated worktree. Rust contract tests set
`AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"` and
`AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"`.

| Command | Actual result |
| --- | --- |
| `make native` | Production facade built; 284 versioned exports |
| `make test-native` | Baseline and full post-Ada clean rebuild: 472/472 passed, zero failed/Not Run |
| `make test-build-isolation` | Both build orders and production/test separation passed |
| `ctest --test-dir native/build-tests -R '^ir_c2_contract$' --repeat until-fail:50 --output-on-failure` | 50/50 passed after Ada and final native rebuild |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_c2_metadata` | Six parent tests passed, child entry ignored only in outer harness; 18 child cases executed |
| Focused integration and `--lib ir_c2_metadata` with `--test-threads=1`, `4`, `16` | Six integration / 15 unit tests passed at each setting |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | 304 passed including doctests; zero failures; seven existing/new outer child entries ignored |
| `cargo check --manifest-path rust/Cargo.toml --workspace` | Passed with production default and contract-facade environment |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | Passed with production default and contract-facade environment |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Passed |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | Passed; two non-Send/non-Sync compile-fail doctests also executed by workspace tests |
| `make test-python` | 354 Python tests passed; package compileall passed |
| `make test-policy` | 234 tests across seven suites passed |
| `make test-ada` | Direct GPR smoke passed using installed GNAT 15.3.1 / GPRbuild 26.0.1; 46 IR coverage regression tests and existing smoke repeats passed |
| `python3 scripts/check_doc_source_integrity.py` | Passed |
| `python3 scripts/check_final_newlines.py` | Passed |
| `git diff --check` | Passed |

Ada used a command-local PATH for the installed toolchains; no global settings
changed and no Ada/Alire build overlapped native or Rust testing. A complete
native test clean/rebuild and CTest execution followed Ada. This is direct GPR
smoke evidence, not a separately claimed Alire package/index qualification.

Independent production linkage used
`AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib"` and a separate
`CARGO_TARGET_DIR="$PWD/rust/target/ir005-production"` to run the integration
executable with only `native_callback_allocation_failure` and
`callback_completion_precedes_provider_unload` excluded (these require native
test-only injection/barriers). Four parent tests passed. `ldd` resolved the
production facade; `nm -u` showed no `ams_mel_test_*` dependency. The native
header, raw Rust declarations and export map hashes match starting main, and
the final 284-symbol production inventory matches the captured baseline exactly.

A separate read-only technical reviewer process was attempted but could not
authenticate: its OAuth session had expired and could not be refreshed. That is
**not a completed independent review**. Independent review remains a delivery
requirement; hosted statuses and mergeability must be checked on the final PR
head and are not asserted by these local results.

### Compiled mutation campaign

**Applied 10; compiled 10; executed 10; killed 10; surviving 0.** Each mutant was
applied separately, compiled with `cargo test --no-run`, then tested; original
source was restored in `finally`. Nine run the C2 unit suite; the communications
ID mutant runs the integration suite. Compilation failures are not counted as kills.

| Mutation | Executed regression gate |
| --- | --- |
| Disable CommandStatus dispatch | active-only dispatch and command domain tests |
| Ignore nested BIT item-name span | complete nested configuration fixture |
| Ignore nested component-ID span | complete nested configuration fixture |
| Replace accepted interface with NotSet | all four interface mappings |
| Replace CannotComply with NotSet | all 47 reason mappings |
| Replace command state with NotSet | all five state mappings |
| Skip explicit event release and forget handle | exact-once event release test |
| Skip event Drop release | conversion-error/unwind cleanup test |
| Skip stream Drop release | stream Drop exact-once test |
| Copy communications request ID into command ID | comms-high and four-family integration cases |

This is focused fault-detection evidence, not exhaustive mutation or sanitizer
qualification. No real provider, credentials, transport, hardware or physical
operation semantics are qualified; all behavioral evidence is mock-only.
