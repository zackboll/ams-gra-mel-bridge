# Task 034E6 — RF VirtualAperture transmit-power queries

## Starting state and exact scope

Fetched origin and verified PR #73 merged. Actual starting origin/main:
`9b8027e831941ced76b084afb3854761bf224855`. Clean worktree, no equivalent branch
or open PR; created `feature/034e6-rf-tx-power-queries` from origin/main. Baseline:
native 272/272, Python 119/119, production function exports 185, vendor 804/804.
Aggregate make check passed in the Alire toolchain environment; the first plain
shell attempt lacked GNAT/GPRbuild on PATH (not a test pass). Logs live under
`/tmp/ams-mel-034e6/`.

RF MEL pin: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Squall pin: `b1015728f904c799fa0c07489fce48e78f67845f`.
This is a READ/QUERY slice, not TX execution. Exactly four new production exports:

- `ams_mel_rf_virtual_aperture_get_tx_radiated_power`
- `ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power`
- `ams_mel_rf_virtual_aperture_get_tx_aperture_gain`
- `ams_mel_rf_virtual_aperture_get_max_tx_attenuation`

ABI remains 0.1; no existing signature or frozen record changes. No vendor
expansion. Rust sys/private Python raw parity only; Ada is first safe-language
coverage. No safe Rust or public Python TX API.

## Pinned contract and forwarding

All four declarations are const virtual methods and `@RequiredIfTransmit`, not
unconditionally meaningful calculations for receive-only providers:

| Provider method | Exact inputs (in order) | Result units |
|---|---|---|
| getTxRadiatedPower | size_t group, uint32 mode, double attenuation, size_t WeightType, double frequency, pair<double,double> UV, uint32 instance | dBW |
| getTxPeakRadiatedPower | size_t group, uint32 mode, double attenuation, double frequency, uint32 instance | dBW |
| getTxApertureGain | size_t group, uint32 mode, size_t WeightType, double frequency, pair<double,double> UV, uint32 instance | dB |
| getMaxTxAttenuation | size_t group, uint32 mode, uint32 instance | dB |

Actual-header static assertions prove all four complete member signatures,
TxPowerModeID/VirtualApertureInstanceID == uint32_t, WeightType == size_t,
AnglePair == std::pair<double,double> and both component types == double.
The signatures also prove the group parameter is size_t, not a label.

Public group/weight inputs are stable uint64_t. Compile-time numeric_limits
digits proves size_t fits uint64_t; runtime bounds reject unrepresentable values
with INVALID_ARGUMENT before entry. On this 64-bit target all uint64 values
including UINT64_MAX are representable; narrower targets execute conditional
SIZE_MAX+1 rejection tests for both semantic inputs, without truncation.

Attenuation inputs are dB, frequency Hz, U,V the documented upstream line-of-sight/
stabilization components. Construct AnglePair{u,v} without arithmetic. WeightType
is only an ID: no Weights object allocation/getStaticWeights/createWeights.
No hidden E1/E2/E3/E4/E5 query, RFMFAInfo/TxPowerModeData capability gate, local
element-group lookup, formula or comparison/reconciliation between results.
Each explicit bridge operation invokes its corresponding provider method once.
Negative values, signed zero, infinity, NaN and out-of-assumed-range U/V pass
without finite/range validation, clamping, normalization or unit conversion.
Return the provider double unchanged; NaN payload-bit preservation is not promised.

## Failure and lifetime

Validate live VA owner, output pointer, diagnostic pairing and size_t
representability before provider entry. INVALID_ARGUMENT invokes no provider.
std::bad_alloc => INTERNAL_ERROR; other std::exception and unknown exception =>
PROVIDER_EXCEPTION. No retry; every failure preserves the caller's output.

Only the existing VA claim protects synchronous calls. No sibling C2 claim,
owner/snapshot, worker, registration or DSO pin is added. Public C2 may close first
while all four queries still work on the live VA. Closed VA rejects new calls.
Same-VA operations including Close remain externally serialized; provider calls
hold no C2 lifecycle, notification, Job or interval-status queue mutex. Once
copied, doubles are ordinary values and never delay provider unload.

## Safe Ada

`AMS.MEL.RF.C2.Transmit_Power` provides Radiated_Power_DBW,
Peak_Radiated_Power_DBW, Aperture_Gain_DB, Max_Attenuation_DB; Unsigned_64
Element_Group_ID/Weight_Type_ID; Long_Float UV_Line_Of_Sight; Unsigned_32 mode/
instance with instance default 0. No public Interfaces.C.double, size_t,
System.Address or native handle. Each function invokes one raw native import,
does not cache, and maps native errors to Provider_Error.

Compile-time checks cover Long_Float digits, binary mantissa, exponent range and
radix versus C double for normal values. Narrowly scoped `Validity_Checks("F")`
conversion/query helpers follow E3's IEEE pattern, with no project-wide switch.
Public API special-output tests classify both zeros/signs, both infinities and NaN.

## Mock evidence

The existing MockVirtualAperture has four dedicated counters and independent
observations of every argument. Primary vector: group SIZE_MAX, mode 0xFEDCBA98,
attenuation -12.75, weight 0x8000000000000000 on 64-bit (SIZE_MAX on narrower),
frequency 987654321.125, U=-0.75, V=0.625, instance 0xDEADBEEF. No default
substitution, sign confusion, narrowing or argument reordering. Generation A:
[47.125, -3.5, 17.75, 63.25]; generation B: [-28.625, 91.5, -6.25, 12.875].
Fresh calls observe B; old scalar copies stay A. Distinct results deliberately
prove no formula/substitution. C/Ada test 0/SIZE_MAX and independent counters.

C tests exercise null VA/output, malformed diagnostics, conditional overflow,
all four methods' standard/unknown/allocation exceptions, output preservation
and recovery. IEEE input observation covers -0 attenuation, -infinite frequency,
NaN U, +infinite V, then NaN attenuation, negative frequency, U=-2.75 and V=-0.
Every method tests +0/-0/+infinity/-infinity/NaN results via classification/sign,
not ordinary NaN equality. E1/E3/E4/E5 and MFA/TX-mode counters stay zero and
forbidden calls (including Weights) remain absent. Existing E1-E5 tests remain.

Dedicated C and public-safe-Ada executables run in isolated processes with no
E2 subscription, Job, ProductRx or permanent callback. Release test dlopen
reference, close public C2, query all four, close VA, require logged VA destruction
-> C2 shutdown -> C2 destruction -> DSO unload. Copied scalars remain A afterward.
No arbitrary sleeps.

## Pinned Squall limitation

Rechecked exact source `SquallC2MEL.cc:508..529`: all four methods return 0.0.
Squall's RF profile is receive-only. Safe Ada VA and C ProductRx integration use
nontrivial inputs and require four OK/zero values, with Ada repeating after public
C2 Close. Preserve E1-E5, Claim, Job/ProductRx assertions. This is call-path/
return-value evidence only, NOT positive transmit capability, hardware accuracy,
gain-model correctness or attenuation-model correctness. Callback-pinned Squall
processes are not DSO unload evidence.

## Validation record

Implementation is not a validation pass. Completed commands/results are recorded
below after execution. Development failure retained: the first native E6 test
expected an untouched zero after an earlier successful query had changed its
sentinel; corrected the test to explicitly set 111 before closed-VA validation.
No production change or weakened failure contract was needed.
The first Ada test build caught an unused use-type clause, removed rather than
suppressing warnings. The first standalone closure-audit command used compiler
`-isystem` instead of the audit script's `--isystem`; corrected invocation passed
both compilers without changing closure. Review restored the existing Rust
c_void import after a fuzzy patch; Rust check/tests/clippy then passed. The first
Python run caught the explicit ordered inventory tuple missing four additions;
updated it (120 tests, not a public Python API change). Native/Ada/closure first
logs are preserved; the Python first-failure excerpt is in the session transcript.
The first Release audit's overly broad substring test matched legitimate
CommsTest exports; corrected it to exclude actual `ams_mel_test_`/`mock_` symbols,
with exact 189-symbol declaration/map parity independently required.

## Completed local validation

| Command/audit | Actual result |
|---|---|
| make test-native | 273/273 |
| make test-build-isolation | PASS |
| make format-ada; make check-ada-format | PASS; final format check repeated |
| alr -C ada build | PASS |
| alr -C ada/tests run | PASS including isolated public E6 suite |
| make test-rust | PASS, all suites (including 20 sys ABI tests) |
| cargo check --manifest-path rust/Cargo.toml --workspace | PASS |
| cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings | PASS |
| cargo fmt --manifest-path rust/Cargo.toml --all -- --check | PASS |
| make test-python | 120/120 after explicit inventory correction |
| aggregate make check (Alire exec toolchain environment) | PASS unchanged rerun; first environmental failure retained |
| Native E6 CTest --repeat until-fail:50 | 50/50, no Not Run |
| Safe Ada E6, 50 separate invocations | 50/50 |
| GCC 14 / Clang 19 actual vendored-header probes | PASS warnings-as-errors; all four exact member signatures/types |
| GCC / Clang closure audits | 712 / 713 pinned headers, unchanged |
| Fresh production Release /tmp/ams-mel-034e6/release | 189 measured function exports, exact declaration/map parity, only four planned additions, no test symbols |
| Frozen ABI audit | All original 185 signatures and 165 records unchanged; ABI 0.1 |
| Full documented vendor SHA-256 + baseline comparison | 804/804 unchanged; no vendor diff |
| make test-squall-rf-ada-va | PASS; all four zeros before and after public C2 Close |
| make test-squall-rf-ada-job | PASS; preserved two-Job parent-first lifecycle |
| make test-squall-rf-rx | PASS; all four OK/zero results; existing receive assertions |
| make test-squall-rf-ada | PASS; preserved safe Ada ComplexINT16 receive |
| git diff --check / final-newline check | PASS |

First aggregate attempt failed compiling unrelated ir_c2.cpp with `No space left
on device` writing temporary assembly to /tmp (tmpfs 99% full). Preserved
`aggregate-first-failure.log`; unchanged rerun passed with newly available space.
No unrelated files were deleted, container replaced or assertion weakened. A
task-specific disk-backed temporary directory was prepared but was not needed.
No legacy Job-abandonment timeout occurred; no fix to that path is claimed.

Squall used isolated control ports 32203/33203/34203/35203 with distinct matching
health/metrics/data ports. No listener/TIME_WAIT failure occurred. Existing
`opencv-imgproc-publish-010` was untouched. C ProductRx received/queued 8 events,
zero dropped/malformed/allocation/after-close counts. Exact receive-only zero
results remain call-path/return-value evidence only, not positive TX support.
Normal commits and final source/remote/PR equality plus hosted checkout evidence
are reported in the PR/final report. Literal source and PR synthetic merge-ref
testing are distinct. No amend, force-push, merge or auto-merge performed.

## Coverage milestone and exclusions

VA query coverage includes Claim-time info; BaseVA status/instance queries;
status subscriptions; ElementGroup descriptors; required cached-waveform/dynamic-
weight Booleans; Local Functions; VA-level DataPipe connections/association;
and these four conditional calculations. VirtualAperture is NOT complete.
Deferred: descriptor createElementGroupCommand public/thick coverage; broader
ElementGroupCommand/JobRequest configuration; static/dynamic Weights; CachedWaveform
allocation; WaveformTxEndpoint; external/RDMA; VADB; TX JobIntervals/TransmitEvent;
additional ProductRx formats. No such expansion started.
