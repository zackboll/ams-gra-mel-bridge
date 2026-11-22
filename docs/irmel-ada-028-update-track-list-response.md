# IRMEL-ADA-028 — Safe Ada Scheduling UpdateTrackListResponse reception

## Source and scope

Fetched `origin/main` at `0ce58e6fb60139d5454db4279fe7c4154cfd6ac4`, tree
`d3eb1be096edab3b349b569e65d018da93247beb`. PR #255 was merged at
`fb4b1aadd2da150029c1efc3f909cd3cd3995e23`, an ancestor of that base, and the
SchedCreatedRep implementation was present. Searches of open PRs and remote
branches found no overlapping UpdateTrackListResponse implementation. PR #257,
opened during development, uses the existing Scheduling ABI and adds no
Scheduling-specific metadata operation.

The pinned IR MEL commit is `8d9224519f12b44e0b28815755c56a32a28d24a0`
(tree `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`). The inspected vendored
blobs were `SchedulingChannel.h` (`f11932ac09f2644aa47b00ed7623bb3b8ca9ddf2`),
`UpdateTrackListResponse.h` (`ac4c7f5be4444192e0330d26f685b5713ad0939b`),
`TrackListResponse.h` (`39172d49d3dbbeb85cdb330d792e87b3c563b8d5`),
`PrioritizedTrackListCmd.h` (`d8ff82d65f0beca36c8d468cc6cde64175d886ae`), and
`CommonIR_MEL.h` (`0fc9bd51b6b4b08b58be85b65cd925d7d133f3a3`). Vendor source,
the pinned upstream inventory, and historical coverage prose are unchanged.

Only `SchedulingChannel::registerMetadataCallback(UpdateTrackListResponse)`
and its complete two-field payload move from `missing` to `safe_ada`.
Unique safe Ada operations increase from 63 to 64. SchedulingChannel remains
`partial`, with two missing Scheduling-specific operations: TaskEvents and
ScanPerformanceReport registration. The previously qualified TrackListResponse
enum and PrioritizedTrackListCmd submission gain no new coverage credit.

## Representation and ABI

| Published getter | Native field | Ada field |
|---|---|---|
| `getCommandID()` → `std::uint32_t` | `uint32_t command_id` | `Command_ID : Interfaces.Unsigned_32` |
| `getTrackListResponse()` → `const TrackListResponse&` | `ams_mel_ir_track_list_response_t response` | `Response : Track_List_Receipt` |

| Upstream | Wire value | Ada |
|---|---:|---|
| `None` | 0 | `None` |
| `Rejected` | 1 | `Rejected` |
| `RejectedExpired` | 2 | `Rejected_Expired` |
| `Accepted` | 3 | `Accepted` |

Both rejections are valid metadata, not malformed input or MEL errors. An
unknown value invalidates the whole event. IDs are copied verbatim: zero,
high bits, duplicates, and numerically out-of-order IDs are valid.

`ams_mel_ir_update_track_list_response_v1` is eight bytes, alignment four,
with offsets 0 and 4. It reuses the existing C receipt constants and capability
number 23. The six additive `AMS_MEL_0.1` exports are
`ams_mel_ir_scheduling_update_track_list_{open,receive,get_counters,close,event_view,event_close}`.
Production exports grow from 344 to 350. Python and Rust expose only private
ctypes and raw sys mirrors, respectively. Ada uses a distinct metadata-owned
enum so Metadata does not depend on the Commands child package.

## Registration, ordering, and lifetime

- `Open_Update_Track_List` opts in explicitly while Attached or Enabled. Its
  one-shot is independent of ExecuteTaskAck, TaskExecutingRep, and SchedCreatedRep.
  Invalid local arguments do not consume it; any provider attempt does, including
  refusal, unknown Return, or exception. Registration releases the Scheduling
  mutex before calling the provider and supports synchronous delivery and reentry
  into Scheduling capability queries. There is no unregister.
- Callback admission serializes overlapping callbacks. Events are dequeued in
  successful-enqueue order, not ID order; no ordering between simultaneous
  invocations is assumed before admission. Both getters are read within the
  callback. A full queue drops a valid incoming event without allocating a
  snapshot; malformed input is still counted while full. Snapshot or queue-node
  allocation failure rejects only that event. Counters saturate at `UINT64_MAX`.
- The event owner contains only the fixed record and does not retain the stream
  or provider. `Receive_Update_Track_List` validates the numeric receipt, copies
  both fields, and releases the event on success or exception. The Ada record
  remains usable after every owner is closed and the provider DSO is unloaded.
- Serialize operations on a single public stream, including stream Close.
  Stream Close discards unread events and deactivates consumption only. Scheduling
  Close can wake a waiting Receive; its queued events drain before stopped or
  cleanup-failed status. Physical teardown drains admitted payload reads without
  holding the Scheduling mutex. Late callbacks cannot read inactive payloads,
  and a failed detach conservatively retains uncertain provider ownership.

## Separate from the typed future

`send(PrioritizedTrackListCmd)` returns a `RequestFor<TrackListResponse>`.
`registerMetadataCallback(UpdateTrackListResponse)` is a separate operation.
Neither mechanism registers, completes, enqueues, or changes the other, even
when IDs match. Closing the observer does not cancel admitted requests. Pending
futures retain their provider independently. A metadata `Accepted` report does
not establish physical tracking, task execution, schedule feasibility, or image
acquisition.

## Public-only example

After `make test-ada`:

```sh
ada/tests/bin/ams_mel_ir_scheduling_update_example \
    "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

The program receives all four mock events, closes the stream, channel, and
session, then reads the final owned record.

## Qualification

Mock-provider qualification only. No real-provider, hardware, or ThreadSanitizer
qualification was run. Hosted CI and independent non-author review are separate
gates; local qualification does not satisfy either of them.

### Local results

| Command or evidence | Exit and result |
|---|---|
| `make format-ada`; `make check-ada-format`; `make check-ada-docs-changed` | 0 each; eight changed public declarations inspected, zero annotation defects. |
| `make test-native` | 0; 1,009 passed, including 16 focused UpdateTrackListResponse cases (one layout probe). |
| `make test-ada` | 0; full smoke suite and public-only ownership example passed. |
| `make test-python` | 0; 840 passed. |
| `make test-rust` | 0; 666 passed, zero failed, 15 ignored. |
| `make check-ir-api-coverage` | 0; 87 regression tests passed; 127 headers and 148 operations audited. |
| `make check` | 0; build-tree isolation, native and Ada tests, formatting and text hygiene passed. |
| `alr -n -C ada build` | 0; Ada and production facade built. |
| `alr -n -C ada/tests run` | 0 on the serial retry; the full Ada smoke suite ran, including UpdateTrackListResponse. |
| `ctest --test-dir native/build-tests -R '^ir_scheduling_\|^(ir_)?common_' --output-on-failure` | 0; 355 passed, including the existing Scheduling command and inherited Return/Comms tests. |
| Strict C11 probe (`gcc` and `clang`, `-std=c11 -pedantic-errors -Wall -Wextra -Werror`) | 0 each; layout, opaque handle pointer layout, constants, and six signatures match. |
| C++20 upstream checks | 0; GCC 14.2 and Clang 19.1 compiled the getter/type/value assertions and the mock's exact overload. The prioritized conversion test and callback mapping tests passed. |
| Clang ASan/UBSan focused CTest (`ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`) | 0; 31 passed, including the SchedCreatedRep and prioritized conversion regressions. |
| Isolated sanitizer CTest `--repeat until-fail:50` (eight focused cases) | 0; 400 executions passed, covering blocked Receive, admitted-copy drain, observer Close during a copy, serialized overlap, reentry, detach failure, family coexistence and future independence. |
| Production export comparison (`nm -D --defined-only`) | 0; exactly the six additive `AMS_MEL_0.1` exports, no removals or test symbols. |
| `python3 scripts/check_doc_source_integrity.py`; `git diff --check` | 0 each; 825 vendor files and seven workflows checked. |

Early runs exposed outdated test inventory expectations, a missing trailing
newline in the new C probe, and a missing C++ sanitizer runtime when linking a
C test executable. An initial stress run also overlapped a build that replaced
its shared library. These failures are not counted as passes; the isolated
stress and strict probe results above are from reruns.

An earlier Ada build stopped on an unused test `use type` clause; the clause
was removed. The first Alire runtime invocation exited 1 with `bad_function_call`
in the existing Image checkout test before reaching the new Scheduling test.
A `--skip-build` diagnostic exited 1 in the Image lease address-log test while
the native test tree was rebuilding. Scratch runtime-comparison runs passed
the new Scheduling tests but exited 1 in an RF trace assertion while sharing
its trace path. None counts as a full pass. The exact serial Alire run passed
without source changes or a runtime override; the first failure's root cause
was not established.

### Hosted qualification and review

For implementation commit `1f31a9ad5ce769ed43bc92771bf1a5e5a5bfba37`,
GitHub tested merge result `9fcfa22bb6bff3c245ae709ecd5db3754215ec66`
(tree `44d91dfca923e8b2c7018f21c67fab3fb38a3a06`) against updated main
`26370d0b0cd203a232389337946d43d7873c60f0`. All ten applicable jobs passed:
policy, build-tree isolation, the four GCC/Clang Debug/Release native jobs,
Ada, Rust, Python, and documentation build. Deployment and live verification
were skipped. This documentation-only follow-up has its own hosted status
on [PR #258](https://github.com/zackboll/ams-gra-mel-bridge/pull/258/checks).
Independent non-author technical review remains required; no eligible reviewer
was available to assign. The PR remains open with auto-merge disabled.
