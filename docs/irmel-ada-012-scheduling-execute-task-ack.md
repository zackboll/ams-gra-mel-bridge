# IRMEL-ADA-012 — complete Scheduling ExecuteTaskAck metadata

## Base and scope

PR #177 was merged before this task. Fresh remote refs selected `origin/main` at
`2596c1cd31b5e974a7306b4a5a5041a9902b91ef`, tree
`5fb47bba874109dc311e8dd40eac0098ec4c6a35` (the same tree as prerequisite head
`30df660cdc29a2d2afd904d0f9a28b62dcdfe28b`). The feature worktree is
`/home/zboll/git/ams-mel-irmel-ada-012`, branch
`feature/irmel-ada-012-scheduling-execute-task-ack`; target is `main`, not stacked.
The prerequisite branch/worktree is unchanged. No rebase, amend, force push, self-approval,
auto-merge or PR merge is performed by this feature. A later fetch found PR #178 merged as
`a885d7730dd1b988ca4c4a95ce4864d5c325df68`. An ordinary merge brought that documentation-only
main advance into this branch (`410202b81d612ca930428503b4eb01cd27efb14c`, tree
`a0f3f97dcdd0705c8c789a1de916d5957fabf0ec`). Shared status-only prose additions were removed
in accordance with the new policy; this task-owned document retains their information.
All compiled implementation/test inputs are identical across that reconciliation.

This slice adds only owned reception of
`SchedulingChannel::registerMetadataCallback(ExecuteTaskAck)` through one explicit opt-in
owner. It never implements or invokes any Scheduling `exec` or
`send(PrioritizedTrackListCmd)` operation. Receipt means the provider delivered an
acknowledgement; it proves neither execution completion nor success and has no bridge-local
request correlation. The Scheduling channel itself remains partial.

## Pinned upstream and field mapping

IR MEL revision `8d9224519f12b44e0b28815755c56a32a28d24a0`, tree
`bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`, is unchanged. The already vendored
`SchedulingChannel.h` declares exactly
`std::function<void(Channel&, const ExecuteTaskAck* const)>` for this overload.
`ExecuteTaskAck.h` supplies exactly two payload getters; compile-time checks bind both
getter return types to upstream. No provider object layout is exposed.

| Upstream getter | Pinned type | C field | Ada field |
|---|---|---|---|
| `getCommandID()` | `std::uint32_t` | `command_id : uint32_t` | `Command_ID : Interfaces.Unsigned_32` |
| `getReqId()` | `std::uint32_t` | `request_id : uint32_t` | `Request_ID : Interfaces.Unsigned_32` |

The complete `ams_mel_ir_execute_task_ack_v1` is 8 bytes (alignment 4, offsets 0 and 4).
Both identifiers retain all 32 bits: zero, UINT32_MAX, high bits, unequal and duplicate values
are valid. The existing `AMS_MEL_IR_METADATA_EXECUTE_TASK_ACK = 14` is unchanged and checked
against the upstream enum. No nonzero, equality or uniqueness requirement is imposed.

## Additive ABI and safe Ada ownership

Six new exports, all under the existing `AMS_MEL_0.1` version:

- `ams_mel_ir_scheduling_execute_task_ack_open`
- `ams_mel_ir_scheduling_execute_task_ack_receive`
- `ams_mel_ir_scheduling_execute_task_ack_get_counters`
- `ams_mel_ir_scheduling_execute_task_ack_close`
- `ams_mel_ir_scheduling_execute_task_ack_event_view`
- `ams_mel_ir_scheduling_execute_task_ack_event_close`

Production inventory increases 289 → 295; no old export, layout, enum or signature changes.
C11 and C++ signature checks cover all six exports. Python ctypes and Rust sys have private
raw ABI mirrors and tests comparing their layouts against a compiled C11 probe. Neither
language gains safe ExecuteTaskAck reception.

`AMS.MEL.IR.Scheduling.Metadata` provides `Execute_Task_Ack`, the limited-controlled
`Execute_Task_Ack_Stream`, `Open_Execute_Task_Ack`, `Receive_Execute_Task_Ack`,
`Execute_Task_Ack_Counters` and `Close`. `Not_Supported` is a distinct exception. Receive
copies both identifiers into Ada ownership and closes the native event before returning;
its exception path also closes the event. Finalization is non-raising. Timeout, stopped and
provider-error conventions match the existing metadata owners. No Ada task execution or
matching API exists.

## Registration and callback lifetime

The independent one-shot is claimed under the Scheduling lifecycle mutex on an Attached or
Enabled owner. Invalid local arguments and pre-registration allocation failure consume no
attempt. Each attempt reaching registration consumes it, including refusal, unknown Return
and provider exceptions. The exact upstream overload is called without either bridge lock;
synchronous callback delivery is safe. Known Return values are separately mapped; unknown
values leave the registration output unchanged and fail, as do provider exceptions with a
distinct status. No exception or allocation failure crosses the callback boundary.

The callback captures shared callback state, never the public wrapper or a provider payload.
Each event owns a complete copy before callback return. The independent bounded FIFO uses
DROP-INCOMING: valid input while already full allocates no snapshot; null input is malformed
even while full. Received, queue-full and malformed counters saturate. Allocation failure
rejects one whole event and subsequent valid delivery recovers.

Scheduling Close stops consumption and wakes blocked receivers. Already queued values drain
first, even after channel/Session closure, and before terminal cleanup failure. Public
metadata Close only deactivates consumption: upstream has no unregister. Closing the public
Session alone leaves a live Scheduling owner usable. Pending inherited common requests still
defer physical teardown with unchanged admission and non-cancellation semantics.

Physical cleanup destroys the provider channel while the Session/provider library graph is
retained, then waits for in-flight bridge callbacks before releasing that graph. Callback
state and lifecycle locks are separate and not nested or held across registration. Late
callbacks during channel destruction are memory-safe through shared callback state and never
enqueue. Failed direct detach retains the owner for retry; uncertain deferred detach retains
the complete graph permanently, preserving the foundation policy.


## Negative controls and mutation evidence

The mock counts ExecuteTaskAck registration attempts independently. Foundation C and Ada
tests require zero such attempts, in addition to the zero-call guard for the ten remaining
unimplemented methods (four exec, one send, five callbacks). Every ExecuteTaskAck C scenario
checks the ten-method negative control and exact registration-attempt count. The mock supports
synchronous delivery and manual asynchronous delivery, overwrites payloads on return, and
uses deterministic barriers/probes for blocked receivers and in-flight teardown, never sleeps
or background polling loops.

`scripts/test_scheduling_execute_task_ack_mutations.py` copies native sources into an isolated
temporary tree. Each mutant must compile and fail a named C11 contract assertion; a compile
failure never counts. Controls: swap identifiers; truncate command; truncate request; remove
the final queued event; miscount null as full; allocate a full-queue snapshot; permit a second
registration; omit in-flight teardown drain; implicitly register from the foundation. Each
mutant is restored; the final restored source is rebuilt and all focused scenarios rerun.

## Coverage and qualification

Exactly the ExecuteTaskAck callback operation and payload type move `missing → safe_ada`,
with both getter mappings represented. Unique safe Ada operations increase 47 → 48.
`SchedulingChannel` remains `partial`; the remaining ten operations remain `missing`.
The reviewed-binding digest and generated report follow the established audit workflow;
upstream Git evidence, catalog anchor, vendored-header identities/licenses and unrelated
classifications remain unchanged. Vendored files remain 824.

The pre-feature detached baseline passed: native 500, Python 407, Rust 305, exports 289,
safe Ada 47, vendored files 824. Baseline and feature qualification use sequential detached
runners with per-stage exit codes and a final success/failure marker. All positive execution
evidence is from the separate mock provider; no real provider, hardware, credentials or
Scheduling task execution was used.

## Recorded qualification commands and results

Implementation commit `4de1ba49c7293b677e90940dc2918d90b9614540`, tree
`7ad52fd076fa9b0705614b24f7e0b542267da29b`, was qualified using the sequential runner
`/tmp/irmel012-qual/qualify.sh` and corrected continuation
`/tmp/irmel012-qual/resume.sh`. The baseline runner was
`/tmp/irmel012-qual/baseline.sh`. Logs and individual exit codes are under
`/tmp/irmel012-qual/{baseline,final,resumed2}`; these are local execution artifacts, not
release dependencies. The final committed-head runner is
`/tmp/irmel012-qual/exact-head.sh`, with results under `/tmp/irmel012-qual/exact-head`.

| Stage | Actual command(s) | Result / exit code |
|---|---|---|
| Production | `sh native/scripts/build.sh` | 0; no compiler warnings |
| Test build | `sh native/scripts/configure-build.sh tests`; `cmake --build native/build-tests --parallel 8` | 0; no compiler warnings |
| ExecuteTaskAck C11 | `ctest --test-dir native/build-tests -R '^ir_scheduling_execute_task_ack_' --output-on-failure` | 0; 23/23 |
| Foundation | CTest `^ir_scheduling_foundation_` and `^ir_scheduling_layout$` | 0; 20/20 |
| Common/header/deferred regressions | CTest public Channel, common requests, C/C++ headers and Track deferred cleanup | 0; 150/150 |
| Full native | `ctest --test-dir native/build-tests --output-on-failure` | 0; 523/523 |
| Ada formatting | `make format-ada`; `make check-ada-format` | 0 |
| Ada full and focused | `alr -C ada/tests exec -- sh /home/zboll/git/ams-mel-irmel-ada-012/scripts/test_ada.sh`; `ada/tests/bin/ams_mel_ir_scheduling_ack_focused` with mock provider and test-facade `LD_LIBRARY_PATH` | 0; full smoke and focused PASS |
| Alire | `alr -C ada build`; `alr -C ada/tests run` | 0; full smoke PASS |
| Python | `make test-python` | 0; 408 tests, including 56 private ABI tests |
| Rust | `make test-rust`; `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | 0; 306 tests |
| Exports | `nm -D --defined-only`, sorted comparison with baseline, version-map inventory | 0; 289 → 295, exactly six additions, zero removals, all AMS_MEL_0.1; no production test exports |
| Policy | `make test-policy` | 0; intentional failing-CTest fixtures verify failure propagation, not feature failures |
| Coverage | `make check-ir-api-coverage` | 0; 52 audit tests; safe Ada 48 |
| Documentation | `python3 scripts/check_doc_source_integrity.py`; all `docs/tests/test_*links.py` with pinned enhancer Python | 0; 824 vendored files; all type-link tests PASS |
| Documentation regeneration | `GNATDOC_PREFIX=/home/zboll/git/ams-mel/.doc-tools/gnatdoc-26.0.0 alr -C ada/tests exec -- make -C /home/zboll/git/ams-mel-irmel-ada-012 test-documentation-source DOC_PYTHON=/home/zboll/git/ams-mel/.doc-tools/enhancer/bin/python` | 0; 6 tests |
| Mutations | `python3 scripts/test_scheduling_execute_task_ack_mutations.py` | 0; 9 compiled mutants detected, source restored, all focused baseline scenarios rerun |

The documentation stage initially stopped with exit 2 because the runner omitted
`GNATDOC_PREFIX`, then exit 1 because `alr exec` changed the working directory for `make`.
Both runner invocations were corrected without changing tests or repository contracts; the
full documentation stage then passed. The original failed markers remain in the local logs.
The initial native warning scan also used a log as both input and output for grep; a separate
read verified zero warning lines in both build logs and the runner was corrected to `grep -q`.

The formatter reports the existing externally-built GPR project warning that language C has
no sources. Compiled first-party sources remain warning-clean with warnings treated as errors.
GNATdoc emits informational pre-existing undocumented-entity warnings; these are not suppressed
or misrepresented as first-party compiler warnings. No unavailable provider/hardware test is
claimed passed. Exact-head generated documentation validation is non-deploying.
