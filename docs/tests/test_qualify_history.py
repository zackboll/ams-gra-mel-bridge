"""Fast historical filesystem bookkeeping tests; no documentation generation."""

import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'docs/tools'))
import qualify_history as history


class HistoricalEvidenceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='historical-bookkeeping-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name) / 'source'
        self.root.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Historical Bookkeeping Test')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.write('.gitignore', '_site/\n__pycache__/\n')
        self.write('docs/original.py', 'value = 1\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'original fixture')
        self.original = self.git('rev-parse', 'HEAD')
        self.original_tree = self.git('rev-parse', 'HEAD^{tree}')
        self.write('docs/original.py', 'value = 2\n')
        self.git('commit', '-qam', 'corrected replay fixture')
        self.replay = self.git('rev-parse', 'HEAD')
        self.replay_tree = self.git('rev-parse', 'HEAD^{tree}')
        self.git('checkout', '-q', '--detach', self.original)
        for owner, values in ((history.epoch_ci, {'HISTORY': self.original, 'HISTORY_TREE': self.original_tree}),
                              (history.replay, {'COMMIT': self.replay, 'TREE': self.replay_tree})):
            for key, value in values.items():
                patch = mock.patch.object(owner, key, value)
                patch.start()
                self.addCleanup(patch.stop)
        self.inventory = {'schema': 1, 'groups': {'scripts': ['test_fixture.Case.test_one']},
                          'shards': {}, 'test_sources': {}}
        for key in ('ORIGINAL_INVENTORY_SHA256', 'INVENTORY_SHA256'):
            patch = mock.patch.object(history.replay, key, history.epoch_ci.ci.digest(self.inventory))
            patch.start()
            self.addCleanup(patch.stop)
        self.evidence()

    def git(self, *args, input=None):
        return subprocess.check_output(['git', '-C', str(self.root), *args],
                                       input=input, text=True, stderr=subprocess.STDOUT,
                                       env={**os.environ, 'GIT_AUTHOR_DATE': '1761782400 +0000',
                                            'GIT_COMMITTER_DATE': '1761782400 +0000'}).strip()

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def evidence(self, corrected=False, successful=True):
        ci = history.epoch_ci.ci
        identity = {'source_sha': self.replay if corrected else self.original,
                    'source_tree': self.replay_tree if corrected else self.original_tree,
                    'run_id': '1', 'run_attempt': '1', 'repository': 'zackboll/ams-gra-mel-bridge',
                    'qualification_mode': 'local-not-deployable',
                    'working_tree_sha256': ci.digest({name: ci.file_sha(self.root / name)
                                                     for name in self.git('ls-files').splitlines()})}
        report = {'schema': 1, 'owner': 'scripts', 'identity': identity,
                  'artifact_name': ci.artifact_name('result-scripts', identity),
                  'inventory_sha256': ci.digest(self.inventory), 'qualified': None,
                  'successful': successful, 'tests_run': 1,
                  'events': [{'id': 'test_fixture.Case.test_one', 'event': event,
                              **({'seconds': 0.1} if event == 'stop' else {})}
                             for event in ('start', 'success', 'stop')]}
        manifest = {'schema': 1, 'identity': identity, 'inventory_sha256': ci.digest(self.inventory),
                    'scripts_sha256': ci.digest(report), 'payload': {'_site/index.html': 'a' * 64}}
        for name, value in zip(history.EVIDENCE, (self.inventory, report, manifest)):
            self.write('qualification/' + name, json.dumps(value))

    def verify(self, corrected=False, sealed=None):
        return history.verify_execution(self.root, corrected, sealed)

    def reject(self):
        with self.assertRaises((ValueError, KeyError, TypeError, OSError)):
            self.verify()

    def test_original_expected_evidence_passes_without_modification(self):
        before = {name: (self.root / 'qualification' / name).read_bytes() for name in history.EVIDENCE}
        self.assertTrue(self.git('status', '--porcelain'))  # Reproduces old false rejection.
        sealed = self.verify()
        self.assertEqual(self.verify(sealed=sealed), sealed)
        self.assertEqual(self.git('rev-parse', 'HEAD'), self.original)
        self.assertEqual(self.git('rev-parse', 'HEAD^{tree}'), self.original_tree)
        self.assertEqual(self.git('diff', 'HEAD'), '')
        self.assertEqual(before, {name: (self.root / 'qualification' / name).read_bytes()
                                  for name in history.EVIDENCE})

    def test_corrected_replay_keeps_distinct_commit_tree_and_evidence(self):
        self.git('checkout', '-q', '--detach', self.replay)
        self.evidence(corrected=True)
        self.verify(corrected=True)
        self.assertEqual(self.git('rev-parse', 'HEAD'), self.replay)
        self.assertEqual(self.git('rev-parse', 'HEAD^{tree}'), self.replay_tree)
        self.reject()

    def test_modified_tracked_source_rejected(self):
        self.write('docs/original.py', 'unauthorized = True\n')
        self.reject()

    def test_staged_source_even_if_worktree_restored_rejected(self):
        self.write('docs/original.py', 'staged = True\n')
        self.git('add', '.')
        self.write('docs/original.py', 'value = 1\n')
        self.reject()

    def test_deletion_rejected(self):
        (self.root / 'docs/original.py').unlink()
        self.reject()

    def test_mode_change_rejected_even_when_git_ignores_filemode(self):
        self.git('config', 'core.filemode', 'false')
        (self.root / 'docs/original.py').chmod(0o755)
        self.reject()

    def test_unmerged_index_rejected(self):
        oid = self.git('rev-parse', 'HEAD:docs/original.py')
        self.git('update-index', '--index-info', input=(
            '0 ' + '0' * 40 + '\tdocs/original.py\n' +
            f'100644 {oid} 1\tdocs/original.py\n100644 {oid} 2\tdocs/original.py\n'))
        self.reject()

    def test_untracked_python_source_rejected(self):
        self.write('docs/unexpected.py', 'print("unqualified")\n')
        self.reject()

    def test_unexpected_qualification_file_rejected(self):
        self.write('qualification/extra.json', '{}')
        self.reject()

    def test_ignored_unexpected_qualification_file_rejected(self):
        self.write('qualification/extra.pyc', 'unexpected')
        self.git('config', 'core.excludesFile', str(self.write('../ignore', '*.pyc\n')))
        self.reject()

    def test_empty_unexpected_directory_rejected(self):
        (self.root / 'unexpected').mkdir()
        self.reject()

    def test_nested_qualification_directory_rejected(self):
        (self.root / 'qualification/extra').mkdir()
        self.reject()

    def test_evidence_symlink_rejected(self):
        path = self.root / 'qualification/scripts.json'
        path.unlink()
        path.symlink_to('inventory.json')
        self.reject()

    def test_qualification_directory_symlink_rejected(self):
        path = self.root / 'qualification'
        path.rename(self.root.parent / 'saved')
        path.symlink_to(self.root.parent / 'saved', target_is_directory=True)
        self.reject()

    def test_nonregular_evidence_rejected_without_reading_fifo(self):
        path = self.root / 'qualification/scripts.json'
        path.unlink()
        os.mkfifo(path)
        self.reject()

    def test_wrong_commit_with_same_tree_rejected(self):
        other = self.git('commit-tree', self.original_tree, '-p', self.original, '-m', 'wrong identity')
        self.git('checkout', '-q', '--detach', other)
        self.reject()

    def test_wrong_expected_tree_rejected(self):
        with mock.patch.object(history.epoch_ci, 'HISTORY_TREE', self.replay_tree):
            self.reject()

    def test_missing_or_malformed_each_evidence_file_rejected(self):
        for name in history.EVIDENCE:
            path = self.root / 'qualification' / name
            before = path.read_bytes()
            for data in (None, b'{', b'[]', b'{}'):
                with self.subTest(name=name, data=data):
                    if data is None:
                        path.unlink()
                    else:
                        path.write_bytes(data)
                    self.reject()
                    path.write_bytes(before)

    def test_wrong_source_run_and_digest_rejected(self):
        path = self.root / 'qualification/manifest.json'
        original = json.loads(path.read_text())
        for field, value in (('source_sha', self.replay), ('source_tree', self.replay_tree),
                             ('run_id', '2'), ('run_attempt', '2'), ('qualification_mode', 'hosted')):
            with self.subTest(field=field):
                changed = copy.deepcopy(original)
                changed['identity'][field] = value
                path.write_text(json.dumps(changed))
                self.reject()
        path.write_text(json.dumps({**original, 'scripts_sha256': '0' * 64}))
        self.reject()

    def test_failed_script_report_cannot_become_success(self):
        self.evidence(successful=False)
        self.reject()

    def test_evidence_changes_after_sealing_rejected(self):
        sealed = self.verify()
        path = self.root / 'qualification/manifest.json'
        path.write_text(path.read_text() + '\n')
        with self.assertRaisesRegex(ValueError, 'sealed historical evidence changed'):
            self.verify(sealed=sealed)

    def test_existing_ignored_generated_sites_and_bytecode_stay_allowed(self):
        self.write('_site/index.html', 'generated site')
        self.write('docs/__pycache__/original.pyc', 'generated bytecode')
        self.verify()

    def test_ignored_arbitrary_source_is_not_runner_output(self):
        self.write('_site/unexpected.py', 'value = "unqualified"\n')
        self.reject()

    def test_empty_ignored_unexpected_directory_rejected(self):
        (self.root / '_site/unexpected').mkdir(parents=True)
        self.reject()

    def test_staged_evidence_rejected(self):
        self.git('add', 'qualification')
        self.reject()

    def test_duplicate_json_key_rejected(self):
        path = self.root / 'qualification/manifest.json'
        path.write_text(path.read_text().replace('{', '{"schema":1,', 1))
        self.reject()

    def exercise_runner(self, failed_owner=None):
        output = self.root.parent / 'run'
        real_run = subprocess.run

        def checkout(destination):
            subprocess.check_output(['git', 'clone', '-q', '--shared', str(self.root), str(destination)],
                                    stderr=subprocess.STDOUT)

        def child(command, **kwargs):
            if command[0] == 'git':
                return real_run(command, **kwargs)
            if 'seal' in command:
                shutil.copytree(self.root / 'qualification', Path(kwargs['cwd']) / 'qualification')
                Path(command[command.index('--output') + 1]).write_bytes(b'deterministic bundle fixture')
            status = 7 if failed_owner and '--owner' in command and command[
                command.index('--owner') + 1] == failed_owner else 0
            return subprocess.CompletedProcess(command, status)

        with mock.patch.object(history.epoch_ci, 'historical_checkout', side_effect=checkout), \
             mock.patch.object(history.subprocess, 'run', side_effect=child):
            result = history.qualify(output, self.root.parent / 'unused-toolchain')
        return result, json.loads((output / 'results.json').read_text())

    def test_runner_final_check_accepts_legitimate_generated_evidence(self):
        result, statuses = self.exercise_runner()
        self.assertEqual(result, 0)
        self.assertTrue(all(item['exit'] == 0 for item in statuses))

    def test_runner_keeps_actual_failed_shard_status_and_attempts_other_shards(self):
        result, statuses = self.exercise_runner('live')
        self.assertEqual(result, 1)
        self.assertEqual(next(item['exit'] for item in statuses if item['label'] == 'live'), 7)
        self.assertTrue(set(history.epoch_ci.ci.SHARDS) <= {item['label'] for item in statuses})


if __name__ == '__main__':
    unittest.main()
