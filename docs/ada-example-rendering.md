# Canonical Ada example rendering (DOC-011)

The nine executable programs and their supporting files remain the only source
of the 31 complete listing occurrences. No example Ada, execution manifest,
dependency, vendor snapshot or production implementation changes in this task.
The renderer is static Python standard-library code; the resulting listings and
ordinary relative links work without JavaScript, fonts, third-party CSS or a CDN.

## Display grammar

`docs/tools/ada_examples.py` scans left to right, retaining exact character
offsets. Whitespace (including CRLF), Unicode and every source character survive
reconstruction. Comments end at CR or LF; strings recognize doubled quotes;
character literals and attribute/qualification ticks are context-sensitive.
Numbers include underscores, fractional/exponent forms and Ada based literals.
Ada reserved words are case-insensitive. Attributes, comments, strings,
characters, numbers, operators and punctuation receive `ada-*` spans. Ordinary
identifiers remain plain text. Unknown characters remain visible, not discarded.
This is a display lexer for compiled source, not an Ada legality checker.

The only additional display wrapper is an empty
`<span class="ada-source-end" aria-hidden="true"></span>`. Its zero-width inline
box retains the final source newline in Chromium selection/copy serialization.
It contains no character and introduces no accessible or generated text. Without
styles, the complete underlying source is still present.

`example_sources.Listings` accepts only the explicit token spans, this end marker,
and `a.ada-reference` with exactly the approved attributes. It checks balanced
nesting and reconstructs the canonical source. `verify_rendered` additionally
requires exact canonical annotated HTML for every occurrence, in manifest order.
It rejects alternate/malformed entities, altered token classes, stale sources,
extra attributes, inconsistent repeated support, arbitrary HTML and forged links.
Merely removing HTML tags is not sufficient authorization.

## Supported references and limits

* Ordinary single-package `with` clauses before the compilation-unit header may
  link their complete public MEL package name to its actual native package page.
* Selected public type/subtype names in explicit type-mark positions (object or
  parameter declarations, returns and `use type`) may link the final identifier
  to its actual native declaration.
* Fully qualified selected operations, or operations through a proven package
  renaming, link only when the public package has exactly one declaration with
  that name and the source occurrence is followed by a call parenthesis.
* A unit-level `package Alias renames AMS.MEL...;` must be in the initial sequence
  of package renamings and use clauses, before any other declaration or body,
  name an explicitly imported public package, and be unique.
  Any other standalone occurrence of the alias anywhere in the unit blocks that
  alias globally. This conservative check includes formals, objects, comma lists,
  loop names, nested declarations and additional renamings. Member names in a
  qualified name do not constitute an alias definition. An `AMS` shadow blocks
  fully qualified references and aliases. References before the alias remain
  unlinked. The alias declaration span and qualification are recorded in evidence.

No use-clause search, inferred types, overload selection, inherited-operation
resolution, generic instance resolution, alias chaining, shortened package suffix
matching or general Ada semantic resolution is claimed. Ambiguous overloads such
as C2 `Close`, `Wait` and `Claim` remain plain text even when a human could infer
the overload. Unknown/private declarations are not destinations. Unsupported
context-clause syntax fails closed for links, not source display. Additional
forms require a separately tested source/scope proof, not a wider regex guess.

## Independent authority

`References` reuses the source-owned `local_type_audit.Authority`: public library
units and direct public declarations come from the current Ada specification
inventory, identities include the declaration profile, and destinations come from
the approved `gnatdoc-native-anchors.json` ledger. The actual pristine native
output must contain each owning package and anchor, with the approved native
profile. No opaque page name is hardcoded in the example resolver.

The publication validator also pins the complete source, ledger, pristine API,
enhanced API, original ordered 1,418-link audit and ten required additions. A
candidate presentation manifest, generated `data-*` value or claimed link count
cannot authorize any occurrence or destination. `occurrence_ledger` recomputes
all example occurrences from source/native authority, recording source SHA-256,
listing index, character offsets, line, lexical context, public identity, alias
proof, qualification, source declaration, native page/anchor and canonical href.
The ledger is evidence, not an input to authorization.

The live verifier qualifies the complete local publication before HTTP, compares
decoded bytes for every one of the 43 entities, verifies the served graph and
native fragments, and reconstructs all 31 listings with pristine authority. Its
redirect, MIME, retry, stale-content and read-only contracts are unchanged.

## CSS and accessibility

The existing portal panel and link palette is retained. Links have underlines
and keyboard-visible outlines. `white-space: pre` and horizontal scrolling retain
indentation without mobile document overflow. No `content`, resource URL, import,
inline CSS or script is added. The empty end marker has no generated content.

Measured Chromium sRGB normal-text contrast against the actual code panel:

| Token | Light (`#eef3f8`) | Dark (`#223245`) |
| --- | ---: | ---: |
| Keyword | 6.73:1 | 7.33:1 |
| String / character | 6.61:1 | 8.57:1 |
| Comment | 6.41:1 | 7.28:1 |
| Number | 6.61:1 | 8.35:1 |
| Attribute | 7.46:1 | 7.75:1 |
| Operator | 8.02:1 | 8.66:1 |
| API link | 6.48:1 | 7.43:1 |

All exceed WCAG AA 4.5:1 without relying on bold weight. DOM text and native
selection are separately checked; visual inspection does not establish source or
destination identity. The browser matrix covers desktop/mobile, both themes and
JavaScript on/off. This is not a claim of a full assistive-technology audit.

## Rebaseline and extension procedure

DOC-009 baseline main is `36ecb1e63c24df855d6f6e85f8245e16fc9d8d53`.
DOC-010 has not merged and is neither changed nor required. The existing enhancer
revision remains pinned; adopting upstream symbol bookmarks is a separate task.

The approved new delta is **266** links: 31 context-package, 99 type/subtype and
136 unique-operation occurrences, including repeated supporting-source copies.
There are **4,411** syntax-token spans across the 31 listings, plus 31 empty end
markers. Pristine references increase from 1,562 to 1,828; enhanced references
increase from 3,021 to **3,287**. The 43-file publication inventory, 38 HTML pages,
29 packages, 1,237 native sections and 1,428 API type links do not change.
Only `guides/examples.html` and `assets/portal.css` change publication bytes.
Every native/enhanced API file and the presentation manifest remain byte-identical.
The old complete pristine fingerprint
`d076b4e9c69f4ffd7c02e82da711704ca90077a82f0db229176492644e0a903d`
becomes `3a853e1a6c06e19f7f21d45622de3135054e7beb711245b1838555246c0d6c49`.
The new complete publication fingerprint is
`54969299f3009717667e77806582c67eb6c96c6ec43156f12e6094ac1da26aee`.
Both API-only fingerprints and the original ordered audit fingerprint remain
unchanged. Qualification retains all 400 original documentation test identities
and adds 40 DOC-011 tests (440 total), rather than counting repeated subsets.

When adding examples or reference support:

1. Add real compiled canonical sources and explicit execution cases through the
   existing inventory contract; do not paste alternate documentation snippets.
2. Add lexical, scope and mutation controls before extending resolution. Exercise
   shadowing, literals, ambiguous operations and wrong existing native targets.
3. Recompute occurrence evidence from pristine authority, not candidate markup.
4. Rebuild twice, verify source fidelity, execute examples and compare the entire
   publication inventory against the previous independently qualified output.
5. Derive reference deltas from the audited occurrences, then update explicit
   count/fingerprint contracts. Never change the API-only hashes to accommodate
   an unrelated portal change. Preserve historical DOC-008/009/010 evidence.
6. Run the repository and documentation suites, strict HTTP fixture verification
   and the real browser matrix. Review the changed-file inventory before publishing.
