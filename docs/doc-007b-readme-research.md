# DOC-007B README source research and retention map

## Baseline and method

Starting fetched main: `291e97c4b1495b594b462982822fa48a2f439527`.
No intervening commits or existing DOC-007B branch/worktree/PR were found.
Branch: `docs/007b-readme-consolidation`. The complete starting README was read,
including all task insertions, quoted current-status text, diagrams and glossary.
It has **1,921 newline-terminated lines / 89,572 Unicode characters** (the task's
approximately 1,922-line estimate is not an exact newline count).
There are two true H1s; the apparent `# AMS MEL` at line 1121 is inside a fenced
text diagram, not a third heading.

The six published guides, current public Ada specifications, native C ABI and
matching implementation/tests take precedence over historical prose. Inspected
CHANGELOG, CONTRIBUTING, coverage, architecture, ABI policy, packaging, upstream
provenance/checksum ledger, GNATdoc foundation/campaign reports and DOC-007A
research/qualification; all six guide sources; Ada/native/Python/Squall READMEs;
Makefile, Alire and Cargo manifests; safe Rust and public Python implementations.
External `research-inputs.json` records hashes and `public-api-survey.txt` records
public declarations and lifetime/scope annotations. No APIs or task reports move.

The native and Ada READMEs, packaging document and older coverage tables include
superseded snapshots (for example native “load/init/version/close only”, Ada
“all other MEL families” deferred, and “No Rust package has been created”). They
are useful build/history inputs, not authorities for current capability claims.
They remain unchanged under this task's strict scope.

## Section-by-section disposition

Locations below refer to the starting README. A task entry's linked report is
its primary retained historical destination; CHANGELOG, docs/coverage.md and
architecture.md also retain the chronological summaries. Earlier exclusions in
those reports apply to their own checkpoint. No removed task account needed a
second archive copy. The only new archive is the unique background glossary and
reading list; unique conceptual rationale is accurately condensed in Project
context without reproducing the published architecture guide.

| Existing README section | Current location | Current factual accuracy | Preserve / condense / relocate / remove duplicate | Authoritative replacement source | Action taken |
|---|---|---|---|---|---|
| Misplaced project title / Developer documentation | README 1–10 | Task title is not project identity; guide link points at an unresolved repository template | Replace title; condense | Published portal; [foundation](gnatdoc-foundation.md) | One H1 and published guide URLs |
| Task 034F25 — ProductRx stabilization-point receive metadata | README 11–30 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f25-rf-product-rx-stabilization-points.md](task-034f25-rf-product-rx-stabilization-points.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F24 — ProductRx VITA receive-event associations | README 31–50 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f24-rf-product-rx-vita-associations.md](task-034f24-rf-product-rx-vita-associations.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F23 — AMS VITA ExtraLarge ProductRx receive | README 51–68 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f23-rf-product-rx-vita-extra-large.md](task-034f23-rf-product-rx-vita-extra-large.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F22 — AMS VITA Large ProductRx receive | README 69–83 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f22-rf-product-rx-vita-large.md](task-034f22-rf-product-rx-vita-large.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F21 — AMS VITA Medium ProductRx receive | README 84–98 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f21-rf-product-rx-vita-medium.md](task-034f21-rf-product-rx-vita-medium.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F20 — AMS VITA Small ProductRx receive | README 99–113 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f20-rf-product-rx-vita-small.md](task-034f20-rf-product-rx-vita-small.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F19 — DirectINT8/DirectINT16 ProductRx receive | README 114–130 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f19-rf-product-rx-direct-samples.md](task-034f19-rf-product-rx-direct-samples.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F18 — RF ProductRx ComplexINT8 receive | README 131–141 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f18-rf-product-rx-complex-i8.md](task-034f18-rf-product-rx-complex-i8.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F17 — copied RF JobInterval user context | README 142–160 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f17-rf-interval-user-context.md](task-034f17-rf-interval-user-context.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F16 — copied RF WaveformStream descriptors | README 161–184 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f16-rf-waveform-stream-values.md](task-034f16-rf-waveform-stream-values.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F15 — copy-owned RF WaveformBuffer samples | README 185–211 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f15-rf-waveform-buffer-samples.md](task-034f15-rf-waveform-buffer-samples.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F14 — copied RF CustomWaveform profiles and patterns | README 212–231 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f14-rf-custom-waveform-values.md](task-034f14-rf-custom-waveform-values.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F13 — copied RF OOK and CPFSK modulation values | README 232–250 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f13-rf-binary-modulations.md](task-034f13-rf-binary-modulations.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F12 — RF value-only modulations and TX references | README 251–267 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f12-rf-value-modulations.md](task-034f12-rf-value-modulations.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F11 — RF TX JobEvent common controls | README 268–283 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f11-rf-tx-event-controls.md](task-034f11-rf-tx-event-controls.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F10 — RF JobInterval transmit-event construction | README 284–300 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f10-rf-tx-events.md](task-034f10-rf-tx-events.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F8 — RX JobInterval ProductStreamParams | README 301–311 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f8-rf-product-stream-params.md](task-034f8-rf-product-stream-params.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F7 — RX JobInterval Local Function command construction | README 312–322 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f7-rf-interval-lf-commands.md](task-034f7-rf-interval-lf-commands.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F6 — RF RX pulse detection settings | README 323–340 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f6-rf-rx-pulse-detection.md](task-034f6-rf-rx-pulse-detection.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F5 — RF RX JobEvent controls | README 341–361 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f5-rf-rx-event-controls.md](task-034f5-rf-rx-event-controls.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| RX JobInterval spatial controls | README 362–373 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f4-rf-interval-spatial-controls.md](task-034f4-rf-interval-spatial-controls.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F3 — ordered mixed RX/TX JobRequest v4 | README 376–390 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f3-rf-job-tx-groups.md](task-034f3-rf-job-tx-groups.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F2 — RF PointingType and RX JobRequest v3 | README 391–406 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f2-rf-job-pointing.md](task-034f2-rf-job-pointing.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034F1 — RX JobRequest v2 and safe Ada | README 407–421 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034f1-rf-job-request-v2.md](task-034f1-rf-job-request-v2.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E6 — RF VirtualAperture transmit-power queries | README 422–434 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e6-rf-tx-power-queries.md](task-034e6-rf-tx-power-queries.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E5 — RF VA DataPipe connections and endpoint association | README 435–446 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e5-rf-datapipe-association.md](task-034e5-rf-datapipe-association.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E4 — VA capability and Local Function queries | README 447–456 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e4-rf-va-capability-local-functions.md](task-034e4-rf-va-capability-local-functions.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E3 — RF element-group descriptor snapshots and safe Ada | README 457–467 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e3-rf-element-group-snapshots.md](task-034e3-rf-element-group-snapshots.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E2 — RF VirtualAperture status-change subscriptions | README 468–482 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e2-rf-va-status-subscriptions.md](task-034e2-rf-va-status-subscriptions.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034E1 — Live RF VA queries and owned instance-status reports | README 483–494 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034e1-rf-va-status-queries.md](task-034e1-rf-va-status-queries.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034D3 — RF Job event duration extension in C and safe Ada | README 495–505 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034d3-rf-job-event-extension.md](task-034d3-rf-job-event-extension.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034D2 — owned RF JobIntervalStatus and safe Ada | README 506–517 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034d2-rf-job-interval-status.md](task-034d2-rf-job-interval-status.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |
| Task 034D1 — bounded receive JobInterval commands | README 518–526 | Accurate at its checkpoint; later exclusions/counts are not current status | Remove duplicate; preserve existing task report | [task-034d1-rf-rx-job-intervals.md](task-034d1-rf-rx-job-intervals.md); CHANGELOG and coverage history | Linked here; README uses current family summary, no historical export count |

| RF PhysicalData / TxPowerModeData | README 527–537 | Implemented; embedded export count and receive-only limitation are historical | Condense; preserve reports | [PhysicalData](task-034c2-rf-physical-data.md), [TX power modes](task-034c3-rf-tx-power-modes.md); current RF spec | RF family summary; detailed snapshots remain in reports |
| Duration quantization | README 538–544 | Live explicit query; no implicit quantization | Condense; preserve | [Quantization](task-034c1-rf-duration-quantization.md) | Current queries summarized, detailed contract retained |
| Job lifecycle / introduction interleaving | README 545–570 | Lifecycle implemented; “status callbacks deferred” obsolete | Remove duplicate narrative; restore introduction | [Safe Ada lifecycle](task-034b2b2-safe-ada-rf-job-lifecycle.md), [status](task-034d2-rf-job-interval-status.md) | Current lifecycle/observer support replaces deferral |
| RF Admin | README 571–577 | Accurate bounded owner/acceptance statement | Condense; preserve | [Admin](task-034b1a-safe-ada-rf-admin.md) | Table and ownership guide |
| RF C2 owner checkpoint | README 578–585 | Open/Close-only description explicitly historical | Remove duplicate; preserve | [C2 owner](task-034b1b1-safe-ada-rf-c2-owner.md), current C2 spec | Current VA/Job support stated |
| RF VA and Jobs | README 586–596 | Implemented; incomplete checkpoint scope | Condense; preserve | [VA](task-034b1b2-safe-ada-rf-virtual-aperture.md), [Job](task-034b2a2-safe-ada-rf-job.md) | Current family summary |
| Initial safe Ada RF slice | README 597–606 | I16-only/Jobs excluded is obsolete; borrow/copy distinction remains important | Correct and condense; preserve history | [Initial RF](task-034a-safe-ada-rf.md), current ProductRx spec, F18–F25 above | Eight formats and current metadata stated |
| Quoted current IR/C2 status | README 607–641 | Core image/C2/metadata accurate; lease supersedes old copy diagram | Condense; preserve | [Commands](task-017-validation.md), [C2 metadata](task-018-validation.md), [Image metadata](task-024-validation.md), [navigation](task-026-validation.md), [lease](task-030a-zero-copy-ada-frame-lease.md), [provider buffer](task-030b-provider-buffer-zero-copy.md) | Owned copies versus provider-scoped leases distinguished |
| Conditional Instrumentation | README 642–652 | Conditional surface exists; inherited-service exclusion superseded | Correct and condense | [Instrumentation](task-028-instrumentation-validation.md), [common Ada](task-032b2-safe-ada-common-channel.md) | Conditional support and weak common services summarized |
| Conditional Track and four callbacks | README 653–719 | Track-specific surfaces represented; mock-positive only | Condense; preserve | [Core](task-029b2-track-report-validation.md), [updates](task-029c-track-data-update-validation.md), [system response](task-029d-system-track-data-response-validation.md), [request callback](task-029e-request-system-track-data-validation.md), [candidate](task-029f-candidate-object-message-validation.md), [preproc](task-029g-candidate-object-preproc-message-validation.md), [Squall negative](task-029b3-track-squall-validation.md) | Track implementation versus provider support stated |
| Raw parity / Rust / Python limitations | README 720–735 | 90 exports and absent common-channel APIs obsolete; safe RF remains absent | Correct; remove count | [Rust common](task-032b3-safe-rust-common-channel.md), [Python common](task-032b4-python-common-channel.md); safe implementations/tests | Include actual weak C2/Image common-channel APIs; no raw-parity inference |
| Non-goals | README 737–741 | Accurate | Condense | Architecture guide, CONTRIBUTING | Introduction and limitations |
| Why This Project Exists | README 742–815 | Rationale useful; duplicated diagrams and malformed sentence | Rewrite unique explanation | [Project context](project-context.md), architecture guide | Provider language independence retained accurately |
| Two Consumer Paths / language table | README 816–872 | Direct C++ correct; Ada IR-only stale | Condense; remove duplicate diagrams | Current public source, architecture guide | One diagram and one comparison table |
| MFA, MEL, Skill, Squall | README 873–966 | Useful concepts; simulator narrative not universal behavior | Rewrite and condense | [Project context](project-context.md), [Squall evidence](../integration/squall/README.md) | Definitions and example-provider distinction retained |
| Why Not Rewrite / advantages | README 967–1052 | Architectural options, not implemented provider or proof results | Rewrite unique rationale | [Project context](project-context.md) | Independent languages, incremental adoption and assurance boundary |
| High-Level Architecture / process diagrams | README 1053–1152 | Ada-only consumer statement obsolete; process distinction useful | Condense; remove duplicated diagrams | [Project context](project-context.md), architecture guide, Squall integration | In-process FFI versus provider transport retained without transport API claim |
| Why a C ABI | README 1153–1208 | Accurate rationale, duplicated | Condense | Architecture guide; [ABI policy](c-abi-policy.md) | Project-owned experimental boundary stated |
| Current Implementation Status | README 1209–1269 | RF/Track and common-service exclusions obsolete | Replace; do not archive as valid current status | Current Ada/native/safe Rust/public Python source and tests | Current surfaces plus precise limitations |
| Callback and Data Ownership Model | README 1270–1311 | “queue owns copied pixels / leases deferred” obsolete | Correct; preserve implementation evolution in existing reports | [Provider buffer history](task-030b-provider-buffer-zero-copy.md), ownership guide | Distinct IR lease / Ada copy / native RF callback copy |
| Repository Layout | README 1312–1363 | Layout largely useful; raw-only Rust description stale | Condense | Makefile/manifests; [build-tree correction](corrective-native-build-tree-isolation.md) | Seven directories; no source-file inventory |
| Build the Native Library | README 1364–1429 | Commands valid; mock/Squall/compiler details belong elsewhere | Condense; remove duplicate | Makefile, [build-tree correction](corrective-native-build-tree-isolation.md), Squall README, [integration](task-004-validation.md) | Production versus test trees; shortest root commands |
| Build Ada with Alire | README 1430–1479 | Commands/local-pin warning valid | Condense; preserve details | Ada README, manifests, foundation, [packaging](packaging.md) | Exact root commands and no registry-release claim |
| Build Rust | README 1480–1517 | External linking/owned frames correct; detailed flags duplicated | Condense | Cargo manifests/build scripts, [Rust validation](task-009-validation.md), [BIT](task-015-validation.md), architecture.md | make test-rust and externally built native dependency |
| Use Python | README 1518–1545 | Missing common-channel and BIT no-op updates | Correct and condense | Public __init__.py, Python README, [BIT](task-016-validation.md), [common](task-032b4-python-common-channel.md) | Actual current safe API and owned bytes |
| Local Validation Gate | README 1546–1559 | make check valid | Condense duplicate | scripts/check.sh, CONTRIBUTING | One root command block |
| Ada/SPARK, Rust, Python, C++ consumer paths | README 1560–1709 | Assurance rationale useful; repeated diagrams, old Python subset | Rewrite unique rationale; remove duplicate diagrams | [Project context](project-context.md); current safe APIs | Assurance boundary and language-specific ownership retained |
| Acronym glossary | README 1710–1752 | Context, not supported-feature inventory | Relocate unique reference | [Legacy glossary](history/readme-legacy-notes.md#background-glossary) | Table retained with explicit scope warning |
| Terminology at a Glance | README 1753–1809 | Duplicates concepts/diagrams | Condense; remove duplicate | [Project context](project-context.md) | MFA/MEL/Skill responsibilities retained |
| Provider and Skill Languages Are Independent | README 1810–1848 | Useful conceptual distinction, not supplied provider implementation | Condense duplicate | [Project context](project-context.md#provider-language-and-consumer-language-are-independent) | Explicit design option |
| Scope and Compatibility | README 1849–1868 | ABI 0.1/Linux/FFI caveats accurate | Condense | Public ABI header, manifests, provenance | Current limitations retained |
| Design Principles | README 1869–1886 | Accurate principles, duplicated in policy and guides | Condense | ABI policy, architecture.md, ownership guide; project context | Ownership, exceptions, callbacks, independence and proof boundary retained |
| Background References | README 1887–1908 | External background, not source contract | Relocate unique citation list | [Legacy references](history/readme-legacy-notes.md#original-external-reading-list) | Original citations retained, no current availability guarantee |
| Licensing | README 1909–1921 | Accurate upstream distinction | Condense | LICENSE, provenance/checksums | Apache-2.0 and retained upstream licenses |

## Current claim-to-source reconciliation

| Question | Current authoritative source and observed distinction | README action |
|---|---|---|
| RF and Track implemented? | `ada/src/ams-mel-rf*.ads`, `ams-mel-ir-track*.ads`, matching bodies/native tests and Ada contracts; Track's four callback kinds and two sends are represented | Removed blanket “not implemented”; conditional provider support remains explicit |
| Health/Instrumentation conditionality | `ams-mel-ir-health_status*.ads`, `ams-mel-ir-instrumentation*.ads`, native channel opens and task-028/029b3 evidence; public API presence is not provider support | Separate supported API from mock-positive/Squall-negative observations |
| ProductRx formats and metadata | `ams-mel-rf-product_rx.ads` Config and Product_Metadata; native `rf_product_rx.cpp`, F18–F25 tests | Four integer and four VITA classes; no PDW/LF, full ReceiveEvent or arbitrary any payload |
| VITA copy versus decode | ProductRx Packet_Byte_Array and format-specific With/Copy contracts; F20–F23 packet fixtures | Serialized bytes in packet/network order, not IQ decoding or complete semantic conformance |
| RF Job/VA ownership | `ams-mel-rf-c2.ads` Wait/Claim/Close; native `rf_job.cpp` JobState/submit_prepared and `rf_virtual_aperture.cpp` | Cached result is not ownership; Claim once; Jobs retain aperture/C2 child state; Close not Cancel |
| Callback queues | IR metadata specs and native callbacks; RF ProductRx and Interval_Status specs | Bounded DROP-INCOMING, owned metadata, no application code on provider callback thread |
| Zero-copy | Image Frame_Lease/With_Pixels versus Receive; native `ir_stream.cpp` and task-030b; ProductRx With/Copy | Scoped IR provider-buffer borrow, native RF payload copy, explicit Ada copies, owned Rust/Python pixels |
| DSO retention | native `rf_product_rx.cpp` PermanentRxRegistration and claim cleanup; `rf_job_interval_status.cpp`; ownership guide | Exact callable/state/DSO can remain permanent; logical Close not universal quiescence |
| Rust safe API | `rust/ams-mel/src/lib.rs` Session, ImageStream, ControlChannel, ChannelView, CommsRequest; `rust/ams-mel/tests/common_channel.rs` and C2/image tests | Includes weak common views, excludes safe RF and expanded commands/metadata/conditional owners |
| Python public API | `python/ams_mel/__init__.py` ChannelView/CommsRequest and public owners; `python/tests` | Same bounded safe surface; private ctypes declarations are not public coverage |
| ABI and exports | `native/include/ams_mel/abi.h` macros 0/1, `native/src/exports.map`; F25 export evidence | ABI 0.1 only; chronological export counts remain in reports, not README |
| Squall positive/negative evidence | Task-004/009/013/015/016, 028, 029b3, 033d, 034a, 034d/e/f reports and integration README | IR and Complex I16 observations distinct from conditional unsupported channels, no-op commands, zero-return power queries and hardware |
| Alire/publication | native/Ada/test `alire.toml` relative pins, Cargo `publish = false`, Python README, packaging release checklist | No invented release/index/PyPI availability |

## Audit boundaries

The true project heading at README 374–375 is replaced by the new opening H1;
its introductory meaning survives in the first two paragraphs. This accounts
for the boundary between the early task insertions and the original introduction.
The F9 endpoint-map report remains linked through the untouched architecture,
coverage and task history, although F9 had no separate block in the old README.
The original all-language provider-integration reference is retained explicitly
as [Task 013 validation](task-013-validation.md), alongside the language-specific
and Task 004 links above. Every individually named task-report path in the old
README has a destination in this matrix.

No unique task implementation evidence was discarded: each removed task block
maps to an existing report above, with historical summaries retained untouched
in CHANGELOG, coverage and architecture. The new historical notes intentionally
do not reproduce known-incorrect status/copy assertions. The source research
is an audit trail, not content appended to the root introduction. No production,
test, rendering, navigation, workflow, vendor or policy file is changed.
