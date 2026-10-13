# Task 034F19 — DirectINT8/DirectINT16 ProductRx receive

## Checkout and baseline

Origin was fetched. PR #93 is merged at starting main
`84e49dbcdb84ea6820db01f2f51dd21d00a82a5b`, exactly the reviewed main, with
no intervening commits, open PRs or matching F19 work. Existing worktrees were
preserved. Work is isolated in `/home/zboll/git/ams-mel-034f19` on
`feature/034f19-rf-product-rx-direct-samples`. Task-owned disk-backed evidence
is under that worktree's `build/034f19-evidence` directory.

Main workflow 38345887399 was inspected once and was in progress. This is not
a feature workflow result. Actual feature and synthetic PR checkout identities
must be distinguished at publication.

Pristine baseline: GCC and Clang Debug/Release each 312/312 native tests;
Python 138; Rust 91 unit/integration plus four separately executed doctests;
Alire library/full Ada, corrected aggregate check, direct GPR through aggregate,
formatting and build-tree isolation passed. Production exports were 208 under
AMS_MEL_0.1; vendor SHA-256 inventory was 804/804. Initial aggregate attempts
failed because GPRbuild was absent from the shell PATH and because Alire exec
uses its crate working directory. Running `alr -C ada/tests exec -- make -C
/home/zboll/git/ams-mel-034f19 check` corrected those invocation errors.

## Pinned native semantics

RF MEL revision is `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Actual-header probes with GCC/Clang Debug and Release/NDEBUG use type traits
and active runtime checks. JobDataFormat values and JobDataPointer alternatives:

| Value | Format | Exact native pointer | Meaning |
|---|---|---|---|
| 0 | DirectINT8 | `int8_t*` | signed scalar, size/alignment 1/1 |
| 1 | DirectINT16 | `int16_t*` | signed scalar, size/alignment 2/2 |
| 2 | ComplexINT8 | `MELComplex<int8_t>*` | Cartesian I/Q pair |
| 3 | ComplexINT16 | `MELComplex<int16_t>*` | Cartesian I/Q pair |

The callback takes its std::function by non-const lvalue reference. Its size_t
counts **elements, not bytes**: scalar values for Direct, pairs for Complex.
Direct values are copied exactly into event-owned signed-width vectors, never
reinterpreted as complex objects, widened, normalized, byte-swapped, converted
to I/Q or supplied with an imaginary component. Vendor source is unchanged.

## Additive ABI and ownership

Exactly `ams_mel_rf_data_submit_product_rx_v3` and
`ams_mel_rf_product_rx_event_view_v3` are added. Config-v1, metadata-v1,
handles, Close/Wait/Claim/Receive/counters, historical records and declarations
remain frozen. V1 submit/view remain ComplexINT16 only. V2 submit/view remain
ComplexINT8/ComplexINT16 only. V1/v2 views reject Direct with INVALID_ARGUMENT
and leave the output pointer untouched. V3 submit accepts precisely 0..3 and
rejects other values before provider creation.

Event-v3 has four typed spans. The authoritative format selects exactly one;
all inactive spans are `{NULL,0}`. Empty events have four empty spans while
retaining their format. Immutable records and samples borrow one event owner
until the original event Close. Complex v1/v2/v3 views share selected sample
storage and the same owned metadata/stream-ID graph; no provider sample pointer
survives the callback. Events survive endpoint and parent Close.

The existing completion carries the exact requested format; the single worker
requires assigned == requested before success/registration. No new worker,
future consumer, callback registry, owner or metadata schema is introduced.
The heap-stable exact registered std::function and provider DSO remain retained
permanently wherever callback quiescence cannot be established. Logical Close
precedes callback drain and endpoint destruction. Late callbacks take the Closed
path before reading metadata or either Direct/Complex sample storage. No new
code-unload or provider-quiescence guarantee is made.

## Safe Ada and raw parity

Existing constructors and Complex accessors remain source-compatible. New
Direct constructors route privately through submit-v3; default I16 uses v1 and
ComplexI8 uses v2. Direct Receive uses v3 and extracts scalar span/metadata
without fabricating a Complex record. Direct_I8_Array/Direct_I16_Array expose
Integer_8/Integer_16 values. Width-specific With_Direct_Samples and Copy_Direct_Samples
operations reject wrong formats before sample address access, including empty
events. Borrowing requires an open event and external serialization with Close;
copies are independently Ada-owned. Count conversions remain checked before
array bounds. Limited controlled owners/finalization are unchanged.

Private Ada, Rust sys and private ctypes mirror v3. A compiled C11 probe checks
exact signatures and every event/span offset plus frozen metadata placement.
Measured 64-bit event-v3 size/alignment is 144/8, field offsets
0,8,16,32,48,64,80. Rust/ctypes compare compiled measurements; Ada checks the
measured representation, retaining historical tests. Complete 32-bit runtime
and cross-language qualification is not claimed.

## Qualification and corrections

The raw C fixture runs 50 iterations of all four submit-v3 formats on coexisting
endpoints, ordered interleaving, inactive spans, v1/v2 rejection, v3 Complex
parity, Direct zero/one/multiple samples, metadata, malformed inputs, configured
limits, allocation failure, queue overflow and event survival. Required Direct
sequences are observed from retained event storage after callback return and
provider storage overwrite:

* I8: [-128,127,-1,0,7,-11,23,42].
* I16: [-32768,32767,-1,0,12345,-23456,17,-19].

Public Ada tests run 50 iterations per Direct width, checking borrowed and
independent copied values, format/count/stream IDs, parent-first closure, event
survival and copies after event Close. Wrong-format borrowed access is tested
across all four formats, including empty Direct events.

Process-isolated DirectI8 and DirectI16 campaigns reuse existing deterministic
barriers for copy/reference/move registration, synchronous callback,
registration throws, held callback/Close drain, endpoint-owned page revocation,
late callback, parent-first lifetime, abandonment, unclaimed destruction,
stopped Receive, saturation and worker-launch failure. Direct page objects are
constructed with their correct scalar types, not aliased MELComplex objects.
Existing destructive negative controls remain active. Ordered four-format
interleaving is not a randomized concurrent-producer stress claim.

Failed attempts are retained: incorrect standalone probe include paths;
Complex-only test Claim helper in the first Direct C fixture; Complex-only safe
Ada Claim guard; and a mismatch mock returning DirectINT16 when that was already
the requested format. Corrections change fixtures/allowed domains rather than
weakening production contracts or removing tests.

Final native GCC Debug/Release and Clang Debug/Release each pass **347/347**
in final-matrix-corrected.log. Python passes **139** tests and compile checks;
Rust passes **92** unit/integration tests plus **four separately run doctests**,
workspace check, Clippy -D warnings and rustfmt. Alire library/full Ada,
formatting, direct-GPR build/run, aggregate make check, build-tree isolation,
whitespace and final-newline checks pass in final-languages.log. The full Ada
suite is additionally run with GCC Release native/mock backing, retaining its
50-iteration DirectI8 and DirectI16 cases. Focused C Direct checks also pass
50 iterations with Release/NDEBUG checks active (final-public-c-release.log).
Production Release has **210** exports, AMS_MEL_0.1 only and no test hooks.
The entire historical C header is byte-identical after removing the additive
v3 block; vendor inventory is **804/804** (production-audit.log).

The vector capacity/arithmetic guards exist
for selected types; oversized fixtures currently reach the configured maximum
first, not independent unlimited-config capacity qualification.

## Provider and scope limits

Pinned Squall is `b1015728f904c799fa0c07489fce48e78f67845f`.
Final-source Squall Job, Ada VirtualAperture, C ProductRx and Ada ProductRx
each passed one iteration sequentially (squall.log), using task-owned backing
and port bases 51000/52000/53000/54000. The C client observed nontrivial I16
samples, subsequent provider buffer reuse and retained-event survival. No
Direct request was attempted against Squall. Docker is absent; the existing
harness selected the available Podman runtime. Only harness-owned resources
were cleaned up.
ComplexINT16 remains the real-provider positive control. No Direct request is
required against Squall and no positive Direct provider capability/fidelity is
claimed without correctly typed observed callbacks. Positive Direct fidelity
comes from pinned-header and mock C/Ada qualification. Job call-path evidence
does not imply payload execution, RF transmission, RDMA, DMA or physical DSP.

Exclusions: VITA packet receive; PDW/LF formats; floating-point Direct samples;
metadata schema changes; RDMA/external memory registration;
WaveformTxEndpoint/CachedWaveform; Weights; DSP/resampling/mixing/conversion;
hardware transmit/RF emission; vendor changes; new public Python or safe Rust
APIs; unrelated lifecycle refactoring. Publication stops at an OPEN, NON-DRAFT,
UNMERGED independent review PR with auto-merge disabled.
