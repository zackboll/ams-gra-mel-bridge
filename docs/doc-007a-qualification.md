# DOC-007A qualification

## Source and recoverable checkpoints

Starting main: `ddc63bf6883d16403db5fb23cd4a087ff5ebcf5c`.
Reviewed corrective `87fe97479ca0b82b8d7f157afb9c12589fff3871` is in its
ancestry. Feature branch: `docs/007a-developer-portal`; isolated worktree:
`/home/zboll/git/ams-mel-doc-007a`. External evidence:
`/home/zboll/git/ams-mel-doc-007a-evidence/r1`.

- 007A1: `87b5ae9`, minimal functional portal and fail-closed tooling.
- 007A2: `0173b7c`, source-backed guides, home, design and README entry.
- 007A3: final qualification/security refinements and this report. Exact final
  source HEAD/tree, remote head and PR merge identities are recorded externally
  after commit, avoiding a self-referential commit hash in tracked content.

## Site and native API qualification

Qualified executables: Alire 2.1.1, GNAT Native 16.1.0, GPRbuild 26.0.1 Alire
distribution (prints `GPRBUILD 26.0.0 (2026-04-15)`), GNATdoc 26.0.0
(`20260415`). Exact output is in `tool-versions.txt`. Ambient GNATdoc 27.0w was
not used. Pristine output was captured from starting main before edits.

| Gate | Observed result |
|---|---|
| Specification semantic compilation | Passed in both fresh complete generations |
| Coverage identity ratchet | 4,294/4,295, one exact Health metadata Status alias exception |
| Native inventory/profile/anchor reconciliation | 1,238 source / 1,237 rendered; frozen manifests unchanged |
| Warning identity ratchet | 0 added / 0 removed; 73 raw / 0 public / 73 private-part |
| Portal inventory | 39 files, 37 HTML pages, 1,516 local links |
| Public package destinations | 29 unique names/destinations; Shared/Core 3, IR 15, RF 11 |
| Independent full-site regeneration | Byte-identical `site-first` and `site-second` |
| Original native API bytes | Entire `api/` equals `pristine-site/api`, including index and CSS |
| Documentation unit/mutation tests | 88 passed, including 13 new portal tests |
| Dedicated source-integrity tests | Six passed, including explicit script authorization mutations |
| Source integrity | 1,162 protected files; production bytes and executable Ada tokens unchanged |
| Vendor checksums | All 804 match the unchanged checksum ledger |
| Baseline delta | No coverage/warning baseline expansion |

The complete package-name-to-observed-destination evidence is
`007a3-package-destinations.json`. Native inventory is measured independently
of portal additions. No API/ABI or provider behavior changes are introduced.

## Security, accessibility and navigation

The strict manifest fixes exactly six guide identities/labels/paths, the home
page and one stylesheet. Builder input/output symlinks are rejected. It never
edits native pages or invokes a network/production build operation. Native
heading destinations reconcile with the frozen source/anchor manifest, then
native index navigation is independently checked. Both directory locations
contain exactly one marked entry per public package; contextual guide links are
additional intentional links. Authored sources contain no opaque page hashes.

Negative controls cover missing/duplicate/swapped packages; unknown/missing/
duplicate/invalid manifest entries; missing guides/CSS; extra files; scripts;
event/style attributes; external resources; CSS imports, escapes and comment-
obfuscated resource loading; broken links/fragments; traversal; symlinks;
unresolved placeholders; local path leakage; and native iframe integrity.
Only canonical guide links to fixed root pages/assets or verified API pages
may use one parent segment. General traversal remains rejected. Portal output
also must exactly match deterministic authored rendering and authorized CSS.

All seven portal HTML pages passed strict HTML5 parsing using locally available
html5lib, with one h1, language, navigation/main landmarks and skip link checked.
Normal text/link contrast ratios against the theme background are 14.30/7.23
(light) and 13.57/9.37 (dark), exceeding 4.5:1. CSS provides visible keyboard
focus, system fonts, responsive text, scrollable preformatted diagrams/tables,
and no animation, remote assets or JavaScript. Diagrams use readable static
text, and categories are named rather than conveyed solely by color.

This is static/structural accessibility validation, not an interactive browser
or assistive-technology audit. No browser certification claim is made.

## Source-integrity policy review

Only two explicit filenames were added to the authorization set:
`scripts/build_docs_portal.py` and `scripts/test_docs_portal.py`. Tests accept
both and reject unrelated new Python scripts, production additions/byte changes,
executable Ada token changes and unexpected workflows. No broad `scripts/`
exemption or protection weakening was added. All protected directories and
vendor/coverage/warning/anchor/renderer/upstream evidence files are unchanged.

## Sequential production regression

Commands run in one stable GNAT/GPRbuild environment, never concurrently.
Each has its own log and actual exit-status file under the evidence directory.

- `make test-native`: exit 0, 434/434 passed.
- `make test-ada`: exit 0, complete smoke/contract execution passed.
- `make check`: exit 0; isolation, 434 native tests, format, Ada and hygiene gates passed.
- `make check-ada-format`: exit 0. Existing external-import GPR projects emit
  the informational no-C-sources warning; no compiled-source warning was introduced.

No first-party Ada source was edited, so no formatting rewrite was required.
The final format check still runs as requested. Frozen protected-source SHA-256
identities are in `frozen-protected-source-sha256.json`. Final whitespace/newline
checks and source-tree identities are recorded before publication.

## Failed attempts and publication policy

Failed pre-checkpoint attempts are preserved, not counted as passing: the first
minimal-site test exposed noncanonical guide-relative navigation; a later
newline check found four new files lacking final newlines. Both were corrected
and gates rerun. An observation-only 30-second sleep/poll hit the tool timeout;
it did not interrupt or qualify a regression command.

The documentation workflow is unchanged. PRs upload the full site artifact
without Pages deployment permissions; only main can deploy. A read-only Pages
API query confirms `build_type: workflow` and the configured project URL.
No feature-branch deployment, manual deployment, merge, tag or release is
performed. Hosted Documentation and Bootstrap runs must be checked on the exact
submitted head; their final observations and PR state are external evidence.
