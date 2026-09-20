# Task 029B3 Track pinned-Squall validation

Task 029B3 validates the already-implemented IR Track `@RequiredIfTrack` core
against the pinned real Squall provider. It implements no Track functionality,
adds no C ABI entry point, and changes no vendored content. It adds exactly a C
expected-negative Track Open probe, an Ada expected-negative Track Open probe,
same-Session recovery evidence, and documentation.

## Pins

| Item | Value |
|---|---|
| Starting bridge SHA (`main`) | `e88b4476b7da92eacde97119d93bd45102fe29ac` |
| Branch | `task/029b3-track-squall-validation` |
| Pinned Squall | `b1015728f904c799fa0c07489fce48e78f67845f` |
| ABI version | 0.1 |
| `exports.map` / dynamic exports | 82 / 82 |
| Rust raw functions / Python bound functions | 82 / 82 |
| Native CTest targets | 12 (12/12 passed) |
| Vendor delta | 0 (`native/vendor/*` and `docs/upstream-files.sha256.md` unchanged) |

The Squall working tree was clean and at the exact pinned commit both before
and after validation. Squall was not modified, and no generated Squall artifact
is committed to the bridge.

## Authoritative pinned provider behavior

`interfaces/squall-ir-mel-impl/src/SquallIRLoaders.cc`,
`SquallControl::attachChannel(const Config&)` supports exactly:

- `ChannelType::IRSTImage` -> `SquallImageChannel`
- `ChannelType::CommandAndControl` -> `SquallC2Channel`
- `ChannelType::HealthAndStatus` -> `SquallHealthStatusChannel`

`ChannelType::IRSTTrack` matches no case, reaches `default:`, and returns
`nullptr`. The facade maps that to `AMS_MEL_FACTORY_FAILED` with the existing
diagnostic `attachChannel returned null`.

`interfaces/squall-ir-mel-impl/src/SquallIRFactory.cc` exports
`createTrackChannel(Config&)` as a hard `return nullptr;`. It is not a positive
fallback, and the bridge architecture was not changed to call it: the bridge
remains `Control::attachChannel` based.

## Expected-negative results (real Squall)

C, `integration/squall/squall_ir_c_smoke.c`, using
`ams_mel_ir_track_config_v1` with `AMS_MEL_IR_CHANNEL_IRST_TRACK` and the
existing channel/platform/location helpers (label `Task 029B3 IRSTTrack`):

```text
Track: pinned Squall unsupported as expected (AMS_MEL_FACTORY_FAILED: attachChannel returned null)
```

The C probe requires all three of `status == AMS_MEL_FACTORY_FAILED`,
`rejected == NULL`, and a diagnostic string equal to exactly
`attachChannel returned null`; any other `FACTORY_FAILED` diagnostic fails the
run. An unexpectedly escaping owner is closed during failure cleanup.

Ada, `integration/squall/ams_mel_squall_ir.adb`, using
`Track.Create_Config (Channel_ID, Platform_ID, Location)`:

```text
Track: pinned Squall unsupported as expected (Provider_Error: attachChannel returned null)
```

An unexpected success closes the channel and raises `Program_Error`. The only
accepted exception is `AMS.MEL.Provider_Error` whose
`Ada.Exceptions.Exception_Message` equals exactly
`attachChannel returned null`.

Because the channel cannot be attached, neither client attempts `Enable`,
`Track.Metadata.Open`, or `IRSTTrackReport` reception against real Squall; doing
so would fabricate a positive-provider expectation the pinned provider does not
support.

## Same-Session recovery

Neither client closes or reopens the Session after the Track failure. The Ada
probe runs while the original `Parent` Session is open, immediately after the
retained Instrumentation probe and before the positive Session-health portions.

C continuation on the same Session after the failed Track Open:

```text
C2 result: TASK_SCHED
BIT result: SUCCESS
frame 1..3: 320x200, 64000 bytes, monotonic frame IDs
counters: received>=3 dropped=0 malformed=0
PASS: real Squall IR C integration
```

Ada continuation on the same Session after the failed Track Open:

```text
Image capability: 320x200 bit-depth=8 bands=1 MONO, all four required metadata types
Image BadPixelList: reported-size=0 reported-count=0 pixels=0
Image NavigationReportResp callback / future / cached Wait(0) all match
Health callbacks: all six registrations succeeded
Health capability, BIT_Configuration, BIT_Status, MFA_Status, SubsystemStatusResp,
DiscreteStatus, MFA_StatusDetailed
C2 capability, KeepAlive SUCCESS, CommsTest response/callback
general mode STANDBY/UNUSED, scan REJECTED/INVALID_PARAMETERS,
ConfigSet empty SUCCESS / payload FAIL, BIT payload FAIL, BIT SUCCESS, TASK_SCHED
frames 1..3 320x200 Mono8, full FrameHeader, LOS Report/Euler consistency
counters received=4 dropped=0 malformed=0
metadata counters received=11 dropped=0 malformed=0
Health metadata counters received=7 dropped=0 malformed=0
PASS: real Squall IR Ada integration
```

This is the proof that a failed conditional-channel attach does not poison the
provider graph:

```text
failed Track attach attempt -> same Session -> established positive flow still succeeds
```

The Instrumentation expected-negative probe is retained unchanged in both
clients. Rust and Python were not changed and remain regression-only during the
all-language run; no safe Rust Track API and no public Python Track API exist.

## Real Squall ports used

Explicit fresh four-port groups, each verified unused before startup. No
unrelated listener was killed or replaced.

| Run | control | couloir metrics | optical health | optical metrics |
|---|---|---|---|---|
| Ada only | 22401 | 22402 | 22403 | 22404 |
| C only | 22411 | 22412 | 22413 | 22414 |
| All languages | 22421 | 22422 | 22423 | 22424 |

The all-language run required C PASS, Ada PASS, Rust PASS, Python PASS, and a
combined PASS. C and Ada exercise the Track expected-negative path; Rust and
Python remain unchanged regression clients.

## Evidence boundary

```text
Mock provider validates positive @RequiredIfTrack behavior and complete
IRSTTrackReport payload fidelity.

Pinned Squall validates clean unsupported-provider behavior only.

Pinned Squall does NOT provide positive Track execution or Track-report
evidence.
```

`TrackDataUpdate`, `SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
unimplemented; Track is not complete.

## Scope

Functional changes are limited to `integration/squall/squall_ir_c_smoke.c`,
`integration/squall/ams_mel_squall_ir.adb`, and
`integration/squall/README.md`, plus documentation (`CHANGELOG.md`,
`README.md`, `docs/coverage.md`, `docs/upstream-provenance.md`, and this file).
There are no changes to `native/include/ams_mel/abi.h`,
`native/src/ir_track.cpp`, `native/tests/mock_provider.cpp`, the Ada Track
implementation packages, the Rust bindings, the Python bindings,
`native/vendor/*`, or `native/src/exports.map`. The bridge working tree is
otherwise clean and `git diff --check` reports no whitespace errors.
