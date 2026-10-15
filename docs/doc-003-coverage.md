# DOC-003 coverage checkpoints

Base: `e7a652c7a62e66d58c275f6ab1f09bb6e2021622`.

## Pristine reproduced baseline

| Package | Documented | Elements |
| --- | ---: | ---: |
| AMS.MEL.IR.Image | 1 | 351 |
| AMS.MEL.IR.Image.Metadata | 1 | 77 |
| AMS.MEL.IR.Health_Status | 0 | 25 |
| AMS.MEL.IR.Health_Status.Metadata | 0 | 143 |
| AMS.MEL.IR.Instrumentation | 0 | 76 |
| AMS.MEL.IR.Instrumentation.Metadata | 0 | 22 |
| Target total | 2 | 694 |

Repository: 934/4,295 documented; 3,361 gaps.
Warnings: 3,036 raw = 2,963 public + 73 private-part; zero unmapped.
Inventory: 29 public pages, 1,238 source declarations, 1,237 native sections.
HTML validation: 31 pages, 1,307 links. Documentation tests: 75 passed.

The exact Health metadata Status package renaming remains source-visible and
undocumented, with no native section. Its exception file is unchanged.

Campaign implementation and final qualification remain pending.
## 003A — Full-frame model and owned path

Image now measures 160/351; 159 newly documented elements. Repository coverage
is 1,093/4,295 (25.4482%), with 3,202 remaining gaps. Warnings are 2,906 raw
(2,833 public, 73 private-part): 130 verified public warning identities removed,
zero added. All other packages are unchanged. Native sections/anchors and the
renderer exception are identical. Only measured fixed gap/warning identities
were removed from the committed baselines.

Required format/check, literal-aware integrity against the exact task base,
strict generation twice, byte-identical site comparison, 75 documentation tests,
HTML/link validation and baseline-delta checks passed. Native/Ada execution and
later Image lease/navigation sections remain pending; this is not final campaign
qualification. Raw logs, measured identities and first site are stored externally.

## 003B through 003G — qualified documentation checkpoints

| Checkpoint | Package checkpoint | Repository documented | Raw warnings | Public | Private-part |
| --- | ---: | ---: | ---: | ---: | ---: |
| 003B lease | Image 260/351 | 1,193/4,295 | 2,806 | 2,733 | 73 |
| 003C navigation | Image 351/351 | 1,284/4,295 | 2,719 | 2,646 | 73 |
| 003D image metadata | Image.Metadata 77/77 | 1,360/4,295 | 2,649 | 2,576 | 73 |
| 003E health channel | Health_Status 25/25 | 1,385/4,295 | 2,629 | 2,556 | 73 |
| 003F health metadata | Health_Status.Metadata 142/143 | 1,527/4,295 | 2,487 | 2,414 | 73 |
| 003G instrumentation | Instrumentation 76/76; Metadata 22/22 | 1,625/4,295 | 2,399 | 2,326 | 73 |

Each checkpoint ran required formatting and format-check, strict qualified
GNATdoc generation twice, recursive published-output byte comparison,
HTML structure/local links and all 75 documentation unit/mutation tests.
The first ratchet runs intentionally failed on stale fixed identities; the
logs are preserved. Only independently measured fixed warning/gap identities
were removed afterward; no new identities were accepted. A temporary authored
comment replacement removed the already-documented full-frame Pixels block
while replacing old lease commentary; the next independent measurement caught
three new gaps/warnings, the block was restored, and the strict qualified
checkpoint accepted zero new identities. Prose underscore normalization also
exposed several uncredited summaries; these were fixed in comments, not tooling.

## 003H — final documentation reconciliation

| Package | Before | After | Newly documented |
| --- | ---: | ---: | ---: |
| AMS.MEL.IR.Image | 1/351 | 351/351 | 350 |
| AMS.MEL.IR.Image.Metadata | 1/77 | 77/77 | 76 |
| AMS.MEL.IR.Health_Status | 0/25 | 25/25 | 25 |
| AMS.MEL.IR.Health_Status.Metadata | 0/143 | 142/143 | 142 |
| AMS.MEL.IR.Instrumentation | 0/76 | 76/76 | 76 |
| AMS.MEL.IR.Instrumentation.Metadata | 0/22 | 22/22 | 22 |
| **Target total** | **2/694** | **693/694** | **691** |

The sole remaining target gap is the unchanged source-visible package renaming
`AMS.MEL.IR.Health_Status.Metadata.Status`. GNATdoc 26.0.0 emits no native
section; no fake anchor or exclusion was introduced. Canonical Status reference
prose does not count that declaration as rendered or documented.

| Repository measure | Before | After |
| --- | ---: | ---: |
| Documented | 934/4,295 | 1,625/4,295 |
| Percentage | 21.7462% | 37.8347% |
| Remaining gaps | 3,361 | 2,670 |
| Raw warnings | 3,036 | 2,399 |
| Public warnings | 2,963 | 2,326 |
| Private-part warnings | 73 | 73 |
| Unmapped/private-unit/tool warnings | 0 | 0 |
| Source declarations / native sections | 1,238 / 1,237 | 1,238 / 1,237 |
| Public package pages | 29 | 29 |
| Renderer exceptions | 1 | 1 |

Stable-identity reconciliation proves **691 removed gaps**, **637 removed public
warnings**, **zero new gaps** and **zero new warnings**. Coverage and warning
identities for the other 23 packages are unchanged; every private warning
identity is unchanged, not merely the private total. Native signatures, complete
overload identities, sections and anchor manifest are identical. Committed
baselines remove only measured fixed identities, never a newly authorized gap.
The complete before/after identity sets, raw streams and per-checkpoint deltas
are retained in the external evidence directory.

### Source research and integrity

Exact base/source identity: `e7a652c7a62e66d58c275f6ab1f09bb6e2021622`.
IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`, Common MEL
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, AMS Math
`00be45190f0e47d268cece8b8c2f8fb58b5418d2` and Squall
`b1015728f904c799fa0c07489fce48e78f67845f` are the source authorities.
Original Git remotes and objects were checked; 101 vendored headers (68 IR,
28 Common, five Math) match exact pinned upstream bytes. All 44 new grouped
ledger citation instances (29 distinct source files) resolve to exact pinned
files. No unverified main, inaccessible IDD or implied real-provider execution
was used.

Research discrepancies include unspecified magnetic-heading and rate/covariance
units, timestamp epochs, validity bit positions, criticality/failure thresholds,
version source encoding and instrumentation Size units. Owned Receive cleanup
ignores native snapshot-close status, unlike explicit lease Close. Historical
030A snapshot ownership and original 027B teardown synchronization are superseded
by pinned-buffer retention and the corrective synchronization protocol. The
separate research report and structured ledger record the full distinctions.

Literal-aware integrity passes against the task base (1,178 protected files)
and the unchanged DOC-001 boundary (1,162). All six executable Ada token streams,
private representations, native bytes, vendor files, C ABI, tests, build projects,
workflow, tooling, anchors and exception file remain unchanged. Only the six
specifications' comments/formatter whitespace and authorized documentation files
changed. The six-test tracked-inventory/mutation integrity suite also passes.

### Final documentation validation

After final formatting: two fresh strict qualified generations pass and every
published output byte is identical. HTML validation passes 31 pages / 1,307 local
links. All 75 documentation tests and the six dedicated integrity tests pass.
Final baseline-delta, source integrity, Git whitespace and newline checks pass.
Full native/Ada/canonical-check qualification is running sequentially and is not
claimed complete until the final section records actual exit statuses.

### Hosted starting-main qualification follow-up

Bootstrap run 38673364892 completed with failure on exact starting main.
The clang Release repeated suite failed rf-job-interval-status at
`native/tests/test_rf_job_interval_status.c:381`, assertion
`shutdowns()==before+1`. Other Bootstrap jobs passed. Documentation run
38673364878 passed; its checkout log confirms the exact starting-main SHA.
This inherited stress failure is retained as evidence, not suppressed or
reclassified as success. Feature-branch CI is evaluated separately.

Warning removals by source file reconcile exactly: Image 317, Image Metadata
70, Health 20, Health Metadata 142, Instrumentation 67, Instrumentation Metadata
21 (637 total). Gap removals by package are the newly documented column above.

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
