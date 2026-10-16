# DOC-005B coverage and qualification

Starting main: `8f4f0b50b0bc0b9db0ab46b99f4dcf238c4d6c08` (DOC-005A
merge), fetched and unchanged at preflight. Existing worktrees, branches and
evidence are preserved. Isolated branch: `docs/005b-rf-c2-va-job`; worktree:
`/home/zboll/git/ams-mel-doc-005b`; external evidence:
`/home/zboll/git/ams-mel-doc-005b-evidence/r1`.

## Pristine reproduction

Qualified semantic generation reproduced repository 2,912/4,295, RF C2 12/771,
1,383 gaps; 1,146 raw warnings = 1,073 public + 73 private-part. Inventory:
1,238 source declarations / 1,237 native sections, one existing Health renderer
omission. Pristine coverage, inventory, warnings, site and log are preserved.

Qualified environment: Alire 2.1.1, GNAT 16.1.0, GPRbuild 26.0.1 distribution
reporting GPRBUILD 26.0.0 (2026-04-15), GNATdoc 26.0.0 (20260415).

## Checkpoint identity measurements

| Checkpoint | Additional removed gaps | Cumulative | RF C2 | Repository |
| --- | ---: | ---: | ---: | ---: |
| 005B1 — C2/VA | 87 | 87 | 99/771 | 2,999/4,295 |
| 005B2 — Job configuration/request | 196 | 283 | 295/771 | 3,195/4,295 |
| 005B3 — Job lifecycle | 58 | 341 | 353/771 | 3,253/4,295 |

Full declaration/profile/member identities, not operation-name matches, are in
external `005b1-removed-identities.json`, `005b2-removed-identities.json` and
`005b3-removed-identities.json`.
The removal-only baseline diff is the committed exact identity record.

005B1 includes the previously undocumented RF C2 package description. Its first
measurement removed 86 gaps; inspecting the full identity inventory identified
the missing package identity, bringing the checkpoint to exactly 87. No
out-of-scope identity was substituted. The initial 005B2 measurement identified
six uncredited attachments; source-backed prose was corrected without modifying
the parser or renderer policy. Previously documented descriptions were preserved;
missing parameter/return tags were added to the applicable overloads only.

Strict baseline failures before reviewed removal-only updates are expected and
retained, not counted as qualified runs. Tool-call wall-clock timeouts interrupted
foreground command groups; later commands use background runners with dedicated
exit files. A partial group without its exit file is not reported as a full pass.

## Qualification status

This report is a progress record until the final combined gates and publication
observations are appended. Intermediate documentation checks do not constitute
production regression qualification or a completed DOC-005B PR.

## Combined documentation qualification

Independent selection against the pristine full declaration identities matches
all three measured removal sets exactly: 87 + 196 + 58 = 341. Zero added gaps,
zero added identities, no removed declarations, no changed previously documented
description or untouched-package gap. Missing member tags on previously described
overloads preserve their original prose. All DOC-005C source comment blocks and
rendered sections remain unchanged: 299 event/modulation/waveform gaps and 119
interval-related gaps, total 418. Repository remaining gaps: 1,042.

| Warning inventory | Pristine | 005B1 | 005B2 | 005B3 |
| --- | ---: | ---: | ---: | ---: |
| Raw | 1,146 | 1,088 | 950 | 892 |
| Public | 1,073 | 1,015 | 877 | 819 |
| Private-part | 73 | 73 | 73 | 73 |
| Additional removals | — | 58 | 138 | 58 |

Exactly 254 warning identities removed, zero added; all 73 inherited private-part
identities unchanged, no tool/unclassified warnings. Warning removals are from
actual generated diagnostics, not inferred from coverage counts. Native manifest
equality confirms unchanged 1,238 declarations, 1,237 sections, profiles, anchors
and one established Health omission.

The external independent audit also compared every unaffected package's HTML
bytes, every prior RF C2 documented rendered description, every DOC-005C section,
and literal-aware executable tokens. All passed. Independent protected inventory:
1,180 non-documentation files, including 804 byte-identical vendor files. The
established integrity entry point checks 1,178 task-base protected files and
1,162 original DOC-001 protected files; both passed. No tooling, tests, renderer
exceptions, native anchors or production files were changed.

Combined semantic compilation, strict generation twice, byte-identical complete
sites, HTML structure/local links (31 pages, 1,307 links), 75 documentation
unit/mutation tests and six dedicated integrity tests passed. Format/check,
baseline-delta, whitespace and final-newline checks passed. Full logs and source
hashes are retained externally. The first 005B3 generation exposed GNATdoc
attaching enum tags to the preceding undocumented interval operation; an explicit
blank separator fixed the attachment without documenting that operation. An
initial final-newline failure in the two newly created reports was corrected;
the passing tail run is recorded separately. Failed attempts remain preserved.

First combined documentation-qualified source SHA-256:
`494ea60e0ee1621833e59d2e4fcc9bd62bcacae2eb0ee569a2ba088edf0909ed`.
Final review corrected the interruptable flag wording to pinned resume/terminate
semantics and made the open-C2-child-admission submission precondition explicit.
The final qualification reruns documentation and all four sequential production
gates on the corrected, frozen source; the earlier sequence is retained but is
not credited as final exact-source qualification.

Full sequential production regression results and final publication observations
are pending; the documentation-only checkpoints alone are not final readiness.

## Exact-source final qualification run

The final runner uses frozen specification SHA-256
`22e05187ab1cce06e2ffb9f90eb0dd143bc1ab2751d4525921e6f1789890fbdd`.
Its format, semantic generation twice, deterministic site comparison, all 75
documentation tests, six dedicated integrity tests, both historical integrity
boundaries and independent exact-identity audit passed before starting production
gates. All 804 manifest vendor checksums also verified independently.

The earlier production sequence passed each command but its final source-hash
comparison reported SOURCE_CHANGED because of the three review-corrected comment
tags. It is explicitly superseded, not counted as exact-source qualification.
The final runner repeats all four production gates sequentially; individual exit
statuses and complete logs live under external `r1/final`.

## Final local qualification — PASS

The final corrected source passed all required gates, with full logs and actual
zero exit statuses retained under external `r1/final`:

| Gate | Result |
| --- | --- |
| Format and format check before semantic compilation | PASS |
| Strict semantic compilation and GNATdoc twice | PASS |
| Exact gap/warning ratchets, profiles and anchors | PASS |
| Two fresh sites, all 32 files byte-identical | PASS |
| HTML structure and links | PASS: 31 pages / 1,307 links |
| Documentation unit/mutation tests | PASS: 75 |
| Dedicated integrity tests | PASS: six |
| Literal-aware executable tokens and protected/vendor bytes | PASS |
| make test-native | PASS: 434/434 |
| make test-ada | PASS: full safe Ada contracts and focused cases |
| make check | PASS: isolation, native, formatting, Ada and text gates |
| make check-ada-format after make check | PASS |
| Final source hash against frozen tested source | PASS |
| Whitespace and final newlines | PASS |

Production gates ran sequentially, without overlapping mutable native trees.
Final specification SHA-256:
`22e05187ab1cce06e2ffb9f90eb0dd143bc1ab2751d4525921e6f1789890fbdd`.
Independent complete-diff review and the seven-file allowlist passed. No tests,
contracts, warnings or policy were weakened. No real-provider/hardware run is
claimed. Main was refetched and remained at the exact starting merge. Hosted
checks and local/remote/PR-head equality are separate publication observations,
recorded externally and on the PR; the PR must remain open for independent review.
