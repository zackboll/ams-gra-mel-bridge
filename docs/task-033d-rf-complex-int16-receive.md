# Task 033D — RF ComplexINT16 ProductRxEndpoint receive

Task 033D implements the first RF MEL data-plane path in native C:

```text
DataMEL --RequestFor<ProductRxEndpoint>--> async create request
    --> ProductRxEndpoint --provider callback--> ComplexINT16 + ProductRxMetadata
    --immediate owned copy--> bounded bridge queue --> immutable C event owner
```

Only ComplexINT16 is supported. The task adds no RDMA,
`getRDMAMemoryRegionParams`, `registerExternalRxEndpoint`, other
`JobDataFormat`, AMS VITA, PDW, LF, PointingType/ReceiveEvent mapping,
`std::any` serialization, RF C2, VirtualAperture, Jobs, VADB, or safe
Ada/Rust/Python RF.

## ABI

ABI 0.1 grows from 106 to **115** production exports. Relative to the starting
`main` (`57d5b13`), the delta is exactly these nine functions:

```text
ams_mel_rf_data_submit_product_rx      ams_mel_rf_product_rx_receive
ams_mel_rf_product_rx_request_wait     ams_mel_rf_product_rx_get_counters
ams_mel_rf_product_rx_request_claim    ams_mel_rf_product_rx_close
ams_mel_rf_product_rx_request_close    ams_mel_rf_product_rx_event_view
                                       ams_mel_rf_product_rx_event_close
```

New opaque owners: `ams_mel_rf_product_rx_request`, `ams_mel_rf_product_rx`,
`ams_mel_rf_product_rx_event`. New records: `ams_mel_rf_product_rx_config_v1`,
`ams_mel_rf_complex_i16_v1`, `ams_mel_rf_complex_i16_span_v1`,
`ams_mel_rf_product_rx_metadata_v1`, `ams_mel_rf_product_rx_event_v1`,
`ams_mel_rf_product_rx_info_v1`, `ams_mel_rf_product_rx_request_result_v1`,
`ams_mel_rf_product_rx_counters_v1`. The rules are in `c-abi-policy.md`.

## Source organization

* `native/src/rf_product_rx.cpp` holds the whole receive lifecycle.
* `native/src/internal/rf_product_rx.hpp` holds the shared `RfDataState`, the
  move-only `RfChildClaim`, and `rf_finish_shutdown`.
* `native/src/rf_data.cpp` keeps the 033B foundation; only the parent
  lifecycle is extended.

No RF code is in the IR sources.

## Request worker ownership model

* **Submit validates first.** The format, nonzero capacity and max samples,
  and `region_size_bytes <= SIZE_MAX` are checked before any provider call.
* **Allocate before the provider call.** Submit then allocates the
  completion, the worker input, the public owner, and the thread object, and
  acquires one `RfChildClaim` (parent child count +1). Only then does it call
  `createProductRxEndpoint(ComplexINT16, size, nullptr)`, outside every bridge
  mutex.
* **Synchronous failures publish nothing.** A synchronous provider throw or an
  invalid future (`valid() == false`) releases the claim, and no request is
  published. No worker is launched around an invalid future.
* **One worker per future.** Exactly one detached worker per future is the
  sole `future.get()` caller, and it caches the terminal outcome exactly once.
  If the worker cannot launch around a valid future, the input (future,
  completion, and claim) is retained forever and no request is published.
* **On success, the worker validates and waits.** It requires a non-null
  endpoint and reads `getEndpointID()` and `getAssignedDataFormat()` once
  each. The format must be ComplexINT16; a mismatch is
  `AMS_MEL_PROVIDER_FAILED`, and the endpoint is destroyed on the worker. The
  worker then caches the endpoint and info, publishes success, and waits for
  Claim or request Close. **It never registers the callback.**
* **On failure, the claim goes first.** The worker releases the claim
  **before** publishing the failure, so any caller that observes the failure
  knows the child is already gone.
* **ErrorOr mapping.** A known MEL `ErrorCode` goes to `error_code`, and the
  provider description becomes the diagnostic (with UTF-8-safe truncation). An
  unknown code is `AMS_MEL_PROVIDER_FAILED` with "malformed provider ProductRx
  creation result: unknown MEL ErrorCode N". `AMS_MEL_COMMAND_REJECTED` is
  never used.

## Claim, request Close, non-cancellation

* **Claim never blocks.** It returns `AMS_MEL_TIMEOUT` while the create is
  pending, and the cached status if the create failed.
* **The first claim registers the callback.** It first builds the callback
  state and the heap-stable `PermanentRxRegistration`, marks the request
  claimed, and moves the endpoint and the same child claim into the public
  owner. Then, outside every mutex, it calls
  `setDataReadyCallback(registration->callback)`.
* **Later claims fail.** A second claim returns `AMS_MEL_PROVIDER_FAILED` with
  "RF ProductRx endpoint already claimed".
* **A throwing registration fails safe.** `*out_endpoint` stays NULL. The
  bridge retains the registration permanently, closes the callback state,
  destroys the endpoint, caches the failure, and releases the claim. A repeated
  Claim returns the same status and text, and registration is never attempted
  twice.
* **Request Close only marks abandonment.** It is idempotent and nonblocking,
  and it is not cancellation.
  * A pending future stays with its worker. In the never-ready test, `wait(0)`
    times out, Close returns immediately, parent Close defers, and the DataMEL
    is neither shut down nor unloaded.
  * An unclaimed successful endpoint is destroyed by the **worker**, without
    calling `setDataReadyCallback`, and then the claim is released. If that
    was the last child of a closed parent, the DataMEL is shut down and
    destroyed and the DSO unloads normally, because no registration ever
    happened.

## Parent-first DataMEL lifetime

`RfDataState` gains `mutex`, `children`, `close_requested`, and
`shutdown_started`. `ams_mel_rf_data_close` always consumes the owner.

* **No children.** Close follows exactly the Task 033B sequence: shutdown,
  destroy the DataMEL, drop the library.
* **Live children.** Close records the request and returns `AMS_MEL_OK`. The
  release of the final child then performs `shutdown()` exactly once, guarded
  by `shutdown_started`.
* **Throwing deferred shutdown.** The complete `RfDataState` graph is retained
  permanently, with no retry. On a public Close this is reported as
  `AMS_MEL_PROVIDER_EXCEPTION`; on a worker it is retained silently.

Requests submitted before parent Close remain fully usable: wait, claim, and
receive all still work.

`RfDataState::library` is now `shared_ptr<SharedLibrary>`, and the DataMEL is
still destroyed before `RfDataState`'s reference drops. With no callback
registration, that reference is the only one. So `no-callback-unload` and all 17
`rf-data-*` cases show unchanged 033B behavior: the DataMEL is destroyed and
the library is unloaded. `SharedLibrary` remains the only loader abstraction.

## Permanent registration holder and the exact callback lvalue

`setDataReadyCallback(DataReadyCallback&)` takes a non-const lvalue reference.
Upstream does not promise that a provider copies it: it may copy it, move from
it, or keep a pointer to the exact object. The bridge therefore registers a
member of a heap-stable holder:

```cpp
struct PermanentRxRegistration {
    std::shared_ptr<SharedLibrary> library;          // provider DSO
    std::shared_ptr<RfRxCallbackState> callback_state;
    DataReadyCallback callback;                      // the EXACT object passed
    PermanentRxRegistration *next;                   // allocation-free root
};
```

* **Retained forever.** Once registration begins, whether it returns or
  throws, the holder is linked into an intrusive process-lifetime root and is
  never freed. The address of `callback` never changes.
* **Move-safe.** The holder owns the callback state and the library
  separately, so a provider that moves from the callback stays safe (the
  `late-move` case).
* **Retains nothing else.** The holder keeps no DataMEL, endpoint, public
  owner, or event. Endpoint Close swaps out the queue, so no payload is kept
  either.

This corrects the 033C design wording ("retains only the library"). Because
upstream does not promise that the provider copies the callback, the exact
callback lvalue and its callback state are also retained for the process
lifetime. This is deliberate fail-safe registration retention. It is distinct
from the full `RfDataState` emergency retention, which happens only after a
throwing `shutdown()`.

## Callback path

1. **Lifecycle check first.** Lock, then increment `callbacks_received` and
   `in_flight`. If the endpoint is Closed, increment `callbacks_after_close`,
   decrement `in_flight`, notify, and return. This all happens **before** any
   metadata dereference, variant payload access, sample read, or allocation.
2. **Validate, outside the lock.**
   * `metadata` is non-NULL.
   * The `JobDataPointer` alternative is `MELComplex<int16_t>*`.
   * The pointer is non-null when `count > 0`.
   * `count <= max_samples_per_event` and `count <= vector::max_size`.
   * The fail-closed metadata checks pass.

   `count == 0` with a NULL pointer is valid and produces an empty event.
3. **Copy.** Samples are copied element-wise
   (`real = source[i].real(); imag = source[i].imag();`). `rxStreamIDs` go into
   an owned vector, and metadata values are copied verbatim.
4. **Publish.** Lock and re-check the lifecycle. If Closed, discard the event
   and increment `callbacks_after_close`. Otherwise increment exactly one of
   `malformed_or_unsupported`, `allocation_failures`, or
   `products_dropped_queue_full` (DROP-INCOMING), or push the event and
   increment `products_queued`. Then decrement `in_flight`.

No exception escapes the callback. `bad_alloc` counts as an allocation
failure, and any other exception counts as malformed. Counters saturate at
`UINT64_MAX` (the `saturation` case).

Endpoint Close runs in four steps:

1. Logical close: set Closed, swap out the queue, and wake Receive.
2. Drop the provider endpoint outside every mutex.
3. Wait until the current `in_flight == 0`.
4. Release the child claim, which may run the deferred shutdown.

The drain in step 3 is not provider quiescence and never authorizes a DSO
unload.

## Mock provider implementation closure delta

This was measured before vendoring anything. A concrete mock
`ProductRxEndpoint` must define the pure virtual `getRDMAMemoryRegionParams()`,
whose return type the consumer closure only forward-declares
(`RFMELTypes.h:183`). The measurement compiled a minimal concrete endpoint with
GCC and Clang 19, against the vendored tree and against the full pinned RF MEL
tree (`762ce84c…`, tree `f5b9d4a8…`). The dependency delta is exactly:

```text
+ rfmel/endpoints/RDMAMemoryRegionParams.h   (includes only RFMELTypes.h)
```

The RF header sets are identical for both compilers and both roots, and the
Common MEL, AMS Math, AMS VITA, and Boost sets are unchanged.

* **Vendored byte-identically.** Git blob `5e15b157…`, sha256 `ca775ff0…`.
  The checksum inventory grows from 627 to **628**, and all 628 verify.
* **Test-provider support only.** Production never includes the header.
  `check_rf_product_rx_header_closure` still pins exactly 25 RF headers / 539
  union with an unchanged expected set, and `check_rf_data_header_closure` is
  also unchanged.
* **Forbidden in production.** The mock's `getRDMAMemoryRegionParams()` counts
  calls and records `rf_forbidden_call`, and every contract case asserts zero.

## Deterministic evidence (mock provider, 30 `rf-product-rx-*` cases)

Rich metadata used by `receive`:

* IDs: `0xFEDCBA98`, `0x80000001`, `0x7FFFFFFE`, `0xDEADBEEF`, `0x00010002`,
  `0xFFFFFFFF`, `0x12345678`.
* Phase coherence 1; UTCTime `-4102444801 s` + `987654321098765 fs`.
* Stream IDs `{0xFFFFFFFF, 0, 0x80000000, 42}`.
* Samples `10+20i`, `30+40i`, `-5+6i`, `INT16_MIN+INT16_MAX i`,
  `INT16_MAX+INT16_MIN i`, `-1+0i`.

| Case | Evidence |
|---|---|
| `receive` | The mock overwrites its sample buffer and metadata right after the callback returns, then reuses the buffer for a later product. Event A is unchanged, including after endpoint and DataMEL Close, and the DSO stays mapped. |
| `fail-closed` | userDefinedData, stabPoints, receiveEvents, and nonempty associations each add 1 to `malformed_or_unsupported` and queue nothing. Empty associations and the sparse default profile are accepted. |
| `malformed` | NULL metadata, wrong variant, NULL pointer with count > 0, and count > max are malformed. count == max is accepted. Count 0 with a NULL pointer gives an empty owned event. Allocation failure while building the event or copying stream IDs counts `allocation_failures`. |
| `queue-full` | Capacity 1: A is kept and B is dropped; `queued 1`, `dropped 1`. |
| `create-failures`, `long-diagnostic` | Known ErrorOr (`INSUFFICIENT_RESOURCES`), long UTF-8 description (607 required, UTF-8-safe truncation), unknown code 77, null endpoint, future exception, getter exception, format mismatch (no registration), invalid future, synchronous throw. Repeated Wait and Claim return an identical cached status and text. |
| `invalid` | Every non-ComplexINT16 format, zero capacity or max, and bad pointers are rejected **before** any provider call. `region_size_bytes = 0` is passed verbatim. |
| `claim-unique` | A pending claim returns TIMEOUT without blocking. The first claim succeeds; the second returns "already claimed". |
| `parent-first`, `pending-parent-first` | Shutdown happens only at endpoint Close, exactly once. Receive works after parent Close, including when the future is released after parent Close. |
| `unclaimed`, `unclaimed-parent-first`, `never-ready` | The worker destroys the endpoint with zero registrations, and the DSO unloads. A never-ready future stays retained, with no shutdown and no unload. |
| `sync-callback` | A callback delivered inside `setDataReadyCallback` is queued and received right after Claim. |
| `registration-throw-before`, `-after` | Both are retained permanently, no endpoint is published, and the failure is cached. A later retained-reference callback is safe (Closed, `callbacks_after_close` +1). |
| `mid-callback-close` | A pipe holds a callback inside the bridge (`in_flight` 1). Close sets Closed and destroys the provider endpoint immediately, but has not returned (poll-bounded observation). After release, Close returns and the held callback has published nothing. |
| `late-copy`, `late-reference`, `late-move` | The defining late-start test (below). |
| `negative-no-library-pin`, `negative-no-exact-lvalue` | Destructive controls in forked children (below). |
| `no-callback-unload` | 033B regression: open, snapshot, Close destroys the DataMEL and unloads the DSO, with zero registrations. |
| `multiple-endpoints` | Two endpoints with independent IDs, queues, counters, callbacks, and Close. Closing one does not close the other or the parent. Two permanent registrations. |
| `concurrent` | Deterministic overlap (A held while B completes), plus 4 unserialized threads x 500 callbacks. Counters are exact (2000/2000) and every event is independent. |
| `receive-close` | Close wakes a blocked Receive (`STREAM_STOPPED`) and discards queued products. |
| `saturation`, `deferred-shutdown-throw`, `worker-shutdown-throw`, `worker-launch-failure` | Counters saturate. A throwing deferred shutdown on the public Close (`PROVIDER_EXCEPTION`) or on the worker retains the full graph, with no destroy and no unload. A failed worker launch publishes nothing and pins. |

### Late-start after zero in-flight (`late-copy`, `late-reference`, `late-move`)

The provider keeps the callback as a copy, as **only a pointer to the exact
lvalue**, or as the moved-from original. Its late thread waits on a test pipe.

1. Arming is the last provider function the test calls. The test then clears
   every `dlsym` pointer and `dlclose`s its own handle.
2. Endpoint Close: endpoint destruction is recorded, and the observer sees
   `in_flight == 0` and Closed.
3. Parent Close: `rf_shutdown` and `rf_data_destroyed` are recorded. The DSO
   is still mapped (`/proc/self/maps`, no `library_unloaded`).
4. Only now is the late callback released, with `metadata = NULL`, count
   1024, and a **PROT_NONE** sample page.

The callback returns normally. `callbacks_received` and `callbacks_after_close`
each increase by 1, the queue is unchanged, the DSO is still mapped, and event
A is unchanged. Any metadata or payload access before the Closed check would
fault on the trap. Mapping evidence is reference-neutral: `/proc/self/maps`
plus the mock's `library_unloaded` record. `RTLD_NOLOAD` is never used as
lifetime proof.

### Negative controls (forked, destructive, non-vacuous)

* **`negative-no-library-pin`.** The permanent registration drops its
  `SharedLibrary`. The child detects "provider DSO unmapped
  (library_unloaded) before the late callback" (exit code 10), and the parent
  sees `library_unloaded`.
* **`negative-no-exact-lvalue`.** The DSO and callback state stay retained,
  but the provider is handed a temporary copy that is destroyed after
  registration (its storage is scrubbed and never freed). The
  reference-retaining late callback finds an **empty** `std::function` (exit
  code 11). An earlier version of this control, before the scrubbing was
  added, hit the same defect as an abort with `std::bad_function_call`.

The destructive controls exist only in test builds
(`ams_mel_test_rf_rx_negative_control`).

## Real Squall receive (`make test-squall-rf-rx`)

This target is opt-in. `SQUALL_SOURCE_DIR` must name the exact pinned checkout
`b1015728…` with verified submodule pins; a registry `latest` is never used.
The 033B smoke `make test-squall-rf-c` (`run-rf.sh` with no argument) is
unchanged.

### Test-only job activation

Pinned Squall drops ProductRx data unless an RX job is active, and production
has no RF C2 or Jobs. The harness therefore builds a **test-only**
`integration/squall/squall_rf_job_helper.cpp`:

* It is compiled inside the pinned Squall builder stage, using the provider's
  own toolchain and the exact pinned RF MEL C++ headers.
* It runs AdminMEL `commandState(OperateRxOnly)`, then the C2MEL virtual
  aperture, `requestJob`, and `finalize`.
* It is loaded with `dlopen`. It is never compiled into `ams_mel_c`, and the
  script asserts that it is not linked and that none of its symbols are
  exported. It is never exposed through C.

The endpoint and every assertion go through the production facade
(`integration/squall/squall_rf_rx_c.c`, C11).

### Non-trivial IQ

Pinned `config/rf-simulated.toml` has zero floor and zero noise, and its
emitters are DIS-driven, so without DIS traffic every IQ sample is 0. The first
run passed that way, with 8 all-zero, identical events. That proved nothing
about the copy boundary, so the harness was tightened:

* The client now requires nonzero samples in event A, and requires every later
  event to differ from A.
* rx mode derives a copy of the pinned file that changes **only**
  `noise_std_dev` (to 0.05, Gaussian AWGN). Sample values are random and never
  asserted; exact I/Q fidelity is mock-proven.

### Result (passed)

```text
ProductRxEndpoint: id=1 assigned_format=3
event A: 4096 ComplexINT16 elements, 4096 nonzero, first=(223,-3441)
received 8 events; 7 later events differ from event A
counters: received=8 queued=8 dropped=0 malformed=0 alloc=0 after_close=0
PASS: Squall RF ProductRxEndpoint ComplexINT16 receive (1 iteration(s), Squall b1015728…)
```

The run verified:

* creation succeeds, the assigned format is ComplexINT16, and the endpoint ID
  is nonzero;
* 8 nonempty events, with Squall's sparse default metadata (all IDs 0, phase
  0, UTCTime 0/0, no stream IDs);
* no malformed callbacks and no queue overflow;
* event A is byte-identical to its first read after 7 later, different
  callbacks reused Squall's single per-endpoint IQ vector. This is the
  real-provider evidence for the copy boundary.
* event A survives endpoint Close and DataMEL Close.

The DSO staying mapped after registration is intentional generic bridge
policy, not a Squall leak.

## Bindings

* **Private Ada FFI (`AMS.MEL_C_API`).** Adds the three handles, seven records,
  and nine imports (115 imports total). There is no `AMS.MEL.RF`.
* **`ams-mel-sys`.** Adds the raw types and functions; the inventory is 115,
  with exact layouts and offsets checked against the C probe.
* **Private `_native.py`.** Adds the ctypes declarations; the inventory is
  115. The public `ams_mel.__all__` is unchanged, and the tests assert that
  the new private names are absent from it.

The native C, C++ header, Rust, and Python ABI probes cover every new record
field offset and every function signature.
