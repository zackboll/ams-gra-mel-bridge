# Repository instructions

Read `docs/architecture.md`, `docs/c-abi-policy.md`, and `docs/coverage.md`
before changing a public interface. `.clinerules/` carries the same key rules
for Cline. The original design review is reference material, not an assertion
that its proposed APIs have already been implemented.

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
- Do not publish, push, create releases, or alter global Git settings unless
  the user explicitly requests that action.

Validation: `make test-native`; `make test-ada`; `make check`.
Alire validation: `alr -C ada build` and `alr -C ada/tests run`.
