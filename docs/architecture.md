# Architecture decisions

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


Task 034F4 extends only the existing RX Add path with ordinary prepared spatial
values, not provider owners, claims, workers or lifetime paths. Receive-event v2
adds uint64 stab index and ordered size_t-compatible group indices; explicit
JobInterval v3 carries the existing scalar/status fields, shared F2 Pointing span
and the v2 event span. All validation/value construction precedes the single
provider Add. The same F2 helpers implement both JobRequest and JobInterval.
Safe Ada uses its existing Pointing representation/serializer and final-sized
Ada-owned borrowed backing; old source defaults to zero index/empty vectors.
v1/v2 leave pinned spatial defaults untouched. No index-count relationships,
capability queries, quantization or TX are inferred. Existing status gate remains.
Pinned Squall Add is a no-op; fidelity/spatial semantics are mock evidence only.
JobInterval setApplicableElementGroups writes applicableElementGroupLabels while
getApplicableElementGroups returns dataPaths; binding this mismatch and interval
endpoints is explicitly deferred. See `task-034f4-rf-interval-spatial-controls.md`.

Task 034F3 extends the existing prepared-group builder and submit_prepared path,
not the async/claim/retention architecture. V4 carries one ordered nonempty mixed
RX/TX sequence with a fixed FFI envelope; only the active mode payload is read.
Strings, frequency vectors, RX endpoint sets/pointings, common binary IDs/instance
vector/power-mode set and estimated pointing are prepared before provider-parent
acquisition. RX preserves mode/duty/frequency/endpoint/pointing order; TX uses
mode/duty/TxPowerLevel/frequency order, never receive-only setters. Each occurrence
creates/adds exactly one command. Mismatch fails without requestJob or fallback;
command/JobRequest destruction precedes VA/C2 claim release. No hidden E1-E6/MFA/
Weights query or capability auto-gate. Safe Ada uses a private variant vector for
global append order and privately serializes v4; frozen C v1/v2/v3 remain. TX
requirements describe/reserve groups only, not TransmitEvent or TX intervals.
Positive TX construction is mock evidence; receive-only pinned Squall's valid TX
input fails the command mode check, then its RX flow remains positive evidence.
Vendor unchanged. See `task-034f3-rf-job-tx-groups.md`.

Task 034F2 adds ordinary bridge-prepared PointingType values before provider-parent
acquisition, not provider pointing owners. A fixed C record uses stable bridge
tags 0..4 for the exact pinned variant alternatives ECEF/LLA/platform/face/baseline.
Only active fields are read/validated; only ECEF/LLA UTC needs canonical fractions.
Every ECEF and NED c_vector component is explicitly assigned; default ECEF numeric
storage is never read or claimed zero. No geodesy, coordinate/unit conversion,
normalization, finite checks or capability gate occurs. Ordered group pointings
follow duty/frequency/endpoint calls and precede addElementGroup; estimated stab
point is independent and set only when enabled. Frozen v1/v2 and additive v3 share
the exact submit_prepared worker/retention/publication path. Safe Ada owns numeric
values/vectors and final-sized raw backing privately, with default no-point behavior.
TX commands/execution, MFADrivenControls/JIB and rejection callbacks/context,
Weights/resources, external/RDMA and VADB remain deferred. Pinned Squall stores
expected points but requestJob does not inspect either pointing field; positive
fidelity is mock evidence, real-provider acceptance is narrower. Vendor unchanged.
See `task-034f2-rf-job-pointing.md` for measured validation.

Task 034F1 adds an RX JobRequest v2 profile without a second request owner.
Both submit exports share parent acquisition, requestJob/future validation,
worker launch, publication, abandonment and emergency retention. Strings,
frequency vectors, endpoint sets, instances, binary IDs and power-mode sets are
prepared before claiming the provider parent. Completion/input/public owner/
worker holder allocations precede requestJob. Builder-local commands and the
JobRequest unwind before VA/C2 claim release on synchronous failures, including
middle/final group failures. Every group is created by label, checked RX and
configured in caller order; repeated group/pipe labels are not deduplicated and
each pipe entry produces one setter call. No E3/E5/E6 lookup or capability check.
Safe Ada stores ordinary ordered group/pipe vectors and uses final-sized raw
backing only privately during Submit_Job; controlled strings clean up on errors.
UTC_Time construction validates canonical fractional femtoseconds and never
normalizes. Old one-group configurations retain upstream v1 defaults even though
Ada now submits through v2. Pointing and MFADrivenControls fields remain untouched;
TX groups, resources, callbacks, external/RDMA and VADB are outside F1. Pinned
Squall iterates RX groups but ignores added scheduling/identity fields except
requestId; acceptance is not semantic fidelity evidence. See the F1 task report.

Task 034E6 exposes getTxRadiatedPower, getTxPeakRadiatedPower, getTxApertureGain
and getMaxTxAttenuation as independent @RequiredIfTransmit scalar queries. The
existing VA claim protects each synchronous exact-once call outside C2 lifecycle,
notification, Job and interval-queue locks; same-VA operations including Close
remain externally serialized. No new claim, owner, worker, registration or DSO
pin; scalar copies outlive unload. No group/descriptor/Weights lookup, capability
auto-gate, local formula, reconciliation, caching or retry. Stable uint64 group
and WeightType IDs check provider size_t representability before entry; doubles
pass without physical validation, finite checks, normalization or unit conversion.
Safe Ada uses ordinary Long_Float and IDs with narrowly scoped C-double conversion,
not project-wide validity changes. Mock-positive evidence is independent of
receive-only pinned Squall's zero-return call-path evidence. Vendor unchanged.
See `task-034e6-rf-tx-power-queries.md`; VirtualAperture is not complete.

Task 034E5 uses a thick key-addressed synchronous model, not a public DataPipe
resource owner. Each explicit snapshot or mutation obtains VA::getDataPipes once,
independent of E3 descriptor pipes. Mutations copy inputs (including upstream set
conversion) before that query, select exact outer/inner keys, reject a null target,
and invoke one exact method once. No sibling claim, worker, callback, resource/DSO
pin or lock is added; live VA's existing claim permits parent-first operations.
No capability auto-gate/cache/retry/readback or association-persistence guarantee.
Snapshots contain only final-sized bridge strings/vectors/records; outer keys
sort by unsigned UTF-8 bytes, inner std::map traversal and ascending endpoint sets
remain. Aliases preserve each occurrence; lookup keys differ from returned labels.
Checked Ada copy-out closes a controlled native temporary before ordinary values
return. Actual no-registration unload tests distinguish E5 from E2's intentional
permanent callback shell. Squall's fresh object-local pipes supply returned-value
and mutation-return evidence, never RDMA/Q-pairs/hardware routing. See
`task-034e5-rf-datapipe-association.md`.

Task 034E4 adds four exact synchronous provider query families under the existing
VA claim, without holding lifecycle/notification/Job/queue locks. Required
cached-waveform/dynamic-weights Booleans are not cached and False is data.
Conditional LF catalog/status calls are independent of Claim, E1 reports, E2
callbacks and E3 descriptors. Catalog map ordering/zero counts and vector
ordering/duplicates survive copying; no cross-call reconciliation or atomic
consistency. Primitive-only snapshots retain no provider/claim/DSO. Safe Ada
controlled temporaries perform checked copy-out and close native owners before
ordinary Ada values return. No new worker/pin/resource functionality. See
`task-034e4-rf-va-capability-local-functions.md`.

Task 034E3 copies VirtualAperture's by-value unordered descriptor map under the
existing live VA claim, outside bridge locks. Validated unsigned UTF-8 outer keys
sort before per-occurrence getter/copy; distinct returned labels and shared-object
aliases remain. Conditional descriptor DataPipes are queried only by explicit
option, never by capability inference or callback. Final-sized nested backing
precedes pointer publication. Published owners contain only bridge values and
retain no provider object/claim/DSO; Ada controlled temporaries perform checked
deep copy-out into ordinary values with list/descriptor inclusion flags. Multiple
getters are not an atomic provider transaction. No Claim/notification/Job lifecycle
change or endpoint mutation. See `task-034e3-rf-element-group-snapshots.md`.

Task 034E2 attaches a private registration-control record to the existing VA
owner, without a shared provider-owner graph refactor. The public subscription
contains only fixed bridge signal state. Its unnamed BaseVirtualAperture& callback
ignores the reference entirely (unlike D2's self-contained by-value payload),
locks signal state, saturates counters, sets Boolean pending, and wakes waiters.
It never queries/retains/compares provider objects or invokes Ada/application code.
One registration attempt per public VA: every allocation and stable callable/DSO
pin precedes exposure; exact-callable/state/DSO retention is then allocation-free
and permanent. Native size_t key plus separate known flag supports 0/SIZE_MAX.
Unsubscribe validates bridge state identity before stop, consumes removal before
provider entry, and caches fixed diagnostics. Local Close calls no provider; VA
Close stops/removes once before provider destruction and existing claim release.
Shutdown failure wins over removal failure; failed C2 shutdown retention remains.
Removal may block and is not callback quiescence. Subscription Wait/Statistics
may overlap VA Close, never their own wrapper destruction. Stopped shells safely
ignore valid-reference late calls, without preserving provider graphs or repairing
provider-internal reference lifetime bugs. See `task-034e2-rf-va-status-subscriptions.md`.

Task 034E1 reuses the existing claimed VA and its C2 child claim for six fresh
synchronous BaseVirtualAperture queries. Same-owner external serialization with
Close keeps the public owner alive; no C2/request mutex is held during provider
calls, no sibling Job claim/worker/callback/DSO pin is created. Claim still reads
only getVAInstanceIDs/getElementGroupLabels/isSingleGroup. Independent plain
instance-list/report owners copy bridge primitive storage and survive provider
teardown. Report creation reads one provider value and each concrete getter once,
obtaining getLFStatus's by-value map locally. Final-sized nested backing precedes
span publication. Ada controlled temporaries close native owners after checked
copy-out or exceptions. Separate live calls have no atomic consistency guarantee.
This represents all six BaseVirtualAperture read/query methods, not the complete
class or VirtualAperture; callbacks/VADB/standalone LF queries were deferred at that checkpoint.
See `task-034e1-rf-va-status-queries.md`.

Task 034D3 reuses interval_command with broad lifecycle admission for synchronous
JobDetail::extendJobEvent. One retained shared JobState covers the invocation;
the lifecycle lock scope ends before the provider call. No queue/C2 lock, worker,
future, sibling claim or registration is added. A distinct status receiver can
run concurrently and a provider may call back synchronously before return.
Every explicit invocation forwards once, without local event lookup, allowance
accounting, quantization or cached outcome. Finalize pending/terminal is admitted;
any full Cancel attempt blocks. Existing snapshots are unchanged. Mock feedback
is provider behavior; Squall is no-op/no-delivery. This is event-extension command
coverage, not complete JobDetail/JobInterval/RF MEL. See
`task-034d3-rf-job-event-extension.md`.

Task 034D2 adds a JobIntervalStatus observer state separate from JobState/provider
ownership. JobState holds state only to logically stop on Close before deferred
finalize cleanup. A permanent holder pins the exact const-reference callable,
bridge queue state and DSO, never the Job/VA/C2 graph. By-value map/vector payloads
allow already-entered/late callbacks to finish without provider resources or a
quiescence wait. A preallocated ring bounds queued copies; immutable events own
logs/activity bytes and survive every owner closure. Copy each non-const log-time
getter's value locally. Safe Ada copies out and closes native events. v1 stays
frozen/Never; v2 reuses the interval builder and requires usable local registration
for enabled reporting. See `task-034d2-rf-job-interval-status.md` for exact
registration transaction, queue bounds/counters and mock versus Squall evidence.

Task 034D1 adds receive-only JobInterval synchronous commands in C + safe Ada.
Complete borrowed-input validation and C++ construction precede one provider
Add; brief lifecycle inspection releases JobState's mutex before Add/Flush/
Cancel_Remaining. Existing worker/ownership machinery is unchanged. Ada owns
nested configs and marshals final-sized arrays with controlled label buffers.
Pinned ContinueFromPrevious count is zero and aliases ordinary zero start;
no translation or automatic quantization. Mock-positive payload fidelity;
pinned Squall no-op call-path/lifecycle only. Status callbacks, extendJobEvent
and TransmitEvent remain deferred. See `task-034d1-rf-rx-job-intervals.md`.

Task 034C3 completes the published RFMFAInfo data/query surface for current
bridge scope in native C + safe Ada, not full RF MEL. TxPowerModeData uses one
provider-independent immutable owner for collection/direct overloads, with
final-sized range/mode backing and nested spans published after copying.
Each represented getter is called once; all doubles and signed nanoseconds are
preserved. Direct returned IDs are authoritative, with no supportsTransmit gate.
Safe Ada copies all modes/ranges and closes the native owner before returning.
Positive transmit data is mock-proven; pinned Squall is receive-only and returns
an empty collection. JobInterval remains deferred. See
`task-034c3-rf-tx-power-modes.md`.

Task 034C2 adds an independent immutable PhysicalData snapshot owner. Creation
reads the published RFMFAInfo/PhysicalData/InstallationDetails getter chain once,
copies all doubles and both ComponentLocation ForeignKey strings, then publishes
bridge-owned storage. It retains no provider reference, DSO pin or DataMEL child
claim. View and Close call no provider code. Safe Ada copies to a private value
record with Long_Float scalars and Unbounded_String storage, closing the native
snapshot before return. Frozen MFA/face v1 records are unchanged. See
`task-034c2-rf-physical-data.md`; JobInterval remains deferred.

Task 034C1 exposes `RFMFAInfo::quantizeDuration` as a synchronous live query
through the existing RF DataMEL owner. The result is not stored in an MFA
snapshot: implementations can quantize dynamically. C and safe Ada preserve
signed int64 femtoseconds unchanged; callers serialize with Close and other
same-owner operations. This is the timing primitive for future JobInterval
work, not an interval implementation. See `task-034c1-rf-duration-quantization.md`.

Task 034B2B2 completes safe Ada RF Job finalize/status/cancel over the existing
135-export ABI. Both opt-in pinned-Squall ProductRx clients now activate their
Jobs via the production facade and close VA/C2 before cancellation. The former
test-only C++ job helper was removed. The historical B2B1 description below
records its native-only checkpoint. See `task-034b2b2-safe-ada-rf-job-lifecycle.md`.

Task 034B2B1 introduces a native-only RF Job lifecycle. The public Job wrapper
owns shared JobState, while the sole finalize-future worker holds that same
state through completion. JobState retains JobDetail, provider VA, and a sibling
C2 claim; explicit Close is nonblocking while pending and does not cancel.
Final claim release follows JobDetail and VA destruction, and throwing deferred
shutdown retains the uncertain C2/DSO graph. No safe Ada lifecycle or ProductRx
helper replacement is included. See `task-034b2b1-native-rf-job-lifecycle.md`.

Task 034B2A2 adds Ada-owned RX group/Job configuration and immutable Job
snapshots to `AMS.MEL.RF.C2`. Limited request/Job owners wrap the existing
native C ABI, retaining no public raw addresses. The native Job retains its
provider VA and sibling C2 claim after public parent Close. Only the
single-RX-group request/snapshot slice is covered; finalize/cancel and helper
removal remain deferred. See `task-034b2a2-safe-ada-rf-job.md`.

Task 034B2A1 adds the native-only single-RX-group Job request and owned
JobDetail snapshot. A new sibling C2 claim and a strong provider VA reference
outlive the public VA/C2 and follow the one asynchronous future into the
claimed Job. No finalize/cancel or safe Ada Job API yet. See
`task-034b2a1-native-rf-job-request.md`.

Task 034B1B2 extends the native RF C2 state with counted child claims.
Exactly one claim moves from the pending VA future through cached success to
the claimed VA. A detached worker alone consumes the future. C2 Close with
children defers shutdown until the final claim releases; uncertain throwing
shutdown retains the complete provider/DSO graph. Successful Claim snapshots
sorted VA instance IDs, ordered labels and single-group status into bridge
storage before publishing an owner; safe Ada copies these into its own vectors.
Timed Wait does not cancel. See `task-034b1b2-safe-ada-rf-virtual-aperture.md`.
RF Jobs are still outside the generic library.

Task 034B1B1 adds an independent `createC2MEL` owner and the limited
`AMS.MEL.RF.C2` Ada lifecycle facade. The private C2 state pins both the C2
object and its SharedLibrary: Close invokes shutdown once, destroys C2 before
dropping the DSO reference, and permanently retains the complete state if
shutdown throws. No children exist in this checkpoint; 034B1B2 will extend
the state with child admission and deferred shutdown before exposing VA
requests. No async workers, VA, or Jobs exist yet. See
`task-034b1b1-safe-ada-rf-c2-owner.md`.

Task 034B1A adds an independent AdminMEL factory owner (not DataMEL) behind
the same C ABI and `AMS.MEL.RF.Admin`. Every state command obtains fresh UCI and
StatusControl shared owners, and preserves the upstream Boolean rejection as a
normal result. Close consumes the public handle, shuts down once, destroys
Admin before dropping its DSO, and permanently retains the whole graph on a
throwing shutdown. The committed 034B1 Admin/C2/VA declaration closure is
reused; this historical 034B1A snapshot predates the C2 lifecycle owner.
VA remains unimplemented. See
`task-034b1a-safe-ada-rf-admin.md`.

Task 034A places the complete **current** native RF foundation behind two
safe Ada packages, `AMS.MEL.RF` and `AMS.MEL.RF.Product_Rx`, distinct from
IR. Limited controlled DataMEL, create request, endpoint, and event owners
hold one private raw handle apiece; neither request nor endpoint retains the
parent Ada object. Native Task 033D owns deferred parent shutdown and callback
drain. The MFA snapshot copies each checked native span into Ada vectors and
releases the native owner even if conversion raises. Unknown raw format values
survive in a 32-bit modular type, and reported face count is distinct from the
actual (non-contiguous-ID) face vector. A ProductRx event owns a native copied
payload and Ada-owned small metadata: `With_Samples` imports the C-ABI array
at the native event sample address for the duration of a callback, without a
second bulk copy; `Copy_Samples` explicitly copies. The native bridge has
already copied provider callback samples, so this is not end-to-end zero-copy.
The test-only child `AMS.MEL.RF.Product_Rx.Testing` observes the address for
alias verification and is not part of the production library. See
`task-034a-safe-ada-rf.md`. No C ABI change (ABI 0.1, 115 exports), no generic
jobs/C2, RDMA, other RF formats, or richer ProductRxMetadata.

Task 032B4 exposes the 032B1 public view in public Python for the two families
that already have public Python typed owners. `ControlChannel.channel_view()`
and `ImageStream.channel_view()` return a `ChannelView` that follows the
existing owner style: a private ctypes handle, pointer-to-handle, and close
function; explicit idempotent `close()`; a non-raising `__del__`; and an
`is_open` property. It owns only the native weak `ams_mel_ir_channel` and holds
no reference to its Python source, the Session, or any provider object, so a
live View delays no teardown and a `weakref` to the source dies while the View
lives. KeepAlive returns the existing `ReturnRequest`; CommsTest returns the
new `CommsRequest` with `CommsCompleted`/`CommsRejected` results in the
established Python style. The native request owners, not the View, own the
admitted family graph. Capabilities are copied into a complete Python-owned
`ChannelCapability` through one checked span helper while a private snapshot
guard keeps the native record alive and always closes it. That helper validates
pointer/size/alignment metadata before dereference but cannot prove that an
arbitrary non-NULL address from a violated C ABI is mapped; the native ABI
contract owns that. The GIL is not a thread-safety claim: operations and Close
on one View, and Wait and Close on one request, require external
serialization, and no Python lock is added. No native, `_native.py`, Rust, or
Ada change. Public Python Health, Instrumentation, and Track owners (and their
views) remain deferred; see `task-032b4-python-common-channel.md`.

Task 032B3 exposes the 032B1 public view in safe Rust for the two families that
already have safe Rust typed owners. `ControlChannel::channel_view` and
`ImageStream::channel_view` return a `ChannelView` that owns only the native
weak `ams_mel_ir_channel`: it has no lifetime parameter or borrow of its
source, retains no Session/provider object, and is `!Send`/`!Sync` through the
crate's `Rc<()>` marker, with every operation taking `&mut self` so one View's
operations and Close are statically serialized. KeepAlive returns the existing
`ReturnRequest`; CommsTest returns the new `CommsRequest`; both native request
owners, not the View, own the admitted family graph. Capabilities are copied
into a complete Rust-owned `ChannelCapability` through a single checked span
helper while a private RAII guard keeps the native snapshot alive and always
closes it. No native, sys, Ada, or Python change. Safe Rust Health,
Instrumentation, and Track owners (and their views) remain deferred; see
`task-032b3-safe-rust-common-channel.md`.

Task 032B2 exposes the 032B1 public view in safe Ada. The controlled owner
`AMS.MEL.IR.Channel_View` is declared limited private in the parent
`AMS.MEL.IR`, with its native `Channel_Handle` in the parent's private part.
Applications see it as `AMS.MEL.IR.Channel.View`. Because every typed family
package is a child of `AMS.MEL.IR`, each family body can initialize the view
from its own private handle (`C2/Image/Health_Status/Instrumentation/
Track.As_Channel`) without any public raw-handle accessor. The View owns only
the weak native view, never the family graph. `AMS.MEL.IR.Channel` now owns the
canonical `Command_ID`; `C2.Command_ID` is a source-compatible subtype, which
removes the old Channel -> C2 dependency. The common Return/Comms owners wrap
the same native handles as the legacy C2 types, and `C2.Common` is unchanged.
See `task-032b2-safe-ada-common-channel.md`.

Task 032B1 publishes the 032A common access as the opaque public
`ams_mel_ir_channel`: a weak view of one existing typed owner (C2, Image,
Health, Instrumentation, Track). The view holds only a `weak_ptr<void>` and
static adapters, so it never delays teardown; an admitted KeepAlive/CommsTest
request, not the view, owns the family graph. Capability temporarily locks the
weak state and runs a per-family helper shared with the typed
`get_capabilities` export, preserving each family's lifecycle and locking
policy (Image keeps its Stopping/Stopped/Failed rule and unlocked provider
call). Legacy C2 inherited-service exports remain and do not route through a
temporary view. The safe Ada façade followed in Task 032B2 and the safe Rust
C2/Image façade in Task 032B3, and the public Python C2/Image façade in Task
032B4. See
`task-032b1-public-common-channel-abi.md`.

Task 032A4 adds private weak common access for Instrumentation
and Track using their existing single family request counters, Session admission
and deferred cleanup. The view and conversion functions are test-only; no
public common Channel API or capability query is introduced. Test-only weak
observers prove closed-parent mixed counts without pinning either graph;
callback quiescence and Session-wide admission are verified across the new
common and existing typed requests. See
`task-032a4-common-access-instrumentation-track.md`.

Task 032A3: Health joins the private weak common access and
shared Return/Comms completion engines. Health Close is logical first with
pending common requests, immediately deactivates metadata, and defers physical
cleanup (including callback drain) to the final claim. See
`task-032a3-common-access-health.md` for deterministic concurrency evidence.

Tasks 032A1 (merged PR #47) and 032A2 extract Return and Comms completion
into a private family-neutral engine and prove weak common Channel submission
through C2 and Image typed adapters. The idle access owns no provider graph;
an admitted request claims the existing family count and uses the shared
Session permit. The Image claim joins Navigation's deferred cleanup protocol.
The common view is test-only; no public Channel view exists yet. See
`task-032a1-common-completion-engine.md` and
`task-032a2-common-access-image.md`.

Task 031B adds opt-in Session-scoped admission before async
RequestFor provider sends. Zero remains unlimited; rejection has no bridge
queue or retry. An admitted future still has its own blocked worker; this
does not multiplex futures. See `task-031b-bounded-completion-admission.md`.

## Scope

Build a consumer-side binding, not a new official MEL standard and not a
provider rewrite. Preserve the published C++ provider boundary. The C facade
adapts it for C, Ada, Rust, and Python.

The bootstrap implemented an independent ABI-version query. Task 001 adds only
the first provider boundary: load a compatible IR MEL library, create and
initialize its `Control`, copy its complete `VersionInfo`, and close it.
Task 002 adds one vertical receive profile: attach an `IRSTImage` channel,
register service-owned host buffers, receive `ImageListener::onImage` callbacks,
copy validated Mono8 frames into a bounded queue, and poll them from C or Ada.
Task 003 adds one command profile: attach and explicitly enable a
`CommandAndControl` channel, send `Operate`/`TaskSched`, and preserve the
asynchronous `RequestFor<MFA_Mode>` result in C and Ada.
Task 004 adds no library API. Its isolated, opt-in applications exercise those
Task-002/003 slices against Squall's published IR MEL provider and hardware-free
simulated optical stack. Container deployment remains test orchestration, not a
generic-library dependency or an application-facing transport.
Task 006 adds the first Rust consumer without changing that C ABI: façade
version query plus provider Session open/version/close. Task 007 adds the Rust
consumer for the existing IR host-memory Mono8 stream ABI. Task 008 adds the
Rust consumer for exactly the existing IR C2 Operate/TaskSched profile. Task 009
adds no library behavior: its standalone, opt-in Rust application exercises the
safe API against the same pinned Squall provider/runtime used by C and Ada. RF
and additional C2 commands remain later vertical slices. Task 010 adds the
first Python consumer over the unchanged C ABI: façade version plus provider
Session open/version/close, structured diagnostics, and deterministic cleanup.
Task 011 extends that same dependency-free consumer through the existing IR
host-memory Mono8 stream ABI: open/start/receive/counters/stop/close, owned-copy
frames, distinct timeout/stopped errors, and parent-first Session lifetime. It
does not add RF, real-provider validation, zero-copy/NumPy views, or packaging.
Task 012 adds the Python consumer for exactly the existing C2
Operate/TaskSched profile: explicit enable, asynchronous finite waits, structured
success/rejection values, complete rejection text, cached terminal results,
retryable close, and independent Session/C2/request lifetime. It adds no native
feature or additional command.
Task 013 adds no library behavior: a standalone, opt-in Python application
exercises the same current image+C2 slice against the pinned real Squall
provider/runtime used by C, Ada, and Rust. It imports only the public `ams_mel`
API and the standard library; container orchestration remains outside the binding.
Task 014 adds the required IR `BIT_Command` only in the empty/no-op profile
accepted by pinned Squall: a caller command ID with empty initiate, cancel, and
clear-fault lists. C and Ada expose a reusable asynchronous `RequestFor<Return>`
owner. The raw Rust sys crate tracks the expanded ABI, while safe Rust and Python
do not yet expose BIT. Task 015 exposes that unchanged ABI through safe Rust with
typed `CommandReturn`, structured normal completion versus MEL rejection, and a
reusable `ReturnRequest`. A completed `Return::Fail` is a normal `ReturnResult`,
not a Rust error. Timeout and request close/drop do not cancel provider work, and
native request ownership remains valid after Rust Session/C2 closure. Python and
payload-bearing BIT remain unsupported.
Task 016 exposes the same unchanged 19-function ABI through dependency-free
Python. It adds typed `CommandReturn`, structured `ReturnCompleted` versus
`ReturnRejected`, and an independent reusable `ReturnRequest`. `Return::Fail`
is normal completion; timeout and close do not cancel, complete terminal
diagnostics are preserved, and unknown MEL rejection codes remain inspectable.
Payload-bearing BIT remains unsupported.
Task 017 begins an Ada-first feature-family cadence and completes the three
published required C2 sends. The additive 22-function C ABI carries complete
ModeCmd/ScanParam, every BIT vector through borrowed spans copied before send,
and complete ConfigSet data. Ada exposes strong state/frame/degradation types,
complete scan units, one-choice BIT operations, and opaque configuration text.
The proven ModeRequest and ReturnRequest workers are reused. Rust sys and private
Python ctypes track the raw ABI only; safe Rust and Python retain their existing
Operate/TaskSched and BIT-no-op subsets. Required C2 callbacks remain Task 018;
no callback or optional command is introduced by Task 017. Task 018's implemented
callback architecture is documented below.

```text
C++ provider -> MEL API -> ams_mel_c -> Ada
                                     |-> Rust
                                     `-> private ctypes -> Python API
```

## Separation

1. `native/include`: genuine C11 declarations, no STL or C++ object layouts.
2. `native/src`: C++20 adapter implementation; CMake owns compilation.
3. `ada/src`: idiomatic public Ada plus private imported C declarations.
4. `rust/ams-mel-sys`: unsafe declarations for the complete current C ABI.
5. `rust/ams-mel`: safe Rust API over `ams-mel-sys`; no direct C++ path.
6. `python/ams_mel`: safe Python Session/image/Operate/BIT-no-op API over a
   private complete 22-function `ctypes` façade; no direct C++ path.
7. Separate integration applications: OMS/UCI, image processing, RF processing.

`integration/squall` contains validation applications rather than production
library code. It may invoke Squall's supported container build/deployment
mechanisms, but the C, Ada, and Rust executables and interpreted Python client use
only this project's public language APIs/C ABI. They do not expose or call
Couloir, backend gRPC, UDP, REST, or Squall-private types. The standalone Rust
application remains outside the published workspace and depends only on the
repository's safe `ams-mel` crate. The standalone Python application imports only
the standard library and public `ams_mel` API.

The shared native library deliberately owns its C++ boundary. GPR imports it
as externally built, and Cargo links it from an explicitly selected external
build directory. CMake remains the sole owner of native C/C++ compilation.
Python loads that same CMake-built façade from an explicit
`AMS_MEL_NATIVE_LIB` path and passes the separate provider path through the C
ABI. The Python layer is not a native extension and makes no zero-copy claim.
Neither Rust crate models or links a provider directly. `Session`,
`ImageStream`, `ControlChannel`, and `ModeRequest` are deliberately not `Send`
or `Sync`; the safe receive API has
one conceptual receiver and does not expose cross-thread stream sharing. A
stream has no Rust borrow of its parent Session because the native stream
independently retains provider state. Closing Session first therefore leaves its
child stream valid, and provider unload waits for the stream owner.

The Rust C2 owner likewise has no borrow of Session, and each `ModeRequest` has
no borrow of either parent. Session and C2 may be closed while a request remains
pending. Request timeout and request drop are not cancellation; terminal waits
are cached and repeatable. A C2 close that cannot detach synchronously retains
its owner for explicit retry, which `ControlChannel::is_open` exposes. Drop makes
one best-effort close attempt and prioritizes native lifetime safety over leak
avoidance when detach still cannot be established.

Rust models command rejection as a terminal `ModeResult::Rejected`, preserving
the MEL error code and complete validated UTF-8 description. The wait wrapper
uses the C ABI's cached-terminal guarantee to repeat only an oversized terminal
wait with timeout zero and exact fallible storage; generic side-effecting calls
retain their non-retrying diagnostic behavior.

Rust receive follows the native non-consuming `BUFFER_TOO_SMALL` handshake: it
waits once with the requested timeout, fallibly allocates the exact required
pixel count, then polls the retained queued frame with timeout zero. Returned
`Frame` pixels are an owned `Vec<u8>` copy and never borrow provider storage.
Timeout and clean stream stop remain distinct errors; a zero timeout is a poll.

The safe Rust wrapper captures diagnostics in a fixed local buffer during each
native call. If an error reports a larger required capacity, Rust preserves the
native error kind and required byte count but exposes no diagnostic string: a
valid UTF-8 prefix is not the complete diagnostic. The wrapper does not retry
Session operations merely to recover text because open and close have provider
and ownership side effects. Consequently, the current C ABI cannot guarantee
recovery of an arbitrarily long provider diagnostic after one such call.

## Native dependency baseline

Do not add unpinned `FetchContent`, build-time network access, or recursive
submodule expectations to the normal build. Select, review, and record a source
closure before adding upstream headers. The earlier review inventory is only a
candidate baseline and belongs under `docs/reference`, not a release lock.

Task 001 freezes the exact source closure in `upstream-provenance.md`. Production
`ams_mel_c` compiles the private adapter against those declarations and has no
link dependency on a provider. At runtime it loads the caller-selected library
locally, resolves the published C-linkage/C++-signature factories, and retains
the loader owner until `Control` and `API_Manager` destruction completes. Mock
providers and failure fixtures are test-only targets and are not installed.

Sessions now hold a shared private provider state. An IR stream retains that
state, so closing the public parent owner while a stream exists does not unload
provider code. Stream close stops adapter acceptance, calls `disable`, detaches,
destroys the last façade channel owners, waits for explicitly counted adapter
callbacks to return, and only then destroys provider `Buffer` objects and host
storage. Provider/library ownership is released last. Failed detach retains the
whole callback-accessible graph for a later close attempt rather than risking a
use-after-free; an open-time detach failure is retained internally because no C
owner can safely be returned.

A C2 channel likewise retains shared provider state. Submission preallocates its
completion, worker input, public owner, and thread holder before provider `send`.
After `send` yields a valid future, moving it into the prepared worker input,
arming that input's self-retention, and incrementing the channel request count
form one nonthrowing accounting sequence under the channel lock. No untracked
provider future can cross that boundary. The blocking (non-polling) worker owns
the provider future and C2 graph until terminal completion, calls `future::get()`
once, and caches success, MEL rejection, provider exception, null success, or
unknown-value failure. Public request close only removes that owner; it neither
joins nor cancels. C2 close stops submissions immediately and defers
disable/detach while requests are in flight. The final worker performs cleanup
and releases provider/library ownership.

Worker creation/detach or later adapter failures are internal failures, not
provider exceptions. If a future cannot be safely handed to a detached worker,
or orphan cleanup cannot safely detach, an intrusive atomic root plus an
already-armed `shared_ptr` self-cycle retains the graph without allocating or
taking a mutex. That emergency graph is intentionally permanent because no safe
completion path remains. A never-completing provider future likewise retains the
graph indefinitely rather than risking unload of live code.

## Asynchronous request execution path

C2 Mode, Return, and CommsTest reserve their request under the channel lifecycle
mutex before invoking provider send outside that mutex. The shared claim
linearizes submission against logical Close: claimed sends keep physical cleanup
deferred, while a claim after Close fails without sending. Synchronous send
exceptions release that reservation; post-send launch failures retain it with
the future. This internal ownership rule does not make arbitrary concurrent
public-wrapper access safe. See
`corrective-c2-unlocked-request-submission.md` for deterministic evidence.

For an asynchronous request the conceptual path is:

```text
Ada application
      |
      | submit
      v
C ABI / native adapter
      |
      | provider send()
      v
provider future
      |
      | retained by a native completion worker
      | the worker blocks in future.get()
      v
Completion state
      |
      | condition-variable notification
      v
Ada Wait(timeout)
```

The distinction that matters is:

```text
Ada Wait(timeout > 0)  = blocking wait
Ada Wait(0)            = nonblocking poll
```

It is *not* the case that Ada continuously loops calling `Wait(0)`. Ada never
repeatedly checks provider futures, and no Ada application thread spins while a
request is outstanding: the native worker consumes the provider future and the
waiting Ada call sleeps on a condition variable until the completion is
published or the timeout expires. An application may write its own zero-timeout
polling loop, but that is an application choice rather than the strategy of this
binding. Provider callback threads never execute Ada application code; they only
publish into adapter-owned state.

The inbound metadata/event path is a different mechanism with the same wait
semantics:

```text
provider callback thread
      |
      | validate and copy
      v
bounded native FIFO (DROP-INCOMING)
      |
      | condition-variable notification
      v
application Receive(timeout)
```

There is no provider future and no completion worker on this path. A
positive-timeout `Receive` blocks on a condition variable; a zero-timeout
`Receive` is a nonblocking poll. Normal usage is not a busy-poll loop. The
separate DROP-INCOMING overflow policy governs only what happens when an event
arrives at a full queue. Queue overflow policy and application polling are
independent concerns: DROP-INCOMING does not imply that a consumer must poll.
Provider callback threads never invoke Ada application code; they only validate,
copy, and enqueue into adapter-owned storage before returning to provider code.

### Task 031A: measured completion-thread scalability

This concerns the `RequestFor<T>` request/future path only. The current
implementation is deliberately correctness-first and uses approximately one
native completion worker thread per outstanding asynchronous provider future.
That worker blocks in `future::get()` until terminal completion. This is
correct, not broken, and it is acceptable for current functional development and
testing. It may, however, scale poorly when many requests are concurrently
outstanding or are issued at a high rate; `TrackDataUpdate`,
`SystemTrackDataResponse`, and other `RequestFor<T>` operations are the relevant
cases when evaluating this.

It does **not** apply to `RequestSystemTrackData`, which is inbound and
callback/queue based: it has no provider future, no request handle, and no
completion worker, so it contributes nothing to this thread count.

Task 031A measures the current implementation using held mock-provider futures.
The deterministic C11 regression confirms 1/10/100 simultaneously blocked C2
Return workers (and a 100-operation C2 Mode/Return/CommsTest mix); all drain
with one `get()` per future, including 100 early request closes and parent-first
Session/C2 closure. Linux thread count rises from 1 to 2/11/101 while held;
virtual memory grows substantially with the blocked threads. This characterizes
the **current implementation**, not an API guarantee or a requirement for a
future executor. See `task-031a-request-completion-scalability.md` for the
measurements and explicit mixed-family coverage limits. Functional correctness
and scalability remain separate concerns.

The seven-type N=100 regression additionally tests controlled C2,
Image/Instrumentation, and Track release waves with all public owners closed.
Both success and stored-exception runs check future invalidation and weak result
expiration before deferred cleanup, and empty Completion graph fields before
local graph-owner release. Captured WorkerInput destruction may follow provider
unload: the published RequestFor alias is a consumed, invalid `std::future`, and
the remaining Completion and inactive emergency bookkeeping are bridge-owned.
The captured-owner marker measures resource reclamation, not provider safety.
Separate seven-type retained-handle success and stored-exception tests verify
zero/positive non-cancelling timeouts, exact cached payloads or exception status,
and exactly-once get through request close. Separate non-gating C2-only and
seven-type mixed N=100 benchmarks observe approximately one additional native
OS thread per pending worker. The mixed benchmark releases its own DSO control
handle after its last provider call and records physical channel, Control,
manager and library teardown via an external test-only monotonic timeline;
it never invokes saved provider pointers after releasing that handle.
The implementation remains functionally correct
under the tested loads; timing and memory measurements characterize this mock
environment, not portable performance guarantees. Task 031B should investigate
bounded resources without assuming a pool of blocking `get()` calls makes
progress when its first futures remain unresolved.

The preferred design space is an investigation rather than a decision already
made. Candidate approaches include a bounded asynchronous completion executor, a
shared worker pool, a shared waiter/completion service, or another bounded
completion mechanism compatible with the upstream `std::future` API. A bounded
or shared mechanism is preferred if the upstream future API permits one without
introducing unsafe polling or unbounded latency. Thread scaling must **not** be
solved by introducing a busy-poll loop.

Any such change must preserve the current externally observable semantics, at
minimum: exactly one `future.get()` per provider future; permanent terminal
result caching; timeout is not cancellation; request-handle close does not
cancel provider work; Session/Channel/Track/provider/library lifetime retention;
synchronous completion safety; post-send failure safety; deferred cleanup
semantics; deterministic error mapping; no callback into Ada application code
from arbitrary provider threads; and ABI compatibility unless a deliberate ABI
revision is separately approved. The eventual optimization should demonstrate
improvement against the measured baseline while keeping all existing lifetime
and status tests passing. None of this work is implemented today.

## First integration profile

Start with IR host-memory, single-band Mono8 reception only after provider
loading, ownership, and shutdown tests exist. Use bounded owned copies first;
leases/zero-copy require a separate reviewed API and explicit lifetime contract.
Keep metadata conversions loss-aware. Do not convert every timestamp to one
nanosecond value or copy Squall-private integer frequency conventions.

The implemented queue owns copied pixels and has caller-selected finite
capacity. Overflow drops the incoming frame. Provider callbacks never enter C or Ada and
release each non-null callback buffer exactly once after processing. Release
status/exceptions poison the stream and wake receivers. The pinned interface
does not state that `disable()` waits for callbacks. More concretely, pinned
Squall `disable()` only clears its enabled state and delegates control disable;
its destructor resets `UdpDataReceiver`. The façade therefore treats successful
detach plus destruction of all façade channel owners—not `disable()`—as the
boundary after which no new image callback can begin, then waits for its own
in-flight callback count to reach zero. The mock has both quiescing-disable and
destruction-quiescing scenarios. Generic unregister was removed because it
cannot establish this boundary and can race non-quiesced provider callbacks.

The stream lifecycle is `Attached`, `Starting`, `Running`, `Stopping`, `Stopped`,
or terminal `Failed`. Any provider operation/release/rollback failure stops frame
acceptance and wakes receivers. Frames copied before a clean stop or failure are
drained first; the next receive reports `STREAM_STOPPED` or `PROVIDER_FAILED`.
At most one consumer thread/task may execute receive on a stream at a time.
Python follows the same external-serialization contract and does not use the GIL
or an added Python lock as a thread-safety claim. Its Frame pixels are an owned
`bytes` copy, and its ImageStream does not retain the Python Session object;
native child ownership keeps provider/library state alive when the parent closes
first.

## Parallel Ada work

Task 018 completes the required C2-specific metadata surface for native C and
safe Ada: BIT_Configuration, CommandStatus, and BIT_Status. All three callbacks
share one bounded FIFO callback-to-owned-event queue with DROP-INCOMING overflow.
The provider callback stack performs validation/deep copy only and never invokes
Ada application code. A successful Ada `Receive` returns a wholly Ada-owned graph
including every nested BIT/fault vector and string.

Upstream publishes no callback unregister. Callback state therefore belongs to
the existing C2 `ChannelState`, not the public metadata owner. Partial registration
failure publishes no metadata owner but retains already registered closures.
Metadata close merely deactivates the public queue. Final C2 cleanup stops
acceptance, disables/detaches under existing rules, destroys the provider channel,
waits for adapter callbacks already in flight, marks metadata stopped, and wakes
receivers. Channel destruction—not `disable()`—is the quiescence boundary.
Immutable received native snapshots are independent of that graph and can outlive
metadata, C2, Session, and provider library teardown.

Task 019 adds the application-facing inherited services on the existing C2
channel. KeepAlive reuses the Return request owner; CommsTest has a parallel
typed asynchronous owner but shares `ChannelState.requests`; both work in
Attached or Enabled lifecycle states. The inherited CommsTest callback is an
explicit fourth registration into Task 018's existing queue and uses the same
callback guard, retained state, and channel-destruction quiescence boundary.

Task 020 adds the required IR HealthAndStatus channel and exactly six callback
overloads: MFA_Status, BIT_Status, SubsystemStatusResp, DiscreteStatus,
MFA_SecurityAuditRecord, and MFA_StatusDetailed. LFStatus and NUC_TempData remain
out of scope. One bounded DROP-INCOMING FIFO deep-copies callback values before
returning to provider code; Ada receives from that queue -- blocking on a
condition variable for a positive timeout, and performing a nonblocking poll
only when the timeout is zero -- and copies each complete graph
again before releasing the native event. No provider thread invokes Ada code.

The callback state belongs to the Health channel, not the public metadata owner.
There is no unregister assumption: partial registration retains earlier closures,
metadata close only deactivates public consumption, and provider channel
destruction is the callback-quiescence boundary. Immutable native snapshots and
Ada values can outlive metadata, Health channel, Session, and provider unload.
Reusable Common-MEL status values live in `AMS.MEL.Status`; Health does not depend
on C2 metadata types. Rust sys and private Python declarations track the 46-export
raw ABI, but no safe Rust or public Python Health API is introduced.

Task 021 preserves the legacy Mono8 frame record and receive operation, adding an
opaque immutable full-FrameHeader snapshot owner instead. Callback-time copying
builds one complete adapter-owned queued frame and one bounded FIFO is consumed by
both legacy and snapshot receive. Snapshot views retain complete contributing sensor,
ordered flags, inertial/nav data, and independently discriminated orientation variants
until close; safe Ada copies the graph before releasing the native owner.

ChannelCapability is validated and deep-copied completely into an opaque native
snapshot, then copied again into reusable Ada-native `AMS.MEL.IR.Channel` values.
Neither snapshot references provider STL storage. `Channel::registerBuffer` and
`unregisterBuffer` are deliberately not application-visible: existing image
buffer registration remains adapter-managed. This completes application-facing
common C2 Channel services, not every raw Channel virtual method. Scheduling is
not part of Task 019.

Task 024 reuses that native ChannelCapability snapshot for the existing sole
ImageChannel. One internal Ada converter deep-copies a native capability view for
both C2 and Image; each caller independently owns acquisition, view, diagnostics,
and exception-safe native-owner closure.

Task 026 registers BadPixelList, LineOfSightReport, LineOfSightEuler, and NavigationReportResp in that order on
one Image metadata FIFO. A stream-retained
callback state supports synchronous registration callbacks and outlives public metadata
closure. It deep-copies each valid event into a bounded DROP-INCOMING FIFO with
saturating counters; ImageChannel destruction is the callback-quiescence boundary.
Native event owners and safe Ada event values remain valid after metadata, stream,
Session, and provider teardown. NavigationReport send,
LineOfSightQuaternion, and other optional Image metadata are not implemented.

Task 027A is an internal Image ownership refactor only. The opaque public Image
stream is a thin owner of shared `ImageStreamState`, which owns its SessionState,
frame CallbackState and Listener, Channel and ImageChannel, stream-owned Image
metadata state, buffer factory/configuration, provider Buffer objects, host
storage, and enable state. This permits a future asynchronous Image request to
retain the private resource graph without retaining `ams_mel_ir_stream *`.
It changes neither ABI 0.1 nor application-visible behavior. No NavigationReport
API exists yet; request submission, waiting, logical close with pending work, and
deferred cleanup remain future work.

Task 027B adds `ImageChannel::send(NavigationReport)` as an asynchronous
request/future vertical slice built on that shared `ImageStreamState`: the
complete published `NavigationReport` (including all 18
`PositionVelocityCovariance` terms) crosses the C ABI as
`ams_mel_navigation_report_v1`; submission is valid while the stream is
logically Attached or Running and does not require a prior Start; provider
`send()` executes with neither the frame callback mutex nor the Image metadata
mutex held, because a pinned provider may invoke the registered
`NavigationReportResp` metadata callback synchronously from within `send()`.
`ImageStreamState` gains a request counter, cleanup-ownership state, and a
`public_owner_closed` flag so that Stop/Close with an outstanding request
defers physical provider teardown (disable/detach/destroy) to final request
completion, mirroring the C2 `ChannelState`/`retain_failed` fail-safe pattern
with its own allocation-free emergency retention root.

That deferred teardown is synchronized against the adapter's own Navigation
completion thread by a single teardown lock, `CallbackState::mutex`. Every
access to `requests`, `channel`, `image_channel`, `enable_attempted`,
`public_owner_closed`, and the `cleanup_in_progress`/`cleanup_complete`/
`cleanup_ok`/`cleanup_failed` fields happens under it. A cleanup owner claims
ownership under the lock, moves the provider `shared_ptr`s into local owners,
runs `disable()`/`detachChannel()`/channel destruction **unlocked** (a pinned
provider may call back synchronously, and `disable()` is not a quiescence
boundary), then republishes the outcome under the lock and signals the
`cleanup_done` condition variable. Stop/Close block on `cleanup_done` rather
than inspecting `channel` concurrently, and Close adopts the published cleanup
outcome instead of a stale status from its own earlier logical Stop; a failed
detach restores the graph and retains the public owner for a retry.

That synchronization also covers the window between the final request-count
decrement and the cleanup ownership claim, during which a racing Close can
observe `requests == 0`, `cleanup_in_progress == false`, and a still-attached
`channel` even though the completion thread is already committed to cleaning
up. Close treats that state as *cleanup owed*: it runs or joins the cleanup
outside the lock and re-decides from the published result, so it can never
return `AMS_MEL_OK` while retaining the public owner. See
`docs/corrective-image-navigation-close-race.md`, which supersedes the Task
027B description of this synchronization.

ABI 0.1 grows from 56 to 59 exports. Safe Ada exposes `Navigation_Report`, `Navigation_Request`, and
`Navigation_Result` in `AMS.MEL.IR.Image`; the canonical safe
`Navigation_Response` also moves there, with
`AMS.MEL.IR.Image.Metadata.Navigation_Response` becoming a source-compatible
subtype. Raw Rust and private Python ABI declarations track all 59 exports; no
safe Rust or public Python Navigation API is added.

Task 028 adds the conditionally required Instrumentation family
(`@RequiredIfInstrumentation`) in `native/src/ir_instrumentation.cpp` and the
new `AMS.MEL.IR.Instrumentation` Ada package with its `Metadata` child. The
slice is deliberately scoped to the Instrumentation-specific conditional
surface -- `InstrumentationChannel::send(InstrumentationLevelCmd)` and
`registerMetadataCallback(InstrumentationReport)` -- plus `Enable` and
`ChannelCapability`. Instrumentation-specific copies of the inherited generic
`Channel` services (KeepAlive, CommsTest, the ChannelCommsTest callback, and
`registerBuffer`/`unregisterBuffer`) are intentionally not cloned; those should
be generalized across non-C2 channel families in a later task.

The private `ChannelState` follows the proven Health/C2 shape: SessionState,
generic `Channel`, `InstrumentationChannel`, metadata callback state, request
count, an explicit Attached/Enabled/Failed/Closed lifecycle, a
`cleanup_started` serialization guard, and an allocation-free emergency
retention root. Asynchronous work never retains the public
`ams_mel_ir_instrumentation *`. Provider `send()` runs with the lifecycle mutex
released, because a provider is permitted to invoke the InstrumentationReport
callback synchronously from inside both `send()` and
`registerMetadataCallback`; `metadata_open` publishes and retains the callback
state before releasing the lifecycle lock and calling provider registration, so
neither synchronous path can deadlock. Close with pending requests prevents new
submissions, deactivates public metadata consumption, releases the public
owner, and defers disable/detach/provider-channel destruction and callback
quiescence to final request completion. One canonical
`ams_mel_ir_instrumentation_report_v1` serves both the
`RequestFor<InstrumentationReport>` completion and the metadata event; upstream
`Priority` is exposed as exactly Normal=0/Debug=1 with no invented
MaxExclusive. Capability snapshots reuse the existing native
`snapshot_capability` and the existing Ada `Capability_Conversion`. ABI 0.1
grows from 59 to 72 exports and native CTest from 9 to 10 targets. Raw Rust and
private Python declarations track all 72 exports; no safe Rust or public Python
Instrumentation API is added. Positive Instrumentation behavior is mock-proven
only: pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` returns
`nullptr` from `attachChannel` for Instrumentation, so real-provider validation
covers clean unsupported-provider failure and continued Session health only.

Task 029B1 adds the conditionally required Track family's (`@RequiredIfTrack`)
channel ownership/lifecycle foundation and nothing else. `AMS.MEL.IR.Track`
and four C exports provide Open, Enable, ChannelCapability, and Close over a
shared private `TrackState` that mirrors the Health/Instrumentation channel
architecture minus metadata state and request accounting. Open validates the
concrete `TrackChannel` type and the reported `ChannelType::IRSTTrack`
capability, rolls back through detach when either check fails, and retains the
complete provider graph through the same allocation-free emergency root when
detach ownership cannot be proven. Close disables only when Enable was
attempted, always attempts detach, and leaves the caller's owner non-null for a
later retry when detach fails. Capability snapshots reuse the existing native
`snapshot_capability` and the existing Ada `Capability_Conversion`. ABI 0.1
grows from 72 to 76 exports and native CTest from 10 to 11 targets. Raw Rust
and private Python declarations track all 76 exports; no safe Rust or public
Python Track API is added. The `IRSTTrackReport` callback, `TrackDataUpdate`,
`SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
unimplemented, and no real Squall Track validation was added: pinned Squall
negative Track validation waits for the complete `@RequiredIfTrack` report
surface. The mock provider derives from the abstract upstream `TrackChannel`,
implements every pure virtual Track operation as unsupported/not-supported, and
records any call so tests prove the deferred surface was never exercised; it
sees only the vendored pinned Boost include root.

Task 029B2 completes the `@RequiredIfTrack` core by adding exactly the
`IRSTTrackReport` metadata callback on top of that foundation, without
redesigning Track ownership. `TrackState` gains only a shared `MetadataState`
and a one-shot `metadata_attempted` flag; it still has no request accounting
because no Track send exists. The metadata machinery reuses the proven
Instrumentation shape: a bounded DROP-INCOMING FIFO of owned events, saturating
counters, an Active/Inactive/Stopped/Failed lifecycle, and an explicit in-flight
callback count whose drain -- taken only after provider channel destruction --
is the quiescence proof. `AMS.MEL.IR.Track` grows safe `IRST_Track_State`,
`IRST_Track_Mode`, a Track-owned `North_East_Down` that adds no `AMS.MEL.IR.Image`
dependency, and the complete `IRST_Track_Report`; the new
`AMS.MEL.IR.Track.Metadata` child receives from that queue -- a blocking wait
for a positive timeout, a nonblocking poll for a zero timeout -- and copies
every field into
Ada-owned storage before closing the native event owner. Because upstream
declares no unregister, registration is one-shot and the callback state belongs
to the Track channel rather than to the public metadata owner, so public Close
is nonblocking and a retained callback invoked afterwards enters and returns
safely while queueing nothing. The mock provider now implements
`registerMetadataCallback(IRSTTrackReport)` positively and emits reports
synchronously from inside registration, which is the hardest ordering the facade
must survive; all other Track callbacks remain `Return::NotSupported` and both
Track sends remain Unsupported, and legitimate report registration is no longer
counted as a deferred operation. ABI 0.1 grows from 76 to 82 exports and native
CTest from 11 to 12 targets. Raw Rust and private Python track all 82 exports;
no safe Rust or public Python Track API is added. `TrackDataUpdate`,
`SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
unimplemented, and no real Squall Track validation was added: the pinned-Squall
negative Track probe is deferred to task 029B3.

Task 029C adds exactly `TrackChannel::send(TrackDataUpdate)` over that
foundation, under its own upstream condition `@RequiredIfTrackUpdate`. The
`@RequiredIfTrack` core stays complete and unchanged; this task does not extend
`@RequiredIfTrack` itself. `TrackState` gains only a `size_t requests` count, so
the existing metadata ownership is untouched. The asynchronous outcome reuses the
proven Instrumentation request pattern: a shared terminal completion state that
never holds a raw `ams_mel_ir_track *`, a single detached completion worker that
is the only `future.get()` caller, a permanently cached terminal result, and an
allocation-free emergency retention root for a future that could not be handed
to a running worker. Because a provider may invoke the registered
`IRSTTrackReport` callback synchronously from inside `send()`, the adapter
validates Enabled, copies the `TrackChannel`, and increments `requests` under the
Track mutex, then releases it before the provider send; a mock scenario emits
exactly that reentrant report and proves no deadlock, a received report, and a
completed request. Track Close with pending requests clears the public owner and
defers physical teardown to final completion, while the no-pending-request path
keeps the Task 029B1/B2 synchronous detach-failure semantics unchanged.
`AMS.MEL.IR.Track.Updates` keeps the Track parent package focused and defines its
own `Command_State`/`Cannot_Comply` with the published numeric representations
rather than depending on `AMS.MEL.IR.C2.Metadata`; no cross-package
neutralization refactor is attempted here. ABI 0.1 grows from 82 to 85 exports
and native CTest from 12 to 13 targets. Raw Rust and private Python track all 85
exports; no safe Rust or public Python Track API is added.
`SystemTrackDataResponse`, `CandidateObjectMessage`,
`CandidateObjectPreProcMessage`, and `RequestSystemTrackData` remain
unimplemented. Pinned Squall still cannot attach Track, so the Track
expected-negative integration probe is unchanged and this task introduces no new
positive real-Squall Track claim: positive `TrackDataUpdate` behavior and payload
fidelity are mock-provider evidence only.

Task 029D adds exactly `TrackChannel::send(SystemTrackDataResponse)` over that
same foundation, under its own upstream condition `@Optional`. All three Track
conditions stay distinct: the `@RequiredIfTrack` core and the
`@RequiredIfTrackUpdate` `TrackDataUpdate` send both remain complete and
unchanged, and this task extends neither. No second async lifecycle model is
created: the Task 029C `CompletionKind`, `Completion`, `WorkerInput`,
`complete(...)`, `finish_request(...)`, `retain_worker(...)`, and
`TrackState::requests` are reused exactly, with `Completion` now storing the
terminal value as the neutral `ams_mel_ir_command_status_v1`/
`ams_mel_error_code_t` pair so neither public result layout is assumed to equal
the other. Critically, `SystemTrackDataResponse` requests share the SAME
`TrackState::requests` accounting domain as `TrackDataUpdate` requests; no
second counter exists, and a deterministic mixed-request regression proves that
one pending request of each family keeps the provider graph alive until both
complete.

The public surface stays semantically honest:
`ams_mel_ir_track_system_response_request` and
`ams_mel_ir_track_system_response_result_v1` are distinct public types rather
than the TrackDataUpdate request/result reused under a misleading name, even
though the result intentionally matches that shape because both upstream
operations return `RequestFor<CommandStatus>`. The complete response reuses the
one canonical `ams_mel_ir_az_el_v1` for both angle pairs, keeps the system time
in signed nanoseconds, copies every range, rate, error, and angle verbatim, and
accepts only 0 or 1 for each of the two published bool values. The provider send
again occurs outside the Track mutex, and a mock scenario emits a reentrant
`IRSTTrackReport` from inside `send(SystemTrackDataResponse)` to prove it.
`AMS.MEL.IR.Track.System_Data` is the safe Ada home for the outbound response.
Task 029D predicted it would also host `RequestSystemTrackData`; Task 029E
superseded that prediction by inspecting the pinned `TrackChannel`, which
declares `RequestSystemTrackData` only as a `registerMetadataCallback` overload,
so its implemented Ada home is `AMS.MEL.IR.Track.Metadata`. ABI 0.1 grows from
85 to 88 exports and native CTest from 13 to 14 targets.
Raw Rust and private Python track all 88 exports; no safe Rust or public Python
Track API is added. As of Task 029D the `RequestSystemTrackData`,
`CandidateObjectMessage`, and `CandidateObjectPreProcMessage` callbacks remained
unimplemented and the Track API as a whole is not complete. Pinned Squall still
cannot attach Track, so positive
`SystemTrackDataResponse` behavior and payload fidelity are mock-provider
evidence only.

Task 029E adds exactly the `@Optional` `RequestSystemTrackData` surface. The
pinned upstream `TrackChannel` declares this type **only** as a
`registerMetadataCallback` overload and declares no `send(RequestSystemTrackData)`
and no `RequestFor<...>` for it anywhere in the snapshot, so it is an inbound
request the provider delivers to the application rather than an operation the
application issues. It therefore reuses the existing bounded DROP-INCOMING Track
metadata queue instead of the 029C/029D request/wait machinery, and adds no
asynchronous infrastructure at all. Both implemented kinds share one queue, one
capacity, and one counter set and preserve FIFO order across kinds; the event
struct gains a discriminated `request_system_track_data` member and a second
kind constant, and only the member selected by `kind` is populated. All four
published fields are copied verbatim, with the signed
`std::chrono::nanoseconds` time carried as `int64_t` nanoseconds and no unit
conversion. Upstream declares no enum and no constrained field for this type, so
an all-zero request is well formed and only a null payload is malformed. Because
the callback is `@Optional`, a `NotSupported` answer to this registration does
not fail the metadata open and leaves the `@RequiredIfTrack` report callback
fully working. `Fail` is treated differently, because upstream defines it as "a
callback is already registered for this datatype on this channel": that is a
genuine conflict in which another subscriber owns the datatype and the bridge's
closure may never be invoked, so returning success would promise deliveries the
bridge cannot make. `Fail`, and any other unrecognized non-`Success` value,
therefore fails the open closed with `AMS_MEL_PROVIDER_FAILED`, releases no
public owner, and leaves the one-shot rule established. The export count is unchanged at exactly 88
because no new C function was required; native CTest grows from 14 to 15.
`AMS.MEL.IR.Track.Metadata.Receive_Event` is the safe Ada home; the existing
report-only `Receive` is retained for current callers. Pinned Squall still
cannot attach Track, so positive `RequestSystemTrackData` behavior and payload
fidelity are mock-provider evidence only.

Task 029F adds the `@RequiredIfDetectCandidateObjects` `CandidateObjectMessage`
on that same inbound path. The pinned `TrackChannel` declares no
`send(CandidateObjectMessage)` and no `RequestFor<CandidateObjectMessage>`, so
this creates no request handle, completion, or worker thread. The complete
payload is deep-copied into event-owned storage: the fixed header, the canonical
`SensorInertialState`, the complete `HotRegion` vector, and exactly
`numberOfCOs` candidate objects from the upstream fixed 900-entry array. A
`numberOfCOs` above 900 and a `HotRegion` enum outside the upstream `0..3` range
are malformed. `EventData` now carries two `std::vector` members so the native
event owns every byte its spans reference, and those spans stay valid after
provider channel destruction and provider library unload.

Registration is conditional on
`ChannelMetadataCapabilityType::CandidateObjectMessage` being advertised, and it
is ordered `IRSTTrackReport`, `CandidateObjectMessage`,
`RequestSystemTrackData`. When the capability is advertised, any non-`Success`
return -- including `NotSupported` -- fails the open closed, which deliberately
differs from the `@Optional` `RequestSystemTrackData` refusal. Only
`CandidateObjectPreProcMessage` now remains unimplemented, so the Track API as a
whole is still not complete, and the CandidateObject evidence is likewise
mock-provider only. *(Superseded by task 029G, which implements
`CandidateObjectPreProcMessage` and completes the Track API; see below.)*

The candidate payload is **not** appended to
`ams_mel_ir_track_metadata_event_v1`. `docs/c-abi-policy.md` prohibits appending
fields to an existing fixed-layout record, and task 029E had historically
appended `request_system_track_data` to that same record. That current-`main`
layout is grandfathered and permanently frozen rather than broken again, and
029F introduces `ams_mel_ir_track_metadata_event_v2`, whose first member is the
complete frozen v1 record and whose second member is the
`CandidateObjectMessage` payload. `offsetof(v2, base)` is 0, no report or
`RequestSystemTrackData` layout is duplicated, and `base.kind` stays the single
discriminator.

`ams_mel_ir_track_metadata_event_view` keeps its exact signature and semantics
and still yields the frozen v1 record, so an existing consumer needs no
recompilation; the new `ams_mel_ir_track_metadata_event_view_v2` export is the
only way to reach the candidate payload. Exports move from 88 to **89** in
`exports.map`, in both dynamic symbol tables, in the raw Rust declarations, and
in the private Python bound names. The facade ABI version remains 0.1. The safe
Ada `Receive_Event` reads the v2 view for every kind and still copies every
value into Ada storage before closing the native event. Future Track metadata
additions must use a further version record rather than appending to v1 or v2.

For each added operation: sketch Ada usage, define C ownership, implement the
adapter, test from a C-compiled client, add Ada import/wrapper/tests, update the
coverage matrix. Test failures must not be hidden by reducing assertions.

The root package `AMS` is owned by `ams_mel` for now. Do not duplicate it in
future companion crates. No standalone-library `Interfaces` clause is needed
in this bootstrap. No SPARK proof is claimed for the provider or FFI boundary.

The validated private-import layout is:

```text
AMS.MEL                 public package and resource owner
AMS.MEL_C_API           private sibling containing C representations/imports
```

`AMS.MEL` may legally depend on this private sibling. The earlier proposed
`AMS.MEL.Internal` / `AMS.MEL.Internal.C_API` hierarchy caused a private-child
dependency failure and is not to be recreated. The corresponding proposal in
`reference/AMS_GRA_MEL_Design_Review.md` is retained as historical material but
is explicitly superseded by this decision.

## References

The full proposal and its source list are preserved in
`reference/AMS_GRA_MEL_Design_Review.md`. An implemented behavior may differ from
a proposal only through a documented design decision and matching tests.

## Task 029A Track declaration dependency

Task 029A adds no Track adapter or public ABI. It vendors a measured,
declaration-only Boost 1.83.0 header closure so the byte-identical pinned IR
MEL `TrackChannel.h` can compile even though its deferred `TrackDataUpdate.h`
declaration includes Boost uBLAS. Boost remains a private native include and
is not exported through the C ABI. A compiler dependency check rejects system
Boost leakage. The presence of TrackDataUpdate declarations and Boost headers
does not implement TrackDataUpdate; Task 029B is the earliest possible Track
adapter task.

## Task 033A RF is a distinct provider family

RF MEL is its own provider family, with its own factories and object graph. It
is not another IR Channel type:

```text
IR:  provider DSO -> Session -> Control -> attachChannel -> Channel
RF:  provider DSO -> create*MEL(config) -> RF MEL object families
                    (AdminMEL, C2MEL, COSITEMEL, DataMEL, DEAMEL, MonitorMEL)
```

Each RF factory has C linkage but a C++ signature (`std::shared_ptr<...>`
result, `std::string_view` argument). A future RF adapter must therefore
resolve it with `dlsym`, call it only from private C++, contain exceptions and
ownership, and keep the DSO loaded until every provider-owned RF object has
been destroyed. RF will not reuse the IR `Session`/`Control`/`Channel` handles.
`RFMEL::shutdown()` makes later requests undefined, so RF owners need their
own logical-close boundary. Task 033A vendors only the measured
DataMEL/RFMFAInfo declaration closure, including the AMS VITA headers it
reaches through `JobDataFormat.h`. It is a private native include that is not
exported through the C ABI, and it adds no RF adapter.

## Task 033B RF DataMEL foundation

Task 033B adds the first RF adapter, confined to `native/src/rf_data.cpp`:

```text
ams_mel_rf_data --shared_ptr--> RfDataState { unique_ptr<SharedLibrary> library;
                                              shared_ptr<rfmel::DataMEL> data; }
ams_mel_rf_mfa_info            owned value copies only (no provider pointer)
```

The existing private `SharedLibrary` is reused (now in
`internal/shared_library.hpp`). RF has its own state graph: no `SessionState`,
no `API_Manager`/`Control`, no IR completion admission, and no aperture config.
`createDataMEL` is called only from C++ with the exact pinned `fnDataMEL` type.
Every path destroys the DataMEL before the DSO unloads. Close consumes the
public owner before calling `shutdown()` exactly once. A throwing `shutdown()`
permanently retains the whole graph through the established allocation-free
emergency root, so process exit is the cleanup boundary and no retryable RF
Close exists.

The RFMFAInfo snapshot is point-in-time, because provider getters may perform
live I/O. After publication it is immutable and independent of the provider.
`quantizeDuration`, `getPhysicalData`, Tx power modes, endpoints, and every
other RF family remain out of scope. IR and RF share only family-neutral
helpers (`internal/provider_common.hpp`: diagnostics, UTF-8, and one
VersionInfo publication path). See `task-033b-rf-datamel-foundation.md`.

## Task 033D RF ComplexINT16 receive (implemented)

Task 033D implements the model below in `native/src/rf_product_rx.cpp`, with
shared private RF owner state in `native/src/internal/rf_product_rx.hpp`. No RF
callback lifecycle code lives in IR sources.

```text
ams_mel_rf_data ------------------+
RfChildClaim (request/endpoint) --+--> RfDataState { shared_ptr<SharedLibrary>,
                                   |                 DataMEL, children, close_requested }
ams_mel_rf_product_rx_request --> CreateCompletion <-- detached worker (sole future.get())
ams_mel_rf_product_rx ---------> provider endpoint + the SAME RfChildClaim
PermanentRxRegistration (never freed) { exact std::function, callback state, SharedLibrary }
ams_mel_rf_product_rx_event -----> owned sample/stream-ID vectors only
```

* **Children and parent-first lifetime.** One admitted create operation holds
  exactly one move-only `RfChildClaim` for its whole life: pending future,
  cached-but-unclaimed endpoint, then claimed endpoint. It is never counted
  twice. RF Data Close with live children consumes the owner and defers
  `DataMEL::shutdown()`. The release of the final child runs it exactly once,
  then destroys the DataMEL, then drops `RfDataState`'s library reference. A
  throwing shutdown retains the complete `RfDataState` graph permanently, as
  in Task 033B. It reports `AMS_MEL_PROVIDER_EXCEPTION` on a public caller and
  is retained silently on a worker.
* **`SharedLibrary` is shared, not duplicated.** `RfDataState::library` is now
  `shared_ptr<SharedLibrary>`. The DSO unloads when the last reference drops.
  With no callback registration, `RfDataState`'s reference is the only one, so
  Task 033B's open, snapshot, Close, destroy, unload sequence is unchanged. There
  is still exactly one dynamic loader abstraction.
* **Registration only at Claim.** Future success does not mean a callback is
  registered. The worker validates the endpoint (non-null, ID and format read
  once, format ComplexINT16), caches it, then waits for Claim or request
  Close. Only Claim calls `setDataReadyCallback`. An abandoned endpoint is
  destroyed by the worker without registration, so it never creates the
  permanent pin.
* **The exact callback lvalue is retained (stronger than the 033C note).**
  `setDataReadyCallback(DataReadyCallback&)` takes a non-const lvalue
  reference, and upstream does not promise that a provider copies it. A
  provider may copy it, move from it, or keep a reference to the exact object.
  The bridge therefore passes `registration->callback`, a member of a
  heap-stable `PermanentRxRegistration`. Once the call begins, whether it
  returns or throws, that registration is linked into an allocation-free
  intrusive root and never freed. The holder separately retains the callback
  state and the provider `SharedLibrary`, so safety does not depend on the
  `std::function` keeping its capture (the moved-from case).
* **Permanent registration holder vs. emergency retention.** The registration
  holder retains only the exact `std::function`, its small callback state, and
  the provider DSO. It does not retain the DataMEL, the endpoint, the public
  owner, or any event: endpoint Close swaps the queue out, so it holds no
  payload. This is deliberate fail-safe registration retention. It is
  distinct from the Task 033B full `RfDataState` emergency retention, which
  happens only when `shutdown()` throws.

* **Drain current callbacks BEFORE endpoint destruction.** Callback-scoped
  provider data (`JobDataPointer` samples, `ProductRxMetadata`) may be backed
  by storage the ProductRxEndpoint owns. The permanent registration keeps the
  callback object, its state, and provider machine code alive, but not that
  storage. Endpoint Close and throwing-registration cleanup therefore share
  one private `logical_close_and_drain` step and then destroy the endpoint:

```text
provider callback that saw Receiving
              |
              | may borrow provider data
              v
          in_flight > 0

Endpoint Close:
    Closed
      |
      v
drain CURRENT callbacks
      |
      | no pre-Close callback still borrows provider payload
      v
destroy ProductRxEndpoint
      |
      v
release child claim

Late callbacks may still start
      |
      v
Closed fast-path only
(no provider payload access)

Permanent callback + state + DSO retention remains
```

  The drain is not provider callback quiescence and never authorizes DSO
  unload. If Closed + drained cannot be established, the endpoint and child
  claim are retained forever.

The Task 033C design note below is preserved as the historical record. Where it
says the pin "retains only the library", 033D implementation evidence proves
the stronger rule above: the exact callback lvalue and its callback state are
also retained for the process lifetime.

## Task 033C RF receive ownership model (design note — implemented by 033D)

Task 033C pinned the `ProductRxEndpoint` declaration closure and recorded its
contract. The model below was the planned 033D direction and is now implemented
(see above), with the stronger exact-callback-lvalue rule.

```text
provider ProductRxEndpoint callback          (provider thread)
          |
          | provider-scoped sample pointer: JobDataPointer + element count
          | (valid only until the callback returns; Squall reuses the buffer)
          v
bridge callback  -- captures only shared_ptr<RfRxCallbackState>
          |
          | validate; immediate element-wise copy via real()/imag();
          | fail closed on non-representable metadata
          v
bounded bridge queue (DROP-INCOMING) of immutable owned events
          |
          v
C receive API (receive(timeout) -> owned event -> view/close)
```

The published interface has **no callback unregister** and does **not** state
that endpoint destruction makes callbacks quiescent. So:

* Endpoint creation is a `RequestFor` future and follows the existing request
  owner + completion worker + `wait(timeout)` model, not a blocking open.
* Close is logical first: it stops public delivery under the callback-state
  mutex, then drains the bridge's own in-flight callback count, and only then
  drops the provider endpoint outside every bridge mutex (033D corrective; see
  below). Bridge-state safety never depends on provider quiescence. Squall's receiver-thread join is
  provider-specific evidence only and is never used to relax a production rule.
* The DSO stays loaded while any endpoint, create request, or worker exists.
  **Once a callback has been successfully registered**, or
  `setDataReadyCallback` threw with unprovable registration state, a dedicated
  provider-library pin keeps the DSO mapped for the rest of the process.
  `in_flight == 0` plus endpoint destruction proves only that no bridge
  callback body is running *now*. It does not prove that a provider-held
  `std::function` copy can never start a new invocation, which would run
  through provider code (thread, call site, dispatch machinery). So it is **not**
  a DSO-unload condition. This is deliberate fail-safe retention, not a leak.

```text
callback registered
       |
       +------------------------+
       |                        |
logical endpoint Close          |
       |                        |
bridge callback drain           |
       |                        |
endpoint destruction            |
       |                        |
bridge resources reclaimable    |
(DataMEL may shut down normally |
 once all children are gone)    |
                                |
                                v
                       provider DSO pin retained
                       for process lifetime
```

The pin applies until a generic, provider-independent quiescence mechanism
exists. It retains only the library, not the DataMEL, the endpoint, the public
owners, queued products, or callback state. A throwing `DataMEL::shutdown()`
still retains the complete provider graph, as Task 033B specifies. See
`task-033c-rf-product-rx-contract.md` → "DSO lifetime".

## Task 029G CandidateObjectPreProcMessage and Track completion

Task 029G implements the `@Optional` `CandidateObjectPreProcMessage`
`TrackChannel` callback, the last previously unimplemented TrackChannel
metadata surface.

Upstream declares no `send(CandidateObjectPreProcMessage)` and no
`RequestFor<CandidateObjectPreProcMessage>`, so this is inbound callback
metadata, not asynchronous request work. It flows through the existing path:
provider callback, complete native deep copy, the one bounded DROP-INCOMING
Track metadata FIFO, the event owner, the v1/v2/v3 event views, and safe Ada
`Receive_Event`. No Submit, Wait, request handle, Completion, WorkerInput,
completion thread, `CommandStatus` mapping, or `TrackState::requests`
participation is involved; the callback creates zero `RequestFor` workers.

Registration order becomes `IRSTTrackReport`, `CandidateObjectMessage` (only
when advertised), `RequestSystemTrackData`, `CandidateObjectPreProcMessage`,
and that order makes the four-kind FIFO deterministic. Every registration runs
with `TrackState::mutex` released, so a provider may deliver synchronously from
inside `registerMetadataCallback`.

The `@Optional` annotation on this callback governs its refusal policy, which
follows the 029E `RequestSystemTrackData` precedent rather than the 029F
advertised-capability precedent: `Return::NotSupported` is **non-fatal** and
metadata open continues, while `Return::Fail` (documented upstream as a
conflicting existing registration), `BadPointer`, `NotImplemented`, and any
future value fail closed with `AMS_MEL_PROVIDER_FAILED`. A throwing
registration maps to `AMS_MEL_PROVIDER_EXCEPTION`. Naming the type in
`ChannelMetadataCapabilityType` does not upgrade an `@Optional` callback into a
required one.

The PreProc payload is **not** appended to v1 or v2. v2 is frozen as 029F
published it, and 029G adds `ams_mel_ir_track_metadata_event_v3` with the
complete v2 record as its first member plus one new export,
`ams_mel_ir_track_metadata_event_view_v3`. The export count becomes 90 and the
ABI version stays `0.1`. The event owner additionally holds the PreProc vector,
so every span stays valid until `event_close`, including after provider channel
destruction and provider library unload.

All published TrackChannel-specific surfaces are now represented:

```text
@RequiredIfTrack core                                      complete
@RequiredIfTrackUpdate TrackDataUpdate                     complete
SystemTrackDataResponse                                    complete
RequestSystemTrackData                                     complete
@RequiredIfDetectCandidateObjects CandidateObjectMessage   complete
CandidateObjectPreProcMessage                              complete
Track API overall                                          complete
```

Native C and safe Ada Track coverage is complete for those surfaces. The safe
Rust Track API and the public Python Track API remain intentionally absent, and
positive Track behavior remains mock-only because pinned Squall cannot attach a
Track channel through `Control::attachChannel`.

## High-rate data ownership

The project separates two categories of value with two different ownership
rules. This split is deliberate and is the intended model for all future
bulk-data work, not an IR-only convenience.

```text
Small / control-plane values
    copied into language-owned representation

Bulk / data-plane values
    native limited owner + temporary borrowed language view
```

Control-plane values -- configuration, identifiers, status, capability
descriptions, command results, metadata records, strings, flags, sensor
inertial and navigation state -- are copied into ordinary language-owned
storage at the boundary. That is simpler, has no lifetime coupling, and costs
nothing measurable relative to their size.

Bulk data-plane values -- image pixel payloads, and in future RF sample and
packet payloads -- are not copied across the boundary. Instead the binding
follows one chain:

```text
native backing owner
        |
        v
opaque C handle
        |
        v
limited language owner
        |
        v
temporary borrowed language view
```

Each level has one job. The native backing owner keeps the storage alive. The
opaque C handle is the only thing that crosses the ABI; no borrowed span is
ever published without an owning object whose lifetime is explicit. The limited
language owner is non-copyable and releases the handle exactly once, by
explicit close or by finalization. The borrowed view is valid only for the
dynamic extent of a borrow operation and must not be retained.

Task 030A implements this for IR Images as `AMS.MEL.IR.Image.Frame_Lease`
(`Acquire_Frame` / `With_Pixels` / `Copy_Pixels`), documented in
`docs/task-030a-zero-copy-ada-frame-lease.md`. Task 030B then made the native
backing owner retain the provider's `irmel::Buffer` itself instead of a copy
of its bytes, documented in `docs/task-030b-provider-buffer-zero-copy.md`.
The public Ada API is unchanged between the two: only the meaning of the owner
changed, which was the point of establishing the pattern first. The public
names are IR-specific; the ownership pattern is not.

### Intended reuse

```text
IR Images            implemented (Tasks 030A and 030B,
                     provider Buffer -> native lease -> Ada view)
RF Receive products  I/Q sample buffers, real sample buffers, VITA packet
                     buffers, PDW buffers
RF waveform          waveform source buffers for streaming transmit
Stacked Image        stacked/accumulated image products
```

None of those are implemented here. The point of doing IR first is that the RF
work should inherit a lifetime model that already has deterministic alias
evidence behind it.

### Memory-kind constraint

Future RF MEL bulk buffers may be supplied from memory regions that are not
ordinary host heap:

- heap allocations;
- RDMA-registered memory;
- GPU memory;
- FPGA or other device memory.

The design must therefore not assume that every future bulk buffer is ordinary
CPU-copyable memory that can be bound directly to an Ada array or a Rust slice.
`docs/c-abi-policy.md` already states the host-memory-only constraint for the
current profile: device addresses must not be dereferenced as Ada arrays or
Rust slices. The opaque-handle level of the chain is what makes a future
non-host memory kind expressible at all -- an owner can describe memory that
the language cannot directly address, while a raw borrowed span cannot. No
RDMA, GPU, CUDA, or FPGA support is implemented in Task 030A.

### Current IR copy levels

```text
After Task 030A                       After Task 030B
MEL provider Buffer                   MEL provider Buffer
        |                                     |
        | COPY remains                        | retained owner
        v                                     v
native snapshot storage               native lease
        |                                     |
        | ZERO COPY                           | ZERO COPY
        v                                     v
Ada borrowed view                     Ada borrowed view
```

Task 030B removed the remaining bridge copy. The frame callback now retains
the callback's `std::shared_ptr<irmel::Buffer>` in the queued frame and
publishes a borrowed span over `Buffer::getImageAddress()`, so the bridge
performs zero bulk payload copies from the MEL callback buffer into Ada and

```text
Buffer::getImageAddress() == snapshot pixels.data == Ada view address
```

holds for a non-empty frame. That claim is scoped to the bridge: Squall still
copies received UDP bytes into the registered MEL host buffer, and nothing is
claimed about NIC DMA, kernel socket buffers, or sensor transport. See
`docs/task-030b-provider-buffer-zero-copy.md`.

### Lifetime consequences of retained external storage

Borrowing external storage rather than copying it changes the lifetime
contract, and this is the part intended to carry over to RF.

A live lease retains whatever is required to keep the borrow valid: the host
storage, the provider object that owns it, the provider library containing the
virtual release implementation, and enough of the channel graph that release
is still legal. Consequently:

```text
logical Stop / Close / Session close   completes immediately
physical provider teardown             deferred until
                                         requests == 0
                                         AND retained buffers == 0
```

This reuses the existing Image deferred-teardown state machine rather than a
second one, and extends the documented single-lock invariant: every racing
read and write of the request count, the retained-buffer count, cleanup
ownership and result, public owner state, channel owners, and logical
lifecycle happens under `CallbackState::mutex`, while provider calls never
do. Release follows `lock to claim, unlock to release, lock to publish`.

Two further properties generalize beyond IR. Retained external storage means
real backpressure: with `Buffer_Count = N` at most N provider buffers can be
checked out at once unless the provider has another independent pool, and a
slow consumer causes provider-level drops rather than a silent hidden copy.
And a failed or uncertain hand-back is a first-class safety case: the owner
and its storage are retained permanently rather than freed, the outstanding
count is never decremented, release is never retried, and explicit operations
report the failure while finalization stays non-raising.

### Reusing the model for RF, not the type

```text
external / native backing storage
        |
explicit lifetime owner
        |
opaque bridge lease
        |
language-safe borrowed view when CPU-addressable
```

What transfers to the RF MEL data plane is the **ownership model**, not the IR
array-view type. The IR view is CPU-host-memory-specific: it binds an Ada
`Pixel_Array` to a host address. RF MEL permits receive endpoints over
application memory regions that may be ordinary host memory, RDMA-registered
memory, GPU memory, or FPGA/device memory, so a future RF region must not be
assumed representable as an Ada `Pixel_Array`. RF may instead require typed
spans, byte spans, packet views, device-memory descriptors, or explicit
non-CPU-addressable handles, and the final "language-safe borrowed view" level
of the chain simply does not exist for a non-CPU-addressable region.

The opaque owner level is what makes that expressible at all. RF receive,
RDMA, GPU/CUDA, FPGA mappings, and Stacked Image remain unimplemented. Task
033B adds only the RF DataMEL foundation (load, version, owned RFMFAInfo
snapshot, shutdown/Close) and no RF data plane. Pinned Squall's RF receive endpoint copies and decodes UDP payloads
into its own IQ vector, so it provides no RF zero-copy evidence.
Task 033C confirms this from the published contract: the ProductRxEndpoint
callback buffer is callback-scoped, and Squall reuses it for every later
datagram. The first RF receive slice therefore copies. See the Task 033C
design note above.
