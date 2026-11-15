# RUST-IR-017 — safe common communications metadata reception

## Source and scope

Started directly from freshly fetched `origin/main` commit
`cb33c5e5434fa27b9d9660f6a9d361ed0993e37b`, tree
`0f6851c7e87ed002491fd61c2c43bd2683243ca1`, in the isolated worktree
`/home/zboll/git/ams-rust-ir-017`, branch
`feature/rust-ir-017-common-comms-metadata`. Preflight verified merged PRs
#202, #208, #214 and #216, closed-unmerged #204, and already-merged #217.
Draft #215 was the only open PR. Its Common-crate extraction was inspected but
not incorporated. Existing Error, CommandReturn, UciId and CommsTestReport
identities remain unchanged. Final commit/tree and hosted synthetic-merge
identities are recorded in the delivery PR, outside their own hashed source.

Coverage delta: safe Rust can now register/receive the required inherited
ChannelCommsTestRep callback for HealthAndStatus, IRSTImage (including
control-only Image), and StackedImage. Existing C2 metadata remains authoritative
and byte-unchanged. No native/sys, Ada/Python, provider fixture, acquisition,
workflow, historical qualification record or four-channel consumer change.
The structured IR inventory classifies native/Ada coverage, which is unchanged;
this Rust-only addition does not change its reviewed classifications or hashes.

## Public API and canonical native mapping

The crate exports `CommsMetadataStream`, `CommsMetadataCounters` and
`CommsMetadataOpenError`. The existing `ChannelView` gains:

```rust,ignore
pub fn open_comms_metadata(&mut self, queue_capacity: usize)
    -> Result<CommsMetadataStream, CommsMetadataOpenError>;
```

The stream exposes `is_open(&self)`, `receive(&mut self, timeout_ms: u32)`,
`counters(&mut self)` and `close(&mut self)`. It is not Clone, Send or Sync.
Mutable receivers serialize public consumption/Close without another lock.
Its Debug representation reports only local open state, not native addresses.

The existing seven-argument native Open is called exactly once:

1. channel (existing live weak View);
2. size_t queue capacity;
3. output owner, initially null;
4. **mandatory provider registration Return**, initially unpublished `u32::MAX`;
5. diagnostic buffer;
6. diagnostic capacity;
7. diagnostic required length.

No six-argument overload or new raw declaration exists. Receive writes the
existing scalar report directly; counters writes the existing three-u64 record;
Close consumes the existing owner. There is no event allocation/view API.

Locally closed Views and zero capacity fail before native entry. Open requires
native success, a recognized provider `CommandReturn::Success`, and a non-null
owner. `CommsMetadataOpenError::error()` preserves the ordinary bridge/safe-layer
Error and diagnostics; `registration()` separately returns the actual recognized
provider Return, if published. Neither an exception before Return nor an unknown
value is guessed from diagnostic text. Malformed successful outputs are protocol
errors. Existing diagnostic decoding distinguishes invalid UTF-8, invalid output
protocol and native status, including unknown native statuses. An invalid
diagnostic takes the existing helper's diagnostic-error precedence; no Open is
retried to obtain a better diagnostic or another provider result.

## Eligibility, one-shot registration and report model

Health/Stacked admit Attached or Enabled; Image admits Attached or Running,
including control-only attachments with zero frame capabilities. Registration
does not require geometry, a buffer factory or configured acquisition. Native
registration is one-shot on the underlying typed state: duplicate Opens fail
through the original or another weak View, including after provider refusal,
partial installation or exception. Preparation failure before provider entry is
distinct; the capacity-overflow test verifies that it need not consume the shot.

Common Open on C2 preserves native ProviderFailed rejection. Use the unchanged
`ControlChannel::open_metadata` and
`C2MetadataStream::enable_comms_test_events`; no second C2 queue/registration
state is introduced. Instrumentation/Track/Scheduling remain outside this slice.

The **existing** owned `CommsTestReport` has exactly `command_id: u32` and
`request_id: u32`. All bits are copied, including high-bit and maximum values.
No channel ID, timestamp, sensor telemetry or success meaning is invented.
Channel association comes from the receiver chosen by the application.

The native preallocated bounded FIFO uses **DROP-INCOMING**, preserving earlier
accepted entries. A provider callback, not a request future, produces metadata.
Zero timeout polls; positive timeout bounds only that wait and cancels nothing.
Active-empty returns Timeout. Accepted values drain after typed closure before
StreamStopped, or ProviderFailed after failed native cleanup. The failed-partial
Receive/counter tests never publish partially written outputs as Rust values.

Counters copy all three raw u64 values unchanged, including `u64::MAX` saturation,
without resetting. `events_received` counts callback invocations, including null
reports, inactive delivery and full-queue drops, **not successful Rust receives**.
`events_dropped_queue_full` counts dropped incoming valid reports;
`malformed_or_unsupported` includes null callback payloads.

## Ownership, reentry and cleanup

The private unique guard owns only native consumption; no Rust reference to the
View, typed channel or Session is held. Closing a weak View does not close the
receiver or typed owner. Closing the receiver does not disable/detach the typed
channel, unregister the callback or cancel pending requests. Native callback
retention is authoritative for provider lifetime. Queued scalar copies remain
usable even after actual provider unload.

Native registration executes outside lifecycle/queue locks under its temporary
family claim. Tests exercise provider synchronous reentry closing the typed
owner during Open, callback entry held during receiver Close, and callback/typed
Close concurrency using existing native hooks and deterministic gates. Public
safe Rust owners remain !Send/!Sync; private concurrency controls borrow real
native handles only until joined, without exposing that facility publicly.
The receive-entry notification control proves the Close/wait boundary (the
scheduler may perform Close just before native wait entry); native C11 gated
callback/teardown regressions provide the stronger held-callback evidence.
No sleep-based race or additional production callback/thread is introduced.

Any owner unexpectedly published on failed Open receives exactly one
best-effort native Close; a secondary Close failure never replaces the primary
Open error. Hostile-output tests use live Box tokens and seam-specific Close,
never fabricated addresses sent to actual native cleanup.

Explicit Close moves the owner out before entering native code. Success is
idempotent. Error, malformed diagnostics or contradictory uncleared output
cannot cause Drop to release a potentially consumed handle again. Rust becomes
locally closed after the one release attempt; uncertain native retention is
preferable to a double release. Drop makes one non-panicking, allocation-free
best-effort call if no explicit release was attempted. Close is not evidence of
callback quiescence or immediate provider unload. Upstream has no unregister;
uncertain native provider teardown can retain graphs indefinitely.

## Compilable safe Health example

The same example is compiled by Rustdoc on `ChannelView::open_comms_metadata`.
The supplied Session/configuration select the provider and identities. Another
application/provider action must actually produce a callback; enabling Health
does not fabricate communications metadata.

```rust
use ams_mel::{CommsTestReport, HealthConfig, Session};

fn receive_health(
    session: &Session,
    config: &HealthConfig,
) -> Result<CommsTestReport, Box<dyn std::error::Error>> {
    let mut health = session.open_health_channel(config)?;
    let mut view = health.channel_view()?;
    let mut metadata = view.open_comms_metadata(16)?;
    health.enable()?;
    let report = metadata.receive(1000)?;
    let counters = metadata.counters()?;
    assert!(counters.events_received >= 1);
    metadata.close()?;
    view.close()?;
    health.close()?;
    Ok(report)
}
```

## Qualification

Local transcripts are retained in `/tmp/rust-ir-017-evidence`; final-source
results and delivery identity are also recorded in the PR. Native build
directories are never concurrently mutated: aggregate isolation owns the normal
trees, while extra Release qualification uses `/tmp/rust-ir-017-native-release`.
Aggregate runs without Cargo/native/loader overrides, with installed pinned
GNAT 16.1.0 and GPRbuild 26.0.1 package on PATH. The complete provider tree is
restored before final Rust regressions after aggregate isolation.

Initial implementation qualification:

| Command | Actual result |
|---|---|
| `make native` | Passed production build. |
| `make test-native` | Passed 744/744 native tests. |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | Passed 601 tests, 15 ignored before the final Debug-redaction test; final count recorded below/PR. |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets` | Passed. |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | Passed after correcting new-test iterator/borrow warnings, without suppression. |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Passed. |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | Passed. |
| Focused Debug common integration | 18 passed, one subprocess entry ignored; 69 child scenarios. |
| Focused Release common integration | 18 passed, one subprocess entry ignored; 69 child scenarios. |
| Private seam/native Debug and Release controls | Passed; hostile status/registration/owner/diagnostic matrices and seven isolated native controls. |
| Release repetition | 25 full integration runs and 25 private native-control runs passed: 1,725 public child scenarios and 175 private child controls. |
| Release native common/C2 metadata CTest | 85/85 passed. |
| Release gated CTest `^comms_metadata_.*(async\|reentry\|held)` with `--repeat until-fail:25` | 20 cases × 25 = 500 executions passed. |
| Strict C11 TU compile with `-pedantic-errors -Wall -Wextra -Werror` | Passed unchanged native common-metadata test source. |
| `python3 scripts/check_doc_source_integrity.py` | Passed: 825 vendor files, four workflows. |
| `make test-policy` | Passed. |

Focused tests preserve the original providers rather than modifying fixtures.
All environment variables, retained registration graphs and actual unload
observations are process-isolated. Existing completion probes wait for request
worker reclamation rather than assuming future readiness proves teardown.

### Actual reference provider, separately qualified

Unmodified `minimal-mel-example` commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`, clean checkout
`/home/zboll/git/ams-rust-ir-014-reference`. Isolated Debug CMake build at
`/tmp/rust-ir-017-reference-build` passed all 4 provider CTests, including the
C++ consumer. DSO SHA-256:
`93dce085b0da9e9fba39430cb016b51da5c17d2ec3e29a20751b0c1f577f3319`.

The opt-in `ir_common_comms_reference` uses **only public safe Rust**, the
production facade, and that actual C++ DSO. It checks vendor
`minimal-mel-example`, API 4/revision 1, discovers and attaches all four channel
identities, registers dedicated C2 plus three common receivers, enables all four,
checks direct/C2-routed future correlations and independently dequeues callbacks:

| Channel | Futures | Actual metadata events |
|---|---:|---:|
| C2 | 2 | 2 |
| Health/Status | 2 | 2 |
| Control-only Image | 2 | 2 |
| StackedImage | 2 | 2 |
| **Total** | **8** | **8** |

Every receiver reports `(received, dropped, malformed) = (2, 0, 0)`. Numeric
ChannelType routing addresses are specific to this reference stub, not a generic
identity conversion. All requests, Views, typed channels, receivers and Session
are explicitly closed. Reference transcripts retain every checked future and
received callback. Debug passed; final Debug/Release and loader verification are
recorded with the delivery qualification. Development-only compile failures
(an incorrect nonexistent typed-C2 method and an omitted `/lib` in a link path)
were corrected; neither was represented as an interoperability result.

## Limitations and delivery disposition

This is reference-stub interoperability, not actual sensor telemetry, acquisition,
hardware qualification, provider-independent numeric routing or complete required
interface parity. General buffers, Image/Stacked acquisition, safe Rust standalone
Disable, other missing callbacks and the next consumer-only eight-callback
increment remain separate work. No semantic uncertainty was resolved by guessing.

Disposition remains **HOSTED_CI_PENDING** until final-head Bootstrap and
Documentation and the corresponding synthetic-merge source qualification are
verified. Independent technical review is requested; no self-approval, auto-merge
or merge is authorized.
