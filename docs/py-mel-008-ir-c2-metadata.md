# PY-MEL-008 — Python IR C2 metadata implementation and qualification

This task-owned document is the authoritative feature-specific implementation
and qualification report. Public usage remains in the
[Python API guide](../python/README.md#ir-c2-metadata-py-mel-008).

## Migrated original coverage (preserved in full)

## PY-MEL-008 — complete safe Python IR C2 metadata

`ControlChannel.open_metadata(queue_capacity=16)` now returns an independent
`C2MetadataStream` over the seven existing published C functions. Native Open
registers BIT_Configuration, CommandStatus and BIT_Status, in that order, while
Attached or Enabled. Separate `enable_comms_test_events()` registers
ChannelCommsTestRep once; success or failure consumes that independent one-shot,
without undoing the base registrations. No implicit enable or capability gate.

| Native callback / event kind | Safe Python payload |
| --- | --- |
| CommandStatus / 1 | `C2CommandStatus`, exact `C2CommandState` 0..4 and all 47 `C2CannotComply` values 0..46, full uint32 ID and complete UTF-8 description |
| BIT_Configuration / 2 | `C2BitConfiguration` with ordered `C2BitType` values, exact IDs/labels, `C2BitControlInterface`, item names, component IDs and signed int64 duration |
| BIT_Status / 3 | Canonical Health `BitStatus` and nested models, copied by the existing shared `_health_convert._bit()`; identical `ams_mel_bit_status_v1` layout |
| ChannelCommsTestRep / 4 | Existing `CommsTestReport(command_id, request_id)`, no invented channel ID |

Frozen `C2MetadataEvent` enforces kind/payload agreement; `C2MetadataCounters`
preserves full uint64 received/dropped/malformed values. One native bounded FIFO
and DROP-INCOMING policy serve all four families. Ordered duplicates, empty
collections, signed times, percentages and IEEE nonfinite values remain intact.
Unpublished enums, malformed spans/views/UTF-8/NUL and diagnostic buffers fail
closed; only the active payload is inspected. Native events are closed before
returning complete Python-owned graphs, including exceptional conversion paths.
Accepted CommandStatus is message acceptance, not physical completion. Metadata
is independent of Mode/Return/Comms request Wait results.

Metadata does not retain Python parents, close C2, cancel requests or unregister
callbacks, and Close does not promise callback quiescence. Parent-first teardown
permits queued drain under native state rules. An unresolved Close is not
silently successful or blindly retried; finalization is non-raising and one-attempt.
Externally serialize metadata operations/Close and registration versus parent
C2 operations; the GIL is not this contract. No Python callback/thread/lock/queue.

`python/tests/test_ir_c2_metadata.py` qualifies all existing requested compiled
mock scenarios, exact rich BIT graphs, high IDs, the 613-byte rejection,
independent CommsTest opt-in and request observations, FIFO/drop counters,
malformed callbacks, callback allocation failure, teardown, actual provider DSO
unload and the existing deterministic nonquiescing callback barrier. Defensive
ABI injection covers otherwise unproducible hostile records, inactive payloads,
diagnostics, publication inconsistencies and consumed/retained cleanup failures.
Positive evidence is mock-only, not Squall/hardware qualification.

Started independently from fetched main
`e746c49fd3ffe57b37064878cdf8225a3745a35e` (tree
`7b022169714075949ff0604ad79d94238cf532cd`), with 354 discovered Python tests.
PR #172 merged during implementation and was incorporated by an ordinary merge
of main `fa91028804f38d3b2bd15ec8e5e2f80cd4a74a7e`; its entire Image implementation,
exports and tests are preserved. Final exact feature identity and measured
qualification counts are recorded in the delivery PR to avoid recursive hashes.
No native ABI/export, `_native.py`, provider/mock, Ada/Rust implementation,
frozen compatibility baseline, workflow or policy changes. Existing Image,
Health, NUC and Ada/Rust documentation and tests remain intact. Other C2 command
and optional callback APIs remain outside this slice. See
[Python API and seven-function mapping](../python/README.md#ir-c2-metadata-py-mel-008).

## Existing seven-function C ABI mapping

| Python operation | Existing published C function(s) |
| --- | --- |
| `ControlChannel.open_metadata` | `ams_mel_ir_c2_metadata_open` |
| `C2MetadataStream.enable_comms_test_events` | `ams_mel_ir_c2_metadata_register_comms_test` |
| `C2MetadataStream.receive` | `ams_mel_ir_c2_metadata_receive`, `ams_mel_ir_c2_metadata_event_view`, `ams_mel_ir_c2_metadata_event_close` |
| `C2MetadataStream.counters` | `ams_mel_ir_c2_metadata_get_counters` |
| `C2MetadataStream.close` | `ams_mel_ir_c2_metadata_close` |

## Qualification scope migrated from the Python README

Qualification is **compiled-mock only**, not real-provider/hardware qualification.
`tests/test_ir_c2_metadata.py` covers rich nested values, all four families,
independent registration and request results, FIFO/overflow/malformed callbacks,
allocation failure, teardown/unload and deterministic nonquiescing callback
barriers. Hostile C outputs cover malformed active records and diagnostics,
poisoned inactive records, and consumed/retained cleanup failures. No native ABI,
private ctypes, provider/mock, Ada/Rust implementation, workflow or policy change
is needed; Image, Health and NUC remain independently supported.

## Historical original-head qualification

The following results belong only to original PR head
`c3d0505a1f4093d343ba56a15b706768a38e3815`, tree
`8c39b1c389354c01c7306aae666f9bc9b498227e`, not to the R1 reconciled head.
They are preserved from the original delivery report, not reused as fresh counts.

## Public API inventory

- `ControlChannel.open_metadata(queue_capacity: int = 16) -> C2MetadataStream`
- `C2MetadataStream`: `is_open`, `enable_comms_test_events()`, `receive(timeout_ms: int = 0)`, `counters()`, `close()`, context management and non-raising one-attempt finalization.
- Explicit new exports: `C2MetadataStream`, `C2MetadataKind`, `C2MetadataEvent`, `C2MetadataCounters`, `C2CommandState`, `C2CannotComply`, `C2CommandStatus`, `C2BitControlInterface`, `C2BitType`, `C2BitConfiguration`.
- Frozen discriminated events enforce exact kind/payload agreement. Full uint64 counters are preserved. Public module identity/pickle behavior is `ams_mel`; the pre-refactor compatibility baseline is unchanged.

## Ownership and defensive behavior

Receive calls native exactly once, validates the selected view/payload, deep-copies all values and closes the event before returning. Native status/diagnostics are preserved. Cleanup makes one attempt, never double-closes consumed handles, and never masks an earlier exception. Explicit Close does not retry unresolved native ownership or falsely report success. Metadata owns no Python parent references, does not cancel requests or unregister callbacks, and does not promise callback quiescence. Queued events drain according to native teardown semantics.

Shared `_ffi` validation additionally rejects native-address overflow and unrepresentable tuple storage, including reused Health BIT nested spans. Nonfinite doubles and percentages remain unchanged. Arbitrary aligned nonnull pointers cannot be proven mapped by ctypes; the valid-memory C ABI obligation remains native-side.

## Regression coverage

48 new C2 tests cover all requested existing compiled scenarios: `metadata-rich`, `metadata-overflow`, `metadata-malformed`, `metadata-register-fail`, `metadata-command-status`, `metadata-command-states`, `metadata-nonquiescing-disable`, `comms-high`, and `comms-register-retain-fail`.

Coverage includes full configuration and canonical BIT graph; all reason categories against the current C header; 613-byte description; independent registration, request Wait and callback observations; shared DROP-INCOMING FIFO; exact 3/1 overflow and 6/4 malformed counters; callback allocation failure draining before PROVIDER_FAILED; full uint64 counters; parent/session/request teardown; weak ChannelView services; actual provider DSO unload; deterministic native callback barrier; finalization and context exceptions.

Hostile ABI injection is limited to outputs unavailable from the compiled provider: unknown enums/kinds; null/misaligned/oversized/address-overflow views/spans; corrupt nested ID labels; UTF-8/NUL violations; every populated rich BIT span/text path; poisoned inactive payloads; publication inconsistencies; malformed diagnostics; consumed/retained owner/event cleanup; MemoryError/KeyboardInterrupt plus cleanup failure. Cleanup call counts and preservation of earlier errors are asserted.

### Original-head local qualification

- `make test-native`: **472/472 passed**.
- `make test-python`: **454 tests passed**, warnings as errors; native prerequisite also **472/472 passed**.
- `make test-policy`: **234 tests passed** across seven suites (18 + 18 + 28 + 64 + 75 + 18 + 13).
- Explicit `python3 -W error -m unittest test_ir_c2_metadata test_control_channel test_common_channel test_ir_health_metadata test_ir_nuc test_abi test_import_identity test_package_layout test_ir_image_metadata -v`: **310 passed**.
- `python3 scripts/check_doc_source_integrity.py`: passed (804 vendor files, 3 workflows).
- `python3 scripts/check_final_newlines.py`: passed.
- `python3 -W error -m compileall -q python/ams_mel python/tests`: passed.
- `git diff --check`: passed; clean worktree.
- Original discovered inventory: **354 tests** in 14 modules. All prior class/method identities retained. Integrated Image adds 52; C2 adds 48 → **454** in 16 modules. No existing tests removed or weakened.
- Package compatibility, import identity, acyclic import graph, public object identities and pickle checks passed.

Original-head hosted Bootstrap run [42537757868](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/42537757868)
completed with all nine applicable jobs SUCCESS; the Ada API Documentation
build in run [42537757619](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/42537757619)
also completed SUCCESS. These are historical only.

## Limitations and unresolved semantics

Mock-only qualification: no real-provider/hardware claim. No additional C2
execution/optional callback interfaces, cancellation, unregister, zero-copy,
Python synchronization guarantees or packaging publication. External
serialization remains required; pinned BIT interface names/numeric values 2/3
are preserved without interpreting contradictory prose. Native valid-memory
obligations remain in force; ctypes cannot prove arbitrary pointers mapped.

## PY-MEL-008-R1 reconciliation

R1 follows [the parallel-development convention](dev-parallel-001-parallel-language-development.md).
The complete newly prepended PY-MEL-008 archive section above was transferred
verbatim; removing only that section restored the archive byte-for-byte to
`fa91028804f38d3b2bd15ec8e5e2f80cd4a74a7e` before integrating current main.
No older archive entry was rewritten. Only the C2 qualification/status paragraph
moved from the README; all public examples, return types, mappings and semantics
remain there. No implementation change is intended by R1.

### Integration identities and preservation evidence

- Original PR head: `c3d0505a1f4093d343ba56a15b706768a38e3815`;
  tree: `8c39b1c389354c01c7306aae666f9bc9b498227e`.
- Documentation migration commit: `8f8794c3b50fab6fba198e5c80bf145f2d489713`.
- Ordinary merge commit: `d56e0352a356a68a490a0bb829491d1ca275de16`;
  tree: `ce2257812da17e84649fb86c5714dd1149fad781`.
- First merge parent: `8f8794c3b50fab6fba198e5c80bf145f2d489713`.
- Second merge parent / freshly fetched integrated main:
  `a885d7730dd1b988ca4c4a95ce4864d5c325df68`;
  main tree: `067d90db02dc67d9dcc85f715e68e3bc72653eb8`.

The ordinary `git fetch origin` / `git merge --no-ff --no-edit origin/main`
completed without conflicts after migration. No strategy override, union merge,
custom driver, rebase, amend or forced update was used.

The seven Python implementation/test files below are byte-identical to the
original PR head. All 48 added C2 test methods remain present. AST comparison
preserves every current-main Python test class/method identity. All four kinds,
independent CommsTest opt-in, canonical Health BIT conversion, exact 47-category
domain, ownership/cleanup, public identity and pickle behavior remain intact.

The archive is byte-identical to integrated main, including the PR #178 notice:
SHA-256 `c12df73b6c9ec3605704594aad09a7780cb5b16d8dea615231b6af65ad412884`.
The private `python/ams_mel/_native.py`, native ABI header and export map are
also byte-identical to integrated main, not restored to the obsolete feature
base. All native, Ada, Rust, workflow and policy paths match integrated main.
This preserves Scheduling, Rust C2 metadata, Track CI corrections and all
previous Python Image/Health/NUC work. No tests or exports were deleted.

### Exact feature diff against integrated main

1. `docs/py-mel-008-ir-c2-metadata.md`
2. `python/README.md`
3. `python/ams_mel/__init__.py`
4. `python/ams_mel/_c2.py`
5. `python/ams_mel/_c2_convert.py`
6. `python/ams_mel/_c2_metadata.py`
7. `python/ams_mel/_c2_types.py`
8. `python/ams_mel/_ffi.py`
9. `python/tests/test_ir_c2_metadata.py`

### First reconciled-head local qualification (superseded)

These were measured and repeated at first reconciled head
`6cf2be74bb0237585b70f772eefe250f90e51f5e`, tree
`5384f3940d9932989836d61a66569fb2a7394466`. They are superseded by the
latest-main integration below, not claimed as final current-main counts.
Builds run sequentially to avoid concurrent native relinking.

| Command/check | Actual R1 result |
| --- | --- |
| `make test-native` | PASS: **500/500**, including Scheduling foundation and C11 layout/ABI tests. |
| `make test-python` | PASS: **455 tests**, warnings as errors; native prerequisite **500/500**. Includes all 48 C2 tests and all current-main Python regressions. |
| `make test-policy` | PASS: **234 tests** (18 + 18 + 28 + 64 + 75 + 18 + 13), including documentation/security policy tests and negative controls. |
| `make check-ir-api-coverage` | PASS: offline audit and **49 regression tests**; 127 headers / 148 operations, 47 safe Ada, 13 partial, 19 missing, 69 provider-only, zero native-only. |
| `python3 scripts/check_doc_source_integrity.py` | PASS: **824 vendor files**, **three workflows**. |
| `python3 scripts/check_final_newlines.py` | PASS. |
| `python3 -W error -m compileall -q python/ams_mel python/tests` | PASS. |
| `git diff --check` | PASS. |
| Explicit warnings-as-errors Python regressions below | PASS: **311 tests**. |
| `make test-rust` | PASS: native prerequisite **500/500**; Rust workspace **305 passed, zero failed, seven ignored subprocess entry points**. C2 metadata integration: six passed, one ignored child entry; Scheduling ABI test passes. |
| `make test-ada` with installed GPRbuild/GNAT on PATH | PASS: **45 smoke PASS lines**, including Scheduling foundation and six-family common Channel; its API audit also passes all 49 regressions. |
| Preservation, scope and documentation | PASS: complete original section retained verbatim; README usage retained; exactly nine feature paths; protected/shared source equality; no unmerged index entries or conflict markers; local Markdown destinations resolve. |

Explicit Python regression command (with the worktree's `python` and
`python/tests` on `PYTHONPATH`, test facade in `AMS_MEL_NATIVE_LIB` and compiled
mock directory in `AMS_MEL_TEST_PROVIDER_DIR`):

```sh
python3 -W error -m unittest \
  test_ir_c2_metadata test_control_channel test_common_channel \
  test_ir_health_metadata test_ir_image_metadata test_ir_nuc \
  test_abi test_import_identity test_package_layout -v
```

For the direct Ada command, PATH was prefixed with the existing local
`/home/zboll/.local/share/alire/toolchains/gprbuild_26.0.1_e3f27f25/bin` and
`/home/zboll/.local/share/alire/toolchains/gnat_native_15.3.1_a6af58cb/bin`.
No toolchain or dependency was downloaded. R1 authors no Ada edits; integrated
main's sources remain untouched. Full local pinned `make docs` and
`make test-documentation-source` were not run: the worktree lacks the pinned
enhancer/GNATdoc environment. Fast documentation checks pass in `test-policy`;
fresh hosted Ada API Documentation build remains separately required.

### Latest-main integration after concurrent PR #179 merge

Main advanced during the first qualification/push. R1 inspected the complete
37-file PR #179 delta and integrated it with another ordinary no-ff merge:

- Latest integrated main: `1dc48caf1811051738cba472c09c6515b60645d4`;
  tree: `6e05ef035a593a38588c3b45a120d77a0e4f51fc`.
- Latest merge: `25b99196891a64d2300286bc54acc452f984eea7`;
  tree: `e627507037604f3d549243d1e662ede8504ca417`.
- First parent: `6cf2be74bb0237585b70f772eefe250f90e51f5e`.
- Second parent: `1dc48caf1811051738cba472c09c6515b60645d4`.

The merge was conflict-free. The exact nine-file feature inventory above remains
unchanged. All Python C2 implementation/test bytes still match the original PR;
all native, Ada, Rust, workflow and policy paths match this latest main. In
particular, Scheduling ExecuteTaskAck exports, ABI declarations, Python private
ctypes mirrors and native/Ada/Rust tests are preserved. The archive remains
byte-identical to latest main with the same SHA-256 above.

Fresh sequential qualification on this latest merged source measured:

| Command/check | Latest-main actual result |
| --- | --- |
| `make test-native` | **523/523 PASS**, including Scheduling foundation and all 23 ExecuteTaskAck scenarios/layout checks. |
| `make test-python` | **456 PASS**, warnings as errors; native prerequisite **523/523 PASS**. |
| `make test-policy` | **234 PASS**, same seven-suite breakdown above. |
| `make check-ir-api-coverage` | Audit and **52 regressions PASS**; 127 headers / 148 operations, 48 safe Ada, 13 partial, 18 missing, 69 provider-only, zero native-only. |
| Source integrity / final newlines / warnings-as-errors compileall / whitespace | All PASS; integrity still **824 vendor files / three workflows**. Exact commands are listed above. |
| Explicit nine-module warnings-as-errors Python regression command above | **312 PASS**. |
| `make test-rust` | Native prerequisite **523/523 PASS**; workspace **306 passed, zero failed, seven ignored subprocess entry points**, including C2 metadata and both Scheduling ABI suites. |
| `make test-ada`, same installed toolchain PATH above | **46 smoke PASS lines**, including Scheduling foundation, ExecuteTaskAck and six-family common Channel; audit and **52 regressions PASS**. |
| Feature preservation / scope / archive and protected-file equality | PASS against latest integrated main; no test or export deletion, unmerged entry or conflict marker. |

The final report-only commit is followed by another full sequential qualification
at its exact committed HEAD. The delivery PR records that final HEAD/tree and
actual rerun results; no older counts substitute for those measurements.

The first reconciled head's fresh Documentation run
[42754906035](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/42754906035)
completed SUCCESS, while Bootstrap run
[42754906278](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/42754906278)
was still running when the second integration began. These runs do not qualify
the latest source and are explicitly superseded.

### Hosted handoff

The delivery PR records final commit/tree identities, the normal push, exact-head
Bootstrap and Ada API Documentation run links and observed conclusions. Historical
green checks above do not qualify the reconciled head. If fresh checks are still
running, disposition is `HOSTED_CI_PENDING`, not merge-ready. Readiness requires
all applicable Bootstrap jobs and the documentation build SUCCESS, unchanged source
identities, no unresolved blocking review issue and GitHub mergeability.
PR #174 remains open and non-draft; no self-approval, auto-merge or PR merge is
authorized or performed.
