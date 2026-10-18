# DOC-007D qualification

This record describes local work for source-verified local type hyperlinks.
Final commit, PR, hosted status and evidence-manifest identities are recorded in
the external handoff after qualification, avoiding a self-referential commit.
No pending check counts as passed. Independent review remains required.

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
pages gain approved type-anchor wrappers and the manifest changes; the native
index, empty root page, native/shared CSS and all authored portal bytes are equal.
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
