# PY-MEL-027 — Safe Python IR Track channel foundation

## Contract and scope

This release adds **Track lifecycle only**. It does not automatically perform
tracking, register Track callbacks, or submit track data. The same
`TrackConfig` and `TrackChannel` classes are exported from `ams_mel` and
`ams_mel.ir`; the generic `ChannelCapability` and `ChannelView` are reused.

The pinned IR MEL `TrackChannel` is a conditionally required (`@RequiredIfTrack`)
virtual `Channel` subtype. See the [pinned IR API inventory](reference/irmel-upstream-api.json),
[Track ABI contract](c-abi-policy.md#task-029b1-track-channel-foundation-contract),
and [`native/src/ir_track.cpp`](../native/src/ir_track.cpp). The binding uses the
existing `ams_mel_ir_track_open`, `ams_mel_ir_track_enable`,
`ams_mel_ir_track_get_capabilities`, `ams_mel_ir_track_close`, and
`ams_mel_ir_channel_from_track` exports and their existing private ctypes
declarations. No native exports, structures, signatures, or provider code change.

- `TrackConfig(channel_id, platform_id, sensor_location)` and `TrackConfig.new(...)`
  create a frozen, pickleable value without provider work. Both canonical `UciId`s
  preserve all 16 UUID bytes. Labels, location key and system name must be valid
  UTF-8 without NUL. The three metre offsets use native doubles, without unit
  conversion, clamping or normalization. Numeric overflow is rejected; signed
  zero, infinities and NaN follow the existing double conversion policy. Open
  captures and validates the input fields again, and encodes only the validated
  copies. The channel type is always `IRSTTrack` (0).
- `Session.open_track_channel(config)` requires an open Session and returns an
  **Attached**, not Enabled, owner. Native calls `attachChannel` once and
  verifies a non-null concrete `TrackChannel` whose capability lists `IRSTTrack`.
  There is no separate Python or native Control discovery gate for this export.
  Unsupported/null attachment returns `FACTORY_FAILED`. Wrong concrete type or
  a missing/throwing capability reports `INITIALIZATION_FAILED` if rollback
  succeeds. Failed rollback reports `PROVIDER_FAILED` and native retains the
  provider graph; Python does not reattach or retry the rollback.
- `TrackChannel.enable()` is explicit. Native Enable is idempotent once Enabled.
  Refusal and provider exceptions are distinct `MelError` categories and leave
  a failed lifecycle, not a channel that can recover by retrying Enable.
- `TrackChannel.capabilities()` works while Attached or Enabled. Its returned
  snapshot owns every field, including ordered metadata, nested bands, UUIDs and
  strings, and remains valid after native snapshot Close, Track Close, Session
  Close and provider unload. Advertising `IRSTTrackReport` is not a callback
  registration or an indication of completed tracking.
- `TrackChannel.channel_view()` returns a weak common `ChannelView`. It retains
  neither Python parent nor native Track state or provider library. Closing the
  view has no effect on Track. Track can close with views still present; an
  expired view can report `is_open` locally but native operations on its target
  fail. Existing inherited operations and their provider support/admission
  requirements are unchanged. Close does not cancel admitted common requests;
  they retain their own native claims until completion.

The unique `TrackChannel` is obtained through `Session`, not constructed
directly, copied, or pickled. The native owner retains its provider graph even
after the public Session closes or is collected. It is independent of C2 and
does not redirect `ControlChannel.submit_system_track_data_response`. Callers
must serialize operations, including Close, on the same Track owner; the GIL
does not provide that guarantee.

## Close, context management and finalization

`track.is_open` is a local ownership check, not an Enabled/health check. Use
`track.close()` or a `with` block. If Enable was never attempted, native does not
call the provider's Disable. Otherwise native attempts Disable, then detach.

| Native Close result | Python behavior |
| --- | --- |
| Success, owner consumed | `is_open` becomes false; later Close is a no-op. |
| `PROVIDER_FAILED`: `Track disable failed`, owner consumed | Raises `MelError`; `is_open` is false and no further native Close occurs. |
| `PROVIDER_FAILED`: `Track detach failed; provider state retained`, unchanged non-null owner | Raises `MelError` and makes `is_open` false. Only an explicit later `track.close()` may retry. Enable, capabilities and view conversion are refused. |
| Unexpected or ambiguous ownership result | Raises the failure and disarms the owner; Python will not attempt another native Close. |

A Python preparation failure before native invocation leaves Close retryable.
Context exit preserves an exception from the body, and finalization is
non-raising and best effort. Neither implicitly retries retained or uncertain
cleanup. If a live object is finalized and detach fails, resources may remain
retained; use explicit Close to observe the failure and keep the option to retry.

## Runnable mock-only example

Build with `make test-native` from the checkout root. Save the example as a
script and run it from that root with `PYTHONPATH=python` and
`AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"`.

```python
from pathlib import Path

from ams_mel import ComponentLocation, UciId
from ams_mel.ir import ChannelType, Session, TrackConfig

mock_provider = Path("native/build-tests/test-providers/libmock_ir_provider.so").resolve()
track_config = TrackConfig.new(
    UciId(bytes(range(16)), "IR track channel"),
    UciId(bytes(range(0xF0, 0x100)), "test platform"),
    ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
)

with Session.open(mock_provider, "track-config", "") as session:
    with session.open_track_channel(track_config) as track:
        before = track.capabilities()
        track.enable()
        after = track.capabilities()

# Both snapshots remain fully owned Python values.
assert before == after
assert before.channel_types == (ChannelType.IRST_TRACK,)
```

## Remaining Python Track parity gaps

Track-specific reception of `IRSTTrackReport`, `RequestSystemTrackData`,
`CandidateObjectMessage`, and `CandidateObjectPreProcMessage`, and Track sends
of `TrackDataUpdate` and `SystemTrackDataResponse`, remain out of scope. There
is no standalone Disable, acquisition geometry, image buffer, or queue
configuration in this foundation. Positive qualification is limited to the
compiled mock; the pinned Squall has no successful Track attachment path.

## Qualification

The task worktree started from freshly fetched main commit
`eab6a8004069933678357adc340a2066eb7f3518`, tree
`1248d4a9962e465e1c057e29deb3b9ff5a1327a1`. No overlapping Python Track branch,
worktree or open PR was found. The earlier native Track foundation is already
in main and is reused as-is.

The compiled mock tests verify complete input delivery (`track-config`) and
all 19 capability fields before and after Enable, including `channel-α` and
the ordered metadata capabilities. Lifetime checks use fresh processes and
the mock event log, not sleeps. They verify that no Track callback or send is
invoked and that a lingering weak view does not prevent provider unload. FFI
fault injection exercises allocation failures, malformed records and
diagnostics, unexpected publication, and uncertain Close consumption.

The production facade and a separate fresh build from fetched `origin/main`
have identical inventories of 350 `AMS_MEL_0.1` exports (`nm -D --defined-only`).
A Python public API snapshot comparison against the same main found only the
two Track exports and `Session.open_track_channel` added. The generated
Python API reference was runtime-qualified and contains both Track classes,
the Session factory, and `enable`, `capabilities`, `channel_view`, and `close`.

| Local command | Observed result |
| --- | --- |
| `make native` | Production build passed. |
| `make test-native` | 1,009 passed. |
| `make test-python` | 905 Python tests passed (including 19 Track foundation tests). |
| `make test-rust` | 710 passed, 16 ignored. |
| `make test-policy` | 333 passed, including the docs CI tests. |
| `make test-docs-ci` | 129 passed. |
| `make check-ir-api-coverage` | Pinned coverage audit and 87 regression tests passed. |
| `make check` | Build-tree isolation, native and Ada checks passed. |
| `python3 scripts/check_doc_source_integrity.py`, `python3 scripts/check_final_newlines.py` | Passed. |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | Passed. |
| `git diff origin/main --check`, `git diff --check` | Passed. |

The focused `python3 -W error -m unittest -v` run with `test_ir_track_foundation`,
`test_instrumentation`, `test_ir_health`, `test_ir_stacked_foundation`,
`test_common_channel`, `test_ir_c2_system_track_response`, `test_abi` and
`test_public_modules` passed 279 tests. The mock example above also ran
successfully against the contract-test facade.
