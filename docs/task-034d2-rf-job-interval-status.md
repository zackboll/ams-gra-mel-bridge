# Task 034D2 — owned RF JobIntervalStatus and safe Ada

Starting main: `4447a2aede89259f2e56adafba4887247e8810e7` (merged PR #66).
Branch: `feature/034d2-rf-job-interval-status`.

## Contract and ownership

The pinned RF MEL registration receives `const std::function<void(JobIntervalStatus)>&`
and returns **void**. Its payload is **by value**. Normal return proves only that
the registration call returned normally, not retention, delivery, or scheduling.
There is no unregister/quiescence contract; the callback getter is never used.

One heap-stable permanent holder per attempted provider registration retains the
exact callable, bridge callback/queue state, and SharedLibrary DSO pin for process
lifetime. It retains no JobState, JobDetail, VA, C2 child claim, public wrapper, or
delivered event. The stream is an observer, not a provider-resource owner.

Before exposure, Open validates options/lifecycle and prepares the public owner,
fixed-size queue ring, state, callable and library pin. Retention is allocation-free.
The attempt is consumed when provider exposure starts. Registration runs outside
JobState/queue locks, supporting synchronous callbacks. A throw closes/discards
state, publishes no stream, preserves the Job, retains the holder, and forbids retry.
Finalize/full Cancel attempted and a second Open are rejected.

Logical stop clears queued payloads and wakes receivers without provider calls or
quiescence waits. Job Close stops status reception before its pending-finalize
early return; it does not implicitly cancel. Already-entered by-value callbacks
can finish without the Job graph: check stop before copying, recheck before
publication, discard if Close wins. Late callbacks capture only bridge state.
The DSO remains pinned through callback return and by-value argument destruction.
Only exceptions inside the bridge callback body are contained; provider-side
argument construction before entry is outside that boundary. Calls sharing a raw
owner remain externally serialized with destruction. Distinct stream Receive
may race Job Close.

## Payload and queue

Every interval ID, all 25 completion statuses, all 8 log triggers, every log map
entry and arbitrary activity bytes are preserved. Known failures are event data,
not Provider_Error. Unknown enums reject the entire notification. Log iteration
is ascending event-ID key order, not chronological order. The pinned log-time
getter is non-const: copy each log value to a local mutable value, never const_cast
or modify vendor code. UTC time remains separate exact signed int64 seconds and
fractional femtoseconds, without recombination, floating point, Duration or
normalization. Activity ID is arbitrary binary length, including empty, zero and
high-bit bytes; it is not UCI_ID, UUID or UTF-8.

Options v1 explicitly bound queue capacity (>0), log entries and activity bytes.
Zero payload limits accept only empty respective payloads. Container max_size and
size arithmetic are checked before registration. FIFO uses DROP-INCOMING, never
replacement. Eight saturating uint64 counters cover callback entries, queued,
delivered, queue-full, malformed, oversize, allocation failure and after-close
discards. A stopped callback takes precedence over decode outcome; one rejection
reason is counted per callback. Allocation failures publish no partial event;
later valid notifications recover. Queue limits do not bound provider argument
construction, all concurrent callback temporaries or application-held events.

Receive zero is a poll, finite waits are bounded, TIMEOUT transfers nothing, and
STREAM_STOPPED transfers nothing. Invalid output arguments do not consume data.
View allocates/calls no provider; Close is null-idempotent.

## Interval reporting and language surfaces

The released interval config v1 is frozen and remains upstream Never. The v2
record embeds v1 plus status_enable; one shared private builder validates and
forwards Never=0, Always=1 and OnException=2 exactly. Enabled modes require a
normally returned registration with locally unstopped state when checked; this
guard is not delivery evidence. Incoming data is not filtered by reporting mode.
The zero continuation sentinel remains unchanged: unspecialized
numeric_limits<Femtoseconds>::max().count() is 0, while Femtoseconds::max().count()
is INT64_MAX. Starts 0, 1, -1 and INT64_MAX are forwarded unchanged.

Safe Ada keeps configuration in AMS.MEL.RF.C2 with an explicit reporting enum and
setter, preserving constructor profiles/default Never. AMS.MEL.RF.C2.Interval_Status
provides a limited controlled Stream and ordinary private Status_Event/log values.
Receive validates native enum/pointer/count representations, copies all logs and
binary bytes into Ada vectors, and closes the exception-safe native event owner
before return. No public addresses, spans or handles; returned values survive all
owner closures. Timeout uses C2.Timeout_Error, stop uses Stream_Stopped, other
bridge failures use Provider_Error. Raw Rust sys and private Python ctypes only;
no safe Rust/public Python status API.

Seven additive ABI 0.1 exports: six stream/event operations and Add v2. Existing
146 operations/layouts are preserved. Test-only controls are excluded from the
production export map. All 804 vendor blobs remain unchanged.

## Evidence (updated as validation completes)

Baseline native: 255/255. Focused native and isolated safe Ada status suites have
each completed their internal 50/50 loops. Mock supports copied and exact-reference
callables, synchronous registration, stored-then-throw, malformed/oversize/empty
payloads, deterministic mapping holds and late invocation without Job dereference.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` was rechecked:
registration/Add/Flush/Cancel_Remaining are no-ops and the getter returns a dummy.
Integration clients open before Finalize, submit enabled intervals, poll TIMEOUT,
retain the existing ProductRx/finalize/cancel/completion assertions, then observe
STREAM_STOPPED after Job Close. This is call-path/lifecycle/no-delivery evidence,
not positive real notification delivery or actual scheduling.

extendJobEvent, conditional commands, TX events, pointing, weights, RDMA, VADB and
additional ProductRx formats remain deferred; no full RF MEL coverage is claimed.