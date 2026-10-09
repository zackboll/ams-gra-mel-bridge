# RX JobInterval spatial controls (Task 034F4)

## Task 034F10 — value-only RF transmit events

JobInterval v9 nests frozen v8 and adds an optional ordered TX event collection.
Exactly `ams_mel_rf_job_add_intervals_v9` is added (198 → 199 exports; ABI 0.1).
RX-only, TX-only and mixed sequences use independent copied event vectors; no
cross-vector interleaving is implied. Presence 0 ignores inactive contents;
presence 1 replaces TX events once, including empty. UTF-8/NUL-free labels,
uint32 IDs, signed int64 femtoseconds, IEEE doubles and size_t-fit uint64 indices
are copied without physical validation or hidden capability queries. Upstream
modulation remains {-1}; weights and waveform resources remain untouched.
Safe Ada owns copyable TX_Transmit_Event_Config values and synchronous backing;
Add_Job_Intervals is a general alias retaining Add_RX_Job_Intervals compatibility.
Raw Ada, Rust sys and private Python mirror the ABI. This configures values,
not RF emission, waveform execution, routing or transfer. Squall Add is a no-op.
See `docs/task-034f10-rf-tx-events.md` for contracts, evidence and exclusions.


Task 034F8 adds copied RX JobInterval ProductStreamParams through nested v7 and
exactly `ams_mel_rf_job_add_rx_intervals_v7` (ABI 0.1). Safe Ada owns ordered,
copyable group and endpoint values; raw Ada/Rust/private Python mirror the ABI.
Presence 0 ignores inactive payloads; 1 copies even empty vectors. Numeric uint64
addresses are not owned pointers or registered memory. Caller/provider enforce
published JobRequest endpoint size/ordered-ID matching and storage lifetime;
structural acceptance does not establish hardware safety. No endpoint creation,
registration, routing or transfer is implemented. Older v1–v6 stay default-empty.
Pinned Squall Add is a no-op. See `docs/task-034f8-rf-product-stream-params.md`.


Task 034F7 adds copied RX JobInterval Local Function commands through nested
JobInterval v6 and `ams_mel_rf_job_add_rx_intervals_v6` (ABI remains 0.1).
Safe Ada owns copyable ordered command/write vectors; raw Ada/Rust/private
Python mirror the C records. Full uint64 addresses/values are numeric values,
never dereferenced. Instances must fit provider size_t. No capability or
hardware validation is performed. Older v1–v5 profiles retain empty LF commands.
Pinned Squall Add is a no-op: normal return is acceptance, not hardware writes.
The upstream `@RequiredIfLFSupport` boundary does not imply every MFA supports LF.
See `docs/task-034f7-rf-interval-lf-commands.md` for contract and evidence.


Task 034F6 adds optional value-only OEM pulse settings to RX events via
ReceiveEvent v4 / JobInterval v5 and exactly `ams_mel_rf_job_add_rx_intervals_v5`.
Presence 0 ignores the entire inactive payload without calling the pulse setter;
presence 1 validates enum domains and threshold span structure. Defaults remain
DBQ, M/N 0/0, width 0 fs, 50-percent time tag, empty thresholds. Reference values
are DBQ=0, DB_ABOVE_NOISE=1, DB_BELOW_SATURATION=2; time-tag values are 50%=0,
90%=1. Ordered duplicate thresholds preserve all IEEE doubles; M/N accepts all
uint8 combinations and signed int64 femtoseconds pass exactly without physical
validation, quantization or capability queries. All values precede one provider
Add, with existing status/lifetime gates and exception mapping. Safe Ada owns
copyable vectors and final-sized synchronous backing; raw Ada/Rust/private
Python match, without a safe Rust or public Python API. Older profiles are frozen,
ABI remains 0.1. Pinned Squall Add is a no-op: acceptance is not pulse detection,
PDW generation, threshold interpretation or hardware support. No TX, modulation,
Weights, ProductStreamParams, LF/context, endpoints, RDMA or VADB implementation.
See `docs/task-034f6-rf-rx-pulse-detection.md` for layouts and validation evidence.


Task 034F5 adds value-only RX receive-event v3 / JobInterval v4 controls through
one new export, `ams_mel_rf_job_add_rx_intervals_v4`. Shared execution values are
Normal=0 and Conditional=1; event termination is Inhibit=0 / Cancel=1. Ordered
polarization consists of zero, one or two four-double Stokes values, without
normalization or finite/range checks. Phase radians and Stokes doubles retain
signed zero, infinities and NaN classification. Boolean encodings are exactly
0/1; uint64 iteration counts must fit provider size_t. No execution/termination,
count/repeat, event/interval, activity or TX mode relationships are inferred.
Activity IDs are arbitrary copied binary bytes; uint32 TX Power Mode ID is not
TxPowerLevel. All values are prepared before the single provider Add, with no
hidden queries; existing status-registration/lifetime/lock/error paths remain.
Older event v1/v2 and interval v1/v2/v3 records/signatures remain frozen, ABI 0.1.
Safe Ada owns backing and privately serializes v4; raw Ada/Rust/private Python
match, with no safe Rust or public Python F5 API. Pinned Squall Add is a no-op:
real-provider acceptance is not execution/polarization/channelization evidence.
Weights (provider pointer ownership), PulseDetectionSettings (cohesive conditional
settings), Modulation (resource variants), endpoints and the inconsistent pinned
interval applicable-group setter/getter remain deferred. No TX events/intervals,
LF/context/ProductStream/RDMA/VADB expansion. See `docs/task-034f5-rf-rx-event-controls.md`.


Safe Ada adds `Set_Stab_Point_Index`, `Append_Applicable_RX_Element_Group`,
and `Append_Stab_Point` to the existing RX configuration types. Existing source
keeps empty spatial vectors and index zero. Private serialization uses additive
JobInterval v3 / receive-event v2; C v1/v2 records and exports remain frozen.
Ordered Pointing values and group indices preserve duplicates; uint64 indices
must fit provider size_t but are not locally checked against any group/point
count. F2 active-only UTC and exact-double policy applies. Status gating remains
v2. No queries, auto-quantization, TX intervals or resource APIs are added.
Pinned Squall Add is a no-op: real acceptance is not spatial semantic evidence.
The inconsistent JobInterval applicable-group setter/getter is deferred.
See [the task report](docs/task-034f4-rf-interval-spatial-controls.md).

# AMS MEL — Language Bridge for Ada/SPARK, Rust, and Python GRA Skills

Task 034F3 adds **ordered mixed RX/TX JobRequest v4 construction** in production
C and safe Ada. One nonempty sequence preserves interleaving and repeated labels;
only each envelope's selected payload is interpreted. RX retains all F1/F2
fields, endpoints and pointings. TX supplies label, finite duty `(0,1]`, exact
uint32 **TxPowerLevel** (not TxPowerModeID), and ordered finite frequency ranges.
TX makes no endpoint/expected-pointing calls or capability queries. Safe Ada
adds a private TX value, TX-only constructor overload and ordered TX append;
existing RX source remains compatible, with private v4 serialization. V1/v2/v3
records/signatures remain frozen; one additive submit export reuses the existing
hardened async owner/pipeline, ABI **0.1**, vendor unchanged. Mock-positive TX
construction is distinct from pinned receive-only Squall's TX mode-mismatch
negative probe followed by positive RX submission. **No TX event execution**,
TransmitEvent, TX intervals, modulation, Weights, waveform resources or callbacks
are implemented here. See `docs/task-034f3-rf-job-tx-groups.md`.

Task 034F2 adds **all five RF PointingType alternatives** and **RX JobRequest
v3** in production C and safe Ada: ECEF, LLA, PlatformRelative, FaceRelative,
and BaselineRelative. The caller selects the coordinates; meters, meters/second
and radians pass unchanged, including signed zero/infinity/NaN. No geometric
conversion, normalization or capability auto-gate occurs. Active ECEF/LLA UTC
must be canonical; inactive payloads are ignored. Safe Ada offers five private
value constructors, ordered `Append_Expected_Pointing`, and independent
`Set_Estimated_Stab_Point`/`Clear_Estimated_Stab_Point`. Existing source APIs/defaults
remain; Submit_Job privately uses v3. V1/v2 records/signatures are frozen, and
all versions share one hardened async owner/pipeline. Exactly one additive export
is measured (190 -> 191), ABI **0.1**, vendor unchanged; raw Rust/private Python
parity only. Positive payload fidelity is mock evidence; pinned Squall accepts
the setter calls/request without interpreting pointings. JobRequest is still
**not complete**: TX execution/commands, MFADrivenControls/JIB and rejection
callbacks/context remain deferred. See `docs/task-034f2-rf-job-pointing.md`.

Task 034F1 adds **RX JobRequest v2** in production C and safe Ada: ordered
multiple RX groups, ordered multiple DataPipe endpoint-set calls per group,
priority/precedence, canonical two-component min/max UTC times, exact signed
duration/lookahead, binary capability/activity IDs, request ID, ordered instance
selection (duplicates retained), interruptable and TX power-mode **set** semantics.
The frozen v1 records/export remain supported. Safe Ada's source-compatible
`Job_Config` now supports `Append_RX_Element_Group`, explicit pipe-label endpoint
collections, `UTC_Time` and scheduling/identity setters; `Submit_Job` uses v2 with
the historical defaults for untouched configurations. One new export takes ABI
**0.1** from **189 to 190**; raw Rust/private Python parity only. JobRequest is
**not complete**: pointing, TX groups/execution, MFADrivenControls/JIB machinery
and rejection callbacks/context remain deferred. Mock fidelity is distinct from
pinned Squall acceptance of two groups and fields it does not interpret. See
`docs/task-034f1-rf-job-request-v2.md`.

Task 034E6 adds all four conditional **VirtualAperture TX power/gain/attenuation
queries** in production C and safe Ada `AMS.MEL.RF.C2.Transmit_Power`. Each call
forwards scalar IDs and doubles to its exact provider method once: no local RF
formula, reconciliation, capability auto-gate, lookup, Weights owner or cache.
Upstream size_t IDs use stable uint64 with checked representability; units remain
dBW/dB/Hz and U,V components unchanged, including special IEEE values. The existing
VA claim permits synchronous parent-first calls, with no new owner/worker/pin.
Four additions take ABI **0.1** from **185 to 189**; raw Rust/private Python parity
only. Vendor **804** unchanged. Receive-only pinned Squall returns four zeros:
call-path/return-value evidence, **not positive TX capability or model accuracy**.
VirtualAperture remains partial; Weights, CachedWaveform, TX endpoints/events/jobs,
external/RDMA and VADB remain deferred. See `docs/task-034e6-rf-tx-power-queries.md`.

Task 034E5 adds **VA-level DataPipe connections and endpoint association** in
production C and safe Ada `AMS.MEL.RF.C2.Data_Pipes`, distinct from E3's optional
descriptor pipes. Copied nested snapshots retain no provider/claim/DSO; synchronous
commands address exact ElementGroup/DataPipe lookup keys, obtain a fresh container,
and invoke the exact mutation once. Multi-endpoint input uses upstream **set**
semantics; false is successful provider data. No cache, capability auto-gate,
retry, readback, persistent DataPipe owner or persistence guarantee. Pinned Squall
returns a fresh pipe each query: true mutation returns do **not** prove persistent
routing, RDMA/Q-pairs, external connectivity or hardware changes. Five additions
take ABI **0.1** from **180 to 185**; raw Rust/private Python parity only, all
**804** vendor blobs unchanged. See `docs/task-034e5-rf-datapipe-association.md`.

Task 034E4 adds required live cached-waveform/dynamic-weights Booleans and
conditional Local Function catalog/status queries in C and safe Ada. Ordinary
copied values preserve ascending type IDs, exact uint64 counts, zero counts,
and status vector ordering/duplicates without cross-call reconciliation or
provider retention. ABI 0.1 adds eight operations (172 -> 180); raw Rust/private
Python parity only, 804 vendor files unchanged. VirtualAperture remains partial:
at that checkpoint no VA DataPipe operations, Weights resources, TX power, cached-waveform allocation,
TX endpoints, RDMA or VADB. See
`docs/task-034e4-rf-va-capability-local-functions.md` for evidence and limitations.

Task 034E3 adds provider-created **element-group descriptor value snapshots** in
C and safe Ada `AMS.MEL.RF.C2.Element_Groups`. Mandatory RX/TX mode and four
numeric limits are copied; descriptor-level DataPipe labels/endpoint sets are
**explicitly opt-in** (Ada defaults false). Lookup keys stay distinct from
returned labels; unordered outer entries use canonical unsigned UTF-8 key order.
Plain nested values survive VA/C2/provider unload, without retaining provider
objects or adding TX execution/routing/association support. ABI stays **0.1**,
with exactly three additions (169 -> 172); frozen records and all 804 vendor
blobs are unchanged. Raw Rust/private Python parity only. See
`docs/task-034e3-rf-element-group-snapshots.md` for contracts and validation.

Task 034E2 adds **signal-only, coalescing VA status-change subscriptions** in C
and safe Ada `AMS.MEL.RF.C2.Virtual_Aperture_Notifications`. Subscribe first,
query E1 on the application thread, wait, then query again: this is **not an
atomic callback-time status snapshot**. The reference callback inspects no
provider object and calls no getter or Ada/application code. Explicit
Unsubscribe removes the exact key once while VA is live; local Close/finalization
only stops the observer. VA Close automatically removes, may block in provider
removal, and does not wait for callback quiescence. One attempt per public VA;
each exposed attempt intentionally retains a small exact-callable/signal/DSO
shell for process lifetime, never the VA/C2 graph. ABI remains **0.1**, with five
additions (**169** production exports); all **804** vendor blobs remain unchanged.
Raw Rust/private Python parity only. Mock-positive transitions and pinned Squall
registration/removal/no-delivery evidence are distinct. See
`docs/task-034e2-rf-va-status-subscriptions.md` for lifecycle and validation.

Task 034E1 adds all six synchronous BaseVirtualAperture read/query methods in
production C and safe Ada `AMS.MEL.RF.C2.Virtual_Aperture_Queries`. Fresh ID/status
calls are distinct from immutable Claim-time VA info. Owned instance lists retain
provider vector order and duplicates; complete reports copy one provider-returned
value, including ordered LF groups and nested statuses. Calls are not an atomic
multi-call transaction. Plain snapshots survive VA/C2/DSO teardown. Known Failed
and Degraded are successful data; unknown status values fail closed. Ten additions
bring ABI 0.1 to **164** production exports; all 804 vendor blobs are unchanged.
Raw Rust/private Python parity only. At that checkpoint VA callbacks, VADB, standalone LF queries,
weights/TX/RDMA and other deferred surfaces are not started. See
`docs/task-034e1-rf-va-status-queries.md`.

Task 034D3 adds the repeatable event-extension command
`ams_mel_rf_job_extend_event` and safe Ada `AMS.MEL.RF.C2.Extend_Job_Event`.
IDs are exact uint32 and added duration is exact signed int64 femtoseconds.
Normal return means only that the provider's void method returned without
throwing, not acceptance, scheduling or notification. Calls are admitted after
Finalize, blocked after any full Cancel attempt, and never automatically retried.
Status reception is optional; mock command-to-eventExtended feedback exercises
the existing owned stream. Pinned Squall's method is a no-op. Its ABI 0.1 checkpoint had **154**
production exports, unchanged 804 vendor files. Conditional commands/TX/RDMA/VADB
remain deferred. See `docs/task-034d3-rf-job-event-extension.md`.

Task 034D2 adds owned RF JobIntervalStatus reception and safe Ada
`AMS.MEL.RF.C2.Interval_Status`: bounded FIFO DROP-INCOMING, full ordered event
logs, exact signed seconds/femtoseconds, and arbitrary binary activity IDs.
The observer stream stops on Job Close without implicit cancellation. One exact
callable/state/DSO shell is retained per attempted registration, never the Job/VA/C2
graph. Registration returns void and does not acknowledge delivery. Frozen interval
v1 remains Never; additive v2 and the Ada setter select Never/Always/OnException.
That checkpoint had 153 exports; vendor unchanged at 804 files. Mock-positive delivery;
pinned Squall registration/commands are no-ops (timeout/stop lifecycle evidence
only). Extension was deferred at that checkpoint. See
`docs/task-034d2-rf-job-interval-status.md` for ownership and validation details.

Task 034D1 adds bounded RF receive JobInterval commands in native C and safe
Ada `AMS.MEL.RF.C2`: ordered receive-event sequences, Add, Flush and
Cancel_Remaining. Its checkpoint had 146 exports; vendor unchanged at 804 files.
**Pinned continuation count is zero**, aliasing an ordinary zero relative start;
INT64_MAX is forwarded unchanged, not treated as continuation. No automatic
quantization. Mock payload fidelity and pinned-Squall no-op call-path/lifecycle
evidence are distinct. Event extension was deferred at that checkpoint; TX remains deferred.
See `docs/task-034d1-rf-rx-job-intervals.md`.

RF PhysicalData is available through an owned point-in-time C snapshot and
the safe Ada-owned `AMS.MEL.RF.Physical_Data` value. It preserves antenna
dimensions, lattice angle, location XYZ and both ForeignKey strings, and
orientation/boresight roll-pitch-yaw in published meters/radians. Values remain
usable after DataMEL Close. ABI 0.1 now has 154 production operations;
Rust sys and private Python have raw parity only. See
`docs/task-034c2-rf-physical-data.md`. Task 034C3 adds complete TxPowerModeData
collection/direct snapshots in C and safe Ada: mock-positive, while pinned Squall
is receive-only with an empty collection. See `docs/task-034c3-rf-tx-power-modes.md`.
Receive-only JobInterval commands are available as described above.

RF duration quantization is available as the live C
`ams_mel_rf_data_quantize_duration` query and safe Ada
`AMS.MEL.RF.Quantize_Duration`, preserving signed 64-bit femtoseconds exactly.
The provider determines the result; callers can explicitly quantize values
before constructing receive JobIntervals. No interval call quantizes implicitly. See
`docs/task-034c1-rf-duration-quantization.md`.

Task 034B2B2: safe Ada RF Job `Finalize_Job`, `Wait_Job_Status`, and
`Cancel_Job` wrap the existing native lifecycle. Both Squall ProductRx
integrations use production Job APIs rather than the former test-only C++
helper. JobInterval status callbacks remain deferred.

An independent, experimental **consumer-side language binding** for the
Agile Mission Suite Government Reference Architecture (**AMS GRA**)
Multi-Function Aperture Encapsulation Layer (**MEL**) interfaces.

Native C and safe Ada support one-shot RF Job Finalize, finite status Wait/poll,
and cached Cancel. Both ProductRx clients activate Jobs without a C++ helper.
See `docs/task-034b2b2-safe-ada-rf-job-lifecycle.md`.

The project preserves the published **C++ MEL provider boundary**, isolates
non-C++ interoperability inside a native adapter, exposes a small **C ABI**, and
builds idiomatic Ada, Rust, and Python interfaces above that ABI. The Ada API
can also serve SPARK-oriented Skills at the reviewed FFI boundary, allowing
higher-assurance application logic to remain in Ada/SPARK. The Rust consumer
and Python's private `ctypes` layer reuse the same C ABI without separate C++
interoperability implementations.

Native C++ Skills do **not** need this bridge: they can consume the published
C++ MEL interface directly. `ams_mel_c` exists to make that same provider
ecosystem practical for languages that should not have to model the C++ ABI
themselves.

**RF Admin (Task 034B1A):** `AMS.MEL.RF.Admin` is a limited safe Ada owner
for the distinct published `createAdminMEL` factory. `Command_State` accepts
the canonical `AMS.MEL.Status.MFA_State` and returns the provider's Boolean
acceptance without treating `False` as an exception. The native bridge shuts
down Admin before releasing its DSO and retains the graph if shutdown throws.
`make test-squall-rf-ada-admin` is an opt-in real-provider check.

**RF C2 owner (Task 034B1B1):** `AMS.MEL.RF.C2` owns the distinct
`createC2MEL` factory through the same C facade. Its only operations are
`Open`, `Is_Open` and `Close` at the 034B1B1 checkpoint; shutdown destroys C2 before provider unload,
while a throwing shutdown permanently retains the uncertain graph. The opt-in
real-provider check is `make test-squall-rf-ada-c2`. VirtualAperture requests
were deferred to 034B1B2. The single-RX-group Job request and JobDetail
snapshot are now available in native C and safe Ada (034B2A1/034B2A2).

**RF VirtualAperture (Task 034B1B2):** Safe Ada `AMS.MEL.RF.C2` now submits
all five published VA request arguments, polls a cached asynchronous result,
uniquely claims a VA and exposes Ada-owned snapshots of instance IDs, element
group labels and single-group status. One native C2 child claim transfers from
the request to the VA; parent-first Close delays C2 shutdown until the last
child is released. `make test-squall-rf-ada-va` exercises this through pinned
Squall. Native C and safe Ada RX Job request/JobDetail snapshots are
implemented. Job finalize/status/cancel is available in native C and safe Ada;
Receive-only JobInterval commands are available. The opt-in `make test-squall-rf-ada-job`
checks two sequential Jobs and parent-first lifecycle against pinned Squall.

**Safe Ada RF (Task 034A):** `AMS.MEL.RF` and `AMS.MEL.RF.Product_Rx`
cover the complete **current native RF slice**: owned DataMEL, provider
version, wholly Ada-owned RFMFAInfo snapshots (including unknown format values),
asynchronous ComplexINT16 ProductRx creation, counters, and leased receive
events. `With_Samples` borrows the native event's C-ABI sample array without
an additional Ada payload copy; `Copy_Samples` makes an explicit Ada copy.
The native bridge itself already copies the provider callback buffer. RF
RDMA, other sample formats, richer metadata, jobs/C2, and safe Rust/public
Python RF remain outside this slice. See `docs/task-034a-safe-ada-rf.md`.

> **Current status:** C and Ada support provider Session lifecycle, IR host-memory
> Mono8 reception, and all three required C2 command sends: general ModeCmd with
> complete ScanParam, BIT, and ConfigSet, plus all three required C2-specific
> metadata callbacks. Ada uses safe one-choice BIT operations and fully owned
> `AMS.MEL.IR.C2.Metadata` values for CommandStatus, BIT_Configuration, and
> BIT_Status (including complete nested faults). Provider callbacks enqueue into
> a bounded native queue -- metadata is copied, and since Task 030B the Image
> payload is retained in place rather than copied; provider callback threads
> never invoke Ada application code. Ada additionally provides `AMS.MEL.IR.Image.Full_Frame`, an owned complete
> FrameHeader snapshot; legacy `AMS.MEL.IR.Receive` remains the Mono8 compatibility
> subset. For the high-rate data plane Ada also provides
> `AMS.MEL.IR.Image.Frame_Lease` with `Acquire_Frame`, `With_Pixels`, and
> `Copy_Pixels`: a limited owner of one native frame snapshot that borrows the
> pixel payload in place. Since Task 030B that borrow reaches all the way to
> the provider: the bridge performs **zero** bulk payload copies from the MEL
> callback buffer into Ada, and
> `irmel::Buffer::getImageAddress` == native snapshot `pixels.data` == the Ada
> `With_Pixels` first-element address. The claim is scoped to the bridge;
> Squall itself still copies received UDP bytes into the registered MEL host
> buffer, and nothing is claimed about NIC DMA or sensor transport. A live
> lease keeps one provider buffer checked out, so it causes real provider-level
> backpressure and defers physical provider teardown until it is closed;
> `Receive` / `Full_Frame` remain the owned-copy compatibility APIs. See
> `docs/task-030b-provider-buffer-zero-copy.md`,
> `docs/task-030a-zero-copy-ada-frame-lease.md`, and the high-rate data
> ownership section of `docs/architecture.md`.
> Image capability access and `AMS.MEL.IR.Image.Metadata` implement owned
> BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp events through one bounded
> DROP-INCOMING queue. `AMS.MEL.IR.Image` additionally exposes
> `Submit_Navigation_Report`/`Wait`/`Close` for `ImageChannel::send(NavigationReport)`:
> the complete published NavigationReport (all 18 covariance terms), asynchronous
> completion independent of the public Image_Stream/Session owners, and deferred
> provider teardown while a request is outstanding. LineOfSightQuaternion
> and other optional Image metadata remain unimplemented.
> C and Ada also implement the conditionally required Instrumentation channel
> (`AMS.MEL.IR.Instrumentation` and its `Metadata` child): `Open`, `Enable`,
> `Capabilities`, `Submit`/`Wait`/`Close` for
> `send(InstrumentationLevelCmd)`, and a bounded DROP-INCOMING queue for the
> `InstrumentationReport` callback with a blocking receive/wait, where timeout
> zero is the nonblocking poll case. Upstream `Priority` is exactly
> Normal/Debug, and one canonical report type carries both the future result
> and metadata events. Positive Instrumentation behavior is mock-validated;
> pinned Squall does not support this channel and is validated only for clean
> unsupported-provider failure. The Instrumentation-specific copies of the
> inherited generic Channel services (KeepAlive, CommsTest, buffers) are
> deliberately not cloned.
> C and Ada further implement the conditionally required Track channel's
> `@RequiredIfTrack` core (`AMS.MEL.IR.Track` and its `Metadata` child):
> `Open`, `Enable`, `Capabilities`, `Close`, and a bounded DROP-INCOMING queue
> for the `IRSTTrackReport` callback with a blocking receive/wait (timeout zero
> is the nonblocking poll case), returning a complete owned
> `IRST_Track_Report`. Upstream `IrstTrackState` is exactly
> Idle/Detected/Coast/Dropped and `IrstTrackMode` exactly Idle/Scan/Stare, with
> no invented MaxExclusive value; registration is one-shot because upstream has
> no unregister. Positive Track behavior and complete `IRSTTrackReport` payload
> fidelity are mock-validated, including a report emitted synchronously from
> inside registration. Pinned Squall does not attach this channel at all, so it
> validates clean unsupported-provider behavior only and provides no positive
> Track execution or Track-report evidence. C and Ada additionally implement the
> separately conditional `@RequiredIfTrackUpdate`
> `TrackChannel::send(TrackDataUpdate)` (`AMS.MEL.IR.Track.Updates`): the complete
> update including all 21 covariance terms, both epoch-second times, and the
> canonical Directional ECEF position/velocity, with an asynchronous request whose
> timeout is not cancellation, whose terminal result is cached, and whose
> CommandStatus rejection is deliberately distinguished from an ErrorOr rejection.
> Positive `TrackDataUpdate` behavior and payload fidelity are mock-validated
> only; pinned Squall cannot attach Track and so provides no positive
> `TrackDataUpdate` evidence. C and Ada also implement the separately optional
> (`@Optional`) `TrackChannel::send(SystemTrackDataResponse)`
> (`AMS.MEL.IR.Track.System_Data`): the complete response including signed
> nanosecond system time, verbatim range/rate/error values, and both
> azimuth/elevation pairs through the one canonical AzEl representation, with the
> same asynchronous request semantics and the same single Track request-accounting
> domain, so a pending update and a pending response together keep the provider
> graph alive until both complete. Positive `SystemTrackDataResponse` behavior and
> payload fidelity are likewise mock-validated only; pinned Squall provides no
> positive `SystemTrackDataResponse` evidence. The optional
> `RequestSystemTrackData` is complete as an inbound metadata callback: upstream
> declares no `send()` overload for it, so it is delivered through
> `registerMetadataCallback` and shares the one bounded Track metadata queue
> with `IRSTTrackReport` rather than being a `RequestFor<T>` operation. A
> `Return::NotSupported` refusal of that optional registration is non-fatal,
> whereas `Return::Fail` means the datatype is already registered on the channel
> and fails the open closed. The `@RequiredIfDetectCandidateObjects`
> `CandidateObjectMessage` is likewise complete as an inbound metadata callback:
> upstream declares no `send()` overload and no `RequestFor<T>` for it either,
> so it shares that same bounded queue and counter set. It is registered only
> when the channel advertises
> `ChannelMetadataCapabilityType::CandidateObjectMessage`; when it is advertised,
> any non-`Success` registration -- including `Return::NotSupported` -- fails the
> open closed, because refusing a callback the channel itself advertised would
> promise an event path the adapter cannot receive. The current Track status is
> therefore:
>
> ```text
> @RequiredIfTrack core                                   complete
> @RequiredIfTrackUpdate TrackDataUpdate                  complete
> SystemTrackDataResponse                                 complete
> RequestSystemTrackData                                  complete
> @RequiredIfDetectCandidateObjects CandidateObjectMessage complete
> CandidateObjectPreProcMessage                           complete
> Track API overall                                       complete
> ```
>
> The `@Optional` `CandidateObjectPreProcMessage` callback is also inbound
> metadata on that same bounded queue. Because its own annotation is
> `@Optional`, a `Return::NotSupported` refusal is non-fatal and the metadata
> open still succeeds; `Return::Fail` and any undocumented value fail closed.
> Native C and safe Ada Track coverage is complete for the published
> TrackChannel-specific surfaces; safe Rust and public Python Track APIs remain
> intentionally absent, and positive Track behavior remains mock-only because
> pinned Squall cannot attach Track through `Control::attachChannel`.
>
> The raw Rust sys crate
> and private Python ctypes layer track the complete current 90-function C ABI.
> Safe Rust and
> Python remain intentionally constrained to
> Session, Mono8, Operate/TaskSched, and the empty/no-op BIT profile. Their mode and Return
> requests include timeout, cached repeated waits, structured
> rejection descriptions, and independent parent/channel/request lifetime. An
> opt-in real Squall integration harness validates C, Ada, safe Rust, and safe Python
> against the same pinned provider/runtime stack; it is not part of ordinary
> builds or CI. Python remains a dependency-free, development-use-only binding:
> payload-bearing BIT and general Mode/ConfigSet are absent from their safe APIs;
> Ada additionally exposes inherited C2 KeepAlive, CommsTest request/callback,
> and complete ChannelCapability values. RF, explicit generic buffer management,
> Scheduling, optional C2 operations, and safe Rust/Python common-channel or
> metadata APIs are not implemented, and there is no wheel/PyPI publication
> or zero-copy/NumPy image API. Raw declarations alone are not safe-language parity.

This is not an official C MEL standard, a replacement for AMS GRA, a Squall
binding, or a claim of GRA compliance.

---

## Why This Project Exists

AMS GRA defines open boundaries between mission-system components so that
software and hardware can evolve independently. At the sensor edge, a
**Multi-Function Aperture (MFA)** exposes capabilities through the
**MFA Encapsulation Layer (MEL)**.

The published MEL interfaces are C++. That works naturally for C++ Skills, but
it creates a difficult interoperability boundary for languages such as Ada,
Rust, and Python because a direct binding would need to understand C++ object
lifetimes,
virtual interfaces, exceptions, smart pointers, callbacks, and ABI details.

This project puts that complexity in one place:

```text
                              PROVIDER SIDE

    C++ MFA       Ada MFA       Rust MFA       Hardware MFA
       \             |             |                /
        \            |             |               /
         +--------- GRA MEL (C++) ----------------+
                         |
                   standardized
                     boundary
                         |
          +--------------+-----------------------------+
          |                                            |
          v                                            v
      C++ Skill                                    ams_mel_c
      (direct)                                    C ABI bridge
                                                      |
                                  +-------------------+-------------------+
                                  |          |          |               |
                                  v          v          v               v
                                Ada        SPARK      Rust            Python
                               Skill       Skill      Skill            Skill

                              CONSUMER SIDE
```

The important idea is that the project is **not tied to one MFA
implementation**.

A C++ MFA, a Rust-based MFA, simulated sensor software such as Squall, or real
Native C++ Skills
can consume the published C++ MEL interface directly. Ada, Rust, and Python
Skills can consume the implemented portions of that same provider interface
through the shared `ams_mel_c` compatibility layer.

For an Ada MFA, Rust MFA, or hardware MFA, the provider-facing MEL library may
still contain a thin C++ layer while the actual implementation lives in Ada,
Rust, firmware, another process, another processor, or physical hardware.

That distinction is important: **"Ada MFA" describes the implementation behind
MEL; it does not mean replacing the published C++ MEL contract.** A typical Ada
provider architecture could look like:

```text
GRA Skill
   |
C++ MEL API
   |
thin C++ MEL provider
   |
C ABI / IPC
   |
Ada MFA backend
```

The same pattern can be used for Rust or other implementation languages.

---

## Two Consumer Paths: Native C++ and the Language Bridge

`ams_mel_c` is **not** intended to replace the native C++ MEL API.

There are two legitimate consumer paths:

```text
Native C++ path

C++ Skill
   |
   | published C++ MEL API
   v
MEL provider
```

and:

```text
Non-C++ language path

Ada / SPARK / Rust / Python Skill
             |
             | language wrapper
             v
         ams_mel_c
          |
          | C++ adapter
          v
      MEL provider
```

The C++ path is shorter because C++ can naturally consume the published MEL
types and object interfaces. There is no reason to force a C++ Skill through a
C ABI merely for architectural symmetry.

The C ABI exists for languages where directly importing the C++ MEL object
model would create unnecessary ABI, ownership, exception, callback, and
toolchain coupling.

This makes `ams_mel_c` an **interoperability bridge**, not a new mandatory GRA
layer.

### Consumer Language Paths

| Skill language | MEL path | Project status |
|---|---|---|
| **C++** | Directly consumes the published C++ MEL API | Native GRA path; does not require `ams_mel_c` |
| **Ada** | Ada API → private C imports → `ams_mel_c` → C++ MEL | Implemented for the current IR vertical slice |
| **SPARK** | SPARK/Ada code → Ada binding → `ams_mel_c` → C++ MEL | Architectural/high-assurance consumer path; FFI/native boundary itself is not SPARK-proved |
| **Rust** | Safe Rust wrapper → `-sys` crate → `ams_mel_c` → C++ MEL | Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op implemented and mock-tested; current safe-Rust slice also validated against real Squall; no payload-bearing BIT, additional C2 commands/callbacks, or RF |
| **Python** | Python API → private `ctypes` → `ams_mel_c` → C++ MEL | Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op implemented and mock-tested; current IR slice also validated against real Squall; no payload-bearing BIT, additional C2/RF, zero-copy/NumPy API, or wheel/PyPI publication |
| **C** | Calls the `ams_mel_c` C ABI directly | Low-level bridge API |

This split is intentional. C++ already speaks the native MEL interface, while
the other language paths benefit from a stable language-neutral ABI.

## MFA, MEL, Skills, and Squall in Plain English

### MFA — Multi-Function Aperture

A **Multi-Function Aperture** is the sensor-side component that interacts with
the physical/free-space environment.

Depending on the system, an MFA can include:

- antennas or optical apertures;
- analog RF or optical electronics;
- digitizers;
- cameras or RF front ends;
- FPGA or embedded processing;
- OEM firmware;
- local signal processing; and
- interfaces that deliver digital sensor data to mission processing.

An MFA converts physical phenomena such as electromagnetic energy or photons
into digital information that mission software can process.

### MEL — MFA Encapsulation Layer

The **MFA Encapsulation Layer** is the standardized interface between an MFA and
the mission software that consumes it.

MEL allows a Skill to request, configure, and receive sensor capabilities
without depending directly on the internal implementation of the aperture.

Examples include:

- **RF MEL** for radio-frequency capabilities and high-rate I/Q data; and
- **IR MEL** for electro-optical / infrared capabilities and image data.

The MEL boundary is the important compatibility point for this project.

### Skill

An **AMS GRA Skill** is mission-processing software that consumes aperture data,
performs domain-specific processing, and can publish higher-level mission data
products.

For example:

```text
IR MFA
  |
IR MEL
  |
IR Search-and-Track Skill
  |
UCI observation / track products
```

or:

```text
RF MFA
  |
RF MEL
  |
RF Processing Skill
  |
UCI signal products
```

"Skill" is a GRA term, not an acronym.

### Squall

**Squall** is the simulated MFA used by the public AMS GRA Hello World Starter
Kit.

At a high level, Squall:

1. consumes simulated world truth through DIS;
2. models RF and optical sensing;
3. synthesizes RF I/Q streams and IR image frames; and
4. presents those capabilities to mission software through MEL.

Squall is especially useful as a development and integration target because
real sensor hardware is not required.

However:

> **`ams-mel-ada` is not a Squall-specific binding.**

Squall is one MEL provider. The goal of this project is to allow Ada, Rust, and
Python software to consume the implemented portions of the **standard MEL
provider boundary**, whether the provider happens to be Squall or something
else. Native C++ Skills continue to use that MEL boundary directly.

---

## Why Not Just Rewrite Squall or the MFA in Ada, Rust, or Python?

Rewriting Squall solves a different problem.

An Ada, Rust, or Python reimplementation or replacement for Squall would
answer:

> How can an MFA or sensor backend be implemented in another language?

This project answers:

> How can an Ada, Rust, or Python **Skill** consume an existing GRA MEL provider
> without becoming a C++ application?

At the same time, an existing C++ Skill can continue using MEL directly; this
project does not insert itself into that native path.

Those are independent choices.

```text
                         PROVIDER IMPLEMENTATION

      C++ MFA        Ada MFA        Rust MFA        Hardware/Firmware
         \              |              |                  /
          \             |              |                 /
             +--------- GRA MEL (C++) ----------------+
                              |
                   standardized boundary
                              |
                  +-----------+---------------------------+
                  |                                       |
                  v                                       v
              C++ Skill                               ams_mel_c
              (direct)                               C ABI bridge
                                                        |
                              +-------------------------+----------------------+
                              |             |             |                  |
                              v             v             v                  v
                            Ada           SPARK         Rust               Python
                           Skill          Skill         Skill               Skill

                           SKILL IMPLEMENTATION
```

This provides several advantages:

1. **Provider independence**  
   An Ada Skill is not coupled to Squall. The same Skill can potentially use a
   simulated provider, lab equipment, or a deployed hardware provider that
   implements the same MEL contract.

2. **Incremental adoption**  
   A program does not need to rewrite an existing C++ GRA ecosystem before
   introducing Ada, Rust, or Python Skills. Existing C++ Skills remain on the
   native MEL path.

3. **One C++ interoperability implementation**  
   C++ exceptions, object ownership, callbacks, virtual interfaces, provider
   loading, and teardown rules are handled once in `ams_mel_c`.

4. **Language-appropriate application code**  
   Ada can expose strong types, deterministic ownership, contracts, and
   potentially SPARK-verifiable logic. Rust can expose ownership and
   memory-safe systems abstractions. Python can support rapid prototyping,
   mission-algorithm experimentation, analysis, test automation, and integration
   with scientific/ML tooling. All can reuse the same underlying native bridge.

5. **Separation of assurance boundaries**  
   Complex C++ provider interaction can remain behind a narrow C ABI while
   higher-assurance application logic is implemented in Ada/SPARK.

6. **SPARK as a Skill implementation option**  
   A Skill can keep the MEL/FFI boundary in ordinary Ada and place selected
   deterministic algorithms, state machines, scheduling/resource logic, and
   safety/security invariants in SPARK. The goal is not to claim that C++ MEL
   or the FFI itself is formally proved; it is to make the boundary narrow
   enough that the proof-oriented portion of the application remains tractable.

7. **Ada on both sides of MEL when useful**  
   The provider backend and the consuming Skill are independent choices. An
   Ada MFA backend can expose the standard MEL boundary through a thin provider
   adapter, while an Ada or SPARK Skill can independently consume MEL through
   `ams_mel_c`. They need not be part of the same process or product.

---

## High-Level Architecture

At the GRA boundary, C++ has a direct path while non-C++ languages can use the
bridge:

```text
                         MEL provider
                              |
                    published C++ MEL API
                              |
                 +------------+-------------------+
                 |                                |
                 v                                v
             C++ Skill                        ams_mel_c
             (direct)                         C ABI bridge
                                                 |
                              +------------------+-------------------+
                              |          |          |               |
                              v          v          v               v
                            Ada        SPARK      Rust            Python
                           Skill       Skill      Skill            Skill
```

The C++ Skill does not call `ams_mel_c`. It uses the same C++ MEL API that the
bridge's internal adapter uses.

The consumer-side path currently implemented by this repository is the Ada
branch:

```text
                   SAME PROCESS

+--------------------------------------------------+
| Ada Skill                                        |
|                                                  |
|   AMS.MEL                                        |
|      |                                           |
|      v                                           |
|   private C imports                              |
|      |                                           |
|      v                                           |
|   libams_mel_c.so                                |
|      |                                           |
|      | C++ adapter                               |
|      v                                           |
|   vendor / Squall MEL provider .so               |
+----------------------+---------------------------+
                       |
                       | provider-specific transport
                       | (may be IPC, network, PCIe, etc.)
                       v
                MFA implementation
```

The Ada program, `libams_mel_c.so`, and the loaded C++ MEL provider currently
share a process because MEL is an in-process C++ object API.

The **MFA implementation behind the provider does not have to share that
process**.

Squall is a good example.

A simplified Squall deployment is:

```text
PROCESS 1
Ada Skill
  |
# AMS MEL
  |
ams_mel_c
  |
Squall C++ MEL provider
  |
  | gRPC / TCP
  v

PROCESS 2
Couloir
  |
  | gRPC / Unix-domain socket
  v

PROCESS 3
Squall RF or Optical Backend
Rust
```

High-rate sensor data can use a separate data path. In Squall, control traffic
is handled through RPC while raw sensor payloads can be delivered over UDP to
the MEL-side data endpoint.

The practical result is:

- **Ada ↔ C/C++** uses an in-process FFI/ABI boundary.
- **C++ MEL provider ↔ Squall Rust backend** can use IPC/network protocols.
- Ada, C++, and Rust therefore do **not** all have to execute in one process.

---

## Why a C ABI?

C is used here as an interoperability boundary, not as the primary application
language.

Instead of exposing this to Ada or Rust:

```text
C++ virtual classes
std::shared_ptr
templates
exceptions
provider-specific object layouts
C++ callback lifetime rules
```

the public native facade can expose language-neutral concepts such as:

```text
opaque handles
fixed-width integers
plain C structures
explicit create / destroy operations
status codes
caller-owned buffers
bounded copies
blocking wait with timeout; nonblocking poll when the timeout is zero
```

That gives the project one controlled native boundary:

```text
                           C++ MEL
                              |
                        C++ adapter
                              |
                           C ABI
                /             |             \
              Ada            Rust          Python
             wrapper        wrapper        wrapper
             /   \
            /     \
         Ada     SPARK
        Skill    Skill

C callers can use the ABI directly.

Native C++ Skills bypass this entire bridge and use C++ MEL directly.
```

The C ABI belongs to this project. It is **not** presented as an official GRA
C interface, and it is **not** intended to become an extra layer for native C++
Skills.

---

## Current Implementation Status

The current implementation is intentionally narrow.

Implemented:

- native C ABI version query;
- runtime loading of a compatible IR MEL provider;
- creation and initialization of the provider `Control`;
- complete provider version-information copy;
- IR `IRSTImage` channel attachment;
- provider-owned host buffer management;
- single-band `Mono8` image reception;
- validation and bounded copying of incoming frames;
- finite DROP-INCOMING receive queue;
- C receive operations that block until the timeout expires and degrade to a
  nonblocking poll only when the timeout is zero;
- idiomatic Ada receive interface usable as the boundary for Ada/SPARK applications;
- native C ABI and safe Ada C2 general ModeCmd with complete ScanParam,
  intended one-choice payload-bearing BIT, BIT no-op, and ConfigSet interfaces;
- native queue and safe Ada support for complete C2-specific BIT_Configuration,
  CommandStatus, and BIT_Status metadata;
- native C and safe Ada application-facing inherited C2 Channel services:
  KeepAlive, ChannelCommsTest request/reply/callback, and complete owned
  ChannelCapability snapshots;
- explicit lifecycle and callback-quiescence handling;
- native and Ada tests using a separately loaded C++ mock provider;
- safe Rust Session, IR host-memory Mono8, C2 Operate/TaskSched, and BIT no-op
  APIs over the existing C ABI, including reusable `ReturnRequest` and typed
  Return completion/rejection behavior (`Return::Fail` is a normal completed
  result); safe Rust does not yet expose payload-bearing BIT;
- dependency-free Python Session, IR host-memory Mono8, C2 Operate/TaskSched,
  and BIT no-op APIs with owned-bytes frames, asynchronous mode/Return requests,
  cached waits, retryable C2 close, and independent parent/child lifetimes; and
- opt-in real Squall validation of the current C, Ada, safe Rust, and safe Python
  IR slices.

Not yet implemented:

- a real hardware provider integration;
- SPARK proof of the native/FFI boundary;
- explicit generic Channel buffer management;
- optional/conditional C2 commands;
- safe Rust/Python catch-up for the expanded Task 017 command surface;
- safe Rust/Python C2 metadata APIs;
- safe Rust/Python common-channel APIs;
- RF MEL;
- stacked images;
- tracking interfaces;
- OMS/UCI integration;
- OpenCV processing;
- device-memory buffers;
- zero-copy/NumPy image views;
- Python wheel/PyPI publication; or
- full AMS GRA compliance.

The present work should be viewed as a validated **vertical slice** of the
consumer architecture rather than a complete MEL binding.

---

## Callback and Data Ownership Model

Provider callbacks do **not** call directly into Ada application code.

For the current IR receive profile:

```text
C++ MEL provider callback
          |
          v
native adapter
  validate metadata
  copy Mono8 pixels
          |
          v
bounded native queue
          |
          v
Ada Receive (timeout > 0)
  blocking wait; timeout 0 is a nonblocking poll
          |
          v
Ada-owned frame
```

`Receive` with a positive timeout blocks on a condition variable inside the
native adapter until an event arrives, the stream stops, or the timeout
expires. A zero timeout is a nonblocking poll. No Ada thread spins, and no Ada
application code runs on a provider callback thread. An application may of
course choose to write its own polling loop with a zero timeout, but that is an
application decision, not the design of this binding.

This isolates provider callback threads from Ada code and gives the binding an
explicit place to enforce ownership, validation, queue capacity, shutdown, and
error handling.

The current queue owns copied pixels. Overflow drops the incoming frame.
Zero-copy or leased-buffer APIs require a separate lifetime contract and are
intentionally deferred.

---

## Repository Layout

```text
native/
  include/               C11 public declarations
  src/                   C++20 MEL adapter implementation
  CMakeLists.txt

ada/
  src/                   idiomatic Ada API and private C imports
  tests/                 Ada integration tests

rust/
  ams-mel-sys/            unsafe declarations for the reviewed C ABI subset
  ams-mel/                safe Session, IR Mono8, and C2 API and tests

python/
  ams_mel/                 dependency-free Session, IR Mono8, and C2 API via ctypes
  tests/                   Python mock-provider and C ABI drift tests

docs/
  architecture.md        implemented architecture decisions
  c-abi-policy.md        ownership and ABI rules
  coverage.md            binding coverage
  packaging.md           build/package design
  upstream-provenance.md pinned upstream declaration provenance
  tasks/                 incremental implementation tasks
  reference/             retained design/reference material

.clinerules/              repository workflow and architecture rules
.github/workflows/        CI
scripts/                  local validation helpers
integration/squall/       opt-in real Squall IR MEL validation
```

There is deliberately no root Alire crate.

The current crates are:

- `ams_mel_c` under `native/`
- `ams_mel` under `ada/`
- Cargo workspace crates `ams-mel-sys` and `ams-mel` under `rust/`

CMake owns native C/C++ compilation. The Ada project consumes the resulting
native library rather than recompiling the C++ adapter through GPRbuild.
Cargo likewise links that externally built library; neither Rust crate compiles
native C++, vendored MEL headers, Squall, or a provider. The Rust crates have not
been published to crates.io. The Python binding is used directly through
`PYTHONPATH`; no wheel or PyPI package has been published.

---

## Build the Native Library

Requirements:

- Linux
- CMake 3.20+
- Make or Ninja
- C++20 compiler

Run:

```sh
make test-native
```

This builds the contract-test facade and mock providers:

```text
native/build-tests/lib/libams_mel_c.so
native/build-tests/test-providers/
```

and runs the native ABI/provider/IR-stream tests from that tree.

The production facade built by `make native` stays in a separate tree:

```text
native/build/lib/libams_mel_c.so
```

Production and contract-test builds never share a CMake build directory: the
test configuration compiles `AMS_MEL_ENABLE_TEST_FAILPOINTS` into the facade and
the production configuration does not, so a shared tree would let one workflow
silently rebuild the other's library. `make test-build-isolation` proves the two
trees stay separate. See
[`docs/corrective-native-build-tree-isolation.md`](docs/corrective-native-build-tree-isolation.md).

Real Squall validation is deliberately separate from ordinary builds and CI.
With an existing checkout at the pinned revision, run:

```sh
SQUALL_SOURCE_DIR=/path/to/ams-gra-hello-world-sk-sensors-squall \
  make test-squall-ir
```

This runs the C, Ada, Rust, and Python integration clients in one Squall startup.
All four validate BIT no-op plus TaskSched and Mono8. Use
`make test-squall-ir-c`, `make test-squall-ir-ada`, or
`make test-squall-ir-rust`, or `make test-squall-ir-python` to select one client. See
`integration/squall/README.md`, `docs/task-004-validation.md`, and
`docs/task-009-validation.md`, `docs/task-013-validation.md`, and
`docs/task-015-validation.md` and `docs/task-016-validation.md` for runtime,
revision, cleanup, and evidence.

To test another compiler, use a separate build directory:

```sh
CC=clang CXX=clang++ cmake -S native -B build/clang \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release

cmake --build build/clang --parallel 2
ctest --test-dir build/clang --output-on-failure
```

---

## Build Ada with Alire

Requirements:

- hosted GNAT with Ada 2022 support;
- GPRbuild;
- Alire; and
- the native prerequisites above.

Run:

```sh
alr -C ada build
alr -C ada/tests run
```

Equivalent test action:

```sh
alr -C ada/tests test
```

To test with GNAT/GPRbuild directly:

```sh
make test-ada
```

The Ada binding links the production facade in `native/build/lib`, and an
ordinary Ada build both links and runs it. The contract tests exercise the
test-only failpoints, so they link the production facade but run against the
contract-test facade in `native/build-tests/lib`: the test project emits
`DT_RUNPATH`, which `LD_LIBRARY_PATH` overrides, and both `make test-ada` and
`alr -C ada/tests run` set it along with `AMS_MEL_TEST_PROVIDER_DIR`.

Format and verify Ada sources with:

```sh
make format-ada
make check-ada-format
```

GNATformat is pinned through the Ada test/development Alire environment. The
canonical Ada line width is 100 characters, and CI rejects formatting drift.

The current development manifests contain relative development pins and are not
yet registry-ready release manifests.

---

## Build Rust

Requirements:

- stable Rust with Cargo and Clippy; and
- the native prerequisites above.

Build the externally owned native library and mock providers first, then run the
workspace checks:

```sh
make test-native
cargo check --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml --workspace
cargo clippy --manifest-path rust/Cargo.toml \
  --workspace --all-targets -- -D warnings
```

An ordinary or direct `cargo` build uses the build script default, which is the
production facade in `native/build/lib`, so downstream Rust builds never link a
test-enabled native library. Set `AMS_MEL_NATIVE_LIB_DIR` to select another
existing CMake build's library directory and `AMS_MEL_TEST_PROVIDER_DIR` to
select its `test-providers` directory. `make test-rust` and the Rust CI job set
both explicitly to the contract-test tree (`native/build-tests`). Separately,
the repository's provider-path helpers and the ABI probe fall back to that test
tree when those variables are unset; that fallback applies only to the
repository's own contract tests, not to the build script's library default.
Cargo never invokes CMake or compiles the native adapter. The safe
layer is `ams-mel -> ams-mel-sys -> ams_mel_c`; neither crate is published.
Rust `Frame` values own copied `Vec<u8>` pixels; this is not a zero-copy API.
Safe Rust BIT support is intentionally limited to `submit_bit_noop`; no
payload-bearing BIT data is exposed. `ReturnRequest` is reusable for cached
terminal waits, and timeout or close/drop does not cancel provider work.
The ordinary Rust tests use the mock provider only. Task 009's preserved,
opt-in evidence records successful real Squall validation.

---

## Use Python

Python 3.11 or newer is the development target. Build the native façade and
mock providers, set the explicit façade path, and place `python` on `PYTHONPATH`:

```sh
make test-python

PYTHONPATH=python \
AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
  python3 -W error -m unittest discover -s python/tests -v
```

The path passed to `Session.open` is the separate provider library. The safe
API accepts only `str` provider paths (including `os.PathLike` values whose
`os.fspath` result is `str`) and strictly encodes all inputs as UTF-8. Python
uses `ams_mel -> private ctypes -> ams_mel_c -> C++ MEL`; it neither models nor
loads C++ provider interfaces directly. This is not a native extension, wheel,
published package, or zero-copy API. The binding has no external Python
dependencies and is intended for development use. Session, IR host-memory Mono8,
and C2 Operate/TaskSched are implemented with owned-bytes frames, asynchronous
mode requests, and independent parent/child lifetimes. The current IR slice has
real Squall validation. RF and additional C2 operations are not implemented;
NumPy/zero-copy image views and packaging remain absent.

---

## Local Validation Gate

Run:

```sh
make check
```

The gate validates the native implementation, Ada integration, and repository
checks. It fails when a required Ada toolchain is unavailable rather than
silently skipping the Ada validation.

---

## Ada/SPARK, Rust, and Python Consumer Paths

### SPARK

SPARK is not a separate binary ABI from Ada. A SPARK-oriented Skill can use the
same Ada binding and keep the language boundary in a small ordinary-Ada package:

```text
C++ MEL provider
      |
  ams_mel_c
      |
ordinary Ada FFI / ownership wrapper
      |
   SPARK core
      |
proved application logic
```

This creates a natural **trusted-boundary pattern**: the native/C++ interaction
is isolated and reviewed, while selected logic above it can be written and
proved in SPARK.

Potential SPARK candidates include:

- deterministic state machines;
- sensor/resource arbitration;
- scheduling and bounded queues;
- validity and range invariants;
- track/state management;
- command validation; and
- other logic where explicit contracts and proof are valuable.

The current project does **not** claim SPARK proof across the C or C++ boundary.

### Rust and Python

The Rust and Python consumers reuse the same native boundary rather than binding
the C++ MEL API independently.

### Rust

The Rust implementation is structured as:

```text
rust/
  ams-mel-sys/       raw C ABI declarations
  ams-mel/           safe Rust ownership/API wrapper
```

Conceptually:

```text
Rust Skill
    |
safe ams-mel crate
    |
ams-mel-sys
    |
C ABI
    |
ams_mel_c
    |
C++ MEL provider
```

The safe Rust layer turns opaque handles and explicit C lifecycle operations into
Rust ownership types and RAII-managed Session, ImageStream, ControlChannel, and
ModeRequest and ReturnRequest resources for the current IR slice. Both request
types are neither `Send` nor `Sync`; their timeout and close/drop operations do
not cancel provider work. BIT is only the empty/no-op profile, not a generic or
payload-bearing API.

### Python

The dependency-free Python binding sits on the same C ABI:

```text
Python Skill
     |
Python package
     |
     private ctypes layer
     |
C ABI
     |
ams_mel_c
     |
C++ MEL provider
```

The current implementation uses `ctypes` for Session, owned-copy IR Mono8
reception, and C2 Operate/TaskSched. Frames own Python `bytes`; mode requests are
asynchronous and preserve cached waits, retryable C2 close, and independent
parent/child lifetimes. The current IR slice is validated against real Squall.
It does not provide RF, additional C2 operations, a native extension,
zero-copy/NumPy image views, wheels, or PyPI publication.

Python is particularly attractive for:

- rapid Skill prototyping;
- algorithm exploration;
- test and simulation tooling;
- data analysis and visualization;
- NumPy/SciPy/OpenCV integration;
- ML inference and experimentation; and
- orchestration around native high-performance processing.

A Python API should still preserve explicit ownership and bounded-buffer rules.
It should not assume that Python callback execution is appropriate on arbitrary
provider threads, and high-rate data paths need to account for copies, Python
object creation, and interpreter/GIL behavior.

### C++

C++ needs none of these wrappers:

```text
C++ Skill
    |
published C++ MEL API
    |
C++ MEL provider
```

This direct path remains the reference/native consumer path.

The overall result is:

```text
                              GRA MEL
                                 |
                 +---------------+----------------------+
                 |                                      |
                 v                                      v
             C++ Skill                              ams_mel_c
             direct                                     |
                                      +----------------+----------------+
                                      |        |         |              |
                                      v        v         v              v
                                    Ada      SPARK     Rust           Python
                                   Skill     Skill     Skill           Skill
```

This keeps provider ownership, callback handling, exception containment, and
C++ ABI compatibility in one bridge for the languages that need it, while
leaving native C++ consumers untouched.

---

## Project-Relevant Acronym Glossary

The GRA ecosystem contains many acronyms. These are the ones most relevant to
this repository and the surrounding Hello World architecture.

| Acronym | Meaning | Relevance |
|---|---|---|
| **AMS** | Agile Mission Suite | Mission-system architecture family. |
| **GRA** | Government Reference Architecture | Government-defined reference architecture; together, **AMS GRA**. |
| **AMS GRA** | Agile Mission Suite Government Reference Architecture | The open mission-system architecture this project targets. |
| **MFA** | Multi-Function Aperture | Sensor/aperture component that interacts with the free-space environment and produces digital sensor data. |
| **MEL** | MFA Encapsulation Layer | Standardized software boundary between an MFA and mission processing. |
| **RF** | Radio Frequency | Radio-frequency sensor domain. |
| **IR** | Infrared | Infrared / optical sensor domain. |
| **EO/IR** | Electro-Optical / Infrared | Optical and infrared sensing domain. |
| **IRST** | Infrared Search and Track | IR sensing/processing function; `IRSTImage` appears in the current receive profile. |
| **I/Q** | In-phase / Quadrature | Complex sample representation commonly used for digitized RF data. |
| **RX** | Receive | Receive-side RF operation. |
| **TX** | Transmit | Transmit-side RF operation. |
| **DIS** | Distributed Interactive Simulation | Simulation protocol used by the Hello World environment to distribute world truth. |
| **OMS** | Open Mission Systems | Mission-system standards used for service/data interoperability. |
| **UCI** | Universal Command and Control Interface | Standard message/data model used for higher-level mission products. |
| **CAL** | Critical Abstraction Layer | OMS software boundary that isolates services from underlying transport details. |
| **LA-CAL** | Language-Agnostic Critical Abstraction Layer | CAL pattern that allows services written in different languages to connect through a language-neutral protocol. |
| **OWP** | OMS WebSocket Protocol | WebSocket protocol used by the Hello World LA-CAL/Sleet implementation. |
| **ASB** | Abstract Service Bus | Logical OMS messaging network carrying UCI data among services. |
| **MASI** | Mission Agnostic Service Infrastructure | Shared mission-system infrastructure such as routing, management, health, and observability services. |
| **MPU** | Minimum Procurable Unit | AMS GRA unit intended to be independently procured/integrated. |
| **OEM** | Original Equipment Manufacturer | Manufacturer of sensor hardware/firmware behind an MFA. |
| **C2** | Command and Control | Mission command/control information and messaging. |
| **API** | Application Programming Interface | Source-level programming interface exposed to callers. |
| **ABI** | Application Binary Interface | Binary contract between compiled components; `ams_mel_c` exposes a C ABI. |
| **FFI** | Foreign Function Interface | Mechanism through which one programming language calls code written in another. |
| **IPC** | Inter-Process Communication | Communication between separate operating-system processes. |
| **RPC** | Remote Procedure Call | Request/response interaction across a process or network boundary. |
| **gRPC** | gRPC remote-procedure-call framework | RPC technology used in Squall's backend control path. |
| **TCP** | Transmission Control Protocol | Reliable byte-stream network transport. |
| **UDP** | User Datagram Protocol | Datagram transport useful for high-rate data paths such as Squall sensor payload delivery. |
| **UDS** | Unix-Domain Socket | Local IPC socket used by Squall/Couloir for backend gRPC communication. |
| **SPARK** | SPARK language/toolset for high-assurance Ada | Ada-based language/toolset used for contract-based analysis and formal proof; a SPARK Skill can use the same Ada MEL binding while keeping native FFI code outside the proved core. |

---

## Terminology at a Glance

A useful mental model for the larger ecosystem is:

```text
Physical / simulated world
          |
          v
         MFA
   sensor / aperture
          |
          | MEL
          v
        Skill
   raw-data processing
          |
          | OMS / UCI
          v
Mission-system services
```

For the public Hello World system:

```text
DIS truth
   |
   v
Squall simulated MFA
   |
   +---- RF MEL ----> RF Skill ----+
   |                               |
   +---- IR MEL ----> IR Skill ----+----> OMS / UCI
```

For this project:

```text
      C++ MFA      Ada MFA      Rust MFA      Hardware MFA
         \           |            |              /
          +--------- GRA MEL (C++) -------------+
                         |
             standardized boundary
                         |
              +----------+------------------------------+
              |                                         |
              v                                         v
          C++ Skill                                 ams_mel_c
          direct                                        |
                                  +--------------------+------------------+
                                  |         |          |                 |
                                  v         v          v                 v
                                Ada       SPARK      Rust              Python
                               Skill      Skill      Skill              Skill
```

---

## Provider and Skill Languages Are Independent

One of the architectural goals is to make these choices independent:

```text
Provider / MFA implementation:
    C++ | Ada | Rust | hardware / firmware | other

                    ↓

              standard GRA MEL

                    ↓

Skill implementation:
    C++ | Ada | SPARK | Rust | Python | other bridgeable language
```

An **Ada MFA** and a **SPARK Skill** therefore solve different problems and can
exist independently:

```text
Ada MFA backend
      |
thin C++ MEL provider
      |
==== standardized MEL boundary ====
      |
ams_mel_c
      |
Ada binding
      |
SPARK Skill
```

They also do not have to be in the same process. MEL-provider internals may use
FFI, IPC, network transport, device drivers, or other implementation-specific
mechanisms behind the standardized boundary.

## Scope and Compatibility

The C-facing ABI remains experimental version **0.1** and is unrelated to:

- an upstream MEL API version;
- an AMS GRA architecture revision; or
- a provider's own version number.

Linux x86-64 is the initial validation target.

The headers contain normal Windows visibility declarations for future work, but
that is not a claim of tested Windows compatibility.

No SPARK proof claim is made for the provider or FFI boundary.

Provider declarations used by the native adapter are pinned and documented in
`docs/upstream-provenance.md`.

---

## Design Principles

1. **Preserve the published C++ MEL provider boundary.**
2. **Do not invent a competing provider protocol.**
3. **Keep C++ implementation details out of Ada/SPARK, Rust, and Python APIs.**
4. **Use a small, explicit, ownership-aware C ABI for languages that need a bridge.**
5. **Do not force native C++ Skills through the C ABI; let them consume MEL directly.**
6. **Contain exceptions at the native bridge boundary.**
7. **Do not call arbitrary language-runtime application code from provider callback threads.**
8. **Prefer bounded ownership and explicit lifetimes before zero-copy designs.**
9. **Keep provider loading and unloading safe across live child resources.**
10. **Make metadata conversions loss-aware.**
11. **Allow MFA implementation language and Skill implementation language to vary independently.**
12. **Preserve a narrow ordinary-Ada boundary so SPARK logic can sit above it without pretending the native FFI is proved.**
13. **Add capability vertically: provider boundary, C tests, language wrapper, language tests, documentation.**

---

## Background References

Useful public background material:

- AMS GRA Hello World — Overview and Terminology  
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/0-overview.html

- AMS GRA Hello World — How to Build a Multi-Function Aperture  
  https://open-arsenal.gitlab.io/ams-gra/hello-world-sk/getting-started/tutorials/3-build-mfa.html

- Open Arsenal — Common MEL  
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel

- Open Arsenal — RF MEL  
  https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel

These references describe the surrounding GRA concepts. The behavior and
supported surface of this repository are defined by this repository's own
source, tests, architecture decisions, and pinned upstream provenance.

---

## Licensing

New scaffold and binding code is supplied under Apache-2.0; see `LICENSE`.

Vendored upstream declaration material retains its upstream licensing and
provenance. See:

```text
docs/upstream-provenance.md
docs/upstream-files.sha256.md
```

before changing, replacing, or publishing the pinned declaration closure.
