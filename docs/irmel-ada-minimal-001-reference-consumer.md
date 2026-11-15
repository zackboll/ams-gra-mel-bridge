# IRMEL-ADA-MINIMAL-001 — safe Ada reference consumer

## Scope and provenance

This is application interoperability qualification, **not a new binding
operation** or hardware/conformance certification. The standalone executable
uses only public safe Ada packages. It contains no C imports, native handles,
unchecked conversions, provider-private interfaces, or C++ glue.

Initial freshly fetched bridge main:
`34cc31f6a5dd183e2197d61dd4d9c62d97ab6ded`, tree
`37924775b8a544a58a365a9e61547b674327cee4`. Work branch:
`feature/irmel-ada-minimal-001-reference-consumer`. Worktrees and open PRs
#221, #224, #228, and #229 were inspected before editing; none was used as a
branch base or modified. PR #227 was confirmed merged. Later mainline
integration and final commit/tree identities are recorded in the delivery
section/PR, without putting a self-referential final commit hash in this file.

Reference repository: <https://github.com/zackboll/minimal-mel-example>.
Qualification pin: `cf22a80ec98398eb361df758ada7bfb4024710a3`, tree
`7e560e7afc8ee5538501ad40cef2a862a35b076f`. No usable checkout was found in
the initial local search. A separate checkout was cloned at
`/tmp/irmel-ada-minimal-001-reference`, detached at the pin, without changing
its tracked sources. Reviewed the exact pinned `examples/minimal_consumer.cpp`,
`src/provider.cpp`, `docs/consumer.md`, `docs/required-interfaces.md`, and
`CMakeLists.txt`, as well as the bridge policies, coverage schema/inventory,
Ada 018–021 reports, and Rust 014 report. The earlier Rust report is not
used as full-parity evidence.

No production ABI, export map, public Ada specification, implementation,
callback/release lifecycle, upstream declarations, Python/Rust API, structured
coverage inventory, generated coverage report, or historical coverage changes
belong to this task. Existing counts remain **331 production C exports** and
**57 unique safe-Ada upstream operations**. Integration tests do not increase
that count.

## Running

The paths in this example are absolute; replace the checkout path with a clean
local checkout already at the pin:

```sh
cd /home/zboll/git/ams-gra-mel-bridge
MINIMAL_MEL_REFERENCE_DIR=/tmp/irmel-ada-minimal-001-reference \
MINIMAL_MEL_LOG_DIR=/tmp/irmel-minimal-harness \
alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge test-minimal-ir-ada

/home/zboll/git/ams-gra-mel-bridge/ada/tests/bin/ams_mel_ir_minimal_consumer \
  /tmp/irmel-ada-minimal-001-reference/build/libminimal_ir_mel_provider.so
```

The second command requires the production facade on its embedded library
search path (or `LD_LIBRARY_PATH` set to the absolute `native/build/lib`).
Do not inherit Alire's contract-test facade override for this real run.

`MINIMAL_MEL_REFERENCE_DIR` chooses a clean checkout and never changes its
revision. Without it, the opt-in harness checks known local locations, then
clones the approved public repository into a separate temporary directory and
checks out the pin. `MINIMAL_MEL_LOG_DIR` selects persistent transcripts; its
default is a new absolute `/tmp` directory. `MINIMAL_MEL_PROVIDER` can select
an absolute DSO path, but its bytes must equal the corresponding qualified
Debug/Release build's DSO. Usually leave this unset, especially for a two-build
run. A missing/mismatched provider fails, never skips. This is not a production
dependency and no provider sources/binaries are committed here.

The harness runs the reference's four CTests before the Ada application in
each configuration, verifies pin/cleanliness and byte identity of the complete
reference IR/Common/Math declaration closure against bridge vendor headers,
builds the production facade with CMake, and builds the Ada application with
GPRbuild. The bridge contains additional optional headers absent from the
reference's smaller closure; their absence is not an ABI difference.
It preserves configure/build/test logs, stdout, stderr, exit codes, cache and
compiler information, and provider/consumer SHA-256 identities. Native and Ada
shared output directories are used sequentially, never concurrently.

The executable is compiled with the ordinary Ada test project, but ordinary
`make test-ada`/`make check` never run it with an external provider or download
the reference. The pure negative-check executable is provider-free and runs
in ordinary `make test-ada`. CLI behavior is `--help` 0, missing/extra args 2,
loading/contract failure 1, full parity plus successful cleanup 0. The supplied
provider path is passed unchanged to `AMS.MEL.Open`, instance
`minimal-consumer`; there is no mock-provider fallback.

## C++ → safe Ada comparison

| Reference consumer operation | Ada operation / verified behavior | Classification |
| --- | --- | --- |
| Resolve `getAPI_Manager`, `getControl`; `Control::init` | `AMS.MEL.Open` loads the supplied DSO and initializes its actual Session | EXERCISED |
| `Control::getVersionInfo` | `Query_Provider_Version`; vendor `minimal-mel-example`, API 4, revision 1, nonempty description | EXERCISED |
| `Control::getCapabilities` | `IR.Discovery.Capabilities`; exactly four single-category descriptors, one of each required category, distinct UUIDs | EXERCISED |
| Select each channel's UCI UUID/label | Copy discovered `Channel_ID`; compare attached UUID and label with selection | EXERCISED |
| Platform/location configuration | Reconstruct with `Create_UCI_ID` / `Create_Component_Location` from reported platform/location, no invented pose | SAFE_EQUIVALENT |
| Unconfigured image layout | Require all seven reported scalars zero before and after attachment | EXERCISED |
| `attachChannel(C2)` | `C2.Open(Create_Config(...))`, keep limited typed owner | EXERCISED |
| `attachChannel(HealthAndStatus)` | `Health_Status.Open(Create_Config(...))` | EXERCISED |
| `attachChannel(IRSTImage)` with ImageListener | `Open_Image_Control`; existing listener plus optional `Image.Checkouts.Open` before Start, no acquisition pool | SAFE_EQUIVALENT |
| `attachChannel(StackedImage)` | `Stacked_Image.Open(Create_Config(...))` | EXERCISED |
| Each attached `Channel::getCapabilities` | `As_Channel` views and `Channel.Capabilities`, category/UUID/label/layout checked | EXERCISED |
| C2 BIT_Configuration / CommandStatus / BIT_Status registration | `C2.Metadata.Open(C2,16)` (three overloads) | EXERCISED |
| C2 common ChannelCommsTestRep registration | `C2.Metadata.Enable_Comms_Test_Events`, not duplicate common owner | EXERCISED |
| Six Health registrations | `Health_Status.Metadata.Open(Health,16)`, including DiscreteStatus and security audit despite discovery-enum omission | EXERCISED |
| Four Image registrations | `Image.Metadata.Open(Image,16)`: BadPixelList, LineOfSightReport, LineOfSightEuler, NavigationReportResp | EXERCISED |
| Three other common communications registrations | `Channel.Metadata.Open` on Health/Image/Stacked views, capacity 16 each | EXERCISED |
| Four `getBuffer` / `Buffer::init` calls | Four independent `Buffer.Open(Session,16,context)` owners, context 1/2/3/4; safe byte copies set 0x5A | SAFE_EQUIVALENT |
| Buffer size/context/address/image getters | `Buffer.Properties`: original/provider size 16, exact signed context, owned-address equality, Null_Image classification | SAFE_EQUIVALENT |
| `setFlags({})`, `addFlag(Host)`, `getFlags` | `Set_Flags([])`, `Add_Flag(Host)`, exact single-entry `Flags` vector | EXERCISED |
| Unrestricted initialized `Buffer::release()` expecting Fail | Intentionally unavailable; no fabricated checkout or release. Verify zero image callbacks/checkouts instead | INTENTIONALLY_EXCLUDED |
| Four `registerBuffer` calls | `Channel.Register_Buffer` while Attached/disabled, exact original owner and channel | EXERCISED |
| Four `enable` calls | C2/Health/Stacked `Enable`, control-only Image `Start` | EXERCISED |
| Four `sendKeepAliveRep` | Four independent common requests; Wait 5000, successful terminal outcome **and** Return_Success payload, explicit request Close | EXERCISED |
| Four direct CommsTest sends | `Channel.Submit_Comms_Test(destination view, numeric category, command, request)` | EXERCISED |
| Four C2-routed CommsTest sends | Same common safe API on the **C2 view**, addressed to each destination's numeric category (not UUID) | EXERCISED |
| Eight future correlations | Eight separate limited requests/Wait results, independent status and both-ID checks, explicit Close | EXERCISED |
| Eight callback correlations | Two actual dequeues from **each** destination's own queue, both IDs checked independently of future results | EXERCISED |
| Native callback Channel pointer identity | Safe API guarantees queue association, not an independently visible native pointer comparison | SAFE_EQUIVALENT |
| `BIT_Command`, command 300, empty action | `Submit_BIT_No_Op`; completed successful future with Not_Implemented, not BIT execution | SAFE_EQUIVALENT |
| `ConfigSetCommand`, command 301, time 0, empty payload | `Submit_Config_Set(C2,301,0,"")`; successful future with Not_Implemented | EXERCISED |
| `ModeCmd`, command 302, Standby/Unused | `Submit_Mode`; explicit Invalid_State rejection plus nonempty description; neutral scan defaults | EXERCISED |
| `NavigationReport`, NotSet/neutral fields | Complete safe record with zero time/values/covariance and Not_Set, explicit Invalid_State rejection/description | EXERCISED |
| Reject unsolicited sensor telemetry / images | Counters, nonblocking Health/Image/C2/common queue polls, and checkout poll; no decoding/pixel access | SAFE_EQUIVALENT |
| Four `disable` calls | Common `Channel.Disable` while typed owners remain alive | EXERCISED |
| Four exact `unregisterBuffer` calls / unchanged backing bytes | Exact original owner/view, byte copies checked after unregister before Buffer Close | EXERCISED |
| Destroy metadata consumers | Explicit Close/deactivation, **not** upstream callback unregistration | SAFE_EQUIVALENT |
| Four `detachChannel` calls | Typed channel Close after registration obligations removed; each explicit Close must succeed | EXERCISED |
| Destroy common views, Session | Explicit Close after typed detach; no cancellation or immediate DSO-unmapping claim | SAFE_EQUIVALENT |

No operation is classified BLOCKED: the first actual reference-provider run
demonstrated that existing safe APIs can represent this required slice.

## Real-provider observed evidence

The initial Debug run exited **0** against the actual pinned DSO, using the
production facade, not the bridge mock. Its transcript is
`/tmp/irmel-minimal-first-run.log`. This initial run is not a substitute for
the complete Debug/Release harness qualification recorded below.

All **17** registrations returned successfully before any Enable/Start:
C2 3 base + 1 common; Health 6 typed + 1 common; Image 4 typed + 1 common;
Stacked 1 common. A successful registration is not sensor telemetry receipt.

| Destination / numeric route | Direct future | C2-routed future | Callback #1 (own queue) | Callback #2 (own queue) |
| --- | --- | --- | --- | --- |
| C2 / 2 | Success, 100/200 | Success, 100/200 | 100/200 | 100/200 |
| Health / 4 | Success, 101/201 | Success, 101/201 | 101/201 | 101/201 |
| Image / 1 | Success, 102/202 | Success, 102/202 | 102/202 | 102/202 |
| StackedImage / 6 | Success, 103/203 | Success, 103/203 | 103/203 | 103/203 |

Each row's queue reports exactly two received callbacks, zero queue-full drops,
zero malformed/unsupported reports, and an empty final zero-time poll. C2's
shared metadata queue is also checked for unexpected non-comms event kinds.
No callback is inferred from a future or reused across queues.

Four KeepAlive requests completed with Return_Success. BIT 300 and ConfigSet
301 completed with Return_NotImplemented; Mode 302 and neutral NotSet
NavigationReport were rejected with InvalidState and the provider's nonempty
`required sensor operation is a stub; hardware is not connected` explanation.
Every request has a finite 5000 ms Wait. A timeout fails; it never cancels work.

Four independently created Buffers each retained capacity/provider size 16,
context 1, 2, 3, or 4, matching owned host allocation, null image address, exact
single Host flag, and sixteen 0x5A bytes. Safe Buffer reads/properties are
intentionally blocked while registered. Original bytes are checked before
registration and after exact successful unregister; no extra provider Buffer
is created for validation.

Image/Health telemetry counters remained zero. Nonblocking telemetry and
checkout polls raised the expected Timeout_Error (no data), not provider
failure. Control-only image counters and checkout diagnostics showed no image
callback, accepted checkout, overflow, malformed frame, or uncertain release.
No acquisition layout is inferred from seven zeros. No fabricated 1×1 Mono8
pool, navigation fix, alignment/covariance quality, or telemetry is introduced.

Normal cleanup printed 24 successful steps: four common Disable operations;
four exact unregister/storage-check/Buffer-Close groups; C2/Health/Image and
three common metadata Close operations; checkout receiver Close; four typed
channel Close/detach operations; four weak view Close operations; Session
Close. The summary is printed only after all those operations succeed and the
complete evidence is checked. Failure paths perform best-effort cleanup and
return 1, without claiming normal teardown. Partial declaration elaboration
uses the existing non-raising limited-owner finalization fallback. Metadata
Close is not callback unregistration; Disable is not quiescence. Session Close
does not cancel work and loader policy may retain the DSO until process exit.

## Negative controls

The CLI harness checks real executable exit codes for help/missing/extra args,
a nonexistent absolute DSO, and an invalid ABI library (the executable itself,
which cannot be loaded as a provider). The pure safe-Ada negative executable
uses the same application validation procedures as the consumer and checks
14 controls: seven callbacks, futures without callbacks, unexpected telemetry
or images, each of four wrong stub outcomes, incomplete cleanup, only seven
futures, only sixteen registrations, wrong provider identity, missing category,
duplicate UUID, and incorrect command/request correlation. These are focused
pure negative tests, **not injected real-provider behavior**. Existing bridge
mock tests provide additional lifecycle coverage but are not positive parity
evidence. No reference-provider sources were modified for negative testing.

## Qualification ledger

The initial pinned reference Debug build ran exactly:

```sh
cmake -S /tmp/irmel-ada-minimal-001-reference -B /tmp/irmel-ada-minimal-001-reference/build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/irmel-ada-minimal-001-reference/build --parallel 2
ctest --test-dir /tmp/irmel-ada-minimal-001-reference/build --output-on-failure
```

Observed: **4/4 passed**, provider_contract, dynamic_load,
consumer_required_api, upstream_declarations. First standalone Ada build
passed the project's `-gnatwa -gnatwe` warning policy and the first real run
and pure negative executable both returned 0. An initial compile attempted to
mix the historical C2.Common and canonical common request types; corrected to
the canonical common API on C2's weak view. A redundant use clause was removed
to satisfy warnings-as-errors. An initial harness closure comparison mistakenly
required the reference to include bridge-only optional headers; corrected to
compare **all reference headers** against the bridge, with zero differing
shared declarations. None of these preliminary attempts is reported as a
passing final qualification.

Final qualification results and delivery disposition follow once actual runs
finish; planned/ongoing checks are not passing evidence.

## Limitations

Qualification is for this pinned stub, compatible native C++ ABIs, and Linux
loading. It establishes no detector transport, physical sensor action,
hardware pose, generic image decoding, unrestricted Buffer release, or full
AMS-GRA conformance. Queue association is the supported safe callback identity
guarantee, not independent native pointer identity. Finite future waits do not
bound provider code that blocks synchronously during submission. Exceptional
ownership failures can conservatively retain provider graphs until exit.
