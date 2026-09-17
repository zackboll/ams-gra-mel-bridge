# Task 004 validation — 2026-09-16

## Scope and pinned target

Task 004 adds an opt-in integration application and no public ABI or native-core
exports. The external target is
`open-arsenal/ams-gra-hello-world-sk-sensors-squall` commit
`b1015728f904c799fa0c07489fce48e78f67845f`. No Squall source is vendored,
installed, or compiled into `ams_mel_c`.

The accepted build used these exact external dependency checkouts:

| Path below Squall root | Repository | Commit |
|---|---|---|
| `ams-interfaces/common-mel` | `open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` |
| `ams-interfaces/ir-mel` | `open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` |
| `ams-interfaces/rf-mel` | `open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` |
| `ams-interfaces/ir-mel/ams-math` | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-math` | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-vita` | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita` | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` |

Pinned source inspection covered the upstream README, default/build compose
files, Containerfile, `tests/mel-boundary-e2e`, the IR MEL profile and
implementation, `SquallIRFactory.cc`, `SquallImageChannel.*`,
`SquallC2Channel.*`, `SquallMelConfig.h`, and Couloir/optical configuration.
Squall states MEL is its only supported application-facing interface. Its
hardware-free E2E optical source is a 30 FPS Mono8, 320x200 simulated
checkerboard. Its 64,000-byte frames fit Squall's single-datagram MEL receiver
profile. Squall's normal deployment host-networks optical and Couloir; the MEL
provider binds its host UDP receiver on an ephemeral port and registers
`127.0.0.1:<ephemeral-port>` with the backend.

The upstream CMake target is C++20 `squall_ir_mel`, producing
`build-ir/libsquall_ir_mel.so`. The upstream MEL consumer image places it at
`/usr/lib64/libsquall_ir_mel.so`. The Task-004 script uses upstream compose build
targets, extracts that binary to ignored `build/squall/provider/`, and runtime
loads it through `ams_mel_session_open`. It does not modify the external checkout.
The E2E compose file is used only for provider image construction/extraction:

```sh
cd "$SQUALL_SOURCE_DIR/tests/mel-boundary-e2e"
<compose> -p <unique-project> \
  -f "$SQUALL_SOURCE_DIR/tests/mel-boundary-e2e/compose.yaml" \
ARTIFACTS_DIR=<local-build-dir> \
SQUALL_MEL_DATA_CONSUMER_IMAGE=localhost/<unique-provider-project>:latest \
  <compose> -p <unique-provider-project> \
  -f "$SQUALL_SOURCE_DIR/tests/mel-boundary-e2e/compose.yaml" \
  build squall-ir-data-consumer
```

Runtime is instead equivalent to:

```sh
cd "$SQUALL_SOURCE_DIR"
<compose> -p <unique-project> -f compose.yaml -f compose.build.yaml \
  -f <task-004-runtime-override> build squall-optical couloir
<compose> -p <unique-project> -f compose.yaml -f compose.build.yaml \
  -f <task-004-runtime-override> up -d --no-build squall-optical couloir
```

The override retains `network_mode: host`, mounts the E2E checkerboard
`optical.toml` read-only, and mounts a generated read-only Couloir TOML. That
TOML has `tcp_addr = "127.0.0.1:<control-port>"`, a free Task-004 metrics port
(default 21318), and unchanged routes to the RF and optical `/sockets` paths.
The override also supplies literal numeric optical health/metrics environment
values (defaults 21315/21316), taking precedence over both inherited host values
and the mounted TOML. It pins the upstream numeric `SQUALL_IR_DATA_PORT=21600`
default as well. The pinned checkerboard sink's `destinations` list is empty;
Squall applies that port override only while looping over configured static
destinations, so the value validates cleanly without converting the empty MEL
runtime-fillable sink into a static destination. The readiness URL uses the
same selected health port.

The override also assigns unique local optical and Couloir image tags. Thus the
three required inputs are rendered to one canonical temporary Compose file
before build, avoiding multi-file implementations that also tag superseded
image names. The build uses the verified checkout and startup cannot substitute
either root Compose registry `:latest` image. The provider tag is removed after extraction;
normal teardown removes only the two Task-004 runtime tags, without pruning
shared layers or upstream/user tags. A retained stack retains its image tags.

The harness verifies that exact image tag with the selected container runtime,
then uses its `create`/`cp` operations to obtain
`/usr/lib64/libsquall_ir_mel.so`. It does not rely on provider-specific Compose
image-listing syntax and never compiles upstream C++ into this repository.

## Harness and expected acceptance evidence

Run:

```sh
SQUALL_SOURCE_DIR=/path/to/ams-gra-hello-world-sk-sensors-squall \
  make test-squall-ir
```

`AMS_MEL_SQUALL_REPEAT=3` is an optional stress setting. The accepted real run
intentionally used the default single iteration.

The combined target builds/runs both clients. The runtime uses Squall root
compose, its build overlay, and a Task-004-owned override; both
`squall-optical` and `couloir` are
required and inspected to use host networking. The host control port defaults to
21203 and can be set explicitly with `AMS_MEL_SQUALL_CONTROL_PORT`; host
networking cannot remap it. Each client opens the provider with a generated
profile containing `control_address=127.0.0.1:<control-port>`,
`data_host=127.0.0.1`, and a unique client ID;
prints the provider version, starts IRSTImage first, enables C2, sends exactly
Operate/TaskSched, requires TaskSched completion, closes the public Session,
re-inspects the completed request, and receives at least three owned Mono8
frames through retained children. It validates checked width-times-height,
complete pixel copies, increasing Squall frame IDs, counters, and zero malformed
frames. Checksums are diagnostic only. Finite defaults are 5 seconds for C2 and
10 seconds per frame. Before launching clients, the harness waits finitely for
optical health, Couloir metrics, and MEL-control TCP connectivity. On a client
failure it emits runtime status/logs and the generated profile; C receive
failures also report IR counters, and host `ss -lun` snapshots are recorded when
available. Both clients must be ELF
executables with readable ELF headers before the runtime stack starts.

The applications use only `<ams_mel/abi.h>` or public `AMS.MEL`, `AMS.MEL.IR`,
and `AMS.MEL.IR.C2`. They contain no Squall headers, private transport calls,
direct UDP receive, REST, OMS/UCI, or image processing. ELF checks reject a
direct `libsquall_ir_mel.so` dependency. The harness refuses to replace an
existing upstream-named stack and traps cleanup of only resources it started;
`AMS_MEL_KEEP_SQUALL=1` is an explicit debugging exception.

## Accepted real-provider result

One complete real-provider run was accepted against Squall revision
`b1015728f904c799fa0c07489fce48e78f67845f`. The extracted provider was
`libsquall_ir_mel.so`, built with GCC 14.3.1 (Red Hat 14.3.1-4), and reported:

```text
api=1
library=1
vendor=Squall
description="Squall Simulator IR MEL"
```

The corrective acceptance run generated
`control_address=127.0.0.1:57485` and
`data_host=127.0.0.1`. C2 completed with `TASK_SCHED`. Both image clients passed:

| Client | Frames | Dimensions | Bytes/frame | Received | Dropped | Malformed | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| C | 3 | 320x200 | 64000 | 3 | 0 | 0 | PASS |
| Ada | 3 | 320x200 | 64000 | 3 | 0 | 0 | PASS |

The combined harness result was:

```text
PASS: Squall IR MEL integration (1 iteration(s), mode=all)
```

This one complete real-provider run was intentionally accepted. Additional
full-stack repetitions were not performed because rebuilding the external
Squall stack is expensive. Provider-independent unit tests retain repeated and
parallel coverage below.

## Integration defects found and corrected

The accepted run exercised the harness itself and exposed issues that source-
only validation did not. Task 004 now includes:

- preflight checks for all required Squall dependency checkouts;
- absolute Compose file and bind-mount paths;
- command forms compatible with podman-compose 1.3.0;
- explicit provider image tagging, inspection, and extraction;
- host-network optical/Couloir topology matching pinned Squall;
- configurable, validated, collision-checked control/runtime ports;
- sanitization of inherited Squall optical and IR-data environment values; and
- isolated stub build outputs plus ELF validation of both integration clients.

## Final project validation

Before and after changes, `make test-native` passed 6/6 and `alr -C ada build`
plus `alr -C ada/tests run` passed all existing Ada suites. These additional
commands passed:

```sh
alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada
alr -C ada exec -- make -C /home/zboll/git/ams-mel check
```

Fresh GCC/G++ 14.2.0 Debug and Release builds each passed serial CTest 6/6 and
50 consecutive parallel repetitions (`--parallel 4 --repeat until-fail:50`). A
fresh GCC ASan+UBSan Debug build passed 6/6 with leak detection and halt-on-error.
Sanitizers were run only against provider-independent tests, not a prebuilt
Squall binary.

The new C application compiled with
`-std=c11 -pedantic-errors -Wall -Wextra -Werror`. The public header separately
passed the same strict C11 mode. The new Ada application compiled with GNAT
16.1.0 through Alire. `nm` reported exactly the existing 16 versioned exports,
with no public ABI/source change. `readelf` showed the C and Ada applications
directly need `libams_mel_c.so.0` and only ordinary language/system runtimes;
neither directly needs Squall. The façade directly needs only `libstdc++`,
`libgcc_s`, and `libc`, not a provider.

A fresh Release install contains only the façade library/header, CMake package,
licenses, and upstream notices—no integration client, Squall source/library, or
container configuration. Shell syntax, missing-checkout failure, `git diff
--check`, and the first-party final-newline audit passed. ShellCheck is
unavailable locally. Upstream links its provider with `-static-libstdc++
-static-libgcc`; the accepted result is therefore specific to the pinned
provider/compiler combination above because the published factory still crosses
C++ `shared_ptr`, string, RTTI, and exception ABI boundaries.

The real provider offers no public unload counter. Successful continued frame
receive after parent close and clean final teardown are observable; exact unload
timing remains limited to loader behavior and cannot be instrumented privately.
The integration remains excluded from `make test-native`, `make test-ada`,
`make check`, ordinary configure, install, and GitHub push/PR CI.
