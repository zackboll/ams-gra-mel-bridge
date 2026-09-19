# Task 029A review note

Pinned IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`
`TrackChannel.h` includes the deferred `TrackDataUpdate.h` declaration, whose
published closure reaches Boost uBLAS. Task 029A vendors the exact measured
Boost 1.83.0 declaration closure only: the official
`boost_1_83_0.tar.bz2` archive SHA-256 is
`6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e`,
and its official source commit is `564e2ac16907019696cdaba8a93e3588ec596062`.
GCC 14.2 observes 481 Boost headers, Clang 19.1 observes 482, and the union is
483 byte-identical Boost 1.83.0 headers under `native/vendor/boost-1.83.0`.
The exact Boost Software License is preserved there. A build-time dependency
probe rejects any Boost header outside that root; no system Boost path, network
fetch, FetchContent, or ExternalProject is part of normal builds.

Boost 1.83.0 is a project compatibility pin because pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` builds with `boost-devel` and
carries Boost 1.83.0. It is not a claim that IR MEL itself requires that Boost
release. Header presence is not feature support: Task 029A adds no Track API;
TrackDataUpdate and all Track behavior remain unimplemented.

# Task 028 review note

Task 028 expands the reviewed vendored closure by exactly three files from
pinned IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`:
`instrumentation/InstrumentationChannel.h`,
`instrumentation/InstrumentationLevelCmd.h`, and
`instrumentation/InstrumentationReport.h`. A GCC 14.2 dependency probe of
`InstrumentationChannel.h` observes 47 headers, 44 of which were already
vendored, so the closure grows from 91 to 94 unmodified headers. Each new file
is byte-identical to that revision; no vendored content was modified and no
build-time network access was introduced.

The used upstream operations are
`InstrumentationChannel::send(InstrumentationLevelCmd)`, whose published return
is `RequestFor<InstrumentationReport>`, and
`InstrumentationChannel::registerMetadataCallback(std::function<void(Channel&,
const InstrumentationReport* const)>)`. Both members, both data classes, and the
class itself are annotated `@RequiredIfInstrumentation`, that is, conditionally
required only of implementations that support instrumentation. Upstream
`Priority` in `CommonIR_MEL.h` is exactly `Normal` and `Debug` and defines no
MaxExclusive sentinel; the facade therefore invents none.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` does NOT support this
channel through the path this bridge uses. Its `SquallControl::attachChannel`
handles only IRSTImage, CommandAndControl, and HealthAndStatus and returns
`nullptr` for Instrumentation, and its exported `createInstrumentationChannel`
also returns `nullptr`. Squall was not modified. Consequently:

- The mock provider validates positive Instrumentation behavior and full
  payload fidelity.
- Pinned Squall validates clean unsupported-provider behavior only.
- Pinned Squall does NOT provide positive Instrumentation execution evidence.

# Task 027B review note

Task 027B uses the already-vendored `NavigationReport.h`/`ImageChannel.h`
closure from pinned IR MEL `8d9224519f12b44e0b28815755c56a32a28d24a0`; no
vendor file or checksum changed. Pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` implements
`send(NavigationReport)` by copying `report.systemTime` into the response
`systemTime` and setting both `commandID` and `reqId` to zero; when a
`NavigationReportResp` callback is registered it is invoked synchronously
inside `send()` before the future completes. Real-provider validation
therefore proves the provider send path, synchronous callback, future
completion, timestamp propagation, and callback/future correlation; it does
not exercise input fields Squall ignores (state, position, attitude, rates,
speed, acceleration, wander angle, magnetic heading, altitude MSL, or the 18
covariance terms), which remain mock-proven only.

# Task 026 review note

Task 026 uses already-vendored `NavigationReportResp` from pinned IR MEL
`8d9224519f12b44e0b28815755c56a32a28d24a0`; no vendor closure expanded.
Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` accepts registration but emits a
response only from `send(NavigationReport)`. Registration/capability is real-provider proven;
payload fidelity is mock-proven; positive real emission is deferred to Task 027.

# Task 025 review note

Task 025 uses the already-vendored ImageChannel closure, including LineOfSightReport and
LineOfSightEuler. No vendor file or checksum changed. Pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` may emit duplicate Reports while registering
the two LOS callbacks; integration drains until both kinds are observed. No vendor-closure
expansion was required.

# Task 024 review note

Task 024 uses the already-vendored ImageChannel, BadPixelList, BadPixel, and complete
ChannelCapability include closure. No vendor file or checksum changed. Pinned Squall
`b1015728f904c799fa0c07489fce48e78f67845f` reports 320x200 Mono8, one band, and
BadPixelList capability, and invokes the registered BadPixelList callback synchronously
with reported size/count zero and no pixels. No vendor-closure expansion was required.

# Task 021 review note

Task 021 uses the already-vendored `FrameHeader.h` include closure, including
`SensorInertialState.h`, `SensorNavState.h`, `NavError.h`, `Uncertainty.h`, and
`IR_Directional.h`.  A test-enabled compiler build required no additional vendor
files; `native/vendor` remains byte-identical to the pinned declaration closure.

# Task 020 review note

Task 020's GCC dependency probe for `HealthStatusChannel.h` observes 52 vendored
headers and adds exactly six IR MEL headers to the existing closure:
`HealthStatusChannel.h`, `LFStatus.h`, `SubsystemCSCIInfo.h`,
`SubsystemDepInfo.h`, `SubsystemStatusResp.h`, and `Version.h`. Each is
byte-identical to IR MEL commit `8d9224519f12b44e0b28815755c56a32a28d24a0`.
`LFStatus.h` is required by the published include graph but LFStatus itself is
not registered by this feature. The closure now contains 91 headers.

Pinned Squall `b1015728f904c799fa0c07489fce48e78f67845f` attaches
HealthAndStatus and registers all six required callbacks. It periodically emits
MFA_Status, BIT_Status, SubsystemStatusResp, DiscreteStatus, and
MFAStatusDetailed but currently does not emit MFA_SecurityAuditRecord. Its
capability metadata contains MFAStatus, BITStatus, SubsystemStatusResp,
MFAStatusDetailed, and ChannelCommsTestRep. The pinned capability enum has no
DiscreteStatus or MFA_SecurityAuditRecord entries; callback registration, not
capability membership for those two values, establishes support.

# Task 019 review note

Task 019 reviewed the pinned base `Channel`, ChannelCommsTest request/reply,
complete ChannelCapability/BandInfo/navigation declarations, and Squall's
`SquallChannelSupport`/`SquallC2Channel`. Squall supports KeepAlive and CommsTest
while attached, invokes the CommsTest callback synchronously during send, and
reports C2 type plus BITConfiguration, CommandStatus, BITStatus, and
ChannelCommsTestRep metadata capabilities. Vendored declarations are unchanged.

# Task 001/002/003/004 upstream provenance

Verified 2026-09-15 from immutable Git commit objects, not default-branch file
URLs. The selected commits are the candidate submodule revisions recorded by
the umbrella inventory and all have commit message `Release 2026.06.01`.

| Component | Repository | Commit | Tree |
|---|---|---|---|
| Common MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-common-mel` | `f6908437d8fd2f7fb69896f9eb9cfd272d10c439` | `f1f146d79f03a2ca0a59412aaf3d2c6d7dfba40f` |
| IR MEL | `open-arsenal/ams-gra-hello-world-sk-interfaces-ir-mel` | `8d9224519f12b44e0b28815755c56a32a28d24a0` | `bfc72a6a2e1929db9a90c0af78328085fb0ae3ec` |
| AMS Math | `open-arsenal/ams-gra-hello-world-sk-libraries-ams-math` | `00be45190f0e47d268cece8b8c2f8fb58b5418d2` | `7101189c30017cc25f277db39866156e4ab5cf38` |

## Selected closure

`native/vendor` contains unmodified files at those revisions: 28 Common MEL
headers, 55 IR MEL headers, and two AMS Math headers. Task 001's Control probe
selected 44 headers. Task 002's GCC 14.2 dependency probe adds
`image/ImageChannel.h`, `ImageListener.h`, `FrameHeader.h`, and `Buffer.h`, whose
published include graph expands the complete closure to 71 headers. Task 003's
GCC 14.2 `C2Channel.h` dependency probe observes 61 headers, 47 already present,
and adds the exact 14-file C2 closure, expanding the union to 85 headers. Exact file
checksums are recorded in
`upstream-files.sha256.md`. Task 028 adds the exact three-file
Instrumentation closure, expanding the union to 94 headers.

The required published boundary is:

```cpp
extern "C" std::shared_ptr<API_Manager>
getAPI_Manager(const std::string& instance);

extern "C" std::shared_ptr<ams::iface::irmel::Control>
getControl(std::string_view instance, std::shared_ptr<API_Manager> sam);

extern "C" std::shared_ptr<ams::iface::irmel::Buffer>
getBuffer(std::string_view instance, std::shared_ptr<API_Manager> sam);
```

The used virtual operations are `Control::init(const std::string&)` and
`Control::getVersionInfo() const`. `mel::VersionInfo` contains `uint32_t`
API/library values and `std::string` vendor/description values; all are exposed
by the façade without substitution or truncation.

No upstream compiled source is required for this declaration-only slice.
Common MEL and IR MEL are upstream interface targets. The complete upstream IR
CMake project nevertheless adds the whole AMS Math static library, whose five
`.cpp` files require Boost headers. That nominal full-project dependency is not
in this slice's compiler-observed closure. Local build dependencies are Linux
`dlopen`/`dlsym`, CMake 3.20+, and a C++20 compiler/standard library mutually
ABI-compatible with the provider. The test provider is built by the same CMake
configuration/compiler as the adapter.

Task 002 uses `Control::attachChannel`/`detachChannel`,
`Channel::registerBuffer`/`unregisterBuffer`/`enable`/`disable`, `Buffer::init`
and `release`, and `ImageListener::onImage`. `Config` carries complete UCI IDs,
component location, and the listener. `FrameHeader` supplies signed nanosecond
times, dimensions, FOV radians, format/frame/subframe/image metadata, dither,
offsets, and band index. The broad additional headers are compiler-required by
the unmodified published `ImageChannel` interface; optional camera/metadata
operations remain unsupported by this façade.

Task 003 uses `C2Channel::send(ModeCmd)`, whose published return is
`RequestFor<MFA_Mode>` = `std::future<ErrorOr<std::shared_ptr<MFA_Mode>>>`.
`ModeCmd` preserves caller command ID and sets only `MFA_State::Operate` and
`MFA_Mode::TaskSched`; `ScanParam` remains default constructed. The adapter also
uses channel/control capability vectors, enable/disable, attach/detach, and all
nine published `ErrorCode` values. The broad C2 header declares unrelated
commands/callbacks, but the façade does not expose or invoke them.

Task 014 additionally uses required `C2Channel::send(BIT_Command)`, whose return
is `RequestFor<Return>`. The pinned `Return : uint32_t` declaration assigns
Success=0, BadPointer=1, Fail=2, NotSupported=3, and NotImplemented=4. The adapter
sets only the exact `uint32_t` command ID; the default initiateBIT_ID,
cancelBIT_ID, and clearFaultCode vectors remain empty. Pinned Squall accepts this
empty profile and returns `Return::Success`, but rejects non-empty BIT input, so
payload-bearing BIT is intentionally outside this task. No vendored file changed.

Task 017 uses all three `@Required` C2 send overloads at the same immutable IR
revision: `send(ModeCmd)`, `send(BIT_Command)`, and `send(ConfigSetCommand)`.
The façade now preserves complete ScanParam values, all three BIT vectors, and
signed nanosecond/configuration data. Pinned Squall accepts supported general
state transitions, BIT no-op, and empty ConfigSet. It returns normal `Fail` for
payload-bearing BIT and nonempty ConfigSet, and rejects scan scheduling in
Operate because that simulator requires TaskSched. Those are provider behaviors,
not adapter restrictions. Required metadata callbacks remain unimplemented.

## License and notices

Each selected repository supplies Apache License 2.0 `LICENSE` and federal
government `INTENT.md`; both are retained under its `native/vendor` directory
and installed as notices. The three license files share SHA-256
`2f1aa718ddb1a34ed1581e3c57dddd1bb516fb7657a9c4076e8ff95b91d9844d`.

## Open assumptions and upstream issues

- C linkage makes factory names discoverable but does not make their C++
  parameters/results ABI-portable. Matching compiler, standard library,
  exception/RTTI settings, architecture, and compatible declarations remain
  required. Version query is not a universal compatibility probe.
- Upstream does not publish a standalone ABI negotiation operation before the
  first C++ factory call.
- `Control::init` documents no concurrency or rollback details beyond its
  `Return`; task 001 treats every non-`Success` value as failed initialization.
- String encoding is not stated by upstream. This profile validates and
  requires UTF-8 vendor/description values without embedded NUL.
- Provider destructors and shared-pointer deleters must not throw. No callbacks,
  pending requests, channels, buffers, or child resources exist in task 001.
- Task 002's `disable()` declaration does not document callback quiescence.
  Pinned Squall (`b1015728f904c799fa0c07489fce48e78f67845f`) only clears
  `enabled_` and delegates control disable there; `~SquallImageChannel` resets
  its `UdpDataReceiver`. The façade therefore retains listener/buffers/storage
  through channel destruction and then drains its own in-flight callbacks.
- Task 018 uses the pinned required C2 overloads for BIT_Configuration,
  CommandStatus, and BIT_Status. The pinned interface has no unregister overload.
  Pinned Squall stores each closure, synchronously emits an empty default
  BIT_Configuration after its registration, synchronously emits an empty default
  BIT_Status after its registration, and emits CommandStatus inside ModeCmd,
  BIT_Command, and ConfigSetCommand sends. Accepted no-op BIT additionally emits
  an empty BIT_Status. These empty values describe this pinned simulator only;
  rich mock values establish complete nonempty fidelity.
- `Control.h` includes a cyclic and broad declaration graph (including Channel
  declarations and Quaternion) even though task 001 invokes no such feature.
- Common MEL's `CMN_MEL_API_VERSION` string (`"4.0"`), numeric provider
  `VersionInfo`, exact source revisions, and façade ABI 0.1 are distinct values.

Normal configure, build, and tests use only the vendored closure and never
fetch source, provider binaries, credentials, containers, or hardware.

## Task 004 external integration baseline

Task 004 does not vendor or install additional upstream source. Its opt-in
integration dependency is
`https://github.com/open-arsenal/ams-gra-hello-world-sk-sensors-squall` at exact
commit `b1015728f904c799fa0c07489fce48e78f67845f`. The harness accepts only an
explicit `SQUALL_SOURCE_DIR`, validates `HEAD` and repository identity, and does
not mutate that checkout.

Relevant immutable files include `README.md`, `compose.yaml`,
`compose.build.yaml`, `Containerfile`, `config/couloir.toml`,
`config/optical-simulated.toml`, `config/squall-ir-mel-profile.json`,
`tests/mel-boundary-e2e/{run.sh,run.py,compose.yaml}`, and
`interfaces/squall-ir-mel-impl/{CMakeLists.txt,src/*}`. Upstream builds the C++20
provider as `build-ir/libsquall_ir_mel.so`; its MEL consumer image installs it as
`/usr/lib64/libsquall_ir_mel.so`. The local harness extracts that binary to an
ignored build directory and passes its path to `ams_mel_session_open`; neither
`ams_mel_c` nor either integration executable links directly to it.

The baseline uses Squall's E2E simulated checkerboard (Mono8, 320x200, 30 FPS),
host-network optical and Couloir services, and a generated profile with
`data_host=127.0.0.1`, a generated loopback Couloir control address, and a unique
client ID. This records one
integration target, not generic compatibility with every GRA provider or C++
runtime combination.
