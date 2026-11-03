# IRMEL-ADA-008 — Independent C2 SystemTrackDataResponse qualification

## Scope and upstream verification

Fetched base commit: `cbe2aaaf8df33663c1bea9a52060ed23a524e341`.
Base tree: `d0c69d43b1d2841232a999125b666b5778625324`.
One optional IR vertical slice; **every invocation uses the test mock provider,
never Squall, another real provider, or physical hardware**.

Inspected pinned `C2Channel.h` (`RequestFor<CommandStatus> send(SystemTrackDataResponse)`),
`SystemTrackDataResponse.h` (twelve fields: `systemTime` nanoseconds, `commandID`,
`requestId`, `trackId` uint32, four doubles, `AzElValid`, `rangeValid`, two `AzEl`
pairs) and `CommandStatus.h`/`CommonIR_MEL.h` (CommandState 0–4, CannotComply 0–46,
no MaxExclusive). Native static assertions bind the identifier/double/AzEl types and
the CommandState/CannotComply upper bounds. Vendor headers are unchanged.

## Complete API mapping

| Upstream | C ABI | Safe Ada |
|---|---|---|
| `SystemTrackDataResponse` (existing) | `ams_mel_ir_system_track_data_response_v1` (unchanged) | `Track.System_Data.System_Track_Data_Response` (reused; `C2.System_Data` subtype) |
| `C2Channel::send(SystemTrackDataResponse)` | `ams_mel_ir_c2_submit_system_track_data_response` | `C2.System_Data.Submit_System_Track_Data_Response` |
| asynchronous `ErrorOr<shared_ptr<CommandStatus>>` | `ams_mel_ir_c2_system_response_result_v1` (reuses `ams_mel_ir_command_status_v1`), `ams_mel_ir_c2_system_response_request_wait` | `C2.System_Data.Response_Request`, `Wait`, `Response_Result`, `Status`, `Command`, `Rejection_Code`, `Description`, owned `Command_Status` |
| observer release | `ams_mel_ir_c2_system_response_request_close` | `C2.System_Data.Close`, non-raising limited-controlled finalization |

Exports 269 → 272 (`AMS_MEL_0.1`); existing records, signatures, exports and enum
values are unchanged. `Command_State`, `Cannot_Comply`, `Response_Outcome` and
`Response_Error_Code` are subtypes of the existing Track types; no second
forty-seven-value mapping exists. The C2 request/result owners are distinct types,
never aliases of the private Track owners.

## Independence from the Track overload

Shared: only the state-free `ams_mel_system_track_data` native unit (`valid_flags`,
`build_response`, `copy_command_status`, extracted unchanged from `ir_track.cpp`)
and the private Ada `System_Track_Data_Conversion.To_Raw`. Not shared: channel
state, request ownership, admission token, accounting domain, completion probe
family or export. The C2 claim is `claim_c2_submission(Enabled)` plus
`CommonRequestClaim{finish_c2_request}`; no Track handle is reachable from it.

Evidence (`test_independent_c2_and_track`, Ada `Test_Mixed`): both overloads are
pending concurrently in one Session; the C2 request increments only
`ams_mel_test_c2_requests` and probe family 11, the Track request only
`ams_mel_test_track_requests` and family 5; the C2 send counter and the
`c2_system_track_data_response_sent`/`track_system_track_data_response_sent` events
each advance only for their own overload; releasing the Track gate completes only
the Track request; Track Close tears Track down while C2 stays pending; C2 Close
defers only C2 teardown until its own gate is released.

## Semantics

Inputs: all twelve fields bit-exact (doubles verified as IEEE bit patterns,
including signed zero, subnormals, ±infinity and NaN classification at the C ABI);
Boolean encodings other than 0/1 are `AMS_MEL_INVALID_ARGUMENT` before admission,
accounting or send. Nothing is clamped, normalized, transformed or zeroed for a
false flag; no epoch is inferred. As in the existing Track Ada contract, the
validity-checked Ada library refuses non-finite `Long_Float` input with
`Constraint_Error` before submission (checked for both overloads); the C ABI
carries non-finite values unchanged. NaN payload bits are not promised.

Results: a CommandStatus whose own state is Rejected is a successful future
(`AMS_MEL_OK`/`Success`); only ErrorOr is `COMMAND_REJECTED`/`Rejected`, with the
known error code and complete cached diagnostic (613-byte text recovered without
resending). Unknown CommandState/CannotComply, malformed reason text, null payloads,
unknown error codes and invalid rejection text are `PROVIDER_FAILED`; future
exceptions `PROVIDER_EXCEPTION`; deferred cleanup failure overrides with
`PROVIDER_FAILED`. **A successful future does not establish operational
incorporation of the track data.**

Lifecycle reuses Session CompletionPermit, Enabled-only lock-linearized claim,
unlocked one-shot send, exactly one worker, cached terminal Wait, allocation-free
emergency retention after post-send failure, and non-cancelling timeout/Close.
Probe family C2SystemResponse (11) and mock send family 12 are appended; older
families and the completion-probe synchronization are unchanged. Deterministic
provider promise gates, completion-owner probes and Session admission tokens, not
sleeps, establish ordering.

## Qualification commands and results

Run from this worktree; production (`native/build`) and contract-test
(`native/build-tests`) trees stay independent, plus isolated `/tmp` trees.

| Check | Command / evidence | Result |
|---|---|---|
| Production native | `make native`; isolated Release `cmake --build` | PASS, `-Wall -Wextra -Wpedantic -Werror` |
| Focused C2 native | `ctest -R ir_c2_system_response` | PASS, 4/4 (contract, two post-send, finish-failure) |
| Track SystemTrackDataResponse | `ctest -R ir_track_system_response` | PASS, test unchanged |
| Full native | `make test-native` | PASS, 466/466 (462 baseline + 4) |
| Mutations | `python3 scripts/test_c2_system_response_mutations.py` | PASS, 9/9 detected; baseline restored; checkout unchanged |
| Focused Ada | `ams_mel_ir_c2_system_data_focused <mock>`; isolated `allocation`, `worker-launch`, `finish-failure` with fresh `AMS_MEL_TEST_LIFETIME_LOG` | PASS, all four |
| Full Ada | `gprbuild -P ada/tests/ams_mel_tests.gpr`; `ada/tests/bin/ams_mel_smoke` | PASS |
| Python | full suite including ABI, test facade | PASS |
| Rust | `cargo test --workspace` including `c2_system_response_abi` | PASS |
| Exports | `nm -D --defined-only` versus `exports.map` | PASS, 272; all 269 baseline preserved |
| Policy / coverage | `make test-policy`; `make check-ir-api-coverage` | PASS; 39 coverage tests; safe_ada 44 → 45 |
| Formatting | `make check-ada-format`; `cargo fmt --check` | PASS |
| Documentation | `check_doc_source_integrity.py`; pinned enhancer `test_*links.py` | PASS |

Mutants: trackId lost; AzElError replaced by inertialAzEl; rangeValid forced true;
Rejected state rewritten; ErrorOr reported as success; C2 send not dispatched; C2
accounting never released; Enabled requirement weakened; C2 completion attributed
to the Track probe family. Each builds and fails the named release-active C11 check.

No Scheduling, image callback, RF, cancellation, retry, replay or metadata
registration is introduced.
