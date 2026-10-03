# Task 034C3 — RF TxPowerModeData snapshots and safe Ada

## Baseline and scope

Starting origin/main: `af0e7f68f6aa3a2f476e3e5b6d4550c62bd26f45`;
PR #64 verified merged. Clean starting worktree; new branch
`feature/034c3-rf-tx-power-modes`. Measured baseline: native 253/253,
139 production exports, Python 111 tests, 804/804 vendor checksums.

Pinned RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` already provides
`native/vendor/rf-mel/include/rfmel/mfa/TxPowerModeData.h`, reached through
RFMFAInfo. Both `getTxPowerModeCharacteristics` overloads are
`@RequiredIfTransmit`. No vendor dependency was added or modified.
The extended declaration/type probe builds warning-clean under GCC and Clang 19.

## C value and ownership contract

`ams_mel_rf_tx_power_mode_v1` represents every published getter:

| Getter | C field / exact representation |
|---|---|
| getTxPowerModeID | tx_power_mode_id: uint32_t |
| getIsLinearOperation | is_linear_operation: uint32_t, exactly 0/1 |
| getTxPowerLevel | tx_power_level: uint32_t |
| getTxFrequencyRanges(face) | tx_frequency_ranges: existing frequency-range span, doubles in Hz |
| getMaxTxDutyFactor | max_tx_duty_factor: double |
| getMaxTxPulseWidth | max_tx_pulse_width_ns: int64_t, signed nanosecond count |
| getMaxTxAtten | max_tx_atten: double; no invented attenuation units |
| getTxAttenStepSize | tx_atten_step_size: double |

Compile-time proofs establish TxPowerModeID/TxPowerLevel == uint32_t,
DutyFactor/Frequency == double, and nanoseconds::rep == int64_t (signed integral).
Each represented getter occurs exactly once in the copy loop; each range bound
is read once. The concrete upstream TxPowerModeData getters are nonvirtual,
so getter-once evidence is a source audit, not an invented mock interception.

Exactly four added production operations:

- `ams_mel_rf_data_get_tx_power_modes`
- `ams_mel_rf_data_get_tx_power_mode`
- `ams_mel_rf_tx_power_mode_snapshot_view`
- `ams_mel_rf_tx_power_mode_snapshot_close`

Both creation APIs use one `ams_mel_rf_tx_power_mode_snapshot` opaque owner.
The collection calls the collection overload once; direct calls the exact direct
overload once and publishes exactly one returned record. Returned IDs are
provider-authoritative, even when different from the requested ID: there is no
published generic not-found mechanism. Neither query calls supportsTransmit.
Applications may use Supports_Transmit as guidance independently.

Provider vector/range order is preserved. Empty collections/range spans are
`{NULL,0}` and successful. Doubles are copied verbatim, including signed zero,
NaN and infinity. No clamping, range normalization, sorting, merging, finite
checks, attenuation policy or time/frequency unit conversion occurs.

Outer range/mode vectors are final-sized before copying. Nested spans are
assigned only after all backing storage is stable. The immutable owner retains
no provider pointer, DSO pin or DataMEL child claim. View allocates nothing and
calls no provider; all nested spans live until Close. Serialize View/Close on
one owner, and creation with other same-DataMEL operations/Close. Close consumes
and nulls the caller handle, and is idempotent for NULL. Standard/unknown provider
exceptions map to PROVIDER_EXCEPTION; bad_alloc maps to INTERNAL_ERROR. RAII
prevents partial publication. Existing diagnostic rules apply.

## Safe Ada and raw parity

`AMS.MEL.RF.Tx_Power_Mode` and `Tx_Power_Mode_List` are private Ada-owned values.
`Snapshot_Tx_Power_Modes` preserves collection order and accepts an empty list;
`Snapshot_Tx_Power_Mode` calls the native direct operation and requires exactly
one native entry, without requiring requested/returned IDs to match.
Both validate spans, copy every mode/range, and close the native snapshot before
return (also on exceptions). No native handle/address is publicly exposed.

Accessors: Tx_Power_Mode_Count, Tx_Power_Mode_At, Tx_Power_Mode_ID,
Is_Linear_Operation, Tx_Power_Level, Tx_Frequency_Range_Count,
Tx_Frequency_Range_At (existing Frequency_Range), Max_Tx_Duty_Factor,
Max_Tx_Pulse_Width_NS, Max_Tx_Attenuation, Tx_Attenuation_Step_Size.
IDs/levels use Unsigned_32, pulse width Integer_64, numeric doubles Long_Float,
and linear operation Boolean. Closed Data_MEL/provider exceptions raise
Provider_Error. Raw Ada, Rust sys and private Python declarations are synchronized;
no safe Rust or public Python TxPowerModeData API was added.

## Evidence and milestone

The existing RF mock supplies two positive modes in vector order (0x80000001,
UINT32_MAX), high-bit power level, two fractional Hz ranges/empty nested ranges,
0.625/0.375 duty, -123456789/9876543210 ns, 63.5/12.25 attenuation,
0.125/0.5 step. Counters prove separate collection/direct calls and exact
0xFEDCBA98 FaceID/0xDEADBEEF requested-ID forwarding. That FaceID would be
rejected by the mock supportsTransmit getter, proving no bridge gate.
Mismatch, changing snapshots, signed zero/+infinity/NaN, invalid arguments,
standard/unknown/allocation exceptions and repeated Close are covered.
C proves A unchanged after B closes, DataMEL closes and actual DSO unload.
Ada proves value independence and usability after Data_MEL Close.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` is receive-only:
Supports_Transmit(face 0) is false and its collection is empty. Existing C smoke
and safe Ada receive path now check that empty collection. Neither invokes
Squall's direct default-object overload. Mock-positive transmit evidence must
not be described as positive real-transmit-provider validation.

Current-scope published RFMFAInfo data/query coverage in native C + safe Ada:
version; face set/count; receive/transmit support; endpoint association; open
additions; scheduler resolution; quantizeDuration; lead/switching times;
Rx/Tx/sample frequency ranges; max user context bytes; supported data formats;
PhysicalData; TxPowerModeData collection/direct. This is not full RF MEL support.
No JobInterval, Sequence/events, RDMA, VADB, broader TX JobRequest or other
ProductRx format work was started. Frozen MFA/Face v1 records are unchanged.

## Validation

Fresh production Release audit: 139 -> 143 exports, exactly the four above;
all original 139 function declarations byte-identical, ABI 0.1 unchanged.
All 143 names present in each raw binding. Vendor checksum audit: 804/804,
no vendor Git blob changes. Native suite 254/254; focused native contract 50/50.
Alire library build and full Ada tests pass. Rust tests/check/clippy/format pass.
Python suite 112 tests passes.

- `make test-native`: final 254/254 (baseline 253/253).
- `ctest --test-dir native/build-tests -R '^rf-tx-power$' --repeat until-fail:50 --output-on-failure`: 50/50.
- Safe Ada full smoke binary repeated 50 times with repository test library/
  provider environment: 50/50, each including TxPowerModeData and prior regressions.
- `make format-ada` applied; `make check-ada-format`: PASS.
- `alr -C ada build`, `alr -C ada/tests run`: PASS.
- `make test-build-isolation`: PASS.
- `make test-rust`: PASS, including new layout/signature/inventory probes.
- `cargo check --manifest-path rust/Cargo.toml --workspace`: PASS.
- `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings`: PASS.
- `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`: PASS.
- `make test-python`: 112/112; compileall PASS (baseline 111).
- `git diff --check`: PASS.
- `SQUALL_SOURCE_DIR=/home/zboll/git/squall make test-squall-rf-c` with
  isolated ports 25203/25318/25313/25314/25601: PASS; empty Tx snapshot also
  readable after DataMEL Close. Squall remains receive-only, not positive TX.
- Safe Ada Squall receive with isolated ports 26203/26318/26313/26314/26601:
  PASS, including empty Tx modes, PhysicalData, quantization, ProductRx,
  Job finalize/cancel, parent-first/event ownership and counter regressions.
  Initial Ada port set was occupied and was replaced; no direct TX lookup used.
- Fresh Release audit in `/tmp/034c3-release`: PASS as described above.
- GCC and Clang 19 exact getter/type probes: PASS; vendor 804/804 unchanged.

Local PATH has no direct GPRbuild; Alire validation uses its pinned toolchain.
Hosted direct-GPR validation is required through exact-head CI, not claimed from
an unavailable local PATH command. Early overlapping build commands interrupted
validation; complete successful runs above were rerun after the conflicting
builds finished. No interrupted/failed run is counted as passed.