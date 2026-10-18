# GNATdoc foundation

## DOC-007E searchable native package browser

Production now enables `navigation.mode = "packages"`, retaining theme, Ada
lexical highlighting and source-verified local type links; guides remain `none`.
The immutable enhancer is `53a321915da2c5a02a54581040673b61f8595b25`
(PR #9 merge), not moving main. Installation below checks the archive and complete
installed implementation/assets fingerprint, not merely version `0.1.0.dev0`.

Optional offline case-insensitive substring search covers exactly the native
sidebar's **29 Compilation Units and eight Tagged Types**, preserving headings,
labels, order, filenames, fragments and `target="document-content"`. Search
reports results; Clear restores all 37 labels and input focus. Approved native
sidebar destinations support active selection, outer `package` bookmarks and,
for the eight tagged entries, `declaration` bookmarks with Back/Forward history.
Package selection uses `aria-current="page"`; selected tagged declarations use
`aria-current="location"`. Re-selecting the same exact destination adds no entry;
package-only and declaration state remain distinct. Approved links followed
inside the iframe use the same history. Other native type/declaration links
remain ordinary navigation, without automatically gaining outer state.

Unknown/duplicate package values fall back to the first native package (`AMS`).
Invalid, duplicate, unknown or wrong-owner declaration values fall back to the
selected approved package. Query values cannot manufacture destinations. Native
outer group fragments remain usable. Without JavaScript, controls remain hidden,
both complete native groups navigate normally, and direct native page/fragment
bookmarks, local type hyperlinks and portal links work without enhancement.

This is **not full-text search**, search of all 1,237 sections, complete Ada name
resolution or general declaration navigation. No operation/category manifest,
external mappings/dependencies/host permissions, generic qualification or shared
authored-guide rendering is enabled. The unchanged shared theme includes exactly
four dormant `.generic-instance-summary` rules; no summary HTML/count is enabled.

Publication is **42 files / 37 HTML pages / 2,965 local href/src references**:
the DOC-007D 2,964 plus one local index script. Its 1,418 independent local type
occurrences and zero external links remain unchanged. The original index and all
native package HTML restore byte-for-byte; native CSS and authored portal bytes
remain intact. Only exact consumer-owned index controls/script insertions are
authorized. The original Page checker and global script rejection are unchanged;
it checks the index after removal of only those exact approved insertions.
Package/portal pages have no exception. Full reconstruction of output and manifest
and the R2 fresh-external-audit contract remain required, on a trusted filesystem
with stable parents (not a hostile-directory sandbox).

Use the repository-root installation/build/verify/test commands below. See
[source research](doc-007e-source-research.md) and
[qualification](doc-007e-qualification.md). DOC-007C/D sections below are historical
checkpoints, not the current navigation configuration or publication inventory.

## DOC-007D local type hyperlinks

The publication now enables only source-verified **local** type references in
native declaration snippets, retaining the DOC-007C theme, lexical highlighting,
native documentation authority and authored portal. Production configuration is
exact: theme/highlighting/type links on; navigation and guides `none`; no external
destinations, dependencies, host permissions or JavaScript. The historical
DOC-007C measurements below describe its types-off publication, not this increment.

The selected immutable shared enhancer is
`478d0bbff0be1b9141b5ad39f07a71f0386d86e9` (PR #5 merge), not later main.
It masks Ada literals/comments before identifying type positions. With links off,
all **40 non-manifest files** equal the qualified DOC-007C publication; the
manifest adds only `linked_local_types: 0` and `linked_external_types: 0` counters.

The consumer independently checks every emitted occurrence against the original
MEL source declaration index, pinned native anchor ledger and pristine section.
`docs/tools/local_type_audit.py` accepts only bounded type contexts and ordinary
current/ancestor/fully qualified/import visibility; it does not import the shared
resolver. Exact wrapper restoration, complete pristine byte equality, unchanged
original validation/coverage, and separate full output/manifest reconstruction
remain mandatory. Audit output must be a fresh file outside the repository,
pristine input and publication trees, with an existing parent directory.
Resolved aliases into those trees and existing files/symlinks are rejected.
To save the complete audit externally:

```sh
.doc-tools/enhancer/bin/python docs/tools/build_enhanced_site.py verify \
  --audit /absolute/external/evidence/type-link-occurrence-audit.json
```

Qualified output is **41 files / 37 HTML pages / 2,964 local links**:
1,516 original links + 30 theme references + **1,418** independently audited local
type links; zero external type links. All 1,237 native sections restore exactly;
coverage remains 4,294/4,295 with the same one renderer omission and no public gaps.
The native and shared CSS and all authored portal bytes remain unchanged.

This is not complete Ada name resolution. Unknown, ambiguous, private, external,
package-alias-dependent and unsupported occurrences can remain unlinked. The
Health/Status `Status` renaming still has no native section and is not fabricated.
Image's public `Euler_Value` and `Attitude_Rate.Value` components are exercised;
the similarly typed `Raw_Euler` parameter is body-only, not a public native target.
See [source research](doc-007d-source-research.md) and
[qualification](doc-007d-qualification.md) for evidence, failures and limitations.

## DOC-007C theme and Ada lexical presentation

The original generator and `_site/` remain the pristine qualification authority.
After all original gates pass, install the reviewed, hash-pinned documentation
enhancer and build a separate publication tree, from the repository root:

```sh
sh docs/tools/install_enhancer.sh .doc-tools/enhancer
.doc-tools/enhancer/bin/python \
  docs/tools/build_enhanced_site.py build
.doc-tools/enhancer/bin/python \
  docs/tools/build_enhanced_site.py verify
.doc-tools/enhancer/bin/python \
  -m unittest discover -s docs/tests -v
```

Installation and output directories must be fresh. Do not overlap pristine
generation with tests/verification that read that output. The publication site
is `_site-published/`: **41 files / 37 HTML pages / 1,546 local links**, compared
with pristine **39 / 37 / 1,516**. Exactly one bundled theme stylesheet and one
presentation manifest are added; 30 API pages gain theme links and viewport
metadata. Declaration text, anchors, native CSS and all authored portal pages
remain unchanged. Light/dark system themes and lexical Ada coloring are enabled;
type hyperlinks, enhanced navigation/search and guide rendering are disabled.

The project-owned validator compares against independently pinned original,
source and anchor identities, reconstructs transformations and the manifest,
and runs the original MEL and shared-tool validators. Output metadata alone
cannot authorize changes. CI retains pristine gates and publishes only verified
enhanced output, preserving read-only PR permissions and main-only deployment.
See `doc-007c-source-research.md` and `doc-007c-qualification.md`, including
preserved R1 failure findings and the R2 qualification addenda.

The foundation measures the existing public API. DOC-002 through DOC-006B
completed public documentation under the strict source/rendered-identity model.
DOC-007A adds an offline developer portal around the unchanged native reference.
The historical DOC-001 measurements below remain initial measurements, not
current coverage. Current coverage is **4,294/4,295**, with one exact renderer
exception and zero public gaps; raw warnings are **73**, all private-part.

## DOC-007A static portal

After native generation, `scripts/build_docs_portal.py` reads
`docs/site-map.json`. Its schema fixes six guide identities, paths and labels,
one home page and `assets/portal.css`; arbitrary assets cannot be authorized
by editing JSON. Authored `{{navigation}}`, `{{package-directory}}` and
`{{package:AMS.MEL.IR.Image}}` substitutions are literal, non-executable and
fail closed on unresolved names. Package names come from public Ada library-unit
headers and actual native h1 headings. Destinations reconcile with the qualified
anchor manifest (the empty AMS root has no declaration anchor).

The builder never alters `api/`, invokes compilation or accesses the network.
Generation retains the native GNATdoc index, iframe navigation, package markup
and stylesheet. The final validator allows only the native inventory plus the
seven portal HTML files and one local stylesheet: **39 files / 37 HTML pages**.
It validates Home/guide navigation, the 29-entry package directories, exact
authored package destinations, local links/fragments, symlinks and static-site
safety. Only canonical guide parent links to fixed root assets/pages or verified
native pages are allowed; arbitrary traversal remains rejected.

The narrow source-integrity expansion authorizes only
`scripts/build_docs_portal.py` and `scripts/test_docs_portal.py`. Production
bytes, executable Ada tokens, arbitrary new scripts and unexpected workflows
remain protected. The existing workflow remains unchanged: PR review artifacts
contain the full site; deployment remains main-only. See
`docs/doc-007a-source-research.md` and `docs/doc-007a-qualification.md` for evidence.

## Qualified tools and invocation

Use documentation-only installations of Alire 2.1.1, GNAT Native 16.1.0,
GPRbuild's 26.0.1 Alire distribution and GNATdoc 26.0.0. The latter two print
`GPRBUILD 26.0.0 (2026-04-15)` and `GNATdoc 26.0.0 (20260415)` respectively.
The generator script checks these executable versions; it never uses an
ambient, different GNATdoc. GNATdoc's version command returns status 1.

The CI workflow demonstrates installation under a dedicated Alire settings
directory. Locally, put the qualified compiler and GPRbuild `bin` directories
on PATH, set `GNATDOC_PREFIX` to the qualified installation prefix, then run:

```sh
sh scripts/generate_docs_baseline.sh
python3 -m unittest discover -s scripts -p 'test_*doc*.py' -v
cp -a _site /tmp/documentation-first
sh scripts/generate_docs_baseline.sh
diff -r /tmp/documentation-first _site
python3 scripts/check_doc_source_integrity.py
```

Documentation semantic compilation is specification-only (`-gnatc`) in
`obj/docs` and `obj/docs-internal`. GPR does not import the production library
project or redeclare its owned sources. Two private units are available for
resolution but excluded from publication. There is no native CMake invocation,
production Alire action or production toolchain selection in generation.

**Never overlap native validation commands.** `make test-ada`, formatting via
Alire, `make test-native` and `make check` can indirectly reconfigure the same
native tree. The configure helper compares compiler *paths*, and resets its
selected tree on a mismatch. A PATH change can select a different `c++` even
when `cc` still resolves to the system compiler. Use a stable environment and
wait for one command's exit before starting the next. Preserve evidence before
reconfiguration. This campaign does not change production build behavior.

## Inventory, warnings and coverage

- The library-unit header determines source visibility. Every public `.ads`
  contributes, including public helper children and package instances.
- Complete source profiles distinguish overloaded operations. The source token
  checker preserves string/character literals and operators. Native GNATdoc
  omits aspects and subprogram renaming targets in signatures; source identities
  retain them, while rendered profiles are matched on the actual native subset.
- `gnatdoc-native-anchors.json` records **observed**, source-matched native
  filenames/anchors/signatures. No hash or anchor is manufactured. Altered
  anchors or profiles fail validation. Updates require explicit review.
- The only renderer exception is the exact source-backed package renaming in
  `gnatdoc-renderer-exceptions.json`. Version, scope, target, path and signature
  must match. It is an undocumented source element, not a native section.
- `gnatdoc-warning-baseline.txt` retains all 3,924 warnings. Classification
  produces `obj/docs/warnings.json`: public, private-part and private-unit
  counts reconcile to the complete raw stream. Unmapped or malformed warning
  diagnostics fail; no unknown tool diagnostic is silently discarded.
- `gnatdoc-coverage-baseline.txt` contains stable identities of current gaps.
  Coverage includes package descriptions, declarations, parameters, function
  returns, enum literals, components and discriminants. Reports include all
  elements and their evidence status. Missing members/sections fail inventory
  validation rather than being silently skipped.
- Rendered descriptions need meaningful attached source prose. Existing
  misattached prose is conservatively uncredited and recorded as a gap. This
  is not a semantic-quality claim for descriptions that mechanically match.
  Native comments that do not render on their own declaration cannot count.
- The strict ratchet rejects both added gaps and stale fixed gaps. The initial
  capture command creates **absent** baselines only, never overwrites them.
  Later changes must deliberately remove fixed identities in reviewed diffs;
  new baseline gaps are not an automatic update operation.

Initial measured coverage is **38/4,295 elements (0.8847%)**, with **4,257
undocumented elements**, including the renderer exception. An early uncommitted
measurement credited four package-opening blocks that actually belonged to
the first declaration. Attachment checks were tightened before qualification,
and those four identities were explicitly restored to the initial gap baseline.
No Ada prose changed. GNATdoc abbreviates nine long enumeration signatures with
an ellipsis; prefixes/suffixes are checked against source, all literals are
checked separately, and the original native signature is preserved.

The source scanner supports the forms present in this API: public package
instances, private/limited types, discriminated records, variants, arrays,
enumerations, objects, exceptions, aspects and subprogram renamings. Generic
instance interfaces are represented by their explicit declaration; generated
members of Ada.Containers are not first-party source declarations. Unsupported
or mismatched native constructs fail rather than creating guessed signatures.

## DOC-001 source-integrity boundary

The integrity checker compares the complete Git index inventory with the pinned,
verified foundation base, using NUL-delimited paths. New tracked paths are denied
unless they are under `docs/` or in the explicit DOC-001 authorization set:
the two documentation GPR projects, named documentation tooling/tests under
`scripts/`, and `.github/workflows/documentation.yml`. Neither arbitrary scripts
nor additional workflows or top-level files are authorized. Production additions
under `native/`, `ada/src/`, `ada/tests/src/`, `rust/`, `python/` and `integration/`
are rejected regardless of file extension. Existing production byte comparisons
and literal-preserving Ada token/inventory comparisons remain active.

`scripts/test_doc_source_integrity.py` uses temporary Git repositories and commits
each fixture before checking it. Both staged and unstaged `git diff --exit-code`
are clean, demonstrating why a worktree-diff check cannot enforce this boundary.

## Publication

PRs (including stacked PRs) generate review artifacts and diagnostics without
Pages permissions. Only main may deploy, after the same validation gates pass.
The intended URL is `https://zackboll.github.io/ams-gra-mel-bridge/`.
If Pages is not configured, the owner must select **Settings → Pages → Build
and deployment → Source: GitHub Actions**. A workflow file alone is not evidence
of a successful deployment. Feature branches are never deployed.

## POL-001 reviewed native-test transition

The fixed DOC-001 foundation remains
`42648a92eb0cc84b2980d16db6028992c0494ba1`; historical evidence above is not
rewritten. One exact, all-or-nothing content transition authorizes the two
reviewed PR #111 native-test files, and no other source change. Both working
tree and Git index must contain the same complete original or approved pair.
Mixed/partial states and unknown bytes fail, as do the existing unauthorized
inventory, production-byte and executable-Ada changes. Approval constants live
in the checker, not an editable manifest. Future test edits require new review.

See [POL-001 source transition](pol-001-native-test-source-transition.md) for
the four exact SHA-256 identities, committed negative fixtures, qualification,
branch-executed CI limitations and required order: policy PR first, then a
separately authorized synchronization/requalification of PR #111. Neither PR is
merged or auto-merged by this task; PR #111 remains unchanged.

## IR-CI-001 independently reviewed, merged singleton

PR #115 independently reviewed the destructor-close negative-control handshake
and its exact-content IR singleton, then merged as
`4c72639579891c563425fd9c6c687ce64f892892`. DOC-007C R4 inherits those four
files unchanged by normal merge; it does not implement another native correction.
The foundation above and POL-001's complete original/approved RF pair remain
unchanged. `PROPOSED_IR_TEST_TRANSITION`, its original/corrected identities,
fixtures and mechanical `proposed_pending_independent_review` label are retained
exactly as reviewed; that label is not rewritten to encode GitHub review state.
See `ir-ci-001-destructor-gap-retirement.md` and the R4 addendum in
`doc-007c-qualification.md`. Historical POL-001 and native-CI findings remain
historical evidence, not a claim that the separate correction never occurred.
