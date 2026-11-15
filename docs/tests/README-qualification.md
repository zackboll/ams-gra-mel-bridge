# Documentation regression qualification

The explicit source and HTTP suites require the pinned documentation toolchain:
GNAT 16.1.0, GPRbuild 26.0.0 (2026-04-15), GNATdoc 26.0.0 (20260415),
and the qualified current enhancer interpreter. They are not provider-free
Bootstrap tests. Use absolute paths for the environment variables below.

## Historical versus current authority

`documentation_fixtures.py` authenticates commit
`740e15de2b8e1372607173440f479bad332315d4`, tree
`41ecd4f3c84c1bc9057bd9c3ff2bb9d8cccb3429`: the last five-job workflow
before DOC-CI-SIMPLIFY-001. Its unchanged `ci001r3_workflow.py` qualifies
the historical preparation, aggregate, deployment and read-only live contracts.
The current three-job workflow is separately checked by `docs_workflow.check`;
security mutations must fail that current contract. Neither production workflow
nor either contract implementation is rewritten by these tests.

The HTTP artifact suite creates a disposable clone of that frozen source and
runs its original `scripts/generate_docs_baseline.sh`. Its original consumer
then qualifies all pristine/native/source fingerprints and reconstructs the
enhanced artifact before any HTTP request. The mutation cases run in a separate
interpreter with frozen consumer modules; they are test code, not substituted
historical source. No repository-root `_site`, downloaded artifact identity,
current-source substitution, skipped case, or mocked authority is used.

The old consumer deliberately requires its old enhancer, not the current 0.1.5
release. Prepare its environment once outside the working tree (with a fresh
destination), using its authenticated installer:

```sh
# Run from the repository root; paths below are illustrative absolute paths.
python3 - /absolute/path/to/frozen-source <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'docs/tests')
import documentation_fixtures
documentation_fixtures.checkout(Path(sys.argv[1]))
PY
sh /absolute/path/to/frozen-source/docs/tools/install_enhancer.sh \
  /absolute/path/to/historical-enhancer
```

The installer verifies the archived implementation digest; the frozen consumer
also verifies installed package/version/asset identities. Supplying an arbitrary
interpreter cannot authorize different enhancer bytes. Keep the qualified GNAT
and GPRbuild on `PATH` and set:

```sh
export GNATDOC_PREFIX=/absolute/path/to/gnatdoc-26.0.0
export HISTORICAL_DOC_PYTHON=/absolute/path/to/historical-enhancer/bin/python
export DOC_PYTHON=/absolute/path/to/current-enhancer/bin/python
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests \
  -p source_epoch_regression_tests.py -v
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests \
  -p test_live_docs_site.py -v
"$DOC_PYTHON" -W error -m unittest discover -s docs/tests \
  -p test_documentation_ci.py -v
make test-policy
python3 scripts/check_doc_source_integrity.py
python3 scripts/check_final_newlines.py
git diff --check
```

The HTTP suite includes a checked child campaign: any child setup/test failure
fails the parent suite. Child case names and counts remain visible in its log.
Missing historical prerequisites fail explicitly rather than skipping tests.
All fixtures are fresh temporary directories, verified against their Git tree
before and after use, and removed by test cleanup.

Current-source qualification independently derives the `Operational_Task_Kind`
line from committed source and checks package, declaration, native anchor and
linked `Task_Kind` target. Negative mutations cover stale lines, wrong owners,
source paths, anchors, identities and destinations, while the existing resealed
artifact and independent-regeneration negatives remain in force.

These checks do not authorize a historical or current artifact for deployment.
PR #221 must remain unchanged during this repair; only after independent review
and merge should it be updated onto corrected main and fully requalified.
