# Implementation coverage

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


Task 034F4 adds RX JobInterval spatial association: ordered stabilization
PointingType values (all five F2 alternatives), event stab-point index, ordered
duplicate-preserving applicable RX group indices. Native C, safe Ada and raw
Ada/Rust/private Python are covered; frozen C interval v1/v2 retain spatial
defaults. Only size_t representability is checked for portable uint64 indices,
not relationships with point/group counts. Active ECEF/LLA UTC must be canonical;
inactive UTC is ignored and active doubles are forwarded without finite checks.
Status Never/Always/OnException retain v2 local-registration gating. No queries or
auto-quantization. Pinned Squall Add is no-op acceptance, not geometry, steering,
scheduling or provider index validation. TX intervals and all unrelated event/
resource fields remain deferred, including the inconsistent pinned interval
applicable-group setter/getter and endpoints. See the task report for actual checks.

Task 034F3 completes the non-callback RX/TX ElementGroupCommand JobRequest
construction profile in production C and safe Ada. V4 supports RX-only, TX-only
and one ordered mixed sequence, including repeated labels. TX label/duty/exact
uint32 TxPowerLevel/ordered frequency ranges are covered; TX has no endpoint or
expected-pointing calls. TxPowerLevel is distinct from the common TxPowerModeID
set and is not physically interpreted. All F1/F2 common fields and five estimated
PointingType alternatives remain, as do RX endpoints and five ordered expected
PointingType alternatives. Active-only envelope validation, rollback/mismatch,
shared async retention and raw Rust/private Python parity are covered. Safe Ada
retains legacy RX constructors while using private v4 submission. C v1/v2/v3
remain frozen. No capability gate or vendor expansion. Mock-positive TX request
construction is **not TX execution**; pinned Squall is receive-only, providing a
negative TX mode probe and subsequent RX success, not positive TX evidence.
Still deferred: TransmitEvent, TX JobInterval sequences, modulation, JobEvent
stab-point use for TX, Weights, CachedWaveform/WaveformTxEndpoint,
MFADrivenControls/JIB/rejection callbacks/context, external/RDMA and VADB.
See `task-034f3-rf-job-tx-groups.md` for measured validation.

Task 034F2 covers all five PointingType alternatives for independent estimated
stab points and ordered RX expected pointing angles, in production C and safe Ada.
Together with F1 this includes multiple RX groups, frequency ranges, multiple
DataPipe endpoint sets, priority/precedence, min/max UTC, duration, request ID,
instance selection, interruptable, binary capability/activity IDs, TX power-mode
ID set and lookahead. Frozen native v1/v2 remain; safe Ada privately serializes v3
with historical no-point defaults and source compatibility. One additive export,
no new owner/vendor dependency, no safe Rust/public Python pointing API. Exact
variant/component/order/duplicate/IEEE fidelity is mock-proven. Pinned Squall stores
expected pointings but does not interpret them or estimatedStabPoint in requestJob;
acceptance is not scheduling/geometry/hardware evidence. **JobRequest is still
not complete.** Deferred: TX ElementGroupCommands (including command power), TX
requests, MFADrivenControls/JIB machinery, rejection callbacks/context, Weights,
CachedWaveform/WaveformTxEndpoint, external/RDMA and VADB. No conversion utilities
or capability auto-gate. See `task-034f2-rf-job-pointing.md`.

At the F1 checkpoint, Task 034F1 extended native C and safe Ada RX JobRequest coverage to multiple
ordered RX groups, multiple ordered RX pipe endpoint-set calls, duty/frequency
ranges, priority/precedence, canonical min/max UTC components, exact signed
duration, request ID, ordered instance-selection vector (duplicates retained),
interruptable, binary capability/activity vectors, TX power-mode ID set and
lookahead. Frozen native v1 remains supported and unchanged; safe Ada preserves
source compatibility and historical provider-visible defaults while using v2.
One additive export takes ABI 0.1 from 189 to 190; no new owner/vendor dependency,
safe Rust v2 or public Python v2 API. Mock tests inspect exact provider getters,
command call counts/order, validation, rollback and shared async retention.
Squall two-group acceptance has a narrower interpretation limit: it validates
RX groups and copies requestId, not the other scheduling/identity fields.
**JobRequest was not complete at F1.** EstimatedStabPoint and RX pointing angles
are now covered by F2 above. Still deferred:
TX ElementGroupCommand fields/Job requests, MFADrivenControls/JIB callback
machinery, rejection callbacks/context. No CachedWaveform/WaveformTxEndpoint,
Weights, external/RDMA or VADB work is included. See the F1 task report.

Task 034E6 implements all four VirtualAperture @RequiredIfTransmit calculations:
getTxRadiatedPower (dBW), getTxPeakRadiatedPower (dBW), getTxApertureGain (dB),
getMaxTxAttenuation (dB), in production C and safe Ada Transmit_Power. Scalar
uint64 group/WeightType IDs check size_t representability; exact doubles/U,V
and independent provider results pass without formulas, local lookups, gates,
normalization, caching or Weights resources. Existing synchronous VA lifetime
works after public C2 Close, with no new owner/worker/pin. Mock evidence covers
exact inputs, exceptions/output preservation, IEEE values, boundaries, freshness
and isolated actual DSO unload. Receive-only pinned Squall's four zero results
are call-path/return-value evidence only, not TX capability or RF model accuracy.
Four ABI 0.1 additions take 185 -> 189; raw Rust/private Python parity, vendor 804
unchanged. Current VA query coverage includes Claim-time info, BaseVA status/
instance queries, status subscriptions, ElementGroup descriptors, required
cached-waveform/dynamic-weight Booleans, Local Functions, VA-level DataPipe
connections/association and all four conditional TX calculations. VirtualAperture
is NOT complete: descriptor command creation/thick coverage, broader command/
JobRequest configuration, static/dynamic Weights, CachedWaveform allocation,
WaveformTxEndpoint, external/RDMA, VADB, TX JobIntervals/TransmitEvent and extra
ProductRx formats remain deferred. See `task-034e6-rf-tx-power-queries.md`.

Task 034E5 implements VirtualAperture::getDataPipes plus DataPipe getLabel,
getAssociatedEndpoints, associateEndpoint and associateEndpoints through production
C and safe Ada `AMS.MEL.RF.C2.Data_Pipes`. E3 descriptor queries remain independent.
Mock evidence covers deterministic ordering, keys versus labels, full-width IDs,
aliases/fresh objects, set semantics/empty input, true/false, lookup failures,
malformed/null data, exceptions/rollback, changing independent snapshots and
parent-first ownership with copied values after actual DSO unload. Five ABI 0.1
exports take 180 -> 185; frozen records remain unchanged, vendor 804 unchanged.
Rust/private Python raw parity only. Pinned Squall has fresh object-local pipes:
snapshot values and mutation true returns do not establish persistence, external
connectivity, RDMA/Q-pairs or hardware routing. VirtualAperture/DataPipe/RF MEL
remain partial at that E5 checkpoint: equality, descriptor command builders, Weights, TX power,
CachedWaveform, WaveformTxEndpoint, external/RDMA, VADB, TX intervals/events and
additional ProductRx formats remain deferred. See the E5 report for actual checks.

Task 034E4 adds production C and safe Ada for required
isCachedWaveformSupported/dynamicWeightsSupported and conditional
getLocalFunctions/getLocalFunctionStatus. Mock-positive LF evidence covers
independent calls, exact IDs/counts/order/duplicates, empty/zero-count data,
changing values, unknown status, rollback and provider-independent snapshots.
Pinned Squall returns false/false and empty map/vector: not positive LF support.
VA read/query coverage now includes Claim info, BaseVA live status/instance
queries/reports, status-change adaptation, descriptors, required capability
Booleans and conditional LF queries. VirtualAperture is NOT complete. Deferred:
at that checkpoint VA DataPipes, descriptor command creation, static/dynamic Weights resources,
TX power, cached-waveform allocation, dynamic TX endpoints, external/RDMA,
VADB and full TX Job/Event support. ABI 0.1 adds eight exports (172 -> 180),
raw Rust/private Python parity only; vendor 804 unchanged. Actual validation:
`task-034e4-rf-va-capability-local-functions.md`.

Task 034E3 represents getElementGroups plus all six mandatory descriptor getters
and explicitly opt-in descriptor getDataPipes / pipe getLabel/getAssociatedEndpoints
in production C and safe Ada ordinary values. Canonical unordered-map key order,
distinct labels, RX/TX, complete numerics, alias occurrences, uint64 endpoint sets,
optionality/fail-closed semantics and provider-independent teardown are mock-tested.
Pinned Squall RX/default-empty-pipe values are asserted in opt-in integration;
no live route/association/TX-execution claim. ABI 0.1: three additions (169 -> 172),
raw Rust/private Python only, vendor inventory 804 unchanged. VirtualAperture,
DataPipe and RF MEL as a whole remain partial. See
`task-034e3-rf-element-group-snapshots.md` for exact scope/evidence.

Task 034E2 adapts both BaseVirtualAperture add/removeStatusCallback methods into
one bounded coalescing notification registration per public VA. C + safe Ada
provide Open, Wait, Statistics, explicit Unsubscribe and local Close. This is
change-notification coverage, not arbitrary callbacks/borrowed-object exposure
or atomic status-event snapshots. Callback arguments are ignored; provider reads
occur only through explicit E1 queries. Exact 0/ordinary/SIZE_MAX keys, synchronous
entry, stored-then-throw, preparation rollback, cached removal exceptions, ownership,
stop races, saturation and subscribe-first immutable reports are mock-tested.
Pinned Squall stores/erases callbacks but shows no emission path; integration is
registration/removal/lifecycle/no-delivery, not positive transition evidence.
ABI 0.1: 169 exports; raw Rust/private Python only; 804 vendor blobs unchanged.
No VADB, standalone LF/descriptor/DataPipe/TX/weights/RDMA/format expansion.
See `task-034e2-rf-va-status-subscriptions.md`.

Task 034E1 represents all six synchronous BaseVirtualAperture read/query methods
in production C and safe Ada: getID, getStatus, getInstanceStatus, getAllInstances,
getInstances and getInstanceStatusReport with complete nested LF status groups.
Claim-time VA info remains unchanged and separate. ABI 0.1 adds ten exports to
164; raw Rust/private Python parity only, vendor tree unchanged. Mock coverage
distinguishes each provider call, ordering/duplicates, all known/unknown enums,
fresh state, returned/requested ID mismatch, exceptions/allocation rollback,
parent-first calls and plain snapshot teardown. Squall configured query values
are positive data evidence, not hardware health/dynamic transitions/callbacks.
BaseVirtualAperture/VirtualAperture as a whole are not complete. Deferred:
at that checkpoint add/removeStatusCallback, VADB/unclaimed discovery, ElementGroupDescriptor/DataPipe,
standalone LocalFunction queries, weights/TX power calculations, broader
JobRequest/conditional/TX work, RDMA and additional ProductRx formats. See
`task-034e1-rf-va-status-queries.md` for validation evidence.

Task 034D3: JobDetail::extendJobEvent command coverage in native C and safe Ada,
with raw Rust/private Python parity only. One added export brings ABI 0.1 to 154;
804/804 vendor blobs unchanged. Exact signed duration/ID forwarding, repeatable
calls, full-Cancel-attempt rejection and unlocked parent-first ownership are
mock-tested. Provider-generated eventExtended feedback uses the existing status
API; no duration field is invented in its payload. Pinned Squall's no-op method
proves command-call/lifecycle/no-delivery only. Conditional commands, TX, pointing,
weights, RDMA, VADB and additional ProductRx formats remain deferred. Not complete
JobDetail/JobInterval or full RF MEL. See `task-034d3-rf-job-event-extension.md`.

Task 034D2: complete JobIntervalStatus/JobEventLogInfo payload reception in native
C and safe Ada, all 25 completion/8 trigger values, arbitrary binary activity,
exact signed seconds/femtoseconds, owned immutable events, bounded FIFO queue and
counters. Reporting Never/Always/OnException via additive v2 and safe Ada setter;
v1 layout/default frozen. ABI 0.1: 153 exports; vendor unchanged 804/804. Positive
callback payload/lifetime evidence is mock-only. Pinned Squall has no-op
registration/interval commands; timeout/stop evidence only, no scheduling claim.
At that checkpoint, event extension, conditional commands, TX/pointing/weights/RDMA/VADB and additional
ProductRx formats remain deferred. See `task-034d2-rf-job-interval-status.md`.
Earlier task paragraphs below describe historical checkpoints.

Task 034D1 RF RX JobInterval command foundation: native C + safe Ada, ordered
receive-event sequences, addJobIntervals / flush / cancelRemainingJobIntervals.
Mock-positive payload fidelity; pinned Squall lifecycle/call-path only because
provider methods are no-ops. Pinned continuation count 0 aliases ordinary zero
start; no scheduling claim. JobIntervalStatus callbacks, extendJobEvent and
TransmitEvent deferred. ABI 0.1: 146 exports; vendor unchanged 804/804. See
`task-034d1-rf-rx-job-intervals.md`. Historical checkpoints below retain their
own scope/counts.

Task 034C2: RF PhysicalData is native C + safe Ada, with complete antenna
dimensions, lattice angle and InstallationDetails (location XYZ, ForeignKey
key/system name, orientation and boresight). Owned point-in-time snapshots
survive DataMEL Close; nontrivial mock-positive and pinned-Squall evidence is
recorded in `task-034c2-rf-physical-data.md`. ABI 0.1: 139 exports; vendor 804/804.
Task 034C3 adds complete TxPowerModeData collection/direct snapshots in native
C + safe Ada; ABI 0.1 now has 143 exports, vendor unchanged at 804/804.
Mock-positive; pinned Squall receive-only/empty collection. Raw Rust sys/private
Python parity only. JobInterval remains unimplemented.

The published RFMFAInfo data/query surface is now represented in native C +
safe Ada for current scope: version, face set/count, receive/transmit support,
endpoint association, open additions, scheduler resolution, quantizeDuration,
lead/switching times, Rx/Tx/sample ranges, max user context bytes, supported
formats, PhysicalData and TxPowerModeData collection/direct. TxPowerModeData is
mock-positive; pinned Squall is receive-only/empty. This is not full RF MEL.

Task 034C1: RF quantizeDuration is native C + safe Ada, a live RFMFAInfo
operation validated with nontrivial mock-positive and pinned-Squall identity
tests. ABI 0.1 has 136 exports, vendor 803/803 unchanged. This is the timing
quantization primitive for future JobInterval support; no JobInterval is added.

Task 034B2B2: ABI 0.1 remains 135 exports; vendor 803/803 unchanged.
RF DataMEL, ProductRx, Admin, C2/VA, Job request/snapshot and Job
finalize/status/cancel: native C + safe Ada. Real ProductRx Job activation
uses production C and safe Ada; the test-only C++ helper was removed.
JobInterval, flush, and interval callbacks are not implemented. Historical
task coverage below describes its own checkpoint, not the current scope.
See `task-034b2b2-safe-ada-rf-job-lifecycle.md`.

Task 034B2B1: ABI 0.1 has 135 native production exports; vendor 803/803.
RF RX Job request/snapshot: native C + safe Ada. RF Job finalize/status/cancel:
native C only; safe Ada lifecycle deferred to 034B2B2. ProductRx activation
through a safe Ada Job is not implemented, and the test-only C++ job helper
is still present. See `task-034b2b1-native-rf-job-lifecycle.md`.

Task 034B2A2: ABI 0.1 remains at 132 exports; vendor 803/803 is unchanged.
Current scope (the 034B2A1 section below is its historical native checkpoint):

```text
RF DataMEL / ProductRx / Admin: native C + safe Ada
RF C2 / VirtualAperture:       native C + safe Ada
RF RX Job request:            native C + safe Ada, one RX group
RF JobDetail snapshot:        native C + safe Ada
RF Job finalize/cancel:       not implemented
RF ProductRx helper removal:  not performed
safe Rust / public Python RF: not implemented
```

See `task-034b2a2-safe-ada-rf-job.md`.

Task 034B2A1: ABI 0.1 has 132 exports; existing 126 remain unchanged.
The following describes the *current* native checkpoint (historical coverage
sections below retain their own starting baselines).

```text
RF DataMEL / ProductRx / Admin: native C + safe Ada
RF C2 / VirtualAperture:       native C + safe Ada
RF RX Job request:            native C only, exactly one RX group
RF JobDetail snapshot:        native C only
RF Job finalize/cancel:       not implemented (034B2B)
safe Ada RF Job API:          not implemented (034B2A2)
safe Rust / public Python RF: not implemented
```

See `task-034b2a1-native-rf-job-request.md`.

Task 034B1B2 extends the C ABI 0.1 to 126 production exports. The 034B1B1
section below is the historical starting baseline. See
`task-034b1b2-safe-ada-rf-virtual-aperture.md`.

```text
RF DataMEL:                  native C + safe Ada
RF ProductRx:                native C + safe Ada
RF Admin commandState:       native C + safe Ada
RF C2 lifecycle:             native C + safe Ada
RF requestVirtualAperture:   native C + safe Ada
RF VirtualAperture snapshot: native C + safe Ada
RF Jobs:                     not implemented (034B2)
safe Rust / public Python RF: not implemented
```

Task 034B1B1 adds RF C2 lifecycle in native C and safe Ada. ABI 0.1 now has
120 production exports. The 034B1A/034A/033D sections below are historical
baselines. See `task-034b1b1-safe-ada-rf-c2-owner.md`.

```text
RF DataMEL:              native C + safe Ada
RF ProductRx:            native C + safe Ada
RF Admin commandState:   native C + safe Ada
RF C2 lifecycle:         native C + safe Ada
RF VirtualAperture:      not implemented (034B1B2)
RF Jobs:                 not implemented (034B2)
safe Rust / public Python RF: not implemented
```

Task 034B1A adds RF Admin state command in native C and safe Ada, using the
previously committed 034B1 closure. ABI 0.1 now has 118 production exports.
The Task 034A and 033D statements below describe their historical baselines.

```text
RF DataMEL:              native C + safe Ada
RF ProductRx:            native C + safe Ada
RF Admin state command:  native C + safe Ada
RF C2/VirtualAperture:   not yet implemented (034B1B)
RF receive job control:  not yet implemented (034B2)
safe Rust / public Python RF: not implemented
```
See `task-034b1a-safe-ada-rf-admin.md`.

Task 034A adds safe Ada `AMS.MEL.RF` and `AMS.MEL.RF.Product_Rx` on the
unchanged native ABI 0.1 (115 production exports). Safe Ada covers the complete
**current native RF slice**, not the complete published RF MEL standard.

```text
RF DataMEL native C:                          complete current foundation
RF DataMEL safe Ada:                          complete current foundation
RF RFMFAInfo scalar/enum/range snapshot:      native C + safe Ada
RF ComplexINT16 ProductRx:                   native C + safe Ada
RF ProductRx safe Ada sample access:         native event lease; no additional
                                               bulk copy in With_Samples
RF other sample formats:                     not implemented
RF richer ProductRxMetadata:                 not implemented; native fail closed
RF RDMA:                                     not implemented
RF C2 / jobs / VADB:                         not implemented
safe Rust RF:                                not implemented
public Python RF:                            not implemented
```

Historical Task 033D coverage below describes the starting state, not the
post-034A safe Ada API. See `task-034a-safe-ada-rf.md`.

Task 033D implements the first RF MEL data-plane path. Native C now covers
asynchronous `ProductRxEndpoint` creation, ComplexINT16 receive into owned,
immutable events, a fixed representable `ProductRxMetadata` subset, a bounded
DROP-INCOMING queue, parent-first DataMEL lifetime, and a permanent
callback-registration holder. ABI 0.1 adds exactly nine exports (106 -> 115).
The raw private Ada FFI, `ams-mel-sys`, and private Python ctypes are
synchronized. At the Task 033D baseline there was no safe language RF API;
Task 034A adds safe Ada support as described above.

```text
RF DataMEL foundation:                          complete
RF RFMFAInfo scalar/enum/range snapshot:        complete
RF ProductRxEndpoint ComplexINT16 receive:      complete in native C
RF ProductRxMetadata:                           fixed representable subset complete;
                                                richer Pointing/ReceiveEvent/std::any
                                                profiles fail closed
RF ProductRxEndpoint other formats:             not implemented
RF RDMA:                                        not implemented
RF C2 / Jobs:                                   not implemented in production API
safe Ada/Rust/Python RF at 033D:                not implemented (Ada added by 034A)
```

The mock RF provider proves fidelity, fail-closed metadata, callback-lifetime,
and DSO-retention behavior, with destructive negative controls. The opt-in
`make test-squall-rf-rx` real Squall receive passed. See
`task-033d-rf-complex-int16-receive.md`.

Task 033C pins the RF ProductRxEndpoint receive declaration closure and records
the callback/buffer/lifetime contract. It is an evidence task only: 16
byte-identical RF MEL headers, a declaration-only compile probe, and the
`check_rf_product_rx_header_closure` check. There is **no** runtime receive
support. ABI 0.1 keeps exactly **106** production exports, and no Ada, Rust, or
Python surface changed.

```text
RF DataMEL foundation:                          complete (033B)
RF ProductRxEndpoint declaration closure:       pinned / measured (033C)
RF ProductRxEndpoint callback/lifetime contract: analyzed (033C)
RF ProductRxEndpoint runtime:                   not implemented (implemented by 033D)
RF ComplexINT16 receive:                        not implemented (implemented by 033D)
RF ProductRxMetadata mapping:                   not implemented (033D policy decided)
RF PointingType / ReceiveEvent mapping:         not implemented (fail closed in 033D)
RF RDMA / external endpoints:                   not implemented
```

See `task-033c-rf-product-rx-contract.md`.

Task 033B adds the first production RF MEL slice: a native C RF DataMEL
foundation (`createDataMEL` load, VersionInfo, an owned RFMFAInfo snapshot, and
shutdown-once Close with DSO lifetime ordering). It adds exactly six exports:
ABI 0.1, 100 -> **106** production exports. The raw Ada FFI, `ams-mel-sys`, and
private Python ctypes are synchronized. There is no safe language RF API.

```text
RF DataMEL:                                   native C foundation complete
RF version:                                   complete
RF RFMFAInfo scalar/enum/range snapshot:      complete
RF quantizeDuration:                          native C + safe Ada; live provider query
RF PhysicalData:                              native C + safe Ada owned complete snapshot
RF Tx power modes:                            native C + safe Ada snapshots
RF ProductRxEndpoint / IQ receive:            not implemented (033C: contract pinned only;
                                              superseded by 033D above)
RF RDMA:                                      not implemented
RF C2 / jobs:                                 not implemented
safe Ada/Rust/Python RF:                      not implemented
```

The mock RF provider proves positive behavior, and the pinned Squall RF smoke
passed (opt-in; not part of CI). See `task-033b-rf-datamel-foundation.md`.

Task 033A pins the RF MEL upstream and vendors the measured declaration
closure for the DataMEL + RFMFAInfo roots. It also inventories the pinned
Squall RF provider contract. It adds **no** RF runtime behavior:

```text
RF MEL declaration baseline:    Task 033A pinned/measured
RF production C API:            not implemented
RF safe Ada/Rust/Python:        not implemented
```

No production source, public header, or binding changed. ABI remains 0.1 with
exactly 100 production exports. See `task-033a-rf-mel-source-closure.md`.

Task 032B4 adds the **public Python** common Channel façade for the two
families that already have public Python typed owners.
`ControlChannel.channel_view()` and `ImageStream.channel_view()` return a weak
`ChannelView` that retains no Python source, Session, or provider object, with
KeepAlive (existing `ReturnRequest`), CommsTest (`CommsRequest` ->
`CommsCompleted`/`CommsRejected`), a complete owned `ChannelCapability`
(frozen dataclasses, tuples, and six loss-aware IntEnums), and idempotent
Close/context manager/finalizer. Final summary:

```text
native C common Channel:      C2/Image/Health/Instrumentation/Track
safe Ada common Channel:      C2/Image/Health/Instrumentation/Track
safe Rust common Channel:     C2/Image
public Python common Channel: C2/Image
safe Rust/Python Health/Instrumentation/Track: typed owners not implemented
```

Python surface only: no native, mock, `_native.py`, Rust, or Ada change. ABI
0.1, exactly 100 production exports, and 100 private ctypes bindings. See
`task-032b4-python-common-channel.md`.

Task 032B3 adds the **safe Rust** common Channel façade for the two families
that already have safe Rust typed owners. `ControlChannel::channel_view` and
`ImageStream::channel_view` create a weak `ChannelView` (`!Send`/`!Sync`, no
borrow of its source) with KeepAlive (existing `ReturnRequest`), CommsTest
(`CommsRequest`), a complete owned `ChannelCapability`, and idempotent Close.
Summary:

```text
native C common Channel:               C2/Image/Health/Instrumentation/Track
safe Ada common Channel:               C2/Image/Health/Instrumentation/Track
safe Rust common Channel:              C2/Image
safe Rust Health/Instrumentation/Track: no typed owners yet
public Python common Channel:          not implemented
```

No native, sys, Ada, or Python change: ABI 0.1, exactly 100 production
exports. See `task-032b3-safe-rust-common-channel.md`.

Task 032B2 adds the **safe Ada** common Channel façade for all five families.
`AMS.MEL.IR.Channel.View` is created only by `C2/Image/Health_Status/
Instrumentation/Track.As_Channel`, and it supports KeepAlive, CommsTest,
ChannelCapability, and weak view Close with no raw handle in the public API.
Summary:

```text
native C common Channel:   C2/Image/Health/Instrumentation/Track complete
safe Ada common Channel:   C2/Image/Health/Instrumentation/Track complete
legacy Ada C2.Common:      preserved (spec, body and tests byte-identical)
safe Rust:                 C2/Image only (Task 032B3)
public Python:             no generic Channel facade yet
```

ABI stays 0.1 with exactly 100 production exports; only the test mock changed
natively. See `task-032b2-safe-ada-common-channel.md`.

Task 032B1 promotes the proven 032A weak common Channel access into the
**native public C ABI** for C2, Image, Health, Instrumentation and Track:
five weak conversions plus KeepAlive, CommsTest, ChannelCapability and view
Close. ABI stays 0.1; the production inventory is exactly **100** exports
(+9). Raw Ada FFI, Rust sys and private Python ctypes declarations are
synchronized. (Safe Ada followed in Task 032B2; safe Rust C2/Image in 032B3.)
Public Python has no common Channel façade. See `task-032b1-public-common-channel-abi.md`.

Task 032A4 adds test-only Instrumentation and Track common
KeepAlive/Comms access for Attached and Enabled, with the existing family
request counters and shared Session permit. Focused C11 tests cover weak idle,
parent-first, finish-exception and detach-failure retention, mixed typed/common
accounting including direct weak-observer counts after logical Close,
metadata callback quiescence, Close-wins KeepAlive/Comms throws, four
cross-family admission directions, and permanent post-send retention.
Fresh GCC Debug/Release suites each have 151 tests and pass full repeat-50;
see `task-032a4-common-access-instrumentation-track.md`.

Task 032A3 adds test-only weak Health common access and request accounting:
C11 evidence covers held KeepAlive/Comms, weak idle, Attached/Enabled deferred
cleanup, pending-request callback quiescence and logical metadata stop,
Close-wins send throw, both limit-one C2 admission directions and permanent
failure retention. No public common view; see `task-032a3-common-access-health.md`.
The Task 030B zero-copy row below describes its historical 90-export snapshot;
the current post-031B production inventory is 91 exports.

Task 032A1 is merged (PR #47). Task 032A2 adds test-only weak common access
for inherited C2/Image KeepAlive and Comms, sharing the existing Return/Comms
engines, Session admission and Image Navigation request/cleanup accounting.
032A2 did not implement inherited services for Health, Instrumentation or
Track; see `task-032a2-common-access-image.md`.

| Area | Status | Evidence / next step |
|---|---|---|
| C ABI header usable from C11 | Implemented | `native/tests/test_c_abi.c` |
| Native facade version query | Implemented | C and C++ tests |
| Private Ada import and public version value | Implemented | `ada/tests/src/ams_mel_smoke.adb` |
| Native CMake package installation | Implemented | Exported target and config files |
| Local Alire dependency manifests | Development only | Relative pins; validate on installed Alire |
| Upstream version inventory | Verified through task 003 | `upstream-provenance.md` exact commits/trees |
| Verified upstream dependency closure | Implemented for IR receive/C2/Health/Instrumentation slices | 94 unmodified headers; no upstream compiled source |
| Separately loadable mock C++ MEL provider | Implemented, test-only | Native C and Ada contract tests |
| Real MEL factory/resource adaptation | Control foundation implemented | load/factory/init/version/close only |
| Buffer and callback lifetimes | PR #43 deferred-cleanup correction implemented | Explicit callback Buffer transfer; one release executor through final wrapper destruction; shared external cleanup helper; public Close, both snapshot wait schedules and Navigation late-work evidence; direct destructor premature-claim and lost-cleanup observation/rescue mutations. Native and Ada discard Fail/throw checks; test observer/rescue absent from production. See corrective protocol for ownership audit and validation |
| IR image receive | Mono8 receive implemented | Host-memory Mono8 only; legacy subset plus complete owned snapshot in C/Ada |
| IR C2 required sends | Complete in native C and safe Ada | General ModeCmd/complete ScanParam, intended BIT choices, and ConfigSet; existing request owners reused |
| IR C2 safe Rust/Python subset | Constrained existing subset | Operate/TaskSched and BIT no-op only; no safe Task-017 expansion |
| Pending C2 lifetime | Hardened and lifecycle-tested | Pre-send allocation; allocation-free emergency roots; launch/allocation failure injection; deferred disable/detach/unload |
| RequestFor completion scalability (Task 031A) | Characterized with mock provider | C11 seven-type N=100 success and stored-exception waves across six engines; invalid futures, expired weak results, empty Completion graphs, exact physical teardown and eventual WorkerInput reclamation. Retained-handle cached-result/exception and non-cancelling timeout tests. Separate non-gating five-sample Debug and three-sample Release C2-only and mixed benchmarks, with external monotonic physical-teardown timestamps after the mixed benchmark releases its own DSO handle, show 100 blocked workers and approximately 100 additional OS threads. WorkerInput destruction is not the provider-safety boundary; see task document. No real-provider scalability result |
| Bounded completion admission (Task 031B) | Implemented; PR #46 merged | Native implementation; isolated options-zero N=100, limit-1 cross-family/reverse-owner/request-close/post-get refusal with direct four-domain request-accounting checks, provider-independent admission observation, parent-close lifetime, two-Session isolation, barrier-started eight-caller limit-4 race, seven-type limit-8 N=100 caller-retry regressions. Bounded synchronous throw recovery across all four accounting domains; Return/Navigation stored-exception cached results and capacity recovery; both permanent-retention failure classes across all seven types; provider InsufficientResources distinguished from bridge ResourceExhausted. GCC Debug/Release 56/56; healthy failure scenarios repeated 30 times. Safe Ada/Rust/Python admission tests pass. Five Debug and three Release limit-eight samples verify 100 sends/gets, zero final workers and peak eight. See task document |
| Internal common Return/Comms engine (Task 032A1) | Merged PR #47; no public common Channel API | Move-only erased request claim and C2 finish adapter; C11 isolated Return/Comms finish-exception tests prove permanent retention after an unexpected adapter exception. Task 032A2 reuses both engines from test-only C2/Image common access; Health, Instrumentation and Track adapters remain future work |
| Weak common Channel access (Task 032A2) | C2 and Image, test-only; no public ABI change | C11 attached/running, weak-idle teardown, parent-first, shared limit-one admission, send-throw, permanent retention, finish-exception and post-decrement race tests; GCC Debug/Release 82/82, 24 common scenarios repeated 30 times in each; see task document |
| C2/image coexistence | Implemented | One Session, parent-first close, provider unload last |
| IR C2-specific required metadata | Complete in native C and safe Ada | BIT_Configuration, CommandStatus, and BIT_Status; complete ordered nested values through one queue |
| C2 application-facing inherited Channel services | Complete in native C and safe Ada | KeepAlive; CommsTest async reply and callback; complete owned ChannelCapability; valid before Enable |
| IR Health/Status required metadata | Complete in native C and safe Ada | Six required callbacks; complete MFA/BIT/subsystem/name-value/SecurityAudit values; bounded owned polling queue |
| Explicit generic buffer management | Not application-exposed | IRSTImage registration remains adapter-managed; future resource task |
| Optional/conditional C2 commands | Not implemented | No calibration, camera, or erase commands. The optional Track `SystemTrackDataResponse` send is implemented separately on the Track channel, not here |
| Bounded receive queue | Implemented | Caller capacity; DROP-INCOMING; saturating counters |
| Legacy FrameHeader interface | Compatibility subset retained | `ams_mel_ir_frame_v1` and `AMS.MEL.IR.Receive` unchanged |
| Full FrameHeader snapshot | Complete in native C and safe Ada | Owned snapshot, contributing sensor, ordered flags, inertial/nav vectors and independent orientations |
| High-rate zero-copy Ada frame lease | Implemented in safe Ada (Task 030A) | `AMS.MEL.IR.Image.Frame_Lease` with `Acquire_Frame`/`Is_Open`/`Close`/`Pixel_Count`/`With_Pixels`/`Copy_Pixels` plus lease metadata accessors. Limited `Ada.Finalization.Limited_Controlled` owner of exactly one `ams_mel_ir_frame_snapshot`; idempotent close, harmless finalization after close, no leak on failed acquisition or metadata-conversion exception. `With_Pixels` binds a constrained `Pixel_Array` directly to `view.pixels.data` with an address clause, so there is no payload-sized Ada allocation, no `memcpy` from the snapshot, no per-pixel FFI call, and no `Ada.Containers` pixel Vector on this path; `Acquire_Frame` and `With_Pixels` setup are both O(1) in pixel count. Null-pointer-with-nonzero-size and out-of-Ada-index-range spans fail closed with `Provider_Error`; zero-length payloads are valid. Multiple outstanding leases are supported and each owns distinct storage; a lease survives queue advance, stream Stop, stream Close, Session close, and provider teardown. Deterministic alias evidence: a test-only facade address log plus an Ada comparison requiring the borrowed view's first address to equal the native snapshot pixel address, with a C-side storage-identity test. No C ABI change; ABI version stays `0.1` and `exports.map` is unchanged. No safe Rust or public Python zero-copy API |
| MEL provider-buffer to Ada zero copy | Implemented in native C and safe Ada (Task 030B) | The bridge performs **zero** bulk payload copies from the MEL callback buffer into Ada. `frame.pixels.assign(...)` and the payload-sized `std::vector<std::uint8_t>` are removed; `QueuedFrame` now owns the callback's `std::shared_ptr<irmel::Buffer>` plus the validated image address and byte count, and the snapshot retains the provider graph so the host storage, Buffer object, and provider library all outlive the borrow. Defining deterministic proof: `Buffer::getImageAddress() == snapshot pixels.data == Ada With_Pixels first-element address`, asserted natively and from Ada through a test-only address log with no production ABI change. A provider buffer accepted by the bridge is released exactly once by bridge logic: the callback releases on malformed, not-accepting, and queue-full rejections and is explicitly disarmed on acceptance; the later release happens in legacy `Receive`, snapshot/lease close, or Close-time queue discard, never via a provider destructor. `CallbackState::retained_frames` counts queued frames plus live snapshots, is guarded by the single teardown mutex, and is not derived from queue length. Physical teardown now requires `navigation requests == 0 AND retained_frames == 0 AND !uncertain_release` through the existing Image cleanup state machine; Stop is logical-now/physical-later and never blocks on an application-held lease, Close discards still-queued frames and releases their buffers while preserving live leases, and a live lease survives public stream and Session close because the actual provider unload is deferred, not because bytes were copied. `Buffer::release()` runs outside the lifecycle mutex (lock to claim, unlock to release, lock to publish); a failed or throwing release moves the exact callback Buffer into its dedicated stream-owned retention slot, never retries, and is reported as `AMS_MEL_PROVIDER_FAILED` / Ada `Provider_Error` from explicit close while `Finalize` stays non-raising. The current PR #43 protocol uses `retention_slots` for HOLD ownership, a single release executor with an exact uncertain-owner slot for the active provider call, and preallocated `deferred_releases` for callback reentry. The old `release_slots` (`2 * buffer_count`) / `begin_release()` model was superseded; see `docs/corrective-provider-buffer-release-handoff.md` for that historical defect and the PR #43 lifecycle correction. Deterministic regressions force the exact release/reuse interleaving with condition variables rather than sleeps -- a release paused after successfully republishing the buffer, and reuse driven from inside the still-executing release call with the provider pool mutex released -- and prove the forced window was reached, the new callback is accepted normally at the returned buffer's address, live siblings keep their addresses and bytes, no false malformed/queue-full/uncertain state appears, each wrapper gets exactly one release attempt, capacity is restored, and channel destruction precedes provider unload; coverage includes legacy `Receive`, Close-time queue discard, concurrent release from distinct snapshot owners, single-executor release admission/backpressure, an Ada `Frame_Lease` regression through the public safe API, and a negative control proving removal of the handoff makes them fail. Uncertain release is handled identically on **every** path including callback-side rejections before queue acceptance: an accepted frame keeps its existing count, a rejected frame gains one, both permanently, so teardown is blocked either way. Deterministic regressions cover the malformed, queue-full, and not-accepting rejection arms plus retention-slot exhaustion, each proving exactly one release attempt, the exact wrapper alive with destructor count 0, host bytes intact, provider channel/`Control` never destroyed, and both public owners closable without freeing the graph; a negative control proves those assertions are non-vacuous, and a 96-buffer/200-cycle test exceeds the old 64-slot assumption. Backpressure is intentional and deterministically tested: with `Buffer_Count = 3`, three retained leases block all reuse, releasing exactly one lease frees exactly one buffer, and a provider-side "no buffer available" event is not counted as a bridge queue-full drop. **Not** end-to-end zero-copy: Squall still copies received UDP bytes into the registered MEL host buffer, and `Receive`/`Full_Frame`/`Copy_Pixels` copy by contract. No C ABI change; ABI stays `0.1`, all 90 exports and `exports.map` unchanged. No safe Rust or public Python zero-copy API; no RF/RDMA/GPU/FPGA/Stacked Image |
| ImageChannel metadata callbacks | All required callbacks implemented in native C and safe Ada | BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp share one bounded DROP-INCOMING queue; owned events, counters, malformed/allocation recovery |
| NavigationReport send | Implemented in native C and safe Ada | Complete published NavigationReport/all 18 covariance terms; async request/future, deferred teardown while pending, allocation-free emergency retention; ABI 0.1 now 59 exports |
| Real IR provider validation | Implemented and passed | Ada adds required C2-specific metadata with 10/0/0 counters plus general mode/rejection, BIT payload, and ConfigSet; all languages retain BIT no-op, TaskSched, and three 320x200 Mono8 frames |
| IR Instrumentation conditional surface | Complete in native C and safe Ada | send(InstrumentationLevelCmd) and InstrumentationReport callback plus Enable and ChannelCapability; positive behavior mock-validated; pinned Squall explicitly unsupported |
| Public common Channel C ABI (Task 032B1) | Native public C: C2/Image/Health/Instrumentation/Track; safe Ada in Task 032B2 | Weak `ams_mel_ir_channel` view; KeepAlive, CommsTest and ChannelCapability through the shared 032A engines, Session admission and family accounting; view Close only drops the wrapper. C11 `test_public_common_channel` (46 cases). Generic Enable/Disable, buffer registration and common metadata callbacks are not exposed |
| Instrumentation inherited generic Channel services | Native public C (Task 032B1) and safe Ada (Task 032B2) | KeepAlive/CommsTest/ChannelCapability through `ams_mel_ir_channel_*` and `AMS.MEL.IR.Channel` via `Instrumentation.As_Channel`; typed Submit remains Enabled-only; no safe Rust/public Python common façade yet; buffers not exposed |
| Safe Ada common Channel façade (Task 032B2) | Complete: C2/Image/Health/Instrumentation/Track | `AMS.MEL.IR.Channel.View` (parent-private controlled owner of the weak native view, created only by five `As_Channel` conversions); `Send_Keep_Alive`/`Submit_Comms_Test`/`Capabilities`/`Close` with own Return/Comms request and result owners; canonical `Channel.Command_ID` with `C2.Command_ID` compatibility subtype; legacy `C2.Common` byte-identical. `AMS_MEL_IR_Channel_Tests` covers Attached/Enabled (Image Attached/Running), high-ID Comms, Return::Fail, long rejections, timeout/cached Wait, parent-first, weak/close-first/multiple views and snapshot lifetime |
| Track channel foundation | Complete in native C and safe Ada | Open/Enable/ChannelCapability/Close; lifecycle, rollback, detach retry, and emergency retention mock-validated; pinned Squall validated only for clean unsupported-provider Open failure |
| Track @RequiredIfTrack core | Channel lifecycle complete; `IRSTTrackReport` callback complete | Native C + safe Ada; positive behavior and complete report payload mock-validated; pinned Squall clean unsupported-provider behavior validated (C `AMS_MEL_FACTORY_FAILED`, Ada `Provider_Error`, both `attachChannel returned null`, same Session continues) — no positive Squall Track execution or Track-report evidence |
| TrackDataUpdate `@RequiredIfTrackUpdate` | Complete in native C and safe Ada | `TrackChannel::send(TrackDataUpdate)` with complete field fidelity, including all 21 covariance terms, both epoch-second times, and the canonical Directional ECEF vectors; async request ownership, deferred Track cleanup, and provider-failure handling are mock-validated. Pinned Squall cannot attach Track, so it provides no positive TrackDataUpdate evidence. No safe Rust or public Python Track API |
| SystemTrackDataResponse `@Optional` | Complete in native C and safe Ada | `TrackChannel::send(SystemTrackDataResponse)` with complete field fidelity: signed nanosecond system time, verbatim range/rangeRate/rangeError/rangeRateError, both validity bools restricted to 0/1, and both AzEl pairs through the one canonical AzEl representation. Shares the single Track request-accounting domain with TrackDataUpdate, proven by a deterministic mixed-request regression. Positive behavior is mock-validated; pinned Squall cannot attach Track, so it provides no positive SystemTrackDataResponse evidence. No safe Rust or public Python Track API |
| Optional Track `RequestSystemTrackData` | Complete in native C and safe Ada | Upstream declares this `@Optional` type only as a `registerMetadataCallback` overload, with no `send`; delivered through the existing bounded DROP-INCOMING Track metadata queue alongside `IRSTTrackReport`. All four fields verbatim, signed nanosecond time; an optional-callback refusal never breaks the required report callback. Mock-only positive evidence |
| CandidateObjectMessage `@RequiredIfDetectCandidateObjects` | Complete in native C and safe Ada | Inbound metadata only: the pinned `TrackChannel` declares no `send(CandidateObjectMessage)` and no `RequestFor<CandidateObjectMessage>`, so it shares the one bounded DROP-INCOMING Track metadata queue, capacity, and counter set with `IRSTTrackReport` and `RequestSystemTrackData`. Complete payload fidelity: the header including binary32 `CFAR` and the undecoded validity bitfield, the complete `HotRegion` vector in published order, the canonical `SensorInertialState`, and exactly `numberOfCOs` candidate objects from the upstream fixed 900-entry array. `numberOfCOs > 900` and a `HotRegion` enum outside `0..3` are malformed. Registered only when the channel advertises `ChannelMetadataCapabilityType::CandidateObjectMessage`; when advertised, any non-`Success` return including `NotSupported` fails the open closed. Event-owned span storage survives provider channel destruction and library unload. The payload lives in the new `ams_mel_ir_track_metadata_event_v2` record, reachable only through the new `ams_mel_ir_track_metadata_event_view_v2` export: `ams_mel_ir_track_metadata_event_v1` is frozen at `kind`/`track_report`/`request_system_track_data` and `ams_mel_ir_track_metadata_event_view` is unchanged, so existing v1 consumers need no recompilation. Positive evidence is mock-only; pinned Squall cannot attach Track. No safe Rust or public Python Track API |
| CandidateObjectPreProcMessage `@Optional` | Complete in native C and safe Ada | Inbound metadata only: the pinned `TrackChannel` declares no `send(CandidateObjectPreProcMessage)` and no `RequestFor<CandidateObjectPreProcMessage>`, so it shares the one bounded DROP-INCOMING Track metadata queue, capacity, and counter set with the other three kinds as metadata kind 4, in deterministic four-kind FIFO order. All 17 published `CandidateObjectPreProc` getters plus all 3 message getters are mapped exactly once; the 3x3 `int16_t` background patch is carried as a fixed row-major nine-element record, each nested `SensorInertialState` is copied, `edge` is normalized to exactly 0/1, and nothing is clamped, normalized, or decoded (`candidateObjectQuality` is deliberately NOT clamped to 0..1). The upstream container is a `std::vector`, and no invariant tying it to `numberOfCOs` is published, so the header count is copied verbatim and the COMPLETE vector is copied at its actual size with no truncation and no mismatch rejection. A null payload or a `HotRegion` enum outside `0..3` is malformed. Because the callback itself is `@Optional`, `Return::NotSupported` is non-fatal and metadata open continues; `Fail`, `BadPointer`, `NotImplemented`, and any future value fail closed. Event-owned span storage survives provider channel destruction and library unload. The payload lives in the new `ams_mel_ir_track_metadata_event_v3` record, reachable only through the new `ams_mel_ir_track_metadata_event_view_v3` export: v1 and v2 are both frozen and their view operations are unchanged, so existing consumers need no recompilation. Positive evidence is mock-only; pinned Squall cannot attach Track. No safe Rust or public Python Track API |
| Other Track optional/conditional surfaces | None remaining | Every published TrackChannel-specific surface is implemented; the Track API is complete in native C and safe Ada |
| RF MEL declaration baseline | Task 033A pinned/measured | RF MEL `762ce84c` DataMEL/RFMFAInfo/RFMEL/RFCreateFunctions declaration closure vendored (GCC 522, Clang 523, union 524), plus the AMS VITA headers it reaches. Declaration-only probe and closure checker; no RF code in `ams_mel_c` |
| RF DataMEL (Task 033B) | Native C foundation complete | `ams_mel_rf_data_open` (`createDataMEL`, exact pinned `fnDataMEL` type, exceptions contained, DSO never leaked), `ams_mel_rf_data_close` (owner consumed, `shutdown()` exactly once, DataMEL destroyed before DSO unload, throwing shutdown permanently retains the graph). Separate from the IR Session. Mock-proven; pinned Squall C smoke passed |
| RF version (Task 033B) | Complete | `ams_mel_rf_data_get_provider_version` shares the IR VersionInfo helper; Squall reports 1/1/`Squall`/`Squall Simulator RF MEL` |
| RF RFMFAInfo scalar/enum/range snapshot (Task 033B) | Complete | Owned, immutable, point-in-time `ams_mel_rf_mfa_info`, independent of provider lifetime: `getNumFaces` and `getFaceIDs` preserved separately, open additions, scheduler resolution (fs), context bytes, raw JobDataFormat set, and for each reported face the three booleans, eight femtosecond durations, and Rx/Tx/sample frequency ranges (Hz, verbatim) |
| RF quantizeDuration | Native C + safe Ada | Live RFMFAInfo operation; mock-positive and pinned-Squall validated; excluded from the snapshot |
| RF PhysicalData | Native C + safe Ada | Complete antenna dimensions, lattice angle, InstallationDetails and ComponentLocation ForeignKey; owned point-in-time snapshot, mock-positive and pinned-Squall validated |
| RF Tx power modes | Native C + safe Ada | Complete TxPowerModeData collection/direct snapshots; mock-positive; pinned Squall is receive-only and reports an empty collection |
| RF ProductRxEndpoint / IQ receive | Not implemented | Never called by 033B |
| RF RDMA | Not implemented | `registerExternalRxEndpoint` never called |
| RF C2 / jobs | Not implemented | No other RF MEL family |
| RF safe Ada/Rust/Python | Not implemented | Raw private Ada FFI, `ams-mel-sys`, and private `_native.py` only (inventory 106) |
| RF apertures/jobs/receive/VADB | Not implemented | Later phase. Squall evidence: ComplexINT16 `ProductRxEndpoint` receive exists but copies and decodes UDP payloads (no zero-copy claim); RDMA, cached waveform, and TX endpoints return `Unsupported` |
| OMS/UCI application integration | Not implemented | Separate project concern |
| Rust sys binding | Complete for the current project C ABI | Exactly 100 C functions (Task 032B1 adds the opaque `AmsMelIrChannel` and nine raw common Channel declarations; the safe C2/Image façade is Task 032B3, sys unchanged), including `ams_mel_ir_track_metadata_event_view` (frozen v1), `ams_mel_ir_track_metadata_event_view_v2` (frozen v2), and `ams_mel_ir_track_metadata_event_view_v3`; raw Instrumentation and complete Track declarations/constants synchronized, including the Track report/event layouts, the complete TrackDataUpdate/covariance/result layouts and update-request handle, and the complete SystemTrackDataResponse/result layouts and system-response-request handle; no safe Instrumentation or Track API |
| Safe Rust binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op + common Channel (C2/Image) implemented | Typed Return values/results and reusable ReturnRequest (C2 BIT and common KeepAlive); weak `ChannelView` from `ControlChannel`/`ImageStream` with CommsTest (`CommsRequest`) and a complete owned `ChannelCapability` (Task 032B3); no safe Health/Instrumentation/Track owners or views; no payload-bearing BIT or additional C2/RF API |
| Real Squall Rust validation | Implemented and passed | Same pinned Task-004 provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 frames, counters, and explicit teardown through safe API |
| Python binding | Session + IR Mono8 + C2 Operate/TaskSched + BIT no-op + common Channel (C2/Image) implemented | Complete current 100-function private ctypes binding. Weak `ChannelView` from `ControlChannel`/`ImageStream` with KeepAlive (reused `ReturnRequest`), CommsTest (`CommsRequest`), and a complete owned `ChannelCapability` (Task 032B4); no public Health/Instrumentation/Track owners or views; no public Image metadata/Navigation/Instrumentation/Track methods |
| Real Squall Python validation | Implemented and passed | Same pinned provider/runtime; BIT Success, TaskSched, parent-first close, both cached waits, real 320x200 Mono8 `bytes` frames, counters, and explicit teardown |
| Additional Python C2/RF | Not implemented | No payload-bearing BIT, scan/config/camera commands, callbacks, or RF; no zero-copy/NumPy views |
| Python packaging/publication | Not performed | `PYTHONPATH=python` development use only; no wheel or PyPI dependency |
| Alire/crates.io publication | Not performed | Rust crates also remain unpublished |
| Formal verification / GRA compliance | Not claimed | Separate evidence required |

## Ada IR feature-family progress

| Feature family | Ada status |
|---|---|
| C2 required sends | Complete |
| C2-specific required metadata callbacks | Complete: BIT_Configuration, CommandStatus, BIT_Status |
| Common inherited Channel services | Complete for all five families through `AMS.MEL.IR.Channel.View` (Task 032B2): KeepAlive, CommsTest, ChannelCapability, weak view Close. Legacy C2-specific `AMS.MEL.IR.C2.Common` (including the CommsTest callback via C2.Metadata) is preserved unchanged |
| Explicit generic buffer management | Not application-exposed |
| Image receive | Partial: host-memory Mono8 |
| Image metadata | BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp complete |
| Scheduling | Not implemented |
| Track | `@RequiredIfTrack` core complete: channel foundation (`Open`/`Is_Open`/`Enable`/`Capabilities`/`Close`) plus the complete `IRST_Track_Report` and `AMS.MEL.IR.Track.Metadata` bounded DROP-INCOMING queue for the `IRSTTrackReport` callback with a blocking receive/wait (timeout zero is the nonblocking poll case). `@RequiredIfTrackUpdate` `TrackDataUpdate` complete via `AMS.MEL.IR.Track.Updates`. Optional `SystemTrackDataResponse` complete via `AMS.MEL.IR.Track.System_Data`. Optional inbound `RequestSystemTrackData` complete via `AMS.MEL.IR.Track.Metadata.Receive_Event`, which shares the one bounded queue with `IRSTTrackReport`. Positive behavior/payload evidence is mock-only; pinned Squall is validated only as a clean unsupported provider (`Provider_Error: attachChannel returned null`). `@RequiredIfDetectCandidateObjects` `CandidateObjectMessage` complete via `AMS.MEL.IR.Track.Metadata.Receive_Event`, which shares that one bounded queue; its owned `Candidate_Object_Message` copies every hot region, every candidate object, and every scalar into Ada storage before the native event is closed, and its Track-facing value types are deliberately independent of `AMS.MEL.IR.Image`. The `@Optional` `CandidateObjectPreProcMessage` is likewise complete via `AMS.MEL.IR.Track.Metadata.Receive_Event` on that same queue; its owned `Candidate_Object_PreProc_Message` copies the header, the message-level inertial state, every hot region, every PreProc entry with all nine background samples and its own nested inertial state, and a safe `Edge : Boolean`, all into Ada storage before the native event is closed. Every published TrackChannel-specific surface is therefore represented and the safe Ada Track API is complete |
| Health/Status | Complete: required channel plus six required callbacks; LFStatus/NUC_TempData excluded |
| Instrumentation | Instrumentation-specific conditional surface complete (send/InstrumentationReport callback plus Enable and ChannelCapability); inherited KeepAlive/CommsTest/ChannelCapability through `AMS.MEL.IR.Channel` via `Instrumentation.As_Channel` (032B2) |
| StackedImage | Not implemented |
| RF | Task 034A: `AMS.MEL.RF` and `AMS.MEL.RF.Product_Rx` implement the complete current native RF DataMEL/MFA snapshot and ComplexINT16 ProductRx slice; other formats, richer metadata, RDMA and jobs/C2 remain outside the generic library |

Implementation and verification are different. See `bootstrap-validation.md`
for the commands actually executed when this starter archive was prepared.
