# IRMEL-ADA-019 — standalone required-channel Disable

## Scope and preparation

Prepared from freshly fetched main `1a3f3efbd5ff2520697fed0c924f9e4eb1c76ebd`
on `feature/irmel-ada-019-common-disable`. Reviewed open PR #204 at
`0cde3ca3935de32b2348a29b32cb3817ae8dce0e` and #208 at
`34d0e4e3f2b3bce4d32dd9e842e6168e3a47eaca`, including diffs, descriptions
and review comments. #204 has an unresolved callback ABI conflict and is not
copied. #208 hardens C2 callback registration without changing the ABI; main
merged it during development at `e00369f`. Reconciliation uses ordinary merge
commits, not rewritten published history. Neither existing branch is modified.

The pinned `Channel.h` declares distinct `enable()` and `disable()` operations
and explicitly permits buffer registration before Enable or after Disable.
There is no upstream callback-quiescence, cancellation, buffer registration,
or guaranteed successful re-enable promise inferred from Disable.

## Implementation and lifecycle

One additive C export, `ams_mel_ir_channel_disable`, under `AMS_MEL_0.1`:
323 → 324 production exports, no record-layout changes. Private Python ctypes
and raw Rust sys mirror it; neither safe language API grows a Disable method.
Safe Ada exposes only `AMS.MEL.IR.Channel.Disable (Channel : View)` using the
existing typed `As_Channel` conversions. Trailing GNATdoc describes eligibility,
failure, lifetime and callback boundaries.

`CommonChannelAccess` adds one optional static family function, defaulting to
unsupported. The idle view remains weak. Disable copies that adapter before
provider entry, temporarily resolves the family state, reserves the existing
family request count, and transitions Enabled/Running → Disabling under the
family mutex. The actual inherited provider call runs without lifecycle or
metadata locks. Publication changes Disabling → Attached on success or Failed
on refusal/exception, but never overwrites a terminal Close. The final operation
claim uses the existing family deferred cleanup. No second request engine,
metadata registration, callback destruction, queue drain, or request cancellation
is introduced. Attached covers never-enabled and successfully disabled states.
Both are successful no-ops without provider invocation. Transient or failed
lifecycle refuses deterministically rather than waiting on reentrant work.

An actual upstream Return is separately published, including Fail and
NotSupported. `UINT32_MAX` means **no Return obtained**, including no-op,
unsupported/expired target and provider exception. Bridge status remains
independent. Disable consumes one enable-cycle obligation before provider
entry, even on failure: Close must detach, not silently retry Disable. Failure
remains sticky for cleanup reporting. A failed detach retains the caller's
owner for retry or permanently retains an orphaned graph; successful detach
can consume the owner while still reporting the earlier Disable failure.

C2 and Health Enable use the same claim/call/publish shape instead of calling
the provider under the lifecycle mutex. Stacked already had this shape.
Control-only Image Start now similarly retains its graph across provider entry.
The operation attempts the actual provider again on each new cycle and does
not manufacture re-enable success.

Only control-only Image is eligible: zero layout is preserved, and no image
buffers, registration, frames or acquisition activity are introduced. It never
routes through Stop. Existing Image Stop remains terminal, including for
control-only owners. Acquisition Disable is explicitly rejected. Mono8 queues,
frame leases, registered host ranges and uncertain-release retention are not
generalized. A failed control-only standalone Disable waits for explicit cleanup
rather than causing the ordinary final-child cleanup loop to spin.

Instrumentation, Track and Scheduling reject standalone Disable, including
through their common views. Their Close-only inherited use is unchanged.
Public-wrapper lifetime still requires external serialization of operations
against Close on that same wrapper; distinct weak views can race typed Close.

## Regression evidence

Compiled C11 tests use active checks in Release, condition-variable barriers
and mock future gates, not sleeps. They exercise all four required profiles:
never-enabled no-op, repeated views/idempotence, Enable/Disable/re-enable,
provider Success/Fail/NotSupported/exception, actual synchronous metadata delivery,
recursive operation refusal, reentrant Close consuming both typed and weak
wrappers, concurrent Close during a blocked provider call, pending KeepAlive and
CommsTest futures unaffected by Disable, callback queue survival, exact call
accounting, failed-detach retries, provider graph destruction and DSO unload.
Event order requires provider entry → callback return → provider return → detach.
Separate C tests reject all unsupported profiles while Attached and Enabled.
Safe Ada consumers call no private C interfaces: all required families exercise
the common API, capabilities, idempotence, re-enable, independently pending
KeepAlive completion and expired views; unsupported families are also tested.

The mock's Ada pending future is deterministically fulfilled by its provider
when its Disable body reaches the return marker; C tests independently hold
both future kinds *after* Disable and release them explicitly to prove no forced
cancellation. This is mock scheduling, not a generic provider completion promise.

## Coverage delta

Authoritative mapping and independently anchored review inventory update C2,
HealthStatus and StackedImage inherited Disable: `partial → safe_ada`.
Image keeps `safe_ada`, explicitly distinguishing control-only non-terminal
Disable from acquisition terminal Stop. Other families stay partial. Unique
Channel Disable remains partial overall; unique safe-Ada operations stay **57**.
The generated report is regenerated. Historical `docs/coverage.md` is unchanged.

## Qualification record

Development feedback (not final qualification): native 699/699 and safe Ada
passed. Earlier development failures exposed transient cleanup-loop handling,
request-only failpoint interaction, metadata FIFO expectations and stale
coverage counts; these were corrected without weakening existing contracts.
Bare `make test-ada` lacks GPRbuild on PATH; the existing pinned Alire environment
supplies GNAT 16.1.0 and GPRbuild 26.0.1 for the same Make target. GNATdoc public
trailing-style HTML generation succeeded and the Disable parameter/exception
sections were inspected. Final integrated commands/results are recorded below
after execution; incomplete checks are not claimed as passed.

### Integrated qualification

Main reconciliation: `da98ae6` merges main `e00369f`; both test additions and
both mock detach-failure scenarios were preserved when resolving the two
ordinary content conflicts. C2 registration hardening came from merged main,
not an unresolved branch. #208 is merged and #204 was subsequently closed by
its existing workflow; this feature did not change either PR or branch.

Compiled lifecycle source was finalized in `54afdcf`; reviewed inventory/report
refresh is `f277aa7`. The ordinary qualification chain is sequential, never
concurrent against mutable native/Ada build trees. Logs are under
`/tmp/irmel019-*` in the qualification workspace.

- `make format-ada`, `make check-ada-format`, `make check-ada-docs-changed`:
  passed; three changed public declarations inspected, zero annotation gaps.
- `make test-native`: 743/743 passed before the last additional failed-Stop
  case; aggregate build-isolation run on final compiled bytes: **744/744**.
- `make test-python`: **586 passed**, including exact private ABI inventory.
- `make test-rust`: **570 passed**, including **53 doctests**, **12 existing
  opt-in ignored**. Exact sys inventory/signature checks passed.
- `alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge test-ada`:
  passed, including safe common Disable and genuinely pending KeepAlive tests.
- `make check-ir-api-coverage`: **67 controls passed**, **57** safe-Ada unique
  operations; report generated and matching.
- GCC 14.2.0 and Clang 19.1.7 `-std=c11 -pedantic-errors -Wall -Wextra -Werror`:
  both new compiled-C tests and every existing layout probe passed syntax and
  exact function-pointer type checks.
- `cargo fmt --manifest-path rust/Cargo.toml --all -- --check`, workspace
  all-target Clippy `-D warnings`, workspace rustdoc `-D warnings`: passed.
- `make test-policy`: passed, including source-integrity/security and current
  documentation controls. Direct integrity check: **825 vendor files**, four
  workflows. `git diff --check` and final-newline checks passed.
- Pinned documentation generation from Git at `f277aa7` passed via
  `alr -n -C docs/toolchain exec -- ... docs/tools/docs_ci.py build
  --directory .doc-tools/irmel019-api --gnatdoc-prefix
  /home/zboll/.alire/gnatdoc_bin_26.0.0_6e6e0320` with the existing enhancer
  interpreter. Public HTML Disable parameter and Provider_Error sections were
  verified. Source-derived coverage **5104/5105**, 101 informational GNATdoc
  warnings; no claim of zero documentation warnings.

Final qualification completed successfully:

- `alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge check`:
  **passed**, including both build-isolation directions, final native **744/744**,
  Ada formatting, 67 coverage controls and complete safe-Ada smoke suites.
- `alr -n -C ada build` and `alr -n -C ada/tests run`: **both passed** using
  the pinned GNAT 16.1.0/GPRbuild 26.0.1 environment, without dependency edits.
- Four isolated CMake trees configured with `AMS_MEL_BUILD_TESTS=ON`, explicit
  `/usr/bin/{gcc,g++,clang,clang++}`, and Debug/Release: **744/744 passed in
  each configuration**, warning-clean under the repository `-Werror` policy.
  Each then ran `ctest -R '^channel_disable_' --parallel 4 --repeat until-fail:25`:
  **54 cases × 25 repetitions passed per configuration**, 5,400 total stress
  executions. Release checks remain active under NDEBUG.
- Production and test facade export audits: **exactly 324 production names**
  matching the map, new symbol `ams_mel_ir_channel_disable@@AMS_MEL_0.1`, no
  unexpected production exports. A premature audit during build isolation found
  its intentionally replaced DSO absent; the successful audit ran after builds.

The isolated matrix build products are moved outside the checkout after
qualification; generated CMake progress files are not first-party source and
are not included in whitespace/newline qualification.

Hosted evidence for pushed source/report commit `bde5d3b`:

- Bootstrap: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43669429422>
- Documentation: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43669429438>

Documentation, policy, Python, Rust, Ada, build isolation, GCC Release and both
Clang configurations passed at the last observation; GCC Debug was still
running. This final report-only commit will have its own exact-head runs.
**HOSTED_CI_PENDING** until all required exact-head workflow results exist.

The Python regression caught an unintended alteration to Health's existing
Close-only failed-detach retry behavior. That behavior is preserved; only an
*explicit standalone Disable* consumes its own attempt. No test was weakened.

## Limits

No application-owned registration/unregistration or callback checkout/release
is implemented. No local `minimal-mel-example` checkout was found; real-provider
Disable/Enable is **PRIVATE_PROVIDER_INTEGRATION_UNTESTED**. Mock results do not
qualify hardware. Provider C++ ABI compatibility remains necessary. Failed
ownership proof can retain resources until process exit. No blanket SPARK claim.
