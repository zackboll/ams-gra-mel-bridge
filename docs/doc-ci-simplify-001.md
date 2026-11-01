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
permissions or Pages actions. Independent matrix jobs perform:

- current-source repeated generation, deterministic equality, deep type-link
  completeness, real source-addition/mutation regressions and compiled examples;
- immutable original historical replay, preserving original failures;
- separately identified corrected historical replay.

Historical failures are not waived or promoted into current build requirements.
Retained historical tooling/tests describe their epochs; their frozen topology
assertions are not applied to the new normal workflow. Full replay is expensive
and is not a claim made by a successful normal build.

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
`docs/tools/docs_ci.py qualify --gnatdoc-prefix "$GNATDOC_PREFIX"` under the pinned
enhancer interpreter.

## Validation

Actual command results and hosted run links are recorded in the PR. No merge or
auto-merge is authorized. Full historical replay and real Pages deployment are
not prerequisites for, or claims of, this local normal-path validation.
