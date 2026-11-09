# IRMEL-ADA-011 — SchedulingChannel foundation qualification

## Scope and base

Base: `e746c49fd3ffe57b37064878cdf8225a3745a35e` (fetched `origin/main`, PR #171; tree
`7b022169714075949ff0604ad79d94238cf532cd`). The task's verified starting main was
`8df77cd`. Fresh main had advanced only by PR #171, which modifies a CameraProtect Ada test and
its native observation helpers. This slice starts from that fresh main; it does not use PR
#171's branch as a base.

This is the ownership/lifecycle foundation of the conditionally required
(`@RequiredIfIRSTTrackTasking`) pinned `SchedulingChannel`, plus its inherited common Channel
services. It is **not** an implementation of any of the eleven Scheduling-specific methods.
Qualification uses the separate mock provider only.

## Pinned upstream API (verified)

IR MEL `open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel` at
`8d9224519f12b44e0b28815755c56a32a28d24a0` (tree `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec`),
read from the frozen `docs/reference/irmel-upstream-git-evidence.json` objects:

- `scheduling/SchedulingChannel.h`: `class SchedulingChannel : public virtual Channel`.
  Scheduling-specific virtuals: four `exec` overloads (`AbortTask`, `IRSTScanTask`,
  `IRSTStareTask`, `IRSTTrackTask`, each `RequestFor<Return>`), one
  `send(PrioritizedTrackListCmd)` returning `RequestFor<TrackListResponse>`, and six
  `registerMetadataCallback` overloads (`TaskExecutingRep`, `ExecuteTaskAck`,
  `SchedCreatedRep`, `UpdateTrackListResponse`, `TaskEvents`, `ScanPerformanceReport`). That is
  4 + 1 + 6 = 11.
- `irmel-types/ChannelType.h`: `Scheduling` is the fourth alternative of
  `enum class ChannelType : std::uint32_t`, numeric value **3**. This is static-asserted
  natively and in the C11 layout probe, and checked in Ada with `'Enum_Rep`.
- `irmel-types/Control.h`: `attachChannel(const Config&)` returns a generic
  `std::shared_ptr<Channel>`; `detachChannel(std::shared_ptr<Channel>)` returns `Return`.
  Compatibility with `SchedulingChannel` therefore has to be checked by `dynamic_pointer_cast`.
- `irmel-types/Channel.h`: `enable()` and `disable()` are inherited `Channel` operations, as
  are `sendKeepAliveRep()`, `send(ChannelCommsTestReq)` and `getCapabilities()`.
- `irmel-types/Config.h`: the seven-argument constructor (channel ID, type, platform, sensor
  location, image listener, two Track flags) is used with an empty listener and both flags
  false.
- `irmel-types/ChannelCapability.h`: upstream documents `TaskScheduleDepth`. `n > 0` means n
  tasks queued in priority order. **0** means a new IMMEDIATE task aborts the current one and
  the MFP manages priority. Zero is therefore a valid mode, not absence of Scheduling.

## Vendored dependency closure

The closure was computed from the frozen catalog's include lists: 66 IR/Common headers plus
AMS Math `RangeAzEl.h` and `Quaternion.h`. It was then measured with the compiler on the new
`scheduling_header_compile_probe.cpp`. GCC 14.2 observes 68 vendored files (IR 38, Common 28,
Math 2). Clang 19 observes the same set plus one alternative path spelling of `UCI_ID.h`. No
Boost header is reached. Exactly 20 IR headers were absent. Each was copied from the frozen
blob after verifying its Git blob id and SHA-256:

| Vendored file (`native/vendor/ir-mel/include/irmel/library/…`) | Git blob | SHA-256 |
|---|---|---|
| `irmel-types/AbortTask.h` | `2223fa50d3a78bd03621a2f910b09dcc69b534a6` | `8eedb701561ef1daaf37c9ded72cd6eb41fb54bad786412072fd1c8ce4b0e7f6` |
| `irmel-types/IRSTScanTask.h` | `469c5755f52c0b43cbe507ff22cbbd5bc0099a0c` | `9793e3135dc3c032c83bb393a8f6e6d04d1bbc60d3481aab8fff64dd41242400` |
| `irmel-types/IRSTStareTask.h` | `23b3a8695abdaf68d072ac4498d9040914e24b83` | `7383731eaf3879acb9935e4090968f5eb0f43f64d8de9f0704dd719e50b7e3db` |
| `irmel-types/IRSTTrackTask.h` | `2d4735bc31bb8fbae32109d3a284667cf4643506` | `1db6720244d23a9859cd7553a85ddba65f534ea04bd93ae533093de7d505af3b` |
| `irmel-types/PrioritizedTrackListCmd.h` | `d8ff82d65f0beca36c8d468cc6cde64175d886ae` | `c0a67ef2d21c2a5ef05390e307565ef748fc54c2d1f4217c06551b7410ebf926` |
| `irmel-types/StareTaskParam.h` | `021364909e907bd17a565ec89475dae3d11d4001` | `91284108a1d920c125b66e8010cae0acc8f19bbfb6289e03dd5cf6fb4efa0b6f` |
| `irmel-types/TaskInfo.h` | `b2590330f32ffb56cab07c9f2ef20c78c53f57c1` | `28951d487d3211ad393b01b5bd8557be7f2ae8753a65706e98cd23fcce49b878` |
| `irmel-types/TrackData.h` | `d019a525a56b62b820bdc61729c3e3e10be3d6a8` | `a20c1a609b4369e70fd1adbfd1d062004cbdcf83ebf1c335c044bbf691ac2220` |
| `irmel-types/TrackListResponse.h` | `39172d49d3dbbeb85cdb330d792e87b3c563b8d5` | `281634d8ad8d646f6dc90b7924fced9538709b459016da1b5fad648b795f308d` |
| `irmel-types/TrackTaskData.h` | `d2aa159a954553b71e470edf161b48727e358c2e` | `6095b9b45a4011fb1ada1ab316161f8ff8f91fb8fafa97168c7bb761b69253e6` |
| `irmel-types/TrackTaskParam.h` | `8f84eae3192e651f78e7e62831bc2ac34e1203d0` | `1adbbac97817aac09203fb3ffc20f1f68e7adea21d58f83e79c1b09a88aab757` |
| `irmel-types/UpdateTrackListResponse.h` | `ac4c7f5be4444192e0330d26f685b5713ad0939b` | `57d1f489aeca4aa7e7d95346cfe0c72045228f99ae69b6f6bc0a06d4e499de96` |
| `scheduling/ActivityRecord.h` | `b82978db90b0d23600c23cb6395fb97228d2d1f2` | `3a9cd51e8558b14d4001c6d29bb67f3e74b9ebdc5d9ecdbff0485299a382bcbc` |
| `scheduling/EventType.h` | `6670b2bc45fe1db337abdc32f204446f4ce4643b` | `b7a75ecd1db411faf74f663dde109d86a6aee01615af063473fb003c571339cc` |
| `scheduling/ExecuteTaskAck.h` | `4593cb1357038f157f77df11a022e706d3cbcd21` | `0372fa68dcfbf698e478425b959151f4dea9cc3905a2a543a8417b58700ea708` |
| `scheduling/ScanPerformanceReport.h` | `210e304bf710083340e77e7c5befe5867810cdef` | `291787d4e5ae4fe8409f4a8b2bfa4718b5051622e13d2d7f233f56593a3884a8` |
| `scheduling/SchedCreatedRep.h` | `240b7b7a0d4e7daa75de45ad83c1c96b3d8f0fed` | `74b3f5fac331fd5352cc36295075e464407abc6f3dc547492775d5ef81ab5492` |
| `scheduling/SchedulingChannel.h` | `f11932ac09f2644aa47b00ed7623bb3b8ca9ddf2` | `9e90fcd4066d82bf67d001283c1e07d8efb32077d1b62938a98bde910af13191` |
| `scheduling/TaskEvents.h` | `06ddc86196fb16ce95531c11531ac1ca82b7ecf7` | `a935607a508bf2b17c8c34380a6d8e73206790ffbeab2590e121d01351c4d0c4` |
| `scheduling/TaskExecutingRep.h` | `31ac02721e3a11bc1f107086efc311e6f2caf9ef` | `c1c1372a3b2922f49f81c6c11534bd9e874c21da021fb81276b688461b47bba8` |

The already vendored IR MEL `LICENSE` and `INTENT.md` govern these files unchanged. No
existing vendored byte, the Git evidence, the API catalog or `CATALOG_SHA256` changed.
`docs/upstream-files.sha256.md` gains these 20 rows (inventory 804 → 824). The coverage
`headers` entries flip `vendored` false → true for exactly these files. Both are enforced
byte-for-byte, by `check_doc_source_integrity.py` and by `check_ir_api_coverage.py`. Header
presence is a compilation dependency only.

## C ABI (`AMS_MEL_0.1`, 284 → 289)

| Export | Purpose |
|---|---|
| `ams_mel_ir_scheduling_open` | Advertisement check, single `attachChannel`, concrete-type and capability validation |
| `ams_mel_ir_scheduling_enable` | Explicit inherited `enable()`; idempotent once Enabled |
| `ams_mel_ir_scheduling_get_capabilities` | Owned `ChannelCapability` snapshot (shared converter) |
| `ams_mel_ir_scheduling_close` | Inherited `disable()` if Enable was attempted, then `detachChannel` |
| `ams_mel_ir_channel_from_scheduling` | Weak common Channel view through the existing generic engine |

There is one new opaque owner, `ams_mel_ir_scheduling`, and one configuration record,
`ams_mel_ir_scheduling_config_v1`. The record has the same members, order and layout
(128 bytes, LP64) as the Track, Health and Instrumentation configurations, and reuses the
canonical `ams_mel_uci_id_v1` and `ams_mel_component_location_v1`. All 16 UUID bytes, the
UTF-8 labels, the sensor offsets, the key and the system name are copied. `channel_type` must
be `AMS_MEL_IR_CHANNEL_SCHEDULING` (3); anything else, invalid UTF-8 or a null/non-zero view is
`AMS_MEL_INVALID_ARGUMENT` before any provider call. No task payload, queue, priority or
request owner is added. No existing export, record, signature or enum value changed.
Production symbols stay versioned `AMS_MEL_0.1`.

## Ownership and lifecycle (`native/src/ir_scheduling.cpp`)

`SchedulingState` holds the Session state, the generic `Channel`, the `SchedulingChannel`
view, the lifecycle (Attached, Enabled, Failed, Closed), `enable_attempted`,
`cleanup_started`, a Scheduling-only `requests` count, and the allocation-free emergency
retention root used by Track and C2.

- **Open** checks `Control::getCapabilities()` for Scheduling. If it is absent, Open returns
  `AMS_MEL_INITIALIZATION_FAILED` with no attachment, and the Session stays usable for other
  families. Otherwise Open builds the complete `Config` and calls `attachChannel` exactly once.
  A null result is `AMS_MEL_FACTORY_FAILED`. A wrong concrete type, or a channel capability
  without Scheduling, is detached and returns `AMS_MEL_INITIALIZATION_FAILED`. If that detach
  fails, the complete graph is permanently retained and Open returns `AMS_MEL_PROVIDER_FAILED`.
  A throwing `attachChannel` returns `AMS_MEL_PROVIDER_EXCEPTION`, since nothing is attached.
  A throwing `getCapabilities` on an attached channel is rolled back through detach and
  returns `AMS_MEL_INITIALIZATION_FAILED`; if that rollback detach also fails, the graph is
  retained and Open returns `AMS_MEL_PROVIDER_FAILED`. The owner is published only after every
  check passes.
- **Enable** is explicit. Open returns Attached. Enable on an Enabled owner returns OK without a
  provider call. A refusal or exception marks the owner Failed; later Enable calls do not retry
  the provider.
- **Close** is idempotent for NULL. It stops common-request admission immediately. The
  inherited `disable()` runs at most once, and only if Enable was attempted; there is no
  standalone Disable. `detachChannel` is attempted regardless of the disable result. While
  admitted common requests are pending, Close returns OK, clears the caller's owner and defers
  teardown to the final completion. A failed detach keeps the owner and the whole graph so
  Close can be retried. Provider channel destruction happens while the state still holds the
  Session graph, so provider code is never unloaded under a live dependent.

## Common Channel integration

`common_from_scheduling` is one more `CommonChannelAccess` adapter, using the same weak-state,
admission, claim and capability function pointers as Track. The generic Return/Comms engines
in `common_channel_requests.cpp` are unchanged and reused; there is no Scheduling-specific copy.
A view holds only a weak reference. Admitted requests hold a `CommonRequestClaim` on the
Scheduling state, which owns the graph until the final completion. Accounting is
Scheduling-only: the `accounting` scenario runs a Scheduling and a Track request concurrently
on one Session and shows each count and teardown are independent. Timeout and request Close
are not cancellation.

## Mock provider

`MockSchedulingChannel` derives from the pinned `SchedulingChannel` and defines every pure
virtual. `enable`, `disable`, `getCapabilities`, KeepAlive and CommsTest have scenario
behavior. The capability reports Scheduling, Reserved1, the shared rich fields, a
TaskScheduleDepth of 23/0/`UINT32_MAX`, and the seven Scheduling-related metadata categories.
The eleven Scheduling-specific methods are explicitly unsupported. Each increments a
process-wide counter, records a distinct lifetime event, and returns `Return::NotSupported` or
a ready `ErrorCode::Unsupported` rejection. Those mock answers are not implementations.
`MockControl` publishes Scheduling only for `sched-*` scenarios, and not for
`sched-not-advertised`.

## Negative controls

Every native scenario ends by asserting `mock_scheduling_specific_calls() == 0` and the
absence of all eleven lifetime events. The Ada suite pins the mock and asserts the counter
is unchanged across the whole suite. The coverage regression test proves that a Scheduling
`exec` cannot be claimed `safe_ada` by borrowing the foundation evidence. No task is ever
reported as scheduled.

## Safe Ada (`AMS.MEL.IR.Scheduling`)

| Ada | Native |
|---|---|
| `Scheduling_Config` (private), `Create_Config` | `ams_mel_ir_scheduling_config_v1` built inside `Open` |
| `Scheduling_Channel` (limited private, `Limited_Controlled`) | `ams_mel_ir_scheduling *` (private handle) |
| `Open` | `ams_mel_ir_scheduling_open` |
| `Is_Open` | local handle presence |
| `Enable` | `ams_mel_ir_scheduling_enable` |
| `Capabilities` | `ams_mel_ir_scheduling_get_capabilities` + shared `Capability_Conversion` |
| `As_Channel` | `ams_mel_ir_channel_from_scheduling` → existing `AMS.MEL.IR.Channel.View` |
| `Close`, non-raising `Finalize` | `ams_mel_ir_scheduling_close` |

No native pointer, `Interfaces.C` type or provider-owned string appears in a public value. The
returned `Channel_Capability` owns all of its strings and collections, and it remains valid
after the owner, the Session and the provider library close; native and Ada tests check this.
A detach failure leaves the handle set, so a later `Close` (or finalization) retries without
destroying the retained graph. There is no `Disable`. Any future explicit Disable would need a
separately reviewed lifecycle contract.

## Coverage

- `SchedulingChannel` type: missing → **partial**, with explicit limitations: all 11
  Scheduling-specific operations are missing, disable happens only inside Close, inherited
  buffer and CommsTestRep registration are not exposed, and evidence is mock-only.
- Inherited `SchedulingChannel|Channel::` exposures `enable()`, `getCapabilities() const`,
  `sendKeepAliveRep()` and `send(ChannelCommsTestReq)` move missing → **safe_ada**.
  `disable()` moves missing → **partial** (Close only). Buffer and CommsTestRep registration
  stay **missing**.
- All four `exec`, `send(PrioritizedTrackListCmd)` and the six Scheduling callbacks stay
  **missing**, and so do `AbortTask`, `IRSTScanTask`, `IRSTStareTask`, `IRSTTrackTask` and every
  other Scheduling payload type. Unique safe Ada operations stay at **47**.
- The 20 header entries flip `vendored` false → true. The reviewed-binding file and
  `REVIEW_SHA256` are updated explicitly, `CATALOG_SHA256` is unchanged, and the generated
  report is regenerated.
- `scripts/test_ir_api_coverage.py` adds three controls: the owner-only scope, a rejected
  borrowed `exec` claim, and enforced Scheduling header presence.

## Tests

- Native C11, `test_ir_scheduling_foundation.c` (19 CTest scenarios, one process each):
  - `open-enable-close`: complete Config, type-3 fidelity, Attached → Enabled, idempotence,
    exact disable → detach → destruction → unload order, snapshot after unload.
  - `close-before-enable`: Close without Enable never calls `disable()`.
  - `depth-zero`, `depth-max`: TaskScheduleDepth 0 and `UINT32_MAX` copied verbatim.
  - `parent-first`: Session closes before the owner.
  - `enable-fail`, `enable-throw`: Enable refusal and provider exception.
  - `disable-fail`, `disable-throw`: provider disable refusal and exception.
  - `detach-fail`, `enabled-detach-fail`: graph retained with no unload after Session Close,
    then a retried Close succeeds.
  - `open-failures`: not advertised, null attach, attach throw, wrong concrete type, wrong
    capability, capability throw; the Session stays usable afterwards.
  - `open-detach-fail`, `open-exception-detach-fail`: ownership-uncertain rollback retains the
    provider library.
  - `invalid`: argument validation.
  - `common`: weak view; typed/weak capability equality while Attached and Enabled;
    KeepAlive; CommsTest with cached result; view Close independence; weak-view refusal after
    typed Close.
  - `pending`: admission limit 1; a request survives view, typed and Session Close; deferred
    cleanup runs exactly once after completion; quiescence through completion owners.
  - `accounting`: Scheduling vs Track request counts and teardown stay independent.
  - `send-exception`: a provider send exception releases the claim.
- `ir_scheduling_layout`: C11 layout/signature probe shared by the Python and Rust mirrors.
- `public_channel_scheduling_*` (8 cases): the existing generic public common Channel suite
  now also covers Scheduling (lifecycle, weak, close-first, pending, multiple, snapshot,
  invalid, allocation).
- `test_c_abi.c` and `test_cpp_header.cpp`: exact C and `noexcept` C++ shapes.
- `scheduling_header_compile_probe.cpp`: the pinned closure compiles with the project
  warnings.
- Ada: the focused `ams_mel_ir_scheduling_focused` suite runs in `ams_mel_smoke`, and a
  Scheduling family is added to the generic `AMS_MEL_IR_Channel_Tests.Family_Tests`.
- Python and Rust: private raw mirrors only (`test_private_scheduling_foundation_abi`,
  `scheduling_abi`), and inventories 284 → 289.
- Docs: `docs/tests/test_scheduling_foundation_links.py` (pinned enhancer).

Mutation controls run during development: forced disable without Enable, Scheduling claim
accounting removed, Close deferral removed, and capability validation removed. Each was
detected by a named release-active assertion (native) or a raised `Program_Error` (Ada), and
the source was restored byte-identically.

## Qualification

<!--@@RESULTS@@-->

No real provider, Squall or hardware task scheduling ran. All positive evidence is
deterministic mock-provider evidence.
