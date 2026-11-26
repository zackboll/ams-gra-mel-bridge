# MEL-GNATDOC-BODY-001: Ada specification and body qualification

**This task does not authorize production implementation-body publication.**
Production remains on schema 1. The independent check and the official
v0.1.8 enhancer agree on the schema-2 candidate for this source revision.
The [v0.1.8 release pins and schema-1 qualification](mel-gnatdoc-018-001.md)
remain unchanged.

## Qualification mechanics

`docs/tools/qualify_body_view.py` is an explicit, non-deploying entry point.
It uses the exact Git revision and two independent disposable exports. Each
generation produces one pristine GNATdoc 26.0.0 input and compares complete
disabled, schema-1, and schema-2 sites, assembled with independently generated
Rustdoc and Python documentation. Schema 1 restores every native HTML byte and
independently validates the search index, router, symbols, local types, authored
HTML and API Map. The body audit restores both native transformations and
requires equality outside exact source pages, approved link insertions and
corresponding metadata. It checks the complete Ada link graph without relaxing
its policy. The enhancer must be exactly idempotent against its complete Ada
output; the complete assembled file fingerprints must match across exports.

The schema-2 effective TOML and evidence exist only inside the export. The
digest binds the five-field source-owned ledger (`schema_version`, `mode`,
`files`, `sources`, `bodies`). Source ownership comes from the leading Ada
unit declaration, the complete named unit boundary, source domain checks,
and compile-only verification of each source identity. There is no body
allowlist. No output manifest or regenerated output hash grants authority.

MEL uses its tokenizer for exact static source pages and separately examines
complete callable profiles. It requires positional parameter names, modes,
lexically supported and resolved subtype/return marks, and equal supported
defaults (integer literals). Fresh GNAT cross-references must independently
bind the exact defining tokens for the declaration and implementation. The
MEL-specific scan assumes the repository's canonical direct-declaration
layout; unfamiliar layout or unsupported source fails qualification rather
than falling back to ordinal, name, or suffix matching. GNAT is an additional
check, not permission to treat the enhancer's lexical rules as a compiler.

## Coverage and omissions

Measured from `e32457e1db65f92914bcee67bfbe37cb1282ce57` (PR #261 remains an
ancestor). The diagnostic JSON records each specification and body path, exact
package spelling, SHA-256 of the complete source bytes, byte/line counts,
eligibility, and per-package coverage. Its file maps identify every assembled
publication file by hash, not by an archive or source excerpt.

| Group | Public specs | Eligible bodies / pages | Callable declarations | Direct destinations | Omitted | Spec lines | Body lines | Overloaded names / declarations | Verified overload destinations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Common | 3 | 2 | 126 | 14 | 112 | 1,388 | 555 | 11 / 34 | 10 |
| IR | 32 | 30 | 582 | 202 | 380 | 8,758 | 9,962 | 70 / 175 | 40 |
| RF | 11 | 11 | 396 | 183 | 213 | 4,893 | 6,140 | 28 / 65 | 32 |
| Total | 46 | 43 | 1,104 | 399 | 705 | 15,039 | 16,657 | 109 / 274 | 82 |

There are direct body destinations for 36.1% of the public callable inventory.
This is a documentation-navigation metric, not Ada implementation coverage.

| Omission | Count | Independent interpretation |
| --- | ---: | --- |
| Implementation expression | 601 | Expression completions are deliberately not destinations. |
| Unsupported or unresolved spec profile | 93 | Examples: named/real/string defaults, class-wide or anonymous access profiles, and marks that need additional visibility rules. |
| Unsupported or nonconformant body profile | 8 | Spec marks are established, but the body spelling depends on a local alias or use visibility. |
| Ambiguous body candidates | 2 | The aliased `Close` helper in `AMS.MEL.IR.Image` conservatively overlaps the supported public profiles. |
| Specification renaming | 1 | `AMS.MEL.RF.C2.Add_Job_Intervals` is not an ordinary declaration/body pair. |

The authentic Image and Scheduling journeys derive lines from the actual inputs.
`Navigation_Report` is specification-only; `Submit_Navigation_Report` and
`Exec_Stare` have ordinary bodies; the Abort and Track List `Wait` overloads
have distinct destinations, while the Track, Scan and Stare expressions do not.
Other browser controls cover `AMS.MEL.Close`, `AMS.MEL.ABI_Version`,
`AMS.MEL.RF.Admin.Open`, and all bodyless packages.

## Visibility and authorization

The eligible inventory covers 924,424 specification bytes and 700,899 body
bytes. `AMS`, `AMS.MEL.IR.Line_Of_Sight_Quaternion`, and `AMS.MEL.IR.NUC`
have no eligible bodies. Eleven private specifications (including the C
binding) and ten corresponding private bodies are excluded. The complete
eligible files expose private helpers, comments, FFI/marshalling and error
handling, data representation and internal implementation choices; a
navigation omission does not imply that its text is hidden. No unsupported
library-unit boundary was found in the selected tree. The source is already
in the public repository with Apache-2.0 license files; a spot check found no
apparent confidentiality label in body files. This is neither a secret scan
nor a determination that redistribution of the generated site is authorized.
Before any production opt-in, obtain explicit review of all complete eligible
bodies and licensing/attribution obligations. If any body cannot be published,
do not enable this mode against the current source tree or omit it from the
evidence to simulate an allowlist.

## Reproduce

1. Start from a clean checkout of the exact branch commit. Run
   `sh docs/tools/install_docs_toolchain.sh`; this hash-checks the official
   v0.1.8 wheel and verifies the installed package fingerprint.
2. Use an isolated Python environment with Playwright 1.64.0 and its matching
   Chromium (`playwright install --with-deps chromium`).
3. From the repository root, run the following, using a fresh diagnostics
   destination outside the checkout or in the ignored `.doc-tools` directory:

   ```sh
   ALIRE_SETTINGS_DIR="$PWD/.doc-tools/settings"
   .doc-tools/bin/alr -n -s "$ALIRE_SETTINGS_DIR" -C docs/toolchain exec -- sh -c '
     cd ../..
     .doc-tools/enhancer/bin/python -W error docs/tools/qualify_body_view.py \
       --gnatdoc-prefix .doc-tools/gnatdoc-26.0.0 \
       --browser-python .doc-tools/browser/bin/python \
       --output .doc-tools/body-diagnostics'
   ```

The `documentation-body.yml` workflow runs the same entry point on manual
dispatch or a push to the exact dedicated qualification branch (so it can
run before GitHub permits dispatch from a default-branch definition). It
has no PR or main-push trigger, uses read-only permissions, checks the exact event commit, and uploads only
sanitized JSON: file fingerprints, counts, source paths/line numbers, mutation
results and browser observations. It has no deployment path. A run does not
pass unless it writes `result.json` with `status: passed` after both builds and
both Chromium campaigns. Hosted diagnostics never include body pages, complete
body text, compiler excerpts, or a schema-2 publication archive.