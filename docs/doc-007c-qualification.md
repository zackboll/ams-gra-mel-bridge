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

## R3: documentation consistency and hosted GCC Release disposition

Starting reviewed HEAD is `2645c856fc08adb32e8f3cd3bd61d8e563079b6c`, tree
`f8b6f75bc80c88dc66d81ad8ccd696d256e5cbf3`, on the existing DOC-007C branch.
Fetched main remains `4b2ccbda6a1ad393b436f0e8f1caac9defb283c2`; the worktree
was clean and no other writer advanced the feature. PR #114 remains the sole
review PR. R1/R2 evidence and the exact approved enhancer/archive/package pins
are preserved. New evidence is outside the repository at
`/home/zboll/git/ams-mel-doc-007c-evidence/r3`.

### Documentation corrections and qualification

Only the necessary README documentation paragraph changes: native GNATdoc
declarations, anchors, profiles and package navigation remain authoritative;
published pages receive reviewed responsive light/dark styling and Ada lexical
highlighting. Type hyperlinks and enhanced search/navigation remain deferred,
and original output remains separately qualified. The rest of the consolidated
README is byte-identical to the reviewed revision.

The DOC-007C setup example now uses repository-relative install/build/verify/test
commands from the repository root. All four commands were actually executed,
including a fresh isolated installation at `.doc-tools/enhancer`. Previous local
installation/output directories were moved into R3 evidence rather than deleted.
Historical external evidence paths in research reports remain unchanged.

All 105 existing documentation tests, 35 presentation tests and 23 separately
rerun source-integrity tests pass. Two pristine generations and two independent
enhanced generations compare byte-for-byte, and both exactly match R2 output
inventories. The presentation suite also confirms enhanced-API idempotence.
No browser rerun is necessary: published bytes are unchanged, and R2 browser
evidence remains applicable to those exact bytes, not to any changed theme.

Sequential `make test-native` (434/434), `make test-ada`, `make check` and
`make check-ada-format` all return 0 in R3. Diagnostic builds/repetitions finish
before this regression sequence starts; no mutable native build-tree writers
overlap. The final 105/35/23 documentation/presentation/policy test rerun and
independent publication verification also pass after the R3 addendum edits.

Measurements remain 29 public packages, 1,238 source declarations, 1,237 native
sections, 4,294/4,295 documented elements, one exact renderer exception,
73 raw / 0 public / 73 private warnings, 39 pristine files / 37 HTML / 1,516
links and 41 published files / 37 HTML / 1,546 links. All 804 vendor checksums
match. POL-001 remains APPROVED with 1,162 protected source files. No source,
anchor/profile, warning/coverage baseline, policy hash, site map, enhancer pin,
implementation, workflow or presentation test changes are made in R3.

### Original hosted failure: exact assertion

The original failed push run `39046321478`, attempt 1, job `117435657074`, tested
the reviewed HEAD with GNU C/C++ 13.3.0 on Ubuntu 24.04. Its initial native run
passed 434/434. Its subsequent unchanged parallel repeat command failed
`ir_stream_contract` (test 419) at the destructor-close-gap mutation:

```text
stage=7 mutation=1 release=11 close=11
```

`AMS_MEL_PROVIDER_FAILED` is 11; `AMS_MEL_OK` is 0. At
`native/tests/test_ir_stream.c:2819`, the first disjunct,
`releaser.status != AMS_MEL_OK`, is therefore true. The closer's status is also
wrong. The combined assertion short-circuits before reading consumed stream or
counter predicates, so the original log alone cannot establish their values.
The printed lifetime log is a stage-5 snapshot, not the final stage-7 state.

All native Git blobs, production and tests, are identical between main and PR
#114. The unmodified isolated acquisition is also byte-identical. In particular:

| File | SHA-256 |
| --- | --- |
| `native/src/ir_stream.cpp` | `4011a04b76251e9c6e8237506f95dedee1367d3c7fe4629453098e465baa2566` |
| `native/tests/test_ir_stream.c` | `6a2bb092ce231dd794769194476500e9498f50d34bb30f8c290784a76e39d46b` |
| `native/tests/mock_provider.cpp` | `8c41c152f18eb9e96c1219d7c4073324719b5364d3a8fa458a59f396dda51666` |

### Isolated reproduction and synchronization evidence

Local diagnostic toolchain is GNU C/C++ 14.2.0 (Debian 14.2.0-19), CMake
3.31.6, not a claim to reproduce hosted GNU 13.3.0 exactly. The isolated Release
tree uses `-O3 -DNDEBUG`, C++20 privately, all existing warnings-as-errors,
and `AMS_MEL_BUILD_TESTS=ON`. Full configure/build logs, CMake cache and compile
commands are retained. The unmodified focused command passed 200 repetitions;
the hosted-equivalent `ctest --parallel 4 --repeat until-fail:50
--output-on-failure` suite passed all 434 tests. These passes do not disprove
the original intermittent failure.

The separate external `instrumented-source` checkout adds diagnostics and one
opt-in schedule-control barrier only; assertions, cleanup predicates, counters,
test order and negative-control semantics are not relaxed. Its complete diff is
saved as `diagnostic-only.patch`, never staged in this documentation worktree.
With `R3_FORCE_POST_DESTRUCTOR_GAP=1`, it pauses the release executor after
`operation.reset()` and before reacquiring the callback mutex, until cleanup's
existing post-drain recheck is observed. The actual bridge and mock paths run.
That deterministic schedule reproduces the original failure, exit 8:

```text
R3 post-drain retained=0 obligations=0 executor=1 uncertain=0 failed=0 late_uncertain=1
R3 stage7 mutation=1 release=11 close=11 stream=(nil) wrappers=1 callbacks=1 destructor_releases=1 baseline=1
```

The healthy case in the same instrumented run passes with release/close 0,
consumed stream, wrapper/callback counts 1, and unchanged destructor release
count. In the forced mutation, all three counter predicates and consumed-stream
predicate pass; only the statuses fail. No provider release failure or extra
destructor-driven release is observed in this schedule.

Source-backed sequence:

1. `ir_stream.cpp:1152–1162` deliberately exposes the premature-destruction gap
   only under the test failpoint: obligation/retained counts drop before wrapper
   destruction; the executor still belongs to the release thread.
2. `mock_provider.cpp:945–948` increments wrapper/callback counters within the
   destructor, before it returns to the bridge. This is not executor completion.
3. `test_ir_stream.c:2805–2811` sees callback count 1 and releases the paused
   cleanup claim. There is no happens-before edge requiring executor retirement
   before cleanup reaches its post-drain recheck.
4. `ir_stream.cpp:1899–1904` sees `release_executor_owned == true`; its safety
   recheck intentionally treats this as outstanding work. It publishes failed
   terminal cleanup. The snapshot closer adopts that failure at 2520–2524,
   producing the two observed status-11 results.

Classification: **A — synchronization error in the negative-control test**.
The negative control assumes a success status that its barriers do not order.
The supported diagnostic schedule explains the exact hosted status signature;
it is not evidence that an infrastructure timeout caused it, nor proof of an
ordinary production lifetime defect. Healthy production behavior and all
negative controls still need independent preservation in a corrective review.

### Separate correction required; no retry as a substitute

No native fix is made or proposed for inclusion in PR #114. A separate,
explicitly authorized corrective PR should retain direct evidence of the
forbidden cleanup claim while B's destructor is held, then establish a real
executor-retirement handshake before releasing physical cleanup. It must keep
the status/counter/lifetime assertions, healthy tests and post-drain guard, and
must not merely sleep, accept status 11, remove the mutation or bypass the guard.
Native source/test changes require independent review and the applicable
DOC-001/POL-001 policy authorization; this documentation task grants neither.

The instrumented but unforced full-contract run also passes 200 repetitions,
exit 0. Thus the ordinary local runs did not spontaneously reproduce the
failure; the explicit diagnostic interleaving does reproduce it. Those are
different evidence claims and neither is presented as a native fix.

No failed-job rerun was requested. The separately triggered original PR run
`39046364220` passed all Bootstrap jobs, including GCC Release, but does not
correct the failed push run or prove the intermittent defect is fixed. Original
failure logs and attempt identity are retained regardless of new-head CI.
The conservative review disposition remains **NATIVE_CI_BLOCKED** until the
native negative-control synchronization finding receives separate disposition;
even a green new-head run is not a permanent correction certificate.

## R4 reconciliation with independently reviewed, merged IR-CI-001

### Source identities and normal merge

The clean existing worktree and branch were reused; R1/R2/R3 and IR-CI-001
external evidence were preserved. New evidence is outside the repository at
`/home/zboll/git/ams-mel-doc-007c-evidence/r4`.

| Identity | Verified value |
| --- | --- |
| Reviewed R3 head / pre-merge branch head | `b7b3d324f5f9ca2f5929d08609be728ee7cddf55` |
| Reviewed R3 tree | `7a141db70cbf0ef3a46746740a1e8f8ed346dfe3` |
| Independently reviewed PR #115 head | `45b641e3a6f6389dbb4f2f077dba9722a677edaa` |
| Actual PR #115 merge / fetched current main | `4c72639579891c563425fd9c6c687ce64f892892` |
| Reviewed PR #115 / actual main tree | `5cfa05982f8a5aa2e4a682ff43c087bbf69b93d4` |
| DOC-007C normal merge commit | `0b5de24e24453bdffb9e96446fd36c6c9bf614b9` |
| Normal merge tree, before R4 record edits | `8592200546e4a82881a9e0df51109179d1f78afd` |

Main had not advanced beyond the supplied merge. Its parents are
`4b2ccbda6a1ad393b436f0e8f1caac9defb283c2` and the reviewed PR #115 head.
The normal DOC-007C merge has the R3 head as first parent and that verified main
as second parent. The reviewed branches changed disjoint paths; the normal
`ort` merge completed without conflicts or manual resolution. There was no
cherry-pick, rebase, force push or separate implementation of the correction.

All four PR #115 files are inherited byte-identically from current main:
`native/tests/test_ir_stream.c`, `scripts/check_doc_source_integrity.py`,
`scripts/test_doc_source_integrity.py` and
`docs/ir-ci-001-destructor-gap-retirement.md`. The corrected IR test SHA-256 is
`fa724b84a4c0ac06e77ffedd4fea81b225e20a0d5a28e12d02eaa2b1a277d400`.
Against old R3, these files change through main inheritance; against reconciled
current main they have **no PR-specific differences**. Only this report and
`gnatdoc-foundation.md` receive authored R4 edits; the PR retains its original
ten documentation-owned changed paths against current main.

The R3 **NATIVE_CI_BLOCKED** finding and original failed run remain above.
Its separately reviewed resolution is IR-CI-001: the mutation still proves the
forbidden cleanup claim while the destructor is held, then uses the existing
snapshot-completion hook to establish release-executor retirement before
resuming physical cleanup. The healthy ordering, success statuses, consumed
handle and exact counter/lifetime assertions remain intact. Production cleanup
guards are unchanged. R4 inherits that disposition rather than treating a new
green run as a substitute for diagnosis. The external forced BEFORE/AFTER native
diagnostics were **not repeated** in R4.

### Main post-merge checks

The complete all-events Actions inventory for actual main was reinspected;
both runs were terminal **success**, not pending checks counted as passing:

- [Bootstrap push 39054111249](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39054111249):
  all eight jobs succeeded, including all four native compiler/build variants.
- [Documentation push 39054111265](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/39054111265):
  build and main-only deploy both succeeded.

### Unchanged authority and presentation implementation

The enhancer installer, configuration, validator, presentation tests, workflow,
assets and reviewed README remain byte-identical to R3. Theme and lexical Ada
highlighting remain enabled; type linking, enhanced search/navigation, guides
and JavaScript remain disabled. Installation is fresh, isolated and non-editable
at `.doc-tools/enhancer-r4`, used consistently for all presentation commands.

| Pin | Unchanged value |
| --- | --- |
| Enhancer revision | `c1a691452cf436624809f4043b30087de6688052` |
| Installed version | `0.1.0.dev0` |
| Archive SHA-256 | `d4b422b73df60d6ed41ec534b71bd0248b43827ce54ed4c323455e225be17493` |
| Implementation/assets fingerprint | `384974fefbc2772ef6c79d43b275b549e5c12758535450a60c852c401e56ba1a` |
| SOURCE_SHA256 | `467cd45ef85888bb22ccee2d9e325b6b55e985dadac66c317dbbc653193114c8` |
| ANCHORS_SHA256 | `06b602439be15557118790a2827bb8bacc45c1b02d055fa3e9502204857f5aed` |
| PRISTINE_SHA256 | `0e6497d6172c918d04ea1c284785f7cfc0fec278e155ab254de0c78e81ac94e2` |
| DOC-001 source-integrity foundation | `42648a92eb0cc84b2980d16db6028992c0494ba1` |

No pin, baseline, expected count, manifest, renderer exception or implementation
was updated. POL-001 retains its complete original/approved RF pair. The exact
independent IR original/corrected singleton, `PROPOSED_IR_TEST_TRANSITION`,
comments, fixtures and `proposed_pending_independent_review` report label remain
unchanged from merged main. Independent review/merge is recorded here, not
encoded by a cosmetic policy rewrite.

### Documentation qualification and complete output equality

Used existing isolated Alire settings and GNATdoc installation with the actual
workflow command sequence. Qualified versions remain Alire 2.1.1, GNAT 16.1.0,
GPRbuild distribution 26.0.1 (binary 26.0.0, 2026-04-15), GNATdoc 26.0.0
(20260415), and Python 3.13.5. Generation and tests reading the same output ran
sequentially. The first complete pristine site and diagnostics were preserved
before a second fresh generation. Both complete filename/byte sets are equal.

| Gate | R4 result |
| --- | --- |
| `python3 -m unittest discover -s scripts -p 'test_*doc*.py' -v` | 116 passed, **including 34 source-integrity tests** |
| `python3 scripts/check_doc_source_integrity.py` | Passed; 1,162 protected files, approved RF, proposed IR mechanical state |
| Baseline-delta against verified current main | Passed; no expansion |
| Fresh isolated pinned enhancer installation | Passed, exact version/archive/implementation |
| Build and independent verification of `_site-published` | Passed |
| Presentation/profile discovery under `docs/tests` | 35 passed |
| Second enhanced build from separately preserved first pristine | Passed; complete bytes equal |
| Existing vendor checksum gate | 804/804 matched |

There are **151 distinct tests**, not 185: the 34 integrity tests are already
included in 116. No redundant separate integrity or mutation campaign is added.
The existing presentation suite exercises enhanced-API idempotence, dry-run
source/staging/consumer preservation and security/profile mutations.

Measurements remain **29 public packages**, **1,203 modeled enhancer profiles
with zero unresolved mismatches**, **1,238 source declarations / 1,237 native
sections**, coverage **4,294/4,295** with the same one exact Health Status Metadata
Status renaming exception, and warnings **73 raw / 0 public / 73 private-part**.
R4 inventory, coverage and warning report bytes also equal preserved R2; coverage
and warning bytes equal R3. Warning identities are unchanged. Pristine remains
**39 files / 37 HTML pages / 1,516 local links**; publication remains
**41 files / 37 HTML pages / 1,546 local links**.

Every relative filename and every complete-file byte was compared across R4's
two publications, preserved R3 first/second publications, R2's independently
generated publication and R2's downloaded hosted publication. All **41 files**
match, including `api/gnatdoc-theme.css` and `api/presentation.json`. External
inventories record every SHA-256, not merely counts or ZIP container hashes.
Native filenames, declaration text/signatures/anchors, destinations, native
stylesheet bytes and authored portal content are preserved.

Because the entire output is identical, the existing Chromium **156.0.8078.4**
qualification (**223 assertions plus two mobile horizontal-scroll checks**) is
reused for those exact bytes. **R4 did not rerun the browser campaign.** This is
not a new browser/accessibility certification or permission for unexplained
output drift.

### Preserved attempts and remaining limits

The required repository regression sequence completed **sequentially**, all
exit 0, with no overlapping mutable native build-tree writers:

| Command | R4 result |
| --- | --- |
| `make test-native` | 434/434 passed; registration count unchanged |
| `make test-ada` | Existing contracts and focused repeats passed |
| `make check` | Build-tree isolation, native, Ada format/contracts and hygiene passed |
| `make check-ada-format` | Passed; existing source-less C-project GPR warnings only |
| `alr -n -C ada build` | Passed |
| `alr -n -C ada/tests run` | Complete existing contract execution passed |

Actual native compilers were absolute `/usr/bin/gcc` and `/usr/bin/g++`, both
Debian 14.2.0-19; both final CMake caches record those selections. Qualified
GNAT/GPRbuild remained on PATH for Ada, avoiding IR-CI-001's documented relative
compiler/runtime-selection failure. CMake was 3.31.6. No extra native repetition
campaign was run; final-head hosted Bootstrap retains its ordinary suite and
parallel repeat gate.

After staging the R4 records, source integrity passed against matching index and
working-tree bytes, baseline-delta against verified current main passed, both
working/staged whitespace checks and the repository final-newline check passed,
and the inherited-file, unchanged implementation, ten-path scope and complete
publication identity checks passed. Independent publication verification also
passed using the separately preserved pristine input. Final gate logs are saved
externally; final commit identities are not inserted into this report.

R4 preparation observations are retained externally in
`preparation-observations.md`: initial optional path lookups found no `docs.yml`,
top-level vendor directory, R1-local settings or not-yet-written progress file.
The tracked workflow, actual checksum authority and existing isolated tool setup
were located before qualification. A progress-only sleep exceeded the foreground
tool timeout without interrupting the background sequential driver. These were
diagnostic lookup/progress errors, not failed test executions or unexplained
failures replaced with green retries. Full command logs and exit statuses are
preserved. Earlier R1/R2/R3 and IR-CI-001 failed attempts remain untouched.

The tool installation's development local pins are not registry readiness;
provider-free mock qualification is not real-hardware/arbitrary-provider
qualification. The exact-content checker still assumes trusted Git/Python and
no concurrent writers; equality proves content identity, not independent safety.
No first-party Ada source changed, so no formatting rewrite was necessary.

Final-head hosted results, artifact validation, final commit/tree, remote/PR
identity equality and synthetic merge identities are recorded in external R4
handoff evidence after publication, avoiding a self-referential report commit.
No required queued or running check may count as passed. PR #114 remains the
existing review vehicle; it must stay open, non-draft, unmerged, with auto-merge
disabled. No DOC-007D work is included.
