# Task 032B1 — public common Channel C ABI

Starting `origin/main`: `054be5a87963e724175dbf8bbb5c5fccab40e217`.
Branch: `feature/032b1-public-common-channel-abi`.

This depends on completed 032A1–032A4: the single Return/Comms completion
engines (032A1), the weak `CommonChannelAccess` with C2/Image adapters
(032A2), Health (032A3) and Instrumentation/Track (032A4), and the Task 031B
per-Session admission. 032B1 does not redesign any of them; it publishes the
already-proven internal access through the C ABI.

## Public weak view

`typedef struct ams_mel_ir_channel ams_mel_ir_channel;` is a WEAK view of one
existing typed owner. It is not a provider Channel attachment, not a strong
owner and not a replacement for the typed owner.

```text
ams_mel_ir_channel
    -> CommonChannelAccess
        -> weak_ptr<void>            (typed family state)
        -> static admission adapter
        -> static claim adapter
        -> static capability adapter (new in 032B1)
```

No `std::function`, no strong state. A `static_assert` pins the public
wrapper's size to `CommonChannelAccess`. The view may outlive its typed owner
and the Session without delaying teardown.

## Exactly nine new exports

```text
ams_mel_ir_channel_from_c2
ams_mel_ir_channel_from_stream
ams_mel_ir_channel_from_health
ams_mel_ir_channel_from_instrumentation
ams_mel_ir_channel_from_track
ams_mel_ir_channel_send_keepalive
ams_mel_ir_channel_submit_comms_test
ams_mel_ir_channel_get_capabilities
ams_mel_ir_channel_close
```

ABI stays **0.1**. Production inventory: **91 -> 100**.

### Conversions

`(const <typed> *source, ams_mel_ir_channel **out_channel, char *diagnostic,
size_t diagnostic_capacity, size_t *diagnostic_required)`. They validate a
non-NULL source, non-NULL output, `*out_channel == NULL` and the diagnostic
pair. A conversion is allocation plus the existing `common_from_*` weak adapter
only: no provider call, family lock, Session admission or lifetime change.
Allocation failure returns `AMS_MEL_INTERNAL_ERROR` with NULL output and the
diagnostic `Channel view allocation failed`.

### KeepAlive and CommsTest

`ams_mel_ir_channel_send_keepalive` delegates directly to
`submit_common_keepalive()`; `ams_mel_ir_channel_submit_comms_test` delegates
directly to `submit_common_comms()`. There is no new engine, admission pool,
request or result type. The path is unchanged from 032A:

```text
preallocate -> lock weak state -> Session CompletionPermit -> family claim
            -> provider send (unlocked) -> shared worker
```

Results use the existing `ams_mel_ir_return_request_wait/close` and
`ams_mel_ir_channel_comms_request_wait/close`. Task 031B ResourceExhausted
behavior is unchanged. Request lifecycle per family is the 032A claim rule
(C2/Health/Instrumentation/Track: Attached or Enabled; Image: Attached or
Running).

### Pending request ownership

The view is weak; once admitted, `CommonRequestClaim` strongly owns the family
state exactly as in 032A. Therefore: held public KeepAlive -> close view ->
close typed owner -> close Session -> release future completes safely, and the
provider graph tears down only at the final claim.

### ChannelCapability

`CommonChannelAccess` gains one static `CapabilityFunction` slot. The export
validates arguments, temporarily locks the weak state (expired ->
`AMS_MEL_PROVIDER_FAILED`), and runs the family adapter while that temporary
strong reference is alive. Each family now has one small internal helper used
by BOTH its typed `*_get_capabilities` export and the adapter, preserving
lifecycle, locking, diagnostics/status and `snapshot_capability()` semantics:

| Family | Helper | Lifecycle rule | Provider call |
|---|---|---|---|
| C2 | `c2_capability` | Attached or Enabled | under C2 mutex |
| Image | `image_capability` | image channel present; not Stopping/Stopped/Failed | unlocked on copied channel |
| Health | `health_capability` | Attached or Enabled | under Health mutex |
| Instrumentation | `instrumentation_capability` | Attached or Enabled | under family mutex |
| Track | `track_capability` | Attached or Enabled | under Track mutex |

Image is deliberately **not** reduced to the common-request Attached/Running
rule. Provider-locking policy was not unified. The returned
`ams_mel_ir_channel_capability *` is the existing independent snapshot owner.

### Close and serialization

`ams_mel_ir_channel_close` deletes only the weak wrapper, is idempotent, nulls
the handle and performs no typed Close, cancellation, enable, disable, detach or
provider call. Operations on one view and Close of that same view must be
externally serialized; no reference counting was added for use-after-close of
the same wrapper. Multiple independent views of one typed owner are allowed.

## Legacy C2 compatibility

`ams_mel_ir_c2_send_keepalive`, `ams_mel_ir_c2_submit_comms_test` and
`ams_mel_ir_c2_get_capabilities` remain unchanged in signature and behavior.
They do not allocate a temporary public view; the typed capability export now
calls the shared `c2_capability` helper with identical logic.

## Language bindings

- **Ada (private FFI only):** `AMS.MEL_C_API` adds `Channel_Handle`,
  `Null_Channel` and nine imports. No safe Ada semantics changed;
  `AMS.MEL.IR.C2.Common` compiles and passes unchanged.
- **Rust sys:** opaque `AmsMelIrChannel` and nine raw declarations with exact
  C constness. Raw inventory 100, checked against `exports.map`. No safe Rust
  common Channel API.
- **Python:** private `IrChannelHandle` and nine ctypes bindings with exact
  argtypes/restype; private inventory 100. No public Python Channel API.

## Why safe Ada is deferred (032B2)

Today `AMS.MEL.IR.Channel` depends on `AMS.MEL.IR.C2` for `Command_ID`, and
the reusable Return request/result ownership (`Command_ID`, `Return_Request`,
`Return_Result`, `Outcome`, `Error_Code`, `Command_Return`, `Comms_Request`) is
C2-resident. A clean safe façade needs a separate source-compatibility
refactor so Channel does not acquire an inverse C2 dependency. 032B1
intentionally establishes the native ABI first and moves no Ada types.

## Not exposed

Generic Enable/Disable, buffer registration, common metadata callback
registration, generic typed operations, Scheduling, RF and StackedImage.

## Evidence

`native/tests/test_public_common_channel.c` (C11, installed header only, one
process per case) runs 46 CTest cases:

- per family (`c2`, `image`, `health`, `instrumentation`, `track`) x
  `lifecycle`, `weak`, `close-first`, `pending`, `multiple`, `snapshot`,
  `invalid`, `allocation`;
- `c2-legacy-{keepalive,keepalive-fail,keepalive-reject,comms,comms-reject,refusal}`.

`lifecycle`: KeepAlive (held, Wait(0) times out, released, repeated cached
Wait with a different diagnostic buffer), high-ID CommsTest (`0x80000001` /
`0xf0000002` / `0xe0000003`, exact reply IDs, cached Wait) and deep capability
fidelity against the typed export, first Attached then after typed Enable
(Image: Start -> Running). Instrumentation `submit_level` and Track
`submit_update` / `submit_system_track_data_response` still return
`PROVIDER_FAILED` while Attached.

`weak`: typed owner and Session close first with the view alive; the channel,
Control, manager and library each tear down exactly once; KeepAlive, CommsTest
and Capabilities return `PROVIDER_FAILED` with NULL outputs and no provider
send; double view Close adds no provider event.

`close-first`: closing the view (twice) adds no provider event; typed
Capabilities and typed Enable still succeed; typed Close tears down normally.

`pending`: Session limit 1; a held public KeepAlive; a second submission is
`RESOURCE_EXHAUSTED` before send (the existing permit is reused); close view,
typed owner and Session with no teardown and one active permit; releasing the
future completes `OK`/Success, releases the permit and tears the graph down
exactly once.

`multiple`: two views; closing one leaves the other fully usable; after typed
Close both are expired and neither delays teardown.

`snapshot`: generic and typed capability snapshots remain readable and
equivalent after view, typed owner, Session and provider library are gone
(rich C2/Health/Instrumentation/Track capability with IDs, location, bands and
nav frames; rich Image capability).

`invalid`: NULL source, NULL output, non-NULL `*output`, invalid diagnostic
pairing for every conversion; NULL channel, NULL request/output, non-NULL
output and invalid diagnostic pairing for the three operations; NULL
pointer-to-handle Close is `INVALID_ARGUMENT`; Close of a NULL handle is
idempotent `OK`; no provider event occurs.

`allocation`: the test-build-only `AMS_MEL_TEST_CHANNEL_VIEW_FAILURE=allocation`
failpoint yields `INTERNAL_ERROR`, NULL output and no provider event; the typed
owner stays usable and a later conversion works. The failpoint is compiled only
with `AMS_MEL_ENABLE_TEST_FAILPOINTS`; production has no behavioral variable.

`c2-legacy-*`: old and new paths agree on status, Return/Comms result record
and provider diagnostic for Success, `Return::Fail`, KeepAlive rejection,
high-ID Comms and Comms rejection, Attached and Enabled, with deep capability
equality; a Failed C2 owner refuses KeepAlive/Comms/Capabilities identically.
Pointer identity and allocation behavior are not compared.

The C header test (`test_c_abi.c`) binds all nine declarations with exact
function-pointer types plus a pointer-size assertion; `test_cpp_header.cpp`
statically checks all nine exact `noexcept` C++ types. Both language ABI probes
(`python/tests/abi_probe.c`, `rust/ams-mel-sys/tests/abi_probe.c`) bind them.

All 032A internal/test-only common-view suites are retained unchanged.

## Validation

Local (Linux, GCC):

- Fresh GCC Debug tree: ordinary CTest **197/197** (151 prior + 46 new) and full
  `--parallel 4 --repeat until-fail:50` PASS.
- Separate fresh GCC Release tree: ordinary **197/197** and full parallel
  repeat-50 PASS.
- Targeted `--parallel 4 --repeat until-fail:100` over 55 cases (all 46 public
  cases, including all five weak-view, all five pending/view-close, C2
  legacy-vs-generic, Image lifecycle/Running, capability snapshot ownership,
  plus the 032A C2/Image/Health/Instrumentation/Track parent-first and
  Image-running cases) PASS.
- `make test-native` 197/197; `make test-build-isolation` PASS.
- `make check-ada-format`, `alr -C ada build` and `alr -C ada/tests run` PASS
  (including `AMS.MEL.IR.C2.Common`).
- `make test-rust` plus workspace `cargo check`, `cargo test`,
  `cargo clippy --all-targets -- -D warnings` and `cargo fmt --check` PASS; raw
  sys inventory 100 equals `exports.map`.
- `make test-python`: 56 tests (54 existing + 2 new raw-binding tests) and
  compileall PASS; private raw inventory 100.
- `scripts/check_final_newlines.py`, `git diff --check` and
  `git diff --cached --check` PASS.
- `make check`: FAIL — GNAT/GPRbuild unavailable on PATH (its native and
  formatting stages passed first). The gate was not weakened; direct Alire
  validation above is separate.

ABI audit: a fresh non-test Release build reports ABI 0.1 and exactly 100
`@@AMS_MEL_0.1` `ams_mel_` exports. After normalizing the ELF version suffix,
all 91 starting-main names are present and the added set is exactly the nine
names above; no test hooks appear. No existing declaration line in `abi.h` was
removed or altered. `native/vendor/**` and provenance files are unchanged.
