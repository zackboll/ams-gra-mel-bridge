# DOC-011 qualification record

## Scope and coordination

Fetched main at start and before publication:
`36ecb1e63c24df855d6f6e85f8245e16fc9d8d53` (DOC-009 merge).
There were no open repository PRs. The existing DOC-010 worktree was clean at
that SHA and was not modified. Upstream enhancer PR #15 is merged, but this task
does not adopt it or change the existing immutable enhancer revision. Native
page/anchor links remain independently usable before or after symbol bookmarks.

Implementation and permanent extension rules are in `ada-example-rendering.md`.
The isolated branch/worktree is `docs/011-ada-example-highlighting-api-links` at
`/home/zboll/git/ams-mel-doc-011`. Evidence is retained separately under
`/home/zboll/git/ams-mel-doc-011-evidence`; DOC-008/009/010 evidence is unchanged.

## Independently reconstructed publication

* 43 published files, 38 HTML pages, 29 public packages.
* 1,238 source declarations, 1,237 native sections; existing renderer omission
  remains exactly unchanged. Coverage stays 4,294/4,295 (99.9767%).
* All 1,428 API type links retained, including the original ordered 1,418 audit
  and ten separately qualified additions.
* 31 complete source listing occurrences; nine unchanged programs and thirteen
  unchanged run cases. No Ada/example/manifest/provider/vendor changes.
* 4,411 syntax-token spans; 31 empty source-end boxes (no text).
* 266 independently derived inline links: 31 package, 99 type/subtype, 136 unique
  operation. Publication references are 3,021 + 266 = **3,287**.
* Only `guides/examples.html` and `assets/portal.css` differ from authenticated
  DOC-009 publication bytes. All 41 other entities, including all API assets and
  `api/presentation.json`, are byte-identical. Both API-only hashes are unchanged.
* Two fresh pristine generations and two fresh enhanced generations are entirely
  byte-identical, not merely count-equivalent. Complete hashes and preservation
  inventories are in `publication-preservation.json` and the generation reports.

Pristine fingerprint:
`3a853e1a6c06e19f7f21d45622de3135054e7beb711245b1838555246c0d6c49`.
Published fingerprint:
`54969299f3009717667e77806582c67eb6c96c6ec43156f12e6094ac1da26aee`.

## Browser and HTTP acceptance

Real Chromium **156.0.8078.4**, 1440×1000 and 390×844, light/dark,
JavaScript on/off: **794 assertions passed**. Sixteen screenshots include syntax
presentation and keyboard focus. Actual clicks landed on the exact native
profiles for Session, Full_Frame and Job_Request; Back/Forward and new tabs
worked. All nine sections and supporting disclosures were exercised. DOM and
selection text, indentation, horizontal scrolling, unstyled source, link cue,
contrast and no external resources were independently checked. No console/page
errors occurred in the passing campaign. Token contrast minima are 6.41:1 light
and 7.28:1 dark; individual measurements are in the rendering guide and evidence.
This is not full assistive-technology or cross-browser certification.

The strict live verifier passed against a local HTTP publication: 43 decoded
entities, 3,287 graph references, 1,428 API type links, 266 inline occurrences and
31 complete listings. No production Pages deployment was attempted. Production
currently serves the old publication until normal independent review and merge;
it is not claimed to serve DOC-011 yet.

## Tests and deviations

The 400 original documentation test identities are retained. Forty additional
DOC-011 tests cover lexing, alias scope/visibility, exact reconstruction, native
profile/anchor authority, forged counts and markup/destination/source mutations.
The original script-side suite has 133 tests; the enhanced suite now has 307.
Focused subset runs do not count as additional tests.

The requested sequence passed with exit zero for each command, in order:

```sh
make test-native
make test-ada
make check
make check-ada-format
alr -n -C ada build
alr -n -C ada/tests run
sh docs/tools/qualify_examples.sh
```

Native tests report 434/434; safe Ada contract/focused-repeat tests pass; the
examples report thirteen actual run cases across nine compiled programs. The
documentation suites pass 133/133 script-side and 307/307 enhanced (440 total).
Source integrity protects all
1,162 inherited production files; Ada/native/Rust/Python/integration/Makefile,
canonical examples, anchor ledger, warning and coverage baselines remain
unchanged. No source-integrity allowlist expansion was made.

Recorded corrective iterations:

* Initial plain-shell `make test-ada` and `make check` lacked GPRbuild on PATH.
  The complete required sequence was restarted in the installed pinned
  GNAT 16.1.0 / GPRbuild environment; no global tool configuration changed.
* An old source-mutation test no longer changed highlighted markup. Its mutation
  was updated to actually alter the candidate, preserving the negative control.
* A qualification run overlapped pristine regeneration and correctly failed on
  changing/missing authority inputs. The full suite was restarted against stable
  trees; these earlier logs are retained and are not counted as passing.
* Chromium omitted the final source newline from selected inline text. The empty
  approved end marker fixes selection without adding source characters.
* The first browser harness selected a nonexistent cached Playwright executable;
  the passing campaign explicitly uses the installed Chromium binary. Browser
  favicon discovery is answered with HTTP 204 by the local fixture, without adding
  a publication file or hiding publication-resource failures.
* Final scope review added a regression proving that a nested package alias is
  never promoted to the outer unit. This tightened proof changes no output bytes.

Final command exits, exact commit/remote/PR identities, hosted checks and the
final disposition belong to the external final handoff after those operations
actually complete; this record does not predeclare their success.
