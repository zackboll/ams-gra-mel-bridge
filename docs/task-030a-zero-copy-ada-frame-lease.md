# Task 030A: zero-copy Ada frame lease

This task establishes the repository's reusable **native-owner + borrowed
language view** model for high-bandwidth MEL data and proves it on the existing
IR Image path. It removes the native-snapshot-to-Ada pixel copy. It does
**not** remove the MEL-provider-buffer-to-native-queue copy, and it therefore
does **not** claim end-to-end zero-copy.

## What changed and what did not

```text
After Task 030A                       Future Task 030B
-------------------------------       -------------------------------
MEL provider Buffer                   MEL provider Buffer
        |                                     |
        | COPY remains                        | retained owner
        v                                     v
native snapshot storage               native lease
        |                                     |
        | ZERO COPY   <-- this task           | ZERO COPY
        v                                     v
Ada borrowed view                     Ada borrowed view
```

Payload copies on the high-rate lease path:

```text
provider -> native snapshot storage    1 copy   (unchanged, Task 030B)
native snapshot -> Ada                 0 copies (removed here)
```

Task 030B, provider-buffer retention and `irmel::Buffer::release()` timing, is
explicitly out of scope, as are RF MEL, RF header vendoring, RDMA, GPU/CUDA
memory, FPGA mapping, Stacked Image, `cv::Mat` wrapping, zero-copy safe Rust,
and zero-copy public Python.

## Ownership model

The conceptual model, intended for reuse by future high-data-rate interfaces:

```text
native backing owner
        |
        v
opaque C handle
        |
        v
limited Ada owner
        |
        v
temporary borrowed Ada view
```

The IR-specific instantiation is:

```text
QueuedFrame storage owned by ams_mel_ir_frame_snapshot   (native backing owner)
        -> ams_mel_ir_frame_snapshot *                   (opaque C handle)
            -> AMS.MEL.IR.Image.Frame_Lease              (limited Ada owner)
                -> AMS.MEL.IR.Pixel_Array view           (temporary borrow)
```

`Frame_Lease` is the domain-specific name; the pattern above is the intended
bulk-data model, not an IR-only memory model.

## Native snapshot lifetime

`ams_mel_ir_stream_receive_snapshot` moves one `QueuedFrame` out of the bounded
receive queue into a heap-allocated `ams_mel_ir_frame_snapshot` and calls
`bind()`, which points `view.pixels` at the snapshot's own
`std::vector<std::uint8_t>`. The header already documents the returned owner as
"independent of stream, Session, and provider lifetime until snapshot_close",
and `native/tests/test_ir_stream.c` asserts that behavior directly rather than
relying on it accidentally: `test_snapshot_pixel_storage_identity` closes the
stream, closes the Session, and unloads the provider while holding a live
snapshot, then still reads the first and last payload bytes at the original
address.

Because the snapshot owns its storage outright, no additional native owner had
to be retained, and no native library or provider code can unload while a live
Ada lease still needs it: the lease references only snapshot-owned heap memory,
not provider code or provider objects.

## Ada lease lifetime

`Frame_Lease` is `limited private`, wrapping a
`Ada.Finalization.Limited_Controlled` owner that holds the opaque handle.

- One live lease owns exactly one native snapshot.
- The owner is limited, so it cannot be copied.
- `Finalize` closes the native snapshot exactly once and clears the borrowed
  address and size.
- `Close` is idempotent; the native close nulls the handle, and closing a null
  handle succeeds.
- Finalization after an explicit `Close` is harmless.
- Failed acquisition leaves the handle null, leaks nothing, and makes a
  subsequent finalization inert.
- A metadata-conversion exception during acquisition closes the snapshot before
  propagating, so it cannot leak.
- Receiving later frames does not invalidate an earlier live lease.
- Multiple outstanding leases are permitted, and each owns distinct storage.
- The lease stays valid after the frame has left the receive queue, and through
  stream Stop, stream Close, Session close, and provider teardown.
- No Ada application callback runs from a MEL/provider callback thread;
  `With_Pixels` runs only on the application thread that owns the lease.

## Borrowed-view rules

```text
Frame_Lease owns native snapshot
        |
        +--> temporary Pixel_Array view
                 |
                 +--> valid only during borrow callback
```

> The pixel array aliases storage owned by `Frame_Lease` and must not be
> retained beyond the dynamic extent of `With_Pixels`.

The view is an anonymous constrained `AMS.MEL.IR.Pixel_Array` object declared
in a nested block and passed to the callback, so the callback parameter cannot
legally be saved in a way that outlives the borrow through the safe API.
Pixel storage is read-only through the safe API: the view is a `constant`, and
`Process` receives an `in` parameter.

`With_Pixels` fails closed rather than performing unchecked memory access when

- the lease is closed;
- the native span has a null pointer and a nonzero size;
- the native span size cannot be represented as an Ada index (rejected at
  acquisition).

A zero-sized payload is valid and yields an empty view.

## How the array view is built without copying

`AMS.MEL.IR.Image.Borrow_Pixels` declares

```ada
View : constant AMS.MEL.IR.Pixel_Array (1 .. Size)
with Import, Convention => C, Address => Payload;
```

`Payload` is the snapshot's own `pixels.data`. `Import` suppresses any default
initialization, and an address clause binds the object to existing storage, so
GNAT emits no allocation, no `memcpy`, and no elementwise loop. Construction is
O(1) with respect to pixel count. Address conversion is confined to the
ordinary-Ada private implementation; the public API exposes no
`System.Address` and no pointer arithmetic.

## Deterministic zero-copy evidence

Structural, not timing-based:

1. **Native storage identity** (`native/tests/test_ir_stream.c`,
   `test_snapshot_pixel_storage_identity`): the view's `pixels.data` is stable
   across repeated `view` calls, is distinct between two live snapshots, is
   unaffected by closing a sibling snapshot, and survives stream/Session/
   provider teardown.
2. **Ada/native alias proof**
   (`ada/tests/src/ams_mel_ir_image_lease_tests.adb`, `Test_Zero_Copy_Alias`):
   a test-only facade hook writes `"<frame_id> <pixel-data-address> <size>"`
   when `AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG` names a log. The Ada test compares
   that value with `Pixels (Pixels'First)'Address` inside the borrow callback
   and requires exact equality:

   ```text
   Ada borrowed pixel first address == native snapshot pixel data address
   ```

   This cannot hold if any intermediate Ada copy exists. The hook publishes no
   new export or ABI type, and the normal safe Ada API still exposes no raw
   address.
3. **No view is built at acquisition**: the observation variables stay at their
   sentinel values across `Acquire_Frame` and only change inside `With_Pixels`,
   so acquisition constructs no payload-sized object.
4. **Repeated borrow aliases the same storage**, so `With_Pixels` is not
   silently re-materializing anything.
5. **`Copy_Pixels` returns a different address**, confirming the borrowed and
   owned paths are genuinely distinct.

## Performance invariants

```text
Acquire_Frame                        O(1) w.r.t. pixel count after dequeue
With_Pixels setup                    O(1) w.r.t. pixel count
payload-sized Ada allocation         none
native snapshot -> Ada payload copy  none
per-pixel FFI calls                  none
per-pixel Ada Append loop            none
Ada.Containers pixel Vector          none on the lease path
```

`Acquire_Frame` copies only small metadata: strings, image flags, sensor
inertial states, navigation states, and scalars. That is deliberate; the
invariant this task defends is that the potentially large pixel payload is not
copied from the native snapshot into Ada on the lease path. Lease metadata
accessors read Ada-owned fields and stay valid for the whole lease lifetime.

## Public Ada API added

```ada
type Frame_Lease is limited private;

function Acquire_Frame
  (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0)
   return Frame_Lease;
function Is_Open (Frame : Frame_Lease) return Boolean;
procedure Close (Frame : in out Frame_Lease);

function Pixel_Count (Frame : Frame_Lease) return Natural;
procedure With_Pixels
  (Frame   : Frame_Lease;
   Process : not null access procedure (Pixels : AMS.MEL.IR.Pixel_Array));
function Copy_Pixels (Frame : Frame_Lease) return AMS.MEL.IR.Pixel_Array;
```

plus the `Frame_Lease` metadata accessors, which mirror the existing
`Full_Frame` accessor names.

Naming rule:

```text
Receive / Pixels             owned data / copy allowed
Acquire_Frame / With_Pixels  borrowed high-rate path / no payload copy
Copy_*                       explicit owned copy
```

No function named simply `Pixels` was added to `Frame_Lease`.

## Compatibility

`AMS.MEL.IR.Receive` and the owned `Frame` record are unchanged and remain the
copying compatibility path, useful for simple applications, testing, code that
needs data to outlive the stream or snapshot, and SPARK-oriented logic that
prefers Ada-owned data over native borrowing.

`AMS.MEL.IR.Image.Receive`, `Full_Frame`, and `Pixels (Full_Frame)` keep their
existing owned-copy behavior and signatures. `Receive` was refactored to share
one metadata conversion with the lease path and now performs its payload copy
explicitly through the same borrow primitive, with `Reserve_Capacity` ahead of
the append loop; the observable contract is identical. Existing applications
acquire no new native lifetime dependency by this task, because they do not use
the new API.

## Concurrency

The existing restriction stands: at most one task may perform acquisition or
receive on a given `Image_Stream` at a time. Once a lease has left the queue it
is an independent owner, so borrowing frame A does not block reception or
processing of frame B, and no lock is taken around the pixel-processing
callback.

## C ABI

No C ABI change was required. No export was added or removed, no public type
changed, `exports.map` is unchanged, and the ABI version stays `0.1`. The
existing `ams_mel_ir_stream_receive_snapshot`, `ams_mel_ir_frame_snapshot_view`,
and `ams_mel_ir_frame_snapshot_close` already provide exactly the
opaque-handle-plus-borrowed-span shape `docs/c-abi-policy.md` requires: an
owning object with explicit lifetime, a borrowed span whose owner is named, and
documented pointer validity. No frozen structure was grown in place. The only
native change is test-facade-only code inside
`#ifdef AMS_MEL_ENABLE_TEST_FAILPOINTS`, which is compiled out of the
production facade.

## Why this precedes RF MEL

RF Receive products, RF waveform streaming, and Stacked Image are all
higher-rate than IR Images, and several plausible RF buffer sources are not
ordinary CPU-copyable host memory. Fixing the ownership and borrowing contract
on the already-working IR path, with deterministic alias evidence, means the RF
work inherits a proven lifetime model instead of inventing a second one under
schedule pressure. See `docs/architecture.md` for the intended reuse and for
the explicit constraint that future bulk buffers must not be assumed to be
ordinary Ada-addressable memory.
