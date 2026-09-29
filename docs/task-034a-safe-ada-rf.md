# Task 034A: safe Ada RF

Starting main: `e332d1a05736c16390246d2ed98563d0453f192e`.
The native ABI stays **0.1 / 115 exports**; there are no native production
source, raw FFI, vendor, Rust, or Python changes.

`AMS.MEL.RF` owns DataMEL, returns the existing `AMS.MEL.Provider_Version`
using the native two-call exact-string interface, and copies complete MFA
snapshots into Ada-owned values before closing the native snapshot. Input
strings are passed as bytes, reject embedded NUL, and have exception-safe
temporary C ownership. Open and Close failures raise `Provider_Error`; a
failed native Close consumes the DataMEL handle, and finalization never raises.
Spans are checked for non-null data when nonempty and for representability in
both Natural and `Ada.Containers.Count_Type` before importing them. Each
nested range, format, and face is copied while the native snapshot remains
owned. Booleans must be 0/1. Frequency ranges are neither sorted nor
normalized. Actual face count and reported face count remain independent;
face IDs are not inferred from their indices. All 14 pinned formats have
constants, while unknown 32-bit raw format values remain representable.

`AMS.MEL.RF.Product_Rx` owns asynchronous creation requests, uniquely claimed
endpoints and received events as limited controlled values. Configuration is
ComplexINT16-only. Wait timeout is not cancellation; cached ErrorOr failures
return the published error code and complete UTF-8 diagnostic (a 0-timeout
retry obtains an oversized diagnostic and verifies status, code and required
length). Claim does not block. Closing a request after claiming does not close
the endpoint. Both request and endpoint rely on native parent-first lifetime,
not an Ada parent reference. Endpoint close propagates native deferred-parent
failure, whereas finalization suppresses exceptions.

An Event owns the native immutable copied ComplexINT16 payload and its copied
Ada metadata and stream IDs. `With_Samples` imports the array directly at the
native event span address only while the callback executes; zero-length events
call the callback with a valid empty array. The test-only child under
`ada/tests/src` verifies `Samples'Address` equals the event's native sample
address. Compile-time size, object-size, and alignment checks compare the
safe Ada record against the **public C ABI** `RF_Complex_I16_V1`, not the
provider's C++ layout. `Copy_Samples` explicitly performs an Ada-owned copy.
The native bridge has already copied the provider callback payload, so this
is no second Ada bulk copy, **not** end-to-end zero-copy. Events remain valid
after later callbacks, endpoint close, and DataMEL close. Explicit Close
invalidates borrowed samples; finalization never raises.

The separate mock RF provider drives the Ada test suites; test-only `dlopen`
obtains its synchronous emission and delayed completion controls. The optional
`make test-squall-rf-ada` uses the pinned Squall runtime and test-only job
helper to activate an RX job. No production RF job/C2 API is introduced.
The test-only `ams_mel_rf_only.gpr` builds `ams_mel_rf_smoke` from the two RF
suites for a focused repeat-50 run. It links the production Ada/C facade and
resolves the contract-test C facade at run time via `LD_LIBRARY_PATH`; it is
not an additional production Ada package or API.

Out of scope: RDMA, external receive endpoints, all other sample formats,
richer Pointing/ReceiveEvent/`std::any` metadata, PhysicalData, quantization,
Tx power modes, RF C2/jobs/VADB, safe Rust RF and public Python RF. This is the
complete **current native RF slice**, not the entire RF MEL standard.

## Local validation

`make test-native` passed 246/246 native tests, including both RF source
closure checks. `make test-build-isolation`, `make check-ada-format`,
`alr -C ada build`, and `alr -C ada/tests run` passed. The two RF suites
passed 50/50 runs through the focused `ams_mel_rf_smoke` executable. The
opt-in `SQUALL_SOURCE_DIR=/home/zboll/git/squall make test-squall-rf-ada`
passed against pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f`:
version 1/1/Squall/Squall Simulator RF MEL, one receive-only face, at least
eight nonempty nontrivial events, no malformed/queue-full counters, and event
A unchanged after later callbacks and endpoint/DataMEL close. Its executable
is ELF and dynamically links `libams_mel_c.so.0`, not Squall or the mock.

`make test-rust`, the Rust workspace cargo check/test/clippy/fmt gates, and
`make test-python` (106 Python tests and compileall) passed; Rust and Python
source are unchanged. A separate Release build exported ABI 0.1 and exactly
115 names equal to the starting main export list. The vendor and checksum
inventory have no diff (628 checksum entries). The newline/whitespace gates
passed. The local `make test-ada` and `make check` reach their Ada step and
stop only because bare `gprbuild` is not on PATH; Alire and direct GPR through
Alire passed instead. An extra opt-in C Squall rerun was attempted but did not
complete after transient Podman image-in-use cleanup and a subsequent upstream
container rebuild; the existing C native regression suite did pass. Do not
report that optional C integration rerun as passed.
