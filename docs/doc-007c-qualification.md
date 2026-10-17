# DOC-007C preflight qualification

R1 findings are preserved below. The **R2 qualification addendum** records the
resolved blocker and subsequent integration gates. Final review/hosted status
is supplied in the task handoff and external evidence.

## Disposition

**BLOCKED_BY_REAL_SITE_COMPATIBILITY** — stopped at the mandatory real-site
preflight. No enhanced publication, integration commit, push or PR was created.
The proposed TOML is schema-valid but is not an operational integration.

## Source and evidence

- Branch: `docs/007c-gnatdoc-presentation`.
- Worktree: `/home/zboll/git/ams-mel-doc-007c`.
- Starting/final inspected main and HEAD:
  `4b2ccbda6a1ad393b436f0e8f1caac9defb283c2`.
- Source tree: `96f765beac229b7d64246ddf85059326d65e7c27`.
- PR #113 was already merged when first checked; main was fetched again and
  remained unchanged. Other worktrees and evidence were preserved.
- External evidence: `/home/zboll/git/ams-mel-doc-007c-evidence/r1`.
- Pinned enhancer: `f2529eef6d3b6cceea1fed0d4347ae13129bf79b`;
  installed CLI/package: `0.1.0.dev0`.

## Pristine baseline: passed

Used existing qualified tools, not ambient GNAT 14 or GNATdoc 27:

| Tool | Recorded version |
| --- | --- |
| Alire | 2.1.1 |
| GNAT Native | 16.1.0 |
| GPRbuild distribution | 26.0.1 (`GPRBUILD 26.0.0 (2026-04-15)`) |
| GNATdoc | `GNATdoc 26.0.0 (20260415)` |

The unchanged `/home/zboll/git/ams-mel-doc-007c/scripts/generate_docs_baseline.sh`
ran successfully with `--generate public --style trailing --warnings`.
Its semantic spec compilation, warning ratchet/classification, existing portal
builder, source/native coverage reconciliation and site validation passed.

| Measurement | Actual |
| --- | --- |
| Published files / HTML pages | 39 / 37 |
| Public packages / local links | 29 / 1,516 |
| Source declarations / native sections | 1,238 / 1,237 |
| Documented elements | 4,294 / 4,295 |
| Public coverage gaps beyond renderer exception | 0 |
| Renderer exceptions | 1 exact Health Status Metadata Status renaming |
| Raw / public / private warnings | 73 / 0 / 73 |
| Existing documentation tests | 105 passed |
| Separately rerun source-integrity tests | 23 passed |
| Protected source files / POL-001 state | 1,162 / approved |
| Vendor checksum entries | 804 / 804 matched |

Commands included:

```sh
sh /home/zboll/git/ams-mel-doc-007c/scripts/generate_docs_baseline.sh
python3 -m unittest discover -s /home/zboll/git/ams-mel-doc-007c/scripts -p 'test_*doc*.py' -v
python3 -m unittest discover -s /home/zboll/git/ams-mel-doc-007c/scripts -p 'test_doc_source_integrity.py' -v
python3 /home/zboll/git/ams-mel-doc-007c/scripts/check_doc_source_integrity.py
python3 /home/zboll/git/ams-mel-doc-007c/scripts/check_doc_baseline_delta.py 291e97c4b1495b594b462982822fa48a2f439527
```

The first complete site was copied byte-for-byte to external `pristine-site/`
before adding the proposed config. Diagnostics, every native anchor/profile,
renderer exceptions, coverage/warning baselines and site map were saved there
separately. `pristine-file-sha256.json` records every published file. A fresh
generation compared with `diff -r` returned 0 with an empty diff. One combined
foreground tool call timed out during that initial regeneration; generation was
rerun to completion in the background with an explicitly recorded exit status.
Only the completed run is counted as deterministic qualification.

## Real-site enhancer preflight: failed

The exact proposed configuration was accepted by the pinned `config.load`.
An isolated staging root contains only `api/` copied from the pristine snapshot:
31 files, including 30 HTML pages and unchanged native CSS. No portal pages or
guide configuration were provided to the enhancer.

Actual CLI invocations are saved in external `reproduce-preflight.sh`:

```text
inventory           exit 2: gnatdoc-enhancer: unterminated record type
enhance --dry-run   exit 2: gnatdoc-enhancer: unterminated record type
```

See [source research](doc-007c-source-research.md) for the exact record/aspect
failure and narrow required shared-tool correction. External evidence contains
stdout/stderr/exit status for each invocation, traceback and failing scanner
locals, full original `ams-mel-rf-product_rx.ads`, exact failing declaration
slices, native signature/anchor entries, their native HTML sections, and the
complete pristine native Product_Rx page.

Before/after SHA-256 inventory comparison passed for all 60 Ada source files,
all 31 staging files, and all 39 consumer-site files. No presentation files or
manifest were emitted. The authoritative native anchors and original package
destinations remain qualified by the existing MEL baseline checks; they have
not been successfully reconciled by this enhancer.

## Scope and deferred gates

The only proposed MEL files are these three documentation-owned additions:

- `/home/zboll/git/ams-mel-doc-007c/docs/gnatdoc-enhance.toml`
- `/home/zboll/git/ams-mel-doc-007c/docs/doc-007c-source-research.md`
- `/home/zboll/git/ams-mel-doc-007c/docs/doc-007c-qualification.md`

They remain uncommitted for the blocked handoff. No tracked original file,
workflow, portal manifest, anchor/profile baseline, native/Ada/Rust/Python/
integration/vendor source, root README or source-integrity authorization changed.

Because the mandatory stop gate failed, no enhanced output inventory/link count,
theme or highlighting acceptance, independent final-site validator, presentation
mutation suite, enhancement determinism/idempotence, real-browser screenshots,
native/Ada regression run, Alire production build/run or hosted feature CI is
claimed. In particular, `434/434` native tests is an expectation, not a measured
result here. Browser qualification was not reached; this is not a claim that
browser tooling is unavailable. No PR HEAD, synthetic merge tree, remote feature
head or hosted DOC-007C Documentation/Bootstrap result exists for this attempt.

Resume only after an independently reviewed shared-tool correction is available
and an explicitly approved exact revision is pinned. Start again at the complete
real-site preflight; a successful narrow record test alone is insufficient.

## R2 qualification addendum

The preceding sections preserve the blocked R1 attempt. R2 resumes on the same
MEL main `4b2ccbda6a1ad393b436f0e8f1caac9defb283c2` and existing feature branch.
R1 evidence and its reproducer are unchanged. Original additions were copied
with hashes before editing to the new external evidence directory:
`/home/zboll/git/ams-mel-doc-007c-evidence/r2`.

### Reviewed shared-tool identity and correction

PR #4 is merged, with reviewed head
`d35596eb301d86cf97857762627d0e109e609630` and exact merged revision
`c1a691452cf436624809f4043b30087de6688052`. The merge tree equals the reviewed
head tree. The non-editable documentation-only installation uses the source
archive for that exact revision; its SHA-256 is
`d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493`.
Installed CLI/package version is `0.1.0.dev0`; the complete installed package
implementation/assets inventory fingerprint is
`384974fefbc2772ef6c79d43b275b549e5c12758535450a60c852c401e56ba1a`.
The installer and validator both fail on mismatched identities, without fallback.

Both real Complex_I16/I8 records now parse through their Convention C aspects
and reconcile with the original native snippets via the approved projection.
`AMS.MEL.RF.C2.Add_Job_Intervals` uniquely resolves its source target
`AMS.MEL.RF.C2.Add_RX_Job_Intervals` and native declaration anchor
`d7b837564e2f584656d3feabae350fd2ebfe358264570a6a780686f9f579b11c`.
The enhancer's full modeled-profile audit passes 1,203 profiles: 1,191 exact,
nine abbreviated enumeration profiles, two Convention C records and one
renaming. Zero unresolved mismatches. MEL independently reconciles all 1,238
source declarations / 1,237 native sections plus the exact renderer exception.
This distinction is retained because the enhancer does not model every kind.

### Original and final site integrity

All historical R1 measurement rows above remain unchanged in the fresh R2 run:
29 packages, 1,238/1,237 declarations/sections, 4,294/4,295 documented elements,
one exact omission, zero public gaps, 73/0/73 warnings and unchanged identities.
All 105 existing documentation tests and the separately rerun 23 policy tests
pass. POL-001 remains APPROVED for 1,162 protected files; its two approved hashes
are unchanged. All 804/804 vendor checksum entries match.

The pristine site is saved externally with its complete byte inventory and
made read-only. Independent fresh generation returns a zero-byte recursive diff.
The source integrity checker, anchor/profile manifest, warning/coverage baselines,
site map, portal sources, native/Ada/Rust/Python/integration/vendor sources and
root README remain unchanged. There is no policy authorization expansion.

| Measurement | Pristine | Published |
| --- | --- | --- |
| Files | 39 | 41 |
| HTML pages | 37 | 37 |
| Local links | 1,516 | 1,546 |
| Public packages | 29 | 29 |
| Native declaration sections | 1,237 | 1,237 |
| New assets | none | theme CSS and presentation manifest only |
| New JavaScript / type links | none | none |

The 30 native API HTML pages gain exactly the approved theme/viewport metadata
and lexical signature spans. The seven authored portal HTML pages, portal CSS
and native GNATdoc CSS remain byte-identical. Native filenames, declaration
source text, signatures, anchors, package navigation and portal destinations are
preserved. Full manifest reconstruction against trusted original bytes catches
forged-but-consistent output metadata. Both independent MEL and enhancer
validation systems pass. See the research addendum for the exact layered checks.

Inventory, dry-run, real enhancement and independent verification all pass on
API-only staging. Dry-run preserves all source, staging and consumer bytes.
Fresh enhanced generation compares byte-identically; already-enhanced API
reruns are byte-idempotent. Complete original/output hashes, source profiles,
tool versions and command results are saved externally, not published.

### Tests and real browser

All **35 new presentation tests** pass using real generated MEL input. They
cover missing/altered theme, missing viewport, duplicate theme, changed/swapped
anchors, wrong-but-existing declaration links, hidden text changes, nested and
out-of-snippet spans, executable JavaScript, inline handlers, unexpected files,
deleted packages, swapped package destinations, altered/forged manifest, native
CSS/portal mutation, changed source/authoritative profiles, broken links,
symlinks, deferred-feature config changes, unqualified tool version/content,
input tampering, dry-run nonmutation, deterministic generation and idempotence.
Profile negatives reject unsupported Convention, changed record fields,
missing/ambiguous renaming target and invalid native callable identity.

Chromium **156.0.8078.4** passes **223/223 browser assertions** on real pages:
AMS.MEL, IR.Image, IR.Track, RF.Product_Rx, RF.C2 and IR.Health_Status.Metadata.
Desktop 1440×1000 and mobile 390×844, light/dark system schemes, colored Ada
keywords/strings/numbers/attributes, code scroll containers, no page overflow,
native anchors/iframe browsing, portal navigation, Back/Forward, visible 3px
keyboard focus and JavaScript-disabled native/portal navigation were exercised.
No unexpected browser console errors. Eight representative screenshots and
the executed browser driver/results are in R2 evidence. This is Chromium
acceptance, not a claim of all-browser or comprehensive assistive-tech testing.

Initial project development failures are retained: an independent manifest
reconstruction initially misclassified public sources by matching nested
`private package` text; it was corrected to use MEL's existing library-header
inventory, not a new source parser. A generation/read overlap briefly caused
pristine fingerprint failures; generation was completed and tests rerun in
sequence. No checker was relaxed. Final CI serializes pristine generation,
original tests, deterministic regeneration, installation and presentation gates.

### Regression, CI and review boundary

Sequential `make test-native`, `make test-ada`, `make check` and
`make check-ada-format` all return 0. Native tests pass **434/434**; make check
also passes its build-tree isolation, native suite, formatting and Ada checks.
No mutable native build-tree writers overlap. `alr -n -C ada build` and
`alr -n -C ada/tests run` subsequently return 0, including the Ada contract
smoke suite. Exact qualified feature/PR head is recorded in final handoff evidence.

The workflow retains exact-source checkout, the existing pinned Ada toolchain,
all original tests and source/baseline checks, pristine deterministic generation,
then the hash-pinned enhancer installation, separate publication build/verify,
new mutations, independent enhanced regeneration and byte comparison. Only
verified `_site-published/` is uploaded for review or main-only Pages deployment.
PR permissions remain read-only; feature branches do not deploy.

The task-owned file inventory is the original three documentation additions,
`/home/zboll/git/ams-mel-doc-007c/docs/tools/build_enhanced_site.py`,
`/home/zboll/git/ams-mel-doc-007c/docs/tools/install_enhancer.sh`,
`/home/zboll/git/ams-mel-doc-007c/docs/tests/test_enhanced_site.py`, and updates to
`/home/zboll/git/ams-mel-doc-007c/docs/gnatdoc-foundation.md`,
`/home/zboll/git/ams-mel-doc-007c/.github/workflows/documentation.yml` and
`/home/zboll/git/ams-mel-doc-007c/.gitignore`. No generated output is tracked.

Type hyperlinks and enhanced navigation/search remain deliberately deferred.
No merge, tag, release or DOC-007D work is authorized by this qualification.
Hosted checks are reported separately for the exact PR head; unfinished jobs
are not treated as passing. The final handoff supplies commit/tree/PR identities
without attempting to embed a commit's own SHA inside itself.
