# DOC-CI-007 — preserve identifier spelling in coverage evidence

## Scope and disposition

**CORRECTIVE_WORK_REQUIRED.** The 17 false coverage gaps are corrected with no
baseline change. The genuine source-epoch suite now passes the coverage ratchet
but encounters a separate pre-existing portal destination/ledger failure in
setup. Its positive and artifact-negative tests have not executed. This is not
a claim of complete local or hosted Documentation qualification.

This branch is stacked on open PR #149, exact commit
`e1d932cd2126fb04e26b333c030eba8cbedcae0f`, tree
`fc95fc3a6ea3368bacbfb3aa585e4a407e046094`. Fetched starting main remains
`888553cba1084efea6a201da27dc1fa7cfd2f34b`, tree
`95df6b06ea0be8c5424bc912eda175ab1b5cbe56`. No other DOC-CI-007 ownership was
found during preflight. PR #149's independent warning correction is unchanged.

Only prose normalization and focused tests change. No Ada/ABI/runtime source,
declaration identity, native-anchor reconciliation, exception policy, warning
comparator, historical source, source-policy admission, permission, workflow,
timeout, warning baseline or coverage baseline changes.

## Reproduction and each original gap

Fresh GNATdoc generation used Alire 2.1.1, GNAT 16.1.0, GPRbuild crate 26.0.1
(reporting 26.0.0, 2026-04-15), GNATdoc 26.0.0 (20260415), and the reviewed
enhancer distribution at `524b7610e21e9a7b87e8e0761580ed59a3b0295c`.
Alire settings are task-isolated; the previously verified enhancer environment
and pinned tools are reused explicitly. Evidence is external at
`/home/zboll/doc-ci-007-evidence`.

`before/{main,pr149,synthetic}/coverage-gaps.json` retains **each full normalized
identity**, original source comment, source path/line and declaration, parsed
source prose, actual native rendered prose, and baseline classification.
All 17 are absent from the admitted coverage baseline. Each source/rendered
pair was checked independently: parsed source equals rendered prose with
underscores deleted, proving the cause for all 17 rather than assuming it.

In the table, Health and Image abbreviate
`AMS.MEL.IR.Health_Status.Metadata` and `AMS.MEL.IR.Image.Metadata`; NUC is
`AMS.MEL.IR.NUC`. The function signatures and member positions are unchanged
and retained verbatim in the JSON evidence.

| # | Declaration/member identity | Identifier spelling destroyed in source evidence |
| --- | --- | --- |
| 1 | Health.NUC_Stream: description | NUC_TempData, Metadata_Stream |
| 2 | Health.Open_NUC: description | NUC_TempData |
| 3 | Health.Open_NUC: parameter 1:Queue_Capacity | NUC_TempData |
| 4 | Health.Receive_NUC: return 0 | NUC_TempData |
| 5 | Image: package description | NUC_TempData, NUC_Stream |
| 6 | Image.NUC_Stream: description | NUC_TempData, Metadata_Stream |
| 7 | Image.Open_NUC: description | NUC_TempData |
| 8 | Image.Open_NUC: parameter 1:Queue_Capacity | NUC_TempData |
| 9 | Image.Receive_NUC: return 0 | NUC_TempData |
| 10 | NUC: package description | NUC_TempData, AMS.MEL.IR.Health_Status.Metadata |
| 11 | NUC.Not_Supported: description | NUC_TempData |
| 12 | NUC.Pixel_Width: literal 0:Unsigned_8_Pixel | uint8_t |
| 13 | NUC.Pixel_Width: literal 1:Unsigned_16_Pixel | uint16_t |
| 14 | NUC.Pixel_Width: literal 2:Unsigned_32_Pixel | uint32_t |
| 15 | NUC.Pixel_Value: component 1:Value_8 | uint8_t |
| 16 | NUC.Pixel_Value: component 2:Value_16 | uint16_t |
| 17 | NUC.Pixel_Value: component 3:Value_32 | uint32_t |

For example, original `--  @enum Unsigned_8_Pixel The uint8_t alternative.`
became source prose `The uint8t alternative.` while actual GNATdoc rendered
`The uint8_t alternative.`. The former blanket `re.sub(r"[`*_]", "", text)`
caused the mismatch; GNATdoc and declaration attachment were not at fault.

## Minimal correction and fail-closed controls

`scripts/check_doc_coverage.py` now removes only paired inline formatting.
Backtick contents are protected while emphasis is processed, so code-span
identifier spelling and literal punctuation survive even inside emphasis.
Emphasis delimiters must be paired outside words. Intraword underscores and
unknown/unpaired markup remain significant. Whitespace is collapsed consistently.
Evidence matching also enforces word boundaries so an identifier suffix/prefix
cannot stand in for another identifier. Rendered text is not used to manufacture
missing source evidence. Category/name selection and attached-block logic are
unchanged.

The new provider-free `scripts/test_doc_coverage.py` has 13 passing tests with
warnings-as-errors. They cover uint8_t versus uint8t, Field_Name, exact code
spans, nested emphasis, whitespace, identifier boundaries, absent/meaningless
source, wrong declaration/member descriptions, missing/duplicate native sections,
changed anchors, the exact existing exception, rejected extra exceptions, and
new/stale baseline gaps. Running these tests against the original implementation
fails (exit 1), demonstrating the regression is detected. An intermediate nested
markup attempt failed one test and was corrected; its log is retained externally.

## Before/after real measurement

| Source | Before | After | Native reconciliation | New/stale gaps |
| --- | --- | --- | --- | --- |
| Unchanged main | 4329/4347 | 4346/4347 | 1255 source / 1254 native / 1 exception | 0/0 |
| PR #149 | 4329/4347 | 4346/4347 | Same | 0/0 |
| Documented synthetic addition | 4330/4348 | 4347/4348 | 1256 source / 1255 native / 1 exception | 0/0 |

Every identity and native ledger entry is identical before/after; exactly the 17
documented flags change from false to true. The sole remaining gap is the
existing approved `Health_Status.Metadata.Status` package-alias exception. No
new renderer exception is admitted. Full reports and explicit comparisons are
in `before/`, `after/`, and `comparison-summary.json` externally.

PR #149 and synthetic warning comparisons both return 0, zero added/removed;
unchanged main still has the four original warnings addressed only by #149.
Baseline SHA-256 values remain identical to main and #149:

- Warning: `1e68ecc1a2808a571499c879580458ae9dfb7483974fd796b1aab3c47809eea2`.
- Coverage: `79b007ac4c5384565915da2df084f7e7e029ddb2f081e2c5c32db0645f58214c`.
- Native anchors: `06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed`.
- Exceptions: `99b3df25fa24e9a201405b9c5cd67a593f5893f2bb336d741cf7305880e5b048`.

## Qualification and separate blocker

Passed: 13 new tests, eight existing non-site coverage/exception/ratchet tests,
42 warning-comparator tests, `make test-policy`, `make check-ir-api-coverage`,
`make check-ada-format`, `python3 scripts/check_doc_source_integrity.py`, and
`git diff --check`. Commands, logs and real exit statuses are retained externally.
No Ada source changes require formatter edits or runtime requalification.

The complete requested source-epoch invocation, with the pinned environment:

```sh
python -W error -m unittest discover \
  -s docs/tests -p source_epoch_regression_tests.py -v
```

returns **5**, still a setup error, now after successful warning and coverage
checks at `source_epoch.generate` -> `build_docs_portal.package_map`:

```text
build_docs_portal.PortalError: swapped or fabricated package destination
```

The unchanged committed native-anchor ledger has no `AMS.MEL.IR.NUC` package
entries, whereas current source and genuine GNATdoc output contain that public
package and seven reconciled declarations. `portal-dependency.json` and
`nuc-native-ledger-difference.json` retain the exact discrepancy. The portal
currently checks the historical committed ledger, not the just-measured
candidate ledger. That separate source/portal reconciliation dependency needs
review; this PR neither replaces its baseline nor bypasses its validation.

Therefore positive anchor/catalog execution, source/epoch tampering rejection,
pristine/publication tampering, forged-consistent hashes, independent regeneration,
and the suite's undocumented-addition negative remain **unexecuted**, not passed.
Complete preparation, fresh historical replay, six real shards and aggregate are
not attempted while the focused gate is red. Historical tests and identities
are unchanged. Hosted Documentation remains main-push-only; Bootstrap is not
Documentation acceptance. Merge nothing until dependencies and full gates are
qualified in order; no deployment or release is authorized by these results.
