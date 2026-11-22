# CI-DOC-SOURCE-FAST-001 — Post-merge source-view qualification

## Purpose

Reduce ordinary MEL API pull-request latency by removing the exhaustive source-view and Chromium campaign from the automatic PR event. Normal Bootstrap, policy, and API documentation checks remain.

## Change

- `.github/workflows/documentation-source.yml` now triggers on a `main` push or explicit `workflow_dispatch`, not on `pull_request`.
- The checkout source identity is the selected event commit (`github.sha`) for both supported events.
- Newer main pushes cancel obsolete main-push source-browser runs. Manual runs use run-specific concurrency groups and are not cancelled by main pushes.
- Pinned official tools, the previous enhancer compatibility control, two full publication reconstructions, source-view parity, source/search/resize tests, Chromium source navigation, exhaustive search, and diagnostic artifacts are retained unchanged.
- `docs/tests/test_documentation_qualification_workflows.py` treats this as a heavy post-merge workflow and asserts all qualification commands remain present.

## CI boundary

- **PR:** `Bootstrap checks` and `Ada, Rust and Python API Documentation` still run. The existing static integrity, security, generated reference, and workflow policy tests remain active.
- **Main push:** the exhaustive source-view and browser campaign runs automatically on the new commit.
- **Manual:** `Documentation source-view qualification (non-deploying)` supports pre-merge qualification when editing documentation-generation or source-browser tools. Dispatch it against the desired ref; checkout uses the event's exact `github.sha`.

The workflow remains read-only and non-deploying. No source-browser assertion, test, permission, artifact retention, or timeout is loosened.

## Validation and merge consideration

Run `make test-docs-ci`, including the new trigger and retained-campaign assertions, and check the exact PR HEAD with hosted Bootstrap and API Documentation. Verify the `main` source-browser run after merging.

At preparation, `main` branch protection reported zero required status check contexts/checks and the repository ruleset listing was empty. Administration of branch-protection settings is not available through this connection; confirm any other organization-level requirements if relevant. This PR does not edit branch protection.
