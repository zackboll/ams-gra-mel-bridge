# IRMEL-ADA-029 — Safe Ada Scheduling TaskEvents reception

## Source and scope

The fetched `origin/main` was `1c471f1cdae52d0d8f0d58ea5b71421807418892`, tree
`e182884d4c30a77c3c18388074c8ae649264c16c`. This is the confirmed merge of PR
#258, including the prior UpdateTrackListResponse receiver, its six exports and
coverage. Open PRs and branch history showed no overlapping implementation.

| Pinned source | Commit | Tree |
|---|---|---|
| IR MEL | `8d9224519f12b44e0b28815755c56a32a28d24a0` | `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec` |
| Common MEL | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` |

The inspected IR header blobs were `SchedulingChannel.h`
(`f11932ac09f2644aa47b00ed7623bb3b8ca9ddf2`), `TaskEvents.h`
(`06ddc86196fb16ce95531c11531ac1ca82b7ecf7`), `EventType.h`
(`6670b2bc45fe1db337abdc32f204446f4ce4643b`), and
`ChannelMetadataCapabilityType.h` (`14b5449058a366dcccf6e9eeb77d7e5ddfcfcfa4`).
Vendor sources, pinned upstream inventories and historical coverage prose are
unchanged.

## Exact owned values and ABI

| Published getter | C snapshot field | Ada `Task_Event` field |
|---|---|---|
| `getEventType()` → `const EventType&` | `uint32_t event_type` | `Event_Type : Task_Event_Type` |
| `getScanBar()` → `uint32_t` | `uint32_t scan_bar` | `Scan_Bar : Interfaces.Unsigned_32` |
| `getStopNumber()` → `uint32_t` | `uint32_t stop_number` | `Stop_Number : Interfaces.Unsigned_32` |
| `getSystemTime()` → `std::chrono::nanoseconds` | `int64_t system_time_ns` | `System_Time_NS : Interfaces.Integer_64` |
| `getTrackId()` → `uint32_t` | `uint32_t track_id` | `Track_ID : Interfaces.Unsigned_32` |

The callback evaluates the getters in this order. Time is the signed nanosecond
count with no epoch or conversion to `Duration`. TrackId is unsigned, not the
signed TrackId in other IR MEL records. All bits of all numeric fields are
preserved; no positivity, uniqueness or monotonicity is required.

| Upstream `EventType` | `uint32_t` | Ada `Task_Event_Type` |
|---|---:|---|
| `enumNotSet` | 0 | `Not_Set` |
| `SLEW_START` | 1 | `Slew_Start` |
| `SCAN_START` | 2 | `Scan_Start` |
| `BREAK_SCAN` | 3 | `Break_Scan` |
| `Reserved11` | 4 | `Reserved_11` |
| `Reserved12` | 5 | `Reserved_12` |
| `ACCEL_START` | 6 | `Accel_Start` |
| `ACCEL_COMPLETE` | 7 | `Accel_Complete` |

NotSet is valid. Both reserved values are valid and uninterpreted; values 8 and
above are malformed. `EventType` is distinct from `Task` and
`TaskCompletionStatus`. The C11 `ams_mel_ir_task_events_v1` record is 32 bytes
with alignment 8 and field offsets 0, 4, 8, 16 and 24 on the qualified target.
The existing capability constant `AMS_MEL_IR_METADATA_TASK_EVENTS` is 26;
C++20 assertions verify it, the eight enum values, the getter types and the
record layout. Strict compiled C11 probes check the layout, types, constants,
opaque handles and all six signatures against private ctypes and raw Rust sys
mirrors. There is no safe Python or Rust TaskEvents API.

The only new production exports are the six `AMS_MEL_0.1` functions
`ams_mel_ir_scheduling_task_events_{open,receive,get_counters,close,event_view,event_close}`
(350 → 356). The registration overload, TaskEvents and EventType move from
`missing` to `safe_ada`. Unique safe Ada operations increase 64 → 65;
SchedulingChannel remains `partial`, with ScanPerformanceReport the only
missing Scheduling-specific operation.

## Registration, queue and lifetime

- `AMS.MEL.IR.Scheduling.Metadata.Open_Task_Events` opts in to only this
  optional callback while Attached or Enabled. Local argument errors do not
  consume its independent one-shot; any provider attempt does, including
  refusal, an unknown result or an exception. The provider call runs without
  the lifecycle mutex, allowing synchronous delivery and reentry into
  capability queries. Known refusal results leave the Scheduling owner usable;
  `NotSupported` is reported distinctly in Ada as `Metadata.Not_Supported`.
- The queue is bounded and drop-incoming. Callbacks count on entry, even late
  ones. Admission and publication are serialized within this family, and
  successful enqueues preserve order, duplicates and non-monotonic values.
  A valid callback on a full queue does not allocate an event. Null and unknown
  EventType inputs are malformed even when full. Allocation failure rejects only
  that event. `Task_Events_Counters` exposes the three saturating counters.
- The C event owner holds only the fixed snapshot. `event_view` borrows from
  that owner until event close. `Receive_Task_Events` checks the native enum,
  copies all values and closes the native event whether conversion succeeds or
  raises. Ada values can outlive the stream, channel, session and provider.
- Stream `Close` (or non-raising finalization) discards queued events and
  deactivates consumption without unregistering. Serialize same-owner
  operations and stream Close. Scheduling Close can run during Receive: it
  stops admission, wakes receivers and preserves queued events ahead of terminal
  stopped or failed status. Zero timeout polls, not cancels.
- Before provider disable/detach, physical cleanup waits for admitted
  TaskEvents payload readers, without holding the Scheduling lifecycle mutex.
  Late callbacks retain only callback state and do not read payloads after stop.
  As with the other Scheduling families, the provider must safely manage its
  own invocation frames and threads. Failed detach conservatively retains the
  graph; successful cleanup allows provider DSO unload. Pending requests
  independently protect the graph and continue after stream Close.

## Qualification and limitations

Mock tests use actual pinned upstream objects, all enum alternatives and field
extrema, deterministic copy/receive barriers, allocation failure and saturation,
active and stopped queues, destructor delivery, detach failure and DSO lifetime.
All five metadata families are registered together; tests park two families'
payload reads during teardown and verify fresh delivery to the other streams
after TaskEvents closes. All five command futures are independently held and
released to verify that neither callback delivery nor future completion
synthesizes work on the other path.

This is mock-provider qualification, not hardware, real-provider or
ThreadSanitizer qualification. A TaskEvents value does not prove task success,
physical movement or pointing, time synchronization accuracy, imagery, or
correlation with a local command. It neither completes a `RequestFor` future nor
synthesizes another Scheduling metadata event. The five command operations and
the four existing metadata APIs are unchanged.