# PY-MEL-016 — safe Python StackedImage channel foundation

## Source, scope and preflight

Initial/final integrated base: `cb33c5e5434fa27b9d9660f6a9d361ed0993e37b`,
tree `0f6851c7e87ed002491fd61c2c43bd2683243ca1`, freshly fetched before
implementation. Dedicated worktree `/home/zboll/git/ams-py-mel-016`, branch
`feature/py-mel-016-stacked-foundation`. PR #216 (Python common metadata),
#198 (native/Ada Stacked), #211 (Rust Stacked) and #217 (common Disable) were
already merged; no overlapping Python Stacked PR was open. #217's merged
native behavior is the baseline, not copied unmerged work. No Python Disable
API is introduced.

Reviewed repository instructions, architecture, ABI policy, provider-foundation
task, inventory schema and native/Ada Stacked/common evidence, exact ABI and
ctypes declarations, native implementation/mock/C11 tests/layout probe, Ada
owner, Rust implementation/tests/qualification, and Python discovery, owners,
common requests, metadata, conversion, ABI and package identity tests.

Pinned upstream IR MEL commit `8d9224519f12b44e0b28815755c56a32a28d24a0`,
tree `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`; Common MEL commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
`f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`. The independently frozen
structured inventory remains authoritative for native/Ada coverage.

Coverage delta: Python gains typed **StackedImage attachment**, complete
**capability advertisement snapshots**, and existing **common communications**.
This completes four-required-channel control/communications attachment parity
(C2, HealthAndStatus, IRSTImage and StackedImage), not MEL compliance or
image-acquisition parity. **Stacked-image acquisition is not implemented.**
No image listeners, buffers, registration/unregistration, pixel/frame receive,
LOS3D, new worker/callback thread, native engine or shared coverage changes.

## Public API and exact ABI mapping

`StackedConfig(channel_id: UciId, platform_id: UciId,
sensor_location: ComponentLocation)` is frozen and has exactly three fields.
Construction performs no provider operation. Exact canonical nested model types
are required. Full 16-byte identities, strict UTF-8/NUL-free strings (including
empty strings), and numeric C-double representability are checked at construction
and rechecked recursively at submission, even after deliberate frozen-object
mutation. Supported IEEE values and signed zero are not normalized; NaN payload
bit preservation is not promised. Labels use byte lengths, not character counts.

`Session.open_stacked_channel(config) -> StackedChannel` requires an open Session,
then uses `_RawStackedConfig` and the existing borrowed-string backing machinery.
The exact existing `ams_mel_ir_stacked_config_v1` / `IrStackedConfigV1` fields are:

| C field, in order | Python input |
| --- | --- |
| `ams_mel_uci_id_v1 channel_id` | full UUID and borrowed UTF-8 label |
| `ams_mel_ir_channel_type_t channel_type` | `AMS_MEL_IR_CHANNEL_STACKED_IMAGE`, exactly 6 |
| `ams_mel_uci_id_v1 platform_id` | full UUID and borrowed UTF-8 label |
| `ams_mel_component_location_v1 sensor_location` | three C doubles, key/system string views |

The backing bundle remains strongly reachable through synchronous Open, including
injected garbage collection. No record or function declaration is changed.
Existing ctypes signatures (all return `c_int32`, with diagnostic tail
`CharPointer, c_size_t, SizePointer`) are:

- `ams_mel_ir_stacked_open(SessionHandle, POINTER(IrStackedConfigV1), POINTER(IrStackedHandle), tail)`
- `ams_mel_ir_stacked_enable(IrStackedHandle, tail)`
- `ams_mel_ir_stacked_get_capabilities(IrStackedHandle, POINTER(IrChannelCapabilityHandle), tail)`
- `ams_mel_ir_stacked_close(POINTER(IrStackedHandle), tail)`
- `ams_mel_ir_channel_from_stacked(IrStackedHandle, POINTER(IrChannelHandle), tail)`

Open is invoked once. Success must publish a non-null owner. Native failure,
malformed output or wrapper construction failure gets at most one best-effort
cleanup of a published real owner; cleanup failure cannot replace the primary
error. Open and ownership-uncertain Close are never retried automatically.
Both new public classes have normal `ams_mel` import/pickle identity.

## Discovery, lifecycle and ownership

Python does not implement attachment discovery. Native Open requires advertised
StackedImage, attaches exactly once, validates concrete `StackedImageChannel`
and attached category, and owns rollback. Missing/empty advertisement reports
`INITIALIZATION_FAILED`; discovery/attach exceptions retain native status,
null attachment is `FACTORY_FAILED`; incompatible concrete/category or throwing
capability after successful rollback is `INITIALIZATION_FAILED`; uncertain
rollback is `PROVIDER_FAILED` with retained provider graph. Invalid local
configuration is `INVALID_ARGUMENT`. Ordinary rejected attachments do not poison
the Session; tests exercise version and C2 attachment afterwards where discovery
permits it.

Open returns Attached. `enable()` is explicit and idempotent after success.
Refusal/exception fails the native lifecycle without automatic Enable retry.
Capabilities and common operations work Attached/Enabled, not Failed/Closed.
No acquisition listener or storage is allocated. `is_open` means local native
handle presence only, not Enabled, healthy or reusable.

`capabilities()` uses the unchanged shared `_channel_capabilities` converter,
copying every identity, layout/pixel field, ordered enum collection, metadata,
task depth, flags, nested band and navigation frame. Zero layouts/depth and
UINT32_MAX depth are preserved, as are reserved/unknown enums, duplicates and
empty collections. No nonzero stack geometry is invented. Returned immutable
Python graphs survive channel/session destruction and provider unload.

`channel_view()` returns the existing weak `ChannelView`, retaining neither
Python parent nor provider code. View closure never closes the typed owner.
It reuses KeepAlive `ReturnRequest`, `CommsTestRequest`/`CommsRequest` and
`CommsMetadataStream`. All three request IDs remain uint32; replies contain
command/request IDs only, never an invented channel ID. Timeouts and request
owner release do not cancel provider work. Session admission is shared but
Stacked accounting is independent. Live channels survive Session closure/GC.
Native completion claims, not Python references, retain admitted work and code.

Common metadata is independently opt-in and one-shot, Attached or Enabled.
It receives actual provider callbacks, never synthesized future results.
Synchronous callbacks, bounded FIFO/drop counters, independent future waits,
weak-view closure, late callbacks and teardown use the unchanged native receiver.
C2 continues using its dedicated metadata API. Closing metadata stops consumption
but does not unregister callbacks or promise quiescence.

### Close state machine

All operations on one owner require external serialization; the GIL does not
satisfy the native contract. Close ends logical admission and native teardown
may defer for admitted requests. Native disables if needed and detaches even
after Disable failure. The wrapper deliberately does not inherit generic cleanup:

| Outcome | Python ownership/action |
| --- | --- |
| Success, null output | consumed; repeated Close harmless |
| Disable failure, null output | surface error; consumed, never retry |
| `PROVIDER_FAILED`, same non-null owner, exact complete `Stacked detach failed; provider state retained` diagnostic | logically closed; explicit Close may retry detach only |
| Malformed diagnostic, thrown call, changed owner, success retaining owner, or any undocumented retained output | uncertain; no explicit/automatic native retry |
| Failure preparing before native invocation | owner remains in previous state; safe to try again |

After native Close begins, finalization/context cleanup never retries, including
the documented retained case. Only an explicit `close()` may retry a documented
detach failure. `is_open` stays true for a retained owner, but Enable/capabilities/
new views reject it and existing views are refused natively. A non-raising
finalizer attempts cleanup at most once while still live. Application exceptions
are preserved over cleanup errors. The safety tradeoff is potential permanent
resource/code retention when consumption cannot be proven, rather than double
release or provider-code unload with live objects. Deferred detach failure is
permanently retained by native code without a public retry owner.

## Executable consumer

`provider_path`, `instance_name`, and `route` are caller supplied. Stacked enum
value 6 is not a universal CommsTest routing ID. The mock accepts the caller's
full-width route; the reference stub documents its own numeric routing rule.

```python
from contextlib import closing
from ams_mel import Session, ChannelType, StackedConfig, CommsTestRequest


def fourth_channel(provider_path, instance_name, route):
    with Session.open(provider_path, instance_name, "") as session:
        advertised = next(
            capability for capability in session.capabilities()
            if ChannelType.STACKED_IMAGE in capability.channel_types
        )
        config = StackedConfig(
            channel_id=advertised.channel_id,
            platform_id=advertised.platform_id,
            sensor_location=advertised.sensor_location,
        )
        with session.open_stacked_channel(config) as stacked:
            capability = stacked.capabilities()
            assert ChannelType.STACKED_IMAGE in capability.channel_types
            with stacked.channel_view() as view:
                with closing(view.send_keepalive()) as request:
                    keepalive_result = request.wait(1000)
                with view.open_comms_metadata() as metadata:
                    request_ids = CommsTestRequest(
                        command_id=0x80000001,
                        channel_id=route,
                        request_id=0xE0000003,
                    )
                    with closing(view.submit_comms_test(request_ids)) as request:
                        callback = metadata.receive(1000)
                        future_result = request.wait(1000)
            stacked.enable()
    return capability, keepalive_result, callback, future_result
```

The test executes this exact block against `common-meta-parity`. Its existing
held KeepAlive promise is released by the test-only native completion gate at
Wait entry; no provider response is fabricated. Comms callbacks arrive before
future Wait. This ordering illustrates mock/provider behavior, not guaranteed
callback delivery while Attached on every provider.

## Qualification and limitations

Development native run: `make test-native`, **744/744 passed**. Focused Python
development run: **52/52 passed**, with
`python3 -W error -m unittest test_ir_stacked_foundation -v` and
explicit test-facade/provider environment. Dangerous ownership/lifecycle tests
run in fresh subprocesses with 45-second bounds; native gates/observers, not
arbitrary sleeps, establish completion. Tests compare only guaranteed provider
lifecycle ordering, not unrelated observer log suffixes. Injected pointers are
real allocated owners/snapshots, never fabricated invalid addresses.

Exact final-source qualification, export comparison, source identities and hosted
run results are recorded in the delivery PR. **HOSTED_CI_PENDING** until all
required exact-HEAD checks pass and independent technical review remains required.
No physical sensor, stacked data, acquisition, or complete MEL compliance claim.

The unchanged local reference-provider checkout is
`/home/zboll/git/ams-rust-ir-014-reference`, commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`. It only selects Comms callbacks
while Enabled, so its separate integration example must Enable before waiting
for a callback. Attached-state registration and common futures remain valid.
Its route 6 is provider-specific. Qualification here is a scaffold integration,
not hardware interoperability.

Separate Release provider build completed successfully, unchanged, using:

```sh
cmake -S /home/zboll/git/ams-rust-ir-014-reference \
  -B /tmp/py-mel-016-reference-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build /tmp/py-mel-016-reference-build \
  --target minimal_ir_mel_provider --parallel 2
```

The second example below passed against that DSO: complete advertised zero-layout
snapshot, explicit Enable, KeepAlive success, actual callback received before
future Wait, and correlated completed future. The fetched-base API comparison
also passed with a deliberate changed-Session-method negative control failing
as required (`python3 -W error python/tests/test_ir_stacked_foundation.py --baseline`).

Separate unchanged-provider integration (Enabled callback selection):

```python
from contextlib import closing
from ams_mel import (
    Session, ChannelType, StackedConfig, CommsTestRequest, CommsTestReport,
    CommsCompleted, ReturnCompleted, CommandReturn,
)


def reference_fourth_channel(provider_path):
    with Session.open(provider_path, "", "") as session:
        version = session.provider_version()
        assert version.vendor == "minimal-mel-example"
        assert (version.api_version, version.library_version) == (4, 1)
        advertised = next(c for c in session.capabilities()
                          if ChannelType.STACKED_IMAGE in c.channel_types)
        config = StackedConfig(advertised.channel_id, advertised.platform_id,
                               advertised.sensor_location)
        with session.open_stacked_channel(config) as stacked:
            assert stacked.capabilities() == advertised
            assert (advertised.height, advertised.width, advertised.buffer_size) == (0, 0, 0)
            with stacked.channel_view() as view:
                assert view.capabilities() == advertised
                with closing(view.send_keepalive()) as request:
                    assert request.wait(1000) == ReturnCompleted(CommandReturn.SUCCESS)
                with view.open_comms_metadata() as metadata:
                    stacked.enable()  # reference stub only delivers callbacks Enabled
                    ids = CommsTestRequest(0x80000001, 6, 0xE0000003)
                    with closing(view.submit_comms_test(ids)) as request:
                        report = CommsTestReport(ids.command_id, ids.request_id)
                        assert metadata.receive(1000) == report
                        assert request.wait(1000) == CommsCompleted(report)
            assert stacked.capabilities() == advertised
    return advertised
```

Run the exact second block with:
`AMS_MEL_REFERENCE_PROVIDER=/absolute/path/to/libminimal_ir_mel_provider.so python3 -W error python/tests/test_ir_stacked_foundation.py --reference`.
