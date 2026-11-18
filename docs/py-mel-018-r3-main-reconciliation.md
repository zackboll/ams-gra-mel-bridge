# PY-MEL-018-R3 — main reconciliation for existing PR #233

## Scope and verified source identities

This is integration, not a new quaternion implementation or IR capability.
The existing worktree `/home/zboll/git/ams-py-mel-018` was clean; local,
remote and open non-draft PR #233 all identified feature HEAD
`991c3a85d21688e3caf43052b440fa60681da37c`. Fetch confirmed current main
`e41493dcef34bb62f45fecb53eceb989ce895500`, tree
`a068be918f765c8968df678a1894f120e720acba`. That is PR #239's merge commit
(merged 2026-11-18 at 04:29:07 UTC), and includes CI-DOC-FAST-001, the
Rustdoc theme, Ada AbortTask and Rust host Buffer changes. Process and Git
lock inspection found no other agent/build modifying this worktree.

An ordinary `git merge --no-edit origin/main` produced
`c0d8725fa389d72d684ce32968e31382d2bbfacd`, tree
`efe69b7fbb3cb5d804066f042673316175fcafe2`, with the original feature and
verified main as its two parents. **No conflicts occurred.** The only path
changed on both sides, `python/tests/test_abi.py`, merged automatically:
main's AbortTask layout/signature coverage and current export inventory coexist
with the feature's quaternion/FRD identity assertions. No wholesale side
selection, rebase, squash, history rewrite or duplicate worktree/PR was used.

## Preservation evidence and coverage delta

The following Git blob IDs are identical before and after integration:

| File | Blob SHA-1 |
| --- | --- |
| `python/ams_mel/_los_quaternion.py` | `61c8eb4ec327869579c9d47405c7d8c47d100842` |
| `python/tests/test_ir_image_los_quaternion.py` | `aff321e7a3e3f55e6ebe6ee19d0b250a535aec06` |

Root and IR exports, namespace/import-order/pickle tests, Image entry point,
OpticalDistortionMap implementation/tests, and source-epoch regression fixture
also retain the feature parent's bytes. Quaternion XYZW order, signed times,
IEEE values, canonical `ForwardRightDown`, separate registration Return,
independent one-shot registration, checked conversion, event release before
publication, uncertain-ownership once-only cleanup, context management,
non-raising finalization and compiled-mock provider teardown tests are preserved.
External serialization remains required; no provider lifetime promise is added.

Native, Ada, Rust and workflow Git subtrees equal incorporated main exactly:

| Subtree | Git tree SHA-1 |
| --- | --- |
| `native` | `b2ced306e9ece558ce343f6aaecd84ff86947434` |
| `ada` | `40852d01e9d62e07a787ad92fc054c47f56e2c0a` |
| `rust` | `840e4bc529e2a3a440cd7d9abf78d4f078a8b3e4` |
| `.github` | `e709ace8e48926691c7efa91216932292cdcce3c` |

All documentation tools and the named search/resize/theme/CI regression tests
equal main. The source-epoch fixture retains actual source-derived Rust/Python
records and complete Ada/Rust/Python assembly; its only difference from main
is a clarifying docstring. No assertions or source-verification checks change.
R3 adds this reconciliation report and corrects one inherited copy-pasted
comment in the private Scheduling ExecuteTaskAck ABI test: that binding is
private, whereas the safe Python quaternion API already exists. Its assertion
is unchanged; focused ABI qualification is rerun after the comment correction.
Coverage
delta: zero new operations, ABI records, signatures or exports; native/Ada
structured inventories are inherited unchanged from main.

## ABI and documentation contracts

The freshly fetched main export map and built production library have the
same **332** versioned `AMS_MEL_0.1` functions, with no additions, deletions or
test/probe/hook exports. The increase from the previous feature's 331 is
main's AbortTask export, not a Python/R3 ABI change. Entire native subtree
identity also proves record layouts, signatures and lifetime protections are
unchanged by R3. Export comparison uses consistent C-locale sorting; an initial
comparison with locale-dependent shell sorting versus Python sorting was an
ordering-only diagnostic, not a symbol difference.

DOC-RESIZE-CI-001 remains source-local: verify source/manifests, native anchors,
search and complete language assembly, independently reconstruct Rust/Python,
derive a disabled control from the same revision, require exactly the four
authorized changes (`api/gnatdoc-resize.css`, `api/gnatdoc-resize.js`,
`api/index.html`, `api/presentation.json`), and verify repeated candidate
generation. Forged records, altered artifacts and incomplete language inventories
remain rejected. No cross-revision publication equality is restored.

CI-DOC-FAST-001 remains unchanged: Bootstrap and normal documentation on PRs;
heavy Search/Resize/Authored qualification on main pushes or explicit dispatch;
exact source SHA, strict ancestor manual resize baseline, read-only heavy jobs,
no deployment authorization and original concurrency/cancellation semantics.

## Qualification commands and evidence location

Commands run from the existing worktree, sequentially for shared build trees.
`make format-ada` passed without changing any Ada bytes before compilation.
`make native` passed; `make test-native` passed **841/841** tests. Actual
production `nm -D --defined-only` output equals freshly fetched main's exports.

The final-source qualification ledger in existing PR #233 records each actual
exit/count, final feature HEAD/tree and hosted run URL, preserving all historical
R1/R2 evidence rather than replacing it. Local command logs and exact runner
scripts are retained in `/tmp/py018-r3-qualification`; generated publication,
repeated reconstruction, parity and browser evidence use uniquely named
`.doc-tools/py018-r3-*` paths. These local diagnostics are not deployment authority.

Required final-source commands (listing a command is not a pass claim):

```sh
make native
make test-native
make test-python
make test-rust
make test-policy
make check-ir-api-coverage
make test-docs-ci
make check-ada-format
make check
git diff --check
```

Focused Python qualification uses the existing test-native library/provider
environment and `PYTHONPATH` containing `python` and `python/tests`:

```sh
python3 -W error -m unittest -v test_ir_image_los_quaternion \
  test_ir_image_optical_distortion_map test_import_identity test_public_modules \
  test_abi test_buffer_abi test_checkout_abi
python3 -W error -m compileall -q python/ams_mel python/tests
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
```

Documentation qualification uses the repository's existing pinned Alire
environment (`.doc-tools/bin/alr -n -s "$PWD/.doc-tools/settings" -C
docs/toolchain exec -- ...`), GNAT 16.1.0, GPRbuild package 26.0.1 reporting
`GPRBUILD 26.0.0 (2026-04-15)`, GNATdoc 26.0.0 (20260415), the verified
enhancer and Rust 1.98.1. It includes `make test-documentation-source`, complete
`docs_ci.py build`/`verify`, independent repeated build, `docs_ci.py qualify`,
same-source `resize_parity.py`, authored HTML qualification and existing
workflow/resize/search/authored/enhancer/Rustdoc-theme regression suites.
Chromium resize qualification includes mandatory search regression. The genuine
`python-reference.json` is also checked against the loaded public API.

After successful local qualification, normal push updates only PR #233's branch.
Automatic Bootstrap and normal documentation must qualify the new exact HEAD.
One manual resize run uses incorporated main
`e41493dcef34bb62f45fecb53eceb989ce895500` as its verified strict ancestor;
acceptance requires the dispatch's actual source SHA to match final HEAD and its
reconstruction/parity/browser checks to pass. Absence of automatic heavy PR
runs is policy, not a pass or failure. No manual Search/Authored hosted campaign
is needed unless integration changes their contracts or an actual failure
justifies it; R3 changes neither contract.

## Limitations and review handoff

Qualification is mock-provider-only: no hardware, Squall, arbitrary provider
C++ ABI compatibility, unregister, physical quaternion accuracy or additional
IR capability is claimed. Existing uncertain-ownership retention and
external-serialization limitations remain. No upstream semantics are guessed.

The prior independent-review request is preserved and renewed against the new
exact HEAD in PR #233. GitHub initially listed only the author as collaborator
and no formal reviewers; an AI comment is not approval by a separate account.
Hosted checks and manual resize must actually complete before declaring
`READY_FOR_INDEPENDENT_REVIEW`. `READY_FOR_USER_MERGE` additionally needs
independent review. The PR remains open, non-draft and unmerged, with auto-merge
disabled; no self-approval, merge, release, tag or force-push is authorized.
