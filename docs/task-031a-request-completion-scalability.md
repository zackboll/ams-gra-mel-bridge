# Task 031A — completion-worker characterization

Starting `origin/main`: `51ec6edda585aed9d25db11e5efaf73a00fc243b`.
## Scope

Mock-provider measurements of the existing consumer-side RequestFor
completion workers, **not** a scheduling redesign or real Squall measurement.

## Published RequestFor contract

The pinned published common MEL header defines:

```cpp
template <typename T>
using RequestFor =
    std::future<ErrorOr<std::shared_ptr<T>>>;
```

## Worker inventory

All entries preallocate the owner/input, retain the provider graph through a
completion-owned channel/stream pointer, detach one native worker, and cache the
terminal result (including rejection/failure) for repeated waits. Post-send
allocation/launch failure retains the input/future through the family's
allocation-free permanent emergency root; deferred cleanup failures retain the
provider graph rather than unloading uncertain provider ownership.

| Family / sends | source function | `RequestFor<T>` | worker / get | accounting | emergency retention | parent-first teardown | cache |
|---|---|---|---|---|---|---|---|
| C2 ModeCmd (Operate/TaskSched/general Mode) | `ir_c2.cpp:submit_mode_command` | `MFA_Mode` | `run_worker` / `complete` | `ChannelState::requests` | `retain_worker` | C2 disable/detach deferred | `Completion::result/kind/message` |
| C2 BIT, ConfigSet, KeepAlive | `ir_c2.cpp:submit_return_operation` | `Return` | `run_return_worker` / `complete_return` | same C2 requests | `retain_return_worker` | same | `ReturnCompletion::result/kind/message` |
| C2 Channel CommsTest | `ir_c2.cpp:submit_comms_operation` | `ChannelCommsTestRep` | `run_comms_worker` / `complete_comms` | same C2 requests | `retain_comms_worker` | same | `CommsCompletion::result/kind/message` |
| Image NavigationReport | `ir_navigation.cpp:ams_mel_ir_navigation_submit` | `NavigationReportResp` | `run_worker` / `complete` | image stream `requests` | `retain_worker` | stream stop/physical cleanup deferred | `Completion::result/kind/message` |
| InstrumentationLevelCmd | `ir_instrumentation.cpp:ams_mel_ir_instrumentation_submit_level` | `InstrumentationReport` | `run_worker` / `complete` | `ChannelState::requests` | `retain_worker` | disable/detach deferred | `Completion::result/kind/message` |
| TrackDataUpdate | `ir_track.cpp:ams_mel_ir_track_submit_update` | `CommandStatus` | shared `run_worker` / `complete` | `TrackState::requests` | `retain_worker` | Track disable/detach deferred | `Completion::status/error_code/reason_text/kind/message` |
| SystemTrackDataResponse | `ir_track.cpp:ams_mel_ir_track_submit_system_track_data_response` | `CommandStatus` | same shared worker | same Track requests | same | same | same |

There are six completion engines for seven request types: Track update and
system response share the Track engine. The Track `RequestSystemTrackData` and candidate callbacks are inbound
callback/queue events: **no** provider future, request handle, or completion
worker. Neither SchedulingChannel nor RF is part of this study.

## Deterministic method and evidence

### Corrected provider-safety boundary

The first cross-family assertion stopped this task on the observed trace:

```text
track_channel_destroyed
control_destroyed
manager_destroyed
library_unloaded
completion_owner_destroyed_5
```

That assertion required all captured WorkerInputs to die before library unload.
It was stronger than the actual safety requirement; this history is retained,
not treated as a demonstrated production defect or silently removed.

The pinned published common MEL header
`native/vendor/common-mel/include/mel/library/CommonMEL.h:125-126`
(common MEL revision `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`) defines:

```cpp
template <typename T>
using RequestFor = std::future<ErrorOr<std::shared_ptr<T>>>;
```

C++ `[futures.unique.future]` specifies that `get()` waits for and retrieves the
stored result, releases the shared state, and leaves `valid() == false`, including
when it rethrows a stored exception. An invalid future no longer refers to a
shared state. This reasoning applies to this published alias, **not** arbitrary
provider-defined future implementations. Invalidation occurs as part of `get`,
not after the caller's result conversion. The probe checks it after all catches,
when both the local outcome and any caught exception have left scope.

Source audit of the six engines:

| Engine | Provider outcome handling | Deferred cleanup | Completion graph reset |
|---|---|---|---|
| C2 Mode `complete` | try-local outcome; mode copied | `finish_channel` after handlers | `state->channel.reset()` |
| C2 Return `complete_return` | try-local outcome; Return copied | `finish_channel` after handlers | `state->channel.reset()` |
| C2 Comms `complete_comms` | try-local outcome; IDs copied | `finish_channel` after handlers | `state->channel.reset()` |
| Image Navigation `complete` | try-local outcome; response copied | `finish_stream` after handlers | `state->stream.reset()` |
| Instrumentation `complete` | try-local outcome; report copied | `finish_channel` after handlers | `state->channel.reset()` |
| shared Track `complete` | try-local outcome; status/text copied | `finish_request` after handlers | `state->channel.reset()` |

All six translate bad allocation, standard exceptions, and unknown exceptions;
none invokes deferred cleanup while `outcome` remains in scope. Each resets its
Completion graph under its mutex while a local channel/stream shared owner still
exists, then releases that local owner. That last release can unload the provider.
The worker subsequently returns and the detached thread releases captured input.
The remaining Completion has only bridge-owned mutex/CV, cached C values and
strings (Track's reason view points into its own string). WorkerInput has the
invalid future, that Completion, and launch/emergency bookkeeping. On healthy
launch, `emergency_self.reset()` precedes the release-store of `launch_state`;
worker acquire-load observes that reset. Probes check no self-cycle or emergency
retention at entry and at successful completion. Emergency failures are separate.

The revised C11 regression retains the exact 15 Return / 15 Mode / 14 Comms /
14 Navigation / 14 Instrumentation / 14 Update / 14 SystemResponse distribution.
It closes all public owners while pending and releases C2, Image+Instrumentation,
then Track (14+14) waves. Test-only CV barriers park workers **after** the outcome
and exception scopes and an explicit `!future.valid()` check, **before** pending
account decrement or cleanup. Mock state holds only weak result observers; at
each barrier all released results have expired while unreleased result owners
remain live. Gate closures move, rather than retain a second strong result owner.
The gate call returns before the weak observation check, also disposing of its
promise/closure storage while workers remain parked and provider code is loaded.

Every Completion reset is checked and logged before releasing the worker's local
graph owner. Final assertions require all 100 consumption, result-scope-exit,
and graph-reset events before unload, exactly-once physical teardown, zero active
workers, and equal constructed/destroyed WorkerInput counts. Each healthy input
records successful per-worker safety-mask validation before its final resource
destructor; all 100 `safe_owner_destroyed` events are required. A separate process
run injects stored exceptions into all 100 futures, covers every engine, and
requires 100 `get_threw` events instead of `get_returned`.

The final Track worker can record
graph release, unload, worker return boundary, then WorkerInput destruction.
This is safe for the audited healthy path because no provider-dependent object
remains in that input. `completion_owner_destroyed` is **resource-lifetime**
evidence, not the provider-library safety boundary. The worker return marker is
inside the function at its successful return boundary; neither it nor the owner
counter claims that OS thread retirement has finished.

The test facade exposes `ams_mel_test_completion_snapshot/wait` via its
generated test-only version script; six counters for each of the six worker
implementations observe start, active, peak, finish, get entry and get exit.
Production preprocessing elides the probes. The mock scenario `completion-scale`
returns actual provider-send `std::future` objects backed by held promises.
Its condition-variable submission gate and controlled FIFO release (waves are
supported) never sleep to establish concurrency. A C11 CTest translation unit
waits for all get entries before releasing; failure cleanup releases every
promise and waits/closes each surviving public owner. Counter completion means
the worker body has exited; OS thread retirement can lag slightly.

The C11 structural test runs 1, 10, 100 C2 KeepAlive requests on a channel,
then 100 with every public request owner closed before release and both Session
and C2 closed parent-first. While held, started/active/get entries equal N,
peak reaches N, get completions are unchanged. After release, completed and get
exits increase by N, active returns to zero, retained request results are cached
for repeated waits, and cleanup succeeds. Closed public owners cannot be waited
on, but their futures still each receive one get. A separate mixed test holds
34 Return, 33 Mode, 33 CommsTest on the same C2/Session; all 100 reach their
respective workers before release; counts remain isolated, with 100 get exits.
Existing C/Ada lifetime and Track mixed-update/response tests remain enabled.

The seven-type test supplies simultaneous cross-channel N=100 and physical
teardown evidence. The separate retained-handle modes
cover one request from each of the seven types (seven requests, six engines).
All futures are held until every worker enters get. Each handle times out at
Wait(0) and Wait(1), then succeeds after explicit release and returns the same
expected semantic fields on a cached Wait(0). Assertions check exact Return,
mode, Comms IDs, all Navigation response fields, all Instrumentation report
fields, and distinct Track command IDs/state/reason/text for each Track type.
The stored-exception mode requires AMS_MEL_PROVIDER_EXCEPTION on both waits
for all seven handles. After handle close, each engine's get-entry/get-exit
delta is exactly one (two for shared Track), active workers are zero, and all
seven WorkerInputs have been reclaimed. No struct-padding comparison is used.
The worker-completed probe fires at the end of `run_worker`; a detached thread's
final captured `shared_ptr` destruction and OS thread retirement can still be
in progress. Consequently the new regression does not prove that every
provider-library deleter has finished at the exact moment of that snapshot;
the added safety-boundary and final-owner probes supply the distinct evidence.

## Debug benchmark results

The separate C2-only table below uses five complete GCC Debug samples. The
seven-type mixed table follows separately; see the Release section for three
complete GCC Release samples. Each cell is minimum / median / maximum, not a
selected run. Reproduce from a test-enabled GCC tree using
`cmake -S native -B native/build-tests -DCMAKE_BUILD_TYPE=Debug -DAMS_MEL_BUILD_TESTS=ON`,
`cmake --build native/build-tests`, then five invocations each of
`native/build-tests/request_completion_scalability` and
`native/build-tests/request_completion_scalability mixed`. For Release, configure
an independent test tree with `-DCMAKE_BUILD_TYPE=Release`, build and run its
full `ctest --output-on-failure` first, then invoke each benchmark three times.
The executable is not a CTest. Numeric values never gate correctness.

### C2 N=1/10/100 structural results

All N pending workers entered `get()` and later exited once; active=N,
peak=N, completion=N, and no request remains active after cleanup.

### Seven-type N=100 structural result

15 Return + 15 Mode + 14 CommsTest + 14 NavigationReport + 14
InstrumentationLevelCmd + 14 TrackDataUpdate + 14 SystemTrackDataResponse =
100 submitted and pending. Active=100, peak at least 100, 100 `get()` exits,
100 Completion graph resets, and 100 eventually destroyed WorkerInputs in every
mixed sample. The C11 success and stored-exception tests also check these
boundaries independently with parent-first close and controlled waves.

### Request-handle early-close result

Closing pending public request handles does not cancel provider futures: all
100 workers finish exactly once in the seven-type controlled-wave regressions.

### Parent channel / Session early-close result

Closing all four parent channels and the Session while futures are held defers
physical teardown; channels precede Control, manager, and library destruction.

### Retained-handle cached-result result

Each of seven retained requests times out at Wait(0) and Wait(1), then returns
the exact expected payload after release and the same cached payload at Wait(0).

### Stored-exception result

All seven retained handles cache `AMS_MEL_PROVIDER_EXCEPTION`; the separate
N=100 run exercises 100 stored exceptions and 100 `get_threw` events.

### Provider lifetime boundary

The original suspicious trace and its stronger-than-necessary assertion are
explained above. Future invalidation, provider result destruction and Completion
graph release precede provider unload. Eventual WorkerInput destruction
is resource reclamation and can follow `library_unloaded`.

## Final timestamped measurement method

The first benchmark implementation held a direct `dlopen` reference to the
mock provider. That observation handle prevented the provider DSO from unloading
during the benchmark, so no library-unload timing is claimed from those runs.
Their former get/graph/input times were polling observations, not transition
timestamps; the tables below replace those samples, not retroactively timestamp them.

The executable calls `dlopen(..., RTLD_NOW)` once in main, and resolves only
`mock_completion_gate` with `dlsym`. There is no secondary handle, cached loader
owner, helper lookup or `RTLD_NOLOAD` call (which would not be reference-neutral).
In mixed mode the final direct provider operation releases all 100 promises.
Then the benchmark clears its gate pointer, successfully calls `dlclose`, clears
the handle, and records `benchmark_handle_closed` before parent teardown.
Its cleanup guard shares that cleared pointer and cannot call the mock afterward.
Only the actual bridge/provider graph can keep the mock loaded in this window.
C2-only mode retains its control handle across N=1/10/100 and claims no unload timing.

The existing mock `record(...)` event points feed an optional test-only external
file as well as the unchanged correctness log. The facade's existing probes use
the same helper. Each transition samples `clock_gettime(CLOCK_MONOTONIC)` in
integer nanoseconds and uses open/append/write/close, with no provider-owned
buffered stream or sink owner. Missing required evidence makes the measurement
unavailable. A unique `mkstemp` path is removed on success and printed/preserved
on failure; no repository artifacts or production logging facility are created.
`AMS_MEL_BENCH_DUMP_TIMELINE=1` prints all raw timestamped entries sorted by their
recorded timestamps (concurrent append order need not equal timestamp order).
The optional `AMS_MEL_TEST_TIMELINE` sink is absent from production compilation.

All pending is established by held futures and get-entry counters, not sleep.
After release and handle closure, the harness waits for consumed-future evidence,
closes request and parent owners, then waits for graph and final-owner evidence.
These waits are observations, not destructor timestamps or imposed destruction
ordering. The existing boundary release is harmless when no family is parked.
Failure cleanup releases held promises, unblocks all test boundaries, closes
owners, and waits against cumulative owners-created targets, not zero. Peaks
are absolute family peaks; one mixed wave per process makes their sum 100.

There is exactly one C2, Image (`channel_destroyed`, existing name),
Instrumentation and Track destruction. Both Track request types share the one
physical channel. All-channels is their maximum timestamp. All-gets, all-graphs,
and all-WorkerInputs are maxima of exactly 100 corresponding transition records;
the latter is the last-field destructor marker after the future and Completion,
not OS thread retirement. Control, manager and unload use their existing destructor
markers. Destructor markers record their existing point inside the destructor,
not the duration of the complete C++ destructor call or loader unmapping.
`library_unloaded` runs during DSO static destruction and is observed before
benchmark process exit. The first inspected Debug trace was nondecreasing after
sorting, with 100 of each worker event and exactly one of each physical event.

The parser checks that all futures were consumed, result scopes exited and
Completion graph releases were recorded before unload. It does **not** require
WorkerInput destruction before unload. Timing values, memory, and delta signs
are non-gating. Logging perturbs these test-facade measurements; no production
performance or real-provider claim follows.

## Final GCC Debug samples (5 per mode)

Full native CTest passed 20/20 before collection. Memory units are Linux kB.

### C2-only N=1

| metric | min / median / max |
|---|---:|
| `workers` | 1 / 1 / 1 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 2 / 2 / 2 |
| `rss_base_kb` | 5148 / 5340 / 5340 |
| `rss_held_kb` | 5808 / 5880 / 5952 |
| `vm_base_kb` | 9064 / 9064 / 9064 |
| `vm_held_kb` | 17372 / 17372 / 17372 |
| `submit_ms` | 0.029515 / 0.032141 / 0.043021 |
| `submit_per_request_ms` | 0.029515 / 0.032141 / 0.043021 |
| `drain_ms` | 0.076143 / 0.086743 / 0.112882 |
| `teardown_ms` | 0.011913 / 0.023665 / 0.029866 |
| `idle_100ms_cpu_ms` | 0.007 / 0.012 / 0.021 |
| `burst_cpu_ms` | 0.029 / 0.062 / 0.129 |

### C2-only N=10

| metric | min / median / max |
|---|---:|
| `workers` | 10 / 10 / 10 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 11 / 11 / 11 |
| `rss_base_kb` | 6048 / 6204 / 6268 |
| `rss_held_kb` | 6124 / 6280 / 6344 |
| `vm_base_kb` | 82908 / 82908 / 82908 |
| `vm_held_kb` | 156672 / 156672 / 156672 |
| `submit_ms` | 0.179537 / 0.27677 / 0.308019 |
| `submit_per_request_ms` | 0.0179537 / 0.027677 / 0.0308019 |
| `drain_ms` | 0.227809 / 0.282972 / 0.368072 |
| `teardown_ms` | 0.012664 / 0.017212 / 0.023485 |
| `idle_100ms_cpu_ms` | 0.008 / 0.013 / 0.014 |
| `burst_cpu_ms` | 0.939 / 1.05 / 1.578 |

### C2-only N=100

| metric | min / median / max |
|---|---:|
| `workers` | 100 / 100 / 100 |
| `threads_base` | 1 / 2 / 3 |
| `threads_held` | 101 / 101 / 101 |
| `rss_base_kb` | 6096 / 6256 / 6328 |
| `rss_held_kb` | 6948 / 7108 / 7180 |
| `vm_base_kb` | 435176 / 566248 / 705516 |
| `vm_held_kb` | 1.22199e+06 / 1.35306e+06 / 1.48414e+06 |
| `submit_ms` | 1.70196 / 1.92237 / 2.12716 |
| `submit_per_request_ms` | 0.0170196 / 0.0192237 / 0.0212716 |
| `drain_ms` | 0.499019 / 0.548512 / 0.554532 |
| `teardown_ms` | 0.009898 / 0.024596 / 0.040846 |
| `idle_100ms_cpu_ms` | 0.007 / 0.012 / 0.015 |
| `burst_cpu_ms` | 3.235 / 3.712 / 4.238 |

### Mixed seven-type N=100: resources and observation intervals

`*_ms` here are harness observation intervals, not physical transition times.

| metric | min / median / max |
|---|---:|
| `submitted` | 100 / 100 / 100 |
| `active` | 100 / 100 / 100 |
| `peak` | 100 / 100 / 100 |
| `gets` | 100 / 100 / 100 |
| `graphs` | 100 / 100 / 100 |
| `inputs` | 100 / 100 / 100 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 101 / 101 / 101 |
| `rss_base_kb` | 5260 / 5316 / 5408 |
| `rss_held_kb` | 6888 / 6904 / 6904 |
| `vm_base_kb` | 9072 / 9072 / 9072 |
| `vm_held_kb` | 828804 / 828804 / 828804 |
| `submit_ms` | 2.32212 / 2.46829 / 2.89555 |
| `pending_ms` | 2.33362 / 2.48052 / 2.90901 |
| `get_ms` | 1.20138 / 1.57314 / 2.51112 |
| `graph_after_get_ms` | 0.165451 / 0.216948 / 0.286489 |
| `graph_ms` | 1.36683 / 1.82638 / 2.79761 |
| `input_ms` | 1.36762 / 1.82722 / 2.79869 |
| `parents_close_ms` | 0.164249 / 0.215656 / 0.284876 |
| `terminal_ms` | 1.36762 / 1.82722 / 2.79869 |
| `duration_ms` | 4.10031 / 4.74162 / 6.21537 |

### Mixed actual transition deltas (microseconds)

| metric | min / median / max |
|---|---:|
| `release_to_gets_us` | 1184.28 / 1543.7 / 2236.88 |
| `release_to_graphs_us` | 1198.17 / 1571.56 / 2259.32 |
| `release_to_workers_us` | 1210.04 / 1585.61 / 2276.99 |
| `release_to_channels_us` | 1298.2 / 1712.56 / 2673.95 |
| `release_to_control_us` | 1303.78 / 1719.58 / 2691.24 |
| `release_to_manager_us` | 1309.48 / 1728.49 / 2700.09 |
| `release_to_unload_us` | 1315.44 / 1736.96 / 2707.93 |
| `gets_to_graphs_us` | 13.896 / 21.39 / 27.853 |
| `graphs_to_unload_us` | 117.271 / 163.617 / 448.613 |
| `unload_to_workers_us` | -430.94 / -149.861 / -104.556 |
| `channels_to_unload_us` | 17.243 / 19.256 / 33.983 |

### Representative last-sample monotonic timestamps (nanoseconds)

| event | timestamp |
|---|---:|
| `provider_release_begin_ns` | 1352561318502762 |
| `benchmark_handle_closed_ns` | 1352561318876675 |
| `parent_teardown_begin_ns` | 1352561320127246 |
| `c2_channel_destroyed_ns` | 1352561320149378 |
| `channel_destroyed_ns` | 1352561320169736 |
| `instrumentation_channel_destroyed_ns` | 1352561320194392 |
| `track_channel_destroyed_ns` | 1352561320215322 |
| `control_destroyed_ns` | 1352561320222345 |
| `manager_destroyed_ns` | 1352561320231252 |
| `library_unloaded_ns` | 1352561320239718 |
| `all_gets_ns` | 1352561320046464 |
| `all_graphs_ns` | 1352561320074317 |
| `all_workers_ns` | 1352561320088373 |

## Final GCC Release samples (3 per mode)

Full native CTest passed 20/20 before collection. Memory units are Linux kB.

### C2-only N=1

| metric | min / median / max |
|---|---:|
| `workers` | 1 / 1 / 1 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 2 / 2 / 2 |
| `rss_base_kb` | 4064 / 4064 / 4068 |
| `rss_held_kb` | 4512 / 4536 / 4536 |
| `vm_base_kb` | 7132 / 7132 / 7132 |
| `vm_held_kb` | 15440 / 15440 / 15440 |
| `submit_ms` | 0.028674 / 0.030207 / 0.033022 |
| `submit_per_request_ms` | 0.028674 / 0.030207 / 0.033022 |
| `drain_ms` | 0.050956 / 0.07421 / 0.076063 |
| `teardown_ms` | 0.011541 / 0.013455 / 0.01596 |
| `idle_100ms_cpu_ms` | 0.014 / 0.015 / 0.019 |
| `burst_cpu_ms` | 0.021 / 0.032 / 0.033 |

### C2-only N=10

| metric | min / median / max |
|---|---:|
| `workers` | 10 / 10 / 10 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 11 / 11 / 11 |
| `rss_base_kb` | 4840 / 4840 / 4908 |
| `rss_held_kb` | 4916 / 4916 / 4984 |
| `vm_base_kb` | 80976 / 80976 / 80976 |
| `vm_held_kb` | 154740 / 154740 / 154740 |
| `submit_ms` | 0.171272 / 0.210045 / 0.257915 |
| `submit_per_request_ms` | 0.0171272 / 0.0210045 / 0.0257915 |
| `drain_ms` | 0.221506 / 0.264748 / 0.295145 |
| `teardown_ms` | 0.004328 / 0.012313 / 0.014828 |
| `idle_100ms_cpu_ms` | 0.011 / 0.013 / 0.015 |
| `burst_cpu_ms` | 0.605 / 0.685 / 0.819 |

### C2-only N=100

| metric | min / median / max |
|---|---:|
| `workers` | 100 / 100 / 100 |
| `threads_base` | 1 / 1 / 5 |
| `threads_held` | 101 / 101 / 101 |
| `rss_base_kb` | 4904 / 4912 / 4960 |
| `rss_held_kb` | 5748 / 5756 / 5812 |
| `vm_base_kb` | 433244 / 515172 / 629852 |
| `vm_held_kb` | 1.22006e+06 / 1.2856e+06 / 1.41667e+06 |
| `submit_ms` | 1.62752 / 1.70975 / 2.00632 |
| `submit_per_request_ms` | 0.0162752 / 0.0170975 / 0.0200632 |
| `drain_ms` | 0.316124 / 0.360087 / 0.391506 |
| `teardown_ms` | 0.007234 / 0.012554 / 0.014567 |
| `idle_100ms_cpu_ms` | 0.014 / 0.015 / 0.016 |
| `burst_cpu_ms` | 2.984 / 3.384 / 3.755 |

### Mixed seven-type N=100: resources and observation intervals

`*_ms` here are harness observation intervals, not physical transition times.

| metric | min / median / max |
|---|---:|
| `submitted` | 100 / 100 / 100 |
| `active` | 100 / 100 / 100 |
| `peak` | 100 / 100 / 100 |
| `gets` | 100 / 100 / 100 |
| `graphs` | 100 / 100 / 100 |
| `inputs` | 100 / 100 / 100 |
| `threads_base` | 1 / 1 / 1 |
| `threads_held` | 101 / 101 / 101 |
| `rss_base_kb` | 4104 / 4120 / 4124 |
| `rss_held_kb` | 5588 / 5664 / 5684 |
| `vm_base_kb` | 7140 / 7140 / 7140 |
| `vm_held_kb` | 826872 / 826872 / 826872 |
| `submit_ms` | 2.19807 / 2.29376 / 2.49702 |
| `pending_ms` | 2.21505 / 2.30577 / 2.50667 |
| `get_ms` | 1.67199 / 1.6906 / 1.85401 |
| `graph_after_get_ms` | 0.153639 / 0.157366 / 0.170761 |
| `graph_ms` | 1.82936 / 1.84424 / 2.02477 |
| `input_ms` | 1.82966 / 1.84454 / 2.02514 |
| `parents_close_ms` | 0.153268 / 0.156885 / 0.17026 |
| `terminal_ms` | 1.82966 / 1.84454 / 2.02514 |
| `duration_ms` | 4.55605 / 4.61845 / 4.73007 |

### Mixed actual transition deltas (microseconds)

| metric | min / median / max |
|---|---:|
| `release_to_gets_us` | 1647.84 / 1670.2 / 1821.19 |
| `release_to_graphs_us` | 1668.72 / 1687.39 / 1850.18 |
| `release_to_workers_us` | 1680.86 / 1703.95 / 1865.39 |
| `release_to_channels_us` | 1773.04 / 1796.14 / 1964.78 |
| `release_to_control_us` | 1779.37 / 1802.2 / 1970.72 |
| `release_to_manager_us` | 1783.76 / 1806.85 / 1975.15 |
| `release_to_unload_us` | 1789.79 / 1813.05 / 1984.27 |
| `gets_to_graphs_us` | 17.192 / 20.88 / 28.984 |
| `graphs_to_unload_us` | 121.067 / 125.656 / 134.092 |
| `unload_to_workers_us` | -118.883 / -109.095 / -108.925 |
| `channels_to_unload_us` | 16.741 / 16.911 / 19.486 |

### Representative last-sample monotonic timestamps (nanoseconds)

| event | timestamp |
|---|---:|
| `provider_release_begin_ns` | 1352567886547407 |
| `benchmark_handle_closed_ns` | 1352567886767711 |
| `parent_teardown_begin_ns` | 1352567888246812 |
| `c2_channel_destroyed_ns` | 1352567888264566 |
| `channel_destroyed_ns` | 1352567888284483 |
| `instrumentation_channel_destroyed_ns` | 1352567888302166 |
| `track_channel_destroyed_ns` | 1352567888320451 |
| `control_destroyed_ns` | 1352567888326773 |
| `manager_destroyed_ns` | 1352567888331171 |
| `library_unloaded_ns` | 1352567888337192 |
| `all_gets_ns` | 1352567888195245 |
| `all_graphs_ns` | 1352567888216125 |
| `all_workers_ns` | 1352567888228267 |

## Interpretation and limitations

Every final mixed sample has 100 workers, 100 get exits, 100 graph releases,
100 WorkerInput reclamations, baseline 1 and pending 101 OS threads. The recorded
unload-to-WorkerInput delta retains its measured sign. The above final samples
have negative deltas: their final WorkerInput marker preceded unload. The C11
regression independently permits and has observed WorkerInput destruction later.
C2 drain includes public waits/close and is not equivalent to mixed get-only time.
Both show roughly one blocked native thread per held future. Virtual-address
growth is not physical stack consumption. Allocator/thread reservations can
persist across C2 sizes; each size has a fresh pre-Session baseline.
Only mock-provider/Linux/GCC behavior on this host is measured. No timing or
memory value is a correctness threshold. Temporary execution logs are local
working evidence, not durable repository evidence; methodology and aggregate
results above are the retained record and the executable reproduces the traces.

## Validation and audit

On the final local tree, GCC Debug `make test-native` and GCC Release
`cmake -S native -B <separate-release-tree> -DCMAKE_BUILD_TYPE=Release
-DAMS_MEL_BUILD_TESTS=ON`, `cmake --build <separate-release-tree>`,
`ctest --test-dir <separate-release-tree> --output-on-failure` each passed all
20 configured native tests. Sequential `make test-build-isolation`,
`make check-ada-format`, `alr -C ada build`, `alr -C ada/tests run`,
`make test-rust`, `make test-python` (52 tests), and `git diff --check` passed.
Direct `make check` **failed** after its native 20/20 phase: bare GNAT/GPRbuild
was unavailable on PATH; the independent Alire successes do not turn it into a
pass. Local `clang++` was unavailable; no local Clang matrix is claimed.
Production ABI remains 0.1 with exactly 90 `ams_mel_` dynamic exports and no
probe symbols/controls in the production shared library. The unchanged export
map, public C/Ada/Rust/Python paths, vendor bytes and provenance are checked
against the starting commit. No real-Squall benchmark was performed.

The stale-030B audit covered README, architecture, coverage, the original 030B
report and the corrective handoff report. Remaining release-slot/2N descriptions
are explicitly superseded history; current admission is single-executor
backpressure, not a bounded overlapping-generation pool. No historical claim
was retroactively rewritten as a successful design.

The local direct `make check` failure is an accepted environment limitation,
not a passing gate. The script itself identifies `alr -C ada/tests run` as the
alternative; Alire build/tests and Ada format checks pass separately. The
final-newline and both staged and unstaged whitespace checks are run directly
because the missing bare GPRbuild prevents `make check` reaching those stages.
Review readiness requires successful exact-head push and pull-request CI,
including hosted Ada's direct-GPR smoke test and formatting step. The PR records
those hosted results; this report does not substitute local Alire for direct GPR.

## Task 031B recommendation

Investigate bounded/shared completion designs: 100 held futures require 100
blocked threads and about 0.7–0.8 GiB additional virtual address space on
this host. A naive fixed pool whose workers block in `future.get()` can starve
later ready requests when earlier futures never resolve. The published
`std::future` gives no provider completion notification to a pool, so do not
assume an executor solves the problem without another wakeup/ownership design.
Preserve one get per future, result caching, timeout/Close non-cancellation,
parent/provider/library lifetime, synchronous completion, post-send failures,
deferred teardown, error mapping, bounded failure behavior, and no busy polling
or provider-thread Ada calls. For example, a pool of eight workers blocked on
eight never-completing futures cannot service request nine, even if it is ready.
Task 031B must evaluate bounded admission/backpressure, the readiness primitives
actually available on this std::future alias, non-busy wait_for scheduling if
semantically acceptable, bounded waiter/executor designs, possible upstream
interface evolution, and reduced per-thread resource cost as an alternative
to multiplexing. No final architecture is selected here.

The observed model is one pending RequestFor to one blocked bridge worker to
approximately one native OS thread. Significant virtual-address growth is
distinct from the modest observed RSS growth: it is consistent with per-thread
virtual resource reservation, not a direct measurement of stack consumption.
