# IRMEL-ADA-016 — Minimal StackedImage channel ownership

## Base and scope

Freshly fetched base `0e4dbd8491ccd13d7dfadbd6ad36766687f5f4ab` includes merged
PR #184, followed by Python NavigationReport and Rust full BIT command work.
There were no open PRs at the initial check. Development starts from that main,
not the old Image feature branch, on
`feature/irmel-ada-016-stacked-channel-foundation`.

`AMS.MEL.IR.Stacked_Image` provides owned configuration, limited-controlled
channel ownership, Open, Is_Open, Enable, Capabilities, As_Channel and Close.
This is **not stacked-image acquisition or complete required-interface parity**.
StackedImage is a required channel-details interface even when a sensor cannot
acquire image stacks. No buffer factory, initialization, registration, listener,
stack callback, sample queue or inferred image format belongs in this foundation.

## Upstream identity and dependency closure

The frozen IR pin remains `8d9224519f12b44e0b28815755c56a32a28d24a0`.
Exactly one vendor file is added:

- `native/vendor/ir-mel/include/irmel/library/stacked-image/StackedImageChannel.h`
- Git blob `7e675aac4d2e7efd15fbdff90ba77d03795b3378`
- SHA-256 `e333f66d597ae337912ec5d1af975e5addac68d1ca1098a0584a94047197124b`

Bytes are decoded from the independently frozen Git evidence and compared with
its catalog SHA-256. All direct compilation dependencies already exist. Neither
StackedImageListener nor StackFrameHeader is added. Original bytes, notices,
licenses and source pin remain intact; vendor integrity contains 825 files.

## ABI and ownership contract

Source-derived production export union: **306 → 311**, with no removals. Exactly
five additive exports remain under `AMS_MEL_0.1`:

```text
ams_mel_ir_stacked_open
ams_mel_ir_stacked_enable
ams_mel_ir_stacked_get_capabilities
ams_mel_ir_stacked_close
ams_mel_ir_channel_from_stacked
```

The new opaque owner and `ams_mel_ir_stacked_config_v1` use the standard UCI and
component-location records. No existing record changes layout. Configuration
has identities, labels, location and channel type **6**, not image-buffer or queue
settings. Private Python ctypes and Rust sys mirror the complete ABI, without
adding safe Python/Rust APIs; compiled C11 probes compare signatures and layout.

Open checks Control discovery before attaching exactly once. Unsupported or
empty discovery returns INITIALIZATION_FAILED without poisoning the Session.
Null attachment returns FACTORY_FAILED. Discovery and concrete validation are
separate: a generic or Image channel advertising StackedImage cannot substitute
for `dynamic_pointer_cast<StackedImageChannel>`. The attached capability must
also list StackedImage. Publication is last. Failed rollback retains the exact
provider graph permanently, rather than unloading code with uncertain ownership.
Provider exceptions follow the existing typed-channel status policy, including
INITIALIZATION_FAILED after a proven open-time rollback.

Enable is explicit and idempotent; refusal/exception marks the lifecycle failed
and is not retried. Close calls inherited disable if Enable was attempted, then
detach. Failed detach retains the public handle for retry; disable is not
silently repeated. A disable failure remains an error even if detach succeeds.
Cleanup never equates successful disable with callback quiescence.

Common views hold weak state, not the typed owner or provider graph. Closing a
view cannot detach its typed owner. Closing the owner stops new view operations.
Admitted common requests own independent family-local claims and reuse the
unchanged common request/completion engines. Public closure does not cancel a
future. Final completion triggers physical cleanup; deferred detach failure
retains the exact graph permanently and reports an explicit completion error.
A cleared public handle therefore does not imply physical cleanup. Provider
calls run outside the lifecycle mutex; synchronous Enable/capability operations
reserve temporary claims so reentrant closure cannot detach mid-call. Serialize
public typed-owner access and Close externally, as with existing typed owners.

Capabilities reuse native `snapshot_capability` and Ada `Capability_Conversion`.
Snapshots own every field and collection, including zero layout and empty nested
collections, and remain usable after channel, Session and provider unload.

## Coverage delta and exclusions

The reviewed inventory and generated report classify StackedImageChannel and
the StackedImage profile **missing → partial**. Inherited exposure changes:

| Operation | Status |
| --- | --- |
| Enable, getCapabilities, KeepAlive, CommsTest | safe_ada (four exposures) |
| Disable | partial: Close cleanup only, no standalone operation |
| registerBuffer, unregisterBuffer, common CommsTest metadata registration | missing |
| Optional StackedImage LOS3D registration | missing |

The unique safe Ada operation count stays **50**: inherited exposure does not
create another upstream declaration. No frame reception, onImageStack,
StackFrameHeader conversion, application-managed buffers, optional LOS3D,
new formats, samples, services, DSP or provider compensation is implemented.
Historical `docs/coverage.md` is untouched.

## Qualification record

The sequential campaign passed 587/587 native contracts, including 29
StackedImage C11/layout cases. Earlier incremental runs passed 579/579 and
585/585 before the zero-layout, pending-pair and reentry cases were finalized.
The C contracts
use active checks in release builds, process isolation for uncertain graphs,
provider promise gates and completion-owner observations rather than sleeps.
Ada tests cover discovery-derived configuration, complete capability equality,
zero layout, common correlation identifiers, expired views, lifecycle errors,
retryable detach and non-raising finalization. Cross-family contracts remain in
the full suite.

| Command / audit | Actual result |
| --- | --- |
| `make format-ada`; `make check-ada-format` | PASS, pinned GNATformat 26.0.0. GPR emits its existing native-import project warning about no C sources; compiled first-party sources remain warning-clean. |
| `make test-native` | PASS, 587/587; 29 new StackedImage contracts, no compiler warnings. |
| `make test-python` | PASS, 539 tests; private raw signatures, layouts and duplicate-free 311-export equality preserved. |
| `make test-rust` | PASS, 460 passed, 0 failed, 9 ignored outer child entries; includes raw StackedImage ABI/layout and full inventory equality. |
| `alr -C ada/tests exec -- sh -c 'cd /home/zboll/git/ams-gra-mel-bridge && make test-ada'` | PASS, 49 PASS lines including StackedImage foundation, pending common futures, discovery/zero layout and existing channel-family regressions. |
| `make check-ir-api-coverage` | PASS, 61 controls; 127 headers, 148 unique operations, safe_ada remains 50. |
| `make test-policy` | PASS, including vendor inventory 825 and unchanged rejection controls. |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS. |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS. |
| C11 `stacked_layout_probe.c`, `-Wall -Wextra -Wpedantic -Werror` | PASS; LP64 size/alignment/offsets/pointer/type: `128 8 0 32 40 72 8 6`. |
| Existing C struct definitions compared with base | PASS, all 314 definitions byte-identical. |
| Production `nm -D --defined-only` versus export map | PASS, exactly 311 unique `@@AMS_MEL_0.1` symbols; zero test exports. |
| Header Git blob + SHA-256; compiler dependency closure | PASS; no StackFrameHeader or StackedImageListener reached. |
| Separate Release CMake build in `/tmp/irmel-016-release`, `ctest -R '^ir_stacked_'` | PASS, 29/29; all 29 repeated 20 times without failure under `NDEBUG`. |
| Pinned enhancer 0.1.5, `python -W error -m unittest discover -s docs/tests -p 'test_*links.py' -v` | PASS, 40 tests, including three new StackedImage visibility/type-link tests. |
| Pinned enhancer 0.1.5, `test_enhancer_release.py` | PASS, 5 tests. |
| `check_doc_source_integrity.py`; `check_final_newlines.py`; `git diff --check` | PASS. |

Ada builds use GNAT 16.1.0 / GPRbuild 26.0.1 through the existing local Alire
pins. The sequential campaign's `make check` and final Alire command results
are recorded below when complete; no simultaneous commands modify its build
trees. Logs are local `/tmp/stacked-final-*.log`, with separate Release,
symbol/layout and pinned documentation checks.

Two fetched-main files lacked their final newline:
`docs/mel-gnatdoc-015-001-official-release.md` and
`docs/tests/test_enhancer_release.py`. Their only changes are adding that byte;
no historical prose or test logic is altered. Initial documentation-test
attempts with system Python lacked the enhancer; a discovered older development
environment was not treated as pinned qualification. The successful results
above use the verified official 0.1.5 environment. An initial Alire `exec make`
attempt ran from the crate directory and found no target; the recorded commands
explicitly return to the repository root. The coverage gate also caught and
prompted removal of an incorrect association between typed capability evidence
and Control discovery; its equality checks were not weakened.

The optional private `minimal-mel-example` checkout was not found locally;
real-provider integration is **UNTESTED**, not passed. Mock evidence is not
hardware, stack-acquisition or complete C++ consumer parity evidence.

## Reviewed file boundaries

- Ada implementation: `ada/src/ams-mel-ir-stacked_image.ads`, `.adb`, and the
  private imports in `ada/src/ams-mel_c_api.ads`.
- Ada tests: `ada/tests/src/ams_mel_ir_stacked_tests.ads`, `.adb`, wired into
  `ada/tests/src/ams_mel_smoke.adb`.
- Native: `native/src/ir_stacked.cpp`, the generic view conversion in
  `native/src/common_channel_requests.cpp` and its internal declaration,
  `native/include/ams_mel/abi.h`, `native/src/exports.map`, `native/CMakeLists.txt`.
- Contracts: `native/tests/mock_provider.cpp`,
  `native/tests/test_ir_stacked_foundation.c`, `native/tests/stacked_layout_probe.c`.
- Private ABI mirrors/tests: `python/ams_mel/_native.py`,
  `python/tests/test_abi.py`, `rust/ams-mel-sys/src/lib.rs`,
  `rust/ams-mel-sys/tests/abi.rs`, `rust/ams-mel-sys/tests/stacked_abi.rs`.
- Source/coverage evidence: the single vendor header identified above,
  `docs/upstream-files.sha256.md`, `docs/irmel-ada-coverage.json`, its generated
  `.md`, `docs/reference/irmel-reviewed-bindings.json`,
  `scripts/check_ir_api_coverage.py`, `scripts/test_ir_api_coverage.py`,
  `scripts/test_doc_source_integrity.py`, and
  `docs/tests/test_stacked_foundation_links.py`.
- This task record and the two newline-only corrections identified above.

All pre-existing status-returning C export declarations are byte-identical to
base. No safe Python/Rust API, workflow, generic architecture prose, historical
coverage archive or dependency pin is changed. Later open PRs #195 (Python
Control discovery), #196 (authored HTML) and #197 (Rust control-only Image) were
inspected during qualification; none implements StackedImage. Main remained at
the exact selected base during those checks.
