# Task 034D3 — RF Job event duration extension in C and safe Ada

Starting main: `e0d151858dfc165715bbb665a8e93b40e319932d`; PR #67 verified merged.
Clean starting worktree; branch `feature/034d3-rf-job-event-extension`.
Measured baseline: native 256/256, Python 113/113, production exports 153,
ABI 0.1, vendor checksums 804/804. No vendor changes.

## Pinned contract and representation

RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` publishes:

```cpp
virtual void extendJobEvent(uint32_t intervalID, JobEventID eventId,
                           ams::util::math::Femtoseconds addedDuration) = 0;
```

It is RequiredIfJobEventDurationExtension, conditional on provider support.
Actual-vendored-header probes reuse exact uint32 JobEventID and int64 Fs::rep
assertions and add the exact member-function signature assertion. The C operation:

```c
ams_mel_status_t ams_mel_rf_job_extend_event(
    ams_mel_rf_job *job, uint32_t interval_id, uint32_t event_id,
    int64_t added_duration_femtoseconds, char *diagnostic,
    size_t diagnostic_capacity, size_t *diagnostic_required);
```

One new production export; no existing declaration/layout changes, no new owner,
result flag, enum, callback or record version. IDs and Fs{signed_count} are forwarded
without narrowing, floating point, unit conversion, Duration arithmetic, clamping,
sentinel translation or implicit quantization. Zero and INT64_MAX are ordinary
duration counts here, not interval-start continuation. INT64_MIN is never negated.
The frozen v1/Never, v2 reporting and ambiguous zero interval-start continuation
contract remain unchanged, including starts 0/1/-1/INT64_MAX.

OK means only the synchronous void provider method returned without throwing. It
does not establish event lookup, effective support, acceptance/application, changed
scheduling or eventual notification. There is no invented UNSUPPORTED/Accepted/
future/acknowledgement. The provider can throw or use its independent status path.

## Lifecycle, errors and ownership

interval_command(job, false, ...) admits open Jobs before Finalize, pending
Finalize and already-published terminal outcomes. It rejects after any full Cancel
attempt, including semantic False/None or exception, with PROVIDER_FAILED and a
clear lifecycle diagnostic. Cancel_Remaining is not full Cancel. The provider
remains authoritative even after terminal publication.

Every admitted explicit invocation calls the provider once. Identical calls are
not suppressed: extension may be additive. Never automatically retry after an
exception or short diagnostic buffer; the provider may already have mutated state.
Explicit application retries require provider-specific knowledge. A failed
extension does not poison/cancel/finalize the Job. Null owners and invalid diagnostic
pairings are rejected before exposure; bad_alloc maps INTERNAL_ERROR, other standard
or unknown exceptions PROVIDER_EXCEPTION. UTF-8-safe truncation preserves required
length without a second mutation.

Source audit: interval_command copies the shared JobState, inspects admission in
a braced JobState-mutex scope, exits that scope, then invokes the lambda, whose
only statement is detail.extendJobEvent(...). Neither queue nor C2 lifecycle mutex
is acquired. The callback feedback test demonstrates one synchronous callback and
distinct receiver interaction, not all possible provider locking behavior.
No worker, future, sibling claim or permanent registration is added. The existing
JobDetail/VA/C2-claim graph supports public parents closed first. Same-Job operations
remain externally serialized with Job Close. Immutable Job snapshot fields remain
point-in-time data, not live scheduling state.

ReceiveEvent already has MaxExtensionDuration: pinned docs say zero disallows
extension. Meaningful integration fixtures set 500,000,000 and request 123,456,789
femtoseconds. No bridge lookup/submission table or cumulative budget accounting is
added. Status registration is optional, including after stream Close.

## Safe Ada and mock evidence

AMS.MEL.RF.C2.Extend_Job_Event is an integer-only procedure taking in out Job,
Unsigned_32 Interval_ID/Event_ID and Integer_64 Added_Duration_Femtoseconds. It uses
the existing 512-byte fixed diagnostic mutation pattern, returns on native OK and
raises Provider_Error otherwise. No cache, retry or additional float suppression.
Native long-error tests verify a 1,205-byte required diagnostic including NUL,
a 510-byte whole-UTF-8 prefix in a 512-byte buffer, and one provider attempt.
GNAT exception occurrences have their own bounded message store (200 bytes on
the local runtime), so safe Ada tests verify the available whole-UTF-8 prefix
rather than promising the exception contains the full native diagnostic.

The existing MockJobDetail records ordered exact argument tuples before exceptions.
Dedicated C11 and safe Ada suites cover IDs FEDCBA98/80000001, independent 0/MAX,
durations 0/1/-1/123456789/INT64_MIN/INT64_MAX, identical repeated calls, immutable
snapshots, standard/unknown/bad_alloc/long UTF-8 errors and inspectable/closable Jobs.
The permissive recording mock proves ABI fidelity, not scheduler domain validity.

Dedicated feedback scenarios retain either a copy or the exact registered callable
reference on that Job. From extendJobEvent itself, outside bookkeeping locks, the
provider emits Started/eventExtended with requested interval ID/event-map key and
exact deterministic seconds/femtoseconds. Added duration is absent from the
published status payload and is checked only through the command observer.
No injector or callback getter supplies evidence for this path.

The condition-variable gate used by both C and Ada holds the provider after callback publication,
so a distinct receiver obtains the event before command return. Held events remain
unchanged across repeat notifications and closure. Stream Close does not block
forwarding; late callbacks are safely discarded. No registration is also tested.
Parent-first submission/Finalize/VA-C2 Close/Extend/Receive/snapshot/Cancel/Complete/
Job Close/STREAM_STOPPED/stream Close uses the existing graph and destruction order.
Status cases run in isolated processes; permanent DSO pins are not confused with
retaining JobDetail, VA or C2 claims. Legacy unload checks remain active.

## Real provider evidence limits

Squall `b1015728f904c799fa0c07489fce48e78f67845f` was rechecked in
SquallC2MEL.h: extendJobEvent and registerJobIntervalStatusCallback have empty
bodies. Existing Job and C/Ada ProductRx clients issue positive extension after
Finalize and public parent Close, then require TIMEOUT rather than eventExtended.
This is production command-call/lifecycle/no-delivery evidence only. Squall is
not modified; no acceptance, actual extension or changed scheduling is claimed.

This is event-extension command coverage, not complete JobDetail, complete
JobInterval or full RF MEL. Conditional commands/TX/RDMA/VADB expansion is excluded.

## Local validation

- `make test-native`: 257/257 (baseline 256/256), including the new C11 suite.
- GCC 14.2.0 and Clang 19.1.7 Debug/Release: 257/257 each; pinned header probe
  and RF closure gates included. Standalone vendored-header signature probes also
  pass with both compilers under `-Wall -Wextra -Wpedantic -Werror`.
- Native and safe Ada active-Job/feedback loops: 50/50 each. In addition,
  Clang Release CTest `--repeat until-fail:50` and 50 isolated safe Ada process
  invocations against that facade/mock pass. Gates/condition variables, no sleeps
  for provider or receiver synchronization.
- `make format-ada`, `make check-ada-format`, `alr -C ada build`,
  `alr -C ada/tests run`: PASS (Alire 2.1.1, GNAT 16.1.0, GPRbuild package 26.0.1;
  the executable reports GPRBUILD 26.0.0). Direct-GPR smoke runs via Alire PATH.
- `make test-build-isolation`: PASS. Build-driving commands sharing repository
  trees are sequential. An early overlapping integration attempt was discarded
  and rerun sequentially; it is not passing evidence.
- `make test-rust`: PASS with repository test-library/provider environment;
  workspace cargo check, clippy all-targets `-D warnings`, fmt check: PASS.
  rustc/cargo 1.98.1. No new safe Rust API.
- `make test-python`: 114/114 (baseline 113), Python 3.13.5; raw signature,
  inventory, C probe and public-surface regressions pass. No public Python API.
- Fresh production Release: **154 measured exports**, exact map parity,
  sole addition `ams_mel_rf_job_extend_event@@AMS_MEL_0.1`, no test-only symbols.
  Original 153 declarations and all record/enum bodies compare unchanged to main.
- Vendor SHA-256: **804/804**, zero vendor diff. ABI remains **0.1**.
- `git diff --check`: PASS; all feature files have final newlines.
- Aggregate `make check` via Alire PATH: native, isolation, Ada formatting and
  direct-GPR smoke pass, but its final-newline stage fails on four byte-identical
  baseline documents: `task-034b2b1-native-rf-job-lifecycle.md`,
  `task-034b2b2-safe-ada-rf-job-lifecycle.md`,
  `task-034c1-rf-duration-quantization.md`, `task-034c3-rf-tx-power-modes.md`.
  This aggregate is **not fully passing**; unrelated defects are preserved.

Pinned Squall commands all pass through the production library:
`make test-squall-rf-ada-job`, `make test-squall-rf-rx`,
`make test-squall-rf-ada`. C/Ada ProductRx additionally pass three iterations each
using `AMS_MEL_SQUALL_REPEAT=3`; isolated control ports 29203/30203 with respective
metrics/health/data overrides. The C runs receive eight 4096-element ComplexINT16
events, seven later events differing from held A, zero dropped/malformed/allocation
failures. No eventExtended notification is received or manufactured. Provider
extension and registration remain the reviewed no-ops: only production
command-call/lifecycle/no-delivery evidence is claimed.
