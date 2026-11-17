# RUST-IR-019 — safe standalone host Buffer

## Preparation and coverage delta

Based on freshly fetched main `624c10a` (the supplied starting main was
`51ede458587698cc1f8e4601d3471852d6f2a9e5`, an ancestor), in isolated worktree
`/home/zboll/git/ams-rust-ir-019`, branch
`feature/rust-ir-019-buffer-foundation`. Existing worktrees, open PRs and Buffer
history were inspected; no duplicate Rust Buffer feature was present. Open
PRs #233, #236 and #239 were unrelated. Repository instructions, architecture,
C ABI policy, Ada Buffer implementation and tasks 018/020, native Buffer and
raw Rust declarations, and the structured IR inventory schema were inspected.

Safe Rust previously had no standalone application-managed Buffer owner. It now
has `Session::open_host_buffer`, unique `HostBuffer`, owned `BufferInfo`,
`ImageLocation`, exact `BufferFlag`, and structured `BufferOpenError` and
`BufferFlagsError`. All are exported from `ams-mel-ir` and re-exported as the
same concrete types by compatibility crate `ams-mel`. No dependency is added.
The raw sys crate already exposed the complete eight-function standalone ABI.
Native exports, layouts, sys declarations, Ada/Python implementations and
structured **Ada** coverage are unchanged; no Ada coverage claim is added.

## Ownership and Returns

`HostBuffer` is opaque, non-Clone, non-Send and non-Sync. Compile-fail doctests
exercise these restrictions. No public function accepts native handles or
borrowed host memory. Private unsafe calls use only the existing C bridge.
The owner retains its original allocation capacity for subtraction-based bounds
checks. No storage is resized or relocated by Rust. The native allocation is
zero-filled and initialized exactly once, preserving the entire signed i64
context. Zero and sizes beyond signed/native allocation bounds are rejected;
native allocation limits and failures remain authoritative.

The native owner retains the complete Session/provider graph, so closing or
dropping the public Session does not invalidate a successful Buffer. Explicit
Close is idempotent and retryable. Failed Close preserves the native output
handle, notably when an extra provider shared owner prevents safe destruction.
Successful Close must clear it. Drop makes one non-panicking best-effort attempt
with null diagnostics. Drop is **not proof of cleanup**: uncertain native graphs
may remain allocated until process exit. No unrestricted `Buffer::release()`
call is made. Native destruction must destroy the provider Buffer while host
storage remains alive, then Control, manager and finally unload the DSO.

Open failure separates existing `Error` (category/diagnostic), actual numeric
native status, and actual numeric initialization Return. Flags failure does the
same for the latest `getFlags` query. The ABI's UINT32_MAX sentinel maps to
`None`, not a fabricated Return. Unknown Return numbers remain numeric. An OK
status alone cannot manufacture successful initialization: non-null ownership
and actual Success Return are required. Unexpected Open ownership on failure
gets one ordinary best-effort Close; failed cleanup retains the native graph.
Malformed diagnostics remain protocol errors without losing structured native
status/Return in these two error types. Void `setFlags`/`addFlag` receive no
invented upstream Return.

## Properties, storage and flags

`info()` copies original storage size/context separately from actual signed
provider size/context. Negative and differing getters are not normalized. The
address-match value is a checked boolean. The image address is only classified
as Null, InsideStorage or OutsideStorage; unknown classifications fail. No raw
address escapes and no readable image length is inferred.

Storage reads/writes call the existing native copying operations. All byte
values are preserved, with independent Rust read vectors and borrowed input
only for the duration of writes. Empty end-of-allocation spans are valid;
out-of-range/overflowing spans fail before allocation/native access. Reads
allocate fallibly and do not return partially written data on failure. No
mutable native-backed slice is exposed.

Flags preserve order, duplicates, emptiness and exact Host/Gpu/Rdma values.
**GPU/RDMA flags are bookkeeping, not GPU/RDMA-backed allocation.** A count
query and at most one copy query use checked fallible allocation. Shrinkage is
accepted; growth fails without retry or partial publication. Unknown flags,
unwritten elements, missing/non-Success Returns, inconsistent count-query
status and successful copied counts beyond capacity fail closed.

## Boundaries and unresolved semantics

This is a standalone application-owned Buffer, **not a delivered image Frame**.
There is no channel attachment, registration, checkout, release, image layout,
generic/StackedImage acquisition or device allocation. The eight-callback
reference consumer and Mono8 acquisition regressions are untouched. Real
hardware/provider qualification is outside this mock-only task.

The compiled retained-owner mock deliberately has no relinquish hook: two Close
refusals and Drop retention are tested with the real native bridge. A narrow
private seam proves refusal followed by successful retry without changing the
mock or production ABI. Other private seams cover malformed outputs/counts,
unknown values, partial read failures and allocation failures that the standard
provider cannot generate. The bridge remains the trusted memory-provenance
boundary; Rust cannot defend against a C implementation writing past a supplied
output capacity.

## Executable example

From the repository root, after building the test provider:

```sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
cargo run --manifest-path rust/Cargo.toml --locked -p ams-mel-ir \
  --example host_buffer -- "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

The example opens a Session, creates a Buffer, writes/reads bytes, sets/appends
flags, inspects properties and explicitly closes Buffer then Session. It creates
no channel, image acquisition or Frame.

## Qualification

Actual commands/results and exact final-source hosted/review evidence are
recorded below as qualification completes; no pending gate is a pass.
