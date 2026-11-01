# DOC-CI-005 — Full Documentation CI only after merge to main (proposal)

## Change

`.github/workflows/documentation.yml` (**Ada API Documentation**) loses its
`pull_request` and `workflow_dispatch` triggers. Its only trigger is now:

```yaml
on:
  push:
    branches: [main]
```

Nothing else in the file changes. The jobs (`prepare-documentation`, six
`enhanced-tests` shards, aggregate `build`, `deploy`, `verify-live`), the
read-only top-level permissions, the deploy-only `pages`/`id-token` grants,
the action pins, the same-run artifact IDs, the aggregate deployment gate and
the main-only deployment conditions are all byte-identical. The CI-001R3
workflow contracts (`live_contract`, `preparation_contract`, `deploy_contract`)
still pass against the new file.

The Bootstrap workflow (`.github/workflows/ci.yml`) is unchanged. It still
runs on pull requests and on pushes to main: the gcc/clang × Debug/Release
native matrix behind `bootstrap_ci_policy.py`, build-tree isolation, Ada
(including the format check), Rust and Python.

## Accepted limitation: no hosted POL-002 gate on PRs

Removing Documentation CI from pull requests **also removes the full hosted
POL-002 pull-request qualification gate.** Until now, every PR's Documentation
run performed these steps before merge:

- `epoch_ci.py select` from the trusted target;
- historical replay;
- candidate epoch generation;
- the 579 historical and 650 current documentation tests;
- the language qualification commands;
- the aggregate gate.

Those checks now run only on the post-merge push to `main`. This deferral is
accepted. A documentation-policy failure on `main` is a follow-up maintenance
task. A lightweight PR source-policy check can be added separately. This
change does not add one.

Deployment safety is unchanged. Pages publication still requires a hosted
`push` to `refs/heads/main`, a successful same-run aggregate gate, and
publication authority recomputed from trusted Git (`publication_decision` /
`deployment`). The publication approval registry in `source_epoch.py` is
unchanged.

## Exact-source authorization

The transition is admitted only as one complete unit, defined from its exact
predecessor main `d5243dd924c4a81a548e38d97533e29f9971deea`:

| File | Predecessor SHA-256 | Successor |
| --- | --- | --- |
| `.github/workflows/documentation.yml` | `1b4be1e4…c711` | `8971d0d3…b4b8` |
| `scripts/check_doc_source_integrity.py` | `3e8d60e6…f602` | `c270fc76…7532` |
| `docs/tools/test_actions_dependencies.py` | `208caa1d…e012` | `4d2362f7…3b7b` |
| `docs/tools/epoch_ci.py` | `19c339d8…a9d9` | must equal the executing policy bytes |

- `check_doc_source_integrity.py` adds the `MAIN_ONLY_WORKFLOWS` pair, the
  unchanged Bootstrap hash plus the new Documentation hash. It reports
  `main_only_proposed_pending_independent_review`.
- `epoch_ci.main_only_delta` admits the unit only when the target's workflow is
  exactly the DOC-CI-004R1 bytes. Every member must have the listed
  predecessor and successor bytes and mode `100644`, and the publication
  registry must be unchanged.
- All earlier identities and their validation routes remain:
  CI-ACTIONS-001, DOC-CI-004R1, the historical pairs and the CI-001R3-R5 scope.
- The classification remains `policy-only`, with `independent_approval` and
  `publication_epoch_approved` both false. The candidate does not approve
  itself.

Two DOC-CI-004R1 parity tests in `pol002_epoch_tests.py` now read that
successor from the immutable predecessor main instead of the working tree.
Their assertions are unchanged. The new `MainOnlyDocumentationTests` class
checks four things:

- the trigger set;
- the exact transition and its policy-only, unapproved result;
- rejection of incomplete, unknown, executable and hitchhiking changes (source
  registry, Bootstrap, production source, protected baseline);
- rejection of workflow mutations that re-add a PR or manual trigger, grant a
  write token, bypass deployment, or substitute an untrusted or unpinned
  action.

## Required checks

Branch protection on `main` lists no required status checks, and the
repository has no rulesets. No Documentation check therefore blocks merging,
and no settings change is needed.

## Qualification

Qualification for this change was focused only:

- actionlint;
- the source-integrity checker;
- the targeted POL-002 and source-policy tests listed in the PR.

The historical generation, the full documentation test campaign, FULL_STRESS
and the five-stage local campaign were not run. They run on `main` after
merge.
