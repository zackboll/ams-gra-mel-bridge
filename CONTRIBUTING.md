# Contributing

This is an experimental bootstrap. Prefer small vertical slices: upstream
contract review, C ABI, C++ implementation, C consumer test, Ada import/wrapper,
Ada test, and coverage documentation in the same change.

Run `make check` with a native toolchain and GNAT/GPRbuild. Also run the Alire
commands in the README when changing manifests, GPR files, or build hooks.
Use an out-of-tree CMake directory when switching compilers or sanitizers.
Do not check in generated build products or modify vendored interfaces silently.

A pull request should state what changed, exact commands run, actual results,
unsupported cases, and any unresolved upstream behavior. Do not claim registry
readiness, real-provider interoperability, or formal proof based on smoke tests.

Ordinary Ada/C++/Rust/Python development is not subject to frozen GNATdoc warning
or documentation-coverage baselines. Bootstrap still enforces compilation, tests,
ABI checks, dependency integrity and deployment security. The separate docs build
generates once from the current committed Git source and rejects actual build,
output-integrity and destination errors. See `docs/doc-ci-simplify-001.md` for
the pinned local docs command and non-deploying scheduled/manual qualification.

Before a first public release, complete the release checklist in
`docs/packaging.md` and select actual maintainer contact information.
