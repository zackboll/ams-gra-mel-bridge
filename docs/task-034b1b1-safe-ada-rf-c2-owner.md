# Task 034B1B1 — safe Ada RF C2 lifecycle owner

Starting SHA: `ed94304679843b813bba7576df805a8be9f95f92`.
Reuse the committed GCC 710 / Clang 711 / union 712 declaration closure;
there is no additional upstream dependency or vendor change (802 files).
Pinned RF MEL `762ce84c5555dd0f3ea66f36b321fecf8839b89f` defines
`rfmel::fnC2MEL`, the type checked against the resolved `createC2MEL` symbol.
This is neither the Admin nor the DataMEL factory.

An opaque `ams_mel_rf_c2` owns one shared `RfC2State` containing the C2 provider
object and the `SharedLibrary`. Configuration is passed as the published
`std::string_view`. Open preallocates state and public owner before calling the
factory, rejects null results, and contains exceptions. Explicit Close consumes
the public handle, calls `shutdown` exactly once, destroys C2 before dropping
the DSO reference and is idempotent for a null owner. A throwing shutdown is
never retried: the uncertain state (object plus DSO) is retained by an
allocation-free intrusive self-reference for process lifetime. Callers
externally serialize operations on one owner. There are no children yet; the
next task adds child admission, parent-first lifetime and deferred shutdown to
this state before exposing any asynchronous resource.

ABI 0.1 adds exactly `ams_mel_rf_c2_open` and `ams_mel_rf_c2_close` (118 to
120 production exports); existing signatures do not change. Private raw Ada,
Rust sys, and Python ctypes bindings and both layout/inventory probes track
the same ABI. There is no safe Rust or public Python RF C2 surface.

Safe Ada `AMS.MEL.RF.C2` provides a limited controlled `C2_MEL`, `Open`,
`Is_Open`, and explicit `Close`; raw handles remain private. Both inputs reject
embedded NUL before C-string allocation; temporary strings have controlled
cleanup. Explicit Close propagates provider failure after consuming the handle;
finalization never raises. No version query is added.

The existing RF mock implements `c2:ok`, `c2:factory-null`,
`c2:factory-throw`, `c2:factory-throw-unknown`, and `c2:shutdown-throw`.
Every unused virtual method traps as a forbidden call: none is an API stub
reporting success. C11 tests check factory count, exact scenario selection,
shutdown → C2 destruction → DSO unload, null-owner Close, and permanent
retention/no retry on shutdown exception. Ada tests exercise explicit Close,
automatic finalization, the three factory failures, shutdown exception,
double Close and embedded-NUL rejection. The opt-in
`make test-squall-rf-ada-c2` validates the pinned Squall factory and shutdown
through production `libams_mel_c`, without loading the test-only Job helper.

**Deferred to 034B1B2:** `requestVirtualAperture`, RequestFor/future handling,
child claims, and VirtualAperture owners. ElementGroupCommand, JobRequest,
JobDetail, finalize and cancel remain deferred to 034B2. This is not the
completion of Task 034B1.

Validation for this local checkpoint: `make test-native` passed 248/248
(starting baseline 247/247), `make test-build-isolation` passed, and C11 C2
lifecycle and the focused safe Ada RF smoke each passed 50/50 repetitions.
`make format-ada`, `make check-ada-format`, `alr -C ada build`, and
`alr -C ada/tests run` passed. Bare host `gprbuild` is not on PATH: direct
GPRbuild outside Alire was not reproduced locally. `make test-rust`,
`make test-python` (108 tests), Cargo check/test/clippy/fmt and the Release
ABI/layout audits passed. Pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` passed both the standalone
safe Ada C2 lifecycle and the preexisting Admin state-control regression. The
Admin rerun used distinct host ports because the first two default-port
attempts reported an occupied health port. C2's client links the production
facade, not Squall, the mock, or the job helper. The checksum inventory checks
all 802/802 pinned files; no original blob or inventory entry changed.