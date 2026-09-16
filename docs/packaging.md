# Packaging and release checklist

## Current development layout

`native/alire.toml` defines `ams_mel_c`; `ada/alire.toml` defines `ams_mel`;
`ada/tests/alire.toml` defines a separate test executable. The latter two use
local development pins. The native crate is self-contained for its present
single source file, script, license, and CMake configuration.

CMake compiles the shared library into `native/build/lib`; `ams_mel_c.gpr`
imports it. One source has one build owner. The current pre-build hook selects
a Debug build. Profile, compiler, cross-toolchain, and shared-cache handling need
explicit validation before publication; this is not a claim of production
package integration.

The host must provide CMake, C++20, and a build tool. Real dependency metadata,
maintainer contact, supported-platform restrictions, and package names must be
finalized before indexing. No maintainer identity or repository URL is invented
in the starter manifests.

## Alire release gate

- Verify/reserve suitable package names and select license/maintainer metadata.
- Validate `alr -C ada build` and `alr -C ada/tests run` in a clean environment.
- Verify native pre-build action and transitive GPR/library resolution.
- Make release archives contain each crate's complete source closure.
- Publish/resolve the native dependency first; remove development path pins from
  consumer release metadata. Do not confuse a workspace build with index
  resolution.
- Keep local test pins out of published dependency requirements as appropriate.
- Declare necessary build tools/dependencies and test target availability.
- Verify static/shared linkage choices, compiler compatibility, installed
  deployment, and runtime library search paths outside the checkout.
- Test an extracted immutable archive without the development workspace,
  private credentials, an initialized submodule tree, or a sensor provider.
- Preserve license/NOTICE files for any vendored upstream code.
- Do not claim a complete Skill or GRA compliance from a library build.

## Rust later

The usual registry route is crates.io via Cargo, not a manual Cargo-index PR.
A future sys crate owns native build/linking; a safe crate wraps it. Both should
reuse this canonical native source through release packaging, never a published
relative path to a sibling checkout. No Rust package has been created here.

## Official references (consult again before release)

- https://alire.ada.dev/docs/catalog-format-spec
- https://alire.ada.dev/docs/
- https://doc.rust-lang.org/cargo/reference/publishing.html
- https://doc.rust-lang.org/cargo/reference/build-scripts.html
