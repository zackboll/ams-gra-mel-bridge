# GNATdoc foundation

This foundation measures the existing public API. It does not claim complete
documentation, modern navigation, guides or browser qualification. DOC-002
through DOC-007 are separate phases.

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
