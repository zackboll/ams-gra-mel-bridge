# Task 034B2B2: safe RF Job lifecycle and helper-free ProductRx

Starting checkpoint: `65aaea3ba133e3de9311db5779e4bf17b0162ed4`.

`AMS.MEL.RF.C2.Job_Status` represents None=0, In_Progress=1, Complete=2,
Failed_Invalid_ID=3, Failed_Interrupted=4, and Failed_Invalid_State=5.
Failed statuses are published results, not provider exceptions. The private
immutable `Cancel_Result` exposes `Cancelled : Boolean` and `Error_Code :
Cancel_Error` (None=0) independently; False/None is valid.

`Finalize_Job` invokes the B2B1 native one-shot operation: repeated successful
calls return normally; repeated synchronous failures raise Provider_Error
without retry. `Wait_Job_Status` raises Timeout_Error on pending work without
changing it, maps all six statuses, and returns cached terminal results on
repeat. `Cancel_Job` reports the provider's bool/error independently, caching
success or failure on repeats. Cancel before Finalize prohibits Finalize.
Explicit Close and controlled Ada finalization only close the public Job;
neither implicitly finalizes nor cancels it. A pending finalize future retains
its provider graph after automatic Close, as documented in B2B1.

Against pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f`,
Finalize activates receive, an immediate status poll times out, ProductRx
events arrive, Cancel succeeds, and finite Wait returns Complete. Both the C
ProductRx and safe Ada ProductRx integrations use production Job interfaces;
the former test-only C++ helper and its build/load path are removed. The Job
retains its provider VA and C2 child claim when public VA/C2 are closed before
events, cancellation, status and snapshot access. This is pinned-provider
evidence, not a universal provider timing requirement.

ABI 0.1 remains at 135 exports; vendored files remain 803/803. JobInterval,
addJobIntervals, flush, cancelRemainingJobIntervals, interval callbacks,
extendJobEvent, multi-group and TX requests are deferred.