# Task 029G validation: IR Track CandidateObjectPreProcMessage metadata

## Provenance

- Starting `main` SHA: `8eee59748ec6add82be9aee990e36238b9f614fc`
- Branch: `task/029g-candidate-object-preproc-message`
- Pinned IR MEL revision: `8d9224519f12b44e0b28815755c56a32a28d24a0`, as
  recorded in `docs/upstream-provenance.md`. `native/vendor/**` and
  `docs/upstream-files.sha256.md` are unchanged by this task.

### Headers inspected

- `irmel/library/track/TrackChannel.h`
- `irmel/library/irmel-types/CandidateObjectPreProcMessage.h`
- `irmel/library/irmel-types/CandidateObjectPreProc.h`
- `irmel/library/irmel-types/CandidateObjectHeader.h`
- `irmel/library/irmel-types/HotRegion.h`
- `irmel/library/irmel-types/SensorInertialState.h`
- `irmel/library/irmel-types/IR_Directional.h`
- `irmel/library/irmel-types/Uncertainty.h`
- `irmel/library/irmel-types/ChannelMetadataCapabilityType.h`

## The authoritative callback

`TrackChannel.h` declares exactly one PreProc surface:

```c++
virtual Return registerMetadataCallback(
    std::function<void(
        Channel& channel,
        const CandidateObjectPreProcMessage* const)> callback) = 0;
```

annotated:

```
/// @Optional This function supports callback registration and IR MEL
/// implementation is optional.
/// Intended for use by IR MFAs that use CandidateObjectPreProc
```

### Direction: inbound metadata only

`TrackChannel` declares exactly two `send` overloads,
`send(SystemTrackDataResponse)` and `send(TrackDataUpdate)`. There is **no**
`send(CandidateObjectPreProcMessage)` and **no**
`RequestFor<CandidateObjectPreProcMessage>` anywhere in the pinned tree. A
repository-wide search for `CandidateObjectPreProc` finds it only in
`TrackChannel.h`, `ImageChannel.h`, `ChannelMetadataCapabilityType.h`, and the
two type headers themselves -- always as a callback registration, never as a
request.

This is therefore inbound callback metadata. No `Submit`, `Wait`, request
handle, `Completion`, `WorkerInput`, `future.get()`, completion thread,
`CommandStatus` mapping, or `TrackState::requests` participation is created.
This callback creates zero `RequestFor` workers.

### Return semantics

`TrackChannel.h` documents three answers for this registration:

| Return | Meaning | Adapter behavior |
| --- | --- | --- |
| `Success` | registration succeeded | kind active |
| `NotSupported` | not implemented | **non-fatal**, metadata open continues |
| `Fail` | *"a callback is already registered for this datatype on this channel"* | `AMS_MEL_PROVIDER_FAILED` |

`NotSupported` is non-fatal because the callback's own annotation is
`@Optional`. This follows the established 029E optional-callback policy used
for `RequestSystemTrackData`, **not** the 029F advertised-capability policy
used for `CandidateObjectMessage`. `ChannelMetadataCapabilityType` does name
`CandidateObjectPreProcMessage`, but naming a type in that enumeration does not
upgrade an `@Optional` callback into a required one; the callback annotation
governs.

`Fail` fails closed because it is a genuine conflict: another subscriber owns
the datatype, so our closure may never be invoked and silently claiming success
would promise deliveries the bridge cannot make. `BadPointer`,
`NotImplemented`, and any value a future upstream revision adds are likewise
not documented refusals and fail closed with `AMS_MEL_PROVIDER_FAILED`. A
throwing registration maps to `AMS_MEL_PROVIDER_EXCEPTION`.

## Registration order

`ams_mel_ir_track_metadata_open` now registers, in order:

1. `IRSTTrackReport` (`@RequiredIfTrack`)
2. `CandidateObjectMessage` (`@RequiredIfDetectCandidateObjects`), only when
   the channel advertised the capability
3. `RequestSystemTrackData` (`@Optional`)
4. `CandidateObjectPreProcMessage` (`@Optional`)

Every registration happens with `TrackState::mutex` released, because a
provider may invoke the callback synchronously from inside
`registerMetadataCallback`. The mock proves exactly that for the PreProc kind.

If the PreProc registration fails after earlier callbacks registered: no public
metadata owner escapes, the metadata one-shot stays consumed, callback state
stays retained by `TrackState`, no unregister is attempted (upstream publishes
none), and Track destruction remains the quiescence boundary.

## Complete getter mapping

### `CandidateObjectPreProcMessage` (3 getters, each called exactly once)

These getters return **by value**, including the vector, so each local result
is retained for the whole copy rather than re-called.

| Upstream getter | C ABI field |
| --- | --- |
| `getCandidateObjectHeader()` | `header` (`ams_mel_ir_candidate_object_header_v1`) |
| `getSensorInertialState()` | `inertial_state` (`ams_mel_ir_sensor_inertial_state_v1`) |
| `getCandidateObjectPreProcs()` | `candidate_object_preprocs` span; `hot_regions` span comes from the header |

### `CandidateObjectPreProc` (17 getters, each mapped exactly once)

| Upstream getter | C ABI field | Type |
| --- | --- | --- |
| `getSystemTime()` | `system_time_ns` | `int64_t` |
| `getDetectionCategory()` | `detection_category` | `uint32_t` |
| `getSensorIndex()` | `sensor_index` | `uint32_t` |
| `getSubpixel()` | `subpixel` | `ams_mel_ir_row_col_v1` |
| `getIntensity()` | `intensity` | `double` |
| `getSenRelUnit()` | `sensor_relative_unit` | `ams_mel_ir_directional_v1` |
| `getSignalToInterferenceRatio()` | `signal_to_interference_ratio` | `double` |
| `getSignalToNoiseRatio()` | `signal_to_noise_ratio` | `double` |
| `getCandidateObjectWithBackground()` | `candidate_object_with_background` | `ams_mel_ir_candidate_background_v1` |
| `getClutter()` | `clutter` | `double` |
| `getCandidateObjectQuality()` | `candidate_object_quality` | `double` |
| `getSirDelta()` | `sir_delta` | `double` |
| `getInertialState()` | `inertial_state` | `ams_mel_ir_sensor_inertial_state_v1` |
| `getEdge()` | `edge` | `uint8_t`, exactly 0 or 1 |
| `getAzSigma()` | `az_sigma` | `double` |
| `getElSigma()` | `el_sigma` | `double` |
| `getBackgroundNormalizer()` | `background_normalizer` | `double` |

Nothing is validated or transformed, because upstream constrains nothing:

- `candidateObjectQuality` is documented `///< 0 to 1`, but
  `setCandidateObjectQuality` enforces no range, so it is **not clamped**. The
  mock fixture deliberately supplies 1.5 and 2.5 and the tests assert those
  exact values survive.
- `senRelUnit` is documented as a unit vector but is **not renormalized**.
- The nested quaternions are **not normalized**.
- `detectionCategory` is an upstream bitfield and is **not decoded**.
- `azSigma` / `elSigma` are **not reinterpreted** into any unit.

`edge` is upstream `bool` and is normalized to exactly `0` or `1` so no
indeterminate byte crosses the C boundary.

## 3x3 background patch

Upstream field: `std::array<std::array<std::int16_t, 3>, 3>`.

```c
#define AMS_MEL_IR_CANDIDATE_BACKGROUND_SIDE UINT32_C(3)
#define AMS_MEL_IR_CANDIDATE_BACKGROUND_SAMPLES UINT32_C(9)

typedef struct ams_mel_ir_candidate_background_v1 {
    int16_t samples[9];
} ams_mel_ir_candidate_background_v1;
```

Mapping is **row-major** and exact:

```
samples[row * 3 + column] == upstream[row][column]    for row, column in 0..2
```

The C++ `std::array` object is **never** `memcpy`'d into C storage; all nine
values are copied individually through the published getter in a nested
row/column loop. The element width stays upstream `int16_t` and is not widened.

Foreign representations:

- Raw Rust: `[i16; 9]`
- Private Python: `ctypes.c_int16 * 9`
- Raw Ada: one fixed nine-element C-compatible array, mapped explicitly to the
  safe owned `Candidate_Background` 3x3 value

ABI probes cover the struct size/alignment, the `samples` field offset, and
both dimension constants.

## Header count vs vector size: evidence and conclusion

Task 029F needed `numberOfCOs` because `CandidateObjectMessage` holds
`std::array<CandidateObject, 900>` -- a fixed array with no intrinsic length,
so the header count is the only thing that can select a meaningful prefix.

`CandidateObjectPreProcMessage` is different:

```c++
std::vector<CandidateObjectPreProc> candidateObjectPreProcs{};
```

A `std::vector` already carries its own explicit size.

**Evidence searched for an equality invariant:**

- `CandidateObjectPreProcMessage.h` -- `setCandidateObjectPreProcs` takes the
  vector by value and moves it. It validates nothing and never inspects the
  header.
- `CandidateObjectHeader.h` -- `numberOfCOs` is a bare `std::uint16_t` with no
  documentation comment and no stated relationship to any container.
  `CandidateObjectHeader` is a standalone class shared by both message types.
- `TrackChannel.h` -- documents only the three `Return` values; states no
  payload invariant.
- A repository-wide search for `numberOfCOs` / `NumberOfCOs` across
  `native/vendor/**` returns only `CandidateObjectHeader.h` itself.

**Conclusion: no such invariant is published.** The adapter therefore:

- copies `numberOfCOs` verbatim into the header,
- copies the **complete** vector using its actual `.size()`,
- does **not** truncate to `numberOfCOs`,
- does **not** reject a mismatch,
- preserves both values and lets the consumer decide.

This is asserted positively rather than left implicit: the mock fixture sets
`numberOfCOs = 3` while supplying a **2**-element PreProc vector, and the
native, Ada, and view tests all assert `header.number_of_cos == 3` together
with `candidate_object_preprocs.size == 2`. A truncating or rejecting adapter
fails that fixture.

## HotRegion validation

The existing 029F representation and validation are reused, not duplicated. The
copy helper was refactored into one shared `copy_hot_regions` used by both
candidate message kinds (and one shared `copy_candidate_header`). Valid
upstream enum representations remain exactly `0 INVALID`, `1 FLARE`,
`2 SOLAR`, `3 MASK`; anything else increments `events_received` and
`malformed_or_unsupported` and enqueues nothing. The safe Ada layer likewise
shares one `Copy_Regions` and one `Copy_Header`.

## ABI versioning

### v1 is frozen

Unchanged: exactly `kind`, `track_report`, `request_system_track_data`. The
existing 029F freeze test is preserved.

### v2 is now frozen

v2 was published by 029F and is treated as frozen from this task onward:

```c
typedef struct ams_mel_ir_track_metadata_event_v2 {
    ams_mel_ir_track_metadata_event_v1 base;
    ams_mel_ir_candidate_object_message_v1 candidate_object_message;
} ams_mel_ir_track_metadata_event_v2;
```

The PreProc payload was **not** appended to it.

### v3 is additive

```c
typedef struct ams_mel_ir_track_metadata_event_v3 {
    ams_mel_ir_track_metadata_event_v2 base;
    ams_mel_ir_candidate_object_preproc_message_v1
        candidate_object_preproc_message;
} ams_mel_ir_track_metadata_event_v3;
```

Proven properties (`test_v2_layout_is_frozen`):

- `offsetof(v2, base) == 0` and v2 ends exactly after
  `candidate_object_message`
- `offsetof(v3, base) == 0`
- `sizeof(v3.base) == sizeof(v2)`
- v3 ends exactly after `candidate_object_preproc_message`
- one discriminator remains nested at `v3.base.base.kind`, at byte offset 0 of
  all three records

No earlier payload record is duplicated.

### New view export

Added exactly one export:

```c
ams_mel_ir_track_metadata_event_view_v3
    -> const ams_mel_ir_track_metadata_event_v3 *
```

`ams_mel_ir_track_metadata_event_view` and
`ams_mel_ir_track_metadata_event_view_v2` keep their exact signatures and
output contracts. Export count 89 -> 90. ABI version remains `0.1`.

## Metadata kind 4

```c
#define AMS_MEL_IR_TRACK_METADATA_CANDIDATE_OBJECT_PREPROC_MESSAGE UINT32_C(4)
```

A PreProc event viewed through each version:

| View | Discriminator | PreProc payload |
| --- | --- | --- |
| v1 | `kind == 4` | none exists |
| v2 | `base.kind == 4` | none; CandidateObjectMessage payload zeroed, both spans NULL/0 |
| v3 | `base.base.kind == 4` | complete |

All three views return the same storage: `view == &view3->base.base` and
`view2 == &view3->base` are asserted. Existing report, request, and
CandidateObjectMessage view behavior is unchanged.

## ABI mutation proof

The v2 freeze was mutation-checked. The PreProc payload was temporarily
appended to `ams_mel_ir_track_metadata_event_v2` in `native/include/ams_mel/abi.h`:

```c
typedef struct ams_mel_ir_track_metadata_event_v2 {
    ams_mel_ir_track_metadata_event_v1 base;
    ams_mel_ir_candidate_object_message_v1 candidate_object_message;
    ams_mel_ir_candidate_object_preproc_message_v1 mutant;   /* temporary */
} ams_mel_ir_track_metadata_event_v2;
```

Observed failures, all three as required:

1. **Native contract** -- `ir_track_metadata_contract` fails in
   `test_v2_layout_is_frozen` at the assertion that `sizeof v2` equals the
   padded end of `candidate_object_message`, because the appended member grows
   `sizeof v2` past that bound.
2. **Rust authoritative ABI probe** -- `abi.rs` fails comparing its
   `AmsMelIrTrackMetadataEventV2` layout vector against the authoritative C
   probe output: the probed `sizeof` and the `ams_mel_ir_track_metadata_event_v3`
   `base` size no longer match the Rust struct.
3. **Python authoritative ABI probe** -- `test_abi.py` fails the same
   comparison for `IrTrackMetadataEventV2` against `abi_probe.c`.

The mutation was then reverted and v3 restored. The mutant is **not**
committed; `git status` confirms `native/include/ams_mel/abi.h` carries only
the v3-additive change.

## Event ownership and lifetime

`EventData` now stores the v3 record and owns three vectors:

```c++
struct EventData {
    ams_mel_ir_track_metadata_event_v3 view{};
    std::vector<ams_mel_ir_hot_region_v1> hot_regions;
    std::vector<ams_mel_ir_candidate_object_v1> candidate_objects;
    std::vector<ams_mel_ir_candidate_object_preproc_v1> candidate_preprocs;
};
```

029F candidate storage is unchanged. Bindings: v1 -> `&view.base.base`,
v2 -> `&view.base`, v3 -> `&view`. Vectors are built completely before span
pointers are bound and are never grown after publication; storage is heap
owned, so moving the owning `unique_ptr` through the queue keeps pointers
valid. No provider-owned pointer or STL storage escapes.

`test_preproc_event_lifetime` proves the event stays readable after metadata
close, Track close, Session close, TrackChannel destruction, Control
destruction, manager destruction, and provider library unload -- re-reading
every field, every nested inertial state, and all nine background samples
after `library_unloaded`, and re-acquiring the v3 view successfully, before
`event_close`.

## Callback behavior

Every callback entry increments `events_received`. A null payload or an invalid
HotRegion enum increments `malformed_or_unsupported` and enqueues nothing. A
valid message is deep-copied and enqueued as one event. A full queue drops the
**incoming** event and increments `events_dropped_queue_full`. Any
allocation/getter/copy exception is caught at the shared boundary and never
crosses back into provider code. All four implemented kinds share one FIFO, one
capacity, one counter set, one lifecycle, and one quiescence model.

## Mock scenarios

| Scenario | Proves |
| --- | --- |
| `track-preproc-rich` | synchronous delivery from inside registration, complete fidelity |
| `track-preproc-async` | delivery after registration returned, on a provider thread, released by a barrier file (no sleeps) |
| any pre-029G scenario | `NotSupported` optional refusal -> metadata open still succeeds |
| `track-preproc-register-fail` | `Fail` -> `AMS_MEL_PROVIDER_FAILED` |
| `track-preproc-register-unknown` | unknown `Return` 99 -> `AMS_MEL_PROVIDER_FAILED` |
| `track-preproc-register-throw` | registration throw -> `AMS_MEL_PROVIDER_EXCEPTION` |
| `track-preproc-null` | null payload malformed |
| `track-preproc-bad-region` | HotRegion enum one past `MASK` malformed |
| `track-preproc-overflow` | six messages into capacity 2, DROP-INCOMING |
| `track-preproc-mixed` | deterministic four-kind FIFO |
| `track-preproc-late` | callback after public metadata close |
| `track-preproc-lifetime` | event fidelity past provider unload |

The fixture uses published setters only and gives every field a distinctive
value chosen to reveal field swaps, sign loss, width errors, and row/column
transposition. Entry 0 carries `edge = false` and entry 1 carries
`edge = true`, so both boolean states are covered. No arbitrary sleeps are used
for callback ordering; the async scenario uses a deterministic barrier file.

## Capability coherence of the mock provider

Pinned evidence,
`native/vendor/ir-mel/include/irmel/library/irmel-types/ChannelCapability.h`:

> Indicates the set of Metadata types that are supported by a channel.
> Query this set to check if a specific metadata type is supported or get the
> full list.
> ...
> Calls to a channel's `registerMetadataCallback` function where the channel
> does not have a corresponding entry in this set are expected to return
> `Return::NotSupported`.

`MockTrackChannel::getCapabilities()` therefore assembles one metadata set
from a common base plus the two conditionally supported kinds, so the reported
capabilities and the `registerMetadataCallback` answers always agree:

```
metadata = { IRSTTrackReport, ChannelCommsTestRep };
if (advertises_candidate_objects())
    metadata.insert(CandidateObjectMessage);
if (exercises_preproc())
    metadata.insert(CandidateObjectPreProcMessage);
value.setChannelMetadataCapabilities(metadata);
```

Final mock policy:

| Scenario class | `CandidateObjectPreProcMessage` advertised | Registration answer |
| --- | --- | --- |
| ordinary / pre-029G, e.g. `track-report` | no | `Return::NotSupported` |
| positive PreProc, e.g. `track-preproc-rich`, `-async`, `-overflow`, `-mixed`, `-lifetime`, `-late` | yes | `Return::Success` |
| explicit conflict/error, `track-preproc-register-fail` / `-unknown` / `-throw` | yes | `Fail` / unknown `Return` 99 / throw, as selected |

`track-preproc-mixed` advertises both `CandidateObjectMessage` and
`CandidateObjectPreProcMessage` alongside the common entries, matching the four
callbacks it registers.

The adapter itself is deliberately unchanged and remains tolerant: the PreProc
registration is attempted for every Track metadata open because the upstream
callback is `@Optional`, and `NotSupported` stays non-fatal, so metadata open
still succeeds against a provider that does not advertise the kind. No
`preproc_advertised` gate was added to `TrackState`; only
`candidate_objects_advertised` gates a registration, because that kind is
`@RequiredIfDetectCandidateObjects` rather than `@Optional`.

`test_capability_coherence` in `native/tests/test_ir_track_metadata.c` proves
this through the public bridge capability API only
(`ams_mel_ir_track_get_capabilities` plus
`ams_mel_ir_channel_capability_view`), using a membership helper over
`metadata_capabilities` rather than a hard-coded element index: PreProc present
for `track-preproc-rich`, absent for `track-report`, and both conditional kinds
present for `track-preproc-mixed`.

## Four-kind FIFO

`track-preproc-mixed` drives a deterministic sequence made possible by the
registration order: `IRSTTrackReport`, `CandidateObjectMessage`,
`RequestSystemTrackData`, `CandidateObjectPreProcMessage`. Exactly that order
is asserted natively and in safe Ada, against a single counter set of
`received=4, dropped=0, malformed=0`. The 029F three-kind and candidate-only
regressions are retained.

## Late callback

`test_preproc_late_callback` proves the retained PreProc callback, invoked from
provider teardown after the public metadata owner was closed, enters, returns
safely, queues nothing, lets no exception escape, and returns strictly before
`track_channel_destroyed`, which in turn precedes `control_destroyed`,
`manager_destroyed`, and `library_unloaded`.

## Safe Ada API

`AMS.MEL.IR.Track.Metadata` gains, reusing every existing 029F safe type
(`Row_Column`, `Directional`, `Quaternion`, `Uncertainty`,
`Sensor_Inertial_State`, `Hot_Region_Type`, `Hot_Region`,
`Candidate_Object_Header`):

```ada
subtype Background_Index is Positive range 1 .. 3;
type Candidate_Background is
  array (Background_Index, Background_Index) of Interfaces.Integer_16;

type Candidate_Object_PreProc is record ... Edge : Boolean; ... end record;

type Candidate_Object_PreProc_Message is private;
function Header                        (...) return Candidate_Object_Header;
function Inertial_State                (...) return Sensor_Inertial_State;
function Hot_Region_Count              (...) return Natural;
function Hot_Region_At                 (...) return Hot_Region;
function Candidate_Object_PreProc_Count(...) return Natural;
function Candidate_Object_PreProc_At   (...) return Candidate_Object_PreProc;
```

`Header`, `Inertial_State`, `Hot_Region_Count`, and `Hot_Region_At` are
overloaded on the message type, which keeps the two message APIs symmetric.
`Metadata_Kind` gains `Candidate_Object_PreProc_Message_Event` and
`Metadata_Event` gains the matching `Candidate_PreProcs` variant.

`Receive_Event` now uses the **v3** view internally for every event kind. It
copies all PreProc fields, all nine background samples, every nested inertial
state, and both complete vectors into wholly Ada-owned storage; it closes the
native event on success and on every conversion exception. No native pointer or
span escapes, and `Edge` is a safe `Boolean`.

## Raw layer synchronization

| Layer | Added |
| --- | --- |
| Raw Ada | `IR_Candidate_Background_V1` (9 x `Interfaces.Integer_16`, row-major), `IR_Candidate_Object_PreProc_V1`, `IR_Candidate_Object_PreProc_Span_V1`, `IR_Candidate_Object_PreProc_Message_V1`, `IR_Track_Event_V3`, `IR_Track_Event_View_V3` |
| Raw Rust | `AmsMelIrCandidateBackgroundV1` (`[i16; 9]`), `AmsMelIrCandidateObjectPreProcV1`, `AmsMelIrCandidateObjectPreProcSpanV1`, `AmsMelIrCandidateObjectPreProcMessageV1`, `AmsMelIrTrackMetadataEventV3`, `ams_mel_ir_track_metadata_event_view_v3` |
| Private Python | `IrCandidateBackgroundV1` (`ctypes.c_int16 * 9`), `IrCandidateObjectPreProcV1`, `IrCandidateObjectPreProcSpanV1`, `IrCandidateObjectPreProcMessageV1`, `IrTrackMetadataEventV3`, bound `ams_mel_ir_track_metadata_event_view_v3` |

Raw v1 and v2 are preserved byte-for-byte in every layer. Raw Ada reuses the
canonical `IR_Row_Col_V1`, `IR_Directional_V1`, `IR_Sensor_Inertial_State_V1`,
`IR_Candidate_Object_Header_V1`, and `IR_Hot_Region_V1` rather than duplicating
them. Rust raw function count is 90 and Python `BOUND_FUNCTION_NAMES` is 90.
There is deliberately **no** safe Rust PreProc API and **no** public Python
PreProc API.

## Mock-only positive evidence / Real-Squall limitation

All positive Track behavior in this task is **mock-only**. Pinned Squall cannot
attach a Track channel through `Control::attachChannel` at all, so no real
provider exercises `CandidateObjectPreProcMessage` here. No Squall positive
support is claimed. The mock implements the callback through published setters
only and never advertises capabilities it does not implement.

## Deferred-surface accounting removed

`track_deferred_calls`, `deferred_registration(...)`, and every
"remaining deferred Track surface" comment are removed. After 029G there is no
deferred TrackChannel surface. The previous `no_deferred_track_operations`
helper in the metadata suite is rewritten as `no_unexpected_track_operations`,
which now asserts the positive fact that the `@Optional` PreProc registration
**is** attempted (and refused non-fatally) by pre-029G scenarios, while the
unadvertised CandidateObjectMessage registration and both sends are still
absent. The Track foundation, update, and system-response suites keep their
unexpected-operation assertions under the clearer semantics.

## Track completion status

Every published `TrackChannel`-specific surface is now represented:

```
@RequiredIfTrack core                                      complete
@RequiredIfTrackUpdate TrackDataUpdate                     complete
SystemTrackDataResponse                                    complete
RequestSystemTrackData                                     complete
@RequiredIfDetectCandidateObjects CandidateObjectMessage   complete
CandidateObjectPreProcMessage                              complete
Track API overall                                          complete
```

Precisely:

- native C and safe Ada Track coverage is complete for the published
  TrackChannel-specific surfaces
- safe Rust Track API is still intentionally absent
- public Python Track API is still intentionally absent
- positive Track behavior remains mock-only because pinned Squall cannot attach
  Track through `Control::attachChannel`
