# RUST-IR-016 — four-channel minimal Rust IR MEL consumer

## Scope, source identities and preflight

This is a consumer integration increment using the already merged public safe
Rust Stacked API. It is four-channel attachment and communications parity, **not
full required-interface parity**, hardware qualification or acquisition support.
No native implementation, C ABI, raw sys declarations, safe library API, provider
fixture, Ada/Python, vendored source, CI or historical coverage inventory changes.
The structured inventory describes native/Ada implementation evidence and has no
classification delta for this application-only task.

Freshly fetched `origin/main` before creating worktree
`/home/zboll/git/ams-rust-ir-016` and branch
`feature/rust-ir-016-four-channel-consumer`, directly based on commit
`ef13833a59429d24031535c91bca8fedbf7ea9e4`, tree
`ed565b70b28775efa418d1cbf141b999669ae240`.
Verified merged prerequisites and ancestry:

- PR #205 (RUST-IR-014): `95404ddb5be293b2335be8aeb0d760f49cad24be`.
- PR #211 (RUST-IR-015): `ef13833a59429d24031535c91bca8fedbf7ea9e4`.

Open PRs #204/#208 (native common callbacks), #210 (Ada/native Buffer), #212
(documentation publication) and #213 (Ada documentation) were inspected along
with local/remote branches. None overlaps these consumer paths; none was adopted
or modified. Reviewed AGENTS, CONTRIBUTING, architecture/ABI policies,
provider-foundation task, IR inventory schema and relevant Stacked/common entries,
the exact application/tests, safe Stacked/discovery exports and the pinned C++
consumer, provider and consumer/required-interface documentation.

Exact changed-file inventory (relative to the worktree above):

1. `rust/ams-mel/examples/minimal_ir_consumer.rs`
2. `rust/ams-mel/tests/minimal_ir_consumer.rs`
3. `rust/ams-mel/tests/minimal_ir_consumer/policy.rs`
4. `docs/rust-ir-016-four-channel-consumer.md`

Final commit/tree and hosted synthetic-merge identities are recorded on the
delivery PR, avoiding a self-referential document hash. **HOSTED_CI_PENDING** until
final-head Bootstrap, Documentation and hosted merge-source qualification finish.

## Application sequence and boundaries

The existing executable, CLI, exit statuses (0 success, 1 contract/operation
failure, 2 usage error), `#![forbid(unsafe_code)]`, std/public `ams-mel` dependency
boundary and strict vendor/API/revision profile are preserved. No second app,
thread, executor or native cleanup engine is introduced.

1. Open Session; require vendor `minimal-mel-example`, API `4`, revision `1`.
2. Discover exactly four single-type capabilities with distinct channel UUIDs.
   Select C2, Health, Image and StackedImage using the existing `select()` helper.
3. Attach all four within that Session. The small `stacked_config()` helper copies
   the selected channel ID, platform ID and complete sensor location into
   `StackedConfig::new`; no provider UUID, label or location is hardcoded.
4. Inspect attached capabilities with the existing `attached()` identity/type and
   zero-layout check. Inspect typed Stacked capabilities and require complete
   equality of its common View snapshot with that typed snapshot.
5. Register the existing required metadata: C2=3, Health=6, Image=4, plus separate
   C2 common CommsTest. No optional Stacked LOS3D registration.
6. Enable C2, Health and Stacked; start control-only Image. Stacked enablement
   does not allocate buffers, install a listener or initiate acquisition.
7. Require the established control-only Image receive failure, then exercise
   four KeepAlive futures using the existing loop. Each must be
   `Completed { value: Success }` and each request is explicitly closed.
   This demonstrates local provider liveness, not sensor hardware health.
8. Exercise direct and C2-routed CommsTest for all four destinations. The existing
   submission/wait/close helper now shares its result check with policy tests.
   All eight futures must complete with exact command and request IDs.
9. Receive exactly two actual C2 metadata events independently of the futures;
   check their IDs. No Health/Image/Stacked common callback owner is exposed by
   the safe Rust API. Completed futures never manufacture metadata events.
10. Preserve BIT 300/NoOp and Config 301/zero timestamp/empty opaque configuration:
    both must complete `NotImplemented`. Mode 302/Standby/Unused/default scan and
    neutral NavigationReport/NotSet must explicitly reject with `InvalidState`
    and nonempty provider diagnostic. No BIT execution, configuration application,
    sensor acceptance, INS fix or transfer alignment is claimed.
11. Preserve zero frame counters, empty sensor metadata queues and exact counters
    (C2 received=2, Health/Image=0, dropped/malformed=0).
12. Stop Image; close all four common Views; close C2, Health, Image and Stacked
    typed owners; close existing metadata owners; close Session; only then report
    `four-channel reference-stub contract complete`.

Stacked Close delegates Disable/detach to the existing safe/native owner. No new
Disable API, cleanup retry policy or unload guarantee is added. A cleanup error
propagates with operation context and prevents success. Failure falls back to
the existing best-effort Drop, not a promise of successful physical teardown.
Logical Close does not establish immediate OS-level provider-library unloading.
Future waits remain finite (5 seconds), not deadlines on provider send/close;
timeout is not cancellation.

### Reference-only communications addresses

The pinned provider explicitly routes using numeric ChannelType values:

| Destination | Address | Command ID | Request ID | Direct | C2-routed |
| --- | ---: | ---: | ---: | ---: | ---: |
| C2 | 2 | 100 | 200 | 1 | 1 |
| Health | 4 | 101 | 201 | 1 | 1 |
| Image | 1 | 102 | 202 | 1 | 1 |
| StackedImage | 6 | 103 | 203 | 1 | 1 |

This is **not a universal MEL address rule or a UCI UUID conversion**. All IDs
remain full-width `u32`. Unknown channel tags are rejected, including Unknown(6).
The four zero-layout descriptors do not establish detector dimensions, pixel
representation, stack geometry or a usable acquisition format.

## Real-provider qualification

Isolated checkout `/home/zboll/git/ams-rust-ir-016-reference`, unchanged commit
`cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`.

Toolchains: Debian GCC/G++ 14.2.0-19; CMake 3.31.6;
rustc 1.98.1 (`48a229ceaefd4985c50990b14116b6d856af0985`, LLVM 22.1.8),
Cargo 1.98.1 (`797e8a9bc`), x86_64-unknown-linux-gnu.

Qualified the provider and C++ consumer first:

```sh
cd /home/zboll/git/ams-rust-ir-016-reference
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

All exit 0, **4/4 CTests passed**, including `consumer_required_api`.
The reference source/provider was not changed to accommodate Rust.

Production facade built with `make native` in the bridge worktree:
`native/build`, `AMS_MEL_BUILD_TESTS=OFF`, `CMAKE_BUILD_TYPE=Debug`,
C/C++ compilers `/usr/bin/cc` and `/usr/bin/c++`. Initial binary SHA-256:

```text
70be3c1b36f75d23b30a0ce65e37cdcc242a29c8ff63df5770af9a6b2f315675  native/build/lib/libams_mel_c.so.0
d0f2f58b783562cf9d6f04acd154e8b4dfde8f8c6d165bd6ee9217ae526a38c9  reference/build/libminimal_ir_mel_provider.so
```

Built and ran Rust Debug and Release against that production facade and actual
reference DSO, with a distinct Cargo target directory from contract tests:

```sh
cd /home/zboll/git/ams-rust-ir-016
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build/lib"
export CARGO_TARGET_DIR="$PWD/rust/target-production"
export AMS_MEL_REFERENCE_PROVIDER=/home/zboll/git/ams-rust-ir-016-reference/build/libminimal_ir_mel_provider.so
for mode in debug release; do
  flags=; test "$mode" = debug || flags=--release
  cargo build --manifest-path rust/Cargo.toml -p ams-mel \
    --example minimal_ir_consumer $flags
  "$CARGO_TARGET_DIR/$mode/examples/minimal_ir_consumer" \
    "$AMS_MEL_REFERENCE_PROVIDER"
  AMS_MEL_CONSUMER_EXE="$CARGO_TARGET_DIR/$mode/examples/minimal_ir_consumer" \
    cargo test --manifest-path rust/Cargo.toml -p ams-mel \
    --test minimal_ir_consumer $flags reference_executable -- --ignored --exact
  cargo test --manifest-path rust/Cargo.toml -p ams-mel \
    --test ir_stacked_reference $flags -- --ignored
done
```

Both executable runs exit **0**, stderr **0 bytes**, stdout **6748 bytes**;
`cmp` confirms byte-identical stdout. Both opt-in executable tests and both
unchanged RUST-IR-015 reference foundation tests pass (1 each).
The executable opt-ins are also rerun with `AMS_MEL_TEST_PROVIDER_DIR` pointing
at the separately built mock tree, to exercise the retained wrong-profile
negative control and its cleanup log. The mock is never the passing interop DSO.

`readelf -d` reports `NEEDED libams_mel_c.so.0` and RUNPATH
`/home/zboll/git/ams-rust-ir-016/native/build/lib`; `ldd` resolves exactly that
production facade. `LD_DEBUG=libs` confirms initialization of that facade and
`/home/zboll/git/ams-rust-ir-016-reference/build/libminimal_ir_mel_provider.so`.
The successful loader trace contains no `libmock_ir_provider.so`.

### Debug and Release transcript

Both runs produced the following identical operational stdout (the initial
provider path/version line and four verbose discovery descriptors are omitted
here; all four descriptors were zero-layout). Each run had empty stderr and
exit 0. Local full transcripts and loader trace are retained in
`/tmp/rust-ir-016-evidence`; stdout SHA-256 in both modes is
`b53edfa2f03dfc37a6080080f913f0d0f3b7e166bafec2e01ac2cd2255c7a394`.

```text
[OK] attached identity/layout: Stub C2
[OK] attached identity/layout: Stub health
[OK] attached identity/layout: Stub single-detector image
[OK] attached identity/layout: Stub stacked-image interface
[OK] required metadata registered: C2=3, Health=6, Image=4; C2 CommsTest separately registered (not sensor measurements)
[OK] C2/Health/StackedImage enabled, Image started (enablement only, no acquisition buffers)
[STUB] Image receive(0): established control-only error; no frame acquired
[OK] Stub C2 KeepAlive: Completed { value: Success }
[OK] Stub health KeepAlive: Completed { value: Success }
[OK] Stub single-detector image KeepAlive: Completed { value: Success }
[OK] Stub stacked-image interface KeepAlive: Completed { value: Success }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 100, request_id: 200 } }, request=CommsTestRequest { command_id: 100, channel_id: 2, request_id: 200 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 100, request_id: 200 } }, request=CommsTestRequest { command_id: 100, channel_id: 2, request_id: 200 }
[OK] actual C2 CommsTest metadata: CommsTestReport { command_id: 100, request_id: 200 }
[OK] actual C2 CommsTest metadata: CommsTestReport { command_id: 100, request_id: 200 }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 101, request_id: 201 } }, request=CommsTestRequest { command_id: 101, channel_id: 4, request_id: 201 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 101, request_id: 201 } }, request=CommsTestRequest { command_id: 101, channel_id: 4, request_id: 201 }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 102, request_id: 202 } }, request=CommsTestRequest { command_id: 102, channel_id: 1, request_id: 202 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 102, request_id: 202 } }, request=CommsTestRequest { command_id: 102, channel_id: 1, request_id: 202 }
[OK] direct CommsTest: Completed { report: CommsTestReport { command_id: 103, request_id: 203 } }, request=CommsTestRequest { command_id: 103, channel_id: 6, request_id: 203 }
[OK] C2-routed CommsTest: Completed { report: CommsTestReport { command_id: 103, request_id: 203 } }, request=CommsTestRequest { command_id: 103, channel_id: 6, request_id: 203 }
[STUB] Health/Image/Stacked common CommsTest callbacks not exposed in safe Rust; eight futures and two C2 events checked, not eight callbacks
[STUB] BIT_Command 300/NoOp: Completed { value: NotImplemented }; no BIT performed
[STUB] ConfigSetCommand 301/zero timestamp/empty config: Completed { value: NotImplemented }; no configuration applied
[STUB] ModeCmd 302/Standby/Unused/default scan: Rejected { code: InvalidState, description: "ModeCmd: required sensor operation is a stub; hardware is not connected" }
[STUB] NavigationReport NotSet/neutral values: Rejected { code: InvalidState, description: "NavigationReport: required sensor operation is a stub; hardware is not connected" }; not an INS fix or delivered alignment
[STUB] all sensor metadata queues empty at nonblocking observation; registration is not telemetry
[OK] StackedImage explicitly closed (typed disable/detach)
[OK] requests, Views, typed channels, metadata and Session explicitly closed
[OK] four-channel reference-stub contract complete; no full C++ parity or hardware qualification
```

## Regression qualification

The application integration tests now run 14 policy checks, preserving the old
C2/Health/Image policies while adding discovery-driven Stacked configuration,
missing/ambiguous Stacked and duplicate identities, each nonzero layout field,
exact routing, full-width correlation, eight-operation/two-event distinction,
unexpected Stacked outcomes and unsafe-code prohibition. They test application
checks, not a duplicate implementation of Stacked lifecycle qualification.
The opt-in subprocess test checks actual four-channel output/correlation,
commands, empty telemetry, explicit cleanup and existing CLI/error paths.

Normal gates run sequentially (no simultaneous mutation of native build trees):

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
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test minimal_ir_consumer --test ir_stacked --test common_channel \
  --test ir_discovery --test ir_image_control --test c2 --test ir_health \
  --test ir_image_metadata --test ir_health_metadata --test ir_c2_metadata
cargo test --manifest-path rust/Cargo.toml -p ams-mel-sys
make test-policy
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
env -u AMS_MEL_NATIVE_LIB_DIR -u AMS_MEL_TEST_PROVIDER_DIR \
  -u CARGO_TARGET_DIR -u LD_LIBRARY_PATH \
  PATH="/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin:/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin:$PATH" \
  make check
# Restore full tests after aggregate's providers-only Ada configuration:
make test-native
cargo test --manifest-path rust/Cargo.toml --workspace
cargo test --manifest-path rust/Cargo.toml -p ams-mel \
  --test minimal_ir_consumer --test ir_stacked --test common_channel \
  --test ir_discovery --test ir_image_control
```

Initial results: native **656/656**, workspace **573 passed, 0 failed, 12 ignored**
(including **53 doctests**). Ten ignored entries are isolated subprocess entry
points exercised by parent scenarios; two are real-provider opt-ins, each run
explicitly in Debug and Release as above. Focused consumer/Stacked/C2/Health/Image/
discovery/common suite: **142 passed, 0 failed, 6 ignored**. Sys ABI/drift suite:
**55 passed**, including C11 layout/signature probes. Check, Clippy with warnings
denied, formatting and warnings-denied Rustdoc all exit 0. Policy suites:
**244 tests in 8 suites**, all pass. Source integrity: **825 vendor files,
4 workflows**, pass.

One initial focused command used the nonexistent target `ir_c2`; Cargo rejected
it before executing tests. Corrected to the repository's existing `c2` target
and reran the complete focused command successfully. A final-newline check caught
the new document's missing newline during drafting; corrected before final
qualification. Neither failure was treated as a test pass or required a contract
change. No Ada source is changed, so no Ada formatting rewrite is needed; the
aggregate checks pinned GNATformat. Aggregate uses GNAT 16.1.0 and GPRbuild package
26.0.1 (banner 26.0.0), with conflicting native/Cargo overrides removed.

Initial full `make check`: **PASS, exit 0**. Native build isolation passed;
native **656/656**, structured inventory **62/62**, pinned GNATformat 26.0.0
and all **49 Ada contract PASS groups** passed, then newline/whitespace checks.
The existing imported native GPR project emits three metadata warnings about no
C sources; no first-party compiled warning is suppressed. Restored full native
tree: **656/656**. Restored workspace: **573 passed, 0 failed, 12 ignored**;
focused application/Stacked/common/discovery/Image control: **87 passed,
0 failed, 2 ignored**. Restored all-target check, Clippy, fmt, Rustdoc and sys
suite passed; native Stacked regressions **46/46** passed. Repeated production
Debug/Release executable opt-ins (including wrong-profile mock) and direct runs
passed after aggregate/restoration; stdout still exactly matches the initial
transcript and stderr is empty. Aggregate rebuilt native artifacts using the
pinned environment; post-aggregate production/test SHA-256 respectively:

```text
5d617ebbc1db8461404ed50d32908ce5d3a5af26f2fbd9d804e3f60d8b0202e5
71f9cd5d76d336524cffd78cf77026fdc2b78343126b90a9b2c44cd6f6388cf0
```

These numbers describe the initially qualified source, not silently a later
integrated HEAD. During qualification main advanced through Ada documentation
PR #213 to `9d25891123d5bd85ed5b06316ff962536f7f071f`. It is incorporated by
ordinary merge; all gates and real-provider evidence are rerun for the integrated
source, with final identities/results on the delivery PR. The task diff against
updated main remains exactly the four listed files. No native or safe Rust API
changes are imported from the still-open callback/Buffer/publication PRs.

## Remaining parity boundaries

| C++ reference capability | Rust status after RUST-IR-016 |
| --- | --- |
| Four required channel attachments | Exercised |
| Four attached capability inspections | Exercised |
| Four channel enable operations | Exercised |
| Four KeepAlive futures | Exercised |
| Eight CommsTest futures | Exercised |
| Two C2 CommsTest metadata events | Exercised |
| Full eight-callback metadata parity | Not yet exposed |
| Required C2 command overloads | Exercised |
| Image NavigationReport | Exercised |
| General Buffer APIs | Not yet exposed |
| Channel Buffer registration/unregistration | Not yet exposed |
| Optional Stacked LOS3D callback | Not exercised |
| Stacked/image acquisition | Not applicable to the unconfigured stub |

Health/Image/Stacked verified common metadata events: **0**. Fabricated sensor
telemetry: **0**. No unresolved provider behavior was guessed; the remaining
callback and Buffer gaps require separate binding work.
