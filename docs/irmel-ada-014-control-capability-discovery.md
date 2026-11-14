# IRMEL-ADA-014 — Control capability discovery

## Scope and source evidence

Base: freshly fetched `main`, `648d08bfc5d3c8a92c52680f5b1c42bbd515a1e8`.
Branch: `feature/irmel-ada-014-control-capability-discovery`.

The pinned IR source is commit `8d9224519f12b44e0b28815755c56a32a28d24a0`:
`native/vendor/ir-mel/include/irmel/library/irmel-types/Control.h` declares
`const std::vector<ChannelCapability>& getCapabilities() const`. The immutable
catalog entry is
`ir:operation:ams::iface::irmel::Control::getCapabilities() const` in
`docs/reference/irmel-upstream-api.json`. The capability getter inventory and
field evidence already describe the complete `ChannelCapability` representation.
No upstream headers, provider implementation, or existing ABI records change.
The separate test mock gains discovery-only scenarios.

An application must discover advertised identities before deciding which
channels to attach. Discovery now exposes the entire Control list, rather than
the bridge's private search for a particular attachable family. Advertisement
does not establish that the sensor implements a working operation, that an
attachment will succeed, or that acquisition is configured.

## Public Ada interface and implementation

`AMS.MEL.IR.Discovery.Capabilities (Session)` returns a
`Capability_Vectors.Vector` indexed by `Positive`, with complete
`AMS.MEL.IR.Channel.Channel_Capability` values in upstream enumeration order.
An empty vector is successful discovery. The public interface contains no raw
address, native owner, or C representation. Existing public APIs are unchanged.

`ada/src/ams-mel-ir-discovery.adb` uses the private imports in
`ada/src/ams-mel_c_api.ads` and the existing
`AMS.MEL.IR.Capability_Conversion.To_Channel_Capability`. There is no second Ada
field mapping. The native list owns stable C views while each value is copied;
the returned vector instead owns Ada strings and nested containers. Every
temporary native entry and list closes on success or exception, including an
Ada allocation/representation exception during conversion. Native/provider
failures raise `AMS.MEL.Provider_Error` with the established diagnostic buffer;
Ada exceptions propagate after cleanup.

For example, an Ada client can copy discovery results while its Session is
open, close the Session, and still inspect `Channel.Channel_ID (Values (1))`,
`Channel.Channel_Type_At (Values (1), 1)` and every nested getter. Applications
must check `Values.Is_Empty` and each collection's count before indexing; no
particular provider count or ordering beyond the provider's own list is assumed.

## Native ABI and lifetime contract

Exactly four additive exports are introduced under `AMS_MEL_0.1`:

- `ams_mel_ir_capability_list_create`
- `ams_mel_ir_capability_list_count`
- `ams_mel_ir_capability_list_get`
- `ams_mel_ir_capability_list_close`

Production exports increase from **295 to 299**, with no removals or record
changes. The new opaque `ams_mel_ir_capability_list` does not cross the ABI by
value. Native indices are zero-based; owner output slots must initially be
null. Invalid input leaves an initially null owner output null. A non-null
output slot is rejected without consuming its existing owner. Count failure
sets a supplied count to zero. Close clears its slot and is idempotent for an
already-null owner. As with existing opaque handles, arbitrary forged/stale
addresses cannot be validated: callers supply live handles of the proper type.

`native/src/ir_channel.cpp` reuses **one** `copy_capability(const
irmel::ChannelCapability&, CapabilityData&)` implementation for attached-channel
and Control snapshots. Immutable bridge-owned data has stable view storage;
retrieving an entry allocates an independent opaque owner sharing immutable
bridge data, not STL storage borrowed from the provider. Neither owner retains
`SessionState`, the Control, API manager, library, or provider deleter. Entries
can outlive their list and both can outlive provider unload. Published native
views remain borrowed from their entry owner; Ada never returns these views.

Creation publishes the owner only after all entries validate and copy.
Automatic ownership destroys a partially copied prefix on malformed data or
exception. Allocation failure returns `AMS_MEL_INTERNAL_ERROR`; malformed
capabilities return `AMS_MEL_PROVIDER_FAILED`; ordinary/unknown provider
exceptions return `AMS_MEL_PROVIDER_EXCEPTION`. Diagnostic construction at the
boundary is non-allocating. No channel attachment or callback registration is
performed.

Image dimensions, bit depth, band count, row pitch, buffer size and image size
are copied independently without normalization. All-zero layouts are valid
unconfigured advertisements, not an excuse to infer Mono8 dimensions. Existing
enum/text validation remains in force. Nested native map/set iteration is
preserved by the existing converter; no new cross-field physical validation is
invented.

Discovery is synchronous. Callers must serialize it against Session closure
and other provider operations. Access to native list/entry handles must be
serialized against closing those handles. No claim is made for discovery
racing closure, provider self-mutation, callback quiescence, or cancellation.

## Coverage delta

`docs/irmel-ada-coverage.json` changes only:

- `Control::getCapabilities() const`: **partial → safe_ada**, with direct native,
  public Ada, compiled-C and Ada regression evidence and explicit limitations.
- `Control` type's `getCapabilities` representation: **see_notes → represented**,
  pointing to the public discovery operation. The Control type remains partial.

The reviewed classification ceiling and its checker digest are updated; its
historical `source_sha256` identities are preserved as required by the current
schema. The generated `docs/irmel-ada-coverage.md` is regenerated. Across the
unchanged **148 operations**, safe Ada increases **48 → 49** and partial falls
**13 → 12**; missing **18**, native-only **0**, provider-only **69** are unchanged.
Outgoing/metadata counts, inherited exposures and all other type classifications
are unchanged. Audit regressions now require discovery's evidence/export and
continue to reject generic attachment/detachment completeness claims.

## Qualification

The mock advertises four independently identified channels in this exact order:
CommandAndControl, HealthAndStatus, IRSTImage, StackedImage. A zero-layout mode
models the unconfigured single-detector advertisement. A rich mode exercises
multiple channel types per entry, UTF-8 labels, all UUID bytes, independent image
scalars, platform/location, sensor types, metadata sets, nested spectral bands,
navigation frames, flags and scheduling depth. Expected values are independently
specified in C and Ada tests, not obtained from the converter.

`native/tests/test_ir_discovery.c` is compiled as strict C11 and checks repeated
discovery, order, empty lists, identity, native list/entry independence, unload,
invalid arguments/indices/output ownership and malformed/throwing providers.
`native/tests/test_ir_discovery_allocation.cpp` interposes allocation and sweeps
every allocation site through a complete rich list, including failure after a
copied prefix and entry-owner allocation failure. No production failpoint or new
test-only export is needed. Discovery mock scenarios abort on any attachment;
there is therefore no channel on which discovery can register callbacks.

`ada/tests/src/ams_mel_ir_discovery_tests.adb` runs from the ordinary Ada smoke
suite. It checks repeated discovery, empty/closed Sessions, malformed/throwing
providers and complete value retention after closure. A test-only `RTLD_NOLOAD`
observer verifies actual provider unload before nested Ada values are inspected.
This loader observation is not part of the public Ada API.
The discovery suite runs before existing failure suites intentionally retain
unsafe-to-release provider graphs, so those negative controls cannot mask its
unload assertion.

Executed on Linux with GCC 14.2.0, Alire 2.1.1, pinned GNAT 16.1.0 and
GPRbuild 26.0.1. `gprbuild` is not on the login PATH, so Make qualification uses
`alr -C ada/tests exec -- make -C /home/zboll/git/ams-gra-mel-bridge <target>`.
This supplies the pinned toolchain without global environment changes.

| Command | Result |
| --- | --- |
| `make format-ada` | Passed; GNATformat 26.0.0 applied. |
| `make check-ada-format` | Passed after the final Ada change. |
| `make test-native` | Passed: 525/525 CTest cases, including both discovery tests. |
| `alr -C ada/tests exec -- make -C /home/zboll/git/ams-gra-mel-bridge test-ada` | Passed: complete Ada smoke/contract suite, including discovery and actual unload. |
| `alr -C ada/tests exec -- make -C /home/zboll/git/ams-gra-mel-bridge check` | Passed: production/test build-tree isolation, 525 native tests, formatting, 55 audit controls, complete Ada suite, newline and whitespace checks. |
| `alr -C ada build` | Passed. |
| `alr -C ada/tests run` | Passed: complete smoke/contract executable, including discovery; exit 0. |
| `make check-ir-api-coverage` | Passed: offline audit and 55/55 regression controls. |
| `python3 scripts/check_ir_api_coverage.py --write-report` | Report regenerated; subsequent read-only audit passed. |
| `python3 scripts/check_final_newlines.py` and `git diff --check` | Passed. |
| `nm -D --defined-only native/build/lib/libams_mel_c.so` compared with base `native/src/exports.map` | Exactly 295 → 299 exports under `AMS_MEL_0.1`; four additions listed above, no removals. |

Additional release-assertion control executed successfully:

```sh
cc -std=c11 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -I /home/zboll/git/ams-gra-mel-bridge/native/include \
  '-DAMS_MEL_TEST_MOCK_PROVIDER="/home/zboll/git/ams-gra-mel-bridge/native/build-tests/test-providers/libmock_ir_provider.so"' \
  /home/zboll/git/ams-gra-mel-bridge/native/tests/test_ir_discovery.c \
  -L /home/zboll/git/ams-gra-mel-bridge/native/build-tests/lib \
  -Wl,-rpath,/home/zboll/git/ams-gra-mel-bridge/native/build-tests/lib \
  -lams_mel_c -ldl -o /tmp/irmel014-c11-ndebug
/tmp/irmel014-c11-ndebug
```

First-party native compilation uses `-Wall -Wextra -Wpedantic -Werror`; Ada
uses `-gnatwa -gnatwe` and runtime assertions/validity checks. The existing
import-only `ams_mel_c.gpr` emits its project-level "no sources of language C"
warning during formatting; no first-party source warnings are suppressed.

Intermediate qualification caught and corrected incorrect private Ada import
spellings, nine stale global safe-operation count expectations, and discovery's
initial position after intentionally retained provider-failure tests. The final
suite keeps the strict unload assertion and runs discovery first. A standalone
C11 link attempt during a build-tree reset had no library available; rerunning
after the build completed passed. No unavailable step is counted as success.

## Limitations and exclusions

Ordinary tests use only the separate mock DSO, without downloads, credentials,
hardware, containers or Squall. No local `minimal-mel-example` checkout was found
in the inspected workspace locations; no optional real-provider integration is
claimed. This is consumer-binding evidence, not sensor qualification.

Metadata-only Image attachment, StackedImage ownership, application-managed
Buffers, generic buffer registration/unregistration, expanded common metadata
registrations, acquisition and new sample formats remain separate work. There
are no unresolved upstream semantics blocking value-only discovery; the existing
physical meaning/unit limitations of individual upstream getters are unchanged.
Historical `docs/coverage.md` and unrelated language documentation are untouched.
