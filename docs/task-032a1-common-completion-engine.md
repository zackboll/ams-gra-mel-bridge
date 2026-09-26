# Task 032A1 — internal common Return/Comms completion

Starting main: `c9fb2a59320c07e9c21e5dfb42da5e42e0bd71e2`.

Task 032 stopped before publishing an inherited-Channel view: Return and
Comms completion were coupled to C2 `ChannelState` and called `finish_channel`
directly. The larger 032A proposal stopped without changes because family
cleanup and ownership handoffs were not yet established. This smaller task
extracts the two existing engines without introducing another family.

`native/src/common_channel_requests.cpp` now owns both completion workers,
their permanent post-send emergency roots, cached results, and the existing
public opaque request Wait/Close implementations. Its private header defines
their WorkerInputs and Completions. Both completions own a move-only
`CommonRequestClaim`, not a C2 state. The claim contains a strong erased family
owner, a static noexcept finish function, and a static cleanup-failure message.
Construction and finish perform no type-erasure allocation. `finish` exchanges
away the function and owner before invoking the adapter, so a subsequent call
or destructor cannot decrement twice. Move assignment finishes its prior claim.
The C2-only erased adapter in `ir_c2.cpp` casts back to `ChannelState` and calls
the unchanged `finish_channel` path; a failed deferred cleanup still becomes
ProviderFailure with `deferred C2 cleanup failed` on completed requests.
Corrective: `finish()` has already moved the strong owner out of the claim when
the adapter runs. An unexpected exception from `finish_channel` previously
returned false without retaining the graph. The C2 adapter now retains the
state allocation-free in its catch before returning false. Normal cleanup
failure continues to retain through `cleanup(state, true)`. Test-build-only
`AMS_MEL_TEST_C2_FINISH_FAILURE=exception` throws inside the adapter before
cleanup, and isolated C11 Return and Comms tests verify one future get, failed
Wait with the existing diagnostic, and no channel/control/manager destruction
or library unload even after closing both public owners and the request.

C2 reserves its existing request count under the lifecycle mutex after acquiring
the existing Session admission slot. The provider send remains unlocked.
Synchronous throw drops the temporary channel, explicitly finishes the claim,
and reports the same ProviderException even if deferred cleanup fails. Healthy
future completion consumes the provider result, finishes the claim, and proves
it empty at the Task 031A graph-release boundary. Return/Comms retain their
original probe-family IDs, worker/input ordering and FinalOwner semantics.
Post-send allocation or launch failure permanently retains the entire input,
including future, claim, permit and provider graph; it does not finish the claim.
Mode retains its separate C2-specific engine unchanged.

This changes no installed C header, export map, ABI version, binding, or
production symbol. ABI remains 0.1 with 91 production exports. Existing C11
C2 submission, stored exception, parent-first, admission, permanent retention,
and cross-family completion probes exercise the extracted engines. The new
finish exception tests add no production export or public binding surface.

032A2 will separately add weak `CommonChannelAccess`, family adapters, Health
request accounting and deferred cleanup, and test-only five-family inherited
KeepAlive/Comms coverage. None of these are implemented here.
