# Task 034F23 — AMS VITA ExtraLarge ProductRx receive

## Source and scope

Starting main: `d34a3908bf048fd70c942031659d9473910aae08`.
PR #97 merged the reviewed predecessor head
`4f9338f8528bd07ab58eb29d8f6d330514dc69b6`. Main workflow 38498292581 completed
successfully; there were no intervening commits or open PRs at reconciliation.
The existing `/home/zboll/git/ams-mel-034f23` worktree and
`feature/034f23-rf-product-rx-vita-extra-large` branch are retained. Evidence,
failed attempts and corrections reside in its `build/034f23-evidence` directory;
TMPDIR is task-owned disk-backed `build/034f23-tmp`, not the pressured /tmp.

Pinned RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Pinned AMS VITA: `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8`.
Pinned Squall: `b1015728f904c799fa0c07489fce48e78f67845f`.
Vendor sources remain byte-identical.

## Pinned header contract

Format 7 selects exactly `DataPacketExtraLarge*`, alias `FixedDataPacket<32768>`.
Actual-header compiled probes establish 32,768-byte sizeof/byte capacity,
8,192-word capacity, mandatory 32-byte overhead and maximum 32,736-byte payload.
The builder generates genuine 32/8196/32768-byte packets declaring 8/2049/8192
words with 0/2041/8184 payload words. Distinct stream IDs, packet counts,
timestamps, trailer indicators and patterned payloads are independently checked;
unused tails are nonzero sentinels. The 8,196-byte packet exceeds Large capacity.

The pinned view validates required packet type/C-bit/TSI/TSF/trailer, forbidden
Nd0/S, OUI/class, storage-bounded declared length and accepted trailer bits
before exposing a valid view. Guard-backed invalid probes include fewer than
8 words, 8193 words and maximum 65535 words. No invalid-view length or payload
accessor is used. This is a pinned tailoring observation, not universal VITA
semantic compliance or physical hardware qualification.

## ABI and shared native implementation

Exactly submit-product-rx-v7 and event-view-v7 are added under AMS_MEL_0.1.
Event-v7 nests complete frozen v6 at offset zero and adds the ExtraLarge span.
Submit domains remain v1={3}, v2={2,3}, v3={0..3}, v4={0..4}, v5={0..5},
v6={0..6}, v7={0..7}. Unsupported formats reject before provider creation.
Every older view rejects format 7 without writing the output pointer. Historical
formats reproduce v6 with NULL/0 ExtraLarge span. Format 7 has only ExtraLarge
descriptors active; empty events retain format 7 and NULL/0 span.

Small/Medium/Large/ExtraLarge share `select_packet_source` and `copy_packets`.
Configured packet maximum, vector/descriptor capacities, native object count
multiplication, PTRDIFF_MAX, pointer extent and alignment are checked before
memory reads. A provider remains responsible for actual contiguous allocation
extent; arbitrary non-null addresses cannot be proved mapped by the bridge.
For every packet isValid precedes getPacketSize, followed by checked word/byte
bounds and exact copying from public bytes(). No native layout reinterpretation,
tail copy, endian conversion, payload decoding, rebuilding or DSP occurs.

All byte vectors finish before descriptor pointers are populated. Immutable,
heap-stable event owners survive callback return, source overwrite, later
callbacks, endpoint Close and parent DataMEL Close. Malformed later packets and
later-copy allocation failures publish no partial event; same-endpoint recovery
checks full byte fidelity. Queue policy/counters remain DROP-INCOMING/saturating.
Three full packets consume 98,304 owned bytes plus containers/descriptors.
Queue memory depends on caller packet-count and queue-capacity configuration;
there is no hidden global cap or independent bounded-total-memory claim.

## Guarded storage and lifetime

Large and ExtraLarge mappings derive page size from sysconf(_SC_PAGESIZE), use
checked object multiplication, page rounding and guard addition. Historical
Small six-object and Medium three-object mappings remain unchanged. ExtraLarge
constructs three aligned native objects in 98,304 writable bytes. On the measured
4096-byte-page host this is 24 writable pages plus one inaccessible guard:
102,400 mapped bytes. These values are observations, not universal assumptions.

The full writable extent is mprotected PROT_NONE only after admitted callback
copying drains; the guard remains inaccessible. Dedicated process-isolated
`rf-product-rx-extra-large-pages` faults each revoked writable page and the guard.
Ordinary lifecycle fixtures observe full-extent revocation and deterministic
held-callback drain ordering without repeating all 24 page faults. Inherited
Large exhaustive protection tests are unchanged. Late callbacks return before
accessing inaccessible metadata/packet storage.

Exact registered std::function, callback state and provider DSO remain retained
permanently, including copy/reference/move and registration-throw paths. Logical
Close precedes drain and endpoint destruction. The single creation worker,
completion cache, unique Claim, parent-first lifetime, abandonment, unclaimed
endpoints, blocked Receive wakeup and worker-launch failure remain unchanged.
Bridge callback drain is not provider-wide quiescence and does not permit DSO
unload. Inherited destructive negative controls remain active. F22's navigation
completion-marker synchronization correction is preserved.

## Safe Ada and raw binding parity

Public ExtraLarge config/count/borrow/copy operations use Packet_Byte_Array and
Positive 1-based indices. Submit/Receive route to v7 only for ExtraLarge; older
routes remain unchanged. Receive checks authoritative format, all inactive
spans, descriptor count/pointer consistency, representability, extent/alignment,
address arithmetic, metadata and stream IDs. Only the descriptor address is
borrowed privately. Open-event access checks index, byte pointer and validated
32..32768 divisible-by-four length before array import. Independent copies
survive Event Close. Borrowed state clears on Close/Finalize; limited-controlled
ownership remains intact. All wrong-format access rejects even empty events.

Copy scratch storage is bounded to one 32,768-byte array; mock fixture collections
and payload patterns are heap-owned. Tests' largest expected-byte collection is
bounded by 8192 words; no multiple ExtraLarge automatic packet arrays are added.
C11 signature/layout probes compare descriptor/span/event-v7 with Rust sys,
private ctypes and private Ada representation checks. On this 64-bit host:
descriptor/span size 16, alignment 8; event-v7 size 208, alignment 8, nested event
at 0 and ExtraLarge span at 192. All historical ABI checks remain active.

## Validation record

Pristine baseline is preserved: GCC/Clang Debug/Release 407/407 each, Python 142,
Rust 95 plus four separate doctests, Alire/full Ada, direct-GPR, formatting,
aggregate/build isolation, 216 exports and vendor 804/804. Baseline is not rerun
merely to regenerate evidence. Final-source results are recorded below after
completion; unfinished checks are not passes.

## Exclusions and qualification limits

No PDW/LF receive, VITA payload decoding, host-byte-order conversion, VITA
transmission, new metadata fields, DMA/RDMA/external memory registration,
WaveformTxEndpoint/CachedWaveform, Weights, DSP, RF emission, vendor changes,
public Python/safe Rust receive API or unrelated lifecycle refactoring.
Full 32-bit cross-language runtime qualification is unavailable and not claimed.
ComplexINT16 is the positive pinned Squall receive control; Squall ExtraLarge
capability/fidelity is not established. Positive ExtraLarge evidence is pinned
header and mock-backed byte checks, not real-provider or physical RF execution.
Publication stops at an OPEN, NON-DRAFT, UNMERGED PR with auto-merge disabled;
local qualification is not independent approval.

### Measured final-source provider-free results

GCC 14.2 and Clang 19.1 Debug/Release each pass **428/428** one-pass native
checks and all 428 tests repeated exactly 50 times with parallelism four:
**21,400 repeat executions per configuration**, **87,312** one-pass plus repeat
executions across the matrix. Focused ExtraLarge packet suites internally execute
50 scenarios with explicit checks active in Release/NDEBUG. All inherited
Small/Medium/Large and destructive lifetime negative controls remain active.
The dedicated ExtraLarge page fixture verifies 24 revoked writable pages and
the preserved guard on the measured 4096-byte host, in isolated children.

Python **143** tests and compile checks pass. Rust **96** unit/integration tests
plus **four separately executed doctests** pass; Cargo check, Clippy -D warnings
and rustfmt pass. Alire library/full Ada, GNATformat, aggregate make check,
build-tree isolation and direct-GPR full smoke pass using GPRbuild 26 through
Alire. Direct-GPR full smoke with GCC Release native/mock backing runs the public
ExtraLarge scenarios 50 times, including dedicated one-packet access and exact
32/8196/32768-byte borrow/copy fidelity and post-Close independent copies.
Production Release has **218 AMS_MEL_0.1 exports**, no test hooks; historical
header/constants are byte-identical after removing only the F23 additive block.
Vendor SHA-256 remains **804/804**. Whitespace/final-newline checks pass.

Failed implementation attempts are preserved: initial generated mock branch
syntax, an ExtraLarge test configuration erroneously set to format 8, private
Ada aggregate/layout declaration errors, and one remaining Python inventory
expectation of 216. Corrections restore exact typed branches/format 7/layouts
and update the inventory to the required 218; no assertion, warning policy or
inherited test was weakened. Full final campaigns exit zero. Earlier toolchain
PATH failures belong to the preserved baseline record and were not repeated.

### Final-source pinned Squall controls

The task-owned sequential Job, Ada VirtualAperture, C ProductRx and Ada
ProductRx sequence exits zero against Squall b1015728f904c799fa0c07489fce48e78f67845f.
Control/metrics/health/data ports use separate 51000/52000/53000/54000 bases.
Job and VA acceptance are control evidence, not payload/RDMA/hardware proof.
C and Ada ProductRx positively receive ComplexINT16 and preserve their existing
copy/lifetime controls. No correctly typed real-provider ExtraLarge callback is
claimed; all ExtraLarge byte fidelity remains mock/pinned-header evidence.
