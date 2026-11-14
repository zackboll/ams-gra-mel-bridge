# IRMEL-ADA-015 — Image commands and metadata without acquisition

## Base and scope

Explicitly stacked on PR #183, whose exact feature HEAD is
`6020ced3c2214977f9ad583ecca47319c89ab870`. PR #183 was open when this
branch was created after fetching its feature branch and main. It was not
merged or mixed into another base. Delivery branch:
`feature/irmel-ada-015-image-control-only`.

An ImageChannel is a command/metadata interface even before the detector has
an acquisition layout. Rejecting an unconfigured layout at attachment time
previously made these independent required interfaces inaccessible. This
change does not infer an image format from channel enable or manufacture
sensor data. The ordinary acquisition path still requires Mono/8/1 at Open
and validates dimensions, format, metadata and registered spans on delivery.

## Public contract and ownership

`AMS.MEL.IR.Create_Image_Control_Config` copies channel identity, platform
identity and component location. `Open_Image_Control` returns the existing
limited `Image_Stream`, not a second navigation/metadata implementation.
Discover the identity using `AMS.MEL.IR.Discovery.Capabilities`, then use
`Image.Capabilities`, `Image.As_Channel`, `Image.Submit_Navigation_Report` and
`Image.Metadata.Open` normally. Navigation `Wait` preserves an explicit MEL
`InvalidState` as `Rejected` / `Invalid_State` with the provider description;
it is not converted into a bridge exception. Callback reports do not complete
command futures.

`Start` is the existing lifecycle enable operation. On this owner it only
enables the channel, without resolving/calling `getBuffer`, allocating sample
storage, initializing buffers or registering acquisition buffers. It is
idempotent when enabled. The metadata queue capacity is selected separately
when opening metadata, not invented as an acquisition configuration parameter.

Legacy Receive, full-frame Receive and Acquire_Frame are unavailable even
after Start: their native paths return `AMS_MEL_INITIALIZATION_FAILED` with
`image receive unavailable on control-only attachment`; safe Ada raises
`AMS.MEL.Provider_Error` with that diagnostic. No lease or pixel span is
published. This is not a new supported sample encoding.

Stop and Close reuse Image logical-stop/deferred-teardown machinery. Close
requests and metadata explicitly, then the Image owner and Session; closing
parents first is also supported by native graph retention. Pending requests
are not cancelled and retain Session/library/channel until completion. There
is no upstream metadata unregister or promise that disable establishes
quiescence. Callbacks remain safe through channel destruction and the existing
in-flight drain. Lifecycle calls remain externally serialized.

Before attach can invoke a listener synchronously, the control owner installs
two empty callback ownership slots, plus the existing release executor's
deferred/uncertain ownership slots. These are shared-pointer bookkeeping,
not image buffers or a sample pool. Start does not rebuild them: attachment
may already have exposed callbacks. Unexpected images are rejected without
reading FrameHeader samples or querying Buffer addresses, size, context or
flags, and use the existing unlocked, serialized, reentry-safe release engine.
Release failure/exception retains the exact checkout and complete provider
graph permanently rather than retrying release or unloading code. Excess
unsolicited concurrent deliveries use the existing fail-safe retention path;
this is defensive containment, not support for acquisition on this owner.

Provider methods are never invoked with the callback lifecycle mutex held.
Attachment exceptions/null results in the new mode conservatively retain the
graph because they cannot establish that the provider discarded the listener.
An incompatible concrete channel is detached; uncertain rollback retains its
owner. Failed Close/detach retains a retryable public owner. A successful retry
still reports the poisoned lifecycle rather than hiding the earlier failure.
Provider exceptions on enable use existing failure/cleanup semantics.

## ABI and coverage delta

- ABI remains 0.1 / `AMS_MEL_0.1`. Exactly one new export:
  `ams_mel_ir_image_control_open` (299 → 300 production exports).
- New identity-only C11 record: `ams_mel_ir_image_control_config_v1`.
  Existing record layouts, exports and signatures are unchanged.
- No new upstream overload is claimed: **49 safe Ada operations remain 49**.
  Control attach, navigation and four required Image metadata entries gain
  control-only evidence; the image-profile inventory gains this attachment
  mode. Unsupported acquisition formats and StackedImage remain missing.
- Authoritative mapping and reviewed binding ceiling updated together;
  report regenerated with `python3 scripts/check_ir_api_coverage.py --write-report`.
  Catalog/pins and historical source digests are unchanged.

## Qualification evidence

The new compiled-C contract uses the existing separately compiled mock DSO.
Cases run in isolated child processes so deliberate retention cannot mask
unload evidence. It covers zero layouts and nested capability preservation,
all four synchronous metadata registrations, navigation InvalidState and
callback reentry, KeepAlive/CommsTest, no factory/init/register calls,
explicit receive failure, callbacks during channel destruction, deterministic
held navigation across parent Close, unexpected delivery and synchronous
release reentry, failed/throwing release, attach-time callbacks and exceptions,
wrong concrete channel, uncertain rollback and failed detach/retry.

The new Ada contract discovers the unconfigured Image identity, compares the
complete discovered and attached capability snapshots, consumes all four
metadata families, checks explicit navigation rejection and common requests,
enables without acquisition, rejects Receive and frame lease access, and
closes parents with pending navigation and registered metadata. Existing
Mono8, unsupported-format, Image metadata/navigation and cleanup suites remain
part of the full qualification.

Commands run from the repository root (Ada commands use the existing Alire
GNAT 16.1.0 / GPRbuild 26.0.1 environment; native C/C++ is GNU 14.2.0):

| Command | Result |
| --- | --- |
| `make format-ada` | Passed; GNATformat 26.0.0. |
| `make check-ada-format` | Passed. |
| `make test-native` | Passed, 526/526 contracts. |
| `alr -C ada/tests exec -- sh -c 'cd /home/zboll/git/ams-gra-mel-bridge && make test-ada'` | Passed, including the new control-only contract and existing Image metadata/navigation/lease suites. |
| `make check-ir-api-coverage` | Passed, 56 regression controls; 127 headers / 148 operations. |
| `alr -C ada/tests exec -- sh -c 'cd /home/zboll/git/ams-gra-mel-bridge && make check'` | Passed: build-tree isolation in both orders, 526/526 native contracts, Ada formatting, 56 coverage controls, full Ada suite and final newline/whitespace checks. |
| `alr -C ada/tests run` | Passed, including control-only, discovery, existing Image contracts and the full smoke suite (exit 0). |
| `alr -C ada build` | Passed on the final serialized run (exit 0). |
| `python3 -W error -m unittest discover -s scripts -p test_doc_source_integrity.py -v` | Passed, 18 controls. |
| `python3 scripts/check_final_newlines.py` and `git diff --check` | Passed. |

Additional Release qualification, with `-O3 -DNDEBUG` and warnings as errors:

```sh
cmake -S /home/zboll/git/ams-gra-mel-bridge/native \
  -B /tmp/irmel-015-release -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/irmel-015-release --parallel 2 --target \
  test_ir_image_control test_ir_stream test_ir_image_metadata test_ir_navigation
ctest --test-dir /tmp/irmel-015-release --output-on-failure \
  -R '^(ir_image_control_contract|ir_stream_contract|ir_image_metadata_contract|ir_navigation_contract)$'
ctest --test-dir /tmp/irmel-015-release --output-on-failure \
  -R '^ir_image_control_contract$' --repeat until-fail:50
```

All four Release contracts passed; the control-only contract passed 50/50
repetitions. The checks use active C condition checks, not disabled `assert`.
An explicit `cc -std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only`
compilation of the new contract with the public include directory also passed.
`nm -D --defined-only` on the production facade matched the base export set
plus exactly `ams_mel_ir_image_control_open@@AMS_MEL_0.1`; all existing C
`typedef struct` definitions compared byte-identically to the feature base.

During development, an overly broad attach-exception retention change was
caught by the existing stream unload test and limited to the new control-only
mode. A test's initial zero-based C metadata-tag assumption and an incorrectly
qualified Ada exception name were corrected, not hidden by weaker gates.
Subsequent native and Ada runs passed. Existing GPR project warnings that its
native import contains no C sources remain informational; first-party compiled
C/C++ and Ada sources build with warnings treated as errors.
An accidental overlap of two Alire production builds caused missing dependency
files when the native configure script reset their shared build tree. This was
a validation invocation error, not suppressed compiler diagnostics; subsequent
Alire commands were serialized, with no source or gate changes to bypass it.

## Boundaries and real-provider status

No StackedImage, general/application-owned buffers, new pixel formats,
optional sensor commands, sensor simulation, DSP or transport is added.
Native provider C++ ABI compatibility is still required behind the C facade.
Conservative retention can intentionally retain memory/code for process
lifetime when ownership is uncertain; it is not successful cleanup.

No local `minimal-mel-example` checkout was found under the workspace/home
repository locations. Its provider integration was **not run**. The bridge
has not been qualified against that real provider or hardware; the mock's
`InvalidState` is contract evidence, not a claim of a real-provider run. The
ordinary suite has no dependency on that private repository.
