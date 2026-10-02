# Task 034C2 — RF PhysicalData snapshot and safe Ada

## Baseline and declaration checkpoint

Started from verified merged PR #63 main
`be78151b7da016fde9b62057a1cca96540c9a997`, on new branch
`feature/034c2-rf-physical-data`, with a clean worktree. Measured baseline:
native 252/252, production Release exports 136, Python 110, vendor 803/803.

Focused compiler-observed `PhysicalData.h` closure against clean full pinned
RF/Common/Math/VITA trees: GCC 516 (RF 4, Common 23, Math 5, VITA 5, Boost 479),
Clang 19 517 (same except Boost 480). Boost uses the existing pinned 1.83.0
closure. Exact missing union: only
`native/vendor/rf-mel/include/rfmel/mfa/PhysicalData.h`, from immutable RF
`762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
Common is `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.
`PhysicalData.h` directly reaches `MFA_Component.h` and `RFMELTypes.h`;
the Common chain includes `Directional.h` and `UCI_ID.h` already present.

The expanded ordinary declaration probe is GCC 712 / Clang 713 / union 714,
and checks the exact dependency lists and all consumed const getter types.
It rejects unpinned/system project headers. Checkpoint commit:
`dc46af3` (`Add RF PhysicalData declaration closure`), not amended.
All original 803 vendor blobs were compared to starting-main Git objects;
the added header was compared to the pinned upstream Git object. All 804
checksum entries pass. No upstream blob was edited.

## Complete mapping and getter boundary

| Published getter | Snapshot field | Unit |
|---|---|---|
| PhysicalData.getAntennaHeight | antenna_height_m | meters |
| PhysicalData.getAntennaWidth | antenna_width_m | meters |
| PhysicalData.getLatticeAngle | lattice_angle_rad | radians |
| InstallationDetails.getLocation → getOffsetX | location.offset_x_m | meters |
| getOffsetY | location.offset_y_m | meters |
| getOffsetZ | location.offset_z_m | meters |
| getLocationId → ForeignKey.getKey | location.key | complete UTF-8 bytes |
| ForeignKey.getSystemName | location.system_name | complete UTF-8 bytes |
| InstallationDetails.getOrientation → Euler.getRoll | orientation.roll_rad | radians |
| Euler.getPitch | orientation.pitch_rad | radians |
| Euler.getYaw | orientation.yaw_rad | radians |
| InstallationDetails.getBoresight → Euler.getRoll | boresight.roll_rad | radians |
| Euler.getPitch | boresight.pitch_rad | radians |
| Euler.getYaw | boresight.yaw_rad | radians |

Creation validates arguments, preallocates the snapshot, calls getRFMFAInfo
and getPhysicalData with the exact uint32 FaceID, then reads each represented
value exactly once. It obtains getInstallationDetails, getLocation,
getLocationId, getOrientation and getBoresight each once. The production
function has one explicit invocation per scalar/string getter; orientation
and boresight each call roll, pitch and yaw once. Nested getters are concrete
nonvirtual inline declarations in the pinned API, so mock runtime counters
cannot intercept them without changing upstream bytes. Source inspection and
the declaration probe establish that boundary; mock counters establish exactly
one getPhysicalData call and exact `0xFEDCBA98` FaceID, and no calls for invalid
bridge arguments. No face-admission policy is invented.

Doubles are copied verbatim: no normalization, reordering, clamping, unit
conversion or finite-value requirement. The C mock also proves signed zero,
infinity and NaN pass through. Strings must be valid UTF-8 without embedded
NUL, as for existing provider strings. Invalid key/system-name encoding or
embedded NUL returns PROVIDER_FAILED and publishes no owner. Empty strings
have non-null bridge storage and zero length. No truncation occurs.
All provider/getter exceptions are contained: bad_alloc → INTERNAL_ERROR;
std::exception and unknown exceptions → PROVIDER_EXCEPTION. Failure publishes
no partial snapshot. Nested inline value getters provide no exception-injection
point; no failpoint framework or changed vendor declaration was introduced.

## Ownership and ABI

New opaque owner: `ams_mel_rf_physical_data`. It contains only two std::strings
and an immutable public record, never provider pointers, shared owners or DSO
pins. No DataMEL child claim is acquired. Provider references exist only within
creation and do not survive its return. Strings are wired only after both copies
are complete. View neither allocates nor calls providers; output conventions
match MFA View (non-null output pointer, unrestricted initial pointed-to value).
Close consumes and nulls the owner, is idempotent for null, and calls no provider.
Caller serialization requirements match the existing synchronous snapshot API.

Exactly three new records:
`ams_mel_rf_euler_v1`, `ams_mel_rf_component_location_v1`,
`ams_mel_rf_physical_data_v1`.
Exactly three new exports:
`ams_mel_rf_data_get_physical_data`, `ams_mel_rf_physical_data_view`,
`ams_mel_rf_physical_data_close`.
ABI remains 0.1; fresh production Release audit observes 139 exports,
preserving all original 136 versioned names. Existing MFA/face v1 declarations
are unchanged. Raw Ada, Rust sys and private Python records/signatures are
synchronized; C/Rust/Python size/alignment/offset probes cover all new records.
No safe Rust or public Python PhysicalData API exists.

## Safe Ada

`AMS.MEL.RF.Physical_Data` is a private ordinary Ada record with Long_Float
scalars and Unbounded_String key/system-name storage. No native handle, address,
Close or Finalize exists on the value. `Snapshot_Physical_Data (Data, Face_ID)`
creates the native snapshot, validates the native view and string lengths,
copies all fields, and closes the native owner on both success and exceptions.
The existing compile-time Long_Float/C-double representation proof applies.

Accessors: Antenna_Height_M, Antenna_Width_M, Lattice_Angle_Radians,
Location_Offset_X_M/Y_M/Z_M, Location_Key, Location_System_Name,
Orientation_Roll/Pitch/Yaw_Radians, Boresight_Roll/Pitch/Yaw_Radians.
Numeric accessors return Long_Float; string accessors return String.
Closed Data_MEL and malformed/throwing providers raise Provider_Error.

## Mock and real evidence

Mock positive values: height 1.25, width 2.5, lattice -0.375;
XYZ (10.125, -20.25, 30.5); key `bay-µ-17`, system `mock/β-installation`;
orientation (0.125, -0.25, 0.5); boresight (-0.75, 1.0, -1.25).
Changing mode replaces all doubles by their value plus one and both strings
with different text. C and Ada prove A unchanged after B, after B cleanup
where applicable, and after DataMEL Close. C proves distinct owned string
payloads and actual mock DSO unload while A remains readable. Tests also
cover all four malformed strings, standard/unknown/allocation exceptions,
invalid arguments and repeated snapshot Close.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` evidence only:
face 0 height/width 0.25 m, lattice 0 rad; default installation XYZ and
both Euler triples zero, key/system empty. These are not generic defaults
promised by the bridge. Existing real C smoke retains native PhysicalData
across DataMEL Close. Existing safe Ada ProductRx integration retains the
Ada value across its normal DataMEL Close, preserving event/Job/counter/
quantization checks. No additional real-provider mode was introduced.

## Validation

- `make test-native`: 253/253 (baseline 252/252).
- `make test-build-isolation`: PASS.
- `ctest --test-dir native/build-tests -R '^rf-physical$' --repeat until-fail:50 --output-on-failure`: 50/50 successful contract runs.
- Full safe Ada smoke repeated 50 times: 50/50, each including all PhysicalData
  checks and existing RF/IR regression checks. An initial run was interrupted by
  concurrent test-library relinking; the complete repeat was restarted after
  builds finished and passed.
- `make format-ada` applied before Ada builds; `make check-ada-format`: PASS.
- `alr -C ada build`, `alr -C ada/tests run`: PASS.
- `make test-rust`: PASS, including record layouts, signature probes and inventory.
- `cargo check --manifest-path rust/Cargo.toml --workspace`: PASS.
- `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings`: PASS.
- `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`: PASS.
- `make test-python`: 111 tests PASS (baseline 110); compileall PASS.
- `git diff --check`: PASS.
- `SQUALL_SOURCE_DIR=/home/zboll/git/squall make test-squall-rf-c`: PASS,
  including native PhysicalData after DataMEL Close.
- Safe Ada Squall: PASS with isolated control/metrics/health/data ports
  24203/24318/24313/24314/24601, after the default health port was occupied.
  PhysicalData, ProductRx, Job Finalize/Cancel, quantization, event lifetime and
  counters all pass in the existing receive mode.
- Fresh production Release configure/build in `/tmp/034c2-final-release`:
  139 versioned exports, exactly the three additions, no original export removed.
  Every pre-existing public function declaration is byte-identical; frozen MFA
  and face records unchanged; all 139 names occur in each raw-language binding.
- Source getter-once audit: PASS for all represented getters and each Euler triple.
- Vendor: 804/804 SHA-256 entries, original 803 Git blobs unchanged, new header
  byte-identical to pinned upstream. GCC and Clang closure/type probes PASS.
No JobInterval, TxPowerModeData, RDMA, VADB, new sample formats or other RF
expansion was started.
