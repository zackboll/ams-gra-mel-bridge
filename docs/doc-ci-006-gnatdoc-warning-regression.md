# DOC-CI-006 — current-source GNATdoc warning correction

## Disposition and scope

**CORRECTIVE_WORK_REQUIRED.** The original warning mismatch is reproduced and
the four unexpected warnings are eliminated by declaration-specific comments.
The focused suite is **not green**: the unchanged coverage ratchet subsequently
rejects 17 pre-existing source/rendered prose gaps. This PR does not claim that
main Documentation qualification is restored. Resolve that separate dependency
before claiming acceptance or running the expensive full qualification again.

The implementation change is four comment lines, two each in
`ada/src/ams-mel-ir-image-metadata.ads` and
`ada/src/ams-mel-ir-health_status-metadata.ads`. No signatures, implementation,
layouts, exports, provider semantics, fixtures, warning/coverage baselines,
historical inputs, source-policy admission rules, timeouts, or workflows change.

## Original evidence and identity

Starting fetched main:

- Commit: `888553cba1084efea6a201da27dc1fa7cfd2f34b`.
- Tree: `95df6b06ea0be8c5424bc912eda175ab1b5cbe56`.

Original failed main:

- Commit: `e84ab3830034f4e2f09ae736901e6ea2526dd1c2`.
- Tree: `56dbe9c7ec780786a238c1572395689e35b60a0c`.
- [Run 41276037954, attempt 1](https://github.com/zackboll/ams-gra-mel-bridge/actions/runs/41276037954),
  failed job `124758894462`, push to main, conclusion `failure`.
- Artifact `13073078674`,
  `gnatdoc-diagnostics-41276037954-1-e84ab3830034f4e2f09ae736901e6ea2526dd1c2`.
- Downloaded ZIP SHA-256, matching GitHub's artifact digest:
  `e0cb77954f971bb794709aa1b9ba8e86cfbdcc343a81bd0f81f10d0b0cc4b7c0`.

Task-only external evidence is retained at
`/home/zboll/doc-ci-006-evidence`, outside tracked repositories. It contains
the original API metadata, complete failed-job log, original diagnostic ZIP
and extracted files, failed-source Git archive, checksums, tool installation
and version logs, separate failed/current/corrected reproductions, raw GNATdoc
logs, actual baselines and Ada sources, synthetic commit/tree identities,
command arrays, real return codes, and qualification logs.

The original temporary `epoch-generation.log` was absent from the downloaded
artifact. Its contents are not asserted from history. The nested output below
was **reproduced**, not recovered. External `sitecustomize.py` instrumentation
observes completed subprocess results and copies diagnostic files before normal
temporary-directory cleanup. It does not change results, exceptions or cleanup,
and no instrumentation is committed to the repository.

No open PR or issue claimed DOC-CI-006 at preflight. Separate clean worktrees
were used for the failed commit and corrective branch. Other worktrees and
running hosted CI were not modified or terminated.

## Reproduction and exact warning identities

Environment: Alire 2.1.1, GNAT 16.1.0, GPRbuild crate 26.0.1 reporting
`GPRBUILD 26.0.0 (2026-04-15)`, GNATdoc 26.0.0 (20260415), and the unchanged
reviewed enhancer archive at `524b7610e21e9a7b87e8e0761580ed59a3b0295c`
with archive SHA-256
`8b7c0a0d0957ef2ebb2cc27888173e653db6987c4279444fcffe095ccc099101`.
Alire settings and the newly installed enhancer Python environment are isolated
under the external evidence directory. `GNATDOC_PREFIX` names the verified
26.0.0 installation.

The qualified Python invocation on both unchanged failed main and current main:

```sh
python -W error -m unittest discover \
  -s docs/tests -p source_epoch_regression_tests.py -v
```

Both exit **5**, with a discovered `GeneratedSourceTests.setUpClass` error at
`source_epoch.generate`'s warning comparator. The nested comparator exits **1**:

```text
GNATdoc warning delta: 4 added, 0 removed
added: ams-mel-ir-health_status-metadata.ads: AMS.MEL.IR.Health_Status.Metadata.Finalize: procedure Finalize (Stream : in out NUC_Stream);: warning: entity `Finalize` is not documented
added: ams-mel-ir-health_status-metadata.ads: AMS.MEL.IR.Health_Status.Metadata.Finalize: procedure Finalize (Stream : in out NUC_Stream);: warning: parameter `Stream` is not documented
added: ams-mel-ir-image-metadata.ads: AMS.MEL.IR.Image.Metadata.Finalize: procedure Finalize (Stream : in out NUC_Stream);: warning: entity `Finalize` is not documented
added: ams-mel-ir-image-metadata.ads: AMS.MEL.IR.Image.Metadata.Finalize: procedure Finalize (Stream : in out NUC_Stream);: warning: parameter `Stream` is not documented
```

The Health diagnostics both locate `347:14`; the Image diagnostics both locate
`249:14`. The exact GNATdoc messages are the entity and parameter messages above.
No diagnostics are unmapped or ambiguous. The baseline contains 73 identities;
the unchanged source and synthetic source each produce 77.

The initial synthetic fixture identities were:

- Failed-main parent: commit `70ec8ce0effcb7cbee7639f1cf05a6da1939defb`,
  tree `c4d9832bcf5be66f3ee74b63db2a696326bea334`.
- Current-main parent: commit `81e5bab33910b7199db870ae53b8e92922e378e0`,
  tree `33eaa7eaf06d8ee8225e534dd8f36dd6659150f6`.

All comparisons retain `--fail-on-removed` and use the sources belonging to
each generated log:

| Comparison | Added | Removed | Exit |
| --- | ---: | ---: | ---: |
| A: original baseline / unchanged failed main | 4 above | 0 | 1 |
| B: original baseline / failed-main synthetic addition | Same 4 | 0 | 1 |
| C: unchanged failed main / synthetic addition | 0 | 0 | 0 |
| A: original baseline / unchanged current main | Same 4 | 0 | 1 |
| B: original baseline / current-main synthetic addition | Same 4 | 0 | 1 |
| C: unchanged current main / synthetic addition | 0 | 0 | 0 |
| Original baseline / corrected source | 0 | 0 | 0 |
| Original baseline / corrected synthetic addition | 0 | 0 | 0 |

Case A is established: the newly added private `Finalize (NUC_Stream)`
declarations lack attached documentation. GNATdoc reports them even in this
pinned public-generation invocation. Existing `Metadata_Stream` finalizers
are distinct, already-baselined identities and remain unchanged. The documented
`Qualification_Observation_Count` insertion causes no warning identity change.
`AMS.MEL.IR.NUC` produces no GNATdoc warnings; `NUC_Conversion` is a private
library unit excluded by the existing header-derived inventory, not by a new
filename whitelist.

Each affected declaration now has an immediately trailing description of its
non-raising close fallback and an `@param Stream` description, matching the
existing bodies. All four added identities disappear; the original 73 remain.

## Negative controls and qualification limits

Actual comparator CLI controls on the corrected real GNATdoc output:

| Control | Actual exit / result |
| --- | --- |
| Unexpected additional warning occurrence | 1; one added identity |
| Removed baseline warning with `--fail-on-removed` | 1; one removed identity |
| Malformed warning diagnostic | 1; unrecognized diagnostic |
| Unmapped source location | 1; no declaration at line 9999 |
| Ambiguous declarations | 1; ambiguous or unmapped declaration |
| Genuine undocumented `Undocumented_Count` source addition | Comparator 1; generation raises `ValueError` for that new identity |

The external control driver's first removed-warning attempt accidentally removed
an informational line and returned 0 with no delta. That unsuccessful control
attempt is retained. The corrected control removes an actual warning and
returns 1; no repository test was changed.

The existing 42 comparator regression tests pass with `-W error` (exit 0),
including overload/scope identity, malformed/ambiguous input, and strict removed
warning controls. The 18 source-policy tests and 64 epoch/aggregation boundary
tests pass with `-W error` (exit 0). Their aggregate fixtures are **not** a real
six-shard historical execution.

After the warning correction the focused source-epoch suite still exits **5**
in `setUpClass`, now at the independent coverage ratchet:

```text
coverage ratchet: 17 new gaps; 0 fixed/stale gaps; baseline updates require explicit reviewed edits
```

The exact 17 identities are recorded externally in
`corrected/coverage-added-gaps.json` and `coverage-dependency.json`. Measuring
unchanged current-main output proves these gaps predate this correction and
are identical before/after. Existing prose includes underscores; for example
the checker normalizes source `The uint8_t alternative.` to
`The uint8t alternative.` while rendered prose retains `uint8_t`.
This is evidence for a separate coverage-evidence investigation, not permission
to expand this warning-only PR into parser policy changes or accept new gaps.

The genuine source-derived anchor/catalog positive assertion and negative
source/epoch identity, pristine/publication byte mutation, and forged-consistent
hash independent-regeneration tests remain unchanged but **cannot execute**
past that setup error. They are not reported as passed. The undocumented-source
negative was independently generated and rejected by its intended comparator.

Full local preparation, fresh historical qualification and real six-shard
aggregate are deferred while the focused gate is red. The original downloaded
logs retain historical 142-script and 437-enhanced test passes for the original
failed run only; these are not exact-head qualification of this correction.
Hosted Documentation runs only after a push to main. A successful PR Bootstrap
check cannot establish Documentation acceptance. Do not merge or deploy on the
strength of this warning-only result.

## Repository qualification

Commands use the isolated pinned environment described above; native/Ada/Alire
builds run serially in the corrective worktree, never as competing builds.
`make format-ada` ran before compilation and changed no unrelated source.

| Command | Exit / result |
| --- | --- |
| `make format-ada` | 0 |
| `python -W error -m unittest discover -s scripts -p test_compare_gnatdoc_warnings.py -v` | 0; 42 tests |
| `make test-policy` | 0; all six constituent suites pass |
| `python -W error docs/tests/pol002_source_tests.py -v --report ...` | 0; 18 tests, complete execution report |
| `python -W error docs/tests/pol002_epoch_tests.py -v --report ...` | 0; 64 tests, complete execution report |
| `make check-ir-api-coverage` | 0; audit and 24 tests |
| `make check-ada-format` | 0 |
| `python3 scripts/check_doc_source_integrity.py` | 0 |
| `git diff --check` | 0 |
| `make test-ada` | 0; provider-free smoke/contract tests, including NUC |
| `alr -n -C ada build` | 0 |
| `alr -n -C ada/tests run` | 0; provider-free smoke/contract tests |

The unchanged warning baseline SHA-256 is
`1e68ecc1a2808a571499c879580458ae9dfb7483974fd796b1aab3c47809eea2`.
Per-command logs and exit files are retained in the external evidence directory.
GPR's existing externally-built C project emits the informational warning that
it has no C sources during formatting; no first-party compiled-source warning
was suppressed or introduced.