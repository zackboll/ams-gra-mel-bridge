# IRMEL-POL-002A — qualify the exact IR MEL audit source unit

## Proposal and review boundary

This is a **policy-only proposal**, not independent approval of itself or of
PR #129. It adds no production behavior, public Ada declarations, ABI exports,
vendor inputs, audit scripts or audit JSON. PR #129 must remain at its original
head until this separate authorization is independently reviewed and merged.
No self-approval, auto-merge, deployment or future epoch approval is implied.

Verified fetched main is `e4e756aafa4edaffdb11fe341bb016380bf19ca9`, tree
`41274f8df25f1f3cfd75879778ba760cb84a19e0`. It contains merged PRs #127, #128,
#130, #131 and #132. Original PR #129 is
`47630e61d2631effbbbe3e222e998e33daa90f60`, tree
`6b16f462a5072c22b52f13f7b59fa13280f3e252`; both feature refs matched that
identity at preflight. Its original 13-path diff and all upstream evidence are
preserved, not replaced by a new feature PR.

The original upstream pins remain IR MEL
`8d9224519f12b44e0b28815755c56a32a28d24a0` and Common MEL
`f6908437d8fd2f7fb69896f9eb9cfd272d10c439`.

## Reproduced trusted-target failure

A disposable detached worktree normally merged verified main into original
PR #129 without conflicts. The resulting local-only commit is
`d92f6dd6983aa6ac0e85ad4f3fd0a871074643c2`, tree
`03cf9d5f54b2331a44c23e1ca84e29ed8ccd6bcd`. It is not a reviewed integration
or publication epoch, and is not pushed. Executing the unchanged main
`docs/tools/feature_source.py --root /home/zboll/git/ams-mel-pol-002a-candidate`
against its committed checkout exited **1**:

```text
ValueError: source qualification: out-of-scope delta: Makefile
```

All nine paths below are outside POL-002's documentation and RF ProductRx
surfaces. Seven are new non-documentation inputs not previously authorized
by POL-002. IRMEL-POL-001 already pins the first five as an atomic transition,
but that does not independently authorize the four JSON files under POL-002.

## Exact nine-member transition

Identities were obtained using `git ls-tree` and complete `git cat-file blob`
reads of immutable PR #129, independently recomputing Git SHA-1 (including its
object header) and SHA-256. Candidate JSON is never an authority for its own
hashes. All original identities below also match the earlier reviewed main
`33720d155d99f183229b6ed87037ecbd53a3875b`.

| Path | Mode | Original main blob (absent is literal) | Candidate blob |
| --- | --- | --- | --- |
| `Makefile` | `100644` | `e4c3441a32169c6474dedef00a2db1f798375970` | `7b608a960241c87f88819dbe2cd6bbe42cdbdebd` |
| `scripts/test_ada.sh` | `100755` | `d32e162093a94ba77ab482cb53e98d91e31ee29a` | `9d35abb1345aa8b6a71a9d7b1229f19c16d33239` |
| `scripts/check_ir_api_coverage.py` | `100644` | absent | `6775d2e9986d3b59233e7c464745a1482d047f81` |
| `scripts/freeze_ir_api_evidence.py` | `100644` | absent | `48af395041dd3babb1a1df378e61643064ff91b0` |
| `scripts/test_ir_api_coverage.py` | `100644` | absent | `fa96e0ceb5c16b911fdb0acdfec15b1b53ca981a` |
| `docs/irmel-ada-coverage.json` | `100644` | absent | `fc2f4ce6321764cc59b2da1fc0baa69c4a600e7d` |
| `docs/reference/irmel-reviewed-bindings.json` | `100644` | absent | `a3e078f122877f48787bb198a7bb8f6932521193` |
| `docs/reference/irmel-upstream-api.json` | `100644` | absent | `70d0e228a013bea067de88ac38879d3cb47d896d` |
| `docs/reference/irmel-upstream-git-evidence.json` | `100644` | absent | `22581228eb16ae5508d8df2e3d510c06649ed1cb` |

Candidate SHA-256, in the same order:

```text
446ed30875726cc7dd45e2ca59a48254fed65e5036c0de1542dd5d3c38803ba8
21ec1d4545f0ef2e6dbb0af110bfa9083b1e102e9b2f32ee6b1e9dedd5ee9acf
6a23700ea3eaa7de8ed7f57d5dcd43d38fc7c5cdd64f67bbf9ad7494202e9e31
a8f371013211ffaba2f0723388b50acb79f24b8c4abe5e1748b42a6dc38abc9e
96ff71c641673f36ea90a9c1a7707e2eae367000c0e9a16dd3bd4bc1d621b333
b6e772e1443264d52e5c9f905516060b45c8bb15328c5cd7c634b3d7961be74f
d0c35f2dc27713bb15581cb1f324a7210d4fce079f39caee371e9ef8fb995f2e
b50839f8a7afe89573a30883613658248ee37e7cf5a983e1674b8987c5861e1e
7f4e10e3a60f38668de3c473288769c7f3fe14d85e6e1489f4e3770f5bbb17f1
```

Original Makefile SHA-256 is
`7cf0c004e277af70766685f91663c1acde5980fcd1037b42c1092e1c310a5236`;
original Ada test script SHA-256 is
`b3d19397440873dbf427fe77663083dc99ab5f13e14cd937e0a250fafb09af66`.

## Mechanical enforcement

The new qualifier accepts only the complete original-to-candidate nine-file
transition. It validates immutable provenance trees and requires target main
to descend from the verified main above. Every original member must match its
exact original presence, mode and contents; every candidate member must match
its exact candidate mode, Git blob and full SHA-256. Duplicate or extra registry
members fail closed. The first five entries must equal the single literal
`IRMEL_POL_001_TRANSITION` in the trusted target's source checker. The checker
is parsed as data, never executed from candidate-controlled source.

Only documentation paths may accompany this atomic transition. Additional
scripts, generic JSON evidence, RF edits, Ada/native changes, source removal
and candidate policy/approval-registry edits are rejected. No `scripts/` or
`docs/` directory exemption is added. The original RF_PRODUCT_RX declaration,
historical identities, scope controls and existing assertions remain intact.

The audit result remains **feature-source**, with both approval flags false.
Unchanged trusted-target POL-002 orchestration therefore still runs native,
Ada, Rust, Python, Alire, repository and formatting gates, deterministic
historical/candidate generation, immutable artifact sealing and all six shards.
It cannot use the policy-only classification to bypass those gates. The policy
proposal itself uses the existing isolated policy-only review mechanism, not
an exception authorizing its own audit payload.

New independent tests use isolated committed clones, a synthetic separately
committed target policy, and the actual exported target CLI. They cover the
exact audit, each missing/modified/mode-changed member, mixed original and
candidate groups, duplicate/extra registry entries, incomplete target groups,
POL-001 disagreement, extra scripts/JSON, unrelated executable changes,
removals, candidate policy edits, forged approval and incorrect ancestry.
Documentation-only and RF candidates still run through that same CLI.

## Qualification evidence and stop condition

Task-owned raw evidence is retained at
`/home/zboll/git/ams-mel-pol-002a-evidence`; the isolated policy worktree is
`/home/zboll/git/ams-mel-pol-002a`. Historical external evidence is not edited.
Local results are not hosted success or independent authorization. The original
579 historical test identities and 71 merged CI-002 additions must remain
discoverable separately from the additional explicitly discovered POL-002 tests.

The original failed GCC Debug run `40446029045`, job `121997407811`, remains
separate Track lifetime evidence: repetition 22/50,
`test_ir_track_system_response.c:513`,
`strstr(log, "library_unloaded") != NULL`. It is **not IR-CI-004**. Its full
job log was retrieved through the authenticated job-log API; the empty CLI-log
attempt is retained as an unsuccessful retrieval. The successful same-head
push run `40445977646` does not invalidate that failure. No Track assertion or
implementation change belongs in this policy proposal or PR #129.

Because Makefile is part of the production-source fingerprint, the audit does
not automatically inherit historical publication authority. The unchanged
main-only POL-002 gate requires a separately reviewed actual post-merge epoch
before publishing this changed source identity. No future merge SHA is invented
or preapproved. PR and local qualification remain nondeployable.

Until this exact policy authorization is independently reviewed and merged,
the disposition for PR #129 is **BLOCKED_POLICY_AUTHORIZATION**. Do not push a
changed feature head, start IRMEL-ADA-002, request merge, or treat mechanical
qualification as review approval.

## R2 reconciliation with merged CI-001R3-R4

Verified fetched main advanced to `4d73fd441a21e9c3cc4c54b1a754f9b15cc1ffaf`
(merged PR #137, CI-001R3-R4, which carries CI-ACTIONS-001 and DOC-CI-004).
It was normally merged into the existing PR head
`ca0221e019481176ddbbe274eef7762a57468f62` as
`69b956fa06928ee4ebd5d2dda14d672def8f2d0e`, tree
`07a35fba9f1369cbd9e40398a7c7ebb6828d571f`. The merge was not rebased and had
no conflicts. Both original commits stay in history. Every main file is carried
byte-identically: the corrected-replay policy, DOC-CI-004 diagnostics, Node 24
action pins, workflows and `docs/coverage.md`. Relative to main, this proposal
still changes only this record, `docs/tests/pol002_source_tests.py` and
`docs/tools/feature_source.py`. The current `epoch_ci.classify` route therefore
classifies it **policy-only**, with both approval flags false.

The audit registry and its controls are unchanged. On the merged tree, the
first five registry entries still equal main's literal
`IRMEL_POL_001_TRANSITION`. Main's Makefile and `scripts/test_ada.sh` still
have the exact original modes and blobs. PR #129 is unchanged at
`47630e61d2631effbbbe3e222e998e33daa90f60`.

### Reproduced CI-001R3-R4 blocker (not resolved here)

Both checks below were run in disposable, local-only clones.

- Normally merging unchanged PR #129 into `69b956f…` yields `5eafc344…`
  (tree `57927587…`), with exactly the original 13-path delta. The proposed
  trusted `feature_source.py` CLI classifies it **feature-source**, never
  policy-only, with both approval flags false.
- Current `epoch_ci.classify` rejects that same merge before reaching
  `feature_source.qualify`:
  `CI-001R3-R4 historical replay: not a complete verified predecessor checkout`.
  The same error results from a one-line documentation-only `docs/coverage.md`
  edit on unchanged main `4d73fd44…`. It is not caused by this proposal.

Cause: `docs/coverage.md` is a member of two one-time successor sets.
`ci001r4_replay.source_transition` (`SUCCESSOR`) and
`doc_ci004_source.diagnostic_transition` (`SUCCESSOR`) are each entered whenever
any member differs from the target. Each then requires the target tree to equal
one of its fixed historical predecessors: `219b00c…`/`e85d99b…` and
`219b00c…`/`e7eaba2…`. Called directly, both validators reject both
reproductions with "not a complete verified predecessor checkout". Any later
change to `docs/coverage.md`, including PR #129's, therefore cannot be
classified on current main.

This proposal does not modify either validator, the replay binding, or any
CI-001R3-R4 test or control. The correction must be a separate, narrowly scoped
CI-001R3-R5 corrective. That corrective should limit both one-time validators to
their intended historical source transitions, and it must preserve every
exact-content historical control, negative test, trusted-source boundary and
publication authority. Merging this proposal alone does not authorize PR #129.
PR #129 stays **BLOCKED_POLICY_AUTHORIZATION** until both this authorization and
that corrective are independently reviewed and merged. Then it can be normally
synchronized and fully requalified.

Qualification results for this exact head stay in external evidence
(`/home/zboll/git/ams-mel-pol-002a-r2-evidence`), not in this record. They are
local, not deployable, and do not constitute approval.
