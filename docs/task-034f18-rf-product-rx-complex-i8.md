# Task 034F18 — RF ProductRx ComplexINT8 receive

## Reconciliation and evidence

Starting main is `4ad59b8d991a363755cf91ae733e98cb6eb21e5e`, exactly reviewed
main and PR #92's merge commit. Origin was fetched; no intervening changes,
open PRs or matching F18 work existed. Earlier worktrees were preserved.
The isolated worktree is `/home/zboll/git/ams-mel-034f18`, branch
`feature/034f18-rf-product-rx-complex-i8`. Disk-backed evidence is under
`/home/zboll/git/ams-mel-034f18-evidence`.
Main workflow 38341407883 was inspected once and was in progress, not failed.

Baseline measured native 295/295, Python 137/137, Rust 90 unit/integration
tests plus four doctests, and Alire library/full Ada success. The background
aggregate baseline continued after edits began and failed its formatter check
on the new Ada sources; it is not a pristine aggregate baseline pass.
All failed attempts and corrections remain in evidence logs.

## Exact pinned contract

RF MEL revision: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
JobDataFormat ComplexINT8 is 2 and ComplexINT16 is 3. JobDataPointer has
distinct `MELComplex<int8_t>*` and `MELComplex<int16_t>*` alternatives.
The callback remains
`std::function<void(std::shared_ptr<ProductRxMetadata>, JobDataPointer, size_t)>&`.
The final argument counts objects/elements, **not bytes**.

The actual-header probe runs explicit checks under GCC and Clang, including
Release/NDEBUG. Signed construction/accessors, independent copy construction
and assignment, default zeros, and variant discrimination are checked.
Measured native I8 size/alignment is 2/2 and I16 is 4/4; C I8 is 2/1.
No native/C layout compatibility is claimed. Every accepted component is read
through native `real()`/`imag()` and copied without scaling, normalization,
widening, reinterpretation or native-object memcpy.

## ABI and shared receive machinery

Exactly two production exports are added (206 → 208), still AMS_MEL_0.1:
`ams_mel_rf_data_submit_product_rx_v2` and
`ams_mel_rf_product_rx_event_view_v2`. They reuse frozen config-v1, sample
records/spans, metadata, and existing opaque request/endpoint/event owners.
Separate exports are necessary because frozen submit accepts only I16 and
frozen event-v1 has only an I16 span. Frozen declarations/layouts remain intact.

V2 submit accepts only I8/I16; every other value rejects before provider calls.
The explicit requested format travels in the existing completion. The sole
worker reads endpoint ID and assigned format once each and rejects any assigned
format not equal to the qualified request before registration. Format is never
inferred from a callback. No extra worker/completion/receiver is introduced.

The existing callback state stores the qualified format. Only its exact typed
variant is accepted, including zero-count callbacks. Null/nonzero, configured
sample limit, selected vector max_size and allocation arithmetic are checked
before reading samples. Existing unsupported metadata remains unsupported.
Separate event-owned selected-width vectors provide stable immutable storage.
V2's data_format is authoritative; inactive and empty spans are `{NULL, 0}`.
For I16, v1 and v2 views share identical I16 samples/metadata. V1 view on I8
returns **AMS_MEL_INVALID_ARGUMENT**, preserving the caller's output pointer.
Both borrowed views live until the existing event-close operation.

The callback queue, DROP-INCOMING policy, saturation counters, failure barrier,
unique Claim, terminal caching, future abandonment, nonblocking request Close
and parent-first child claims are unchanged. No lock is held during provider
methods. The heap-stable exact registered function and DSO are retained
permanently even if registration throws or the provider retains a reference.
Logical Close wakes receivers and drains executing callbacks before endpoint
destruction. Late callbacks do not touch provider sample memory. Drain is not
provider quiescence and does not grant a new unload guarantee. Copied events
have no provider owners and survive graph teardown when the existing graph
permits it.

## Safe Ada and raw parity

Create_Config remains source-compatible and defaults to I16.
Create_Config_Complex_I8 stores the width privately; I8 Submit uses v2 while
I16 retains frozen submit. I8 Receive uses v2, I16 keeps its v1 path.
With_Samples/Copy_Samples remain I16 operations; With_Samples_I8 and
Copy_Samples_I8 expose signed Integer_8 pairs. Wrong-width access raises
Constraint_Error even for empty events, before accessing any sample address.
Borrowing requires an open Event externally serialized with Close; explicit
copies are independent Ada arrays. Owners remain automatically finalized.

Private Ada, Rust sys and private ctypes mirror both exports and event-v2.
Compiled C11 measurements check signatures, size/alignment and all event
fields, typed spans and nested metadata offsets. Current 64-bit event-v2 is
112/8, offsets 0,8,16,32,48. Rust/ctypes compare actual C output; Ada compares
the measured layout. No public Python or safe Rust API is added.

## Qualification status

Final native GCC 14.2 and Clang 19 Debug/Release each pass **312/312** in
final-native-matrix-final.log. Python passes **138/138** plus compile checks;
Rust passes **91** unit/integration tests plus **four separately run doctests**,
workspace check, Clippy with warnings denied and formatting. Final Alire library
and full Ada suite pass; aggregate make check, direct-GPR build/run, Ada
formatting, build-tree isolation and newline/whitespace checks pass in
final-validation-fourth.log. Production Release has exactly **208** exports
under AMS_MEL_0.1, no test symbols; vendor inventory is **804/804** and frozen
C declarations/constants are byte-identical outside the additive block.

final-public-ada-release.log and final-public-c-release.log qualify the focused
C and public Ada I8 checks for **50 iterations** with GCC Release native backing.
Samples include (-128,127),
(127,-128),(-1,0),(0,-1),(7,-11),(23,42). I8 metadata uses interval 18 and
stream IDs [18,0,18], distinct from I16. Provider storage is overwritten after
callbacks, with copied events observed after endpoint and parent Close.
Process-isolated I8 lifecycle regressions reuse deterministic barriers and
observation hooks rather than timing guesses.

I8 cases directly cover exact-function copy/reference/move retention, late
callbacks with inaccessible selected-width sample memory, drain before page
revocation, registration throws before/after retention and with active callbacks,
pending abandonment/parent-first close, unclaimed destruction, worker-launch
failure, stopped receivers and saturation. Initial fixture failures (persistent
provider create counters, I16-only view assertions, late callback lookup after
endpoint destruction) were corrected without weakening production contracts.
Ada layout probe warnings were fixed by initialization. A constant object's
alignment can exceed its type's alignment: the erroneous object-alignment check
was corrected to compare the C-compatible type alignment. Missing final
newlines were corrected. All failed logs remain available.

Pinned Squall revision is `b1015728f904c799fa0c07489fce48e78f67845f`.
Real-provider acceptance retains I16 as the control. No positive I8 Squall
fidelity is claimed without an observed I8 callback. Mock callbacks and pinned
actual-header probes provide positive I8 fidelity. Complete 32-bit runtime and
cross-language bridge qualification remains unavailable, not claimed.

Final-source Squall Job, Ada VirtualAperture, C ProductRx and Ada ProductRx each
pass **one iteration**, sequentially, in final-validation-fourth.log, concluding
FINAL_SQUALL_COMPLETE. Task-owned backing is evidence/squall with port bases
57000/58000/59000/60000. C ProductRx observes nontrivial I16 samples, later
buffer reuse and event survival; Ada ProductRx passes the preserved I16 path.
No I8 request was attempted against Squall and no I8 capability/fidelity claim
is made. Job acceptance remains call-path evidence, not transmission or payload
execution. Only harness-owned integration resources were cleaned up.

Coverage boundaries for review: simultaneous mixed-width C endpoints are
interleaved deterministically, not a dedicated concurrent mixed-width producer
stress. Existing concurrency is directly rerun with selected I8 for drain and
late callbacks. Ada directly tests positive I8 fidelity and both wrong-width
borrow/copy directions; exhaustive malformed callback/counter cases are C
fixtures. Oversized counts reject before access through the configured limit;
the vector-capacity/arithmetic guards are implemented but not separately reached
with an effectively unlimited I8 config. No new malformed metadata getter
implementation was introduced. Hosted CI/publication identities are recorded in
the PR and handoff, not inferred from starting-main CI. Independent review is
required; local qualification is not approval.

## Explicit exclusions

No DirectINT8/DirectINT16, floating-point, VITA, PDW or LF payload receive;
new metadata fields; RDMA registration/external memory ownership; endpoint
association/routing; WaveformTxEndpoint/CachedWaveform; Weights; waveform
transfer, DSP, RF generation/transmission; vendor modifications; public Python
or safe Rust APIs; unrelated lifecycle refactoring. Publication stops at an
open, non-draft, unmerged independent PR review gate with auto-merge disabled.
