# DOC-007D qualification

This record describes local work for source-verified local type hyperlinks.
Final commit, PR, hosted status and evidence-manifest identities are recorded in
the external handoff after qualification, avoiding a self-referential commit.
No pending check counts as passed. Independent review remains required.

## R2 — external audit destination correction

Independent review accepted the type-link generation, dependency selection,
semantic audit and hosted publication, but found that R1's optional `audit_path`
could mutate a checked tree after validation. R2 starts from reviewed head
`dc216a3ebcf77f1235afe49b3b486fa80f7e0c1a`, tree
`c4ea6e1ded9bfc166c5a319a0b58b60a149a5863`, on the same branch/worktree and
existing PR #116. Main was still `0e64ea0689f2fae360baba78b6ed6f74aeef08ca`.
All 2,203 R1 evidence entries were hash-verified; R1 is retained untouched.
Corrective evidence is `/home/zboll/git/ams-mel-doc-007d-evidence/r2`.

### BEFORE and correction

Using isolated copies of the qualified pristine/publication, R1 validation with
`audit_path = published / "type-audit.json"` returned success with `files=41`
but left 42 files; subsequent validation rejected the unexpected file. With
`audit_path = published / "api/index.html"`, it returned the original hashes
after replacing the API index with audit JSON; subsequent validation rejected
the modified page. `before-audit-output-defects.json` records both before/after
inventories and reports. Original source, R1 evidence and qualified publication
were not mutation targets.

The consumer now checks audit output before qualification and again immediately
before writing. The destination must be a **fresh external file**, outside the
resolved validation repository root, trusted pristine input and publication
trees, with an existing parent. Parent aliases are resolved for containment;
existing destinations (including hardlinks, symlinks and dangling symlinks) are
rejected. Only the checked resolved path is used, with exclusive creation so an
intervening final-component file/link cannot be overwritten. Invalid destinations
fail clearly without any checked-tree byte change. Valid output contains all
1,418 occurrences, leaves checked trees unchanged and returns hashes that still
equal the complete publication after the call.

As with source qualification, the filesystem and parent directories must be
trusted and not concurrently replaced. This is not a sandbox against hostile
concurrent directory replacement or mount/bind aliases; it does not claim such
protection. No shared enhancer, configuration, linker, semantic helper, authority,
original validator, workflow, portal/theme or production byte was changed.

### R2 tests, output and rendering wording

Twelve new tests exercise actual consumer validation on qualified temporary
source/pristine/publication trees: extra publication file, existing HTML
overwrite, pristine/repository destinations, parent/file/dangling symlink aliases,
`..` and repository-root aliases, hardlinks, existing external files, missing
parents and successful fresh external output (including an external parent alias).
Rejected destinations assert preservation of all three checked trees; successful
calls assert complete audit and post-return inventory/hash accuracy. No output
writer is mocked and inputs are not made invalid merely to obtain rejection.
All existing 54 presentation/profile/semantic tests are retained: **66 tests
pass**, plus **116 original documentation tests**, including 34 integrity tests
counted once. BEFORE tests are also executed against the captured reviewed
consumer: ten of the twelve test methods fail there, while both valid-external
methods pass. Unittest reports twelve failure entries (including subtests) and
one error; these entries must not be subtracted from the method count.

The first BEFORE test run was stopped after its expected preservation failures
spent excessive CPU formatting multi-megabyte unittest dictionary diffs. Its
partial log and exit 143 are preserved. Assertions still compare exact bytes,
but now use a bounded failure message; the captured immutable R1 consumer is
rerun against the same cases. This is a test-diagnostic correction, not reduced
preservation coverage or a passed retry claim. The temporary BEFORE summary's
naive subtraction produced an invalid negative pass count; the separate
`tests-before-immutable-accounting.json` corrects that diagnostic arithmetic
without altering the original log/summary or the actual 66-test passing result.

The corrected build and validation reproduce **all 41 relative filenames and
complete-file bytes** of reviewed R1: 37 HTML pages / 2,964 local links / 1,418
audited local types / zero external types. R2 audit bytes also equal R1. R2 does
not run a new browser campaign: reuse of R1's Chromium 455 assertions/eight
screenshots is justified by complete publication byte equality, and those are
not counted as newly executed assertions.

The DOC-007C comparison is precisely 28 package pages plus manifest, with 1,418
new links. In eight pages, 35 qualified linked names absorb 113 prior
`ada-operator` dot spans into whole-name links. Independently removing links and
restoring those specific spans reproduces **all 40 non-manifest files** exactly;
native restoration remains byte-exact. `rendering-delta.json` records the actual
names and counts. R1's wrapper-only shorthand was incomplete, not an additional
unexplained rendering change. Its successful types-off control and historical
findings below remain preserved.

R2 ran all six required regression commands sequentially, all exit 0:
`make test-native` (434/434), `make test-ada` (contracts/existing repeats),
`make check` (isolation/native/format/Ada/hygiene), `make check-ada-format`,
`alr -n -C ada build`, and `alr -n -C ada/tests run`. Absolute `/usr/bin/gcc`
and `/usr/bin/g++` are Debian 14.2.0-19; GNAT/GNARL runtime is 16.1.0,
GPRbuild distribution 26.0.1, Alire 2.1.1, Python 3.13.5 and CMake 3.31.6.
Compiler caches/runtime observations and individual command logs/exits are
external evidence. No mutable build-tree writers or site-generation/readers
overlapped. No Ada source changed; the required formatting check still passed.
All 804 vendor hashes and 1,341 R1 tracked paths outside the four corrective
paths are preserved, including the semantic helper and inherited PR #115 files.

Final R2 scope, commit/remote/PR/synthetic-merge and hosted
job/checkout/artifact results are recorded in the external final handoff once
complete; pending results cannot authorize readiness. The R1 sections below
remain historical qualification, not claims that R1 guarded audit destinations.

## Isolation and fixed authority

Worktree `/home/zboll/git/ams-mel-doc-007d`, branch
`docs/007d-local-type-links`, starting main
`0e64ea0689f2fae360baba78b6ed6f74aeef08ca`; qualified tree and parent verification
are in [source research](doc-007d-source-research.md). Full external evidence is
`/home/zboll/git/ams-mel-doc-007d-evidence/r1`. DOC-007C and IR-CI-001 worktrees and
their evidence are retained, not reused as claims of new qualification.

The trusted DOC-007C pristine/publication copies were available, read, and
verified against immutable consumer pins and successful main artifacts. Fresh
generation also matches them. The source-preserving authority hashes stay fixed:

| Authority | SHA-256 |
|---|---|
| Source | `467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8` |
| Native anchors | `06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed` |
| Pristine site | `0e6497d6172c918d04ea1c284785f7cfc0fec278e155ab254de0c78e81ac94e2` |

## Publication and preservation

Two fresh pristine generations have identical relative-file inventories and
complete bytes: 39 files / 37 HTML pages / 1,516 links. Two fresh enhanced
generations also match completely: 41 / 37 / 2,964. Exactly the established theme
CSS and presentation manifest are additional files. Against DOC-007C, 28 package
pages gain 1,418 type-reference links and the manifest changes. In eight pages,
35 qualified linked names also absorb 113 prior `ada-operator` dot spans into
whole-name links: this is not a literal anchor-wrapper-only byte delta. Removing
the links and restoring those specific dot spans reproduces all 40 non-manifest
DOC-007C files exactly. Native restoration remains byte-exact. The native index,
empty root page, native/shared CSS and all authored portal bytes are equal.
The types-off control preserves all 40 non-manifest files; only two zero counters
are added to its manifest. No unexplained inventory, HTML, CSS or manifest drift.

The independent audit derives **1,418** local type links and zero external links:
1,516 original/portal + 30 theme + 1,418 = **2,964** final local links.
`type-link-occurrence-audit.json` contains every source/native occurrence and
destination; `determinism-and-exact-delta.json` records complete output inventories.
Complete native restoration, unchanged validators and source/anchor coverage,
expected enhanced-file reconstruction and full manifest reconstruction pass.

Measurements remain 29 public packages; 1,203 modeled profiles with zero unresolved
mismatches; 1,238 source declarations / 1,237 sections; coverage 4,294/4,295 with one
exact renderer omission and zero public gaps; 73 raw / zero public / 73 private
warnings; 1,162 protected files. All **804/804** vendor checksums match the
unchanged authority. All **1,334 out-of-scope tracked paths** match selected main
in both index and working tree; the ten-path task scope is checked separately.
The inherited IR test remains SHA-256
`fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400`;
the existing mechanical proposed policy label is not amended.

## Tests and browser

Complete original documentation discovery passes **116 tests**, including its
34 source-integrity tests (not 150 tests). The presentation suite retains the
35 existing tests and adds 19 direct independent semantic/integrity cases:
**54 tests pass**. Tests include real complex-array
and Euler targets, conservative alias omissions, current/ancestor/qualified cases,
wrong-existing targets, forged metadata, changed/moved text, strings/comments,
defining names, class attributes, ambiguous/shadowed targets, nested/duplicate/
malformed anchors, active/duplicate attributes, external/traversal URLs, removal
with forged consistent manifest, source/anchor/package/manifest tampering,
full restoration, deterministic build, API byte-idempotence and dry-run preservation.

A **new** Chromium **156.0.8078.4** campaign passes **455 assertions** at 1440×1000
and 390×844, light/dark, page JavaScript disabled throughout. It covers AMS.MEL,
Image, Track, RF Product_Rx, RF C2, Health_Status.Metadata and Status destinations:
lexical/link styling, real activation, exact landed declaration, direct fragments,
Back/Forward, keyboard focus/activation, iframe and portal navigation, actual code
scrolling/no page overflow, zero console errors and failed local requests.
Eight representative destination screenshots, actual assertions and destinations
are external evidence. This is a bounded Chromium campaign, not all-browser or
comprehensive accessibility certification. DOC-007C's old assertions are historical.

## Preserved failures and diagnoses

1. Initial directory inspection in DOC-007B found no later `docs/tools` or
   `docs/tests`; the verified DOC-007C tree was then read. An optional native
   filename lookup used a nonexistent path; the real inherited test was found
   and hash-checked. These are lookup failures, not passed validation.
2. An external installer preflight copy could not infer repository-relative
   imports from its evidence-directory location. Installation completed; its
   post-install wrapper import failed. Explicit fingerprint checking succeeded.
3. The first production installer exposed a genuine helper integration error:
   `runpy.run_path` does not add `docs/tools` to `sys.path`. The wrapper now adds
   its absolute helper directory, and a fresh complete install is rerun. The
   first foreground command timed out; its installer failure log is retained.
4. First documentation test discovery ran before `_site` existed: 103 tests
   reported 14 errors. After fresh generation, full discovery reports 116 passing
   tests. No test was removed or weakened.
5. The initial independent helper rejected legitimate returns after formal
   defaults, components following discriminant defaults and alternating context
   clauses. Those exact inputs exposed bounded-parser bugs, fixed using balanced
   depth/declaration boundaries and source clauses. Both failed diagnostic audits
   are retained; all real occurrences then passed. No shared-tool output patch.
6. A subsequent pristine generation was incorrectly scheduled while a presentation
   suite still read `_site`. Its missing-file/pristine-byte failures are preserved
   and not counted. Tests are rerun without concurrent site generation; fresh
   pristine equality is verified separately. No native build-tree writers overlap.
7. Two new acceptance tests assumed contiguous highlighted qualified spelling and
   a public Image Euler `Value` parameter. Actual native/source inspection showed
   punctuation spans and a body-only parameter. Tests now inspect restored text
   and accurately distinguish public components from `Raw_Euler` in the body.
   The task's requested nonexistent public parameter is not fabricated.
8. The final-newline gate caught missing newlines in two added helper/test files;
   they were corrected and the unchanged gate passes.
9. `gh run view --log` returned empty output for completed baseline/upstream runs.
   Those empty files were not treated as inspected logs. Raw workflow-log ZIPs
   were acquired through the API, and actual per-job checkout/deployment identity
   lines were extracted and checked from their complete contents.

These diagnoses are not reasons to discard failures or accept a green rerun
without explanation. Earlier campaign failures remain untouched. Final-head
hosted failures, if any, must be preserved and diagnosed separately.
An ambient `ldd` observation lacked the facade search path; the qualified
runtime observation with `native/build-tests/lib` resolves the facade and GNAT 16
tasking/runtime libraries. Actual contract executions use the existing scripts'
runtime environment and pass. A broad shadow-name diagnostic found eight prior
type marks before defaults; lexical defining-name refinement confirmed these
were not shadowing declarations and found zero preceding defining-name collisions.

## Regression and handoff boundary

All required regression commands completed **sequentially, exit 0**:

| Command | Result |
|---|---|
| `make test-native` | 434/434 passed |
| `make test-ada` | Complete contracts and existing focused repeats passed |
| `make check` | Existing build isolation, native, format, Ada and hygiene passed |
| `make check-ada-format` | Passed; existing source-less C-project warnings only |
| `alr -n -C ada build` | Passed |
| `alr -n -C ada/tests run` | Complete contract execution passed |

They used absolute `/usr/bin/gcc` and
`/usr/bin/g++` (Debian 14.2.0-19), qualified GNAT 16.1.0 / GPRbuild distribution
26.0.1 (`GPRBUILD 26.0.0 (2026-04-15)`), Alire 2.1.1, CMake 3.31.6, Python 3.13.5
and GNATdoc 26.0.0 (20260415). Existing repeat gates remain unchanged, including
the hosted parallel 50-repeat native gate; no unrelated diagnostic native
campaign was added. Actual command exits, compiler caches and runtime/build logs
are external evidence. No first-party Ada source changed, so no formatting
rewrite was necessary; format checking still ran in the required sequence.

The final handoff must account for all six regression commands, exact ten-file
task scope, every out-of-scope path/index byte, 804 vendor checksums, final source
integrity/baseline delta/whitespace/newlines, final HEAD/remote/PR equality,
current main/synthetic-merge parents/tree, all expected final-head hosted jobs,
and complete hosted-artifact equality using the final independent validator.
Only after those gates may disposition be READY_FOR_INDEPENDENT_REVIEW.
No merge, auto-merge, force-push, tag, release or manual deployment is authorized.
