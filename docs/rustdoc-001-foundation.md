# RUSTDOC-001 — standalone Rustdoc build and qualification

## Identity, scope and coverage delta

Freshly fetched `origin/main`: `76631880ee236b9839b8b9668be36e423ef1b050`,
tree `e21e61cf735ac37d56588e5798d997ebe67e5d8b`. Independent branch
`docs/rustdoc-001-foundation`, worktree `/home/zboll/git/ams-rustdoc-001`.
The final implementation commit/tree and PR URL are recorded in the PR and
delivery report rather than embedding a self-referential hash here.

This task adds only a standalone Python-standard-library builder/verifier, its
focused regression tests, and this document. No Rust API, ABI, source comments,
native behavior, Ada GNATdoc, existing workflow, publication destination, or
dependency changes. No implementation coverage classifications change: the
structured IR inventory reviewed under `docs/reference/irmel-api-schema.md`
classifies native/Ada evidence, not Rust documentation generation. It remains
unchanged, as does the historical coverage archive.

Coverage delta: reproducible safe-crate HTML generation, separately named raw
reference, diagnostic measurement, local file-link verification, and opt-in
doctest qualification. This is not documentation completeness, real-provider
interoperability, a deployment artifact attestation, or a mandatory CI gate.

## Reproduction and prerequisites

From a clean checkout, with its absolute root in `repo`:

```sh
repo=$(pwd -P)
python3 "$repo/scripts/rustdoc_001_foundation.py"
python3 "$repo/scripts/rustdoc_001_foundation.py" --doctests
python3 -W error -m unittest discover -s "$repo/scripts" \
  -p test_rustdoc_001_foundation.py -v
```

The first invocation needs Python 3.9+ and an installed host Rust toolchain
(Cargo, rustc, rustdoc, standard library, host linker). It **does not** need
CMake, `ams_mel_c`, a mock or real provider, GNAT, credentials, containers,
hardware or downloaded crates. The checked-in workspace lockfile currently
contains only `ams-mel` and its path dependency `ams-mel-sys`. The script sets
Cargo offline mode and uses `--locked`; it does not install a toolchain.

The second invocation also requires CMake >= 3.20, a C11/C++20 toolchain and
native build tool, including the host C++ runtime and platform dynamic loader.
It calls the existing production-only `native/scripts/build.sh`, which builds
the vendored closure locally with two parallel jobs and sanitizes `LD_RUN_PATH`.
`AMS_MEL_BUILD_TESTS=OFF`; no test facade, failpoints, mock-provider targets or
`native/build-tests` reconfiguration is involved. CMake remains the only native
compiler owner. The builder selects the absolute `native/build/lib` directory
via `AMS_MEL_NATIVE_LIB_DIR`, ignoring a caller's alternate facade selection.

Qualified host: Linux x86_64, Python 3.13.5, Cargo 1.98.1
(`797e8a9bc 2026-08-05`), rustc/rustdoc 1.98.1
(`48a229cea 2026-09-01`), CMake 3.31.6, Debian GCC/G++ 14.2.0-19.
The toolchain is **recorded, not newly pinned or installed** by this task.
No MSRV determination, macOS/Windows or cross-target qualification is claimed.
`CARGO_BUILD_TARGET` is rejected; use a normal host configuration. Caller Cargo
configuration and compiler flags still apply and may cause a real failure;
use a consistent host environment for reproduction. Versions and applicable
Rust flags are retained in the report. HTML bytes/filenames can change across
Rustdoc releases; reproducible here means repeatable generation/verification
with the same source, lockfile and toolchain, not a cross-version byte guarantee.

### Why HTML needs no native library but doctests do

Both crates execute build scripts. `ams-mel/build.rs` adds native search and
Unix runtime paths; `ams-mel-sys/build.rs` additionally requests dynamic linking
of `ams_mel_c`. Their default is `native/build/lib`, overridable by
`AMS_MEL_NATIVE_LIB_DIR`. Neither script compiles C++, checks that the facade
exists, nor loads a provider. Rustdoc/type metadata generation does not link a
native executable, so both doc commands succeed with that directory absent.
Executable doctests inherit the sys crate's native link requirement, even if
the example itself calls no native function. The qualified Linux runtime finds
the library through the safe build script's emitted absolute rpath; no custom
`LD_LIBRARY_PATH` was needed.

## Exact commands and diagnostic baseline

The unmodified workspace was probed with these commands (manifest shown
relative to the checkout root to match the requested Cargo interface):

```sh
cargo doc --manifest-path rust/Cargo.toml -p ams-mel --no-deps --locked
cargo doc --manifest-path rust/Cargo.toml -p ams-mel-sys --no-deps --locked
cargo test --manifest-path rust/Cargo.toml -p ams-mel --doc --locked
```

Both doc commands passed on the clean checkout **before native compilation**,
with no default diagnostic warnings. `--no-deps` does not generate the raw
dependency's reference: build `ams-mel-sys` explicitly as a second command into
the same fresh target directory. Readers see distinct crate names and a landing
page labelling the sys reference **raw, unsafe C ABI**, not another safe API.

The standalone builder adds Cargo `--message-format=json` and Rustdoc flags:

```text
--deny rustdoc::broken_intra_doc_links --warn missing_docs
```

Both diagnostic builds pass. Broken intra-doc links: **0** for each crate.
Explicitly denying them is a local build check, not a new repository workflow.
`missing_docs` is a rustc lint accepted by Rustdoc; it is not named
`rustdoc::missing_docs`. Default silence does not mean all items are documented.
Machine-readable warning totals at the base above:

| Missing documentation kind | `ams-mel` | `ams-mel-sys` |
|---|---:|---:|
| Struct | 14 | 329 |
| Struct field | 305 | 1,300 |
| Enum | 8 | 0 |
| Enum variant | 211 | 0 |
| Associated function | 9 | 0 |
| Method | 24 | 0 |
| Function | 1 | 311 |
| Type alias | 0 | 16 |
| Constant | 0 | 428 |
| **Total** | **572** | **2,384** |

These are diagnostic events, not an API count, coverage percentage or complete
prose/semantic-quality assessment. The script records current totals without
asserting these numbers or failing on missing documentation. No blanket
`-D warnings`, new lint attributes, comment changes or repository-wide coverage
ratchet is added. Cargo summaries are not double-counted.

## Doctest qualification

The exact safe-crate doctest command without native artifacts failed with
**32 passed, 1 failed**: the runnable `ModeCommand` example could not find
`-lams_mel_c`. After `sh native/scripts/build.sh`, the same command with the
absolute production `AMS_MEL_NATIVE_LIB_DIR` passed **33/33**, zero ignored.
The script's complete `--doctests` invocation reproduced that success.

| Kind | Count | What was actually qualified |
|---|---:|---|
| `compile_fail` | 27 | Compile-time ownership/trait/interface restrictions; not runtime provider behavior. |
| `no_run` | 5 | Type-check examples for BIT, ConfigSet, Image control-only, RF Admin and RF Data. Not executed. |
| Runnable Rust | 1 | `ir_mode::ModeCommand` construction/default/state assertion. Native link dependency, but no provider call. |

Plain-text code blocks are not doctests. The five `no_run` examples describe
operations that would need valid channel/session/provider inputs if executed;
RF examples use illustrative `/opt/provider/librfmel.so`, not an installed test
facility. No current doctest requires mock-provider delivery or native test-only
hooks. Doctest success is not qualification of those workflows at runtime.
Rust unit/integration suites do use the separate test-enabled facade and mock
providers through `make test-rust`; they are not substituted for this probe.

## Output, public entry points and future relative-link contract

Each invocation creates a fresh, uniquely named ignored directory:

```text
rust/target/rustdoc-001/run-<unique>/
  qualification.json
  <stage>.stdout.log / <stage>.stderr.log
  target/doc/
    index.html                 task-owned safe/raw landing page
    ams_mel/index.html         safe public crate root
    ams_mel/struct.Session.html
    ams_mel/all.html           safe item inventory
    ams_mel_sys/index.html     raw public crate root
    ams_mel_sys/all.html       raw item inventory
    src/ams_mel/               linked source HTML
    src/ams_mel_sys/           linked raw source HTML
    static.files/             hashed CSS, JS, fonts, icons and license notices
    search.index/             version-specific search shards (qualified Rustdoc)
    trait.impl/ and type.impl/ version-specific implementation metadata
    crates.js / src-files.js / help.html / settings.html
```

Public safe exports are at the crate root or re-export target pages; root
`Session`, `ImageStream`, `ControlChannel`, `HealthStatusChannel`, `ChannelView`,
request owners, metadata/value types, `RfAdmin`, `RfData` and the public
`ir_nuc` module are examples, not a new authoritative API inventory.
Rustdoc's normal public-only output is used: no `--document-private-items`.
Some re-export pages live under declaring module directories, which must remain.

The verifier checked **1,429 HTML files** in the qualified build, including
the landing page. It rejects missing local `href`/`src` file targets, path escapes,
root-relative URLs and symlinks, and requires both crate roots, representative
safe API, source and shared assets. Native `cargo doc --no-deps` emitted two
otherwise dangling `./index.html` links in root help/settings pages; adding the
landing page repairs them without rewriting Rustdoc HTML.

For future GitHub Pages integration, copy the **entire** `target/doc` subtree
under a single prefix such as `rust/`, retaining names, relative depth, source,
search shards, implementation metadata, fonts and notices. Do not copy just
`ams_mel/` or merge individual `static.files` trees from different toolchains.
Link the portal to `rust/ams_mel/index.html` (safe),
`rust/ams_mel_sys/index.html` (raw), or `rust/index.html` (landing), all relative
to the portal. No absolute project/domain prefix is inserted. Standard-library
links point externally to `doc.rust-lang.org`; they are not fetched or vendored.

Local verification is intentionally **not** a Pages security gate: HTML anchor
IDs, dynamically loaded JS search/trait resources, CSS URLs, external URLs,
browser execution and visual rendering are not exhaustively checked. All files
are preserved and SHA-256 inventoried to support later integration work.
`qualification.json` also records Git HEAD, dirty status, tool versions and
diagnostic counts; logs preserve full compiler messages and test results.
Dirty runs are labelled, not certified as committed-source artifacts. Existing
source-identity, artifact-origin and deployment-security checks are untouched;
a future deployment must retain those checks and qualify this new destination.
Nothing here uploads, publishes, changes workflows or silently claims to satisfy
the existing Pages artifact contract. Retained run directories need manual
cleanup when no longer useful; the script never deletes an existing run.

## Actual validation and limitations

In `/home/zboll/git/ams-rustdoc-001`:

- Requested safe and raw `cargo doc` commands: passed without native output.
- Explicit broken-link/missing-doc probes: passed with totals above.
- Missing-native doctest negative probe: expected link failure, 32/33 passed.
- Production native build: passed, no provider targets built.
- Production-linked safe doctests: passed 33/33.
- `python3 scripts/rustdoc_001_foundation.py --doctests`: passed; 1,429 HTML
  files verified, both references generated, full logs/report/hash inventory.
- A second `python3 scripts/rustdoc_001_foundation.py` invocation: passed in a
  different fresh target directory with the same HTML/diagnostic counts.
- `python3 -W error -m unittest discover -s scripts -p test_rustdoc_001_foundation.py -v`:
  **8/8 passed**, including link/asset mutations, symlink/path rejection,
  diagnostic counting, process failure, fresh outputs, offline/locked commands
  and opt-in-only native/doctest orchestration.
- `python3 -W error -m unittest discover -s scripts -p test_doc_source_integrity.py -v`:
  **18/18 passed**.
- `python3 -W error -m unittest discover -s docs/tests -p test_docs_ci.py -v`:
  **18/18 passed**, including source/run identity and artifact-security tests.
- Python byte compilation, `python3 scripts/check_final_newlines.py` and staged
  Git whitespace checks: passed. Restricted source/workflow paths unchanged.

Full native contract/FFI, Ada, Alire, real-provider, full Rust integration,
GNATdoc and Pages deployment suites are not claimed by these documentation-only
tests; no compiled implementation or workflow changes. Existing source comments
and inventories remain unchanged. No upstream MEL semantic question needs to be
guessed for this build slice. Future work includes pinned documentation tooling,
browser/search qualification, reviewed source/artifact provenance integration,
relative portal navigation, and any separately approved documentation cleanup.
