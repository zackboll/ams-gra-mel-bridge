# Minimal upstream request: source-backed authored HTML blocks

This is a proposed contract, not implemented functionality or a request to add
general template execution. v0.1.5 remains unmodified. The authentic regression
oracle is the MEL source/native pair and the complete production reconstruction
retained by MEL-GNATDOC-015-002, not a manifest that approves itself.

## Input contract

Add one opt-in source-backed block capability to `authored-html`, using a new
versioned contract rather than changing schema-2 semantics. Suggested extension:

```json
{
  "source_blocks": {
    "profile": "executable-ada-catalog-v1",
    "manifest": "docs/examples/manifest.json",
    "project": "docs/examples/ams_mel_doc_examples.gpr",
    "guide": "guides/examples.html",
    "catalog_guide": "guides/api-map.html",
    "symbol_policy": "stable-public-types-and-root-constants-v1",
    "use_policy": "qualified-types-and-unique-operations-v1"
  }
}
```

The enclosing contract binds these files and every owned main/support file by
SHA-256; config binds the contract digest. Inputs remain exact regular Git-export
files with no symlink ancestors. Independently regenerated native hashes and
complete profile/anchor correspondence remain mandatory. Consumers must compare
the complete contract against independently derived Git/source/native authority,
not merely trust its matching digest. No callbacks, executable commands, Python
imports, arbitrary rendered-HTML fields or externally fetched input are accepted.

The example manifest is declarative schema 1 with ordered entries containing
`id`, `title`, `purpose`, `main`, `support`, `provider_family`, `mock_library`,
`guide`, `packages`, `cases`, `executable` (existing MEL format). Cases contain
`id`, `configuration`, `provider`, `exit`, `expected`. Read these as text only;
never execute build/run commands during rendering. Verify main ownership using
the literal GPR Main and Source_Dirs; support files must belong to the exact
source inventory. Each example ID has exactly one token and destination section;
every source has explicit ownership, and no unknown files are silently included.
Executable qualification remains a separate consumer test with loader evidence.

Only three additional token forms are permitted, at their declared guide:
`{{example:known-id}}`, exactly one `{{public-symbol-index}}` and exactly one
`{{example-usage-index}}`. Reject unknown IDs, duplicates, missing tokens and
every other template. Do not accept a caller-provided list of catalog rows as
authority. Derive symbols from public source declarations and independently
matched native anchors. Stable public symbol selection and conservative example
use resolution must reproduce MEL's existing policies, not infer overloaded
operations by name, private members, shadowed aliases or ambiguous declarations.

## Output contract

For each example token: complete canonical main bytes with current Ada span/link
serialization; owned support listings in `<details><summary>Supporting source:
PATH</summary>…</details>`; exact canonical source labels; and declarative case
instructions/expected observations in manifest order. Escape all source text.
The complete code text including final newline must round-trip to original bytes.
No compiler-success claim follows from source equality.

For each catalog token: exact current section IDs/headings, Shared/Core/IR/RF
ordering, source ownership, public symbol destinations and verified example-use
associations. Include every eligible row once in source order within the existing
scope/group ordering; preserve canonical no-use wording and link serialization.
The native anchor omission exception remains omitted, never fabricated.

Permit `<details>` and `<summary>` as inert elements with bounded attributes;
permit `data-example-source` only on generated `<code>` with verified source
identity, and `data-catalog-entry`/`data-kind`/`data-scope` only on generated
catalog `<li>` with independently recomputed row evidence. Permit only the exact
generated `<span class="ada-source-end" aria-hidden="true">` marker. Do not add
a blanket `data-*`/ARIA allowlist, disable strict HTML checks or allow scripts,
event handlers, active resources or unsafe URLs. Validate actual generated rows,
links and source text, not just their annotation syntax or hash.

All seven guide files, landing and stylesheet must be byte-identical to the
existing consumer output for identical source/native inputs. Complete API output
must remain identical except the explicitly documented presentation-manifest
extension that records the new guide outputs/input identity. No duplicate
consumer feature rendering path is an acceptable production implementation.

## Required regression acceptance

1. Run the authentic **seven** guides from the new source epoch, not upstream's
   old six-guide MEL fixture. Require all nine portal outputs byte-identical.
2. Compare complete public package ownership, profiles/native anchors, stable
   symbols, every type occurrence, all catalog rows/use associations and every
   canonical source listing against independent consumer reconstruction.
3. Reuse the pilot's 18 rejection cases, including resealed source/native swaps;
   add missing/duplicate example tokens, cross-guide tokens, reordered/missing
   catalog rows, ambiguous/shadowed use, incorrect support ownership, extra attrs,
   malicious source text, duplicate source IDs and forged execution claims.
4. Strict allowlists must still reject executable HTML/templates and invalid
   source/catalog annotation values. Verify full pipeline URI/link validation,
   not just element parsing.
5. Add an ordinary public declaration in an isolated source fixture, regenerate
   native output and the contract, then verify success and catalog completeness.
   The old native digest must fail on the new native output. A resealed forged
   contract must still fail independent authority. No historical count gates.
6. Require deterministic repeat generation, enhancer idempotence, atomic failure,
   full output inventory and independent publication reconstruction. Complete
   candidate browser and hosted consumer qualification precede adoption.

Consumer follow-up work is limited to source-epoch contract derivation/verification,
the versioned capability opt-in, new enhancer pin and removal of the superseded
rendering path after parity. No upstream build farm, service, deployment redesign,
general template language or production API change is needed for this request.