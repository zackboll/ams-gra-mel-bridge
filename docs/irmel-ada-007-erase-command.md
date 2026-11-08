# IRMEL-ADA-007 — C2 EraseCommand qualification

## Scope and upstream verification

Fetched base commit: `24eb066122c8bd15f7d79edcf71ee04f7b8efbc1`.
Base tree: `a810d48198e5eb7f3ffc6d20677463d9401933d7`.
One optional IR vertical slice; **every erase invocation uses the test mock
provider, never Squall, another real provider, or physical hardware**.

Inspected pinned `EraseCommand.h`, `EraseCommandType.h`, and `C2Channel.h`:
`commandID` is uint32 and `eraseCommandType` is the exact enum. Native static
assertions bind its uint32 underlying type and every alternative:

| Upstream | Numeric | Safe Ada |
|---|---:|---|
| `Zeroize` | 0 | `Erase.Zeroize` |
| `Classified_data_erase` | 1 | `Erase.Classified_Data_Erase` |
| `Disable` | 2 | `Erase.Disable` |
| `CDE_AND_Zeroize` | 3 | `Erase.CDE_AND_Zeroize` |

There is no `MaxExclusive` enumerator. Values 4, 99 and UINT32_MAX are invalid,
not sent as inputs and not remapped as successful responses.

## Complete API mapping

| Upstream | C ABI | Safe Ada |
|---|---|---|
| `EraseCommand` | `ams_mel_ir_erase_command_v1` (`command_id`, `erase_type`) | `Erase.Erase_Command`, `Create_Command (ID, Operation)`, `ID`, `Operation` |
| `EraseCommandType` | `ams_mel_ir_erase_type_t`, four `AMS_MEL_IR_ERASE_*` constants | `Erase.Erase_Type`, explicit representation clause |
| `C2Channel::send(EraseCommand)` | `ams_mel_ir_c2_submit_erase` | `C2.Submit_Erase_Command` |
| asynchronous `ErrorOr<shared_ptr<EraseCommandType>>` | `ams_mel_ir_erase_result_v1`, `ams_mel_ir_erase_request_wait` | `Erase.Erase_Request`, `Wait`, `Erase_Result`, `Status`, `Response`, `Rejection_Code`, `Description` |
| observer release | `ams_mel_ir_erase_request_close` | `Erase.Close`, non-raising limited-controlled finalization |

Safe Ada requires explicit operation selection through `Create_Command`; a
default-initialized private value is unselected and cannot be submitted.
There is no default Zeroize submission. Commands and terminal values are owned,
copyable Ada values; no C imports, C types or borrowed provider objects are public.

The result distinguishes successful asynchronous return of any exact erase type
from an explicit MEL rejection with known error code and complete owned diagnostic.
Provider send/future exceptions, invalid futures, null/unknown successful payloads,
unknown errors, invalid diagnostic encoding and cleanup failures raise
`Provider_Error`. Long valid rejection diagnostics are fetched from the cached
native result at the exact required size; the retry does not resend the command.
**A successful future is not proof of completed physical erasure. No physical
erasure verification is defined by this interface.**

Exactly three exports extend **266 → 269**, all under `AMS_MEL_0.1`. Existing
records, signatures, exports and enum values are unchanged. Private Python ctypes
and Rust sys mirrors include layout and signature checks; no safe Python/Rust
erase API is added.

## Lifecycle and ownership evidence

Submission reuses Session `CompletionPermit`, the Enabled-only lock-linearized
`claim_c2_submission`, `CommonRequestClaim` and `finish_c2_request`. Provider send
runs unlocked exactly once. One completion worker consumes the future once,
releases the claim, caches the terminal result/diagnostic, and finally releases
the permit. The request is an observer, not the owner of provider execution.
Wait timeout neither cancels nor consumes; repeated terminal Wait is cached.
Request Close neither cancels nor waits for execution. Pending work retains
C2/Session/provider after public owners close. Post-send allocation/launch failure
retains the preallocated self-owning graph without allocation. Uncertain cleanup
is retained rather than retried or unloaded.

Completion probe family Erase=10 and mock send family 11 are appended; old
families and PR #164's process-lifetime observation synchronization remain intact.
Deterministic provider promise gates, completion-owner observations and
Session-specific admission tokens—not sleeps—establish lifetime/order.
The refused-submission negative control advances a family owner count while
the actual Session permit remains held. Separate-Session Ada tests disprove a
global admission limit. Pending-observer tests close request/C2/Session, then
release provider futures and verify retained graph teardown and cached results.

No callback, metadata result, cancellation, automatic retry, watchdog, timer,
duplicate send, replay, scheduling, calibration execution or RF extension exists.

## Qualification commands and results

Run from this worktree; production and test trees remain independent. Initial
development compile errors and an incorrectly production-bound Python test run
were corrected; the results below refer to the corrected qualification runs.

| Check | Command / evidence | Result |
|---|---|---|
| Production native | `make native`; `cmake --build native/build --parallel 2` | PASS, first-party `-Wall -Wextra -Wpedantic -Werror` |
| Focused C11 mock contract | `ctest --test-dir native/build-tests -R ir_erase --output-on-failure` | PASS, 4/4 |
| Full native | `make test-native`; full `ctest --test-dir native/build-tests --output-on-failure` | PASS, 462/462 (458 baseline + 4) |
| Ada build | `alr -C ada build`; `alr -C ada/tests exec -- gprbuild -j4 -p -P ams_mel_tests.gpr` | PASS, warnings treated as errors |
| Focused Ada | `ada/tests/bin/ams_mel_ir_erase_focused native/build-tests/test-providers/libmock_ir_provider.so` with test facade on `LD_LIBRARY_PATH` | PASS |
| Isolated Ada failures | same executable, additional argument `allocation`, `worker-launch`, or `finish-failure`; fresh `AMS_MEL_TEST_LIFETIME_LOG` per process | PASS, all three |
| Full Ada | `ada/tests/bin/ams_mel_smoke`; `alr -C ada/tests run` | PASS |
| Python | `PYTHONPATH=python AMS_MEL_NATIVE_LIB="$PWD/native/build-tests/lib/libams_mel_c.so.0" AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" python3 -W error -m unittest discover -s python/tests -v` | PASS, 276 tests including ABI |
| Rust | `AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib" AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers" cargo test --manifest-path rust/Cargo.toml --workspace` | PASS; sys ABI 44 tests plus new erase ABI test; existing opt-in real-provider tests remain ignored |
| Exports | complete `nm -D --defined-only native/build/lib/libams_mel_c.so.0` versus `native/src/exports.map`, including symbol versions | PASS, 269 exports, all 266 baseline symbols preserved, no extra/test-only definitions |
| Policy | `make test-policy` | PASS |
| Coverage | `make check-ir-api-coverage` | PASS, 37 regression tests |
| Formatting | `make format-ada`; `make check-ada-format`; `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS |
| Documentation | `python3 scripts/check_doc_source_integrity.py`; pinned enhancer Python `-W error -m unittest discover -s docs/tests -p 'test_*links.py' -v` | PASS, 804 dependency files; 20 type-link tests |
| Mutation controls | `python3 scripts/test_erase_mutations.py` | PASS, type alteration and command-ID loss both detected; restored baseline passes |
| Whitespace | `git diff --check` | PASS |

The Alire toolchain supplies GPRbuild (not initially on the ordinary PATH).
GNATformat emits the existing imported-native-project “no C sources” project
warning; first-party compiled Ada and native sources are warning-clean.
The optional whole-repository `scripts/check_final_newlines.py` also identified
an existing missing final newline in `docs/rust-ir-003-nuc-temperature.md` at the
base commit. That unrelated document is unchanged; all files in this task have
final newlines and `git diff --check` passes.

Mutation controls copy native sources to a temporary independent source/build
tree, use a Release build and the mock provider, and require runtime C11 fidelity
failure (not compilation failure) for each mutant. The altered type fails
`values[1] == c->erase_type`; ID loss fails `values[0] == c->command_id`.
They restore the copied source, rebuild and pass baseline, verify the checkout
source hash never changed, and delete the temporary tree before qualification.

Coverage changes only `C2Channel::send(EraseCommand)`, `EraseCommand`, and
`EraseCommandType`: missing → safe_ada; safe Ada operations **43 → 44**.
The reviewed binding anchor is refreshed for those entries only. Pinned vendor
headers, catalog generation/catalog anchor, historical source hashes, inherited
classifications, unrelated entries and workflows are unchanged.

## Limitations and delivery

This is mock qualification, not physical-erasure, real-provider, C++ ABI
compatibility or hardware qualification. Provider support is optional; an
Unsupported MEL rejection remains a rejection, never a successful erase type.
Provider execution may outlive all public owners. Deliberate emergency/uncertain
retention can last for process lifetime; it is safer than unloading live code.
No claim of SPARK proof or provider-boundary verification is made.

The PR handoff records the exact head commit/tree and one hosted-CI observation.
Hosted CI is distinct from these local checks; pending checks are reported as
`HOSTED_CI_PENDING`, not as passed. The PR remains open and unmerged, without
self-approval or auto-merge.
