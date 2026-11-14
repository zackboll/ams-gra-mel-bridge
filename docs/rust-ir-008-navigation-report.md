# RUST-IR-008 — Complete safe Rust NavigationReport submission

## Source preflight and boundaries

Starting source is freshly fetched `origin/main`, commit
`de529f92f36623db8cdd3ccc6f3cdf7da31a1be8`, tree
`4c418dde53e5e6650e70b5342268056628b9b87d`. Branch:
`feature/rust-ir-008-navigation-report`; worktree:
`/home/zboll/git/ams-mel-doc`. Preflight checked open GitHub PRs and remote
branches: no overlapping Rust NavigationReport work was present. Existing native
NavigationReport and corrective branches were identified and left untouched.
Unrelated branches/worktrees are preserved; no history is rewritten.

Reviewed repository instructions, contribution/architecture/C ABI policies,
Task 027B and its close-race correction, the native implementation and C tests,
raw Rust declarations/ABI probes, current Rust C2/Image ownership patterns,
pinned `mel/library/NavigationReport.h`, and the Ada Image navigation API/body.
The structured inventory schema is specifically a native/Ada audit; its existing
complete navigation entries and reviewed implementation identities remain valid
and unchanged. No Rust coverage classification exists in that schema.

Production changes are safe Rust only. Native implementation, exports, sys
declarations, providers, upstream headers, Ada, Python, dependencies, CI policy
and the historical coverage archive are unchanged. The existing response type's
Rust documentation now explains its reuse for metadata and request completion.

## Public API and field mapping

`ImageStream::submit_navigation_report(&mut self, &NavigationReport)` returns
`Result<NavigationRequest, Error>`. `NavigationRequest::wait(&mut self, u32)`
returns `Result<NavigationResult, Error>`; consuming `close(self)` returns
`Result<(), Error>`. The integration test constructs a complete application
value and exercises Session → Image attachment → submission → wait → cleanup.

All input records are fully owned, copyable Rust values. Existing `Euler` and
`NavigationResponse` are reused, not redefined. Snake-case fields match the
existing native record spelling and published source member order:

| Upstream getter/member | Rust / native v1 field | Representation and explicit units |
|---|---|---|
| `getSystemTime` | `system_time_ns` | signed i64 nanoseconds, no inferred clock/epoch |
| `getState` | `state` | PositionSolutionState: NotSet=0, Aligning=1, FreeInertial=2, Gps=3, Blended=4 |
| `getLatitude` | `latitude_rad` | f64 radians |
| `getLongitude` | `longitude_rad` | f64 radians |
| `getAltitude` | `altitude_m` | f64 metres |
| `getAttitude` | `attitude` | existing Euler roll/pitch/yaw, unchanged |
| `getAttitudeRate` | `attitude_rate` | AttitudeRate: Euler `attitude_rate`, independent signed i64 `attitude_rate_time_ns`; rate units unspecified |
| `getSpeed` | `speed` | NorthEastDown north/east/down, f64 metres/second |
| `getAcceleration` | `acceleration` | NorthEastDown north/east/down, f64 metres/second² |
| `getWanderAngle` | `wander_angle_rad` | f64 radians |
| `getMagneticHeading` | `magnetic_heading` | f64; no units inferred |
| `getAltitudeMSL` | `altitude_msl` | f64 barometric altitude; no units inferred |
| `getPositionVelocityCovarianceUncertainty` | `position_velocity_covariance_uncertainty` | complete named PositionVelocityCovariance below; units unspecified |

The 18 covariance fields, in source order, are:

1. `position_position_pn_pn`
2. `position_position_pn_pe`
3. `position_position_pn_pd`
4. `position_position_pe_pe`
5. `position_position_pe_pd`
6. `position_position_pd_pd`
7. `position_velocity_pn_vn`
8. `position_velocity_pn_ve`
9. `position_velocity_pn_vd`
10. `position_velocity_pe_ve`
11. `position_velocity_pe_vd`
12. `position_velocity_pd_vd`
13. `velocity_velocity_vn_vn`
14. `velocity_velocity_vn_ve`
15. `velocity_velocity_vn_vd`
16. `velocity_velocity_ve_ve`
17. `velocity_velocity_ve_vd`
18. `velocity_velocity_vd_vd`

Conversion explicitly assigns every field to `AmsMelNavigationReportV1`.
`TryFrom<u32>` rejects MaxExclusive (5) and all unpublished states before a
report can be constructed with them; conversion checks the encoded domain.
No unsafe integer-to-enum cast, numeric narrowing, normalization, coordinate
transformation, covariance completion, angle conversion or physical validation
is performed. Full signed timestamp ranges and IEEE values pass unchanged;
NaN payload preservation beyond the native contract is not promised.

## Native contract, ownership and errors

Reuses exactly the existing functions:

- `ams_mel_ir_stream_submit_navigation_report`
- `ams_mel_ir_navigation_request_wait`
- `ams_mel_ir_navigation_request_close`

Submission is valid while Attached or Running, including before Start. After
Stop, native submission returns ProviderFailed; Image Close consumes the Rust
owner, making subsequent submission a compile error. Input is synchronously
copied; the request has no borrowed dependency on the report, Image or Session.
It is unique and implements neither Clone, Send nor Sync (compile-fail tests).
Rust adds no callbacks, queues, synchronization engine or worker threads.

Native completion owns the future and retained provider graph and is the only
future consumer. Timeout is `ErrorKind::Timeout`, never cancellation; `wait(0)`
polls. All terminal outcomes are cached natively and repeatable. Metadata
callback arrival alone is not future completion. Logical Image Close defers
physical teardown while work remains. Requests survive both parent closures;
cached Rust-owned values remain readable after provider unload.

`NavigationResult::Success { response }` retains all response fields:
`system_time_ns` (i64), `command_id` and `request_id` (u32, including high bits).
`Rejected { code, description }` preserves the actual native MEL error code and
complete UTF-8 description. Timeout, provider exceptions, failed futures, null
provider results and deferred-cleanup failures are `Error`, not rejections or
fabricated responses. Existing diagnostic-required lengths are retained in
errors. Oversized terminal diagnostics use a fallible exact allocation and a
cached zero-timeout retry, checking status, required size and active result
fields for consistency. Inactive rejection/failure response fields are ignored.
Submission and release are never retried to recover diagnostic text.

A guard cleans up unexpected non-null submission owners on failure, including
diagnostic decode failure; successful null owners fail closed. Before release,
the wrapper relinquishes its pointer. A failed/malformed native release cannot
cause Drop to retry a possibly freed pointer. An uncertain pointer is abandoned,
not dereferenced or retried; this deliberately favors safety over reclaiming an
owner whose release contract has been violated. Drop is non-raising best-effort.

## Qualification coverage

Focused integration tests in `rust/ams-mel/tests/ir_navigation.rs` exercise the
public safe API with the separate existing mock provider:

- Every input getter including all 18 distinct covariance terms, before Start;
  complete high-bit response IDs and negative response time.
- Complete state domain, invalid sentinel/out-of-domain rejection, boundary
  numeric acceptance, immediate completion and Running submission.
- Exact short and long multibyte UTF-8 rejection and cached replay.
- Send exceptions, failed futures, null successful provider result, exact error
  descriptions/lengths and cached replay.
- Barrier-held pending requests, zero-timeout polling, callback arrival before
  future completion, timeout without cancellation, multiple outstanding owners.
- Both parent-close orders, request Close/Drop while pending, deferred cleanup
  failure with cached error and subsequent native cleanup retry by Image Close.
- Exact lifecycle log ordering: completion before channel destruction, callback
  quiescence before unload, channel destruction before library unload; no
  premature teardown while the future is held.

Existing completion-owner probes synchronize worker graph reclamation. Marker
polls have bounded deadlines and observe explicit barriers, not guessed provider
delays. Unit tests inject malformed FFI outputs without dereferencing fake opaque
tokens: null successful owner, non-null failed owner, malformed diagnostics,
uncertain release, uncleared release, invalid wait error/diagnostic outputs and
changed cached retry results. Boundary conversion tests check every float field
and both full-range signed timestamps. Compile-fail doctests enforce ownership
and threading exclusions and use-after-Image-Close prevention.

## Actual commands/results

Qualification results are recorded below after execution. Local raw logs are
retained at `/tmp/rust-ir-008-evidence` on the qualification host.

All commands run from `/home/zboll/git/ams-mel-doc`. Cargo qualification uses:

```sh
export AMS_MEL_NATIVE_LIB_DIR="$PWD/native/build-tests/lib"
export AMS_MEL_TEST_PROVIDER_DIR="$PWD/native/build-tests/test-providers"
```

Installed toolchains: rustc/cargo 1.98.1, GNAT 16.1.0 and GPRbuild 26.0.1.
GNAT/GPRbuild binaries from the existing local Alire toolchain installation
are explicitly added to PATH for `make check`; no toolchain downloads were
introduced. Cargo's
test-provider environment variables must be unset for `make check`, because
Alire supplies the provider path itself using a lexically different spelling.

| Command | Actual result |
|---|---|
| `make native` | PASS, fresh production build, warnings-as-errors |
| `make test-native` | PASS, 523/523, including C Navigation and ABI regression tests |
| `cargo test --manifest-path rust/Cargo.toml --workspace` | PASS, 403 passed / 0 failed / 9 pre-existing ignored stress tests; ignored tests are not counted as passes |
| `cargo check --manifest-path rust/Cargo.toml --workspace --all-targets` | PASS |
| `cargo clippy --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings` | PASS |
| `cargo fmt --manifest-path rust/Cargo.toml --all -- --check` | PASS |
| `RUSTDOCFLAGS="-D warnings" cargo doc --manifest-path rust/Cargo.toml --workspace --no-deps` | PASS |
| `make check` | PASS, build-tree isolation, 523 native tests, pinned Ada formatting check, 52 structured coverage tests, warning-clean Ada compilation/smoke suites, final-newline and Git whitespace checks |
| `python3 scripts/check_doc_source_integrity.py` | PASS, 824 vendor files and 3 workflows |

The Rust total includes 10 new public integration tests, 6 new private boundary
tests and 4 new compile-fail doctests. The existing raw
`declarations_match_the_c_header` test passes unchanged. All build-tree-mutating
qualification commands run sequentially, not concurrently.

Initial `make native` and `make test-native` attempts failed before compilation:
pre-existing CMake caches referenced `/home/zboll/git/ams-mel`, a different
checkout. Both stale build directories were preserved under the evidence
directory and fresh local production/test trees created. No source change was
made to repair this environment issue. An initial all-target Cargo check caught
a test trying to reuse consuming Image Close; it was corrected to a compile-fail
ownership test rather than weakening the existing API.

The first focused integration run exposed an incorrect test expectation for
native poisoned lifecycle semantics: successful retry detach still reports the
earlier ProviderFailed. The test now requires that exact failure, cached request
error and destruction ordering. The first Clippy run flagged the mock fixture's
deliberately approximate angle `0.523598`; the test now spells its exact f64 bits
rather than suppressing the lint or changing the value to pi/6. Final Rust
workspace, static and documentation runs after these corrections all pass.

An initial `make check` completed build-tree isolation and all 523 native tests,
then Alire rejected a duplicate `AMS_MEL_TEST_PROVIDER_DIR` environment assignment
(absolute Cargo path versus Alire's equivalent `ada/tests/../../native` path).
The aggregate gate is rerun without the Cargo-specific environment overrides;
this is an invocation correction, not a source or policy change.
A second aggregate attempt without those overrides completed formatting and
the 52 inventory tests but stopped because the new shell had omitted the local
GPRbuild PATH. The final invocation both unsets the Cargo-specific variables
and exports the installed GNAT/GPRbuild binary directories.
The final aggregate run exits 0. GPR project discovery prints the existing
`ams_mel_c.gpr` warning about declaring language C in a source-free imported
library project; no first-party compiled-source warning was introduced.

## Coverage delta and limitations

Previously raw-only NavigationReport submission now has complete safe Rust
input, request, response/rejection and lifetime coverage. This is one focused
vertical slice, not the entire minimal C++ consumer. Optional Image sends and
metadata not already exposed in safe Rust, Instrumentation/Track/Scheduling
owners, algorithms, services and provider-private transports are out of scope.

Positive qualification uses the separate mock provider, not hardware or a real
Squall deployment. No real-provider interoperability, physical navigation
accuracy, cancellation, callback-arrival completion or broader threading promise
is inferred. Provider C++ ABI compatibility and compliance with native memory
and lifetime contracts remain prerequisites. No unresolved upstream semantics
are guessed; unspecified units remain unspecified. Hosted CI and independent
review are separate from local qualification; no self-approval or merge occurs.
