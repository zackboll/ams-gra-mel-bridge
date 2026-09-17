# Task 009 validation — 2026-09-17

## Scope

Task 009 adds a standalone, unpublished Rust integration application and extends
the opt-in Task-004 Squall harness. It adds no MEL functionality and changes no
public/native ABI, safe Rust semantics, Ada public API, or provider code. The
only normal-workspace Rust source changes are rustfmt output needed to close the
known Task-008 formatting-CI gap. The
application is deliberately outside `rust/Cargo.toml` and depends only on the
repository's safe `ams-mel` crate by path.

The same runtime invocation now validates:

```text
C public ABI -----------+
Ada public API -> C ABI +-> libams_mel_c -> Squall IR MEL provider
safe Rust -> sys -> C ABI+
```

No language client calls Couloir, gRPC, UDP, REST, or a Squall-private API.
Ordinary CI remains mock-provider based; the real integration is opt-in. This
work implies no RF support and no C2 operation beyond Operate/TaskSched.

## Pinned target and orchestration

The accepted target remains Squall
`b1015728f904c799fa0c07489fce48e78f67845f` with the unchanged dependency
closure:

| Path below Squall root | Commit |
|---|---|
| `ams-interfaces/common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` |
| `ams-interfaces/ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` |
| `ams-interfaces/rf-mel` | `762ce84c5555dd0f3ea66f36b321fecf8839b89f` |
| `ams-interfaces/ir-mel/ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` |
| `ams-interfaces/rf-mel/ams-vita` | `8e12a4cd7ac8ea8776d40b9d0b22fc4a22adaad8` |

`integration/squall/run.sh` still verifies exact revisions and clean external
checkouts, builds Task-owned provider/optical/Couloir image tags, extracts the
provider, renders `compose.yaml` plus `compose.build.yaml` plus its runtime
override, explicitly builds before `--no-build` startup, uses host networking,
generates a loopback profile with a unique client ID, waits finitely, and cleans
only Task-owned resources. `AMS_MEL_KEEP_SQUALL=1` remains the explicit retention
option; no image or system prune is used.

## Rust application contract

`integration/squall/rust` is a standalone package with `publish = false`, its
own empty `[workspace]`, a committed lockfile, and no crates.io dependencies.
The harness builds it locked and offline with an isolated
`build/squall/rust-target` and selects CMake's existing native façade through
`AMS_MEL_NATIVE_LIB_DIR`.

Through the safe API, one Session queries provider version, starts an IRSTImage
stream with four 1-MiB buffers and queue capacity eight, enables C2, submits
command `0x00400403`, and requires TaskSched within five seconds. It closes the
Session first, repeats the completed request wait with timeout zero, then receives
the requested real frames. Every frame must be 320x200, 8-bit, one-band, and
64,000 bytes, with increasing IDs after the first. It prints FNV-1a diagnostics,
requires received count at least requested and malformed count zero, permits
timing-dependent drops, and explicitly closes request, C2, and stream. Retryable
C2 close receives at most one explicit retry.

## Validation

Portable shell syntax and no-container regressions pass with:

```text
sh -n integration/squall/run.sh
dash -n integration/squall/run.sh
sh integration/squall/test-run.sh
```

The regressions cover Rust mode acceptance, invalid modes, all-language mode,
language-specific build isolation, exact provider/profile/frame/timeout inputs,
the Task-owned Cargo target directory, locked/offline Cargo, Rust ELF/link
auditing, unchanged pinned Compose/image behavior, and selective cleanup.

The standalone package passes offline `cargo check` and Clippy with warnings
denied. Local rustfmt is unavailable in the distribution Rust toolchain; the
hosted Rust job now installs Clippy and rustfmt and runs workspace
`cargo fmt --all -- --check` in addition to check, test, and Clippy. Exact
ordinary-validation and real-provider results are recorded below after final
execution.

### Ordinary validation results

The following passed:

- `make test-native`: 6/6 CTest tests;
- `alr -C ada build` and `alr -C ada/tests run`: all provider, image, and C2
  suites;
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel test-ada`;
- `alr -C ada exec -- make -C /home/zboll/git/ams-mel check`;
- workspace `cargo check`, `cargo test` (25 integration/ABI tests), and Clippy
  with `-D warnings`;
- standalone locked/offline `cargo check` and Clippy with `-D warnings`;
- `git diff --check` and the first-party final-newline audit.

The distribution toolchain did not initially provide `rustfmt`/`cargo-fmt`; the
matching Debian 1.85.1 package was extracted without system installation and
used to apply and check formatting for both manifests. The hosted job installs
rustfmt explicitly and enforces the normal workspace format check.

The public C header, `native/src`, `native/vendor`, exports map, Rust sys crate,
and Ada public source are unchanged. The safe Rust crate has rustfmt-only changes
and no semantic changes. Audits found exactly 16 public C function declarations,
16 Rust sys extern declarations, and 16 versioned ELF exports.
The release Rust application is an ELF PIE whose direct dependencies include
`libams_mel_c.so.0` and exclude `libsquall_ir_mel` and mock providers.

### One final real acceptance run

Exactly one final all-language run used:

```text
AMS_MEL_SQUALL_REPEAT=1
AMS_MEL_SQUALL_CONTROL_PORT=48673
AMS_MEL_SQUALL_COULOIR_METRICS_PORT=53497
AMS_MEL_SQUALL_OPTICAL_HEALTH_PORT=42549
AMS_MEL_SQUALL_OPTICAL_METRICS_PORT=43187
SQUALL_SOURCE_DIR=/home/zboll/git/squall
make test-squall-ir
```

The generated profile used `control_address=127.0.0.1:48673`,
`data_host=127.0.0.1`, and unique client ID `ams-mel-task004-1276283`. Exact
checkout verification accepted the root and dependency revisions listed above.
The extracted provider reported compiler comment
`GCC: (GNU) 14.3.1 20251022 (Red Hat 14.3.1-4)` and version API 1/library 1,
vendor `Squall`, description `Squall Simulator IR MEL`.

All clients returned `TASK_SCHED`. Each received frame IDs 0, 1, and 2 at
320x200 Mono8 and 64,000 bytes per frame. C and Rust printed FNV-1a checksum
`68caeb29100eae83`; Ada printed the equivalent decimal `7551106286835641987`.
Observed counters were received=3, dropped=0, malformed=0 for every client, and
all three printed their language-specific PASS line. The combined result was:

```text
PASS: Squall IR MEL integration (1 iteration(s), mode=all)
```

The exit status was zero. The exit trap removed the Task-owned containers and
local image tags; a post-run query found none with project name
`ams-mel-task004-1276283`. No prune was performed. The expensive stack was not
run again after this accepted success.
