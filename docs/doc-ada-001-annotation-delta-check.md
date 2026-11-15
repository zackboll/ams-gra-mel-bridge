# DOC-ADA-001 — changed-public-Ada annotation check

## Purpose and boundary

GNATdoc 26.0.0 `--warnings` diagnostics, including `gnatdoc.log`,
`warnings.json` and `coverage.json`, are informational in the current source
documentation pipeline. Descriptive prose therefore did not prevent the
structured parameter/return omissions identified in PR #202. This local tool
checks source annotations directly, without generating HTML or requiring
GNATdoc, Alire, providers, downloads or an enhanced-site build.

Starting freshly fetched main: `084a8906f3cd729b7a20a921f39bd513d8ac02c2`.
Implementation branch: `docs/ada-annotation-delta-check`. PR #202, its branch
and its actual annotations are not modified. A synthetic equivalent of its
original `AMS.MEL.IR.Channel.Metadata` specification lives in the regression
test: ordinary descriptive comments do not satisfy structured annotations.

No native ABI, Ada interface, runtime behavior, reviewed IR coverage inventory,
warning classification/baseline, historical coverage archive, documentation
enhancer, installation pin, publication authority, source-epoch trust rule,
workflow or protected CI policy changes are made. This is not a 100% coverage
gate, and it is deliberately not added to existing CI or `make check`.

## Enforced contract

On selected declarations:

- Functions: exactly one meaningful `@param Name` for each formal parameter,
  and exactly one meaningful `@return`, including parameterless functions.
- Procedures: exactly one meaningful `@param Name` for each formal parameter;
  no `@return`.
- Public records: exactly one meaningful `@field Name` per component and
  discriminant represented by the shared documentation model.
- Parameter and field identifiers match case-insensitively. Stale/unknown,
  duplicate, empty and placeholder annotations fail. A description must contain
  a word character and must not contain the standalone words TODO, TBD or FIXME.
  There is no natural-language semantic evaluation.
- `@enum`, `@formal` and `@exception` remain accepted and optional. This task
  introduces no completeness/content requirements for those categories. Unknown
  structured tags or misplaced param/return/field tags fail on selected APIs.

Only the immediately trailing consecutive comment block is associated, using
`check_doc_coverage.attached_block`. Blank/code lines stop attachment; preceding
package comments and the next declaration's annotations cannot be borrowed.
Descriptions, inline formatting and continuation lines use `source_prose`.

## Selection and legacy exclusion

The checker resolves the explicit base to a commit, failing clearly if it is
unavailable. `git diff BASE` discovers tracked working-source differences;
`git ls-files --others --exclude-standard` also includes new untracked specs.
The result includes committed branch changes plus staged and unstaged working
edits, rather than comparing only HEAD or only the index. Current working
content wins if a staged change has subsequently been edited again.

As in `gnatdoc_inventory`, the library surface is `ada/src/*.ads`. Private
library units are excluded using the inventory's header rule; the first private
part and everything after it are excluded. Deleted files impose no requirements.

For each changed file, the existing `gnatdoc_coverage_model.specification`
(which uses `compare_gnatdoc_warnings.declaration_index`) provides declaration
scopes, spans and members. `expected_items`/`parameter_names` supply formal and
field names. This tool does not introduce a second Ada declaration parser.

Selection uses a multiset of enclosing scope, complete token-normalized source
declaration and normalized attached documentation. Tokens retain string/character
literals, defaults, aspects and record component details, while ignoring Ada
identifier case, code whitespace and comments embedded inside declarations.
Comment reflow/indentation is ignored, but prose/tag-content changes select the
owner. Overloaded profiles remain distinct; record component changes select the
owning record. Line numbers locate diagnostics only and are not identities.
Unchanged declarations are subtracted individually, so moving a declaration or
editing its neighbor does not promote unrelated legacy gaps into failures.

## Local usage

From the repository root:

```sh
make check-ada-docs-changed
# Default: origin/main; does not fetch implicitly.
python3 scripts/check_ada_doc_annotations.py --base origin/main
# Reproducible explicit base:
python3 scripts/check_ada_doc_annotations.py --base 084a8906f3cd729b7a20a921f39bd513d8ac02c2
```

Run after changing a public Ada declaration or its attached documentation.
Diagnostics use repository-relative filenames and declaration-start line numbers,
name the declaration and tag, and return status 1 for findings. Git/input errors
return status 2. Success returns 0 and explicitly reports the number inspected,
including zero when no public declarations changed. The task-owned branch
changes no Ada specs, so its ordinary check correctly selects zero declarations;
synthetic regression tests, not that zero result, qualify detection behavior.

## Parser limitations

This intentionally lightweight lexical check is not an Ada compiler or GNATdoc
replacement. It retains the shared index's supported ordinary package-level
subprograms and records, multiline signatures, grouped names and defaults.
The shared inventory stops visibility at the first standalone `private` line.
Nested public packages are diagnosed as unsupported rather than inferring their
visibility incorrectly. Operator-symbol subprograms skipped by the index,
generic subprogram instantiations without locally available profiles, variant
records and multiple declarations/components on one line produce explicit
diagnostics when selected. Other shared-parser failures (including unterminated
profiles/scopes and unsupported record forms) fail the changed file explicitly,
with the parser's source line where available, rather than silently passing.
Consequently an unsupported historical construct in a changed public file may
require manual investigation; the tool never claims compiler-level coverage.
Duplicate normalized declarations are ambiguous and fail when selected.

Only directly declared record components/discriminants in the shared model are
required; inherited semantic members are not resolved. Private implementation
components are never checked. Annotation prose is checked for simple content,
not accuracy, units, ownership semantics or correspondence with generated HTML.

## Qualification

Actual local commands/results on the implementation branch:

- `python3 -W error -m unittest discover -s scripts -p test_ada_doc_annotations.py -v`
  — PASS, 55 tests in isolated temporary Git repositories. Covers all requested
  categories, the prose-only PR #202 fixture, missing base, CLI exit statuses,
  working-tree/committed selection, overloads, formatting, visibility and
  explicit unsupported/ambiguous diagnostics.
- `make check-ada-docs-changed` — PASS, 0 inspected, 0 findings.
- `make test-docs-ci` — PASS, 40 tests across three suites.
- `make test-policy` — PASS, 244 tests including its nested documentation suites.
- `python3 scripts/check_doc_source_integrity.py` — PASS, 825 vendored files,
  4 workflows; existing security/dependency rules unchanged.

- `python3 scripts/check_final_newlines.py` — PASS after correcting final
  newlines in the new scripts.
- `git diff --check` — PASS.

No first-party Ada files changed, so Ada formatting/build
validation is not applicable. Delivery HEAD/tree and hosted check statuses are
recorded in the PR/delivery summary rather than embedding a self-referential
commit identity in this file.
