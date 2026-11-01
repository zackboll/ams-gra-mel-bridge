# PY-POL-001-R1 — ordinary development without source authorization

## Development contract

Native/C++, Ada, Rust, Python, tests, build tooling and documentation evolve
through ordinary feature branches, PRs, functional/ABI testing, hosted CI and
code review. No exact filename allowlist, production-byte freeze, historical
current-test comparison, policy-only route, approval registry or exact-content
transition table is required for normal development. Additions, modifications,
removals and renames are source deltas, not authorization requests.

PR #142 is updated in place. No new PR, force push, auto-merge or publication.
PR #140 stays unchanged until this correction is qualified. Its exact head is
`e738a001e620b85abf2a38cb32b1ffad24713c4c`; its three byte-identical Python files
pass the normal revised source checkers in a disposable checkout.

This revision supersedes the narrow PY-POL-001 proposal and the development
admission requirements of earlier policy task records. Their original evidence
is not rewritten: the complete preceding proposal and failed campaigns remain
at Git commit `32d375aeb76e7b19ec42079523d5c8ab9c65f493` and in the external
`/home/zboll/git/ams-py-pol-001-evidence/` directory. In particular, the pinned
pre-R1 epoch rerun finished 180 tests with two authorization errors, exit 1;
those were historical-policy incompatibilities, not functional binding defects.

## Removed development gates

- `feature_source.py` records full Git source/tree/delta identity without
  filename permissions or private-binding freezes. Clean index/worktree and
  regular file checks prevent substitution of unqualified inputs.
- `check_doc_source_integrity.py` checks the existing 804-file dependency
  checksum document and CI privilege/artifact boundaries, not first-party bytes.
- `documentation_ci.py` discovers current tests, assigns new modules to the
  remaining shard and seals their current identities/hashes. It does not compare
  their source or method bodies with inherited tests or require frozen totals.
- The IR API audit retains pinned upstream evidence, coverage evidence and vendor
  integrity, but no longer freezes the current implementation to its baseline
  source hashes. Those hashes remain unchanged historical evidence.
- Obsolete source-admission tests and exact-content transition tables are removed.
  Retained event, callback, failure-diagnostic, artifact and deployment controls
  test real security/compatibility properties rather than historical filenames.
- Pages publication no longer consults the empty per-feature approval ledger.
  It requires successful main CI and rechecks source/tree/run/artifact digests.

## Retained controls

Bootstrap PR permissions remain read-only. There is no `pull_request_target`,
PR deployment or credential-bearing test job. Documentation still triggers only
on pushes to `main`; its sole Pages-writing job retains the main-only guard,
required aggregate dependencies, exact same-run artifact IDs, verified bundle/
manifest/gate digests and source-bound reconstruction before Pages upload.
Deployment code separately requires a hosted `push` to `refs/heads/main` whose
source SHA matches the event and qualified artifact. Local qualification cannot
publish. Workflow security mutation regressions exercise these requirements.

Native C11/C++20 tests, release-active contracts, C export/layout compatibility,
Ada ownership tests and Rust/Python ABI/layout and functional tests remain.
Vendor bytes must match `docs/upstream-files.sha256.md`; native dependency closure
checks still exclude unpinned system/sibling upstream headers. No new dependency
or runtime/public ABI is introduced.

`python/tests/test_import_identity.py` checks the actual imported
`ams_mel._native.__file__`, module identity and loaded ctypes library. A negative
fixture adds a functional-looking `_native/` package and proves that the same
identity contract detects shadowing. This is tested behavior, not a filename
approval rule or a change to the Python import mechanism.

## Historical qualification

Original epoch `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`, is cloned in isolation. Its own
unchanged generator, integrity checker, test inventory and original tests run
there. Current source is never overlaid onto it. The pre-existing corrected
replay remains separately identified; it is not relabeled as original execution.
Original failures and logs retain their actual exit statuses and are sealed as
historical diagnostics, not rewritten to pass or used to freeze current tests.

Reproduce with the pinned GNAT 16.1/GPRbuild 26 environment and exact enhancer:

```sh
/path/to/pinned/enhancer/bin/python docs/tools/qualify_history.py \
  --output /absolute/fresh/history-output \
  --gnatdoc-prefix /absolute/gnatdoc-26.0.0
```

The optional `--corrected-replay` uses the already existing replay and records
its distinct identity. An original known fixture failure remains a failure in
`results.json` and shard logs, not a fabricated success. Bootstrap also executes
the original 60 source-integrity tests in their immutable checkout as a small
historical-isolation regression; they do not constrain today's sources.

## Qualification commands and evidence

```sh
make test-policy
python3 scripts/check_doc_source_integrity.py
make test-native
make test-ada
make test-python
make test-rust
make check
alr -n -C ada build
alr -n -C ada/tests run
cargo check --manifest-path rust/Cargo.toml --workspace
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
```

Run native-tree writers serially. No Ada source is modified by this task.
Current documentation controls use the pinned enhancer interpreter.
Final command results, exact commit/tree and hosted Bootstrap conclusions are
recorded in PR #142 and `/home/zboll/git/ams-py-pol-001-r1-evidence/`. Pending or
failed checks are not reported as successful. Original historical fixture
failures are distinguished from actual current functional/security defects.

After qualification, resume Python MEL binding implementation with PR #140;
no further source-policy task is a prerequisite for bindings over existing C
interfaces. Merge/approval and implementation of that feature are not performed
by this revision.
