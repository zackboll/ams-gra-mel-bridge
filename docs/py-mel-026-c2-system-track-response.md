# PY-MEL-026 — Safe Python C2 SystemTrackDataResponse

## Coverage and authority

Adds safe Python submission of the optional IR MEL
`C2Channel::send(SystemTrackDataResponse)` overload. There is no Python Track
overload, metadata registration, implicit enable, or native ABI change. The
three existing C2 submit/Wait/Close exports use a C2 request owner and C2 pending
accounting distinct from the Track equivalents. See the existing native mixed
test and [IRMEL-ADA-008](irmel-ada-008-c2-system-track-response.md).

The authoritative definitions are in the pinned IR MEL
`8d9224519f12b44e0b28815755c56a32a28d24a0` (`C2Channel.h`, `TrackChannel.h`,
`SystemTrackDataResponse.h`, `CommandStatus.h`, and `CommonIR_MEL.h`) and
AMS Math `00be45190f0e47d268cece8b8c2f8fb58b5418d2` (`AzEl`). See
[upstream provenance](upstream-provenance.md) and the
[pinned IR API inventory](reference/irmel-upstream-api.json).

## Input and submission

`SystemTrackDataResponse` is a frozen value with twelve required fields:

| Field | Representation and units |
| --- | --- |
| `system_time_ns` | Signed 64-bit nanoseconds; no epoch or clock inferred |
| `command_id` | Unsigned 32-bit command identifier |
| `request_id` | Unsigned 32-bit request identifier |
| `track_id` | Unsigned 32-bit track identifier |
| `range_m` | Double, meters |
| `range_rate_mps` | Double, meters per second |
| `range_error_m` | Double, meters |
| `range_rate_error_mps` | Double, meters per second |
| `az_el_valid` | Python `bool`, encoded as 0 or 1 |
| `range_valid` | Python `bool`, encoded as 0 or 1 |
| `inertial_az_el` | `AzimuthElevation`: `azimuth_rad` and `elevation_rad`, both doubles in radians |
| `az_el_error` | `AzimuthElevation`: `azimuth_rad` and `elevation_rad`, both doubles in radians |

The input and both nested angle pairs are checked at construction and before
encoding. IDs and timestamps require exact Python `int` values in range, not
booleans or coercible objects. The eight doubles follow the binding's numeric
conversion policy: overflow is rejected, finite double values and signed zero
are preserved, and signed infinities and NaN are allowed. NaN payload identity
is not promised. There is no normalization, clamping, unit conversion, or
zeroing when a validity flag is false. `AzimuthElevation` is the existing
canonical IR type, not a second angle representation.

Call `ControlChannel.submit_system_track_data_response(response)` on an open,
explicitly Enabled C2 channel. The facade enforces Enabled admission and the
Session's asynchronous request limit. Submission copies the validated input
into native storage and calls C2 once. An error after native send can leave
provider work active; the binding does not retry it.

## Results, diagnostics and lifetime

`wait(timeout_ms=0)` polls; positive uint32 milliseconds give a finite wait.
A timeout raises `MelError` and leaves the request open. Repeat Wait reads the
cached outcome without another submission:

- `C2SystemTrackDataCompleted` contains the canonical, owned `C2CommandStatus`.
  This includes the **returned** command ID, one of all five `C2CommandState`
  members, one of all 47 `C2CannotComply` members, and the complete reason
  description. A status whose state is `REJECTED` is still a completed future;
  even `ACCEPTED` does not establish operational incorporation of the data.
- `C2SystemTrackDataRejected` is only for native `COMMAND_REJECTED` (MEL
  ErrorOr). It preserves the `MelErrorCode` and the complete description.
- Native argument, admission, provider and internal failures remain structured
  `MelError`s. At the Python boundary, malformed output fails closed.

On success the outer diagnostic must be empty and complete; reason text is a
separate borrowed string view. Python checks its metadata and copies strict
UTF-8 before the request can close. On rejection or terminal failure an
oversized diagnostic is recovered by a zero-time cached reread that must agree
on status, byte count and any active result fields. Inactive status fields are
never read. Timeout is not reread for diagnostic text. The returned values are
independent of the request, C2, Session and the provider library.

`C2SystemTrackDataRequest` is a unique, noncopyable, nonpickleable observer and
does not keep Python parents alive. Externally serialize Wait and Close on the
same request and serialize submission with other C2 operations. Close, timeout
and dropping the Python parents do not cancel provider work. Native retains
the admitted graph and Session permit until completion accounting finishes.
Close is idempotent after success and remains retryable if it failed before
invocation. If native Close was invoked and consumption is uncertain, Python
disarms the owner, reports the original failure and never invokes Close again.
Context exit preserves a primary exception; finalization is non-raising.

## Runnable mock example

Build with `make test-native` from the checkout root. Save the following as a
script and run it with `PYTHONPATH=python`,
`AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0"` and
`native/build-tests/test-providers/libmock_ir_provider.so` as its only argument.

```python
import sys
from pathlib import Path

from ams_mel import ComponentLocation, UciId
from ams_mel.ir import (
    AzimuthElevation, C2CommandState, C2SystemTrackDataCompleted,
    ControlConfig, Session, SystemTrackDataResponse,
)

config = ControlConfig.new(
    UciId(bytes(range(16)), "IR C2 channel"),
    UciId(bytes(range(16, 32)), "IR platform"),
    ComponentLocation(0.0, 0.0, 0.0, "mock", "mock"),
)
response = SystemTrackDataResponse(
    system_time_ns=-8765432109876,
    command_id=0xF1234567, request_id=0xE2345678, track_id=0xD3456789,
    range_m=123456.75, range_rate_mps=-456.125,
    range_error_m=12.5, range_rate_error_mps=-0.875,
    az_el_valid=True, range_valid=True,
    inertial_az_el=AzimuthElevation(-1.25, 0.625),
    az_el_error=AzimuthElevation(0.03125, -0.015625),
)

with Session.open(Path(sys.argv[1]).resolve(), "c2-sys-echo", "") as session:
    with session.open_control_channel(config) as c2:
        c2.enable()
        with c2.submit_system_track_data_response(response) as request:
            completed = request.wait(5000)

# Context exits closed the request, C2 and Session; this is still owned data.
assert isinstance(completed, C2SystemTrackDataCompleted)
assert completed.status.command_id == response.command_id
assert completed.status.state is C2CommandState.ACCEPTED
assert completed.status.reason_description == "C2 system response accepted µ"
```

## Qualification and limits

The task worktree started from `26370d0b0cd203a232389337946d43d7873c60f0`
(tree `3d541998fda1283733c8169948c5303cfb645a7e`). The focused suite checks the
compiled mock's fourteen observed scalar slots, both validity bits without
zeroing, bit fidelity for all eight doubles, the complete status/rejection
domains, and isolated worker retention/reclamation with exact native barriers.
Hostile FFI tests check metadata and diagnostics as well as uncertain Close.

All positive functional qualification uses the compiled mock. This optional
operation may be unsupported by a real provider; no hardware operation,
cancellation, real-provider timing, or operational incorporation is claimed.
The facade remains responsible for the validity and lifetime of borrowed native
memory; Python's span checks cannot establish that an arbitrary ABI-violating
non-null pointer is mapped and readable.
