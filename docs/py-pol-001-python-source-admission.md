# PY-POL-001 — Python safe API source admission (proposal)

This is a source-policy proposal, **not independent authorization**. No approval
ledger, publication epoch, CI trigger, or exact-content transition is added.
The correction must receive independent policy review before it can be trusted
as main's evaluator. Nothing here modifies, approves, or merges PR #140.

## Reproduction and authority

Fetched `origin/main`: `b4a550adf222b99a46b3218d00c4b080ec314a25`, tree
`9167c94b7b9183c01ecb3c63fccc8d90f9831d31`. Fetched `pull/140/head` and the
GitHub API both identify `e738a001e620b85abf2a38cb32b1ffad24713c4c`, tree
`9ac12012e694c6f80d7b0875ceb7a1a1f382ae8c`. The feature is exactly three paths:
`python/README.md`, `python/ams_mel/__init__.py`, and the added
`python/tests/test_rf_admin.py`. The corrective branch starts from fetched main
in a separate worktree; the reproduction worktree is detached at the feature.

Both original commands exited 1 in the pristine detached feature worktree:

```sh
python3 docs/tools/feature_source.py --root "$PWD"
python3 scripts/check_doc_source_integrity.py
```

POL-002 rejected `out-of-scope delta: python/README.md`. Its exact RF ownership
list does not include safe Python implementations, tests, or their README; its
general documentation classifier only includes the root README and selected
`docs/` prose. The later implementation and test paths would also be rejected.

DOC-001 rejected `unauthorized tracked files added:
['python/tests/test_rf_admin.py']`. This is a different authority: the frozen
DOC-001 foundation inventory (`42648a9`) and explicit additions. After inventory
admission, its production-byte comparison would independently reject changes
to `python/ams_mel/__init__.py` and `python/README.md`.

## Ownership correction

Both evaluators admit ordinary Python source additions and modifications,
without per-feature filenames, commit locks, manifests, or content hashes:

- `python/ams_mel/` and `python/tests/`: nested `.py` modules with ASCII Python
  identifier path components; **not** an arbitrary directory-content exemption.
- Exactly `python/README.md`: ordinary Python API prose. POL-002 classifies a
  README-only change as documentation-only; implementation/tests remain feature
  source and cannot use documentation-only qualification.
- `_native.py` and `test_abi.py` are excluded at every nesting depth. Existing
  private ctypes declarations and ABI tests retain their previous RF/shared
  surface review or DOC-001 production-byte authority. `abi_probe.c` and any
  other C/native fixtures are not Python-owned `.py` modules.
- Generated/cache/build/distribution/vendor subdirectories are excluded:
  `__pycache__`, `generated`, `_generated`, `build`, `dist`, and `vendor`.
  Bytecode, binaries, arbitrary fixtures, and other extensions are not admitted.
- Only regular nonexecutable Git mode `100644` is admitted. Existing removals,
  renames (delete/add pairs), executable transitions, links, gitlinks, FIFOs,
  directories, unmerged entries, traversal/ambiguous spellings, and inconsistent
  index/working-tree views are rejected, not read as authorized source.

The two small predicates intentionally live in their existing evaluators, so
neither tool gains a new executable dependency or candidate-controlled policy
manifest. Classification parity is regression-tested. The integrity checker
validates admitted Python paths before exempting their **bytes** from the old
production freeze; their inventory/modes/index/working views remain checked.

No C ABI, native implementation/mock, Ada, Rust, workflow, vendor, publication,
or protected documentation-input authority is broadened. Separately authorized
RF and documentation routes keep their prior decisions and review obligations;
mechanical acceptance still returns `independent_approval=False` and
`publication_epoch_approved=False`. No Python source admission grants permission
to alter a protected file. Ordinary code review remains responsible for safe API
semantics; this is ownership admission, not a Python sandbox or proof of FFI
correctness. Future private binding locations or generated-source conventions
need explicit policy review rather than relying on this safe-source route.

## Regression qualification

Run the additional suite explicitly (outside the frozen historical discovery):

```sh
python3 -W error -m unittest discover -s docs/tests -p py_pol_001_tests.py -v
```

All destructive cases use temporary Git repositories or shared-object clones;
no tracked feature or corrective worktree source is destructively tested.
Positive cases cover unchanged fixtures and real main, existing/nested safe
implementation, new nested tests, README-only, combined changes, and the exact
PR #140 delta. Adversarial cases cover all ownership classes above, protected
documentation inputs and generated output, with and without safe Python edits.
Existing-authority cases compare old and corrected evaluator decisions and
review obligations for both replacement and additive changes. Filesystem cases
cover symlinks (including parents), executable bits, gitlinks, FIFOs,
directories, index/working divergence, unmerged entries, deletion and rename.

Three inherited negative fixtures previously rejected all safe Python changes.
They now test an excluded private binding or an unowned Python-root file instead;
the new suite explicitly tests both new safe-source acceptance and retained
private ABI rejection. No historical method is removed and no assertion is
disabled. The DOC-CI-005 exact historical successor fixture now reads its frozen
successor Git blobs instead of today's policy files; all original exact hashes
and assertions remain unchanged. This avoids misrepresenting the current
correction as part of that older exact transition. Other inherited policy tests
are unchanged.

### Exact PR #140 admission proof

`ExactFeatureTests` constructs a **synthetic future trusted target only inside
a disposable clone**, starting at verified main with the two corrected policy
files. It then commits the exact three Python blobs from the verified feature
object and runs both CLI validators. Both exit 0. It compares all three files
byte-for-byte and records SHA-256 identities:

| Feature file | SHA-256 |
| --- | --- |
| `python/README.md` | `eade0e2d31196f6c1638b23845254afebc63e74daa47e5181e13d0cd686bd44d` |
| `python/ams_mel/__init__.py` | `7f5e3d3065d82773308e648e02e8036c1299e46b998027d6d70d83d61bfdb5b0` |
| `python/tests/test_rf_admin.py` | `14e8153e3a4b735a65f106acab3617b5fa1c8d4544e602eeeff48988978175b2` |

The resulting qualification tree is
`d82c4fd4ac0141db7ba3633fde8f748a3788d821`. Synthetic commit identities depend
on test execution time and are printed in the test log, never trusted as policy
authority. Added native, private ctypes, and vendor mutations on that exact
feature overlay are rejected by both evaluators. The actual PR ref is checked
unchanged. The real `origin/main` ref is never changed by this fixture.

The self-authorization negative control proves a candidate cannot change its
evaluator and that a candidate-supplied CLI fails trusted-target byte identity.
The actual policy PR is expected to require independent authorization: this
qualification does not make the proposal its own trusted main.

## Local evidence and commands

Logs are retained outside the source worktree at
`/home/zboll/git/ams-py-pol-001-evidence/`. Commands run from the corrective
worktree unless noted. Successful policy runs use `python3 -W error` and
`-m unittest discover -v` with the following source directories/patterns:

| Directory | Pattern | Tests | Exit |
| --- | --- | ---: | ---: |
| `docs/tests` | `py_pol_001_tests.py` | 16 | 0 |
| `scripts` | `test_doc_source_integrity.py` | 60 | 0 |
| `docs/tests` | `test_bootstrap_ci_policy.py` | 71 | 0 |
| `docs/tests` | `pol002_source_tests.py` | 55 | 0 |
| `docs/tools` | `test_irmel_source_transition.py` | 24 | 0 |
| `docs/tools` | `test_ir_counter_source_policy.py` | 8 | 0 |
| `docs/tools` | `test_actions_dependencies.py` | 11 | 0 |
| `docs/tests` | focused epoch historical-scope + main-only classes | 17 | 0 |

The 55-test source suite includes actual pinned GNATdoc regeneration and
forged-output rejection. It uses installed GNAT 16.1.0, GPRbuild 26.0.0
(Alire package 26.0.1), GNATdoc 26.0.0 (20260415), and the already installed
enhancer archive pinned by the unchanged consumer. No policy pin is changed.
The enhancer site-packages directory is supplied explicitly through
`PYTHONPATH`; `GNATDOC_PREFIX` names the existing pinned GNATdoc installation.

Other completed checks:

- `make test-native`: exit 0, **434/434** CTest tests, including compiled-C
  translation units with warnings as errors.
- `make test-python`: exit 0, **145** Python tests and package compilation.
- `make test-rust`: exit 0; **102** tests across ten unit/integration/doc-test
  result groups pass.
- `make check`: final serialized run exit 0, including build-tree isolation,
  native tests, Ada formatting check, direct-GPR Ada tests, final newlines and
  Git whitespace checks. No first-party Ada source is edited or reformatted.
- `alr -n -C ada build` and `alr -n -C ada/tests run`: exit 0 using the
  installed pinned toolchain; provider-free Ada smoke/contract coverage passes.
- `cargo check --manifest-path rust/Cargo.toml --workspace`: exit 0.
- `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets --
  -D warnings`: exit 0.
- `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`: exit 0.
- `python3 scripts/check_doc_source_integrity.py`: exit 0; 1,162 protected
  foundation files, historical exact transitions retained.
- `python3 scripts/check_final_newlines.py`, `git diff --check`, and compilation
  of all changed Python evaluators/tests: exit 0.
- The unchanged Bootstrap classifier returns `FULL_STRESS` for both this policy
  change and safe Python source changes. No smoke-only shortcut is introduced.
- The actual corrected-policy CLI against real `origin/main` exits 1 with
  `qualifier is not trusted target implementation`, as required before independent
  authorization. This expected rejection is **not** worked around on the PR.

### Failed attempts and limits (not passes)

- Initial new-suite run: 15 tests, six fixture setup errors. A synthetic mock
  filename unintentionally activated an unrelated atomic Instrumentation pair;
  the fixture now uses an unrelated mock filename. Production guards unchanged.
- Initial Bootstrap run: 71 tests, one missing enhancer import. Corrected by
  explicitly supplying the installed dependency; final 71/71 passes.
- Initial IRMEL transition run: 24 tests, one obsolete all-Python-additions
  expectation. The ownership-boundary fixture is updated as described above;
  final 24/24 passes.
- First broad POL-002 run: 45 tests and one setup error (unqualified GNAT on
  PATH); second: 45 tests and one enhancer-version setup error. The pinned
  environment subsequently passes all 55 tests. No check or version pin changed.
- First `make check`: exit 2 because simultaneous language qualification rebuilt
  `native/build-tests` while the isolation test removed it. The final rerun is
  serialized with all other native-tree writers; this was orchestration error,
  not a passing run or justification to change the isolation test.
- Extra `test_live_fixture_transition.py` invocation: exit 5, zero tests and a
  setup error (`source package lacks unique native heading`) because this fresh
  worktree has no generated pristine `_site`. No full historical live-site,
  six-shard, aggregate or publication campaign is claimed by this source-policy
  task. Publication inputs/authority remain unchanged.
- The initial broad epoch run exposed the third obsolete safe-Python rejection
  fixture and the DOC-CI-005 fixture's live-policy dependency. The 17 focused
  historical-scope/main-only tests pass after the fixture corrections. A complete
  rerun uses the pinned enhancer interpreter, not a current unrelated enhancer
  checkout. Full epoch results and hosted status are recorded in the PR handoff;
  an unfinished or failed campaign is never represented by the focused result.

## Delivery boundary

The proposal changes only these eight files (paths relative to the repository):

- `docs/coverage.md`
- `docs/py-pol-001-python-source-admission.md`
- `docs/tests/pol002_epoch_tests.py`
- `docs/tests/py_pol_001_tests.py`
- `docs/tools/feature_source.py`
- `docs/tools/test_irmel_source_transition.py`
- `scripts/check_doc_source_integrity.py`
- `scripts/test_doc_source_integrity.py`

All 804 tracked vendor files and all native, public C ABI, Ada, Rust, Python
implementation/private binding, and workflow files are unchanged from fetched
main. No generated artifacts are tracked or modified. Documentation publication
is main-only under unchanged workflow authority; Bootstrap remains on PRs and
uses full stress classification for policy and Python source changes. The PR
records final commit/tree identities and hosted run links after creation.
Independent policy authorization is pending; no auto-merge, approval artifact,
publication permission or self-approval is granted by this implementation.


