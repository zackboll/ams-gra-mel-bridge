# Task 034F20 — AMS VITA Small ProductRx receive

## Checkout and evidence

Starting main: `e581f6b251b67407fb170f0460125a24774b371e` (PR #94 merged).
The existing worktree `/home/zboll/git/ams-mel-034f20` and branch
`feature/034f20-rf-product-rx-vita-small` are reused. Earlier worktrees and
unrelated changes are preserved. Disk-backed commands, source probes, failed
attempts and results live in `/home/zboll/git/ams-mel-034f20-evidence`.
Main workflow 38481342404 was inspected once while in progress, not failed.

Pristine baseline: native GCC/Clang Debug/Release each **347/347**; Python
**139**; Rust **92** unit/integration tests plus **four** separately executed
doctests; Alire library/full Ada, aggregate check, formatting, direct-GPR smoke
and build-tree isolation pass. Vendor SHA-256 inventory **804/804**. The initial
210 non-test symbol measurement came from a test-enabled library, not a
production Release audit.

## Pinned contract and validation boundaries

RF MEL is pinned at `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
AMSVitaSmall is format **4**, selecting exactly `DataPacketSmall*`, the fifth
JobDataPointer alternative. A callback contains an array of packet objects;
its count and the frozen `max_samples_per_event` count **packet objects**, not
bytes, IQ pairs or decoded sample values.

Each native object owns **512 bytes / 128 words**. Capacity is not serialized
length. The public `view()` constructs the pinned DataPacketView; only after
`isValid()` succeeds does the bridge read `getPacketSize()` (32-bit words).
The checked length is 8..128 words, or **32..512 bytes**. Only those bytes from
the public `bytes()` span are copied, including prologue, payload and trailer.
No storage-tail bytes are read, no object-layout memcpy is used, and no host
byte-order conversion, normalization or IQ decoding occurs.

The pinned view checks a seven-word prologue, signal-data packet type 1,
C-bit 1, TSI 3, TSF 2, mandatory trailer flag, Nd0/S bits zero, OUI 0xAAAAAA,
Tx/Rx Comm Base Set info class, info class version zero, packet class 0x0D and
packet version 0x05. It checks declared length against prologue plus trailer
and supplied storage **before** indexing the trailer. Trailer bits outside
the two sample-frame enable and two indicator bits reject. It does not prove
payload semantics, timestamp meaning, hardware validity or universal VITA
conformance. The bridge reuses, rather than duplicates, this validation.

Actual-header builder probes already passed GCC 14.2 and Clang 19.1.7 Debug
and Release/NDEBUG with explicit checks. They use 32-, 68- and 512-byte packets,
distinct IDs/counts/timestamps and packet-order payloads, nonzero irrelevant
tails and guarded fixed storage. Malformed header/class/trailer/length cases
reject without crossing the tested inaccessible-page boundary. These probes
are not a provider or full semantic validation claim.

## Additive ABI and ownership

Exactly two production exports are added: submit_product_rx_v4 and
product_rx_event_view_v4, under **AMS_MEL_0.1** (target **212** exports).
The packet descriptor wraps frozen u8-span-v1; its ordered span is appended to
an event-v4 that **nests frozen event-v3**. Historical declarations, constants
and layouts are unchanged. Submit accepted sets remain v1={3}, v2={2,3},
v3={0,1,2,3}; v4 adds only 4. Assigned format must equal requested before
callback registration. Views v1/v2/v3 reject VITA without publishing an output
pointer. Integer events reproduce v3 with an empty packet span; VITA events
retain format 4 with four inactive integer spans, including zero packets.

Each event is heap-stable behind its existing owner. Separate byte vectors are
fully prepared before descriptor pointers are formed; neither those vectors
nor the descriptor vector is resized after publication. Queue transfers move
only unique pointers, not event objects. Borrowed records and bytes remain
valid until the existing event Close, independently of provider buffer reuse,
callback return, subsequent callbacks, endpoint Close and parent Close.
Events own bytes only, not native packet objects, endpoints or provider DSOs.

Any malformed packet rejects the **entire callback**; copying an earlier
packet never publishes a partial event. Allocation exceptions use the existing
allocation-failure counter path. Exact variant selection, null/count checks,
configured maximum, container capacity and object/descriptor extent checks
precede packet access. DROP-INCOMING and saturating counters are unchanged.

The existing worker, completion, unique Claim, future ownership and bounded
queue are reused. Logical Close precedes admitted-callback drain, and provider
endpoint destruction follows drain. Late callbacks take the Closed path before
payload access. The exact registered function, state and provider DSO remain
permanently retained where provider callback quiescence cannot be proven.
No new unload guarantee is asserted.

## Ada and raw bindings

Safe Ada adds Create_Config_AMS_VITA_Small and 1-based packet count/borrow/copy
operations over Packet_Byte_Array. Sample_Count counts packet objects. Receive
uses v4, checks authoritative format and inactive integer spans, checks outer
descriptor count before narrowing and copies metadata as before. Payloads are
not copied again during Receive. Individual access checks index and the
32..512-byte multiple-of-four span before importing an array. Explicit copies
are independent Ada owners; borrowed descriptor state is cleared on Close and
finalization. Wrong-format operations reject even on empty events.

Private Ada, Rust sys and private ctypes mirror the additive ABI. A compiled
C11 signature/layout probe measures every new field, size and alignment;
historical v1/v2/v3 probes remain active. No public Python or safe Rust receive
API is introduced. Complete 32-bit cross-language runtime qualification is
not claimed.

## Qualification status

Initial focused C packet qualification passes **50 iterations**, checking all
serialized bytes of 32/68/512-byte packets after provider overwrite and parent/
endpoint Close. Malformed later packets, metadata, exact alternatives, null,
oversized count, zero packets, queue overflow and later-copy allocation failure
are checked. Focused lifecycle fixtures use actual aligned native packet
objects and deterministic existing gates, not scalar reinterpretation or sleeps.

Corrections preserved in evidence: the original standalone probe destructor
syntax; a nonexistent fixture request-close helper; an Ada fixture using RX.Close
instead of RF.Close for the parent. An initial Ada/native lifecycle validation
launch overlapped a shared native build tree; those runs are not authoritative
and the affected checks are rerun serially. Further retained corrections: the exact Python symbol tuple needed the two
additive exports; Ada's zero-index fixture required a constant for warning-clean
compilation; the new C layout probe required a final newline for Clang. Final
review corrected VITA worker-launch fixture routing to submit-v4 and added a
VITA assigned-format mismatch plus an unlimited-config object-extent guard test.
The mismatch fixture was corrected to expect the inherited cached result write.

Final-language validation passes Python **140**, Rust **93** unit/integration
plus **four separately executed doctests**, workspace check, Clippy -D warnings,
rustfmt, Alire library/full Ada, direct-GPR, formatting, aggregate check,
build-tree isolation and whitespace/newline checks. Full Ada additionally
passes with GCC Release native backing, including the 50-iteration packet case.
Production Release exports exactly **212 AMS_MEL_0.1** symbols and no test hooks;
vendor inventory remains **804/804**. Removing the additive header block
reproduces the historical header byte-for-byte. Final native GCC Debug/Release and Clang Debug/Release each pass **367/367**
after the final fixture corrections (final-native4.log). Focused packet tests
execute 50 iterations in every configuration, including Release/NDEBUG.
Eighteen separately process-isolated VITA lifecycle/format cases retain the
existing deterministic gates; inherited destructive negative controls stay active.

Pinned Squall Job, Ada VirtualAperture, C ProductRx and Ada ProductRx each pass
**one iteration**, sequentially, using task-owned backing and port bases
61000/62000/63000/64000. The C receive control observes nontrivial I16 products,
buffer reuse and retained-event survival. No VITA request is attempted; no
positive real-provider VITA capability/fidelity is claimed. Publication/hosted
checkout identities are recorded in the PR and handoff after verification.

## Provider and scope limits

Pinned Squall: `b1015728f904c799fa0c07489fce48e78f67845f`.
ComplexINT16 remains the real-provider control. Squall VITA support is not
required, and no positive Squall VITA fidelity is claimed without an observed
correctly typed callback and packet bytes. Mock/header qualification supplies
positive packet evidence; no physical RF transmission, DMA/RDMA or DSP claim.

Excluded: AMSVitaMedium/Large/ExtraLarge, packet decoding or full class-ID
interpretation, host-order samples, VITA generation/transmission, new metadata,
PDW/LF receive, RDMA/external memory, WaveformTxEndpoint/CachedWaveform,
Weights/DSP, public Python/safe Rust, vendor modifications and unrelated
threading/lifetime redesign. Publication stops at independent review: OPEN,
NON-DRAFT, UNMERGED, auto-merge disabled. Local qualification is not approval.
