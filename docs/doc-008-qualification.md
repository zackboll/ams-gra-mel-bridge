# DOC-008 qualification

## Source identity and review boundary

Starting/final fetched main during local qualification:
`a5484514cfae3b7bccde078118b838fa40ce4d0e`, identical to the task's reference.
Branch: `docs/008-compilable-ada-examples`. Isolated worktree:
`/home/zboll/git/ams-mel-doc-008`. Bulky logs, captured sites, loader observations
and browser evidence are outside committed source under
`/home/zboll/git/ams-mel-doc-008-evidence`.

Exact submitted HEAD/tree, remote/PR head, PR URL and final-head hosted results
are recorded in the PR/final handoff after commit, avoiding a self-referential
commit hash here. Pending or failed hosted checks are not local passing gates.
No merge, force-push, tag, release or feature-branch Pages publication is authorized.

## Tools and executable examples

Actual local versions: Alire 2.1.1; GNAT Native 16.1.0; GPRbuild 26.0.1
distribution printing `26.0.0 (2026-04-15)`; GNATdoc `26.0.0 (20260415)`;
pinned GNATformat 26.0.0 printing `26.0 (20260415)`; CMake 3.31.6;
Debian C++ 14.2.0; Python 3.13.5. GNATdoc's version command exits 1 even
when printing the expected version; generation uses the existing qualified driver.

All nine real public-library programs compile with Ada 2022, assertions, validity
checks, all warnings and warnings-as-errors. No private FFI/testing imports or
native provider edits. All thirteen cases execute with inspected exits and
semantic observations, not just a PASS string. Main sources below are relative
to `docs/examples/src/`.

| Program | Mock configuration | Compile | Execution / actual observation |
|---|---|---|---|
| `provider_session.adb` | `ada-explicit`; nonexistent library | Pass | Owned vendor/description and exact version numbers after Close; missing library exits 1 with Provider_Error/cannot-open diagnostic |
| `owned_image.adb` | `full-frame-rich` | Pass | 4×3 Mono8, rich header, frame 0xFEDCBA98, two nav states, sensor face-α; 12 owned pixels, 160/171 endpoints after Close |
| `frame_lease.adb` | `ada-lease-success` | Pass | Scoped borrow sum 258; explicit independent 12-pixel copy sum 258 after lease/stream/Session Close |
| `common_channel.adb` | `keepalive-delayed` | Pass | Owned Command_And_Control capability; immediate poll or timeout/re-wait allowed; KeepAlive Return_Success and identical cached result |
| `navigation_report.adb` | `navigation-fidelity`, `navigation-reject` | Pass | Complete supplied report accepted with expected response IDs/time; rejected alternative has Invalid_Parameters and exact diagnostic |
| `track_metadata.adb` | `track-metadata-mixed` | Pass | Finite request/report/request sequence; exact high-bit request/activity IDs and 123456.75 m report range |
| `rf_discovery.adb` | Data `success`; `admin:ok`, `admin:reject` | Pass | Owned faces 7/42 at positions 1/2, four advertised formats after Data Close; independent Standby acceptance/rejection |
| `rf_rx_job.adb` | `c2:job-ok` | Pass | Unique claims, details ID 0x10203040, one finite interval/event, Finalize future Complete, explicit Close |
| `rf_product_rx.adb` | `rx:ok:sync`, `rx:error-known` | Pass | Actual pairs (77,-77),(-1,1), real sum 76, copied 0xDEADBEEF metadata, one queued product, deterministic empty timeout; expected resource rejection |

Commands from repository root:

```sh
sh docs/examples/format.sh format
sh docs/examples/format.sh check
sh docs/examples/run.sh
sh docs/tools/qualify_examples.sh
```

The docs-owned formatter explicitly selects the new GPR, UTF-8 and
`--no-subprojects`. The final execution gate followed `gprclean` of the new
project and rebuilt all nine mains. Every documented individual case command
in the examples README was also run with `--no-build` after the successful
collection build. Initial native/mock/example compilation began in the fresh
isolated worktree with no build products. Existing root formatting was checked
separately; no first-party source under `ada/src` or `ada/tests/src` changed.

The final report records thirteen actual child loader initializations resolving
to `native/build/lib/libams_mel_c.so.0.1.0`, SHA-256
`6091e3042e650c3b9ffee6d550d192ee421cb8bea8a11df79d99b95b095b1db9`.
`ldd`, canonical-path checks, production test-export rejection and each actual
child's glibc `calling init` diagnostic agree. The production facade has 222
AMS MEL exports, no native test entry points. Earlier builds have a different
binary hash because later canonical Alire builds rebuild native artifacts;
each execution report records the binary actually used, not an assumed identity.
Inherited loader/preload and AMS_MEL test settings are cleared. API waits are
bounded to 1,000 ms; every executable has a 15-second process timeout. Expected
child failure is distinguished from runner success. No external provider,
hardware, credentials, Squall, container or provider download was required.

## Static source, site and presentation qualification

All **31** decoded complete-source listing occurrences equal canonical compiled
source text: nine mains, repeated shared support, and complete mock constructors.
No pasted excerpts, source-download assets or JavaScript dependency. Manifest,
GPR mains, physical source inventory, README commands, guide IDs/placeholders,
public package references and actual execution inventory agree.

Twenty focused mutation/control tests cover absent/duplicate/unknown IDs and
cases, fixture/exit changes, malformed paths, missing/symlinked source/parents,
unlisted source, GPR/README/guide/package disagreement, private dependencies,
HTML escaping, stale/tampered/missing/duplicated listings, wrong facade, inherited
loader settings, wrong exits/content/order and outer process timeout. Controls
also verify consistent explicit isolated Alire settings. Existing
portal mutations continue to cover broken guide/package/local links and active
content. No published source URL allowance was added.

Two fresh complete pristine generations have identical inventories and bytes;
two fresh complete enhanced publications are independently verified and equal.
Fresh external type audits also agree. Current complete metrics:

| Artifact | Files | HTML pages | Local href/src references |
|---|---:|---:|---:|
| Pristine DOC-008 | 40 | 38 | 1,562 |
| Enhanced DOC-008 | 43 | 38 | 3,011 |

Compared with the authenticated starting main, the only changed published files
are the new `guides/examples.html`, the six prior guides and home. Shared
navigation adds Examples to each authored page; Getting Started and IR/RF gain
workflow links. No stylesheet change. There are **46** additional references;
native/API references are unchanged. Exact inventory/byte fingerprints (SHA-256
of canonical sorted compact JSON filename→SHA-256 mappings):

- Complete pristine: `d076b4e9c69f4ffd7c02e82da711704ca90077a82f0db229176492644e0a903d`.
- Complete enhanced: `72dff1a35df2b21305227da712bad728bd41746e62a7bc5dcf7df0696c675a96`.
- Inherited pristine `api/`, all 31 files:
  `d2026657ddd02d2513514c545633c13fbaf8ce772e1bf35bf51223e16d811983`.
- Inherited enhanced `api/`, all 34 files:
  `d391e78aca127e3c14cce805b25c17c9ea8638aa21d5d1998be22116c4583b32`.

Both API inventories/bytes exactly equal the independently captured baseline.
Additional hardcoded API fingerprints preserve that comparison, without replacing
the prior independent type audit, complete reconstruction or native restoration.
The source/profile fingerprint, authoritative anchors, 1,238 source declarations,
1,237 native sections, 4,294/4,295 coverage, exact renderer exception, warning
baseline, 1,418 audited local type links, zero external links, enhancer revision
`53a321915da2c5a02a54581040673b61f8595b25`, installed implementation/assets,
package navigation and original rejection/mutation checks are retained.

Fresh isolated enhancer installation uses the unchanged immutable installer.
GNATdoc generation remains specification-semantic-only. Example execution is
an explicit separate documentation-workflow step before review upload/Pages;
actual PR-source-head checkout and main-only deploy conditions remain unchanged.

## Browser checks

Chromium `156.0.8078.4`: eight fresh combinations of 1440×1000 / 390×844,
light/dark and JavaScript enabled/disabled. **304** layout/source/navigation
assertions pass; all example API links also return HTTP 200. All nine mains and
31 source listings are visible or expand using native details; long source
scrolls locally, not by overflowing the page. Light/dark backgrounds differ
appropriately. Eight screenshots retained externally; desktop light and narrow
dark/no-JavaScript screenshots inspected. A final fresh browser run was performed
against the final authored publication, not merely reused from DOC-007.

## Regression gates and honest intermediate findings

- `make check` through the existing Alire tool environment: exit 0. Its canonical
  aggregate includes native build-tree isolation, native CTest **434/434**, root
  formatter check and the complete Ada suite; separate `make test-native` and
  `make test-ada` were not redundantly repeated outside that aggregate.
- `alr -n -C ada build`: exit 0.
- `alr -n -C ada/tests run`: exit 0.
- `make check-ada-format`: exit 0; new docs-owned format/check: exit 0.
- Existing script documentation discovery: **133 tests**, exit 0.
- Initial full presentation discovery: **119 tests**, exit 0, comprising all 100
  inherited tests plus 19 DOC-008 controls. R2 adds one isolated-Alire-settings
  control: final full discovery is **120 tests**, **253 distinct documentation
  tests** (133 + 120); its final result is recorded in the handoff.
- Unchanged source-integrity checker: 1,162 protected files, approved reviewed RF
  pair and unchanged IR transition labels. Native/vendor's 804 files unchanged.
  Coverage/warning baseline-delta check, whitespace and final-newline checks pass.

Intermediate failed/incomplete attempts are retained outside source, not reported
as qualification: the first presentation suite's wrong-facade negative fixture
depended on a native artifact temporarily absent during canonical build-tree
isolation. It now uses independent temporary files. A second overlapping site
regeneration/read attempt was invalidated by authored/newline updates; final
generations and suites are sequenced on stable source. An attempted output build
correctly rejected a pre-existing publication directory; final builds use fresh
destinations. A summary command initially used system Python without the isolated
enhancer and failed import; the same verification uses its qualified venv.
None required relaxing production/test contracts or documentation safety checks.

### Hosted R1 settings collision and R2 correction

The first submitted head `37d8eb56d71684ef987112ab58c148427d0d6ab3` passed
local gates but hosted Documentation run `39201413195` failed its explicit
examples step. The workflow wrapped the driver in the isolated documentation
crate's `alr exec`; nested formatter/build commands used default Alire settings.
On a clean runner these select distinct installation prefixes for the same
compiler, so Alire correctly rejected conflicting `GNAT_NATIVE_ALIRE_PREFIX`
values before compiling examples. Full raw logs are retained; no rerun of that
unchanged failure is presented as success.

R2 removes the outer nested environment. The workflow explicitly passes
`DOC_EXAMPLES_ALIRE_SETTINGS` to the docs-owned driver; formatter and build both
use that exact settings directory through `alr -s`. Ordinary local commands
remain unchanged. A new unit control checks both isolated and default command
construction, and local qualification executes the complete workflow-style
command with a fresh isolated settings directory. Production/GPR/toolchain
policy and canonical listings/site fingerprints do not change. R1 native/Ada
regression evidence remains applicable because this correction changes only
documentation execution wiring, tests and records; final-head hosted results
are inspected afresh rather than reused from R1.

## Remaining coordination

PR #119 remained open at the final pre-submission main refresh; main had not
advanced. Overlap is limited to the five files listed in source research. It
must be reconciled normally if merged later; DOC-008 does not take over enhancer
adoption. Hosted results for the submitted head must be inspected before claiming
READY_FOR_INDEPENDENT_REVIEW. Stop for independent review without merging.
