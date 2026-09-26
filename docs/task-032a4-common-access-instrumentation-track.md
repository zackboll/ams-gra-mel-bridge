# Task 032A4 — internal Instrumentation/Track common access

Starting `origin/main`: `e820a288f85bddc1736ca61f4e5cdb60e3ec7dd6`.
Branch: `refactor/032a4-common-channel-access-instrumentation-track`.
This depends on 032A1's single Return/Comms completion engines, 032A2's weak
common view and 032A3's Health adapter. The view remains test-only; there is
no public capability query or language-surface change.

Both new idle adapters contain only a weak family state and static admission
and claim functions. A claim accepts Attached or Enabled, copies the provider
base Channel and increments the family's *existing* `requests` under the family
mutex. The shared common submission acquires the per-Session permit before the
claim and sends outside the mutex. InstrumentationLevelCmd and both typed Track
sends keep their Enabled-only rules. Claim finish reuses Instrumentation's
`finish_channel` or Track's `finish_request`; unexpected exceptions retain the
complete graph and report the family-specific deferred-cleanup diagnostic.
The family Close paths still stop metadata logically and wait for callback
quiescence during deferred physical cleanup; those paths were not replaced.
Post-send failures use the shared engine's permanent retention and permit.

The C11 suite proves idle weak access (including Attached/Enabled submissions)
and common-close-first without keeping a typed channel alive. A separate
test-build-only observer stores **only** `weak_ptr<ChannelState>` or
`weak_ptr<TrackState>`; it temporarily locks and reads the existing request
count under the family mutex, then releases the lock and strong reference.
With no requests, explicit family and Session Close destroy the graph while
the observer still exists; its next read reports expired. No observer or
conversion is present in the production library.

Enabled Instrumentation concurrently holds common KeepAlive and typed
InstrumentationLevelCmd: the existing family count reads 2. After both public
request owners, the family owner and Session are closed, the weak observer
still reads 2 without pinning the graph. Releasing exactly one future and
waiting for **FinalOwner** reads 1 without any physical cleanup; releasing
the other gives exactly one disable, detach, channel/Control/manager destruction
and an expired observer. The open-parent version reads 2 -> 1 -> 0 without
cleanup until public Close. Enabled Track holds common KeepAlive,
TrackDataUpdate and SystemTrackDataResponse in the same existing count:
closed-parent observer readings are 3 -> 2 -> 1 -> expired, with teardown
only on the final completion; open-parent reads 3 -> 2 -> 1 -> 0.
FinalOwner, not terminal completion, is the admission-permit release boundary.

Combined metadata tests register the InstrumentationReport or IRSTTrackReport
callback, hold common KeepAlive in `future.get()`, and deterministically park
the callback **inside** the bridge. Close makes metadata Inactive immediately;
empty receive reports STREAM_STOPPED and the weak observer still reads one
request, one Session permit, and no physical cleanup. Releasing the callback
returns safely, drains the in-flight callback counter to zero and publishes
no late event; the retained request still prevents teardown. Releasing the
future triggers exactly-once teardown after the logged callback return.

Provider send-entry barriers prove parent-first KeepAlive Close-wins throws
for both families and Comms Close-wins throw for Instrumentation: a claimed
request protects the graph until the synchronous exception finishes the
claim, returns PROVIDER_EXCEPTION with null output, and performs one deferred
cleanup with no worker or `future.get()`. Four limit-one admission tests cover
common Instrumentation -> typed Track, typed Track -> common Instrumentation,
common Track -> typed Instrumentation and typed Instrumentation -> common
Track. All reject before provider send or family claim; after the first
FinalOwner releases the permit, the refused submission succeeds on retry.

Each family exercises common Return and Comms allocation and worker-launch
failpoints **after** provider send. Each returns INTERNAL_ERROR with null
output, one family request and Session permit, a retained provider future,
no completion worker/get and no second provider send when the limit is full.
There is no rescue. Existing finish-exception and orphan deferred-detach
failure regressions permanently retain the uncertain graph with
PROVIDER_FAILED; existing typed, no-pending explicit detach failures keep
their public retry owner and permit a later successful Close.

The final focused high-risk selection (31 cases including prior failure
regressions) passed parallel repeat-100; a fresh GCC Debug and separate
GCC Release tree each passed 151/151 ordinary tests and full parallel
repeat-50. Native build-tree isolation passed, direct Alire build/tests
passed, Rust workspace check/test/clippy/fmt and `make test-rust` passed,
and Python 54 tests plus compileall passed. `make check` fails locally at
its direct GPR gate because GNAT/GPRbuild is unavailable on PATH; the
separate Alire validation succeeds. The test-owned DSO pin stays alive
until every `dlsym` function-pointer call completes.

The public C header, production export map and Ada/Rust/Python declarations
remain unchanged. A fresh non-test Release build has ABI 0.1 and exactly
91 versioned production exports; after removing the ELF version suffix,
their complete name set equals the starting-main inventory. No test hooks
appear in that DSO; vendor sources and provenance are unchanged.
