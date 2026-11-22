# RUST-IR-020 — Safe Rust IR Buffer registration

## Coverage and lifecycle

`ChannelView::{register_buffer, unregister_buffer, disable}` expose the existing
synchronous C bridge operations for **C2, HealthAndStatus, StackedImage and
control-only Image**. The shared `ChannelOperationError` and the existing owner
types are identical in `ams-mel-ir` and `ams-mel`. No raw handle escapes. The
structured IR MEL inventory tracks native and safe Ada evidence; neither of
those surfaces, nor the sys ABI or Python API, changes here.

Register while attached and disabled, before Enable/Start or after a successful
Disable. Use a Buffer created by the **same native Session**. Unregister while
disabled through any live view of the **same channel**. A view is still weak;
the native registration retains the required graph without transferring the
unique, non-Clone, non-Send, non-Sync Rust Buffer owner. Rust neither mirrors the
association nor holds a mutex across provider entry.

Disable is non-terminal. An eligible already-disabled channel is an OK no-op
with no provider Return. A provider refusal or exception leaves lifecycle
failure sticky and consumes the Disable attempt; Close does not repeat it.
Image Stop is terminal and cannot substitute for Disable. For control-only
Image, `ImageStream::try_close(&mut self)` keeps the Rust wrapper available
when native Close retains its handle. The existing consuming `close(self)`
keeps its behavior. Close can report a prior Disable failure even after detach
succeeds; a returned error alone does not establish whether a handle remains.

If register or unregister fails **after provider entry**, keep both owners.
The association may be uncertain even when the provider did not retain an
extra shared owner. Only explicit same-channel unregister while disabled can
reconcile that attempt. Even successful unregister can leave extra provider
ownership, blocking property, flag and byte access, and Close. No operation
implicitly enables, disables, stops, closes, unregisters, retries registration,
checks out or releases a Buffer. Existing Drop is best-effort ordinary Close,
not an unregister or proof of cleanup. Unresolved native graphs can remain
retained for the life of the process. Neither Disable nor unregister is proof
of callback quiescence.

`ChannelOperationError` carries the existing `Error`, the exact native status
when a call occurred, and the independent numeric provider Return. Unknown
Return values are preserved; `UINT32_MAX` maps to `None`. Local closed-wrapper
errors have no native status. A malformed diagnostic can change the Rust error
category without erasing either numeric field. A native failure remains an error
even if the provider Return was Success.

## Example

After `make test-native`, run from the repository root:

```sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
cargo run --manifest-path rust/Cargo.toml --locked -p ams-mel-ir \
  --example buffer_registration -- "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

The example uses only safe APIs: open a control-only Image and host Buffer,
write bytes and flags, register, Start, Disable, unregister, verify the bytes,
then close Buffer, Image, view and Session. It does not receive a frame.

## Qualification and limits

The compiled Rust tests use process-isolated mock controls and lifetime logs.
The reentry hooks are synchronous and operate on independent safe wrappers;
no sleeps or overlapping Rust mutable borrows establish the ordering. The C
suite continues to cover native-only unsupported families (Track, Scheduling
and Instrumentation) and cross-thread lifecycle interleavings; safe Rust does
not expose those families' typed owners. Rust tests exercise unsupported Mono8
acquisition views directly. Private Rust tests cover malformed diagnostics and
raw numeric fidelity that the mock need not manufacture.

Mock-provider evidence is not hardware or private-provider qualification. This
change does not add generic image acquisition, StackedImage acquisition, frame
leases, checkout/release, GPU/RDMA allocation or full image acquisition parity.

Final exact-source qualification and hosted run identities are recorded in the
delivery PR to avoid self-referential commit identifiers here.
