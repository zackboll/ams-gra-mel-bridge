# PY-MEL-018 — Safe Python LineOfSightQuaternion reception

## Source identity, preflight and coverage delta

Isolated worktree `/home/zboll/git/ams-py-mel-018`, branch
`feature/py-mel-018-los-quaternion`, starts at freshly fetched main commit
`34cc31f6a5dd183e2197d61dd4d9c62d97ab6ded`, tree
`37924775b8a544a58a365a9e61547b674327cee4`. GitHub PR #226 was merged at
that commit on 2026-11-15, and its public canonical `ForwardRightDown` is
present. Open PR and local/remote branch inspection found no overlapping Python
quaternion implementation. Existing worktrees, branches and modifications were
preserved. A subsequent pre-qualification fetch identified the same main.

Reviewed repository instructions, contributing guidance and all six clinerules,
architecture, ABI policy, provider-foundation task, structured IR inventory and
schema, native header/implementation/C11 tests/mock, private ctypes and Python
owner/diagnostic/pointer facilities, PY-MEL-017, and the native/Ada/Rust quaternion
implementations and task documents. The immutable IR upstream pin is commit
`8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`; Common MEL is commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
`f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`.

The coverage delta is complete safe Python reception of the conditionally
supported Image `LineOfSightQuaternion` callback, previously private-ctypes-only.
All eight scalar fields are represented. The authoritative structured inventory
classifies native/Ada evidence, not Python, and remains unchanged. No native,
Ada, Rust, vendor, mock, workflow, historical coverage archive or public API
baseline changes are necessary. One existing ABI test's obsolete assertion of
no public quaternion API is replaced with identity checks; all its C layout and
raw signature assertions remain intact.

## Public API and exact mapping

All new public classes have `__module__ == "ams_mel"` and root exports. Values
are frozen dataclasses, Python-owned and pickleable with canonical class identity:

```text
Quaternion(x: float, y: float, z: float, w: float)
LineOfSightQuaternion(system_time_ns: int, q_xyzw: Quaternion,
                      pqr_rps: ForwardRightDown)
ImageStream.open_line_of_sight_quaternion(queue_capacity: int = 16)
    -> LineOfSightQuaternionStream
LineOfSightQuaternionStream.is_open
LineOfSightQuaternionStream.receive(timeout_ms: int = 0) -> LineOfSightQuaternion
LineOfSightQuaternionStream.counters() -> ImageMetadataCounters
LineOfSightQuaternionStream.close() -> None
```

`ForwardRightDown` is exactly the existing PY-MEL-017 type, not a second
representation. `ImageMetadataCounters` is unchanged: all three unsigned 64-bit
values are retained, without reset or narrowing.

| Upstream getter | Existing C record field | Python field |
| --- | --- | --- |
| `getSystemTime().count()` | `system_time_ns` (`int64_t`) | `system_time_ns: int` |
| `getQ_xyzw().getQuaternionX()` | `q_xyzw.x` (`double`) | `q_xyzw.x: float` |
| `getQ_xyzw().getQuaternionY()` | `q_xyzw.y` (`double`) | `q_xyzw.y: float` |
| `getQ_xyzw().getQuaternionZ()` | `q_xyzw.z` (`double`) | `q_xyzw.z: float` |
| `getQ_xyzw().getQuaternionW()` | `q_xyzw.w` (`double`) | `q_xyzw.w: float` |
| `getPqr_rps().getForward()` | `pqr_rps.forward` (`double`) | `pqr_rps.forward: float` |
| `getPqr_rps().getRight()` | `pqr_rps.right` (`double`) | `pqr_rps.right: float` |
| `getPqr_rps().getDown()` | `pqr_rps.down` (`double`) | `pqr_rps.down: float` |

The existing `ams_mel_ir_line_of_sight_quaternion_v1` is a fixed 64-byte record
on LP64. Quaternion order is **XYZW**, describing navigation-frame orientation;
angular rates are image-frame forward/right/down **radians per second**. Signed
nanoseconds retain the full INT64_MIN through INT64_MAX range without an inferred
epoch or synchronization convention. No normalization, sign canonicalization,
Euler conversion, frame conversion, clamping, interpolation, attitude estimation,
validity inference or sensor fusion is performed. Zero and non-unit quaternions,
and opposite-sign representations, remain distinct as supplied.

IEEE doubles preserve signed zeros, subnormals, maximum finite values and
infinities. NaN classification is retained; payload-bit fidelity is not promised
beyond the native contract. Tests use bit comparisons for non-NaN floats, not
NaN dataclass equality. Public value construction does not invent physical
validity requirements.

## Conditional registration and errors

The pinned callback is `@RequiredIfLOSQuaternion`. Registration is explicit,
independent of ordinary four-kind Image metadata, NUC_TempData,
OpticalDistortionMap and CameraCommandResp, and never Starts acquisition.
Attached and Running Image states are eligible; Stopped, Failed and closed
Images are rejected. Queue capacity must be a positive non-Boolean integer
representable by native `size_t`. Local validation does not consume the one-shot.
The one-shot is consumed immediately before an actual native Open, even on
rejection, exception or uncertain output. No automatic provider retry occurs.

Success requires native OK, a separately published provider `SUCCESS`, and a
non-null consumer owner. `LineOfSightQuaternionOpenError(MelError)` retains
`kind`, `diagnostic`, `diagnostic_required`, `native_status` and separately
`registration: CommandReturn | None`. Known Returns include SUCCESS,
NOT_SUPPORTED, FAIL, NOT_IMPLEMENTED and BAD_POINTER. An unpublished UINT32_MAX
sentinel or unknown Return yields `None`, as do local failures and provider
exceptions without a published known Return. Diagnostics are never parsed to
invent a Return. An unexpectedly published failed-Open owner gets at most one
non-raising cleanup attempt while preserving the primary failure.

## Receive, ownership and lifetime

Receive validates the owner and uint32 timeout, initializes a null event, calls
native Receive exactly once, requires a non-null event, and requests the event
view. Existing checked-pointer utilities validate nullability, alignment,
fixed-record byte representability and address-space overflow **before** the
first dereference. There are no collections or variable-length allocations to
validate. The trusted native facade remains the memory-provenance boundary:
these checks cannot prove arbitrary aligned addresses are mapped memory.

All eight scalars are copied into an entirely Python-owned graph while the
native event is alive. Event Close must succeed and consume its handle before
the value is published. Failure in receive, view, conversion or release publishes
no value. Event cleanup is attempted at most once, including failed Receive
with unexpected output. An uncertain Close is never retried, even if its output
still appears non-null. Primary exceptions survive secondary cleanup failures.
Recoverable reception allocation/overflow failures become INTERNAL_ERROR;
malformed record/owner output becomes PROTOCOL_INCONSISTENCY. Existing native
status categories and diagnostic validation conventions are preserved.
Zero timeout is one poll, nonzero uint32 milliseconds pass unchanged, and native
TIMEOUT is not cancellation.

The consumer independently owns its native handle and retains neither Python
Image nor Session. `is_open` reports local ownership, not provider health. Close
only ends this consumer; it does not Stop Image, close parents, unregister the
provider callback or promise immediate callback quiescence. Successful Close is
idempotent; uncertain Close is remembered and surfaced without another native
attempt. Context management preserves an application exception. Finalization is
non-raising and one-attempt, including after failed explicit Close.
Copy, deepcopy and pickle of the consumer itself are rejected so that ordinary
Python copying cannot create a second owner of the same native handle. Immutable
received values, unlike the owner, remain copyable and pickleable.

Externally serialize receive, counters and close on one consumer, and registration
versus parent operations. Python's GIL is not the native synchronization contract.
No production Python thread, lock, queue, worker, dispatcher or executor is added.
Test-only threads coordinate existing native bounded probes.

The unchanged native callback copies before provider payload destruction and
catches callback exceptions. Native lifetime management retains provider code
until physical teardown can safely drain callbacks. Queued events drain before
STREAM_STOPPED after ordinary teardown, or PROVIDER_FAILED after uncertain
detach. Independent subprocess tests observe actual `library_unloaded` records
and absence from `/proc/self/maps`, not merely Python Close. Copied values are
rechecked after event release, consumer Close, Image Stop/Close, Session Close,
garbage collection and actual DSO unload. The uncertain-detach case verifies
provider retention and safe late callbacks until teardown.

## Consumer example

The caller supplies an already configured `image`. Sensor capabilities determine
whether registration is supported; catch `LineOfSightQuaternionOpenError` to
inspect its separately published `registration`. Reception does not establish
physical accuracy or suitability for pointing/navigation.

```python
with image.open_line_of_sight_quaternion(
    queue_capacity=16
) as metadata:
    los = metadata.receive(timeout_ms=1000)

    print("Timestamp (ns):", los.system_time_ns)

    q = los.q_xyzw
    print("Quaternion XYZW:", q.x, q.y, q.z, q.w)

    rates = los.pqr_rps
    print(
        "Image-frame rates (rad/s):",
        rates.forward,
        rates.right,
        rates.down,
    )

    print(metadata.counters())
```

## Qualification record

All positive provider evidence is the separately compiled compatible mock, not
real hardware or Squall. The focused module uses scoped hostile-output injection
with native-provenance owners/events, eight compiled fixtures and 28 isolated
provider-global/lifetime scenarios. Malformed pointer tests reject structural
errors without dereferencing arbitrary unmapped addresses. Full-width counter
injection verifies Python representation; saturation itself is implemented by
the unchanged native saturating increment, not simulated by iterating 2^64 calls
or adding a production test hook.

Initial development invocation caught an incomplete copied test-helper body
(IndentationError); it was corrected before the successful 18-test focused run.
No native or unrelated test contract was weakened. The final expanded campaign
and exact-source qualification results are recorded below after execution.

The expanded focused/API selection passed 114 tests (22 quaternion tests,
21 unchanged ODM tests, package layout, import identity, pickle and ABI tests).
An additional explicit identity-quaternion regression was then added. A later
focused invocation raced the acceptance runner's native rebuild and could not
load the temporarily absent test DSO; that run is not a pass and is repeated
after builds complete. The documentation final-newline check initially failed;
the newline was corrected and the check passed. Source integrity passed with
825 vendor files and four workflows. Package/test Python compilation passed.

Production symbols were captured from freshly fetched main before Python edits,
then compared against the feature production build: **331 versioned function
exports, identical**, all `@@AMS_MEL_0.1`, exactly matching `exports.map`, no
test-only exports, additions or removals. The complete sorted defined-symbol
inventory SHA-256 is
`6e74fb44b81f003bd5ee14305ffb5ced7bfd1a4a7d0ef1113b026d5c6c070589`.
The native sources, structures, six quaternion signatures and version script
remain byte-identical to the recorded base.

A read-only independent technical review was attempted with the installed review
client but failed authentication: `OAuth session expired and could not be
refreshed` (exit 1). GitHub lists only the author as repository collaborator and
assignable user. No independent review or approval is claimed. Final-source
qualification and review blockers are recorded in the PR rather than describing
pending checks as successful.

Execution snapshot before source sealing (final committed-head repetitions and
remaining commands are recorded with immutable identities in the PR):

| Command | Observed result |
| --- | --- |
| `make native` | PASS, warning-as-error production build |
| `make test-native` | PASS, 821/821 CTests |
| `make test-python` | PASS, 710 tests, zero failures/skips; includes all 24 quaternion tests and compilation |
| `make test-rust` | PASS, 608 tests; 15 existing ignored subprocess entry functions, no failures |
| `make test-policy` | PASS, 263 tests across ten suites |
| `make check-ir-api-coverage` | PASS, 70 controls; existing classifications unchanged |
| `python3 -W error -m unittest test_ir_image_los_quaternion test_ir_image_optical_distortion_map test_package_layout test_import_identity test_abi -v` | PASS, 114 tests before the final two added regression cases |
| `python3 -W error -m compileall -q python/ams_mel python/tests/test_ir_image_los_quaternion.py` | PASS |
| `python3 scripts/check_final_newlines.py` | PASS after newline correction |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files / four workflows |
| `git diff --check`; `git diff --cached --check` | PASS |

The acceptance runner executes builds sequentially: `make native`,
`make test-native`, `make test-python`, `make test-rust`, `make test-policy`,
`make check-ir-api-coverage`, `make check`. The final-head campaign additionally
repeats focused tests with `-W error`, unchanged ODM/package/identity/ABI tests,
compilation/integrity/newline checks, and the production symbol comparison.
Current-Git documentation qualification uses the installed pinned enhancer
0.1.5 interpreter and GNATdoc 26.0.0, a fresh output directory, and the command:

```sh
/home/zboll/git/mel-gnatdoc-015-002-evidence/enhancer/bin/python \
  docs/tools/docs_ci.py build \
  --root /home/zboll/git/ams-py-mel-018 \
  --directory /home/zboll/git/ams-py-mel-018/.doc-tools/py-mel-018-final \
  --gnatdoc-prefix /home/zboll/git/ams-mel-ada/.doc-tools/gnatdoc-26.0.0
```

This documents the reproducible command, not an unexecuted pass. The delivery
PR records its actual result together with final-head hosted status. Local
documentation output is non-deployable and does not replace hosted evidence.

The documented current-Git documentation command subsequently passed (exit 0)
on commit `0151b157ce420da9dd616dcb66665c842a51fd5d`, tree
`2f4a067a252c3d47b2aa86143720bba3dda442b5`: source-derived coverage
5,145/5,146, 101 informational GNATdoc warnings; locked/offline Rustdoc 1.98.1
produced 1,497 HTML files and 652 assets. These are measured earlier-source
results, not a waiver for final-head repetitions after this evidence update.
The PR records the exact final-source repetition and hosted run identities.

## Final integration and resumed qualification

The interrupted client polling was not a build failure. The original sequential
runner completed on commit `59e9b3d1a26f5979be7d5a571bd2551a9dea520d`, tree
`b746908d72379ab54cab953bf5e20cf119f606d6`, recording exit 0 for every required
make target, focused/API tests, compilation, integrity, newlines, whitespace and
production exports. Focused results were 24 quaternion tests (28 isolated
provider scenarios) and 116 combined quaternion/ODM/package/identity/ABI tests.
The full suites passed 821 native tests, 710 Python tests, 608 Rust tests (15
existing ignored subprocess entries), 263 policy tests and 70 coverage controls.
Current-Git documentation generation also passed on that exact commit.

A fresh fetch during delivery identified newer main commit
`5b8ec76bc7a5ce27558988c619e96e5dce6159e7`, tree
`400012fb08a7aa573a0d32e425e76e73f7bb046a`, incorporating PRs #228 and #229
(Rust crate extraction and completion-owner lifetime regression correction).
It was merged normally without conflict, preserving both feature commits. Its
native tree is byte-identical to the original production-export baseline; the
Python implementation and tests required no integration changes. The feature
delta against incorporated main remains exactly the six paths above, with no
native, Ada, Rust, workflow, vendor or historical-baseline feature changes.

All required acceptance commands, focused tests and current-Git documentation
are repeated on the final integrated commit; their measured results, exact
commit/tree and final-head hosted checks are recorded in the delivery PR.
Earlier successful qualification is not substituted for those final-source
checks. Independent review remains blocked by the recorded reviewer-client
authentication failure; no approval or readiness is inferred from local passes.

Commands run from `/home/zboll/git/ams-py-mel-018`. GNAT-dependent checks use the
installed GNAT 16.1.0 and GPRbuild 26.0.1 directories under
`/home/zboll/.local/share/alire/toolchains` on PATH. Focused Python commands set:

```sh
export PYTHONPATH=/home/zboll/git/ams-py-mel-018/python:/home/zboll/git/ams-py-mel-018/python/tests
export AMS_MEL_NATIVE_LIB=/home/zboll/git/ams-py-mel-018/native/build-tests/lib/libams_mel_c.so.0
export AMS_MEL_TEST_PROVIDER_DIR=/home/zboll/git/ams-py-mel-018/native/build-tests/test-providers
```

### Limits and delivery

No hardware, Squall, arbitrary-provider C++ ABI compatibility, pointer mapping
proof, unregister, automatic acquisition or physical validity/accuracy is
qualified. LOS3D, command submission, optical processing and navigation
integration are excluded. Recoverable Open allocations follow PY-MEL-017's
pre-native exception convention without consuming registration; reception
allocations are classified INTERNAL_ERROR. Uncertain ownership favors retention
over unsafe retry and can leak native resources after a violated ABI contract.

Exact final commit/tree, final-source hosted Bootstrap and Documentation results,
and independent-review status belong in the delivery PR, avoiding a recursive
self-hash in this file. Pending hosted checks mean HOSTED_CI_PENDING, never a
pass. Independent technical review is not self-approval; no merge or auto-merge
is authorized or performed.
