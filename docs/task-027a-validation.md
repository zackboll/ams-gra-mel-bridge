# Task 027A validation

Task 027A refactors private Image stream ownership only. It adds no public C,
Ada, Rust, or Python API and leaves ABI version 0.1 at exactly 56 exports.
NavigationReport is not implemented.

`ams_mel_ir_stream` is now a thin opaque owner of shared `ImageStreamState`.
That state owns the SessionState, CallbackState, Listener, Channel,
ImageChannel, ImageMetadataState, buffer factory/configuration, provider Buffer
objects, host storage, and enable state. A future asynchronous Image request can
therefore retain this state without retaining a public stream pointer.

The refactor preserves the established callback mutexes and metadata mutex,
detach-failure retry behavior, provider-channel destruction before host storage
release, frame callback in-flight draining, metadata callback lifetime, and the
capability query's unlock-before-provider-call behavior. It adds no request
counting, deferred request cleanup, or Navigation-specific state; those are
reserved for Task 027B.

Required validation includes normal native, Ada, Rust, Python, aggregate, stress,
ABI, and opt-in Squall gates. Existing C contract tests cover open/close,
start/stop, detach retry, frame/buffer lifetimes, capabilities, Image metadata
registration and callback lifetime, C2 coexistence, and parent-first Session
ownership.

Initial all-language validation encountered an environmental collision on the
default optical-health port 21315. Final acceptance used the supported explicit
host-port override: Couloir control 45203, Couloir metrics 45318, optical health
45315, and optical metrics 45316. The harness printed that plan and completed
the C, Ada, Rust, Python, and combined integration run. No unrelated listener,
container, or system resource was modified.
