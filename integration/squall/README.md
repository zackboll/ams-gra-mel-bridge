# Real Squall IR MEL integration

This opt-in integration validates the public C, Ada, safe Rust, and safe Python façades
against the real Squall IR MEL provider at commit
`b1015728f904c799fa0c07489fce48e78f67845f`. It uses Squall's hardware-free
simulated checkerboard optical MFA (`320x200` Mono8 at 30 FPS), Couloir, and the published MEL
boundary. C, Ada, safe Rust, and safe Python require one empty/no-op BIT command
to return Success in addition to TaskSched and real Mono8 frames. All four clients use the same
provider/runtime invocation. The
applications do not call Squall gRPC, Couloir, UDP, REST, or any private backend
API. Rust uses `ams-mel` -> `ams-mel-sys` -> `libams_mel_c`; Python uses the
public `ams_mel` API -> private `ctypes` -> `libams_mel_c`. Both reach only the
runtime-loaded provider through the façade.
Ada additionally exercises Task 017's general Mode, BIT payload, and ConfigSet
safe APIs. It requires Standby/Unused and Operate/TaskSched success, provider
rejection of Operate/ScanVolumeSched, empty ConfigSet Success, nonempty ConfigSet
Fail, and payload-bearing BIT Fail. Complete ScanParam fidelity is mock-proven;
pinned Squall rejects scan scheduling before meaningfully consuming ScanParam.
Task 021 additionally has Ada consume one further complete FrameHeader snapshot after
the legacy frames. Pinned Squall currently reports sparse values: 320x200 Mono8,
Staring/None, zero integration time, configured contributing-sensor location with ID
zero, and empty flag/inertial/nav vectors. Rich nested fidelity is mock-proven.
C, Rust, and Python retain the earlier integration subset.
Ada also opens Task 018's metadata stream before command activity. It validates
pinned Squall's current empty default BIT_Configuration/BIT_Status, correlates
Accepted and Rejected CommandStatus values (including complete rejection text)
for Mode, BIT, and ConfigSet command IDs, observes the no-op BIT status event,
and requires zero malformed/dropped metadata counters. Rich nonempty metadata
fidelity is established by the separate mock-provider tests, not by Squall's
current empty defaults. Safe C/Rust/Python integration behavior is unchanged.
Task 024 additionally has Ada query the Image capability before start and require
320x200, 8-bit, one-band Mono with BadPixelList advertised. It opens Image metadata
before image start and validates Squall's synchronous initial empty BadPixelList
(reported size/count and actual pixel count all zero). Rich BadPixel fidelity and
callback lifetime are mock-proven. C, Rust, and Python safe integration behavior remains
unchanged; LOS and NavigationReport are not asserted.

Provide an existing checkout with this exact source closure:

| Path below Squall root | Repository (`open-arsenal/...`) | Commit |
|---|---|---|
| `.` | `ams-gra-hello-world-sk-sensors-squall` | `b1015728f904c799fa0c07489fce48e78f67845f` |
| `ams-interfaces/common-mel` | `ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` |
| `ams-interfaces/ir-mel` | `ams-gra-hello-world-sk-interfaces-ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` |
| `ams-interfaces/rf-mel` | `ams-gra-hello-world-sk-interfaces-rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` |
| `ams-interfaces/ir-mel/ams-math` | `ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-math` | `ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-vita` | `ams-gra-hello-world-sk-libraries-ams-vita` | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` |

For example, after cloning Squall, populate its ignored dependency directories
explicitly (repeat the two AMS Math clones because they are separate build
contexts):

```sh
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-sensors-squall.git squall
git -C squall checkout --detach b1015728f904c799fa0c07489fce48e78f67845f
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel.git squall/ams-interfaces/common-mel
git -C squall/ams-interfaces/common-mel checkout --detach f6908437d8fd2f7fb69896f9eb9cfd272d10c439
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel.git squall/ams-interfaces/ir-mel
git -C squall/ams-interfaces/ir-mel checkout --detach 8d9224519f12b44e0b28815755c56a32a28d24a0
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-interfaces-rf-mel.git squall/ams-interfaces/rf-mel
git -C squall/ams-interfaces/rf-mel checkout --detach 762ce84c5555dd0f3ea66f36b321fecf8839b89f
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-math.git squall/ams-interfaces/ir-mel/ams-math
git -C squall/ams-interfaces/ir-mel/ams-math checkout --detach 00be45190f0e47d268cece8b8c2f8fb58b5418d2
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-math.git squall/ams-interfaces/rf-mel/ams-math
git -C squall/ams-interfaces/rf-mel/ams-math checkout --detach 00be45190f0e47d268cece8b8c2f8fb58b5418d2
git clone https://github.com/open-arsenal/ams-gra-hello-world-sk-libraries-ams-vita.git squall/ams-interfaces/rf-mel/ams-vita
git -C squall/ams-interfaces/rf-mel/ams-vita checkout --detach 8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8
```

The harness verifies every checkout's exact `HEAD`, expected remote identity,
and absence of tracked or untracked non-ignored changes. It never clones,
checks out, cleans, pulls, or writes into any external checkout. Run it with:

```sh
SQUALL_SOURCE_DIR=/path/to/ams-gra-hello-world-sk-sensors-squall \
  make test-squall-ir
```

The checkout must be exactly the pinned commit and have a remote URL identifying
`open-arsenal/ams-gra-hello-world-sk-sensors-squall`. Podman or Docker plus a
Compose implementation is required. The E2E compose file is used only to build
and extract the MEL consumer/provider image. Runtime uses Squall's root
`compose.yaml`, `compose.build.yaml`, and a Task-004-owned temporary override,
with `squall-optical`
and `couloir` both on `network_mode: host`. The resulting
`/usr/lib64/libsquall_ir_mel.so` is copied to ignored local
`build/squall/`. The MEL consumer build receives a unique explicit local image
tag, which the selected container runtime inspects and uses directly for
extraction; image discovery does not depend on Compose CLI extensions.
The provider tag is removed after extraction. Runtime optical and Couloir are
built from this same verified checkout under unique Task-004 tags. The harness
first renders the required three-file composition to one canonical temporary
Compose file, preventing multi-file Compose implementations from also tagging
the historical registry image names. It then builds, inspects, and starts the
unique images without another build; upstream registry `:latest` tags cannot
satisfy the run. Normal teardown removes only those unique tags and keeps shared
layers. `AMS_MEL_KEEP_SQUALL=1` retains both the stack and its runtime tags.
Running service discovery, status, logs, and network-mode inspection use the
selected container runtime's Compose labels directly rather than version-specific
Compose `ps` or `logs` syntax.

For the host-side C/Ada/Rust/Python clients, the harness writes a temporary runtime override
and Couloir TOML under ignored `build/squall/tmp/`. The TOML preserves Squall's
pinned RF/optical Unix-socket routes and binds Couloir directly at
`127.0.0.1:<control-port>`; it also assigns a free Task-004 metrics port. The
generated MEL profile is `control_address=127.0.0.1:<control-port>`,
`data_host=127.0.0.1`, and a unique Task-004 client ID. No bridge route, host
alias, port publication, or gateway discovery is used. The harness asserts host
network mode for both services and waits finitely for optical HTTP readiness,
Couloir metrics TCP, and MEL-control TCP. The generated override explicitly
sets numeric optical health/metrics ports, so inherited `SQUALL_OPTICAL_*_PORT`
values cannot override the Task-004 selection. It also pins Squall's numeric
`SQUALL_IR_DATA_PORT=21600` default. The checkerboard sink has no static
destinations, so Squall validates this deployment override but its static-
destination loop does not replace the runtime-fillable MEL destination. All
four host-network ports are
validated, required to be distinct, checked for existing listeners, and printed
before startup.

The Ada client registers CommsTest metadata, queries the sparse C2 capability,
sends KeepAlive, and submits CommsTest before `C2.Enable`. It verifies async and
callback IDs, then continues all Task 017/018 commands, metadata, and Mono8
frames. Zero/default capability fields are observations of pinned Squall only;
mock tests establish complete capability fidelity.

The Ada client also opens Health/Status and proves all six required callback
registrations before enabling it. Its capability check requires HealthAndStatus
and the currently enumerable ChannelCommsTestRep, MFAStatus, BITStatus,
SubsystemStatusResp, and MFAStatusDetailed entries; the pinned interface has no
capability enum values for the required DiscreteStatus and SecurityAudit
callbacks. It polls without assuming order until Squall's five emitted Health
kinds are observed. SecurityAudit registration succeeds, but pinned Squall does
not emit a SecurityAudit event; mock coverage exercises its complete value graph.

The final Task 020 Ada-only acceptance ran 2026-09-18 against Squall
`b1015728f904c799fa0c07489fce48e78f67845f` with Podman 5.4.2/podman-compose
on ports 39203 (control), 39318 (Couloir metrics), 39315 (optical health), and
39316 (optical metrics). It reported provider API/library 1/1, vendor Squall,
and description `Squall Simulator IR MEL`; Health received five events with no
drops or malformed events. See `docs/task-020-validation.md` for exact pinned
Health values. Final all-language one-startup acceptance passed C, Ada, Rust,
Python, and combined on ports 40203, 40318, 40315, and 40316.

Before runtime startup, each selected compiled integration client (C, Ada, and
Rust) must pass `file` and `readelf` checks as a real ELF executable with a
readable dynamic section. The interpreted Python client instead passes
`python3 -W error -m py_compile`, with bytecode redirected below ignored build
storage. No client may directly use Squall or a mock provider, and the Rust
executable must need `libams_mel_c`. Internal stub validation can redirect all
Task-004 outputs with `AMS_MEL_SQUALL_BUILD_DIR`; ordinary runs use the ignored
repository `build/squall/` directory.

Port 21203 is the default host MEL-control port. If it is occupied, select a
reproducible free port explicitly:

```sh
AMS_MEL_SQUALL_CONTROL_PORT=21204 \
SQUALL_SOURCE_DIR=/home/zboll/git/squall \
  make test-squall-ir
```

Optional controls:

- `make test-squall-ir-c`, `make test-squall-ir-ada`,
  `make test-squall-ir-rust`, or `make test-squall-ir-python` selects one client;
  `make test-squall-ir` runs all four in one runtime startup.
- `AMS_MEL_SQUALL_REPEAT=3` repeats complete open/command/receive/teardown runs.
- `AMS_MEL_SQUALL_FRAMES=N` requests at least `N` frames (default 3).
- `AMS_MEL_SQUALL_FRAME_TIMEOUT_MS=N` sets each finite receive timeout.
- `AMS_MEL_SQUALL_CONTROL_PORT=N` selects the host MEL-control port (default 21203).
- `AMS_MEL_SQUALL_COULOIR_METRICS_PORT=N` selects Couloir metrics (default 21318).
- `AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=N` selects optical health (default 21315).
- `AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=N` selects optical metrics (default 21316).
- `AMS_MEL_KEEP_SQUALL=1` leaves a stack started by the harness running.

The script refuses to replace pre-existing `couloir` or `squall-optical`
containers because upstream's default compose file fixes those container names.
Otherwise its exit trap tears down only the compose project it started. No
integration target is included in `make check`, normal CTest, Ada tests, or CI.
On client failure it prints runtime status/logs and the generated profile; C
receive failures additionally print IR stream counters. When available, `ss -lun`
is recorded before and after each client for host UDP diagnostics. The Rust
and Python clients open image and C2 graphs on one Session, obtain BIT Success
and TaskSched, close the Session parent first, repeat both cached request waits,
receive real 320x200 Mono8 checkerboard frames, validate counters, and explicitly
close their child resources. For `all`, C, Ada, Rust, and Python validate the
shared integration subset: BIT no-op, TaskSched, and Mono8. Ada additionally
validates Task 017's general Mode, payload-bearing BIT, and ConfigSet; Task 018's
required C2-specific metadata; and Task 019's pre-enable KeepAlive, CommsTest
request/reply and callback, and ChannelCapability; plus Task 024 Image capability and
initial BadPixelList. Explicit generic buffer
management, optional/conditional C2 commands, Scheduling, RF, other unbound MEL
families, and safe Rust/Python common-channel APIs remain unsupported. The Python
client uses only the public safe API,
including Python-owned `bytes` frames and explicit Return/mode request, control,
and stream teardown.
