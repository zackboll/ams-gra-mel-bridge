# Task 029F validation: IR Track CandidateObjectMessage inbound metadata

    Direction: CandidateObjectMessage is delivered INBOUND through
    TrackChannel::registerMetadataCallback. The pinned TrackChannel defines
    no send(CandidateObjectMessage) and no
    RequestFor<CandidateObjectMessage>.

## Provenance

- Starting `main` SHA: `0809cbfa9f948bae19b45d119c563506ce68f2e2`
- Branch: `task/029f-candidate-object-message`
- Pinned IR MEL revision: `8d9224519f12b44e0b28815755c56a32a28d24a0`, as
  recorded in `docs/upstream-provenance.md`. The vendored tree under
  `native/vendor/ir-mel` is unchanged; the vendor delta and
  `docs/upstream-files.sha256.md` are both unchanged by this task.

### Headers inspected

    native/vendor/ir-mel/include/irmel/library/track/TrackChannel.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/CandidateObjectMessage.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/CandidateObject.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/CandidateObjectHeader.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/HotRegion.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/SensorInertialState.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/IR_Directional.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/Uncertainty.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/ChannelMetadataCapabilityType.h
    native/vendor/ir-mel/include/irmel/library/irmel-types/ChannelCapability.h

## Authoritative callback signature

`TrackChannel.h:48`:

```cpp
virtual Return registerMetadataCallback(
    std::function<void(Channel& channel,
                       const CandidateObjectMessage* const)> callback) = 0;
```

The complete published `TrackChannel` operation set is two `send` overloads
(`SystemTrackDataResponse`, `TrackDataUpdate`) and four
`registerMetadataCallback` overloads (`CandidateObjectMessage`,
`IRSTTrackReport`, `RequestSystemTrackData`, `CandidateObjectPreProcMessage`),
plus `using Channel::registerMetadataCallback;` and `using Channel::send;`.

**There is no `send(CandidateObjectMessage)` and no
`RequestFor<CandidateObjectMessage>` anywhere in the pinned tree.** Therefore
CandidateObjectMessage is implemented purely on the existing inbound Track
metadata path. This task creates zero request handles, zero `Submit`/`Wait`
pairs, zero `Completion` objects, zero `WorkerInput` objects, zero completion
threads, and zero `CommandStatus` mappings, and it does not participate in
`TrackState::requests`.

## Annotation distinction

These are three genuinely different upstream conditions and are deliberately
not collapsed:

| Entity | Annotation |
| --- | --- |
| `CandidateObjectMessage` class | `@RequiredIfBuiltInTracker` |
| `TrackChannel` CandidateObjectMessage callback | `@RequiredIfDetectCandidateObjects` |
| `CandidateObject` class | `@RequiredIfTrack` |
| `CandidateObjectHeader` class | `@RequiredIfTrack` |
| `HotRegion` class and `HotRegionTypeEnum` | `@Optional` |
| `SensorInertialState` class | `@Required` |

The **callback** annotation, `@RequiredIfDetectCandidateObjects`, is the
contract that governs registration here.

## Capability-advertisement rule

`ChannelMetadataCapabilityType::CandidateObjectMessage` exists
(`ChannelMetadataCapabilityType.h:34`). `ChannelCapability.h:266-268`
documents:

> Calls to a channel's registerMetadataCallback function where the channel does
> not have a corresponding entry in this set are expected to return
> Return::NotSupported.

Track `Open` therefore performs one `getCapabilities()` query that serves both
the existing channel-type check and a new private
`TrackState::candidate_objects_advertised` flag. No new public state API is
added and no additional provider call is introduced.

- **Not advertised**: the CandidateObjectMessage registration is skipped
  entirely. Every pre-existing metadata deployment keeps its exact previous
  behavior.
- **Advertised**: the callback is registered, second, between
  `IRSTTrackReport` and `RequestSystemTrackData`.

`TrackState::mutex` is not held across any registration call, because a
provider may deliver a message synchronously from inside
`registerMetadataCallback`.

## Callback `Return` semantics

When the capability **is** advertised:

| `Return` | Result |
| --- | --- |
| `Success` | registration active |
| `NotSupported` | **fail closed** to `AMS_MEL_PROVIDER_FAILED` |
| `Fail` | **fail closed** to `AMS_MEL_PROVIDER_FAILED` |
| `BadPointer`, `NotImplemented`, unknown/future value | **fail closed** to `AMS_MEL_PROVIDER_FAILED` |
| exception | `AMS_MEL_PROVIDER_EXCEPTION` |

Rationale: the channel advertised the metadata type. Refusing the
`@RequiredIfDetectCandidateObjects` callback contradicts that advertisement, so
Metadata Open must not return success while promising an event path the adapter
cannot receive. This deliberately differs from the `@Optional`
`RequestSystemTrackData`, whose `NotSupported` remains non-fatal, and that
existing behavior is unchanged.

On a CandidateObjectMessage registration failure after `IRSTTrackReport` had
already registered:

- no `Metadata_Channel` owner escapes
- the one-shot metadata attempt stays consumed
- the retained `IRSTTrackReport` callback state stays valid
- no unregister is attempted (upstream declares none)
- Track teardown remains the callback-quiescence boundary

## Complete payload mapping

`CandidateObjectMessage` publishes exactly `getHeader`, `getInertialState`, and
`getCandidateObjects`. `MAX_CANDIDATE_OBJECTS` is **900**.

### CandidateObjectHeader

| Upstream getter | C field | Representation |
| --- | --- | --- |
| `getNumberOfCOs` | `number_of_cos` | `uint16_t` |
| `getStackFrameIndex` | `stack_frame_index` | `uint16_t` |
| `getCFAR` | `cfar` | **`float`** (C binary32; deliberately NOT widened to `double`) |
| `getValidityFlagBitField` | `validity_flag_bitfield` | `uint16_t`, carried verbatim and deliberately NOT decoded |
| `getTOVutcNanoseconds` | `tov_utc_ns` | `int64_t` signed nanoseconds |
| `getHotRegions` | `hot_regions` span | complete vector, published order |

### HotRegion

Every published getter is copied exactly once: `getType`, `getSize`, `getTop`,
`getLeft`, `getRight`, `getBottom`. `size`, `top`, `left`, `right`, and
`bottom` stay `uint16_t`. No additional geometric semantics are invented.

`HotRegionTypeEnum` is exactly `HOTREGIONTYPE_INVALID = 0`,
`HOTREGIONTYPE_FLARE = 1`, `HOTREGIONTYPE_SOLAR = 2`, `HOTREGIONTYPE_MASK = 3`,
with **no `MaxExclusive` enumerator**. A provider enum representation outside
`0..3` is malformed.

The HotRegion vector has no published fixed maximum in this interface, so the
complete vector is deep-copied.

### SensorInertialState

The existing canonical C ABI types are reused verbatim;
`ams_mel_ir_directional_v1`, `ams_mel_ir_quaternion_v1`,
`ams_mel_ir_uncertainty_v1`, and `ams_mel_ir_sensor_inertial_state_v1` were
relocated earlier in `abi.h` so the Track metadata event can reference them. No
layout changed and no duplicate representation was created.

`getSystemTime` (`int64` signed nanoseconds), `getQ_xyzw` (x y z w),
`getQECEF_xyzw` (x y z w), `getSensorPosition` (x y z), `getSensorVelocity`
(x y z), and `getUncertainties` (two `uint32`) are copied verbatim. There is no
normalization, no unit-vector enforcement, and no quaternion normalization.

### CandidateObject

| Upstream getter | C field | Representation |
| --- | --- | --- |
| `getSystemTime` | `system_time_ns` | `int64_t` |
| `getDetectionCategory` | `detection_category` | `uint32_t` |
| `getSensorIndex` | `sensor_index` | `uint32_t` |
| `getSubpixel` | `subpixel` | `ams_mel_ir_row_col_v1` (`row`, `column` doubles) |
| `getIntensity` | `intensity` | `double` |
| `getSenRelUnit` | `sensor_relative_unit` | canonical `ams_mel_ir_directional_v1` |
| `getSignalToInterferenceRatio` | `signal_to_interference_ratio` | `double` |
| `getSignalToNoiseRatio` | `signal_to_noise_ratio` | `double` |

`RowCol` is represented as a genuine row/column pair, not as an XYZ triple with
a meaningless third component: upstream `RowCol` publishes only `getRow` and
`getCol`. No floating-point value is clamped or normalized.

## numberOfCOs / 900 prefix interpretation

`CandidateObjectMessage` stores `std::array<CandidateObject, 900>` while
`CandidateObjectHeader` separately carries `numberOfCOs`.

Pinned evidence examined:

- `CandidateObjectMessage.h:18` defines
  `static constexpr std::uint32_t MAX_CANDIDATE_OBJECTS = 900`.
- `CandidateObjectMessage.h:50-57` publishes only a whole-array getter and a
  whole-array setter; there is no per-slot validity accessor and no published
  "used length" other than the header's `numberOfCOs`.
- `CandidateObjectHeader.h:100` declares `numberOfCOs{0}` with no documented
  alternative meaning, and `CandidateObjectHeader.h:103` documents
  `validityFlagBitField` as the validity of **`stackFrameIndex` and `CFAR`**
  only; it says nothing about the array length.
- `CandidateObject` default-constructs every member to zero, so trailing slots
  of a partially filled array are indistinguishable from real zero-valued
  detections without a length.

**Interpretation adopted:** `numberOfCOs` is the meaningful prefix length.

- `numberOfCOs <= MAX_CANDIDATE_OBJECTS` is required.
- `numberOfCOs > 900` is malformed: `malformed_or_unsupported += 1` and nothing
  is enqueued, because such a count cannot describe a prefix of the published
  array.
- For a valid message, exactly `candidateObjects[0 .. numberOfCOs-1]` is
  exposed. All 900 storage slots are never exposed when `numberOfCOs` is
  smaller, and trailing entries are neither read nor converted.

The mock places a recognizable sentinel in slot 3 while `numberOfCOs` is 3, and
both the native and Ada tests assert that sentinel is never reachable.

## Malformed rules

Every callback invocation increments `events_received`. Then:

| Condition | Result |
| --- | --- |
| metadata consumption inactive | return safely under existing rules |
| null `CandidateObjectMessage *` | `malformed_or_unsupported += 1`, enqueue nothing |
| `numberOfCOs > 900` | `malformed_or_unsupported += 1`, enqueue nothing |
| HotRegion enum outside `0..3` | `malformed_or_unsupported += 1`, enqueue nothing |
| allocation/conversion exception | no exception crosses the provider callback boundary; the metadata state enters the existing failed behavior |
| valid | deep-copy the complete payload and enqueue exactly one event |
| queue full | **DROP INCOMING**, `events_dropped_queue_full += 1` |

All three implemented kinds share one FIFO, one capacity, and one counter set.

## C ABI

Added:

    AMS_MEL_IR_MAX_CANDIDATE_OBJECTS                 900
    ams_mel_ir_hot_region_type_t
    AMS_MEL_IR_HOT_REGION_INVALID / _FLARE / _SOLAR / _MASK
    ams_mel_ir_row_col_v1
    ams_mel_ir_hot_region_v1
    ams_mel_ir_hot_region_span_v1
    ams_mel_ir_candidate_object_header_v1
    ams_mel_ir_candidate_object_v1
    ams_mel_ir_candidate_object_span_v1
    ams_mel_ir_candidate_object_message_v1
    AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_MESSAGE = 3

Reused without duplication: `ams_mel_ir_sensor_inertial_state_v1`,
`ams_mel_ir_quaternion_v1`, `ams_mel_ir_directional_v1`,
`ams_mel_ir_uncertainty_v1`. Those four declarations were relocated earlier in
`abi.h` purely for declaration order; their layouts are unchanged, which the
ABI probes verify.

### Corrective ABI versioning (supersedes the first 029F draft)

The first 029F implementation appended `candidate_object_message` to
`ams_mel_ir_track_metadata_event_v1`. Peer review identified that as a merge
blocker: `docs/c-abi-policy.md` prohibits appending fields to an existing
fixed-layout record without a compatible version scheme or a new type and
operation. That append has been removed.

Historically, task **029E** had already appended `request_system_track_data` to
this same v1 record, which was itself inconsistent with the stated policy. The
029E documentation is not rewritten as though that append never happened; this
is a corrective/supersession note. The layout currently on `main` is
**grandfathered and permanently frozen** rather than broken a second time,
because reverting it would be another ABI break relative to current `main`:

```c
/* FROZEN */
typedef struct ams_mel_ir_track_metadata_event_v1 {
    ams_mel_ir_track_metadata_kind_t kind;
    ams_mel_ir_track_report_v1 track_report;
    ams_mel_ir_request_system_track_data_v1 request_system_track_data;
} ams_mel_ir_track_metadata_event_v1;
```

029F adds a new version record instead:

```c
typedef struct ams_mel_ir_track_metadata_event_v2 {
    ams_mel_ir_track_metadata_event_v1 base;
    ams_mel_ir_candidate_object_message_v1 candidate_object_message;
} ams_mel_ir_track_metadata_event_v2;
```

- v1 remains frozen; its size, alignment, and three member offsets are
  unchanged from the base `main` SHA.
- `offsetof(ams_mel_ir_track_metadata_event_v2, base) == 0`.
- Neither the report nor the `RequestSystemTrackData` layout is duplicated: the
  complete v1 record is reused as the first member.
- `base.kind` remains the single discriminator.
- Candidate variable-size storage remains event-owned.

`ams_mel_ir_track_metadata_event_view` is unchanged in signature and semantics
and still returns `const ams_mel_ir_track_metadata_event_v1 *`, so an existing
consumer needs no recompilation merely because CandidateObjectMessage was
added. It deliberately does **not** gain a larger output contract.

One new C function was required:

    ams_mel_ir_track_metadata_event_view_v2

The exported function count becomes **89** (88 -> 89). Raw Rust declarations
and Python `BOUND_FUNCTION_NAMES` are 89 as well. The facade ABI version
remains **0.1**.

**Future rule:** Track metadata additions must not append fields to v1 or v2.
A further version record with the earlier version as its first member, plus a
matching view operation, is required.

### v1 compatibility tests

`native/tests/test_ir_track_metadata.c` adds `test_v1_layout_is_frozen`, which
asserts purely from the header that v1 ends immediately after
`request_system_track_data` (so a fourth member would grow it), that
`offsetof(v2, base) == 0`, that `sizeof(v2.base) == sizeof(v1)`, and that v2
ends immediately after `candidate_object_message`.

`test_v1_view_compatibility` drives the mixed 3-kind FIFO and proves:

- the v1 view still works for `IRSTTrackReport`
- the v1 view still works for `RequestSystemTrackData`
- a Candidate event through v1 reports `kind == 3` while v1 carries no
  candidate payload at all and keeps its own members zeroed
- the v2 view exposes the complete CandidateObjectMessage
- `v2.base.kind` is the CandidateObjectMessage kind
- both views address the same storage (`&view2->base == view`)

Every Candidate-specific fidelity, FIFO, malformed, lifetime, and quiescence
test now reads the payload through the v2 view. The C, Rust, and Python ABI
probes assert the exact v1 member set alongside the v2 record.

#### Mutation proof

The compatibility check was verified to be capable of detecting future
accidental v1 growth. `candidate_object_message` was temporarily re-appended to
`ams_mel_ir_track_metadata_event_v1` in `abi.h` and the affected suites were
rerun. All three authoritative layers failed, exactly as intended:

- Native CTest: `93% tests passed, 1 tests failed out of 15`, the failure being
  `11 - ir_track_metadata_contract`, at
  `test_ir_track_metadata.c:713`:

      FAIL sizeof v1 == offsetof(ams_mel_ir_track_metadata_event_v1,
                                 request_system_track_data) +
                        sizeof v1.request_system_track_data
      FAIL test_v1_layout_is_frozen() == EXIT_SUCCESS

- Rust `ams-mel-sys --test abi`: `declarations_match_the_c_header` panicked;
  the authoritative C probe reported v1 size `368` where the frozen Rust
  `AmsMelIrTrackMetadataEventV1` is `184`.
- Python `tests.test_abi`: the ctypes/C comparison failed at the same element,
  `368 != 184`.

The v2 implementation was then restored and all suites pass again. A future
accidental v1 append therefore cannot ship silently.

## Event ownership and storage

`EventData` now owns the variable-size storage:

```cpp
struct EventData {
    ams_mel_ir_track_metadata_event_v1 view{};
    std::vector<ams_mel_ir_hot_region_v1> hot_regions;
    std::vector<ams_mel_ir_candidate_object_v1> candidate_objects;
};
```

Both vectors are built completely first; only then are the span pointers and
sizes bound into `view`, and the vectors are never grown afterwards. Because
`std::vector` storage is heap allocated, moving the owning `unique_ptr` through
the queue and into the public event owner never invalidates those pointers.

`metadata_callback_impl` was generalized so a builder receives the whole
`EventData` rather than only the fixed view; the `IRSTTrackReport` and
`RequestSystemTrackData` builders were updated mechanically and their behavior
is unchanged.

Guarantees, all covered by tests:

- no provider-owned pointer escapes
- no provider STL storage escapes
- no callback-stack pointer escapes
- event span pointers stay valid until `event_close`
- the event may outlive the public `Metadata_Channel`, the Track, the Session,
  provider channel destruction, and provider library unload
- event contents are immutable after publication
- `EventData` starts zeroed, so unselected fixed members stay zero and every
  unselected span keeps a NULL pointer and a zero size

## Registration ordering

1. `IRSTTrackReport` (`@RequiredIfTrack`)
2. `CandidateObjectMessage` (`@RequiredIfDetectCandidateObjects`), only when
   advertised
3. `RequestSystemTrackData` (`@Optional`)

## Mixed FIFO

The `track-candidate-mixed` scenario emits the report from the first
registration, the candidate message from the second, and the request from the
third, producing the deterministic queue order

    IRSTTrackReport, CandidateObjectMessage, RequestSystemTrackData

with a single shared counter set reading `received=3, dropped=0, malformed=0`.
Both the native contract test and the Ada test assert this exact order and
complete payload fidelity for each kind.

Shared capacity and DROP-INCOMING across kinds is separately proven by
`track-candidate-overflow`: six candidate messages into a capacity-2 queue give
`received=6, dropped=4`, and the two retained events are the first two in
arrival order, each still owning its own complete hot-region and candidate
storage.

## Lifetime and quiescence proof

`track-candidate-lifetime` runs in a forked child so a provider already loaded
by the parent cannot hide unload behavior. It acquires one candidate event,
then closes metadata, Track, and Session. The ordered lifetime log proves

    track_channel_destroyed
      -> control_destroyed
      -> manager_destroyed
      -> library_unloaded
      -> candidate_event_only_owner
      -> candidate_event_close_begin

and the complete payload (header, hot-region span, candidate-object span, and
SensorInertialState) is re-verified field by field *after* provider library
unload, while only the native event owner remains. `event_close` then releases
it.

`track-candidate-late` proves the late-callback path: after the public metadata
owner is closed the provider invokes the retained candidate callback from
`disable()`. The log proves the callback entered and returned, that it returned
strictly before `TrackChannel` destruction, and that provider unload remains
strictly after channel destruction. No event is queued and no exception
escapes.

## Safe Ada

`AMS.MEL.IR.Track.Metadata` gains the `Candidate_Object_Message_Event` kind and
owned Track-facing value types `Row_Column`, `Directional`, `Quaternion`,
`Uncertainty`, `Sensor_Inertial_State`, `Hot_Region_Type`, `Hot_Region`,
`Candidate_Object`, and `Candidate_Object_Header`. These are deliberately
independent of `AMS.MEL.IR.Image`, so Track gains no Image dependency even
though the C ABI reuses the one canonical native layout underneath.

`Candidate_Object_Message` is private and holds `Ada.Containers.Vectors`
storage, exposed through `Header`, `Inertial_State`, `Hot_Region_Count`,
`Hot_Region_At`, `Candidate_Object_Count`, and `Candidate_Object_At`. Every hot
region, every candidate object, and every scalar is copied before the native
event is closed, so no native pointer, native span, or provider storage is ever
exposed. On a conversion failure the native event owner is still closed.

The report-only `Receive` error text no longer names `RequestSystemTrackData`
as the single alternative; it now reports a non-report metadata event and
directs the caller to `Receive_Event`.

## Mock-only positive evidence and the real-Squall limitation

Positive CandidateObjectMessage evidence is **mock provider only**. Pinned
Squall still cannot attach a Track channel through the bridge's established
`Control::attachChannel` path, so it yields only the existing expected-negative
Track Open result (`attachChannel returned null`). The bridge was not modified
to bypass `Control::attachChannel`, and no positive Squall CandidateObject
evidence was manufactured.

## Mock deferred-surface accounting

After 029F the mock positively implements the `IRSTTrackReport` callback, both
published `send` overloads, the `@Optional` `RequestSystemTrackData` callback,
and the `@RequiredIfDetectCandidateObjects` `CandidateObjectMessage` callback.
Legitimate CandidateObjectMessage registration is no longer counted as a
deferred-operation violation. The **only** remaining deferred Track metadata
callback is `CandidateObjectPreProcMessage`, which the mock still instruments
and reports as `NotSupported`, and which the mock never advertises.

Scenarios that do not advertise `CandidateObjectMessage` keep their exact
previous advertised capability set and their previous behavior, and the
existing regression tests assert that the candidate registration never happened
for them.

## Remaining Track gap

`CandidateObjectPreProcMessage` (`@Optional`) remains unimplemented, so the
Track API as a whole is still **not** complete.

    @RequiredIfTrack core                                   complete
    @RequiredIfTrackUpdate TrackDataUpdate                  complete
    SystemTrackDataResponse                                 complete
    RequestSystemTrackData                                  complete
    @RequiredIfDetectCandidateObjects CandidateObjectMessage complete
    CandidateObjectPreProcMessage                           unimplemented
    Track API overall                                       incomplete
