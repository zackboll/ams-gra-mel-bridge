# DOC-005 source research

## Starting identity and provenance

Fetched main and task base: `d4a4e262eb1404c963c509cf1e5d32f0c6712b99`.
GitHub confirms PR #104 merged at that identity with reviewed head
`03e4e0d0a2699a9c5a47f71d4385487839b28d67`.
No intervening main changes required reconciliation.

All 85 vendored RF/Common headers were compared byte-for-byte with immutable
Git blobs in the local upstream checkouts, and their SHA-256 values checked
against `docs/upstream-files.sha256.md`. Records and per-file hashes are in
external `r1/verified-upstream-headers.json`. Pins agree with
`docs/upstream-provenance.md`:

- RF MEL: `762ce84c5555dd0f3ea66f36b321fecf8839b89f`.
- Common MEL: `f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.

## 005A source evidence matrix

All RF header paths below are under `native/vendor/rf-mel/include/rfmel`
at the RF pin above. Common declarations use the Common pin above.

| Claim | Pinned declaration | Ada bridge | Native implementation | Existing regression evidence |
| --- | --- | --- | --- | --- |
| Independent factory owner | `factory/RFCreateFunctions.h`, `admin/AdminMEL.h` | `ams-mel-rf-admin.ads`; body Open | `rf_admin.cpp:80–139` | `test_rf_admin.c` load/symbol/null/throw cases; Ada Expect_Open_Failure |
| Embedded NUL rejection, exception-safe temporary strings | No upstream Ada-string contract; bridge policy | body String_Owner and Open, lines 16–23 and 48–76 | Native receives terminated strings | Ada Run path/config NUL cases |
| Local handle openness | No upstream handle concept | body Is_Open, lines 78–79 | Public opaque owner | Ada Run closed-owner checks |
| Fresh UCI/status owners and Boolean rejection | `admin/AdminMEL.h`, `admin/UCI_Control.h`, `admin/StatusControl.h:25–29` | body Command_State, lines 81–99; private `ams-mel_c_api.ads:2908–2929` | `rf_admin.cpp:142–170` | C reject/null-interface/throw cases; Ada Expect_Command_Failure |
| All fifteen state representations, not a separate RF state model | Common `mel/library/CommonMEL.h` MFA_State | `ams-mel-status.ads` representation; Admin body Enum_Rep | `rf_admin.cpp:27–46`, range check at 148 | C Standby/RX-only/Degraded and out-of-range cases |
| Consumed handle, shutdown once, destruction before DSO release | `rfmeltypes/RFMEL.h` shutdown declaration; retention is bridge policy | body Close/Finalize, lines 101–116 | `rf_admin.cpp:48–68,173–197` | C lifetime log ordering, repeated close, shutdown-throw retention; Ada throwing-close and finalization paths |

Paths in the Ada column are under `ada/src`; native implementation paths under
`native/src`; tests under `native/tests` and `ada/tests/src`.
Historical reconciliation: `docs/task-034b1a-safe-ada-rf-admin.md` agrees with
these implementation paths. DOC-004 reports and trailing-comment conventions
were consulted; the qualified pipeline and renderer policy were not changed.

### Qualification limits

The listed regression tests were read, not executed as production tests during
005A. Executed observations are documentation generation, semantic compilation,
coverage/warning ratchets, HTML/link validation and documentation/integrity tests;
see the separate coverage report. Prior historical test reports are not new runs.

State acceptance is not a physical transition or provider-independent support
guarantee. The bridge admits enum values 0–14 but has no transition table.
Shutdown failure consumes the public owner and intentionally retains the native
graph permanently; a second Close does not retry. Dropping the library reference
after successful shutdown is not a promise of unloading if other references exist.
Operations require external serialization. No new Admin implementation defect
was established in this research.

## Outstanding research and qualification

C2's specification was inspected, but its body, versioned serialization, native
VA/Job retention, historical 034B–034F evidence, tests and upstream semantics have
not yet been fully reconciled. No new C2 documentation or source-backed completion
claim is made. DOC-005 remains incomplete and unpublished.

## DOC-005A recovery review — Admin only

The earlier incomplete DOC-005 checkpoint is retained above. RF C2 is explicitly
outside this independent PR and remains deferred, without source changes or
new claims of qualification. DOC-005A recovers the completed Admin documentation;
it does not restart documentation development.

A fresh review checked every public Admin declaration and its rendered package,
type, subprogram, parameter and return descriptions against the Ada body, private
C imports, native owner and command paths, pinned declarations, both regression
test sources and historical Task 034B1A. The existing source-backed Admin prose
was retained without changes. Rendered text is retained externally as
`r2-admin-publication/admin-rendered-review.txt`.

### Source citation clarification

The earlier matrix's MFA_State citation names the umbrella CommonMEL header.
The actual enum declaration is in `include/mel/library/MFA_Component.h:102–142`,
included by CommonMEL.h, at Common revision
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`. All fifteen declarations and native
static assertions match. Upstream's introductory prose says OperateTxOnly is
excluded, but its actual enum includes it at representation 9. The bridge follows
those declarations and checks 0–14; documentation deliberately does not infer a
provider transition model or universal state support from that inconsistent prose.

All 85 vendored RF/Common headers were freshly rechecked against their pinned
Git blobs and the repository checksum manifest. Per-file verification is retained
in `r2-admin-publication/verified-upstream-headers.json`. The RF pin remains
`762ce84c5555dd0f3ea66f36b321fecf8839b89f`.

### Confirmed ownership and failure boundaries

- The factory is only createAdminMEL, independent of DataMEL/C2MEL. Native owner
  allocations precede the provider factory. Temporary Ada C-string allocations
  have controlled cleanup; Ada allocation exceptions are not claimed to be
  translated by the native status checker.
- Embedded NUL is rejected before C-string allocation. Is_Open only compares
  the public handle to null; it does not query physical state or provider health.
- Every command obtains fresh UCI and status-control shared owners. False is a
  normal provider rejection; closed handles, null interfaces and provider
  exceptions raise the bridge provider exception. Accepted commands do not
  establish physical transition or completion.
- Close consumes the public handle before shutdown, including on shutdown
  exception. Successful cleanup releases Admin before the DSO reference;
  uncertain throwing shutdown permanently retains the graph without retry.
  Controlled finalization does not propagate exceptions. Owner operations
  require external serialization.

No real-provider or hardware execution is claimed by this continuation. Existing
mock tests are the production regression basis; upstream documentation, pinned
actual declarations, bridge behavior and newly executed tests remain distinct
forms of evidence. No implementation defect requiring production edits was found.

## DOC-005A final local qualification — complete

All required local gates passed under the saved stable compiler environment.
The production gates ran sequentially, never overlapping native build trees:

| Gate | Observed result |
| --- | --- |
| Required format and format validation | PASS |
| Qualified semantic compilation / strict GNATdoc twice | PASS |
| Full gap/warning identities, native profiles/anchors | PASS; no additions |
| Two fresh complete sites | PASS; all 32 generated files byte-identical |
| HTML structure / local links | PASS; 31 pages / 1,307 links |
| Documentation unit and mutation tests | PASS; 75 tests |
| Dedicated integrity tests | PASS; six tests |
| Literal-aware integrity against starting main / DOC-001 | PASS; 1,178 / 1,162 protected files |
| Independent production/vendor byte inventory | PASS; 1,180 non-documentation files, only Admin comments differ |
| make test-native | PASS; 434/434 |
| make test-ada | PASS; full smoke/contracts and focused repeat checks |
| make check | PASS; isolation, 434/434 native, format, Ada and text gates |
| Final make check-ada-format | PASS |
| Final Admin source hash versus tested source | PASS |
| Whitespace and final newlines | PASS |

Full output and actual exit statuses are retained under external
`r2-admin-publication`, with separate `test-native`, `test-ada`, `check`,
`final-format-check` logs and `.exit` files. No inherited test failure occurred
in this sequence; no tests were weakened, suppressed or reclassified. A polling
command exceeded the tool's 30-second timeout; it was not a gate failure and did
not interrupt the background qualification runner. The documentation mutation
suite's expected warning-ratchet error text appears inside a passing 75-test run.

Final measured inventory remains Admin 15/15, repository 2,912/4,295, 1,383 gaps;
15 removed gaps and 15 removed public warnings, zero additions. All other package
identities and private warning identities remain unchanged. No new pinned Squall
or hardware execution was performed. RF C2 remains untouched and deferred.

Local qualification is complete; publication and exact-head hosted statuses are
separate observations recorded in the external handoff. Independent review is
required; no merge, tag, release or feature-branch Pages deployment is authorized.
