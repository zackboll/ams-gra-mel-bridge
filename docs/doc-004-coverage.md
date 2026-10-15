# DOC-004 coverage checkpoints

Base: `61ac2aeb5f6ed1be70a853015861d5decb352dbd`.

## Pristine reproduced baseline and checkpoint 004A

| Package | Pristine documented | 004A documented | Elements |
| --- | ---: | ---: | ---: |
| AMS.MEL.IR.Channel | 1 | 202 | 289 |
| AMS.MEL.IR.C2 | 1 | 1 | 179 |
| AMS.MEL.IR.C2.Common | 0 | 0 | 33 |
| AMS.MEL.IR.C2.Metadata | 0 | 0 | 288 |
| AMS.MEL.IR.Track | 0 | 0 | 55 |
| AMS.MEL.IR.Track.Metadata | 0 | 0 | 153 |
| AMS.MEL.IR.Track.System_Data | 0 | 0 | 124 |
| AMS.MEL.IR.Track.Updates | 0 | 0 | 153 |
| Target total | 2 | 203 | 1,274 |

004A independently fixes 201 coverage identities and removes 205 public warning
identities, with zero additions. Repository coverage is 1,826/4,295 (42.5146%),
with 2,469 gaps. Raw warnings are 2,194: 2,121 public plus 73 private-part.
All other package coverage and all private warning identities are unchanged.

Native inventory remains 1,238 source declarations / 1,237 native sections.
The native profiles/anchor manifest and exact existing renderer exception are
unchanged. Both complete generated sites are byte-identical. HTML validation
passes 31 pages / 1,307 local links. All 75 documentation tests pass.
Format/check, removal-only baseline validation and literal-aware source integrity
against the task base pass (1,178 protected files).

The current checkpoint documents capability declarations, constructor/accessor
profiles, every capability enum literal and collection/record member. It does
not qualify the remaining View/request documentation. 1,071 target gaps remain.
Final gates, full production test qualification and PR submission are pending.

## 004B — common View and requests qualified

Resumed existing worktree without resetting, stashing or recreating the branch.
Main and HEAD remain `61ac2aeb5f6ed1be70a853015861d5decb352dbd`; original
ancestry verified, no running task validation found. The seven-file checkpoint
was preserved externally in `r1/resume.patch` and copies of untracked reports.
Qualified reproduction exactly matched 004A before edits.

Channel is now 289/289. Relative to 004A, 87 gaps and 77 public warnings were
removed, zero added. Repository: 1,913/4,295 documented, 2,382 gaps; warnings
2,117 raw = 2,044 public + 73 unchanged private-part. All other packages, native
profiles/anchors and renderer exception are unchanged. Five existing 004A enum
prose attachments temporarily became uncredited when new identifiers changed
native cross-reference rendering; equivalent prose without underscored names
restored them. No 004A coverage identity was removed.

Source research: Channel body, native common request engine and private access/
claim definitions, family lifecycle adapters and tasks 032B1/032B2. The existing
five-family common tests cover attachment/enabling, expired views, independent
request lifetime, repeated cached waits, rejection and Return::Fail. The native
submission path acquires a Session permit and family claim before unlocked send.
A weak handle does not establish resource existence or lifecycle eligibility.
Admitted work retains resources independently. Closing an observer is not
cancellation or direct pending-permit release. Accessor preconditions are not
independently checked by expression-function bodies. Outcome Success can carry
Fail. Physical execution or metadata correlation is not established by Wait.

Required format/check, strict generation twice, byte-identical site, HTML links,
75 documentation tests, six integrity tests, task-base literal integrity, Git
whitespace and final newlines passed. Evidence and failed intermediate runs:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004b-*`. Next: 004C C2.
Final production qualification and publication remain pending.

## 004C — AMS.MEL.IR.C2 qualified

Package coverage 179/179; 178 fixed gap identities and 165 public warnings removed, zero additions. Repository 2091/4,295; 2204 gaps. Raw/public/private warnings 1952/1879/73. Other packages, private warnings, profiles/anchors and renderer exception unchanged. Format/check, two strict byte-identical generations, HTML/links, 75 documentation tests, six integrity tests, task-base token integrity and whitespace/newlines pass. Evidence: `/home/zboll/git/ams-mel-doc-004-evidence/r1/004c-*`.

Mode/BIT/config typed sends require Enabled; shared C2 state remains distinct from the Status Ada type. Scan controls remain unsigned scalars. Validation checks representation domains, not physical feasibility. Config/fault strings copy full UTF-8 spans including NUL. Config nanoseconds establish no epoch. BIT-specific functions populate only one list; nonempty preconditions and assertion-disabled body behavior are documented. Native claim precedes unlocked send; callers still serialize Ada owner operations. Deferred cleanup retains pending provider work. Source: C2 spec/body, native ir_c2.cpp, pinned C2Channel/ModeCmd/BIT_Command/ConfigSetCommand/ScanParam/ScanType/CommonIR_MEL/CommonMEL headers, C2 Ada/native tests and corrective unlocked-submission report. A tool-wrapper timeout interrupted dedicated integrity tests after deterministic generation; the dedicated suite was rerun successfully before credit. Next: 004D compatibility.

## 004D — AMS.MEL.IR.C2.Common qualified

Package coverage 33/33; 33 fixed gap identities and 33 public warnings removed, zero additions. Repository 2124/4,295; 2171 gaps. Raw/public/private warnings 1919/1846/73. Other packages, private warnings, profiles/anchors and renderer exception unchanged. Format/check, two strict byte-identical generations, HTML/links, 75 documentation tests, six integrity tests, task-base token integrity and whitespace/newlines pass. Evidence: `/home/zboll/git/ams-mel-doc-004-evidence/r1/004d-*`.

Legacy direct typed-C2 wrappers share native request engines/accounting and common payload types, not canonical Ada request owners. Legacy diagnostic helper substitutes fixed text for an empty diagnostic and starts with a larger buffer; cached retry preserves long diagnostics. Preconditions lack independent body checks. Source: C2.Common body, native ir_c2.cpp and common_channel_requests.cpp, pinned Channel.h and comms declarations, legacy Ada tests and public-common native comparison cases. Next: 004E metadata. Final production qualification and publication pending.

## 004E — C2 metadata qualified

All 288/288 elements documented. Exactly 288 gaps and 288 public warnings
removed, zero additions. Repository 2,412/4,295 documented, 1,883 gaps; warnings
1,631 raw = 1,558 public + 73 unchanged private. Other package identities,
native profiles/anchors and renderer omission unchanged. Two strict generations
are byte-identical, all 75 documentation and six integrity tests pass, required
format/check, HTML links, source integrity and text gates pass. Evidence:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004e-*`.

Research: public spec/body; native ir_c2.cpp registration/copy/queue/cleanup;
pinned CommandStatus/CommonIR_MEL/Channel/C2Channel and Common MEL
BIT_Configuration/ActiveBIT/CompletedBIT/Fault; Ada C2 metadata tests, native
C2 metadata/callback tests and task-018 corrective validation. Command metadata
is independent of future Wait. Base registration is one-shot, ordered BIT
configuration/command status/BIT status, with no capability gate. Comms is a
separate one-shot attempt valid Attached/Enabled and not automatically opened.
Failure cannot undo upstream registrations; callbacks retain inactive/failed
state. Close is not unregister or quiescence. All kinds share bounded incoming-
drop FIFO by successful enqueue. Malformed values are nonterminal drops; callback
exceptions fail the queue. Queued events drain before stopped/failed reporting.
Accessor preconditions have no independent body checks. Counters saturate.
Every Cannot_Comply value retains numeric mapping; vague source categories add
no invented policy. BIT-interface values two/three retain the DOC-002 name/prose
ambiguity. Active completion 1.0 means 100%, with values above one permitted.
Active/completed BIT timestamps are labeled UTC upstream without a more specific
epoch; fault detection chrono alone establishes no epoch. Next: 004F Track.
Final production qualification and publication remain pending.

## 004F — Track foundation qualified

Track 55/55, with 55 gap and 49 public-warning removals, zero additions.
Repository 2,467/4,295 documented, 1,828 gaps; warnings 1,582 raw = 1,509 public
+ 73 unchanged private. Other package identities, anchors/profiles and renderer
exception unchanged. Format/check, two strict byte-identical generations, HTML
links, all 75 documentation and six integrity tests, literal-aware integrity and
text checks pass. Evidence: `r1/004f-*` under the external evidence directory.

Pinned IRSTTrackReport establishes meters for range/error, radians for spatial
extent, nanosecond age since first detection and quality zero-to-one (not
enforced). NED components, intensity/SNR and clutter lack stronger source units
or scales. The filtered NED field's adjacent comment also says latest measurement;
no algorithm is invented. Track owner retention and deferred physical cleanup
include common requests, updates and system responses; provider destruction
then callback drain is the quiescence boundary, not disable return alone.

Sources: Track spec/body; native ir_track.cpp lifecycle/copy/common adapter; pinned
TrackChannel/IRSTTrackReport/CommonIR_MEL and Common Directional; native/Ada Track
foundation/report tests; tasks 029B1/B2/B3. Exact pinned SquallIRLoaders.cc was
extracted outside Git: attachChannel has Image/C2/Health cases only, then null.
Thus positive Track behavior remains mock-only; Squall qualification is expected
negative. Next: 004G Track metadata. Production final gates and publication pending.

## 004G — Track metadata qualified

All 153/153 elements documented. Removed 153 gap and 152 public warning
identities, zero additions. Repository 2,620/4,295; 1,675 gaps. Warnings 1,430
raw = 1,357 public + 73 unchanged private. Other package coverage and private
warning identities, native anchors/profiles and renderer exception unchanged.
Format/check, two strict byte-identical generations, HTML/links, all 75 docs
tests and six integrity tests, literal integrity, text checks pass. Evidence:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004g-*`.

All four families are inbound callbacks. Report registration is conditionally
required; candidate registration is gated by advertisement and must succeed
when advertised; system-data-request and PreProc callbacks tolerate optional
NotSupported. Registration order and one-shot partial-failure retention are
verified in native ir_track.cpp; callbacks run outside the Track lifecycle lock.
Queue order means successful enqueue, not timestamp order. Report-only Receive
consumes a wrong-kind event and raises rather than skipping/requeueing it. Native
events are disposed on Ada conversion failure. Counters saturate; malformed
events drop nonterminally while callback exceptions fail the stream.

Candidate message capacity 900 has a meaningful declared prefix; PreProc vector
length is independent of header count. Binary32 CFAR is average false alarms
per pixel. Header stack index and hot-region pixel bounds are explicitly zero-
based upstream; candidate subpixel origin and edge inclusivity are not inferred.
Navigation quaternions/position/velocity retain their defined frames and units;
uncertainty/validity bits, SIR/SNR scales and sigma units are not invented. Signed
3x3 background samples, vector order/duplicates and per-entry navigation survive.
Sources: Track metadata spec/body, native copy/queue/registration, pinned
TrackChannel and all candidate/header/PreProc/hot-region/inertial/request headers;
native/Ada Track metadata tests; tasks 029E/F/G. Mock positive only; pinned Squall
negative attach boundary unchanged. Next: 004H optional outbound System_Data.
Final production qualification and publication pending.

## 004H — System_Data qualified

System_Data 124/124; removed 124 gaps and 120 public warnings, zero additions.
Repository 2,744/4,295 documented, 1,551 gaps. Warnings 1,310 raw = 1,237 public
+ 73 unchanged private. Native profiles/anchors, renderer exception and other
package coverage unchanged. Format/check, two strict byte-identical sites, HTML
links, 75 documentation tests, six integrity tests, literal integrity and text
checks pass. Evidence: external `r1/004h-*`.

Optional outbound response is distinct from inbound request metadata. Enabled-
only send copies every field, including values marked invalid, without zeroing
or normalization. Time is signed nanoseconds with no epoch; ranges/errors are
meters, rates/errors meters/second and AzEl/errors radians. Future Success can
carry CommandStatus whose command state is Rejected; only ErrorOr gives a rejected
future outcome. Preconditions lack independent body checks. Explicit Ada request
Close ignores native close status and is not cancellation. Shared Track request
accounting defers disable/detach until all update/response/common work finishes;
failed deferred cleanup can replace a terminal outcome with provider failure
and safely retain the graph. Sources: System_Data spec/body, native ir_track.cpp
build_response/submission/complete/cleanup, pinned SystemTrackDataResponse,
TrackChannel, CommandStatus/CommonIR_MEL and Math RangeAzEl; task 029D, native
test_ir_track_system_response.c and Ada system-data tests. Positive mock only.
Next: C2 NUL correction requalification, then 004I updates. Final gates pending.

### Semantic correction discovered during cross-family review

Earlier 004C notes/commentary incorrectly described embedded NUL as preserved
in configuration/fault text. Native ir_c2.cpp valid_utf8 explicitly rejects
lead byte zero; valid_view and string-span validation use it before copying.
The actual contract is length-delimited UTF-8 without embedded NUL. Two Ada
comments are corrected and strict requalification is running. Earlier historical
observations are retained but superseded by this explicit correction. No tokens,
validation rules or production behavior change.

## 004I — Track updates qualified; 004J reconciliation started

Track.Updates 153/153, removing 153 gaps and 149 public warnings, zero additions.
All eight targets now total 1,274/1,274; repository 2,897/4,295 (67.4505%),
1,398 gaps. Warnings measured as 1,161 raw = 1,088 public + 73 private.
Other packages/private identities and native profiles/anchors/renderer exception
unchanged. Format/check, two strict byte-identical generations, HTML links, all
75 documentation tests, six integrity tests, token integrity and text gates
pass. Exact evidence: external `r1/004i-*`.

Updates use the track-update condition, not the basic Track condition. All 21
Track covariance fields are documented individually, distinct from eighteen
Image navigation terms; no packed-matrix transform or estimator is implied.
Pinned times are double epoch seconds, not nanoseconds and without a more
specific epoch. Maneuver probability source range is zero-to-one and update
quality zero-to-fifteen; setters/bridge do not enforce these. ECEF frame is
established but stronger component units are not supplied by the pinned fields.
All identifiers and labels copy before return; labels must be UTF-8 without NUL.
Future success carrying a Rejected command state differs from ErrorOr rejection.
Timeout/observer closure are not cancellation. Pending update/response/common
claims share Track lifetime and final cleanup; failed deferred detach can replace
a result with provider failure and permanently retain unsafe-to-release resources.
Sources: Updates spec/body, native ir_track.cpp build_update/complete/accounting/
cleanup; pinned TrackDataUpdate/TrackStatus/TrackChannel/CommandStatus and
Common Directional; task 029C, native update/system-response mixed lifetime tests
and Ada update tests. Positive behavior is mock-only.

C2 embedded-NUL correction requalification passed with zero identity deltas,
two identical sites, 75+6 tests and all documentation/text/integrity gates
(`r1/004c-nul-correction-*`). During independent 004J review, C2 Close prose was
also narrowed: detach failure retains a retryable handle, but disable failure
after successful detach can consume it while raising. Final qualification will
use this corrected final source. No production tests have yet been credited.

## 004J — final documentation reconciliation

| Package | Starting main | Final | Newly documented |
| --- | ---: | ---: | ---: |
| AMS.MEL.IR.Channel | 1/289 | 289/289 | 288 |
| AMS.MEL.IR.C2 | 1/179 | 179/179 | 178 |
| AMS.MEL.IR.C2.Common | 0/33 | 33/33 | 33 |
| AMS.MEL.IR.C2.Metadata | 0/288 | 288/288 | 288 |
| AMS.MEL.IR.Track | 0/55 | 55/55 | 55 |
| AMS.MEL.IR.Track.Metadata | 0/153 | 153/153 | 153 |
| AMS.MEL.IR.Track.System_Data | 0/124 | 124/124 | 124 |
| AMS.MEL.IR.Track.Updates | 0/153 | 153/153 | 153 |

Repository 2,897/4,295 documented (67.4505%), 1,398 gaps. Independent full
identity comparison removes 1,272 gaps since main and 1,071 since 004A; zero
added gaps or changes to untouched-package gaps. Measured warnings: 1,161 raw,
1,088 public and 73 private-part, zero tool/unclassified/private-unit warnings.
Exactly 1,238 public warning identities removed since main, 1,033 since 004A;
zero added warning identities and all private identities unchanged.

All three 47-literal Cannot_Comply enums have verified explicit 0..46 mapping.
Native inventory remains 1,238 source / 1,237 native sections with identical
observed profiles/anchors and the single unchanged Health metadata omission.
All historical ledger entries are preserved and new referenced test/report paths
exist. Complete change scope is exactly fourteen authorized files: eight Ada
spec comments/formatter whitespace and six documentation artifacts. No body,
production/vendor byte, test, configuration, tooling, workflow, anchor or exception
file changes. Literal-aware integrity passes at both main (1,178 protected files)
and DOC-001 (1,162 protected files).

Final formatted-source documentation qualification passes required format/check,
strict generation twice, every site byte identical, HTML 31 pages / 1,307 links,
75 documentation tests and six dedicated integrity tests, baseline removal-only
gates, whitespace and final newlines. C2 NUL and cleanup wording corrections
are included in this final source. Logs: external `r1/004j-*`; independent
reconciliation: `r1/004j-independent-reconciliation.json` and
`r1/004j-warning-reconciliation.json`.

Sequential final native/Ada/canonical-check qualification is running under the
saved stable compiler environment. No production gate is credited until its
exit file and log have been inspected. The external runner initially used
unsupported POSIX-shell brace expansion and stopped before any build; that
runner-only failure is preserved and corrected with explicit paths. No test
was suppressed or changed. Publication remains gated on all final local gates.

## Final local qualification — complete

Every required local gate passed on the final formatted eight specifications.
Recorded SHA-256 checks after the sequential production gates match all eight
submitted Ada files. Final source includes both C2 semantic corrections.

| Gate | Actual result |
| --- | --- |
| Required format and final format check | PASS |
| Strict qualified GNATdoc/semantic compilation and identity ratchets | PASS |
| Eight target packages | 1,274/1,274 documented |
| Repository coverage | 2,897/4,295; 1,398 gaps |
| Raw/public/private warnings | 1,161 / 1,088 / 73 |
| Gap/warning additions | Zero / zero |
| Native profiles/anchors | 1,238 source / 1,237 native, unchanged |
| Known renderer omission | Exactly one unchanged Health alias |
| Two fresh site generations, every output byte | PASS, identical |
| HTML structure/local links | PASS, 31 pages / 1,307 links |
| Documentation unit/mutation suite | PASS, 75 tests |
| Dedicated integrity suite | PASS, six tests |
| Literal-aware main/DOC-001 integrity | PASS, 1,178 / 1,162 protected files |
| make test-native | PASS, 434/434 |
| make test-ada | PASS, full contracts and focused repeats |
| Sequential canonical make check | PASS, isolation/native/format/Ada/text gates |
| Final make check-ada-format after make check | PASS |
| Whitespace/final newlines | PASS |

Exact production logs and inspected exit files: external `r1/final-test-native`,
`r1/final-test-ada`, `r1/final-check`, `r1/final-check-ada-format`; final source
hashes in `r1/final-tested-ada.sha256` and verification log. Native commands never
overlapped. No inherited rf-job-interval-status instability occurred in these
final sequences, and no tests/contracts were weakened. No real-provider or
hardware execution is claimed. Main refetched unchanged at the original base.

Exactly fourteen authorized files changed: eight specifications' comments/
formatter whitespace and six documentation artifacts. Complete production token/
byte inventory, prior ledger records, historical DOC-001–003 reports, native
anchor/renderer manifests, tooling and workflows remain unchanged.

Local qualification is complete. Commit/publication and hosted exact-head CI
are separate subsequent observations, not inferred here. Independent review
remains required; no merge, release, tag or feature-branch Pages deployment.
