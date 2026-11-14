# IRMEL-ADA-014-R2 — reconcile Control discovery with main

## Identity and integration boundary

This is an integration correction for PR #183, not another feature. The existing
branch is `feature/irmel-ada-014-control-capability-discovery`. Fetching both
remote branches found the feature unchanged at
`c95330cd99a47651742f3fb53803934af0c58898`. Main had moved from the task's observed
`e50079ad59ec4b37678f73a4f318d264f3a4639f` to
`c4490a9ce509512e0de6a79d42a61a12c00a7b9a`. The additional mainline merges are
PR #185 (Python C2 ConfigSet) and PR #186 (Rust NavigationReport); these are part
of this reconciliation as well as Scheduling TaskExecutingRep, Python Image
CameraResponse, and Rust LineOfSightQuaternion.

The first ordinary merge, `d87c7571872aeba9edc720b03f441ae3296ac7dc`, has
tree `6f401101692183c9e3acdf26ecdb931fd9596dad` and these ordered parents:

1. `c95330cd99a47651742f3fb53803934af0c58898` — existing feature history.
2. `c4490a9ce509512e0de6a79d42a61a12c00a7b9a` — freshly fetched main.

During qualification main advanced again to
`eb64b3f5a071d8784a1da04b25afd661bba25bdd` through PR #187. A second ordinary
merge incorporates that tip, with ordered parents
`d87c7571872aeba9edc720b03f441ae3296ac7dc` and
`eb64b3f5a071d8784a1da04b25afd661bba25bdd`. It has no textual conflicts and
preserves the executable fixture correction byte-for-byte: automatic maintenance is
disabled only in the temporary event-test clone, with a trace-based regression
test. No production policy or workflow changes. The latest main still has 301
production exports, so the combined count remains 305. Its new task document
lacked a final newline; this integration adds only that newline so the existing
repository-wide newline gate remains enforced. Final qualification is
repeated sequentially after this second merge; its post-commit evidence is
recorded on PR #183.

No rebase, amend, reset, squash, or force push is used. The complete resulting
tree is recorded as `Integration-tree` in the merge commit message and in the
PR qualification record. Recording the tree there avoids placing a tree's own
hash inside a document that contributes to that hash. The PR record also names
the final commit and applicable hosted PR merge-result identity.

## Resolution decisions

Git reported actual textual conflicts in four files (all paths in this report
are repository-relative):

- `python/tests/test_abi.py`
- `rust/ams-mel-sys/tests/abi.rs`
- `scripts/check_ir_api_coverage.py`
- `scripts/test_ir_api_coverage.py`

The native header, export map, CMake definitions, private Ada imports, Ada smoke
driver, mock provider, Python ctypes declarations, Rust sys declarations, and
both structured inventories merged textually. They were nevertheless reviewed
as shared contracts, rather than accepted solely because Git found no conflict.
The header adds only discovery's opaque owner and four declarations relative
to main; existing record layouts and Scheduling declarations remain unchanged.
CMake retains both the discovery C tests and Scheduling tests. The mock retains
separate discovery and Scheduling scenarios, and the Ada driver invokes both
test families. Neither language mirror loses the other branch's declarations.

Both conflicting ABI tests now require exactly 305 exports, not the older 299
or 301. Complete inventory equality, ctypes signatures, Rust function-pointer
checks, layout probes, and representation checks remain active. Rust also has
explicit sorted-adjacency duplicate checks on both declaration and export
inventories. No success stub, disabled test, ABI version, or layout change is
needed for the integration.

The coverage-test conflict has a semantic answer: Scheduling exposes both
ExecuteTaskAck and TaskExecutingRep, leaving nine Scheduling-specific operations
missing. The complete audit has 50 safe Ada operations after including Control
discovery. Existing assertions about complete payload getters and independent
callback owners are preserved.

## ABI inventory and coverage reconciliation

The fetched feature has 299 production exports and fetched main has 301. Their
exact union is 305, with no duplicates or removals. Relative to main, the only
additions are:

- `ams_mel_ir_capability_list_create`
- `ams_mel_ir_capability_list_count`
- `ams_mel_ir_capability_list_get`
- `ams_mel_ir_capability_list_close`

Relative to the feature parent, the additions are the six Scheduling
TaskExecutingRep exports. Symbol versioning remains `AMS_MEL_0.1`. The native
header, export map, Python private inventory and Rust raw inventory are compared
as complete sets, not just spot-checked for these additions.

The authoritative coverage JSON and reviewed binding ceiling preserve both
parents' evidence. Compared with main, only Control's capability operation and
its getter representation change. Compared with the feature parent, the
Scheduling callback, TaskExecutingRep, Task, TaskCompStatus, and SchedulingChannel
evidence change. All previously safe Ada operation/type classifications survive;
inherited exposures and image-capability classifications are unchanged. The
combined operation totals are 50 safe Ada, 12 partial, 17 missing, and 69
provider-only, across the same 148 operations and 127 headers. There are 150 safe
Ada types. Qualification narratives from both sides are retained.

`scripts/check_ir_api_coverage.py --write-report` regenerates the derived Markdown
from the authoritative data. The checker anchor is recomputed from the merged
review's canonical JSON. Its `source_sha256` remains the immutable historical
baseline, not a new lock on current implementation bytes, as required by the
checker and schema. The upstream evidence, catalog, pins, historical coverage
archive and protected workflows are unchanged.

An independent byte comparison also confirms that all nine files changed only
on the feature side and all 51 files changed only on main are preserved exactly
from their respective parents. The semantic reconciliation is confined to the
15 shared changed files and this integration report.

## Local qualification

Qualification results below describe the first reconciled source; the identical
production/language source plus PR #187 is requalified sequentially for the final
merge. Logs are retained locally under `/tmp/irmel-014-r2`. Make targets
run through `alr -n -C /home/zboll/git/ams-gra-mel-bridge/ada/tests exec -- make -C
/home/zboll/git/ams-gra-mel-bridge TARGET`, using the existing pinned environment
without changing global configuration or repository pins.

Tools are Alire 2.1.1, GNAT 16.1.0, GPRbuild 26.0.0 (crate 26.0.1), GNATformat
26.0, GCC 14.2.0, CMake 3.31.6, and Cargo 1.98.1.

| Command/check | Actual result |
| --- | --- |
| `make format-ada` | Passed; no additional source changes required. |
| `make check-ada-format` | Passed. |
| `make test-native` | Passed: 557/557, including discovery, allocation failure and all Scheduling scenarios. |
| `make test-python` | Passed: 502 tests with warnings as errors and package byte-compilation; includes CameraResponse, ConfigSet and complete ABI inventory/signature controls. |
| `make test-rust` | Passed: workspace tests and doctests, raw ABI 45/45, Scheduling layout probes, LOS quaternion 26 passing integration scenarios and NavigationReport 10/10; existing opt-in tests and subprocess entry points retain their intended ignored status. |
| `make test-ada` | Passed: complete smoke/contract suite including Control discovery and Scheduling foundation, ExecuteTaskAck and TaskExecutingRep. |
| `make check-ir-api-coverage` | Passed: offline audit and 59/59 regression controls. |
| `make check` | Passed: build-tree isolation in both directions, repeated 557-test native suite, formatting, coverage controls, Ada suite, final-newline and whitespace checks. |
| `make test-policy` | Passed: all eight suites, including protected workflow, source-integrity, documentation security, static subtype and coverage controls. |
| `alr -n -C ada build`; `alr -n -C ada/tests run` | Both passed with the pinned toolchain; complete Ada contracts executed. |
| Production DSO audit | Passed: exactly 305 functions, all `@@AMS_MEL_0.1`, exactly equal to the export map; no test hooks. The separate test DSO contains the same 305 production functions plus its existing 64 private observation hooks. |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings`; `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Both passed. |
| `python3 scripts/check_doc_source_integrity.py` | Passed: 824 vendor files and three workflows. |
| `python3 -W error -m unittest discover -s docs/tests -p py_pol_001_tests.py -v` | Passed. |
| Pinned-enhancer unittest discovery of `test_scheduling*links.py` | Passed: eight tests. |
| `sh docs/tools/qualify_examples.sh` in pinned Alire environment | Passed: 13 run cases across nine programs, each verified against the production facade and separate mock providers. |
| C11 discovery runtime rebuilt with `-DNDEBUG -Wall -Wextra -Wpedantic -Werror` | Passed; validation remains active with assertions disabled. |
| Final `make check-ada-format`, newline and whitespace checks | Passed. |
| Strict C11 discovery signature probe | Passed with `-std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only`; `_Generic` assertions preserve constness, pointer depth, status and `size_t` shapes. |

Documentation generation that requires a clean committed checkout is qualified
after the merge commit; its exact commit, tree and actual results are recorded
on PR #183 without creating a self-invalidating documentation-only commit.

Intermediate audit attempts are not counted as successful qualification: a
header-name regex initially failed to account for `AMS_MEL_NOEXCEPT`; the corrected
complete-header audit passed. One DSO audit ran during the supported build-tree
reset and found the test DSO temporarily absent; the sequential post-build audit
is the authoritative result. The test DSO also intentionally exports private
observation hooks; the audit separates those from the production inventory
rather than treating the test DSO as a production artifact. The review anchor initially used a file-byte hash;
the checker correctly rejected it, and it was corrected to the required
canonical-JSON hash before qualification. Historical source hashes were left
unchanged after checking their documented baseline meaning. The new report's
initial missing newline was corrected and the final-newline gate rerun.

## Final main movement during delivery

After pushing the second merge, main was found to have advanced to
`76ade6cd6d4b0fa51c04947c040dfac3633d8886` through PR #188, safe Rust C2 ModeCmd.
The final ordinary merge has ordered parents
`8d473d9b8cd804aeda2c4a48108d27f54f73f9ff` and
`76ade6cd6d4b0fa51c04947c040dfac3633d8886`, without textual conflicts. The
second merge's tree is `ca99ce568f79f6c59d6d7a63c4ba733dfacd9d49`.
All five incoming Rust feature files are retained exactly. This introduces no
native exports, so 301 mainline and 305 combined exports still hold. The
partially started second-merge requalification is superseded by the final
sequential run; no partial run is reported as a complete final qualification.
The final commit's `Integration-tree` trailer and PR record identify this last
tree, and PR evidence records tests against that exact commit. The initial
parent-specific 9/51 byte comparison above describes the first merge, not a
claim that later mainline additions were absent.

## Limitations and delivery rules

This remains provider-independent consumer-binding qualification using separate
mock DSOs. No ordinary test acquires a minimal-example-provider, Squall, hardware,
container, or credential dependency. Real-provider/sensor qualification is not
claimed. No new upstream semantic decision is necessary for this reconciliation.
The existing import-only GPR project may report its known project-level
"no sources of language C" warning; first-party source warning policies remain
unchanged and warnings are not suppressed to pass a gate.

Local success is not hosted success. Hosted Bootstrap qualifies the applicable
PR merge result, while the documentation workflow explicitly checks out the PR
head. The final PR qualification record distinguishes those identities and gives
workflow URLs. If the initial hosted check has not completed, the delivery state
is `HOSTED_CI_PENDING`, not an assertion that the PR is qualified for approval.
PR #183 stays open, non-draft, unmerged, with auto-merge disabled; PR #184 is not
modified. No approval or merge is performed.
