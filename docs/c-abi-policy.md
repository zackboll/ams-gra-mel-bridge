# Experimental C ABI policy

The current ABI is 0.1, not yet stable. Header version macros describe this
facade only. Provider API, upstream source, architecture, and package versions
must remain distinct.

## Required for subsequent features

- Fixed-width numeric status and enum representations, not compiler-dependent
  enum storage. Validate unknown values and conversions.
- Typed opaque resource handles; ownership, borrowing, and close behavior are
  part of the contract. Do not promise detection of arbitrary dangling pointers.
- Explicit string length, encoding, output capacity, and lifetime rules.
- No C++ exception propagation through exported operations or foreign callbacks.
  `noexcept` alone is not an exception translator; fallible operations must catch
  and map ordinary C++ exceptions without losing useful error information.
- Keep metadata and optional/variant discriminants explicit. Do not memcpy C++
  objects into C records or treat their in-memory bytes as serialized messages.
- Per-call diagnostics or explicit error objects, not only mutable global state.
- Versioned value records. Do not append fields to an existing fixed-layout
  record without a compatible size/version scheme or a new type and operation.
- Asynchronous submission, pending, completion, wait timeout, and cancellation
  must be distinct concepts. Do not imply that a timeout stops provider work.
- Poll/wait/receive from application threads first. Queue provider callbacks in
  native code with bounded capacity and a documented overflow policy.
- Never unload provider code until all children, requests, deleters, and in-flight
  callbacks are finished. A C ABI does not make a mismatched C++ provider safe.
- Host-memory-only is an explicit initial constraint. Do not dereference device
  addresses as Ada arrays or Rust slices.

Current `ams_mel_get_abi_version` has no allocations or fallible C++ operations;
its null-pointer rejection is the only failure path. Future provider-related
status values should be designed with the corresponding feature, not prefilled
with fake implementations returning success.
