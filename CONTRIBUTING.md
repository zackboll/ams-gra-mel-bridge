# Contributing

This is an experimental bootstrap. Prefer small vertical slices: upstream
contract review, C ABI, C++ implementation, C consumer test, Ada import/wrapper,
Ada test, and coverage documentation in the same change.

Run `make check` with a native toolchain and GNAT/GPRbuild. Also run the Alire
commands in the README when changing manifests, GPR files, or build hooks.
Use an out-of-tree CMake directory when switching compilers or sanitizers.
Do not check in generated build products or modify vendored interfaces silently.

A pull request should state what changed, exact commands run, actual results,
unsupported cases, and any unresolved upstream behavior. Do not claim registry
readiness, real-provider interoperability, or formal proof based on smoke tests.

## Conflict-resistant parallel language development

Every feature must own a uniquely named task-specific Markdown document, for
example `docs/py-mel-009-<feature>.md`, `docs/irmel-ada-012-<feature>.md` or
`docs/rust-ir-007-<feature>.md`. Include:

- Coverage delta: what becomes implemented and what remains unsupported.
- Implementation boundaries, ownership/lifetime semantics and shared dependencies.
- Tests: exact source identities, commands run, actual results and unavailable
  checks, distinguishing mock qualification from real-provider validation.
- Limitations and unresolved upstream semantics, without inferred guarantees.

`docs/coverage.md` is a historical archive. Preserve its existing information;
future features must not prepend sections or add status prose to it. Task
documents replace those shared narrative updates, not authoritative coverage data.
Do not create a central feature-status index that every feature PR must edit.

Wherever practical, Ada, Python and Rust tasks should touch only their own
implementation/test files and uniquely owned feature documentation. Do not edit
shared `CHANGELOG.md`, generic README sections, `docs/architecture.md` or
`docs/c-abi-policy.md` merely to announce a feature. Necessary changes to the
native ABI, actual shared interfaces, security contracts and authoritative
structured coverage/API inventories remain required for correctness. Regenerate
their reports when applicable; the IR Ada inventory update procedure is in
`docs/reference/irmel-api-schema.md`. Do not omit a required shared change to
make a PR conflict-free, or substitute feature prose for reviewed evidence.

Start independent tasks from freshly fetched `main` on dedicated branches.
Uniquely owned paths let GitHub's normal **Update branch** merge bring unrelated
language work into a PR without shared status-file conflicts. This does not
guarantee conflict-free updates when code, inventories or contracts really
overlap: review and reconcile those changes normally. Do not add blanket
`merge=union` attributes or custom conflict resolvers that could silently combine
incompatible changes. CI workload selection and existing review/security gates
are unchanged.

For open PRs using the old convention, authors should move only their new
coverage/status prose into their uniquely owned task document, preserving all
information and leaving historical archive entries intact. Remove status-only
edits to other shared documents, but retain necessary contract, inventory and
generated-report updates. Then update from `main`, resolve genuine overlaps and
rerun applicable tests. This guidance is not authorization to modify another
author's PR or to approve, merge or enable auto-merge.

## Validation and release

Ordinary Ada/C++/Rust/Python development is not subject to frozen GNATdoc warning
or documentation-coverage baselines. Bootstrap still enforces compilation, tests,
ABI checks, dependency integrity and deployment security. The separate docs build
generates once from the current committed Git source and rejects actual build,
output-integrity and destination errors. See `docs/doc-ci-simplify-001.md` for
the pinned local docs command and non-deploying scheduled/manual qualification.

Before a first public release, complete the release checklist in
`docs/packaging.md` and select actual maintainer contact information.
