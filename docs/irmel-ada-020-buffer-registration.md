# IRMEL-ADA-020 — application-owned Buffer registration

## Preparation and scope

Freshly fetched main: `cb33c5e5434fa27b9d9660f6a9d361ed0993e37b`, tree
`0f6851c7e87ed002491fd61c2c43bd2683243ca1`. Work branch:
`feature/irmel-ada-020-buffer-registration`. Initial open PR #215 was unrelated;
later PRs #218 (safe Python Stacked) and #219 (safe Rust communications metadata)
were inspected and do not implement registration. Their branches are not edited.

The pinned Channel.h and Buffer.h are the interface authority. This slice adds
application Buffer registration on C2, HealthAndStatus, StackedImage and
**control-only** Image. It does not add generic acquisition, GPU/RDMA access,
unchecked release, or safe Python/Rust registration APIs. Sixteen-byte host
bookkeeping Buffers require no image dimensions or internal acquisition pool.

## Ownership and lifecycle

The standalone owner explicitly distinguishes Unregistered, Registering,
Registered, Unregistering and OwnershipUncertain. It retains its fixed storage,
provider Buffer, native Session and, during an obligation, the concrete family
state. The public Ada limited owner remains the owner; common Views stay weak.
Registration identity uses the native family state and exact SessionState, not
an instance string, provider path, or original public Session handle.

Each family has independent persistent registration and uncertainty counts and
a short-lived exclusive operation flag under its lifecycle mutex. These are
not ordinary asynchronous request counts. Buffer admission uses both Buffer
and family locks; provider register/unregister runs with neither held. Enable,
Disable, typed Close and competing registration operations reject an in-flight
operation instead of waiting for synchronous provider reentry. Typed Close
rejects active/uncertain obligations **before consuming the public handle**.
Image terminal Stop also rejects an outstanding registration, preserving the
caller’s ability to Disable and explicitly Unregister.

Pre-entry validation failures do not change Buffer state/accounting or invoke
the provider. Refusal or exception after entry conservatively keeps the exact
association and lifetime obligation, even if no extra shared owner is visible.
New registration/Enable cannot overwrite an uncertain obligation. An explicit
same-channel unregister may clean it up while the lifecycle remains Attached.
Failed lifecycle has no speculative cleanup call: resources remain retained.

Successful unregister is recorded separately from ownership reconciliation.
Provider extra shared ownership blocks byte/flag/property access and Buffer
Close; Image also requires no active callbacks, unresolved checkouts or release
obligations. The original provider shared_ptr is always forwarded. No new
Buffer is created, initialized or implicitly released by registration cleanup.
After reconciliation, ordinary Buffer and channel Close semantics resume.
Finalization never raises; an unresolved Buffer native handle is deliberately
retained, rooting storage and the entire Session/provider DSO graph.

This is **not** a claim of callback quiescence after Disable or unregister.
Providers must obey the published C++ ownership contract rather than accessing
destroyed objects through unowned raw addresses. Extra shared ownership and
known unresolved Image checkout/release work are explicit fail-safe boundaries.
The existing Image listener still rejects unexpected control-only checkouts
without querying sample addresses/header layout. Only an actually delivered
checkout enters the existing release engine. Uncertain release permanently
retains the graph; no fabricated frame is produced.

## Public interfaces and coverage delta

Exactly two `AMS_MEL_0.1` C exports, **324 → 326**, no record-layout changes:

- `ams_mel_ir_channel_register_buffer`
- `ams_mel_ir_channel_unregister_buffer`

Both publish exact provider Return independently from bridge status;
`UINT32_MAX` means no Return was obtained. Private Python ctypes and Rust sys
mirror the signatures/inventory only.

Safe Ada exposes `Channel.Register_Buffer` and `Channel.Unregister_Buffer`.
The Buffer package owns their implementation entries (`Register_With_Channel`,
`Unregister_From_Channel`) so raw C handles remain private without illegal
sibling access to a private child. The public Channel wrappers accept only the
existing View and limited Host_Buffer. Complete trailing GNATdoc documents
association, ownership, failure, retention and callback limitations.

Both unique upstream Channel operations remain **partial** overall: eight
inherited exposures (two operations × four required families) become safe_ada.
Other family exposures are unchanged. Unique safe-Ada operations remain **57**;
inherited improvements are not counted again. Buffer release/address and Mono8
acquisition classifications are unchanged. The authoritative JSON inventories
and generated report carry the actual exposure evidence.

## Qualification evidence

Initial qualification and subsequent final-source qualification:

- Isolated GCC Debug build `/tmp/irmel020-native`: warning-clean native build;
  **48/48** initial compiled-C registration scenarios passed.
- Additional Image delivered-checkout, refusal, and existing Buffer tests are
  being qualified with the final suite; earlier test development failures are
  not represented as final successes.
- Raw Rust ABI test: **47/47 passed**, including new function-pointer types and
  C-header/export inventory equality.
- IR coverage checker and **67 controls passed** after inventory updates.
- Ada GNATdoc changed-declaration check passed; final formatting/docs/full
  build results will be recorded after qualification.

The compiled-C tests use condition variables, not sleeps, to hold each provider
operation while Enable/Disable/Close and competing registration are attempted.
They exercise same/second View duplicates, wrong channel, distinct native
Sessions with identical instance/path, all specified registration Return values,
exceptions in both methods, explicit cleanup after refusal, extra ownership
after successful unregister, parent-first Close, unchanged bytes and exact
Buffer identity. FoundationBuffer destruction reads every initialized byte;
its release aborts because no checkout was delivered. DSO unload is checked
after the final graph owners, using the separate provider’s event log.

Safe Ada tests use public APIs only and cover all four families, repeated
Enable/Disable cycles, independent context/storage, duplicate/wrong-channel
rejection, recoverable out-of-order Close, explicit refusal cleanup,
parent-first closure and non-raising finalization. The public example is
`/home/zboll/git/ams-gra-mel-bridge/ada/tests/src/ams_mel_ir_registration_example.adb`.

No local `minimal-mel-example` provider checkout was found.
**PRIVATE_REFERENCE_PROVIDER_INTEGRATION_UNTESTED**. Mock qualification is not
physical sensor acquisition or full minimal-consumer parity.

## Final-source qualification

Implementation commit: `3e163abefaec688d87a9d921e72fd3adce746e6a`.
Ordinary merge of current main `ad35ec4d5fc2faa9296b530defad108bf1dbac31`:
`24c6be64f4631c0faec74f4ff2b767adb9fa5db8`, tree
`1d030bdafa669f5da07efd6855e7aada5181108d`. No published commit was
rewritten. Main's common Rust crate changes are merge provenance, not feature
scope. Later PRs #220 and #221 were also checked; neither is registration work.

Actual completed validation:

- Initial sequential `make format-ada`, `make check-ada-format`,
  `make check-ada-docs-changed`, `make test-native`, `make test-python`,
  `make test-rust`, `make test-ada`, `make check-ir-api-coverage`, `make check`:
  passed. Initial totals were native 795/795, Python 610/610, Rust 574 passed
  (including 53 doctests), 12 existing opt-in ignored cases. Subsequent review
  added four no-retention refusal cases, bringing native to 799.
- Final-source GCC 14.2.0 / Clang 19.1.7 × Debug/Release CMake matrix:
  **799/799 passed in each configuration**, warnings treated as errors.
  Commands: `cmake -S /home/zboll/git/ams-gra-mel-bridge/native -B TREE
  -DAMS_MEL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=CONFIG
  -DCMAKE_C_COMPILER=/usr/bin/COMPILER -DCMAKE_CXX_COMPILER=/usr/bin/CXX`,
  `cmake --build TREE --parallel 2`, `ctest --test-dir TREE --output-on-failure`.
  Separate trees `/tmp/irmel020-{gcc,clang}-{Debug,Release}`; no shared outputs.
- Each matrix tree: `ctest --test-dir TREE
  -R '^channel_buffer_registration_.*(reentry|concurrent)$'
  --repeat until-fail:50 --parallel 2 --output-on-failure`: eight cases × 50,
  **400 executions per configuration, 1,600 total**, all passed. Barriers are
  condition-variable driven; no sleep establishes a race.
- Focused final-source registration/unsupported-family controls: **56/56 passed**.
  This includes refusal with no extra shared owner as well as actual retention;
  neither becomes Unregistered without explicit successful unregister.
- Safe Ada full smoke suite with synchronous metadata reentry from provider
  register and unregister passed using the isolated native tree. Only public
  Session/Buffer/Channel/typed-family APIs are used. The standalone registration
  example compiled and ran successfully against the separate mock.
- Coverage: **70/70 controls passed**, eight inherited exposures improved;
  unique operations safe_ada=57, partial=11, missing=16, provider_only=64.
- GCC and Clang strict `-std=c11 -pedantic-errors -Wall -Wextra -Werror`
  registration signatures and existing Buffer layout probe passed.
- Private Python and Rust ABI inventories/signatures include exactly two new
  functions. Production `nm -D --defined-only` equality: **326/326**, new
  symbols versioned `@@AMS_MEL_0.1`. Test-only probes are not production exports.
- `cargo fmt --all -- --check`, workspace/all-target Clippy `-D warnings`,
  workspace rustdoc `-D warnings`: passed after common-crate merge.
- `make test-policy`: passed after common-crate merge, including source
  integrity/security, documentation and hostile negative controls. Expected
  failing child-fixture output is not a failing parent policy suite.
- Pinned documentation: `alr -n -C /home/zboll/git/ams-gra-mel-bridge/docs/toolchain
  exec -- /home/zboll/git/ams-gra-mel-bridge/.doc-tools/enhancer/bin/python
  /home/zboll/git/ams-gra-mel-bridge/docs/tools/docs_ci.py build
  --directory /home/zboll/git/ams-gra-mel-bridge/.doc-tools/irmel020-api
  --gnatdoc-prefix /home/zboll/.alire/gnatdoc_bin_26.0.0_6e6e0320`: passed from
  clean commit `24c6be6`. Coverage **5116/5117**, 101 informational GNATdoc
  warnings. All 12 new descriptions/parameter elements are documented; complete
  exception annotations are checked by the changed-declaration gate.
- Final-newline and Git whitespace checks passed. Vendored source is unchanged.
- Reviewed-binding implementation hashes were refreshed for the actual native
  and Ada registration implementation, headers and qualification sources; the
  independent checker anchor was updated without altering upstream pins.

Preliminary failures were corrected, not waived: misleading-indentation mock
warnings; a Stacked fixture missing advertised capabilities; test event names
and an unrelated wrong-channel detach in a global count; an illegal Ada private
sibling-child access; and expected linking before the production facade had
been rebuilt. Final tests retain active checks under NDEBUG.

The final-source sequential rerun (after the ordinary common-crate merge) has
also passed formatting, changed GNATdoc declarations, native **799/799**, Python
**610/610**, Rust **576 passed / 12 existing opt-in ignored** (the common-crate
merge adds two tests), the complete safe Ada smoke suite, and all **70** IR
coverage controls. The second `make check` also passed, including both build-tree isolation
directions, repeated native 799/799, 70 coverage controls and the full Ada suite.
Pinned `alr -n -C /home/zboll/git/ams-gra-mel-bridge/ada build` and
`alr -n -C /home/zboll/git/ams-gra-mel-bridge/ada/tests run` both passed, including
the four-family registration and synchronous metadata-reentry qualification.
The pinned environment is GNAT 16.1.0 / GPRbuild 26.0.1; no dependency pin changed.
All requested local qualification is complete.

## Delivery

PR: <https://github.com/zackboll/ams-gra-mel-bridge/pull/222>, non-draft,
auto-merge disabled; no approval or merge performed.

Hosted source `24c6be64f4631c0faec74f4ff2b767adb9fa5db8`:

- Documentation **passed**:
  <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43674068587>.
- Bootstrap:
  <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43674068608>.
  All Bootstrap jobs passed on this implementation/merge HEAD.

Reviewed-source anchor commit `d3a12036a1c6316563307abd8e0997b909b1d0f0`, tree
`237203c538f633d6d6477a022f36505e90b65356` changes only the reviewed digest
inventory/checker anchor, not compiled source. Its hosted documentation passed:
<https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43674660619>.
Bootstrap <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/43674660625>
has policy, Python, Rust, Ada, build isolation, GCC Release and both Clang
configurations passed at observation; GCC Debug is still running.

Final report commits require their own exact-HEAD hosted conclusions. Exact
final HEAD/tree will be recorded in the PR delivery comment to avoid a
self-referential commit hash. **HOSTED_CI_PENDING** until exact-head checks finish.
