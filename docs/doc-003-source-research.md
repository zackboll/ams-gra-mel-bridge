# DOC-003 source research

## Starting identity and qualification

Verified fetched main and task base:
`e7a652c7a62e66d58c275f6ab1f09bb6e2021622`.
PR #102 is merged and its reviewed implementation
`f494782b907a3eea43a7d7a3feff10079784ccfc` is an ancestor.
The original checkout was clean and remains untouched. No conflicting local or
remote feature branch existed. Existing worktree registrations were preserved.

Worktree: `/home/zboll/git/ams-mel-doc-003`.
Branch: `docs/003-ir-image-health-instrumentation`.
External logs and pristine copies:
`/home/zboll/git/ams-mel-doc-003-evidence`.

The unchanged qualified generation script reproduced 934/4,295 documented,
3,036 warnings (2,963 public, 73 private-part), 1,238 source declarations and
1,237 native sections. All 75 documentation unit/mutation tests passed before
Ada comment edits. Alire 2.1.1, GNAT Native 16.1.0, qualified GPRbuild 26.0.1
distribution and GNATdoc 26.0.0 were used with trailing style.

Post-merge documentation run 38673364878 succeeded on the exact base.
Bootstrap run 38673364892 was in progress at initial inspection; no success
is inferred from that observation.

## Research in progress

This opening section records the initial checkpoint. Later sections supersede
its in-progress state; submission and execution gates are recorded separately.

Pinned IR MEL revision: `8d9224519f12b44e0b28815755c56a32a28d24a0`.
Pinned Common MEL revision: `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.
Pinned AMS Math revision: `00be45190f0e47d268cece8b8c2f8fb58b5418d2`.
Pinned Squall revision: `b1015728f904c799fa0c07489fce48e78f67845f`.

### Full-frame model

FrameHeader defines row-major raster storage, nanosecond image time (integration
start for staring, first-line time for scanning), pixel dimensions, radians FOV,
ordered flags, dithering sign and sensor-coordinate offsets. Its numBands prose
calls the field a spectral-band indicator despite its name. Preserve the raw
number without inventing a spectral decoding rule.

SensorInertialState specifies sensor-to-NED and NED-to-ECEF quaternions, ECEF
position in meters and velocity in meters/second. SensorNavState preserves
separate orientation variants for orientation and both derivatives, with an
explicit coordinate-system enum. Its header does not establish component units
for every coordinate system or derivative. NavError defines three/four error
components (W only for quaternion error), not a covariance estimator or statistical
confidence level. Uncertainty defines two uint32 values without bit meanings.

Ada conversion and native copy_full_header preserve these values without
normalization, transformation or numerical-validity checks. Rich Ada tests verify
mixed orientation variants, duplicate flag ordering, all metadata families and
owned pixels after stream/Session closure. Receive uses the snapshot path, copies
the payload, and closes the native snapshot before returning; its private cleanup
helper ignores the native close status, unlike explicit lease Close.

The corrected release protocol and navigation/Close synchronization supersede
earlier historical teardown claims. They must be consulted before documenting
lease closure or pending-request lifetime.
## 003B — Provider buffer ownership and release

The pinned Buffer interface requires release to permit buffer reuse. Its header
and ImageListener establish the published callback hand-back protocol, not a
requirement to release within the callback invocation. The pinned Squall
`SquallImageChannel.cc` was extracted directly from commit
`b1015728f904c799fa0c07489fce48e78f67845f`. Its RequeueBuffer release puts the
underlying registered buffer back in the pool, whereas udpCallback line 442
copies UDP bytes into the host image buffer before notifying the bridge.

The Frame_Lease borrowed path performs no bulk image-payload copy from the MEL
provider's registered host buffer into Ada. This is not sensor-to-Ada, DMA or
kernel/network-stack zero-copy. Native QueuedFrame retains the exact provider
buffer, registered host storage and provider graph; the safe Ada borrow imports
an array at the native payload address. Copy_Pixels allocates and copies.

The final corrective release document supersedes historical 030A/030B teardown
claims. Explicit lease Close consumes the handle even when release fails, clears
payload address/size, and reports Provider_Error. Failed release is not retried;
uncertain ownership permanently retains the exact buffer and provider graph.
Finalization suppresses the report, not the safety retention. Copied metadata
remains in the Ada lease after explicit Close. Stop/Close need not await the
application releasing a held lease, but may wait for native release/cleanup work;
there is no blanket at-once or nonblocking teardown promise in the new prose.

With_Pixels is a synchronous read-only-by-contract alias, valid only during the
processor invocation. Callback exceptions propagate without consuming the lease.
Declared open preconditions and explicit body closed/address checks are distinct.
Independent acquired leases can be processed separately, but arbitrary concurrent
lifecycle operations on the same Ada object are not supported. Lease tests and
native release/handoff mutations substantiate these distinctions; no real-provider
execution was performed in this documentation task.

## 003C — Navigation and corrective teardown

Common MEL NavigationReport.h supplies radians for latitude, longitude and wander
angle, meters for altitude, meters/second for NED speed and meters/second squared
for acceleration. It stores a separate nanosecond attitude-rate timestamp and
all 18 named covariance terms. Covariance units, attitude-rate units, magnetic
heading units and timestamp clock/epoch are not explicitly specified. The
altitudeMSL member is described as barometric altitude without an explicit unit.
Those omissions are recorded, not filled from an inaccessible IDD.

Ada Raw_Covariance and native convert_report preserve each distinct covariance
entry and input field; they do not compute a solution. Native admission permits
Attached and Running stream states. Provider send occurs unlocked because it may
synchronously invoke navigation metadata. A private worker consumes the future
once, caches the result and retains provider resources independently of public
owners. Timeout is not cancellation; Close is public-handle release only.
Outcome accessors have the exact declared preconditions, not invented checks in
their expression-function bodies. Long rejection descriptions use checked cached
reads; invalid UTF-8/NUL receives a fallback.

The corrective navigation/Close report describes the current lifecycle lock,
cleanup ownership and outcome-adoption protocol, superseding original Task 027B
synchronization. Native tests cover the failed detach, successful cleanup and
request-decrement/cleanup-claim race schedules with mutations. Pending requests
and leases independently participate in deferred physical cleanup. Pinned Squall
only echoes report time with zero command/request IDs and ignores most kinematics;
rich mock fidelity tests, not Squall's limited behavior, prove complete conversion.

## 003D — Image metadata

BadPixelList explicitly separates reported byte size and count from the actual
vector. Native copying traverses the complete vector and Ada retains reported
values separately. Coordinates are not renumbered; the pinned BadPixel header
specifies no coordinate-origin convention. Only Unknown reason is defined/bound.
AMS Math RangeAzEl defines radians for angle components. LineOfSightReport uses
AzEl for pointing rates without specifying the time denominator; its validity
field names three affected values but defines no bit positions. Image rotation
is explicitly radians. LineOfSightEuler separately specifies radians and
radians/second, and a nanosecond time of validity. These distinct evidential
strengths are preserved in the prose.

Native ir_image.cpp registers exactly bad pixels, LOS report, LOS Euler and
navigation response; the public optional upstream quaternion, distortion-map,
NUC and other interfaces are not bound by this Ada package. All four kinds share
one bounded drop-incoming queue. Successful enqueue establishes order, not
concurrent callback entry or timestamps. Received counts callback entries;
malformed/copy failures and full-queue drops have separate saturating counters.
Registration is once per image stream, including failed attempts, while Attached
or Running. Pinned Squall can emit metadata synchronously during registration.
No unregister operation exists. Metadata Close deactivates consumption without
closing the image stream or promising callback quiescence. Native temporary events
are released before owned Ada values return. Wrong-kind accessors explicitly
raise Provider_Error, unlike the Health variant-access path.

## 003E/003F — Health channel and subsystem data

HealthStatusChannel.h establishes the typed channel and six required callbacks.
The native bridge verifies dynamic type and advertised channel type, attaches
without enabling, permits capabilities and metadata registration before Enable,
and exposes inherited common services through weak Channel views. The view does
not keep the typed owner alive. Child state retains Session/provider resources;
pending inherited requests can defer cleanup after public Close. Native detach
failure can retain the public owner for retry or deliberately retain an orphaned
graph. Automatic finalization attempts cleanup without raising; it does not prove
physical unload or successful detach.

Pinned CommonIR_MEL.h describes Critical as no heartbeat, Available as no failure
and NotPresent as not installed (physical or simulated). Other failure thresholds
are unspecified, as are the VSA acronym expansion and mode transition criteria.
Subsystem criticality has no documented numeric severity or bitmask meaning.
SubsystemCSCIInfo explicitly calls BIT_report a counter incremented on BIT_Status
changes, not a BIT report payload. Version has four unsigned source/revision
components with no source encoding; it is distinct from provider and bridge ABI
versions. Source-reported subsystem/CSCI counts are preserved separately from
actual copied vectors; rich tests deliberately report 99/88 while copying two/two.

Pinned Squall HealthStatusChannel stores one callback per type, copies closures
under a mutex in its polling loop and joins that thread on destruction. This
supports the distinction between public metadata Close and physical provider
teardown, not a general immediate-unregister or lifecycle-concurrency promise.

Shared MFA, BIT/fault, discrete and security data reuse the DOC-002 canonical
AMS.MEL.Status descriptions. Health metadata Receive deeply copies every active
variant and closes native temporaries. Accessor Pre aspects require the matching
kind; enabled precondition violations raise Assertion_Error, and direct wrong
variant selection is subject to Ada discriminant checking (Constraint_Error),
not an invented Provider_Error conversion. Native callbacks count malformed
conversion separately; copying/allocation exceptions mark metadata failed.
Queued events drain before an empty failed/inactive status is reported.

The existing Status renaming is still a source declaration, with no native
GNATdoc section. Package prose refers readers to canonical AMS.MEL.Status without
crediting that alias in coverage. The exception file remains byte-identical.

## 003G — Instrumentation

Pinned InstrumentationChannel.h is RequiredIfInstrumentation, not a promise that
every IR provider implements instrumentation. The command controls recording
level; Priority is exactly Normal = 0 and Debug = 1. InstrumentationReport has
four fields and no variable data payload. Its Size units/payload relation and
Timestamp epoch are not stated. The same owned report record serves future and
metadata paths. Provider rejection is a normal result; exceptions, null success,
unknown priority/error enums and deferred-cleanup failure are operational errors.

Submit requires Enabled; common services/capabilities and metadata registration
are allowed while Attached. Native send is unlocked for synchronous callbacks.
A private worker consumes the future once and caches terminal results. Timeout
and request Close do not cancel; pending work pins the native provider graph after
public channel/Session closure. Weak common views do not own the typed channel.

The metadata callback validates and copies into a bounded native drop-incoming
queue. Receive returns owned Ada values, not provider pointers. Registration is
once, may synchronously emit, and has no unregister counterpart. Metadata Close
only deactivates public consumption. Actual provider-channel destruction followed
by bridge callback draining is the cleanup boundary, potentially deferred behind
pending requests. Malformed null/priority values increment the malformed counter;
callback copying/allocation exceptions instead fail the metadata stream. Rich
native/Ada tests cover both synchronous registration/send callbacks, repeat Wait,
long rejection recovery and parent-first closure. The pinned Squall source tree
contains no Instrumentation channel implementation, so no invented Squall
instrumentation behavior is used as authority.

## Provenance audit and remaining ambiguities

Original upstream Git repository remotes and exact commit objects were verified.
All 68 vendored IR headers, 28 Common MEL headers and five AMS Math headers were
compared byte-for-byte to pinned source (101 headers total). No missing necessary
interface header required retrieval from unpinned main. Pinned Squall image and
health sources were extracted from their original repository by exact commit.
SHA-256 records and extracted sources are retained externally. No IDD review,
provider execution, hardware qualification or stronger numerical semantics is
claimed.

The grouped structured ledger records each major declaration family, upstream
paths/revisions, bridge implementations, tests, confirmed claims and discrepancies.
Unspecified units, timestamps, validity bits, criticality/enum thresholds and size
encoding remain explicitly unresolved in pinned source; safe representation and
implemented behavior are documented rather than inventing missing semantics.

## Final local qualification

Final documentation and integrity gates passed after formatting. Sequential
native/Ada/canonical-check validation is in progress; completion and actual
results must be recorded after inspecting exit files and logs. No commit, push
or PR submission is claimed yet.

### Hosted starting-main qualification follow-up

Bootstrap run 38673364892 completed with failure on exact starting main.
The clang Release repeated suite failed rf-job-interval-status at
`native/tests/test_rf_job_interval_status.c:381`, assertion
`shutdowns()==before+1`. Other Bootstrap jobs passed. Documentation run
38673364878 passed; its checkout log confirms the exact starting-main SHA.
This inherited stress failure is retained as evidence, not suppressed or
reclassified as success. Feature-branch CI is evaluated separately.

## Final local qualification — complete

All required gates passed on the final formatted six specifications. A final
parameter-description review was followed by fresh formatting, two strict
byte-identical documentation generations and a second complete sequential native
qualification. Exit files/logs were inspected after command completion; the
recorded six source SHA-256 hashes still match the final submitted sources.

| Gate | Actual result |
| --- | --- |
| make format-ada and final make check-ada-format | PASS |
| Strict qualified GNATdoc and stable-identity ratchets | PASS |
| Renderable target coverage | 693/694; exactly one unchanged alias omission |
| Native declarations/signatures/anchors | 1,238 source / 1,237 native, unchanged |
| Fresh site regeneration twice, every output byte | PASS, identical |
| HTML structure and local links | PASS, 31 pages / 1,307 links |
| Documentation unit/mutation suite | PASS, 75 tests |
| Dedicated tracked-inventory/source-integrity suite | PASS, six tests |
| Literal-aware source integrity at task base | PASS, 1,178 protected files |
| Original DOC-001 integrity boundary | PASS, 1,162 protected files |
| make test-native | PASS, 434/434 |
| make test-ada | PASS, full safe Ada contracts and focused repeats |
| Canonical make check, sequential | PASS, build isolation, native suite, format, Ada and text checks |
| Final make check-ada-format after make check | PASS |
| Baseline-delta, Git whitespace and final newlines | PASS |

Native gates never overlapped against mutable CMake trees. No local inherited
stress failure occurred in these final sequences. No tests/contracts were changed
or weakened. Real-provider/hardware execution is not claimed. Only twelve task-
authorized files changed; prior ledger entries and DOC-001/DOC-002 history remain
unchanged. Main was refetched and remained at the exact base. Local qualification
is complete; hosted feature-branch results and independent review are separate.
