# Task 034F21 — AMS VITA Medium ProductRx receive

## Checkout and evidence

Starting main: `023f094fd666884ff3deb79b9c3e3936dd01b30b`, PR #95 merged.
The existing `/home/zboll/git/ams-mel-034f21` worktree and
`feature/034f21-rf-product-rx-vita-medium` branch are reused. Earlier worktrees
are preserved. Commands, failed attempts, corrections and qualification logs
are retained in `/home/zboll/git/ams-mel-034f21-evidence`.
Main push workflow 38487877462 was inspected once while in progress; it is not
a feature validation result.

Pristine baseline: GCC/Clang Debug/Release each **367/367**; Python **140**;
Rust **93** unit/integration tests and **four** separately run doctests. Alire
library/full Ada, formatting, direct-GPR, aggregate check and build isolation
completed with exit zero. Vendor SHA-256 inventory is **804/804**.

## Pinned packet contract

RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Format 5 selects exactly `DataPacketMedium*`, variant alternative 5, alias of
`FixedDataPacket<1024>`. Object storage is **1,024 bytes / 256 words**;
maximum payload is **992 bytes** after the mandatory 32-byte overhead. Small
remains 512 bytes / 128 words / 480-byte maximum payload. Capacity is never
treated as serialized length. Callback/configuration counts mean packet
objects, not bytes, words or IQ samples.

The existing actual-header probe passes GCC and Clang in Debug and
Release/NDEBUG with explicit checks. Builder-generated lengths are 8, 129 and
256 words: **32, 516 and 1,024 bytes**. The 516-byte case crosses Small's
capacity with a genuine header length, not arbitrary padded storage. Distinct
headers, stream IDs, counts, timestamps, payloads and trailer indicators are
used. Unused tails contain a nonzero sentinel. Invalid length/header/class/
trailer cases are checked adjacent to an inaccessible guard page.

The pinned view checks the seven-word prologue; signal-data type 1; C-bit 1;
TSI 3; TSF 2; mandatory trailer flag; Nd0/S zero; OUI 0xAAAAAA; Tx/Rx Comm
Base Set info class, version zero; packet class 0x0D, version 0x05. Declared
length must include the prologue/trailer and fit the supplied storage before
the trailer is indexed. Trailer bits outside its allowed sample-frame enable
and indicator masks reject. This is **not** full protocol semantic validation,
payload decoding, timestamp interpretation or hardware qualification.

## Additive ABI and shared implementation

Exactly submit-product-rx-v5 and event-view-v5 are added; target production
inventory is **214 AMS_MEL_0.1** functions, with no test hooks. V5 nests the
complete frozen v4 and adds a Medium descriptor span. Historical declarations
and domains remain frozen: v1={3}, v2={2,3}, v3={0..3}, v4={0..4}, v5={0..5}.
Old views reject Medium without altering the output pointer. Medium has four
inactive integer spans and an inactive Small span. Earlier formats reproduce
v4 and have an inactive Medium span. Empty events retain their format.

Small and Medium share type-specific checked source selection and copying.
Exact variant, null/count, configured maximum, container capacity, native
object extent, PTRDIFF_MAX, address overflow and alignment are checked before
provider packet access. The contiguous-object allocation extent remains the
provider's contract; the bridge cannot prove a real allocation from a pointer.

Only after `isValid()` succeeds is the header word count read. Checked word
and byte bounds select exactly the serialized bytes from public `bytes()`.
No network/packet order conversion occurs. Any malformed later packet rejects
the whole callback. All byte vectors are prepared before descriptor addresses
are formed; no backing mutation follows publication and the event stays
heap-stable. A later allocation failure cannot publish a partial event.

Events own independent bytes/metadata and survive provider overwrite,
subsequent callbacks, endpoint Close and parent Close. Descriptor and byte
borrows last until the existing Event Close. Safe Ada adds Medium config,
packet count, 1-based borrowing and independent copies using Packet_Byte_Array.
Receive uses v5 only for Medium and checks all inactive spans. Wrong-format
operations reject even for empty events; private borrowed state clears on
Close/finalization. No public foreign pointers or native packet objects appear.

The single worker, completion cache, unique Claim, bounded DROP-INCOMING
queue, saturating counters and permanent exact-function/callback-state/DSO
retention are unchanged. Logical Close precedes admitted-callback drain;
endpoint destruction follows drain. Late callbacks access no packet memory.
Drain proves no current bridge callback, **not provider quiescence or safe DSO
unload**.

The guarded mock retains six Small objects unchanged. Medium uses three
properly aligned/constructed objects: **3,072 bytes fit the 4,096-byte writable
page**, checked before delivery. No Small object is reinterpreted as Medium.

## Qualification status and limitations

The initial focused C run passes **39/39**, including 50-iteration Medium
packet fidelity, the compiled C11 v5 signature/layout probe, inherited Small
checks and 18 process-isolated Medium lifecycle/format cases. Full-byte expected
sequences are independently computed, not copied from provider objects.
Mock storage and metadata are overwritten after delivery. Malformed later
packets, wrong Small variant (including zero count), null, arithmetic overflow,
misalignment, queue overflow, failure-atomic allocation and recovery are covered.

Retained corrections: GCC rejected a temporary nested-vector capacity
expression inside the template; a local const vector retains the same bound.
An Ada fixture initially retained Small's metadata ID; Medium uses ID 22.
The Python public-surface inventory also needed the additive export count.
No warning suppression, vendor edit or inherited test removal was used.

Final GCC/Clang Debug/Release native suites each pass **387/387**. The complete
Ada smoke suite also passes with GCC Release native/mock backing, including
50-iteration Medium byte fixtures; focused C Medium additionally passes 50
iterations with Release/NDEBUG checks active. Python passes **141** tests.
Rust passes **94** unit/integration tests and **four** separately counted
doctests, workspace check, Clippy -D warnings and rustfmt. The initial Rust
inventory check retained 212; its two expected counts were corrected to 214,
with the failure retained. Production Release audit measures **214 AMS_MEL_0.1**
exports with no test hooks. Removing the F21 additive block reproduces the
starting C header byte-for-byte. Vendor SHA-256 inventory remains **804/804**.
Formatting, whitespace and final-newline checks pass (the new document's
initial missing newline was corrected). Alire library/full Ada, aggregate
make check and build-tree isolation pass. The last direct-GPR invocation in
final-languages.log failed on an already-exported provider environment variable
with an equivalent differently spelled path; unsetting it corrected the
invocation. direct-gpr-corrected.log completes the direct-GPR build/full smoke
and format/newline/whitespace checks. The campaign's exit 1 is retained, not
reported as a wholly successful script. Final-source Squall Job, Ada VirtualAperture, C ProductRx and Ada ProductRx
each pass **one iteration**, sequentially, in final-squall.log, ending with
FINAL_SQUALL_COMPLETE (exit 0). Task-owned resources use port bases
41000/42000/43000/44000. The C control observes nontrivial I16 data and retained
events; no Medium request is made. Hosted workflow and publication identities
are recorded in the PR/handoff after verification, not inferred from main CI. Complete 32-bit cross-language
runtime qualification is unavailable and not claimed.

Pinned Squall: `b1015728f904c799fa0c07489fce48e78f67845f`.
ComplexINT16 remains the real-provider positive receive control. Medium support
is not established; no positive real-provider Medium fidelity is claimed
without an observed correctly typed callback and verified packet bytes. Mock
and actual-header evidence provide the positive Medium qualification.

Coverage limits: public Ada directly checks zero and three Medium packets;
one-packet acceptance is exercised through C earlier-format compatibility and
mock count fixtures, not a dedicated public Ada one-Medium fixture. Medium
wrong-format integer borrowing and Small/Medium copy operations are directly
checked; exhaustive malformed callback/counter campaigns are C fixtures.
Medium-specific copy/reference/move late callbacks, registration throws,
held-callback Close, page revocation ordering, parent-first closure, abandonment,
unique Claim, stopped Receive, saturation and worker failure reuse deterministic
barriers. Inherited destructive controls remain active. No randomized producer
stress or full provider quiescence is claimed.

## Exclusions and review gate

AMSVitaLarge/ExtraLarge, VITA payload decoding, host-order conversion, packet
generation/transmission, new ProductRx metadata, PDW/LF, RDMA/external memory
registration, WaveformTxEndpoint/CachedWaveform, Weights/DSP/RF emission,
public Python/safe Rust APIs, vendor changes and unrelated threading/lifetime
refactoring are excluded. Publication must stop at an open, non-draft, unmerged
PR with auto-merge disabled. Local qualification is not independent approval.
