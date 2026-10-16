# DOC-005C source research

## Selected base and provenance

PR #106 was merged as `4d4b07f83831ee524b5c91074ba84dd22070129d`.
Its reviewed head `ce470c267de9c45c7cb4c64b00e98c52bdd16fe5` is an
ancestor of fetched main. The previous verified main was
`8f4f0b50b0bc0b9db0ab46b99f4dcf238c4d6c08`. No equivalent DOC-005C
branch, PR or partial worktree existed. Work is isolated on
`docs/005c-rf-c2-events-waveforms-intervals` at
`/home/zboll/git/ams-mel-doc-005c`; all earlier worktrees/evidence are preserved.

RF MEL remains pinned to `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
All 57 vendored RF headers were compared byte-for-byte to immutable Git blobs
in the existing pinned checkout, not guessed filenames. All 804 manifest vendor
checksums verified. Common MEL remains
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, AMS Math remains
`00be45190f0e47d268cece8b8c2f8fb58b5418d2`. Historical Squall revision is
`b1015728f904c799fa0c07489fce48e78f67845f`; no new provider/hardware run.

## Source-evidence matrix

Pinned RF declarations below are relative to the absolute root
`/home/zboll/git/ams-mel-doc-005c/native/vendor/rf-mel/include/rfmel/`, all at
the RF revision above. Bridge and test paths are relative to
`/home/zboll/git/ams-mel-doc-005c/`. Citations identify stable operations rather
than comment-sensitive line numbers. All safe operations are implemented in
`ada/src/ams-mel-rf-c2.adb`, with raw v16 declarations privately in
`ada/src/ams-mel_c_api.ads`; none of these implementation files is changed.

| Claim family | Pinned declaration | Current native implementation | Safe Ada implementation | Existing evidence / limits |
| --- | --- | --- | --- | --- |
| Pulse defaults, enums, M/N, ordered thresholds | `jobs/PulseDetectionSettings.h`; `jobs/JobEvent.h`: ReceiveEvent | `native/src/rf_job.cpp`: pulse validation and ReceiveEvent preparation in `add_rx_intervals` | Create/Append/Set/Clear pulse settings; presence-gated threshold backing | `native/tests/test_rf_job_pulse_detection.c`; `ada/tests/src/ams_mel_rf_interval_spatial.adb`; no detector algorithm/performance |
| RX/TX defaults and common controls | `jobs/JobEvent.h`: JobEvent, ReceiveEvent, TransmitEvent; `rfmeltypes/RFMELTypes.h`: ExecutionType/StokesVector | Same template: typed RX-v4/TX-v3 access, double copies, Boolean/enumeration/count checks | Separate RX/TX constructors and overloads, at-most-two polarization append | `native/tests/test_rf_job_interval_event_controls.c`, `test_rf_job_tx_events.c`, `test_rf_job_tx_controls.c`; `ada/tests/src/ams_mel_rf_tx_events_focused.adb`; copied values, not beam steering/channelization/RF transmission |
| Stabilization and applicable indices | `jobs/Pointing.h`, `jobs/JobEvent.h`, `jobs/JobInterval.h` | `valid_pointing`, `prepare_pointing`, `fits_count`; no membership checks | Raw_Pointing, event index setters, Append_Stab_Point | `native/tests/test_rf_job_interval_spatial.c`; safe spatial suite; numeric provider indices, not Ada Positive positions |
| Modulation selection and references | `jobs/CommonModulations.h`: CommonModulations/Modulation; `jobs/JobEvent.h`: modIndex; `jobs/JobInterval.h`: modulations | `add_rx_intervals`: kind-specific typed variant construction, final same-interval reference validation | Tone/Phase_Code/LFM, owned table/references, explicit presence | `native/tests/test_rf_job_modulations.c`; safe TX suite; no registry/cross-interval lookup or synthesis |
| OOK/CPFSK | `jobs/CommonModulations.h`: OnOffKeying, CpfskPulse | Active bits 0/1; OOK length equals interval repeat count; one OOK entry cannot serve distinct TX events | Boolean arrays, kind-guarded append/replacement | `native/tests/test_rf_job_binary_modulations.c`; safe TX suite; signed CPFSK timings not physically validated |
| Custom profiles/patterns | `jobs/CommonModulations.h`: WaveformProfileParams, WaveformProfileIterations, EventWaveformProfilePattern, CustomWaveform | Active update flags, native int bounds and definition-local profile references | Profile setters preserve numeric values with False flags; copied tables/patterns; guarded replacement | `native/tests/test_rf_job_custom_waveform.c`; safe TX suite; degrees stay degrees, no timing summation or amplitude clamp |
| Buffer sample ownership | `jobs/CommonModulations.h`: WaveformBuffer; `rfmeltypes/RFMELTypes.h`: JobTransmitVector/MELComplex | Fresh shared vector per active definition per Add; selected signed width, max_size/overflow checks | I8/I16 records, arbitrary-lower-bound arrays, copied vectors, guarded append/replacement | `native/tests/test_rf_job_waveform_buffer.c`; safe TX suite; native copies share storage, not deep-copy or caller-storage aliases |
| Stream descriptor | `jobs/WaveformStream.h`; `rfmeltypes/RFMELTypes.h`: DataSampleFormat/DataItemFormat/BufferRepeatMode | Outer variant selected explicitly; six copied scalars, NumBytes checked against size_t | Waveform_Stream and guarded replacement; checked size_t conversion/round trip | `native/tests/test_rf_job_waveform_stream.c`; safe TX suite; numeric address never mapped/dereferenced, no endpoint ownership/transfer |
| Interval timing/defaults/presence | `jobs/JobInterval.h`: Sequence/JobInterval; Math `math/units/UTCTime.h` | Exact Fs counts; frozen typed profile traits; optional setters independently gated | Create_RX_Job_Interval, ordered RX/TX/list/point append, status and other controls | `native/tests/test_rf_job.c`, spatial/controls suites; no scheduler, quantization or event-overlap validation |
| Local Function commands | `jobs/LocalFunctionCommand.h`; `jobs/JobInterval.h`: setLfCommands | Ordered copied commands/writes; full uint32/uint64 domains, instance fits size_t | Create/Append writes/Append interval command, ordinary value copies | `native/tests/test_rf_job_interval_lf_commands.c`; safe spatial suite; no register writes/capability proof |
| Product destination descriptors | `jobs/ProductStreamParams.h`; `jobs/JobInterval.h`: setEndpointParameters | Ordered groups/endpoints; groups fit size_t, endpoint/address/maxBytes uint64 unchanged | Create/Append/Set/Clear copied parameters | `native/tests/test_rf_job_product_stream_params.c`; safe spatial suite; endpoint/JobRequest matching and safe writable memory remain application/provider obligations |
| Endpoint association map | `jobs/ElementGroupToEndpointConnections.h`; `jobs/JobInterval.h`: setEndpoints | UTF-8/NUL structural validation; nested insert_or_assign, last composite-key occurrence wins | NUL rejection, ordered owned triples, Set/Clear presence | `native/tests/test_rf_job_endpoint_map.c`; safe spatial suite; unordered native iteration, no membership/connectivity validation |
| Binary user context | `jobs/JobInterval.h`: setUserDefinedContextData, unsafe typed getter | Fresh vector<uint8_t> placed in std::any for each active interval, max_size/extent checks | Temporary prepared Set, append, present-empty, Clear; separate Activity backing | `native/tests/test_rf_job_user_context.c`; safe TX suite; concrete schema compatibility required, no absent/mismatched getter invocation |
| Synchronous interval operations/admission | `c2/JobDetail.h`: addJobIntervals/flush/cancelRemainingJobIntervals/extendJobEvent | `native/src/rf_job.cpp`: `interval_command`, full-batch validation/preparation before one unlocked Add | Add_RX_Job_Intervals calls RF_Job_Add_Intervals_V16; Add_Job_Intervals is its public rename; fixed diagnostic commands | `native/tests/test_rf_job.c`, `test_rf_job_extension.c`, full specialized suites; void return not effective scheduling/cancellation/extension |
| Status and retained provider graph | `c2/JobDetail.h`: registration; `c2/JobIntervalStatus.h`; `c2/VirtualAperture.h` | `native/src/rf_job.cpp` registration gates; `native/src/rf_job_interval_status.cpp` logical stop; `native/src/rf_virtual_aperture.cpp` claims/snapshots | Existing Job owner and separate status child; no lifecycle API prose changed | `native/tests/test_rf_job_interval_status.c`; `ada/tests/src/ams_mel_rf_interval_status.adb`; registration not delivery or quiescence proof |

## Reconciled semantics and discrepancies

- Continuation is **zero**, not INT64_MAX: pinned `numeric_limits<Femtoseconds>`
  is unspecialized. The scalar aliases ordinary zero-relative-start intent.
  Neither documentation attachment nor successful submission fixes scheduling.
- Event stabilization indices always have a numeric value (default zero), not
  an optional presence bit. Provider size_t checks do not establish membership.
  Ordered group indices and Stokes vectors retain duplicates; third Stokes
  append fails before mutation. RX-only sample/AGC/pulse controls are not TX APIs.
- Bridge kinds 0–6 explicitly select the inner CommonModulations alternatives;
  kind 7 selects WaveformStream in the outer Modulation variant. Native variant
  positions differ from bridge tags. Inactive payloads are ignored. Cached and
  extension alternatives remain unexposed; Ada construction is not support proof.
- Signed modulation -1 is the no-modulation sentinel, distinct from Tone.
  Nonnegative references require the same interval's final active table and
  native int range. Omitted reference collection retains native {-1}; explicitly
  empty calls the setter with an empty vector. No previous Add/registry lookup.
- OOK length must equal repeat count even for unused definitions. A single OOK
  table entry cannot be referenced by distinct TX events; repeated references
  within one event are accepted. CPFSK has its own inner variant despite the
  introductory upstream CustomWaveform comment. No modulation algorithm added.
- Profile steps store two native ints, not chip duration. Profile indices are
  nonnegative and definition-local even for zero/negative iterations. Numeric
  fields survive False update flags. Upstream amplitude 0–1 and combined-duration
  requirements are not bridge clamps/timing checks. HzSec is forwarded verbatim,
  not reinterpreted by a dimensional conversion algorithm.
- Buffer samples are signed Cartesian I/Q pairs, copied componentwise without
  scaling. Empty active buffers receive nonnull empty selected-width vectors.
  Native copies share the fresh allocation; separate definitions/intervals/Adds
  do not share it. No DMA, zero-copy caller lifetime, transfer or emission claim.
- Stream ID/address are full uint64 numeric descriptors; NumBytes must fit
  size_t. Stream format enums are distinct from JobDataFormat/buffer width.
  Product endpoint MaxBytes remains uint64, not a size_t narrowing. Neither API
  checks backing, maps memory, computes address extents or acquires an endpoint.
- Endpoint map last-wins is deliberate bridge insert_or_assign behavior, unlike
  upstream addEndpoint emplace/first-wins. Pinned getApplicableElementGroups
  incorrectly returns dataPaths, not its stored applicable-label vector; this
  bridge uses setEndpoints and does not claim applicable-label setup.
- User context is exactly copied vector<uint8_t> in std::any, not a universal
  context schema. Pinned getter dereferences pointer-form any_cast without checking
  null; its nullopt prose is not a safe absent/wrong-type contract. No speculative
  getter was invoked. Absent, present-empty and present-nonempty remain distinct
  configuration states; providers may expose equivalent default/empty values.
- All signed femtoseconds pass exactly, without Duration/float arithmetic or
  quantization. Long_Float-to-C-double conversion and compiled representation
  checks support normal C-double values; signed zero and nonfinite classification
  are established by existing tests, **not NaN payload-bit preservation**. The
  existing body disables only optional GNAT floating-validity checks for this
  intentional data profile; documentation does not change compiler policy.
- Both public Add spellings use current **v16**, with v8 common base, TX-v3,
  modulation-v5 and context. Historical v1–v15 layouts/tag domains stay frozen;
  v10/v11 and later do not simply append every previous interval layout.
- Add/Flush require no Finalize or full Cancel attempt. CancelRemaining/Extend
  are admitted before, during and after Finalize until **any** full Cancel
  attempt (False/exception included). CancelRemaining is not full Cancel.
  Status registration is required only for status-enabled Add, not Extend/Flush/
  CancelRemaining. Snapshot values do not refresh. Same-Job/Close calls require
  external serialization. No automatic retries; provider mutation can precede
  an exception. Invalid final Add configuration raises Constraint_Error; native
  lifecycle/provider failures use Provider_Error. Normal void return is not an
  execution, extension, cancellation or callback-delivery acknowledgement.

## Historical reports versus current source

Read Task 034C1–C3, 034D1–D3 and 034F4–F17. Earlier reports' deferred TX,
modulation, endpoints and context statements describe their original checkpoint,
not current omissions. Current safe Ada serialization supersedes their old
v3–v15 selections with v16. Task 034C quantization remains a separate explicit
DataMEL operation, never an implicit Add step; PhysicalData/power-mode metadata
are not proof of live TX capability. TX Power Level, interval TX Power Mode ID
and event attenuation dB remain distinct quantities/identities.

Positive value fidelity, validation, ownership, exception and admission evidence
comes from existing actual-header and independent mock C/Ada suites. Historical
pinned receive-only Squall Add/Flush/CancelRemaining/Extend/registration no-ops
establish at most call-path acceptance or no-delivery observations, not payload
interpretation, scheduling, register writes, modulation/detection or RF emission.
No new real-provider or hardware qualification, 32-bit runtime coverage, universal
C++ ABI compatibility, callback delivery or safe mapped-memory access is claimed.
