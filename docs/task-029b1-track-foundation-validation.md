# Task 029B1 validation: IR Track channel foundation

## Scope

This task adds only the Track channel ownership/lifecycle foundation:

```text
Track Open
Track Enable
Track ChannelCapability
Track Close
```

plus a safe Ada Track channel owner, raw Rust ABI synchronization, private
Python ABI synchronization, a concrete mock `TrackChannel`, and native
lifecycle contract tests.

No Track metadata and no Track commands were added. The following are
explicitly **not implemented**:

```text
IRSTTrackReport callback
Track metadata queue
TrackDataUpdate
SystemTrackDataResponse
RequestSystemTrackData
CandidateObjectMessage
CandidateObjectPreProcMessage
real Squall Track validation
Scheduling
StackedImage
RF
```

Track is **not** complete.

## Upstream pin

Track declarations come from pinned IR MEL
`8d9224519f12b44e0b28815755c56a32a28d24a0`, header
`include/irmel/library/track/TrackChannel.h`. The channel type is
`ChannelType::IRSTTrack = 0`, reusing the existing
`AMS_MEL_IR_CHANNEL_IRST_TRACK` constant.

## Task 029A preservation

The pinned Boost closure from Task 029A remains authoritative and unchanged:
Boost 1.83.0, 483 measured headers (GCC 481, Clang 482, union 483),
`check_track_header_boost_closure`, and `track_header_compile_probe`.

Vendor delta: **0**. No Boost or upstream files were fetched, added, modified,
or removed, and no provenance/checksum document was edited.

The mock provider now derives from `TrackChannel`, so it was given the vendored
Boost include root as a SYSTEM PRIVATE include:

```cmake
target_include_directories(mock_ir_provider SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/boost-1.83.0)
```

No system Boost is used. The closure checker remains green:

```text
TrackChannel Boost closure: 481 headers under
  /home/zboll/git/ams-mel/native/vendor/boost-1.83.0
```

## C surface

Added configuration:

```c
typedef struct ams_mel_ir_track_config_v1 {
    ams_mel_uci_id_v1 channel_id;
    ams_mel_ir_channel_type_t channel_type;
    ams_mel_uci_id_v1 platform_id;
    ams_mel_component_location_v1 sensor_location;
} ams_mel_ir_track_config_v1;
```

`channel_type` must be `AMS_MEL_IR_CHANNEL_IRST_TRACK`. The same UTF-8 and
copied-string rules as Health and Instrumentation apply.

Added owner: `typedef struct ams_mel_ir_track ams_mel_ir_track;` only. No Track
metadata owner, Track metadata event, or Track request owner was added.

Exactly four exports were added:

```text
ams_mel_ir_track_open
ams_mel_ir_track_enable
ams_mel_ir_track_get_capabilities
ams_mel_ir_track_close
```

## Private state

`TrackState` holds the shared `SessionState`, the shared `Channel`, the shared
`TrackChannel`, the lifecycle, `enable_attempted`, `cleanup_started`, and the
allocation-free emergency retention fields (`emergency_self`, `emergency_next`,
`emergency_retained`). There is no metadata state and no request count.

Lifecycle: `Attached`, `Enabled`, `Failed`, `Closed`.

## Open

Open constructs the pinned upstream configuration:

```cpp
irmel::Config{channel_id, irmel::ChannelType::IRSTTrack, platform_id,
              sensor_location, {}, false, false};
```

then requires all three of a non-null `Control::attachChannel()` result, a
successful `dynamic_pointer_cast<TrackChannel>()`, and a
`getCapabilities().getChannelTypes()` containing `ChannelType::IRSTTrack`.

| Open failure | Behavior | Status |
|---|---|---|
| `attachChannel` returns null | no detach attempted | `AMS_MEL_FACTORY_FAILED` |
| wrong concrete channel type | rollback detach proven | `AMS_MEL_INITIALIZATION_FAILED` |
| capability omits IRSTTrack | rollback detach proven | `AMS_MEL_INITIALIZATION_FAILED` |
| `getCapabilities` throws | safe rollback, detach proven | `AMS_MEL_INITIALIZATION_FAILED` |
| rollback detach unproven | complete graph retained permanently | `AMS_MEL_PROVIDER_FAILED` |

No provider object is destructed while detach ownership remains uncertain. When
no public Track owner has yet been returned and rollback detach fails, the graph
is retained by the allocation-free emergency root, the same safety principle
already used by the C2, Image, Health, and Instrumentation families.

## Capability

`ams_mel_ir_track_get_capabilities` is valid while `Attached` or `Enabled` and
reuses `ams_mel::internal::snapshot_capability`; no conversion code was
duplicated. Ada reuses the existing `Capability_Conversion` converter.

## Enable

| Condition | Result |
|---|---|
| `Attached`, provider enable Success | `Enabled`, `AMS_MEL_OK` |
| already `Enabled` | `AMS_MEL_OK` |
| `Failed` or `Closed` | `AMS_MEL_PROVIDER_FAILED` |
| provider enable non-Success | `Failed`, `AMS_MEL_PROVIDER_FAILED` |
| provider enable throws | `Failed`, `AMS_MEL_PROVIDER_EXCEPTION` |

## Close

Null pointer-to-owner returns `AMS_MEL_INVALID_ARGUMENT`; an already null owner
returns `AMS_MEL_OK`. Provider `disable()` is called only when Enable was
attempted.

A failed disable does not prove ownership safety, so detach is still attempted.
If that detach succeeds, the provider channel is destroyed safely, the caller's
owner is cleared, and `AMS_MEL_PROVIDER_FAILED` reports the disable failure.

If detach fails, Close returns `AMS_MEL_PROVIDER_FAILED`, leaves the caller's
Track owner non-null, resets `cleanup_started`, and retains the complete graph,
so a later Close retry is possible and succeeds when the provider recovers.

## Mock TrackChannel

`MockTrackChannel` implements every pure virtual Track operation of the abstract
upstream `TrackChannel`. Because 029B1 must not provide positive behavior for
deferred features:

- `send(SystemTrackDataResponse)` and `send(TrackDataUpdate)` return a completed
  future carrying MEL `ErrorCode::Unsupported`.
- Callback registration for `CandidateObjectMessage`, `IRSTTrackReport`,
  `RequestSystemTrackData`, and `CandidateObjectPreProcMessage` returns
  `Return::NotSupported`. `IRSTTrackReport` is `NotSupported` in 029B1; positive
  report callback behavior belongs exclusively to the later report slice.

Each deferred operation increments a counter and writes a distinct lifetime-log
event, so the native tests prove none of them was invoked.

## Native test

`native/tests/test_ir_track_foundation.c`, CTest target
`ir_track_foundation_contract`. The name `ir_track_contract` is reserved for the
eventual complete Track-core contract.

Covered:

```text
Open success and correct IRSTTrack Config propagation
Capabilities while Attached
Capabilities while Enabled
Enable success and Enable idempotence
Session parent-first close; Track retains provider/session state
wrong ChannelType rejected before provider activity
attach null
wrong concrete channel
missing IRSTTrack capability
capability provider exception
open-time detach failure retention
enable Return::Fail
enable exception
Close without Enable (provider disable never called)
Close after Enable
disable failure + successful detach
detach failure leaves owner non-null
second Close retries detach successfully
provider TrackChannel destroyed before provider library unload
```

Throughout, the tests assert no Track send overload and no Track metadata
registration was invoked.

Ordering note: the fork-based provider-unload ordering tests run before any test
that permanently retains a provider graph. Emergency retention deliberately
keeps the provider library loaded in the test process, and a forked child would
otherwise inherit an already-loaded library.

## Safe Ada

`AMS.MEL.IR.Track` exposes exactly the private `Track_Config` with
`Create_Config`, and the limited private controlled `Track_Channel` with
`Open`, `Is_Open`, `Enable`, `Capabilities`, and `Close`. No `IRST_Track_Report`,
Track metadata package, Track enumerations, or NED type was added.

Ada lifetime behavior proven in `ams_mel_ir_track_tests.adb`: Open, Capabilities
while attached, Enable (including idempotence), Capabilities while enabled,
Session-first close followed by a valid child Capabilities call, explicit Track
Close, finalization of an un-closed channel, Enable failure and exception
mapping, and every Open failure path.

## Raw Rust and private Python

Raw Rust adds `AmsMelIrTrackConfigV1`, the opaque `AmsMelIrTrack`, and the four
`extern "C"` declarations; size/alignment, field offsets, function signatures,
and the function inventory were updated. Raw function count: **76**. No safe Rust
Track API was added.

Private Python adds `IrTrackConfigV1`, `IrTrackHandle`, and the four ctypes
declarations. `BOUND_FUNCTION_NAMES` is **76**. No public Python Track API was
added.

## Real Squall

`integration/squall/*` was not modified. Pinned Squall negative Track validation
is deliberately deferred until the complete `@RequiredIfTrack` report surface
exists.

## ABI audit

```text
ABI version             0.1
exports.map             76
dynamic exports         76
Rust raw functions      76
Python bound functions  76
native CTest targets    11
```

## Validation commands

```sh
make format-ada
make test-native
make test-rust
make test-python
alr -C ada build
alr -C ada/tests run
make check-ada-format
alr -C ada/tests exec -- make -C /home/zboll/git/ams-mel check
cmake --build native/build --target check_track_header_boost_closure
git diff --check
ctest --test-dir native/build --repeat until-fail:50 --output-on-failure
ctest --test-dir native/build -R '^ir_track_foundation_contract$' \
  --repeat until-fail:100 --output-on-failure
```
