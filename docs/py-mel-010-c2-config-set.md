# PY-MEL-010 — Safe Python C2 ConfigSet submission

## Source and coverage delta

Started from freshly fetched main `de529f92f36623db8cdd3ccc6f3cdf7da31a1be8`
on `feature/py-mel-010-c2-config-set`, in a separate clean worktree. Open PRs
183, 184 and 182 contained independent Ada work; no overlapping Python ConfigSet
PR was open. The delivery PR records exact qualified head/tree identities.

The existing native-only Python ctypes ConfigSet operation gains two public API
elements, with no new request/completion types:

- Frozen `ConfigSetCommand(command_id: int, system_time_ns: int = 0, config: str = "")`.
- `ControlChannel.submit_config_set(command: ConfigSetCommand) -> ReturnRequest`.

Public symbols retain the defining implementation object's identity and the
`ams_mel` module identity for pickling. The reviewed structured IR inventories
describe native/Ada coverage, which is unchanged; they are not a Python coverage
inventory and no classification or report change is appropriate here.

## Native mapping and validation

Uses exactly the existing `ams_mel_ir_c2_submit_config_set` and
`IrConfigSetCommandV1`; no new or changed ctypes declaration or ABI export.

| Published field | Python and C mapping |
| --- | --- |
| Command ID | `command_id`, exact unsigned 32-bit integer, including high bit and maximum. |
| System time | `system_time_ns`, exact signed 64-bit nanosecond count; no epoch or clock domain inferred. |
| Config | `config`, opaque strict UTF-8, empty permitted, byte length retained. |

Construction and submission reject bool/noninteger numeric inputs, underflow,
overflow, non-string text, embedded NUL and unpaired surrogates with existing
`MelError(INVALID_ARGUMENT)`. Submission revalidates deliberate frozen-dataclass
mutation. Text is neither parsed nor normalized. Submission encodes once and
keeps the ctypes buffer, exact record and request shell alive through the one
native call. Native code copies borrowed strings before provider send.

## Ownership and completion

Open C2 is checked locally; Enabled-only eligibility remains native-authoritative.
Submission does not enable C2 or register metadata. No second lifecycle state
machine, worker, lock, retry, cancellation, correlation or polling is introduced.
Externally serialize operations on one C2 owner, and Wait versus Close on a
request. The GIL does not establish this contract.

Success requires a non-NULL request owner. Any unexpected owner published on a
failed call is closed once using the existing non-raising cleanup fallback; the
original error is preserved even if cleanup fails. The failed shell is disarmed
so garbage collection cannot attempt a second release of an uncertain handle.

`ReturnRequest.wait(timeout_ms)` and `close()` remain unchanged. Completed
`SUCCESS`, `FAIL`, `NOT_SUPPORTED`, and `NOT_IMPLEMENTED` are Return payloads,
not automatically MEL errors. `ReturnRejected` preserves the MEL error code and
complete description; cached terminal Wait can retrieve a long diagnostic
without resubmitting the command. Timeout does not consume/cancel the result.
Close releases the public observer, not necessarily provider work. Admitted
requests can remain valid after C2 and Session closure; native work retains the
required provider graph until completion. CommandStatus metadata is independent:
the mock can emit Accepted then return an explicit MEL rejection.

## Public usage (executed by the feature test)

The consuming application supplies `provider_path`, `instance_name`, and
`control_config` (a `ControlConfig`). No real provider path is implied.

```python
from ams_mel import (
    ConfigSetCommand,
    ControlConfig,
    ReturnCompleted,
    Session,
)

with Session.open(provider_path, instance_name, "") as session:
    with session.open_control_channel(control_config) as control:
        control.enable()

        command = ConfigSetCommand(
            command_id=301,
            system_time_ns=0,
            config="",
        )

        request = control.submit_config_set(command)
        try:
            outcome = request.wait(timeout_ms=5000)
        finally:
            request.close()

        if isinstance(outcome, ReturnCompleted):
            print(outcome.value)
        else:
            print(outcome.code, outcome.description)
```

Neither admission, Accepted metadata nor a normal Return proves physical
configuration application. The paired minimal C++ provider may legitimately
return `NOT_IMPLEMENTED`: this is a stub result, not hardware evidence.

## Qualification and limitations

Feature tests use the existing compiled mock and private boundary injection only.
The `config-full`, `config-empty`, `config-positive`, `config-fail` and
`config-reject` scenarios are unchanged. Signed int64 endpoints, byte counts,
Unicode non-normalization, and hostile boundary behavior are checked through
test-only injection around real native submission. The existing completion-scale
promise gate and native owner-destruction probe provide deterministic lifecycle
evidence without sleeps; provider unload is checked after synchronized cleanup.

No native/provider/mock implementation, ABI layout/export, ctypes declaration,
Ada/Rust source, workflow, dependency, historical coverage, general ModeCmd,
NavigationReport, CameraCommand, StackedImage or additional metadata support is
changed. Mock qualification is not real-provider interoperability or physical
application evidence. Provider-specific configuration syntax, timestamp domain,
and eventual hardware effect remain provider/application responsibilities.

### Measured local evidence

Commands run sequentially, with no concurrent native builds/relinks. Local
qualification uses Python 3.13, the C11 ABI probes, and the compiled mock. Native
tests run in the separate `native/build-tests` tree.

| Command | Result |
| --- | --- |
| `make test-native` | PASS: 523/523 native tests, including C-compiled C2 tests. |
| `make test-python` | PASS: 500 tests (483 baseline + 17 ConfigSet), warnings-as-errors; native prerequisite 523/523 also passed. |
| `make test-policy` | PASS: 234 tests (18 + 18 + 28 + 64 + 75 + 18 + 13). |
| `make check-ir-api-coverage` | PASS: unchanged reviewed inventory and 52 regression tests. |
| `python3 scripts/check_doc_source_integrity.py` | PASS: 824 vendor files, three workflows. |
| `python3 scripts/check_final_newlines.py` | PASS after correcting the two new files' final newlines. |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS. |
| `git diff --check` | PASS. |

Focused/relevant command (same environment as `make test-python`, plus
`python/tests` on `PYTHONPATH`):

```sh
python3 -W error -m unittest -v test_ir_config_set test_control_channel \
  test_ir_c2_metadata test_common_channel test_admission test_abi \
  test_package_layout test_import_identity
```

PASS: 205 tests, including all 17 ConfigSet methods, C2, ReturnRequest, bounded
admission, C-compiled ABI/layout, public compatibility, pickle and import identity.

Baseline negative control: archived main lacks both `ConfigSetCommand` and
`ControlChannel.submit_config_set`; running the new public-model and compiled
field-fidelity methods against it yields the expected failure and AttributeError.
An initial *Python-only* baseline archive run was invalid as full-suite evidence:
it lacked native headers/probes (33 errors and one failure). The complete source
archive passed all **483 baseline tests**, warnings-as-errors, against the same
unchanged native facade/provider. No production contract or test was weakened to
address the incomplete fixture. Newline validation initially
identified missing final newlines in the two newly created files; both were fixed.

The final committed source is requalified with every required command, including
`python3 scripts/check_final_newlines.py`, the focused warnings-as-errors run and
the relevant suite above. The delivery PR records the exact final commit/tree,
actual final results, changed files and exact-HEAD hosted check disposition. No
approval, merge or auto-merge is authorized; independent technical review remains
required.
