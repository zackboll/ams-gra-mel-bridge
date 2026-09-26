# Task 032A3 — internal Health common access

Starting `origin/main`: `34fb0a1029db1815d8b4390a820a4e0c987050f4`.
This work depends on merged 032A1 (shared Return/Comms engines) and 032A2
(weak common access for C2/Image). The old Health Close always performed
physical teardown immediately. Health now counts admitted common requests
under its mutex. Closing with requests pending marks Health Closed, stops
metadata consumption immediately, deletes the public owner, and leaves
physical cleanup to the last request. With no requests, explicit synchronous
Close and detach-retry semantics remain in effect. Cleanup itself refuses to
start while requests remain.

The idle test-only Health view is weak. A claimed request owns Health strongly
through `CommonRequestClaim`, shares the Session `CompletionPermit`, and sends
outside the Health mutex. Send exceptions finish the claim; post-send failures
permanently retain the future, claim and permit. An unexpected finish exception
retains the graph. A deferred detach failure has no public retry owner and
retains the graph. Physical cleanup still waits for metadata callback drain
after channel destruction; disable alone is not treated as quiescence.

The C11 executable covers 22 isolated scenarios: held Attached and Enabled
KeepAlive/Comms (including high-bit IDs and cached Wait), weak-idle teardown,
common-close-first, parent-first Attached/Enabled, metadata logical stop,
callback-in-flight with a pending request, synchronous KeepAlive/Comms send
throws, Enabled and Attached Close-wins/send-throw, both directions of limit-one
Health/C2 Mode admission, four post-send permanent-retention cases,
finish-exception retention, and deferred detach-failure retention. In the
callback case, the pre-existing metadata barrier proves a callback is inside
the bridge while logical Close stops metadata; physical cleanup begins only
after the held future is released. The mock logs disable returning with the
callback active, followed by callback return, channel destruction, callback
drain, and provider graph teardown. A test-only callback-count observer checks
one callback in flight and zero after teardown; metadata receive remains
stopped without an enqueued event. The pre-existing Health tests also cover
rich metadata copies, malformed/overflow/allocation handling, partial
registration, ordinary no-request detach retry and non-quiescing disable.

The send-entry barrier proves the provider send executes outside the Health
mutex: the test reads `requests == 1` while the send is blocked, closes Health
and Session, then releases the barrier to throw. The claim's sole finish
decrements to zero, defers cleanup exactly once, and publishes no worker or
request. The Session's existing admission pool refuses either Health or C2
while the other owns its only permit; no Health capacity pool was added. Both
directions retry successfully after the previous FinalOwner releases capacity.

Final native validation: targeted seven high-risk cases pass 100 parallel
repetitions each. Fresh GCC Debug and Release builds each pass ordinary
**104/104** CTest and the complete `--parallel 4 --repeat until-fail:50`
suite (**104/104**, 91.70s Debug and 88.71s Release after the last test change).
`make test-native` and build-tree isolation pass. Direct Alire build and tests,
Rust workspace check/test/Clippy/fmt with the test-facade environment, and
Python's 54 tests and compileall pass. The native export inventory is the exact
starting-main set of **91** `ams_mel_` symbols at ABI **0.1**; test-only Health
view/counter/callback observers are absent from the production library.
Installed header, production export map, Ada/Rust/Python bindings, vendor and
provenance remain untouched. No public common Channel API, capability query,
Instrumentation/Track adapter, or real-provider Health result is claimed.

Local `make check` cannot finish because bare GNAT/GPRbuild is unavailable on
PATH: its native 104/104 and formatting gates pass, then the direct-GPR gate
fails. Foreground `alr -C ada build` and `alr -C ada/tests run` pass separately.
An initial parallel run of build-tree isolation and Alire briefly collided in
the shared `native/build` tree; the serial Alire run passed. Bare `cargo test`
links the production facade, which deliberately lacks test observers; Rust
workspace tests pass with the test-library/provider environment specified by
`make test-rust`. Hosted direct-GPR and Clang are required for complete CI
acceptance; this local result does not substitute for either.
