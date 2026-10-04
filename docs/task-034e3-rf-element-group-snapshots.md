# Task 034E3 — RF element-group descriptor snapshots and safe Ada

## Baseline and scope

Fetched origin and verified PR #70 merged. Actual updated main was
`e5337584600205e83f4467000ab98a0f621bd37a`; clean worktree, no matching branch
or PR. Created `feature/034e3-rf-element-group-snapshots` from that revision.
Starting native suite measured 269/269. ABI stays 0.1; three additive exports
are planned over the original 169, without changing any frozen record.

This slice exposes provider-created read-only descriptors, not full
VirtualAperture, DataPipe or RF MEL implementation. Claim still queries only
getVAInstanceIDs/getElementGroupLabels/isSingleGroup; its stored accessors
remain immutable. E1 queries, E2 notification registration/removal, Job workers,
interval queues/versions and the pinned zero sentinel are unchanged.

## Exact pinned declarations

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f`, already vendored:

```cpp
ElementGroupDescriptorLookupMap VirtualAperture::getElementGroups() const;
// wrapper: unordered_map<ElementGroupLabel, shared_ptr<ElementGroupDescriptor>>
ElementGroupLabel ElementGroupDescriptor::getElementGroupLabel() const;
Mode ElementGroupDescriptor::getMode() const;
double ElementGroupDescriptor::getMaxRfBandwidth() const;
double ElementGroupDescriptor::getMaxSampleRate() const;
double ElementGroupDescriptor::getMaxDataRate() const;
DutyFactor ElementGroupDescriptor::getMaxDutyFactor() const;
std::map<DataPipeLabel, std::shared_ptr<DataPipe>>
    ElementGroupDescriptor::getDataPipes() const;
const DataPipeLabel DataPipe::getLabel() const;
std::set<EndpointID> DataPipe::getAssociatedEndpoints() const;
enum class Mode : uint8_t { RX = 0, TX = 1 };
// ElementGroupLabel/DataPipeLabel = std::string; DutyFactor = double;
// EndpointID = uint64_t.
```

All containers above are returned **by value**, not references. The GCC/Clang
actual-vendored-header probe checks exact member-pointer signatures, wrapper
const-iterator type, scalar aliases, enum underlying type and RX/TX values.
No vendor file or pin changes are needed. The existing closure gates remain.

## Additive C ABI

Exactly three operations:

- `ams_mel_rf_virtual_aperture_get_element_groups`
- `ams_mel_rf_element_group_snapshot_view`
- `ams_mel_rf_element_group_snapshot_close`

One opaque `ams_mel_rf_element_group_snapshot`, with versioned primitive records:

- `ams_mel_rf_element_group_snapshot_options_v1`: uint32 include_data_pipes,
  exactly 0 or 1; C requires a valid options pointer.
- `ams_mel_rf_data_pipe_info_v1`: lookup_label, label, canonical
  `ams_mel_u64_span_v1 associated_endpoint_ids`.
- `ams_mel_rf_data_pipe_info_span_v1`: const record pointer, size_t element count.
- `ams_mel_rf_element_group_descriptor_v1`: lookup_label, label, uint32 mode,
  four doubles, pipe-info span.
- `ams_mel_rf_element_group_descriptor_span_v1`: const pointer, element count.
- `ams_mel_rf_element_group_snapshot_v1`: uint32 data_pipes_included, descriptors.
- `ams_mel_rf_element_group_mode_t`: distinct uint32 with explicit
  `AMS_MEL_RF_ELEMENT_GROUP_MODE_RX=0`, `..._TX=1` constants.

Creation outputs are nonnull and initially null. Invalid arguments make no
provider call and never overwrite an owner. View requires a null-initialized
output pointer and preserves it on failure; View/Close allocate/call no provider.
Close consumes/nulls the handle and is null-idempotent. Diagnostics use existing
bounded UTF-8 helpers, without repeating live queries to obtain longer text.
Raw private Ada, Rust sys and private Python ctypes track every new declaration,
layout, signature and inventory entry; no safe Rust or public Python API added.

## Optionality, identity and ordering

False inclusion copies all six mandatory descriptor getter results, makes **zero**
descriptor getDataPipes or pipe calls, stores empty pipe spans and marks omitted
data. True calls descriptor getDataPipes once per occurrence and copies every
returned map entry using getLabel/getAssociatedEndpoints once each. Any opted-in
failure rejects the entire snapshot; there is no fallback or automatic capability
gate. False (not queried), true/empty (observed empty), and failure (no owner)
are distinct. An empty outer map still preserves the inclusion flag.

Outer unordered-map entries sort by **lookup key** in ascending lexicographic
unsigned UTF-8 byte order, with no locale, Unicode normalization or case change.
Insertion order is not preserved. Inner pipes retain ascending std::map key
traversal; endpoint sets retain ascending unsigned numeric order including 0,
the high bit and UINT64_MAX. Empty spans use `{NULL,0}`.

Map keys and separately returned labels are independently observable. Mismatches
are allowed and never overwritten. Neither string is provider-object identity.
Shared descriptor/pipe aliases preserve every map entry; getters run once per
**represented occurrence**, not once per shared_ptr identity. Sorting performs
no provider getter. Each explicit snapshot makes one top-level getElementGroups
call; several subsequent getters mean no atomic multi-getter consistency guarantee.

All copied keys/labels must be complete UTF-8 without embedded NUL. Empty strings
are valid. Null descriptor, unknown mode, malformed string, or opted-in null pipe
rejects the whole result with PROVIDER_FAILED. Bad allocation from provider or
bridge gives INTERNAL_ERROR; other standard/unknown exceptions give
PROVIDER_EXCEPTION. Failure does not poison subsequent explicit queries.

## Numeric representation

The four doubles are copied without conversion or arithmetic:

| Field | Published unit |
|---|---|
| max_rf_bandwidth_hz | Hz |
| max_sample_rate_samples_per_second | samples/second |
| max_data_rate_bits_per_second | bits/second |
| max_duty_factor | dimensionless |

Upstream states duty factor `0 < value <= 1`. This descriptive API does not
clamp, repair, or silently normalize. Negative values, signed zero, infinity
and NaN are forwarding evidence, not physically valid capability claims.
NaN payload-bit preservation is not promised. RX and TX are normal descriptive
values; accepting TX does **not** broaden the single-RX-group Job profile.

## Provider-independent ownership and safe Ada

The existing live VA/C2 child claim protects the synchronous getter/copy operation.
Same-VA external serialization includes Close. No C2/request/notification lock
is held during provider calls. No Job sibling claim, worker, registration or
permanent library pin is acquired. All provider shared_ptrs stay local and are
released while that VA claim still protects provider code.

The published owner contains only bridge strings/vectors/public records. Outer
ordering precedes copying; backing vectors are final-sized and every nested string
and endpoint vector is complete before any address is published. No subsequent
sort, growth or string move can invalidate a view. Failed partial storage unwinds
without publishing. Snapshots work after public C2 Close, VA Close and actual DSO
unload. Fresh queries work after public C2 Close, but not after VA Close.

`AMS.MEL.RF.C2.Element_Groups` exposes private ordinary list/descriptor/pipe
values, Receive/Transmit, Positive indexing and typed accessors. The query reuses
Virtual_Aperture and defaults Include_Data_Pipes to False. Both the list and an
individual descriptor preserve Data_Pipes_Included. Counts mean stored entries;
check inclusion before treating zero as observed absence. Returned values contain
no native handle/address, Close operation or provider-aware finalizer.

A private limited controlled temporary closes the native owner on all copy-out
paths. One creation, View, checked copy, then native Close before return. Counts
must fit Natural, container Count_Type and storage-offset arithmetic; nonempty
pointers/alignment/end addresses, Boolean flags and modes are validated before
imported views/dereferences. Strings use explicit lengths, never strlen. C-double
size/precision checks gate Long_Float conversion. Private uint64 numeric storage
permits ordinary container copying of IEEE special values; float-validity treatment
is limited to numeric copy/access routines, not the package/project generally.
Native failures raise Provider_Error using fixed diagnostics without retries.

## Evidence and limits

Mock-positive RX/TX descriptors use three differently inserted keys ordered
`"", "z-map", "µ-map"`, independent returned labels and nontrivial fractional
numeric values. Nested pipes include Unicode/mismatched labels, empty sets and
`{0,0x8000000000000000,UINT64_MAX}`. Dedicated fixtures cover long strings, aliases,
empty collections, unknown mode/null objects, each malformed string category,
every getter's standard/unknown/allocation exception, partial descriptor/pipe/
endpoint allocation failure, later recovery, special numerics and changing values.
Both C and safe Ada drive these values, bounds, inclusion and parent-first lifetime.

The no-registration native process checks temporary object counts return to zero,
releases its own dlopen reference **before** VA/C2 destruction, observes actual
DSO unload, then reads nested snapshot contents and closes the snapshot. The E2
regression separately retains descriptors across VA teardown, closes snapshots,
and proves its intentional callable/DSO registration pin still exists. Callback
and ProductRx processes are never used as unload evidence. No descriptor query
runs in a notification callback.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` source was rechecked in
SquallC2MEL.cc and config/rf-simulated.toml. It creates one RX descriptor keyed
and labelled `"0"`, duty 1.0. rf_environment configured at 2,048,000 samples/second
yields bandwidth 2,048,000 Hz, maximum sample rate 2,048,000 samples/second and
data rate 65,536,000 bits/second. Opt-in returns one `"default"` map key/label
with no associated endpoints. The getter constructs a **fresh pipe**: read-only
returned-value evidence, not persistent routing/effective association. Source
capability acquisition can fail to an upstream all-zero fallback; integration
must assert the exact positive values, not accept that fallback.

Deferred: live descriptor/pipe owners, association/equality, VA-level getDataPipes,
descriptor-based commands, cached-waveform/dynamic-weight queries, standalone LF,
TX power calculations, broader TX/multi-group JobRequest, conditional commands,
weights/pointing/RDMA/VADB and additional ProductRx formats. No such expansion
is needed or started.

## Validation ledger

Executed local results (shared build-driving targets sequential):

| Command/audit | Actual result |
|---|---|
| `make test-native` | 270/270, GCC Debug, warning-clean |
| `make test-build-isolation` | Pass, production/test trees and failpoints isolated |
| `make format-ada`; `make check-ada-format` | Pass, pinned formatter |
| `alr -C ada build`; `alr -C ada/tests run` | Pass, full public Ada suite including isolated E3 |
| `make test-rust` | 73 ordinary tests and 4 compile-fail doctests pass |
| Rust workspace check/clippy (`-D warnings`)/fmt check | Pass with test-library environment scoped to Rust |
| `make test-python` | 117/117 plus compileall |
| `git diff --check`; final-newline gate | Pass |
| `make check` | Pass, direct-GPR GNAT 13.2.1/GPRbuild 22.0.1 on PATH |
| Fresh Clang 19 Release native suite | 270/270 |
| GCC/Clang actual-vendored-header probes | Exact E3 signatures/types, 712/713 pinned closure headers |
| GCC and Clang focused native repetitions | 50/50 isolated processes each |
| Safe Ada focused repetitions | 50/50 with GCC facade/provider, also 50/50 with Clang Release |
| Fresh production Release export audit | Measured 172, exactly three additions, no test-only exports |
| Runtime ABI version from that Release DSO | 0.1 |
| Full vendor SHA-256 audit | 804/804 unchanged; vendor/pin diff empty |
| Original API comparison | All 169 function declarations and 154 records unchanged |

Claim implementation is byte-identical to starting main. Native C2/Job/interval
status/VA signal implementation and E1/E2 safe Ada bodies are byte-identical.
The first Clang build caught a missing final newline in the new C test; adding
that newline resolved the actual warning. An initial Python binding-name typo
was corrected, then full Python validation found a second export-count inventory
assertion outside the ABI suite; it now correctly requires 172. Alire initially
rejected a globally set test-provider variable with a different path spelling;
scoping Rust-only environment variables resolved the invocation error. A
foreground build hit the tool's 30-second limit; the background build completed.
These first failure logs were preserved under `/tmp/ams-mel-034e3`. No legacy
Job-abandonment timeout recurred in completed local runs; no causal fix is claimed.

Pinned Squall Ada VA passed exact descriptor-only/full assertions, including
fresh post-C2-Close queries and retained post-VA-Close values. The first Job
target attempt stopped before execution on occupied port 21313. Inspection found
**no listener/container**, but a TCP TIME-WAIT connection on that same local port.
After waiting until bindability was verified, the unchanged Job target passed.
All four requested Squall targets completed successfully in sequence:
`make test-squall-rf-ada-va`, `make test-squall-rf-ada-job`,
`make test-squall-rf-rx`, `make test-squall-rf-ada` (one iteration each).
C ProductRx additionally retained native descriptors across public VA/C2 Close;
its original eight-event data/counter, Job/status timeout/stop and extension
assertions passed. Event A had 4096 nonzero ComplexINT16 samples; seven later
events differed, received/queued=8, dropped/malformed/allocation/after-close=0.
The remaining safe Ada RF receive target passed unchanged. Port bindability
checks before subsequent targets avoided the same observed TIME-WAIT condition;
this is invocation scheduling, not a library/provider causal fix.

The existing isolated ports and unrelated running container were preserved;
no readiness assertion or capability value was weakened.

Command results and final production export/vendor audits are recorded only after
execution. Shared build-driving targets run sequentially; Alire can recreate the
test tree without native executables, so CTest repetitions require a rebuild or
independent tree. Not Run is never counted as passing. Hosted push/PR revisions
and the open unmerged review state are recorded separately from local evidence.
