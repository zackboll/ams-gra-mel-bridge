# DOC-006A — ProductRx documentation qualification

## Recovery and selected base

Recovered `/home/zboll/git/ams-mel-doc-006-evidence/r1/RECOVERY.md` in full.
The unchanged r1 inventory, site, successful command logs/exit files and eight
source hashes were verified rather than regenerating a pristine campaign.
The reuse manifest hashes every r1 evidence file in new external evidence:
`/home/zboll/git/ams-mel-doc-006-evidence/r2-product-rx`.

Fetched main: `c7f81919f125048ca81662d7d9534b4708e0e331`, the expected PR #107
merge. Reviewed source head `b2ac07f85f646c23023e9c4ff223c41a47fc9e4c` is an
ancestor. No intervening change or equivalent feature branch/PR was found.
Original branch `docs/006-rf-child-product-rx` is preserved; continuation branch
`docs/006a-rf-product-rx` uses the existing worktree
`/home/zboll/git/ams-mel-doc-006`. No destructive Git operation or prior-worktree
modification was performed.

## Exact inventory and checkpoints

Reused baseline: ProductRx 2/263; repository 3671/4295 with 624 gaps;
599 raw warnings = 526 public + 73 private-part. The exact 261 ProductRx gap
identities were verified against r1's complete inventory before editing and
saved with full signatures, overloads and member identities. Baseline changes
are removal-only; the final baseline diff records the complete removed set.

| Checkpoint | Additional credited gaps | Cumulative removals | ProductRx | Repository | Raw/public/private warnings |
| --- | ---: | ---: | ---: | ---: | --- |
| A1 creation/endpoint | 82 | 82 | 84/263 | 3753/4295 | 503/430/73 |
| A2 samples/ownership | 54 | 136 | 138/263 | 3807/4295 | 452/379/73 |
| A3 packets | 33 | 169 | 171/263 | 3840/4295 | 420/347/73 |
| A4 metadata and attachment completion | 92 | 261 | 263/263 | 3932/4295 | 368/295/73 |

These are actual credited identity deltas, not counts of comments written.
Some A1–A3 prose initially rendered without qualifying for strict source-backed
credit. A4 includes their attachment/prose corrections, not just metadata.
All 261 original identities are now credited; zero declaration identity changes,
zero new gaps, zero other-package coverage changes, and zero added warnings.
The exact cumulative sets and per-checkpoint warning removals are external JSON.

Each successful checkpoint ran formatting, semantic compilation and strict
GNATdoc, native profile/anchor comparison, HTML/link validation, 75 documentation
unit/mutation tests, six dedicated integrity tests, selected-base literal-aware
source/protected-byte checking and whitespace checking. Source hashes, complete
logs and individual actual exit statuses accompany checkpoint result files.
No production regression was repeated at intermediate checkpoints.

Failed attempts are not qualified passes: A1 first attached an overload block
incorrectly; A4 exposed prose underscore-matching and aspect-bearing complex
record boundaries. Source comments, not tooling, were corrected. The two
complex record descriptions straddle the aspect boundary with identical prose
to satisfy the existing native renderer and source scanner simultaneously.
All inherited source comments are preserved byte-for-byte.
The task-external runner reused A1/A4 development log filenames, so earlier
failed full pre-ratchet logs were overwritten; this evidence-retention limitation
and observed diagnostics are explicitly recorded in external
`development-attempts.md`. Final successful checkpoint logs and separate final
qualification logs/exit statuses are retained. No missing failed log is claimed
as a complete preserved artifact.

## Final scope and ratchets

ProductRx is 263/263; repository is 3932/4295 (91.5483%). Exactly 363 gaps
remain: 362 in the seven deferred RF C2 child packages and the one established
Health metadata package-renaming renderer exception. DOC-006B is deferred and
has not been started; this is not completion of the original full DOC-006 task.

Exactly 231 ProductRx public warning identities are removed, leaving 368 raw =
295 public + 73 private-part. Every inherited private identity, including the
three ProductRx Finalize operations and their parameters, remains unchanged.
No tool, private-unit or unclassified diagnostic is accepted. The seven deferred
packages and completed DOC-001–DOC-005C source files are unchanged.

Native inventory remains 1238 source declarations and 1237 rendered sections.
All native profiles/anchors and the Health renderer manifest remain unchanged.
The sole authorized Ada file has comments/formatter whitespace only; production
bytes, literal-aware executable tokens, vendor bytes, tests and tooling remain
protected. Source-backed semantics and qualification limitations are recorded
in `doc-006a-source-research.md`.

## Final-source qualification

Final qualification results and tested hash are appended only after the actual
commands complete. Checkpoint success alone is not publication readiness.

### Final documentation and production qualification — PASS

Qualified tools: Alire 2.1.1, GNAT Native 16.1.0, GPRbuild 26.0.1 Alire
installation reporting GPRBUILD 26.0.0 (2026-04-15), GNATdoc 26.0.0 (20260415).
The established qualified environment was reused without ambient substitution.

Frozen tested specification SHA-256:
`dfa6d851e81e7619d824ca5c87e1c9e473dfd0f5f58b4dbc63a0f47ec823d578`.

Two fresh strict semantic/site generations passed; all 32 site files are
byte-identical. HTML validation: 31 pages and 1307 local links. ProductRx page:
`api/6ed8ea2a1a59f5d24924546adaf2f81b35ead370e48aebb921cc771ad72242c5.html`.
All 28 other-package pages are byte-identical to r1. Final 75 documentation
unit/mutation tests and six dedicated integrity tests passed. Selected-base
integrity protects 1178 files; original DOC-001 integrity protects 1162 files.
All 804 vendor checksums and eight independently fetched pinned declaration
roots passed. Complete gap/warning identity, native profile/anchor, executable
Ada token, production-byte, whitespace and final-newline audits passed.

Production gates ran sequentially on the frozen specification, with complete
logs and individual exit files in external `r2-product-rx/final`:

| Gate | Actual completed result |
| --- | --- |
| make test-native | Exit 0; 434/434 tests |
| make test-ada | Exit 0; full safe Ada contracts and focused suites |
| make check | Exit 0; build isolation, native 434/434, format, Ada and text gates |
| make check-ada-format | Exit 0 after make check |
| Before/after tested-source hash | Identical |
| Post-regression seven-file and full identity audit | Exit 0 |

No provider/hardware run is claimed. Historical comments and report entries
are retained; the evidence ledger is append-only, including its original byte
representation. Main was refetched unchanged before publication. Feature-head,
remote/PR equality and hosted CI observations are preserved separately in the
external publication evidence; pending hosted jobs are not passing jobs.
