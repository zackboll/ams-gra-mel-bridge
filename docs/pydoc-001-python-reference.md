# PYDOC-001 — Python reference and guides

## Purpose and boundary

Add the safe Python API to the existing Ada/Rust documentation publication.
The reference follows the checked-in `ams_mel.__all__` surface, with authored
setup, ownership/error, IR and RF Admin guides. It does not infer language parity
from private ctypes declarations or advertised enum vocabulary.

Work began at main commit `34cc31f6a5dd183e2197d61dd4d9c62d97ab6ded`, tree
`37924775b8a544a58a365a9e61547b674327cee4`. The Python implementation changes are
docstrings only: all 17 edited modules have identical executable ASTs after
stripping docstrings. Native code, public exports, signatures, values, ownership
and behavior are unchanged. Historical task reports and `docs/coverage.md` remain
intact; this document owns the new task evidence.

Main's Rust crate extraction (#228) was integrated before publication, using
base `8ff6ec0574f4266d7cba0610f339209589778f65`. The new independent Rust IR
reference remains included in generation and verification. The published
implementation checkpoint is `707752c435efca8e7ece7496d6a4d6724ab71ba8`, tree
`b81d381b7e2b504634aebfb68bbde363d8702dc0`; this task-evidence update follows it.

## Source-derived reference

`docs/tools/python_api_reference.py` reads Python source through the standard
library AST. Importing the actual binding loads the facade named by
`AMS_MEL_NATIVE_LIB`, so documentation generation must not import it. The source
reader neither loads that library nor invokes a provider. It introduces no
third-party documentation dependency or native compilation into the docs job.

The reader resolves literal ordered exports, reexports, source definitions,
signatures, decorators, first-party inherited public operations, frozen dataclass
fields/defaults/factories, enum values/aliases and structured attributes. It rejects
unsupported public source constructs explicitly instead of silently omitting them.
Current deliberate limitations include multiple inheritance, unknown decorators,
dynamic public class bodies and decorated first-party ancestors.

Every public export has an API page and source links with line anchors. Methods,
properties, fields and enum values have stable member anchors. The static index
links every export and member without JavaScript; optional local search uses
escaped generated data and `textContent`. Source views identify private names as
implementation details. Factory-only owners explicitly describe rejected direct
construction, using the source's literal error message.

### Measured inventory

| Surface | Count |
|---|---:|
| Public exports | 162 |
| Frozen dataclasses / fields | 93 / 364 |
| Enums / named values, including aliases | 43 / 314 |
| Native owner classes / structured error classes | 21 / 4 |
| Module functions | 1 |
| Public operation occurrences, including inherited context protocols | 149 |
| Structured attributes | 26 |
| Search records | 1,015 |
| Generated HTML pages / local assets | 191 / 3 |
| Validated local links | 3,398 |

Explicit export descriptions increased from **63/162 to 162/162**. Explicit
public operation descriptions increased from **99/149 to 149/149**, counting
inherited occurrences and actual context protocols. Source edits add 99 export
summaries, 14 ordinary method/property summaries and 18 declared context-protocol
summaries. These counts measure the presence of prose, not complete semantic
documentation of every field. Missing prose remains informational; there is no
completeness threshold or reviewer-admission mechanism.

## Guides and implementation scope

The five authored pages under `docs/python/` cover the current public interface:

- `index.html`: supported families, navigation and explicit unbound capabilities.
- `getting-started.html`: production facade setup, import requirements, Session
  discovery, attachment values and an owned Mono8 frame example.
- `ownership-and-errors.html`: immutable values, native owners, weak views,
  request observation, context managers, diagnostics and external synchronization.
- `ir-workflows.html`: Image acquisition/control-only attachment, C2 Mode/ConfigSet,
  navigation, common services, StackedImage foundation, metadata and Instrumentation.
- `rf-admin.html`: independent RF Admin factory, state acceptance and shutdown.

The examples respect the actual ownership protocols: ModeRequest, ReturnRequest
and CommsRequest use explicit close in `finally`; NavigationRequest and
InstrumentationRequest support context managers. Timeout and close do not cancel
native work. A returned provider rejection, a completed `CommandReturn.FAIL`, a
bridge exception and RF command refusal remain distinct outcomes. Registration
attempts and close-retry behavior retain their concrete owner-specific rules.

Safe Python RF support is Admin only. Track/Scheduling owners, generic buffer
checkout, stacked acquisition, CameraCommand submission and LineOfSightQuaternion
reception are not exposed. Broader private native declarations do not add them.

## Publication integration

`docs_ci.py` generates Ada, Rust and Python from the same exported commit. Python
files occupy only `python/`, with one prescribed portal insertion. The canonical
`python-reference.json` and generation log are included in the existing sealed
artifact and diagnostic upload. The record includes source hashes, the complete
API inventory, informational descriptions and exact generated-file hashes.

Verification reconstructs every Python byte and its report from exported source.
It then removes exactly the verified subtree and portal insertion, recovering the
Ada/Rust publication for the existing Rust verifier. Ada pristine validation,
coverage evidence, enhancement reconstruction and Rust qualification remain in
place. Python's HTML/resource policy is separate: only local approved scripts and
styles, resolved links/fragments, a constrained attribute set and the exact portal
escape are permitted. Live verification includes every Python file using the
existing ordinary MIME/framing checks.

The existing workflow jobs, action pins, permissions, events, source/run identity,
external digests and main-push-only deployment conditions are unchanged. Only the
workflow display name includes Python. No native-backed qualification or browser
dependency is added to the documentation build.

## Reproduction commands

Generate just Python with a fresh output directory; no facade is required:

```sh
python3 -W error docs/tools/python_api_reference.py --output /tmp/ams-mel-python-docs
make test-docs-ci
```

Separately compare the model with the real package and an already built facade:

```sh
make test-python
AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" \
  python3 -W error scripts/qualify_python_reference.py
```

The optional `--inventory` argument accepts a direct inventory, generator report
or assembled `python-reference.json`. Qualification checks actual export identity,
source origins/hashes, callable structure, dataclass generated constructors,
fields/defaults/factories, enums and inherited descriptors. It does not construct
owners, invoke properties, call public operations or execute default factories.

For the complete publication, use the pinned environment installed by
`docs/tools/install_docs_toolchain.sh` and the command in
`.github/workflows/documentation.yml`. `docs_ci.py build` requires a clean committed
checkout and fresh output. Local artifacts are explicitly non-deployable.

An optional browser check, with Playwright and Chromium already installed, is:

```sh
python3 docs/tools/python_reference_browser.py \
  --site .doc-tools/current/published --executable /usr/bin/chromium \
  --output /tmp/python-reference-browser.json
```

It exercises project-prefix navigation, inherited-member search, source anchors,
representative narrow viewports and navigation with JavaScript disabled. It is
separate from deployment authority and the provider-free test suite.

## Validation evidence

- `make test-native`: **821/821 passed**.
- `make test-python`: **686 tests passed**; package compilation with warnings
  treated as errors also passed.
- `make test-policy`: **302 tests passed** after integrating current main,
  including 17 source-reader, 10 Python publication and 10 runtime-qualification
  fixture tests. The CMake 3.31.10 tools were placed on PATH for its real CTest
  failure-propagation fixtures.
- Native-backed `qualify_python_reference.py`: **passed** for 162 exports,
  268 callable signatures, 93 frozen dataclasses/364 fields, 43 enums/314 named
  values and 31 Python source files, using Python 3.12.14. The signatures include
  149 operations, 25 explicit source constructors, 93 generated dataclass
  constructors and the public function. Both direct generation and an explicit
  publication-style `--inventory` report passed.
- Source comparison with the starting commit: **all 17 edited Python modules
  are AST-identical after docstrings are removed**.
- Six guide Python examples parse and import public names; all 171 authored API
  references resolve. The examples were not executed against a hardware provider.
- Generated reference: **194 files and 3,398 local links validate**. Tests cover
  missing/extra/substituted artifacts, forged records even after resealing,
  source/run mismatch, portal collisions, symlinks, active content, remote
  resources and live rejection before network access.
- `git diff --check`: **passed**.

### Complete publication qualification

The pinned `docs_ci.py build` passed at implementation commit
`707752c435efca8e7ece7496d6a4d6724ab71ba8`, including its independent verification
before sealing. It used GNAT 16.1.0, GNATdoc 26.0.0, GPRbuild's qualified
26.0.0 executable, enhancer 0.1.5 and Rust 1.98.1. Rust generation remained
locked/offline and did not compile or load the native facade.

The publication contains **2,573 files**: 59 Ada portal/reference assets,
2,320 Rust assets and 194 Python assets. The Rust portion includes 1,651 HTML
pages; Python includes 191. Ada prose coverage remains informational at
5,145/5,146 elements with 101 classified GNATdoc warnings at this checkpoint.
These existing Ada measurements are recorded, not made into Python acceptance
thresholds.

The local artifact has `mode=local-not-deployable`. Its external digests are:

```text
manifest sha256: 2fe64bf91999d2fbccb2473d01a0ca664f120ebca0eee69cc08bba247f32abd4
bundle sha256:   9d3e9d7e38b9926764e9f8e5a4b6fd07d6885edc548a5ac5c112f2a4bb064e35
```

An actual `docs_ci.py unpack` into a fresh directory, supplied with both digests,
also passed full source/run and reconstructed-publication verification. The
corresponding hosted [documentation build](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43743253124)
passed at the same implementation commit. Its PR deployment and live-site jobs
were correctly skipped. A normal merge and successful main-push workflow are
required to publish these pages to the live site.

### Browser limitation

Chromium could not launch in this workspace: its local process socket was denied
by the managed execution policy. The configured approval policy rejected an
unrestricted invocation. Consequently no Chromium rendering, responsive-layout
or interactive-search pass is claimed here. Structural links, complete no-script
navigation, asset content and escaping are covered by the source/publication
tests; visual/browser qualification remains a separately runnable check.
