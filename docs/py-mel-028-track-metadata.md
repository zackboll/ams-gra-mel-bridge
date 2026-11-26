# PY-MEL-028 — Safe Python IR Track metadata

Python now receives all four published `TrackChannel::registerMetadataCallback`
payloads through one explicitly opened, uniquely owned `TrackMetadataStream`.
The API is available from `ams_mel.ir` and from `ams_mel` with identical class
identity. Owned values are frozen dataclasses or enums, with tuples for ordered
collections, and use the canonical `ams_mel` pickle paths. This is a Python
coverage change only: no change to the native C ABI or the Ada coverage inventory.

## Registration, queue and lifetime

`track.open_metadata(queue_capacity=16)` works while Attached or Enabled. It
does not enable Track. Capacity must be a positive `size_t`; Python validates
input and prepares owner, output and diagnostic storage before marking the
attempt. Once the native call is attempted (even if it fails or invocation is
uncertain), no second attempt is allowed on that Track owner, even after Close.
Native Open owns registration and rollback. Python does not query capabilities
for registration decisions or expose a separate provider `Return` result.

| Native registration order | Policy | `TrackMetadataKind` value |
| --- | --- | --- |
| `IRSTTrackReport` | Required for Track; anything other than Success fails Open. | `IRST_TRACK_REPORT` = 1 |
| `CandidateObjectMessage` | Only if advertised when Track was opened; if advertised, anything other than Success fails Open. | `CANDIDATE_OBJECT_MESSAGE` = 3 |
| `RequestSystemTrackData` | Optional; NotSupported is nonfatal. | `REQUEST_SYSTEM_TRACK_DATA` = 2 |
| `CandidateObjectPreProcMessage` | Optional; NotSupported is nonfatal. | `CANDIDATE_OBJECT_PREPROC_MESSAGE` = 4 |

Conflicts, unexpected return values and exceptions during optional registration
still fail Open. Callbacks may arrive during registration. There is one native
bounded FIFO in successful-enqueue order, not a queue per kind or a Python
worker. DROP-INCOMING preserves the earliest already-enqueued events across
kinds. `receive(0)` polls; positive timeouts are finite uint32 milliseconds.
The queue drains before terminal status: when empty, the native statuses are
`TIMEOUT` (active), `STREAM_STOPPED` (inactive/stopped), or `PROVIDER_FAILED`
(failed). Timeout does not cancel registration.

`counters()` snapshots `events_received`, `events_dropped_queue_full` and
`malformed_or_unsupported`, preserving native saturating uint64 values.
Receive never resets them. Callback entry can increment the received counter
even when inactive; the three values do not imply a partition of deliveries.

`receive()` acquires one initially null event owner and requests the existing
`ams_mel_ir_track_metadata_event_view_v3`. It checks the complete v3 fixed
extent before any dereference, then reads `base.base.kind` and validates and
copies only the selected record (v1 report/request, v2 candidate, v3 PreProc).
Unknown tags, unknown enums, non-0/1 booleans and malformed spans fail closed
with `PROTOCOL_INCONSISTENCY`. Span checks cover null/nonzero combinations,
alignment and representable sizes and address ranges, not the provenance or
mapped-memory validity of structurally valid addresses. Empty spans do not
need a data pointer. Only after event release succeeds is the completed
`TrackMetadataEvent` returned; no borrowed object reaches the caller.

The stream retains neither its Python parents nor the provider graph. Track
Close stops admission, but the still-open stream can drain its snapshots even
after the provider unloads. Stream Close does not close Track, unregister
callbacks or establish callback quiescence. Serialize operations on the same
stream, and registration against other Track operations; the GIL does not
provide that guarantee. Use explicit Close or a context manager. Finalization
is best effort and never raises. Cleanup preserves a primary exception,
including an exception from the context body. An allocation failure before a
stream Close call leaves it retryable; once native event or stream Close is
invoked, ownership that may have been consumed is never reused or closed
again. This is **not** the documented exception for `TrackChannel.close()`:
an explicitly retained typed-channel detach failure can be retried by an
explicit typed-channel Close, not by finalization or metadata Close.

## Field mapping

Field names below map the pinned getters, not inferred semantics. Signed
nanoseconds are copied as counts; no epoch is inferred for system timestamps.
All numerical components pass through, including signed zero and nonfinite
IEEE values; NaN payload-bit identity is not promised. No normalization,
bitfield decoding, unit conversion, tracking or estimation is done.

| Upstream `IRSTTrackReport` getters | Python fields | Representation / published units |
| --- | --- | --- |
| `getSystemTime`, `getActivityId` | `system_time_ns`, `activity_id` | Signed ns, uint32 |
| `getMeasuredNed`, `getMeasuredIntensity`, `getMeasuredSnr` | `measured_ned`, `measured_intensity`, `measured_snr` | Independent `NorthEastDown` and two doubles |
| `getFilteredNed`, `getFilteredIntensity`, `getFilteredSnr` | `filtered_ned`, `filtered_intensity`, `filtered_snr` | Independent `NorthEastDown` and two doubles |
| `getRange`, `getRangeError`, `getSpatialExtent` | `range_m`, `range_error_m`, `spatial_extent_rad` | Metres, metres, radians |
| `getTrackQuality`, `getClutter` | `track_quality`, `clutter` | Doubles; no quality clamp |
| `getAge` | `age_ns` | Signed ns since first detection |
| `getState`, `getMode` | `state`, `mode` | State: IDLE/DETECTED/COAST/DROPPED = 0–3; mode: IDLE/SCAN/STARE = 0–2; no sentinels |

`RequestSystemTrackData` maps `getSystemTime` to `system_time_ns` (signed ns),
and `getCommandID`, `getRequestId`, `getTrackId` to independent uint32-valued
`command_id`, `request_id`, `track_id`. This is inbound data, not a request
owner or an implicit association with the C2 `SystemTrackDataResponse`.

| Shared candidate getters | Python fields | Representation / published units |
| --- | --- | --- |
| `getNumberOfCOs`, `getStackFrameIndex`, `getValidityFlagBitField` | `header.number_of_cos`, `header.stack_frame_index`, `header.validity_flag_bitfield` | uint16; frame index is zero-based; bitfield is opaque |
| `getCFAR`, `getTOVutcNanoseconds` | `header.cfar`, `header.tov_utc_ns` | Exact received binary32 average false alarm rate (FA/pixel), signed UTC ns |
| `getHotRegions` | `hot_regions` | Full independent header vector, preserved in order with repeats |
| `getType`, `getSize`, `getTop`, `getLeft`, `getRight`, `getBottom` on each region | `kind`, `size`, `top`, `left`, `right`, `bottom` | INVALID/FLARE/SOLAR/MASK = 0–3; uint16 pixel count and zero-based pixel positions |
| `getSystemTime`, `getQ_xyzw`, `getQECEF_xyzw` on inertial state | `system_time_ns`, `q_xyzw`, `q_ecef_xyzw` | Signed ns; XYZW quaternions from sensor to local NED and NED to ECEF respectively |
| `getSensorPosition`, `getSensorVelocity` | `sensor_position`, `sensor_velocity` | XYZ in ECEF, metres and metres/second |
| `getUncertainties` | `uncertainties` | `sensor_uncertainties` and `platform_uncertainties` from `getSensorUncertainties` and `getPlatformUncertainties`, independent uint32 |

`CandidateObjectMessage` maps `getHeader`, `getInertialState`,
`getCandidateObjects` to `header`, `inertial_state`, `candidate_objects`.
The last is the meaningful prefix of a fixed array with 900 entries. The ABI
span length must equal `number_of_cos`, which cannot exceed 900; unused
entries are never read. `CandidateObjectPreProcMessage` maps
`getCandidateObjectHeader`, `getSensorInertialState`, `getCandidateObjectPreProcs`
to `header`, `inertial_state`, `candidate_object_preprocs`. The last is an
independent vector: neither the header count nor 900 limits it. The rich mock
fixture keeps both its reported count of three and its two actual entries.
Neither message caps the independent hot-region vector.

| Entry getters | Python fields | Representation / published units |
| --- | --- | --- |
| `getSystemTime`, `getDetectionCategory`, `getSensorIndex` | `system_time_ns`, `detection_category`, `sensor_index` | Signed ns time of validity, opaque uint32 bitfield, uint32 index |
| `getSubpixel` | `subpixel` | Canonical `RowCol`, row then column |
| `getIntensity`, `getSenRelUnit` | `intensity`, `sensor_relative_unit` | Merged intensity, unnormalized-as-received sensor-relative XYZ unit vector |
| `getSignalToInterferenceRatio`, `getSignalToNoiseRatio` | `signal_to_interference_ratio`, `signal_to_noise_ratio` | SIR and SNR as published, no inferred dB conversion |
| PreProc `getCandidateObjectWithBackground` | `candidate_object_with_background` | Nine signed int16 intensity samples, 3 × 3 in row-major order |
| PreProc `getClutter`, `getCandidateObjectQuality`, `getSirDelta` | `clutter`, `candidate_object_quality`, `sir_delta` | Doubles including values outside the descriptive quality range |
| PreProc `getInertialState`, `getEdge` | `inertial_state`, `edge` | Complete per-entry state independent of the message-level state; exact bool |
| PreProc `getAzSigma`, `getElSigma`, `getBackgroundNormalizer` | `az_sigma`, `el_sigma`, `background_normalizer` | Azimuth and elevation measurement uncertainties and background noise statistic used for SIR; no inferred angle unit |

## Runnable mock-only example

Build with `make test-native`. Save the example as a script and run it from the
checkout root with `PYTHONPATH=python` and
`AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"`.

```python
from pathlib import Path

from ams_mel import ComponentLocation, UciId
from ams_mel.ir import (
    CandidateObjectMessage, CandidateObjectPreProcMessage, IRSTTrackReport,
    RequestSystemTrackData, Session, TrackConfig,
)

provider = Path("native/build-tests/test-providers/libmock_ir_provider.so").resolve()
config = TrackConfig.new(
    UciId(bytes(range(16)), "Track"),
    UciId(bytes(range(0xF0, 0x100)), "Platform"),
    ComponentLocation(1.25, -2.5, 3.75, "station-1", "mock-aircraft"),
)
with Session.open(provider, "track-preproc-mixed", "") as session:
    with session.open_track_channel(config) as track:
        with track.open_metadata(queue_capacity=8) as metadata:
            track.enable()
            copied = tuple(metadata.receive(timeout_ms=1000) for _ in range(4))

assert tuple(type(event.payload) for event in copied) == (
    IRSTTrackReport, CandidateObjectMessage, RequestSystemTrackData,
    CandidateObjectPreProcMessage,
)
assert copied[2].payload.track_id == 0xE3456789
assert copied[3].payload.header.number_of_cos == 3
assert len(copied[3].payload.candidate_object_preprocs) == 2
for event in copied:
    print(event.kind.name)
```

## Scope and qualification

Positive functional and lifetime qualification uses only the compiled mock.
No real-provider, Squall or hardware qualification is claimed. Python Track
sends (`TrackDataUpdate` and the Track overload of `SystemTrackDataResponse`)
remain unimplemented. No automatic response, inherited callback expansion,
sensor alignment, or tracking algorithm is added.

Preflight fetched base `c882c474fe162615c2e65e6f5864c7a75a9264dd`, tree
`88e1f3e20f80109f69b6284729a2b6580e4cd2d4`, after PY-MEL-027 (PR #264).
There was no overlapping task branch, worktree or PR. Intervening main
`e32457e1db65f92914bcee67bfbe37cb1282ce57`, tree
`ddbc3f3900aca6b2ee2bdebecc231f94ff11e0c6` (Rust Scheduling StareTask,
PR #262) was incorporated by a normal Git merge.

The Python suite checks full mock payload equality and pickle round trips,
single-queue capacity 8/1/2/3 behavior, optional registration, hostile FFI
outputs and one-time cleanup. Subprocesses use native lifetime logs and
callback barriers to establish actual provider teardown, with independent
views of retained stream and event lifetimes. The existing native contract
tests also exercise the unchanged v1, v2 and v3 layouts and registrations.

| Local command | Exit status and observed result |
| --- | --- |
| `make native` | 0; production build succeeded. |
| `make test-native` | 0; 1,026 passed. |
| `make test-python` | 0; 1,026 native and 928 Python tests passed. |
| `make test-rust` | 0; 1,026 native and 736 Rust tests passed, with 16 Rust tests ignored. |
| `make test-policy` | 0; 333 unittest cases passed, including the documentation suite. |
| `make test-docs-ci` | 0; 129 passed. |
| `make check-ir-api-coverage` | 0; the coverage checker and 90 controls passed. |

The separate production build from incorporated main and the feature build
have identical `nm -D --defined-only` inventories (356 `AMS_MEL_0.1`
exports). The example printed all four kinds in FIFO order. Generated Python
reference HTML and inventory contain all 18 new root/IR types, `open_metadata`,
and the stream lifecycle, receive and counter operations. Explicit runtime
reference qualification against the production ABI passed.

The PR qualification ledger records the final commit and tree, focused and
aggregate check results, and hosted workflow conclusions against that exact
head without requiring a new source commit after hosted validation.
