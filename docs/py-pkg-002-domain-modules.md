# PY-PKG-002 — Public Common, IR and RF Python modules

## Scope and coverage delta

Starting main: `43f8acc8843a907dd91150961bc94023ec23c414` (merged Python
documentation PR #232). This task organizes the existing Python public API and
its documentation into three domain modules within the existing `ams_mel`
package. It adds no MEL operation, native export, provider capability or safe
language coverage. Native C/C++, Ada, Rust and integration sources are unchanged.

| Public module | Interface | Existing exports |
| --- | --- | ---: |
| `ams_mel.common` | `AbiVersion`, `ProviderVersion`, `UciId`, `ComponentLocation`, `ErrorKind`, `MelError`, `MFAState`, `abi_version` | 8 |
| `ams_mel.ir` | IR Session, typed channels, requests, commands, configurations, metadata and result values | 153 |
| `ams_mel.rf` | Independent `RFAdmin` owner | 1 |

These are disjoint sets covering all 162 existing root exports. Counts describe
this implementation checkpoint and are not frozen limits for future development.
Future public additions should be exported by their domain and by the compatible
root facade, with the ordinary matching tests and documentation.

## Why this structure fits Python

The earlier PY-PKG-001 extraction already separated implementation concerns into
private modules. Three public namespace modules give Python applications the same
Common / IR / RF domain vocabulary as Rust while retaining one package and its
existing private implementation graph. Each public module explicitly imports
its existing implementation objects and declares a literal `__all__`.

`ams_mel.__init__` imports the three modules and explicitly reexports their
symbols. Both styles work:

```python
from ams_mel import common, ir, rf

from ams_mel.common import MFAState, MelError
from ams_mel.ir import Session
from ams_mel.rf import RFAdmin
```

The inherited `ChannelView` services, `CommandReturn` and `MelErrorCode` are IR
interfaces. The private filename `_common.py` refers to shared IR channel
mechanics, not the new cross-domain public Common namespace. `MFAState` is the
one existing enum shared by IR mode/health and RF Admin. The bridge-wide native
ABI query belongs to Python Common.

### Compatibility and loading

- The ordered root `ams_mel.__all__` and its public metadata assignment are
  unchanged. `common`, `ir` and `rf` are available as root module attributes;
  root wildcard imports retain the existing symbol set.
- Domain and root imports refer to identical objects. No classes, enums,
  dataclasses, functions, owner handles or wrappers are duplicated.
- Existing signatures, dataclass fields/defaults, enum values, errors and
  canonical `__module__ == "ams_mel"` introspection/pickle paths are preserved.
- The private implementation dependency graph remains unchanged and does not
  import the public namespaces. No native ownership or lifecycle code moves.
- Importing any namespace initializes the parent package and still requires an
  absolute `AMS_MEL_NATIVE_LIB`. Common is not a native-independent package.
  Import loads the shared facade without opening an IR or RF provider.
- There are no new distribution artifacts, native libraries, dependency
  selection options, background workers or provider features.

## Documentation

The source-only generator reads public modules' literal `__all__`, descriptions
and source paths, and resolves each export to its canonical root definition.
It rejects unsupported or mismatched declarations without importing the native
binding. The runtime qualifier independently checks ordered namespace exports,
parent-module bindings, exact object identity, checkout origins and source hashes.

The generated reference adds `modules/common.html`, `modules/ir.html` and
`modules/rf.html`, public-module source pages, module directories, navigation and
domain import guidance on each symbol page. All existing `api/Name.html` and
member-fragment URLs remain valid. Search accepts qualified module names and
members through aliases on each canonical record, preserving one result per
symbol/member. Guides use domain imports and explain the native loading contract.

The existing publication verifier accepts and fully checks the additional pages.
The Ada/Rust output projection, deployment authority, workflow jobs, permissions
and tool pins are unchanged. Prose coverage remains informational.

## Validation evidence

The complete Python suite used the already built test facade and mock providers
from the merged documentation worktree. `git diff` confirmed the entire native
source tree is unchanged between that implementation and this task's main base.
The facade SHA-256 is
`95044d4a02dde94cb5e5911d00321f79c1298c5025f7077b3d73c309993c83f4`.

```sh
PYTHONPATH="$PWD/python" \
AMS_MEL_NATIVE_LIB=/workspace/scratch/f3f7e8ebd38d/ams-gra-mel-bridge-pydoc-001/native/build-tests/lib/libams_mel_c.so.0 \
AMS_MEL_TEST_PROVIDER_DIR=/workspace/scratch/f3f7e8ebd38d/ams-gra-mel-bridge-pydoc-001/native/build-tests/test-providers \
python3 -W error -m unittest discover -s python/tests -v
```

**692 tests passed**, including six new namespace tests covering complete domain
ownership and alias identity, shared exact-type values, pickle paths, all six
import permutations, native-library requirements, and IR/RF mock-provider use
through domain imports. Package compilation with `python3 -W error -m compileall
-q python/ams_mel` also passed.

A complete before/after `public_snapshot()` plus `error_snapshot()` comparison
passed for all 162 root exports: names/order, callable signatures, class
metadata, dataclass fields/defaults, enum members/values/representations, aliases
and observable errors. This one-task comparison supplements the retained
54-export historical compatibility tests; it adds no permanent frozen API total.

```sh
make test-docs-ci
PATH=/workspace/scratch/f3f7e8ebd38d/doc-ada-002-cmake331/bin:$PATH make test-policy
AMS_MEL_NATIVE_LIB=/workspace/scratch/f3f7e8ebd38d/ams-gra-mel-bridge-pydoc-001/native/build-tests/lib/libams_mel_c.so.0 \
python3 -W error scripts/qualify_python_reference.py
python3 scripts/check_final_newlines.py
git diff --check
```

- Documentation controls: **107 tests passed**.
- Full policy suite, including those documentation controls: **311 tests passed**.
- Runtime qualification: **3 public modules / 162 namespace exports**, unchanged
  **162 root exports, 268 callables, 93 dataclasses / 364 fields, 43 enums / 314
  values**, across 34 package source files.
- Actual source generation and publication validation: **200 files**, including
  197 HTML pages, 1,018 search records and **4,377 validated local links**.
- The actual generated search inventory and `search.js` passed seven Node VM
  cases covering legacy inherited-property search, qualified IR/Common/RF
  names, module discovery, no matches and cleared input. No duplicate result
  was introduced for an inherited member.
- Independent review confirmed the disjoint export mapping, identical objects,
  unchanged metadata/pickle paths and source/publication link correctness.
- Newline and Git whitespace checks passed.

## Limits

The qualification above uses mock providers. Existing provider support,
external serialization, request timeout versus cancellation, callback draining
and cleanup guarantees are unchanged. Namespace organization does not extend
Python RF beyond Admin or introduce native-free Common imports.

The Node search check is not a browser rendering check. The workspace's known
Chromium process-socket restriction prevents the optional browser qualification;
no responsive-layout or Chromium pass is claimed. The source/publication tests
cover generated links, no-script navigation, escaping and artifact integrity.

Complete combined-publication and hosted CI results are recorded with the pull
request. Merge and successful main-push publication are required before the new
module pages become available on the live documentation site.

## R1 — Complete the documentation regression fixture

The initial PR head `994d20efc544dc2a7a56098846c51fbf6dd29753` passed every
Bootstrap check and the production documentation workflow, but the separate
authored-HTML qualification job failed in
`test_normal_verifier_rejects_corruption_even_with_recomputed_payload_hashes`.
Its positive control raised `FileNotFoundError` for `python-reference.json`
before reaching the corruption cases. The
[failed job](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43762218487/job/132959321770)
records the full traceback.

The shared `assembled_fixture()` helper still built an Ada/Rust-only publication.
The Python publication integration in PR #232 had made the Python record and
subtree part of the production verification contract. Both the outdated fixture
and the current verifier were inherited unchanged from main by PR #234; the
Common/IR/RF namespaces were not necessary to trigger the failure.

The correction generates Python files and their record from the same temporary
Git export and identity used for Rust, assembles them over the Ada/Rust output,
and writes the canonical `python-reference.json`. The existing successful
verification remains before every mutation. All four original Ada corruption
cases and the independent subtype-link test remain intact. Two added cases
alter a Python API page and forge the canonical Python report's export count;
the outer bundle hashes are recalculated in both cases, so its own manifest
cannot authorize either corruption.

Only the test fixture, its mutation coverage and this task record change in R1.
Production verifiers, generation, workflows, dependencies and language bindings
remain unchanged. Missing Python records remain errors. The test does not treat
`FileNotFoundError` as a successful corruption rejection.

Independent review found no blockers and separately confirmed that the real
Python publication rejects both new mutations at the expected inventory and
source-reconstruction boundaries. A focused local run of the original fixture
reproduced the identical missing-record error (**1 test, expected failure**).
`make test-docs-ci` passed all **107 tests** after the correction. The complete
regression command is `GNATDOC_PREFIX=<pinned-prefix> make
test-documentation-source`, with the pinned GNAT/GPRbuild/Rust environment.
Full source-regression and hosted qualification results are recorded with the
pull request.
