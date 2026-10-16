# DOC-006B source research

Selected main: `36ad420380ca0d717242681be7b60e7ae1ccfe35`, merged PR #108.
Reviewed DOC-006A head `2daa3eecb273018d3ccab3728ace570ac20e96db` is an
ancestor. This task adds comments only to seven RF C2 child specifications.
It does not implement new RF functionality or qualify hardware.

## Provenance and recovery

The repository's `docs/upstream-provenance.md` and
`docs/upstream-files.sha256.md` establish RF MEL
`762ce84c5555dd0f3ea66f36b321fecf8839b89f` and Common MEL
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`. The external evidence directory
`/home/zboll/git/ams-mel-doc-006b-evidence/r1` retains fresh upstream fetches'
URL/revision/path/hash comparisons in `pinned-upstream-verification.json`.
Twelve pertinent published headers were fetched at their exact pins and compared
byte-for-byte to vendored headers. No vendor source was changed. The full
804-file checksum audit is separate from these focused upstream comparisons.

The DOC-006 recovery record and its original 006B/006C/006D identity selections
were reused as DOC-006B checkpoints B1/B2/B3. Fresh baseline generation and set
comparison confirmed complete, disjoint 119/134/109 selections still present
after DOC-006A. No historical ProductRx identities are substituted.

## Contract evidence matrix

All paths below are repository-relative. The named Ada bodies are current
implementations, not inferred from historical reports. Public C declarations
and ownership comments are in `native/include/ams_mel/abi.h` (interval observers
2982–3054; VA signals 3562–3609; power/LF queries 3669–3755;
descriptor/connection snapshots and associations 4725–4822).

| Package/contracts | Pinned declaration | Current native and safe Ada implementation | Existing regression sources |
|---|---|---|---|
| Interval_Status: registration, queue, copies, enums, counters, time and binary IDs | RF `c2/JobDetail.h`, `c2/JobIntervalStatus.h`, `rfmeltypes/RFMELTypes.h` | `native/src/rf_job.cpp` registration/Job stop; `rf_job_interval_status.cpp` enum checks, by-value callback, bounded FIFO, counter saturation and copy; `internal/rf_job_interval_status.hpp` permanent holder; `ada/src/ams-mel-rf-c2-interval_status.adb` copy/validation/cleanup | `native/tests/test_rf_job_interval_status.c`, `ada/tests/src/ams_mel_rf_interval_status.adb` |
| Virtual_Aperture_Notifications: signal, coalescing, removal, permanent shell | RF `virtualaperture/BaseVirtualAperture.h` add/remove callback | `native/src/rf_va_status.cpp`, `internal/rf_va_status.hpp`; `rf_virtual_aperture.cpp` registration, matching VA removal and Close; `ada/src/ams-mel-rf-c2-virtual_aperture_notifications.adb` | `native/tests/test_rf_va_notifications.c`, `ada/tests/src/ams_mel_rf_va_notifications.adb` |
| Virtual_Aperture_Queries: required scalar/vector/report calls and required capability Booleans | RF `virtualaperture/BaseVirtualAperture.h`, `VirtualApertureInstanceStatusReport.h`, `c2/VirtualAperture.h`, `rfmeltypes/RFMELTypes.h` | `native/src/rf_virtual_aperture.cpp` query helpers and report conversion; `ada/src/ams-mel-rf-c2-virtual_aperture_queries.adb` checked copy and enum decoding | `native/tests/test_rf_va_queries.c`, `test_rf_local_functions.c`; `ada/tests/src/ams_mel_rf_va_queries.adb`, `ams_mel_rf_local_functions.adb` |
| Local_Functions: conditional catalog/status, ordering and independent snapshots | RF `c2/VirtualAperture.h` getLocalFunctions/getLocalFunctionStatus | `native/src/rf_virtual_aperture.cpp` map/vector copy; `ada/src/ams-mel-rf-c2-local_functions.adb` temporary owners, exact counts and reused status enum | `native/tests/test_rf_local_functions.c`, `ada/tests/src/ams_mel_rf_local_functions.adb` |
| Transmit_Power: four independent conditional methods, IDs, units and IEEE semantics | RF `c2/VirtualAperture.h` four RequiredIfTransmit declarations; `rfmeltypes/RFMELTypes.h` aliases | `native/src/rf_virtual_aperture.cpp` query_tx size_t bounds and exact forwarding; `ada/src/ams-mel-rf-c2-transmit_power.adb` double conversions and errors | `native/tests/test_rf_transmit_power.c`, `rf_transmit_power_abi_probe.h`; `ada/tests/src/ams_mel_rf_transmit_power.adb` |
| Element_Groups: non-atomic multi-getter copy, lookup/display distinction, optional nested pipes and numeric units | RF `c2/VirtualAperture.h`, `ElementGroupDescriptor.h`, `jobs/DataPipe.h` | `native/src/rf_virtual_aperture.cpp` unsigned-byte outer sorting, mode/text validation, snapshot ownership; `ada/src/ams-mel-rf-c2-element_groups.adb` copied values and numeric storage | `native/tests/test_rf_element_groups.c`, `ada/tests/src/ams_mel_rf_element_groups.adb` |
| Data_Pipes: VA-level snapshot, nested ordering, exact associations, input validation | RF `c2/VirtualAperture.h`, `ElementGroupConnections.h`, `jobs/DataPipe.h` | `native/src/rf_virtual_aperture.cpp` snapshot/mutate_data_pipe helpers; `ada/src/ams-mel-rf-c2-data_pipes.adb` NUL rejection, set input and checked copy | `native/tests/test_rf_data_pipes.c`, `ada/tests/src/ams_mel_rf_data_pipes.adb` |

## Source-backed distinctions

### B1: asynchronous observers

Job status registration returns void upstream. Normal bridge Open means normal
registration return, not delivery acknowledgement. The by-value payload permits
logical stop without retaining a JobDetail/VA/C2 graph or waiting for provider
callback quiescence. The exact callable, bridge state and DSO remain permanent.
The queue is fixed-capacity DROP-INCOMING; log/activity limits bound queued
copies, not provider argument construction or concurrent temporary payloads.
Unknown enums, oversize data and allocation failures discard whole observations;
known failure statuses remain data. The eight counters saturate. Receive zero
polls; stop wins over queued data; timeout is neither cancellation nor closure.
Ada events own copied data, ascending event-ID map order (not time order), exact
separate signed seconds/femtoseconds, and arbitrary binary activity bytes.

VA notification callbacks have a fundamentally different reference argument.
The current lambda leaves it unnamed and never dereferences it or queries state.
The permanent signal/callable/DSO shell holds no VA/C2 owner. Registration is
one-shot after exposure and permits synchronous callback entry. Boolean pending
coalesces entries; a successful wait consumes it and increments delivery.
Stopped takes priority and clears pending. Local Close never calls remove;
Unsubscribe uses the matching live VA and exact key once, caches failure as well
as success, and does not promise quiescence. VA Close has its own cleanup path.
Register/query/wait/query is the recommended sequence, not an atomic observation
or proof that state changed. Owner destruction must not race owner use.

### B2: queries and calculations

BaseVA queries and cached-waveform/dynamic-weight Boolean queries are required
upstream. LF-specific and TX calculation methods are conditional, respectively
RequiredIfLFSupport and RequiredIfTransmit. The bridge does not auto-gate them.
Each explicit query is fresh; independent calls may disagree. Failed/Degraded
statuses, False capability values and empty collections are successful data,
distinct from Provider_Error. Unknown enums reject entire results.

Instance vectors preserve provider order/duplicates. Reports copy the returned
identity even when it differs from the request. LF maps ascend type-ID keys;
vectors preserve provider LF index order, with Ada position 1 corresponding to
provider index 0. Catalog zero counts remain. No implicit catalog/status count
or membership reconciliation occurs. Copied snapshots retain no provider owner
and survive teardown. Query results do not establish LF execution/register writes.

TX methods forward numeric group/weight IDs only after native size_t bounds
checks. Weight type is not a Weights owner; mode and instance IDs retain their
separate meaning. Frequency is Hz, powers dBW, gain/attenuation dB; U/V are passed
as AnglePair without normalization. There is no RF formula, interpolation,
finite/range gate, cache or cross-result consistency inference. Signed zero,
infinity and NaN classification are preserved semantically, not NaN payload bits.
The existing VA claim protects calls after public C2 Close; same-VA operations,
including Close, need external serialization. Numeric return is not RF emission,
antenna performance or valid hardware-state evidence.

### B3: copied descriptors and connections

Descriptor acquisition obtains several provider values, not an atomic multi-getter
transaction. Lookup keys differ from display labels. Outer keys sort by unsigned
UTF-8 bytes without normalization/locale. Inner pipe std::map traversal and
endpoint std::set order survive. Included=False means unqueried, not absent or
unsupported; included-empty is distinct. Numeric limits retain upstream units
without clipping/repair/physical validity. Duty factor's published (0,1] interval
is descriptive, not locally enforced. Copied Ada getters perform no provider calls.

VA-level connections are separate from descriptor pipes. Every association
obtains a fresh getDataPipes result and selects existing exact keys without
insertion. Ada rejects embedded NUL; native code validates UTF-8. Endpoint IDs
preserve unsigned 64 bits; multi-input forms a set with duplicate collapse and
valid empty-set forwarding. The selected mutation is invoked once; False and
True are provider data, with no retry/readback. Acceptance is not proof of
connectivity, RDMA/Q-pairs, writable memory registration, hardware routing,
persistence or physical delivery.

## Provider-specific evidence and discrepancies

Read-only Squall source was checked at
`b1015728f904c799fa0c07489fce48e78f67845f` in
`/home/zboll/git/squall`. `SquallC2MEL.h` Job callback registration is a no-op;
its getter supplies dummy state. `SquallC2MEL.cc` stores/removes VA callables,
returns False capability flags, empty LF collections and four TX zeros. It
constructs fresh DataPipe objects whose local endpoint-set mutations return True;
a subsequent fresh snapshot need not retain associations. RX descriptor getters
reflect that implementation's capabilities. These are source observations, not
new execution or hardware qualification.

Historical reports `task-034d2` and `task-034e1` through `task-034e6` were read
as context and reconciled with current source. Their Squall execution is historical
call-path/returned-value/lifecycle evidence, including no positive observer
delivery; it is not promoted to this task's new evidence. Mock tests establish
bridge forwarding, ordering, errors, saturation, retained code and copy lifetime,
not universal provider behavior. No upstream callback quiescence, physical TX
meaning or persistent connection guarantee was found; none is invented.

GNATdoc's coverage comparison removes markup characters when comparing source
prose. New prose avoids underscore-bearing identifiers where that would distort
attachment matching; parameter/enum/field tag names retain exact Ada spelling.
Inherited comments remain byte-preserved. Attachment fixes use blank separators,
not renderer exceptions or tooling changes. Numerical coverage alone is not an
independent proof of semantic correctness.

## DOC-006B-R1 source correction

BaseVirtualAperture.h:30 declares getID returning VirtualApertureDefinitionID.
The native get_id implementation at rf_virtual_aperture.cpp:395–403 forwards
that value without status decoding. Ada Query_ID at lines 78–84 checks the
call result and returns the unsigned 32-bit ID, without calling Decode. The
previous description incorrectly generalized status-query behavior to this ID
query. Its three-line description is corrected; all other API prose is retained.
