# DOC-005C coverage and qualification

Selected base: latest fetched main
`4d4b07f83831ee524b5c91074ba84dd22070129d`, merge of dependency PR #106;
reviewed dependency head `ce470c267de9c45c7cb4c64b00e98c52bdd16fe5`
verified in its ancestry. Previous verified main:
`8f4f0b50b0bc0b9db0ab46b99f4dcf238c4d6c08`.

Branch: `docs/005c-rf-c2-events-waveforms-intervals`; isolated worktree:
`/home/zboll/git/ams-mel-doc-005c`; external evidence:
`/home/zboll/git/ams-mel-doc-005c-evidence/r1`. No destructive Git operation or
modification to prior worktrees/evidence. Eventual PR targets main, not a stacked
dependency branch. No partial DOC-005C PR is authorized.

## Qualified pristine reproduction and intended identities

The untouched source passed the qualified pipeline: repository 3,253/4,295,
RF C2 353/771, 1,042 repository gaps. Warnings: 892 raw = 819 public + 73
private-part; zero private-unit/tool/unclassified. Source/native declaration
counts: 1,238/1,237, with the one established Health renderer exception.

All 418 identities were derived from this complete measured inventory, selected
by exact declaration/profile/member identity before editing, and reconciled into
disjoint sets of 133/165/120. The baseline diff records the complete exact removal
set; external selected/removed identity JSON files preserve each checkpoint.
No operation-name-only coverage substitution or other-package documentation.

| Checkpoint | Additional removals | Cumulative | RF C2 | Repository | Remaining gaps |
| --- | ---: | ---: | ---: | ---: | ---: |
| 005C1 pulse/events | 133 | 133 | 486/771 | 3,386/4,295 | 909 |
| 005C2 modulation/waveforms | 165 | 298 | 651/771 | 3,551/4,295 | 744 |
| 005C3 intervals/operations | 120 | 418 | 771/771 | 3,671/4,295 | 624 |

| Warning inventory | Pristine | 005C1 | 005C2 | 005C3 |
| --- | ---: | ---: | ---: | ---: |
| Raw | 892 | 795 | 682 | 599 |
| Public | 819 | 722 | 609 | 526 |
| Private-part | 73 | 73 | 73 | 73 |
| Additional removals | — | 97 | 113 | 83 |

Exactly 293 public warning identities removed; all private identities, including
the ten RF C2 Finalize warnings, remain unchanged. No suppression or new warning.
The 624 final gaps belong to RF child packages plus the unchanged Health exception.

## Checkpoint qualification and retained attempts

Each complete group is formatted before semantic generation, then checked for
exact coverage/warning deltas, unchanged profiles/anchors and other-package
identities, semantic compilation, HTML/links, all 75 documentation unit/mutation
tests, six dedicated integrity tests, literal-aware tokens/protected bytes and
whitespace. Complete logs, actual exit statuses, generated inventories/sites,
source SHA-256 and identity deltas are external, named `005c1-*` through `005c3-*`.

Pre-ratchet strict failures on fixed stale baselines are retained, not claimed
as qualified passes. First 005C1 prose containing underscores failed the existing
source/rendered strict matcher; prose was clarified without changing that matcher.
At the continuation/pulse boundary GNATdoc first attached @enum to the constant;
a separator exposed an additional undocumented-constant diagnostic. A one-word
boundary comment restored the inherited warning state without crediting its gap
until 005C3 supplied the complete attached constant description. All attempts
are retained; no baseline addition or renderer/parser policy change was used.

Source research and provider limitations are in
`/home/zboll/git/ams-mel-doc-005c/docs/doc-005c-source-research.md`.
Previously completed DOC-005A/B lifecycle/VA/Job source and rendered descriptions
are preserved. New interval prose supplements previously credited interval-type
comments within DOC-005C scope, not lifecycle APIs. The inherited leading Extend
prose had rendered as the CancelRemaining description; those source lines are
preserved while proper trailing descriptions now distinguish the two interval
operations. This changes that in-scope rendered description, not a previously
completed DOC-005B lifecycle API. No executable Ada tokens change.

Other pre-existing credited descriptions inside the assigned DOC-005C groups
were inherited adjacent-comment attachments: Modulation_Table,
Waveform_Sample_I16_Array, Replace_Phase_Chips, both Set_Channelization_Enabled
overloads, Set_Empty_Modulations and Set_Interval_Activity_ID. Their new trailing
blocks attach the intended source-backed descriptions. Every inherited source
comment line is retained; completed DOC-005B lifecycle/VA/Job sections are
unchanged. The strict audit treats these in-scope attachment corrections
separately from protected prior-campaign descriptions.

## Final combined qualification status

Final-source deterministic generation, independent complete-diff audit, full
sequential production gates and publication identity checks are recorded below
only when actually completed. Checkpoint qualification alone is not publication
readiness. No new Squall/hardware execution is claimed.

### Frozen-source documentation qualification — PASS

Qualified version outputs: Alire 2.1.1; GNAT 16.1.0; GPRbuild 26.0.1 Alire
distribution reporting GPRBUILD 26.0.0 (2026-04-15); GNATdoc 26.0.0 (20260415).
No ambient GNATdoc substitution. Stable compiler paths from the inherited
qualified environment are retained for the full sequential run.

Final specification SHA-256:
`509c94edb5f7d3a95d057a8c2064b77c0379a15115d436d282592ee37ba232aa`.

Two fresh complete site generations passed strict semantic compilation,
coverage/warning ratchets, native profiles/anchors and HTML/link checks;
all **32 site files are byte-identical**. HTML inventory is **31 pages / 1,307
links**. All **75 documentation unit/mutation tests** and **six dedicated
integrity tests** passed. Both the selected-base and original-foundation
integrity boundaries passed, protecting 1,178 selected-base files and all
literal-aware Ada tokens. Complete 804-checksum validation and historical
evidence-ledger preservation passed. Git whitespace and final-newline checks
passed after correcting the newly added reports' missing final newline; the
initial failure is preserved externally. No production gate is inferred from
these documentation passes; sequential production results follow separately.

### Final sequential production qualification — PASS

The frozen-source runner completed every gate with exit status zero; complete
output and individual `.exit` files are under external `r1/final`:

| Gate | Actual result |
| --- | --- |
| `make test-native` | PASS, 434/434 |
| `make test-ada` | PASS, full safe Ada contracts and focused suites |
| `make check` | PASS, isolation, native 434/434, formatting, Ada and text gates |
| `make check-ada-format` after `make check` | PASS |
| Frozen specification hash before/after production | PASS, unchanged |
| Final independent full identity/protected-byte audit | PASS |
| Final whitespace/newline checks | PASS |

Commands ran sequentially, without overlapping shared native-tree writers.
Final tested source SHA-256 remains
`509c94edb5f7d3a95d057a8c2064b77c0379a15115d436d282592ee37ba232aa`.
The complete seven-file diff was inspected separately from numerical ratchets;
all inherited comment lines remain, all previous lifecycle/VA/Job descriptions
are unchanged, all executable tokens/vendor bytes are unchanged. No tests,
contracts, warning policy or renderer tools were weakened.

Main was refetched and remains the selected dependency merge; PR #106 remains
merged at its reviewed head. Its successful hosted main checks do not qualify
the new feature. Commit/push/PR-head equality, synthetic merge identity and
hosted feature results are separate publication observations recorded externally
and on the eventual PR. Leave that PR open for independent review; no merge,
tag, release, deployment or DOC-006 work.
