# RUST-IR-022 — Safe Rust Scheduling channel foundation

## Scope and source

This is **partial Rust Scheduling coverage**: a typed channel owner and its
inherited capability, KeepAlive and direct CommsTest operations. It does not
expose task `exec`/`send`, task queues, task payloads, metadata registration,
buffer registration, or standalone Scheduling Disable. Provider-advertised
metadata is data, not a registration or a promise of Rust support. Attachment
and Enable are not evidence of actual task execution or hardware operation.

The starting `main` is `0ce58e6fb60139d5454db4279fe7c4154cfd6ac4` (tree
`d3eb1be096edab3b349b569e65d018da93247beb`), the merge of PR #253. The
implementation uses the existing native contract described in
[IRMEL-ADA-011](irmel-ada-011-scheduling-foundation.md) and follows the
[safe Stacked ownership model](rust-ir-015-stacked-foundation.md). No C ABI,
raw Rust declarations, provider or mock implementation, vendor, Ada, Python,
CI workflow, or authoritative Ada coverage inventory changes. The latter
describes the native/Ada binding, not Rust parity.

## Public Rust API

`ams-mel-ir` and `ams-mel` export the same concrete `SchedulingConfig` and
`SchedulingChannel` types. Construction through
`SchedulingConfig::new(channel_id, platform_id, sensor_location)` owns both
`UciId`s and the `ComponentLocation`. The existing identity/location
constructors reject embedded NUL; empty text is valid. A synchronous
`Session::open_scheduling_channel(&config)` passes both UUIDs and labels and
all three offsets, key and system name to `AmsMelIrSchedulingConfigV1` with
channel type **3** (`AMS_MEL_IR_CHANNEL_SCHEDULING`). It does not coerce values,
infer geometry, or hold a reference to the input after Open.

| API | Contract |
| --- | --- |
| `Session::open_scheduling_channel` | Returns **Attached**, not Enabled. Native checks advertisement, attaches once, and verifies the concrete channel and its advertised type. Missing advertisement is nonfatal `InitializationFailed`. An incompatible attachment is rolled back; uncertain rollback can retain the native graph without publishing an owner. |
| `SchedulingChannel::is_open` | Reports whether the Rust owner still holds a handle, not whether the channel is operational. A handle retained for detach retry still returns `true`. |
| `SchedulingChannel::enable` | Explicit and idempotent after success. Provider refusal (`ProviderFailed`) and exception (`ProviderException`) retain the native diagnostic and mark the lifecycle Failed. There is no second provider Enable attempt. |
| `SchedulingChannel::capabilities` | Returns the common owned `ChannelCapability` while Attached or Enabled. Ordered collections, reserved and unknown tags, nested fields and full-width `task_schedule_depth` are copied using the shared converter. Depth `0` is a valid application-managed priority mode, not absence; `u32::MAX` is not narrowed. The snapshot survives closure and provider unload. |
| `SchedulingChannel::channel_view` | Returns the existing weak `ChannelView`. Closing a view does not close the channel, and holding a view does not retain the channel or provider. |
| `SchedulingChannel::close` | Stops admission and delegates teardown to native code. Disable is attempted at most once and only if Enable was attempted; detach is attempted regardless of a disable error. A consumed owner makes subsequent Close a no-op. |

The channel is neither `Clone`, `Send`, nor `Sync`. No raw handle is published
or printed in its `Debug` representation. The native graph outlives the public
Session when a Scheduling owner or an admitted request still holds a claim.
KeepAlive and direct CommsTest reuse common admission and future ownership.
Request timeout, Close and Drop do not cancel execution. Scheduling Close
can consume the public owner while native cleanup is deferred to the final
admitted request. A deferred cleanup error can be reported by completion;
uncertain deferred detach can retain the graph without a public retry handle.

### Native output and cleanup rules

Open is never replayed to recover diagnostics. Null-success Open is a protocol
error. If failed Open or malformed Open diagnostics accompany a non-null owner
output, the wrapper never publishes it and attempts cleanup once without
diagnostics. Shared capability/view output validation remains unchanged.

Explicit Close disarms the wrapper before calling native Close. Only
`ProviderFailed` with the original non-null pointer and a validated diagnostic
record restores the handle for retry. A disable failure after successful
detach still reports `ProviderFailed` but consumes the owner. A different
non-null pointer, a non-null success output, or missing, invalid or uncaptured
diagnostics do not authorize a repeat Close. The wrapper can conservatively
abandon uncertain ownership but will not manually free it, report contradictory
success, or close it again. Drop makes at most one best-effort Close call with
no diagnostic buffer, allocation or retry in Rust. Use explicit Close when
the cleanup result matters; it is not proof of immediate physical teardown.

## Safe executable example

The [example](../rust/ams-mel-ir/examples/scheduling_foundation.rs) selects an
advertised Scheduling capability, uses its IDs/location to attach, queries
the Attached snapshot through a weak view, then explicitly enables the owner.
It closes the public Session before the channel and issues no task operation.
Run against the existing compiled mock from the repository root:

```sh
make test-native
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
export LD_LIBRARY_PATH="$AMS_MEL_NATIVE_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cargo run --manifest-path rust/Cargo.toml -p ams-mel-ir \
  --example scheduling_foundation -- \
  "$AMS_MEL_TEST_PROVIDER_DIR/libmock_ir_provider.so" sched-basic
```

## Evidence and limits

The compiled-mock tests use the existing `sched-*` scenarios. `sched-config`
checks every input field at native attachment, and the Rust tests compare
the entire rich capability value, including after DSO unload. Subprocesses
isolate log state and deliberately retained graphs. Test-only completion gates
allow requests to finish in a controlled order without sleeps. Lifetime events
check exact cleanup counts and order, and mock counters and event checks verify
that no Scheduling-specific task command or metadata call ran. Unknown enum
values and malformed FFI outputs are injected privately, without modifying
the mock or passing fake owner pointers to native code.

Focused commands (with the native library and test provider paths set as above):

```sh
cargo test --manifest-path rust/Cargo.toml -p ams-mel-ir --lib ir_scheduling
cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_scheduling
cargo test --manifest-path rust/Cargo.toml -p ams-mel-ir --doc SchedulingChannel
ctest --test-dir native/build-tests -R '^ir_scheduling_(foundation_|layout$)' --output-on-failure
```

Local development qualification recorded **4** private tests, **16** compiled
mock scenarios (one ignored child entry is invoked by the parent tests), and
**3** compile-fail doctests passing. The example against GCC Release/`sched-basic`
printed `Enabled channel-α; attached TaskScheduleDepth=23`. Compiled C11
Scheduling foundation/layout tests passed **20/20** in separately configured
GCC Release, Clang Debug and Clang Release trees. The coverage audit passed
**84** regression tests; `make test-policy` passed **333** tests, and the source
integrity check verified **825** vendored files and **7** workflows. The delivery
PR records final-source identity, full gate commands/results and hosted status.
These results qualify the compiled mock only, not Squall, a real provider,
sensor hardware, or full IR MEL parity.
