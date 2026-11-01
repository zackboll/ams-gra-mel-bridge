# IR MEL inventory schema and trust model (version 1)

This is a consumer binding audit, not implementation, normative compliance
certification, or a promise that every provider supports every declaration.
The source of truth is the pinned upstream Git objects, **not** the existing
vendor closure or the historical prose in `docs/coverage.md`.

## Artifacts

All paths below are repository-relative for archive portability; the checker
resolves them under its absolute `--root` and rejects absolute/traversal evidence
paths. JSON duplicate keys and stable-ID duplicates are errors.

- `irmel-upstream-git-evidence.json`: independent immutable source evidence.
  `schema_version: 1`; `repositories.{ir,common}` contains URL, commit, tree and
  an object dictionary keyed by Git SHA-1. Each object has `kind` and
  `zlib_base64`, the compressed **raw** Git payload (not its object header).
  The checker computes Git object hashes, verifies the commit's tree, walks the
  complete root/subtree objects, and requires every `include/**/*.h` and `.hpp`
  blob. Thus deleting a header from the catalog cannot delete its independent
  evidence. LICENSE and INTENT.md blobs are preserved as well; both upstream
  repositories carry Apache-2.0 and US Government copyright notices. The full
  original headers/comments/notices remain losslessly recoverable here. These
  objects are audit data, **not** additional native build/vendor inputs.
- `irmel-upstream-api.json`: reproducible immutable declaration catalog.
  `schema_version`, `repositories`, `headers`, `types`, `operations`.
  Headers contain `id`, repository, path, blob SHA-1, content SHA-256, section
  (`library`/`utility`), expected vendor path and literal direct includes.
  Every type definition/alias contains fully qualified symbol, kind, path/blob,
  source line, base declaration, ordered access-labelled member declarations and
  normalized-body SHA-256. Members include getters, setters, constructors,
  private data and enum alternatives; these are not extra channel sends.
  Operations contain fully qualified symbol, interface, path/blob, exact
  whitespace-normalized declaration, parameter-type overload identity, category
  and verbatim upstream requirement tags. Source repository/commit/tree are
  resolved through the repository key. `required_types` links operation
  inputs/results; type `dependencies` links nested declarations. These lexical
  links prefer the declaring MEL namespace for unqualified type names and keep
  explicitly qualified Common references distinct. They do not resolve a third
  repository's Math declarations. IDs are `repo:header:path`,
  `repo:type:qualified-symbol`, and
  `repo:operation:qualified-interface::method(parameter-types)[ const]`.
  Channel identity is mandatory: C2/Track SystemTrackDataResponse and C2/Image
  CameraCommand must never be collapsed.
- `../irmel-ada-coverage.json`: mutable coverage classifications with
  `schema_version`, exact base SHA, header presence/rationale, operation/type
  arrays, inherited exposure array, image capabilities, qualifications and
  ordered priorities. Every catalog ID has exactly one classification.
  Entry keys are `id`, `status`, `native`, `ada`, `tests`, `notes`, `limitations`,
  `representation`. Evidence items have `path` and `symbol`. Type representation
  items have `upstream` getter, `status` (`represented`, `not_exposed`,
  `see_notes`) and safe Ada evidence. This inventories **every getter**, not just
  a hand-selected scalar subset; collections and private/accessorless state are
  retained in the declaration members and discussed explicitly in notes.
  Evidence on a type names its complete receive/submit conversion path; it does
  not assert that the Ada operation has the same spelling as the C++ getter.
- `irmel-reviewed-bindings.json`: independently frozen reviewed coverage ceiling
  and first-party implementation byte identities. Its digest and the catalog's
  digest are anchored in the checker. A mutable manifest cannot turn an absent
  Optional callback into implemented by borrowing unrelated Receive evidence.
  A future implementation PR must explicitly update/review this baseline and
  hash, with native/Ada tests; it must not merely edit the coverage manifest.
  Test source bytes are intentionally not frozen: observer-only changes in
  concurrent PRs #127/#128 do not change implementation qualification. Evidence
  test paths/symbols are still checked. No documentation qualification policy,
  workflow, Pages output, or published documentation artifact is modified.
- `../irmel-ada-coverage.md`: deterministically generated from catalog/mapping;
  byte-for-byte comparison is part of every successful check.

## Status categories

| Status | Meaning |
|---|---|
| `safe_ada` | Working native definition **and export**, safe public Ada path/body, compiled-C and Ada regression evidence; limited to the stated input/provider profile. |
| `native_only` | Working exported native implementation/tests; no safe Ada operation. Currently zero, but a supported category. |
| `partial` | Some fields, domains, inherited family exposures or lifecycle access remain unavailable. Notes identify the exact boundary. |
| `missing` | No working consumer binding. This is a valid expected result, not a failed gate. |
| `provider_only` | C++ provider construction, storage, destruction, registry, utility, exception or implementation infrastructure, with rationale; no direct safe Ada mirror required. |

Declarations alone never establish implemented status. The checker checks C
prototypes, export-map entries, native function definitions, public Ada
declarations and bodies, test symbols, and reviewed implementation hashes. It
does not infer correctness from a name: human source audit and runtime tests
remain necessary. Invented claims fail against the independently hashed review.

Inherited exposure IDs are `derived-family|declaring-operation-id`. All eight
non-destructor Channel methods are mapped onto each of seven derived families.
These 56 exposures are not added again to the 21 outgoing/38 metadata totals.
Outgoing includes `send`, `exec`, `sendKeepAliveRep`; metadata includes only
`registerMetadataCallback`. Destructors, buffer methods and image callbacks are
inventoried separately. Nineteen metadata declarations have a working path, but
the common CommsTest registration is partial overall because only C2 exposes it.

## Extraction limits and reviewed exceptions

The stdlib lexical extractor intentionally accepts the **frozen source dialect**,
not arbitrary C++. It preserves complete type bodies, all enum alternatives,
includes and source blob identities even for non-operation helpers. It does not
preprocess, resolve typedefs through external libraries, prove C++ types equal,
or turn helper getters into consumer commands. There is no network/compiler
dependency in ordinary validation. The catalog hash prevents an extractor change
from silently approving dropped declarations. These reviewed exceptions are
explicitly exercised by `test_reviewed_extraction_exceptions`:

1. Derived destructors and StackedImageListener::onImage omit `virtual` but use
   `override`; both are inventoried. The latter is an upstream no-op, not a
   working StackedImage receive path.
2. LibraryLoader's three `decltype(factory) *ptr...` members are **data**, not
   function declarations; they remain in the type members but are excluded
   from operation counts. Both parseAndLoadLibrary overloads are inventoried.
3. IRInterfaceBase::QueryInterface has an integer virtual and a public template
   wrapper. Both are provider infrastructure. Protected implementation templates
   (for example IRFactory::baseCreateChannel) stay in members, not public counts.
4. Callback types preserve `*const` and trailing `const&`; parameter names are
   removed without collapsing independently declared channel families.
5. Requirement tags are taken from the immediately preceding method comment,
   not inherited from the enclosing class. Upstream prose inconsistencies are
   not resolved by guessing. `unspecified` means no method-local tag was found.
6. Global API_Manager, namespace `library`, nested SecurityAudit enums/variant
   aliases and referenceimplementation enum helpers retain their actual scopes.
7. Braced constructor initializers and private member initializers can yield
   lexical member fragments. The complete normalized body hash and raw pinned
   blob retain the exact declaration; operation extraction does not rely on
   reconstructing constructor initializers. Type constructors/accessors are
   support declarations, not extra outgoing overloads.

TempPoint's PixelValue is exactly `variant<uint8_t,uint16_t,uint32_t>`, **not**
an invented RGB/gray compile switch. Its optional `tempError` is constructor-set
private state with no getter/setter in the pin: a future complete binding needs
an explicit upstream decision, not silent omission. SensorNavState has three
independent Euler/Quaternion variants and associated uncertainties; existing
conversion probes each actual alternative. SystemTrackDataResponse uses the
pinned AMS Math `RangeAzEl.h` AzEl; no fallback branch is invented. External
Math dependencies are recorded in literal includes, but this task does not
pretend to inventory a third upstream repository.

## Reproduction and updates

Initial verification cloned the two authoritative repositories and used
`git cat-file commit`, `git rev-parse COMMIT^{tree}`, `git ls-tree -r -t TREE`
and `git cat-file blob` before comparing each existing vendor byte sequence.
Both commit/tree pairs match the task pins exactly. Base main equals the
reference baseline; there are no base-main differences to reconcile.

To reproduce candidate frozen evidence using already fetched checkouts:

```sh
python3 scripts/freeze_ir_api_evidence.py \
  --ir-checkout /absolute/path/to/ir-mel \
  --common-checkout /absolute/path/to/common-mel --output /absolute/existing/tmpdir
```

Compare candidate JSON to the committed evidence/catalog. The generator does
not change coverage, native headers, or approved digests. Review source/member
and overload differences independently before accepting any pin/extractor
change. For binding work, update field/collection evidence, tests, limitations
and the reviewed coverage ceiling; regenerate the report explicitly:

```sh
python3 scripts/check_ir_api_coverage.py --write-report
make check-ir-api-coverage
```

Normal checks are read-only and offline. The fast target runs only Python
stdlib checks and isolated temporary-copy unittest controls. The existing
`scripts/test_ada.sh` invokes the same gate, so hosted Ada CI and `make check`
inherit it without new runners or workflow edits. This does not change the
existing documentation qualification trust model.
