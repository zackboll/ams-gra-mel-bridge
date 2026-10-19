# DOC-007E qualification

This increment enables the shared package browser, not operation navigation or
full-text search. Final commit, PR, synthetic merge, hosted results and external
evidence-manifest identity are recorded in the external final handoff to avoid
self-referential commit identities. Pending checks do not count as passing.

## Starting state and authorized scope

Fetched main / actual PR #116 merge:
`8be05e9d0284e81a1769b2e1639a4b96b191324c`, tree
`8158378afbfb59686a396e2f758ae30545610c8a`. Ordered parents are
`0e64ea0689f2fae360baba78b6ed6f74aeef08ca` and reviewed R2 head
`1e1849ce1d01e3600256ae15b220c8ac1c5265ea`. GitHub and immutable fetched
objects agree. Existing DOC-007C/D worktrees and evidence remain untouched.
Task worktree: `/home/zboll/git/ams-mel-doc-007e`; branch
`docs/007e-package-browser`; evidence:
`/home/zboll/git/ams-mel-doc-007e-evidence/r1`.

Both post-merge main workflows completed successfully on attempt 1:
[Documentation 39095043552](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39095043552)
and [Bootstrap 39095043538](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39095043538).
The exact-main `ada-api-8be05e9d0284e81a1769b2e1639a4b96b191324c` artifact
passes the unchanged DOC-007D R2 validator with trusted pristine input and a fresh
external audit. All 41 filenames/full bytes equal qualified R2 publication. The
baseline artifact, trusted pristine and all experimental controls are separate.

Original ten authorized paths are retained. One explicit user-authorized scope
extension adds **two lines** to `docs/tests/test_local_type_audit.py`: compose
index-only `presentation.restore_navigation` before its existing metadata removal
and complete pristine-byte equality assertion. All 66 prior cases/assertions are
retained. The type-only helper remains unchanged. The failed test and concrete
reason were presented before requesting this minimal eleventh-path authorization;
the user's affirmative response is recorded in the external scope diagnosis.
No production, Ada, native test, Rust, Python, original script/validator, ledger,
baseline, workflow, vendor, portal/template/CSS or historical evidence edit.

## Immutable distribution and exact consumer contract

See [source research](doc-007e-source-research.md) for the complete runtime delta,
merge/head/tree relationships, native target authority and literal insertion
contract. Selected enhancer merge is `53a321915da2c5a02a54581040673b61f8595b25`.
Archive SHA-256:
`d324735784435176c0f1a7093cf462e15b2bd92be429ef5e1e42e802331ca52e`.
All 1,089 archive files equal immutable Git blobs. Fresh isolated non-editable
installation equals all 29 implementation/assets files. Complete fingerprint:
`54d9327e57662a5843198fbc5352d4c5f544a3d5e3047c8269c6b9c68c5e878b`.
JS SHA-256: `b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd`.
CSS SHA-256: `cf06637732a8b047a7191bfb991a5c3da7cd31a52076876880d5f8a3f63929bc`.

The consumer independently authorizes unique exact controls and script insertion,
each in its reviewed index position, and complete JavaScript identity. It removes
only these exact insertions for the unchanged global script-rejecting Page check.
All active attributes/additional scripts remain forbidden. Index restoration
requires full trusted pristine bytes, not a label count or output-side permission
manifest. Each native type link is independently audited; package pages restore
exactly. The original MEL checks run on the restored site. All publication output
and the entire presentation manifest are independently reconstructed from trusted
inputs using the immutable distribution. The R2 exclusive external audit creation,
rechecking and post-return inventory guarantees remain unchanged.

## Compatibility and deterministic output

Complete relative-filename/full-file inventories are retained externally.

| Comparison | Added | Changed | Existing files identical |
|---|---|---|---:|
| A: new pin/navigation off vs DOC-007D | none | theme CSS, presentation manifest | 39/41 |
| B: packages vs same new pin/off | navigation JS | API index, presentation manifest | 39/41 |
| C: final vs DOC-007D | navigation JS | API index, theme CSS, presentation manifest | 38/41 |

The shared CSS delta is exactly four `.generic-instance-summary` rules inserted
before `body.main`, unchanged upstream bytes. No summary HTML or generic counter
exists. All package HTML, type occurrences, native CSS and authored portal bytes
are otherwise unchanged. No new native anchors are added.

Two fresh pristine generations equal each other and the trusted pristine baseline.
Two fresh enhanced generations equal in every filename/full byte. Re-enhancing
enhanced API output is byte-idempotent. Dry-run preserves sources, staging,
pristine and publication inventories. Independent restoration and tamper rejection
are exercised against authentic qualified inputs, not fabricated fixtures.

Actual output: **42 files, 37 HTML pages, 29 public packages, 1,418 local type
links, zero external type links, 2,965 local href/src references**. The extra
reference is exactly the local script src: 2,964 + 1. Complete type-audit JSON
equals qualified DOC-007D byte-for-byte. Coverage remains 4,294/4,295, 1,238 source
declarations / 1,237 native sections with the exact renderer omission. All 1,203
modeled profiles reconcile uniquely with zero unresolved mismatches. Warnings
remain 73 raw / zero public / 73 private. Integrity checks retain 1,162 protected
files and 804/804 vendor checksums. No authority hash was updated.

## Reproducible local commands and distinct tests

From the task repository root, with GNAT 16.1.0 and GPRbuild 26.0.1 first on PATH
and `GNATDOC_PREFIX` pointing to the qualified GNATdoc 26.0.0 installation:

```sh
sh scripts/generate_docs_baseline.sh
cp -a _site /absolute/external/evidence/pristine-first
sh scripts/generate_docs_baseline.sh
diff -r /absolute/external/evidence/pristine-first _site
sh docs/tools/install_enhancer.sh .doc-tools/enhancer
.doc-tools/enhancer/bin/python docs/tools/build_enhanced_site.py build
.doc-tools/enhancer/bin/python docs/tools/build_enhanced_site.py verify \
  --audit /absolute/external/evidence/fresh-audit.json
.doc-tools/enhancer/bin/python -m unittest discover -s docs/tests -v
python3 -m unittest discover -s scripts -p 'test_*doc*.py' -v
```

Installation/output/audit paths must be fresh; the audit parent must already
exist outside resolved repository/pristine/publication trees. Keep parents trusted
and stable. No original generation overlaps any reader of the original output.
Experimental off/old-pin controls use external configurations/copies and shared
APIs, never a consumer validation bypass or production override.

**116 original documentation tests** pass unchanged (including the 34 integrity
tests counted once), separately from **66 retained presentation/semantic/audit
tests + 34 new navigation cases = 100** passing tests. Cases cover exact
restoration, insertion count/content/placement, extra/remote/inline script,
attributes/resources, package/portal insertion, missing/modified/substituted/
symlink JS, wrong-but-existing destinations/fragments, native labels/groups/order/
iframe, forged manifests, forbidden feature configuration and native/authored
preservation. Twelve retained audit-output cases exercise checked-tree preservation
on rejection and fresh external success with accurate post-return hashes.

Required regression commands ran **sequentially**, with no overlapping native
build-tree writers:

| Command | Result |
|---|---|
| `make test-native` | 434/434 passed |
| `make test-ada` | Complete contracts and existing required repeats passed |
| `make check` | Build isolation, native, Ada, format and hygiene passed |
| `make check-ada-format` | Passed |
| `alr -n -C ada build` | Passed |
| `alr -n -C ada/tests run` | Passed |

Tools: `/usr/bin/gcc` and `/usr/bin/g++` Debian 14.2.0-19; GNAT 16.1.0;
GPRBUILD 26.0.0 (2026-04-15), distributed as 26.0.1; Alire 2.1.1;
CMake 3.31.6; Python 3.13.5. Generation explicitly uses
`GNATDOC_PREFIX/bin/gnatdoc` 26.0.0 (20260415), not ambient GNAT Studio's 27.0w.
No Ada source edit or formatting rewrite was needed. Required format checks ran.

## Failures retained and diagnosed

1. An attempted policy JSON read used a nonexistent filename; the actual
   source-integrity policy is implemented in unchanged
   `scripts/check_doc_source_integrity.py` and documented in the foundation.
2. The first 100-test presentation run failed the retained index restoration
   contract and two new fixtures. The scope conflict was resolved by explicit user
   authorization above; native list items omit closing li tags and the native
   heading is `Tagged types`, corrected in new fixtures. All assertions remain.
3. First comparison harness assumed generic CSS rules were appended. Actual diff
   shows four inserted rules; revised harness removes exactly those four and
   requires complete old CSS equality. No asset was edited.
4. Ambient Python lacks Playwright. A fresh external environment was installed;
   it is not a production dependency. Early browser harness attempts had a duplicate
   argument name, tested focus-visible before switching to keyboard modality, and
   tried locating an original href after deliberately mutating it. Their harnesses,
   results and logs are retained separately. A no-JS polling attempt timed out;
   navigation now synchronizes on real frame URL/load and reacquires the frame.
   Intentionally superseded rapid navigations produce `net::ERR_ABORTED` requests,
   recorded separately from unexpected errors; settled destination assertions remain.
   The first extended old-pin control reached the expected wrong Back destination,
   then timed out reading a nonexistent expected profile. The corrected recorder
   records missing expected DOM as failure and continues identical BEFORE/AFTER
   assertions, rather than waiting for nonexistent old-control content.
5. Final-newline checking caught two newly created files without terminators;
   these were corrected and the unchanged checker passes.

No failed attempt is relabeled as passing or erased. No hosted rerun is authorized
without diagnosis. Browser and final hosted qualification details are completed
below and in the external final handoff before a ready disposition.

## Fresh browser qualification

The fresh campaign uses Chromium **156.0.8078.4**, local HTTP, viewports
**1440×1000** and **390×844**, light/dark and JavaScript enabled/disabled
(eight contexts). `browser_qualification.py`, screenshots, full publication
hashes, request observations and `browser-results.json` are external evidence.
The completed campaign records 2,169 observations, including one explicitly
expected old-pin control failure; all final-publication assertions pass.

Every context exercises all 29 package and eight tagged destinations against
actual iframe URL, owning native package, exact fragment, unique declaration
heading and complete native profile. JS contexts cover all 37 search labels,
case-insensitive substrings, no-results/count, Clear/focus, keyboard activation,
initial selection, aria-current, same-package and cross-package Back/Forward,
same-selection deduplication, distinct package-only state, approved inner links,
ordinary non-sidebar type navigation boundary, rapid selections, bookmarks,
native outer fragments and malformed/duplicate/unknown/wrong-owner query fallbacks.
Mutated/injected sidebar links cannot confer approved state. No-JS contexts keep
controls hidden, preserve both complete groups, native anchors, direct declaration
bookmarks, type links and portal entry/return. Across contexts, lexical colors,
keyboard focus, long-code scrolling, mobile scrolling and no new outer overflow
pass. No unexpected console/network errors or remote assets were observed.
Deliberately superseded navigation aborts are recorded separately.

Separate `browser_history_controls.py` applies identical meaningful assertions
to old/new experimental navigation-enabled controls through Job_Request →
Virtual_Aperture → Back → Forward twice, without stopping at the first expected
old failure. It records actual outer query/frame/fragment/heading/profile and
selection for each step. It additionally reloads all 37 approved final bookmarks.
This is a bounded Chromium campaign, not cross-engine certification.

## Review boundary

Search is limited to 37 existing native labels. Only exact sidebar destinations
gain outer selection/bookmarks/history, including the eight tagged declarations;
ordinary non-sidebar type/declaration links stay native. No full-text, complete
Ada declaration navigation, external integration, operation categories, generic
summaries or shared authored-guide rendering is claimed. Browser evidence is
bounded Chromium qualification, not cross-engine or accessibility certification.

One normal push and one non-draft review PR are intended. Final-head Bootstrap
push/PR and Documentation PR conclusions, actual checkout logs, synthetic ordered
parents/tree and all 42 hosted artifact bytes/audit must be verified. Deployment
skip on PR is expected. Stop for independent review: no merge, auto-merge, tag,
release, manual deployment or subsequent documentation phase.

## R2 reconciliation with independently reviewed, merged IR-CI-002

The sections above preserve the R1 campaign and its historical counts; they do
not replace the failed required Bootstrap push with later successful results.
R1 source head is `d5f397425c907bf27cecc115919cb48a5c65f940`, tree
`50333c5e6885eb7d9c062e7f35284dc01b523e4b`. Before reuse, the complete R1
evidence manifest and every listed file were verified. Manifest SHA-256:
`4a0162ce29b2e47c306a4e66f765c3956d3b374aad0ab1521b68a7c0194bb5c6`.
Original handoff, failed run, raw logs, diagnostics, publication, occurrence audit,
browser harnesses/results and screenshots remain untouched under
`/home/zboll/git/ams-mel-doc-007e-evidence/r1`.

Fetched main is the actual PR #118 merge
`592f72aa48202a4f5e4ee0890e66556dc0b739e2`, tree
`a490dad7710b20663b6913c30f60ff4c64d3344b`, merged on 2026-10-18.
Its ordered parents are `8be05e9d0284e81a1769b2e1639a4b96b191324c` and
independently reviewed R2 head `7cea5c1a7354a14055b21e275cbac57368d3d49b`;
its tree equals that reviewed head's tree. Normal integration merge
`bdd6379796c035cdb0d455eba171b656d99172ca` has ordered parents R1 head
`d5f397425c907bf27cecc115919cb48a5c65f940` then actual main
`592f72aa48202a4f5e4ee0890e66556dc0b739e2`. No cherry-pick, rebase or manual
recreation of the native correction was used.

### Three distinct findings, not a reconstructed hosted schedule

1. R1 [Bootstrap push 39097077370](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39097077370)
   failed the immediate zero-poll TIMEOUT assertion in Clang Debug's repeat gate.
   The unchanged mock could complete after 40 ms. This establishes a concrete
   timing assumption consistent with failure, not the unrecorded runtime
   interleaving or actual returned status.
2. IR-CI-002 R1 introduced held completion, retaining pending, close, result,
   cache, callback, consumed-handle and lifetime contracts. Its later hosted
   repetition aborts did not identify their abort sites.
3. IR-CI-002 R2 corrected a separately demonstrated logging-resource cleanup race:
   drain earlier completion owners, bind the exact created-owner target after
   submission, and wait for retirement before final log validation or cleanup.
   Acquire observation establishes completion of the relevant logging calls and
   file closes, **not OS-thread exit**. Controlled cleanup reproduction does not
   identify the original hosted abort sites. See the inherited
   [IR-CI-002 record](ir-ci-002-instrumentation-completion-handshake.md).

### Exact inherited bytes and authored boundary

All five PR #118 files are inherited directly from actual merged main, including
complete-file bytes and modes: its qualification record, Instrumentation C test,
mock, source-integrity checker and literal-fixture tests. Instrumentation test
SHA-256 is `617b95eae28527b383b4d05708dfc667e633047b88313a038272b52c2221eb02`;
mock SHA-256 is `5c816a106bd310443620921979a99ccda6ad830102ac9c65a63766ff4fd95b37`.
The immutable foundation, complete RF pair, IR-stream singleton, atomic
Instrumentation pair, literal fixtures and mechanical proposed-policy labels
are unchanged. No authorization hash is regenerated and no label is rewritten
to represent approval; the approved correction arrives through actual ancestry.

R2 authors only this file and `docs/gnatdoc-foundation.md`. All other DOC-007E
implementation files retain R1 bytes and modes. The complete PR still has the
same eleven documentation-owned paths against integrated main. Its out-of-scope
inventory is recomputed from that base, not copied from R1's historical 1,337;
every out-of-scope blob and mode must match. No native implementation/test,
policy, workflow, dependency, missing-type-link, Navigation_Report or additional
navigation feature is authored here.

### Integrated qualification method and evidence-reuse conditions

Complete these authored edits before final qualification, freeze the source/index
tree, and prove the subsequent committed tree is identical. Record final commit,
qualified tree, workflow/artifact identities and evidence-manifest digest in the
external R2 handoff and existing PR #117 description to avoid self-reference.
New evidence lives under `/home/zboll/git/ams-mel-doc-007e-evidence/r2`.
Pending or failed gates never count as passing; the external handoff supplies
the actual final disposition.

Use stable recorded absolute compiler, GNAT, GPRbuild, GNATdoc and Alire paths,
isolated build/output trees and no overlapping mutable-tree writers or readers.
Fresh non-editable installation uses the unchanged installer and the same
revision/archive/installed-assets/JavaScript identities recorded above. Discovery
is exactly `python3 -m unittest discover -s scripts -p 'test_*doc*.py' -v`:
**133 documentation tests**, including **51 source-integrity tests counted once**.
Pinned-enhancer discovery is `python -m unittest discover -s docs/tests -p
'test_*.py' -v`: **100 presentation tests = 66 retained + 34 navigation**,
or **233 distinct tests** overall. Investigate discrepancies, never omit cases.

Run sequentially, in this order: `make test-native` (require **434/434**),
`make test-ada`, `make check`, `make check-ada-format`, `alr -n -C ada build`,
`alr -n -C ada/tests run`; require exit zero from each. Verify working/index
integrity, baseline delta against actual main, exact scope/modes, **1,162**
protected files, **804** vendor checksums, whitespace and final newlines. No
tracked generated HTML, diagnostic instrumentation or evidence is allowed.
Exact inheritance avoids repeating the separate four-configuration IR-CI-002
local campaign, not the required final integrated hosted native matrix.

Produce two fresh pristine and two fresh enhanced generations; compare every
relative filename and complete-file byte. Independently validate using a fresh
external audit destination outside repository/pristine/publication, retaining
containment, existing-file and link-alias rejection. Require exact pristine
restoration, independent local-type occurrences, complete output/manifest
reconstruction, API byte-idempotence, dry-run source/staging/consumer preservation,
retained tamper/audit-output tests, and accurate post-return publication hashes.

The reconciliation target is the authenticated **42-file DOC-007E R1** publication,
not main's 41-file DOC-007D publication: **37 HTML pages, 29 public packages,
2,965 references, 1,418 local type links, zero external type links**. Require
complete names/bytes equality, including index, JavaScript, CSS, manifest, API
pages and portal, plus complete occurrence-audit equality with R1 and its preserved
DOC-007D audit. Exact equality carries forward the preserved compatibility A/B/C
results. Reuse R1's bounded Chromium campaign only when that authenticated
publication also equals both integrated local and downloaded final-head hosted
output. Explicitly report **browser-evidence reuse**, not a new browser campaign.
Unexpected differences or unauthenticated evidence block that reuse.

After a normal push to the existing branch, require final-head Bootstrap push,
Bootstrap pull_request and Documentation pull_request success (expected deploy
skip allowed). Inspect raw logs for actual checkout/compiler identities and all
eight native jobs' ordinary **434/434** and unchanged parallel-4/until-fail:50
repeat results. Preserve any new failure before diagnosis; never retry a test
failure to obtain green. Authenticate the final run's artifact and independently
validate all 42 hosted files, full audit and post-return hashes against local/R1.
Refresh actual main, remote/PR head and synthetic merge; require local = remote
= PR head, synthetic parents actual current main then final head, and synthetic
tree = independently computed merge tree = qualified source tree. Main advancement
requires inspection and appropriate normal integration/requalification.

Only all passing final-head gates and preservation checks permit
**READY_FOR_INDEPENDENT_REVIEW**. Keep existing PR #117 open, non-draft, unmerged,
auto-merge disabled. No merge of the PR, force-push, tag, release, manual deployment
or next documentation phase is part of R2.
