# Task 034B2A2 — safe Ada RF receive Job

Starting SHA `1840a81aca51062b595e4f080c58c4608686113b`.
This task uses the existing 034B2A1 ABI 0.1: 132 exports, no new C
operations, and the unchanged pinned 803-file vendor inventory.

`AMS.MEL.RF.C2` now provides Ada-owned `RX_Element_Group_Config` and
`Job_Config` values. Their strings and vectors are copied into temporary
C-layout storage during `Submit_Job`; views and spans never outlive that call.
Strings reject embedded NUL; duty factor is finite and in (0,1], frequency
endpoints are finite and ordered, and duplicate endpoint IDs are rejected.
Instance selection retains order. The C boundary independently validates
UTF-8, spans and numeric values. Empty Ada vectors map to null/zero C spans.
Only request ID, priority, precedence, interruptable, ordered instance IDs
and exactly one provider-created RX group are represented. All other upstream
JobRequest fields retain the pinned defaults documented in
`task-034b2a1-native-rf-job-request.md`: capability/activity IDs `{0}`,
default-constructed start/complete times and estimated pointing, zero duration
and lookahead, Tx power modes `{0}`, null callbacks/context and zero JIB count.

Limited controlled `Job_Request` owns one native request. `Wait(0)` polls,
finite waits are bounded and neither cancels; provider rejection returns
`Failed` with the shared MEL error enum and the complete cached UTF-8
diagnostic (retrying the terminal result into an exact-sized buffer when
necessary). Timeouts raise `Timeout_Error`; provider/bridge failures raise
`Provider_Error`. `Close` is nonblocking abandonment. One successful `Claim`
produces a limited controlled `Job`; second Claim fails. Both owners support
idempotent explicit Close and nonraising finalization. Explicit Job Close
propagates deferred C2 shutdown failure; it never invokes finalize or cancel.

Claim copies integral start seconds, normalized fractional femtoseconds,
duration, lookahead, four distinct identifiers and ordered RX stream IDs into
Ada-owned storage. Getters do not borrow native view memory. The native Job
retains provider VA and one sibling C2 claim even after public VA/C2 Close;
the Ada snapshot remains readable through that parent-first sequence.

The opt-in `make test-squall-rf-ada-job` uses pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f`. It creates and claims a
first Job, closes it, then creates and claims a second Job on the same VA with
a distinct request ID; closing the first unfinalized Job did not prevent the
second request on this provider. The second Job snapshot remains available
after public VA and C2 Close. This is evidence about pinned Squall's behavior,
not a bridge cancellation guarantee. The new integration uses the production
C facade and safe Ada only, not the test-only C++ job helper. The existing
ProductRx integration may still use that helper. Finalize, cancelJob,
JobInterval, helper removal and safe-Ada-driven ProductRx activation are
outside this task.
