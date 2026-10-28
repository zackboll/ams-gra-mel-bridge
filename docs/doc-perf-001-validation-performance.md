# DOC-PERF-001: independent documentation validation performance

## Scope and starting authority

Work starts from fetched `origin/main`
`33720d155d99f183229b6ed87037ecbd53a3875b`, tree
`76d7988ea723a07ced054c0275e80763cbdcb772`, in the independent worktree
`/home/zboll/git/ams-mel-doc-perf-001`, branch
`perf/001-documentation-validation`. Preflight found open PRs #127 (CI-002)
and #129 (IR Ada coverage), and no open DOC-013 or documentation-performance
PR. Neither existing worktree nor either PR branch is modified.

The workflow is **Ada API Documentation**, GitHub workflow ID **384927174**,
path `.github/workflows/documentation.yml`, SHA-256
`f00cf46475d73cb6e9e70d1d202d5cd83a1e6bedec5c394732d838e9020ea9d9`.
The workflow, Bootstrap policy, six-shard ownership, inventory policy, protected
source, bindings, and executable examples remain unchanged.

## Measurement protocol and evidence

Raw evidence is external to the source tree at
`/home/zboll/git/ams-mel-doc-perf-001-evidence`. Its `original` detached Git
worktree retains unmodified main. Both candidates use Python 3.13.5, Alire
2.1.1, GNAT 16.1.0, GPRbuild crate 26.0.1, GNATdoc 26.0.0 (20260415), and
the exact reviewed enhancer revision
`524b7610e21e9a7b87e8e0761580ed59a3b0295c`. Enhancer installation uses the
existing archive and package authentication, not an editable install.

The external `measure.py` wrapper limits children to CPUs 2 and 3 and an 8 GiB
per-process virtual-address-space limit. It records monotonic wall time,
waited-child user/system CPU, and 50 ms samples of total live process-tree RSS
from `/proc`, plus child `getrusage` maximum RSS. The sampled tree peak can
miss sub-50-ms peaks; `getrusage` is not mislabeled as simultaneous tree RSS.
This is an AMD Ryzen 9 7945HX shared development machine, not an exclusive or
hosted runner. Affinity does not reserve CPUs or fix frequency. Raw environment
and timing variability must accompany any claimed speedup.

The unmodified implementation was genuinely profiled with:

```sh
cd /home/zboll/git/ams-mel-doc-perf-001-evidence/original
python3 /home/zboll/git/ams-mel-doc-perf-001-evidence/measure.py \
  /home/zboll/git/ams-mel-doc-perf-001-evidence/profile-original-build-qualified \
  .doc-tools/enhancer/bin/python -m cProfile \
  -o /home/zboll/git/ams-mel-doc-perf-001-evidence/original-build-qualified.prof \
  docs/tools/build_enhanced_site.py build --site _site-published
```

That successful run took 260.409332 s wall, 224.603333 s user CPU,
0.831204 s system CPU, and 61,014,016 bytes sampled peak tree RSS.
Profile timings include profiler overhead and are **not** unprofiled A/B
performance measurements. Cumulative values overlap and must not be summed.

| Original build operation | Calls | Cumulative profiler seconds |
| --- | ---: | ---: |
| `symbol_audit.inventory` | 10 | 199.630 |
| `gnatdoc_coverage_model.specification` | 3,667 | 200.061 |
| `declaration_index` | 3,667 | 157.311 |
| `qualified_original` | 2 | 136.522 |
| original `validate_docs.validate` | 3 | 124.993 |
| publication `validate` | 1 | 167.518 |
| `Authority.__init__` | 10 | 13.598 |
| enhancer `expected_output` | 4 | 25.723 |
| enhancer `verify` | 2 | 17.360 |
| enhancer authentication `tool` | 2 | 0.024 |
| complete input `files` | 7 | 0.033 |
| `hashes` | 17 | 0.015 |
| `fingerprint` | 15 | 0.018 |

An exploratory candidate build profile (not an A/B shard result) retains ten
symbol inventories but reduces total independent specification parses from
3,667 to 570; `Authority` constructions fall from ten to seven. Both candidates
still invoke the enhancer's `expected_output` four times and `verify` twice.
The candidate profile's 68.421 s elapsed time was collected on different
development affinity CPUs and is therefore not used for the target speedup
calculation. It identifies the remaining enhancer/source and HTML parsing work;
its raw profile is retained as `candidate-build.prof`.

Source parsing, not hashing or enhancer authentication, dominates. In particular,
the symbol auditor parsed the entire owning Ada package for each type instead
of once per owning source in each inventory. Catalog reconstruction and original
portal validation invoke this auditor repeatedly. Raw `.prof`, textual profile,
function counts, logs, and machine information are preserved externally.

Two failed setup profiles are retained but excluded: one raced regeneration of
its pristine input; another correctly rejected a symlinked enhancer environment.
Successful qualification uses separate regular-directory tool installations
and never regenerates a site concurrently with a reader in that checkout.

## Implementation and reuse model

1. `symbol_audit.inventory` keeps a local dictionary of immutable public-token
   tuples, parsed once per source path during that call, and a local dictionary
   of native page text for abbreviated enumeration checks. Every symbol still
   undergoes its own complete public-token inclusion, source/native profile,
   ordered enumeration literal, package owner, canonical identity, approved
   ledger destination, and destination-format checks.
2. Adjacent example-ledger and independent catalog checks share their freshly
   constructed `References` object. Both independent walks still execute.
3. Final API/publication fingerprints and the returned output map share hashes
   of the already captured immutable publication byte values.

No data is retained globally, on `Authority`, or on disk between invocations.
No timestamp, size, manifest, filename, or previous passing publication is a
cross-call identity. Dictionary lifetime ends when the call returns or raises;
memory is proportional to the source files and enumeration pages processed in
that invocation, not to the number of validation calls. Normal publication
qualification first checks the complete pinned source and native inventories.
Direct symbol-audit calls also reparse current source and reread native pages
on every invocation, even with a reused `Authority` object.

As before, authoritative directories must not be concurrently replaced while
qualification runs. This change introduces no concurrency guarantee and no
filesystem acceptance cache. The complete publication is inventoried anew on
every validation. Symlink and regular-file rules remain intact. Neither the
enhancer manifest nor output fingerprints replace independent semantics.

The pristine qualification, restored-native validator/coverage pass, enhancer
output reconstruction, both enhancer verifications during a build, independent
type-link occurrence audit/completeness guard, historical projections, and all
complete-byte publication assertions remain present.

## Inventory and additive invalidation controls

The original source-qualified inventory is **579** identities: **142** script
tests and **437** enhanced tests. Its complete JSON is retained externally.

| Shard | Modules | Tests |
| --- | --- | ---: |
| navigation | `test_navigation` | 34 |
| publication | `test_enhanced_site` | 35 |
| live | `test_live_docs_site` | 72 |
| symbols-catalog | `test_symbol_bookmarks`, `test_api_catalog` | 23 + 34 |
| types | `test_type_link_completeness`, `test_local_type_audit` | 33 + 19 |
| remaining | examples, metadata, audit, CI controls, Navigation observer | 187 |

No inherited test or inventory assertion is edited. Additional controls live in
`docs/tools/test_documentation_validation_performance.py`, following the existing
separate-tool-control pattern, and are run explicitly rather than silently
altering the frozen CI-001 inventory:

```sh
cd /home/zboll/git/ams-mel-doc-perf-001
.doc-tools/enhancer/bin/python docs/tools/test_documentation_validation_performance.py
```

They check per-source/per-call parsing counts and successive mutations of source
declarations, enumeration HTML, publication anchors, local links, required-link
removal, existing symbol destinations, public/private identities, JavaScript,
configuration, native ledger, pristine native input, installed implementation,
installed asset, extra files and symlinks. Publication mutations recompute forged
manifest hashes. Restored bytes are revalidated before later mutations; a reused
`Authority` must not retain a public-source projection from an earlier call.
These controls supplement, never replace, the inherited mutation suite.

The seven additive controls passed in 322.779 s in the initial development run,
including 24 expected mutation rejections. The source-derived original/candidate
inventory JSON files compare exactly. `make test-native` passed 434/434 native
tests and `make test-ada` passed. The source-integrity gate checked 1,162 protected
files; existing proposed-pending-independent-review labels for native test
transitions are preserved, not promoted to approved by this task.

## Publication identities

Two independent original pristine generations compare byte for byte, both with
fingerprint `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`.
Two independent original enhanced publications and two optimized enhanced
publications compare byte for byte across all 43 files, fingerprint
`2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
All existing fingerprint constants, warning/coverage baselines, renderer
exceptions, authored inventories, assets and manifests remain unchanged.

## Qualification status and rollback

Completed local checks include all 142 script tests, all 15 standalone tooling
controls (seven new plus eight existing), native-site validation, coverage,
enhancer verification, independent complete-publication validation, deterministic
regeneration, `make test-native`, `make test-ada`, `make check`,
`make check-ada-format`, `alr -n -C ada build`, `alr -n -C ada/tests run`,
and `sh docs/tools/qualify_examples.sh` (13 cases / nine programs). Existing GPR no-C-source
project warnings are retained in logs; no compiler warning policy is changed.

## Three-repetition performance qualification

The interrupted conversation did not stop the benchmark processes. On resumption,
no task benchmark processes remained; all 36 measurements had explicit exit code
0 and matching successful unittest reports. All 437 enhanced identities executed
exactly once in each candidate/repetition, with no failures, skips or omitted
mutations: 1,311 test executions per candidate, 2,622 overall. The unchanged
source-derived 579-test inventory (142 script / 437 enhanced) compares exactly.
Seven additive controls remain separately executed, not substituted into the
frozen inventory. Original raw files are preserved, never overwritten.

The full six-shard workloads ran sequentially, not concurrently; orchestration
was paused only between shards to interleave the first optimized workload.
No measured child was suspended. CPU affinity and address-space limits are equal.
The shared machine was substantially busier during the first original repeat;
retain all samples and use the median rather than selecting a favorable pair.

### Raw wall times (seconds)

| Shard | Original 1 | Original 2 | Original 3 | Candidate 1 | Candidate 2 | Candidate 3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| navigation | 985.726 | 479.158 | 434.790 | 113.048 | 87.014 | 88.272 |
| publication | 786.600 | 427.322 | 409.943 | 121.320 | 112.033 | 111.937 |
| live | 404.881 | 346.323 | 343.360 | 106.042 | 97.783 | 98.469 |
| symbols-catalog | 403.098 | 349.751 | 347.792 | 58.850 | 53.521 | 53.479 |
| types | 292.605 | 270.388 | 270.927 | 82.120 | 77.514 | 77.581 |
| remaining | 188.612 | 175.738 | 176.614 | 79.516 | 75.350 | 75.159 |

### Median wall, variability and CPU

| Shard | Original median (range), s | Candidate median (range), s | Wall reduction | Original / candidate median CPU, s |
| --- | ---: | ---: | ---: | ---: |
| navigation | 479.158 (434.790–985.726) | 88.272 (87.014–113.048) | 81.58% | 478.799 / 88.220 |
| publication | 427.322 (409.943–786.600) | 112.033 (111.937–121.320) | 73.78% | 427.169 / 111.997 |
| live | 346.323 (343.360–404.881) | 98.469 (97.783–106.042) | 71.57% | 333.012 / 84.952 |
| symbols-catalog | 349.751 (347.792–403.098) | 53.521 (53.479–58.850) | 84.70% | 349.628 / 53.503 |
| types | 270.927 (270.388–292.605) | 77.581 (77.514–82.120) | 71.36% | 270.840 / 77.552 |
| remaining | 176.614 (175.738–188.612) | 75.350 (75.159–79.516) | 57.34% | 161.551 / 60.348 |

Navigation reduction is `100 * (1 - 88.271937 / 479.158294) = 81.577709%`,
exceeding 25% on the required three-run medians. Median aggregate CPU decreases
from 2,019.618947 to 476.190465 s (76.42%). Median summed six-shard wall decreases
from 2,048.680619 to 504.897472 s (75.35%); this represents 34.145 versus 8.415
local-equivalent shard runner-minutes, not observed hosted billing.

With the actual six-shard fan-out, the simulated queue-free shard-only critical
path is the maximum shard per repetition: original 985.726 / 479.158 / 434.790 s,
candidate 121.320 / 112.033 / 111.937 s. Its median decreases 76.62%, from
479.158294 to 112.032604 s. Preparation, installation, aggregate, deployment and
queues are excluded. No observed hosted wall-clock improvement is claimed before
exact-final-head hosted jobs complete.

All per-test durations are retained in the event reports, including the separate
symbol, catalog, local-type and completeness modules. Method-duration sums exclude
class fixtures; shard wall above includes fixtures. Do not present method sums
as independent full-module timings.

### Memory and unequal ignored build products

| Shard | Original maximum sampled tree RSS, MiB | Candidate maximum sampled tree RSS, MiB |
| --- | ---: | ---: |
| navigation | 65.254 | 65.445 |
| publication | 73.160 | 70.168 |
| live | 84.430 | 85.891 |
| symbols-catalog | 73.652 | 74.242 |
| types | 77.703 | 78.059 |
| remaining | 308.730 | 1174.793 |

The remaining-shard increase is real in the recorded runs: candidate Python
parent RSS is about 588 MiB and a transient fork can double tree RSS. Investigation
found unequal ignored inputs: repository qualification compiled nine example
binaries (34,028,144 bytes) and object files in the candidate documentation tree,
but not in the baseline tree. The unchanged audit-output tests copy/read the
**entire** documentation directory, including ignored binaries, into retained
byte mappings. Therefore the raw remaining-shard memory comparison is confounded;
it cannot establish the optimization's memory effect on equal source-only inputs.
The original measurements and this caveat are retained. An isolated source-only
candidate worktree is used for a supplemental bounded remaining-shard measurement,
without deleting build products, modifying inherited tests or replacing raw runs.

That source-only control passed all 187 identities in 75.000238 s, CPU
59.898506 s, sampled peak tree RSS **294.218750 MiB** (308,510,720 bytes),
with Python parent peak 185.500 MiB. Original source-only peaks were
291.730 / 293.332 / 308.730 MiB. This supports the ignored-build-product
explanation; it is one supplemental observation, not a replacement three-run
memory series or a claim that every memory change is zero. The original 8 GiB
per-process bound was respected in all recorded measurements.

### Commands and evidence limitations

The preserved external `benchmark_original.sh`, `benchmark_candidate.sh`,
`run_suite.py` and `measure.py` specify the exact workload and resource envelope.
Each raw `<candidate>-<repeat>-<shard>.json` holds command, cwd, exit status, wall,
user/system CPU and sampled tree RSS; the matching `-tests.json` holds the complete
inventory and start/success/stop events, and `.log` holds complete output.
`resume-reconciled-three-repetitions.json` records the independently rechecked
raw arrays. The earlier sampler did not record UTC start/end timestamps or enforce
a workload timeout; those cannot be retroactively invented. Subsequent measurements
use `measure_bounded_resume.py`, exclusive evidence creation, UTC timestamps,
explicit bounded timeout and process-group cleanup on timeout. Existing successful
runs are not discarded or rerun merely to improve their timing presentation.

An additional final local qualification attempted the unchanged official CI
inventory, artifact seal, six shard commands and aggregate gate in explicitly
`local-not-deployable` mode; this cannot authorize deployment. It stopped on a
source-identity gate failure described below, so the official aggregate is **not
qualified**. The seven additive tooling controls are explicit local gates; they
are not automatically discovered by the unchanged frozen CI-001 workflow.

### Starting-main hosted reference (not candidate qualification)

The exact starting-main Documentation run
[`40445654064`](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/40445654064)
completed successfully: preparation 616 s, navigation 879 s, publication 1,066 s,
live 837 s, symbols-catalog 625 s, types 523 s, remaining 416 s, aggregate 17 s.
Its qualification-only queue-free path was 1,699 s; observed workflow wall was
11,643 s, including hosted scheduling and deployment/live verification. Summed
completed job runtime was 85.433 runner-minutes. The exact starting-main Bootstrap
run `40445654030` also succeeded. These establish a reference only; local timing
is not compared directly to hosted runtime to manufacture a candidate speedup.

## Historical stopped qualification (superseded by R1 continuation)

On resumption, all original three-repeat measurements were verified successful;
none was restarted, overwritten or terminated. The additional bounded official
runner then passed inventory, all 142 script identities, seal, navigation and
publication. Its live shard passed all 72 test identities, but the command exited
**1** after those tests with:

```text
Ran 72 tests in 99.737s

OK
documentation qualification rejected: test execution changed qualified source bytes
```

This is a genuine source-identity qualification failure, not a failing unittest
assertion or a benchmark timeout. Its recorded interval is
`2026-10-28T00:07:42.477821+00:00` through
`2026-10-28T00:09:22.633553+00:00`; the report document was edited at
`2026-10-28T00:09:20.897910+00:00`, inside that interval. Editing the report
changed local qualification's complete working-tree identity. The tooling source
hashes remain the same as the three-repeat benchmark candidate. The gate correctly
refused to accept a changed source identity; no validation is bypassed to recover.

The failed evidence is preserved as `resume-official-live.json`,
`resume-official-live.log`, and `resume-official-reports/live.json`. The dependent
supplemental sequence stopped with `official qualification did not complete
successfully; stop`; no standalone full-module timings or instrumented rejection
counts were collected by that sequence. Three-repeat per-test events and all
original mutation-suite successes remain available, but are not represented as
an official aggregate result. The seven additive controls previously exercised
24 expected rejection scenarios successfully; no exact total for every inherited
mutation is invented.

That attempt stopped without automatic retry, commit, push or PR submission.
The measured navigation target and byte equality remained established, but that
attempt did not qualify the official aggregate. Its evidence and stopped handoff
remain preserved without modification.

## DOC-PERF-001-R1: source-frozen completion protocol

The user explicitly authorized R1 to complete qualification. Preflight verified
that no prior qualification processes remained and that both implementation files
and the seven additive controls still match the preserved benchmark candidate.
The three successful original/optimized campaigns are retained, not restarted.

Main advanced through the reviewed CI-002 merge to
`7d5f88164bacfdc40add4b92c74cfaefbefad0a6`, tree
`2ce582703250815109cc203c393df5460068e455`. The feature branch fast-forwarded
to that revision before freezing. No rebase, amend, force-push or modification
to PR #127's worktree was performed. The optimization and additive controls are
unchanged. CI-002's inventory reconciliation, workflow, stress policy and
source-integrity transition are inherited byte-identically from current main.

The official current inventory is **650 distinct identities: 142 scripts and
508 enhanced**, retaining all original **579** identities and adding CI-002's
**71** required controls. Shard counts are 34 navigation, 35 publication, 72 live,
57 symbols/catalog, 52 types and 258 remaining. R1 runs the entire current
inventory; it never filters away the additions to reproduce a historical count.
Historical three-repeat performance figures above describe the original
437-enhanced-test workload; they are not relabeled as a benchmark of the added
CI-002 controls. The source/configuration/enhancer identities used by that
workload remain unchanged.

R1 finishes this report, stages all four task-owned files, and records the staged
Git tree and full file-level SHA-256 inventory with worktree/index parity in fresh
external evidence at
`/home/zboll/git/ams-mel-doc-perf-001-evidence/R1-source-frozen`.
From that checkpoint through the official aggregate and supplemental qualification,
no protected repository source, tests, workflow, documentation or report may be
edited. Only external evidence receives progress/results. Each qualification
command is bounded, captures its full output/status and process metrics, and
stops the sequence on failure. The official interface runs in explicitly
local-not-deployable mode and retains all source/artifact checks.

After inventory, script execution, sealing, all six shards and aggregate,
supplemental full-module measurements and actual expected-rejection accounting
run against the same frozen candidate, along with all standalone tooling controls.
Previously passing repository gates are reconciled against unchanged native,
Ada, binding and example inputs; any necessary rerun remains sequential.
All 650 actual identities and the historical 579 subset must reconcile exactly
once with zero skipped/failed/errored outcomes in the official run. Repetitions
for supplemental accounting remain separate reports, not duplicate official
identities.

Final results, frozen/committed tree equality, PR URL and exact-head hosted
Documentation/Bootstrap status belong in external evidence and the PR handoff,
not a progress edit to this frozen report. Submission is authorized only after
all required local gates pass. Hosted queued/running checks are pending, never
represented as green. Leave exactly one non-draft PR open, unmerged, without
auto-merge, approval, tagging, release or manual deployment.

Rollback is a normal reviewed revert of the task-owned tooling changes. Never
change publication fingerprints, bypass independent validation, alter stress
policy, force-push, or deploy to compensate for a failed qualification. A revert
still requires normal source/native/publication qualification.
