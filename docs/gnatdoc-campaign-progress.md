# GNATdoc campaign progress

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
