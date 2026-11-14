# IRMEL-ADA-017 — required-channel communications-test metadata

## Scope and source contract

Base: freshly fetched `main`, `76631880ee236b9839b8b9668be36e423ef1b050`
(includes merged PR #198 and subsequent Python/Rust work). Remote identity was
verified as `git@github.com:zackboll/ams-gra-mel-bridge.git`; no open overlapping
PRs existed when implementation began. Existing branches/worktrees were preserved.

The pinned required `Channel::registerMetadataCallback(ChannelCommsTestRep)`
operation is exposed on HealthAndStatus, IRSTImage and StackedImage through
`AMS.MEL.IR.Channel.Metadata`. C2 retains its existing three-family metadata
Open, separate `Enable_Comms_Test_Events`, one-shot flag and shared bounded queue.
The shared receiver refuses C2, Instrumentation, Track and Scheduling views.
No existing C2 implementation or public record layout changes.

The reference consumer was reviewed through authenticated GitHub source access
at `minimal-mel-example` commit `cf22a80ec98398eb361df758ada7bfb4024710a3`,
`examples/minimal_consumer.cpp`: four explicit registrations, one direct and one
C2-routed request per destination, two callbacks per destination, and separately
checked future results. Its numeric ChannelType routing addresses are a property
of that provider, not a generic channel-identity mapping. Reports have exactly
two uint32 fields (command ID and request ID), no UUID or channel-ID field.

## Registration and ownership

The public limited-controlled Ada stream has explicit idempotent Close and a
non-raising finalizer. Open accepts the existing weak common View; the View
continues to own neither the typed channel nor its provider. Health/Stacked admit
Attached or Enabled; Image admits Attached or Running, including control-only
attachments with zero layout. There is no capability-advertisement prerequisite.

The native weak adapter temporarily claims the existing family accounting while
registering, preventing physical detach during synchronous reentry. It invokes
provider registration without holding either lifecycle or queue mutex. The
one-shot lives on the underlying typed state, so failure/exception consumes the
attempt even through another View. Allocation/preparation failure before provider
entry is distinct from provider exception. Known provider Return values are
returned explicitly; NotSupported and Fail are not registration success.

The independently owned queue preallocates a ring of complete copied scalar
reports. Provider callbacks increment saturating received counters, reject/count
null reports, copy both full-width IDs, then admit only while active. A full FIFO
drops the incoming event, preserving earlier accepted entries. No per-event owner,
provider storage, channel identity inference or Ada callback is involved.

Zero timeout polls; finite timeout affects only that receive. Neither timeout nor
future completion cancels registration or fabricates metadata. Native request
workers and completion semantics are unchanged.

Public receiver Close deactivates/releases consumption but cannot unregister:
upstream publishes no unregister operation. The installed callback and typed
state retain queue state. Typed logical closure stops admission immediately;
accepted copies drain before stopped/failure status. Explicit receiver Close
releases its handle, so it cannot subsequently drain. Typed destruction precedes
the callback drain while Session/provider-code ownership is still retained.
Failed detach keeps the provider graph for retry (or existing permanent orphan
retention); disable failure is reported even after successful detach. No claim
that disable establishes callback quiescence is made. Pending common requests
retain the graph independently.

Operations/Close on the same public handle require external serialization.
Provider callback delivery may race consumption and Close; separate weak views
share typed one-shot accounting. Test-only callback barriers/preset counters are
not production exports or behavior.

## ABI and coverage delta

Exactly four additive `AMS_MEL_0.1` exports:

- `ams_mel_ir_channel_comms_metadata_open`
- `ams_mel_ir_channel_comms_metadata_receive`
- `ams_mel_ir_channel_comms_metadata_get_counters`
- `ams_mel_ir_channel_comms_metadata_close`

Existing `ams_mel_ir_channel_comms_test_report_v1` and
`ams_mel_ir_metadata_counters_v1` are reused unchanged. Production declarations
and export-map inventory: **311 → 315**. Private Python ctypes and raw Rust sys
mirror every new signature; no public Python or safe Rust receiver is added.

Reviewed inherited exposures: HealthStatusChannel, ImageChannel and
StackedImageChannel common communications metadata **missing → safe_ada**.
C2 remains safe_ada. Instrumentation, Track and Scheduling remain missing.
The unique upstream registration stays **partial overall** and the unique
safe-Ada upstream operation count stays **50**. Authoritative classifications,
reviewed ceiling and generated report are updated; historical `docs/coverage.md`
is untouched.

## Qualification

Development feedback: the initial 52 compiled-C receiver cases passed. They
exercise both Image modes, all three new families, direct/routed callbacks,
high-bit IDs, duplicate registration through a second View, synchronous callback,
refusal/exception/partial installation, null payload, overflow, timeout, draining,
callback/future independence, held futures, callback reentry, receiver Close in
flight and normal unload. Additional registration-reentry, typed-close-in-flight,
failure-retention and counter saturation cases supplement these before final
qualification. Safe Ada tests independently check exactly eight callbacks and
eight futures across the four required destinations, plus lifecycle/error cases.

An ambient `make test-ada` attempt failed because gprbuild was not on PATH.
Subsequent Ada commands use the existing Alire-resolved GNAT 16.1.0 / GPRbuild
26.0.1 environment without changing pins. Final sequential command results are
recorded below after execution.

Qualification on the implementation before the final mainline reconciliation:

| Command | Actual result |
|---|---|
| `make format-ada` | Passed, pinned GNATformat 26.0.0. |
| `make check-ada-format` | Passed. Existing imported native GPR reports no C sources; it intentionally imports rather than compiles native code. |
| `make test-native` | Passed **656/656**, including **69** new receiver cases. |
| `make test-python` | Passed **563** Python tests, full 315-function inventory and signature checks; also reruns native tests. |
| `make test-rust` | Passed **484** tests across workspace suites, with **9** existing ignored tests; complete 315-export raw inventory and new signatures checked. |
| `alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge test-ada` | Passed the full safe Ada smoke/focused suite, including the eight-callback/eight-future test, zero-layout enabled Image, and future-only negative control. |
| `make check-ir-api-coverage` | Passed **62** regression controls; 50 unique safe-Ada operations, common metadata partial overall. |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | Passed. |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | Passed. |
| `python3 scripts/check_doc_source_integrity.py` | Passed, **825** vendor files / **4** workflows; no vendor bytes changed. |
| `make test-policy` | Passed all retained policy/security/documentation controls. |
| Strict C11 receiver test TU: `cc -std=c11 -pedantic-errors -Wall -Wextra -Werror -I... -c native/tests/test_channel_comms_metadata.c` | Passed exact signatures and reused record layout assertions. |
| Production `nm -D --defined-only` versus export map | Passed **315** exact `@@AMS_MEL_0.1` functions, no test hooks. `readelf -d` finds no RPATH/RUNPATH in either facade. |
| Release: `cmake -S native -B /tmp/irmel017-release -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release`; build receiver/C2/Image/Stacked targets; filtered CTest | Passed **99/99** receiver and existing foundation regressions. |
| Release CTest concurrency filter `^comms_metadata_.*(async\|reentry\|held)` with `--repeat until-fail:25` | Passed **500** executions (20 cases × 25), deterministic provider barriers, no sleeps. |
| `python3 scripts/check_final_newlines.py`; `git diff --check` | Passed. |

The first full native qualification found two incorrect new-test expectations:
Image preserves an earlier cleanup failure even when a detach retry succeeds.
The tests now assert `PROVIDER_FAILED` with a cleared Image handle on that retry;
the implementation's existing failure contract was not weakened. A development
smoke invocation without `AMS_MEL_TEST_PROVIDER_DIR` failed in an unrelated RF
child; the repository wrapper supplies it and the full subsequent Ada run passed.
An extra Rust rerun attempted while build-tree isolation was replacing its linked
DSO failed to link; it is not counted as a passed run. Final reruns wait for the
sequential build-tree qualification to finish.

Aggregate `make check` passed (production/test-tree isolation, native **656/656**,
Ada formatting, coverage controls, full Ada suites and whitespace/newline checks).
`alr -n -C ada build` and `alr -n -C ada/tests run` both passed in sequence using
the existing pinned environment. The ambient make targets were invoked through
`alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge ...` where GNAT
was needed; this supplies the existing toolchain rather than changing it.

Mainline reconciliation uses ordinary merge commits, never rebase/amend or force
push. `1d7cf8a11ee6fc154fee2b577aa7296b5a1fbd0a` (Python Image control-only)
merged cleanly in `58065c4940554a26e091a74f2372d6dc99cad260`. The subsequent
Python rerun passed **584** tests (the extra 21 are mainline's Image control-only
coverage). Rust raw ABI remains 315 functions. Later mainline reconciliation and
final rerun results are recorded before delivery.

Real-provider status: **PRIVATE_PROVIDER_INTEGRATION_UNTESTED**. No local
`minimal-mel-example` provider checkout was found. Authenticated source review is
not an integration run, and mocked callbacks do not qualify physical sensor
communications transport.

## Boundaries and unresolved semantics

This establishes required-channel communications-test callback parity, not full
C++ consumer parity. No standalone Disable, generic/application Buffer API,
StackedImage acquisition, LOS3D, other telemetry, image format, extra sensor
command, or Instrumentation/Track/Scheduling registration is added. Provider
C++ ABI compatibility remains necessary. Upstream offers no unregister or
disable-quiescence guarantee; those limitations are preserved, not guessed away.
