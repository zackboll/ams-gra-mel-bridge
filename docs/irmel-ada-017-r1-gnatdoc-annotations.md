# IRMEL-ADA-017-R1 — public GNATdoc annotations

## Scope and audit

Correction to existing PR #202 on
`feature/irmel-ada-017-common-comms-metadata`, starting from
`3c8b25579cfa5d54ae4e24b568c26c729a98e899`. History is append-only; the PR is
left open and unmerged, with auto-merge disabled.

The sole Ada change is 33 added comment lines in
`ada/src/ams-mel-ir-channel-metadata.ads`. The repository uses GNATdoc 26 trailing
comments with public generation. Added all seven parameter descriptions, four
return descriptions, three record-field descriptions, the limited-private stream
description, and exceptions matching the existing bodies. `Is_Open` only tests
local handle presence and has no explicit exception path to annotate.

Every original explanatory comment is preserved byte-for-byte, including one-shot
registration, partial callback installation and lifetime, FIFO drop-incoming
behavior, draining after typed closure, timeout without cancellation, and
independence from request-future completion. A comment-stripped source comparison
against the starting commit confirms unchanged declarations. No ABI, body,
behavioral test, CI trust policy, warning threshold, or coverage classification
changes. Historical reviewed source hashes remain historical, not current locks;
the existing coverage checker passes without inventory edits.

## Qualification

Commands run from `/home/zboll/git/ams-gra-mel-bridge`:

| Command / inspection | Result |
|---|---|
| `make format-ada` | PASS; no unrelated source changes. |
| `make check-ada-format` before validation and again after Ada tests | PASS. |
| `alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge test-ada` | PASS; production facade, separate mock providers, all 62 coverage controls and full Ada smoke suite, including communications metadata reception. |
| `make test-docs-ci` | PASS; 18 documentation CI, nine static-subtype and 13 coverage controls, 40 total. |
| `python3 scripts/check_ir_api_coverage.py` | PASS; 50 safe-Ada unique operations, unchanged classifications/report. |
| `python3 scripts/check_doc_source_integrity.py` | PASS; 825 vendor files, four workflows, ordinary development policy. |
| `python3 scripts/check_final_newlines.py`; `git diff --check` | PASS. |

Pinned generation command (the output is ignored, not committed):

```sh
alr -n -C ada exec -- make -C /home/zboll/git/ams-gra-mel-bridge docs \
  GNATDOC_PREFIX=/home/zboll/git/ams-mel-ada2/.doc-tools/gnatdoc-26.0.0
```

PASS on annotation commit `ac3deaf978323ae3ea77a9e24004ff3ade36b1f2`.
The existing generator first compiles the public documentation project, then runs
GNATdoc with `--generate public --style trailing --warnings`. Tool identities:
GNATdoc **26.0.0 (20260415)**, GNAT **16.1.0**, GPRbuild **26.0.0 (2026-04-15)**
(Alire package 26.0.1), and the repository's verified enhancer **0.1.5** installed
using the unchanged pinned-wheel installer. The ambient GNATdoc and a previously
installed sibling enhancer did not match the pins and were not used.

Generation and manifest-bound `docs/tools/docs_ci.py verify` both passed.
The report records **5016/5048** documented elements (**99.3661%**) and **124**
warnings across the project, informational under the unchanged build contract.
The only warnings for this package are the existing private `Finalize` and its
`Owner` parameter; no requested public declaration/parameter/return/field warns.

## Generated-page inspection

Inspected both pristine native and enhanced published package pages:

`api/2c2126912f724c50d198145700a97e56a3f085e11932e3b3eb65c42a3f43bcda.html`

An HTML-parser inspection independently checks each entity and subsection, not
merely the presence of text anywhere on the page:

- `Open`, `Is_Open`, `Receive`, `Counters`: **Parameters** and **Return Value**.
- `Close`: **Parameters**.
- `Metadata_Counters`: **Record fields**, all three names and descriptions.
- `Open`, `Receive`, `Counters`, `Close`: **Raised exceptions**, with
  `Provider_Error` and Receive's `Timeout_Error` / `Stream_Stopped` in the correct
  entity. No fabricated exception section for `Is_Open`.
- `Comms_Metadata_Stream`: attached ownership/lifetime and future-independence
  description.

All checks passed in both versions. Native page SHA-256:
`864acf1fe7b4a771c74f07109414fe65985797bb101a35a77a231df2c84913a3`.
Annotated spec SHA-256:
`ee13b99fb773a5b46e5a743aa82ae9e6388e31e13af29797f0d7410d9bd3791d`.

Final HEAD and its regenerated documentation qualification are recorded in the PR
follow-up after this evidence is committed, avoiding a self-referential commit ID.
This documentation-only correction does not change the existing
`PRIVATE_PROVIDER_INTEGRATION_UNTESTED` limitation.
