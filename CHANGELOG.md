# Changelog

## Unreleased

- Add the conditionally required IR Instrumentation channel
  (`@RequiredIfInstrumentation`) in native C, safe Ada, raw Rust, and private
  Python. The slice is exactly the Instrumentation-specific conditional surface
  -- `send(InstrumentationLevelCmd)` and the `InstrumentationReport` metadata
  callback -- plus `Enable` and `ChannelCapability`. One canonical
  `ams_mel_ir_instrumentation_report_v1` carries both future completions and
  metadata events, preserving full `uint32` command ID/size and signed
  nanosecond timestamps; upstream `Priority` is exposed as exactly Normal/Debug
  with no invented MaxExclusive value. The mock provider validates positive
  behavior, full payload fidelity, and synchronous callbacks emitted from both
  `registerMetadataCallback` and `send`. Pinned Squall does not support this
  channel and is validated only for clean unsupported-provider behavior. ABI 0.1
  grows from 59 to 72 exports; native CTest grows from 9 to 10. No safe Rust or
  public Python Instrumentation API is added, and Instrumentation-specific
  copies of the inherited generic Channel services are deliberately not cloned.

- Add the `ImageChannel::send(NavigationReport)` request/future vertical slice
  in native C, safe Ada, raw Rust, and private Python: the complete published
  `NavigationReport` (all 18 covariance terms), asynchronous submit/wait/close,
  deferred provider teardown while a request is outstanding, allocation-free
  emergency retention for post-send facade failures, and a dedicated native
  contract test. ABI 0.1 grows from 56 to 59 exports.
- Refactor private Image stream ownership into shared `ImageStreamState`. The
  public stream remains an opaque thin owner; ABI 0.1, its 56 exports, and all
  application-visible behavior are unchanged. NavigationReport remains unimplemented.
- Add owned Image `NavigationReportResp` metadata (kind 4) in C and safe Ada.
  ABI 0.1 remains exactly 56 exports; NavigationReport send is not implemented.
- Extend the existing Image metadata FIFO with complete owned LineOfSightReport and
  LineOfSightEuler values in C and Ada. Raw Rust and private Python ABI declarations
  track the added records and constants; ABI 0.1 remains exactly 56 exports.
- Add ImageChannel capability access and the first Image metadata vertical slice:
  bounded DROP-INCOMING BadPixelList callbacks with complete owned C and Ada values,
  synchronous-registration safety, malformed/allocation recovery, and teardown-safe
  event lifetime. The raw Rust/private Python ABI now tracks all 56 exports; no safe
  Rust or public Python metadata API is added.
- Add an additive owned full `FrameHeader` snapshot (`receive_snapshot`/view/close)
  while preserving legacy `ams_mel_ir_frame_v1` and `ams_mel_ir_stream_receive`.
  One bounded FIFO serves both receives.  Native C and safe Ada preserve complete
  contributing sensor, ordered duplicate flags, inertial/nav states, independent
  Euler/quaternion variants, and owned pixels; raw Rust/private Python track 49 exports.

- Add application-facing inherited C2 Channel services in native C and safe Ada:
  pre-enable KeepAlive, asynchronous ChannelCommsTest request/reply, its callback
  through the existing metadata queue, and complete owned ChannelCapability
  snapshots. Synchronize the 36-function raw Rust/Python ABI only; explicit
  generic buffer management, Scheduling, and safe Rust/Python APIs remain out of scope.
- Add all three required C2-specific metadata callbacks through a bounded native
  FIFO queue with DROP-INCOMING overflow, complete deep-copied CommandStatus,
  BIT_Configuration, BIT_Status, and nested Fault values, immutable event owners,
  and safe Ada-owned events. Expand only the raw Rust sys/private Python ABI to
  the complete 28-function façade; their safe metadata APIs remain unchanged.
- Complete all three required IR C2 command sends in the native façade and Ada:
  general ModeCmd with complete ScanParam, full raw BIT payload transport with a
  safe one-choice Ada API, and opaque ConfigSet. Synchronize only the raw Rust
  sys and private Python ctypes layers, expanding the façade to 22 functions.
- Expose the existing IR BIT empty/no-op profile through dependency-free Python
  with typed `CommandReturn`, structured completion/rejection values, reusable
  `ReturnRequest`, complete cached diagnostics, and parent-independent ownership.
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
