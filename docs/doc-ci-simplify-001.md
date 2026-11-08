# DOC-CI-SIMPLIFY-001 — source-derived documentation CI

## Contract and scope

The normal documentation build is independent of historical documentation
completeness. It compiles public Ada specifications semantically and runs GNATdoc
once against a clean export of the current Git commit. Compilation errors,
GNATdoc failures or skipped files, source/native declaration mismatches, unsafe
output, missing files, duplicate anchors and broken destinations still fail.
GNATdoc warnings and rendered source-backed coverage are retained as diagnostics
and summarized without enforcing a count, threshold or frozen identity list.

The site remains the Ada API/consumer portal; this task does not invent C++, Rust
or Python API documentation generators. Development in all four languages remains
subject to ordinary compilation/tests, not documentation completeness. There are
no new per-feature source locks, approval ledgers or renderer exceptions.

The existing pinned GNATdoc and reviewed enhancer remain unchanged. The useful
private-finalizer comments from PR #149 and coverage identifier/emphasis
normalization plus 13 regression tests from PR #152 are incorporated directly,
without depending on either open PR or importing their obsolete blocker reports.
The stale native-anchor ledger is not expanded: current source/native anchors are
measured before portal construction and passed explicitly to its destination
checks. Only a disposable export receives a ledger for the renderer.

## Normal path

`documentation.yml` runs for PRs targeting main, main pushes and manual builds.
It uses read-only contents/actions permissions and credential-free checkout.
Bootstrap's native matrix, stress classification, C consumers, ABI checks,
build-tree isolation and Ada/Rust/Python jobs remain unchanged. Its fast policy
target retains dependency/source security and Bootstrap policy controls, replacing
obsolete historical-epoch topology assertions with current artifact/security tests.

1. Validate exact checkout/index/source modes against Git, plus hosted event,
   repository, commit/tree, run ID and attempt where applicable.
2. Export committed Git objects, semantically compile and generate native docs.
3. Measure coverage and native anchors from current source/output; retain warnings.
4. Build the portal with those anchors. Verify every required file and destination.
5. Enhance once. Independently restore every native HTML byte; require unchanged
   authored pages/CSS, exact pinned script/theme assets and complete file inventory.
6. Seal pristine/published output, coverage, anchors and logs in a manifest and tar
   bundle. Publish bundle/manifest SHA-256 through successful job outputs.

Verification does not regenerate documentation. It recomputes source/native
reconciliation and validates presentation/destinations using a fresh source export.
Even a self-consistent modified presentation manifest does not authorize altered
native output. The old baseline comparator/ratchet utilities remain available for
historical replay and their own tests, but are not called by normal generation.
`generate_docs_baseline.sh` is a compatibility entry to the new current-source build.

## Deployment boundary

Only a successful main **push** build can supply Pages. PR, manual, scheduled and
local results cannot authorize deployment. Only the deploy job has `pages: write`
and `id-token: write`, scoped to the `github-pages` environment. Action pins are
unchanged. No `pull_request_target`, secrets or cross-run artifact selection exists.

The deploy job downloads the exact same-run immutable artifact ID, checks its
bundle and manifest digests against build outputs, rejects unsafe/duplicate tar
members, verifies every payload digest, binds the original source/tree and
run/attempt, and independently verifies the complete site before Pages upload.
The Pages artifact name includes run, attempt and source commit. Missing or failed
build/verification skips deployment; diagnostics cannot substitute for the bundle.
The read-only live job verifies the same artifact authority and compares every
served entity byte-for-byte with bounded retries. It reports failure, not rollback.

## Explicit qualification

`documentation-qualification.yml` runs weekly or manually, with no deployment
permissions or Pages actions. Its sole job, **Current-source qualification**,
performs repeated current-source generation, deterministic equality, deep
source/native and type-link audits, real source-addition/mutation regressions
and compiled example qualification. Check failures still fail the job; diagnostic
artifacts are retained even on failure. The pinned tools and 180-minute timeout
are unchanged.

DOC-CI-009 removes both historical replay matrix lanes and their replay step from
scheduled runs and manual dispatch. In scheduled run
[`41445598551`](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/41445598551),
current-source qualification passed, but both historical lanes failed with
`local mode forbidden in hosted CI`: `qualify_history.py` sets
`DOC_LOCAL_QUALIFICATION=1`, which the immutable historical runner rejects when
`GITHUB_ACTIONS=true`. Retiring automatic replay does not repair that obsolete
hosted/local mode conflict or change the normal documentation/Pages workflow.

Historical scripts remain available for explicit offline investigations; immutable
historical source identities, checkouts and original evidence are unchanged.
Neither scheduled nor manually dispatched qualification invokes historical replay.
Historical failures are not waived or promoted into current build requirements,
and retained historical bookkeeping tests do not execute full replay. Full replay
is not a claim made by a successful normal build or current-source qualification.

## Local commands

Install tools explicitly with `sh docs/tools/install_docs_toolchain.sh` on a fresh
tool directory, or reuse the documented pinned Alire/GNAT/GPRbuild/GNATdoc and
enhancer environment. Ordinary builds do not install documentation dependencies.
With a clean committed source and fresh `.doc-tools/current` / `current.tar`:

```sh
export GNATDOC_PREFIX="$PWD/.doc-tools/gnatdoc-26.0.0"
alr -n -s "$PWD/.doc-tools/settings" -C docs/toolchain exec -- sh -c \
  'cd ../..; make docs'
make test-docs-ci
make test-policy
```

Open `.doc-tools/current/published/index.html`; pristine output is alongside it.
The output is deliberately fresh-only, never an implicit destructive cleanup.
Local artifacts record `local-not-deployable` and cannot be promoted to hosted
deployment authority. Explicit deep qualification uses the same environment with
`docs/tools/docs_ci.py qualify --directory .doc-tools/qualification --gnatdoc-prefix "$GNATDOC_PREFIX"` under the pinned
enhancer interpreter.

## Validation

Local validation uses the existing pinned tool installations. The initial
`make test-native` attempt encountered pre-existing CMake caches from another
workspace; those generated directories were preserved outside this repository
and validation reran with fresh production/test trees. No source workaround or
test weakening was needed.

Validated commands include `make test-policy` (including dependency security,
source identity, historical bookkeeping, epoch boundaries, Bootstrap policy,
current pipeline and coverage tests), `make test-docs-ci`, the 42 warning parser
tests, actionlint 1.7.12 for all workflows, `make format-ada` and
`make check-ada-format`. The real normal build reports 73 warnings,
4,346/4,347 documented elements, 1,255 source declarations and 1,254 native
sections plus the existing renderer omission. All seven IR.NUC native entries
are regenerated. Native tests pass 435/435; Python tests pass 234/234; Rust
workspace tests, check, clippy with warnings denied and format checks pass
(three existing ignored Rust tests remain ignored).

Actual exact-head commands and hosted run links are recorded in the PR. No merge
or auto-merge is authorized. Full historical replay and real Pages deployment
are not prerequisites for, or claims of, this local normal-path validation.
