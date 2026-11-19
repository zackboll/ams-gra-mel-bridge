# MEL-GNATDOC-SOURCE-001: offline Ada specification browsing

## Dependency and scope

The documentation-only installer uses the official GNATdoc Enhancer **0.1.7**
wheel, never an editable/development installation. Release commit:
`5efaea9ac018101f33ac5b68b1b98ab4b3e170f6`; tree:
`733c472ab4c3a570f2156487c3cde7b6fd0c187a`.

Wheel SHA-256:
`957c65797b03b5c6b3779e2a1918730a2e44749670e37535d2c8ff3e109f3ba4`.
Complete installed package/module/asset fingerprint, using MEL's existing
sorted path/hash JSON procedure:
`f6c8557c71092a2ef741f1d534a5948cdcef3805244306fb0b8424f51011ff04`.
The installer retains hash-required binary-only pip installation, PEP 610 URL
and archive digest checks, distribution/import-origin checks, a complete
implementation/asset audit and an exact CLI version check.

No MEL runtime or upstream enhancer implementation is changed.

## Source-owned opt-in and evidence

The tracked `[source_view]` table in `docs/gnatdoc-enhance.toml` expresses the
schema-1 `public-specifications` contract. Its zero digest is an intentionally
unusable sentinel, like the existing authored-HTML digest, not an authorization
to render. Directly loading that tracked configuration must fail closed.

`source_epoch.export()` exports the exact immutable Git revision. Native
generation first compiles the documentation project with the pinned toolchain,
then invokes GNATdoc 26.0.0 with public visibility and trailing comments. MEL's
coverage/native reconciliation validates its pristine declarations and anchors.
`source_view_audit.evidence()` independently inventories eligible public
specifications and hashes their complete bytes and every pristine native API
HTML/CSS file. `prepare()` writes canonical schema-1 JSON to
`docs/gnatdoc-source-native.json` **only inside the disposable export**, computes
and checks its SHA-256, and creates `docs/gnatdoc-effective.toml` with that binding.
It does this before any relevant enhancer configuration load, including search,
idempotence reconstruction and resize-disabled control builds. Neither generated
file is committed, copied into the publication or an output-side authority.

The independent verifier derives the expected evidence again from source/native
inputs. A publication manifest or a recomputed publication digest cannot authorize
different source bytes, owners, declaration lines, native anchors or overloads.
Source changes therefore do not require maintaining a stale committed manifest.

## Visibility boundary and navigation

**The complete selected public `.ads` files are published, including private
sections, comments, C imports and representation clauses within those files.**
Private sections are not redacted and must not contain confidential material.
Private library-unit specifications and all `.adb` implementation bodies remain
excluded. Selection uses library-unit headers, not filenames alone. The existing
public specifications already reside in this public repository; this opt-in makes
their entire text directly accessible in the documentation publication.

Supported declaration headings and package headings gain **View source**. These
links leave the existing iframe (`target="_top"`) for a directly addressable
`api/source/ada/src/<specification>.ads.html#L<number>` URL. Browser Back returns
to the API. Every physical source line has an ordered `L` anchor and a
keyboard-focusable line link. CSS-generated line numbers do not enter copied
source text. Source browsing itself needs no JavaScript or external resources.

Upstream source browsing remains **disabled by default**. Its conservative
declaration support does not claim a full Ada resolver: supported public types,
subtypes, functions/procedures and narrowly supported flat-package constants
receive links; unsupported constructs receive no guessed destination. Bodies,
spec-to-body mapping and arbitrary repository browsing are unsupported.

## Independent verification and qualification

`source_view_audit.py` reconstructs every source page byte from approved source,
including UTF-8 text, whitespace, CRLF references, ordered anchors, owner/path,
title and source hash. It pins the official source stylesheet digest. Declaration
destinations use MEL's complete-profile native reconciliation, not declaration
name matching or overload order. Only exact expected heading/link and stylesheet
insertions are reversed before existing presentation/type/syntax restoration.
Every original native page must be restored byte-for-byte. Source pages are new
outputs, not native pages to restore.

Escaped HTML-like comments and path-like strings are admitted only as exact
reconstructed source bytes; no general active-content exemption is introduced.
The existing graph policy still rejects scripts, external resources and active
attributes. Inventory, manifest correspondence, authored output, search, resize,
Rustdoc and Python checks remain in force.

The non-deploying `documentation-source.yml` workflow runs independent repeated
generation, complete source-disabled/source-enabled assembly comparison, mutation
controls, the dedicated Chromium source campaign and the existing exhaustive
search browser campaign. `source_view_parity.py` classifies only source pages,
the official stylesheet, declaration-page insertions and manifest changes. All
other assembled bytes, including nine authored outputs and Rust/Python trees,
must be identical. Existing deployment remains main-push-only; this PR must not
deploy, merge, approve or enable auto-merge.

## Qualification status

Starting main: `63008c88e8987842a795ad61a7c3c739f3549c47`, tree
`1d151d1b4c9cd8ea08549cdc47d51bc09cfd8a22`. Referenced starting Documentation,
Bootstrap, Resize, Search and Authored HTML runs all ultimately completed with
`success`; their final conclusions were queried, not inferred from pending jobs.
The released wheel was independently downloaded and its actual bytes verified.
Task-HEAD source confirms Navigation Report at line 663 and Submit Navigation
Report at line 775. These are acceptance examples, not permanent current-source
inventory counts.

Implementation is under qualification. Do not interpret this document or local
artifact hashes as independent review approval or deployment authority. Final
test commands, results and any blockers belong in the delivery evidence/PR.
