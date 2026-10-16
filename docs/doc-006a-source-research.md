# DOC-006A — ProductRx source reconciliation

## Scope and provenance

Continuation of the incomplete DOC-006 investigation, restricted to
`AMS.MEL.RF.Product_Rx`. Current bridge source at selected main
`c7f81919f125048ca81662d7d9534b4708e0e331` is authoritative over historical
implementation reports. No executable source or provider behavior is changed.

Current provenance in `upstream-provenance.md` identifies RF MEL
`762ce84c5555dd0f3ea66f36b321fecf8839b89f`, Common MEL
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, and AMS VITA
`8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8`. Historical Squall evidence is
restricted to `b1015728f904c799fa0c07489fce48e78f67845f`; it does not qualify
present hardware or another provider. The complete 804-file checksum inventory
is retained and checked without modifying vendored declarations. Eight substantive
declaration roots were independently fetched from the immutable upstream commits
and byte-matched; URLs and SHA-256 values are in external
`pinned-upstream-verification.json`.

## Source-evidence matrix

Paths below are repository-relative. RF declarations are under
`native/vendor/rf-mel/include/rfmel/`, Common under
`native/vendor/common-mel/include/mel/`, and VITA under
`native/vendor/ams-vita/include/ams/iface/vita/` at the revisions above.

| Group | Pinned declarations | Current native implementation | Safe Ada implementation | Regression evidence and limitations |
| --- | --- | --- | --- | --- |
| A1 configuration and submit | RF `data/DataMEL.h`, `rfmeltypes/JobDataFormat.h`; Common `library/Error.h`, `library/ErrorOr.h` | `native/src/rf_product_rx.cpp`: submit_product_rx, settle, run_create_worker, request wait/claim/close | `ada/src/ams-mel-rf-product_rx.adb`: constructors, Submit, Wait, Claim, result accessors | `native/tests/test_rf_product_rx.c`: configuration, future, rejection, malformed result, abandonment and unique-claim cases; `ada/tests/src/ams_mel_rf_product_rx_tests.adb`: safe request/format/error contracts. Creation is not sample delivery. |
| A1 parent and callback ownership | RF `data/ProductRxEndpoint.h`: non-const callback lvalue registration, no unregister contract | `native/src/internal/rf_product_rx.hpp`, `native/src/rf_data.cpp`: RfChildClaim and deferred shutdown; ProductRx PermanentRxRegistration and logical_close_and_drain | Limited controlled request/endpoint owners, Close and non-raising Finalize | Native mid-callback-close, registration-throw-active, move/reference callable, late-callback, parent-first and negative-old-close-order cases. No provider-wide quiescence or DSO-unload inference. |
| A2 samples and queue | RF `rfmeltypes/MELComplex.h`, `rfmeltypes/JobDataFormat.h`, `data/ProductRxEndpoint.h` | select_source, build_event, on_data_ready, receive, counter snapshot and event close | Receive checked active/inactive spans; With/Copy sample families, representation checks and Event cleanup | Native I8/direct contract probes, C payload/overflow/allocation/saturation cases; safe Ada wrong-format/empty/borrow/copy/lifetime cases. Component fidelity is not upstream C++ object-layout compatibility, DSP or physical reception. |
| A3 packets | RF `rfmeltypes/JobDataFormat.h`; VITA `FixedDataPacket.h`, `DataPacket.h` | select_packet_source and copy_packets validate view before declared length, copy actual bytes, publish only complete events | Four packet count/borrow/copy families check format/index/byte span; native v9 view nests prior payload profiles | C packet campaigns and real-header fixtures; Ada all-width byte fidelity, wrong-format/empty/index/parent-first tests; historical F20–F23 header evidence. Not positive Squall VITA reception or universal VITA semantics. |
| A4 ordinary metadata | RF `rfmeltypes/ProductRxMetadata.h`; Math UTCTime declaration reached from that header | build_event copies IDs, coherence, first-event time and ordered stream vector | Receive copies into Product_Metadata; read-only accessors have no provider calls | C and Ada metadata overwrite/lifetime, identifier/order and view-validation tests. Provider IDs are not positions; stored observation is not physical provenance. |
| A4 associations | RF ProductRxMetadata optional vector getter; JobEventID is uint32 | v8/v9 opt-in and exact VITA packet cardinality; historical views reject nonempty unsupported metadata | v9 Submit/Receive, presence and extent checks, independent Ada association vector | C test_associations and association-close; Ada F24 campaign; C11 v8/v9 layouts. Absence and present-empty differ; repeated IDs are not validated referents. |
| A4 stabilization | RF ProductRxMetadata getStabPoints and five-alternative PointingType | copy_pointing, active UTC canonical checks, whole-event failure atomicity, v9 view | Copy_Stabilization reuses C2.Pointing and copies active fields into Ada values | C test_stabilization/stabilization-close; Ada F25 all-format campaign and boundary checks; C11 v9 probe. No frame conversion, physical validation or full ReceiveEvent reference mapping. |

The listed tests are existing regression sources, not newly added tests. Their
execution status for this exact documentation source is recorded in
`doc-006a-coverage.md`; historical successful runs do not substitute for the
required final-source regression.

## Reconciled semantics

- Formats 0/1 are signed direct 8/16-bit scalars; 2/3 are complex Cartesian
  signed 8/16-bit pairs; 4/5/6/7 are Small/Medium/Large/ExtraLarge packet objects.
  The pinned ComplexINT16 enum comment incorrectly says “8 bit” but its actual
  int16_t declaration and probes establish 16 bits. No extra enum alternatives
  such as PDW/LF are exposed by this receive slice.
- Safe Ada currently uses submit/view-v9 for every format. Historical submit
  domains remain v1={3}, v2={2,3}, v3={0..3}, v4={0..4}, v5={0..5},
  v6={0..6}, v7/v8/v9={0..7}. Metadata profile 9 is not format 9. F24's older
  account of v8 VITA routing is superseded by F25/current code.
- Constructors store values. Submit checks positive queue and maximum, native
  format domain and region size_t representability before provider creation.
  Zero region is not forbidden by the bridge. Provider interpretation remains
  separate. There is no new physical buffer-registration or capability claim.
- The completion worker consumes the future exactly once, caches its result,
  checks endpoint ID/assigned format before registration, and preserves the
  DataMEL child claim through pending/cached/claimed ownership. Wait timeout
  does not cancel. Failed result accessors retain their Failed preconditions.
- Malformed normal creation results can carry code None plus a diagnostic;
  provider exceptions and bridge failures are not successful numeric/result
  data. Closing an unclaimed request abandons the result, not the future.
- Registration retains the exact heap-stable std::function lvalue, state and
  DSO permanently. The shell does not retain endpoint/DataMEL/event payloads.
  Correction `5922bc3bb5cfe399817e0dab6fe98fcb25837a0e` fixes the historical
  destroy-before-drain order: logical close/discard/wakeup, admitted-body drain,
  endpoint destruction, then child release. Failure to establish safety retains
  the graph. Late callbacks cannot justify unloading provider code.
- Events own plain immutable native copies, not borrowed provider buffers.
  Receive transfers a single native event owner and copies small metadata to
  Ada. Borrowed access is scoped to Process and serialized against Event Close;
  explicit copies survive closure. Selected-format checks apply even at zero
  count. Direct scalars, complex pairs and packet objects are not byte counts.
- Packet storage capacities are 512/1024/8192/32768 bytes, corresponding to
  128/256/2048/8192 words. Pinned views validate before length access; declared
  serialized length is 8 words through capacity (32 bytes through capacity,
  multiple of four). Maximum payloads are 480/992/8160/32736 bytes. Raw bytes
  include prologue and trailer; no unused tail, decoding or endian conversion.
  Any invalid packet rejects the whole callback, with no partial publication.
- Associations preserve absent/present-empty/present-nonempty, order, duplicate
  IDs and all uint32 bits. Present VITA cardinality matches packet-object count;
  direct/complex accept only absent or present-empty. Actual ReceiveEvent
  referents and commanded event relationships are not validated.
- Stabilization observations preserve five active variants and original units.
  Only active ECEF/LLA time fractions require canonical 0 <= fs < 10^15.
  First receive-event time is a distinct legacy field copied verbatim without
  an added canonicality check. No normalization, coordinate transformation,
  finite-only policy or physical truth computation is introduced.
- Nonempty full ReceiveEvent vectors and user-defined std::any content remain
  rejected. Metadata observations do not create new Job commands.

## Historical versus current qualification

Tasks 033C/033D and 034F18–034F25 are retained unchanged. Their progressive
limitations are reconciled above, rather than copied as current scope claims.
F25 records historical positive Squall ComplexINT16 reception through v9, but
no nonempty real-provider association/stabilization observation. Mock/header
VITA positives do not establish positive Squall VITA reception. DOC-006A runs
provider-free regression only and claims no new real-provider/hardware result.
