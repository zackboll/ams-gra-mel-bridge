# DOC-010 — corrected stable Ada symbol bookmarks

## Approved behavior and scope

The immutable GNATDOC-011R3 development merge
`524b7610e21e9a7b87e8e0761580ed59a3b0295c` adds opt-in source-verified public
type/subtype bookmarks to the existing package browser:

```toml
[navigation]
mode = "packages"
symbol_bookmarks = true
default_package = "AMS.MEL"
```

`api/index.html?symbol=AMS.MEL.Session` and
`api/index.html?symbol=AMS.MEL.Version` select genuine native declarations,
scroll to their existing anchors, and canonicalize to package/symbol state.
`AMS.MEL` is independently established from its public specification and native
package heading, not inferred from a hash filename. Package switching, approved
native declaration navigation and browser history remain available. Unknown,
duplicate, encoded, conflicting and unsupported query inputs fail closed to the
default package; canonical URLs discard inherited query keys and outer fragments.
Unsupported outer fragments do not gain native anchor authority.

The bounded eligible inventory is **238 symbols: 231 types and seven subtypes
across 24 owners**. Owners with unsupported nested public scopes are excluded:
`AMS.MEL.IR.C2`, `AMS.MEL.IR.Channel`, `AMS.MEL.IR.Health_Status.Metadata`, and
`AMS.MEL.Status`. Private completions, internal packages and synthetic instance
members do not acquire bookmarks. Public limited-private views remain public.
This is not a complete Ada semantic resolver, generic-instance router, full-text
search, runtime feature, or full accessibility certification.

Without JavaScript the complete native package/type navigation still works.
DOC-011's 266 example links remain direct native page/anchor links: no router or
script execution is required to reach their declarations. The nine canonical
programs, supporting sources, thirteen execution cases, 31 listings and 4,411
syntax-token spans are unchanged. Examples HTML and portal CSS are byte-identical
to DOC-011; no DOC-011 rendering semantics change.

## Immutable dependency identity

| Identity | Value |
| --- | --- |
| Merge | `524b7610e21e9a7b87e8e0761580ed59a3b0295c` |
| Reviewed/merge tree | `90288aa10b7cc18989f595f8195d4c431129596f` |
| Exact codeload archive SHA-256 | `8b7c0a0d0957ef2ebb2cc27888173e653db6987c4279444fcffe095ccc099101` |
| Installed implementation/assets fingerprint | `af5c5fec03ec3fbc41e3a55a55424a3ce65cf1bb5101d97b0c65ff5614e40632` |
| Corrected symbol router SHA-256 | `233cf43211a711d0d64dfba2815bc2d9cc887720faed0ca38f464bc6d29e145f` |
| Unchanged default router SHA-256 | `b3fb428b507844d6be76fb40156058d39a5b5f57004bde04e33b0411f68504cd` |
| Unchanged incoming-instance router SHA-256 | `97dcc75055842e98a6294809b8e8c4e7436a427039d19c8afb6fdd1ca21ea310` |
| Installed version | `0.1.0.dev0` |

All 1,533 archive files equal the fetched Git tree; all 35 installed non-bytecode
implementation/assets files equal archive source. The default router equals the
original MEL dependency; the instance router equals R3's incoming parent
`697d4ca91dc81812e501e501c9766d9b3ec88924` (it was absent at MEL's original pin).
It is not selected or enabled. The separate GNATDOC-012 release-preparation PR was
open and unmerged at adoption preflight; no release-candidate package is used.

## Independent consumer and HTTP contracts

`docs/tools/symbol_audit.py` derives canonical identities, public owners, complete
source declarations, source lines, native profiles, observed pages/anchors and
approved ledger identities independently of the enhancer. Abbreviated native
enumerations require matching source prefix/suffix **and the complete ordered
native literal list**. Duplicate identities or destinations and wrong native
owners fail. Metadata is serialized in source declaration order within sorted
owners. Only the exact unique, correctly placed metadata block is removed before
the existing native index restoration and complete pristine-byte comparison.
Extra hidden content, duplicate metadata, different anchors and wrong-but-existing
destinations cannot be authorized by a self-consistent candidate manifest.

The consumer reconstructs the complete presentation manifest from qualified
pristine/source authority, independently inspected output and independently
derived counts. It additionally compares pinned transformations and runs the
upstream verifier, neither of which supplies symbol authority. Both the new
enhanced API fingerprint and a complete symbols-disabled projection back to the
original DOC-011 API fingerprint are enforced. All 1,428 type-reference records,
the original ordered 1,418 fingerprint and ten required additions remain audited.

The live verifier qualifies the complete artifact **before HTTP**, compares all
43 decoded entities and revalidates symbol metadata against the independently
derived 238-destination ledger. Its report adds `verified_symbol_destinations`.
DOC-009 transport/MIME/redirect/compression/stale-content guards, bounded retries,
read-only workflow permissions and diagnostic artifacts are unchanged. The full
266-link example ledger, 31 canonical listings and original type audit remain
required. Invocation remains as documented in `doc-009-live-publication.md`.

## Main readiness and fresh publication comparison

DOC-011 merge `8242bfd98c7359fe72a7555d431d1dc7cd33a9f8` was independently fetched.
Documentation run `39364739455` passed build, deployment and `verify-live`;
Bootstrap run `39364739442` passed all eight jobs. Same-run downloaded publication
and pristine artifacts equal fresh local originals. A separate read-only live
check matched all 43 entities in one attempt: 38 HTML, 3,287 references, 1,428 type
links, 266 inline links, 31 listings and the nine-program/thirteen-case inventory.
No production adoption edits preceded this successful readiness gate.

Two fresh pristine generations from current source are completely byte-identical.
Fresh complete publications share that source and pristine input:

| Publication | Complete fingerprint | References |
| --- | --- | --- |
| A: DOC-011 production enhancer/configuration | `54969299f3009717667e77806582c67eb6c96c6ec43156f12e6094ac1da26aee` | 3,287 |
| B: R3, symbols disabled | `54969299f3009717667e77806582c67eb6c96c6ec43156f12e6094ac1da26aee` | 3,287 |
| C: R3, symbols enabled | `22e51b0a72744cf4a18991b54433ec802872d6a427d5cee1fe97a8ac33cd83d7` | 3,525 |

A and B are completely byte-identical. C changes only `api/index.html`,
`api/gnatdoc-navigation.js`, and `api/presentation.json`; the other **40 files are
byte-identical**. All retain 43 files and 38 HTML pages. The observed reference
delta equals the 238 independently identified metadata links; 3,525 was derived
from output, not used as authority to accept metadata.

Enhanced API fingerprint:
`9790f7666b867250e8c78e998a32eb233c3cbe76f9e83bbda7f4d8978f0b6f37`.
Original enhanced API, pristine, native API, source and ledger identities remain
retained by the consumer wherever their inputs are unchanged.

## Historical negative evidence and qualification boundary

The original DOC-010 failure is preserved under
`/home/zboll/qualification/mel-doc010-20261019`; all 1,765 sealed historical files
were independently hash-verified unchanged. Its original router retained unknown
keys after fallback, then lost a legitimate Session selection on reload. R3 fixes
canonicalization upstream, not by copying a consumer-patched JavaScript asset.
The prior DOC-011 evidence under `/home/zboll/git/ams-mel-doc-011-evidence` is also
retained unchanged. The clean DOC-010 branch incorporated DOC-011 main by normal
fast-forward; no reset, rebase or unrelated-work overwrite occurred.

New implementation/qualification evidence is separate:
`/home/zboll/qualification/mel-doc010r2-20261020`. The preliminary real Chromium
156.0.8078.4 campaign passed eight desktop/mobile, light/dark, scripts-on/off
contexts, dirty fallback/click/reload, malformed inputs/fragments, history, new
tabs, keyboard selection, package/search behavior and direct no-JavaScript
example links. This is bounded browser evidence, not broad a11y certification.

The final handoff records final test identities, completed regression commands,
two-copy enhanced qualification, mutations, exact commit/remote/PR identities and
hosted conclusions only after those gates actually run. No merge or manual Pages
deployment is authorized by this record.

### Executed local implementation qualification

The complete required sequence passed in order on the implementation:

```sh
make test-native
make test-ada
make check
make check-ada-format
alr -n -C ada build
alr -n -C ada/tests run
sh docs/tools/qualify_examples.sh
```

Native CTest reports 434/434, with distinct test identities retained. Ada
contract/focused tests pass; the examples compile all nine programs and execute
all thirteen cases against the production facade and separate mock providers.
The final stable documentation suites pass **133/133 script tests and 330/330
enhanced tests: 463 total**, retaining every original 440 test identity and adding
23 symbol tests. Focused subset runs are not counted as additional tests.
All 1,162 inherited production files pass source-integrity guards; no warning,
coverage, native anchor, example, provider, vendor, ABI or Ada-source change.
Two-copy enhanced output is completely byte-identical and equals fresh C.
The full original ordered 1,428 type audit equals DOC-011 and fresh A exactly.

The consumer's 23 new tests exercise source/native ownership and profiles,
complete enum literals, exact source ordering/restoration, source/native default,
ledger anchor changes, malformed/misplaced/duplicate/forged symbol metadata,
hidden content, wrong-but-existing destinations and router mutations. Independent
guards are reached directly as well as through full forged-manifest candidates;
altered artifact/router tests prove rejection before any HTTP.

Final real Chromium checks resolve **all 238 symbols** to their observed native
anchors. The eight-context desktop/mobile, light/dark, scripts-on/off navigation
matrix passes dirty fallback/click/reload, twelve malformed query classes,
unsupported fragments, package switching, history, fresh tabs, keyboard and
source-qualified scrolling; it records zero page errors and zero external
requests. The unchanged full DOC-011 browser campaign passes **794/794** assertions,
including canonical DOM/selected source, highlighting, direct links without
JavaScript and presentation checks. Strict local HTTP passes all 43 entities,
3,525 references, 238 symbols, 1,428 native type links, 266 example links and 31
complete listings. The pinned installer passes end-to-end from a fresh venv.

Historical DOC-011 evidence was also hash-verified: all 359 sealed files remain
unchanged. Corrective local runs are retained: an old-tool suite overlapped the
configuration edit; a browser harness started before its publication existed;
an external oracle initially used symbol-alphabetical rather than source order
and the wrong manifest serialization/output-map boundary; a broad projection
guard initially masked an existing theme-specific mutation diagnostic. The last
was corrected by preserving both checks and running specific transformation
checks before the broad projection guard. No old assertion, source-integrity
contract, native declaration or upstream asset was weakened to pass a gate.
