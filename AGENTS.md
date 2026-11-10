# Repository instructions

Read `docs/architecture.md`, `docs/c-abi-policy.md`, relevant task documents,
and the applicable structured coverage inventories before changing a public
interface. `docs/coverage.md` is a historical archive, not current coverage
authority; preserve its existing information and do not prepend new feature
sections or add new feature status there. `.clinerules/` carries the same key
rules for Cline. The original design review is reference material, not an
assertion that its proposed APIs have already been implemented.

## Parallel language development

- Require one uniquely named, task-owned Markdown document per feature, such as
  `docs/py-mel-009-<feature>.md`, `docs/irmel-ada-012-<feature>.md`, or
  `docs/rust-ir-007-<feature>.md`. Record the coverage delta, implementation
  boundaries, tests with actual commands/results, limitations and unresolved
  semantics there. Do not create a shared feature-status index to update per PR.
- Ada, Python and Rust tasks should change only their own implementation/test
  files and uniquely owned feature documentation wherever practical. Avoid shared
  `CHANGELOG.md`, generic README sections, `docs/architecture.md`,
  `docs/c-abi-policy.md` and `docs/coverage.md` edits merely for feature status prose.
- This separation is not a correctness exemption: update the native ABI, actual
  shared interfaces, security contracts and authoritative structured coverage/API
  inventories (and their generated reports) whenever the implementation requires
  it. For IR Ada, follow `docs/reference/irmel-api-schema.md`; task prose must not
  replace or leave stale the reviewed coverage evidence.
- Keep ordinary Git merge behavior. Do not use blanket `merge=union` attributes
  or custom conflict resolvers. Disjoint task documents remove a routine conflict
  source; genuine shared-code or contract conflicts still require review.

## Implementation and validation

- Keep one consumer-side C ABI, implemented privately in C++20. Public headers
  must compile as C11 and must never expose STL, C++ references, or exceptions.
- Ada is a client of that ABI. Keep raw addresses and C imports in private
  packages; do not add `Interfaces.C` types to the public Ada API.
- CMake alone compiles native sources. GPR imports the resulting native library.
- Keep OMS/UCI, processing algorithms, Squall-private transports, and Rust out
  of the generic MEL library. Do not add them to make a test pass.
- Public API and ownership changes require matching tests in C and Ada.
- All first-party compiled source must be warning-clean and build with warnings
  treated as errors. Fix warnings when introduced rather than suppressing them;
  vendored upstream source remains byte-identical and is not modified to satisfy
  the local warning policy.
- Keep builds provider-free. Use a separate mock provider before a real one.
- Treat upstream revisions in the review as candidates, not verified locks.
- Never silently drop metadata, narrow numeric representations, equate timeout
  with cancellation, or promise callback quiescence without evidence.
- Never unload provider code while any object, deleter, request, or callback can
  still use it. C++ ABI compatibility remains necessary behind the C facade.
- Do not report a test as passed if the toolchain or provider was unavailable.
- Whenever a task changes first-party Ada sources under `ada/src` or
  `ada/tests/src`, run `make format-ada` before build/test and
  `make check-ada-format` before final validation/commit. Do not manually fight
  formatter output; investigate/configure GNATformat when its output is undesirable.
- Do not publish, push, create releases, or alter global Git settings unless
  the user explicitly requests that action.

Validation: `make test-native`; `make test-ada`; `make check`.
Alire validation: `alr -C ada build` and `alr -C ada/tests run`.
