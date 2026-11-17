# CI-DOC-FAST-001 — post-merge documentation qualification

## Scope and source

Base: `51ede458587698cc1f8e4601d3471852d6f2a9e5` (`main`). The exact final
source HEAD, hosted run URLs/results and review handoff are recorded in the PR
body, not a self-referential commit identifier in this document.

Only CI orchestration, its provider-free regression fixtures, the Makefile test
entry and this document change. No production MEL behavior, generators, sidebar
implementation, parity assertions, existing negative tests or publication
authorization changes. No API coverage delta; no structured API inventory change
is needed. Historical `docs/coverage.md` is untouched.

## Trigger matrix

| Workflow | Before: PR to main | After: PR to main | Before: main push | After: main push | Manual |
| --- | --- | --- | --- | --- | --- |
| Bootstrap checks | yes | yes | yes | yes | retained |
| Normal API documentation | yes | yes | yes | yes | retained |
| Search qualification | yes | no | no | yes, all paths | retained |
| Resize qualification | yes | no | no | yes, all paths | retained; baseline required |
| Authored HTML qualification | docs/scripts/Ada/workflow paths | no | no | yes, all paths | retained |
| Weekly deep qualification | no | no | no | no | retained, plus Monday schedule |

Automatic heavy runs use workflow/event/ref concurrency groups and cancel obsolete
push runs. Manual runs have separate run-ID groups and cannot cancel or be
cancelled by automatic pushes. All heavy workflows remain read-only and upload
diagnostics only, never Pages artifacts. The normal documentation workflow alone
retains its exact main-push deployment and live-verification authorization.

## Source and resize baseline contract

All three heavy workflows check out `github.sha`, not a moving branch or PR merge
commit. Full history and disabled persisted checkout credentials are retained.

Resize push execution selects exactly the event's `before` SHA: the previous
main tip before that push, including multi-commit pushes. It does not select the
new commit's first parent or resolve a moving `main`. The helper checks event
repository, main ref, after/source SHA, checkout identity, nonzero full lowercase
40-hex commit identities and strict baseline ancestry. Missing/unavailable,
zero (branch creation/deletion), unrelated, equal or malformed baselines fail;
there is no fallback or qualification skip. This repository forbids force pushes;
a non-ancestor push also deliberately fails qualification.

For manual pre-merge qualification, select the feature branch and explicitly
provide a full ancestor SHA (normally the reviewed main/base commit):

```sh
gh workflow run documentation-resize.yml \
  --repo zackboll/ams-gra-mel-bridge --ref YOUR_FEATURE_BRANCH \
  -f baseline_sha=FULL_40_HEX_ANCESTOR_COMMIT_SHA
```

The other two workflows need only `--ref YOUR_FEATURE_BRANCH`. The source is the
dispatch event's exact SHA. A manual baseline must exist in fetched history and
be a strict ancestor; do not use a sibling branch or silently substitute HEAD.
Older baselines must support the existing complete-publication build and resize
control contracts; unsupported historical sources fail, rather than skipping.

Cross-commit content equality is not sidebar presentation parity: legitimate API,
Rustdoc, Python and authored-guide changes must be allowed. The previous main
publication is still fully reconstructed with its own source validation and
manifest. The unchanged parity checker qualifies its resize-disabled control
against that source's own resize-enabled output. Two independent exact-candidate
builds then undergo the same existing strict publication-equality and
resize-disabled-control checks. Candidate repeated deep qualification, all
resealed mutation negatives, enhancer release tests and Chromium/search checks
remain mandatory. Neither source is compared to unrelated content from the
other commit. No generator or existing test expectation was changed.

Diagnostics retain both source-local parity reports, previous-main and repeated
candidate publications/manifests, source SHA selection in `resize-source.json`,
qualification logs and browser evidence. Baseline manifests remain explicitly
local/non-deployable, as before; candidate identity remains event/run-bound.

## Required-check audit

Authenticated REST inspection before implementation:

- `GET /repos/zackboll/ams-gra-mel-bridge/branches/main/protection`:
  `required_status_checks.contexts = []`, `checks = []`, `strict = false`.
- `GET /repos/zackboll/ams-gra-mel-bridge/rulesets?includes_parents=true`: `[]`.
- `GET /repos/zackboll/ams-gra-mel-bridge/rules/branches/main`: `[]`.
- Repository search found no maintained required-status configuration in
  `.github`, `docs/tools` or `scripts`. Workflow contracts enforce publication
  security, not branch-protection required-check names.

None of the three heavy checks is required by the inspected settings. No settings
were changed and no manual removal is currently required. Bootstrap, normal docs,
admin enforcement, conversation resolution and all unrelated settings are intact.
If settings change independently, remove only these heavy job names from any
new required-check lists: `Current-source search and native navigation`,
`Complete publication parity and Chromium sidebar interaction`, `pilot`.
Never remove Bootstrap jobs or the normal documentation `build` check.

## Qualification and expected latency

Local commands run from `/home/zboll/git/ams-ci-doc-fast-001`:

- `make test-policy`: **328 tests passed**, including the 12 new event/source
  fixtures and all existing policy/artifact/security controls. An initial new
  fixture had an erroneous literal `+` in an expected string; corrected before
  the complete successful rerun. Existing expectations were not altered.
- `python3 scripts/check_doc_source_integrity.py`: **pass**, 825 vendored files,
  six workflow privilege boundaries.
- `python3 scripts/check_final_newlines.py`: **pass** after adding final newlines
  to the two new Python files.
- Actionlint **1.7.12** over all six workflows: **pass**.
- PyYAML **6.0.2**, `BaseLoader`, parses all six workflow files with event/job
  mappings: **pass**. This is syntax validation; Actionlint supplies Actions
  expression/schema checks. No new runtime YAML dependency was added.
- `git diff --check`: **pass**.

Fixtures prove ordinary Rust-only (also Ada/Python/native/docs) PRs do not trigger
the three campaigns, while Bootstrap and normal docs still do. They also prove
main-push/manual eligibility, non-main-push rejection, multi-commit push baseline
selection, explicit manual ancestry, invalid-source rejection, provenance output,
retained commands and source-local parity wiring. No test commit is merged to
demonstrate push behavior. Hosted manual campaign and PR results belong in the
final PR handoff; local fixture success is not a claim that Chromium ran locally.

Representative successful prior PR head
`a3713aa09bc985637ad1a08aaa6af099252fa1fb` had workflow wall times (including
queues): normal docs **3m08s** (run `44015325570`), Bootstrap **8m50s**
(`44015325634`), resize **9m23s** (`44015325640`), search **16m50s**
(`44015325551`), authored **19m03s** (`44015325701`). Waiting for all applicable
checks would fall from **19m03s to 8m50s**, approximately **10m13s (54%)** on that
sample, not the sum of parallel campaign durations. Rust-only PRs previously
launched search/resize, not path-filtered authored HTML; on the same timing sample
their potential all-check wait falls from **16m50s to 8m50s**, about **8m (48%)**.
These are estimates, not a measured new-PR speedup or a branch-protection promise:
there are currently no configured required status names. Actual timing depends
on runners, content and remaining checks.

## Delivery boundary

Keep the focused PR non-draft, unmerged, without self-approval, auto-merge or
protection bypass. Independent review should examine source-local resize parity,
strict baseline handling and unchanged publication authority in particular.
The repository currently lists only the authenticated owner as collaborator and
no teams; if no independent reviewer can be assigned, request review in the PR
and report the reviewer-assignment limitation explicitly.
