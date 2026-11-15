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

## Qualification evidence (in progress)

Early focused qualification:

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

## Delivery

No commit or hosted result is claimed yet. Final HEAD/tree and exact-head
workflow URLs will be recorded with delivery. **HOSTED_CI_PENDING**.
