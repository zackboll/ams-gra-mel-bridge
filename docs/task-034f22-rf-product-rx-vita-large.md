# Task 034F22 — AMS VITA Large ProductRx receive

## Checkout and contract

Starting main: `84a16f81c03b53d8772ca37faf900eda4916eb7c`, PR #96 merged.
Existing branch `feature/034f22-rf-product-rx-vita-large` and worktree
`/home/zboll/git/ams-mel-034f22` are reused. Evidence, failed attempts and
corrections remain in `/home/zboll/git/ams-mel-034f22-evidence`.
Main workflow 38491639584 was inspected once as queued, not failed.

Pinned RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Format 6 selects exactly variant alternative 6, `DataPacketLarge*`, alias of
`FixedDataPacket<8192>`. The object is 8,192 bytes / 2,048 words with mandatory
32-byte overhead and maximum 8,160-byte payload. Capacity is not serialized
length. Callback/configuration counts mean contiguous packet objects.

The actual-header probe passes GCC/Clang Debug and Release/NDEBUG with active
checks and builder-generated 32-, 1,028- and 8,192-byte packets. The intermediate
packet genuinely declares 257 words (249 payload words), crossing Medium's
1,024-byte capacity. Distinct IDs, counts, timestamps and trailer indicators
accompany patterned payloads; unused tails contain a nonzero sentinel.

Pinned view validation requires the seven-word prologue, signal-data type 1,
C-bit 1, TSI 3, TSF 2, trailer bit 1, Nd0/S zero, OUI 0xAAAAAA and accepted
Tx/Rx Comm Base Set class codes/version. Declared length must be at least eight
words and fit supplied storage before trailer indexing. Trailer bits outside
the sample-frame enable/indicator masks reject. Invalid header/class/trailer
and lengths up to the maximum 65,535-word header were tested against a guard
page. No invalid-view length/payload accessor is used. This is not universal
VITA semantic validation, payload interpretation or hardware qualification.

## Additive ABI and ownership

Exactly submit-product-rx-v6 and event-view-v6 are added under AMS_MEL_0.1.
V6 nests complete frozen v5 and adds a Large descriptor span. Submit domains
remain v1={3}, v2={2,3}, v3={0..3}, v4={0..4}, v5={0..5}, v6={0..6}.
Old views reject Large without changing output pointers. Historical formats
reproduce nested v5 with NULL/0 Large descriptors. Large retains format 6 even
with zero packets and has all integer, Small and Medium spans inactive.

Small/Medium/Large share exact typed selection and checked copying. Configured
maximum, descriptor/backing container limits, object extent, PTRDIFF_MAX,
address overflow and alignment are checked before provider storage is read.
The bridge cannot prove an arbitrary non-null pointer's actual allocation;
the pinned contiguous-object contract remains the provider's responsibility.

Only after isValid succeeds is the header-derived length accessed. Exactly
32..8,192 bytes from public bytes() are copied unchanged in packet/network
order. No object-layout memcpy, tail reads, endian conversion or decoding.
Malformed later packets or later-copy allocation failures publish no partial
event. Final backing vectors precede descriptor publication; immutable event
owners survive provider overwrite, later callbacks, endpoint and parent Close.
No hidden global cap or config ABI change is added. Applications/providers must
choose practical queue capacities and maximum packet counts: Large objects can
substantially increase event-owned memory and allocation pressure.

Safe Ada adds Large config/count/borrow/copy operations with Positive 1-based
indices, checked descriptor/byte spans and metadata copy-out. Borrowed addresses
are private and cleared on Close/Finalize. Independent Ada copies survive
Event Close. Wrong-format access rejects even empty events. Private Ada, Rust
sys and ctypes mirror v6; compiled C11 probes retain historical layout checks.

## Guarded mock and lifetime

Three actual Large objects require 24,576 writable bytes. The mock obtains
sysconf(_SC_PAGESIZE), checks rounding/extent arithmetic, rounds writable bytes
up to pages and maps one additional guard page. On the qualified 4,096-byte
page platform this is six writable pages plus a guard: 28,672 mapped bytes.
The endpoint tracks writable extent, constructs aligned typed Large objects,
and revokes every writable page only after admitted callbacks drain. Child
process probes directly require SIGSEGV at the guard and every revoked page;
the guard remains inaccessible. Historical Small six-object and Medium
three-object 4,096-byte mappings remain unchanged. Large emitter collections
are heap-owned; builder temporaries/payload arrays are bounded by 8,192 bytes
each and live only during synchronous fixture construction.

The single creation worker, unique Claim, bounded DROP-INCOMING queue,
saturating counters and parent-first lifetime machinery are reused. Dedicated
process-isolated Large cases exercise copy/reference/move, synchronous callback,
registration exceptions and active-callback failure, held-callback Close,
late inaccessible storage, abandonment, unclaimed destruction, blocked Receive,
saturation and worker failure. Permanent exact-function and provider DSO
retention remains mandatory. Callback drain is not provider quiescence and
never authorizes DSO unload. Inherited destructive controls remain active.

## Qualification status

Completed pristine baseline: GCC/Clang Debug/Release each 387/387; Python 141;
Rust 94 unit/integration plus four separately executed doctests. Alire library,
full Ada, formatting, direct-GPR, aggregate check and build isolation passed.
Production Release measured 214 exports; vendor SHA-256 matched 804/804.

Initial implementation qualification: native full suite 407/407; focused
packet/lifecycle and inherited Small/Medium cases 58/58. Large C fidelity runs
50 iterations. Public Ada directly exercises zero/one/three Large packets and
50 iterations with complete independently expected serialized bytes. Python
142 and Rust 95 unit/integration plus four separately executed doctests passed
the initial binding campaign. Final native GCC/Clang Debug/Release each pass **407/407** in
final-matrix-retry.log (exit zero). Focused C Large passes 50 iterations with
Release/NDEBUG checks active; full Ada smoke with GCC Release/mock backing
passes, including 50 public Large zero/one/three-packet iterations. Production
Release measures **216 AMS_MEL_0.1** exports, no test hooks; frozen header is
byte-identical after removing F22 and vendor SHA-256 remains **804/804**.
Final language/aggregate campaign exits zero: Python **142**, Rust **95**
unit/integration plus **four** separately executed doctests, cargo check,
Clippy -D warnings, rustfmt, Alire library/full Ada, formatting, direct-GPR
full smoke, aggregate make check and build isolation pass. The task-owned
command unsets provider-path overrides before entering Alire/direct-GPR.
Squall Job, Ada VA, C ProductRx and Ada ProductRx each pass **one iteration**
sequentially in final-squall.log (FINAL_SQUALL_COMPLETE, exit zero), using
task-owned resources and port bases 45000/46000/47000/48000. Publication is
recorded independently in the PR/handoff after remote verification.

Retained corrections include generated test nesting/routing, the intermediate
mock fixture's payload/seed and corresponding trailer location, and additive
test construction scripts. No warning suppression, vendor modification,
inherited test deletion or weakened lifecycle contract is used. Initial final
builds failed when /tmp filled; task-owned TMPDIR corrected compilation. A
Release campaign encountered missing fixture-log observations during temporary
filesystem pressure; affected tests and the complete matrix passed on rerun
without source changes. The original failure logs and exit statuses are kept.
The new document's missing final newline was corrected. Toolchains: GCC 14.2.0,
Clang 19.1.7, Rust/Cargo 1.98.1, Alire 2.1.1; runtime page size 4,096 bytes.

Pinned Squall: `b1015728f904c799fa0c07489fce48e78f67845f`.
Final-source Job, Ada VA, C ProductRx and Ada ProductRx controls pass. ComplexINT16 is the real-provider receive control. Squall Large
capability/fidelity is not established; positive Large evidence comes from
actual-header and mock-backed byte checks. Full 32-bit cross-language runtime
qualification is unavailable and is not claimed.

## Exclusions and review gate

AMSVitaExtraLarge, IQ/payload decoding, host-byte-order conversion, packet
generation/transmission APIs, metadata-schema expansion, PDW/LF receive,
RDMA/external registration, WaveformTxEndpoint/CachedWaveform, Weights/DSP/RF
emission, vendor changes, public Python/safe Rust and unrelated lifecycle
redesign are excluded. Publication must stop at an OPEN, NON-DRAFT, UNMERGED PR
with auto-merge disabled. Local qualification is not independent approval.
