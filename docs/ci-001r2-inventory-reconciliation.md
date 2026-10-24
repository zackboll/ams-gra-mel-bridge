# CI-001R2: reviewed IR-CI-003 inventory reconciliation

## Failure and immutable authorities

PR #125 remains the only feature PR, on
`ci/001-parallel-documentation-qualification`. Documentation run `39903927650`
at `c5fc34e5216e17e2366f3b3afc4b6f3fea7f4823` correctly rejected
`scripts/test_doc_source_integrity.py` under the original DOC-012 byte comparison.
Preparation failed, `build` failed, and the matrix was skipped. This is retained
failure evidence, not a native regression or permission to relax assertions.

The reconciled head has tree `5427bdfb7219a20ea67c01bb6375a66668f0a58f` and ordered
parents `a0900430d3b2db8ac5dd9d065eaa66aa88ed6751` (original CI-001) and
`45f0d147521354777a8ace214172ff268e1c3355` (reviewed IR-CI-003 main merge).
The correction preserves this merge history without rebase or force push.

`BASELINE_SHA` remains `9288f6acd4cbf807f576d387db390459f0b117c0`.
The original `docs/tests/ci001-inherited-inventory.json` is unchanged and is now
also compared with its immutable original CI-001 Git object, not a candidate
inventory. All unrelated DOC-012 sources retain their original byte comparisons;
the pre-existing narrow `WorkflowTests` AST exception is unchanged.

The second authority is exactly `45f0d147521354777a8ace214172ff268e1c3355`, tree
`158ee2f4e04b1f2ecf9de33bebd1c5edb61866e4`, with ordered parents
`9288f6acd4cbf807f576d387db390459f0b117c0` and
`27b26cbf5f8b683deeaa731c74c7e37dd4a429a3`.
The checker verifies full commit/tree/parent identities and current ancestry for
DOC-012, original CI-001, and IR-CI-003 on every inventory validation. Neither a
newer main nor the working candidate can supply replacement authority.

Verified SHA-256 values from `git show` of the immutable source commits:

| Source | Authority | SHA-256 |
| --- | --- | --- |
| Original source-integrity tests (51 identities) | DOC-012 | `2d593c5d60f231139355cc40a776249f8d4e0993ab2af62c2c0942d2d9413448` |
| Complete approved source-integrity tests (60 identities) | IR-CI-003 | `1ad3dd314e0b5ef1411a1438b9897a8e7e9c66717b10a4f6776409cb4bfd7e13` |
| `docs/tests/test_navigation_completion_observation.py` (12 identities) | IR-CI-003 | `9155fd124ee69ed0e2a7345d35369c786e885739012d0cd186d940dec2793c72` |

Both working transition files must equal the complete reviewed bytes. Static
method identities are checked for duplication; all original 51 policy methods
must retain their exact ASTs, the nine additions must be discovered, and all
twelve observers must be discovered. Original CI-001's 49 control method ASTs
also remain intact. Unauthorized modules, renamed/missing identities, modified
assertions, and self-consistent forged candidate inventories are rejected.

## Actual inventory and ownership

Discovery, not count-based fabrication, supplies identities. Exact-set
reconciliation preserves all 546 original CI-001 identities and adds the 21
approved IR-CI-003 identities. Twelve CI-001R2 integration controls are additive
above that 567-test starting requirement:

| Source | Scripts | Enhanced | Total |
| --- | ---: | ---: | ---: |
| Original DOC-012 | 133 | 364 | 497 |
| Original CI-001 controls | 0 | 49 | 49 |
| Reviewed IR-CI-003 | 9 | 12 | 21 |
| CI-001R2 controls | 0 | 12 | 12 |
| Required reconciled total | **142** | **437** | **579** |

| Shard | After IR-CI-003 | With CI-001R2 controls |
| --- | ---: | ---: |
| navigation | 34 | 34 |
| publication | 35 | 35 |
| live | 72 | 72 |
| symbols-catalog | 57 | 57 |
| types | 52 | 52 |
| remaining | 175 | 187 |
| Enhanced total | **425** | **437** |

The C observer module belongs to `remaining`, not the previously dominant
navigation shard. It compiles and executes its actual C helper, including the
real failure/timeout/order assertions. No observer or native test source is
modified. All six shards remain; no extra runners or trust-model changes.

`ReviewedTransitionTests` uses isolated candidate directories and independent
immutable Git source inputs. Its twelve methods exercise original admission,
exact transition admission, nine-policy/twelve-observer discovery, exact-once
ownership, missing/duplicate rejection, DOC-012 and IR-CI-003 assertion mutation,
forged inventory rejection, wrong authority/non-ancestor rejection, and
failed/cancelled/skipped/missing dependency rejection. These controls supplement,
not replace, real execution and the original artifact/aggregate controls.

## Qualification protocol and handoff

Local evidence is stored outside the source tree at
`/home/zboll/git/ams-mel-ci-001r2-evidence`. Run the seven normal repository gates
sequentially, then two fresh pristine and two enhanced generations, complete
byte comparisons, direct inventory, script execution, sealing, all six shards,
and real callback-report aggregation in `local-not-deployable` mode. Retain
actual failures as well as successful attempts. Local reports never authorize
deployment. An independent callback reconciliation must find 579 distinct tests,
each with exactly one start/success/stop and no skip/failure/error.

Publication contracts remain 43 files, 38 HTML pages, 4,052 references, 238 stable
symbols, 266 inline example API links, and 31 canonical Ada listings. Pristine
fingerprint remains `2ee21243c766d5183587633947f055a1602c4d744040218db13ee01a8eb33291`;
published remains `2249019abba15ec3107069c18dea5e0934eab25d303524e9a5e9768b476838cb`.
Compare fresh outputs with authenticated GitHub artifact bytes; do not substitute
historical local copies for hosted authority. Native qualification requires
434/434, with no production, provider, binding, example, or assertion changes.

Record completed local commands/results, new exact commit/tree, authenticated
artifact IDs/digests, exact-head Documentation and both eight-job Bootstrap runs,
and queue-free/observed timing in the existing PR qualification comment. This
avoids invalidating exact-head hosted evidence with a later evidence-only commit.
Never infer hosted success from local results or an earlier feature head.

Corrected-main Documentation run `39902708414` at
`45f0d147521354777a8ace214172ff268e1c3355` must independently pass `build`, `deploy`,
and `verify-live`; Bootstrap prerequisite is run `39902708388`. Do not rerun,
cancel, or modify those runs. Feature work does not wait for main completion.
Keep PR #125 open, non-draft, unmerged, and without auto-merge. No merge or manual
deployment is authorized. Until all evidence is complete, no readiness claim is
made by this protocol document.
