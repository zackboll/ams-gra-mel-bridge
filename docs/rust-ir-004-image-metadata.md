# RUST-IR-004 — safe Rust IR Image metadata reception

## Reconciliation with current main

The original implementation and test blobs reviewed at
`c69d04dbb0dfea6c14673107b5dce7706be5b516` are preserved during ordinary merges
with main through `4472baed06c1cfd32e798605306be4b4657ad36f`. The source and
qualification sections below record the **historical original campaign**; their
435-test and 229-export counts are not current-main acceptance counts. The current
production baseline is **272 AMS_MEL_0.1 exports**, unchanged by RUST-IR-004.
The 14 compiled mutation kills remain historical evidence for unchanged targets;
fresh reconciliation commands, results and source identities are recorded in PR
#159 and its merge handoff.

The four ordinary Image metadata families remain independent of Image/Health NUC
reception. Main also bridges the separate Image CameraCommandResp callback in C
and Ada; that callback is not part of this safe Rust API. Current Python Health
metadata/NUC and Ada coverage through IRMEL-ADA-008 are retained.

## Source and boundaries

Implementation and all qualification use the dedicated
`/home/zboll/git/ams-rust-ir-004` worktree on
`feature/rust-ir-004-image-metadata`. Initial fetched main was
`6f56f44813cd0a23b8312050eda9f7ed32c88a60`. During development, main advanced to
`1ab36aa488d9c12273efa5f338ef1f2dcbbe323d`; an ordinary `git merge origin/main`
fast-forward incorporated that CI-only update before combined-tree qualification.
No shared checkout, CI policy, native source/mock, raw bindings, Ada, Python or
vendored implementation was changed by this feature.

The final feature commit and tree are recorded in the PR and handoff rather than
embedded recursively in this file. Upstream declaration locks remain unchanged;
no provider transport or hardware semantics are inferred from mock evidence.

## Public API and complete payloads

`ImageStream::open_metadata(&mut self, queue_capacity: usize)` returns
`Result<ImageMetadataStream, Error>`. The metadata owner provides:

- `is_open(&self) -> bool` (local handle ownership, not provider liveness);
- `receive(&mut self, timeout_ms: u32) -> Result<ImageMetadataEvent, Error>`;
- `counters(&mut self) -> Result<ImageMetadataCounters, Error>`;
- `close(&mut self) -> Result<(), Error>` and non-panicking best-effort Drop.

The owner is unique, neither Send nor Sync (also tested with compile-fail
doctests), and independent of the Image/Session Rust lifetimes. Returned events
contain no native addresses and remain valid after actual provider unload.

`ImageMetadataEvent` has the exact published discriminants:

| Native value / Rust variant | Complete owned data |
| --- | --- |
| 1 / `BadPixelList` | `reported_size`, `reported_count`, ordered `Vec<BadPixel>` of full uint32 row/column and strict `BadPixelReason::Unknown = 0` |
| 2 / `LineOfSightReport` | signed `system_time_ns`, independent azimuth/elevation pointing angles and rates, strict `at_speed`/`in_tolerance`, all platform-attitude Euler components, uint32 validity bitfield and image rotation radians |
| 3 / `LineOfSightEuler` | signed `system_time_ns`, independent complete attitude and attitude-rate Euler triples |
| 4 / `NavigationResponse` | signed `system_time_ns`, full uint32 `command_id` and `request_id` |

`AzimuthElevation` stores radian angle components; no time denominator is invented
for upstream's undocumented pointing rates. The canonical existing `Euler` is
reused. No canonical safe NavigationResponse existed, so the new type represents
only asynchronous callback metadata, not Navigation request completion, a request
handle, or a synthetic provider error code. All floating bits are copied without
normalization, including NaN payloads, subnormals, infinities and signed zero.
Coordinate origin, clock epoch and undocumented validity bit positions are not
interpreted. Reported bad-pixel counts never truncate the actual vector.

## Registration, errors, FIFO and lifetimes

Exactly these existing exports are reused:

```text
ams_mel_ir_image_metadata_open
ams_mel_ir_image_metadata_receive
ams_mel_ir_image_metadata_get_counters
ams_mel_ir_image_metadata_close
ams_mel_ir_image_metadata_event_view
ams_mel_ir_image_metadata_event_close
```

Image owner liveness is checked first: null returns ProviderFailed even at zero
capacity. Live zero capacity returns InvalidArgument before native registration,
preserving the one-shot. Every actual attempt consumes the native one-shot,
including partial failure. Native registration order is BadPixelList, report,
Euler, Navigation response; Attached and Running both work. Prefer registration
before Start so early events are not missed. Synchronous callback delivery is
supported. No provider Return output is invented: native registration rejection
and NotSupported preserve ProviderFailed, while exceptions preserve
ProviderException. Partial failure publishes no safe owner; any unexpectedly
published owner on error is guarded for cleanup.

All four callbacks share the existing bounded DROP-INCOMING FIFO. Metadata
capacity is separate from frame capacity. Counters are saturating uint64 native
callback snapshots (`events_received`, `events_dropped_queue_full`,
`malformed_or_unsupported`), not Receive counts. Receiving does not decrement them.
Malformed callback/allocation failures increment malformed and allow recovery.
Rust adds no callback trampoline, thread, lock or queue.

Receive calls native exactly once and preserves Timeout, StreamStopped,
ProviderFailed, ProviderException and other categories. Its event RAII guard
checks non-null owner and aligned non-null view before dereference, dispatches
only the active payload, validates the entire active span before slicing,
fallibly reserves/copies nested data, and closes the event before returning.
Conversion errors, unexpected owners on native failure and unwind also close.
Close success without clearing the handle is ProtocolInconsistency; the stale
handle is forgotten, including when diagnostics are malformed, avoiding a second
free. A failed Close retaining its owner permits the guard's cleanup fallback.

Metadata Close is idempotent and ends consumption, not Image ownership or native
callback registration. Image Stop/Close logically ends enqueueing, with queued
events drainable before StreamStopped. Provider callbacks may remain retained
because upstream has no unregister. Native channel destruction is the
quiescence/provider-unload boundary. Rust does not strengthen that contract.

## Deterministic tests

The 13 private unit tests cover four discriminants/unknown kinds, poisoned
inactive payloads, strict reason/Boolean domains (including 2 and 255), empty,
null-nonempty, misaligned and impossible spans, reported count/vector mismatch,
order/duplicates, high-bit coordinates/IDs, signed timestamp extremes and every
floating field's bits/mapping. Injected Rust-only FFI operation pointers cover
null successful Receive, View failure/null/misalignment, Close failure/malformed
success, conversion failure/unwind, unexpected Open/Receive outputs, all Receive
status classes, full-width saturation snapshots, closed operations and exactly
one Close from stream Drop. No native test-only export was added.

Integration tests run every case in an isolated child to contain process-global
mock failpoints, lifetime logs and barriers. Five outer tests execute 31 child
cases (the child entry is deliberately ignored only in the outer harness):

- `sync`, `los-rich`, `four-mixed`: full payload equality against the C/mock
  fixtures, including 0xA5A50001/0x5A5A0003 reported fields, high-bit coordinates,
  all pointing fields, signed timestamps and 0xF1234567/0x89ABCDEF IDs. Four-mixed
  capacity 4 observes six callbacks/two drops/no malformed, exactly four retained
  kinds in order, and Timeout rather than an unexpected fifth event.
- `malformed`, `overflow`, `allocation`, `los-mixed`, report/Euler null,
  report allocation, Navigation null/allocation: exact callback/drop/malformed
  counters and complete recovered retained payloads, not just event kinds.
- All nine requested registration-failure scenarios: native error category,
  consumed one-shot, no published owner, teardown/unload; Navigation partial
  failure additionally invokes the retained callback during channel destruction.
- Metadata-first, Image-first, Session-first, Stop/drain before Image Close,
  implicit Drop, Running registration, zero-capacity retry, repeated Close,
  closed Receive/Counters, owned events after unload, normal frames after
  metadata Close, and NUC registration/reception in both registration orders.
- Nonquiescing callback Close and Drop: observe `.entered`, close/drop public
  owner, then channel destruction releases the mock barrier and joins. Lifetime
  log asserts callback entered < owner closed < callback returned < channel
  destroyed < library unloaded. No sleep is used as synchronization evidence.

The existing C contract owns simultaneous-registration testing; safe Rust uses
exclusive mutable borrowing instead of unsound aliases.

## Compiled mutations

Each mutation independently changed implementation only, compiled with
`cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib ir_image_metadata --no-run`,
then ran that ordinary test filter. Source was restored in a `finally` path.
Every kill was a runtime assertion, not a compile or loader failure.

**14 applied; 14 compiled; 14 killed; 0 invalid; 0 surviving.**

| Mutation | Killing unit test suffix |
| --- | --- |
| Swap report/Euler dispatch kinds | `dispatch_only_reads_active_payload_and_rejects_unknowns` |
| Map unknown kind to Navigation | `dispatch_only_reads_active_payload_and_rejects_unknowns` |
| Copy pointing rates into pointing angle | `all_float_fields_preserve_bits_and_field_mapping` |
| Copy Euler rates into attitude | `all_float_fields_preserve_bits_and_field_mapping` |
| Copy request ID into command ID | `signed_timestamp_extremes_and_navigation_ids` |
| Copy column into row | `bad_pixels_complete_order_duplicates_counts_and_reason_domain` |
| Accept any nonzero at_speed | `booleans_are_strict_and_independent` |
| Use reported count as span length | `bad_pixels_complete_order_duplicates_counts_and_reason_domain` |
| Ignore nonempty span | `spans_reject_null_misalignment_and_impossible_sizes_accept_empty` |
| Copy dropped counter into received | `counters_preserve_full_width_saturation_and_distinct_fields` |
| Remove explicit event Close | `event_view_conversion_close_errors_and_unwind_cleanup` |
| Remove event guard Drop Close | `event_view_conversion_close_errors_and_unwind_cleanup` |
| Remove stream Drop Close | `stream_drop_calls_native_close_exactly_once` (zero observed versus one required) |
| Collapse Receive status into protocol error | `receive_once_preserves_status_and_cleans_unexpected_error_outputs` |

Mutation drivers, JSON and raw logs are local qualification artifacts, not
committed source. Stream Drop's regression gate is an ordinary Rust test, not a
LeakSanitizer-only check. No sanitizer qualification is claimed for this task.

## Qualification commands and results

Native outputs are isolated in this worktree's `native/build` (production) and
`native/build-tests` (test-enabled). Rust test runs set:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
export CARGO_TARGET_DIR="$PWD/rust/target/ir004"
```

| Command | Result |
| --- | --- |
| `make native` | production facade built |
| `make test-native` | 435/435 passed |
| `make test-build-isolation` | passed both build orders, failpoint separation and cross-tree checks |
| `ctest --test-dir native/build-tests -R '^ir_image_metadata_contract$' --output-on-failure` | 1/1 passed |
| `cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_image_metadata` | 5 passed; one outer child entry ignored |
| focused integration with `--test-threads=1`, `4`, `16` | passed at each setting |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | 275 passed including doctests; six outer child entries ignored; existing Health metadata and NUC suites passed |
| `cargo check --manifest-path rust/Cargo.toml --workspace` | passed |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | passed |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | passed |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | passed |
| `make test-policy` | passed on updated main policy |
| `git diff --check` | passed |

One intermediate run was discarded: running build-isolation and Rust concurrently
let a native relink transiently expose a short shared-library file to the loader.
The successful final Rust run used stable artifacts after isolation completed.
The initial NUC-independent assertion was also corrected from four to three
points after checking the existing mock; no NUC implementation was changed.

Production qualification uses `AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib"`
and independent `CARGO_TARGET_DIR="$PWD/rust/target/ir004-production"`:

```sh
cargo test --manifest-path rust/Cargo.toml -p ams-mel --test ir_image_metadata \
  -- --skip native_test_allocation_failpoints
cargo test --manifest-path rust/Cargo.toml -p ams-mel --lib ir_image_metadata
cargo check --manifest-path rust/Cargo.toml --workspace
```

All pass: four production-compatible integration tests and all 13 unit tests.
Only the explicitly marked allocation failpoint test is excluded; callback
barriers and registration failure tests still run. ELF RUNPATH selects the
production directory; NEEDED includes `libams_mel_c.so.0`. The executable requires
no undefined `ams_mel_test*` symbol. Production's 229 versioned exports match the
starting main export map exactly, including all six existing Image metadata
functions. The public C header, export map, raw Rust bindings and native Image
implementation are byte-identical to initial main: no C record layout changed.

## Scope and handoff

This completes safe Rust coverage of the four **currently C-bridged** Image
metadata families, not all upstream callbacks. NUC remains independent and its
inaccessible upstream optional `tempError` remains unrepresented. Optional
LineOfSightQuaternion, OpticalDistortionMap, CameraCommandResp, LOS3D kinematics,
candidate-object and other unbridged metadata are not added. No Navigation
request interface, DSP, service, provider-private protocol or hardware/Squall
qualification is included. Hosted Bootstrap disposition, exact final source
identities and PR URL are recorded in the PR/handoff. Auto-merge remains disabled;
stop for independent technical review without merging.
