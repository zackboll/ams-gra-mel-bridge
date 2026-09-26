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
Evidence is mock-provider-only. The test-only observer/DSO handles are dropped
before assertions about physical teardown; a permanent retention failpoint has
process exit as its only cleanup boundary.

Local GCC Debug and Release CTest each pass **82/82**. The **24** common-view
CTest scenarios pass **30 repetitions in each** configuration (720 executions
per configuration). `make test-native` and `make test-build-isolation` pass.
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
