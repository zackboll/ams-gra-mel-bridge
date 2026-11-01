"""Exact PR #140 admission through normal checks, without feature authorization."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MAIN = 'b4a550adf222b99a46b3218d00c4b080ec314a25'
FEATURE = 'e738a001e620b85abf2a38cb32b1ffad24713c4c'
FEATURE_PATHS = ('python/README.md', 'python/ams_mel/__init__.py', 'python/tests/test_rf_admin.py')


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.STDOUT)


class ExactFeatureTests(unittest.TestCase):
    def test_exact_pr140_passes_normal_source_checks(self):
        with tempfile.TemporaryDirectory(prefix='r1-pr140-') as temp:
            root = Path(temp) / 'source'
            subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(root)], check=True)
            git(root, 'checkout', '-q', '--detach', MAIN)
            git(root, 'config', 'user.name', 'Ordinary Feature Test')
            git(root, 'config', 'user.email', 'fixture@example.invalid')
            self.assertEqual(git(root, 'diff', '--name-only', MAIN, FEATURE).decode().splitlines(), list(FEATURE_PATHS))
            # Exercise today's checkers, not a historical checkout's obsolete policy.
            # There is no trusted evaluator-byte match or source approval commit.
            for name in ('docs/tools/feature_source.py', 'scripts/check_doc_source_integrity.py',
                         'docs/tools/ci001r3_workflow.py', 'docs/tools/epoch_ci.py',
                         'docs/tools/docs_workflow.py', '.github/workflows/documentation.yml',
                         '.github/workflows/documentation-qualification.yml'):
                (root / name).write_bytes((ROOT / name).read_bytes())
            hashes = {}
            for name in FEATURE_PATHS:
                content = git(root, 'show', FEATURE + ':' + name)
                (root / name).write_bytes(content)
                hashes[name] = hashlib.sha256(content).hexdigest()
            git(root, 'add', '-A')
            git(root, 'commit', '-qm', 'ordinary PR140 feature fixture with current checks')
            git(root, 'update-ref', 'refs/remotes/origin/main', MAIN)
            statuses = []
            for name in ('scripts/check_doc_source_integrity.py', 'docs/tools/feature_source.py'):
                result = subprocess.run([sys.executable, name], cwd=root, capture_output=True, text=True)
                statuses.append(result.returncode)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for name in FEATURE_PATHS:
                self.assertEqual((root / name).read_bytes(), git(root, 'show', FEATURE + ':' + name))
            self.assertEqual(git(root, 'status', '--porcelain'), b'')
            evidence = {'feature_head': FEATURE, 'feature_sha256': hashes, 'validator_exit_codes': statuses,
                        'candidate': git(root, 'rev-parse', 'HEAD').decode().strip(),
                        'tree': git(root, 'rev-parse', 'HEAD^{tree}').decode().strip()}
            destination = os.environ.get('PY_POL_EXACT_FEATURE_EVIDENCE')
            if destination:
                Path(destination).write_text(json.dumps(evidence, indent=2) + '\n')
            print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    unittest.main()
