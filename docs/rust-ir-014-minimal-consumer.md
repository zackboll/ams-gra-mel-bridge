# RUST-IR-014 — executable minimal Rust IR consumer

## Scope and source preflight

This is an application interoperability increment, not a new binding family.
The executable uses only `std` and public `ams-mel` types, forbids unsafe code,
and loads the caller-supplied library through the existing safe Session/C facade.
No native ABI, sys declarations, safe library API, provider fixtures, vendored
declarations, other language bindings, CI or historical coverage are changed.
The native/Ada structured coverage inventory therefore has no classification
delta. The new evidence is a runnable three-channel safe Rust application.

Fetched `origin/main` before creating clean worktree
`/home/zboll/git/ams-rust-ir-014` on
`feature/rust-ir-014-minimal-consumer`, based on
`084a8906f3cd729b7a20a921f39bd513d8ac02c2`.
PR #200 was confirmed MERGED, merge
`8652ed7cc5705a5c863fc7d8509a1b231438fdec`.
RUST-IR-008 through 012 are present through merges #186, #188, #191, #194,
and #197. No open PRs existed at preflight; remote branch inspection found no
overlapping minimal-consumer work. RUSTDOC-001 (#201) is already merged and
its files are untouched.

During qualification, PR #202 (Ada common communications metadata) and #203
(RUSTDOC-002 core API documentation) opened independently. Their inspected files
do not overlap this task's four new paths. In particular #202 does not supply a
public safe Rust Health/Image common callback owner, so this application's
stated callback parity gap remains intentional. Neither PR was modified.

Reviewed repository instructions, contribution/architecture/ABI policies,
provider-foundation task, IR inventory schema and relevant reviewed operation
entries, Rust source API and existing tests, and the exact reference C++ consumer,
provider, build definition and consumer/required-interface documentation.

## Running the application

From the bridge worktree:

```sh
make native
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib" \
cargo run --manifest-path rust/Cargo.toml -p ams-mel \
  --example minimal_ir_consumer -- \
  /home/zboll/git/ams-rust-ir-014-reference/build/libminimal_ir_mel_provider.so
```

`--help` returns 0; missing/extra arguments return 2; a loading, native-operation
or profile/contract failure returns 1. Only completion of every supported check
returns 0. Paths are passed as OS arguments, not shell-evaluated.
The only accepted profile is vendor `minimal-mel-example`, API integer `4`,
library revision integer `1`. These are not universal MEL version encodings.

Discovery remains in provider enumeration order, printed with UUID bytes,
labels, platform identity, component location and every layout field. Selection
uses type membership, rejects missing/ambiguous selections, requires exactly
four single-type descriptors with distinct channel UUIDs, and validates all
seven zero layout scalars. The three attachments copy discovered identities and
locations verbatim, then compare attached capabilities against discovery.
StackedImage is never attached. Image uses `ImageControlConfig`, never
`ImageConfig`, a replacement 1x1 format, or acquisition buffers.

Metadata registration is before enablement: C2's three required families and
separate CommsTest registration, all six Health families, and Image's four
required families. Registration is not a measurement. Health families are
MFA_Status, BIT_Status, SubsystemStatusResp, DiscreteStatus,
MFA_SecurityAuditRecord and MFA_StatusDetailed. Image Start means enablement only;
`receive(0)` must return InitializationFailed with the control-only diagnostic.

The reference's local CommsTest routing convention is the published ChannelType
numeric value (C2=2, Health=4, Image=1), selected from each discovered type.
This is explicitly provider-specific, never an integer reinterpretation of UCI
UUIDs. Each destination gets direct and C2-routed requests; each future is
independently inspected and closed. Two actual C2 events are separately received
and correlated. Health/Image callback registration is not exposed by safe Rust.
Nonblocking polls and counters require two C2 events, zero Health/Image events,
zero queue drops and zero malformed events for this synchronous reference stub.
This is an observation of this run, not a general guarantee of absent telemetry.

BIT 300/NoOp and ConfigSet 301/zero timestamp/empty opaque text must complete
with NotImplemented. Mode 302/Standby/Unused/default ScanParameters and the
complete explicitly populated neutral NavigationReport/NotSet must be explicit
InvalidState rejections with nonempty provider descriptions. No BIT execution,
configuration application, INS fix or successful alignment delivery is claimed.

## Ownership and timeout boundary

Every request wait is finite (5000 milliseconds). This bounds the future wait,
not a provider's synchronous send or close call. Timeout is never cancellation.
Completed/rejected payloads are validated before request Close; even unexpected
terminal payloads receive explicit Close. Native wait errors propagate and Drop
provides the existing best-effort fallback. No new threads, locks or callbacks
are implemented in Rust.

Normal cleanup stops Image, closes all Views before typed owners, closes typed
C2/Health/Image through their existing lifecycle methods, then closes retained
metadata consumers and Session. C2/Health expose typed Close rather than an
independent Disable. Metadata consumption Close is not upstream callback
unregistration; the native typed-channel destruction remains that boundary.
Any Close failure is reported and prevents a successful completion message.
Early unwinding relies on existing Drop and does not promise successful cleanup
or provider unload. No existing ownership semantics are changed.

## Comparison with the reference C++ consumer

Reference: `minimal-mel-example/docs/consumer.md` at
`cf22a80ec98398eb361df758ada7bfb4024710a3`.

| Surface | Classification | Rust scenario and boundary |
| --- | --- | --- |
| Manager/Control factories, init, version and discovery | **Exercised** | Through safe Session and production C facade, not direct C++ calls |
| C2, Health, Image attach/capabilities/enable | **Exercised** | Actual discovered identities; Image control-only |
| StackedImage safe ownership and attach | **Not yet exposed** | Advertisement reported only; no safe Rust typed owner |
| General Buffer factory, init/getters/flags/release, per-channel buffer register/unregister | **Not yet exposed** | General safe Buffer surface absent; acquisition-specific internals are not a substitute |
| Three-channel KeepAlive | **Exercised** | Three independent Success completions |
| Three-channel direct and C2-routed CommsTest | **Exercised** | Six independently checked command/request future correlations |
| C2 common CommsTest callback reception | **Exercised** | Two real C2 events; independent of futures |
| Health/Image/Stacked common CommsTest callback coverage | **Not yet exposed** | No equivalent to all four registrations/eight C++ callbacks |
| C2 required BIT/ConfigSet/Mode commands | **Exercised** | Exact incomplete/rejected reference profiles |
| Image NavigationReport | **Exercised** | Complete NotSet report; explicit rejection preserved |
| C2/Health/Image required metadata registration | **Exercised** | 13 required registrations plus C2 common CommsTest |
| Actual sensor acquisition and sensor telemetry | **Not applicable** | Reference has no operational sample or measurement producer |
| Strict Mono8 acquisition, optional NUC/temperature, optical distortion map, LOS quaternion | **Available, not exercised** | Public safe APIs exist; not part of the required reference scenario |
| Other BIT actions, general mode/config/navigation values | **Available, not exercised** | Public safe inputs exist; no invented reference semantics |
| Supported typed cleanup and Session closure | **Exercised** | Explicit requests/Views/channel/metadata/Session cleanup |
| Exact four-channel C++ independent disable/unregister/detach sequence | **Not yet exposed** | Safe typed lifecycle only; no general buffer/Stacked ownership |
| Real provider interoperability | **Exercised** | Debug and Release runs, not mock substitution |

No attempted operation is classified Blocked in the passing real-provider run.
This is not full C++ consumer parity, complete AMS-GRA compliance, registry
readiness, or hardware qualification.

## Reference source and ABI qualification

Isolated clean reference checkout:
`/home/zboll/git/ams-rust-ir-014-reference`.

- Commit: `cf22a80ec98398eb361df758ada7bfb4024710a3`.
- Tree: `7e560e7afc8ee5538501ad40cef2a862a35b076f`.
- IR MEL: `8d9224519f12b44e0b28815755c56a32a28d24a0`.
- Common MEL: `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.
- AMS Math: `00be45190f0e47d268cece8b8c2f8fb58b5418d2`.
- All 92 overlapping vendored include files compare byte-for-byte equal.
  Reference `upstream_declarations` integrity test also passes.
- Both native CMake builds use `/usr/bin/c++`, Debian GCC 14.2.0-19,
  C++20, the same libstdc++ ABI and `/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.33`.
  glibc is 2.41-12+deb13u4. No alternate `_GLIBCXX_USE_CXX11_ABI` build is used.
- Rust: rustc 1.98.1 (`48a229cea`), Cargo 1.98.1 (`797e8a9bc`).

Reference qualification commands (all exit 0):

```sh
cmake -S /home/zboll/git/ams-rust-ir-014-reference \
  -B /home/zboll/git/ams-rust-ir-014-reference/build -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/zboll/git/ams-rust-ir-014-reference/build --parallel 2
ctest --test-dir /home/zboll/git/ams-rust-ir-014-reference/build --output-on-failure
```

4/4 pass: provider_contract, dynamic_load, consumer_required_api (the actual
C++ consumer), upstream_declarations. This preceded Rust interoperability.

Production and contract-test Cargo artifacts use separate directories
`rust/target-production` and `rust/target-tests` (local ignored products, not
committed). The production executable has NEEDED `libams_mel_c.so.0`, not a
provider link dependency, and RUNPATH
`/home/zboll/git/ams-rust-ir-014/native/build/lib`. `ldd` resolves the production
facade there. With LD_LIBRARY_PATH unset, `LD_DEBUG=libs,files` confirms actual
initialization of that facade AND the supplied absolute reference provider,
not `libmock_ir_provider.so`. Loader evidence reports GNU NODELETE activation
for the reference DSO; its finalizer runs at process exit. Explicit safe Close
success must not be interpreted as proof of immediate OS unmapping.

## Local qualification and exact output

Final local results and executable transcript are recorded below after the
sequential gates. Hosted delivery is **HOSTED_CI_PENDING** until final-HEAD and
corresponding synthetic-merge checks complete. Independent technical review
remains required; no merge, self-approval or auto-merge is authorized.

The first foreground `make native` exceeded the command runner's 30-second
limit; it was rerun to completion in a logged background process before tests.
The first policy-test compile exposed two omitted fields in a *test-only*
NavigationResponse initializer; both were supplied before successful tests.
No native/provider changes were needed. Build operations on a given native
tree were sequential. Existing GPR metadata warnings about the imported native
project having no C sources are recorded, not suppressed by this task.

### Final executable and focused policy evidence

Both `cargo build --manifest-path rust/Cargo.toml -p ams-mel --example
minimal_ir_consumer` and the same command with `--release` passed (exit 0), with
`AMS_MEL_NATIVE_LIB_DIR=/home/zboll/git/ams-rust-ir-014/native/build/lib` and
`CARGO_TARGET_DIR=/home/zboll/git/ams-rust-ir-014/rust/target-production`.
Both executable invocations against the pinned reference returned **0**,
with **empty stderr** and byte-identical stdout below. The final checks also
require zero Image frame counters; no fabricated samples were observed.

The optional integration test was explicitly enabled in Debug and Release:

```sh
AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib" \
CARGO_TARGET_DIR="$PWD/rust/target-production" \
AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" \
AMS_MEL_REFERENCE_PROVIDER=/home/zboll/git/ams-rust-ir-014-reference/build/libminimal_ir_mel_provider.so \
AMS_MEL_CONSUMER_EXE="$PWD/rust/target-production/debug/examples/minimal_ir_consumer" \
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test minimal_ir_consumer -- --include-ignored
```

Repeat with `--release` and the release executable path: **11/11 pass** in
each configuration, no ignored tests in these explicit invocations. Ten pure/
local policy tests are also included in ordinary workspace tests. Executable
checks cover help/missing/extra/path failures and a successful real-provider run;
with the existing fixture directory set they additionally verify wrong-profile
exit 1 and Control/Manager/DSO cleanup on that early failure. Synthetic policy
values test missing/ambiguous discovery, zero/nonzero layouts, exact expected
and unexpected terminal outcomes, rejection preservation, correlation, counters
and unexpected native failures. They do not reproduce private hostile-ABI tests.

Final production facade SHA-256:
`0604b614b08db062f2655050028ced81ff2886cc1b38024960656bd9719fe1b7`.
Reference provider SHA-256:
`c7ebfbf2be227779efdd15a843e4ec7df90274157f2b310b7f2fa812e11a1c6c`.
The production CMake configuration has `AMS_MEL_BUILD_TESTS=OFF` and no
`AMS_MEL_ENABLE_TEST_FAILPOINTS`; contract tests use the separate test tree.
No native facade interoperability defect was observed.

Exact final Debug stdout (Release identical):

```text
[OK] reference-stub identity: ProviderVersion { api_version: 4, library_version: 1, vendor: "minimal-mel-example", description: "IR MEL 4.0 required-interface provider stub; no sensor connected" }; supplied library=/home/zboll/git/ams-rust-ir-014-reference/build/libminimal_ir_mel_provider.so
[OK] advertised[0] ChannelCapability { channel_id: UciId { uuid: [66, 113, 109, 101, 108, 115, 64, 0, 128, 0, 0, 0, 0, 0, 0, 3], descriptive_label: "Stub C2" }, height: 0, width: 0, bit_depth: 0, row_pitch: 0, buffer_size: 0, image_size: 0, number_of_bands: 0, pixel_format: Mono, sensor_types: [Unspecified], platform_id: UciId { uuid: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], descriptive_label: "" }, sensor_location: ComponentLocation { offset_x_m: 0.0, offset_y_m: 0.0, offset_z_m: 0.0, key: "", system_name: "" }, channel_types: [CommandAndControl], task_schedule_depth: 0, odc_available: false, nuc_available: false, metadata_capabilities: [BitConfiguration, CommandStatus, BitStatus, ChannelCommsTestRep], image_bands: [], nav_frames: [] }
[OK] advertised[1] ChannelCapability { channel_id: UciId { uuid: [66, 113, 109, 101, 108, 115, 64, 0, 128, 0, 0, 0, 0, 0, 0, 5], descriptive_label: "Stub health" }, height: 0, width: 0, bit_depth: 0, row_pitch: 0, buffer_size: 0, image_size: 0, number_of_bands: 0, pixel_format: Mono, sensor_types: [Unspecified], platform_id: UciId { uuid: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], descriptive_label: "" }, sensor_location: ComponentLocation { offset_x_m: 0.0, offset_y_m: 0.0, offset_z_m: 0.0, key: "", system_name: "" }, channel_types: [HealthAndStatus], task_schedule_depth: 0, odc_available: false, nuc_available: false, metadata_capabilities: [MfaStatus, MfaStatusDetailed, BitStatus, SubsystemStatusResp, ChannelCommsTestRep], image_bands: [], nav_frames: [] }
[OK] advertised[2] ChannelCapability { channel_id: UciId { uuid: [66, 113, 109, 101, 108, 115, 64, 0, 128, 0, 0, 0, 0, 0, 0, 2], descriptive_label: "Stub single-detector image" }, height: 0, width: 0, bit_depth: 0, row_pitch: 0, buffer_size: 0, image_size: 0, number_of_bands: 0, pixel_format: Mono, sensor_types: [Unspecified], platform_id: UciId { uuid: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], descriptive_label: "" }, sensor_location: ComponentLocation { offset_x_m: 0.0, offset_y_m: 0.0, offset_z_m: 0.0, key: "", system_name: "" }, channel_types: [IrstImage], task_schedule_depth: 0, odc_available: false, nuc_available: false, metadata_capabilities: [BadPixelList, LineOfSightReport, LineOfSightEuler, ChannelCommsTestRep, NavigationReportResp], image_bands: [], nav_frames: [] }
[OK] advertised[3] ChannelCapability { channel_id: UciId { uuid: [66, 113, 109, 101, 108, 115, 64, 0, 128, 0, 0, 0, 0, 0, 0, 7], descriptive_label: "Stub stacked-image interface" }, height: 0, width: 0, bit_depth: 0, row_pitch: 0, buffer_size: 0, image_size: 0, number_of_bands: 0, pixel_format: Mono, sensor_types: [Unspecified], platform_id: UciId { uuid: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], descriptive_label: "" }, sensor_location: ComponentLocation { offset_x_m: 0.0, offset_y_m: 0.0, offset_z_m: 0.0, key: "", system_name: "" }, channel_types: [StackedImage], task_schedule_depth: 0, odc_available: false, nuc_available: false, metadata_capabilities: [ChannelCommsTestRep], image_bands: [], nav_frames: [] }
[STUB] StackedImage advertised, not attached or exercised by safe Rust
[OK] attached identity/layout: Stub C2
[OK] attached identity/layout: Stub health
[OK] attached identity/layout: Stub single-detector image
[OK] required metadata registered: C2=3, Health=6, Image=4; C2 CommsTest separately registered (not sensor measurements)
[OK] C2/Health enabled, Image started (enablement only, no acquisition buffers)
[STUB] Image receive(0): established control-only error; no frame acquired
[OK] Stub C2 KeepAlive: Completed { value: Success }
[OK] Stub health KeepAlive: Completed { value: Success }
[OK] Stub single-detector image KeepAlive: Completed { value: Success }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 100, request_id: 200 } }, request=CommsTestRequest { command_id: 100, channel_id: 2, request_id: 200 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 100, request_id: 200 } }, request=CommsTestRequest { command_id: 100, channel_id: 2, request_id: 200 }
[OK] actual C2 CommsTest metadata: CommsTestReport { command_id: 100, request_id: 200 }
[OK] actual C2 CommsTest metadata: CommsTestReport { command_id: 100, request_id: 200 }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 101, request_id: 201 } }, request=CommsTestRequest { command_id: 101, channel_id: 4, request_id: 201 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 101, request_id: 201 } }, request=CommsTestRequest { command_id: 101, channel_id: 4, request_id: 201 }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 102, request_id: 202 } }, request=CommsTestRequest { command_id: 102, channel_id: 1, request_id: 202 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 102, request_id: 202 } }, request=CommsTestRequest { command_id: 102, channel_id: 1, request_id: 202 }
[STUB] Health/Image common CommsTest callbacks not exposed in safe Rust; six futures and two C2 events checked, not eight callbacks
[STUB] BIT_Command 300/NoOp: Completed { value: NotImplemented }; no BIT performed
[STUB] ConfigSetCommand 301/zero timestamp/empty config: Completed { value: NotImplemented }; no configuration applied
[STUB] ModeCmd 302/Standby/Unused/default scan: Rejected { code: InvalidState, description: "ModeCmd: required sensor operation is a stub; hardware is not connected" }
[STUB] NavigationReport NotSet/neutral values: Rejected { code: InvalidState, description: "NavigationReport: required sensor operation is a stub; hardware is not connected" }; not an INS fix or delivered alignment
[STUB] all sensor metadata queues empty at nonblocking observation; registration is not telemetry
[OK] requests, Views, typed channels, metadata and Session explicitly closed
[OK] three-channel reference-stub contract complete; no full C++ parity or hardware qualification
```

Debug stderr: empty. Debug exit: 0. Release stderr: empty. Release exit: 0.
Loader-traced run exit: 0; trace confirms the production facade and supplied
reference library initialization and NODELETE/finalizer behavior described above.

### Sequential repository gate commands

The native/Rust gates below were rerun after the last source change, sequentially,
with contract-test environment set only for Cargo qualification:

```sh
make native
make test-native
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
export CARGO_TARGET_DIR="$PWD/rust/target-tests"
cargo test --manifest-path rust/Cargo.toml --workspace
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps
```

All exit 0: native **587/587**, workspace **512 passed, 0 failed, 10 ignored**
(including 34 doctests). Nine ignored entries are existing isolated-child entry
points exercised by their parent scenarios; the tenth is this task's explicitly
opt-in reference test, separately run successfully as above. All-target check,
Clippy with `-D warnings`, formatting and Rustdoc with `-D warnings` pass.
Additional source-integrity checks pass (825 vendored files/four workflows),
and the structured inventory regression suite passes **61/61**.

Aggregate uses the pinned GNAT/GPRbuild environment, with conflicting overrides
removed, after the above gates:

```sh
env -u AMS_MEL_NATIVE_LIB_DIR -u AMS_MEL_TEST_PROVIDER_DIR \
  -u CARGO_TARGET_DIR -u LD_LIBRARY_PATH \
  PATH="/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH" \
  make check
```

GNAT 16.1.0, pinned GPRbuild package 26.0.1 (banner 26.0.0), and GNATformat
26.0.0 were available. No Ada source changed, so no formatting rewrite was
necessary. The first aggregate run passed all native/Ada stages but returned 2
at the final-newline check on this new document; the newline was corrected and
the full aggregate gate rerun. No test or contract was weakened.

Final full `make check`: **PASS, exit 0**. Native isolation passes, native
**587/587** passes, pinned formatting checks pass, inventory **61/61** passes,
and all **49 Ada contract PASS groups** pass, followed by newline/whitespace
validation. The pre-existing imported-project GPR metadata warnings remain
visible. The full contract-test tree is restored with `make test-native` after
aggregate qualification (rather than leaving only the provider targets).

Final commit/tree identities and final-HEAD hosted run/synthetic-merge evidence
are recorded on the delivery PR, avoiding a self-referential commit hash here.
Independent review is still required even after passing hosted checks.

## Mainline integration during delivery

After initial local qualification and successful hosted Bootstrap run
43655382809 and Documentation run 43655382808, main advanced to
`8892834fb41f3929083df898e472e20160212f79` (PRs #202 and #203).
It was incorporated with ordinary merge
`0473ac9e5a5cf529a4e639e318d6f3ee6a719c6d`; no published history was rewritten.
Those mainline changes include native/Ada common CommsTest reception and raw
mirrors, but still no public safe Rust Health/Image/Stacked metadata receiver.
The application's safe Rust parity boundary is unchanged. The task diff against
updated main remains exactly the four task-owned new paths.

The above numeric results/transcript/library hash describe the initially
qualified tree, not silently the integrated facade. All gate commands, actual
reference-provider Debug/Release runs, runtime library identity, and aggregate
checks are rerun for the integrated source. Integrated counts, exact hashes,
stdout comparison, final commit/tree, hosted runs and synthetic-merge identity
are recorded as delivery evidence on PR #205. Keep **HOSTED_CI_PENDING** until
those final-source runs complete. The imported Ada changes were formatted with
`make format-ada` before builds; it changed no files. Pinned formatting is checked
again by the aggregate gate. No new API or provider change belongs to this task.
