# Task 019 validation — inherited C2 Channel services for Ada

Task 019 adds application-facing common services on the existing IR C2 channel:
KeepAlive through the existing Return request owner, typed asynchronous
ChannelCommsTest request/reply, explicit ChannelCommsTest callback registration
through Task 018's queue, and complete immutable ChannelCapability snapshots.
They work while Attached or Enabled.

Comms requests share `ChannelState.requests`: one future get, cached terminal
result, non-cancelling timeout/close, deferred cleanup, failpoint retention, and
parent-independent provider lifetime. Callback code never calls Ada; the existing
bounded DROP-INCOMING queue, counters, callback guard, retained state, and
channel-destruction quiescence boundary are reused.

The failed CommsTest registration case runs in an isolated child process. The
mock retains and invokes the callback despite returning failure, and the lifetime
log requires `retained_comms_callback_invoked` before `c2_channel_destroyed`
before `library_unloaded`. The retry remains `AMS_MEL_INVALID_ARGUMENT`, while
the existing Task 018 metadata owner remains independently and safely closeable.

Capabilities validate and deep-copy every field, enum, UTF-8 string, ID,
ordered vector, set iteration, map entry, nested BandInfo vector, and nav frame.
Native snapshots outlive provider teardown; Ada copies the complete graph and
closes the native owner before returning. Rich mock values prove full fidelity;
pinned Squall's sparse defaults are separate provider observations.

ABI 0.1 has exactly 36 exports. Rust sys and private Python ctypes plus drift
probes are synchronized; safe Rust and public Python APIs are unchanged.
Explicit `Channel::registerBuffer`/`unregisterBuffer`, Scheduling, optional C2
commands, and other MEL families are excluded.

## Validation results

- `make test-native`: 6/6 passed.
- `alr -C ada build` and `alr -C ada/tests run`: passed, including
  `PASS: Ada common IR C2 Channel contract`.
- `make test-rust`: 31 existing safe tests, raw ABI drift test, and doc tests passed.
- `make test-python`: 52 tests passed with the 36-function private ABI.
- `ctest --test-dir native/build --repeat until-fail:50 --output-on-failure`:
  all six targets passed 50 consecutive executions in 62.05 seconds.
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check`: native,
  Ada, whitespace, and newline gates passed. Plain `make check` passed native
  6/6 then stopped because GNAT/GPRbuild is absent from ordinary `PATH`.
- `nm` reports exactly 36 `AMS_MEL_0.1` exports.

## Real Squall evidence

The Ada-only run passed on the default 21203/21318/21315/21316 ports. The first
final all-language attempt then correctly refused occupied port 21315; the rerun
passed with explicit control/Couloir-metrics/optical-health/optical-metrics ports
39541/42065/55585/46051. Runtime was podman 5.4.2. The extracted provider was
`libsquall_ir_mel.so`, built with GCC 14.3.1; it reported API 1, library 1,
vendor `Squall`, description `Squall Simulator IR MEL`.

Before Enable, Ada observed KeepAlive Success with cached wait; CommsTest reply
and callback both preserved command 4200705 (`0x00401901`) and request
2151684354 (`0x80401902`). C2 capability contained exactly CommandAndControl and
four metadata capabilities (BITConfiguration, CommandStatus, BITStatus,
ChannelCommsTestRep); TaskScheduleDepth was 0 and ODC/NUC were false.

Task 017/018 behavior remained: general mode acceptance/rejection, ConfigSet,
payload/no-op BIT, correlated CommandStatus, empty default BIT metadata, and
three 320x200 Mono8 frames. Ada metadata counters were 11 received, 0 dropped,
0 malformed; frame counters were 3/0/0. C, Ada, Rust, and Python each printed
PASS, followed by the combined all-language PASS line.
