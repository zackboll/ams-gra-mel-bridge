"""Independent CI-ACTIONS-001 workflow preservation and authorization controls."""

import hashlib
import itertools
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import check_doc_source_integrity as integrity

BASELINE = 'e4e756aafa4edaffdb11fe341bb016380bf19ca9'
IDENTITIES = (
    ('.github/workflows/ci.yml',
     'c89a50fab57f9bbb7653cf26a8b0260f72d9436c1ebde188dde4b29f257514f2',
     'de38dd1e155bd1cf7d256995f6c8bded61e2fe000f1e733d3dad8a8f255fe235'),
    ('.github/workflows/documentation.yml',
     '979931734ae82a2a98b8684231f06ce17f202c4115eafdf439136c238724a847',
     'cb7ea4aa28d20bec2976af8d430fbff84d00bdc69f19b1dd13885f41022cc7d3'),
)
PINS = {
    'checkout': ('fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09', 'v5.1.0', 'v4'),
    'upload-artifact': ('b7c566a772e6b6bfb58ed0dc250532a479d7789f', 'v6.0.0', 'v4'),
    'download-artifact': ('37930b1c2abaa49bbe596cd826c3c89aef350131', 'v7.0.0', 'v4'),
    'upload-pages-artifact': ('fc324d3547104276b827a68afc52ff2a11cc49c9', 'v5.0.0', 'v3'),
    'deploy-pages': ('cd2ce8fcbc39b97be8ca5fce6e763baed58fa128', 'v5.0.0', 'v4'),
}


def original(name):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', BASELINE + ':' + name])


def validate_workflow(name, text):
    expected = original(name).decode()
    for action, (sha, release, old) in PINS.items():
        expected = expected.replace('actions/' + action + '@' + old,
                                    'actions/' + action + '@' + sha + ' # ' + release)
    if name.endswith('/documentation.yml'):
        expected = expected.replace('          path: .doc-tools/epochs/candidate/published/\n',
                                    '          path: .doc-tools/epochs/candidate/published/\n'
                                    '          include-hidden-files: true\n')
    if text != expected:
        raise ValueError('unauthorized action migration workflow bytes')


class DependencyWorkflowTests(unittest.TestCase):
    def test_only_reviewed_dependency_and_hidden_file_compatibility_changes(self):
        for name, _, _ in IDENTITIES:
            validate_workflow(name, (ROOT / name).read_text())

    def test_all_references_full_sha_and_original_job_inventory(self):
        for name, _, _ in IDENTITIES:
            text = (ROOT / name).read_text()
            refs = re.findall(r'uses: actions/([^@\s]+)@([^\s]+)', text)
            self.assertTrue(all(action in PINS and sha == PINS[action][0] for action, sha in refs))
            jobs = re.findall(r'^  ([a-z][a-z-]*):\n', text.split('jobs:\n', 1)[1], re.M)
            expected = (['native', 'build-tree-isolation', 'ada', 'rust', 'python'] if name.endswith('/ci.yml')
                        else ['prepare-documentation', 'enhanced-tests', 'build', 'deploy', 'verify-live'])
            self.assertEqual(jobs, expected)
        native = (ROOT / IDENTITIES[0][0]).read_text()
        self.assertIn('compiler: [gcc, clang]', native)
        self.assertIn('build_type: [Debug, Release]', native)
        docs = (ROOT / IDENTITIES[1][0]).read_text()
        self.assertIn('shard: [navigation, publication, live, symbols-catalog, types, remaining]', docs)

    def test_each_unreviewed_pin_rejected(self):
        for name, _, _ in IDENTITIES:
            text = (ROOT / name).read_text()
            for action, (sha, _, _) in PINS.items():
                if sha in text:
                    with self.subTest(action=action), self.assertRaises(ValueError):
                        validate_workflow(name, text.replace(sha, '0' * 40, 1))

    def test_changed_jobs_gates_permissions_commands_and_cancellation_rejected(self):
        name = IDENTITIES[1][0]
        text = (ROOT / name).read_text()
        for old, new in [('cancel-in-progress: true', 'cancel-in-progress: false'),
                         ('if: always()', 'if: success()'), ('pages: write', 'pages: read'),
                         ('epoch_ci.py select', 'true'), ('merge-multiple: true', 'merge-multiple: false'),
                         ('artifact-ids:', 'name:'), ('needs: prepare-documentation', 'needs: build')]:
            self.assertIn(old, text)
            with self.subTest(change=old), self.assertRaises(ValueError):
                validate_workflow(name, text.replace(old, new, 1))

    def test_hidden_files_compatibility_cannot_be_omitted(self):
        name = IDENTITIES[1][0]
        text = (ROOT / name).read_text()
        for bad in [text.replace('          include-hidden-files: true\n', ''),
                    text.replace('include-hidden-files: true', 'include-hidden-files: false')]:
            with self.assertRaises(ValueError):
                validate_workflow(name, bad)


class DependencyAuthorizationTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='ci-actions-policy-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git('init', '-q')
        self.old = {name: original(name) for name, _, _ in IDENTITIES}
        self.new = {name: (ROOT / name).read_bytes() for name, _, _ in IDENTITIES}
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
        return integrity.actions_workflow_state(self.root, self.git, set(self.old))

    def test_independently_pinned_old_new_complete_pairs(self):
        self.assertEqual(integrity.ACTIONS_WORKFLOWS, IDENTITIES)
        for name, old, new in IDENTITIES:
            self.assertEqual(hashlib.sha256(self.old[name]).hexdigest(), old)
            self.assertEqual(hashlib.sha256(self.new[name]).hexdigest(), new)
        self.assertEqual(self.state(), 'predecessor')
        self.stage(self.new)
        self.assertEqual(self.state(), 'proposed_pending_independent_review')

    def test_both_partial_transition_directions_rejected(self):
        for name in self.old:
            self.stage(self.old)
            self.stage({name: self.new[name]})
            with self.assertRaisesRegex(ValueError, 'mixed or partially'):
                self.state()

    def test_both_index_working_parity_directions_rejected(self):
        for indexed, working in [(self.old, self.new), (self.new, self.old)]:
            for name in self.old:
                self.stage(indexed)
                (self.root / name).write_bytes(working[name])
                with self.assertRaisesRegex(ValueError, 'state mismatch'):
                    self.state()

    def test_unknown_bytes_in_each_workflow_and_view(self):
        for name, indexed in itertools.product(self.old, (False, True)):
            self.stage(self.new)
            path = self.root / name
            path.write_bytes(self.new[name] + b'# unauthorized\n')
            if indexed:
                self.git('add', name)
                path.write_bytes(self.new[name])
            with self.assertRaisesRegex(ValueError, 'unlisted workflow bytes'):
                self.state()

    def test_missing_symlink_executable_and_unmerged_rejected(self):
        for name in self.old:
            self.stage(self.new)
            path = self.root / name
            path.chmod(0o755)
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            self.git('add', name)
            path.chmod(0o644)
            with self.assertRaisesRegex(ValueError, 'stage-0'):
                self.state()
            self.stage(self.new)
            path.unlink()
            path.symlink_to(ROOT / name)
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            path.unlink()
            with self.assertRaisesRegex(ValueError, 'not regular'):
                self.state()
            self.stage(self.new)
            oid = self.git('hash-object', '-w', str(path)).decode().strip()
            self.git('update-index', '--force-remove', name)
            subprocess.run(['git', '-C', str(self.root), 'update-index', '--index-info'],
                           input=f'100644 {oid} 1\t{name}\n100644 {oid} 2\t{name}\n'.encode(), check=True)
            with self.assertRaisesRegex(ValueError, 'stage-0'):
                self.state()
            self.git('update-index', '--force-remove', name)

    def test_historical_pairs_and_mixed_history(self):
        names = list(self.old)
        historical = {n: subprocess.check_output(['git', '-C', str(ROOT), 'show',
                      '5c136a04ff64d980df7aaa2bd1ee49e1ed80daaf:' + n]) for n in names}
        self.stage(historical)
        self.assertEqual(self.state(), 'historical')
        self.stage({names[1]: self.new[names[1]]})
        with self.assertRaisesRegex(ValueError, 'mixed or partially'):
            self.state()


if __name__ == '__main__':
    unittest.main(verbosity=2)
