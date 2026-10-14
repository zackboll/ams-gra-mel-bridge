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