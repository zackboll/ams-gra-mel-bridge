# DOC-007A focused source research

Starting main: `ddc63bf6883d16403db5fb23cd4a087ff5ebcf5c`; reviewed corrective
commit `87fe97479ca0b82b8d7f157afb9c12589fff3871` is an ancestor. No main
advancement existed at initial fetch. No matching branch/worktree/PR existed.
Previous documentation worktrees and evidence were preserved.

## Tooling and pristine reference

Read the repository instructions, architecture, ABI policy and coverage, then
the generator, validator, coverage/baseline/integrity checkers and their tests,
native anchor/exception manifests, documentation workflow, foundation report,
root/Ada READMEs, Makefile and both Ada Alire manifests. Ambient GNATdoc 27.0w
was explicitly rejected. The existing isolated GNATdoc 26.0.0 installation and
Alire GNAT Native 16.1.0/GPRbuild 26.0.1 installation generated a pristine site
before portal edits. Exact version output and API bytes are preserved externally.

## Claim-to-source map

| Guide | Current source and evidence inspected |
|---|---|
| Getting Started | `Makefile`, `ada/README.md`, `ada/alire.toml`, `ada/tests/alire.toml`, `docs/bootstrap-validation.md`, `docs/packaging.md`, `scripts/check.sh`, `scripts/format_ada.sh` |
| Architecture | `ada/src/ams-mel.ads`, private C imports, `native/include/ams_mel/abi.h`, `native/src/provider.cpp`, native IR/RF state implementations; `docs/architecture.md`, `docs/c-abi-policy.md`, `docs/upstream-provenance.md` |
| IR workflows | `ada/src/ams-mel-ir*.ads`, Image Receive/Acquire/borrow/Close and navigation contracts; C2 and conditional metadata/Track contracts; `ada/README.md`, Task 029 provenance |
| RF workflows | `ada/src/ams-mel-rf*.ads`, VA/Job Submit/Wait/Claim and ProductRx format/queue/metadata contracts; Task 034 evidence and upstream provenance |
| Ownership/errors | Public limited-owner and result preconditions, matching Ada finalization bodies and native claim/cache/cleanup implementations; ABI policy |
| API map | `gnatdoc_inventory.specifications`, all 29 public library-unit headers, actual generated h1 headings, `docs/gnatdoc-native-anchors.json` |

All website source citations are repository-relative code text, not broken
published links to files outside the site. Package hyperlinks use verified
substitution, never authored opaque hashes. Illustrative flows are explicitly
not standalone compilable examples. No provider configuration is invented.

Historical reports contain earlier scope snapshots. Guides use current public
specifications, not stale historical exclusions. In particular Image's complete
header representation still uses the Mono8 single-band receive path; Track's
positive evidence is mock-backed; RF integer/VITA receive is not PDW/LF receive
or packet sample decoding. Local pins are not registry-publication evidence.

## Checkpoint 007A1

Commit `87b5ae9` preserves a minimal functional portal, strict manifest/builder,
validator extension and narrowly authorized tests. 86 documentation tests passed;
1,162 protected files passed source integrity; native API output compared equal
to pristine. The first test attempt exposed noncanonical guide-relative links;
the builder was corrected without broadening traversal acceptance. Failed output
is retained in the external evidence directory.

## Evidence location

`/home/zboll/git/ams-mel-doc-007a-evidence/r1` (not published as site content).

## DOC-007A-R1 guide-contract correction

Reviewed starting head: `69d6f1947bcfb4f9677dcc24a279f2387b002174` on PR #110.
The original broad source survey did not prevent four inaccurate guide summaries;
the following public declarations are the authority for their correction:

- `ada/src/ams-mel-ir-c2-metadata.ads`: `Receive` returns `Metadata_Event`.
- `ada/src/ams-mel-ir-image-metadata.ads`: `Receive` returns `Metadata_Event`.
- `ada/src/ams-mel-ir-health_status-metadata.ads`: `Receive` returns `Metadata_Event`.
- `ada/src/ams-mel-ir-instrumentation-metadata.ads`: `Receive` returns `Instrumentation_Report`.
- `ada/src/ams-mel-ir-track-metadata.ads`: `Receive_Event` returns the tagged,
  discriminated `Metadata_Event`; `Receive` is report-only and consumes a
  non-report event before raising `Provider_Error`.
- `ada/src/ams-mel-rf-c2-virtual_aperture_notifications.ads`:
  `Wait_For_Change` consumes a coalesced pending signal, without a provider
  status snapshot or a guarantee that a later query differs.
- `ada/src/ams-mel-rf-c2-virtual_aperture_queries.ads`: `Query_ID`,
  `Query_Status`, `Query_Instance_Status`, `Cached_Waveform_Supported`,
  `Dynamic_Weights_Supported`, `Snapshot_All_Instances`, `Snapshot_Instances` and
  `Snapshot_Instance_Status_Report` define the query/copy surface, not RF
  physical-data configuration.
- `ada/src/ams-mel-rf-c2-local_functions.ads`: `Snapshot_Local_Functions` and
  `Snapshot_Local_Function_Status` are conditional catalog/status queries;
  they neither execute Local Functions nor perform register writes.
- `ada/src/ams-mel-rf-c2.ads`: `Create_Local_Function_Command`,
  `Append_Local_Function_Write` and `Append_Interval_Local_Function_Command`
  build command configuration in the parent C2 package, not the query child.

Only the IR workflow, RF workflow and API map prose and these task-owned reports
change. Existing package-link placeholders are preserved. The API map's repeated
LF scope and notification descriptions are aligned with the RF workflow correction.
Corrective evidence is under
`/home/zboll/git/ams-mel-doc-007a-evidence/r2-guide-contracts`; r1 is preserved.
