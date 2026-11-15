# RUST-IR-018 — eight-callback four-channel consumer parity

## Scope and source preflight

This consumer-only increment upgrades the existing Cargo example, not the binding
or the provider. It qualifies communications against the unconfigured C++ reference
stub; it does **not** establish complete required-interface parity, AMS-GRA
compliance, physical sensor operation or acquisition support.

Fetched `origin/main` before creating branch
`feature/rust-ir-018-eight-callback-consumer` and isolated worktree
`/home/zboll/git/ams-rust-ir-018`, directly based on commit
`604ef38f8f81a9c71add60c5ca2bac4db218ea7b`, tree
`c2bf94b76fa1aa841345484ba2e8efa89e783cce`. Existing worktrees and published history
were preserved. GitHub merge state and local ancestry both confirmed:

| Prerequisite | Merge commit |
|---|---|
| PR #219, RUST-IR-017 | `b4c095ca92af5bb9f14c7c0dcadceee749c50620` |
| PR #214, RUST-IR-016 | `8e5a9947b9dd1c47f0322970cd2ee5da755df738` |
| PR #215, Common crate extraction | `ad35ec4d5fc2faa9296b530defad108bf1dbac31` |

Inspected the Common extraction: `Error`, `ErrorKind`, identity, location and
version models are canonical `ams-mel-common` re-exports, not application copies.
`CommsTestReport` and `CommandReturn` use the existing facade models. Open PRs #220
(RF extraction) and #221 (Rustdoc theme) do not overlap these consumer paths and
were not incorporated. Ada Buffer registration PR #222 had already merged into
the fetched base; no Buffer functionality is added to this application.
During qualification, new open PR #223 (documentation qualification repair) was
also inspected; its six documentation-test paths do not overlap this task.

Read AGENTS, CONTRIBUTING, architecture/ABI and workflow policies, foundation task,
the existing consumer/tests, RUST-IR-016/017 reports, common and dedicated C2
metadata implementations and reference tests, the IR inventory schema and relevant
common/typed-channel inventory entries, and the pinned reference's provider,
consumer and required-interface documentation. The structured native/Ada inventory
has no classification or implementation-evidence delta for this application-only
change. Historical coverage and RUST-IR-016/017 reports remain untouched.

Exact changed-file inventory, relative to the isolated worktree:

1. `rust/ams-mel/examples/minimal_ir_consumer.rs`
2. `rust/ams-mel/tests/minimal_ir_consumer.rs`
3. `rust/ams-mel/tests/minimal_ir_consumer/policy.rs`
4. `docs/rust-ir-018-eight-callback-consumer.md`

Final commit/tree and hosted synthetic-merge identities belong in the delivery PR
outside their own hashed source. **HOSTED_CI_PENDING** until exact-final-head
Bootstrap, Documentation and synthetic-merge qualification complete. Independent
technical review is required; the PR must remain open, unmerged, without
auto-merge or self-approval.

## Registration, ownership and execution

The CLI and exit codes remain 0 (complete contract), 1 (operation/contract failure)
and 2 (usage error). The existing executable retains `#![forbid(unsafe_code)]`,
public `ams-mel` plus std only, strict vendor/API/revision identity, discovery-driven
attachment of four distinct identities, four attached capability checks, zero
Image/Stacked layouts, four enables and four completed/closed KeepAlive requests.
No second executable, application queue, callback implementation, worker, thread,
Buffer API or acquisition path is introduced.

Before enabling any channel or submitting CommsTest, register:

1. Dedicated C2 `open_metadata(16)`: BIT_Configuration, CommandStatus, BIT_Status.
2. That same C2 stream's `enable_comms_test_events()`.
3. Health typed `open_metadata(16)`: all six existing required families.
4. Image typed `open_metadata(16)`: all four existing required families.
5. Health View `open_comms_metadata(16)`.
6. Existing control-only Image View `open_comms_metadata(16)`.
7. StackedImage View `open_comms_metadata(16)`.

Thus 13 typed required registrations plus four common CommsTest families = **17**.
C2 does not open a second common stream. All registration operations propagate
failure with `?`; none is conditional or silently skipped. The application's
additional `Operation<T>` conversion for `CommsMetadataOpenError` prints the
complete native/safe error Debug representation (including diagnostic metadata)
and the separate actual optional provider registration Return. `None` stays None;
no Return is inferred from text. No public error type is changed.

Four independent receiver ownership paths remain live through submission, future
completion, metadata Receive, counters, empty-queue checks and typed channel cleanup:

| Destination | Receiver owner | Report type |
|---|---|---|
| C2 | `c2_metadata`, dedicated typed C2 stream | only `C2MetadataEvent::CommsTest` accepted |
| Health | `health_comms`, from Health View | `CommsTestReport` |
| Image | `image_comms`, from control-only Image View | `CommsTestReport` |
| StackedImage | `stacked_comms`, from Stacked View | `CommsTestReport` |

The native report contains only command/request IDs. Channel association comes
from the selected receiver, **not** a manufactured report channel field. Pure
application helpers build the requests and check callback IDs against that receiver
and submission path independently of `comms_result()`'s future check.

### Identifier and correlation policy

Unlike RUST-IR-016's intentionally identical direct/routed IDs, this increment
keeps direct IDs and adds 10 to each routed command/request ID. All eight pairs
are unique; all request/report values remain `u32`, without narrowing.

| Destination | Stub address | Direct IDs | Routed IDs | Completed futures | Received callbacks |
|---|---:|---|---|---:|---:|
| C2 | 2 | 100 / 200 | 110 / 210 | 2 | 2 |
| Health | 4 | 101 / 201 | 111 / 211 | 2 | 2 |
| Image | 1 | 102 / 202 | 112 / 212 | 2 | 2 |
| StackedImage | 6 | 103 / 203 | 113 / 213 | 2 | 2 |
| **Total** | | | | **8** | **8** |

Only this pinned provider documents numeric ChannelType routing addresses. This
is not a general MEL UUID conversion. For each destination: submit direct, wait
for exact completed correlation, explicitly close future, receive/check that
destination's actual callback; then repeat with the routed request through C2.
Each future is checked using existing safe `submit_comms_test` and `comms_result`.
Each Receive uses finite `WAIT_MS=5000`, including C2, even though this provider
delivers callbacks synchronously. Completion never substitutes for reception.
Timeout does not cancel provider work. Send and Close calls themselves are not
bounded by that wait constant.

### Counters, empty queues and stub behavior

After the eight actual events, all four native snapshots must be exactly:

| Receiver | events_received | events_dropped_queue_full | malformed_or_unsupported |
|---|---:|---:|---:|
| Dedicated C2 | 2 | 0 | 0 |
| Common Health | 2 | 0 | 0 |
| Common Image | 2 | 0 | 0 |
| Common StackedImage | 2 | 0 | 0 |

Every active stream then receives a nonblocking empty-queue observation requiring
`ErrorKind::Timeout`. Extra events, dropped events, malformed reports, missing
events, wrong IDs/receiver/path, unexpected C2 variants or incorrect counters fail
the contract. Dedicated C2 also retains the separate sensor queue observation;
Health and Image typed sensor streams must remain empty with `(0,0,0)` counters.
Common CommsTest is not health telemetry, sensor metadata or an image frame.

Unchanged command contract:

- BIT 300/NoOp: `Completed(NotImplemented)`, no BIT performed.
- ConfigSet 301/zero timestamp/empty opaque config:
  `Completed(NotImplemented)`, no configuration applied.
- Mode 302/Standby/Unused/default scan: explicit `InvalidState` rejection and
  nonempty provider diagnostic.
- Image NavigationReport NotSet/neutral fields: explicit `InvalidState` rejection,
  not INS alignment.
- Control-only Image Receive(0): established InitializationFailed/control-only
  diagnostic, no frame acquired. All three image acquisition counters stay zero.

### Explicit cleanup

Each KeepAlive, CommsTest, BIT, ConfigSet, Mode and Navigation request is explicitly
closed after waiting, before proceeding past its result check. After the contract:

1. Stop control-only Image.
2. Close C2, Health, Image and StackedImage Views, in that order.
3. Close typed C2, Health, Image and StackedImage owners, in that order, using their
   existing lifecycle operations.
4. Close dedicated C2, Health and Image typed metadata owners.
5. Close common Health, Image and StackedImage CommsTest receiver owners.
6. Close Session.
7. Only after successful cleanup, print final communications parity and completion.

All Close errors propagate. Early failure uses established best-effort Drop, not a
new release engine or a claim of completed provider unload. Common stream Close
deactivates consumption; it does not unregister upstream callbacks, cancel futures
or establish callback quiescence. Native provider retention conventions remain
unchanged.

## Reference-provider qualification and reproduction

Cloned the actual `https://github.com/zackboll/minimal-mel-example` into clean
`/home/zboll/git/ams-rust-ir-018-reference`; checked detached commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`, using Git objects and clean status.
No reference files were modified. `src/provider.cpp` confirms four required
types, per-channel common callback registration, synchronous copied-ID report
delivery outside provider locks, and C2 routing to the addressed attached channel.
Its `complete_comms` requires the destination's numeric ChannelType; the header
values and C++ consumer establish routing 2/4/1/6. Zero layouts and absent sensor
publication are explicit stub semantics, not invented Rust behavior.

Toolchains: Debian GCC/G++ `14.2.0-19`; Rust `1.98.1`
(`48a229ceaefd4985c50990b14116b6d856af0985`, LLVM 22.1.8);
Cargo `1.98.1 (797e8a9bc 2026-08-05)`, x86_64-unknown-linux-gnu.
Aggregate uses installed GNAT 16.1.0 and GPRbuild package 26.0.1
(banner 26.0.0), and pinned GNATformat 26.0.0.

```sh
cmake -S /home/zboll/git/ams-rust-ir-018-reference \
  -B /tmp/rust-ir-018-reference-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/rust-ir-018-reference-build --parallel 2
ctest --test-dir /tmp/rust-ir-018-reference-build --output-on-failure
```

Complete C++ reference CTest: **4/4 passed**: provider_contract, dynamic_load,
consumer_required_api (the actual C++ consumer), upstream_declarations.

Production interoperability uses `native/build` with `AMS_MEL_BUILD_TESTS=OFF`,
never the test facade. Test/mock qualification uses a separate Cargo target and
`native/build-tests` with tests enabled. Production example commands:

```sh
cd /home/zboll/git/ams-rust-ir-018
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib"
export CARGO_TARGET_DIR=/tmp/rust-ir-018-target-reference
export AMS_MEL_REFERENCE_PROVIDER=/tmp/rust-ir-018-reference-build/libminimal_ir_mel_provider.so
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
for mode in debug release; do
  flags=(); [ "$mode" != release ] || flags=(--release)
  cargo build "${flags[@]}" --manifest-path rust/Cargo.toml -p ams-mel \
    --example minimal_ir_consumer
  export AMS_MEL_CONSUMER_EXE="$CARGO_TARGET_DIR/$mode/examples/minimal_ir_consumer"
  "$AMS_MEL_CONSUMER_EXE" "$AMS_MEL_REFERENCE_PROVIDER"
  cargo test "${flags[@]}" --manifest-path rust/Cargo.toml -p ams-mel \
    --test minimal_ir_consumer reference_executable -- --ignored --nocapture
  cargo test "${flags[@]}" --manifest-path rust/Cargo.toml -p ams-mel \
    --test ir_common_comms_reference -- --ignored --nocapture
  LD_DEBUG=libs "$AMS_MEL_CONSUMER_EXE" "$AMS_MEL_REFERENCE_PROVIDER"
done
```

Both Debug and Release executable runs: **exit 0, empty stderr**, byte-identical
stdout. Both opt-in executable tests pass, including existing CLI status checks
and wrong-profile mock rejection/early-Drop negative control. That mock is not
reference-provider qualification. Both unchanged RUST-IR-017 actual-provider tests
also pass. Loader traces in both modes confirm `calling init` for:

```text
/home/zboll/git/ams-rust-ir-018/native/build/lib/libams_mel_c.so.0
/tmp/rust-ir-018-reference-build/libminimal_ir_mel_provider.so
```

`readelf -d` confirms the executable needs `libams_mel_c.so.0` with production
RUNPATH, not a statically linked mock provider. Full local evidence is retained
under `/tmp/rust-ir-018-evidence`; persistent transcripts and final artifact
identities are recorded below and on the delivery PR.

## Qualification commands and focused coverage

Native gates were run sequentially (`make native`, then `make test-native`):
**799/799 passed**, including genuine C translation units. No native build trees
were concurrently mutated. The following Rust gates ran sequentially with:

```sh
export AMS_MEL_NATIVE_LIB_DIR=/home/zboll/git/ams-rust-ir-018/native/build-tests/lib
export AMS_MEL_TEST_PROVIDER_DIR=/home/zboll/git/ams-rust-ir-018/native/build-tests/test-providers
export CARGO_TARGET_DIR=/tmp/rust-ir-018-target-tests
cargo test --manifest-path rust/Cargo.toml --workspace
cargo check --manifest-path rust/Cargo.toml --workspace --all-targets
cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
cargo fmt --manifest-path rust/Cargo.toml --all -- --check
RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test minimal_ir_consumer --test ir_common_comms_metadata \
  --test ir_c2_metadata --test c2 --test ir_health --test ir_health_metadata \
  --test ir_image_control --test ir_image_metadata --test ir_stacked \
  --test common_channel --test common_boundary
make test-policy
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

Initial workspace: **608 passed, 0 failed, 15 intentionally ignored**.
Focused application/common/C2/Health/Image-control/Stacked/Common-identity suite:
**157 passed, 0 failed, 7 intentionally ignored**. Raw ABI/drift suite within the
workspace: **55 passed**. Check, Clippy with `-D warnings`, fmt and warnings-denied
Rustdoc all exit 0. Policy: **263 tests in 10 suites, all passed**; expected
negative-control diagnostics inside those tests are not gate failures. Integrity:
**825 vendor files, 4 workflows**, pass. No ordinary test requires the reference
provider or build-time downloads. Opt-in/helper ignores are not counted as passes.

The focused minimal consumer policy suite is **18 passing tests** in Debug and
Release. Its pure checks exercise missing/ambiguous/reused discovery identity,
zero-layout/neutral-navigation/stub-return policy, distinct direct/routed IDs,
fixed provider-specific routing, all eight unique ID pairs, independent future
and callback failures, wrong command/request IDs, wrong receiver and submission
path, missing callbacks despite successful futures, duplicate/extra callbacks,
counter 1/3 versus 2, dropped/malformed events, mandatory pre-enable registrations,
dedicated C2 use, explicit cleanup ordering and forbidden unsafe code. Source
checks bind the small pure checks to the actual receiver selection/registration
sites. Native callback lifecycle tests from RUST-IR-017 are reused unchanged,
not duplicated as another application framework.

Aggregate `make check` **passed, exit 0** with conflicting native/Cargo/loader/GPR
overrides removed and the installed pinned toolchains prepended to PATH:

```sh
unset AMS_MEL_NATIVE_LIB_DIR AMS_MEL_NATIVE_LIB AMS_MEL_TEST_PROVIDER_DIR \
  CARGO_TARGET_DIR LD_LIBRARY_PATH GPR_PROJECT_PATH CC CXX CFLAGS CXXFLAGS LDFLAGS
export PATH=/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH
make check
make test-native
```

Aggregate verifies native tree isolation and **799/799** native tests, pinned Ada
formatting, **70/70** structured inventory/audit regression tests, **52 Ada contract
PASS groups**, final newlines and whitespace. The existing imported native GPR
project emits three metadata warnings that it has no C sources; no new compiled
source warning is suppressed. No Ada sources are changed, so no Ada formatting
rewrite is performed. Aggregate's Alire compiler-path transitions rebuild native
artifacts; the full native tree is restored with `make test-native` before rerunning
the Rust gates and production-provider Debug/Release qualification. Final restored
counts and artifact identities follow. Restored native **799/799**; restored
workspace **608 passed, 0 failed, 15 ignored**; restored focused **157 passed,
0 failed, 7 ignored**; all-target check, Clippy, fmt, Rustdoc, policy **263/263**
and final source checks pass again. Post-aggregate production Debug and Release
direct runs and both opt-in tests pass again, including loader verification and
the wrong-profile mock negative control; stdout remains byte-identical to the
initial transcript. No qualification command remains blocked locally.

Final SHA-256 identities after aggregate/restoration:

| Artifact | SHA-256 |
|---|---|
| Production facade `native/build/lib/libams_mel_c.so.0` | `3c61b9075682744da065727eff9eabd465d0400d33c1862f97598388d4eb7551` |
| Test facade `native/build-tests/lib/libams_mel_c.so.0` | `4f1d34abb0f40f91f8b94a023207823f449ea7d67944e0a2e8acb6c246d91186` |
| Actual reference provider DSO | `f5d1380d4a7edae8a93986124776d63f19048b4086fdb383032543452d87f8a8` |
| Debug example | `5b629b590571ac5cc42918d2788156c95928d3faa0cb9cde4f72da7c2781c6e6` |
| Release example | `f654de3c6ab50d667515ac93903358db593b201223d79f44e1281d63acee20e5` |
| Debug and Release complete stdout, each 8090 bytes | `f2c6e9ed950f194675817b66fae2b43bc09cd58efb4185e9ce77a0a8a246e7cb` |

The initial pre-aggregate production facade hash was
`64713e9873b2555fabd0865c08d751652b6cd32ee6ea20205eaf835e555b2cda`;
the final source-identical facade rebuilt by aggregate is the one listed above
and used for the repeated production-provider observations. The unchanged C++
reference consumer was also executed directly: exit 0, empty stderr, reporting
eight callbacks, 17 registrations, no sensor reports or frames, and successful
required API cleanup.

## Debug and Release observation transcript

Initial focused tests exposed two whitespace-sensitive source assertions (rustfmt
split Close and C2 Receive method chains). The assertions were corrected to use
whitespace-normalized source; runtime checks were not weakened. The draft report
also initially lacked a final newline, caught by the repository checker and fixed.
These drafting failures were not counted as qualification passes.

Both modes produced the following same contract transcript (the preceding lines
print the strict reference identity, four complete advertised capability records,
and four attached identity/layout reports). Both stderr files are empty and both
exit statuses are 0. Complete stdout, including capabilities, is also retained on
the delivery PR; no future result is counted as a metadata reception line.

```text
[OK] required metadata registered: C2=3, Health=6, Image=4; CommsTest=4 (dedicated C2 plus Health/Image/StackedImage common); total=17 (not sensor measurements)
[OK] C2/Health/StackedImage enabled, Image started (enablement only, no acquisition buffers)
[STUB] Image receive(0): established control-only error; no frame acquired
[OK] Stub C2 KeepAlive: Completed { value: Success }
[OK] Stub health KeepAlive: Completed { value: Success }
[OK] Stub single-detector image KeepAlive: Completed { value: Success }
[OK] Stub stacked-image interface KeepAlive: Completed { value: Success }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 100, request_id: 200 } }, request=CommsTestRequest { command_id: 100, channel_id: 2, request_id: 200 }, destination=C2
[OK] actual C2 CommsTest metadata (direct): command_id=100 request_id=200
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 110, request_id: 210 } }, request=CommsTestRequest { command_id: 110, channel_id: 2, request_id: 210 }, destination=C2
[OK] actual C2 CommsTest metadata (C2-routed): command_id=110 request_id=210
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 101, request_id: 201 } }, request=CommsTestRequest { command_id: 101, channel_id: 4, request_id: 201 }, destination=Health
[OK] actual Health CommsTest metadata (direct): command_id=101 request_id=201
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 111, request_id: 211 } }, request=CommsTestRequest { command_id: 111, channel_id: 4, request_id: 211 }, destination=Health
[OK] actual Health CommsTest metadata (C2-routed): command_id=111 request_id=211
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 102, request_id: 202 } }, request=CommsTestRequest { command_id: 102, channel_id: 1, request_id: 202 }, destination=Image
[OK] actual Image CommsTest metadata (direct): command_id=102 request_id=202
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 112, request_id: 212 } }, request=CommsTestRequest { command_id: 112, channel_id: 1, request_id: 212 }, destination=Image
[OK] actual Image CommsTest metadata (C2-routed): command_id=112 request_id=212
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 103, request_id: 203 } }, request=CommsTestRequest { command_id: 103, channel_id: 6, request_id: 203 }, destination=StackedImage
[OK] actual StackedImage CommsTest metadata (direct): command_id=103 request_id=203
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 113, request_id: 213 } }, request=CommsTestRequest { command_id: 113, channel_id: 6, request_id: 213 }, destination=StackedImage
[OK] actual StackedImage CommsTest metadata (C2-routed): command_id=113 request_id=213
[STUB] BIT_Command 300/NoOp: Completed { value: NotImplemented }; no BIT performed
[STUB] ConfigSetCommand 301/zero timestamp/empty config: Completed { value: NotImplemented }; no configuration applied
[STUB] ModeCmd 302/Standby/Unused/default scan: Rejected { code: InvalidState, description: "ModeCmd: required sensor operation is a stub; hardware is not connected" }
[STUB] NavigationReport NotSet/neutral values: Rejected { code: InvalidState, description: "NavigationReport: required sensor operation is a stub; hardware is not connected" }; not an INS fix or delivered alignment
[OK] C2 CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0
[OK] C2 CommsTest queue empty (active Timeout)
[OK] Health CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0
[OK] Health CommsTest queue empty (active Timeout)
[OK] Image CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0
[OK] Image CommsTest queue empty (active Timeout)
[OK] StackedImage CommsTest counters: events_received=2 events_dropped_queue_full=0 malformed_or_unsupported=0
[OK] StackedImage CommsTest queue empty (active Timeout)
[STUB] Image acquisition counters: frames_received=0 frames_dropped_queue_full=0 malformed_or_unsupported=0
[STUB] all sensor metadata queues empty at nonblocking observation; registration is not telemetry
[OK] StackedImage explicitly closed (typed disable/detach)
[OK] requests, Views, typed channels, metadata and Session explicitly closed
[OK] CommsTest parity: 8 correlated futures and 8 independently received callbacks across 4 channels
[OK] four-channel reference-stub contract complete; no full C++ parity or hardware qualification
```

## Remaining required-interface parity gaps

The parity gaps below remain despite these successful communications observations.

| C++ reference capability | Safe Rust consumer |
|---|---|
| Four required channel attachments | Exercised |
| Four attached capability inspections | Exercised |
| Four enable operations | Exercised |
| Four KeepAlive requests | Exercised |
| Eight CommsTest futures | Exercised |
| Eight actual CommsTest metadata callbacks | Exercised |
| Required C2 commands | Exercised |
| Image NavigationReport | Exercised |
| General host Buffer APIs | Not yet exposed in safe Rust |
| Channel Buffer registration/unregistration | Not yet exposed in safe Rust |
| Optional Stacked LOS3D | Not exercised |
| Image/Stacked acquisition | Not available in the unconfigured stub |

No binding defect or unresolved upstream semantic was worked around. The remaining
gaps require separate tasks; this application does not claim full C++ parity or
hardware functionality.

---

## RUST-IR-018-R1 — corrected-main integration (2026-11-15)

### Source integration and historical blocker

This section records fresh integration evidence; the earlier counts, artifacts,
review and failed hosted run are historical, not current qualification. Existing
PR [#224](https://github.com/zackboll/ams-gra-mel-bridge/pull/224), branch
`feature/rust-ir-018-eight-callback-consumer`, and clean worktree
`/home/zboll/git/ams-rust-ir-018` were reused. All remote references were fetched
before integration. Main had advanced beyond the task-preparation snapshot to
`43f8acc8843a907dd91150961bc94023ec23c414`; the feature was one commit ahead and
49 behind. No newer feature work or unrelated worktree was overwritten.

Ordinary merge `c21cabc470c4a0c0b06f333400c179fe10de69b0` has parents
`55a5d19673a2a1044cf9c54a364c6af3cd631638` and
`43f8acc8843a907dd91150961bc94023ec23c414`. Git's ordinary `ort` merge completed
without conflicts. No consumer compatibility correction was necessary: all three
consumer implementation/test files remain byte-identical to the original PR
HEAD. The final diff against main remains the original four consumer-owned paths
listed above; this R1 adds only evidence to this task document.

The historical Bootstrap failure
[43677846726](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43677846726)
on `55a5d19673a2a1044cf9c54a364c6af3cd631638` remains a **failure**. Its private
native-test lifetime-log cleanup blocker was corrected separately by merged
PR [#229](https://github.com/zackboll/ams-gra-mel-bridge/pull/229), merge
`5b8ec76bc7a5ce27558988c619e96e5dce6159e7`. That merge is an ancestor of this
integration. The canonical
`rust/ams-mel-ir/src/ir_comms_metadata/tests.rs` is byte-identical to integrated
main (Git blob `7f828319f96ff23f54f271f44912718f4837e2ed`). The retired
`rust/ams-mel/src/ir_comms_metadata` module is absent; no duplicate fix was added.

`ams-mel-ir` remains the canonical IR implementation, depending only on Common
and sys. `ams-mel` retains exact concrete-type compatibility re-exports; the
consumer deliberately retains `use ams_mel::*`. Direct IR, IR/RF crate-boundary,
Common identity, raw ABI/drift and existing compatibility tests are part of the
fresh five-crate workspace run. No production native source, ABI, exported symbol,
binding, provider fixture, vendor source, workflow, policy or shared inventory is
changed by R1. Application-only integration has no native/Ada inventory coverage
delta; the current authoritative inventory is checked, not replaced by prose.

### Corrected process-isolated completion controls

The focused canonical IR parent was executed normally and with single-CPU
affinity against the contract-test facade:

```sh
cd /home/zboll/git/ams-rust-ir-018
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
export CARGO_TARGET_DIR=/home/zboll/git/ams-rust-ir-018-r1-evidence/cargo-contract
cargo test --manifest-path rust/Cargo.toml --locked -p ams-mel-ir --lib \
  ir_comms_metadata::tests::native::isolated_native_controls -- --exact --nocapture
CPU=$(python3 -c 'import os; print(min(os.sched_getaffinity(0)))')
taskset -c "$CPU" cargo test --manifest-path rust/Cargo.toml --locked \
  -p ams-mel-ir --lib ir_comms_metadata::tests::native::isolated_native_controls \
  -- --exact --nocapture
```

Both passed, **one parent / eight successful child processes per invocation**.
CPU 0 was selected from the observed allowed mask `0-31`, not assumed. The
parent checks whole child-process exit status as well as the completion marker.
Reviewed assertions remain active: exact worker/created/final-owner counts
(saturation 4, C2 and parked completion 1, other controls 0), bounded native
owner-notification waiting, lifetime-log use before provider DSO pin release,
positive unload before directory removal, deterministic parked-completion strict
waiter, saturation full-width counters and original FIFO assertions. No retries,
ignored parent, disabled probe, timeout change or assertion weakening was used.
This verifies the narrow last-log-use/owner-notification boundary, not a detached
thread join or general provider callback-quiescence guarantee.

### Fresh production/reference interoperability

The actual upstream checkout at `/home/zboll/git/ams-rust-ir-018-reference` has
remote `https://github.com/zackboll/minimal-mel-example.git`, a clean tree and
unchanged pinned HEAD `cf22a80ec98398eb361df758ada7bfb4024710a3` after fetching.
No reference source was modified. Fresh isolated build and C++ consumer:

```sh
E=/home/zboll/git/ams-rust-ir-018-r1-evidence
cmake -S /home/zboll/git/ams-rust-ir-018-reference -B "$E/reference-build" \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build "$E/reference-build" --parallel 2
ctest --test-dir "$E/reference-build" --output-on-failure
"$E/reference-build/minimal_ir_mel_consumer" \
  "$E/reference-build/libminimal_ir_mel_provider.so"
cmake -S /home/zboll/git/ams-rust-ir-018/native -B "$E/production" \
  -DCMAKE_BUILD_TYPE=Debug -DAMS_MEL_BUILD_TESTS=OFF
cmake --build "$E/production" --parallel 2
```

Reference CTest **4/4 passed**; C++ consumer exited 0 with empty stderr and
reported eight callbacks, 17 registrations, no frames/sensor telemetry and
successful cleanup. The isolated production facade cache confirms tests **OFF**;
`nm -D` finds no `ams_mel_test_*` exports. Its build tree and Cargo target are
separate from all mock-provider/contract testing and aggregate tree switching.

With `AMS_MEL_NATIVE_LIB_DIR="$E/production/lib"`,
`CARGO_TARGET_DIR="$E/cargo-production"`,
`AMS_MEL_REFERENCE_PROVIDER="$E/reference-build/libminimal_ir_mel_provider.so"`
and mock-negative fixtures selected only through
`AMS_MEL_TEST_PROVIDER_DIR=/home/zboll/git/ams-rust-ir-018/native/build-tests/test-providers`,
the following ran in both Debug and Release (adding `--release` for Release):

```sh
cargo build --manifest-path rust/Cargo.toml --locked -p ams-mel \
  --example minimal_ir_consumer
export AMS_MEL_CONSUMER_EXE="$CARGO_TARGET_DIR/debug/examples/minimal_ir_consumer"
"$AMS_MEL_CONSUMER_EXE" "$AMS_MEL_REFERENCE_PROVIDER"
cargo test --manifest-path rust/Cargo.toml --locked -p ams-mel \
  --test minimal_ir_consumer
cargo test --manifest-path rust/Cargo.toml --locked -p ams-mel \
  --test minimal_ir_consumer reference_executable -- --ignored --nocapture
cargo test --manifest-path rust/Cargo.toml --locked -p ams-mel \
  --test ir_common_comms_reference -- --ignored --nocapture
cargo test --manifest-path rust/Cargo.toml --locked -p ams-mel \
  --test ir_stacked_reference -- --ignored --nocapture
LD_DEBUG=libs "$AMS_MEL_CONSUMER_EXE" "$AMS_MEL_REFERENCE_PROVIDER"
```

For Release, `AMS_MEL_CONSUMER_EXE` selects `release/examples/minimal_ir_consumer`.
Each direct executable run exited **0**, with **empty stderr**. Debug and Release
stdout are byte-identical; comparison with the historical qualified output is
identical after normalizing only the supplied provider path. Each reports:

- All **17** mandatory registrations before enablement/submission.
- **Eight** exact correlated futures, followed independently by **eight** actual
  destination receiver dequeues, using the unchanged direct/routed ID table.
- Four exact receiver counters **(2,0,0)** and four active nonblocking empty
  **Timeout** observations.
- Existing BIT/Config `NotImplemented`, Mode/Navigation `InvalidState` diagnostic
  checks, zero acquisition and explicit successful cleanup.

The actual executable, not its printed summary, performs every future wait,
request-owner close, destination receive and correlation check. Existing
failure-semantic policy tests remain unchanged: **18 passed** per profile, with
the opt-in executable initially ignored and then explicitly run (**1 passed**).
That opt-in run includes wrong-profile mock rejection, lifetime-log cleanup,
usage and absent-provider controls. Common eight-callback and Stacked reference
tests each additionally passed (**1 per profile per test**). No mock run is
substituted for reference-provider interoperability.

Both loader traces show `calling init` for exactly the intended facade
`$E/production/lib/libams_mel_c.so.0` and pinned provider
`$E/reference-build/libminimal_ir_mel_provider.so`, and corresponding finalizers.
`readelf -d` confirms the executable's production-only RUNPATH. Loader-enabled
stdout equals the ordinary run; loader diagnostics are kept separate from the
ordinary empty-stderr requirement. SHA-256 identities:

| Artifact | SHA-256 |
|---|---|
| Isolated production facade | `eae29432e3f4f26db7aabe4616fa7cb1dcc45d8c8b16293259fa2d2a9dcfb76f` |
| Pinned reference DSO | `b82c723eb26ecedf09a81563efaedb6496649ca0c5c2dfc1c6fd1de448bd8516` |
| Debug executable | `a179570a179a76c4bfcfa104f27457df40787caf392bca7cf1b97eb988c0a2a5` |
| Release executable | `5f0aa50a580689a9d4f4212e62734d38a38d5a36ecb3d617ff0be8bed8fc6664` |
| Debug and Release stdout, each | `467879ba80bd2b610bc07c45a980073f10f444642988258227e503b8788f9827` |

### Local and hosted qualification ledger

Qualification scripts, logs, binaries, loader traces, source identities and
machine-readable counts are outside the repository at
`/home/zboll/git/ams-rust-ir-018-r1-evidence`. Initial integrated-source native
qualification passed **821/821** and Debug workspace qualification passed
**616 passed / 0 failed / 15 expected ignored**, across 51 result groups including
doctests. These are fresh counts, not the earlier 799/608 totals.

The required sequence ran from `/home/zboll/git/ams-rust-ir-018`, with conflicting
native/loader overrides cleared, external `cargo-contract` target, and pinned
GNAT **16.1.0** / GPRbuild **26.0.1 package** (reports **26.0.0**) prepended to
PATH. Rust was **1.98.1**, Cargo **1.98.1**, Alire **2.1.1**. Commands/results:

| Command | Fresh result |
|---|---|
| `make native` | Passed, production tests OFF. |
| `make test-native` | **821/821 passed**, including C-compiled translation units. |
| `cargo test --manifest-path rust/Cargo.toml --workspace --locked` | **616 passed / 0 failed / 15 ignored**, 51 result groups; test facade/provider paths explicitly selected. |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets --locked` | Passed against production default. |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets --locked -- -D warnings` | Passed against production default. |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Passed. |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --locked --no-deps` | Passed, five crates. |
| `make check` | Passed: build isolation, native **821/821**, pinned GNATformat, **70/70** IR inventory regressions, **53 Ada smoke PASS groups**, separate minimal-consumer **14 negative controls**, newline/whitespace. |
| `make native`, then `make test-native` after aggregate | Both passed; complete provider/test tree restored, **821/821** native. |
| Post-restoration workspace Debug test | **616 passed / 0 failed / 15 ignored**, 51 groups. |
| `cargo test --manifest-path rust/Cargo.toml --workspace --release --locked` | **616 passed / 0 failed / 15 ignored**, 51 groups against test facade. |
| Release single-CPU canonical native-control parent | **1 passed**, all eight child cases, no retries. Ordinary Release parent also passes in workspace. |
| `make test-python` | **686/686 passed**, package compileall passed; prerequisite native **821/821** passed. |
| `make test-policy` | **302/302 passed in 13 suites**, including expected deliberate failing-CTest negative controls. |
| `make check-ir-api-coverage` | Passed: **127 headers / 148 operations**, **70/70** regressions; safe_ada=57, partial=11, missing=16, provider_only=64, native_only=0. |
| `python3 scripts/check_doc_source_integrity.py` | Passed: **825 vendor files / 4 workflows**. |
| `python3 scripts/check_final_newlines.py`; `git diff --check`; `git diff --cached --check` | Passed. |
| Additional `cargo test --manifest-path rust/Cargo.toml --workspace --all-targets --locked` against test facade | **578 passed / 0 failed / 15 ignored**, 47 groups (includes example targets, excludes doctests). |

The 15 default Rust ignores are **12 subprocess helper entries** (exercised by
their parents) and **three real-provider opt-ins**: common eight-callback,
Stacked foundation, and consumer executable. All three provider opt-ins were
explicitly executed successfully in both profiles above. Ignored entries are not
counted as passed. Crate-boundary evidence includes IR identity **2**, direct IR
API **2**, RF compatibility **1**, Common boundary **2**, and checkout ABI **1**
passing tests; these are included in, not added to, workspace totals. The
canonical IR unit group is **155 passed / 1 helper ignored**. Consumer policy is
also executed as the actual example test target: **18/18** in each profile.

Three existing imported-native-GPR metadata warnings about no C sources appeared
in aggregate; there was no first-party compiled-source warning suppression. No
Ada source changed and no formatting rewrite was made. Aggregate toolchain
switching was followed by full production/test-tree restoration before workspace
Debug/Release and Python qualification. The reference production tree is separate
and was never reconfigured by aggregate.

An additional, non-required `cargo test --workspace --all-targets --locked --no-run`
probe against the production facade failed to link private native tests because
production intentionally omits `ams_mel_test_*` probes (including completion,
Comms, Health, quaternion and ODM controls). This is **not a passed test** or a
reason to add probes to production: complete contract tests run against the
test facade; production all-target check/Clippy and actual consumer tests pass.
The production example was rebuilt and rerun afterward: exit 0, empty stderr,
stdout identical. Required qualification did not fail or require a source fix.

Final production Debug/Release executable runs were repeated after all aggregate
and restoration work: both exit 0, empty stderr and unchanged stdout. The
production all-target check was also repeated successfully. Hosted identities
are recorded in the qualification follow-up. Hosted qualification and independent final-head
review remain pending at source commit time. Final commit/tree and synthetic
merge identities must be recorded outside their own hashed source, in the PR
qualification comment. The old COMMENTED review is not final-head approval.
PR #224 must remain open and non-draft, unmerged, with auto-merge disabled. Only
the author was returned as an available collaborator; no self-review request or
fabricated approval substitutes for independent review. The review request must
cover all four receiver owners, eight independent future/callback checks, routing,
missing/duplicate/extra/misrouted failures, error propagation, cleanup order,
extracted-crate compatibility, real-provider evidence and corrected lifetime
controls. The unchanged stub cannot qualify image acquisition, physical hardware,
complete C++ parity or general callback quiescence.
