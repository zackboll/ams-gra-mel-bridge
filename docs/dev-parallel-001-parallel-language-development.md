# DEV-PARALLEL-001 — Conflict-resistant parallel language development

## Source identity and scope

- Repository: `https://github.com/zackboll/ams-gra-mel-bridge`.
- Dedicated branch: `docs/dev-parallel-001`, created after `git fetch origin main`.
- Source main commit: `240c299ca3630c243ed64bc3e1c0692a562316e6`.
- Source main tree: `e8fce61870900497e3865c7f0f45d8e9214d2456`.
- The delivery PR records its exact candidate commit/tree; this document cannot
  embed its own containing commit identity.

Coverage delta: documentation/workflow conventions only; no API implementation
or coverage classification changes. Historical coverage remains intact. Current
structured IR Ada coverage remains 127 headers / 148 operations: 47 safe Ada,
13 partial, 19 missing, 69 provider-only and zero native-only.

This change requires uniquely named, task-owned feature documents instead of
routine shared status-file edits. It does not change runtime behavior, native
ABI, public binding APIs, CI workload selection, build scripts, dependency pins,
security/deployment configuration or Git merge attributes/drivers. It adds no
automatic conflict resolver or blanket union merge. It changes conventions, not
enforcement scripts or review gates.

## Changed-file inventory

| Path | Purpose |
| --- | --- |
| `AGENTS.md` | Task-document ownership, archive policy and correctness exceptions. |
| `.clinerules/06-agent-workflow.md` | Matching agent workflow rules; existing validation requirements retained. |
| `CONTRIBUTING.md` | Contributor convention, required task-document contents and migration guidance. |
| `docs/coverage.md` | One archive notice only; all historical entries preserved byte-for-byte. |
| `docs/dev-parallel-001-parallel-language-development.md` | This task's boundaries, proof, actual validation and migration plan. |

No shared feature-status index is introduced. Native ABI, shared interfaces,
security contracts and authoritative structured coverage/API inventories must
still change whenever correctness requires it, including generated reports.
For IR Ada, use the existing [inventory update procedure](reference/irmel-api-schema.md#reproduction-and-updates).

## Disposable-branch proof

A temporary local shared clone of the repository used three separate feature
branches with the exact common parent above. Each branch added just one uniquely
owned document under `docs/`, containing coverage delta, boundaries, tests and
limitations. None changed implementation or a shared status file. The fixture
identities from the actual run were:

| Language | Feature commit | Feature tree |
| --- | --- | --- |
| Ada | `6d0a43aff82d08427d3a19bab2a5f8536f3c9498` | `db76a0370e35c8df90b5b490f8f2ca60ea4fd9a3` |
| Python | `8fd09cbeddd126451f0a11611af8ca3e9512bfec` | `c2022505e7ff20fa7937d50af5efb4483698c554` |
| Rust | `0607c7128cb67d086ee933774f67e0b71e8bbf27` | `ab99a8b603398912c43e9bb0958982e13159c23b` |

Every merge used ordinary `git merge --no-edit --no-ff`, without a strategy
override, merge driver, union attributes or rerere assistance. For each order,
an integration branch started at the common source commit. Before integrating
each feature, another disposable branch at that feature's original head merged
the advanced integration head, exercising GitHub **Update branch**'s merge
direction as well as feature-into-main merges.

| Integration order | Feature into integration | Advanced integration into feature |
| --- | --- | --- |
| Ada → Python → Rust | PASS | PASS |
| Ada → Rust → Python | PASS | PASS |
| Python → Ada → Rust | PASS | PASS |
| Python → Rust → Ada | PASS | PASS |
| Rust → Ada → Python | PASS | PASS |
| Rust → Python → Ada | PASS | PASS |

Results: 18 integration merges and 18 update-direction merges completed with no
conflicts. Every intermediate result had exactly the expected added paths,
byte-identical document contents, a clean working tree and no unmerged index
entries. All six final trees were identical:
`16e39f800896f4d321de7ed58ad084ffd2cb3b71`.

A negative control created two branches from the same source and prepended
different headings to `docs/coverage.md`. Their normal merge exited 1 with
unmerged index entries, as expected. The merge was aborted. This demonstrates
that the proof did not hide conflicts using permissive merge behavior. The
temporary clone and all fixture branches were deleted; nothing was pushed.

### Reproduction

From the repository root, the following Python stdlib snippet creates disposable
branches and verifies all six orders in both directions. It uses the recorded
source commit, not a moving branch. Fixture commit hashes vary with timestamps;
the content and final tree are reproducible.

```python
import itertools
from pathlib import Path
import subprocess
import tempfile

source = Path.cwd().resolve()
base = "240c299ca3630c243ed64bc3e1c0692a562316e6"
with tempfile.TemporaryDirectory(prefix="dev-parallel-001-") as tmp:
    repo = Path(tmp) / "repo"
    subprocess.run(["git", "clone", "--quiet", "--shared", "--no-checkout",
                    str(source), str(repo)], check=True)

    def git(*args):
        return subprocess.check_output(["git", "-C", str(repo), *args], text=True)

    for key, value in (("user.name", "DEV-PARALLEL-001 fixture"),
                       ("user.email", "fixture@example.invalid"),
                       ("commit.gpgsign", "false"), ("rerere.enabled", "false"),
                       ("core.hooksPath", "/dev/null")):
        git("config", key, value)  # temporary clone only; not global settings
    files = {}
    for language in ("ada", "python", "rust"):
        git("checkout", "--quiet", "-b", "fixture/" + language, base)
        name = f"docs/dev-parallel-001-{language}-fixture.md"
        files[name] = (
            f"# DEV-PARALLEL-001 {language} documentation fixture\n\n"
            "Coverage delta: simulated documentation only; no API change.\n"
            f"Boundaries: only this {language}-owned task document.\n"
            "Tests: ordinary Git merges in all six language orders.\n"
            "Limitations: no implementation or provider qualification.\n")
        (repo / name).write_text(files[name])
        git("add", name)
        git("commit", "--quiet", "-m", language + " owned documentation fixture")
        assert git("rev-parse", "HEAD^").strip() == base
    trees = set()
    for number, order in enumerate(itertools.permutations(("ada", "python", "rust"))):
        branch = f"integration/{number}"
        git("checkout", "--quiet", "-b", branch, base)
        expected = set()
        for position, language in enumerate(order):
            advanced = git("rev-parse", "HEAD").strip()
            git("checkout", "--quiet", "-b", f"update/{number}/{position}",
                "fixture/" + language)
            git("merge", "--no-edit", "--no-ff", advanced)
            expected.add(f"docs/dev-parallel-001-{language}-fixture.md")
            for target in (branch, None):
                assert set(git("diff", "--name-only", base, "HEAD").splitlines()) == expected
                assert all((repo / name).read_text() == files[name] for name in expected)
                assert not git("status", "--porcelain") and not git("ls-files", "-u")
                git("diff", "--check", base, "HEAD")
                if target:
                    git("checkout", "--quiet", target)
                    git("merge", "--no-edit", "--no-ff", "fixture/" + language)
        trees.add(git("rev-parse", "HEAD^{tree}").strip())
        print("PASS", " -> ".join(order))
    assert trees == {"16e39f800896f4d321de7ed58ad084ffd2cb3b71"}
```

## Actual validation

Commands ran from the dedicated worktree; no tests or contracts were weakened.

| Command/check | Actual result |
| --- | --- |
| `python3 scripts/check_doc_source_integrity.py` | PASS: 804 vendor files and three workflows. |
| `make test-policy` | PASS: 234 tests across integrity (18), source policy (18), historical bookkeeping (28), epoch boundaries (64), Bootstrap policy (75), current docs pipeline (18), documentation coverage (13). |
| `make test-docs-ci` (invoked by `make test-policy`) | PASS: 18 pipeline + 13 coverage tests, included in the total above. |
| `make check-ir-api-coverage` | PASS: offline structured-inventory audit and 46 regression tests; coverage unchanged. |
| Disposable merge proof | PASS: all six orders, both directions, exact content/tree checks and expected shared-prepend negative control. |
| Historical archive comparison | PASS: removing only the new notice yields the source archive byte-for-byte. |
| Local Markdown links/anchors and reproduction snippet | PASS: new links resolve; the snippet runs successfully in a fresh temporary clone. |
| `git diff --check` | PASS. |

Bootstrap policy tests intentionally execute failing CTest fixtures to verify
failure propagation; the enclosing 75-test suite passed. These are not full
runtime/native-suite results.

Full `make docs` and `make test-documentation-source` are unavailable in this
worktree: the pinned `.doc-tools/enhancer/bin/python` and
`.doc-tools/gnatdoc-26.0.0/bin/gnatdoc` are absent, and GPRbuild is not on PATH.
No documentation dependencies were installed. A `gnatdoc` executable on PATH
alone is not the required pinned environment. No full generated-site, compiled
language, provider/hardware, hosted CI or deployment success is claimed by this
local validation. Native/Ada/Rust/Python runtime suites and `make check` were not
run for this documentation-only change. No Ada source changed, so Ada formatting
was not required.

## Migration plan for existing feature PRs

Read-only inspection found these open heads; this task does not modify them:

| PR | Observed head | Author migration after this convention is accepted |
| --- | --- | --- |
| #174 PY-MEL-008 | `c3d0505a1f4093d343ba56a15b706768a38e3815` | Move its new archive section to a uniquely owned `py-mel-008-…` task document, retaining every coverage/test/limitation detail. Keep Python implementation/tests; move status-only README additions to the task document, retaining necessary user guidance. |
| #175 RUST-IR-006 | `d3be5368f04f2483ef11c6bc8b61bf1cc28d88cf` | Move its new archive section into its existing `rust-ir-006-optical-distortion-map.md`; retain Rust implementation/tests and all evidence. |
| #177 IRMEL-ADA-011 | `81211b4d3c87fa1704d25678929b364930b8274d` | Consolidate new archive/changelog/status-only architecture prose into its existing `irmel-ada-011-scheduling-foundation.md`. Retain necessary native ABI, shared raw mirrors, contracts, provenance, structured inventories/generated reports and tests. |

Authors should restore the historical archive to the accepted main version,
not discard their new information. Then use normal **Update branch**, reconcile
any genuine shared-code/contract overlaps and rerun applicable validation.
This one-time migration may itself need conflict resolution; the change cannot
retroactively remove overlaps from existing heads. Shared native work remains
coordinated work, not a promise of conflict-free merging. No existing feature PR
was modified, approved, merged or given auto-merge. The new PR remains open for
independent review.
