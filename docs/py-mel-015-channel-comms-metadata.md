# PY-MEL-015 — safe common communications metadata

## Contract and coverage delta

`ChannelView.open_comms_metadata(queue_capacity=16)` now exposes the merged
consumer-side ChannelCommsTestRep receiver. The pinned published IR interface
at IR commit `8d9224519f12b44e0b28815755c56a32a28d24a0` (Common commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`) defines this callback as
**provider to consumer**, not a completion notification
invented from a future. Its entire report is the existing frozen
`CommsTestReport(command_id, request_id)`: two unsigned 32-bit identifiers, with
no channel ID. Both are copied verbatim into Python-owned integers.

The initial freshly fetched base is commit
`e00369ff95d8adbccd8d2e32012addf2f966a2ec`, tree
`779b6395b31148eb2e0e517ee0bee6e0fe7ca449`. PRs #202/#208 were merged; #204
was closed unmerged. No overlapping Python feature PR was open at preparation.
The base already had the four private ctypes bindings, but neither this public
method nor the new safe classes; a fresh-interpreter archive test proves this.

The canonical C Open has seven arguments: channel, size_t capacity, output
owner, output provider Return, diagnostic, diagnostic capacity, required length.
The four unchanged exports are `ams_mel_ir_channel_comms_metadata_open`,
`ams_mel_ir_channel_comms_metadata_receive`,
`ams_mel_ir_channel_comms_metadata_get_counters`, and
`ams_mel_ir_channel_comms_metadata_close`. No layout, symbol binding, export,
native implementation, mock, or shared coverage classification changes.
Successful C operation status additionally requires provider Return Success and
a non-null owner. Contradictory success is a protocol inconsistency; any
unexpected owner is released once. Full existing diagnostic validation and
structured `MelError` status conversion apply, including unknown statuses.

Health registration works Attached or Enabled, without implicitly enabling.
Image registration works with ordinary acquisition attachments and zero-layout
`Session.open_image_control()` attachments, without allocating control-only
buffers, manufacturing geometry or starting acquisition. Ordinary Image metadata
and communications metadata have separate subscriptions and queues. The method
is family-generic, ready for a future StackedImage weak view, but **no safe Python
StackedImage attachment exists** here. Native/Ada tests remain its authority.
C2 common views are rejected: use `control.open_metadata()` followed by
`enable_comms_test_events()` instead. Instrumentation/Track/Scheduling are not
made supported, and no second C2 callback owner is installed.

## Ownership, queue and errors

The new `CommsMetadataStream` owns only one native consumption handle. It has
no strong Python reference to its weak view, typed owner, or Session. Closing
the view leaves reception intact; retaining the view does not keep the provider
graph alive. Native callback state retains what it needs. Requests independently
retain their admitted native graph. A callback may arrive before its request
future completes; Receive neither completes nor cancels that future.

Registration is explicit and one-shot, including failed provider attempts.
Refusal, partial installation and provider exceptions do not permit a retry
through another view. Partially retained callback state is inactive and safe.
Close does not reset registration, unregister callbacks, disable/detach a typed
channel, close Session, cancel requests or authorize provider unload.

The native bounded FIFO uses DROP-INCOMING, preserving previously queued
reports. Frozen `CommsMetadataCounters` copies the complete uint64
`events_received`, `events_dropped_queue_full`, `malformed_or_unsupported` values.
Counters saturate at UINT64_MAX, include null callbacks as malformed, and are
neither inferred from queue length nor reset by Receive. No Python queue exists.

`receive(0)` polls; positive uint32 milliseconds performs a finite native wait.
Active-empty raises TIMEOUT without cancelling registration. Previously queued
reports drain after typed teardown, then STREAM_STOPPED (or PROVIDER_FAILED
after failed teardown). No partial output escapes a failing Receive/counter call.

Explicit successful Close is idempotent. Its attempted flag is set immediately
before native invocation, not before diagnostic allocation. Once native Close
begins, failure, exception or a retained owner makes the outcome uncertain and
no context manager or finalizer retries it. The finalizer is non-raising, and
an earlier application/Open/construction error wins over cleanup failure.
Local `is_open` means handle presence, not provider health or permission to
reuse an uncertain owner.

Externally serialize operations and Close on the **same public owner**, and
registration with typed-channel operations. The GIL is insufficient. Typed
closure may race metadata Receive under the native contract; receiver Close
must not race its own operations. No background workers, provider locks or
Python callback threads are added. Callback teardown/quiescence is a provider
boundary, not implied by receiver Close or an empty queue.

## Executable Health and control-only Image example

The caller supplies identities and routing selectors. Numeric routing is
provider-specific, **not derived from UUIDs**. For the compiled mock only, the
scenario `common-meta-parity` accepts Health route 4 and Image route 1.

```python
from contextlib import closing
from ams_mel import Session, CommsTestRequest


def receive_health(provider, scenario, health_config, route, command_id, request_id):
    with Session.open(provider, scenario, "") as session:
        with session.open_health_channel(health_config) as health:
            with health.channel_view() as view:
                with view.open_comms_metadata() as metadata:
                    with closing(view.submit_comms_test(
                            CommsTestRequest(command_id, route, request_id))) as request:
                        callback_report = metadata.receive(1000)
                        future_result = request.wait(1000)
                        return callback_report, future_result, metadata.counters()


def receive_image(provider, scenario, image_config, route, command_id, request_id):
    with Session.open(provider, scenario, "") as session:
        with session.open_image_control(image_config) as image:
            with image.channel_view() as view:
                with view.open_comms_metadata() as metadata:
                    with closing(view.submit_comms_test(
                            CommsTestRequest(command_id, route, request_id))) as request:
                        callback_report = metadata.receive(1000)
                        future_result = request.wait(1000)
                        return callback_report, future_result, metadata.counters()
```

The focused test extracts and executes this exact block against the compiled
mock, independently asserting callback and future IDs and no control-only
storage activity. No physical sensor or arbitrary provider routing is claimed.

## Qualification evidence and limitations

Focused warnings-as-errors tests live in
`python/tests/test_ir_common_comms_metadata.py`. Real compiled callbacks cover
Attached/Enabled Health, ordinary/control-only Image, synchronous delivery,
overflow/FIFO, null reports, uint64 saturation, multiple high-bit/max identifiers,
timeouts, weak lifetimes, retained futures, failure states and safe unload.
Hostile ABI controls use real mock-owned handles, never invalid addresses, and
cover contradictory Open outputs, diagnostics, unknown statuses, construction
failure, uncertain Close outcomes and partial failing Receive/counter writes.

Dangerous scenarios run in isolated interpreters with finite subprocess
deadlines and deterministic callback/completion barriers. Logs verify no unload
before retained work/callback return, destructor-time callbacks after partial
registration, and actual `library_unloaded` after safe teardown. Failed detach
is checked before the native-supported typed-owner retry; that typed retry does
not authorize retrying this receiver's uncertain Close.

Final-source local and hosted results, exact HEAD/tree, final base, export-set
comparison and independent review disposition are recorded in the delivery PR.
Disposition is **HOSTED_CI_PENDING** until all required exact-source hosted
qualification succeeds. Independent technical review is mandatory; author
qualification is not approval. This is mock-only qualification, not transport,
hardware, routing or full four-channel Python consumer parity.
