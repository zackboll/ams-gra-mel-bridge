# Changelog

## Unreleased

- Expose the existing IR BIT empty/no-op profile through safe Rust with typed
  `CommandReturn`, structured `ReturnResult`, and reusable asynchronous
  `ReturnRequest` ownership; `Return::Fail` remains a normal completion.
- Add the required IR BIT command in the pinned-provider-compatible empty/no-op
  profile for C and Ada, with reusable asynchronous `RequestFor<Return>` ownership.
- Synchronize the complete 19-function raw Rust sys ABI while leaving safe Rust
  and Python BIT APIs for later tasks.
- Add the safe Rust Session foundation over the existing C ABI.
- Add safe Rust IR host-memory Mono8 and C2 Operate/TaskSched bindings.
- Validate the current safe Rust IR slice against the pinned real Squall stack.
- Add the dependency-free Python Session foundation and IR host-memory Mono8
  binding with owned-copy frames and mock-provider tests.
- Add the dependency-free Python IR C2 Operate/TaskSched binding with explicit
  enable, asynchronous requests, complete structured rejection results, cached
  waits, retryable close, and independent/coexisting native lifetimes.
- Validate the current safe Python IR Mono8 plus C2 slice against the pinned real
  Squall stack and extend the opt-in harness to run C, Ada, Rust, and Python.
- Add an opt-in real Squall IR MEL integration harness with strict pinned-source
  validation, hardware-free simulated optical/Couloir orchestration, and public
  C11/Ada C2-plus-Mono8 smoke clients; ordinary builds and CI remain provider-free.
- Add IR CommandAndControl attach/enable and asynchronous
  `ModeCmd(Operate, TaskSched)` requests for C and `AMS.MEL.IR.C2`.
- Distinguish timeout, normal MEL `ErrorOr` rejection, provider exception,
  provider failure, and null successful mode results.
- Retain C2/provider/library ownership through pending requests, including after
  public request, C2, or Session close; add C2/image coexistence coverage.
- Expand the immutable upstream declaration closure from 71 to 85 headers.
- Add a host-memory, single-band Mono8 `IRSTImage` receive profile for C and Ada.
- Add bounded DROP-INCOMING frame queues, exact-size caller copies, saturating
  counters, and distinct timeout/stopped results.
- Extend the separately loaded C++ mock with asynchronous image production,
  malformed/failure scenarios, and lifecycle instrumentation.
- Expand the immutable upstream declaration closure from 44 to 71 headers.
- Harden IR callback teardown around channel destruction and explicit in-flight
  tracking; propagate release/provider cleanup failures through C and Ada.
- Validate Mono8 channel capabilities and configuration integer ranges, and
  document the per-stream single-consumer receive contract.
- Add the task 001 IR MEL provider load/init/version/close slice in C and Ada.
- Vendor the exact reviewed Common MEL, IR MEL, and AMS Math declaration closure.
- Add separately loaded mock/failure providers and native/Ada lifecycle tests.

## Unreleased — 0.1.0-dev

- Create separate native and Ada development crates.
- Implement one C-callable ABI version query with C and C++ tests.
- Add an idiomatic Ada version query and a separate smoke-test crate.
- Add native installation support, local gates, and Linux CI definitions.
- Record review provenance, scope, ABI policy, and the next implementation task.

At that bootstrap snapshot, no provider, IR, RF, OMS client, or Rust binding was
implemented yet.
