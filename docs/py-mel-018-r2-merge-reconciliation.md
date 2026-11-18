# PY-MEL-018-R2 — Reconcile current main in PR #233

## Scope and source identities

Ordinary merge of main `86eee19b252da563246d87072f4d71f69a377e30` into
`feature/py-mel-018-los-quaternion`, starting at feature commit
`df65105b17aaa12583fe8639df733a86609a366d`, tree
`0912a8dd975253d9ca4dad332ddc7b0735e4dda3`. The merge base is
`43f8acc8843a907dd91150961bc94023ec23c414`; pre-merge counts are six
feature-only and 24 main-only commits. The worktree was clean and the local,
remote and open non-draft PR heads matched. Auto-merge was disabled.

## Actual conflicts and resolution

- `docs/tests/source_epoch_regression_tests.py`: R1 and main independently
  repaired the same incomplete publication fixture. Retain real source-derived
  Python generation from the same exported synthetic commit as Rust, Ada/Rust/
  Python assembly, and the genuine `python-reference.json`. Use main's current
  helper composition and `python.log` filename. Retain the non-deployable
  synthetic identity boundary, every original assertion, current search label/
  count protections, and main's additional Python-page and publication-record
  corruption cases. No production verifier changes or fabricated records.
- `python/ams_mel/__init__.py`: main introduced Common/IR/RF public namespaces
  while the feature introduced quaternion exports. Import the four existing
  quaternion objects through `ams_mel.ir`, export them in that module's literal
  `__all__`, and retain the compatible root exports and canonical `ams_mel`
  introspection/pickle identities. No duplicate classes or implementation moves.

The domain partition, pickle and fresh-process import-order tests explicitly
cover the integrated quaternion exports and canonical `ForwardRightDown`.
The quaternion receiver, its existing tests, ODM receiver and native sources
are unchanged. Coverage delta: no new operation or native ABI; the existing
Python quaternion API is now also classified in the current IR namespace.
Structured native/Ada coverage inventories therefore require no changes.

## Qualification and limitations

Required local commands: `make native`, `make test-native`, `make test-python`,
`make test-rust`, `make test-policy`, `make check-ir-api-coverage`, `make check`,
`make test-docs-ci`, focused quaternion/ODM/identity/ABI tests, pinned
`make test-documentation-source`, documentation build/publication verification,
authored HTML and current search/resize qualification, source-integrity/newline
checks, and `git diff --check`. Actual exits, test counts, native export
comparison, final commit/tree and exact-HEAD hosted links are recorded in the
existing PR #233 qualification ledger, avoiding recursive self-hashes here.
Commands listed here are requirements, not claims of successful execution.

No native ABI/functionality, workflow, deployment authority, public class
identity or provider ownership change is authorized by this correction.
Qualification remains mock-provider-only; no hardware/Squall claim is made.
Independent review remains outstanding; no approval, merge or auto-merge is
performed. Pending hosted work is `HOSTED_CI_PENDING`, not a pass.
