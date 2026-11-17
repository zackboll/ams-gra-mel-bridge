# DOC-RESIZE-CI-001 — same-source resize qualification

## Baseline, root cause and scope

Base main commit: `51ede458587698cc1f8e4601d3471852d6f2a9e5`.
Base tree: `c2eada141d0b86de5acb39d6e58b275539d1d1ea`.
Dedicated branch: `fix/doc-resize-same-source-parity`.

No existing open corrective PR or matching remote branch was found at preflight.
PR #233 is unchanged by this task. No runtime, ABI, binding, vendored source,
public API baseline, branch protection or unrelated workflow is modified.

Failed hosted run: <https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/44018141078>.
Downloaded diagnostics reproduce the exact original four-file assertion failure:
main `86eee19b252da563246d87072f4d71f69a377e30` generated 197 Python HTML
pages; feature `991c3a85d21688e3caf43052b440fa60681da37c` generated 202.
The 12 differing files are all legitimate Python reference files (five added
API pages plus indexes, module/source listings and search records). Resize was
already enabled in both revisions. Comparing these different sources cannot
measure the effect of a resize toggle. Current main's Rust-palette projection
does not correct this source-model error either.

## Corrected authority and invariants

The parity entry point requires the exact candidate manifest digest, performs
`docs_ci.verify`, and exports that verified commit, not a floating main ref.
Verification retains exact checkout/event identity, source/native reconciliation,
native anchors, coverage, Ada restoration, authored destinations, search checks,
complete assembly, source-derived Python reconstruction and independent Rust
regeneration. Only then may the already verified candidate language bytes be
reused in the disabled control.

Exactly one expected `resizable_sidebar = true` is changed to `false` in a
disposable export. The existing renderer/consumer verifier re-renders Ada from
the verified current native artifacts. Real Rust and Python assembly functions
produce the complete disabled site. Both Ada-only and complete-site comparisons
require precisely:

```text
api/gnatdoc-resize.css
api/gnatdoc-resize.js
api/index.html
api/presentation.json
```

All other file bytes are equal, including every Rust/Python byte, authored page,
native/search destination and asset. Only the CSS/JS files may be added, and no
file may be deleted. There are no Rust theme exceptions or ignored subtrees.
The control records `modified-export-not-deployable` and a `derived_from`
identity plus original/modified configuration hashes; it never fabricates an
immutable commit for the changed export. Source and candidate payload identities
are rechecked after rendering. Disabled files are temporary and cannot deploy.

## Regression coverage

The explicit pinned-toolchain suite uses actual complete publications and real
isolated fixture commits. It checks current-source parity; a Python-only public
class producing additional HTML (old cross-revision rejection, new parity pass);
and a subsequent documented Ada subtype and Rust type (old comparison rejection,
each revision independently qualified). Page counts are measured, not globally
hardcoded. Existing source-epoch regression tests remain unchanged.

Adversarial tests cover unauthorized non-resize files, missing delta files,
subset deltas, extra Rust/Python output, changed HTML/search/resize JS/CSS,
forged presentation data, wrong source identity, Python records, resealed Rust
output, invalid native anchors, altered separator markup, broken destinations
and incomplete language assembly. Resealing the outer manifest does not grant
authority. Existing resize markup/asset, search corruption and enhancer-release
negative tests remain required.

The hosted workflow builds exact PR HEAD, retains deep source reconstruction,
runs the same-source gate and new fixture suite, retains existing resize/release
tests and full Chromium resize/search/accessibility/security behavior. Pinned
tools, triggers, fail-closed exits and non-authorizing diagnostic retention remain.
No mainline publication is used as a resize baseline.

## Qualification ledger

Implementation qualification is in progress. Actual command exits and results
will be recorded here before the corrective PR is finalized. Evidence directory:
`/tmp/doc-resize-ci-001-evidence/`. Initial downloaded hosted artifacts and
`historical-mismatch.json` preserve the exact failure reproduction.

No `READY_FOR_INDEPENDENT_REVIEW` claim is made until mandatory local and hosted
qualification passes. Independent review and authorization remain prerequisites
to merge. PR #233 can incorporate this fix only after independent review and
mainline merge, then must requalify via an ordinary merge; neither PR is merged
by this task.
