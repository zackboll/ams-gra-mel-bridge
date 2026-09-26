# Task 032A2 — weak common Channel access for C2 and Image

Starting `origin/main`: `e28c0152a217ce39bcde81c35da57483a53a7116`
(merged Task 032A1 / PR #47). Branch:
`refactor/032a2-common-channel-access-image`. The old parked
`feature/032-common-channel-services` is untouched.

## Scope and representation

032A1's single Return and Comms completion engines remain the only engines for
these inherited requests. `CommonChannelAccess` contains only `weak_ptr<void>`
and two static function pointers: one reads Session admission from a temporarily
locked state, and one claims a typed family request. It has no Session, Channel,
Control, library, `std::function`, or public discriminator. Its test-only opaque
view may outlive either typed owner without preventing provider unload; closing
the view merely destroys the weak wrapper. No public Channel view exists yet.

The C2 translation unit validates Attached/Enabled under `ChannelState::mutex`,
copies the provider Channel, increments the existing `requests`, and constructs
the 032A1 claim with the same fail-safe C2 finish adapter. The Image translation
unit does the same under `CallbackState::mutex` for Attached/Running, using
`ImageStreamState::requests` (the Navigation count); Starting, Stopping, Stopped,
Failed, and absent channels cannot claim. Neither provider send runs under the
family mutex. No second Image count exists.

Submission validates its arguments and preallocates Completion, WorkerInput,
public owner and thread storage. It then locks weak state, obtains Session
admission, acquires a `CompletionPermit` (full means ResourceExhausted before
family accounting or send), claims the family, unlocks, and sends. Both operations
reuse their existing opaque Return/Comms owners, completion worker, cached Wait,
and provider error mapping. Comms copies all three IDs into the upstream request.
Synchronous send exceptions drop the temporary channel, finish exactly one claim
and return ProviderException with no request or worker. A generic TEST-only
post-send allocation/launch failpoint permanently retains the armed input,
future, claim, permit, and provider graph without rescue or decrement.

`finish_image_request` is shared between Navigation and the common claim:
decrement under the callback lock, then call the external-owner deferred cleanup
helper unlocked. The existing Navigation post-decrement barrier remains at the
same point via a callback; a common Image barrier targets that same interleaving.
Unexpected finish exceptions permanently root Image state before reporting
`deferred Image cleanup failed`. Public Stop/Close, provider Buffer release and
snapshot ownership continue through the existing cleanup helper.

The only new C-facing calls and header live in the test build (`native/tests/`)
and its generated export map. Public `abi.h`, production `exports.map`, Ada,
Rust, Python, vendor, and provenance are unchanged. ABI remains **0.1** and
the required production inventory remains **91** symbols. Health's deferred
request lifecycle and common adapters for Health, Instrumentation, and Track
remain future work. This task does not implement common capability queries.

## Evidence and limits

The separate C11 test covers attached C2 and Image KeepAlive and Comms (exact
IDs/cached Wait), running Image, weak idle teardown and common-close-first for
both, parent-first pending Return for both, Image finish-exception retention,
post-send allocation and worker-launch retention, unlocked Image race handoff,
synchronous send throws, and shared Session limit-one admission against typed
C2 Mode and Image Navigation. Existing C2 and Image contracts stay enabled.
Evidence is mock-provider-only. Parent-first tests retain their test-owned DSO
pin until the mock gate has released the held future, then prove channel,
Control, and manager destruction while the weak common view is still alive.
After all mock function-pointer calls they drop the pin and prove library
unload. A permanent retention failpoint has process exit as its only cleanup
boundary.

Initial local GCC Debug and Release CTest each passed **82/82**. The **24**
common-view CTest scenarios initially passed **30 repetitions in each**
configuration (720 executions per configuration); see the corrective validation
below for the final stress results. `make test-native` and
`make test-build-isolation` passed initially.
`make check-ada-format`, `alr -C ada build`, and `alr -C ada/tests run` pass
independently; Rust workspace check/test, all-target Clippy `-D warnings`,
fmt check and `make test-rust` pass; `make test-python` passes 54 tests and
compileall. Local Clang validation is unavailable. The local full gate is
recorded separately, without weakening it:

```text
make check: FAIL
reason: GNAT/GPRbuild unavailable on PATH
```

Hosted exact-head Clang and direct-GPR jobs remain required. The production
library reports ABI **0.1** and exactly **91** `ams_mel_` exports; the public
header, production export map, all language bindings, vendor and provenance
files remain unchanged. Weak-idle C2 and Image tests observe channel, Control,
manager and library destruction while the common view stays alive. Parent-first
Return completion releases the Session permit and physically tears down the
provider graph while that view still exists. The Image post-decrement Close
barrier proves exactly one cleanup adoption/detach. Return and Comms send
throws leave no request or worker and restore accounting; post-send failures
retain claim and permit permanently. A forced Image finish exception reports
ProviderFailed and retains the graph rather than unloading it.

## PR #48 parallel-stress corrective

At head `51bedb591bcc2b186f108a778045bcb99f9d7163`, hosted push and PR
CI passed the first ordinary CTest run but all four native GCC/Clang Debug and
Release jobs failed the parallel `--repeat until-fail:50` phase. Ada, Rust,
Python, and build isolation passed. Investigation found two **test ordering**
defects, not missing production graph ownership: `comms-high` validates IDs but
returns an already-ready future, so its worker can finish before the test reads
the request count; and the parent-first/finish tests used a mock DSO function
pointer after dropping their explicit `dlopen` reference. Both C2/Image
`CommonRequestClaim` owners hold their family state, whose Session owns the
provider graph until claim finish. No production source or public contract was
changed by this corrective.

The mock now has a separate `comms-high-held` scenario. It validates the three
high-bit IDs exactly and uses the existing family-2 held future; the old
`comms-high` path is unchanged. The common Comms test observes one Image/C2
request and one admission permit with the worker blocked in `future.get`, then
explicitly releases the held future and verifies exactly one get, both reply
IDs, the cached Wait, zero requests and the FinalOwner permit release. Parent
tests keep their DSO pin through all gate calls, prove no channel/Control/manager
destruction before release, prove exactly one of each after worker reclamation,
verify both inherited calls fail from the expired weak view without sends, then
drop the test pin and prove library unload. The forced Image finish-exception
test also releases via the still-pinned gate before dropping its pin and proves
permanent graph retention afterward. No provider function pointer is called
after `dlclose`. Image FrameLease/buffer release, post-drain recheck and
lost-cleanup paths are unchanged.

Final corrective local validation: a fresh GCC Debug ordinary CTest passed
**82/82**, followed by the exact hosted `--parallel 4 --repeat until-fail:50`
suite **82/82** (102.28 seconds). The five targeted Comms/parent/finish cases
also passed 100 parallel repetitions each. A fresh GCC Release ordinary CTest
passed **82/82** and the same full parallel repeat-50 suite passed **82/82**
(88.30 seconds). Sequential `make test-native`, `make test-build-isolation`,
Ada format and direct Alire build/tests, Rust workspace check/test/Clippy/fmt
and `make test-rust`, and `make test-python` (54 tests plus compileall) pass.
The corrective changes only this document, the test mock, and the C11 common
view test. Production ABI 0.1 and its original 91-export set remain unchanged.
