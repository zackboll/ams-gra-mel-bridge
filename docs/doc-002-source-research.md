# DOC-002 source research

Status: all five packages pass documentation coverage and rendering. The
final sections supersede earlier in-progress checkpoint descriptions. Native
qualification and PR state are recorded separately, never inferred.

## Starting identity and evidence

The isolated branch starts at verified main
`85b5437e39de1387f3f1678d6f09a9f85e16cfac`. GitHub confirmed PR #101
merged with reviewed head `3b9dffa542d1133d2d6d40be8b152354607baac1`;
the merge tree and reviewed head tree are identical. Existing worktrees,
including prunable registrations, were left untouched. No conflicting DOC-002
branch or open PR existed. Both main documentation run `38665457826` and
bootstrap run `38665457895` are now completed successfully on the exact main
SHA. Bootstrap was initially running and was not credited until completed.

Disk-backed local evidence is retained outside this repository in the task's
designated evidence directory. It contains pristine raw diagnostics, source
inventory, coverage, warning classification, the original baselines and site,
and the temporary attachment qualification project.

## Toolchain and attachment qualification

The established script passed unchanged with Alire 2.1.1, GNAT 16.1.0,
GPRbuild reporting 26.0.0 (2026-04-15), and GNATdoc 26.0.0 (20260415).
No installation or toolchain substitution was performed.

A separate specification-only fixture was compiled and generated with public
scope, HTML backend and trailing style. Its raw GNATdoc diagnostic stream was
empty. Inspection of the native declaration sections verified:

- Package prose immediately after the package opening, followed by a blank
  line, renders as the package description.
- Ordinary declaration prose follows the terminating semicolon, including
  procedures/functions with Pre/Post aspects and multiline profiles.
- Each overload's own block renders its own summary, parameters and return.
- Enumeration and record descriptions render after a separate representation
  clause. A follow-up fixture also verified rendering immediately after the
  type semicolon and before that clause. The repository's source-attachment
  validator requires the latter placement for these enum declarations.
- Discriminants use field annotations and render alongside record fields.
- An exception declaration accepts its own summary; an exception annotation on
  a subprogram renders under Raised exceptions.

The source-derived attachment checker independently requires the block after
the indexed declaration end. Package descriptions must satisfy its explicit
blank-line contract. Later production generation must verify both source
attachment and rendering; a successful fixture alone does not qualify comments.

One existing validator behavior was encountered: source prose normalization
removes underscores, whereas native prose can retain them in identifiers.
The root Open summary was rewritten without an underscored identifier rather
than changing or weakening the validator.

## Root foundations

The structured ledger groups source evidence by semantics, not by repeated
getter boilerplate. It records exact upstream revisions and original paths.

### ABI versus provider versions

The native ABI query only writes compile-time bridge constants. It does not
load a provider. The Ada wrapper checks status and Natural representability.
Common MEL VersionInfo separately stores unsigned 32-bit API/library numbers
and two strings. The bridge preserves these numbers without interpreting their
encoding. The upstream Common MEL API string `4.0` is a third, distinct value.

Version-query text is validated by the bridge as UTF-8 without embedded NUL.
Ada uses a size query followed by a copy query and returns owned strings.
No immutable provider snapshot across the two calls is promised. A provider
that changes values or sizes between calls can cause a failed copy query.

### Open, Close and provider lifetime

Ada Open delegates to the options overload with a zero limit. All three
strings are checked for embedded NUL before allocation/loading, then copied
without transcoding. Unlike version-query text, this open path does not
validate UTF-8. UTF-8 is the documented input convention, not a validated
property of these arguments.

Native opening resolves getAPI_Manager/getControl, requires non-null results,
and accepts only Return::Success from Control::init. Provider C++ ABI
compatibility remains required. The published Control interface has no
shutdown method. Public Session closure consumes its handle and releases one
shared SessionState reference. Children and requests can outlive that handle.
The SessionState member order destroys Control, manager and library in safe
order once its last reference is released; emergency retention paths can keep
provider code alive indefinitely. Close is not cancellation or a global wait.

Ada finalization directly attempts native closure, suppresses Ada exceptions,
and clears the local handle in its fallback. Prefer explicit Close for
observable operation errors. Is_Open only tests local handle presence.

### Admission

The configured bound is copied at opening and applies to participating
Session-backed asynchronous submissions. Native acquisition checks capacity
before provider send. Refusal maps to Resource_Exhausted, not command rejection.
Permits belong to worker inputs and survive public request closure. Waiting or
timing out does not cancel a future and is not itself permit release. No
queue/retry behavior is supplied by these wrappers.

## Status BIT and fault subcheckpoint

BIT and fault descriptions now cover 231 of Status's 531 elements. The exact
delta from the root checkpoint is 231 fixed gaps and 232 removed public
warnings; zero new warning/gap identities. Coverage is 312/4,295 repository-wide
with 3,983 remaining gaps. Raw warnings are 3,655 = 3,582 public + 73 private.
All other packages, private warning identities and native anchors are unchanged.

The native generator accepts comments before or after a representation clause,
but source indexing ends these enum declarations at their type semicolons.
Initial after-clause comments rendered but correctly failed source attachment;
they were moved before representation clauses and the mismatch disappeared.
No tool changes were made. Required formatting/check and two fresh strict
generations passed, including byte-identical site comparison and all 75 tests.

Pinned BIT_ControlInterface values two and three have inconsistent names and
adjacent descriptions. The bridge performs numeric-preserving checked mapping;
tests establish values one and three, not a resolution of the prose conflict.
The public enum docs explicitly preserve this unresolved distinction.

ActiveBIT's double progress uses 1.0 for 100 percent, explicitly permits values
above 1.0, and the mock/Ada tests preserve 1.25. Completion, completed-test end
and fault detection counts are signed nanoseconds. Their specific upstream
fields say UTC; none establishes an epoch, so no Unix epoch is inferred.

Ada constructors copy without semantic consistency checks. In particular,
Create_BIT_Type's Names argument supplies one name if nonempty, not a delimited
list. The append-name operation also accepts an empty string. Fault severity
and state are independent; isolated component references differ from unresolved
ambiguity groups, and diagnostic tests are not paired by index with candidates.
These distinctions are mapped in the structured evidence ledger.

Native qualification launched at the root checkpoint has finished successfully:
434/434 native tests and the full safe Ada smoke/contract executable passed.
Because Status comments were edited during that run, those logs are not claimed
as final exact-source qualification of the Status subcheckpoint. Final native
and Ada runs remain required after documentation edits cease.

## State and BIT-status subcheckpoint

Overall MFA, component, thermal and transition enums plus the BIT-status
collection family now raise Status coverage to 296/531. The incremental change
is 65 fixed gaps and 65 removed public warnings. Pinned MFA_State introductory
prose claims transmit-only is excluded, but its actual enum contains that
identifier. The authored description preserves the declared identifier and
does not infer provider support. State storage is not transition execution;
temperature state is not computed from the reading by Ada.

Source base is unchanged. Two strict generations, byte comparison, all 75
documentation tests, formatting/check, baseline delta and source-integrity
checks passed. Current totals: 377/4,295 documented (8.7776%), 3,918 gaps;
3,590 raw warnings = 3,517 public + 73 private-part. No private identity,
other-package coverage or native anchor changes. Fresh sequential native,
Ada and full make-check validation is pending inspection of completed logs.

## Remaining research

Remaining Status families, IR and RF research/documentation are not complete.
No official IDD has been claimed reviewed. Units, enum ambiguities and provider
support must be established from the pinned headers and bridge paths before
authoring the remaining descriptions.

## Authored remaining Status families (render qualification pending)

Component, installation, About, overall status, name/value and security prose
has now been authored, but is not yet credited as coverage. Native formatting
is queued after the already-running shared-tree make-check command to avoid
concurrent reconfiguration. Do not mistake this source-writing milestone for
completed Status qualification.

Pinned Directional.h describes Euler yaw/pitch/roll sequence in the host frame
centered on the navigational center, with radians. ComponentLocation uses
forward/right/down meters. TemperatureStatus is double degrees Celsius.
Foreign keys identify another naming system rather than resolving a link.
Detailed status metrics are provider-documented textual name/value pairs;
they are not a bridge-standardized metric dictionary or a keyed replacement map.

Each concrete security subtype was examined. Authentication categories 0..7
carry subsystem/service/MDF references; integrity categories 0..6 and file
management categories 0..4 carry MDF references. Key management (0..6), system
(0..4) and sanitization (0..3) expose no such subtype IDs. Native variant copying
validates those category domains and only copies applicable IDs. Ada's local
constructor instead retains every supplied field without kind/category checks.
Audit timestamps use signed nanoseconds, but the pinned declaration specifies
neither UTC nor epoch, unlike the explicit UTC wording in BIT/fault fields.
The generic base-class account-access wording was not propagated to unrelated
security event families. The ledger records these separate source mappings.

## Final Status rendering result

All 531 Status elements now render and satisfy source attachment. The final
package-opening summary was rephrased to avoid the existing underscore
normalization mismatch, without changing validator logic. Two fresh strict
sites are byte-identical and all 75 documentation tests pass. Repository totals
are 612/4,295 documented (14.2491%), 3,683 gaps and 3,356 raw warnings
(3,283 public, 73 unchanged private-part). IR and RF roots remain pending.

Sequential native tests, Ada tests, make check and final format-check all
completed successfully during Status authoring. A new Ada run on final formatted
Status comments is pending; earlier runs are not substituted for final campaign
qualification. Complete logs and delta identities remain outside Git.

## DOC-002 IR foundation checkpoint

IR foundation is now 106/106, with zero public gaps. AMS 1/1, AMS.MEL 45/45
and Status 531/531 remain complete. RF remains 1/217 and is not yet authored
or qualified. Target coverage is 684/900, with 216 target gaps remaining.
Repository coverage is 718/4,295 (16.7171%), with 3,577 gaps. Raw warnings are
3,252 = 3,179 public + 73 private-part. Independent delta from the completed
Status checkpoint: 106 fixed gaps and 104 removed public warnings. No new
warning/gap identities, other-package changes, private warning changes, anchor
changes or renderer exceptions were accepted.

Pinned IR research confirms binary sixteen-byte UUIDs and equality comparing
both UUID and label, host-relative forward/right/down meter offsets, copied
Mono8 single-band legacy frames and selected scalar metadata rather than the
full Image child API. Image time means integration start for stare and first
line for scanning/TDI; FOV is radians and positive dither subtracts from prior
row/column location. Counters count callbacks before validation, separately
tracking malformed/unsupported data and queue-full drops. Receive's two-call
protocol requires one consumer. Weak common views do not retain typed resources.
Stop/Close do not imply universal cancellation; corrective buffer-lifetime
evidence was reviewed. Native reserved image kind two is not accepted by the
legacy Ada wrapper, which only exposes stare/scan.

Required formatting/check, two fresh strict byte-identical generations, all
75 documentation tests, native sections/anchors and links, baseline delta,
source integrity and whitespace pass. The final-Status Ada run completed with
exit zero; a fresh IR Ada run is in progress, with external ir-test-ada logs.
The earlier sequential native/Ada/make-check/format series passed, but was
performed before the final IR comments and is not final campaign qualification.
RF research has only begun. No commit, push or PR has occurred; DOC-002 remains
incomplete. Resume from this measured checkpoint, inspect the Ada exit first,
and do not overlap any native build/format command with the pending run.

## RF foundation source review and completion

Original main and fetched main agree at the original campaign base. The
previous IR Ada run is now verified complete, exit zero. Pre-edit RF resumption
reproduced the full 718/4,295 and 3,252-warning checkpoint without changing tools.

Verified pinned source objects and byte-identical vendored includes: RF MEL
`762ce84c5555dd0f3ea66f36b321fecf8839b89f` (57 headers), Common MEL
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439` (28), AMS Math
`00be45190f0e47d268cece8b8c2f8fb58b5418d2` (5). Squall reference remains
`b1015728f904c799fa0c07489fce48e78f67845f`. These are sources, not a claim
that real-provider execution or an inaccessible official IDD was reviewed.

RF evidence groups cover the complete root declaration families in the JSON
ledger. Reviewed authoritative headers include RFMFAInfo, PhysicalData,
TxPowerModeData, RFMELTypes, JobDataFormat, RF factory/DataMEL, Common Euler,
ComponentLocation/ForeignKey/VersionInfo and Math Femtoseconds. Ada RF body,
native rf_data.cpp, native/Ada RF contracts and tasks 033B, 034C1-C3 and
034F18-F25 establish the supported consumer behavior.

### Lifecycle and timing

Open calls the RF factory, not IR initialization. Configuration syntax remains
provider-defined. Ada rejects embedded NUL before loading; neither RF open path
validates UTF-8, although UTF-8 remains the documented convention. Provider
version text does receive native UTF-8/NUL validation and independent Ada copy.
Close consumes public ownership, defers shutdown while child claims survive,
and retains an unsafe throwing-shutdown graph. Callback code retention can
outlive public closure. These are not cancellation promises.

Quantization passes a signed 64-bit femtosecond count to the provider unchanged.
The dedicated mock truncates signed division by ten; pinned Squall returns its
input unchanged. Neither behavior is a universal rule. Pinned scheduler prose
mentions truncate/closest but does not settle rounding. All face timing FS
accessors preserve signed femtoseconds; pulse width alone is nanoseconds.

### Formats and installation

ComplexINT16 actually uses signed int16 real/imaginary pairs despite the
header's signed-eight-bit prose. The fourteen numeric format identifiers do
not all have data-pointer alternatives: PDW/LF remain unsupported by current
ProductRx. VITA capacity is 512/1,024/8,192/32,768 bytes, not serialized packet
length or all signal payload. Provider format reporting is not an Ada binding
completeness guarantee.

PhysicalData defines height along the short antenna axis, width along the long
axis, both meters. Lattice is relative element-grid orientation in radians.
Common installation offsets are host forward/right/down meters from navigation
center; Euler orientation and boresight are radians, with upstream yaw/pitch/roll
sequence. Location text refers to external logical mounting keys, not a resolved
coordinate system. Snapshots own all copied strings and scalars after closure.

### Capabilities and transmit constraints

Reported face count differs from the copied set of actual IDs. Face and format
sets are copied in iteration order; vector ranges/modes retain provider order
and duplicates. Positive indexed access starts at one and raises Constraint_Error
beyond length. Reversed/nonfinite ranges and signed timings are preserved rather
than silently normalized. Support flags prove neither active RF nor hardware
qualification. Endpoint association is a requirement report, not an action.
Open-additions is a provider flag whose further generic inventory is unspecified.
User-context maximum is a byte constraint, not allocated/registered storage.

Mode IDs are assigned by providers and differ from power levels. Direct queries
preserve returned IDs even if they differ from requested IDs; no generic not-found
rule or implicit supports-transmit gate is invented. Upstream warns of hardware
damage outside mode characteristics, explicitly valid frequency ranges. Copies
report rather than enforce future-job hardware limits. Duty is dimensionless,
pulse nanoseconds and ranges hertz. Attenuation units are unspecified and are
not labeled dB; maximum attenuation can change with calibration.

All 217 RF elements render on their declarations. Final source-derived coverage
is 934/4,295 (900/900 targets); warning total 3,036 = 2,963 public + 73 unchanged
private-part. No new warnings/gaps or renderer omissions were accepted. Two
fresh strict sites are byte-identical and all 75 documentation tests pass.

## Final local qualification — complete

All commands below completed with exit zero on the final formatted five-package
Ada sources; no native commands overlapped. Logs and raw diagnostic/delta reports
are retained in the existing external disk evidence directory.

| Gate | Actual result |
| --- | --- |
| Qualified GNATdoc generation and strict ratchets | PASS, two fresh runs |
| Byte-identical regeneration | PASS, recursive comparison of all site files |
| Native declarations/anchors and exact omission | PASS, 1,238 source / 1,237 native / one unchanged alias |
| HTML structure/local links | PASS, 31 pages / 1,307 links |
| Documentation unit/mutation suite | PASS, 75 tests |
| Dedicated source-integrity regression suite | PASS, 6 tests including tracked-addition rejections |
| Required make format-ada / make check-ada-format | PASS |
| make test-native | PASS, 434/434 |
| make test-ada | PASS, safe Ada smoke/contracts and focused repeats |
| Sequential make check | PASS, native suite, build-tree isolation, formatting, Ada contracts, newlines/whitespace |
| Final make check-ada-format after make check | PASS |
| Literal-aware integrity, original DOC-002 main | PASS, 1,178 protected files |
| Default DOC-001 integrity boundary | PASS, 1,162 protected files |
| Git whitespace/final newlines | PASS |

All production/vendor bytes and Ada tokens are unchanged. Only the five target
specifications' documentation/comments (and formatter whitespace) plus the six
authorized documentation reports/baselines changed. DOC-001 tooling, workflow,
anchors, renderer exception and original inventory remain unchanged. No inherited
native failure occurred in this final sequence; no tests or assertions were
weakened. Real-provider/hardware execution was not part of this documentation
qualification. The remaining semantic ambiguities are explicitly documented,
not blockers concealed by invented contracts.

The original main ancestry is verified; main remained at the original base at
resumption. Local qualification is complete. Publication/hosted workflow status
must be checked against the eventual submitted head and is not inferred here.
