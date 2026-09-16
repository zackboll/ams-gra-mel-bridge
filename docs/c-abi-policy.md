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

## Task 001 provider session contract

The façade ABI remains 0.1. `ams_mel_session_open` creates one uniquely owned,
opaque session only after the provider library is loaded, both published
factories return non-null owners, and `Control::init` returns `Success`. Failure
leaves the caller's initially-null output unchanged. `ams_mel_session_close`
accepts a pointer to that owner, clears it, destroys `Control`, then
`API_Manager`, and only then unloads the library. Closing an already-cleared
owner succeeds. A null owner pointer is invalid; arbitrary non-null or dangling
handles are outside the contract.

`ams_mel_session_get_provider_version` preserves all four fields of upstream
`mel::VersionInfo`: two `uint32_t` values and the vendor/description strings.
Strings must be valid UTF-8 without embedded NUL, are validated and copied to
caller-owned storage, and have no lifetime after the call. Capacities and
required lengths count bytes including the NUL.
Callers may first supply null/zero buffers to discover both lengths. Insufficient
capacity returns `AMS_MEL_BUFFER_TOO_SMALL`, writes both required lengths, and
does not modify numeric fields or either buffer; strings are never truncated on
success.

Each fallible call optionally writes a per-call UTF-8 diagnostic and its full
required capacity including NUL. A short buffer receives a NUL-terminated,
valid UTF-8 prefix rather than a partial code unit. Diagnostic reporting does
not allocate; invalid provider exception text is replaced with a fixed valid
UTF-8 fallback. Diagnostics are informational; output validity is determined by
status. All ordinary C++ exceptions are translated. Provider destructors and
deleters are required not to throw, consistent with ordinary C++ destruction
rules.

The initial threading contract permits concurrent use of independent sessions.
Calls using the same session, including close, must be externally serialized.
No callback, asynchronous work, cancellation, timeout, or child-resource
guarantee is introduced by this slice.
