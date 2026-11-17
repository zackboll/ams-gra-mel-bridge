# MEL-GNATDOC-015-003 — official source-backed authored HTML

## Scope and authority

Baseline main: `3e462a6d5448918813a81b45653f9340a66b7c3d`, fetched again
before an isolated worktree was created. No runtime, public Ada, Rust, Python,
provider, vendored or executable-example source changes belong to this task.
The current source epoch, not the archived 40/43/55-file publication contracts,
is the admission authority. Historical evidence and immutable checkout support
remain intact.

Official distribution:
`https://github.com/zackboll/ada-gnatdoc-enhancer/releases/download/v0.1.6/ada_gnatdoc_enhancer-0.1.6-py3-none-any.whl`

- Release revision: `8661855bfc245c3389e2ef30a423c100776ff525`.
- Wheel SHA-256: `adfec71188a868e3fb70a6da71a124c937f9e63e26a2f2e31af45364f49ffb27`.
- Independent complete package/assets fingerprint:
  `ec58fd6e393bf2f88fdeddccb590d7c9e95319e3440d132ffd19b5b64e0a8377`.
- Symbol navigation JavaScript remains
  `233cf43211a711d0d64dfba2815bc2d9cc887720faed0ca38f464bc6d29e145f`.
- Installer retains pip hash enforcement, wheel-only/no-dependency installation,
  installed version, complete package fingerprint, import origin and PEP 610
  release URL/digest checks. No replacement wheel or source build is used.

## Production path and retirement

`source_epoch.generate()` exports Git objects, builds the documentation project
with GNAT 16.1.0/GPRbuild 26.0.0, regenerates native GNATdoc 26.0.0 with public,
trailing-comment scope, and independently reconciles every declaration/anchor.
`authored_html.contract()` then derives schema 3,
`executable-ada-catalog-v1`, binding all public specifications, example mains and
supporting sources, literal GPR ownership, site map, guides, stylesheet and the
regenerated native ledger. Its temporary manifest is not committed or deployed.
The configuration's zero digest is a fail-closed sentinel; only the disposable
export's independently derived digest reaches the renderer.

The official enhancer renders the complete publication in one pass. The pristine
site retains unmodified native API bytes and the nine authored outputs. The
normal build reuses that full candidate rather than rendering again. Explicit
qualification repeats independent native generations and runs the official
renderer/verifier on the complete candidate to prove idempotence.

Retired:

- The consumer `build_docs_portal.render()` full-page/navigation/directory/token
  rendering implementation and production call from `source_epoch.generate()`.
- The API-only enhancement plus consumer-rendered portal assembly in the current
  `build_enhanced_site.build()` entry point.
- Current-source admission through frozen publication/declaration counts in that
  compatibility entry point; archived read-only checks remain explicitly named
  historical evidence, not the production route.
- The partial-overlay pilot workflow, replaced with complete-candidate,
  non-deploying schema-3/security/browser qualification.

Retained deliberately:

- `build_docs_portal` source-owned identities, native destination/package audit,
  and a compatibility wrapper delegating rendering exclusively to the wheel.
- `portal_audit` is a validation-only inverse: checks static authored bytes,
  exact token ownership, navigation and directory serialization/order. It never
  writes a publication or supplies HTML to the enhancer.
- Independent `example_sources` listings, occurrence resolution, block-level
  serialization oracle and compiler/run-case qualification, and `api_catalog`
  source/native reconstruction, conservative usage and exact row ordering.
  These bounded audit oracles cannot render a full authored page/publication.
- The old pilot and browser driver as historical probes; its 18 negative cases
  are reused by current qualification. Historical claims are not rewritten as
  v0.1.6 qualification.

## Output classification

For the identical current Ada/native inputs the initial full migration candidate
has **58 files**, including **nine byte-identical authored assets**: Home, all
seven guides and portal CSS. Every native/enhanced API HTML, CSS and JavaScript
file is also byte-identical. The only permitted presentation-manifest changes:

1. `enhancer_version`: `0.1.5` to `0.1.6`.
2. `output_sha256`: includes the nine authored assets now owned by the enhancer.
3. `authored_source_blocks`: schema-3 manifest/profile/input identity.

The qualification script rejects any other manifest difference, any file
inventory change or any non-manifest byte difference. Consumer verification
restores all native declarations, source-derived type wrappers, navigation and
document metadata independently of the enhancer's self-verifier. Source addition
tests regenerate manifests and native anchors instead of changing fixed counts.
Resizable sidebar, global search and other new presentation features stay off.

## Qualification record (in progress)

External evidence root: `/home/zboll/git/mel-gnatdoc-015-003-evidence`.

Completed initial gates:

- Original v0.1.5 baseline: two immutable-source/native reconstructions.
- Full v0.1.6 candidate: two independent reconstructions and idempotence, 58 files.
- Nine-asset and complete API parity: pass; only classified metadata changes.
- 32 adversarial cases: prior 18 plus 14 schema-3 negatives, all rejected.
- Official installed release identity/origin/tamper tests: 5 pass.
- `make test-docs-ci`, `make test-policy`, source/vendor/workflow integrity: pass.
- Chromium: 744 assertions / 16 baseline/candidate desktop/mobile,
  light/dark, JS/no-JS contexts. Includes full portal, API navigation/filter,
  keyboard clear, bookmarks, back/forward, direct native fallback, canonical
  listings/support expansion, catalog links, URL clipboard and overflow checks.
- Executable examples: 13 cases / 9 programs, production facade identity and
  loader ownership verified; source semantics unchanged.

Initial source regression run: 10 of 11 tests passed; the remaining test used a
literal `0.1.5` in its tampering fixture. It is corrected to use the pinned version
without relaxing the rejection check and must be rerun. The initial `make check`
collided with example qualification rebuilding the same native tree; diagnostics
are retained and it is rerun serially after examples. An initial HTTP campaign
omitted `GNATDOC_PREFIX`; that setup failure is not a product regression. Local
build attempts while edits were in progress correctly failed exact-checkout
validation; final builds must use a clean committed HEAD.

Final source identities, complete regression results and exact-HEAD hosted CI
are not yet concluded. **Disposition: QUALIFICATION_INCOMPLETE**, not
`READY_FOR_INDEPENDENT_REVIEW`. PR must be non-draft with auto-merge disabled;
no merge, Pages publication or upstream release-asset changes are authorized.

## Subsequent qualification and remaining blockers

- Source/native repeat qualification at `4fb26fb5ebe3e4a3fc127a42728a32d02e1284a6`
  passed, with 55 pristine files / 58 Ada publication files, 1,523 native
  declaration sections, 322 public bookmarks and 1,797 independently enumerated
  type-link occurrences. These are observations, not admission constants.
- Baseline tree: `a8efcacf5a763867a149eb1a507caf75d4ff164c`.
  Complete production-source fingerprint (baseline and candidate identical):
  `16883b0065ba8d685650c767ec7d9789d0aa161bdfb9251ddc8f6849e92ec6c3`.
- The corrected source regression suite passed all 11 cases, including legitimate
  new declarations, regenerated catalogs/manifests, resealed artifacts, native
  restoration and independently repeated reconstruction (376.170 seconds).
- Final complete-page Chromium campaign passed **2,856 assertions / 16 contexts**
  on Chromium **156.0.8078.4**, including every native API page (not just selected
  bookmarks). Its candidate is the full 58-file source-qualified publication.
- Current authored qualification: all 32 negatives and five evidence-contract
  tests passed; nine portal assets and every non-manifest API file unchanged.
- Local normal Ada/Rust build, artifact sealing and independent verification passed
  at `6d3da4f69af8fb0fae50aa55ac2d664c6148117b`. Hosted documentation build at
  `4fb26fb5ebe3e4a3fc127a42728a32d02e1284a6` passed; its deployment and live
  jobs were skipped for the PR as required. These are not final-HEAD conclusions.
- The corrected HTTP campaign passed 58 parent tests plus its 15-case historical
  artifact child campaign; documentation CI tests passed 50 cases. Policy,
  exact historical Python feature admission and installer/origin tests passed.
- Serial `make check` completed native (799 cases), build-tree isolation,
  formatter and Ada execution gates, then failed the final-newline check on five
  newly added text files. The newlines are now fixed and the check passes. This
  was a migration regression, not a pre-existing issue; the full serial command
  is rerun, not silently reported green.
- Broad unprepared discovery is **not green**. Exact starting-main discovery
  reproduced 11 documentation setup errors / six pilot skips, plus 14 script
  errors, with missing root `_site` / explicit historical environment. Candidate
  unprepared discovery produced 25 errors / six skips: the same prerequisites,
  newly added evidence prerequisites and an import-order-dependent manifest mock.
  The mock now patches the actual consumer alias and the fast suite passes.
  Full legacy artifact/count-oriented tests have not yet all been qualified
  against a prepared current-source fixture. Historical suites are being run
  in their immutable checkout rather than rewritten to accept current counts.
- Initial hosted authored qualification at `6d3da4f` failed due to native-ledger
  serialization in its disposable export. `bind()` now writes exactly the
  independently measured ledger bytes; the subsequent local full negative suite
  passes. The hosted failure remains in evidence, not suppressed.

PR: <https://github.com/zackboll/ams-gra-mel-bridge/pull/230>, non-draft,
auto-merge disabled. Latest required hosted qualification is still pending.
Main advanced during parallel work (remote-tracking main observed at `34cc31f`);
no unrelated runtime changes were adopted into this branch. Final integration
against advanced main, full prepared current-source legacy suites and exact-HEAD
hosted qualification remain blockers. **QUALIFICATION_INCOMPLETE** remains the
only justified disposition. No Pages deployment or merge was performed.

Hosted follow-up found an existing fixture/environment boundary bug: the synthetic
source-addition commit inherited `GITHUB_ACTIONS=true`, so its assembly identity
was incorrectly compared to the event PR HEAD. Only that fixture now scopes
`GITHUB_ACTIONS=false` while deriving its genuine Git identity, consistently with
its existing verifier calls. Production event validation remains unchanged.
This failure blocked the newly added hosted qualification job before Chromium;
it is not counted as a successful hosted browser run. The dedicated historical
HTTP child campaign actually ran **22**, not 15, tests; all passed.
