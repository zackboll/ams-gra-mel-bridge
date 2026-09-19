# Task 028 Instrumentation validation

This records what was actually implemented and verified for the conditionally
required IR MEL Instrumentation family, and -- just as importantly -- what was
not.

## Scope

Implemented, in native C and safe Ada, with raw Rust and private Python ABI
synchronization only:

```text
InstrumentationChannel             @RequiredIfInstrumentation
InstrumentationLevelCmd            @RequiredIfInstrumentation
InstrumentationReport              @RequiredIfInstrumentation
send(InstrumentationLevelCmd)      @RequiredIfInstrumentation
InstrumentationReport callback     @RequiredIfInstrumentation
```

plus `Enable` (upstream `Channel::enable()`) and `ChannelCapability`.

Deliberately NOT implemented, and NOT marked complete for Instrumentation:
Instrumentation-specific copies of the inherited generic `Channel` services --
KeepAlive, CommsTest, the ChannelCommsTest callback, and
`registerBuffer`/`unregisterBuffer`. Those inherited operations should be
generalized across non-C2 channel families rather than cloned into each family.

Track, Scheduling, StackedImage, and RF remain unimplemented. No optional Image
additions, no CameraCommand, no safe Rust Instrumentation API, and no public
Python Instrumentation API were added.

## Upstream basis

```text
IR MEL:     8d9224519f12b44e0b28815755c56a32a28d24a0
Common MEL: f6908437d8fd2f7fb69896f9eb9cfd272d10c439
Squall:     b1015728f904c799fa0c07489fce48e78f67845f
```

The API was read from the pinned upstream headers, not inferred from Squall
stubs:

```text
include/irmel/library/instrumentation/InstrumentationChannel.h
include/irmel/library/instrumentation/InstrumentationLevelCmd.h
include/irmel/library/instrumentation/InstrumentationReport.h
include/irmel/library/irmel-types/CommonIR_MEL.h
include/irmel/library/irmel-types/Channel.h
```

### Vendor closure delta

The three Instrumentation headers were NOT already in the reviewed vendored
closure. A GCC 14.2 dependency probe of `InstrumentationChannel.h` observes 47
headers, 44 of which were already vendored. Exactly three files were added,
byte-identical to the pinned IR MEL revision, and the closure grew from 91 to
94 headers:

```text
native/vendor/ir-mel/include/irmel/library/instrumentation/InstrumentationChannel.h
native/vendor/ir-mel/include/irmel/library/instrumentation/InstrumentationLevelCmd.h
native/vendor/ir-mel/include/irmel/library/instrumentation/InstrumentationReport.h
```

Checksums were added to `upstream-files.sha256.md`. No upstream content was
modified and no build-time network access was introduced.

## Types

Upstream `Priority` is exactly `Normal = 0` and `Debug = 1` and defines no
MaxExclusive sentinel, so the facade invents none. Any value greater than Debug
is rejected on input with `AMS_MEL_INVALID_ARGUMENT` and treated as
`AMS_MEL_PROVIDER_FAILED` when received from a provider.

C shapes:

```c
typedef uint32_t ams_mel_ir_priority_t;
#define AMS_MEL_IR_PRIORITY_NORMAL UINT32_C(0)
#define AMS_MEL_IR_PRIORITY_DEBUG  UINT32_C(1)

typedef struct ams_mel_ir_instrumentation_level_command_v1 {
    uint32_t command_id;
    ams_mel_ir_priority_t priority;
} ams_mel_ir_instrumentation_level_command_v1;

typedef struct ams_mel_ir_instrumentation_report_v1 {
    uint32_t command_id;
    uint32_t size;
    int64_t timestamp_ns;
    ams_mel_ir_priority_t priority;
} ams_mel_ir_instrumentation_report_v1;
```

Ada shapes:

```ada
type Priority is (Normal, Debug);
for Priority use (Normal => 0, Debug => 1);

type Instrumentation_Level_Command is record
   Command_ID : Interfaces.Unsigned_32;
   Priority   : Instrumentation.Priority;
end record;

type Instrumentation_Report is record
   Command_ID   : Interfaces.Unsigned_32;
   Size         : Interfaces.Unsigned_32;
   Timestamp_NS : Long_Long_Integer;
   Priority     : Instrumentation.Priority;
end record;
```

The single canonical report record carries BOTH the
`RequestFor<InstrumentationReport>` completion and the `InstrumentationReport`
metadata callback. No separate future-only or metadata-only report type exists.
The `Instrumentation_Result` rejection type is kept local to this package
rather than making Instrumentation depend on C2 or churning existing C2/Image
APIs.

## Lifecycle and ownership

The facade lifecycle is Attached, Enabled, Failed, Closed. `Open` attaches the
upstream channel; `Capabilities` is valid while Attached or Enabled; metadata
registration may occur while Attached; `Enable` explicitly calls upstream
`Channel::enable()`; Instrumentation-specific submission requires Enabled.

The private channel state retains SessionState, the generic `Channel`, the
`InstrumentationChannel`, metadata callback state, a request count, the
lifecycle value, a `cleanup_started` serialization guard, and an
allocation-free fail-safe retention root. Asynchronous work never retains the
public `ams_mel_ir_instrumentation *`.

Request lifetime follows the proven C2/Image pattern. Before provider `send()`
the adapter allocates the Completion, the WorkerInput, the public request
owner, and the thread holder; accounts the request against channel state; and
copies the shared `InstrumentationChannel` locally. The request is not
published until worker launch succeeds. The lifecycle mutex is NOT held across
`send()`, because a provider is permitted to invoke metadata callbacks
synchronously from inside it.

The worker calls `future.get()` exactly once and caches the terminal result
permanently. Timeout does not cancel, does not consume, and may be retried.
Request `Close` is nonblocking, idempotent, and is not cancellation.

Parent-first lifetime is proven safe in both C and Ada: Open, Enable, Submit,
Wait(0) -> TIMEOUT, close the Session, close the Instrumentation channel, then
Wait(request, finite), Wait(request, 0) again, then close the request.

Close with pending requests prevents new submissions, deactivates public
metadata consumption, does not disable or detach yet, releases the public
channel owner, and defers provider teardown. Final request completion performs
disable, detach, provider channel destruction, callback quiescence, and release
of the provider/session graph. If that final deferred cleanup cannot detach
after the public owner is gone, the complete channel/provider/callback graph is
retained permanently using the same allocation-free emergency-root pattern
proven in C2 and Image. Provider code is never unloaded while provider objects
may remain live.

## Metadata architecture

```text
provider callback
   -> validate/copy complete InstrumentationReport
   -> bounded native queue
   -> safe Ada polling
```

The provider callback never invokes Ada directly. The queue is bounded FIFO
with DROP-INCOMING and saturating `events_received`,
`events_dropped_queue_full`, and `malformed_or_unsupported` counters reusing
the existing `ams_mel_ir_metadata_counters_v1`. A null callback payload is
malformed. No callback exception may cross into provider code.

There is no unregister assumption: upstream defines none. The callback state
belongs to the Instrumentation channel state, not to the public metadata owner.
Metadata `Close` only deactivates public consumption; channel destruction
remains the provider callback-quiescence boundary.

`metadata_open` publishes and retains the callback state first, releases the
lifecycle lock, and only then calls provider registration, so the mock's
synchronous emission from inside `registerMetadataCallback` cannot deadlock.
The mock also optionally emits a report synchronously inside
`send(InstrumentationLevelCmd)`; the tests prove submit does not deadlock, the
metadata event is delivered, and the future result is delivered independently.

## Fidelity

The rich command is `command_id = 0xE1234567`, `priority = Debug`. The mock
provider inspects the upstream getters `getCommandID()` and
`getInstrumentationPriority()` and fails the run if conversion is wrong.

The rich report is `command_id = 0xF1234567`, `size = 0x89ABCDEF`,
`timestamp_ns = -8_765_432_109`, `priority = Debug`. Exact preservation is
verified through the native future, native metadata, safe Ada future, and safe
Ada metadata.

## Failure coverage

Native `native/tests/test_ir_instrumentation.c` (CTest `ir_instrumentation_contract`)
and the Ada AUnit-style suite together cover:

```text
invalid Priority (Debug + 1 and 0xFFFFFFFF)
null arguments across the new surface
wrong ChannelType in configuration
attachChannel null, wrong dynamic type, wrong advertised capability
submit before Enable
submit after Enable failure (Failed lifecycle)

enable failure (Return::Fail)
enable exception

send exception
future exception
successful null shared_ptr result
known MEL rejection with error code mapping
unknown MEL ErrorCode
unknown provider Priority in a successful result
long UTF-8 rejection diagnostic with exact-storage recovery

callback null payload
callback invalid Priority payload
callback allocation failure (no exception crosses into provider code)
queue overflow with FIFO DROP-INCOMING and saturating counters
metadata registration failure and repeat-registration refusal
metadata Close before provider channel destruction
channel Close with callback still possible
```

Long rejection recovery in safe Ada uses the same protocol as C2/Image: a small
fixed diagnostic first, then, when `required > capacity`, a repeated
`Wait(timeout = 0)` with exact storage, verifying status, error code, and
required size are unchanged.

Post-send failpoints are deterministic via
`AMS_MEL_TEST_INSTRUMENTATION_POST_SEND_FAILURE` with values `allocation` and
`worker-launch`. Each case verifies that provider send already happened, submit
returns `AMS_MEL_INTERNAL_ERROR`, no public request escapes, the provider
future is retained safely, the channel and Session may still be closed
publicly, and the provider library is NOT unloaded.

## Inventory

```text
ABI version             0.1
exports.map             72
dynamic exports         72
Rust raw functions      72
Python bound functions  72
native CTest targets    10
```

The thirteen new exports are:

```text
ams_mel_ir_instrumentation_open
ams_mel_ir_instrumentation_enable
ams_mel_ir_instrumentation_get_capabilities
ams_mel_ir_instrumentation_submit_level
ams_mel_ir_instrumentation_request_wait
ams_mel_ir_instrumentation_request_close
ams_mel_ir_instrumentation_metadata_open
ams_mel_ir_instrumentation_metadata_receive
ams_mel_ir_instrumentation_metadata_get_counters
ams_mel_ir_instrumentation_metadata_close
ams_mel_ir_instrumentation_metadata_event_view
ams_mel_ir_instrumentation_metadata_event_close
ams_mel_ir_instrumentation_close
```

Capability support reuses the existing native `snapshot_capability` and the
existing Ada `AMS.MEL.IR.Capability_Conversion`; no ChannelCapability
conversion logic was duplicated.

## Real Squall

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` does NOT support this
channel through the path this bridge uses. Its `SquallControl::attachChannel`
supports only `IRSTImage`, `CommandAndControl`, and `HealthAndStatus` and
returns `nullptr` for `Instrumentation`; its exported
`createInstrumentationChannel` also returns `nullptr`. Squall was not modified.

The opt-in integration clients probe this explicitly and require a clean
negative result -- native `AMS_MEL_FACTORY_FAILED` with the existing
`attachChannel returned null` diagnostic, and Ada `Provider_Error` with the
same message -- then continue the existing run and still require Image,
NavigationReport, C2, and Health to pass on the same Session. An unsupported
provider profile is not converted into a binding failure. Rust and Python
remain regression-only.

Exactly:

```text
Mock provider validates positive Instrumentation behavior and full payload
fidelity.

Pinned Squall validates clean unsupported-provider behavior only.

Pinned Squall does NOT provide positive Instrumentation execution evidence.
```
