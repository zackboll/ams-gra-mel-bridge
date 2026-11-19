# IRMEL-ADA-025 — safe Scheduling IRSTTrackTask execution

## Preflight and source identities

Task worktree: `/home/zboll/git/ams-mel-irmel-ada-025`; branch
`feature/irmel-ada-025-scheduling-track-task`, based on fetched main
`f76b827bb2d3fa967c8f3624cb1b9deee13f54b6`, tree
`d7b4125a25a92a676147071004f20542d73e4328`.
PR #246 is MERGED at `f22d95469521aadf749e869bf74bb8eab52d6539`;
`git merge-base --is-ancestor` returned 0. GitHub open-PR searches returned
no open PRs; remote branch searches found only historical unrelated Track
metadata/C2 work, no overlapping Scheduling TrackTask branch.

Read repository instructions, architecture, ABI policy, coverage schema and
inventory, and IRMEL-ADA-022/023/024 evidence. IR source remains commit
`8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`. Common source remains commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
`f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`. All vendor bytes are preserved.
The independently frozen catalog records these inspected IR header blobs:

| Header | Git blob |
|---|---|
| SchedulingChannel.h | `f11932ac09f2644aa47b00ed7623bb3b8ca9ddf2` |
| IRSTTrackTask.h | `2d4735bc31bb8fbae32109d3a284667cf4643506` |
| TrackTaskParam.h | `8f84eae3192e651f78e7e62831bc2ac34e1203d0` |
| TrackTaskData.h | `d2aa159a954553b71e470edf161b48727e358c2e` |
| TaskInfo.h | `b2590330f32ffb56cab07c9f2ef20c78c53f57c1` |
| CommonIR_MEL.h | `0fc9bd51b6b4b08b58be85b65cd925d7d133f3a3` |

## Representation and conversion

Exactly one production export is added, `ams_mel_ir_scheduling_exec_track`,
under unchanged `AMS_MEL_0.1`. Existing records and exports are unchanged.

| Ada field | C field | Published getter/setter |
|---|---|---|
| Track_Target.Target_ID | target_id, int32_t | getTargetID / setTargetID |
| Location_Azimuth_Rad | location_azimuth_rad, double | getTargetLocation().az / setTargetLocation |
| Location_Elevation_Rad | location_elevation_rad, double | getTargetLocation().el / setTargetLocation |
| Rate_Azimuth_Rad_Per_Second | rate_azimuth_rad_per_second, double | getTargetRate().az / setTargetRate |
| Rate_Elevation_Rad_Per_Second | rate_elevation_rad_per_second, double | getTargetRate().el / setTargetRate |
| Track_Parameters.Duration_NS | duration_ns, int64_t | getDurationNanoseconds / setDurationNanoseconds |
| Count | count, uint32_t | getCount / setCount |
| Targets | targets[255] | getTargetData / setTargetData |
| Track_Task.Command_ID | command_id, uint32_t | getCommandID / IRSTTrackTask constructor |
| Parameters | track_parameters | getParams / constructor |
| Info | unchanged ams_mel_ir_task_info_v1 | getTaskInfo / constructor |

Ada owns a statically bounded array indexed 1..255; native arrays use 0..254.
Every stored entry is copied, even when Count is zero or less than 255. Count
only describes the active count; the bridge neither removes nor interprets
remaining values. Counts above 255 reject before provider admission. Order,
duplicate IDs, negative IDs including INT32_MIN, signed nanoseconds and all
four TaskListAction alternatives are preserved. Location and rate are four
independent binary64 components. No floating arithmetic or normalization is
applied; getter verification compares binary64 bits, including signed zero.
No epoch, frame conversion, association, correlation or sensor feasibility is
inferred. Unit-labelled fields carry the published AzEl values unchanged.

The private converter constructs all 255 actual TrackTaskData values through
published setters, then sets duration/count/the full array on TrackTaskParam,
constructs `IRSTTrackTask(commandID, params, taskInfo)`, and checks every
published getter before dispatch. It never compares private object memory.
Static assertions pin capacity, return types, signed nanosecond representation,
binary64 component types and existing action constants.

### Qualified LP64 C layout

| Record | Size / alignment | Member offsets in declaration order |
|---|---|---|
| ams_mel_ir_track_task_data_v1 | 40 / 8 | 0, 8, 16, 24, 32 |
| ams_mel_ir_track_task_param_v1 | 10216 / 8 | 0, 8, 16 |
| ams_mel_ir_track_task_v1 | 10248 / 8 | 0, 10216, 10240 |

The strict C11 probe checks every member, record size/alignment, capacity and
export function signature. Private Python ctypes and raw Rust sys tests compare
their layouts to its compiled output. Neither language gains a safe TrackTask
execution API. The ABI contains no C++ object, container, exception or reference.

## Ownership and asynchronous lifecycle

Safe Ada adds Track_Target, Track_Parameters, Track_Task, Exec_Track and distinct
limited Track_Request / owned Track_Result types with Wait, Status, Value,
Rejection_Code, Description and Close. The private request contains the existing
limited-controlled Return observer. No new asynchronous engine is introduced.

The native export reuses Enabled-only Scheduling admission, Session completion
limits, Scheduling-specific accounting and retained provider graph. Dispatch
occurs outside the lifecycle mutex after admission. Reentry and concurrent
Close after admission retain admitted work. Observer Close/finalization and
Session Close do not cancel or prematurely unload it. Timeout is observation,
not cancellation. Outcomes and full rejection diagnostics are cached. Failed
detach conservatively retains uncertain provider ownership; eventual unload is
claimed only on paths with resolved ownership. Callers serialize observer
Wait/Close and channel handle access against Close until successful admission.

Return::Success means scheduling acceptance, not physical tracking success.
Submission does not generate, correlate or consume ExecuteTaskAck or
TaskExecutingRep, nor register any metadata callback. Existing Abort, Stare and
Scan implementation paths are unchanged and included in regression testing.

## Coverage delta

The operation `SchedulingChannel::exec(IRSTTrackTask)` and types IRSTTrackTask,
TrackTaskParam and TrackTaskData move from missing to safe_ada, with all nine
published getters inventoried. Unique safe Ada operations: **60 → 61**.
Missing Scheduling-specific operations: **6 → 5**. Production exports:
**334 → 335**. SchedulingChannel remains **partial**. Reviewed binding evidence,
its integrity anchor, generated report and negative controls are updated;
the historical coverage archive is untouched.

## Qualification evidence and limitations

Logs are task-owned under `/home/zboll/git/irmel025-evidence`.
The host `/tmp` initially exhausted all 1,048,576 inodes. Initial native and
mirror attempts failed creating temporary files; these are not passing runs.
Qualification reruns use `unshare -Ur -m`, make mounts private and mount a fresh
tmpfs on `/tmp` inside that process namespace only. No unrelated artifacts are
deleted. GPRbuild/GNAT are supplied from the already installed Alire toolchains,
without changing dependency pins or global settings.

Observed checkpoints (not a substitute for final-commit hosted CI):

| Command / check | Exit | Result |
|---|---:|---|
| make format-ada | 0 | GNATformat applied before build/test |
| make test-native, private /tmp | 0 | 952/952 |
| final make test-native, private /tmp | 0 | 953/953, including explicit admission exhaustion |
| final make test-ada, installed Alire toolchains on PATH | 0 | full smoke, negative controls and standalone examples |
| final make test-python, private /tmp | 0 | 777 tests and compileall |
| make test-rust, private /tmp | 0 | workspace tests and raw C11 mirror |
| final make test-rust, private /tmp | 0 | full workspace and raw C11 mirror |
| alr -n -C ada build | 0 | explicit Alire build |
| alr -n -C ada/tests run | 0 | full smoke, including safe TrackTask |
| Python unittest discovery, private /tmp | 0 | 777 tests |
| make check-ir-api-coverage | 0 | 79 regression tests; 61 safe operations |
| make check-ada-format | 0 | first-party sources formatted |
| make check-ada-docs-changed | 0 | 14 public declarations; zero invalid annotations |
| strict C11 probe with -pedantic-errors -Wall -Wextra -Werror | 0 | all three records and signature |
| python3 scripts/check_doc_source_integrity.py | 0 | 825 vendor files; seven workflows |

Earlier tests exposed stale count assertions and a TrackTask rejection-prefix
length error in the new tests; these were corrected without weakening contracts.
A stress attempt overlapped a native relink and reported transient "file too
short" loader errors; that campaign failed and must be rerun without builds.
Initial make test-ada without GNAT/GPRbuild on PATH failed and is not a pass.

The actual production DSO was compared directly with exports.map: exactly 335
exports, all versioned AMS_MEL_0.1, no test exports and no existing export removed.
GCC and Clang standalone conversion matrices also passed with C++20,
`-Wall -Wextra -Wpedantic -Werror`. Native tests exercise all 255 getter values,
counts 0/1/254/255, invalid bounds, signed boundaries, duplicate IDs, and all
four floating components independently; the converter negative controls also
exercise infinities, subnormals and NaN payload bits. Native/Ada tests explicitly
exercise completion admission exhaustion without a second provider dispatch.

PR: <https://github.com/zackboll/ams-gra-mel-bridge/pull/248>, non-draft, open,
auto-merge disabled. Independent technical review is requested for exact array
fidelity, numeric preservation, bounds, ABI, dispatch/lifetime, concurrent
cleanup/unload and regression isolation. The collaborator API lists only the
author, so no self-review is assigned. Independent review remains unresolved.

Implementation-checkpoint hosted evidence (not final-head substitutions):

- Bootstrap: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44447080150>.
- API documentation: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44447079838>,
  build passed on `1178561ab928880a302a075f7497f766c2ae3fbe`; deploy/live jobs
  intentionally skipped for a PR and not counted as qualification passes.
- Source browser: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44447079831>.

Final integrated `make check`, post-build deterministic native/Ada stress and
exact-final-commit hosted evidence are recorded as commit-specific PR evidence
after completion, rather than claiming pending work passed here or embedding a
self-referential commit identity. At this documentation checkpoint those gates
are still in progress. Only a fully passing local qualification permits the
READY_FOR_INDEPENDENT_REVIEW disposition; hosted CI and independent review
remain mandatory separate gates. No hardware, real provider, physical tracking,
image acquisition or normative compliance is qualified.
