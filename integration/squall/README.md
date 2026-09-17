# Real Squall IR MEL integration

This opt-in integration validates the public C and Ada façades against the real
Squall IR MEL provider at commit
`b1015728f904c799fa0c07489fce48e78f67845f`. It uses Squall's hardware-free
simulated checkerboard optical MFA (`320x200` Mono8 at 30 FPS), Couloir, and the published MEL
boundary. The applications do not call Squall gRPC, Couloir, UDP, REST, or any
private backend API.

Provide an existing checkout; the harness never clones, checks out, cleans,
pulls, or writes into it:

```sh
SQUALL_SOURCE_DIR=/path/to/ams-gra-hello-world-sk-sensors-squall \
  make test-squall-ir
```

The checkout must be exactly the pinned commit and have a remote URL identifying
`open-arsenal/ams-gra-hello-world-sk-sensors-squall`. Podman or Docker plus a
Compose implementation is required. The E2E compose file is used only to build
and extract the MEL consumer/provider image. Runtime uses Squall's root
`compose.yaml` plus a Task-004-owned temporary override, with `squall-optical`
and `couloir` both on `network_mode: host`. The resulting
`/usr/lib64/libsquall_ir_mel.so` is copied to ignored local
`build/squall/`. The MEL consumer build receives a unique explicit local image
tag, which the selected container runtime inspects and uses directly for
extraction; image discovery does not depend on Compose CLI extensions.
Running service discovery, status, logs, and network-mode inspection use the
selected container runtime's Compose labels directly rather than version-specific
Compose `ps` or `logs` syntax.

For the host-side C/Ada clients, the harness writes a temporary runtime override
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

Before runtime startup, both integration clients must pass `file` and `readelf`
checks as real ELF executables. Internal stub validation can redirect all
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

- `make test-squall-ir-c` or `make test-squall-ir-ada` selects one client.
- `AMS_MEL_SQUALL_REPEAT=3` repeats complete open/operate/receive/teardown runs.
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
is recorded before and after each client for host UDP diagnostics.
