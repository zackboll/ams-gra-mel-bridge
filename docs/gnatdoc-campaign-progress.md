# GNATdoc campaign progress

## DOC-005B — combined documentation checkpoints qualified

Starting fetched main is DOC-005A merge
`8f4f0b50b0bc0b9db0ab46b99f4dcf238c4d6c08`, with no intervening commits or
equivalent active DOC-005B branch/worktree/PR. Isolated worktree:
`/home/zboll/git/ams-mel-doc-005b`; branch `docs/005b-rf-c2-va-job`;
external evidence `/home/zboll/git/ams-mel-doc-005b-evidence/r1`.
All historical evidence and unrelated worktrees/branches remain preserved.

Pristine reproduction: repository 2,912/4,295, RF C2 12/771, 1,383 gaps.
005B1 removes exactly 87 full gap identities (99/771; 2,999/4,295).
005B2 removes exactly 196 more (295/771; 3,195/4,295).
005B3 removes exactly 58 more (353/771; 3,253/4,295).
Combined: 341 removals, zero additions, 1,042 remaining repository gaps.
All 418 DOC-005C identities (299 + 119) remain untouched, not begun.

Warnings: 1,146 to 892, exactly 254 genuine removals, zero additions;
819 public and 73 unchanged private-part. Native 1,238/1,237 inventory,
profiles/anchors and single Health omission unchanged. All previously documented
descriptions and unaffected rendered packages remain unchanged. Eighty-five
RF/Common headers match exact pinned upstream Git blobs; all 804 vendor bytes
remain protected. Source research distinguishes current v4 configuration from
historical single-RX reports and mock TX evidence from pinned Squall RX-only
observations. No new provider/hardware execution is claimed.

Qualified semantic generation twice, byte-identical sites, HTML 31 pages/1,307
links, 75 documentation tests, six dedicated integrity tests, task-base/original
literal-aware integrity, removal-only ratchets and formatter checks pass.
Initial attachment/newline failures are retained without policy or test changes.
See `docs/doc-005b-coverage.md` and `docs/doc-005b-source-research.md`.
Full production gates are running sequentially; no publication/readiness claim
is made at this checkpoint.

## DOC-005 — 005A Admin checkpoint; task incomplete

Fetched PR #104 merge `d4a4e262eb1404c963c509cf1e5d32f0c6712b99`;
no intervening main commits or equivalent DOC-005 branch/open PR found.
Created isolated `/home/zboll/git/ams-mel-doc-005` on `docs/005-rf-admin-c2`,
preserving existing worktrees. External evidence is under
`/home/zboll/git/ams-mel-doc-005-evidence/r1`.

Pristine qualified reproduction matched DOC-004. Admin now measures 15/15;
C2 remains 12/771. Repository: 2,912/4,295, 1,383 gaps. Exactly 15 gap and
15 public-warning identities removed, zero added. Raw/public/private warnings:
1,146/1,073/73. Other packages, native 1,238/1,237 inventory and the Health
renderer exception remain unchanged. All 85 RF/Common headers match pinned
Git blobs and checksum records.

Format/check, strict semantic generation twice, byte-identical sites, HTML
31 pages/1,307 links, 75 documentation tests, six integrity tests, task-base
1,178-file production integrity and removal-only baseline validation passed.
Initial ratchet/attachment failures remain in evidence. No test or policy changed.
See the new DOC-005 reports for source mapping and exact limitations.

C2 phases and sequential production regression qualification remain outstanding.
No commit, push or PR; no review-readiness claim. This is a partial checkpoint,
not completed DOC-005.

## DOC-003 preflight checkpoint

Verified fetched main `e7a652c7a62e66d58c275f6ab1f09bb6e2021622` includes
merged PR #102 and reviewed DOC-002 head. Created isolated worktree
`/home/zboll/git/ams-mel-doc-003` on
`docs/003-ir-image-health-instrumentation`, without changing the clean original
checkout or existing worktrees. External evidence is retained in
`/home/zboll/git/ams-mel-doc-003-evidence`.

Qualified untouched baseline reproduced: 934/4,295 elements, 3,036 warnings
(2,963 public, 73 private-part), 1,238 source / 1,237 native declarations and
one exact renderer exception. All 75 documentation tests passed. Post-merge
documentation run 38673364878 passed; Bootstrap 38673364892 was still running
at first inspection. Research and implementation are in progress; no final
qualification or submission is claimed. See the separate DOC-003 reports.

## Reconciled starting identity

- Repository: `https://github.com/zackboll/ams-gra-mel-bridge`.
- Starting main: `42648a92eb0cc84b2980d16db6028992c0494ba1`.
- Starting tree: `17d1737015a875eca8b5f7695dae744480599323`.
- `origin` fetched before work; local main and origin/main agreed.
- GitHub confirmed PR #100 merged at that main SHA; no open PRs returned.
- Original working tree clean; existing worktree registrations preserved,
  including registrations marked prunable. Nothing pruned or reset.
- Isolated campaign branch: `docs/001-gnatdoc-foundation`, from that exact main.
- This is an **in-progress checkpoint**, not completed DOC-001. The checkpoint
  commit is discoverable with `git log` for this file; no self-referential SHA
  is embedded in its own commit.

## Immutable reference sources

Verified commit objects and repository remotes, then extracted immutable Git
archives outside the campaign repository:

| Family | Repository | Commit |
| --- | --- | --- |
| Core | `zackboll/opencv_core_ada` | `f8aef00c5366b4275ccd3d934f9c73ea5b83cd4f` |
| Imgproc | `zackboll/opencv_imgproc_ada` | `08724e9ce0753546709345d5b657eff714748e04` |
| Geometry | `zackboll/opencv_geometry_ada` | `9142e9c3134ded907204ae0304acc28ab602b7a9` |

Reference file availability and SHA-256 audit retained outside Git. Initial
implementation study covered Geometry's generation/GPR/workflow, warning
comparator, coverage reader, validator and comment-only checker. Theme,
navigation, guides, browser and mutation implementations still require full
study/adaptation; archived availability does not mean qualification completed.
Adaptation notices: `gnatdoc-adapted-source-notices.md`.

## Qualified executable observations

| Tool | Actual executable output |
| --- | --- |
| Alire | `alr 2.1.1` |
| GNAT Native | `GNAT 16.1.0` |
| GPRbuild 26.0.1 Alire distribution | `GPRBUILD 26.0.0 (2026-04-15) (x86_64-pc-linux-gnu)` |
| GNATdoc | `GNATdoc 26.0.0 (20260415)` |

Used explicit cached release paths, not the ambient GNATdoc 27.0w installation.
GNATdoc's version command returns status 1 even for this qualified version.
The baseline script verifies executable output and requires an explicit
`GNATDOC_PREFIX`; it does not install or silently substitute tools.
Production manifests, dependencies and compiler settings are unchanged.
The documentation GPR imports only its own private-resolution project, not the
production owner. Source-list generation is required before project loading.
Specification semantic compilation uses `-gnatc`, not code generation.

## Pristine baseline observations

No Ada comments or other Ada source bytes have changed.

- 31 library specifications: **29 public**, 2 explicitly private.
- Excluded units: `AMS.MEL_C_API`, `AMS.MEL.IR.Capability_Conversion`.
- Initial source diagnostic: **1,238 visible declarations** beyond package
  headers: 706 functions, 168 procedures, 318 types, 11 subtypes, 9 exceptions,
  17 objects, and 9 package declarations/renamings.
- Each overload is indexed by full normalized profile, not name alone.
- Actual native output: 29 package pages, 1,237 declaration sections.
- **Unsupported renderer construct:** public package renaming
  `AMS.MEL.IR.Health_Status.Metadata.Status` has no native section.
  It remains in the source inventory; do not fabricate a native section or
  silently count it covered.
- Untouched trailing-style run: **3,924 raw warnings**, all mapped to stable
  scope/signature identities in `gnatdoc-warning-baseline.txt`.
- Before/after raw warnings: **3,924 / 3,924**; no documentation changed.
- GNATdoc emits warnings for private-part declarations even with
  `--generate public`, while excluding their HTML. Current baseline deliberately
  records the raw stream. Public/private warning accounting remains to be
  qualified; do not represent 3,924 as a public-only warning total.
- Coverage, member/discriminant totals, complete profile/rendering matching and
  comment attachment: **not yet qualified**. No coverage baseline is claimed.
- Observed misplaced trailing descriptions in pristine output. Fixes must be
  researched in later phases, not normalized away by the coverage checker.

## Validation evidence

Task-owned logs, pristine site, source hashes, inventory and reference archives
are retained locally outside the repository in the campaign evidence directory.
No machine-local path is inserted into generated HTML by campaign tooling.

- `make test-ada` with GNAT 16.1.0/GPRbuild 26.0.1: **PASS**, exit 0,
  including native builds, mock providers and existing Ada contract suites.
- Warning comparator: **42 unit tests passed**, including every repository
  specification, package renamings and multiline component lists.
- Source-inventory tests: **3 passed**; combined documentation tests: **45 passed**.
- Structured warning comparison: **0 added, 0 removed**.
- Documentation GPR semantic compilation: **PASS**.
- Two fresh baseline generations: **byte-identical site trees** (`diff -r`).
- `git diff --check`: **PASS** at checkpoint validation.
- `make test-native`: **FAIL**, exit 2; CTest reported 208/434 tests failed or
  not run, including missing `test_rf_product_rx` executables despite a
  successful build phase. Root cause not yet established; no production files
  changed to work around it. Preserve the log and investigate build-tree state
  before attributing this to an upstream defect.
- Full `make check` launched separately; result pending at checkpoint.
- Working-tree production integrity: `git diff --exit-code` against starting
  main for `ada`, `native`, `rust`, `python`, `.github` and `Makefile`: **PASS**.
- Generated HTML spot/inventory checks: 29 distinct package headings, excluded
  private package headings absent; no `/home/` paths in generated HTML.
- No source formatting required yet: first-party Ada files are untouched.
- No HTML enhancement, browser, guide/example or hosted CI qualification yet.

## Next action / remaining campaign

DOC-001 is incomplete: finish a conservative source inventory covering enum
literals, fields, discriminants, generics, parameters and returns; qualify
rendering against complete signatures; resolve/report the renaming omission;
separate private warning behavior; build the rendered-coverage baseline and
fail-closed validators/mutation tests; add documentation-only tool installation
and CI. Preserve this pristine capture when changing attachment/style policy.

Then research pinned Common MEL status semantics and begin DOC-002. No package
semantic documentation is complete yet; all 29 public packages remain.
Upstream provenance has been inspected, but no new semantic declaration
documentation or upstream-evidence ledger entries have been authored. Use the
pinned families in `upstream-provenance.md`, not upstream main.

No branch pushed or PR opened at this checkpoint: the required DOC-001 phase
gates are incomplete. Do not label this checkpoint a completed/review-qualified
phase. No PR merged, auto-merge enabled, release tagged, Pages deployment or
feature-branch site publication performed. Later phase branches must stack on
the exact completed/tested predecessor head, with explicit dependencies.

## DOC-001-R1 recovery and qualification (2026-10-14)

Preserved checkpoint `1101865846ba9cf276712df3d36a6d507dd09df9` without amend
or rewrite. Reused `docs/001-gnatdoc-foundation` and its isolated worktree.
Fetch reconfirmed original main `42648a92eb0cc84b2980d16db6028992c0494ba1`;
no concurrent campaign writer or active task-owned native build at recovery.
Unrelated OpenCV processes were observed and left alone. Complete checkpoint
Git diff and recovery environment/process inventory retained under local `r1`.

### Native recovery

The previous `make check` had finished, with native isolation and 434/434 tests
passing; it ultimately failed only for missing final newlines in seven new
documentation files. Those newlines are repaired, not production code.

The original native failure passed tests 1–226, then lost `test_rf_product_rx`
between tests 226 and 227. The remaining 208 were executable lookup/Not Run
failures, not demonstrated contract assertion failures. Current stale tree was
reproduced before any rebuild: all 434 were Not Run, with its cache still ON
but test executables absent. Original temporary CTest logs had already been
removed by subsequent reconfiguration; preserve original full command logs and
state this evidence limitation rather than claiming they were recovered.

The reset mechanism is independently reproduced using copies of the production
configure/selector scripts in a tiny evidence-only project: system PATH uses
`/usr/bin/c++`, while the existing Alire environment uses GNAT Native's `c++`.
The path mismatch triggers `rm -rf` of the selected test tree and deletes a
sentinel executable. A provider-only build then need not recreate every native
test executable. The original mid-CTest disappearance establishes build-tree
interference; the exact deleting process was not traced in the original run.
No production defect or assertion failure is inferred.

A clean external native configuration/build/enumeration/CTest control passed
434/434; canonical `make test-native` then passed 434/434 under a stable system
environment. All subsequent native operations run in one sequential driver;
no documentation generator invokes native builds or production Alire actions.
Caches, compiler identities, test enumeration, executable timestamps, process
inventory, reproduced failure logs and separate control/canonical logs retained.

### Foundation implementation and measurements

- Public packages: 29; private library units excluded: 2.
- Source-visible declarations: 1,238 (874 subprogram profiles: 706 functions,
  168 procedures); native sections: 1,237; verified renderer omission: 1.
- Exact omission `AMS.MEL.IR.Health_Status.Metadata.Status` independently
  reproduced with GNATdoc 26.0.0. Version/scope/path/target/signature must match
  the exception inventory exactly. No fake section or native anchor added.
- Raw warnings unchanged: 3,924. Source classification: 3,851 public, 73
  private-part, 0 private-unit, 0 tool diagnostics, 0 mapping failures.
  Original all-warning baseline remains byte-identical to checkpoint.
- Complete coverage: 4,295 elements, 38 documented, 4,257 undocumented,
  0.8847%. All gaps retained for future phases, not suppressed.
- Members: 587 enum literals; 343 record components; 4 discriminants;
  1,388 parameters; 706 return values, all currently undocumented by the
  strict attached-and-rendered test.
- Nine native long-enum signatures are abbreviated by GNATdoc. Preserve
  native text; validate prefix/suffix and the complete separately rendered
  literal lists. Source identities retain full declarations.
- Early uncommitted coverage was 42/4,295; four falsely credited package-opening
  blocks were disqualified before qualification. This explicit initial-baseline
  correction is retained in failed/passing evidence, not hidden by regeneration.
- Source/member profile validation, literal-aware token checks, observed-anchor
  manifest, warning classifier, exact alias exception, conservative coverage
  ratchet, local HTML/resource/navigation validator and mutation tests added.
- Fresh isolated Alire settings/tool installation and documentation-only crate
  reproduced the workflow generation successfully, without native actions.
- Pages API reports `build_type: workflow`; no settings change needed at this
  observation. No deployment has occurred from this branch.

### Validation status (append final results below)

Original and R1 failed logs are retained. Documentation unit/mutation suite now
has 67 passing tests. The source checker protects 1,162 pre-existing non-doc
files against original main; Ada tokens and every production non-Ada byte are
unchanged. Original warning-baseline bytes are unchanged. Source/Ada formatting
was not modified. Dedicated production checks are being run sequentially; do
not treat the running driver as a completed result.

DOC-002 through DOC-007 remain outside this corrective task. No API prose or
upstream semantic ledger entries added. After independent DOC-001 review, next
phase is DOC-002 foundation/status research against pinned Common MEL.

R1 final documentation refinement: literal-preserving complete coverage identities
now include defaults, aspects and renaming targets; observed native anchors are
unchanged. The original all-warning baseline remains intact, while warning
reports also retain full source-profile tokens. Initial identity refinement
evidence is retained separately; counts did not change. Documentation suite
now has 69 tests. Initial inventory is published in `gnatdoc-initial-inventory.md`.

### Final local qualification completed

All required local gates passed, each with recorded exit 0. Sequential native
commands and timestamps are retained in local `r1/final-commands.txt`:

| Command / gate | Result |
| --- | --- |
| Qualified tool versions and documentation GPR semantic compilation | PASS |
| Two fresh generations using independently installed documentation tools | PASS, byte-identical |
| Documentation unit and mutation suite | PASS, 69 tests |
| Full raw-warning comparator/classification | PASS, 0 added / 0 removed; 3,924 reconciled |
| Coverage/source/native/alias validation | PASS, 38/4,295; 1,238 / 1,237 / 1 |
| Local HTML/assets/navigation/fragment validation | PASS, 31 HTML pages, 1,307 links |
| `make test-ada` | PASS, exit 0 |
| `make test-native` | PASS, 434/434, exit 0 |
| `make check` | PASS, exit 0, includes isolation/native/format/Ada/newlines/whitespace |
| `make check-ada-format` | PASS, exit 0 |
| Literal-aware production-source integrity | PASS, 1,162 protected pre-existing files |
| Git whitespace/final-newline checks | PASS |

The final documentation refinements were requalified with fresh generation,
byte comparison and all 69 tests (`r1/final-doc-v3*`). No production or Ada
source bytes changed. Some GPR formatting checks emit inherited no-C-sources
warnings for the external native import; exit status is 0, and these are not
GNATdoc warnings. No warning/test suppression added.

Disposition at corrective commit: DOC_001_LOCAL_QUALIFIED. Push/PR/hosted state
will be recorded after the exact corrective head exists. No merge authorized.

### Independent-review submission

- Corrective implementation commit: `52b0c56d433e0ef79f4362c74dff04e138029a78`.
- PR: https://github.com/zackboll/ams-gra-mel-bridge/pull/101
- Base: main `42648a92eb0cc84b2980d16db6028992c0494ba1`.
- Branch: `docs/001-gnatdoc-foundation`; non-draft, OPEN, auto-merge null/disabled.
- Local corrective head = fetched remote branch = PR head was verified exactly.
- Initial documentation PR run: `38656319522` (queued at observation), exact
  head `52b0c56d433e0ef79f4362c74dff04e138029a78`; workflow explicitly checks
  out PR head rather than its synthetic merge. Bootstrap PR run `38656319485`
  and push run `38656292274` were queued/running, not reported passed.
- This journal-only follow-up is a normal descendant commit, not an amendment;
  its exact SHA and updated run state are available in Git/PR and local evidence.
- Disposition: DOC_001_HOSTED_CI_PENDING until fresh hosted checks complete.
- No merge, auto-merge, tag, release or feature-branch Pages deployment performed.

All 29 published package names and detailed kind/member counts are in
`gnatdoc-initial-inventory.md`. DOC-002–007 remain unstarted/out of scope here.

## DOC-001-R2 — tracked production additions

Fetched and verified reviewed PR head
`077cc18892b6c4fcccaa7ccc132e9a7993809a3a` against the local and remote branch;
GitHub confirmed OPEN, non-draft PR #101 with auto-merge disabled. Current main
remained `42648a92eb0cc84b2980d16db6028992c0494ba1`, also confirmed by GitHub.
Existing branch, commits, worktree and earlier evidence were preserved.

The checker now compares complete NUL-delimited tracked inventories and rejects
all additions outside `docs/` and the explicit DOC-001 file authorization set.
The set lists the existing documentation projects, tooling/tests and workflow,
plus the new integrity test; no broad scripts/workflow allowance was added.
Existing production-byte and Ada-token/inventory checks are retained.

Negative-first fixtures were committed in temporary repositories with clean
staged/unstaged diffs and status. Before the correction, nine non-Ada additions
escaped detection; four Ada additions were rejected by the older Ada inventory
check rather than the requested complete inventory diagnostic. After correction,
all 13 cases reject: native C++, Rust, Python, Ada specs/bodies in both source
directories, integration, unexpected workflow, executable unrelated script,
unrelated top-level text/GPR, and a newline-containing native filename.
Documentation additions and existing byte/token/removal regressions pass.

Actual local qualification on 2026-10-14 (all final gates exit 0):

| Gate | Result |
| --- | --- |
| Source-integrity fixture suite | PASS, 6 tests with 13 rejection subcases |
| Documentation unit/mutation suite | PASS, 75 tests; repeated on final sources |
| Qualified GNATdoc generation and semantic compilation | PASS, two fresh generations |
| Deterministic regeneration | PASS, recursive byte comparison |
| Warning/coverage ratchets and baseline delta | PASS, unchanged; 3,924 warnings, 38/4,295 coverage |
| `make check`, sequential | PASS, native 434/434, isolation, formatting and Ada contracts |
| `make check-ada-format`, after `make check` | PASS |
| Production/vendor integrity | PASS, 1,162 protected files; no production-path changes against base |
| Production workflow integrity | PASS, `.github/workflows/ci.yml` unchanged against base |
| Git whitespace and final-newline checks | PASS |

An initial final-newline check caught the new test's missing newline; it was
corrected before final qualification. No production, vendor, Ada source, CI,
warning/coverage baseline or renderer-exception changes. Local logs and negative-
first evidence are retained at `/tmp/ams-mel-doc-r2-evidence/`; prior campaign
evidence remains untouched. The corrective SHA is discoverable in this file's
Git history. Hosted qualification must use that new head, not earlier passing CI;
publication/head equality and fresh run status are recorded in the PR follow-up.
DOC-002–007 remain outside scope. No merge or auto-merge authorized.

## DOC-002 — root-foundation documentation checkpoint

DOC-002 starts from verified main
`85b5437e39de1387f3f1678d6f09a9f85e16cfac`, the merge of PR #101 and
reviewed DOC-001 head `3b9dffa542d1133d2d6d40be8b152354607baac1`.
The trees match exactly. Branch `docs/002-ada-foundations-status` was created
in the requested isolated worktree without altering original or unrelated
worktrees. Main documentation run `38665457826` and bootstrap run
`38665457895` both subsequently completed successfully on that main SHA.

Pristine qualified generation reproduced all DOC-001 counts, including
38/4,295 coverage, 3,924 raw warnings, 3,851 public and 73 private-part warnings.
All 75 documentation tests passed before edits. The historical inventory above
is retained unchanged. Local logs, original site/reports/baselines and the
attachment fixture are retained in the requested external disk evidence area.

The temporary trailing-style fixture qualified package-opening blank lines,
ordinary/multiline declarations, overloads, parameters/returns, enum literals,
record fields/discriminants, exceptions, representation clauses and aspects.
Pinned upstream commit objects/remotes were verified for Common, IR, RF, Math
and Squall. Vendored headers in the four library families were byte-compared
against their pinned Git objects without modifying either checkout.

Root descriptions now cover AMS 1/1 and AMS.MEL 45/45. Independent identity
comparison proved exactly 43 fixed gaps and 37 removed public warnings, with
no new gaps/warnings and unchanged other-package coverage/private identities.
Repository checkpoint: 81/4,295 documented; 4,214 gaps; 3,887 raw warnings =
3,814 public + 73 private-part. Baselines remove only those proven identities.
All native anchors and the exact package-renaming exception are preserved.

Evidence findings: IR Control has no separate shutdown operation; Session
closure releases shared ownership and is not global cancellation/completion.
Open rejects embedded NUL before loading but does not validate UTF-8; version
text is validated. Version data is copied to Ada ownership. Admission refusal
is bridge-defined and occurs before provider send. Detailed mappings are in
`gnatdoc-upstream-evidence.json` and `doc-002-source-research.md`.

Documentation gates passed: required formatting and format-check, two fresh
byte-identical generations, strict warning/coverage ratchets, exact native
section/anchor and local-link checks, 75 documentation tests, baseline-delta
check, and literal-aware integrity against verified main (1,178 files).
An underscore-normalization mismatch in the existing coverage validator was
avoided by rephrasing one summary; no validator criteria changed.

Native then Ada qualification has been launched sequentially; results remain
pending until exit files and complete logs are inspected. Status/IR/RF work,
full sequential make check, final reconciliation, commit and PR are pending.
This is a documentation-tested partial checkpoint, not completed DOC-002.

### DOC-002 Status BIT/fault subcheckpoint

Source base remains verified main above; current documentation-only edits are
uncommitted. Status has 231/531 documented elements after BIT value/list,
constructor/accessor and fault-family work. Remaining Status gaps: 300.
Repository coverage: 312/4,295, 3,983 gaps. Warnings: 3,655 raw = 3,582 public
+ 73 private-part. Against the root checkpoint, independently proved removals
are 231 gaps and 232 warnings; no new identities and all other-package gaps,
private warnings, native anchors and renderer exception unchanged.

Pinned findings include the unresolved BIT interface values two/three prose
conflict, progress fraction 1.0 = 100% with 1.25 preserved in tests, UTC-tagged
nanosecond counts without an established epoch, single initial BIT name versus
append behavior, independent fault severity/state and non-positional ambiguity
group test references. Evidence ledger and research notes have been extended.

Attachment qualification was refined: GNATdoc renders before/after separate
representation clauses, but this repository's source checker requires these
enum blocks immediately after the type semicolon. Corrected placement passes
without changing tooling. Required formatting/check, strict generation twice,
byte-identical output, 75 documentation tests, baseline delta and source
integrity pass. Complete raw logs and exact removed identities are external.

The earlier sequential native/Ada command completed: 434/434 native tests and
safe Ada contracts passed. Status comments changed during that command, so it
is not final exact-source qualification of this subcheckpoint. Remaining MFA,
component and security families, both IR/RF roots and full final gates remain.
No commit, push, PR, tag, release or merge has been performed for DOC-002.

### DOC-002 state and BIT-status subcheckpoint

Status now covers 296/531 elements after overall MFA, transition, component
and thermal enum descriptions and BIT-status snapshot constructors/accessors.
The independent incremental delta is 65 fixed gaps and 65 removed public
warnings. Current repository: 377/4,295 documented (8.7776%), 3,918 gaps;
3,590 raw warnings = 3,517 public + 73 private-part. Target coverage: 343/900,
557 remaining target gaps. No other-package coverage, private identity,
native anchor or exact renderer-exception changes.

MFA_State's pinned introductory prose incorrectly says transmit-only is
excluded although the declaration includes it. Documentation follows the
actual declaration without promising provider support. Component capability
state, overall state, transition progress and thermal reporting are distinct;
Ada storage does not execute a transition or calculate thermal thresholds.

All 75 documentation tests, two strict deterministic generations, format/check,
baseline-delta and literal-aware integrity pass. Fresh native then Ada then
make check then format-check commands are running sequentially on these exact
Ada comments; do not credit completion until logs/exit files are inspected.
The evidence directory's `partial-final-*` logs identify this run. Existing
earlier passing native/Ada logs are preserved separately. Remaining Status
component/about/security/name-value descriptions and IR/RF roots are untouched.
No DOC-002 commit/push/PR has occurred; full qualification is not established.

### DOC-002 completed Status documentation checkpoint

All 531/531 Status elements now pass native rendering and source attachment;
AMS 1/1 and AMS.MEL 45/45 remain complete. IR 0/106 and RF 1/217 are unchanged.
Target coverage is 578/900, with 322 gaps left. Repository coverage is
612/4,295 (14.2491%), with 3,683 gaps. Raw warnings are 3,356 = 3,283 public
+ 73 private-part. The last increment independently removed 235 gaps and
234 public warnings; total campaign removals are 574 gaps and 568 warnings.
No new identities, other-package coverage changes, private warning changes,
anchor changes or renderer-exception changes were accepted.

Geometry units, component/state distinctions, provider-defined name/value
metrics and each concrete security subtype were researched and documented.
Security audit nanoseconds do not imply UTC or an epoch; authentication and
integrity/file subtype references differ. Constructors copy without enforcing
provider semantic consistency. The evidence ledger and research notes preserve
these findings and the unresolved BIT interface-name/prose discrepancy.

Formatting/check and two fresh strict deterministic generations pass, as do
all 75 documentation tests, exact native sections/anchors, local links,
baseline delta, literal-aware integrity and whitespace. A package-summary
underscore normalization mismatch was resolved by rephrasing, not tool edits.
The earlier sequential native (434/434), Ada, make check and final format-check
all completed with exit zero. Since comments changed while that series ran,
a fresh final-Status Ada run is in progress; inspect status-complete-test-ada
logs/exit before crediting it. IR/RF and full final reconciliation remain.
No commit, push or PR has been performed; DOC-002 is not fully qualified.

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

## DOC-002-RF — resumed and documentation-complete

Resumed existing uncommitted branch/worktree without reset, stash or replacement.
Fetched main still equals original base
`85b5437e39de1387f3f1678d6f09a9f85e16cfac`. Prior IR Ada exit file is zero
and its complete log ends in the safe Ada contract pass; no task-owned validation
process remained. Pre-RF strict generation reproduced 718/4,295 and 3,252 raw
warnings exactly. Existing four completed packages were preserved.

RF now covers 217/217, yielding 900/900 targets and 934/4,295 repository-wide
(21.7462%), 3,361 gaps. Raw warnings: 3,036 = 2,963 public + 73 private-part.
Independent RF delta is 216 fixed gaps and 216 removed public warnings; total
DOC-002 delta is 896 gaps and 888 warnings. All source identities, other-package
gaps, private warning identities, native anchors and exact renderer exception
remain unchanged. Only proven removals were applied to structured baselines.

RF ledger now covers lifecycle/version/quantization, physical installation,
format identifiers, face/MFA snapshots and transmit mode characteristics.
Pinned vendor bytes were reverified (RF 57 headers, Common 28, Math 5); Squall
commit identity confirmed. Key distinctions: no universal quantization rounding;
RF Open rejects NUL but does not validate UTF-8; ComplexINT16 uses int16 pairs
rather than its incorrect eight-bit prose; packet capacity is not serialized
length; PDW/LF identifiers do not add receive support; signed femtoseconds differ
from nanosecond pulse width; provider mode IDs differ from power levels and
returned IDs remain authoritative. Hardware-damage limits are reported, not
future-Job validation. Attenuation units remain unspecified. BIT interface
values two/three remain explicitly ambiguous. No official IDD review claimed.

Final format/check and strict generation twice passed; byte-identical sites,
75 documentation tests, 31 HTML pages/1,307 links, 1,238 source/1,237 native
sections, source integrity against original main (1,178 protected files) and
baseline delta pass. Full sequential native/Ada/check/format qualification is
running under the stable documentation environment; completion will be recorded
only after exit files/logs are inspected. Native test suite has already finished
434/434 with exit zero. No DOC-002 commit/push/PR yet.

A full ledger-reference audit also caught two upper line bounds beyond the
112-line pinned UCI_ID header; these citation bounds were corrected to 112.
No API prose or production semantics were changed by that evidence correction.

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

## DOC-003A — full-frame checkpoint

Completed full-frame data model and owned-copy path documentation. Image is
160/351; repository 1,093/4,295. Removed 159 measured gaps and 130 public warnings;
no new identities. Private warnings (73), all other packages, native inventory,
anchors and renderer exception are unchanged. Format/check, strict generation
twice, byte determinism, HTML links, 75 documentation tests and base-relative
literal-aware integrity passed. Lease/navigation and the other five packages,
full native/Ada qualification and PR submission remain pending.

## DOC-003B through DOC-003H — documentation qualification

All renderable target elements are documented: Image 351/351, Image Metadata
77/77, Health 25/25, Health Metadata 142/143, Instrumentation 76/76 and its Metadata
22/22. Total 693/694, with the exact existing source-visible, unrendered Status
renaming still outstanding. Repository coverage is 1,625/4,295 (37.8347%), 2,670
gaps; warnings 2,399 = 2,326 public + 73 unchanged private-part.

B lease checkpoint measured Image 260/351 and repository 1,193; C navigation
completed Image and measured 1,284; D completed image metadata at 1,360; E health
at 1,385; F health metadata at 1,527; G completed instrumentation at 1,625.
Every checkpoint passed format/check, strict generation twice, byte determinism,
HTML links and all 75 documentation tests. Independent final identity audit
proves 691 gaps and 637 public warnings removed, zero additions; other 23 packages,
all private warning identities, native sections/signatures/anchors and renderer
exception are unchanged. Temporary authoring/attachment failures were diagnosed
and retained externally; no tool or contract was weakened.

Grouped source research covers all declaration families against the exact pinned
IR/Common/Math headers, safe Ada/native implementations, tests and corrective
history. Original upstream remotes/revisions and 101 vendored header bytes were
verified; pinned Squall sources were extracted by exact commit. Unsupported
semantic inference (units, epochs, bit meanings, numeric scales) is explicitly
avoided. Documentation highlights retained provider-buffer ownership, bounded
queues, failure-safe retention, preconditions versus body checks, timeout versus
cancellation and weak common views. DOC-007 site enhancements remain untouched.

Final formatted-source docs gates passed twice, 75 documentation tests, six
integrity tests, HTML links, baseline-delta and literal-aware integrity against
both task base and DOC-001 boundary. Full native/Ada/check validation is running
sequentially under a stable environment; only native has completed so far,
434/434 tests passing. No final commit or PR exists yet.

DOC-002 post-merge Bootstrap run 38673364892 has now completed **failure**, not
success: the clang Release repeated native gate failed rf-job-interval-status
(test 55). Full failed-job logs are preserved externally. The post-merge docs
run 38673364878 passed on the exact base. The inherited hosted stress failure
is not hidden or treated as a documentation change; final feature-branch CI
will be reported separately.

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

## DOC-004 reconciliation and 004A — capability model checkpoint

Task base is fetched main `61ac2aeb5f6ed1be70a853015861d5decb352dbd`, the
PR #103 merge. No intervening commits or conflicting DOC-004 branch/worktree/PR
were found. Existing worktrees remain untouched. Worktree and evidence live at
`/home/zboll/git/ams-mel-doc-004` and
`/home/zboll/git/ams-mel-doc-004-evidence`; branch is
`docs/004-ir-c2-channel-track`.

The qualified unchanged toolchain reproduced the complete DOC-003 baseline:
1,625/4,295 documented, 2,670 gaps, 2,399 raw warnings (2,326 public, 73 private),
1,238 source / 1,237 native declarations and one unchanged renderer omission.
Post-merge documentation run 38678008854 passed on the exact base; Bootstrap
38678008773 was in progress at initial inspection and is not credited as passed.

004A completes Channel capability model comments: Channel now measures 202/289,
with 201 new documented identities. Other packages remain unchanged. Repository
coverage is 1,826/4,295, with 2,469 gaps. Warnings are 2,194 raw = 2,121 public
+ 73 unchanged private-part. Exactly 205 public warnings were removed and zero
warning/gap additions accepted. The target set is 203/1,274, not complete.

All 101 vendored IR/Common/Math headers match exact pinned upstream bytes.
Research and grouped evidence distinguish reports from implemented operations,
metadata advertisement from registration, map keys from vector positions,
provider set/map iteration from copied vectors, vacuum wavelengths in meters,
independent size scalars and provider ODC/NUC reports from bridge algorithms.

Format/check, strict qualified generation twice, byte-identical output, HTML
structure/links, all 75 documentation tests, native profiles/anchors, removal-only
ratchets and task-base literal-aware integrity pass. External evidence retains
initial pristine-site test ordering failure, stale-baseline failures and
attachment corrections. No scripts, tests or contracts were weakened.

004B–004J, complete native/Ada/canonical-check qualification and publication are
pending. No commit, push or DOC-004 PR has been made. This is a resumable partial
checkpoint, not completion of the requested campaign.

### Starting-main hosted follow-up

Bootstrap run 38678008773 subsequently completed successfully on exact base
`61ac2aeb5f6ed1be70a853015861d5decb352dbd`. This completed observation
supersedes the initial in-progress observation; it is not DOC-004 branch CI.

## 004B — common View and requests qualified

Resumed existing worktree without resetting, stashing or recreating the branch.
Main and HEAD remain `61ac2aeb5f6ed1be70a853015861d5decb352dbd`; original
ancestry verified, no running task validation found. The seven-file checkpoint
was preserved externally in `r1/resume.patch` and copies of untracked reports.
Qualified reproduction exactly matched 004A before edits.

Channel is now 289/289. Relative to 004A, 87 gaps and 77 public warnings were
removed, zero added. Repository: 1,913/4,295 documented, 2,382 gaps; warnings
2,117 raw = 2,044 public + 73 unchanged private-part. All other packages, native
profiles/anchors and renderer exception are unchanged. Five existing 004A enum
prose attachments temporarily became uncredited when new identifiers changed
native cross-reference rendering; equivalent prose without underscored names
restored them. No 004A coverage identity was removed.

Source research: Channel body, native common request engine and private access/
claim definitions, family lifecycle adapters and tasks 032B1/032B2. The existing
five-family common tests cover attachment/enabling, expired views, independent
request lifetime, repeated cached waits, rejection and Return::Fail. The native
submission path acquires a Session permit and family claim before unlocked send.
A weak handle does not establish resource existence or lifecycle eligibility.
Admitted work retains resources independently. Closing an observer is not
cancellation or direct pending-permit release. Accessor preconditions are not
independently checked by expression-function bodies. Outcome Success can carry
Fail. Physical execution or metadata correlation is not established by Wait.

Required format/check, strict generation twice, byte-identical site, HTML links,
75 documentation tests, six integrity tests, task-base literal integrity, Git
whitespace and final newlines passed. Evidence and failed intermediate runs:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004b-*`. Next: 004C C2.
Final production qualification and publication remain pending.

## 004C — AMS.MEL.IR.C2 qualified

Package coverage 179/179; 178 fixed gap identities and 165 public warnings removed, zero additions. Repository 2091/4,295; 2204 gaps. Raw/public/private warnings 1952/1879/73. Other packages, private warnings, profiles/anchors and renderer exception unchanged. Format/check, two strict byte-identical generations, HTML/links, 75 documentation tests, six integrity tests, task-base token integrity and whitespace/newlines pass. Evidence: `/home/zboll/git/ams-mel-doc-004-evidence/r1/004c-*`.

Mode/BIT/config typed sends require Enabled; shared C2 state remains distinct from the Status Ada type. Scan controls remain unsigned scalars. Validation checks representation domains, not physical feasibility. Config/fault strings copy full UTF-8 spans including NUL. Config nanoseconds establish no epoch. BIT-specific functions populate only one list; nonempty preconditions and assertion-disabled body behavior are documented. Native claim precedes unlocked send; callers still serialize Ada owner operations. Deferred cleanup retains pending provider work. Source: C2 spec/body, native ir_c2.cpp, pinned C2Channel/ModeCmd/BIT_Command/ConfigSetCommand/ScanParam/ScanType/CommonIR_MEL/CommonMEL headers, C2 Ada/native tests and corrective unlocked-submission report. A tool-wrapper timeout interrupted dedicated integrity tests after deterministic generation; the dedicated suite was rerun successfully before credit. Next: 004D compatibility.

## 004D — AMS.MEL.IR.C2.Common qualified

Package coverage 33/33; 33 fixed gap identities and 33 public warnings removed, zero additions. Repository 2124/4,295; 2171 gaps. Raw/public/private warnings 1919/1846/73. Other packages, private warnings, profiles/anchors and renderer exception unchanged. Format/check, two strict byte-identical generations, HTML/links, 75 documentation tests, six integrity tests, task-base token integrity and whitespace/newlines pass. Evidence: `/home/zboll/git/ams-mel-doc-004-evidence/r1/004d-*`.

Legacy direct typed-C2 wrappers share native request engines/accounting and common payload types, not canonical Ada request owners. Legacy diagnostic helper substitutes fixed text for an empty diagnostic and starts with a larger buffer; cached retry preserves long diagnostics. Preconditions lack independent body checks. Source: C2.Common body, native ir_c2.cpp and common_channel_requests.cpp, pinned Channel.h and comms declarations, legacy Ada tests and public-common native comparison cases. Next: 004E metadata. Final production qualification and publication pending.

## 004E — C2 metadata qualified

All 288/288 elements documented. Exactly 288 gaps and 288 public warnings
removed, zero additions. Repository 2,412/4,295 documented, 1,883 gaps; warnings
1,631 raw = 1,558 public + 73 unchanged private. Other package identities,
native profiles/anchors and renderer omission unchanged. Two strict generations
are byte-identical, all 75 documentation and six integrity tests pass, required
format/check, HTML links, source integrity and text gates pass. Evidence:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004e-*`.

Research: public spec/body; native ir_c2.cpp registration/copy/queue/cleanup;
pinned CommandStatus/CommonIR_MEL/Channel/C2Channel and Common MEL
BIT_Configuration/ActiveBIT/CompletedBIT/Fault; Ada C2 metadata tests, native
C2 metadata/callback tests and task-018 corrective validation. Command metadata
is independent of future Wait. Base registration is one-shot, ordered BIT
configuration/command status/BIT status, with no capability gate. Comms is a
separate one-shot attempt valid Attached/Enabled and not automatically opened.
Failure cannot undo upstream registrations; callbacks retain inactive/failed
state. Close is not unregister or quiescence. All kinds share bounded incoming-
drop FIFO by successful enqueue. Malformed values are nonterminal drops; callback
exceptions fail the queue. Queued events drain before stopped/failed reporting.
Accessor preconditions have no independent body checks. Counters saturate.
Every Cannot_Comply value retains numeric mapping; vague source categories add
no invented policy. BIT-interface values two/three retain the DOC-002 name/prose
ambiguity. Active completion 1.0 means 100%, with values above one permitted.
Active/completed BIT timestamps are labeled UTC upstream without a more specific
epoch; fault detection chrono alone establishes no epoch. Next: 004F Track.
Final production qualification and publication remain pending.

## 004F — Track foundation qualified

Track 55/55, with 55 gap and 49 public-warning removals, zero additions.
Repository 2,467/4,295 documented, 1,828 gaps; warnings 1,582 raw = 1,509 public
+ 73 unchanged private. Other package identities, anchors/profiles and renderer
exception unchanged. Format/check, two strict byte-identical generations, HTML
links, all 75 documentation and six integrity tests, literal-aware integrity and
text checks pass. Evidence: `r1/004f-*` under the external evidence directory.

Pinned IRSTTrackReport establishes meters for range/error, radians for spatial
extent, nanosecond age since first detection and quality zero-to-one (not
enforced). NED components, intensity/SNR and clutter lack stronger source units
or scales. The filtered NED field's adjacent comment also says latest measurement;
no algorithm is invented. Track owner retention and deferred physical cleanup
include common requests, updates and system responses; provider destruction
then callback drain is the quiescence boundary, not disable return alone.

Sources: Track spec/body; native ir_track.cpp lifecycle/copy/common adapter; pinned
TrackChannel/IRSTTrackReport/CommonIR_MEL and Common Directional; native/Ada Track
foundation/report tests; tasks 029B1/B2/B3. Exact pinned SquallIRLoaders.cc was
extracted outside Git: attachChannel has Image/C2/Health cases only, then null.
Thus positive Track behavior remains mock-only; Squall qualification is expected
negative. Next: 004G Track metadata. Production final gates and publication pending.

## 004G — Track metadata qualified

All 153/153 elements documented. Removed 153 gap and 152 public warning
identities, zero additions. Repository 2,620/4,295; 1,675 gaps. Warnings 1,430
raw = 1,357 public + 73 unchanged private. Other package coverage and private
warning identities, native anchors/profiles and renderer exception unchanged.
Format/check, two strict byte-identical generations, HTML/links, all 75 docs
tests and six integrity tests, literal integrity, text checks pass. Evidence:
`/home/zboll/git/ams-mel-doc-004-evidence/r1/004g-*`.

All four families are inbound callbacks. Report registration is conditionally
required; candidate registration is gated by advertisement and must succeed
when advertised; system-data-request and PreProc callbacks tolerate optional
NotSupported. Registration order and one-shot partial-failure retention are
verified in native ir_track.cpp; callbacks run outside the Track lifecycle lock.
Queue order means successful enqueue, not timestamp order. Report-only Receive
consumes a wrong-kind event and raises rather than skipping/requeueing it. Native
events are disposed on Ada conversion failure. Counters saturate; malformed
events drop nonterminally while callback exceptions fail the stream.

Candidate message capacity 900 has a meaningful declared prefix; PreProc vector
length is independent of header count. Binary32 CFAR is average false alarms
per pixel. Header stack index and hot-region pixel bounds are explicitly zero-
based upstream; candidate subpixel origin and edge inclusivity are not inferred.
Navigation quaternions/position/velocity retain their defined frames and units;
uncertainty/validity bits, SIR/SNR scales and sigma units are not invented. Signed
3x3 background samples, vector order/duplicates and per-entry navigation survive.
Sources: Track metadata spec/body, native copy/queue/registration, pinned
TrackChannel and all candidate/header/PreProc/hot-region/inertial/request headers;
native/Ada Track metadata tests; tasks 029E/F/G. Mock positive only; pinned Squall
negative attach boundary unchanged. Next: 004H optional outbound System_Data.
Final production qualification and publication pending.

## 004H — System_Data qualified

System_Data 124/124; removed 124 gaps and 120 public warnings, zero additions.
Repository 2,744/4,295 documented, 1,551 gaps. Warnings 1,310 raw = 1,237 public
+ 73 unchanged private. Native profiles/anchors, renderer exception and other
package coverage unchanged. Format/check, two strict byte-identical sites, HTML
links, 75 documentation tests, six integrity tests, literal integrity and text
checks pass. Evidence: external `r1/004h-*`.

Optional outbound response is distinct from inbound request metadata. Enabled-
only send copies every field, including values marked invalid, without zeroing
or normalization. Time is signed nanoseconds with no epoch; ranges/errors are
meters, rates/errors meters/second and AzEl/errors radians. Future Success can
carry CommandStatus whose command state is Rejected; only ErrorOr gives a rejected
future outcome. Preconditions lack independent body checks. Explicit Ada request
Close ignores native close status and is not cancellation. Shared Track request
accounting defers disable/detach until all update/response/common work finishes;
failed deferred cleanup can replace a terminal outcome with provider failure
and safely retain the graph. Sources: System_Data spec/body, native ir_track.cpp
build_response/submission/complete/cleanup, pinned SystemTrackDataResponse,
TrackChannel, CommandStatus/CommonIR_MEL and Math RangeAzEl; task 029D, native
test_ir_track_system_response.c and Ada system-data tests. Positive mock only.
Next: C2 NUL correction requalification, then 004I updates. Final gates pending.

### Semantic correction discovered during cross-family review

Earlier 004C notes/commentary incorrectly described embedded NUL as preserved
in configuration/fault text. Native ir_c2.cpp valid_utf8 explicitly rejects
lead byte zero; valid_view and string-span validation use it before copying.
The actual contract is length-delimited UTF-8 without embedded NUL. Two Ada
comments are corrected and strict requalification is running. Earlier historical
observations are retained but superseded by this explicit correction. No tokens,
validation rules or production behavior change.

## 004I — Track updates qualified; 004J reconciliation started

Track.Updates 153/153, removing 153 gaps and 149 public warnings, zero additions.
All eight targets now total 1,274/1,274; repository 2,897/4,295 (67.4505%),
1,398 gaps. Warnings measured as 1,161 raw = 1,088 public + 73 private.
Other packages/private identities and native profiles/anchors/renderer exception
unchanged. Format/check, two strict byte-identical generations, HTML links, all
75 documentation tests, six integrity tests, token integrity and text gates
pass. Exact evidence: external `r1/004i-*`.

Updates use the track-update condition, not the basic Track condition. All 21
Track covariance fields are documented individually, distinct from eighteen
Image navigation terms; no packed-matrix transform or estimator is implied.
Pinned times are double epoch seconds, not nanoseconds and without a more
specific epoch. Maneuver probability source range is zero-to-one and update
quality zero-to-fifteen; setters/bridge do not enforce these. ECEF frame is
established but stronger component units are not supplied by the pinned fields.
All identifiers and labels copy before return; labels must be UTF-8 without NUL.
Future success carrying a Rejected command state differs from ErrorOr rejection.
Timeout/observer closure are not cancellation. Pending update/response/common
claims share Track lifetime and final cleanup; failed deferred detach can replace
a result with provider failure and permanently retain unsafe-to-release resources.
Sources: Updates spec/body, native ir_track.cpp build_update/complete/accounting/
cleanup; pinned TrackDataUpdate/TrackStatus/TrackChannel/CommandStatus and
Common Directional; task 029C, native update/system-response mixed lifetime tests
and Ada update tests. Positive behavior is mock-only.

C2 embedded-NUL correction requalification passed with zero identity deltas,
two identical sites, 75+6 tests and all documentation/text/integrity gates
(`r1/004c-nul-correction-*`). During independent 004J review, C2 Close prose was
also narrowed: detach failure retains a retryable handle, but disable failure
after successful detach can consume it while raising. Final qualification will
use this corrected final source. No production tests have yet been credited.

## 004J — final documentation reconciliation

| Package | Starting main | Final | Newly documented |
| --- | ---: | ---: | ---: |
| AMS.MEL.IR.Channel | 1/289 | 289/289 | 288 |
| AMS.MEL.IR.C2 | 1/179 | 179/179 | 178 |
| AMS.MEL.IR.C2.Common | 0/33 | 33/33 | 33 |
| AMS.MEL.IR.C2.Metadata | 0/288 | 288/288 | 288 |
| AMS.MEL.IR.Track | 0/55 | 55/55 | 55 |
| AMS.MEL.IR.Track.Metadata | 0/153 | 153/153 | 153 |
| AMS.MEL.IR.Track.System_Data | 0/124 | 124/124 | 124 |
| AMS.MEL.IR.Track.Updates | 0/153 | 153/153 | 153 |

Repository 2,897/4,295 documented (67.4505%), 1,398 gaps. Independent full
identity comparison removes 1,272 gaps since main and 1,071 since 004A; zero
added gaps or changes to untouched-package gaps. Measured warnings: 1,161 raw,
1,088 public and 73 private-part, zero tool/unclassified/private-unit warnings.
Exactly 1,238 public warning identities removed since main, 1,033 since 004A;
zero added warning identities and all private identities unchanged.

All three 47-literal Cannot_Comply enums have verified explicit 0..46 mapping.
Native inventory remains 1,238 source / 1,237 native sections with identical
observed profiles/anchors and the single unchanged Health metadata omission.
All historical ledger entries are preserved and new referenced test/report paths
exist. Complete change scope is exactly fourteen authorized files: eight Ada
spec comments/formatter whitespace and six documentation artifacts. No body,
production/vendor byte, test, configuration, tooling, workflow, anchor or exception
file changes. Literal-aware integrity passes at both main (1,178 protected files)
and DOC-001 (1,162 protected files).

Final formatted-source documentation qualification passes required format/check,
strict generation twice, every site byte identical, HTML 31 pages / 1,307 links,
75 documentation tests and six dedicated integrity tests, baseline removal-only
gates, whitespace and final newlines. C2 NUL and cleanup wording corrections
are included in this final source. Logs: external `r1/004j-*`; independent
reconciliation: `r1/004j-independent-reconciliation.json` and
`r1/004j-warning-reconciliation.json`.

Sequential final native/Ada/canonical-check qualification is running under the
saved stable compiler environment. No production gate is credited until its
exit file and log have been inspected. The external runner initially used
unsupported POSIX-shell brace expansion and stopped before any build; that
runner-only failure is preserved and corrected with explicit paths. No test
was suppressed or changed. Publication remains gated on all final local gates.

## Final local qualification — complete

Every required local gate passed on the final formatted eight specifications.
Recorded SHA-256 checks after the sequential production gates match all eight
submitted Ada files. Final source includes both C2 semantic corrections.

| Gate | Actual result |
| --- | --- |
| Required format and final format check | PASS |
| Strict qualified GNATdoc/semantic compilation and identity ratchets | PASS |
| Eight target packages | 1,274/1,274 documented |
| Repository coverage | 2,897/4,295; 1,398 gaps |
| Raw/public/private warnings | 1,161 / 1,088 / 73 |
| Gap/warning additions | Zero / zero |
| Native profiles/anchors | 1,238 source / 1,237 native, unchanged |
| Known renderer omission | Exactly one unchanged Health alias |
| Two fresh site generations, every output byte | PASS, identical |
| HTML structure/local links | PASS, 31 pages / 1,307 links |
| Documentation unit/mutation suite | PASS, 75 tests |
| Dedicated integrity suite | PASS, six tests |
| Literal-aware main/DOC-001 integrity | PASS, 1,178 / 1,162 protected files |
| make test-native | PASS, 434/434 |
| make test-ada | PASS, full contracts and focused repeats |
| Sequential canonical make check | PASS, isolation/native/format/Ada/text gates |
| Final make check-ada-format after make check | PASS |
| Whitespace/final newlines | PASS |

Exact production logs and inspected exit files: external `r1/final-test-native`,
`r1/final-test-ada`, `r1/final-check`, `r1/final-check-ada-format`; final source
hashes in `r1/final-tested-ada.sha256` and verification log. Native commands never
overlapped. No inherited rf-job-interval-status instability occurred in these
final sequences, and no tests/contracts were weakened. No real-provider or
hardware execution is claimed. Main refetched unchanged at the original base.

Exactly fourteen authorized files changed: eight specifications' comments/
formatter whitespace and six documentation artifacts. Complete production token/
byte inventory, prior ledger records, historical DOC-001–003 reports, native
anchor/renderer manifests, tooling and workflows remain unchanged.

Local qualification is complete. Commit/publication and hosted exact-head CI
are separate subsequent observations, not inferred here. Independent review
remains required; no merge, release, tag or feature-branch Pages deployment.

## DOC-005A — recovered Admin-only scope, final local qualification complete

Preserved the original DOC-005 worktree, seven-file checkpoint and all `r1`
evidence. Fetched main remains `d4a4e262eb1404c963c509cf1e5d32f0c6712b99`.
Created `docs/005a-rf-admin` non-destructively; original branch reference preserved.
RF C2 is unchanged and deferred. The earlier incomplete-task checkpoint above
is history, not the current Admin-only qualification status.

Admin remains 15/15; repository 2,912/4,295, 1,383 gaps; warnings 1,146 raw /
1,073 public / 73 private-part. Exactly 15 gap and 15 public-warning identities
removed; zero additions; full untouched-package and private-warning identities,
native profiles/anchors and Health renderer exception unchanged.

Fresh toolchain/provenance verification, semantic compilation, two byte-identical
sites, HTML 31 pages/1,307 links, 75 documentation tests, six integrity tests,
production/vendor byte inventories and formatting/text checks passed. Sequential
make test-native (434/434), make test-ada, make check and final
make check-ada-format all passed. Final Admin SHA-256 matches the tested source:
`e6693ac514c6d72a015b9b62f9598e7e9d9e6806517c8a00f613ca4a955b0c92`.
No provider/hardware qualification is claimed. Complete logs/statuses are in
`/home/zboll/git/ams-mel-doc-005-evidence/r2-admin-publication`.

Both DOC-005 reports preserve the recovery history and explicitly narrow this PR
to Admin. Source citation clarification records MFA_Component.h as the Common
state declaration and the upstream prose/declaration inconsistency. Exact removed
identities are included in the coverage report. Publication and hosted results
are separate subsequent observations; leave the PR open for independent review.

## DOC-005B — final exact-source local qualification complete

All final gates passed on frozen specification SHA-256
`22e05187ab1cce06e2ffb9f90eb0dd143bc1ab2751d4525921e6f1789890fbdd`:
strict semantic/docs twice, 32 byte-identical site files, HTML 31/1,307, 75
documentation tests, six integrity tests, exact identity/protected-byte audits,
then sequential make test-native (434/434), make test-ada, make check and
make check-ada-format. Final tested source hash unchanged; exactly seven
authorized files differ. The superseded earlier source-hash mismatch is retained
and not credited as final qualification. Coverage remains 3,253/4,295, RF C2
353/771; 341 gaps and 254 public warnings removed, zero additions. All 73
private warning identities, 418 deferred DOC-005C gaps and historical evidence
remain unchanged. No new Squall/hardware execution. Publication/hosted evidence
is recorded separately; no merge, tag, release or feature Pages deployment.

## DOC-005C — all three documentation checkpoints qualified

Selected main `4d4b07f83831ee524b5c91074ba84dd22070129d` contains merged
DOC-005B reviewed head `ce470c267de9c45c7cb4c64b00e98c52bdd16fe5`.
Isolated worktree `/home/zboll/git/ams-mel-doc-005c`, branch
`docs/005c-rf-c2-events-waveforms-intervals`; evidence
`/home/zboll/git/ams-mel-doc-005c-evidence/r1`. Earlier worktrees/evidence intact.

Measured exact removals: 005C1 133, 005C2 165, 005C3 120, total 418.
RF C2 is 771/771; repository 3,671/4,295, remaining gaps 624. Public warning
removals 97/113/83 total 293; final raw/public/private 599/526/73, with private
identities unchanged. No new gaps/identities, unrelated package coverage, native
profiles/anchors or Health exception changes. Every checkpoint passed strict
semantic/docs/HTML/link generation, 75 documentation tests, six integrity tests,
format checks and token/protected-byte integrity. Attachment corrections and
failed pre-ratchet attempts remain recorded, not credited as passing runs.

Final combined deterministic-site/regression/publication checks are pending at
this checkpoint record; no incomplete PR publication or hardware claim.

### DOC-005C final exact-source local qualification — PASS

Frozen specification SHA-256
`509c94edb5f7d3a95d057a8c2064b77c0379a15115d436d282592ee37ba232aa`
passed two fresh byte-identical complete sites (32 files), HTML 31 pages/1,307
links, 75 documentation tests, six integrity tests, literal-aware executable-token
and protected-byte checks, 804 vendor checksums, exact coverage/warning identities,
then sequential `make test-native` (434/434), `make test-ada`, `make check` and
`make check-ada-format`. Final hash unchanged; exactly seven authorized files.
All inherited comment lines and completed DOC-005B lifecycle/VA/Job descriptions
are preserved. In-scope historical adjacent-comment attachments are explicitly
reconciled in the coverage report. No new provider/hardware claim or DOC-006.
Main refetched unchanged; publication/hosted checks are separate exact-head
observations. Full output/statuses remain in external `r1/final`.
