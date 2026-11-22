# RUST-IR-021 — Safe opaque Image checkout and release

## Coverage and ownership

`ImageStream::open_checkout_receiver`, `ImageCheckoutReceiver` and
`ImageCheckoutLease` bind the existing five checkout operations. `ams-mel-ir`
and `ams-mel` export identical concrete owners, counters, and release errors.
This is control-only IR Image callback/release parity, not full Image API
parity. The native ABI, provider, Mono8 Frame path, and Ada/Python coverage
inventories are unchanged. No pixels, layout, address, or header are exposed.

Install once, while Attached and before the first Start. The native bounded,
preallocated FIFO uses `queue_capacity` for queued callbacks and
`max_checkouts` for queued plus leased HOLD owners. It drops incoming
checkouts on overflow, safely releasing or retaining those callback wrappers.
There is no Rust queue, callback trampoline, worker, or completion protocol.
Receive is serialized by the exclusive borrow; zero polls, and a timeout
does not cancel or change registration. A receiver can outlive its public
Image and Session, but that does not prevent logical Stop. The six counters
are one cumulative, saturating native snapshot.

A lease owns the exact delivered callback Buffer, not the application's
registered `HostBuffer`. Release does not retire the obligation until native
reconciliation; unregister is refused while obligations remain. Receiver
Close drains queued wrappers without disabling, unregistering, or touching
independently held leases. Public Image Close itself refuses while a Buffer
remains registered; use `try_close` to keep the Image owner for cleanup after
that refusal. The Rust owners are not `Send` or `Sync`, and the lease is not
`Clone`. Native concurrency evidence remains in the existing C/C++ tests.

Prefer explicit lease Close. `CheckoutReleaseError` preserves the diagnostic,
exact bridge status, and separate numeric provider Return (`UINT32_MAX` means
none). A provider refusal or exception consumes the public lease and retains
the uncertain native graph without retry. Drop attempts only valid outstanding
ownership and does not allocate diagnostics. The binding does not adopt an
owner from a malformed acquisition result. A null Close slot means consumed
even when diagnostics are malformed; the same handle is kept only for a
contractual pre-admission refusal. Contradictory results give no safe handle
to call again, even if that means permanently retaining a native obligation.

## Example

After `make test-native`, from the repository root:

```sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" \
cargo run --manifest-path rust/Cargo.toml --locked -p ams-mel-ir \
  --example opaque_image_checkout -- "$PWD/native/build-tests/test-providers/libmock_ir_provider.so"
```

The example uses only public safe operations. The mock invokes genuine
`onImage` during KeepAlive. This is a deterministic mock trigger, not expected
behavior from a real camera. The example reported one accepted callback, then
explicitly released its lease and unregistered the original Buffer after
verifying that unregister was blocked while the lease was held.

## Qualification and limits

The focused Rust unit and compiled-mock integration commands passed during
development (`cargo test --locked -p ams-mel-ir --lib ir_image_checkout` and
`cargo test --locked -p ams-mel-ir --test image_checkout`, with the test facade
and provider paths). The mock's ordered wrapper-ID and DSO logs distinguish
FIFO delivery, exact release, reentrant/deferred release, and permanent uncertain
retention. The existing mock checkout wrapper implements Success, Fail, and
throw, not NotSupported. The numeric NotSupported case is tested at the private
binding seam alongside inconsistent handles, malformed diagnostics, and unknown
numeric values; it is not claimed as a provider-originated integration result.

Final exact-source commands, outcomes, and hosted workflow identities are kept
in the delivery PR to avoid self-referential commit identifiers. No real camera,
private provider, GPU/RDMA, or StackedImage acquisition was qualified.