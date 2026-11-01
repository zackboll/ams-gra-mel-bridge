"""Ordinary multi-language source evolution and exact source identity."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'docs/tools'), str(ROOT / 'scripts')]
import feature_source as source
import documentation_ci as ci


class SourceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Development Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.write('README.md', 'base\n')
        self.write('python/ams_mel/existing.py', 'value = 1\n')
        self.write('docs/tests/test_existing.py', 'original tests\n')
        self.base = self.commit()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT)

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def commit(self):
        self.git('add', '-A')
        self.git('commit', '-qm', 'ordinary development')
        return self.git('rev-parse', 'HEAD').decode().strip()

    def check(self):
        return source.qualify(self.root, self.base, 'HEAD')

    def test_add_modules_tests_and_docs_without_filename_permission(self):
        paths = ('native/src/new_feature.cpp', 'native/tests/new_feature.c',
                 'ada/src/new_feature.ads', 'ada/tests/src/new_test.adb',
                 'rust/ams-mel/src/new_feature.rs', 'rust/ams-mel/tests/new_test.rs',
                 'python/ams_mel/future/package.py', 'python/tests/test_future.py',
                 'python/README.md', 'docs/new/deep/guide.md', 'scripts/new_check.py')
        for name in paths:
            self.write(name, 'new source\n')
        self.commit()
        self.assertEqual({d['path'] for d in self.check()['delta']}, set(paths))

    def test_current_test_and_policy_bytes_can_change_with_source(self):
        self.write('docs/tests/test_existing.py', 'new assertions\n')
        self.write('python/ams_mel/existing.py', 'value = 2\n')
        self.write('docs/tools/epoch_ci.py', '# tooling evolves through review too\n')
        self.commit()
        self.assertEqual(len(self.check()['delta']), 3)

    def test_delete_and_rename_are_ordinary_reviewable_deltas(self):
        (self.root / 'python/ams_mel/existing.py').rename(self.root / 'python/ams_mel/renamed.py')
        self.commit()
        delta = {row['path']: row for row in self.check()['delta']}
        self.assertIsNone(delta['python/ams_mel/existing.py']['new'])
        self.assertIsNone(delta['python/ams_mel/renamed.py']['old'])

    def test_executable_build_scripts_can_evolve(self):
        self.write('scripts/new.sh', '#!/bin/sh\nexit 0\n').chmod(0o755)
        self.commit()
        self.assertEqual(self.check()['delta'][0]['new'][0], '100755')

    def test_dirty_worktree_cannot_claim_committed_identity(self):
        self.write('python/ams_mel/existing.py', 'uncommitted\n')
        with self.assertRaisesRegex(ValueError, 'working tree differs'):
            self.check()

    def test_divergent_index_cannot_claim_committed_identity(self):
        self.write('python/ams_mel/existing.py', 'staged\n')
        self.git('add', '.')
        with self.assertRaisesRegex(ValueError, 'index differs'):
            self.check()

    def test_untracked_source_rejected_as_unqualified_input(self):
        self.write('python/ams_mel/untracked.py', 'value = 3\n')
        with self.assertRaisesRegex(ValueError, 'untracked'):
            self.check()

    def test_symlink_source_rejected(self):
        (self.root / 'python/ams_mel/link.py').symlink_to('existing.py')
        self.commit()
        with self.assertRaisesRegex(ValueError, 'nonregular'):
            self.check()

    def test_source_modes_are_bound_to_commit(self):
        (self.root / 'python/ams_mel/existing.py').chmod(0o755)
        with self.assertRaisesRegex(ValueError, 'mode differs'):
            self.check()

    def test_dynamic_test_inventory_does_not_compare_historical_bytes(self):
        first = {'scripts': ['test_current.C.test_one'], 'enhanced': ['test_new.C.test_two']}
        self.assertEqual(ci.validate_inventory(first, self.root), first)
        self.write('docs/tests/test_existing.py', 'arbitrarily changed test implementation\n')
        self.assertEqual(ci.validate_inventory(first, self.root), first)
        second = {'scripts': ['test_other.C.test_replacement'], 'enhanced': ['test_future.C.test_feature']}
        self.assertEqual(ci.validate_inventory(second, self.root), second)

    def test_duplicate_current_test_identity_rejected(self):
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            ci.validate_inventory({'scripts': ['same.C.test_one'], 'enhanced': ['same.C.test_one']})

    def test_new_test_module_assigned_without_registry_change(self):
        assigned = ci.assign(['test_future.C.test_binding'], {'remaining': []})
        self.assertEqual(assigned, {'remaining': ['test_future.C.test_binding']})


class HistoricalIsolationTests(unittest.TestCase):
    def test_original_failures_are_recorded_not_rewritten_to_success(self):
        import epoch_ci
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            # Exercise status/log recording, not a simulated historical pass.
            failure = subprocess.CompletedProcess(['original-tests'], 7)
            with mock.patch.object(epoch_ci.subprocess, 'run', return_value=failure), \
                 mock.patch.object(source, 'exact_checkout'), mock.patch.object(source, 'tree'):
                result = epoch_ci.execute_original_tests(ROOT, root, {})
            self.assertEqual(result['commit'], source.HISTORICAL_MAIN)
            self.assertEqual({v['exit'] for v in result['results'].values()}, {7})
            self.assertEqual(set(result['results']), {'scripts', 'enhanced'})

    def test_original_tests_execute_only_in_immutable_checkout(self):
        with tempfile.TemporaryDirectory(prefix='r1-original-tests-') as temp:
            checkout = Path(temp) / 'history'
            subprocess.run(['git', 'clone', '-q', '--shared', '--no-checkout', str(ROOT), str(checkout)], check=True)
            subprocess.run(['git', '-C', str(checkout), 'checkout', '-q', '--detach', source.HISTORICAL_MAIN], check=True)
            self.assertEqual(source.git(checkout, 'rev-parse', 'HEAD^{tree}').decode().strip(), source.HISTORICAL_TREE)
            result = subprocess.run([sys.executable, '-W', 'error', '-m', 'unittest', 'discover',
                                     '-s', 'scripts', '-p', 'test_doc_source_integrity.py', '-v'],
                                    cwd=checkout, capture_output=True, text=True,
                                    env={k: v for k, v in __import__('os').environ.items() if k != 'PYTHONPATH'}, timeout=180)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('Ran 60 tests', result.stderr)
            self.assertEqual(source.git(checkout, 'status', '--porcelain'), b'')
            self.assertEqual(source.revision(checkout, 'HEAD'), source.HISTORICAL_MAIN)


class HistoricalReplayConstructionTests(unittest.TestCase):
    def setUp(self):
        import ci001r4_replay as replay
        self.replay = replay
        self.temp = tempfile.TemporaryDirectory(prefix='ci001r4-source-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'source'
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(ROOT), str(self.root)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.git('checkout', '--detach', source.HISTORICAL_MAIN)
        self.git('config', 'user.name', 'CI-001R3-R4 control')
        self.git('config', 'user.email', 'ci001r4@example.invalid')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.STDOUT).decode().strip()

    def blob(self, revision, name):
        return source.blob(ROOT, revision, name)

    def test_construction_is_deterministic_and_bound_to_reviewed_sources(self):
        first = self.replay.build_objects(self.root)
        self.assertEqual(first, (self.replay.TREE, self.replay.COMMIT))
        self.assertEqual(self.replay.build_objects(self.root), first)
        self.assertEqual(self.git('rev-parse', self.replay.COMMIT + '^'), source.HISTORICAL_MAIN)
        names = self.git('diff-tree', '-r', '--name-only', '--no-commit-id', source.HISTORICAL_MAIN, self.replay.COMMIT).split()
        self.assertEqual(sorted(names), sorted(self.replay.SOURCES))
        self.assertEqual(self.git('rev-parse', 'HEAD'), source.HISTORICAL_MAIN)  # Original never rewritten.
        self.assertEqual(self.git('rev-parse', source.HISTORICAL_MAIN + '^{tree}'), source.HISTORICAL_TREE)

    def test_replay_changes_only_reviewed_helper_and_admission(self):
        sources = self.replay.successor_sources(self.blob)
        original = self.blob(source.HISTORICAL_MAIN, self.replay.LIVE)
        self.assertEqual(self.replay.restore_helper(sources[self.replay.LIVE], original), original)
        self.assertEqual(sources[self.replay.LIVE], self.blob(self.replay.REVIEWED, self.replay.LIVE))
        qualifier = sources[self.replay.QUALIFIER]
        self.assertIn(b'def live_fixture_source(content):', qualifier)
        self.assertNotIn(b'def live_fixture_source(content):', self.blob(source.HISTORICAL_MAIN, self.replay.QUALIFIER))

    def test_missing_or_altered_replay_sources_reject(self):
        for revision, name in ((source.HISTORICAL_MAIN, self.replay.LIVE), (source.HISTORICAL_MAIN, self.replay.QUALIFIER),
                               (self.replay.REVIEWED, self.replay.LIVE), (self.replay.REVIEWED, self.replay.QUALIFIER)):
            for mutation in ('missing', 'altered'):
                with self.subTest(revision=revision, name=name, mutation=mutation):
                    def blob(rev, path):
                        if (rev, path) == (revision, name):
                            if mutation == 'missing':
                                raise subprocess.CalledProcessError(128, ['git', 'show'])
                            return self.blob(rev, path) + b'\n# altered\n'
                        return self.blob(rev, path)
                    with self.assertRaisesRegex(ValueError, 'historical replay'):
                        self.replay.successor_sources(blob)

    def test_other_helper_test_body_or_admission_changes_reject(self):
        reviewed = self.blob(self.replay.REVIEWED, self.replay.LIVE)
        qualifier = self.blob(self.replay.REVIEWED, self.replay.QUALIFIER)
        # (mutation, outside the reviewed api_page() helper?) Every mutation is
        # rejected by exact replay bytes; outside-helper edits also by restore.
        live_cases = [(reviewed.replace(b'self.assertEqual(count, 1)', b'self.assertEqual(count, 0)', 1), True),
                      (reviewed.replace(b'def reject(self, guard):', b'def reject(self, other):', 1), True),
                      (reviewed.replace(b'required[0]', b'required[-1]', 1), False),
                      (reviewed.replace(b'    def test_wrong_native_anchor(self):', b'    def test_renamed(self):', 1), True),
                      (reviewed + b'\nclass Extra(unittest.TestCase):\n    def test_extra(self): pass\n', True)]
        admission_cases = [qualifier.replace(b'content in (original, proposed)', b'True', 1),
                           qualifier.replace(b'live fixture historical source mismatch', b'changed', 1)]
        for number, (data, outside) in enumerate(live_cases):
            with self.subTest(live=number):
                self.assertNotEqual(data, reviewed)
                blob = lambda rev, path: data if (rev, path) == (self.replay.REVIEWED, self.replay.LIVE) else self.blob(rev, path)
                with self.assertRaisesRegex(ValueError, 'historical replay'):
                    self.replay.successor_sources(blob)
                if outside:
                    with self.assertRaisesRegex(ValueError, 'historical replay'):
                        self.replay.restore_helper(data, self.blob(source.HISTORICAL_MAIN, self.replay.LIVE))
                else:  # Helper-internal: restoration alone cannot judge reviewed bytes.
                    self.replay.restore_helper(data, self.blob(source.HISTORICAL_MAIN, self.replay.LIVE))
        for number, data in enumerate(admission_cases):
            with self.subTest(admission=number):
                blob = lambda rev, path: data if (rev, path) == (self.replay.REVIEWED, self.replay.QUALIFIER) else self.blob(rev, path)
                with self.assertRaisesRegex(ValueError, 'historical replay'):
                    self.replay.successor_sources(blob)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    ids = sorted(t.id() for t in ci.flatten(suite))
    result = unittest.TextTestRunner(verbosity=2 if args.v else 1, resultclass=ci.EvidenceResult).run(suite)
    if args.report:
        ci.write(args.report, {'successful': result.wasSuccessful(), 'tests': ids, 'events': result.events})
    sys.exit(0 if result.wasSuccessful() else 1)
