# Task 013 validation — 2026-09-17

## Scope

Task 013 adds one standalone Python integration client and extends the opt-in
Task-004 Squall harness. It adds no MEL functionality and changes no C ABI,
native adapter/provider semantics, Python public binding semantics, Rust binding,
Ada binding, or mock provider. Ordinary tests and CI remain provider-free.

The validated Python path is:

```text
integration Python client -> public ams_mel API -> private ctypes
                          -> libams_mel_c -> Squall IR MEL provider
```

The client uses only the Python standard library and public `ams_mel` names. It
does not import `_native`, call `ctypes`, gRPC, Couloir, UDP, or Squall-private
APIs. It opens one Session, queries the provider version, starts an IRSTImage
stream with four 1-MiB buffers and queue capacity eight, explicitly enables IR
CommandAndControl, and submits exactly Operate/TaskSched command `0x00400403`.
It requires `ModeSuccess(MfaMode.TASK_SCHED)` within 5000 ms, closes the parent
Session first, requires the cached result again with `wait(0)`, receives owned
Python `bytes` frames, validates counters, and explicitly closes ModeRequest,
ControlChannel, and ImageStream. Retryable C2 detach receives at most one retry.

Each real frame must be 320x200, 8-bit, one-band, and 64,000 bytes. IDs must
increase after the first observed frame. FNV-1a is printed only as a diagnostic.
Received count must be at least the requested count and malformed count must be
zero; timing-dependent queue drops are permitted.

## Pinned target and dependency closure

The harness accepted the unchanged external source closure:

| Path below Squall root | Repository (`open-arsenal/...`) | Commit |
|---|---|---|
| `.` | `ams-gra-hello-world-sk-sensors-squall` | `b1015728f904c799fa0c07489fce48e78f67845f` |
| `ams-interfaces/common-mel` | `ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` |
| `ams-interfaces/ir-mel` | `ams-gra-hello-world-sk-interfaces-ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` |
| `ams-interfaces/rf-mel` | `ams-gra-hello-world-sk-interfaces-rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` |
| `ams-interfaces/ir-mel/ams-math` | `ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-math` | `ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-vita` | `ams-gra-hello-world-sk-libraries-ams-vita` | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` |

`integration/squall/run.sh` still verifies exact revisions, expected remote
identity, and clean tracked/untracked state before use. It retains unique image
tags, host networking, the generated loopback MEL profile, finite optical/
Couloir/control readiness, collision-checked Task-owned ports, and selective
cleanup. The external checkout was not modified and remained clean after both
accepted runs.

## Harness changes

Accepted modes are now `all`, `c`, `ada`, `rust`, and `python`; `all` executes
all four clients against one startup. C, Ada, and Rust retain their ELF dynamic
section/link audits. Python instead receives a `python3 -W error -m py_compile`
source preflight with `PYTHONPYCACHEPREFIX` under ignored `build/squall/` storage.
Execution explicitly selects repository `python/` with `PYTHONPATH`, the built
façade with `AMS_MEL_NATIVE_LIB`, and its runtime directory with
`LD_LIBRARY_PATH`. It does not use `AMS_MEL_TEST_PROVIDER_DIR`.

The new `make test-squall-ir-python` target selects Python only. The existing
`make test-squall-ir` target remains opt-in and now means C + Ada + Rust + Python.
No integration target was added to `make check`, CTest, Ada/Python/Rust unit
tests, or GitHub Actions.

## Ordinary validation

The following passed:

```text
sh -n integration/squall/run.sh
dash -n integration/squall/run.sh
sh integration/squall/test-run.sh
make test-python
make test-rust
git diff --check
```

The no-container harness regression covers Python mode acceptance, the invalid
mode diagnostic, `all` selection, conditional execution, source preflight,
public binding/native library environment, exact provider/profile/frame/timeout
arguments, retained C/Ada/Rust ELF checks, retained Rust link/build checks, and
the unchanged pinned topology and cleanup assertions.

`make test-python` passed native CTest 6/6 and 41 Python tests. `make test-rust`
passed native CTest 6/6 and all 25 Rust workspace integration/ABI tests. Plain
`make check` passed native CTest 6/6, then correctly stopped because
GNAT/GPRbuild is not on the ordinary shell `PATH`; it is not reported as passed.
The supported local Alire invocation passed completely:

```text
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
```

That run passed native CTest 6/6, all Ada provider/image/C2 suites, whitespace,
and the first-party final-newline audit.

The protected-area diff for `native/include/ams_mel/abi.h`, `native/src`,
`native/tests`, `rust`, and `ada` is empty. No generated Python bytecode or build
artifact is tracked.

## Real Python-only acceptance

The Python-only bring-up used:

```text
AMS_MEL_SQUALL_CONTROL_PORT=55083
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=38541
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=53861
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=50661
SQUALL_SOURCE_DIR=/home/zboll/git/squall
make test-squall-ir-python
```

The provider reported API 1, library 1, vendor `Squall`, and description
`Squall Simulator IR MEL`. Python received TaskSched and the cached repeated
TaskSched result. Frame IDs were 0, 1, and 2; every frame was 320x200 Mono8,
64,000 bytes, with diagnostic checksum `68caeb29100eae83`. Counters were
received=3, dropped=0, malformed=0. Explicit child teardown completed and the
result was:

```text
PASS: real Squall IR Python integration
PASS: Squall IR MEL integration (1 iteration(s), mode=python)
```

Exit status was zero.

## Final all-language real acceptance

No source changed after the Python-only acceptance. The final accepted command
used one runtime startup and explicit free Task-owned ports:

```text
AMS_MEL_SQUALL_REPEAT=1
AMS_MEL_SQUALL_CONTROL_PORT=33555
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=47151
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=32887
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=36595
SQUALL_SOURCE_DIR=/home/zboll/git/squall
make test-squall-ir
```

The generated profile used `control_address=127.0.0.1:33555`,
`data_host=127.0.0.1`, and client ID `ams-mel-task004-1447504`. The extracted
provider compiler comment was
`GCC: (GNU) 14.3.1 20251022 (Red Hat 14.3.1-4)`. Every client reported provider
API 1/library 1, vendor `Squall`, description `Squall Simulator IR MEL`, and
returned TaskSched. C, Ada, Rust, and Python each received frame IDs 0, 1, and 2
at 320x200 Mono8 and 64,000 bytes per frame, with counters received=3, dropped=0,
malformed=0. C, Rust, and Python printed checksum `68caeb29100eae83`; Ada printed
the equivalent decimal `7551106286835641987`. Python also printed its cached
zero-time TaskSched result and completed explicit child teardown.

All four language-specific PASS lines were present. The combined result was:

```text
PASS: Squall IR MEL integration (1 iteration(s), mode=all)
```

Exit status was zero. The exit trap removed the final run's Task-owned containers
and image tags; post-run queries found none containing
`ams-mel-task004-1447504`. No prune was used.

## Remaining unsupported scope

This evidence adds no RF support, additional C2 commands/callbacks, device-memory
images, zero-copy or NumPy API, wheel/PyPI publication, OMS/UCI integration, or
formal verification claim. Real Squall remains external-checkout and
container-runtime dependent, opt-in, and excluded from ordinary CI.
