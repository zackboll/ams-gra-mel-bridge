# DOC-002 coverage checkpoints

This report is separate from the immutable DOC-001 initial inventory.
All five target packages are now documented; final native qualification and
PR handoff are recorded in the final section below. Earlier sections retain
their historical checkpoint status.

## Reproduced pristine baseline

Source: `85b5437e39de1387f3f1678d6f09a9f85e16cfac`, unchanged merge tree
of the reviewed DOC-001 head. Qualified generation and all 75 documentation
unit/mutation tests passed before Ada source modification.

| Package | Elements | Initially documented | Root checkpoint documented |
| --- | ---: | ---: | ---: |
| AMS | 1 | 0 | 1 |
| AMS.MEL | 45 | 3 | 45 |
| AMS.MEL.Status | 531 | 0 | 0 |
| AMS.MEL.IR | 106 | 0 | 0 |
| AMS.MEL.RF | 217 | 1 | 1 |
| **Target total** | **900** | **4** | **47** |

## Root-foundation documentation checkpoint

| Measure | Pristine | Root checkpoint |
| --- | ---: | ---: |
| Repository documented elements | 38 / 4,295 | 81 / 4,295 |
| Remaining gaps | 4,257 | 4,214 |
| Raw warnings | 3,924 | 3,887 |
| Public warnings | 3,851 | 3,814 |
| Private-part warnings | 73 | 73 |
| Source declarations | 1,238 | 1,238 |
| Native sections | 1,237 | 1,237 |
| Exact renderer omissions | 1 | 1 |

The independent identity comparison proved 43 fixed gaps and 37 removed
warnings, all in the two root packages. No new warning/gap identities appeared.
All other packages' coverage and every private warning identity are unchanged.
Only proven fixed identities were removed from the structured baselines;
neither capture-pristine nor raw-log baseline replacement was used.

The strict generation/ratchets passed twice after the update. The sites were
byte-identical. All 75 documentation tests passed again. Native section/anchor
validation and local HTML links passed through the unchanged pipeline.
Formatting and format-check passed. Literal-aware integrity against verified
main passed for 1,178 protected files. Native/Ada execution is pending at this
checkpoint; no full-campaign qualification is claimed.

## Status BIT/fault subcheckpoint

Status now has **231/531** documented elements (300 remaining). This is a
coherent partial section, not the completed Status package. Target-package
coverage is 278/900, with 622 target gaps still open.

Repository coverage is **312/4,295**, with **3,983 gaps**. Warnings are
**3,655 raw = 3,582 public + 73 private-part**. The independently verified
delta from the root checkpoint is 231 fixed gaps and 232 removed warnings,
all in Status. No other-package gaps, private identities or anchors changed.
Formatting/check, two strict deterministic generations and all 75 documentation
tests passed. The earlier launched native/Ada run also passed (434 native tests
and safe Ada contracts), but is not final exact-source Status qualification.

## Status state and BIT-status subcheckpoint

The overall MFA, transition, component and temperature enum descriptions and
the BIT-status snapshot family bring Status to **296/531** (235 gaps).
Target coverage is **343/900**, with **557 target gaps** remaining.
Repository coverage is **377/4,295 (8.7776%)**, with **3,918 gaps**.
Warnings are **3,590 raw = 3,517 public + 73 private-part**.

This subcheckpoint independently removed 65 gaps and 65 public warnings,
without changing any other-package coverage, private warning identities or
native anchors. Strict generation twice, deterministic comparison, all 75
documentation tests, format/check, baseline delta and source integrity pass.
Fresh sequential native/Ada/full-check validation has been launched on these
exact Ada comments; its results must be inspected before crediting those gates.

## Completed Status documentation checkpoint

All **531/531 Status elements** now pass source attachment and native rendering.
AMS remains 1/1 and AMS.MEL 45/45. IR remains 0/106 and RF 1/217, so the
five-package total is **578/900**, with **322 target gaps** remaining.

Repository coverage: **612/4,295 (14.2491%)**, **3,683 remaining gaps**.
Warnings: **3,356 raw = 3,283 public + 73 private-part**. This final Status
increment independently removes 235 gaps and 234 public warnings. Across the
campaign so far, 574 gaps and 568 warnings have been removed. All nontarget
package coverage and all private warning identities remain unchanged.

Required formatting/check, two fresh byte-identical strict generations, 75
documentation unit/mutation tests, exact native anchors/sections and local
links, baseline delta, source integrity and Git whitespace pass. Sequential
native (434/434), Ada and full make check completed successfully during Status
authoring; a fresh Ada run on the final formatted Status comments is pending.
IR/RF research and documentation, final all-package reconciliation and PR
handoff remain incomplete. No completion of DOC-002 is claimed.

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

## RF completion and full documentation reconciliation

Resumption preserved the uncommitted campaign and reproduced 718/4,295
coverage and 3,252 warnings before RF edits. The prior IR Ada log and exit
file were inspected: exit zero, complete safe Ada contract pass. No overlapping
task-owned build/test process remained. Fetched main was unchanged at
`85b5437e39de1387f3f1678d6f09a9f85e16cfac`.

| Target package | Initial documented | Final documented | Final gaps |
| --- | ---: | ---: | ---: |
| AMS | 0/1 | 1/1 | 0 |
| AMS.MEL | 3/45 | 45/45 | 0 |
| AMS.MEL.Status | 0/531 | 531/531 | 0 |
| AMS.MEL.IR | 0/106 | 106/106 | 0 |
| AMS.MEL.RF | 1/217 | 217/217 | 0 |
| **Total** | **4/900** | **900/900** | **0** |

| Repository measure | DOC-001 baseline | Final documentation result |
| --- | ---: | ---: |
| Documented elements | 38/4,295 | 934/4,295 |
| Documented percentage | 0.8847% | 21.7462% |
| Remaining gaps | 4,257 | 3,361 |
| Raw warnings | 3,924 | 3,036 |
| Public warnings | 3,851 | 2,963 |
| Private-part warnings | 73 | 73 |
| Private-unit/tool/unclassified warnings | 0 | 0 |
| Public packages | 29 | 29 |
| Public declarations / native sections | 1,238 / 1,237 | 1,238 / 1,237 |
| Exact renderer omissions | 1 | 1 |

RF independently removed 216 gaps and 216 public warnings. Campaign totals
are 896 fixed gaps and 888 removed warnings. Source identities, all other
packages' gaps, every private warning identity, native sections/anchors and the
exact Health_Status.Metadata package-renaming exception are unchanged.
Structured baselines remove only independently proven fixed identities; no
capture-pristine overwrite or warning/coverage tool change was used.

All final source documentation gates passed: required format/check, strict
qualified generation twice, recursive byte comparison, all 75 documentation
unit/mutation tests, native anchor/section validation and 31 HTML pages with
1,307 local links. Literal-aware source integrity against original main checks
1,178 protected files; all five Ada token streams and production/vendor bytes
are unchanged. Historical DOC-001 inventory is unmodified. Final sequential
native/Ada/full-check results will be recorded only after command completion.

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
