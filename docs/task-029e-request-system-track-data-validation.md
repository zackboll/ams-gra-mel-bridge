# Task 029E RequestSystemTrackData validation

## Upstream-direction correction

    Upstream-direction correction:
    RequestSystemTrackData is delivered inbound through
    TrackChannel::registerMetadataCallback; the pinned TrackChannel interface
    defines no send(RequestSystemTrackData) overload.

Task 029E was originally scoped as an outbound `send()` / `RequestFor<T>`
operation with a request handle, `Wait(timeout)`, `CommandStatus` result,
rejection mapping, and completion workers. Inspection of the pinned vendored
headers before any code was written showed that this is not what upstream
declares, so the task was re-scoped to the real contract rather than
implemented against an invented API.

`native/vendor/ir-mel/include/irmel/library/track/TrackChannel.h` declares
exactly two sends, and `RequestSystemTrackData` is not one of them:

```cpp
virtual ams::iface::mel::RequestFor<CommandStatus> send(SystemTrackDataResponse) = 0;
virtual ams::iface::mel::RequestFor<CommandStatus> send(TrackDataUpdate) = 0;
```

`RequestSystemTrackData` appears only as an inbound callback registration
(TrackChannel.h line 63):

```cpp
/// @Optional This function supports callback registration and is optional for
/// IR MEL implementations. IR MFAs that do not support track can still request
/// data to support pointing Algorithms
virtual Return registerMetadataCallback(
    std::function<void(Channel& channel, const RequestSystemTrackData* const)> callback) = 0;
```

A repository-wide search of the pinned snapshot confirms there is no send
overload and no `RequestFor<...>` for this type anywhere:

```console
$ grep -rn "send(RequestSystemTrackData\|RequestFor<.*RequestSystemTrackData" native/vendor/
NO SEND OVERLOAD ANYWHERE IN VENDOR
```

The only other references are an `#include` in `C2Channel.h`, the
`ChannelMetadataCapabilityType::RequestSystemTrackData` enumerator (already
mapped as `AMS_MEL_IR_METADATA_REQUEST_SYSTEM_TRACK_DATA = 22`), and
`Config::supportsSystemTrackData`, whose comment reads "can provide track data
**through** RequestSystemTrackData" -- that is, it is the request delivered to
the application, not something the bridge sends.

Consequently none of the async request machinery applies. This task adds no
request/future handle, no submit/wait/close API, no `Completion`, no
`WorkerInput`, no `finish_request`, no `retain_worker`, no `future.get()`, no
`CommandStatus` result, no `AMS_MEL_COMMAND_REJECTED` mapping, no request
timeout semantics, no per-request completion worker, and no
`TrackState::requests` participation.

## Direction of each Track operation

| Task | Operation | Direction | Architecture |
|---|---|---|---|
| 029C | `TrackDataUpdate` | outbound | `RequestFor<CommandStatus>` request/future |
| 029D | `SystemTrackDataResponse` | outbound | `RequestFor<CommandStatus>` request/future |
| 029E | `RequestSystemTrackData` | **inbound** | metadata callback + bounded queue |

## Upstream type

`native/vendor/ir-mel/include/irmel/library/irmel-types/RequestSystemTrackData.h`
declares a plain value class with only getters and setters, no enum, no
optional field, and no constrained range:

```cpp
class RequestSystemTrackData
{
public:
    [[nodiscard]] std::uint32_t getCommandID() const;
    void setCommandID(std::uint32_t);
    [[nodiscard]] std::chrono::nanoseconds getSystemTime() const;
    void setSystemTime(std::chrono::nanoseconds);
    [[nodiscard]] std::uint32_t getRequestId() const;
    void setRequestId(std::uint32_t);
    [[nodiscard]] std::uint32_t getTrackId() const;
    void setTrackId(std::uint32_t);

    std::chrono::nanoseconds systemTime{0};
    std::uint32_t commandID{0};
    std::uint32_t requestId{0};
    std::uint32_t trackId{0};
};
```

## Field mapping

Only published getters are used; no private member is read directly.

| Upstream getter | Upstream type | C ABI field | C type | Ada field | Ada type |
|---|---|---|---|---|---|
| `getSystemTime()` | `std::chrono::nanoseconds` | `system_time_ns` | `int64_t` | `System_Time_NS` | `Long_Long_Integer` |
| `getCommandID()` | `std::uint32_t` | `command_id` | `uint32_t` | `Command_ID` | `Interfaces.Unsigned_32` |
| `getRequestId()` | `std::uint32_t` | `request_id` | `uint32_t` | `Request_ID` | `Interfaces.Unsigned_32` |
| `getTrackId()` | `std::uint32_t` | `track_id` | `uint32_t` | `Track_ID` | `Interfaces.Unsigned_32` |

`std::chrono::nanoseconds` has a signed representation, so the time is carried
as `int64_t` and `count()` is preserved verbatim. The units are nanoseconds and
no conversion is performed. Signedness and width are preserved for every field,
and no convenience field that upstream does not define was added.

## Validation rules

Upstream declares no enum, no optional field, and no constrained range for this
type, so there is nothing to validate and nothing to reject. Every value the
provider supplies is a well-formed request, including a default-constructed
all-zero request, which is delivered rather than counted as malformed. Only a
null payload pointer is malformed; it is counted in
`malformed_or_unsupported` exactly like a null `IRSTTrackReport`, nothing is
queued, and the subscription stays usable.

## Metadata event and queue design

The event struct gains a second discriminated member and a second kind:

```c
#define AMS_MEL_IR_TRACK_METADATA_IRST_TRACK_REPORT         UINT32_C(1)
#define AMS_MEL_IR_TRACK_METADATA_REQUEST_SYSTEM_TRACK_DATA UINT32_C(2)
```

This follows the existing project convention: kinds are 1-based `uint32_t`
macros, exactly as `AMS_MEL_IR_IMAGE_METADATA_*` numbers its four kinds 1..4.
Only the member selected by `kind` is populated; the others stay zeroed, and
tests assert that explicitly so a consumer cannot read a stale sibling payload.
A consumer must still fail closed on an unrecognized kind, and the Ada
conversion raises `Provider_Error` for any kind this release does not implement.

Both implemented kinds share one bounded queue, one capacity, and one counter
set, and preserve strict FIFO order across kinds. The DROP-INCOMING overflow
policy is unchanged and applies identically to the optional kind: six requests
into a capacity-2 queue retain the first two in arrival order and count four
drops.

## Callback registration behavior

The adapter registers the required `IRSTTrackReport` callback first. If that
fails, the metadata open fails as before. It then registers the `@Optional`
`RequestSystemTrackData` callback. Upstream documents three distinct answers to
that registration, and they are deliberately **not** treated as equivalent:

| `Return` | Upstream meaning | Adapter behavior |
| --- | --- | --- |
| `Success` | Registration succeeded | Optional kind is active |
| `NotSupported` | Optional callback is not implemented | **Non-fatal**; open succeeds |
| `Fail` | A callback is already registered for this datatype on this channel | **Fails closed**, `AMS_MEL_PROVIDER_FAILED` |
| anything else | Undocumented for this call | **Fails closed** |

`NotSupported` is non-fatal because the callback is `@Optional` and pinned
Squall is exactly such a provider: the open still succeeds, the required report
callback keeps working, and only reception of the optional kind is lost.

`Fail` is **not** an optional refusal. Upstream defines it as "a callback is
already registered for this datatype on this channel", meaning another
subscriber owns the datatype and the bridge's closure may never be invoked.
Returning `AMS_MEL_OK` there would promise deliveries the bridge cannot make, so
the open fails closed, no public metadata owner escapes, and the one-shot
registration rule stays established. The already-registered `@RequiredIfTrack`
callback and its retained state remain live until Track teardown because
upstream provides no unregister operation. Any other value -- `BadPointer`,
`NotImplemented`, or one added by a future upstream revision -- fails closed for
the same reason rather than silently claiming success.

A registration that throws preserves the existing provider-exception behavior
and reports `AMS_MEL_PROVIDER_EXCEPTION`.

Both registrations happen without `TrackState::mutex`
held, because the provider may deliver synchronously from inside
`registerMetadataCallback`.

## Lifetime and quiescence

The existing Track metadata ownership model is unchanged. Registration remains
one-shot per Track channel because upstream declares no unregister operation.
`MetadataState` is owned by the Track channel and retained by the callback
closures; metadata close only deactivates public consumption. An event already
acquired by the caller is owned independently of the public subscription and
stays valid after that subscription is closed. Provider channel destruction
remains the callback-quiescence boundary, and the mock joins its asynchronous
metadata producer thread before the channel is destroyed.

## Cross-mechanism lifetime test

`test_mixed_request_accounting` in `native/tests/test_ir_track_system_response.c`
now exercises all four surfaces at once: a pending `TrackDataUpdate`
`RequestFor`, a pending `SystemTrackDataResponse` `RequestFor`, an inbound
`RequestSystemTrackData` metadata event, and an inbound `IRSTTrackReport`
metadata event. It proves that draining both metadata events and closing the
metadata subscription do **not** change async request accounting, that releasing
only one future still tears nothing down, and that disable, detach, channel
destruction, and library unload happen only after the second future is
released. A mutation that mislabels the request kind is detected by this test.

## Scope

`CandidateObjectMessage` and `CandidateObjectPreProcMessage` remain
unimplemented; the mock still reports them unsupported and records any call so
tests prove they were never exercised. The Track API as a whole is **not**
complete. No safe Rust or public high-level Python Track API was added. The
vendor delta is zero.

## Real Squall

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` hard-returns
`nullptr` from `createTrackChannel`, so no Track channel can be attached at all
and positive `RequestSystemTrackData` evidence from real Squall is impossible.
That is an upstream capability gap, not a bridge failure. Real-Squall
validation continues to cover the clean expected-negative Track path, and the
mock provider is the positive evidence for this callback.

    Track: pinned Squall unsupported as expected (AMS_MEL_FACTORY_FAILED: attachChannel returned null)
    Track: pinned Squall unsupported as expected (Provider_Error: attachChannel returned null)

## Resulting Track status

```text
@RequiredIfTrack core                     complete
@RequiredIfTrackUpdate TrackDataUpdate    complete
SystemTrackDataResponse                   complete
RequestSystemTrackData                    complete
CandidateObjectMessage                    unimplemented
CandidateObjectPreProcMessage             unimplemented
Track API overall                         incomplete
```
