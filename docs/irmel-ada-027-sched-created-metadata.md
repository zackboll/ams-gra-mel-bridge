# IRMEL-ADA-027 — Safe Ada SchedCreatedRep reception

## Baseline

Fetched `origin/main` at `5a9962d052445a3f2198199d94bf2235339b8425`, tree
`e0f1314f9d1323c6de293c86a140f7e24aa998a0`, the merge of PR #252. Searches of
remote branch heads and GitHub PRs found no overlapping SchedCreatedRep work.
Open PR #253 concerns safe Rust image checkout; its changed files do not
overlap this slice.
The base has 338 production exports, 62 unique safe Ada operations and four
missing Scheduling-specific operations.

The declarations used here are from these unchanged vendor pins:

| Source | Commit | Tree |
|---|---|---|
| IR MEL | `8d9224519f12b44e0b28815755c56a32a28d24a0` | `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec` |
| Common MEL | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` |
| AMS Math | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` |

## Complete getter mapping

`SchedulingChannel.h` publishes a distinct
`registerMetadataCallback(std::function<void(Channel&, const SchedCreatedRep* const)>)`
overload. This change binds only that overload. The payload comes from
`SchedCreatedRep.h`, `ActivityRecord.h`, `CommonIR_MEL.h` and `RangeAzEl.h`.

| Pinned source | C v1 field | Safe Ada field |
|---|---|---|
| `SchedCreatedRep::getCommandID()` | `command_id` (`uint32_t`) | `Command_ID` |
| `getScanId()` | `scan_id` (`uint32_t`) | `Scan_ID` |
| `getEstDuration().count()` | `estimated_duration_ns` (`int64_t`) | `Estimated_Duration_NS` |
| `getActivityCount()` | `activity_count` (`uint32_t`) | `Activity_Count` |
| `getActivity()` | `activities.data` / `.size` (`const*` / `size_t`) | `Activities` (owned, one-based, `Length` and `Element`) |
| `ActivityRecord::getActivityId()` | `activity_id` (`uint32_t`) | `Activity_ID` |
| `getTaskId()` | `task_id` (existing `ams_mel_ir_task_t`) | `Task_ID` (existing `Task_Kind`) |
| `getTrackId()` | `track_id` (`int32_t`) | `Track_ID` |
| `getPosition().range` | `position.range_m` | `Position.Range_M` |
| `getPosition().az` | `position.azimuth_rad` | `Position.Azimuth_Rad` |
| `getPosition().el` | `position.elevation_rad` | `Position.Elevation_Rad` |
| `getPosition().rangerate` | `position.range_rate_mps` | `Position.Range_Rate_MPS` |
| `getDuration().count()` | `duration_ns` (`int64_t`) | `Duration_NS` |

All four independent position values are copied as binary64 bits, including
signed zero, subnormals, infinities and NaN payloads; no arithmetic or unit
conversion is used. Ada copies the composite record representation without
evaluating non-finite scalar values under GNAT validity checks. Durations
stay signed nanoseconds. Task values 0–10 map
to meaningful literals and 11–16 to the defined non-operational reserved
literals. `NumTaskTypes` (17) and unknown values reject the **whole** event.
Zero/high-bit IDs, signed Track IDs, and duplicates are preserved in vector
order. `Activity_Count` is independent of `Length (Activities)`. The bridge
never truncates, pads or rejects a vector on account of that scalar and does
not use TrackTask's 255-element capacity.

## Ownership, queue and lifecycle

- `Open_Schedule_Created` opts in on an Attached or Enabled Scheduling
  owner. Invalid local arguments do not consume a registration attempt.
  Provider success, refusal, unknown Return or exception does. Registration
  is unlocked: synchronous callbacks and capability-query reentry work.
  No other operation registers this callback. ExecuteTaskAck and
  TaskExecutingRep have separate attempts, public handles, queues and counters.
- A callback retains only bridge callback state. Admission reserves an event
  slot under the queue mutex; the slot counts toward capacity until the
  callback finishes. It checks the upstream vector size and allocation
  bounds, validates the nested discriminators even on a full queue, and
  deep-copies valid, admitted events outside the mutex. Commit publishes only
  a complete snapshot in successful-enqueue order. Null input, invalid Task
  and allocation/copy failure are malformed; a full queue drops valid
  incoming events without allocating a snapshot. Earlier entries are not
  evicted and a failed copy releases its reservation. The three counters
  saturate; `Events_Received` counts callback entries, including late entries.
- C `event_view` borrows both the immutable report and its activity span
  from the event alone. The span uses an element count and is `{NULL, 0}`
  when empty. The event is independent of the stream and provider. Ada
  checks span bounds, copies into its own vector and closes the event even
  if conversion raises. Values and copies of values remain valid after all
  owners and the provider close. No native pointer is public in Ada.
- Explicit stream Close (or non-raising limited-controlled finalization)
  disposes of consumption, not registration. No unregister is assumed.
  Serialize operations and Close on the same stream; Scheduling Close can
  run while Receive waits. Scheduling Close stops new admission and wakes
  receivers. Queued events drain before terminal stop or cleanup failure.
  Pending Scheduling commands continue retaining the provider independently.
- Physical cleanup waits for admitted SchedCreatedRep payload reads/copies
  before provider disable/detach. It does not hold the Scheduling lifecycle
  mutex across provider calls or queue waits. Late callbacks may still enter
  but cannot read payloads or queue events. The provider must manage the
  lifetime of its own invocation frames and threads when detaching/destroying
  the channel; no upstream unregister or general callback-quiescence API is
  claimed. Failed detach retains the uncertain graph for retry (or
  conservatively retains an orphan if deferred teardown failed). Successful
  cleanup allows provider unload without invalidating received events.

## ABI and coverage

The three new C records are `ams_mel_ir_activity_record_v1`,
`ams_mel_ir_activity_span_v1` and `ams_mel_ir_sched_created_rep_v1`. Six
`AMS_MEL_0.1` exports under `ams_mel_ir_scheduling_schedule_created_` are
added: `open`, `receive`, `get_counters`, `close`, `event_view`, `event_close`.
Production exports are **338 → 344**, with no removals or test hooks. The
compiled strict C11 probe checks the six signatures and types, and both raw
mirrors compare all four records' sizes, alignments and offsets (including
the reused `RangeAzEl` layout). No safe Python or Rust API is added.

Only the SchedCreatedRep registration, SchedCreatedRep and ActivityRecord
change from `missing` to `safe_ada`. Unique safe Ada operations **62 → 63**;
missing Scheduling operations **4 → 3**. `SchedulingChannel` stays
`partial`: UpdateTrackListResponse, TaskEvents and ScanPerformanceReport
reception are still missing. No command behavior or response future was
changed. The pinned inventories and vendor bytes are unchanged; the reviewed
binding ceiling, its integrity anchor and generated coverage are updated.

## Example

After `make test-ada`, run the standalone public-only Ada executable:

```sh
ada/tests/bin/ams_mel_ir_scheduling_created_example \
    "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

It receives a report with scalar count 0 and vector length 300, then reads
the owned result after all owners close.

## Qualification boundary

Mock-provider evidence only; no real provider, hardware, task execution,
request correlation, feasibility, completion or imagery is inferred.
The event bound is in reports, not bytes; oversized native spans that cannot
be represented by Ada's collection/index types are rejected, not truncated.
Copying requires available memory and a binary64 Ada `Long_Float` target.

### Local results

| Command or evidence | Result |
|---|---|
| `make format-ada`; `make check-ada-format`; `make check-ada-docs-changed` | Passed; 12 public declarations inspected with no missing or invalid annotations. |
| `make test-native` | 992 passed, including 14 SchedCreatedRep cases (one layout probe) and the existing Scheduling command/metadata and common KeepAlive/Comms regressions. |
| `make test-ada` | Passed with the Alire-installed GNAT/GPRbuild on PATH. The complete smoke suite and public-only standalone example both ran. |
| `make test-python` | 818 passed, including ctypes layout/signature and production export-set checks. |
| `make test-rust` | 652 passed, 0 failed, 15 ignored; includes raw sys layout/signature and export-set checks. Ignored cases are not counted as passes. |
| `make check-ir-api-coverage` | 84 regressions passed; 127 headers and 148 operations audited. |
| `make check` | Passed, including build-tree isolation, the full native and Ada smoke suites, the standalone Ada example and text hygiene. |
| `alr -n -C ada build` | Passed with GNAT 16.1.0 and GPRbuild 26.0.1. |
| `alr -n -C ada/tests run` | Passed; verified execution of the full smoke suite, including the new SchedCreatedRep checks. |
| Strict C11; GCC 14.2 and Clang 19.1 | Passed with `-std=c11 -pedantic-errors -Wall -Wextra -Werror`. GCC built the full facade and test suite; Clang built the facade, mock and focused suite (14 passed). |
| Clang ASan/UBSan (`detect_leaks=1`, `halt_on_error=1`) | All 14 focused cases passed. |
| `ctest --repeat until-fail:50` (six focused cases) | 300 executions passed, covering blocked receive, active copy, no external DSO pin, sync/reentry, all-family coexistence and detach failure. |
| `nm -D --defined-only`; source integrity and whitespace | Six new `AMS_MEL_0.1` exports; no removals or test symbols in production. Source integrity checked 825 vendor files and seven workflows; vendor, pinned evidence and historical coverage bytes are unchanged. |

Early attempts exposed build/coverage issues and a scalar Ada validity check
rejecting non-finite values; the final implementation copies the composite
position representation. An isolated Ada launch passed the SchedCreatedRep
tests but could not qualify the full smoke suite without the RF subprocess
executables. A stress rerun against a build directory concurrently reset by
Alire did not run; the recorded 300 executions use an isolated Clang tree.
Alire also reported ambiguous executable discovery when the isolated Ada
binary was under the regular test output directory; that binary was moved
outside the crate before rerunning.
These earlier attempts are not counted as full passes. No ThreadSanitizer or
real-provider test is claimed.

Hosted CI and separate non-author technical approval remain independent gates.
