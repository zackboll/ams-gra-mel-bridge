# Task 034B1B2 — asynchronous RF VirtualAperture

Starting SHA: `e6b41dc810062503f751d05a8b6114ba66de37df`.
The GCC 710 / Clang 711 / union 712 declaration closure and all 802 vendor
files are reused unchanged; RF MEL is pinned at
`762ce84c5555dd0f3ea66f36b321fecf8839b89f`.

The six ABI 0.1 additions are `ams_mel_rf_c2_submit_virtual_aperture`,
`ams_mel_rf_virtual_aperture_request_wait`,
`ams_mel_rf_virtual_aperture_request_claim`,
`ams_mel_rf_virtual_aperture_request_close`,
`ams_mel_rf_virtual_aperture_view` and `ams_mel_rf_virtual_aperture_close`.
The starting 120 exports remain intact; the current total is 126.

Submission validates then copies all five published arguments *before* calling
the provider: exact 32-bit VA definition ID and priority, UTF-8 ordered local
function strings, UTF-8 VA file info, and ordered Common MEL UCI IDs (all 16
UUID bytes and descriptive labels). Empty strings are preserved. Input spans
are borrowed only for submission. Safe Ada uses the existing canonical
`AMS.MEL.IR.UCI_ID` for Common MEL IDs; moving that type to a neutral package
would be a separate compatibility task.

The C2 state counts child claims under a mutex; one move-only claim follows a
submitted request from pending future to cached unclaimed VA to claimed owner.
It is never released and reacquired or double counted. Submission preallocates
the completion, worker input, public request owner and thread holder, then
acquires the claim and calls `requestVirtualAperture`. Exactly one worker calls
`future.get()` (in `settle`). A `launch_state` handshake prevents the worker
from consuming a future until the submitter has either published the request
or decided to retain the complete graph. A post-provider allocation,
publication or worker-launch failure permanently retains the valid future and
claim without claiming cancellation or unloading the DSO.

This borrows the preallocation, worker handshake, exception containment and
intrusive emergency retention principles of common-channel requests, **not**
its `CommonChannelAccess`, weak IR channel, IR completion admission, Session
permit or request limits. ProductRx provides the one-claim transfer, cached
outcome, nonblocking abandonment, and deferred parent-shutdown pattern. A
timed Wait is only a poll and never cancels. Repeated terminal Wait returns
the same status, MEL error code and complete UTF-8 description (with a required
buffer length). An abandoned successful VA is destroyed before the claim is
released; detached cleanup suppresses a deferred shutdown error and retains
the uncertain provider graph. A synchronous submission throw or invalid
future publishes no request and releases its claim.

Successful Claim is unique. The provider VA getters run once at Claim and
snapshot a sorted set of instance IDs, ordered UTF-8 element-group labels and
single-group status. Getter failure is a cached one-shot claim failure: no
partly built VA is published and the provider VA is destroyed before releasing
the claim. The C view points solely into the immutable native VA owner until
Close. Safe Ada owns its own copied ID/label vectors and exposes no raw address.
It provides `Virtual_Aperture_Config`, `Virtual_Aperture_Request`,
`Virtual_Aperture_Result` and `Virtual_Aperture` alongside the existing C2
owner. Explicit final-child VA Close reports a deferred throwing shutdown;
non-raising finalizers suppress it. C2 Close before the VA's Close defers
shutdown; the VA and DSO remain live until the VA is destroyed.

The mock checks nontrivial input fidelity, success, known and long UTF-8
ErrorOr failure, future exceptions, invalid future, null success, getter
exception, delayed release, pending abandonment, two simultaneous claims,
throwing deferred shutdown and failpoints. C11 and safe Ada verify parent-first
snapshots. The opt-in Squall integration commands Standby and Operate_Rx_Only,
requests VA definition 0 with priority 1 and empty optional inputs, then
validates instance 0, label `0`, single-group status, and snapshot access after
C2 Close. It links only through production `libams_mel_c`, without linking
Squall or loading the job helper.

Task 034B2 still owns ElementGroupCommand, JobRequest, requestJob, JobDetail,
finalize, cancelJob, ProductRx job activation and removal of the test job helper.

Local validation: native 248/248 at entry, 249/249 with the new C11 VA case;
native C2/VA and focused safe Ada RF each passed 50/50. Production/test-tree
isolation passed. Alire Ada build and smoke, Ada formatting, Rust workspace
check/test/clippy/fmt, and Python 109 tests passed. Bare host `gprbuild` is
unavailable outside Alire; hosted direct-GPR remains required. Pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` passed VA, Admin and C2
checks; the C2 default-port attempt hit an occupied health port, then passed
using distinct ports. The existing safe Ada ProductRx ComplexINT16 real Squall
integration also passed after the earlier, interrupted gRPC build's result
became unavailable; its fresh run used the same pinned Squall revision and
retained a successful result. Release exports audited at 126 with all original 120
unchanged. Vendor checksums verified 802/802, with zero vendor changes.
