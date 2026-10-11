# Task 034F11 — RF TX JobEvent common controls

## Starting state and scope

Fetched main: `b7c94670f040a787ed3d000409e4d1542f5ad56e`, exactly the
reviewed main. PR #85 was merged at that commit; no open PR or matching F11
branch was found. Worktree `/home/zboll/git/ams-mel-034f11` uses branch
`feature/034f11-rf-tx-event-controls`. Original and F10 worktree registrations
were preserved, with no reset, stash, rebase or force push.

This is copied value configuration, not RF transmission, waveform execution,
conditional triggering, scheduling or hardware processing. Conditional is an
enum value, not an implementation of a conditional trigger API. TX pulse
detection, modulation objects, weights, waveform resources, RDMA, routing,
memory registration, VADB, MFADrivenControls and JIB machinery remain excluded.

## Qualified pinned contract

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f`: the vendored JobEvent.h,
JobInterval.h and RFMELTypes.h match that checkout byte-for-byte. TransmitEvent
inherits the JobEvent setters directly. Actual-header GCC and Clang 19 probes
compiled and ran with `-O3 -DNDEBUG`; explicit checks remain active. The committed
probe is included in the existing pinned-header runtime test.

| Setter | Argument / getter representation | Default |
| --- | --- | --- |
| setPolarization | const vector<array<double,4>>& / const vector& | empty |
| setPolarizationBeemSteerCorrectionEnabled | bool / bool | false |
| setPhaseOffset | Angle = double / double | zero |
| setExecutionType | ExecutionType | Normal = 0 |
| setEventTerminationType | JobEvent::EventTerminationType | InhibitEvent = 0 |
| setAllowDelayStart | bool / bool | false |
| setIterationHoldCount | size_t / size_t | zero |
| setIterationTerminationCount | size_t / size_t | zero |
| setChannelizationEnabled | bool / bool | false |

Native enum declarations establish Conditional = 1 and CancelEvent = 1.
The upstream `Beem` spelling stays at the C++ boundary; Ada uses Beam.
Copy independence is tested with actual TransmitEvent values. TX direction,
modulation {-1}, empty weights and waveform resources stay unchanged. No RX-only
or TX pulse-detection setter is used.

## Additive layout and validation

TX event v2 nests frozen TX v1 and reuses RX v3's Stokes and enum representations.
Interval v10 deliberately nests **v8**, not v9, with a single TX-v2 span. V9
remains frozen and constructs TX-v1 events with original common defaults.
No array cast, layout reinterpretation or parallel TX list/override exists.
Exactly one export is added: `ams_mel_rf_job_add_intervals_v10`; ABI remains 0.1.

Measured LP64 C layout, alignment 8 throughout:

| Record | Size | Offsets in declaration order |
| --- | ---: | --- |
| TX event v2 | 168 | 0,96,112,120,128,132,136,144,152,160 |
| TX event span v2 | 16 | 0,8 |
| Interval v10 | 264 | 0,240,248 |
| Interval span v10 | 16 | 0,8 |

Presence 0 ignores the complete TX payload. Presence 1 copies the collection,
including explicit empty; other encodings reject. Active v8 fields are validated
independently. Every F10 label/scalar/span/index rule remains in force. Stokes
accepts zero, one or two ordered vectors, preserving duplicates; larger counts
reject before element access. Pointers/counts and multiplication bounds are
checked. Every boolean is exactly 0/1; enum values are only 0/1. Both counts must
fit actual provider size_t. Phase radians and Stokes doubles preserve IEEE
classification, infinities and signed zero, not NaN payload bits. No normalization,
angle wrapping, inferred count relationships or physical validation is added.

One existing builder validates the batch, constructs actual TransmitEvent values,
applies common controls once per v2 event and calls setTxEvents once per active
interval. The single unlocked provider Add, exception mapping, registration
policy, Finalize/full-Cancel gates and resource lifetimes are unchanged.

## Safe Ada and raw parity

TX configs own their Stokes vectors and scalar controls with qualified defaults.
All F10 constructors and mutators remain source compatible; the nine common RX
names have TX overloads. A third append raises Constraint_Error before mutation.
Container copies own independent values; interval append copies the event.
Both Add names serialize through v10 with final-sized synchronous backing for
events, labels, group indices and RX/TX Stokes values. Checked Natural count
conversion/addition and explicit native byte-size bounds precede allocation and
pointer publication. Controlled label owners unwind on errors. No address or
native owner escapes the safe API; floating-point suppression is not broadened.
Raw Ada, Rust sys and private ctypes mirror the new records/span/function; public
Python and safe Rust RF APIs are unchanged. Old-profile inventories remain active.

## Evidence and development failures

Baseline native **283/283**, Python **130/130**, Rust workspace tests/check/Clippy/
formatting, Alire library/full Ada smoke and formatting passed. Actual baseline
Rust test output totals **87** tests (historical F10 prose said 115; not adopted).
Production Release had **199** versioned exports; vendor SHA-256 **804/804** matched.
Evidence is disk-backed under the task worktree's `evidence/task-034f11` directory
(local artifacts are not release contents).

Development failures caught and corrected: Ada outer/nested TX aggregate target;
missing TX Stokes aggregate backing count; redundant Ada use clause rejected by
warnings-as-errors; nested C fixture main-macro collision. These failed attempts
are not qualification passes. An initial log move across filesystems left the
writer on its original open inode; output was recovered through its open descriptor.

Final qualification results and real-provider evidence are recorded below after
the corresponding commands finish. No 32-bit runtime qualification is implied
by LP64 measurements or compile-only probes.

## Measured qualification

- GCC 14 Debug/Release and Clang 19 Debug/Release: **284/284 each**.
  CI-style parallel-four until-fail:50 campaigns: **278/278 each**. Six internally
  repeating RF scenarios are excluded from the outer campaign; the focused TX
  controls executable itself executes 50 iterations with Release checks active.
  Logs: `evidence/task-034f11/native-matrix-rerun.log`.
- Python warnings-as-errors **131/131**, compileall passed. Rust workspace **88**
  tests including **28** sys tests; check, Clippy warnings denied and formatting
  passed. Log: `evidence/task-034f11/development-rerun4.log`.
- Alire library build and full Ada smoke passed, including TX common-control
  observations, copied values and third-append rejection in **50/50** iterations.
  Direct-GPR smoke in the Alire environment passed. Existing GPR warnings about
  the imported source-free native project remain; compiled first-party sources
  remain warnings-as-errors.
- Actual production Release: **200** exports, all `AMS_MEL_0.1`; sole addition
  `ams_mel_rf_job_add_intervals_v10`. Mock observations/allocation hooks are absent
  from the production export inventory. All **221** previous C record declarations
  and **199** previous function declarations are byte-identical. Vendor **804/804**
  SHA-256 values match, with no vendor modifications.
- C11 layout probes compare every new field size/alignment/offset against Rust
  and ctypes, including the exact Add signature. Public header C++ compatibility
  stays active in the native suite. No 32-bit runtime toolchain was qualified.

Additional corrected test-only failures: stale Python export-count assertion,
missing C RX observation helpers, and lifecycle checks initially placed before
recovery assertions. An intermediate native campaign failed compilation and was
restarted on the corrected source; only the complete rerun is counted above.
Local generated CMake evidence initially appeared in the final-newline scan;
task evidence is now locally excluded from Git and release source checks.

- Aggregate `make check` in the Alire environment passed: build-tree isolation,
  native **284/284**, GNATformat, full Ada smoke, final-newline and whitespace.
- Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f`: Job, Ada VA,
  C ProductRx and Ada ProductRx passed **one iteration each**, sequentially, with
  task-owned build/runtime resources and port bases 31000/32000/33000/34000.
  Job fixture submitted non-default TX common values through the real Add call.
  Squall is receive-only and Add is a no-op: normal return establishes call-path
  acceptance only, not polarization, conditional execution, channelization,
  TX capability or emission. Positive payload evidence remains mock/header probes.
  Log: `evidence/task-034f11/full-qualification.log`, completion marker present.

### Review limitations

No 32-bit runtime qualification is available. C/Rust/ctypes new-field layout
comparisons are exhaustive; raw Ada uses Convention C and exercised serialization,
not a separate exhaustive field-offset comparison against C. The focused v10
native test covers asymmetric TX controls plus differing RX controls, rejection,
recovery, status modes, parent-first close and terminal gates. F10 label/scalar/
nested-field regressions remain active separately and through the public Ada mixed
fixture. It does not exhaustively repeat every F10 malformed payload under v10,
or inject allocation failure after each possible earlier event construction.
The Ada common-control fixture retains duplicate TX events rather than proving
all new fields independently with two asymmetric public-Ada events. These are
qualification gaps for independent review, not claims of exhaustive task coverage.
No new callbacks, queries, workers or ownership paths are introduced.

The PR must remain open, non-draft, unmerged, with auto-merge disabled. Hosted CI
results are reported separately, never inferred from local qualification.
