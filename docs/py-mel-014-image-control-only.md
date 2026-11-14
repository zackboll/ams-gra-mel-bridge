# PY-MEL-014 — Safe Python Image control-only attachment

## Source authority and delivery

- Repository: `https://github.com/zackboll/ams-gra-mel-bridge`.
- Branch: `feature/py-mel-014-image-control-only`; target: `main`.
- Freshly fetched base commit: `76631880ee236b9839b8b9668be36e423ef1b050`.
- Base tree: `e21e61cf735ac37d56588e5798d997ebe67e5d8b`.
- The delivery PR records the final exact HEAD/tree and hosted qualification;
  this document cannot contain its own containing commit identity.

Authority is the existing public C11 record/function in
`native/include/ams_mel/abi.h`, implementation in `native/src/ir_stream.cpp`,
compiled C contract `native/tests/test_ir_image_control.c`, and separate mock
`native/tests/mock_provider.cpp`. The Ada sibling slice is documented in
`docs/irmel-ada-015-image-control-only.md`. Existing ctypes definitions are used
unchanged. No Squall-private transport or provider is required by these tests.

## Public API and coverage delta

`ImageControlConfig(channel_id: UciId, platform_id: UciId,
sensor_location: ComponentLocation)` is a frozen, public, pickleable value.
`Session.open_image_control(config) -> ImageStream` returns the **existing**
ImageStream, not a subclass or second command implementation. No buffer count,
buffer size, queue capacity, format, dimensions or acquisition settings occur
in the configuration. Ordinary `Session.open_image_stream()` is unchanged.

Python can now attach an unconfigured IRSTImage channel, query complete immutable
capabilities, use common KeepAlive/CommsTest, register the four ordinary Image
metadata families, and submit NavigationReport before Start. This is a Python
exposure delta over an existing native interface, not an Ada coverage change;
the structured Ada inventory remains unchanged and is checked for integrity.

The native mapping is exactly `ams_mel_ir_image_control_config_v1` through
`_native.IrImageControlConfigV1`, submitted once to
`_native.ams_mel_ir_image_control_open`. It contains only channel type
(`AMS_MEL_IR_CHANNEL_IRST_IMAGE`), channel ID, platform ID and component location.
The private `_RawImageControlConfig` borrows explicitly retained encoded buffers
through that call, with UTF-8 **byte** lengths and all 16 UUID bytes.
There are no new native exports, layouts or ctypes declarations.

## Validation, ownership and lifecycle

Construction and submission validate the whole graph, including deliberately
mutated frozen objects and missing fields. Canonical UciId/ComponentLocation
types are required. NUL, invalid Unicode, invalid UUIDs, Boolean offsets and
non-numeric/overflowing doubles are rejected before native submission. Offsets
use the existing numeric-to-C-double validation policy; IEEE NaN/infinity and
signed zero are not replaced, and no location transform, finite-only rule,
physical bounds or calibration is invented.

Open requires an open Session and a non-null native owner on success. Failed
Open (including diagnostic decoding and Python owner-wrapping failures) makes
at most one cleanup attempt if a handle was unexpectedly published. Cleanup
failure cannot mask the original exception and uncertain cleanup is not retried.
Successful Open returns the existing Image owner and its existing lifecycle.
Native graph retention, not a Python Session reference, keeps provider code alive.
Calls on each owner require external serialization; no Python worker is added.

Start is explicit and only enables this channel. It does not call the buffer
factory, allocate image storage, initialize buffers or register acquisition
buffers. Capability geometry remains zero. Python `receive()` and the existing
native snapshot receive entry point reject this mode with structured
`INITIALIZATION_FAILED` and `image receive unavailable on control-only attachment`.
There is no invented Frame, pixel snapshot or acquisition guarantee.
Stop/Close retain existing native semantics. Metadata and already admitted
requests have independent native lifetimes; request Close is not cancellation.

`channel_view()` remains weak: closing it does not close Image. Capabilities
copy the entire native record (including nested spans) into immutable Python
values. Metadata can register before Start and receive BadPixelList,
LineOfSightReport, LineOfSightEuler and NavigationReportResp synchronously.
Callbacks during provider teardown are safely handled but are not promised as
post-Close user events. Metadata receipt does not complete a command future.

Unexpected Image callbacks use the native release-retention machinery. Tests
exercise release reentry, attach reentry, failure and exception in isolated
processes. Ownership uncertainty deliberately retains the provider graph for
process lifetime, rather than claiming safe unload or retrying a release.

## Consumer example

`provider_path`, `instance_name`, `platform_id` and `sensor_location` are
caller-supplied. The exact block below is executed against the compiled mock
by `test_documented_consumer_example`; it assumes no real sensor or pixels.

<!-- executable-control-example -->
```python
from ams_mel import (
    ChannelType,
    ImageControlConfig,
    Session,
)

with Session.open(provider_path, instance_name, "") as session:
    advertised = session.capabilities()

    capability = next(
        item for item in advertised
        if ChannelType.IRST_IMAGE in item.channel_types
    )

    config = ImageControlConfig(
        channel_id=capability.channel_id,
        platform_id=platform_id,
        sensor_location=sensor_location,
    )

    with session.open_image_control(config) as image:
        with image.channel_view() as view:
            attached = view.capabilities()
            assert attached.channel_id.uuid == capability.channel_id.uuid

        with image.open_metadata() as metadata:
            image.start()
            # Metadata and commands are available.
            # No image pixels are assumed.
```

### Navigation commands are separate from metadata

Supply a complete caller-owned `NavigationReport` with explicitly chosen time,
solution state, latitude/longitude/altitude, attitude, attitude-rate plus its
timestamp, NED speed/acceleration, wander angle, magnetic heading, MSL altitude
and all 18 named position/velocity covariance entries. The test fixture
`test_ir_navigation.report` supplies a complete **mock/test report**, not actual
INS readings. The focused control-only tests submit that report while Attached.
For an already constructed complete `report`, use:

```python
from ams_mel import MelErrorCode, NavigationCompleted, NavigationRejected

with image.submit_navigation_report(report) as request:
    result = request.wait(5000)
    if isinstance(result, NavigationRejected):
        # The control-only mock explicitly rejects with these values.
        assert result.code == MelErrorCode.INVALID_STATE
        assert result.description == "no navigation hardware"
    else:
        assert isinstance(result, NavigationCompleted)
        response = result.response
```

A timeout raises the existing structured timeout error; it does not cancel.
A NavigationReportResp metadata callback is a separate event, even when it
arrives synchronously during command submission. An explicit mock rejection
must not be interpreted as a successful navigation report or native exception.
Other providers can return different documented results; the assertions above
describe the named mock only, not an operational sensor.

## Qualification and limitations

The sequential local qualification (no shared-artifact rebuild/test overlap)
produced these results:

| Command | Actual result |
| --- | --- |
| `make test-native` | PASS: 587/587 C/C++ contracts, including the C11 control-only contract. |
| `make test-python` | PASS: 583 tests, warnings treated as errors; includes 21 new Image-control tests. |
| `make test-policy` | PASS: 244 controls (18 + 18 + 28 + 65 + 75 + 18 + 9 + 13). Intentional failing CTest fixtures are negative controls, not hidden failures. |
| `make check-ir-api-coverage` | PASS: 61 controls and pinned offline inventory audit. |
| `make test-build-isolation` | PASS: both build orders, 587/587 native contracts, production/test hashes unchanged by opposite-tree builds and failpoints absent from production. |
| `python3 scripts/check_doc_source_integrity.py` | PASS: 825 vendor files, four workflows. |
| `python3 scripts/check_final_newlines.py` | PASS after adding final newlines to the two new files. |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS. |
| `git diff --check` | PASS. |

Focused regression command (with `PYTHONPATH`, `AMS_MEL_NATIVE_LIB` and
`AMS_MEL_TEST_PROVIDER_DIR` selecting this worktree's test artifacts):

```sh
python3 -W error -m unittest test_ir_image_control test_image_stream \
  test_ir_image_metadata test_ir_navigation test_session test_common_channel \
  test_abi test_import_identity test_package_layout -v
```

PASS: 236 tests. Eleven new tests launch fresh interpreters with 45-second
timeouts, covering all named adversarial/failure scenarios plus pending work.
The pending scenario uses the existing mock completion barrier, not sleeps:
after Python Image/Session destruction, no detach occurs until the admitted
request completes. Its result and provider teardown are then observed. Normal
scenarios observe actual DSO unload; uncertain-release scenarios observe two
release attempts total (one per checkout), one reentry return, no channel
destruction and no DSO unload. Attach-null/attach-throw also conservatively
retain the native graph: absence of a returned channel cannot prove the
provider discarded the listener. No pixel/sample/flags access is tolerated by
the mock's unsolicited buffer methods (they abort if queried).

Hostile ABI injection tests cover success without owner, failure with an
unexpected owner, UTF-8/NUL/termination/capacity diagnostic corruption,
wrapping failures, BaseException and cleanup failure. They verify one submission,
at most one cleanup attempt and preservation of the original failure. The
published initial-base package was extracted with `git archive` and imported
against the compiled facade: both new public names are absent (negative control
PASS). Existing public baseline, import/pickle and C layout probes pass.

Changed-file inventory: `python/ams_mel/__init__.py`, `_types.py`, `_ffi.py`,
`_session.py`; `python/tests/test_ir_image_control.py`; this task document.
Native, ctypes, Ada, Rust, CI, policy and historical coverage files are unchanged.
The PR records the actual independently built base/feature production symbol
comparison and final commit/tree, along with exact-HEAD hosted results.
GPRbuild is not on local PATH: no local Ada qualification is claimed; hosted Ada
must be verified independently. Hosted disposition remains `HOSTED_CI_PENDING`
until the delivery PR records completed exact-source checks. Do not infer
independent approval from any author-account qualification comment.

No real-provider/hardware qualification, StackedImage, generic buffer
registration, new frame abstraction, new callback family, background Python
worker, sensor format/calibration, DSP or full IR consumer parity is added.
The C++ provider ABI must still be compatible behind the C facade. Native
fail-safe retention can intentionally keep resources/code alive indefinitely.
Independent technical review is mandatory; author comments are not approval.
