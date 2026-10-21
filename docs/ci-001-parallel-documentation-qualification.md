# CI-001: parallel source-qualified documentation

## Baseline and scope

Implementation starts at DOC-012 merge `9288f6acd4cbf807f576d387db390459f0b117c0`,
tree `78bc1f5b5087847ff385debc60340b14461c20b7`, on the isolated branch
`ci/001-parallel-documentation-qualification` in
`/home/zboll/git/ams-mel-ci-001`. No existing CI-001 remote branch or open PR
existed at initialization. Production code, canonical examples, authored API
content, historical ledgers and `.github/workflows/ci.yml` are unchanged.

The previous historical directory `/home/zboll/git/ams-mel-doc-007b` exists only
as a marker and is not a Git worktree. DOC-012 recorded historical-worktree
disappearance. Its cause is unknown; no missing evidence is reconstructed or
claimed recovered. CI-001 uses a fresh independent clone/worktree and external
evidence at `/home/zboll/git/ams-mel-ci-001-evidence`.

DOC-012 main qualification is independent of feature development. Initial API
observations: Documentation run `39546475157` in progress; Bootstrap run
`39546475170` incomplete, with `native (clang, Release)` failed at
`ir_navigation_contract` (422), 433/434 tests passing. Those runs are neither
rerun nor cancelled by CI-001. Their final statuses must be reconciled before
merge approval; feature success cannot retrospectively qualify main.

## Topology and ownership

```text
prepare-documentation
  |-- enhanced-tests (navigation)
  |-- enhanced-tests (publication)
  |-- enhanced-tests (live)
  |-- enhanced-tests (symbols-catalog)
  |-- enhanced-tests (types)
  `-- enhanced-tests (remaining)
             |
       build (always evaluated)
             |
       deploy (main only)
             |
       verify-live (main only)
```

Preparation retains pinned Alire 2.1.1, GNAT 16.1.0, GPRbuild 26.0.1 and
GNATdoc 26.0.0. It generates pristine documentation, checks warnings, coverage,
source integrity and baseline deltas, compiles nine real Ada programs and runs
thirteen cases, compares two pristine generations, installs the immutable
reviewed enhancer, builds and independently verifies the complete publication,
and compares two enhanced generations. All 133 script tests execute here with
per-identity evidence. Preparation cannot upload Pages or deploy.

The six independently writable runner checkouts consume the same prepared
pristine/publication/diagnostics bytes. They never run GNATdoc source generation.
Existing class-level builds of mutation fixtures remain unchanged: these are
temporary enhanced-site test fixtures, not independent source qualification.
No cache or shared writable checkout/site is introduced.

| Shard | Entire owned modules | Prior measured module total |
| --- | --- | ---: |
| navigation | `test_navigation` | 17m46s |
| publication | `test_enhanced_site` | 16m32s |
| live | `test_live_docs_site` | 13m47s |
| symbols-catalog | `test_symbol_bookmarks`, `test_api_catalog` | 14m20s |
| types | `test_type_link_completeness`, `test_local_type_audit` | measured in retained DOC-012 log |
| remaining | `test_ada_examples`, `test_document_metadata`, `test_audit_output`, `test_examples`, additive `test_documentation_ci` | measured in retained DOC-012 log |

These are initial module-level assignments, not claims of final runtime balance.
Full prior hosted logs are retained from run `39537915912`. New reports include
every test's measured elapsed seconds, including setup/teardown inside each
test; job measurements include class fixtures and tool installation.

## Exact-once reconciliation

`docs/tools/documentation_ci.py` uses only the Python standard library. Normal
unittest source discovery includes dynamic generated test methods; import errors
fail inventory creation. The immutable inherited JSON enumerates all 497
identities (133 scripts, 364 enhanced). Inherited test sources are also
compared byte-for-byte against the DOC-012 merge, preserving assertions, with
one narrow exception: the two `WorkflowTests` in `test_live_docs_site.py` assert
the new dependency/artifact/discovery topology instead of obsolete serial
command strings. Their identities remain; main-only/read-only/security guards
remain and exact-ID/gate assertions are additive. The rest of that module's AST,
including every publication/HTTP/security assertion, must equal the baseline.

Each discovered enhanced identity must map to exactly one shard. Unknown modules,
duplicate identities/owners, missing owners, empty shards and deleted inherited
identities fail. Each shard records actual unittest start, outcome and stop
callbacks, not just selection or counts. Aggregation requires exactly one ordered
start/success/stop sequence for every assigned identity, no unknown identities,
no skipped/expected-failure/error/subtest-failure events and consistent totals.
Setup failures cannot produce fictitious test execution evidence.

All six result artifacts are required. Matrix fail-fast is disabled so another
shard's failure does not suppress diagnostics. `build` uses `if: always()` and
checks both preparation and matrix dependency results before downloading. Failed,
cancelled, skipped or missing dependencies fail the aggregate. A missing or
malformed report also fails. Whole-workflow cancellation cannot produce a green
aggregate. Additive controls exercise these cases, wrong SHA/run/attempt,
duplicate execution, omitted new tests, forged totals, changed publication,
unsafe archives and unqualified deployment.

## Artifact identities and trust boundaries

Preparation seals only explicit regular files from `_site`, `_site-published`
and `obj/docs`, plus inventory, script results and a manifest. The manifest binds
every payload path/hash to repository, source commit/tree, workflow run and
attempt, inventory digest and script-report digest. Symlinks are rejected.
The bundle and canonical manifest SHA-256 values flow through preparation job
outputs, independently of downloaded file contents.

The v4 upload action creates an immutable artifact ID. Every consumer downloads
that exact ID from the current run, checks the bundle digest, safely unpacks only
approved paths into nonexistent private destinations, rediscovers the inventory
from its checkout, and checks every payload byte. Shards check source and input
integrity again after execution. Reports bind the same artifact ID/name/digests,
source tree, SHA, run and attempt. Artifact names contain all three run identities.
Duplicate JSON keys and duplicate shard result artifacts are rejected.

Only successful `build` emits an authorization gate, bound to the full result
digests and qualified input identity. Deploy downloads its exact gate artifact
ID, verifies its separately conveyed SHA-256, rechecks inputs and authorization,
then creates the run/attempt/SHA-specific Pages artifact. Both deploy and live
verification are main-only and depend on successful aggregate authorization.
Live verification reuses those exact same-run pristine/publication inputs and
the original unchanged verifier, retaining all HTTP byte comparisons, freshness
and transport-security checks. PRs never deploy.

This protects against accidental/cross-run substitution and malformed evidence;
it is not an attestation against a malicious workflow author or a compromised
GitHub runner. Review of workflow/tool changes and protected-branch policy remain
required. Do not treat a JSON boolean or numerical count alone as authority.

## Publication invariants

The existing independent verifier still requires all 43 files, 38 HTML pages,
4,052 internal references, native declarations, symbols and direct executable
example links. No publication bytes are authorized to change. DOC-012 canonical
mapping fingerprints remain:

* Pristine: `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`.
* Published: `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.

Baseline copies are generated with the unchanged existing tooling before local
candidate qualification, then compared recursively with candidate outputs.
Historical API-only fingerprints and source-integrity rules are unchanged.

## Branch protection and future tests

Require the exact `build` check from this workflow, not preparation, an individual
matrix entry or a check allowed to skip after dependency failure. Also retain
the independent Bootstrap required checks. Initial API inspection reported main
as **not protected**, with no repository rulesets returned; this task does not
change repository settings. A maintainer must establish required checks before
merge. Retaining a check name does not create branch protection by itself.

To add tests, keep normal `unittest` discovery conventions. New methods in an
owned additive module automatically join that shard and inventory. The inherited
DOC-012 modules remain frozen (apart from the two reviewed workflow-shape tests);
put new cases in additive modules rather than
editing their assertions. Add a new module to
exactly one `SHARDS` owner in `docs/tools/documentation_ci.py`; an unassigned
module deliberately fails preparation. Never edit the inherited inventory or
inherited tests to hide omissions. If ownership changes, run all controls and
the entire script/shard suites, reconcile all reports, compare publication bytes,
then qualify the exact final head on GitHub again. Use per-test and job timings
to rebalance without changing identities/assertions.

## Qualification, measurement and rollback

Run the repository gates sequentially: `make test-native`, `make test-ada`,
`make check`, `make check-ada-format`, `alr -n -C ada build`,
`alr -n -C ada/tests run`, `sh docs/tools/qualify_examples.sh`, using the pinned
toolchain. Then execute preparation, all shard owners and aggregate reconciliation.
Retain local logs, full inventories, manifests, individual reports and gate.

Hosted collection command (run from the isolated checkout):

```sh
python3 docs/tools/measure_documentation_ci.py RUN_ID \
  --output /home/zboll/git/ams-mel-ci-001-evidence/hosted-RUN_ID.json
```

It records run/attempt/SHA, every job/step, queue delays, runner durations and
artifact API identities. Compare preparation + longest shard + aggregate with
the **88m42s** baseline (enhanced suite **78m53s**), also reporting workflow wall
time including runner queues and summed runner-minutes. A 25–35-minute target
does not authorize weaker checks. Pending runs yield explicitly partial data,
not a demonstrated performance improvement. Final hosted results and any
corrective iterations belong in the PR handoff so recording them does not
invalidate an already-qualified head with a documentation-only follow-up commit.

Local environment corrections retained in evidence: `/tmp` inode exhaustion
during the first enhancer install (retried successfully using task-owned TMPDIR);
an initial gate launch remained in `docs/toolchain` because `alr -C ... exec`
changes working directory (restarted the complete sequence with explicit root).
Neither failure is counted as a passing gate. The first complete local shard
attempt also rejected the obsolete serial workflow-shape assertions in the live
module. Those two tests were migrated to equivalent/stronger new-topology
assertions; complete local qualification was restarted. No HTTP, publication,
source-integrity, or native test assertion was weakened.

Rollback is a reviewed normal revert of CI-001's workflow and documentation CI
tools to the DOC-012 serial workflow. Never force-push/rebase main, alter approved
publication fingerprints, bypass qualification, or manually deploy to compensate
for a failing shard. A revert still requires full source/native/publication and
hosted qualification. Leave this feature PR open, non-draft, unmerged and without
auto-merge; stop at independent review.

Before committing, local execution may set `DOC_LOCAL_QUALIFICATION=1` with
explicit local run/attempt identifiers and the baseline `DOC_SOURCE_SHA`. This
mode hashes all tracked files and new documentation/script/workflow source files
into the identity,
labels evidence `local-not-deployable`, rejects use inside GitHub Actions, and
cannot authorize deployment. Hosted qualification always requires a clean exact
commit checkout. Local evidence is not represented as hosted exact-head proof.
