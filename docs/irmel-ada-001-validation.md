# IRMEL-ADA-001 validation and qualification boundary

## Source and scope

Base: `5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf`, tree
`0073b8627bab4f8409438fdf7102ba5e461fb439`. Fetched `origin/main` remained
at that exact reference. The supplied repository pathname did not initially
exist; the correct remote was verified in the clean
`/home/zboll/git/ams-mel-ada2` checkout, and a new worktree was created at
`/home/zboll/git/ams-gra-mel-bridge` on
`feature/irmel-ada-001-coverage-baseline`. Main and unrelated worktrees were
not modified. No rebase, force push, merge, approval, auto-merge, tag or release.

The inventory report and schema record the exact verified upstream Git objects,
counts, extraction exceptions, provider limits and implementation order. All
96 existing IR/Common library headers are byte-identical to their pinned
upstream blobs. No missing header was added to the native vendor closure.
No production native, mock, ABI, Ada, RF, Rust, Python, Squall or workflow source
changed. The production version map remains 222 exports; final ELF/map equality
and source diff checks are separate from declaration existence checks.

## Local gates

The initial `make test-ada` attempt failed immediately because `gprbuild` was
not on the default PATH. The already-installed toolchain was then used without
changing global configuration:

```sh
export PATH=/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:$PATH
```

The following were run locally; shared native build-tree commands were
sequential, never concurrent:

- `python3 -m unittest discover -s scripts -p 'test_ir_api_coverage.py' -v`:
  **24/24 PASS**, including 21 isolated negative controls, truthful-missing
  success and two extraction/independent-scan controls. Also passed with
  `/usr/bin/python3` (3.13.5).
- `make check-ir-api-coverage`: **PASS**; offline checker plus the same 24 tests,
  approximately ten seconds locally. Does not require GNAT/CMake/provider/network.
- `make test-native`: **434/434 PASS**.
- `make test-ada` with explicit installed toolchain PATH: **PASS**, including
  existing IR/RF contracts and focused repeats.
- `make check` with that PATH: **PASS** on the first full run (native build-tree
  isolation, 434 native tests, GNATformat check, coverage tests, Ada contracts,
  final-newline checks and Git whitespace checks). Final rerun results and
  Alire results are reported in the PR/handoff rather than assumed here.
- JSON parse validation of all four audit JSON artifacts: **PASS**.
- `python3 scripts/freeze_ir_api_evidence.py` against both full pinned upstream
  checkouts, followed by `cmp` of candidate evidence and catalog: **PASS**.
- `python3 scripts/check_ir_api_coverage.py`: **PASS**, including vendored byte
  identity, reviewed implementation hashes and deterministic Markdown comparison.
- `python3 -m py_compile` on all three new Python scripts: **PASS**.
- `git diff --check` and `git diff --cached --check`: **PASS** after fixing
  generated Markdown trailing whitespace; the renderer now strips line endings
  deterministically.
- Explicit base-to-head source diff checks for `ada/src`, `ada/tests/src`,
  `native/src`, `native/include`, `native/tests`, `native/vendor` and workflows:
  **unchanged**. No Ada formatting action was needed; formatting checks passed.

## Unresolved independent documentation policy gate — BLOCKED

An additional check, `python3 scripts/check_doc_source_integrity.py`, **FAILS**
with the unchanged existing policy:

```text
ValueError: unauthorized tracked files added:
['scripts/check_ir_api_coverage.py', 'scripts/freeze_ir_api_evidence.py',
 'scripts/test_ir_api_coverage.py']
```

The policy is anchored at DOC-001 base
`42648a92eb0cc84b2980d16db6028992c0494ba1`, permits only individually named
new scripts, and also rejects byte changes to pre-existing files outside its
explicit transitions. Thus merely authorizing the three script additions would
still leave the required `Makefile` and `scripts/test_ada.sh` integration changes
to resolve. This is not an IR implementation change or a failing native test.
It is an authorization conflict between the required new audit gate and the
immutable documentation foundation policy.

The task explicitly prohibits changing that policy, Documentation CI and its
qualification trust model. They were **not modified**, tests were not weakened,
and this result is not reported as green. A separate independently reviewed
authorization is needed; do not expand ongoing PRs #127/#128 or silently reset
the foundation baseline. Hosted CI must be assessed at the exact pushed SHA;
pending jobs are not passing jobs. This feature PR is **BLOCKED**, not declared
merge-ready, even when its ordinary native/Ada checks pass.

## Proposed IRMEL-ADA-002

One Image/Health NUC temperature receive vertical slice: preserve the ordered
TempPoint collection and its uint8/uint16/uint32 variant alternatives, with
separate callback identities and C/Ada tests. Resolve upstream optional
`TempPoint::tempError` access semantics first: the pinned declaration has private
constructor-set storage but no getter/setter. Do not claim complete field
fidelity or silently omit that optional field. No speculative NUC command,
image format expansion or scheduling implementation belongs in that slice.
