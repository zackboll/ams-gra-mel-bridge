# Task 034B2B1 — Native RF Job lifecycle

Starting origin/main: `07e67fe19f435d9484aeb922c46277d3c137cc5e` (merged PR #61).
The baseline has 250 native tests, 132 production exports, 110 Python tests,
and 803/803 vendored files. ABI remains 0.1; the Job snapshot v1 is unchanged.

Pinned JobStatus: None=0, InProgress=1, Complete=2, FailedInvalidID=3,
FailedInterrupted=4, FailedInvalidState=5. All six are successful **result
data**, including None/InProgress/Failed*. An unknown enum is PROVIDER_FAILED.
Pinned CancelError has only None=0. CancelStatus::operator bool() is the
semantic cancellation success, independently of getError(); false/None is
preserved as cancelled=0/error_code=NONE, not fabricated as a different error.

Finalize is one-shot: the first provider invocation, including synchronous
exceptions or an invalid future, is cached. Repeated successful Finalize is
idempotent OK; repeated failures have the same status and diagnostic. After
Cancel has been attempted first, Finalize fails without calling the provider,
even if Cancel reported semantic failure or threw. Cancel likewise calls the
provider only once, caches bridge status, bool and error separately, and does
not destroy the Job. A provider callback can fulfill the finalize future during
Cancel: no bridge mutex is held across cancelJob().

One detached worker is the sole consumer of the returned std::future<JobStatus>;
there is exactly one Job-finalize future.get() in `native/src/rf_job.cpp`.
Valid futures are retained via the preallocated FinalizeInput and intrusive
emergency root if worker launch fails. The launch-state handshake prevents a
worker from consuming a future after failed publication. Wait(0) polls; finite
Wait bounds blocking, leaves out_status untouched on timeout/failure, and caches
terminal result/diagnostic. Exceptions from future.get() become
PROVIDER_EXCEPTION; bridge allocation failures become INTERNAL_ERROR.

JobState keeps JobDetail, provider VA, immutable snapshot and C2 child claim
alive independently of the public wrapper. With no pending worker, explicit
Close destroys JobDetail then releases VA then the child claim and reports any
deferred C2 shutdown exception. With a pending worker, Close consumes only the
wrapper, neither waits nor implicitly cancels. The worker retains the graph
until future completion; if the provider never resolves without cancellation,
this graph may remain for process lifetime. Once future ownership is released,
worker-only cleanup follows JobDetail -> VA -> C2 claim -> deferred C2 shutdown
-> C2 destruction -> DSO release. A throwing shutdown uses existing permanent
C2 retention and is never retried; on a worker-only path it cannot be reported.

Only the native C foundation and private raw Ada/Rust/Python ABI parity are in
scope. Safe Ada Finalize/Cancel, safe-Job ProductRx activation, and removal of
`squall_rf_job_helper.cpp` are deferred to 034B2B2. No JobInterval, flush,
callback, real-Squall finalize/cancel, algorithm or service work is included.
