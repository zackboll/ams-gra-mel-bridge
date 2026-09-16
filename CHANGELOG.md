# Changelog

## Unreleased

- Add a host-memory, single-band Mono8 `IRSTImage` receive profile for C and Ada.
- Add bounded DROP-INCOMING frame queues, exact-size caller copies, saturating
  counters, and distinct timeout/stopped results.
- Extend the separately loaded C++ mock with asynchronous image production,
  malformed/failure scenarios, and lifecycle instrumentation.
- Expand the immutable upstream declaration closure from 44 to 71 headers.
- Add the task 001 IR MEL provider load/init/version/close slice in C and Ada.
- Vendor the exact reviewed Common MEL, IR MEL, and AMS Math declaration closure.
- Add separately loaded mock/failure providers and native/Ada lifecycle tests.

## Unreleased — 0.1.0-dev

- Create separate native and Ada development crates.
- Implement one C-callable ABI version query with C and C++ tests.
- Add an idiomatic Ada version query and a separate smoke-test crate.
- Add native installation support, local gates, and Linux CI definitions.
- Record review provenance, scope, ABI policy, and the next implementation task.

No provider, IR, RF, OMS client, or Rust binding is implemented yet.
