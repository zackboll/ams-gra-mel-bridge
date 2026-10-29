# CI-ACTIONS-001: reviewed Node 24 action dependencies

## Scope, identity and disposition

Protected main baseline `e4e756aafa4edaffdb11fe341bb016380bf19ca9`, tree
`41274f8df25f1f3cfd75879778ba760cb84a19e0`. Isolated branch
`ci/actions-001-node24`, worktree `/home/zboll/git/ams-mel-ci-actions-001`.
No production/Ada/binding/provider changes; no trigger, job/check-name, permission,
concurrency, workload, shard, artifact identity or publication-gate changes.
The only additional action input preserves historical Pages hidden-file behavior.

Disposition: **CORRECTIVE_WORK_REQUIRED — qualification/review incomplete**.
No commit/push until mandatory local gates pass. Require one non-draft separate
PR, auto-merge disabled, exact-head hosted qualification and independent review.
This document records a proposal, not approval of its own dependency/source policy.

CI-003R1 in `/home/zboll/git/ams-mel` remains untouched: HEAD
`74141c0463d112a0408ff9815c1b212030171322`, staged tree
`c21779654211ad4e22b2822022b0bd208de5ae35`, PR #133. Do not commit its staged
changes, merge into it, rebase, force-push or replace it during this migration.

## Baseline lint and complete occurrence inventory

Pinned `kjanat/actionlint` v1.17.0, source/build
`08bb2c4f0d039744455b87aef1c647fb8b66d37b`, executable SHA-256
`6e0e370a627682be87b2f20e4b4e9bed0d675cec3f7feac206baed7873e8e5fd`.
The immutable release archive checksum and verified Sigstore attestation bind the
artifact to the GitHub-hosted release workflow at that exact source commit; the
verification is repeated for this task. Raw provenance/release/verification and
original unsuppressed output: `/tmp/ci-actions-001-evidence`.

```sh
cd /home/zboll/git/ams-mel-ci-actions-001
/tmp/ci003r1-evidence/tools/actionlint .github/workflows/ci.yml .github/workflows/documentation.yml
```

Unchanged baseline: exit 1, **23 Node 20 findings**. No ignores, custom policies,
content sanitization or clock overrides. Every line below is a separate finding;
the reference versions declare `runs.using: node20`. Version tags were resolved
for audit only; old workflow tags are not immutable historical release locks.

| Workflow | Baseline line | Action/version | Dependency | Successor / compatibility review |
| --- | ---: | --- | --- | --- |
| ci.yml | 25 | checkout@v4 | direct | C |
| ci.yml | 47 | checkout@v4 | direct | C |
| ci.yml | 54 | checkout@v4 | direct | C |
| ci.yml | 69 | checkout@v4 | direct | C |
| ci.yml | 96 | checkout@v4 | direct | C |
| documentation.yml | 31 | checkout@v4 | direct | C |
| documentation.yml | 72 | upload-artifact@v4 | direct | U |
| documentation.yml | 81 | upload-artifact@v4 | direct | U |
| documentation.yml | 99 | checkout@v4 | direct | C |
| documentation.yml | 104 | download-artifact@v4 | direct | D |
| documentation.yml | 129 | upload-artifact@v4 | direct | U |
| documentation.yml | 157 | checkout@v4 | direct | C |
| documentation.yml | 162 | download-artifact@v4 | direct | D |
| documentation.yml | 168 | download-artifact@v4 | direct | D |
| documentation.yml | 195 | upload-artifact@v4 | direct | U |
| documentation.yml | 216 | checkout@v4 | direct | C |
| documentation.yml | 221 | download-artifact@v4 | direct | D |
| documentation.yml | 227 | download-artifact@v4 | direct | D |
| documentation.yml | 255 | deploy-pages@v4 | direct | P |
| documentation.yml | 268 | checkout@v4 | direct | C |
| documentation.yml | 277 | download-artifact@v4 | direct | D |
| documentation.yml | 283 | download-artifact@v4 | direct | D |
| documentation.yml | 315 | upload-artifact@v4 | direct | U |

Additional nonreported direct composite at documentation.yml:249:
`upload-pages-artifact@v3` invokes **transitive** `upload-artifact@v4` (Node 20).
It is migrated too (H below), not ignored because top-level lint missed it.

Resolved old refs: checkout v4 `11d5960a326750d5838078e36cf38b85af677262`,
upload v4 `ea165f8d65b6e75b540449e92b4886f43607fa02`, download v4
`d3f86a106a0bac45b974a628896c90dbdf5c8093`, deploy v4
`d6db90164ac5ed86f2b6aed7e0febac5b3c0c03e`. Exact action.yml evidence retains
the declared runtimes and old Pages composite commands.

## Selected immutable revisions and compatibility

| Key | Selected release | Full commit SHA | Runtime |
| --- | --- | --- | --- |
| C | checkout v5.1.0 | `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` | node24 |
| U | upload-artifact v6.0.0 | `b7c566a772e6b6bfb58ed0dc250532a479d7789f` | node24 |
| D | download-artifact v7.0.0 | `37930b1c2abaa49bbe596cd826c3c89aef350131` | node24 |
| H | upload-pages-artifact v5.0.0 | `fc324d3547104276b827a68afc52ff2a11cc49c9` | composite |
| P | deploy-pages v5.0.0 | `cd2ce8fcbc39b97be8ca5fce6e763baed58fa128` | node24 |
| H child | upload-artifact v7.0.0 | `bbbca2ddaa5d8feaa63e36b76fdaad77386f024f` | node24 |

Each exact release tag resolves to the listed commit; GitHub reports valid
verified commit signatures. Retain signed payload/signature, release metadata,
tag resolution and exact Git tree. Downloaded commit-addressed archives are
checked against that signed tree's Git blob identities for action.yml,
package.json and every distributed executable used. This is source/release
provenance, not a fabricated independent reproducible-build attestation. No
GitHub artifact attestation is assumed where one was not supplied. Moving major
tags and release metadata alone are not trust anchors; workflow pins are full SHAs.

- **C:** v5 is the narrow Node 24 line (runner >=2.327.1), avoiding v6/v7's
  credential-storage change. v5.1.0 includes safer custom fork checkout guards
  for privileged events and whitespace handling fixes. Our ordinary PR/default
  merge checkout and explicit 40-hex documentation source SHA remain supported;
  no privileged trigger or unsafe-checkout opt-in is added. Existing fetch depth,
  ref, default credential persistence and post-cleanup remain intact. Inputs used
  here and outputs are unchanged; no artifact format is involved.
- **U:** v6.0.0 changes runtime to Node 24 and updates artifact dependencies;
  action input/output schema matches old v4 except the runtime. Immutable ZIP
  artifact service, `artifact-id`/digest outputs, retention, compression and
  hidden-file defaults remain. Existing named unique artifacts and tar payloads
  are preserved. We deliberately do not adopt direct-upload mode from v7 here.
- **D:** v7.0.0 is Node 24. The v5-era single-ID extraction now goes directly to
  `path`; every ID download here already specifies `merge-multiple: true`, so its
  expected path is unchanged. Pattern-based six-shard downloads retain distinct
  artifact directories (at least six artifacts; missing shards fail aggregate).
  Same-run default token/repository/run scope and supplied IDs remain. No v8
  direct-download/ESM/digest-policy migration is bundled into this change.
- **H:** v4 Pages upload excluded dotfiles; v5 exposes `include-hidden-files`.
  We set it **true** to preserve v3 tar membership, including `.nojekyll` and any
  approved `.well-known` content, while excluding `.git` and `.github`. The child
  is pinned upstream to upload v7.0.0, Node 24; its new `archive` input defaults
  **true**, so upload remains ZIP-wrapped `artifact.tar`, not a raw direct upload.
  Artifact naming, error-on-missing, retention, tar dereference behavior and
  `artifact_id` output remain. No Pages deployment protocol change is intended.
- **P:** v5.0.0 changes Node runtime with matching input/output schema. Select it
  instead of v5.0.1's polling backoff/jitter change to preserve timing behavior.
  OIDC/pages permissions, environment, artifact-name lookup, default timeout,
  deployment gate and main-only condition remain. Upstream also removes one
  obsolete permission-error message, not our validation or deployment checks.

All jobs use GitHub-hosted ubuntu-24.04; no self-hosted requirement is introduced.
Additional existing actions were inspected: setup-alire v6 is composite, uses
setup-node v6 selecting Node 24 and cache restore/save v5; rust-toolchain stable
is composite shell. Neither is implicated by these 23 diagnostics. Their existing
behavior/cache use is not changed or represented as newly reviewed cache approval.

## Exact source authority and controls

`ACTIONS_WORKFLOWS` admits only complete predecessor/candidate workflow pairs,
plus exact historical pairs required by frozen fixtures. Both index and working
bytes/modes, stage zero, parity, symlinks, missing files, unknown content and
partially applied transitions are checked. DOC-001 foundation and CI-002
predecessor remain; rejected HEAD-only CI-002 proposal stays rejected.

POL-002's policy-only path has an exact SHA-256 old/new exception for these two
workflows, the integrity checker and new independent control module, not a broad
path allowance. All other protected production and publication inputs retain
existing checks. New controls join the sealed integration suite; frozen historical
test sources/inventory, publication fingerprints and authority registry do not
change. Negative controls mutate pins, permissions, conditions, commands,
artifact IDs, extraction/hidden-file behavior, bytes, modes and transition parity.

## Qualification and delivery

Evidence root `/tmp/ci-actions-001-evidence`; build/tool directories are ignored.
Compatible unsuppressed lint on migrated workflows exits **0**, distinct from
preserved baseline exit 1. Completed local gate results:

| Gate | Actual result |
| --- | --- |
| Native C/C++ | 434/434 passed |
| Ada smoke, make check, Ada formatting | exit 0 each |
| Alire Ada build and tests | exit 0 each, pinned toolchain |
| Python | 145 passed; compileall passed |
| Rust tests/check/clippy/format | exit 0 each |
| Canonical examples | 13 cases / 9 compiled programs passed |
| Script / Bootstrap policy controls | 142 / 71 passed |
| Standalone tool controls | 59 passed |
| POL-002 source / complete epoch integration | 41 / 65 passed; callbacks reconciled |
| Targeted migration integration | 14 passed |
| Independent dependency/runtime/I/O/Pages tar inspection | passed |
| Historical sealed preparation + six isolated shards + aggregate | 142 scripts + 437 enhanced passed |
| Source integrity / final newlines / Git whitespace | passed |

The six frozen shard totals are live 72, navigation 34, publication 35, remaining
187, symbols-catalog 57 and types 52. Every report was independently reconciled
against its exact inventory and start/success/stop callbacks. Two separate source
clones regenerated identical pristine/publication artifacts. These reports are
explicitly **local-not-deployable**, not hosted artifact-service qualification.
Original inventory authority and fingerprints remain:
`2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`
(pristine),
`2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`
(published). Local settings reuse only the explicitly identified pinned toolchain
installation/release cache; no test/site/native result cache is used. The reviewed
enhancer was freshly installed from its independently checked immutable archive.
No unfinished attempt is counted as success. Complete outer exact-commit
preparation/artifact transfer and hosted qualification remain separate requirements.

Before committing, prove CI-003R1 HEAD/index/worktree identities are unchanged.
Before pushing, require all mandatory local gates green. Open one non-draft PR
against reviewed main with auto-merge disabled. Hosted qualification must retain
all eight Bootstrap instances, preparation/six shards/build, same-run bundle and
result IDs, digest/manifest reconciliation and original check names. Main-only
deploy/live are not falsely claimed as executed on a PR; actual publication
qualification is required on main without changing conditions to deploy PR code.

After independent acceptance and merge only, resume PR #133 by normal Git merge,
preserve its staged serialization correction, recalculate exact authorities on
new main and rerun every CI-003/local/lint gate before a normal commit/push.
Require corrected exact-head hosted CI and independent review. Actual shared
three-runner contention, main deployment/live and timing comparison remain
post-merge gates for CI-003, not consequences of a green dependency PR.
