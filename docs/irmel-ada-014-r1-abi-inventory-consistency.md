# IRMEL-ADA-014-R1 — Cross-language ABI inventory correction

## Starting identity and cause

This is a normal follow-up to PR #183 on
`feature/irmel-ada-014-control-capability-discovery`, not a replacement feature.
After fetching the remote branch, local HEAD, remote HEAD and the PR head all
matched `6020ced3c2214977f9ad583ecca47319c89ab870`; the worktree was clean.
The original feature commit is retained unchanged. PR #184 is outside this task.

[Bootstrap run 43540020128](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43540020128)
failed the Python complete-facade and public-export inventory tests and the Rust
raw inventory test. The native facade already exported 299 functions, while the
private/raw mirrors and exact count expectations still described 295.

## Coverage delta and boundaries

The Python private ctypes layer and Rust sys layer now mirror all four existing
`ams_mel_ir_capability_list_{create,count,get,close}` exports. Python uses the
existing `c_void_p` opaque-handle convention; Rust uses a private-field opaque
`repr(C)` type with the existing non-Send/non-Sync marker. Count and get borrow
`const` list pointers in Rust; owner outputs and close use double mutable pointers.
All sizes and indices use `c_size_t`/`usize`, diagnostics retain their existing
pointer/capacity/required shape, and returns remain signed 32-bit native statuses.

Exact inventory expectations increase from 295 to 299. Full name equality and
duplicate checks are retained, including the unchanged public-surface inventory
and missing/duplicate negative controls in the Python common Channel tests.
New Python signature assertions and Rust function-pointer type checks cover the
four mirrors explicitly. No safe Python or Rust discovery API is introduced.

The native header, records, implementation, export map and symbol version remain
unchanged: these four functions were already published under `AMS_MEL_0.1`.
Ada discovery, capability conversion, provider/session lifetime rules, vendored
sources, and authoritative Ada coverage inventories/reports are unchanged.
Consequently this correction changes no Ada coverage classification or public
operation count and requires no regenerated Ada coverage report.

## Qualification

Local command results and exact-head hosted results are recorded separately;
local success is not evidence of hosted success. The PR qualification record
will identify the corrective commit and its actual hosted runs after pushing.

Local validation uses the existing Alire environment, without changing pins:
`alr -n -C /home/zboll/git/ams-gra-mel-bridge/ada/tests exec -- make -C
/home/zboll/git/ams-gra-mel-bridge <target>`. Available tools are Alire 2.1.1,
GNAT 16.1.0, GPRbuild 26.0.0, GNATformat 26.0, and Rust/Cargo 1.98.1.

| Command or check | Actual local result |
| --- | --- |
| `make test-native` | Passed: 525/525 CTest cases. |
| `make test-python` | Passed: 457 tests with warnings as errors; package byte-compilation passed. Both formerly failing inventory tests and missing/duplicate negative controls passed. |
| `make test-rust` | Passed: workspace tests and doctests; raw ABI tests 45/45, including inventory equality and the four explicit signature checks. Existing eight opt-in provider tests remain ignored. |
| `make test-ada` | Passed: complete smoke/contract suite and 55 coverage regression controls, including existing discovery and unload checks. |
| `make check` | Passed: build-tree isolation, 525 native tests, Ada formatting, 55 coverage regression controls, complete Ada suite, final-newline and Git whitespace checks. |
| `make test-policy` | Passed: source-integrity, policy, documentation security and coverage regression controls. |
| `cargo fmt --manifest-path /home/zboll/git/ams-gra-mel-bridge/rust/Cargo.toml --all -- --check` | Passed. |
| `python3 scripts/check_doc_source_integrity.py` | Passed: 824 vendor files, three workflows. |
| `python3 scripts/check_ir_api_coverage.py` | Passed: 127 headers, 148 operations, 49 safe Ada operations; no classification change. |
| `python3 scripts/check_final_newlines.py`; `git diff --check` | Passed after correcting the new document's missing final newline. |
| Strict C11 signature probe | Passed with `cc -std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only`: `_Static_assert`/`_Generic` verifies each of the four header function-pointer types, including constness and pointer depth. |
| Production DSO versus `native/src/exports.map` | Passed after the current-branch rebuild: `nm -D --defined-only` reports exactly 299 functions, every one versioned `@@AMS_MEL_0.1`, with exact name equality and no unexpected additions/removals. |
| Original feature HEAD versus correction | Native and Ada trees, structured inventories and generated reports are byte-identical. |

An early symbol probe encountered the previous checkout's stale production DSO
(300 functions); it is not current-source evidence. Repeating after the supported
production rebuild produced the exact 299-function result above. An extra Python
probe attempted during a supported build-tree reset found no DSO; repeating once
the build completed verified all 299 configured private bindings. Neither
intermediate probe is counted as a passing validation.

First-party native compilation retains `-Wall -Wextra -Wpedantic -Werror`, and
Ada retains its existing warning-as-error policy. No source warning was reported.
The aggregate formatting check emits the existing import-only GPR project's
"no sources of language C" project-level warning; no warning policy is weakened.

Hosted results are qualified by the exact corrective commit in PR #183's
description and qualification comment, with run URLs and conclusions. Keeping
that post-push evidence on the PR avoids changing the tested source HEAD merely
to record its own hosted run. The original failing run is retained above; no
claim of hosted success is inferred from this local table.

## Limitations and unresolved semantics

This is declaration/inventory consistency evidence, not real-sensor qualification.
Ordinary qualification uses the existing separate mock providers; no Squall,
hardware, credentials, containers or new dependencies are needed by the tests.
No new upstream-semantic decision is made, and there is no unresolved upstream
semantic issue specific to these four raw mirrors. Existing discovery limitations
remain those in the original feature qualification document.
