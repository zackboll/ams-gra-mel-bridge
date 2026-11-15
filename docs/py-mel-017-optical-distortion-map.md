# PY-MEL-017 — Complete safe Python OpticalDistortionMap reception

## Source, preflight and coverage delta

Dedicated worktree `/home/zboll/git/ams-py-mel-017`, branch
`feature/py-mel-017-optical-distortion-map`, from freshly fetched `origin/main`
commit `604ef38f8f81a9c71add60c5ca2bac4db218ea7b`, tree
`c2bf94b76fa1aa841345484ba2e8efa89e783cce`. PR #218 / PY-MEL-016 was merged
at `9f0ed1e67c3715c0638184591d3421d26a5cd04b`; ancestry was verified before
implementation. Open PR and local/remote branch inspection found no competing
Python ODM work. Other worktrees were not modified. A subsequent fetch before
qualification still identified the same main.

Reviewed `AGENTS.md`, `CONTRIBUTING.md`, all six `.clinerules`, architecture and
ABI policy, provider-foundation task, IR inventory schema and ODM evidence,
native header/implementation/mock/C11 tests, ctypes declarations, Python Image,
ordinary metadata, CameraCommandResp, shared FFI/error/owner helpers, package
compatibility tests, and Ada/Rust ODM implementation/qualification references.

Pinned IR MEL: commit `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`. Pinned Common MEL: commit
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`, tree
`f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f`. These are the existing frozen
inventory identities, not new downloads or a provider compatibility promise.

Python now safely receives the optional Image `OpticalDistortionMap` family:
all eleven getters, both complete collections and all nested components.
No native, raw ctypes, Ada, Rust, mock, vendor, CI or historical baseline change
is needed. The authoritative inventory schema classifies native/Ada evidence,
not Python; no shared inventory or generated report is changed. No
LineOfSightQuaternion Python API, optical correction, DSP or acquisition
automation is introduced. The reusable `ForwardRightDown` is not ODM-specific.

The final feature commit/tree and exact-head hosted results are recorded in the
delivery PR rather than embedding a recursively self-referential hash here.

## API and field mapping

Root exports (their public `__module__` is `ams_mel`, including for pickle):

- Frozen `ForwardRightDown(forward: float, right: float, down: float)`.
- Frozen `RowCol(row: float, column: float)`.
- Frozen `OpticalDistortionMap`, with the fields below.
- `OpticalDistortionMapOpenError(MelError)`, adding
  `registration: CommandReturn | None`.
- Unique `OpticalDistortionMapStream`, created only through
  `ImageStream.open_optical_distortion_map(queue_capacity: int = 16)`.

The stream offers `is_open`, `receive(timeout_ms: int = 0)`,
`counters() -> ImageMetadataCounters`, `close()`, and context management.
`is_open` means local handle ownership, not acquisition or provider health.
Counters reuse the existing model: full-width uint64 `events_received`,
`events_dropped_queue_full`, and `malformed_or_unsupported`; reception does not
reset them. They count native callbacks, not Python Receive calls.

| Upstream getter | Existing C / ctypes field | Python owned field |
| --- | --- | --- |
| `getWidth`, `getHeight` | `width`, `height` (`uint32`) | same names, `int` |
| `getRightSize`, `getDownSize` | `right_size`, `down_size` (`uint32`) | same names, `int` |
| `getPixelsPerOdm` | `pixels_per_odm` (`uint32`) | same name, `int` |
| `getRightMin`, `getRightMax` | `right_min`, `right_max` (`double`) | same names, `float` |
| `getDownMin`, `getDownMax` | `down_min`, `down_max` (`double`) | same names, `float` |
| `getFrd` | `frd` span of `IrForwardRightDownV1` | `tuple[ForwardRightDown, ...]` |
| `getRowCol` | `row_col` span of canonical `IrRowColV1` | `tuple[RowCol, ...]` |
| `ForwardRightDown::getForward/getRight/getDown` | `forward`, `right`, `down` | same names, `float` |
| `RowCol::getRow/getCol` | `row`, `column` | same names, `float` |

All five integers retain zero through UINT32_MAX. Each collection uses its own
actual length, independent of dimensions and `pixels_per_odm`. Ordering and
duplicates survive. Nothing is normalized, sorted, clamped, interpolated,
truncated, padded, frame-converted or assigned inferred units. Python floats
retain IEEE double finite values, subnormals, signed zero, infinity and NaN
classes. NaN payload bits are not promised. Equality follows Python floating
semantics; tests compare finite/zero/infinite bits and NaN classification.

## Registration and errors

Only the six existing exports are used, with declarations unchanged:
`ams_mel_ir_image_odm_open`, `_receive`, `_get_counters`, `_close`,
`_event_view`, `_event_close`. The production inventory remains **326 exports**.

Registration is independently opt-in and eligible while Image is Attached or
Running; it does not Start Image. Stopped, Failed and closed parents reject it.
Positive integer capacity must fit native `size_t`; bools and non-integers are
rejected. Local invalid arguments and failures preparing the call do not consume
the one-shot. Once native Open is invoked, Python conservatively forbids another
attempt, including provider refusal/exception, malformed diagnostic and uncertain
call outcome. Consumer Close never resets it. Ordinary metadata, NUC,
CameraCommandResp and LineOfSightQuaternion have independent registrations.

Open requires native OK, separately published provider SUCCESS, and a non-null
owner. A UINT32_MAX sentinel distinguishes unpublished Return. Only repository
`CommandReturn` members are retained: SUCCESS, BAD_POINTER, FAIL, NOT_SUPPORTED,
NOT_IMPLEMENTED. Unknown Return, provider exceptions without a Return, local
validation and unpublished output produce `registration=None`; no parsing of
diagnostic text or inference from native status occurs. `OpticalDistortionMapOpenError`
retains `kind`, `diagnostic`, `diagnostic_required` and `native_status` just like
`MelError`. Unexpected failed-Open owners get at most one non-raising cleanup
attempt, preserving the original failure.

Receive calls native exactly once. Zero polls; nonzero uint32 millisecond
timeouts pass unchanged. TIMEOUT is not cancellation of registration, acquisition
or provider work. Native errors retain their existing categories. Malformed
event/view/span output is PROTOCOL_INCONSISTENCY. Recoverable allocation failure
while copying is INTERNAL_ERROR; no partially copied map is published. Ordinary
conversion/application exceptions remain primary even if cleanup also fails.

## Ownership, serialization and teardown

Receive validates the event handle and view before dereferencing, then validates
**both** spans before reading either collection. Checks include nullability,
alignment, native INT32_MAX collection ceiling, byte/allocation representability
and address overflow using the established checked-span helpers. Empty spans do
not inspect inactive pointers. Structural checks cannot prove that an arbitrary
aligned pointer denotes mapped memory: live, correctly typed native handles and
native pointer provenance remain part of the trusted C ABI boundary. Tests never
intentionally dereference unmapped addresses.

All data is deep-copied while its native event is alive. The event is released
before success returns. Failed Receive output, view, conversion or release gets
safe cleanup only if event Close has not already been invoked. An uncertain
release result is never retried. Returned snapshots contain only Python scalars,
frozen component values and tuples: no ctypes storage, borrowed pointer, native
owner, provider reference or Python parent survives in them.

Consumer Close only ends ODM consumption. It does not close/stop Image or
Session, unregister provider callbacks, or promise immediate callback quiescence.
Success is idempotent; once native release begins, a failure/uncertain result is
remembered and re-raised by subsequent explicit Close/Receive/counters/context
entry without calling native again. Failure preparing the diagnostic before
native entry leaves Close eligible. Finalization is non-raising and one-attempt,
including failed-Open cleanup; exceptional context exit preserves the application
exception. Safety may require retaining an uncertain native graph rather than
risking double release or premature provider unload.

The stream holds no Python Image/Session reference. Provider retention is the
existing native facade's responsibility, not a Python parent-reference scheme.
Native Stop/parent Close allows queued maps to drain before STREAM_STOPPED;
failed detach drains before PROVIDER_FAILED. Late callbacks remain safe according
to the native contract, with no inferred unregister/quiescence guarantee. Tests
retain maps through consumer, Image and Session closure, garbage collection and
observable actual DSO unload; parent-first scenarios also receive queued events
**after** provider unload. In-flight teardown uses existing native barriers.

Externally serialize operations on one ODM owner and registration against parent
Start/Stop/Close. The GIL does not meet the native ownership contract. No Python
callbacks, worker threads, locks, queues or handle sharing are introduced in the
implementation. Test helper threads only drive existing mock hooks/barriers,
never share safe Python owners.

## Runnable consumer example

Call this function with a caller-supplied compatible provider path, instance,
aperture and valid `ImageConfig`. These are the existing Session configuration
arguments; no provider configuration syntax is invented. The function deliberately
registers before explicitly starting acquisition. It does not invent a supported
sensor, universal channel identity, provider configuration syntax or hardware
result. A provider may reject optional ODM or produce no metadata within 1 s.

```python
from ams_mel import ImageConfig, MelError, OpticalDistortionMapOpenError, Session


def receive_optical_map(provider_path, instance_name, provider_aperture,
                        image_config: ImageConfig):
    try:
        with Session.open(provider_path, instance_name, provider_aperture) as session:
            with session.open_image_stream(image_config) as image:
                with image.open_optical_distortion_map(queue_capacity=16) as metadata:
                    image.start()  # explicit acquisition; ODM registration did not Start
                    odm = metadata.receive(timeout_ms=1000)
                    print(odm.width, odm.height)
                    for point in odm.frd:
                        print(point.forward, point.right, point.down)
                    for point in odm.row_col:
                        print(point.row, point.column)
                    return odm  # valid after all native owners have closed
    except OpticalDistortionMapOpenError as failure:
        print("ODM registration:", failure.registration, failure.kind, failure.diagnostic)
        raise
    except MelError as failure:
        print("MEL failure:", failure.kind, failure.native_status, failure.diagnostic)
        raise
```

## Qualification record

Qualification uses only the separately compiled mock provider, never Squall or a
real sensor. The focused test module exercises 21 tests, including 26 fresh-process
provider/lifecycle scenarios and additional scoped native-output injections.
Initial test development exposed incorrect fixture/scenario expectations, corrected
against the existing compiled contract without native changes or weakened checks.

One necessary existing-test change replaces the obsolete `test_private_image_odm_abi`
assertion that no public ODM API exists with a positive public-export assertion.
Every C11 layout and raw function-signature check remains intact. The historical
`python/tests/public_api_baseline.json` remains byte-identical. New exact public
signatures, identities, frozen values and pickle behavior have focused regression
coverage. The runnable documentation example is itself executed with caller-supplied
mock configuration, without claiming any real-provider result.

A read-only independent-review client was attempted but could not authenticate:
`OAuth session expired and could not be refreshed`. No independent review or
approval is claimed. The delivery PR requests review through the repository;
only the author account was listed as a collaborator and no reviewer team was
available during preflight.

Exact final-source commands/results and hosted evidence are recorded below and
in the delivery PR. Independent technical review remains required; no self-approval
or merge is authorized. Hosted pending/failing gates are not readiness.

### Reproducible commands and observed results

Run from `/home/zboll/git/ams-py-mel-017`. Native/Ada/Rust builds are sequential.
For GNAT/GPRbuild-dependent commands the environment adds the installed
`/home/zboll/.local/share/alire/toolchains/gnat_native_16.1.0_9f74f58a/bin` and
`/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin` to PATH.
No source warning policy is relaxed. Final committed-source repetitions and
hosted commit/run links belong in the PR, including any final-source failure
instead of treating this table as a waiver.

| Command | Observed local result |
| --- | --- |
| `make test-native` | PASS, 799/799 CTest tests, including existing compiled-C11 ODM contract/layout/teardown tests |
| `make test-python` | PASS, 684 tests, zero failures/skips; includes 21 new ODM tests and package compilation |
| `make test-rust` | PASS, 604 tests, 15 existing ignored subprocess entry functions; parent scenarios exercise those entries |
| `make test-policy` | PASS, 263 tests across ten suites |
| `make check` | PASS, production/test build isolation, 799 CTests, Ada formatting, 70 structured-coverage controls, 52 Ada smoke PASS groups, final-newline and whitespace checks |
| `make test-ada` | Executed separately as well as through `make check`; final result recorded in PR |
| `python3 -W error -m unittest test_ir_image_optical_distortion_map test_package_layout test_import_identity test_abi -v` | PASS, 92 tests; includes 26 isolated ODM provider/lifecycle scenarios as subtests |
| `python3 -W error -m compileall -q python/ams_mel python/tests/test_ir_image_optical_distortion_map.py` | PASS |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 825 vendor files and four workflows |
| `python3 scripts/check_ir_api_coverage.py` | PASS, 127 headers / 148 operations; classifications unchanged |
| `python3 docs/tools/feature_source.py --root /home/zboll/git/ams-py-mel-017` | PASS on clean committed source; commit/tree and six-file delta recorded |
| `python3 scripts/check_final_newlines.py`; `git diff --check` | PASS |
| Production `nm -D --defined-only native/build/lib/libams_mel_c.so.0` compared to `native/src/exports.map` | PASS, 326 exports, zero missing/unexpected, all six ODM exports present; test-only facade intentionally has additional probes |

The focused command uses these explicit environment variables (also supplied by
`make test-python` for full discovery):

```sh
export PYTHONPATH=/home/zboll/git/ams-py-mel-017/python:/home/zboll/git/ams-py-mel-017/python/tests
export AMS_MEL_NATIVE_LIB=/home/zboll/git/ams-py-mel-017/native/build-tests/lib/libams_mel_c.so.0
export AMS_MEL_TEST_PROVIDER_DIR=/home/zboll/git/ams-py-mel-017/native/build-tests/test-providers
```

Local normal documentation build passed on commit
`4117f6abcbcc1325f0620e33fb46aadfceca28f1` with the pinned GNATdoc 26.0.0,
GNAT 16.1.0, GPRbuild 26.0.0 and already-installed enhancer 0.1.5:

```sh
/home/zboll/git/mel-gnatdoc-015-002-evidence/enhancer/bin/python \
  docs/tools/docs_ci.py build \
  --root /home/zboll/git/ams-py-mel-017 \
  --directory /home/zboll/git/ams-py-mel-017/.doc-tools/py-mel-017-corrected \
  --gnatdoc-prefix /home/zboll/git/ams-mel-ada/.doc-tools/gnatdoc-26.0.0
```

Result: source-derived coverage 5,116/5,117, 101 informational GNATdoc warnings;
locked/offline Rustdoc 1.98.1 produced 1,475 HTML and 638 assets. Final-source
build uses a fresh output directory and is recorded in PR. This is local,
non-deployable evidence, not hosted final-head documentation acceptance.

Earlier unsuccessful attempts are not counted as passes: fixture/scenario test
expectations, the obsolete no-public-ODM assertion, missing final newlines in new
files, and one focused invocation during a native tree rebuild were corrected or
rerun. The first local docs environment failed its pinned enhancer-version check;
the corrected run reused an existing 0.1.5 installation whose wheel SHA-256 equals
the repository pin `d712d330f213fc1909a66c47bc2c7e5e15d5ec687222bea3c4922f5243db4247`.
An initial export-audit script incorrectly applied the production-only allowlist
to the test facade; the corrected audit targets only the production DSO.
No failing check was bypassed and no upstream/provider behavior was modified.

### Limitations and outstanding review

Qualification is Linux/compatible compiled mock only. No hardware, Squall,
arbitrary-provider ABI compatibility, native pointer mapping proof, automatic
acquisition, optical correction, unit inference, LOS quaternion Python reception,
callback unregister or immediate quiescence is claimed. The deliberately
uncertain ownership cases favor retention over unsafe retry. All safe owners
still require external serialization. Hosted Bootstrap/documentation checks and
independent review must be evaluated on the final PR commit; pending hosted jobs
mean `HOSTED_CI_PENDING`, not merge readiness. Auto-merge stays disabled, and the
author does not approve or merge this PR.

## Integration with newer main

After all local gates (including qualified-PATH `make check` and the 92-test
focused/API selection) and all nine Bootstrap jobs plus documentation passed for
`7809932e2c1a4c004f133335dc790b4057042f43`, a fresh fetch identified newer main
`3e462a6d5448918813a81b45653f9340a66b7c3d`, tree
`a8efcacf5a763867a149eb1a507caf75d4ff164c`. It incorporates PR #220's Rust RF
crate split and PR #223's documentation-qualification repairs. They were merged
normally without conflict, not copied or modified as part of this feature.
The feature delta against the incorporated base remains the same six paths;
native, Ada, Rust, mocks, raw ctypes and historical baseline have no feature delta.
No Python ODM implementation/test behavior changed during integration.

The preceding counts and hosted runs are **earlier-source evidence**, not a waiver
for integrated source. All required local gates, focused warnings-as-errors/API
tests, compilation, integrity and current-source docs generation are rerun after
this report update; the PR records exact integrated HEAD/tree, changed counts,
commands, failures/skips and hosted results. The earlier successful hosted runs
were Bootstrap `43678376367` and documentation `43678376450`. The failed local
`make check` attempt lacking GPRbuild on PATH was followed by a successful run with
the documented installed toolchain; neither result is misrepresented.
