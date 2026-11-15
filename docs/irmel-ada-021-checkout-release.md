# IRMEL-ADA-021 — opaque application checkout and release

## Preparation and scope

Freshly fetched main: `604ef38f8f81a9c71add60c5ca2bac4db218ea7b`.
Branch: `feature/irmel-ada-021-checkout-release`, directly from that main.
Open PRs #220 and #221 were inspected before editing; they concern unrelated
Rust crate/documentation work and are not used as branch bases.

This implements an **opaque checkout/release notification**, only for the
existing listener on `Open_Image_Control` IRSTImage attachments. Zero width,
height, bit depth, buffer size and image size remain valid. Neither callback
Buffer getters nor FrameHeader fields nor payload bytes are read. No additional
listener, internal image Buffer pool, `getBuffer` or `init` occurs on delivery.
The callback wrapper need not equal the registered descriptor.

Excluded: Mono8 acquisition changes, C2/Health/Stacked callbacks, generic raster
access, unrestricted Host_Buffer release, decoding, DSP, GPU/RDMA allocation,
safe Python/Rust checkout APIs, and generic full acquisition parity. Existing
Mono8 Frame_Lease tests and implementation remain on the existing path.

## Ownership vocabulary

- **Registered application Buffer**: initialized descriptor plus fixed host
  storage, retained by the existing application-registration owner. It is not
  the callback wrapper and must never receive an invented release call.
- **Delivered callback Buffer**: exact shared owner supplied by a real upstream
  `ImageListener::onImage`. The listener transfers this owner, not an address or
  reconstructed descriptor.
- **Queued checkout**: delivered wrapper and its HOLD-slot identity in a fixed
  ring FIFO, contributing one outstanding `retained_frames` obligation.
- **Ada-owned Checkout_Lease**: limited owner obtained by FIFO transfer; the
  outstanding obligation does **not** decrease on Receive. No address, image
  span, provider size, header or format can be obtained from the lease.
- **Successfully released checkout**: the release executor received Success,
  destroyed its last exact callback-wrapper reference outside bridge locks,
  then retired the obligation under the lifecycle mutex.
- **Uncertain release**: failure, exception or inability to reserve safe release
  ownership. Public lease is consumed, exact wrapper is retained, registration
  and storage cleanup stay blocked, provider graph is retained permanently.
  Finalization does not retry. There is no speculative recovery API.

## Receiver policy and synchronization

Installation is one-shot, Attached, control-only, before the first Start. It
may precede Buffer registration. A closed receiver cannot be reopened. Private
allocation prepares the entire ring and ownership bookkeeping before publication;
failed creation leaves the output null and installation unconsumed.

Queue_Capacity must be positive and at most Max_Checkouts. Max_Checkouts bounds
queued/live **HOLD** owners, not overlapping wrapper generations whose release
has begun. Each HOLD/deferred/uncertain pool has Max_Checkouts + 2 entries; the
two extra HOLD entries are withheld from acceptance and serve rejection/reentry.
Admission examines free HOLD capacity, not retained_frames: an active successful
release still contributes to retained_frames until wrapper destruction, but may
already have made a physical Buffer reusable. The one-checkout reentry test
exercises this distinction. Queue admission itself allocates nothing.

No active receiver, inactive lifecycle, in-flight registration/unregistration,
absent/uncertain registration, null delivery or capacity exhaustion makes a
callback valid. Non-null rejected wrappers use the same release engine as
Mono8. A callback during enable is rejected while Starting; a callback after
Disable is rejected, not fabricated as a valid checkout. No new upstream
quiescence claim is made.

All admission, FIFO, counters, unregister gates and release accounting use
CallbackState::mutex. Provider methods run unlocked. The existing release
executor serializes release, handles synchronous reentrant callbacks through
preallocated deferred slots, and reserves uncertain ownership **before** calling
the provider. The executor permit participates in unregister reconciliation,
including the post-result wrapper-destruction interval.

The existing raw C handle lifetime contract requires external admission and
destruction serialization. One Receive is permitted at a time. C Close may
overlap an already-entered Receive after that call has captured its independent
graph, but applications must prevent new handle uses once Close begins. The
deterministic test waits for a native entered marker before Close. The safe Ada
API deliberately requires external serialization of receiver operations/Close;
it does not advertise an Ada object-level concurrent destruction protocol.
Independent leases and Disable can operate concurrently.

Receiver Close atomically stops queue admission and wakes waiters, then drains
each queued wrapper unlocked through the existing engine. Already acquired
leases survive. It neither disables nor unregisters nor detaches. Drain failure
is reported even though the public receiver is consumed. Late callbacks still
have a deterministic rejection/release or conservative-retention outcome.

## Bounded emergency limitation

Non-conforming delivery beyond per-stream bookkeeping uses the existing
process-wide **64-owner pre-release reserve**. No allocation is attempted there.
Control-only reserve exhaustion is explicitly **fail-stop (`abort`)**, never a
drop of the only unreturned opaque wrapper. There is no finite-memory promise
for infinitely many hostile provider deliveries. Reserved owners intentionally
have no static destructor, so process shutdown cannot destroy an unreleased
wrapper after provider teardown. Ordinary uncertain releases remain in the
stream's preallocated uncertain slots, not this overflow reserve.

## ABI and coverage delta

Five additive `AMS_MEL_0.1` exports, **326 → 331**:

- `ams_mel_ir_image_checkout_open`
- `ams_mel_ir_image_checkout_receive`
- `ams_mel_ir_image_checkout_get_counters`
- `ams_mel_ir_image_checkout_close`
- `ams_mel_ir_image_checkout_lease_close`

Opaque receiver and lease handles; new frozen C11-compatible six-uint64 counters
record. No existing layout or ABI version changes. Lease Close reports actual
upstream Return independently; `UINT32_MAX` means none obtained, including an
exception, no-op Close or refused release admission. Creation outputs must be
null and remain unchanged on failure. Zero timeout polls; an ordinary timeout
neither consumes another checkout nor cancels the receiver.

Private Python ctypes and raw Rust sys mirror all five functions and counters;
their exact ABI inventories and strict compiled-C layout/signature probes are
updated. No safe Python/Rust API is added.

Authoritative reviewed coverage broadens `Buffer::release` and
`ImageListener::onImage` evidence; both remain **partial**. Unique safe-Ada
operations remain **57**. Mono8 capability classification is unchanged.
Historical `docs/coverage.md` is untouched.

## Qualification evidence

Initial implementation runs (superseded when later sources change):

- `make test-native`: 820/820 passed; existing Mono8/release stress tests included.
- `make test-python`: 665/665 passed, including raw checkout signature/layout.
- `make test-rust`: 605 passed, 15 existing opt-in ignored, including doctests.
- `make check-ir-api-coverage`: 70 controls passed; safe-Ada count 57.
- `make format-ada`, `make check-ada-format`, `make check-ada-docs-changed`:
  passed; nine public declarations, zero annotation gaps.
- Rust workspace formatting, all-target Clippy `-D warnings`, rustdoc
  `-D warnings`: passed.
- GCC/Clang strict C11 probe: `48 8 0 8 16 24 32 40` on LP64.
- `make test-policy`: passed, including source integrity and documentation tests.

Final-source full-suite/matrix, pinned Alire, documentation generation and
delivery results are recorded below as they complete. Incomplete runs are not
passing evidence.

Deterministic C tests use ordered wrapper-ID logs and explicit barriers. The
mock aborts on unauthorized getters, duplicate release, or unreleased wrapper
destruction. Tests exercise genuine distinct wrappers, no layout, FIFO/drain,
held leases after receiver/Session close, timeout, overflow/excess delivery,
creation/receive/admission allocation failures, registration/unregistration,
enable/disable/release/destruction reentry, in-progress release versus Disable
and unregister, failed/throwing release permanent retention, and no early DSO
unload. Native checks remain active under NDEBUG. Ada tests use only public safe
APIs and verify timeout, genuine delivery, counters, disabled-but-held unregister
refusal, exactly-once Close and failed/throwing release consumption. The public
`ada/tests/src/ams_mel_ir_checkout_example.adb` never reads image bytes; the mock
uses an actual callback during KeepAlive to make delivery deterministic, not a
claim that real providers deliver images for KeepAlive.

Positive reference-provider/hardware integration: **UNTESTED**. The reference
minimal example does not produce image callbacks and cannot qualify release.
No positive hardware or zero-copy acquisition claim is made.
