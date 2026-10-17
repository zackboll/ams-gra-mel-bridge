# DOC-007B qualification

## Baseline and scope

Starting fetched main: `291e97c4b1495b594b462982822fa48a2f439527`, exactly the
expected base. PRs #110, #111 and #112 were verified merged. No existing DOC-007B
branch, worktree or PR was found. Existing worktrees, task reports, evidence and
commits were preserved. No reset, destructive clean, force-push or history rewrite
was used.

Branch: `docs/007b-readme-consolidation`.
Isolated worktree: `/home/zboll/git/ams-mel-doc-007b`.
External evidence: `/home/zboll/git/ams-mel-doc-007b-evidence/r1`.
Exact qualified commit/tree, fetched remote/PR head, synthetic merge identity and
hosted check observations are recorded externally after the normal commit, to
avoid self-referential hashes in this report.

Only these five Markdown files change:

- `README.md`
- `docs/doc-007b-readme-research.md`
- `docs/doc-007b-qualification.md`
- `docs/project-context.md`
- `docs/history/readme-legacy-notes.md`

All 1,328 pre-existing tracked files other than README match starting main
byte-for-byte. This independently covers production code, public specifications,
tests, the integrity policy and approved hashes, renderer infrastructure, site
inputs/navigation, workflows, manifests, CHANGELOG, coverage and every task
report. No generated site is tracked. No enhancer integration or DOC-007C work.

## README review and historical retention

The complete starting README measured 1,921 lines / 89,572 Unicode characters.
The final introduction is 232 lines, with exactly one true H1, one bridge diagram
and the requested section order: introduction; architecture; language surfaces;
getting started; documentation; providers/validation; limitations; layout;
provenance; contributing/license. The old `# AMS MEL` inside a fenced diagram
was not miscounted as a real heading.

The [research matrix](doc-007b-readme-research.md) maps every removed block to
existing task reports, CHANGELOG/coverage/architecture, the concise
[conceptual context](project-context.md), or the explicitly
[historical reference notes](history/readme-legacy-notes.md). An independent
retention check found all 45 task-report paths individually named by the old
README in the matrix; every original glossary row and external background
citation remains in the notes. Git history is secondary evidence, not the sole
archive for unique material.

Corrected statements include RF DataMEL/ProductRx/Admin/C2/VA/Job support,
conditional Track/Instrumentation and metadata, eight integer/VITA receive
formats, VITA byte copying rather than decoding, current RF ownership and
callback retention, and the safe Rust/Python C2/Image common-channel services.
Raw sys/ctypes parity is not safe API parity. IR provider-buffer borrows, native
RF callback copies and independent Ada copies are kept distinct. Squall no-op
acceptance, unsupported channels and successful Finalize do not establish
hardware execution. No publication, formal-proof or compliance claim is added.

## Links and reader navigation

The external checker validates every README local link and every Markdown
fragment used by the task documents, balanced fences, one H1, ordered sections,
real Make targets, no task-number headings and no machine-local README paths.
All eight unique published URLs (home, six guides, native API index) returned
HTTP 200 and also exist in the generated starting-main site. No unresolved
repository HTML template is presented as a rendered guide. Readers can reach
build instructions, IR/RF workflows, ownership, provenance and historical
technical evidence directly. Link details and HTTP observations are in
`readme-check-results.json`; the final checked-link count is external.

## Source and documentation gates

POL-001 was checked first: the complete reviewed native-test transition is
**APPROVED**, with both original checker constants unchanged. The source-integrity
checker protects 1,162 foundation files; the native-test SHA-256 pair remains
`ef8c1dc59987ea2ff16cb00d6a60880ab560b910127105b6dd7698498be9d957` and
`96f71931cbf3c03937d352997666ad459db9f517d514c7827579bbfa22a62930`.

Qualified executable versions were checked: Alire 2.1.1, GNAT Native 16.1.0,
GPRbuild 26.0.1 distribution (prints 26.0.0, 2026-04-15) and GNATdoc 26.0.0
(20260415). The ambient different GNATdoc was not used. Pristine generation was
captured from the unchanged starting main before editing README.

| Gate | Actual result |
|---|---|
| Initial source integrity | Exit 0; reviewed native tests approved, 1,162 protected files |
| Documentation unit/mutation discovery | Exit 0; 105 tests |
| Dedicated source-integrity discovery | Exit 0; 23 tests |
| Fresh complete site generation, twice | Exit 0 both; semantic spec compilation and strict ratchets passed |
| Documentation coverage | 4,294/4,295; zero public gaps; one exact Health metadata Status alias renderer exception |
| Warning identity | 73 raw / zero public / 73 private-part; zero added/removed warning identities |
| Native declarations/rendering | 1,238 source / 1,237 sections; all 1,237 native anchors/profiles unchanged |
| Full site inventory | 39 files / 37 HTML pages / 1,516 local links |
| Public package destinations | 29 unique names and unique destinations |
| Main / first / second full-site bytes | Both recursive diffs exit 0; all SHA-256 manifests identical |
| Baseline delta | Exit 0; no coverage or warning baseline changes |
| Vendor checksums | All 804 match the unchanged ledger |
| Historical retention | All original named task reports, glossary rows and background citations accounted for |

The full-site manifest SHA-256 is
`e83f173f4267b26948a0ab898035e2981419ec14b2be8f11f948e396f1465380`.
Full tree comparisons include native `api/`, CSS, index/navigation and portal
pages, not just selected HTML pages. Frozen manifests and the one renderer
exception were not edited.

## Sequential build and regression qualification

All mutable native writers run in one external sequential driver. Each command
has its exact command, complete log and numeric exit-status file. Required order
is `make test-native`, `make test-ada`, `make check`, `make check-ada-format`.
Additional README command verification follows those gates, never overlaps them.

| Command | Actual result |
|---|---|
| `make test-native` | Exit 0; 434/434 passed |
| `make test-ada` | Exit 0; complete contracts and focused repeats passed |
| `make check` | Initial exit 2 at staged whitespace; complete corrected rerun `check-r2` exit 0, including isolation/native/format/Ada/hygiene |
| `make check-ada-format` | Exit 0; existing externally built import “no C sources” warnings only |
| `make native` | Exit 0; production facade built |
| `alr -C ada build` | Exit 0; pinned local workspace built |
| `alr -C ada/tests run` | Exit 0; complete contract execution passed |
| `make test-rust` | Exit 0; workspace and compile-fail ownership tests passed |
| `make test-python` | Exit 0; 145 tests and warning-as-error compileall passed |
| Final source integrity | Exit 0; 1,162 protected files, reviewed native tests approved |
| Final newlines / unstaged and staged whitespace | Exit 0 each |

The exact Alire invocations use `-n` for noninteractive execution; the README
shows their equivalent standard user commands. Vendor and whole-source identity
were checked again after qualification. All 804 vendor checksums and every
pre-existing non-README file match the starting main. Complete command/status
inventory and logs are retained externally, including the first whitespace
failure. The two final static-review documents were rechecked before commit.

## Attempts and review corrections

No failed gate is suppressed. An initial
editor request to delete/add README in one patch was rejected without changing
the file; the complete candidate was then installed. A retention assertion
initially identified a missing explicit Task 013 destination in the matrix;
that link was added and all 45 original task references passed. Research source
path review removed a nonexistent split RF view filename. The initial report
creation lacked its final newline; both the standalone newline check and external
link checker detected it, and both passed after correction. All observations
and earlier logs are preserved externally. The first `make check` completed
native/Ada execution but exited 2 at its final staged-whitespace check: four
inherited Markdown hard-break spaces in the relocated reading list. Those were
removed, both Git whitespace checks passed, and the complete gate was rerun with
separate `check-r2` logs/status. The original failure remains in `check.log` and
`check.exit`. No tests or policy were weakened.

## Publication boundary

Main is refetched before publication and compared for README/documentation
advancement. The complete five-file diff, whitespace and final newlines are
reviewed before normal commit/push. The requested PR targets main, non-draft,
open and unmerged, with auto-merge disabled. Exact local/remote/PR head equality,
synthetic merge parents/tree equality and new-head Documentation/Bootstrap
observations are external post-commit evidence. No merge, tag, release or manual
deployment is performed.
