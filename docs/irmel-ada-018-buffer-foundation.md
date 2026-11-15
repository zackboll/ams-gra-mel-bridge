# IRMEL-ADA-018 — application-owned host Buffer foundation

## Scope and base

Prepared from freshly fetched `main`,
`417f4008ff480c68ce8a63e887e70e989dc744b9`, on
`feature/irmel-ada-018-buffer-foundation`. The supplied preparation reference
was `8892834fb41f3929083df898e472e20160212f79`; main advanced by merging
DOC-ADA-001 / PR #206. This feature did not stack on or change that branch.
Preparation checked open PRs and remote branches: no concurrent standalone
Buffer foundation was found. Open PRs #204 (communications callback) and #205
(Rust reference consumer) were separate work.

This is a standalone, **unregistered provider Buffer with bridge-owned host
storage**, not the complete upstream application-managed Buffer contract.
It adds safe `AMS.MEL.IR.Buffer`, private Python ctypes and raw Rust sys
mirrors; it does not add public safe Python or Rust Buffer owners. The existing
Mono8 implementation and its pool/release behavior are not modified.

## Native ownership and lifetime

`native/src/ir_buffer.cpp` reuses the existing `SessionState` and
`SharedLibrary::symbol` mechanism. The factory receives the Session's actual
instance and API manager. The opaque owner retains, in destruction order:

1. The provider `shared_ptr<irmel::Buffer>`.
2. The bridge's fixed, zero-initialized host byte vector.
3. The complete Session graph, including Control, API manager and provider DSO.

Creation validates nonzero uint64 size against signed int64, native size_t and
the allocation container's maximum, allocates storage once, calls `getBuffer`,
rejects null, and calls `Buffer::init` exactly once. The original address, byte
count and entire signed context are supplied. Publication occurs only after
Success. No long-term Ada address is passed to the provider. The storage vector
is never resized after allocation; moving ownership of its containing pointer
does not relocate its allocation.

Operations on a live owner are mutex-serialized. As with existing bridge
owners, callers must externally serialize handle lifetime and Close with all
uses: a raw C pointer is not a concurrent handle-admission protocol. No channel,
listener, registration, checkout or asynchronous adapter operation is created.

Ordinary Close destroys the sole provider shared owner before host storage,
then releases the Session graph and clears the handle. Repeated Close succeeds.
An unexpected additional provider shared owner makes Close fail while retaining
the native owner and graph for retry. A failed init with additional shared
ownership also retains the graph. An init exception conservatively retains the
entire allocated native owner, because init could have retained its address
before throwing. These exceptional retention paths deliberately trade resources
for lifetime safety and may persist until process exit. No general recovery or
cleanup claim is made. Ada finalization calls the same close operation and never
raises; clearing or losing an Ada handle is not proof of provider cleanup.
Provider destructors/deleters must obey the C++ non-throwing destruction contract.

## Exact methods, values and byte semantics

The new implementation calls only `getBuffer`, `Buffer::init`,
`getBufferAddress`, `getImageAddress`, `getSize`, `getContext`, `getFlags`,
`setFlags`, `addFlag`, and ordinary provider shared-owner destruction.

`init` and `getFlags` publish the actual upstream uint32 Return, separately from
bridge status. `UINT32_MAX` means no Return was obtained. Non-Success is distinct
from an empty flag list. Void `setFlags`/`addFlag` have no invented upstream
Return; bridge OK means the call returned normally. Allocation exceptions map
to INTERNAL_ERROR; other provider exceptions are caught at the C boundary.

Flags map exactly to Host=0, GPU=1, RDMA=2. Retrieval copies a fresh vector into
caller storage; count/copy uses capacity checking, without partial publication.
Replacement and append retain provider order and duplicates; empty replacement
is valid. Unknown provider values fail rather than being reinterpreted. The Ada
`Flags` function owns its returned array. Its count/copy pair can fail if an
independently mutating provider grows the vector; it never borrows vector memory.
GPU/RDMA bookkeeping flags **do not** allocate physical GPU or RDMA memory.

Info preserves both initialized allocation/context and the actual provider
signed size/context, even if the provider reports different values. Safe Ada
exposes only buffer-address equality and null/inside/outside image-start
classification. The latter is not validation of an image span or layout. No
provider pointer is dereferenced, no image offset/view is exposed, and host byte
operations always use the original known allocation, never a provider address.

Read/write spans are checked with subtraction after validating offset <=
capacity, preventing addition overflow. Zero-length spans at capacity are valid.
Copies never resize storage. Ada uses the existing `Pixel_Array` octet convention
for storage copies (not a pixel-format claim), accepts arbitrary source lower
bounds, and returns independent one-based arrays. Invalid spans raise
Constraint_Error; closed owners and native/provider failures raise
`AMS.MEL.Provider_Error`. Full signed int64 contexts include negative values and
both extremes. Local Ada allocation may also raise ordinary Storage_Error.

## ABI and coverage delta

Eight additive `AMS_MEL_0.1` exports, 315 → **323**:

- `ams_mel_ir_buffer_open`
- `ams_mel_ir_buffer_get_info`
- `ams_mel_ir_buffer_get_flags`
- `ams_mel_ir_buffer_set_flags`
- `ams_mel_ir_buffer_add_flag`
- `ams_mel_ir_buffer_read_storage`
- `ams_mel_ir_buffer_write_storage`
- `ams_mel_ir_buffer_close`

Existing layouts/exports are preserved. The new `ams_mel_ir_buffer_info_v1` is
40 bytes with 8-byte alignment on the qualified LP64 platform. C11 probes and
private Python/Rust comparisons qualify all its fields and exact flag values.

The mutable mapping, reviewed binding ceiling and generated report record:

- Seven operations → safe_ada: factory, init, getSize, getContext, getFlags,
  setFlags, addFlag; unique safe-Ada operations 50 → 57 of 148 inventoried.
- BufferFlag missing → safe_ada, with all three alternatives represented.
- Buffer stays partial; getter representation evidence now identifies the
  compiled standalone owner, with address classifications rather than pointers.
- getBufferAddress/getImageAddress remain partial; existing Mono8 evidence is
  preserved alongside the standalone address inspections.
- Generic application-managed buffers missing → partial, not complete.
- General release stays partial with only its pre-existing frame-lease evidence.

Regression controls require the new native exports, public safe Ada bodies,
compiled C/Ada evidence, and unchanged registration/release boundaries.
`docs/coverage.md` is untouched.

## Qualification evidence

Initial native qualification: `make test-native`, 657/657 passed before adding
the allocation-failure executable. Initial offline coverage qualification:
`make check-ir-api-coverage`, 65/65 regression controls passed.
`make check-ada-docs-changed` inspected 16 public declarations with zero missing
or invalid annotations; PR #206 was available on the fetched base. All new
declarations use trailing structured comments, including every formal, return,
public record field and enum alternative.

Native qualification includes signed-context extremes, exact forwarding, null
and throwing factories, bridge allocation injection, init refusal/exception,
actual getter mismatches, null/non-null/outside image addresses, independent
fixed storage, zero-length and overflow bounds, all flags and duplicate order,
getFlags refusal/exception/unknown enum, void setter exceptions, parent-first
and child-first closure, repeated close, and deliberately retained ownership.
The mock Buffer destructor reads initialized storage before recording its
destruction; lifecycle logs check Buffer → Control → manager → DSO ordering.
The standalone mock has no pool and aborts if release is called. Existing
acquisition tests separately retain their strict checkout/release accounting.

Ada qualification exercises the public Session/owner, independent byte arrays,
ordered repeated flags, exceptions, post-parent-close operation, idempotent
closure and non-raising successful/uncertain finalization. The compiled safe
example is `ada/tests/src/ams_mel_ir_buffer_example.adb`:

```sh
ada/tests/bin/ams_mel_ir_buffer_example /absolute/provider.so instance
```

Qualification corrections retained for transparency: the first Ada compile
identified an unused import under warnings-as-errors and a wrongly qualified
exception name in tests; both were fixed, not suppressed. A direct GPR link
before rebuilding the production facade found the expected missing new symbols.
The added same-Session acquisition regression initially used the wrong expected
mock pixels (`1 + index`); it was corrected to the existing contract's
`16 + index`, without changing the provider/acquisition implementation. The
complete sequential qualification was restarted after that correction.

The corrected sequence has so far passed `make format-ada`,
`make check-ada-format`, `make test-native` (658/658), `make test-python`
(586 tests), and `make test-rust` (510 passing tests including 40 doctests,
9 existing ignored opt-in tests). The actual production and contract DSO
production-export sets each equal the 323-name map. Strict C11
`-pedantic-errors -Wall -Wextra -Werror` probes passed all eight signatures,
layout fields and flag values. Source-integrity checks passed for 825 vendored
files and four workflows. Documentation CI tests (18), static-subtype tests (9),
documentation coverage tests (13), annotation checker regressions (55),
source-integrity regressions (18), source-policy regressions (18), bootstrap
policy regressions (75), and coverage controls (66) passed.

Main advanced during qualification to
`8a2d409b53b90161bee01dcdd2f3641480d178d6` by merging PRs #205 and #207.
Those changes contain the Rust minimal consumer and Rust IR documentation, with
no native/Ada Buffer overlap. They will be reconciled by ordinary merge commit,
not history rewriting. Final integrated results follow after completion.

## Exclusions and unresolved provider semantics

No Channel::registerBuffer/unregisterBuffer, callback checkout/release,
standalone channel Disable, generic image acquisition, image-layout inference,
GPU/RDMA storage, DSP or service orchestration is implemented. A standalone
host Buffer is not an acquired frame and does not modify Mono8 behavior.

General `Buffer::release()` was deliberately deferred. Upstream describes it as
returning a delivered/checked-out Buffer for reuse. The existing acquisition
mock rejects release without checkout; the reference stub's Fail on an
undelivered frame is not generalized to all providers. A future registration
and frame-delivery slice must establish explicit checkout/lease ownership and
channel teardown rules before exposing release. This API cannot issue it.

Real-provider qualification has not been performed. Mock/provider C++ ABI
compatibility, not a C boundary alone, is necessary. Providers must honor their
published object/host-memory lifetime contract and not perform unsynchronized
access to standalone storage. No hardware, credentials, containers or build-time
downloads are required by ordinary qualification.
