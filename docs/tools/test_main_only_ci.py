"""DOC-CI-005 main-only Documentation transition controls; proposal, not approval.

Run explicitly: python3 docs/tools/test_main_only_ci.py
"""

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import check_doc_source_integrity as integrity
import epoch_ci as e
import feature_source as s

PREDECESSOR = 'd5243dd924c4a81a548e38d97533e29f9971deea'
PREDECESSOR_TREE = 'ea4acd74aa14ea67bdc5b618d763a70d7e6dac5d'
DOCS = '.github/workflows/documentation.yml'
SOURCE_POLICY = '.github/workflows/source-policy.yml'
CHECKER = 'scripts/check_doc_source_integrity.py'
RUNNER = 'docs/tools/epoch_ci.py'
DOCUMENTATION_JOBS = ['prepare-documentation', 'enhanced-tests', 'build', 'deploy', 'verify-live']


def triggers(text):
    # PyYAML (YAML 1.1) reads the bare `on` key as boolean True.
    return yaml.safe_load(text)[True]


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.STDOUT).decode().strip()


class TriggerTests(unittest.TestCase):
    def test_documentation_runs_only_after_push_to_main(self):
        self.assertEqual(triggers((ROOT / DOCS).read_text()), {'push': {'branches': ['main']}})

    def test_bootstrap_keeps_pull_request_coverage(self):
        on = triggers((ROOT / '.github/workflows/ci.yml').read_text())
        self.assertIn('pull_request', on)
        self.assertEqual(on['push'], {'branches': ['main']})

    def test_source_policy_is_pull_request_only_and_read_only(self):
        text = (ROOT / SOURCE_POLICY).read_text()
        value = yaml.safe_load(text)
        self.assertEqual(value[True], {'pull_request': None})
        self.assertEqual(value['permissions'], {'contents': 'read'})
        self.assertEqual(list(value['jobs']), ['pr-source-policy'])
        job = value['jobs']['pr-source-policy']
        self.assertNotIn('permissions', job)
        for forbidden in ('secrets.', 'pull_request_target', ': write', 'continue-on-error',
                          'upload-pages-artifact', 'deploy-pages', 'gnatdoc', 'generate_docs_baseline',
                          'documentation_ci.py', ' prepare ', ' run --owner'):
            self.assertNotIn(forbidden, text)
        checkout, classify = job['steps']
        self.assertEqual(checkout['uses'], 'actions/checkout@fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09')
        self.assertIs(checkout['with']['persist-credentials'], False)
        self.assertEqual(checkout['with']['ref'], '${{ github.event.pull_request.head.sha }}')
        run = classify['run']
        self.assertIn('git archive --format=tar "$BASE_SHA" docs/tools scripts', run)
        self.assertLess(run.index('check "$trusted" trusted'), run.index('check "$PWD" candidate'))
        self.assertIn('set -euo pipefail', run)
        self.assertIn('NOT approval', run)

    def test_full_campaign_jobs_and_publication_gates_unchanged(self):
        new = (ROOT / DOCS).read_text()
        old = s.blob(ROOT, PREDECESSOR, DOCS).decode()
        removed = 'on:\n  pull_request:\n  push:\n    branches: [main]\n  workflow_dispatch:\n'
        self.assertEqual(old.count(removed), 1)
        self.assertEqual(new, old.replace(removed, 'on:\n  push:\n    branches: [main]\n'))
        self.assertEqual(list(yaml.safe_load(new)['jobs']), DOCUMENTATION_JOBS)
        from ci001r3_workflow import live_contract, preparation_contract, deploy_contract
        for contract in (live_contract, preparation_contract, deploy_contract):
            contract(ROOT)


class RegistryTests(unittest.TestCase):
    def test_exact_predecessor_and_successor_identities(self):
        self.assertEqual(git(ROOT, 'rev-parse', PREDECESSOR + '^{tree}'), PREDECESSOR_TREE)
        self.assertEqual(integrity.DIAGNOSTIC_WORKFLOWS[1],
                         hashlib.sha256(s.blob(ROOT, PREDECESSOR, DOCS)).hexdigest())
        self.assertEqual(integrity.MAIN_ONLY_WORKFLOWS,
                         (integrity.DIAGNOSTIC_WORKFLOWS[0],
                          hashlib.sha256((ROOT / DOCS).read_bytes()).hexdigest()))
        self.assertEqual(integrity.SOURCE_POLICY_WORKFLOW,
                         (SOURCE_POLICY, hashlib.sha256((ROOT / SOURCE_POLICY).read_bytes()).hexdigest()))
        rows = {name: (old, new) for name, old, new in e.MAIN_ONLY_SOURCE}
        self.assertEqual(set(rows), {DOCS, SOURCE_POLICY, CHECKER, RUNNER, 'docs/tools/test_main_only_ci.py'})
        for name, (old, new) in rows.items():
            if old is None:
                self.assertNotIn(name, s.tree(ROOT, PREDECESSOR))
            else:
                self.assertEqual(hashlib.sha256(s.blob(ROOT, PREDECESSOR, name)).hexdigest(), old)
            if new is not None:
                self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), new)
        self.assertIsNone(rows[RUNNER][1])  # Executing bytes are compared directly.

    def test_historical_identities_preserved(self):
        self.assertEqual(integrity.ACTIONS_WORKFLOWS[1][1:], (
            '979931734ae82a2a98b8684231f06ce17f202c4115eafdf439136c238724a847',
            'cb7ea4aa28d20bec2976af8d430fbff84d00bdc69f19b1dd13885f41022cc7d3'))
        self.assertEqual(integrity.DIAGNOSTIC_WORKFLOWS, (
            'de38dd1e155bd1cf7d256995f6c8bded61e2fe000f1e733d3dad8a8f255fe235',
            '1b4be1e4cb526d58021a8aac25154faf8f8ee3e2172a98931f359d720983c711'))
        self.assertEqual(len(integrity.ACTIONS_HISTORICAL_PAIRS), 2)
        self.assertEqual(e.DIAGNOSTIC_SOURCE[1][1], integrity.DIAGNOSTIC_WORKFLOWS[1])
        self.assertEqual(e.ACTIONS_SOURCE[1][1:], integrity.ACTIONS_WORKFLOWS[1][1:])


class CheckerStateTests(unittest.TestCase):
    """Complete pair transitions in both index and working-tree views."""

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='doc-ci-005-state-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        git(self.root, 'init', '-q')
        self.old = {name: s.blob(ROOT, PREDECESSOR, name) for name, _, _ in integrity.ACTIONS_WORKFLOWS}
        self.new = {name: (ROOT / name).read_bytes() for name in [*self.old, SOURCE_POLICY]}
        self.stage(self.old)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT)

    def stage(self, contents):
        for name, content in contents.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.git('add', '.')

    def state(self):
        current = set(self.git('ls-files', '-z').decode().split('\0')) - {''}
        return integrity.actions_workflow_state(self.root, self.git, current)

    def test_predecessor_and_complete_successor(self):
        self.assertEqual(self.state(), 'diagnostics_proposed_pending_independent_review')
        self.stage(self.new)
        self.assertEqual(self.state(), 'main_only_proposed_pending_independent_review')

    def test_partial_transitions_reject(self):
        self.stage({DOCS: self.new[DOCS]})
        with self.assertRaisesRegex(ValueError, 'change together'):
            self.state()
        self.stage(self.old)
        self.stage({SOURCE_POLICY: self.new[SOURCE_POLICY]})
        with self.assertRaisesRegex(ValueError, 'change together'):
            self.state()

    def test_unlisted_bytes_in_each_view_reject(self):
        mutations = {
            'pr trigger': (DOCS, b'on:\n  push:\n', b'on:\n  pull_request:\n  push:\n'),
            'dispatch': (DOCS, b'    branches: [main]\n', b'    branches: [main]\n  workflow_dispatch:\n'),
            'write token': (DOCS, b'permissions:\n  contents: read\n', b'permissions:\n  contents: write\n'),
            'deploy bypass': (DOCS, b"if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'\n"
                              b"    needs: [prepare-documentation, build]\n",
                              b'if: always()\n    needs: [prepare-documentation, build]\n'),
            'unpinned pages': (DOCS, b'deploy-pages@cd2ce8fcbc39b97be8ca5fce6e763baed58fa128 # v5.0.0',
                               b'deploy-pages@v4'),
            'source-policy write': (SOURCE_POLICY, b'  contents: read\n', b'  contents: write\n'),
            'source-policy action': (SOURCE_POLICY, b'actions/checkout@fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09',
                                     b'evil/checkout@v1'),
            'source-policy candidate-first': (SOURCE_POLICY, b'check "$trusted" trusted', b'check "$PWD" trusted'),
        }
        for label, (name, old, new) in mutations.items():
            for indexed in (False, True):
                with self.subTest(mutation=label, indexed=indexed):
                    self.stage(self.new)
                    self.assertEqual(self.new[name].count(old), 1, label)
                    path = self.root / name
                    path.write_bytes(self.new[name].replace(old, new))
                    if indexed:
                        self.git('add', name)
                        path.write_bytes(self.new[name])
                    with self.assertRaisesRegex(ValueError, 'unlisted|mismatch'):
                        self.state()

    def test_nonregular_untracked_and_unmerged_source_policy_reject(self):
        self.stage(self.new)
        path = self.root / SOURCE_POLICY
        path.chmod(0o755)
        with self.assertRaisesRegex(ValueError, 'not regular'):
            self.state()
        path.chmod(0o644)
        path.unlink()
        path.symlink_to(ROOT / SOURCE_POLICY)
        with self.assertRaisesRegex(ValueError, 'not regular'):
            self.state()
        path.unlink()
        path.write_bytes(self.new[SOURCE_POLICY])
        oid = self.git('hash-object', '-w', str(path)).decode().strip()
        self.git('update-index', '--force-remove', SOURCE_POLICY)
        subprocess.run(['git', '-C', str(self.root), 'update-index', '--index-info'], check=True,
                       input=f'100644 {oid} 1\t{SOURCE_POLICY}\n100644 {oid} 2\t{SOURCE_POLICY}\n'.encode())
        with self.assertRaisesRegex(ValueError, 'stage-0'):
            self.state()
        self.git('update-index', '--force-remove', SOURCE_POLICY)
        path.unlink()  # stage() uses `git add .`; keep the later copy untracked.
        self.stage(self.old)
        path.write_bytes(self.new[SOURCE_POLICY])
        with self.assertRaisesRegex(ValueError, 'untracked'):
            self.state()


class EpochRouteTests(unittest.TestCase):
    """Real committed predecessor/successor objects through epoch_ci.classify."""

    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='doc-ci-005-epoch-')
        cls.root = Path(cls.temp.name) / 'source'
        subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(cls.root)], check=True)
        git(cls.root, 'checkout', '-q', '--detach', PREDECESSOR)
        git(cls.root, 'config', 'user.name', 'DOC-CI-005 control')
        git(cls.root, 'config', 'user.email', 'doc-ci-005@example.invalid')
        for name, _, _ in e.MAIN_ONLY_SOURCE:
            (cls.root / name).write_bytes((ROOT / name).read_bytes())
        git(cls.root, 'add', '-A')
        git(cls.root, 'commit', '-qm', 'DOC-CI-005 successor')
        cls.head = git(cls.root, 'rev-parse', 'HEAD')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def reset(self):
        git(self.root, 'reset', '-q', '--hard', self.head)

    def mutate(self, name, old, new):
        path = self.root / name
        data = path.read_bytes()
        self.assertEqual(data.count(old), 1, name)
        path.write_bytes(data.replace(old, new))

    def commit(self):
        git(self.root, 'add', '-A')
        git(self.root, 'commit', '-qm', 'mutation')
        return git(self.root, 'rev-parse', 'HEAD')

    def classify(self, candidate=None):
        return e.classify(self.root, PREDECESSOR, candidate or git(self.root, 'rev-parse', 'HEAD'))

    def test_exact_transition_is_policy_only_and_not_approval(self):
        self.reset()
        value = self.classify(self.head)
        self.assertEqual(value['case'], 'policy-only')
        self.assertFalse(value['independent_approval'])
        self.assertFalse(value['publication_epoch_approved'])
        self.assertEqual({d['path'] for d in value['delta']}, {n for n, _, _ in e.MAIN_ONLY_SOURCE})
        self.assertEqual(e.main_only_delta(self.root, PREDECESSOR, self.head, s.tree(self.root, PREDECESSOR),
                                           s.tree(self.root, self.head)), {n for n, _, _ in e.MAIN_ONLY_SOURCE})

    def test_each_incomplete_or_unlisted_member_rejects(self):
        for name, predecessor, _ in e.MAIN_ONLY_SOURCE:
            for mutation in ('predecessor', 'unknown', 'executable'):
                with self.subTest(name=name, mutation=mutation):
                    self.reset()
                    path = self.root / name
                    if mutation == 'predecessor':
                        if predecessor is None:
                            path.unlink()
                        else:
                            path.write_bytes(s.blob(ROOT, PREDECESSOR, name))
                    elif mutation == 'unknown':
                        path.write_bytes(path.read_bytes() + b'\n# unreviewed\n')
                    else:
                        path.chmod(0o755)
                    self.commit()
                    with self.assertRaises((ValueError, AssertionError)):
                        self.classify()
        self.reset()

    def test_hitchhikers_registry_and_protected_inputs_reject(self):
        for name in ('native/src/rf_product_rx.cpp', 'docs/tools/source_epoch.py', 'docs/tools/feature_source.py',
                     '.github/workflows/ci.yml', 'docs/tools/test_actions_dependencies.py',
                     'docs/gnatdoc-coverage-baseline.txt', 'docs/tools/documentation_ci.py',
                     '.github/workflows/unreviewed.yml'):
            with self.subTest(name=name):
                self.reset()
                path = self.root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((path.read_bytes() if path.exists() else b'') + b'\n# hitchhiker\n')
                self.commit()
                with self.assertRaises((ValueError, AssertionError)):
                    self.classify()
        self.reset()

    def test_workflow_security_mutations_reject(self):
        cases = {
            'pr trigger': (DOCS, b'on:\n  push:\n', b'on:\n  pull_request:\n  push:\n'),
            'dispatch': (DOCS, b'    branches: [main]\n', b'    branches: [main]\n  workflow_dispatch:\n'),
            'write token': (DOCS, b'permissions:\n  contents: read\n', b'permissions:\n  contents: write\n'),
            'deploy bypass': (DOCS, b"if: github.ref == 'refs/heads/main' && github.event_name != 'pull_request'\n"
                              b"    needs: [prepare-documentation, build]\n",
                              b'if: always()\n    needs: [prepare-documentation, build]\n'),
            'untrusted pages action': (DOCS, b'actions/deploy-pages@cd2ce8fcbc39b97be8ca5fce6e763baed58fa128',
                                       b'evil/deploy-pages@cd2ce8fcbc39b97be8ca5fce6e763baed58fa128'),
            'source-policy write': (SOURCE_POLICY, b'  contents: read\n', b'  contents: write\n'),
            'source-policy action': (SOURCE_POLICY, b'actions/checkout@fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09',
                                     b'evil/checkout@v1'),
        }
        for label, (name, old, new) in cases.items():
            with self.subTest(mutation=label):
                self.reset()
                self.mutate(name, old, new)
                self.commit()
                with self.assertRaises((ValueError, AssertionError)):
                    self.classify()
        self.reset()

    def test_route_not_selected_from_other_targets(self):
        # From the CI-ACTIONS-001 migration main the DOC-CI-005 successor is not
        # a defined transition; it must not substitute for DOC-CI-004R1.
        migration = '219b00c75210f5985d9889e8b541da2e7b658f04'
        old, new = s.tree(self.root, migration), s.tree(self.root, self.head)
        self.assertEqual(e.main_only_delta(self.root, migration, self.head, old, new), set())
        with self.assertRaises((ValueError, AssertionError)):
            e.classify(self.root, migration, self.head)


if __name__ == '__main__':
    unittest.main(verbosity=2)
