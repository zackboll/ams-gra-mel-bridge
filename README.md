# AMS MEL Bridge

AMS MEL Bridge is an experimental, consumer-side library for the published
C++ MFA Encapsulation Layer (MEL) interfaces of the Agile Mission Suite
Government Reference Architecture (AMS GRA). It lets Ada, Rust and Python
applications consume implemented MEL capabilities without modeling C++ object
layouts, exceptions and resource lifetimes themselves.

The project supplies a versioned C ABI, a private C++ adapter and language
bindings with explicit ownership and error contracts. IR and RF support differ
by language and depend on the runtime provider's capabilities. This is not a
complete MEL binding, MFA, Skill, OMS service, hardware provider or GRA
implementation.

## Architecture overview

```text
Ada / Rust / Python consumer
             |
     Language binding
             |
         C ABI
             |
      C++ MEL adapter
             |
    Published MEL API
             |
      Runtime provider
```

Native C++ consumers can use the published MEL interfaces directly; they do not
need this additional bridge. The C ABI is project-owned and experimental,
**not an official AMS GRA C binding**. Provider-side C++ compiler, standard
library and ABI compatibility still matter.

See the [architecture guide](https://zackboll.github.io/ams-gra-mel-bridge/guides/architecture.html)
for resource paths, or [project context](docs/project-context.md) for the
distinction between MFA, MEL and Skill, independent provider/consumer languages,
process boundaries and Ada/SPARK assurance boundaries.

## Implemented language surfaces

This table describes application-facing APIs, not parity inferred from raw
imports or enum vocabulary. Individual source specifications remain authoritative.

| Surface | Implemented scope | Important boundary |
|---|---|---|
| C ABI | Provider lifecycle; IR image, channel, C2 and metadata families; Health/Status plus conditional Instrumentation/Track services; RF DataMEL, ProductRx, Admin, C2, VirtualAperture (VA) and Job families | Low-level opaque owners, versioned records, explicit close and borrowed-span rules; incomplete upstream coverage |
| Ada | IR Mono8 image copies and leases, common channels, general Mode/ScanParam, supported BIT operations, ConfigSet, navigation and relevant metadata; Health/Status and conditional Instrumentation/Track; RF DataMEL/ProductRx/Admin/C2/VA/Job and query/observer families | Safe owned values and limited resource owners; conditional operations still require provider support |
| Rust | Session/version, owned-copy IR Mono8, C2 Operate/TaskSched and BIT no-op; weak C2/Image channel views with KeepAlive, CommsTest and complete owned capability snapshots | No safe RF, general Mode/ConfigSet, payload-bearing BIT, metadata queues or Health/Instrumentation/Track owners |
| Python | Session/version, owned-bytes IR Mono8, C2 Operate/TaskSched and BIT no-op; weak C2/Image channel views with KeepAlive, CommsTest and complete owned capability snapshots | Dependency-free development API; no public RF, expanded C2 commands, metadata queues or Health/Instrumentation/Track owners |
| Native C++ (direct upstream path) | Uses the provider's published C++ MEL interfaces directly | Not a wrapper implemented by this project; supported behavior belongs to that provider |

Ada IR metadata includes C2 command/BIT status and configuration, opt-in CommsTest,
Image bad-pixel, line-of-sight and navigation responses, Health/Status reports,
InstrumentationReport, and Track report/system-data-request/candidate/preprocessed-
candidate events. Track update and SystemTrackDataResponse sends are represented;
the provider must support the corresponding conditional or optional operation.
See the IR workflow guide for the exact kinds and receive contracts.

Ada RF ProductRx supports signed Complex INT8/INT16 I/Q pairs, Direct
INT8/INT16 scalars, and serialized AMS VITA Small/Medium/Large/ExtraLarge
packets. Packet bytes are validated and copied in packet/network order;
**VITA sample decoding, PDW and Local Function receive payloads are not supported**.
Copied ProductRx metadata includes stabilization points and VITA receive-event
associations, not arbitrary `std::any` or full ReceiveEvent objects.

Ada RF C2 supports asynchronous VA/Job creation, unique resource claims,
RX/TX group and interval configuration, copied pointing/modulation/waveform
values, Job lifecycle commands, status observers and conditional queries.
Configuration acceptance does not establish scheduling, waveform execution or
RF emission. Full upstream resource APIs are not bound.

## Getting started

Use a Linux development checkout with CMake 3.20+, Make or Ninja, a C11/C++20
compiler and Python 3. Ada needs an Ada 2022-capable GNAT and GPRbuild;
Alire supplies the pinned development dependencies, including GNATformat.
Run the following **sequentially from the repository root**:

```sh
make native
make test-native
make test-ada
make check
```

`make native` builds the production facade in `native/build`; contract tests
use the separate `native/build-tests` facade and mock providers. The direct Ada
targets need GNAT/GPRbuild on `PATH`. Do not overlap commands that write these
shared native build trees.

For the local Alire layout, also from the repository root:

```sh
alr -C ada build
alr -C ada/tests run
```

There is no root Alire crate. CMake owns native compilation; Ada and Cargo link
the resulting library rather than recompiling its C++ sources. Building the
bridge does not install a runtime MEL provider. Applications pass a compatible
provider library path and provider-defined configuration to the relevant Open
operation; no universal provider configuration is implied.

Detailed setup: [Getting Started](https://zackboll.github.io/ams-gra-mel-bridge/guides/getting-started.html),
[Ada](ada/README.md), [native build integration](native/README.md) and
[Python](python/README.md). Language READMEs retain some older scope snapshots;
use the table above and current specifications for capabilities.

Rust workspace checks, after the native prerequisites are built:

```sh
make test-rust
```

The workspace is under `rust/`; its crates do not build a provider or invoke
CMake themselves. See the [packaging checklist](docs/packaging.md) for the
development layout and remaining release work, not a package availability claim.

## Documentation

**[Open the developer documentation portal](https://zackboll.github.io/ams-gra-mel-bridge/)**

- [Getting Started](https://zackboll.github.io/ams-gra-mel-bridge/guides/getting-started.html)
- [Architecture](https://zackboll.github.io/ams-gra-mel-bridge/guides/architecture.html)
- [IR Workflows](https://zackboll.github.io/ams-gra-mel-bridge/guides/ir-workflows.html)
- [RF Workflows](https://zackboll.github.io/ams-gra-mel-bridge/guides/rf-workflows.html)
- [Ownership and Errors](https://zackboll.github.io/ams-gra-mel-bridge/guides/ownership-and-errors.html)
- [API Map](https://zackboll.github.io/ams-gra-mel-bridge/guides/api-map.html)
- [Full GNATdoc Ada API reference](https://zackboll.github.io/ams-gra-mel-bridge/api/index.html)

The API map links public package names to their generated destinations. Native
GNATdoc declarations, anchors, profiles and package navigation remain authoritative.
Published pages receive a reviewed responsive light/dark theme and lexical syntax
highlighting for Ada declaration snippets. Supported, unambiguously resolved public
MEL type references link to source-verified local native declarations; unsupported
or alias-dependent references may remain plain text. Optional offline search matches
the 37 native sidebar labels (29 Compilation Units and eight Tagged Types), not
full documentation text. Approved sidebar destinations support active selection,
package/declaration bookmarks and Back/Forward. Ordinary non-sidebar declaration
links remain native navigation and do not automatically update outer bookmarks
or sidebar selection. With JavaScript disabled, both native groups, direct native
bookmarks, type links and the authored portal remain usable; search stays hidden.
External type links, operation categories, generic summaries and shared authored-
guide rendering remain deferred. Original GNATdoc output is separately qualified.
DOC-007F adopts the merged GNATDOC-009A/009B/010 enhancer: one ordinary
subtype base and nine current-package references are independently required,
alongside source-owned API titles, English language and UTF-8 metadata. All
1,428 local type occurrences and exact native restoration are checked independently;
authored portal metadata and bytes remain unchanged.
[GNATdoc foundation](docs/gnatdoc-foundation.md) records the qualified toolchain,
generation commands and exact renderer exception.

For development evidence, see the [architecture decisions](docs/architecture.md),
[C ABI policy](docs/c-abi-policy.md), [coverage history](docs/coverage.md) and
[README research/retention map](docs/doc-007b-readme-research.md). Historical
reports describe their checkpoints, not necessarily the current feature set.

## Providers and validation

A working language binding, mock-backed API behavior, a pinned simulator
observation and production hardware qualification are different kinds of evidence:

- **Ordinary builds and tests** use separate mock providers. They require no
  Squall checkout, hardware or credentials. Native and Ada tests exercise
  conversions, ownership, bounded queues, asynchronous results and failure paths.
- **Opt-in Squall integration** records behavior at a specific pinned revision
  and configuration. The IR clients have positive image/control evidence; Ada
  Health observations do not establish every possible callback payload.
  Instrumentation and Track positive behavior is mock-backed: pinned Squall
  supplies clean unsupported-channel evidence, not positive execution.
- **RF Squall evidence** includes Complex INT16 receive and control/lifetime
  observations, not validation of all formats or TX hardware. Several interval
  commands are no-ops in that provider; successful Add or Finalize does not prove
  RF transmission. Zero-return power queries do not establish a transmit model.
- **Production hardware qualification** is not supplied by these results.

See [Squall integration instructions and evidence](integration/squall/README.md)
for opt-in prerequisites, pins, cleanup and the limits of each observation.
No real-provider or hardware run is implied by an ordinary passing test suite.

## Scope and limitations

- **Experimental ABI:** version 0.1 is this bridge's version, not an upstream MEL,
  AMS GRA or provider version. Versioned records have explicit compatibility
  rules; see the [ABI policy](docs/c-abi-policy.md).
- **Partial upstream coverage:** unsupported families include Scheduling,
  StackedImage, application-exposed generic/device buffers and broader RF
  resource/RDMA/VADB operations. Represented capabilities are not implemented
  operations or guarantees that a particular provider supports them.
- **Explicit ownership:** use documented Close operations and serialize access
  to each owner. Timeout and request Close are not cancellation. Pending work,
  RF Jobs and child resources can retain native parents after public closure;
  RF resource creation requires a separate successful, unique Claim.
- **Copies and borrows differ:** Ada frame/snapshot/metadata copies are independent.
  IR frame leases retain provider host buffers and borrow pixels only during
  `With_Pixels`. RF provider callback payloads are copied natively before queueing;
  RF `With_*` borrows that event storage during a synchronous processor, while
  `Copy_*` creates independent Ada storage. Rust/Python frames own copies.
  These APIs do not imply end-to-end zero-copy or a NumPy view interface.
- **Callbacks and provider lifetime:** metadata uses bounded native queues that
  can drop incoming events; provider callback threads do not invoke application
  code. RF registrations can permanently retain the exact callable/state and
  provider DSO. Close is not a universal callback-quiescence or unload guarantee;
  uncertain cleanup may deliberately retain resources.
- **Assurance:** no SPARK proof is asserted for the native FFI or provider, and
  no general AMS GRA compliance, certification or complete Skill is claimed.
- **Platforms and packaging:** Linux x86-64 is the initial validation target;
  Windows visibility declarations are not Windows qualification. Alire manifests
  use local development pins and are not registry-ready releases. Rust crates
  remain unpublished; Python is development-use-only, with no wheel/PyPI release.

Read [Ownership and Errors](https://zackboll.github.io/ams-gra-mel-bridge/guides/ownership-and-errors.html)
before designing shutdown, callback or borrowed-payload handling.

## Repository structure

```text
native/              C11 ABI, C++20 adapter, mock providers and native tests
ada/                 Public Ada specifications, bindings and contract tests
rust/                Raw sys crate and narrower safe Rust consumer API
python/              Dependency-free public API over private ctypes bindings
integration/squall/  Opt-in pinned-provider integration and observations
docs/                Portal sources, API evidence, design and task history
scripts/             Build validation and documentation qualification tools
```

## Upstream provenance

Pinned MEL declarations and their dependency closure are protected vendored
inputs, not a bundled production provider. Ordinary native builds use that
closure without fetching upstream code. Revisions, source origins, licensing
and checksum identities are recorded in
[upstream provenance](docs/upstream-provenance.md) and the
[vendored-file SHA-256 ledger](docs/upstream-files.sha256.md).

## Contributing and license

See [CONTRIBUTING.md](CONTRIBUTING.md) for the validation and review process and
[CHANGELOG.md](CHANGELOG.md) for historical implementation records. The
[retention map](docs/doc-007b-readme-research.md) locates technical evidence
removed from this introduction; [legacy reference notes](docs/history/readme-legacy-notes.md)
retain background terminology and citations.

New bridge code is licensed under [Apache-2.0](LICENSE). Vendored declarations
retain their upstream licensing and provenance; review those records before
redistributing or replacing them.
