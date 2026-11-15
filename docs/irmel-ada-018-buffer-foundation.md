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
no native/Ada Buffer overlap. They were reconciled by ordinary merge commit
`eb73f549032751c5149400efb8d397f8def8d463`, not history rewriting. The
finalization-comment follow-up is `09f773d8d01ed68ab4b21d68ad4bbfffb1c34e33`.

### Completed local commands

Commands execute from the repository root in the pinned Alire GNAT 16.1.0 /
GPRbuild 26.0.1 environment (`alr -C ada/tests exec -- ...` with an explicit
return to the repository root). No native/Ada builds sharing artifacts overlap.

| Command | Result |
|---|---|
| `make format-ada` | passed |
| `make check-ada-format` | passed |
| `make test-native` | 658/658 passed, active checks with `-Werror` |
| `make test-python` | 586 passed; includes native prerequisite and bytecode compilation |
| `make test-rust` | 510 passed, 9 existing ignored opt-in cases; includes 40 doctests and native prerequisite |
| `make test-ada` | complete smoke suite passed, including standalone Buffer |
| `make check-ir-api-coverage` | 66 regression controls passed |
| `make check` | passed, including both production/test tree isolation directions, repeated 658-test suite, formatting, Ada and whitespace/newlines |
| `alr -C ada build` | passed |
| `alr -C ada/tests run` | complete smoke suite passed, including standalone Buffer |
| `make check-ada-docs-changed` | 16 public declarations; no annotation gaps |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | passed on merged main |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | passed on merged main |
| `RUSTDOCFLAGS='-D warnings' cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | passed on merged main |
| `ada/tests/bin/ams_mel_ir_buffer_example .../libmock_ir_provider.so foundation-normal` | passed; provider size 64, owned bytes/flags verified after parent Close |

The sequence above ran in exactly the requested order; main integration happened
before `make check`/Alire qualification. A complete sequence is also rerun after
integration so Rust's newly merged consumer/documentation tests are included.

GNATdoc current-source generation used the pinned 26.0.0 generator through
`docs/tools/docs_ci.py build`, on clean committed source at `09f773d...`.
Buffer public documentation is **55/55 (100%)**, with **zero Buffer warnings**.
Whole-tree informational coverage is 5071/5103 (99.3729%), with 124 existing
whole-tree diagnostics preserved. The initial two private Finalize diagnostics
were fixed with trailing documentation, without changing warning policy.
Both pristine and enhanced sites and source-bound manifests were generated;
no documentation deployment was attempted.

Release qualification used a separate `/tmp/irmel-ada-018-release` CMake tree:

```sh
CC=/usr/bin/gcc CXX=/usr/bin/g++ cmake -S native -B /tmp/irmel-ada-018-release \
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/irmel-ada-018-release --parallel 2 \
  --target test_ir_buffer test_ir_buffer_allocation test_ir_stream
ctest --test-dir /tmp/irmel-ada-018-release --output-on-failure \
  -R 'ir_buffer|ir_stream_contract'
```

All **3/3 Release tests passed**, including allocation injection and the existing
Mono8 contract. Every first-party target uses warnings as errors, and C test
CHECKs remain active with NDEBUG. The initial Release regex selected the two
Buffer tests only; the corrected regex above additionally ran Mono8.

### Final integrated qualification

The complete post-integration sequence passed all eight Make targets in order:
formatting, formatting check, 658 native tests, 586 Python tests, Rust workspace,
Ada smoke suite, 66 coverage controls, and `make check` (including both build-tree
isolation directions and repeated native/Ada runs). This rerun included the
private Finalize annotation and main's Rust IR consumer/documentation changes;
Rust then had 528 passing tests, 48 doctests included, and 10 existing opt-in
ignored cases.

Main subsequently advanced with PR #209's Rust RF documentation only, to
`99d7f11bc3e5b19c10c6d9daf0e7fe1c89b6fd41`. Ordinary merge
`a8a4e62e4dc879c0c319ea2729ac8f3a16b735c1` incorporated it without rewriting
history. No native/Ada/Python implementation or test changed in that merge.
The affected Rust formatting, complete tests, Clippy and rustdoc were rerun and
passed: **530 tests**, including **50 doctests**, with 10 existing opt-in ignored
cases. Native was rebuilt and rerun first: **658/658 passed**.

One direct Rust attempt after the Ada provider-only rebuild found that the
missing-symbol RF fixture DSO was absent, so its missing-factory test correctly
reported LibraryLoadFailed instead of SymbolNotFound. Rebuilding the **complete**
native test target restored the fixture and the unchanged Rust tests passed.
No contract or test was weakened to accommodate that setup failure.

Source-bound documentation generation also passed on `a8a4e62...` and its tree
`046ba384606b262d3bdb93c422e98449d1df5b43`: Buffer 55/55 public elements,
zero Buffer warnings, whole-tree informational 5071/5103 with 124 diagnostics.
The final report-only commit does not change compiled implementation or tests.
Strict C11, source integrity, annotation coverage, DSO/map equality at 323,
whitespace and final-newline checks were rechecked.

PR #210 remains open/non-draft, unmerged, with auto-merge disabled and no
self-approval. Hosted results must be read against the exact final pushed HEAD;
prior-HEAD successes are not final-HEAD qualification. Final check URLs/status
are recorded in the PR delivery comment and response. Pending checks are
explicitly reported as `HOSTED_CI_PENDING`.

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

## IRMEL-ADA-018-R1 — preserve caller-owned output handles

Starting HEAD: `691aee1c38f5c60bd62ec5182274ead90be8f2b5`. Open previously
cleared the output slot before validation, losing the caller's reference when
it contained a live Buffer. The correction removes that write and checks both
`output != nullptr` and `*output == nullptr` before owner/storage allocation,
factory resolution, `getBuffer`, or `init`. The sole owner publication remains
`*output = owner.release()` after successful initialization. Every failure leaves
the caller's slot unchanged: an initially null slot stays null, and a live slot
keeps its original owner. The independent provider Return sentinel remains
`UINT32_MAX` until an upstream Return is actually obtained.

The compiled-C `output_slot_preservation` regression in `ir_buffer_foundation`
uses a real owner, rejects reopen with valid/invalid sizes and a null Return
pointer, and checks pointer identity, properties, stable address classification,
and stored bytes after every rejection. Existing mock logs require exactly one
factory call, initialization and destruction, then normal Control/manager/DSO
cleanup. Null-output validation and successful initially-null Open are covered;
the existing factory-failure cases still require null slots and the sentinel.
No mock instrumentation or cleanup/retention behavior changed.

Red/green evidence: `ctest --test-dir native/build-tests -R
'^ir_buffer_foundation$' --output-on-failure` failed against the original
implementation at the live-slot reopen check (exit 8), then passed **1/1** with
the correction. Before main reconciliation, `make test-native`,
`make test-python`, and `make test-rust` passed (658 native, 586 Python, 530 Rust
including 50 doctests; 10 existing opt-in ignores). The final combined-tree
qualification and exact pushed-HEAD hosted status are recorded below and in the
PR delivery comment; no superseded-HEAD CI is treated as corrective evidence.

Private Ada Open initializes its owner to null; Python/Rust declarations and
surrounding failure tests do not assume clearing a live slot. No public Ada API,
C signature/layout, export, or coverage classification changed. Only the
reviewed implementation digest and its checker anchor are refreshed for the
corrected native bytes; the coverage report remains unchanged at 57 safe-Ada
operations. Real-provider qualification remains outside this correction.
