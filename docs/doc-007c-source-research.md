# DOC-007C shared enhancer source research

R1 findings are preserved below. The **R2 addendum** records the reviewed fix
and successful real-site integration; the historical blocker is resolved.

Disposition: **BLOCKED_BY_REAL_SITE_COMPATIBILITY**. This is a preflight
finding, not a successful integration or publication qualification.

## Revisions and isolation

The task expected MEL main `291e97c4b1495b594b462982822fa48a2f439527`.
Fetching main found `4b2ccbda6a1ad393b436f0e8f1caac9defb283c2`, the merge
of PR #113 at 2026-10-17T05:05:22Z. Its reviewed feature head remains
`a43c7ad59d9433f6db7d32885161bf6c3fe67de3`. No change was made to that PR
or its worktree. DOC-007C branches directly from the fetched merged main.

Shared tool: <https://github.com/zackboll/ada-gnatdoc-enhancer>, exact commit
`f2529eef6d3b6cceea1fed0d4347ae13129bf79b`, tree
`8a788d97af57e81d99dee047ea9c5025a590732b`, CLI/package version
`0.1.0.dev0`. A separate clean detached clone and documentation-only venv
live outside MEL. Installation used a Git direct reference to the exact
revision in that clone; installed `direct_url.json` records the same commit.
The existing enhancer worktree contained unrelated unfinished changes and
was not used or modified. No tool implementation or CSS was copied into MEL.

## Inspected implementation and boundaries

All references below describe the pinned shared-tool source, not MEL code.

- `cli.py`: `inventory` calls native `snapshot`; `enhance --dry-run` runs the
  actual pipeline against temporary copied output. Errors exit with status 2.
- `config.py`: strict schema 1 accepts `docs/gnatdoc-enhance.toml` unchanged
  from the task's proposed configuration. The one allowed renderer exception
  is the exact Health Status Metadata Status identity. Navigation, type links,
  and guide rendering are disabled explicitly.
- `inventory.py`: `.ads` scanning excludes private library packages and
  private parts; declarations are modeled for types/subtypes and subprograms.
  `_one` calls `last_semicolon` on comment/literal-masked text. Record closure
  currently requires the regular expression `\bend\s+record\s*;`.
- `native_html.py`: discovers real hash-named pages from native h1 headings,
  native h4 sections and compilation-unit index entries. It reconciles each
  modeled source declaration against a unique same-name normalized profile.
  Its aspect removal currently names only Pre/Post/Global/Depends. This
  inspection is not evidence of complete reconciliation of MEL's declarations.
- `adapters/mel.py`: requires the exact `package Status renames
  AMS.MEL.Status;` source alias and its absence as a rendered renaming section.
  This adapter was not reached in the failed preflight.
- `pipeline.py`: requires a site root containing `api/`; output has exact
  inventory checks. It reconstructs transformations from captured original
  HTML in `api/presentation.json`; that output payload alone cannot establish
  equivalence to the independently qualified MEL original site.
- `theme.py`: inserts one viewport and theme link at the native stylesheet
  link, and installs bundled `api/gnatdoc-theme.css`. Native CSS is retained.
- `syntax.py`: applies lexical spans to declaration snippets and checks plain
  text equality; unsupported/nested tags fail. These transformations were not
  reached on the real site.
- `validation.py`: checks exact output inventory, HTML safety, resources,
  duplicate IDs, internal links and fragments. MEL would still need independent
  original/source/anchor/authored-portal comparison before publication.
- `guides.py`: HTML guide mode requires `assets/gnatdoc-guides.css`, whereas
  MEL owns `assets/portal.css`. No manifest change was attempted. The preflight
  staging root contains only the original `api/` subtree, not portal pages.

## Actual real-source incompatibility

Source: `/home/zboll/git/ams-mel-doc-007c/ada/src/ams-mel-rf-product_rx.ads`.

- `Complex_I16`: lines 342–351.
- `Complex_I8`: lines 363–372.

Both use a valid record aspect specification after `end record`, with
trailing documentation comments before `with Convention => C;`. For example:

```ada
   type Complex_I16 is record
      Real : Interfaces.Integer_16;
      Imag : Interfaces.Integer_16;
   end record
   --  C-compatible Cartesian I/Q value with two signed 16-bit integer components. Native
   --  MELComplex values are copied component-wise; no upstream C++ object-layout compatibility,
   --  sample scaling or normalization is assumed.
   --  @field Real Signed real (in-phase) component, copied without DSP conversion.
   --  @field Imag Signed imaginary (quadrature) component, copied without DSP conversion.
   with Convention => C;
```

The complete byte-exact source, including actual comments, is saved externally;
this abbreviated explanation is not substituted for it in testing. Both exact
source slices independently reproduce `EnhancementError: unterminated record
type` using the installed pinned `last_semicolon`, without modifying it.

The whole-site CLI fails first while scanning `Complex_I16`. Its regex match
begins in masked whitespace belonging to preceding comments; the declaration
keyword itself is at line 342. Both `inventory` and `enhance --dry-run` report:

```text
gnatdoc-enhancer: unterminated record type
```

Both exit 2. Failure occurs before native page reconciliation, highlighting,
theme installation or manifest generation. Recognition of all packages and all
1,237 sections by the shared tool is therefore **not established**.

## Required independently reviewed shared-tool correction

Correct record-declaration termination in the enhancer's `inventory.py` to
recognize `end record` followed by an optional top-level aspect specification,
including intervening comments, and consume through the actual declaration
semicolon. Preserve the full source signature and literal/default identities.
Reconcile native signatures using a nesting-aware top-level aspect boundary:
GNATdoc renders these records without their `Convention` aspects, and the
current named-aspect split does not cover this case.

Add exact real-source regressions for both records and negative controls for
changed fields, literals, aspects and malformed/incomplete declarations. This
first correction does not establish full compatibility: rerun the entire real
29-package inventory and dry-run, requiring all 1,238 declarations to reconcile
with 1,237 native sections and the single exact renderer exception. Address any
further measured incompatibilities in that shared-tool review, not in MEL.

No parser replacement, source edit, synthetic-fixture substitution, declaration
reconciliation bypass or unreviewed newer enhancer revision was used here.

## R2: reviewed correction and consumer integration

The preceding sections are preserved R1 findings, not the current disposition.
R1 evidence remains at `/home/zboll/git/ams-mel-doc-007c-evidence/r1`.
Before any R2 edit, all three uncommitted documentation additions were copied
with hashes into `/home/zboll/git/ams-mel-doc-007c-evidence/r2/prior-doc-additions`.

Enhancer PR #4 merged at 2026-10-17T15:23:24Z. Fetched main and the pinned
merge revision are `c1a691452cf436624809f4043b30087de6688052`; the reviewed
head `d35596eb301d86cf97857762627d0e109e609630` is an ancestor and its tree
is identical to the merge tree. A separate clean detached clone was inspected.
The installation is non-editable, from an exact Git-revision source archive:

```text
https://codeload.github.com/zackboll/ada-gnatdoc-enhancer/tar.gz/c1a691452cf436624809f4043b30087de6688052
SHA-256 d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493
CLI/package 0.1.0.dev0
```

`inventory.py:last_semicolon` now scans after `end record` through the actual
terminating semicolon. Both complete real record signatures retain intervening
comments and `with Convention => C;`. `profiles.py:record_convention_profile`
accepts only the documented simple component-only record grammar and exact
Convention C aspect; it preserves source ownership of omitted tokens.
`profiles.py:renaming_profile` requires a uniquely established direct public
source target with a matching callable profile, rejecting missing/ambiguous
targets, chains and unsupported scopes. `native_html.py:snapshot` uses this
reconciliation and rejects duplicate/ambiguous native matches. The original
source-backed exception adapter remains unchanged in purpose.

Fresh MEL inventory and dry-run pass, with 29 public packages and 1,237 native
sections. The complete enhancer modeled-profile audit has 1,203 matches:
1,191 exact, nine native abbreviated enumerations, one subprogram renaming and
two Convention C records; zero unresolved mismatches. `Add_Job_Intervals` has
its actual native anchor and uniquely established source target. The enhancer
does not model all declaration kinds: MEL's unchanged independent coverage
checker separately reconciles all 1,238 source declarations / 1,237 sections
and the one exact package-renaming omission. Neither count is misrepresented
as the other. Exact source/profile and output identities are retained externally
in `profile-audit.json`, the original coverage report and published hash inventory.

### Narrow publication boundary

`/home/zboll/git/ams-mel-doc-007c/docs/tools/build_enhanced_site.py` qualifies
the full original 39-file site with fixed reviewed byte/source/anchor aggregate
identities and runs the unchanged MEL validator and coverage ratchet. It builds
an isolated API-only copy, calls the installed enhancer, verifies that API, and
assembles a separate complete output. The seven authored pages and portal CSS
are copied unchanged; no guide renderer runs and no portal schema changes.

Final verification independently removes only the exact single metadata
insertion and flat qualified lexical spans, requiring byte equality to pristine
HTML, then reruns MEL's original validator and source/anchor coverage on the
restored tree. It also reconstructs expected transformations and the complete
presentation manifest from trusted original bytes and authoritative source,
compares every output byte, and runs the enhancer verifier on API-only output.
Wrong-but-existing links/anchors and forged manifest payloads cannot authorize
changes. Newly allowed files are exactly `api/gnatdoc-theme.css` and
`api/presentation.json`; no JavaScript is required or authorized. The complete
installed implementation/assets fingerprint is checked, not just its dev version.

The hard pins intentionally fail on future source, authored portal, renderer or
anchor changes until independently reviewed; they are not automatically updated
by generation. They do not replace the original checks. Temporary verification
trees never become publication artifacts. Output destinations must be fresh.
`/home/zboll/git/ams-mel-doc-007c/docs/tools/install_enhancer.sh` verifies the
archive digest before installation and pins documentation build helpers
setuptools 80.9.0 / wheel 0.45.1. No production dependency is added.

The existing source-integrity authorization already permits documentation-owned
files under `docs/` and the named workflow. It is not modified; no scripts-wide
or API-directory exemption was introduced. `.gitignore` adds only the separate
generated publication directory. The original generator, portal builder,
validator, coverage model and policy checker remain byte-identical.
